/* Read-only original lifecycle fields; no calls, UI policy, heuristics or state mutation. */
#include "rx_event_ui.h"
#include <stddef.h>

_Static_assert(sizeof(rx_event_ui_state) == 32, "event UI ABI size");
_Static_assert(offsetof(rx_event_ui_state, activity_type) == 4, "event UI ABI type");
_Static_assert(offsetof(rx_event_ui_state, native_pause_owner) == 28, "event UI ABI tail");

void rx_event_ui_snapshot(rx_addr a, rx_event_ui_state* out) {
    *out = (rx_event_ui_state){0, -1, -1, -1, -1, -1, -1, -1};
    if (!a) return;
    const uint32_t vt = U32(a, 0);
    /* Exact constructors: 005562c0 base; 0055fdb0 FreeRide; 00565830 race stub; factory0055ef80 race; 00562280 Freestyle stub;
     * 00561910 Freestyle. Do not treat arbitrary/derived vtables as one of these layouts. The offline modes' other classes
     * (w-pausemodes, read-only: every one of them runs the base constructor 005562c0, which sets +0x23c / +0x21c, and the base's
     * Pre-enter 005589c0 for +0xff068): the Waypoint race 0x00943c14 and its championship classes, the Section Race 0x00944a74 (a
     * race stub), the championship race classes, the Freestyle championship classes and the Free Ride challenges (MXActivityChallenge
     * 0x009450cc and its HillClimb / BigAir / Pathfinder / StepUp / Trailblazer / TargetPractice classes). */
    switch (vt) {
    case 0x00942904: case 0x0094329c: case 0x00943454:
    case 0x00943554: case 0x009441cc: case 0x009442cc:
    case 0x00943c14: case 0x00943d14: case 0x00943e14: case 0x00943f14:   /* Waypoint race + championship intro / practice / main */
    case 0x00944a74:                                                       /* Section Race */
    case 0x00943654: case 0x00943754: case 0x00943854: case 0x00943954: case 0x00943a54:   /* race championship intro / practice / heat / main / LCQ */
    case 0x009443cc: case 0x009444cc: case 0x009445cc:                     /* Freestyle championship intro / practice / main */
    case 0x009450cc: case 0x009451dc: case 0x009452ec: case 0x009453fc: case 0x00945514: case 0x00945624: case 0x00945734:   /* challenges */
        break;
    default:
        return;
    }
    /* 005562c0 sets type/state; 005589c0 clears/sets rules hold and both
     * Activity_Pre00558ce0 and RaceStub_Pre005681e0 consume it. */
    out->activity_type = I32(a, 0x23c);
    out->race_state = I32(a, 0x21c);
    /* SkipToNextScenario00736d60 sets this word; scenario update007473f0
     * consumes/clears it. Expose the word, not an inferred scenario capability. */
    out->skip_requested = I32(0, 0x00a7f748);
    out->known = RX_EVENT_UI_ACTIVITY_TYPE | RX_EVENT_UI_RACE_STATE |
        RX_EVENT_UI_SKIP_REQUESTED;
    /* Pre-enter initializes this; the base constructor's Inactive state does not. */
    if (out->race_state >= 0 && out->race_state <= 4) {
        out->rules_hold = I32(a, 0xff068);
        out->known |= RX_EVENT_UI_RULES_HOLD;
    }
    if (out->race_state == 4) {
        /* The Post sub-state (0 before, 1 the finish scenario, 2 the results, 3 the results scenario): MXActivityRace and
         * MXActivitySectionRace Post-enter00568da0 initializes it;00569a40 sets it;00569a90 consumes0..3 (+0x13e640); the Waypoint race
         * keeps it at +0x13e618 (0057a500 / 0057a570) and Freestyle at +0x13e5f8 (00563aa0 / 00563b10); each raises "Show Activity
         * Results" at 2 (00559a40 for the Waypoint). Stub/FreeRide have no audited same field. */
        uint32_t at = 0;
        if (vt == 0x00943554 || vt == 0x00944a74) at = 0x13e640;
        else if (vt == 0x00943c14) at = 0x13e618;
        else if (vt == 0x009442cc) at = 0x13e5f8;
        if (at) { out->post_state = I32(a, at); out->known |= RX_EVENT_UI_POST_STATE; }
    }
    /* UI manager+28c alone cannot prove page ownership/initialized AllowPause.
     * Core does not execute the imported HUD/intro/outro Lua policy. Keep unknown. */
}
