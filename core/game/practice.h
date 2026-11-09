/* practice.h -- PORT ADDITION (not original game code): PRACTICE and the LAP LOG, for OPEN REFLEX's lap leaderboards (core/plan/LEADERBOARDS.md
 * 2.1, 3.3 and package C).
 *
 * PRACTICE: an exhibition lap race (ActivityMode +0x238 == 1 of a lap type +0x23c: National 0, Supercross 1, Terracross 0x11, UTV Moto 0x12,
 * Powercross 0x13) made endless with the original's own Practice rule: MXActivityRaceStub::vf2 00566170 writes NumLaps A+0xff070 = 0x7fffffff
 * for ActivityMode 2 (build_activity_start.c); with the flag on it does so for the exhibition race too. The lap counter D+0xf1c stops at 0x61
 * (race_driver.c lap_completed) and the race finishes only when the laps reach NumLaps, so the race never finishes (the end check skips
 * nothing: a driver is done only at laps >= NumLaps). The flag is a session static (rx_session_set_practice, before the event loads); off it
 * is one test of a host static and the game is the original bit for bit.
 *
 * LAP LOG: every line crossing of the local human that the race counted (lap_completed after its early returns), in ANY exhibition lap race
 * (practice or not; online races too): a ring of RX_LAP_LOG_RING records with a session serial (D+0xf1c stops at 97, the serial does not).
 * Per lap it keeps what the boards need to decide a clean flying lap: the lap began at the line (D+0x10f0 clear), the raw lap clock D+0xf20,
 * the time corrected to the sub-frame crossing instant (below), the off-track rule's penalty at the line, seconds off the track, the
 * resets, whether the wrong-way indicator showed, and the non-original settings in use (mods bits, OR over the lap's frames).
 *
 * The sub-frame correction. RaceDriver::vf7 0050f620 adds the frame's dt to D+0xf20 and THEN runs the rules (00511430: RuleFinishLine
 * 0051a7a0 tests the move D+0x80 -> V+0xc0 against the line plane), so at a crossing D+0xf20 already holds the whole frame. The vehicle
 * crossed at the fraction f of that frame's move, (1 - f) * dt before the frame's end: over = (1 - f) * dt. The lap that ends here really
 * ended `over` earlier, and the lap that begins really began `over` earlier, so exact = time - over + carry, carry = over (the previous
 * crossing's). The exact times of a session add up to the raw ones minus the last crossing's over. The move is the vehicle's position at the
 * end of the previous frame (kept by the session layer) to its position now; when that does not straddle the plane (a frame with a reset),
 * the finish rule's own D+0x80 is used; when neither does, over = 0 (exact = time + carry).
 *
 * Every hook only writes host statics (nothing in the guest), so the game runs the same with or without them. The statics are reset with
 * the session (core/src/rx_reset.h).
 *
 * SPAWN MARKERS IN PRACTICE (w-lb-markers; the user's MX Bikes rule): OPEN REFLEX's own SPAWN MARKER (rx_session_spawn_at, the game's own
 * reset to a spot) also works in PRACTICE, any time while racing (the RESET key keeps the exhibition race's rule). Riding back to the
 * marker invalidates the lap in progress and the timing restarts from the finish line:
 *   - rx_lap_log_marker (the session layer, right after the respawn began): the open lap counts a marker (its record's `markers`), and
 *     the driver's "lap did not begin at the line" flag D+0x10f0 is set as the original sets it for the lap from the gate (RaceDriver::vf3
 *     0050f090), so the game's own best lap (D+0x10c4, "Best Lap") never takes it. The respawn's own reset is the marker's: not a reset
 *     of the lap (the rising edge of P+0x116c is taken as already seen).
 *   - the lap runs on (an out-lap) to the next crossing of the line, which ends it (logged with markers > 0: invalid) and begins a fresh
 *     flying lap from the line (lap_completed clears D+0x10f0 there, as after the gate lap).
 *   - rx_practice_marker_settle (the session layer, after the frame that put the vehicle at the marker): the driver's place on the track
 *     is the marker's, as RaceDriver::vf3 places a Free Ride driver (the nearest node of the whole track, 00538440: section D+0x14, the
 *     segment D+0x18 -> D+0x1c, the lap progress node D+0x90; the track-advance rule's section), the last position D+0x80 is the
 *     vehicle's (so the jump itself is never a crossing of the line) and the off-track rule's excursion ends (on track): without it the
 *     rules would see the rider far from its old place on the track (off track, then an off-track reset back there).
 * These write the guest, but only in a PRACTICE session (a port addition of its own); practice off never reaches them. */
