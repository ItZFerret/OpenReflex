/* race_driver.c -- the drivers (BaseDriver / RaceDriver / NetworkDriver / FreeRideDriver): one per racer, owned by the activity
 * (activity +0x268[i], count +0x26c). Driver: +0x10 track (race data), +0x14 / +0x18 / +0x1c section and segment nodes, +0x70
 * finished, +0x74 player, +0x78 place, +0x80 last position, +0x90 progress node, +0xa0 rules / +0xe8 behaviours, +0x130 clock,
 * +0x134 race clock, +0xee8 AI personality, +0xf1c laps done (-1 before the first crossing), +0xf20 lap time, +0xf24 lap times,
 * +0x10b0 time base, +0x10c4 best lap, +0x10cc distance to the line. (_re/notes/race-state.md, core/notes/race.md "Driver") */
#include "race.h"
#include "../src/fmt_modtrack.h"
#include "race_extern.h"
#include "race_driver.h"
#include "race_net_session.h"
#include "build_activity_freeride_run.h"
#include "race_beh1.h"
#include "world_node.h"
#include "practice.h"   /* PORT ADDITION: the lap log */

/* 00508b60 (EAX = float *) -> xmm0: the float. */
float rx_float_get(rx_addr p) { return F32(p, 0); }
RX_FUNCTION(0x00508b60, rx_float_get)
/* 00508b70 (EAX = object, xmm0 = value): object +4 = value. */
void rx_float_set4(rx_addr obj, float v) { SF32(obj, 4, v); }
RX_FUNCTION(0x00508b70, rx_float_set4)
/* 00508c00 (ECX = object, EAX = out): out = the object's (x, z) pair at +0x20 / +0x24 (x87 copies). */
void rx_copy_pair_20(rx_addr obj, rx_addr out) { SF32(out, 0, F32(obj, 0x20)); SF32(out, 4, F32(obj, 0x24)); }
RX_FUNCTION(0x00508c00, rx_copy_pair_20)
/* 00508a90 (EAX = object) -> EAX = object + 0xb58. */
rx_addr rx_field_b58(rx_addr obj) { return obj + 0xb58; }
RX_FUNCTION(0x00508a90, rx_field_b58)

/* 005089a0 DebugKeyMap::vf2 (ECX = key map, args a, b; RET 8) -> EAX: 0 while debug keys are off (byte 0x9b716d), else the
 * KeyMap lookup 008115f0 (not in the core). */
uint32_t rx_debug_keymap_get(rx_addr map, uint32_t a, uint32_t b) {
    if (U8(0, 0x9b716d) != 0) return 0;
    uint32_t r;
    RACE_CORE_CALL(2, r = rx_world_keymap_down(map, a, b));   /* KeyMap::vf2 008115f0 */
    return r;
}
RX_FUNCTION(0x005089a0, rx_debug_keymap_get)

/* 0050deb0 (ECX = a (unit), EDX = b, arg = out; cdecl) -> EAX = out: the projection of b onto a, a * dot(a, b). */
rx_addr rx_race_project_on(rx_addr a, rx_addr b, rx_addr out) {
    const float ax = F32(a, 0), ay = F32(a, 4), az = F32(a, 8);
    const float dot = (F32(b, 4) * ay + F32(b, 0) * ax) + F32(b, 8) * az;
    SF32(out, 0, ax * dot); SF32(out, 4, ay * dot); SF32(out, 8, az * dot);
    return out;
}
RX_FUNCTION(0x0050deb0, rx_race_project_on)

/* 00512620 (ECX = driver) -> xmm0: the distance left to the finish line along the progress node (+0x90): its remaining distance
 * (+0x4f4) minus the driver's position along its direction (+0x50). */
float rx_driver_distance_to_line(rx_addr d) {
    rx_addr n = U32(d, 0x90);
    const float dz = F32(d, 0x88) - F32(n, 8), dy = F32(d, 0x84) - F32(n, 4), dx = F32(d, 0x80) - F32(n, 0);
    return F32(n, 0x4f4) - ((F32(n, 0x58) * dz + F32(n, 0x54) * dy) + F32(n, 0x50) * dx);
}
RX_FUNCTION(0x00512620, rx_driver_distance_to_line)

