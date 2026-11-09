/* race_hud.c -- what the race HUD components read that the other HUD copies of the core (rx_race_format, rx_powercross_hud) do not carry:
 * the Eliminator's "Player Eliminated" message, the Outrun gate counter, the Section Race split times and section leaders, the event
 * timer, and the Freestyle (FMX) judges. The include/rx_race_hud.h comment lists each component, its original function and the fields.
 *
 * The original builds these as UI components (core-free MXUI classes) that subscribe to the activity's typed events and read the local
 * driver every frame. The product has no UI classes, so this file keeps, outside the guest memory, what each component would hold:
 *   - an observer of every dispatched event (rx_world_event_dispatch calls it after the canonical listeners, like the career result
 *     observer), which records the same payload fields the component handlers read, at the moment the handler would run;
 *   - the per-frame update of the one component that accumulates over frames (the FMX judge bars).
 * Nothing here writes guest memory, so the oracle's memory comparisons are unaffected (the file is product only).
 * The presenter formats the strings (the install's localization tables) and draws the layers. Single session / thread. */
#include "race_hud.h"
#include "restored_champ_weekend.h"   /* RESTORED CONTENT: rx_restored_champ_base_type */
#ifndef RX_ORACLE
#include "crt.h"
#include <string.h>
#include <float.h>
#include <limits.h>
#include "rider_cap.h"

#define EVENT_CHANNEL_GLOBAL    0x00a93f74u   /* the global channel "Activity Init" / "Activity UnInit" are raised on */
#define NAME_ACTIVITY_INIT      0x00935ea4u
#define NAME_ACTIVITY_UNINIT    0x00941d90u
#define NAME_RESTART            0x00941ba0u
#define NAME_PLAYER_ELIMINATED  0x0093ef48u
#define NAME_SECTION_WINNER     0x0093ef5cu
#define NAME_ENTERED_SECTION    0x0092b8bcu
#define NAME_HIT_DISCOVERY      0x00942898u   /* "HitDiscoveryFound" */
#define EVENT_CHANNEL_STUNTS    0x00a94304u   /* the stunt channel "StuntBegin" / "PhysicsStuntBegin" are raised on */
#define NAME_STUNT_BEGIN        0x0095ba28u
#define NAME_PHYSICS_STUNT_BEGIN 0x009607a0u

static struct {
    rx_addr activity;                                   /* the bound activity (the last "Activity Init"), 0 none */
    uint32_t restarts;
    uint32_t eliminated_events;
    rx_addr eliminated_player, eliminator_player;
    char eliminated_name[RX_RACE_HUD_NAME], eliminator_name[RX_RACE_HUD_NAME];
    uint32_t gate_events;
    int32_t gate_last, gate_number, gate_total;         /* gate_last = the component's +0x3a0 */
    uint32_t leader_set, leader_events;
    char leader_names[RX_RACE_HUD_SECTIONS][RX_RACE_HUD_NAME];
    int32_t bars[4];
    uint32_t final_events, final_shown;
    float final_scores[5];
    uint32_t discovery_events;                           /* MXUIHitDiscoveredComponent: the local player's "HitDiscoveryFound" events */
    uint32_t stunt_events, stunt_count;                  /* MXUIStuntFeedbackOverlay: begin events of the local player, ring entries kept */
    rx_stunt_hud_entry stunts[RX_STUNT_HUD_RING];        /* [0] the newest */
} hud;

void rx_race_hud_reset(void) { memset(&hud, 0, sizeof hud); rx_reflex_hud_reset(); }

/* the player's name buffer (player + 0x54, the table's DisplayName converted to UTF-8) */
static void copy_name(rx_addr player, char* out) {
    memset(out, 0, RX_RACE_HUD_NAME);
    if (!player) return;
    for (uint32_t i = 0; i + 1 < RX_RACE_HUD_NAME; i++) {
        const uint8_t c = U8(player, 0x54 + i);
        if (!c) break;
        out[i] = (char)c;
    }
}

static int is_name(rx_addr name, rx_addr literal) { return name != 0 && crt_strcmp(name, literal) == 0; }

/* the activity's local human player (its driver table at +0x268, count +0x26c): the one the dashboard components are bound to */
static rx_addr local_human(rx_addr activity) {
    const rx_addr list = U32(activity, 0x268);
    const uint32_t n = U32(activity, 0x26c);
    for (uint32_t i = 0; list && i < n && i < 64; i++) {
        const rx_addr d = U32(list, 4u * i);
        const rx_addr p = d ? U32(d, 0x74) : 0;
        if (p && U32(p, 4) != 0 && U32(p, 0x10) == 0) return p;
    }
    return 0;
}

