/* reflex_hud.c -- MXUIReflexComponent (the HUD layers' "Arrows"): the wreck-recovery hint, kept outside guest memory for the presenter.
 * The include/rx_reflex_hud.h comment has the original's functions and rules. Product only (the oracle build gets stubs): the original
 * builds the component with the HUD layer, the product has no UI classes, so this file holds what the component holds -
 *   - which arrow nodes it made visible (vf11 005f7b80, once per frame: rx_reflex_hud_tick) and the frame it turned visible / hidden
 *     (+0x114: the sound slots 0 and 5 start, slot 5 stops),
 *   - the counts of the four wreck events the HUD / audio listeners hear (WreckRecoveryBegin / Success / Failed / Reset).
 * The wreck state itself (P + 0xb38 ...) is the game's, read at the query. Nothing here writes guest memory. */
#include "reflex_hud.h"
#ifndef RX_ORACLE
#include "crt.h"
#include "sound_state.h"
#include <string.h>

#define NAME_WRECK_BEGIN    0x00941278u   /* "WreckRecoveryBegin" */
#define NAME_WRECK_SUCCESS  0x00941260u   /* "WreckRecoverySuccess" */
#define NAME_WRECK_FAILED   0x009416fcu   /* "WreckRecoveryFailed" */
#define NAME_WRECK_RESET    0x009416e8u   /* "WreckRecoveryReset" */
#define PAD_BLOCKS          0x009c52c4u   /* the input blocks of the controllers (world_misc.c) */

static struct {
    uint32_t shown, lit, armed;                        /* the component's visibility, its arrows' visibility, +0x114 */
    uint32_t begin, success, fail, fail_any, reset, last_result;
} rh;

void rx_reflex_hud_reset(void) { memset(&rh, 0, sizeof rh); }

static int is_name(rx_addr name, rx_addr literal) { return name != 0 && crt_strcmp(name, literal) == 0; }

/* P + 0xb44 -> the arrow (RX_REFLEX_DIR_*) of vf11's byte table 0x5f7e64: 2, 3 top; 5, 6 bottom; 8, 9 left; 11, 12 right; the rest none */
static int32_t direction_of(uint32_t mode) {
    switch (mode) {
    case 2: case 3: return RX_REFLEX_DIR_UP;
    case 5: case 6: return RX_REFLEX_DIR_DOWN;
    case 8: case 9: return RX_REFLEX_DIR_LEFT;
    case 11: case 12: return RX_REFLEX_DIR_RIGHT;
    default: return RX_REFLEX_DIR_NONE;
    }
}
/* the arrow's bit in lit_mask: ArrowTop 0, ArrowRight 1, ArrowBottom 2, ArrowLeft 3 (the component's +0x100 .. +0x10c) */
static uint32_t bit_of(int32_t dir) {
    switch (dir) {
    case RX_REFLEX_DIR_UP: return 1u;
    case RX_REFLEX_DIR_RIGHT: return 2u;
    case RX_REFLEX_DIR_DOWN: return 4u;
    case RX_REFLEX_DIR_LEFT: return 8u;
    default: return 0u;
    }
}

/* the vehicle the events name: the local player's (P + 0x1ac) */
static int ours(rx_addr player, rx_addr vehicle) { return player && vehicle && U32(player, 0x1ac) == vehicle; }

void rx_reflex_hud_event(rx_addr local, rx_addr name, rx_addr payload, uint32_t bytes) {
    if (bytes != 8 || !name || !payload) return;
    /* payload {arg, sender}: rx_world_raise_event 00554da0 (arg: the activity for Begin / Failed / Reset, the driver for Success; sender: the vehicle) */
    const rx_addr vehicle = U32(payload, 4);
    if (is_name(name, NAME_WRECK_BEGIN)) { if (ours(local, vehicle)) rh.begin++; }
    else if (is_name(name, NAME_WRECK_SUCCESS)) { if (ours(local, vehicle)) { rh.success++; rh.last_result = 1; } }
    else if (is_name(name, NAME_WRECK_FAILED)) {
        /* 005f7b00: ArrowTop / ArrowRight / ArrowBottom get the colour (1, 0, 0, 0): invisible until "ReflexIn" plays again (ArrowLeft is
         * not in the handler's loop of three); the handler does not look at whose vehicle fell - the presenter reads fail_any_events */
        rh.fail_any++;
        if (ours(local, vehicle)) { rh.fail++; rh.last_result = 2; }
    } else if (is_name(name, NAME_WRECK_RESET)) { if (ours(local, vehicle)) rh.reset++; }
}