/* 00512870 (ESI = driver) -> xmm0: the total race time: time base (+0x10b0) + the lap times (+0xf24[0 .. laps)), summed as the
 * original does (even and odd entries separately, then the last odd one out); with a session result (activity +0xfe8) the time
 * base + +0x10d8. */
float rx_driver_total_time(rx_addr d) {
    if (U32(U32(U32(d, 0x74), 0x94), 0xfe8) != 0) return F32(d, 0x10b0) + F32(d, 0x10d8);
    const int32_t n = I32(d, 0xf1c);
    float even = 0.0f, odd = 0.0f, last = 0.0f;
    int32_t used = 0;
    if (n >= 2) {
        uint32_t pairs = ((uint32_t)(n - 2) >> 1) + 1;
        used = (int32_t)(2 * pairs);
        for (uint32_t k = 0; k < pairs; k++) {
            even = F32(d, 0xf24 + 8 * k) + even;
            odd = F32(d, 0xf28 + 8 * k) + odd;
        }
    }
    if (used < n) last = F32(d, 0xf24 + 4 * (uint32_t)used);
    return F32(d, 0x10b0) + ((odd + even) + last);
}
RX_FUNCTION(0x00512870, rx_driver_total_time)

/* 00512c80 (ECX = driver, EAX = the riders left behind it; plain RET): the driver is eliminated (the "Lap Completed" handler of the
 * elimination mode, 00567660): +0x1120 = the riders left, the elimination time +0x1124 = the total time (00512870) + the penalty
 * +0xf20; a local human (player +4 set, +0x10 clear) runs the session static (006de1f0 behind its guard), any other driver has its
 * vehicle reset (0050abe0); then the "Player Eliminated" event (0053ac80) on the activity's channel (activity +0x290) with the
 * activity (player +0x74 -> +0x94), the driver and the activity's +0x2d0 member. Frame: push esi. */
void rx_driver_eliminate(rx_addr d, uint32_t remaining) {
    RACE_FRAME_PLAIN(F, 0, 1);
    SU32(d, 0x1120, remaining);
    float total;
    RACE_CORE_CALL(0, total = rx_driver_total_time(d));
    SF32(d, 0x1124, rx_adds(total, F32(d, 0xf20)));
    const rx_addr player = U32(d, 0x74);
    if (U32(player, 4) != 0 && U32(player, 0x10) == 0) race_static_dcf304();
    else RACE_CORE_CALL(0, rx_driver_reset_vehicle(d));
    const rx_addr activity = U32(U32(d, 0x74), 0x94);
    RACE_CORE_CALL(1, rx_event_player_eliminated(U32(activity, 0x290), activity, d, U32(activity, 0x2d0)));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00512c80, rx_driver_eliminate)

/* 00512910 (EAX = driver) -> xmm0: the race time for the standings: the frozen finish time (+0x1124) once the activity is over
 * (+0x238 == 5) and the driver has one (+0x1120 != -1), else the total time (00512870). */
float rx_driver_standings_time(rx_addr d) {
    if (U32(U32(U32(d, 0x74), 0x94), 0x238) == 5 && I32(d, 0x1120) != -1) return F32(d, 0x1124);
    return rx_driver_total_time(d);
}
RX_FUNCTION(0x00512910, rx_driver_standings_time)

/* 006de1f0: the session/stats static block 0xda6ca0 (the guarded constructor behind FUN_006de1c0): the stats root / session
 * pointer (+0, +4, +0xb0) 0, the std::string at 0xda6cbc, the replay/stats fields +0x40 .. +0x4c 0, the Quazal mutex object at
 * 0xda6cf0 (FUN_00883d40: a MutexPrimitive from the Quazal allocator, FUN_008a0220's fields), +0x64 .. 0, then 0xda6cb4's 0x2c
 * bytes cleared (over the string too). Returns 0xda6ca0 (EAX).
 *
 * Boundary: FUN_00883d40 is the Quazal/CRT allocator + critical section (MutexPrimitive.cpp), not core code. Oracle: the original
 * call (the emulator's kernel stand-ins). Product: no Quazal runtime, so the mutex slot stays 0 and the four deterministic fields
 * FUN_008a0220 writes are written directly (nothing in the core dereferences 0xda6cf0: the profiler scopes are unported). */