/* the section race driver of a player (the activity's driver table entry whose player it is) */
static rx_addr driver_of(rx_addr activity, rx_addr player) {
    const rx_addr list = U32(activity, 0x268);
    const uint32_t n = U32(activity, 0x26c);
    for (uint32_t i = 0; list && i < n && i < 64; i++) {
        const rx_addr d = U32(list, 4u * i);
        if (d && U32(d, 0x74) == player) return d;
    }
    return 0;
}

/* a LocString key (a record's +0x40 reader string pointer) into a fixed buffer */
static void copy_key(rx_addr record, char* out) {
    memset(out, 0, RX_STUNT_HUD_KEY);
    const rx_addr key = record ? U32(record, 0x40) : 0;
    if (!key) return;
    for (uint32_t i = 0; i + 1 < RX_STUNT_HUD_KEY; i++) {
        const uint8_t c = U8(key, i);
        if (!c) break;
        out[i] = (char)c;
    }
}

/* MXUIStuntFeedbackOverlay 00603130: the newest slot takes the tracker's trick / stunt text, the older ones move up (the ring is the slots'
 * history; the presenter keeps the three the overlay shows and its 3 s clock) */
static void stunt_begin(rx_addr player, int physics) {
    const rx_addr K = player + 0xb58;
    const rx_addr trick = U32(K, 0x1c), stunt = U32(K, 0x68);
    if (!trick && !stunt) return;                       /* 006033d0 builds nothing */
    memmove(&hud.stunts[1], &hud.stunts[0], sizeof hud.stunts - sizeof hud.stunts[0]);
    memset(&hud.stunts[0], 0, sizeof hud.stunts[0]);
    hud.stunts[0].physics = physics ? 1u : 0u;
    copy_key(trick, hud.stunts[0].trick);
    copy_key(stunt, hud.stunts[0].stunt);
    hud.stunt_events++;
    hud.stunts[0].number = hud.stunt_events;
    if (hud.stunt_count < RX_STUNT_HUD_RING) hud.stunt_count++;
}

