/* free_air.h -- PORT ADDITION (not original game code): FREER AIR, the OPEN REFLEX option after Open Unleashed's FREER AIR ROTATION
 * (core/notes/bike.md "FREER AIR").
 *
 * The original's in-air control (MX 0043f2e0 bike_air.c, ATV 00494910 vehicle_atv_controls.c) has two helpers that turn the bike
 * back toward its take-off orientation S+8c10 (ATV S+9810: level, nose along the flight path) by rotating the bodies directly
 * (slerp + phys_mech_rotate), not through the angular velocity:
 *   RX_FREE_AIR_NO_HOLD    the trick hold: once a cone latch has set (pitch S+8c60.., roll S+8cd0..) and the steering stick is
 *                          pushed past 0.9 the other way (the counter-steer that ends a whip), stick control stops and the bike is
 *                          slerped back at 0.012 per 7.5 ms step;
 *   RX_FREE_AIR_NO_SETTLE  the auto-level: once the sticks have been active for 0.25 s of air (S+8b74), every step without a stick
 *                          slerps the bike back at 0.008 per step.
 * With a bit set, that helper does not run for the local human players' bikes (never the AI's): without the hold the stick keeps
 * control (the counter-steer turns the bike back at the stick's own rate), without the settle a released bike keeps its angle and
 * coasts on its angular velocity (the original's per-step AngularRateDamping S+5680 still slows it). Everything else of the air
 * model is the original's: the stick mix and its 0.1 s lag filters, the assist fades, the gestures, the 10 rad/s cap, gravity.
 *
 * AIR LOOSENESS (round 2, rx_session_set_free_air_looseness, L = 0 .. 1): for the same local players, while airborne, the
 * original's air values are eased toward "looser", each relative to the value the simulation holds (so a tuned value, e.g. from a
 * physics editor writing S+5680 / S+56e0 / the filter time constants, is eased from where it is):
 *   damping     AngularRateDamping per step d (S+5680, 0.985):  1 - (1 - d) * (1 - L * damp)      (momentum carries longer), only
 *               while coasting after stick use (no stick held, S+8b74 > 0): the stick's own rate and a hands-off jump stay the original's
 *   take-off    OnTakeoffDamping t (S+56e0, first air step):     1 - (1 - t) * (1 - L * takeoff)   (takeoff = 0: kept, see free_air.c)
 *   filters     the stick mix's lag time constants (S+8b78 / 8b8c / 8ba0 +4, 0.1 s): tau * (1 - L * lag)   (quicker, still smooth)
 *   authority   the stick mix's angular increment:               * (1 + L * gain)
 * with the bounded factors of rx_fa_curve (free_air.c). L = 0 is FREER AIR's helpers-only model bit for bit. The 10 rad/s cap,
 * the grounded code and the AI are never touched.
 *
 * AUTO LEVEL and COUNTER-STEER SNAP strengths (w-airsliders, rx_session_set_air_helpers; 0 .. 1 each, 1 = the original's helper): the two
 * mode bits are the ends of two strengths. snap = 0 is bit 1 (NO_HOLD), level = 0 is bit 2 (NO_SETTLE); in between (RX_FA_PART_HOLD /
 * RX_FA_PART_SETTLE) the helper still runs but its per-step slerp blend (0.012 / 0.008) is scaled by the strength (rx_fa_hold_rate /
 * rx_fa_settle_rate), so the bike is turned back that much slower; at 1 the helper's code takes the original's path untouched (no
 * flag, no multiply). rx_session_set_free_air(mode) maps its bits to 0 / 1.
 *
 * Off (mask 0, strengths 1 and looseness 0, the default) is the original bit for bit: the gate reads one host static and nothing of the guest. */
#ifndef RX_FREE_AIR_H
#define RX_FREE_AIR_H

#include "../src/rx_mem.h"
#include <stdint.h>

#define RX_FREE_AIR_NO_HOLD   1   /* the same spelling as rx_api.h */
#define RX_FREE_AIR_NO_SETTLE 2
#define RX_FREE_AIR_ALL       3

#define RX_FA_LOOSE           0x100        /* rx_fa_for: AIR LOOSENESS applies to this simulation (rx_fa_looseness_value > 0) */
#define RX_FA_PART_HOLD       0x200        /* rx_fa_for: the trick hold runs at 0 < strength < 1 (rx_fa_hold_rate) */
#define RX_FA_PART_SETTLE     0x400        /* rx_fa_for: the auto-level runs at 0 < strength < 1 (rx_fa_settle_rate) */

extern uint32_t rx_fa_mask_value;          /* the mask of the strengths at 0 (bit 1 snap, bit 2 level); restored with the core's statics */
extern float    rx_fa_level_value;         /* AUTO LEVEL strength 0 .. 1 (rx_session_set_air_helpers), 1 = the original */
extern float    rx_fa_snap_value;          /* COUNTER-STEER SNAP strength 0 .. 1, 1 = the original */
extern float    rx_fa_looseness_value;     /* the session's AIR LOOSENESS 0 .. 1 (rx_session_set_free_air_looseness) */
extern uint32_t rx_fa_any;                 /* mask or looseness set: the only word the gate reads when FREER AIR is off */
void     rx_fa_set(uint32_t mask);                 /* the bits as strengths: a set bit = 0, a clear bit = 1 */
void     rx_fa_set_helpers(float level, float snap);   /* 0 .. 1 each (the caller validates) */
void     rx_fa_set_looseness(float l);
uint32_t rx_fa_local(rx_addr sim);         /* mask | RX_FA_LOOSE (when looseness > 0) if `sim` is a local human player's vehicle simulation, else 0 */

/* the helpers to leave out / the looseness for simulation S this step (0: all of the original's) */
static inline uint32_t rx_fa_for(rx_addr S) { return rx_fa_any ? rx_fa_local(S) : 0u; }

/* AIR LOOSENESS's bounded factors at L = 1 (rx_fa_curve, free_air.c; development tools may change them before a ride) */
typedef struct rx_fa_curve { float damp, takeoff, lag, gain; } rx_fa_curve;
extern rx_fa_curve rx_fa_curve_value;
/* the helpers' slerp blends scaled by their strength (only called with RX_FA_PART_HOLD / RX_FA_PART_SETTLE set) */
float rx_fa_hold_rate(float t);            /* the trick hold's 0.012 * snap */
float rx_fa_settle_rate(float t);          /* the auto-level's 0.008 * level */
/* the eased values (only called with RX_FA_LOOSE set) */
float rx_fa_loose_damping(float d);        /* a per-step damping factor (S+5680 lanes) */
float rx_fa_loose_takeoff(float t);        /* a first-air-step damping factor (S+56e0 lanes) */
float rx_fa_loose_gain(void);              /* the stick increment's factor */
/* 0042d6b0's first-order lag (phys_lag_filter_update) with the time constant F+4 eased (F+4 itself unchanged; F+8 = the k used) */
float rx_fa_lag_update(rx_addr f, float dt, float target);

#endif