rx_addr rx_stats_static_construct(void) {
    SU32(0, 0x00da6ca0, 0);
    SU32(0, 0x00da6ca4, 0);
    SU32(0, 0x00da6cb0, 0);
    crt_str_ctor(0x00da6cbc);
    SU32(0, 0x00da6ce0, 0);
    SU32(0, 0x00da6ce4, 0);
    SU32(0, 0x00da6ce8, 0);
    SU32(0, 0x00da6cec, 0);
#ifdef RX_ORACLE
    { const uint32_t a[1] = {0x40000000}; rx_call_original(0x00883d40, RX_THISCALL, 0x00da6cf0, 1, a); }   /* FUN_00883d40 */
#else
    SU32(0, 0x00da6cf0, 0);                    /* boundary: the Quazal MutexPrimitive is not built */
    SU8(0, 0x00da6cf4, 0);
    SU32(0, 0x00da6cf8, 0x40000000);
    SU32(0, 0x00da6cfc, 0);
    SU32(0, 0x00da6d00, 0);
#endif
    SF32(0, 0x00da6d04, 0.0f);
    SU32(0, 0x00da6d08, 0);
    SU32(0, 0x00da6d0c, 0);
    SU32(0, 0x00da6d10, 0);
    SU32(0, 0x00da6d14, 0);
    crt_memset(0x00da6cb4, 0, 0x2c);
    return 0x00da6ca0;
}
RX_FUNCTION(0x006de1f0, rx_stats_static_construct)

/* The function-local static guarded by bit 0 of 0xdcf304 (an engine profiler/stat block: 006de1f0 + atexit 0x913e00). */
void race_static_dcf304(void) {
    if ((U8(0, 0xdcf304) & 1) != 0) return;
    SU32(0, 0xdcf304, U32(0, 0xdcf304) | 1);
    rx_stats_static_construct();
#ifdef RX_ORACLE
    uint32_t thunk = 0x00913e00;
    rx_call_original(0x008f9fbf, RX_CDECL, 0, 1, &thunk);   /* _atexit: the core never exits (build_globals_int.h's RX_ATEXIT) */
#endif
}

/* 00512690 (EAX = driver, arg = position; RET 4) -> xmm0: the driver's race progress in track units: (laps + 1) * lap length
 * (track +0x1dc) + track +0x1e0 minus the distance left to the line: from the segment holding the vehicle (005387c0 within the
 * track window both ways, up to the finish node track +0x9c), measured from the given position along that node's direction to
 * the next node, plus the next node's remaining distance; when the search finds nothing (or the stats gate 0xda6cb0 skips it)
 * the driver's own distance to the line (+0x10cc). */
float rx_driver_progress(rx_addr d, rx_addr pos) {
    rx_addr vehicle = U32(U32(d, 0x74), 0x1ac), track = U32(d, 0x10);
    const float window = F32(track, 0x1b8 + 4 * U32(vehicle, 0x194));
    race_track_query q = {vehicle + 0xc0, U32(d, 0x14), U32(d, 0x18), U32(track, 0x9c), window, window, 1, 1};
    race_static_dcf304();
    rx_addr gate = U32(0, 0xda6cb0), sec = 0, node = 0;
    if (U32(gate, 0x1c4c) == 0 || U32(gate, 0x1b0) == 1) {
        if (race_track_find(&q, 0, &sec, &node) != 0) {
            rx_addr nn = U32(node, 0x64);
            vehicle = U32(U32(d, 0x74), 0x1ac);
            const float dz = F32(vehicle, 0xc8) - F32(nn, 8), dx = F32(vehicle, 0xc0) - F32(nn, 0);
            if ((F32(nn, 0x58) * dz + F32(nn, 0x50) * dx) + F32(nn, 0x54) * 0.0f > 0.0f && nn == 0)
                nn = U32(U32(U32(sec, 8), 0), 0);
            track = U32(d, 0x10);
            const float along = ((F32(node, 0x58) * (F32(nn, 8) - F32(pos, 8)) + F32(node, 0x54) * (F32(nn, 4) - F32(pos, 4)))
                                 + (F32(nn, 0) - F32(pos, 0)) * F32(node, 0x50)) + F32(nn, 0x4f4);
            return ((float)(I32(d, 0xf1c) + 1) * F32(track, 0x1dc) + F32(track, 0x1e0)) - along;
        }
    }
    track = U32(d, 0x10);
    return ((float)(I32(d, 0xf1c) + 1) * F32(track, 0x1dc) + F32(track, 0x1e0)) - F32(d, 0x10cc);
}
RX_FUNCTION(0x00512690, rx_driver_progress)