void rx_race_hud_observe(rx_addr channel, rx_addr name, rx_addr payload, uint32_t bytes) {
    if (!name || !payload) return;
    if (bytes == 4 && channel == U32(0, EVENT_CHANNEL_GLOBAL)) {
        /* the dashboard components of the activity are created with it and die with it */
        if (is_name(name, NAME_ACTIVITY_INIT)) { rx_race_hud_reset(); hud.activity = U32(payload, 0); }
        else if (is_name(name, NAME_ACTIVITY_UNINIT) && U32(payload, 0) == hud.activity) rx_race_hud_reset();
        return;
    }
    const rx_addr activity = hud.activity;
    if (!activity) return;
    if (bytes == 8 && U32(0, 0x00a89bc8) == channel && is_name(name, NAME_ENTERED_SECTION)) {
        /* 00605190: payload {vehicle, TimedRaceSection}: +0x288 its number, +0x280 whether it gives time */
        const rx_addr section = U32(payload, 4);
        if (section && hud.gate_last <= I32(section, 0x288) && U32(section, 0x280) != 0) {
            hud.gate_number = I32(section, 0x288);
            hud.gate_total = I32(activity, 0xff08c);
            hud.gate_last = hud.gate_number;
            hud.gate_events++;
        }
        return;
    }
    if (channel == U32(0, EVENT_CHANNEL_STUNTS) && bytes >= 8 && (is_name(name, NAME_STUNT_BEGIN) || is_name(name, NAME_PHYSICS_STUNT_BEGIN))) {
        /* 00603130 / 00603110: only the overlay's own (local) player; the text is built from his trick tracker as it is now (006033d0) */
        const rx_addr player = U32(payload, 0);
        if (player && player == local_human(activity)) stunt_begin(player, is_name(name, NAME_PHYSICS_STUNT_BEGIN));
        return;
    }
    if (channel != U32(activity, 0x290)) return;
    if (bytes == 8) rx_reflex_hud_event(local_human(activity), name, payload, bytes);   /* the wreck-recovery events (reflex_hud.c) */
    if (bytes == 12 && is_name(name, NAME_PLAYER_ELIMINATED) && U32(payload, 0) == activity) {
        /* 00605de0: payload {activity, driver, next}: "<next's name>\n<STR_ELIMINATED>\n<driver's name>!" */
        const rx_addr driver = U32(payload, 4), next = U32(payload, 8);
        hud.eliminated_player = driver ? U32(driver, 0x74) : 0;
        hud.eliminator_player = next ? U32(next, 0x74) : 0;
        copy_name(hud.eliminated_player, hud.eliminated_name);
        copy_name(hud.eliminator_player, hud.eliminator_name);
        hud.eliminated_events++;
    } else if (bytes == 8 && is_name(name, NAME_SECTION_WINNER)) {
        /* 00606970 (through the thunk 005f34f0): payload {driver or 0, section}: label section = "#<section + 1> <name>" */
        const uint32_t section = U32(payload, 4);
        if (section < RX_RACE_HUD_SECTIONS) {
            const rx_addr driver = U32(payload, 0);
            copy_name(driver ? U32(driver, 0x74) : 0, hud.leader_names[section]);
            hud.leader_set |= 1u << section;
            hud.leader_events++;
        }
    } else if (bytes == 12 && is_name(name, NAME_HIT_DISCOVERY)) {
        /* 006046f0 (the component's handler of 006045b0): payload {player, found, total}: the text "found/total" for the component's own
         * driver's player; the counts themselves are read from the tracker (rx_race_hud_fill) */
        const rx_addr player = U32(payload, 0);
        if (player && player == local_human(activity)) hud.discovery_events++;
    } else if (bytes == 4 && is_name(name, NAME_RESTART) && U32(payload, 0) == activity) {
        /* the components' "Restart" handlers: the section labels cleared (00606820), the gate counter's last number 0 (00605280),
         * the judge bars hidden (00607660), the final score card hidden (006036d0) */
        hud.restarts++;
        hud.leader_set = 0;
        memset(hud.leader_names, 0, sizeof hud.leader_names);
        hud.gate_last = 0;
        memset(hud.bars, 0, sizeof hud.bars);
        hud.final_shown = 0;
        hud.stunt_count = 0;   /* 006030e0: the stunt overlay hides and empties its slots */
    }
}

/* MXUIFMXFinalScoreCard (the layer's "Judges Panel"): "WindDown Entered" (00603b10) copies the local driver's scorer (player + 0xc70) and
 * shows the card, "Post Entered" (005e2710) hides it; Activity_SetState raises those only for a registered listener, so the state change is
 * observed there (after the old state's Exited handlers and the new state's enter, where the listeners would run). */
void rx_race_hud_state_entered(rx_addr activity, uint32_t state) {
    if (activity == 0 || activity != hud.activity) return;
    if (state == 3 /* WindDown */) {
        const rx_addr player = local_human(activity);
        if (player) {
            const rx_addr judge = player + 0xc70;
            hud.final_scores[0] = F32(judge, 0x14);   /* Overall */
            hud.final_scores[1] = F32(judge, 0x18);   /* Difficulty */
            hud.final_scores[2] = F32(judge, 0x1c);   /* Execution */
            hud.final_scores[3] = F32(judge, 0x24);   /* Flow */
            hud.final_scores[4] = F32(judge, 0x20);   /* Variety */
        }
        hud.final_shown = 1;
        hud.final_events++;
    } else if (state == 4 /* Post */) hud.final_shown = 0;
}

/* MXUIFMXFeedbackComponent::vf11 (00607540 -> 00607590): per judge column the bars below trunc(value) are shown and nothing hides
 * them before the next "Restart", so the lit count is the peak (cvttss2si: out of range / NaN = INT_MIN). */
void rx_race_hud_tick(rx_addr player) {
    const rx_addr activity = hud.activity;
    rx_reflex_hud_tick(activity, player);   /* MXUIReflexComponent::vf11 (reflex_hud.c) */
    if (!activity || !player || U32(player, 0x94) != activity || rx_restored_champ_base_type(I32(activity, 0x23c)) != 5) return;   /* (RESTORED CONTENT: a Freestyle championship stage too) */
    const rx_addr judge = player + 0xc70;
    const float value[4] = {F32(judge, 4) - F32(judge, 0x38), F32(judge, 8), F32(judge, 0x10) - F32(judge, 0x34), F32(judge, 0xc) - F32(judge, 0x30)};
    for (int i = 0; i < 4; i++) {
        const int32_t n = (value[i] > -2147483648.0f && value[i] < 2147483648.0f) ? (int32_t)value[i] : INT_MIN;
        if (n > hud.bars[i]) hud.bars[i] = n > 10 ? 10 : n;
    }
}