#pragma once
#include "rx_game.h"
#include <stdint.h>

/* ---- PRACTICE ---- */
extern uint32_t rx_practice_value;                 /* the session's flag (rx_session_set_practice) */
void rx_practice_set(int on);
static inline int rx_practice_on(void) { return rx_practice_value != 0; }
/* the activity A takes the practice rule: the flag is on, A is an exhibition race (+0x238 == 1) of a lap type (+0x23c) */
int  rx_practice_applies(rx_addr A);
/* the activity types that are lap races (National, Supercross, Terracross, UTV Moto, Powercross) */
int  rx_practice_lap_type(uint32_t type);

/* ---- LAP LOG ---- */
#define RX_LAP_LOG_RING 64
typedef struct rx_lap_note {
    uint32_t serial;
    int32_t  flying;
    float    time, exact, penalty, off_track, race_time;
    uint32_t resets;
    int32_t  wrong_way;
    uint32_t mods;
    uint32_t markers;   /* rides back to a SPAWN MARKER in the lap (practice; w-lb-markers) */
} rx_lap_note;
/* mods bits (the same as rx_api.h's rx_lap_record) */
#define RX_LAP_MOD_PHYS_CONFIG   1u   /* rx_ps_active bit 1: a PHYSICS STUDIO config with changes */
#define RX_LAP_MOD_PHYS_RESTORED 2u   /* bit 2: the guard put a diverged vehicle back on the file's values */
#define RX_LAP_MOD_PHYS_TAINT    4u   /* bit 4: a config was set at some point of the session */
#define RX_LAP_MOD_AIR_HELPERS   8u   /* FREER AIR: the air helpers are not the original's (auto level != 1, snap != 1, looseness != 0, mode bits) */
#define RX_LAP_MOD_MAP_SCALE    16u   /* MAP SCALE != 1 */

/* the session layer (api/practice_session.inc), once per frame: the local human's race driver (0: none) before the frame */
void rx_lap_log_track(rx_addr driver);
/* after the frame: the per-lap accumulators from the frame's state (the session layer reads it as rx_session_hud does) */
void rx_lap_log_frame(int racing, int on_track, int wrong_way, int resetting, uint32_t mods, const float vehicle_pos[3]);
/* RaceDriver::vf7 0050f620: the lap clock moved by dt this frame (only the tracked driver counts) */
static inline void rx_lap_log_clock(rx_addr driver, float dt);
/* race_driver.c lap_completed: a counted crossing of the tracked driver, before D+0x10f0 / D+0xf20 / the penalty are cleared */
void rx_lap_log_note(rx_addr driver, float penalty);
/* race_rules.c, next to rx_race_line_crossed: f in [0, 1] where the move prev -> cur meets the line's plane, -1 when it does not cross */
float rx_race_line_fraction(rx_addr line, const float cur[3], const float prev[3]);
/* PRACTICE's SPAWN MARKER (w-lb-markers; above): the respawn to the marker began for the tracked driver d (rx_session_spawn_at) */
void rx_lap_log_marker(rx_addr d);
/* after a frame: once the respawn's reset has run out (P+0x116c <= 0, the vehicle at the marker), the driver's place is the vehicle's
 * (done after that frame and the next one); 1 while one is pending */
int  rx_practice_marker_settle(rx_addr d);
/* the records after `after_serial`, oldest first, at most max; the count */
int  rx_lap_log_read(uint32_t after_serial, rx_lap_note* out, int max);

extern rx_addr rx_lap_log_driver;                  /* the tracked driver (rx_lap_log_track) */
extern float   rx_lap_log_dt;                      /* the tracked driver's lap clock step of this frame */
static inline void rx_lap_log_clock(rx_addr driver, float dt) { if (driver == rx_lap_log_driver) rx_lap_log_dt = dt; }