/* Vehicle vf17 (slot 17, 004301f0): its position (x87 copies of +0xc0..+0xc8) into out. */
void race_vehicle_position(rx_addr vehicle, rx_addr out) {
    typedef void (*fn_t)(rx_addr, rx_addr);
    rx_addr fn = race_vfn(vehicle, 17);
    fn_t f = RX_FN(fn, fn_t);
    if (f) f(vehicle, out); else race_thiscall(fn, vehicle, 1, out, 0, 0, 0);
}

/* 00512d00 (ESI = driver a, EDI = driver b) -> EAX: has a lapped b (a's progress more than a lap length ahead of b's, b has
 * started (laps > -1) and a has done a lap)? */
int rx_driver_has_lapped(rx_addr a, rx_addr b) {
    RACE_FRAME16_ENTER(F, 0x20);
    race_vehicle_position(U32(U32(a, 0x74), 0x1ac), F);
    race_vehicle_position(U32(U32(b, 0x74), 0x1ac), F + 0x10);
    const float lap = F32(U32(a, 0x10), 0x1dc);
    const float pa = rx_driver_progress(a, F);
    const float pb = rx_driver_progress(b, F + 0x10) + lap;
    RACE_FRAME16_LEAVE(F);
    return pa > pb && I32(b, 0xf1c) > -1 && I32(a, 0xf1c) > 0;
}
RX_FUNCTION(0x00512d00, rx_driver_has_lapped)

/* ---- RaceDriver::vf9: the rules' events --------------------------------------------------------------------------------------- */

/* 00884950 (ECX = net object) -> AL: the network object is ours / valid (not in the core). */
static int net_valid(rx_addr net) {
    uint32_t r;
    RACE_CORE_CALL(0, r = rx_net_ref_valid(net));
    return (r & 0xff) != 0;
}
/* 0053aa40 (ESI = manager, arg = driver, xmm0 = time; RET 4): the "Best Lap" event void(BaseDriver*, float) (race_event.c). */
static void event_best_lap(rx_addr manager, rx_addr driver, float t) {
    RACE_CORE_CALL(1, rx_event_best_lap(manager, driver, t));
}
/* The network session's lap report (00884950, 0050bc70; 006fe870, 006fe9c0, 006ff8d0 in race_net_session.c, whose Quazal leaves are the online boundary). */
static void net_report_lap(rx_addr d) {
    rx_addr net = U32(d, 0xef0);
    if (net == 0 || (U8(net, 0x20) & 1) == 0 || !net_valid(net)) return;
    if (U32(U32(U32(d, 0xef0), 0x1d4), 0) == 0) RACE_CORE_CALL(1, rx_net_bind_player(U32(d, 0xef0), U32(d, 0x74)));
    RACE_CORE_CALL(0, rx_net_player_apply(U32(d, 0xef0)));                              /* 006fe870 (EDI = the network object) */
    RACE_CORE_CALL(0, rx_net_player_set_flag(U32(d, 0xef0), 1));                        /* 006fe9c0 (EAX = the network object, CL = 1) */
    RACE_CORE_CALL(3, rx_net_send_event(U32(d, 0xef0), 1, U32(d, 0xf1c), U32(d, 0x10e0)));   /* 006ff8d0 (EDI = the network object; stdcall) */
}
/* 005256d0 (EAX = lap pacing behaviour, xmm1 = gap to the best human lap): race_beh1_pace.c. */
static void lap_pacing_gap(rx_addr lp, float gap) {
    RACE_CORE_CALL(0, rx_behavior_lap_pacing_gap(lp, gap));
}
static rx_addr event_manager(rx_addr d) { return U32(U32(U32(d, 0x74), 0x94), 0x290); }

/* A lap is complete (RuleFinishLine): counted only while the race runs (activity state 2), the vehicle is not wrecked, the network
 * object (if any) is valid and the off-track rule says the rider is on the track; its time with the off-track penalty may be the
 * best lap ("Best Lap"); the lap time goes into the lap table (the start "lap" -1 into the time base +0x10b0, shown 3 s: +0x10dc),
 * the lap counter (at most 0x61) raises "Lap Completed" (driver, laps, not racing, laps of the race) or, in a ghost session (state
 * 3), "Ghost Session Completed"; then the distance to the line, the finish (laps == the race's), the standings time, the network
 * report and the AI's lap pacing (the gap to the best human lap time +0x10b4). */