static int32_t slot_of(rx_addr game, rx_addr player) {
    return rx_seat_of(game + 0x7460, player);   /* (w-rc-core) seats 0..31 */
}

int rx_race_hud_fill(rx_addr game, rx_addr player, rx_race_hud_state* v) {
    const rx_addr A = player ? U32(player, 0x94) : 0;
    if (!A) return 0;
    v->valid = 1;
    v->activity_type = I32(A, 0x23c);
    v->activity_mode = I32(A, 0x238);
    v->activity_state = I32(A, 0x21c);
    v->clock = F32(A, 0x294);
    v->clock_limit = F32(A, 0x2bc);
    v->clock_counts_up = U32(A, 0x2c8) != 0;
    v->eliminated_slot = v->eliminator_slot = -1;
    if (hud.activity != A) return 1;
    v->restart_events = hud.restarts;
    v->eliminated_events = hud.eliminated_events;
    if (hud.eliminated_events) {
        v->eliminated_slot = slot_of(game, hud.eliminated_player);
        v->eliminator_slot = slot_of(game, hud.eliminator_player);
        memcpy(v->eliminated_name, hud.eliminated_name, RX_RACE_HUD_NAME);
        memcpy(v->eliminator_name, hud.eliminator_name, RX_RACE_HUD_NAME);
    }
    v->gate_events = hud.gate_events;
    v->gate_number = hud.gate_number;
    v->gate_total = hud.gate_total;
    v->section_current = -1;
    if (v->activity_type == 12) {   /* MXActivitySectionRace */
        const int32_t count = I32(A, 0x13e650);
        v->section_count = count;
        const rx_addr driver = driver_of(A, player);
        const int32_t at = driver ? I32(driver, 0x113c) : -1;
        v->section_current = at;
        if (at >= 0 && at < count) {
            const rx_addr running = U32(driver, 0x1140), best = U32(A, 0x13e654);
            if (running) v->section_time = F32(running, 4u * (uint32_t)at);
            if (best) v->section_best = F32(best, 4u * (uint32_t)at);
        }
        v->leader_set = hud.leader_set;
        v->leader_events = hud.leader_events;
        memcpy(v->leader_names, hud.leader_names, sizeof v->leader_names);
    }
    v->discovery_events = hud.discovery_events;
    if (v->activity_type == 8 && I32(A, 0xff180) == 1) {   /* MXActivityFreeRide in discovery mode (vf37): the tracker's counts */
        const rx_addr tracker = A + 0xff080;
        const int32_t total = I32(tracker, 0), found = I32(tracker, 0x84);
        if (total > 0 && total <= 0x20 && found >= 0 && found <= total) { v->discovery_total = total; v->discovery_found = found; }
    }
    if (v->activity_type == 5) {   /* MXActivityFreestyle */
        for (int i = 0; i < 4; i++) v->judge_bars[i] = hud.bars[i];
        v->final_events = hud.final_events;
        v->final_shown = hud.final_shown;
        memcpy(v->final_scores, hud.final_scores, sizeof v->final_scores);
    }
    return 1;
}

int rx_stunt_hud_fill(rx_addr player, rx_stunt_hud_state* v) {
    const rx_addr A = player ? U32(player, 0x94) : 0;
    if (!A) return 0;
    v->valid = 1;
    if (hud.activity != A) return 1;
    v->events = hud.stunt_events;
    v->restart_events = hud.restarts;
    v->count = hud.stunt_count;
    memcpy(v->entries, hud.stunts, sizeof v->entries);
    return 1;
}
#else
void rx_race_hud_reset(void) {}
void rx_race_hud_observe(rx_addr channel, rx_addr name, rx_addr payload, uint32_t bytes) { (void)channel; (void)name; (void)payload; (void)bytes; }
void rx_race_hud_tick(rx_addr player) { (void)player; }
void rx_race_hud_state_entered(rx_addr activity, uint32_t state) { (void)activity; (void)state; }
int rx_race_hud_fill(rx_addr game, rx_addr player, rx_race_hud_state* out) { (void)game; (void)player; (void)out; return 0; }
int rx_stunt_hud_fill(rx_addr player, rx_stunt_hud_state* out) { (void)player; (void)out; return 0; }
#endif
