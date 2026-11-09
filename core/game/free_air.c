/* free_air.c -- PORT ADDITION (not original game code): FREER AIR and AIR LOOSENESS (free_air.h, core/notes/bike.md "FREER AIR"). */
#include "free_air.h"
#include "phys_vec.h"
#include "rider_cap.h"

uint32_t rx_fa_mask_value;
float    rx_fa_level_value = 1.0f;     /* non-zero initial values are restored with the other statics (session-reset.md) */
float    rx_fa_snap_value = 1.0f;
float    rx_fa_looseness_value;
uint32_t rx_fa_any;
/* the factors at L = 1, chosen from the w-freeair2 measurements (core/plan/QUEUE.md w-freeair2): the coast damping 0.985 -> 0.9895
 * (time constant 0.50 -> 0.71 s; 0.991 let a full whip coast > 190 deg), the lag 0.1 s -> 0.08 s (0.07 s made the first frame of
 * a flick 56 % more abrupt than the original, 0.08 s 23 %), +10 % stick authority. The take-off
 * damping stays the original's (0): easing it lets the lip's kick through (a hands-off jump went from 1.45 to 4.2 rad/s and landed
 * 78 deg sideways with 40 % of its effect removed; a 0.6 stick whip's rate spiked to 1.8 x its plateau): that is the snappy, random kind of loose. */
rx_fa_curve rx_fa_curve_value = { 0.30f, 0.0f, 0.20f, 0.10f };

static uint32_t fa_part(void) {
    return (rx_fa_snap_value > 0.0f && rx_fa_snap_value < 1.0f ? RX_FA_PART_HOLD : 0u) | (rx_fa_level_value > 0.0f && rx_fa_level_value < 1.0f ? RX_FA_PART_SETTLE : 0u);
}
static void fa_update_any(void) {
    rx_fa_mask_value = (rx_fa_snap_value <= 0.0f ? RX_FREE_AIR_NO_HOLD : 0u) | (rx_fa_level_value <= 0.0f ? RX_FREE_AIR_NO_SETTLE : 0u);
    rx_fa_any = rx_fa_mask_value != 0 || fa_part() != 0 || rx_fa_looseness_value > 0.0f;
}
static float fa_unit(float x) { return x > 0.0f ? (x < 1.0f ? x : 1.0f) : 0.0f; }
void rx_fa_set(uint32_t mask) {
    rx_fa_snap_value = mask & RX_FREE_AIR_NO_HOLD ? 0.0f : 1.0f;
    rx_fa_level_value = mask & RX_FREE_AIR_NO_SETTLE ? 0.0f : 1.0f;
    fa_update_any();
}
void rx_fa_set_helpers(float level, float snap) { rx_fa_level_value = fa_unit(level); rx_fa_snap_value = fa_unit(snap); fa_update_any(); }
void rx_fa_set_looseness(float l) { rx_fa_looseness_value = l > 0.0f ? (l < 1.0f ? l : 1.0f) : 0.0f; fa_update_any(); }

/* MXGame [0x00a946b8]: the player slots G+0x153ec (count byte G+0x1541c), players G+0x7460 + slot * 0x12a0 (+0x10: 0 human,
 * 1 computer; +0x1ac the vehicle), the vehicle's simulation at +0x234 (the slot walk of api/rx_session.c local_player). Every
 * local human player counts (split screen), never a computer rider. Read only. */
uint32_t rx_fa_local(rx_addr sim) {
    const rx_addr g = U32(0, 0x00a946b8u);
    if (!g || !sim) return 0;
    const uint32_t count = U8(g, 0x1541c);
    for (uint32_t i = 0; i < count && i < 8; i++) {
        const uint32_t slot = rx_seat_order(g + 0x7460, i);
        if (slot >= RX_RC_MAX) continue;
        const rx_addr p = rx_seat(g + 0x7460, slot);
        if (U32(p, 0x10) != 0) continue;
        const rx_addr v = U32(p, 0x1ac);
        if (v && U32(v, 0x234) == sim) return rx_fa_mask_value | fa_part() | (rx_fa_looseness_value > 0.0f ? RX_FA_LOOSE : 0u);
    }
    return 0;
}

float rx_fa_hold_rate(float t) { return phys_mul(t, rx_fa_snap_value); }
float rx_fa_settle_rate(float t) { return phys_mul(t, rx_fa_level_value); }

static float fa_ease(float x, float share) {          /* 1 - (1 - x) * (1 - L * share) */
    const float keep = 1.0f - rx_fa_looseness_value * share;
    return 1.0f - (1.0f - x) * keep;
}
float rx_fa_loose_damping(float d) { return fa_ease(d, rx_fa_curve_value.damp); }
float rx_fa_loose_takeoff(float t) { return fa_ease(t, rx_fa_curve_value.takeoff); }
float rx_fa_loose_gain(void) { return 1.0f + rx_fa_looseness_value * rx_fa_curve_value.gain; }

float rx_fa_lag_update(rx_addr f, float dt, float target) {
    const float tau = F32(f, 4) * (1.0f - rx_fa_looseness_value * rx_fa_curve_value.lag);
    const float step = tau > dt ? dt : tau;
    const float k = phys_mul(step, phys_div(1.0f, tau));
    const float value = F32(f, 0);
    const float next = phys_add(phys_mul(phys_sub(target, value), k), value);
    SF32(f, 8, k);
    SF32(f, 0, next);
    return next;
}