static void lap_completed(rx_addr d, rx_addr act0) {
    if (fmt_modtrack_racing() && U32(d, 0x70)) return;
    rx_addr player = U32(d, 0x74), act = U32(player, 0x94);
    if (U32(act, 0x21c) != 2 || U32(U32(player, 0x1ac), 0x280) != 0) return;
    if (U32(d, 0xef0) != 0 && !net_valid(U32(d, 0xef0))) return;
    rx_addr pen = rx_driver_offtrack_penalty_rule(d);
    float penalty = 0.0f;
    if (pen != 0) {
        /* Imported corridors meet at the finish with independently authored edges.
         * Their ordered course-sector check plus crossing between the posts is
         * authoritative here; a one-frame quad seam must not discard a full lap. */
        if (U32(pen, 0x10) == 0 && !fmt_modtrack_racing()) return;
        penalty = F32(pen, 0xd0);
    }
    rx_lap_log_note(d, penalty);   /* PORT ADDITION (game/practice.h): the lap log, before the lap's values are cleared (host statics only) */
    if (U32(d, 0x10f0) == 0 && U32(d, 0x70) == 0) {
        const float t = F32(d, 0xf20) + penalty;
        if (F32(d, 0x10c4) > t) {
            SF32(d, 0x10c4, t);
            event_best_lap(event_manager(d), d, t);
        }
    }
    const int32_t lap = I32(d, 0xf1c);
    if (lap > -1) {
        SF32(d, 0xf24 + 4 * (uint32_t)lap, F32(d, 0xf20));
        SF32(d, 0x10dc, F32(d, 0x130) + 3.0f /* 0x92fe48 */);
    } else {
        SF32(d, 0x10b0, F32(d, 0xf20));
    }
    SF32(d, 0x10e0, F32(d, 0xf20));
    SU32(d, 0x10f0, 0);
    SU32(d, 0x10f4, 0);
    SF32(d, 0xf20, 0.0f);
    pen = rx_driver_offtrack_penalty_rule(d);
    if (pen != 0) SF32(pen, 0xd0, 0.0f);
    act = U32(U32(d, 0x74), 0x94);
    if (U32(act, 0x23c) != 0xc) {
        if (U32(act, 0x238) == 3) {
            rx_event_activity_driver(U32(act, 0x290), 0x0093ee68 /* "Ghost Session Completed" */, act, d);
            SF32(d, 0x134, 0.0f);
        } else {
            uint32_t laps = U32(d, 0xf1c) + 1;
            if (!(laps < 0x61)) laps = 0x61;
            SU32(d, 0xf1c, laps);
            act = U32(U32(d, 0x74), 0x94);
            rx_event_driver_3int(U32(act, 0x290), 0x0093eefc /* "Lap Completed" */, d, laps, U32(act, 0x238) != 2,
                                 U32(act0, 0xff070));
        }
    }
    rx_addr v = U32(U32(d, 0x74), 0x1ac);
    SF32(d, 0x80, F32(v, 0xc0)); SF32(d, 0x84, F32(v, 0xc4)); SF32(d, 0x88, F32(v, 0xc8));
    SF32(d, 0x10cc, rx_driver_distance_to_line(d));
    if (U32(d, 0xf1c) == U32(act0, 0xff070)) { SU32(d, 0x70, 1); SF32(d, 0x10cc, 0.0f); }
    SF32(d, 0x10d8, rx_driver_standings_time(d));
    net_report_lap(d);
    rx_addr lp = rx_driver_lap_pacing_behavior(d);
    if (lp == 0) return;
    player = U32(d, 0x74);
    act = U32(player, 0x94);
    const float none = rx_bf(0x7f7fffff) /* 0x977494 */;
    float best = none;
    const int32_t n = I32(act, 0x26c);
    for (int32_t i = 0; i < n; i++) {
        rx_addr o = U32(U32(act, 0x268), 4 * (uint32_t)i);
        if (o == d || !rx_player_is_local_human(U32(o, 0x74)) || U32(o, 0xf1c) == 0) continue;
        if (best > F32(o, 0x10b4)) best = F32(o, 0x10b4);
    }
    if (!rx_player_is_local_human(player) && U32(d, 0xf1c) != 0 && best != none) lap_pacing_gap(lp, best - F32(d, 0x10b4));
    typedef void (*reset_fn)(rx_addr);
    rx_addr fn = race_vfn(lp, 6);
    reset_fn f = RX_FN(fn, reset_fn);
    if (f) f(lp); else race_thiscall(fn, lp, 0, 0, 0, 0, 0);
}