/* MXUIReflexComponent::vf11 005f7b80 */
void rx_reflex_hud_tick(rx_addr activity, rx_addr player) {
    if (!activity || !player || U32(player, 0x94) != activity) { rh.shown = 0; return; }
    const rx_addr vehicle = U32(player, 0x1ac);
    const uint32_t kind = vehicle ? U32(vehicle, 0x194) : 2u;
    if (!vehicle || kind > 1u) { rh.shown = 0; return; }   /* the outer else: SetVisible(false), nothing else */
    const uint32_t mode = U32(player, 0xb44);
    if (mode == 0) { rh.lit = 0; rh.shown = 0; }
    else {
        const int32_t dir = direction_of(mode);
        if (dir != RX_REFLEX_DIR_NONE) { rh.shown = 1; rh.lit |= bit_of(dir); }
    }
    if (rh.shown && !rh.armed) {
        rh.armed = 1;
        rx_sound_rider_sound(vehicle, 0);   /* 006817c0 (0): WreckRecoveryBegin, FX/WRECK_AVOIDANCE_WARN */
        rx_sound_rider_sound(vehicle, 5);   /* 006817c0 (5): WreckAvoidanceLoop (an empty event in the install) */
    } else if (!rh.shown && rh.armed) {
        rh.armed = 0;                       /* 00681840 (5): the loop stops (the sound ring has no stop; the event is empty) */
    }
}

int rx_reflex_hud_fill(rx_addr player, rx_reflex_hud_state* v) {
    const rx_addr A = player ? U32(player, 0x94) : 0;
    if (!A) return 0;
    v->valid = 1;
    const rx_addr vehicle = U32(player, 0x1ac);
    v->vehicle_kind = vehicle ? U32(vehicle, 0x194) : 0xffffffffu;
    v->mode = U32(player, 0xb44);
    v->direction = direction_of(v->mode);
    v->wreck_class = U32(player, 0xb30);
    v->wreck_type = U32(player, 0xb34);
    v->time_left = F32(player, 0xb38);
    v->time_max = F32(player, 0xb3c);
    v->progress = F32(player, 0xb40);
    v->threshold = 0.25f;                 /* 0x92ff84 in 0054ea30 */
    v->stick_x = F32(player, 0xb48);
    v->stick_y = F32(player, 0xb4c);
    v->recovered = U32(player, 0x11d0);
    v->falls = U32(player, 0x11d4);
    const int32_t pad = I32(player, 0x1f0);
    if (pad >= 0 && pad < 4) {
        const rx_addr block = U32(0, PAD_BLOCKS + 4u * (uint32_t)pad);
        if (block) {
            const uint32_t right = U32(block, 0x512c) == 1u, motion = U32(block, 0x5130);
            v->handedness = right;
            v->motion = motion;
            /* the wreck steer map 00548fc0 (the lean map 00549050 makes the same choice for the Y sources) */
            v->input_source = motion == 1 ? (right ? 1u : 3u) : motion == 2 ? (right ? 3u : 2u) : (right ? 0u : 2u);
        }
    }
    v->shown = rh.shown;
    v->lit_mask = rh.lit;
    v->begin_events = rh.begin;
    v->success_events = rh.success;
    v->fail_events = rh.fail;
    v->fail_any_events = rh.fail_any;
    v->reset_events = rh.reset;
    v->last_result = rh.last_result;
    return 1;
}
#else
void rx_reflex_hud_reset(void) {}
void rx_reflex_hud_event(rx_addr local, rx_addr name, rx_addr payload, uint32_t bytes) { (void)local; (void)name; (void)payload; (void)bytes; }
void rx_reflex_hud_tick(rx_addr activity, rx_addr player) { (void)activity; (void)player; }
int rx_reflex_hud_fill(rx_addr player, rx_reflex_hud_state* out) { (void)player; (void)out; return 0; }
#endif