/* "Off Track" (driver, value) and "Hide Arrow" (driver). */
static void off_track_events(rx_addr d, uint32_t value) {
    RACE_CORE_CALL(1, rx_event_driver_int(event_manager(d), 0x0093ef24 /* "Off Track" */, d, value));   /* push ebp; call 0053a470 */
    RACE_CORE_CALL(1, rx_event_driver(event_manager(d), 0x0093eef0 /* "Hide Arrow" */, d));            /* push manager; call 0053a770 */
}

/* Rule vf3 (slot 3): the type id. */
static uint32_t rule_type(rx_addr r) {
    typedef uint32_t (*fn_t)(rx_addr);
    rx_addr fn = race_vfn(r, 3);
    fn_t f = RX_FN(fn, fn_t);
    return f ? f(r) : race_thiscall(fn, r, 0, 0, 0, 0, 0);
}

/* 00512140 RaceDriver::vf9 (ECX = driver, arg = the rule that raised it; RET 4): a rule's event, by the rule's type:
 * 0 RuleBackwards -> "Backwards" (driver, wrong way) unless wrecked; 1 RuleOfftrack -> "Off Track" + "Hide Arrow" while off
 * (+0x50), else the arrow time (+0x10d0 = clock + 1.5); 2 RuleTrackAdvance -> the AI controls off unless behaviour 5 holds them
 * (+0x58 == 2, +0x54), lap pacing's new segment (00525790); 4 RuleFinishLine -> lap completed; 0xb RuleOfftrackTimePenalty ->
 * "Off Track" + "Hide Arrow" while on track (+0x10), else the arrow time and both "no penalty" flags (+0x10d4, rule +0xd8). */
static void driver_on_rule(rx_addr d, rx_addr rule);
void rx_race_driver_on_rule(rx_addr d, rx_addr rule) {
    /* push ebp; push esi; push edi (the callees, the event raisers' handlers, run below them) */
    const rx_addr sp_in = rx_sp();
    rx_set_sp(sp_in - 12);
    driver_on_rule(d, rule);
    rx_set_sp(sp_in);
}
static void driver_on_rule(rx_addr d, rx_addr rule) {
    rx_addr act0 = U32(U32(d, 0x74), 0x94);
    switch (rule_type(rule)) {
    case 0:
        if (U32(U32(U32(d, 0x74), 0x1ac), 0x280) != 0) return;
        RACE_CORE_CALL(1, rx_event_driver_int(event_manager(d), 0x0093ef3c /* "Backwards" */, d, U32(rule, 0xc)));
        return;
    case 1:
        if (U32(rule, 0x50) != 0) { off_track_events(d, U32(rule, 0x50)); return; }
        SF32(d, 0x10d0, F32(d, 0x130) + 1.5f /* 0x9322a4 */);
        return;
    case 2: {
        rx_addr b5 = rx_driver_behavior_5(d);
        if (b5 == 0 || U32(b5, 0x58) != 2 || U32(b5, 0x54) == 0) rx_ai_controls_off(d);
        rx_addr lp = rx_driver_lap_pacing_behavior(d);
        if (lp != 0) rx_behavior_lap_pacing_new_lap(lp);
        return;
    }
    case 4:
        lap_completed(d, act0);
        return;
    case 0xb: {
        rx_addr pen = rx_driver_offtrack_penalty_rule(d);
        if (U32(pen, 0x10) != 0) { off_track_events(d, U32(pen, 0x10)); return; }
        SF32(d, 0x10d0, F32(d, 0x130) + 1.5f);
        SU8(d, 0x10d4, 1);
        SU8(pen, 0xd8, 1);
        return;
    }
    default:
        return;
    }
}
RX_FUNCTION(0x00512140, rx_race_driver_on_rule)
