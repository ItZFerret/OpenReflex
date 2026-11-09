/* race_beh1_small.c -- the small AI behaviours and the state dispatchers of the bigger ones:
 * BehaviorAvoidWreck (0x940220), BehaviorOfftrack (0x9401d0), BehaviorBrakeTurn (0x940050), BehaviorBlueYellowFlag (0x940248),
 * BehaviorMonitorCorner (0x93f9c8), BehaviorNavigateTrack (0x93f8b0), BehaviorOverrideSpline (0x93f928).
 * Behaviour vf4 (thiscall, stack dt; RET 4) runs one frame and returns nonzero when the behaviour takes control of the rider (the
 * driver then skips the lower-priority behaviours); vf5 returns the behaviour's type id (the driver finds behaviours by it);
 * vf6 resets its state. (core/notes/race.md "AI behaviours") */
#include "race_beh1_extern.h"
#include "race_braketurn.h"

/* ---------------------------------------------------------------------------------------------------- BehaviorAvoidWreck */
/* +0x0c time since the decision, +0x10 reaction delay (s), +0x14 decided, +0x18 will react. Reacts to the player's wreck warning
 * (player +0xb44: the clock direction of a wreck ahead, 0 = none): once per warning a roll (rand % 100 < the tuning's chance,
 * 0051e090) decides whether it reacts and after which delay (rand % the tuning's max delay in ms, 0051e0e0); after the delay the
 * avoidance input player +0xb50 (steer: -1 / +1 for wrecks at 8-9 / 11-12 o'clock) / +0xb54 (+1 / -1 for 2-3 / 5-6 o'clock) is set
 * for the frame and the behaviour resets. It never takes control. */

/* 005223b0 BehaviorAvoidWreck::vf5: the type id 0x1a. */
uint32_t rx_behavior_avoid_wreck_type(rx_addr self) { (void)self; return 0x1a; }
RX_FUNCTION(0x005223b0, rx_behavior_avoid_wreck_type)

/* 00522390 BehaviorAvoidWreck::vf6 (ECX = behaviour): reset. */
void rx_behavior_avoid_wreck_reset(rx_addr b) {
    SF32(b, 0xc, 0.0f);
    SU32(b, 0x18, 0);
    SU32(b, 0x14, 0);
}
RX_FUNCTION(0x00522390, rx_behavior_avoid_wreck_reset)

/* 005223e0 BehaviorAvoidWreck::vf4 (ECX = behaviour, stack dt; RET 4) -> 0. */
uint32_t rx_behavior_avoid_wreck_update(rx_addr b, float dt) {
    float steer = 0.0f, gas = 0.0f;                       /* -> player +0xb50 / +0xb54 */
    if (U32(b1_player(b1_driver(b)), 0xb44) == 0) {
        b1_behavior_reset(b);
    } else {
        if (U32(b, 0x14) == 0) {
            float roll = (float)((int32_t)race_rand() % 100);
            rx_addr drv = b1_driver(b);
            if (rx_ai_param_e090(drv) > roll) {
                SU32(b, 0x18, 1);
                int32_t max_ms = rx_cvtt(rx_ai_param_e0e0(drv));
                if (max_ms != 0) SF32(b, 0x10, (float)((int32_t)race_rand() % max_ms) * 0.001f /* 0x97743c */);
            } else {
                SU32(b, 0x18, 0);
            }
            SU32(b, 0x14, 1);
        }
        if (U32(b, 0x18) == 0) goto out;
        float t = F32(b, 0xc) + dt;
        SF32(b, 0xc, t);
        if (!(t > F32(b, 0x10))) goto out;
        switch (U32(b1_player(b1_driver(b)), 0xb44)) {
        case 2: case 3: gas = 1.0f; break;
        case 5: case 6: gas = b1_data(0x009cd9e8) /* -1 */; break;
        case 8: case 9: steer = b1_data(0x009cd9e8); break;
        case 0xb: case 0xc: steer = 1.0f; break;
        default: break;
        }
        b1_behavior_reset(b);
    }
out:;
    rx_addr p = b1_player(b1_driver(b));
    SF32(p, 0xb50, steer);
    SF32(p, 0xb54, gas);
    return 0;
}
RX_FUNCTION(0x005223e0, rx_behavior_avoid_wreck_update)

/* ------------------------------------------------------------------------------------------------------ BehaviorOfftrack */
/* +0x10 / +0x14 / +0x18 the driver's track position (+0x14 track, +0x18 previous node, +0x1c next node) when the rider left the
 * track, +0x1c the RuleTrackAdvance (+0x50: on track), +0x20 the gate rule (+0x10: on track), +0x30 state (0 watching, 1 going
 * back (00529190), 2..4 returning to the track (00529330, takes control)). AI riders only (player +0x10 != 0). */

/* 00529140 BehaviorOfftrack::vf5 (shared by 4 classes): the type id 7. */
uint32_t rx_behavior_offtrack_type(rx_addr self) { (void)self; return 7; }
RX_FUNCTION(0x00529140, rx_behavior_offtrack_type)

/* 00529150 (EAX = behaviour): watching: when the rule says the rider is off the track, remember where (the driver's track, previous
 * and next node) and go to state 1. */
void rx_behavior_offtrack_watch(rx_addr b) {
    uint32_t on;
    rx_addr r = U32(b, 0x1c);
    if (r != 0) on = U32(r, 0x50);
    else {
        r = U32(b, 0x20);
        if (r == 0) return;
        on = U32(r, 0x10);
    }
    if (on != 0) return;
    rx_addr drv = b1_driver(b);
    SU32(b, 0x10, U32(drv, 0x14));
    SU32(b, 0x14, U32(drv, 0x18));
    SU32(b, 0x30, 1);
    SU32(b, 0x18, U32(drv, 0x1c));
}
RX_FUNCTION(0x00529150, rx_behavior_offtrack_watch)

/* RavenVehicle::vf14 (slot 14, 00430240 = &+0xf0): a pointer to the vehicle's world matrix (rows x, y, z, position). */
typedef rx_addr (*b1_matrix_ptr_fn)(rx_addr self);
static rx_addr b1_vehicle_matrix_ptr(rx_addr veh) {
    rx_addr fn = race_vfn(veh, 14);
    b1_matrix_ptr_fn f = RX_FN(fn, b1_matrix_ptr_fn);
    return f ? f(veh) : B1_ORIG0(fn, RX_THISCALL, veh);
}

/* 00529190 (ESI = behaviour): state 1, going back: aim at the return point. The vehicle's horizontal heading (+0x130 / +0x138,
 * normalized) against the direction to the point: the track rule's when the behaviour has one (+0x1c, its point at +0x10 + 0x30 /
 * +0x38), else the gate rule's (+0x20 + 0x90); the point's x / z go to +0x28 / +0x2c (+0x2c is the original's whole dword store
 * of the point's z). Heading toward it (dot >= 0.2, 0x92e388; comiss / jbe, so unordered counts as toward) -> state 3, else 2
 * (00529330 takes over). With neither rule the original reads its own scratch slot [esp+4] (the heading length^2 word) as the
 * point's address: watch() (00529150) only enters state 1 with one of the two, so it is unreachable; kept as the original has it.
 * Frame: push ebp; and esp, -16; sub esp, 0x20 (no callee keeps a pointer into it: x / z of the vehicle at +0x10 / +0x14, the
 * SqrtF argument at +4). */
void rx_behavior_offtrack_aim(rx_addr b) {
    const rx_addr veh = b1_vehicle(b1_driver(b));
    const float vx = F32(veh, 0xc0), vz = F32(veh, 0xc8);
    float hx = F32(veh, 0x130), hz = F32(veh, 0x138);
    const float l2 = hz * hz + hx * hx;
    if (l2 != 0.0f) {
        const float inv = 1.0f /* 0x931f24 */ / sqrtf(l2);
        hx = inv * hx;
        hz = hz * inv;
    }
    rx_addr src = U32(b, 0x1c);
    if (src != 0) src += 0x10;
    else if ((src = U32(b, 0x20)) != 0) src += 0x90;
    else src = (rx_addr)rx_fb(l2);            /* [esp + 4]: the heading length^2 word read as an address (unreachable, see above) */
    const float tx = F32(src, 0x30);
    SU32(b, 0x2c, U32(src, 0x38));
    SF32(b, 0x28, tx);
    const float ax = tx - vx, az = F32(b, 0x2c) - vz;
    const float d2 = az * az + ax * ax;
    float nx = ax, nz = az;
    if (d2 != 0.0f) {
        const float inv = 1.0f /* 0x931f24 */ / sqrtf(d2);
        nx = ax * inv;
        nz = az * inv;
    }
    const float dot = nz * hz + nx * hx;
    SU32(b, 0x30, !(0.2f /* 0x92e388 */ > dot) ? 3u : 2u);
}
RX_FUNCTION(0x00529190, rx_behavior_offtrack_aim)

/* 00529330 (ECX = behaviour, stack behaviour; RET 4): states 2..4, the way back (takes control). First the node run: the next node
 * (+0x18) is dropped when the vehicle is past it (the node's direction +0x50..0x58 against vehicle - node > 0) and the section
 * (+0x10) advances when the run runs out. The aim point: state 3 keeps the left-the-track point but re-checks the heading (dot
 * < 0.2 -> state 2), state 2 / 4 take the track's look-ahead point (0050a650 / 0050a5f0, the AI tuning distance 0xa04b38); state
 * 3 / 4 keep the look-ahead direction as the aim, state 2 gets it from the xz variant.
 * Then the steering: the offset from the look-ahead point (the AI records 0x9e3468: (c / 1^a) - (b / 1^d), x / z normalized and
 * scaled) rotated into the vehicle frame (005084f0), and with the aim direction not mostly ahead (dot < 0.98) or the vehicle
 * more than 3 units off the line of its heading (the matrix's x row), the driver's steer bias +0x60 += the rotated x x the
 * heading factor (0050b150) and, unless airborne (vf43), +0x64 = 0.3 above 58.6664 speed else 0.
 * Finally, back on the track (the rule's +0x50 / +0x10) -> state 0 and the behaviour's +0x0c vf6.
 * Frame: push ebp; and esp, -16; sub esp, 0x44; push ebx / esi / edi (the heading at +0x18, the vehicle at +0x20, the aim at
 * +0x28, the look-ahead direction at +0x30, its point / the rotated vector at +0x40). */
void rx_behavior_offtrack_return(rx_addr b) {
    RACE_FRAME_EBP(F, 16, 0x44, 3);
    const rx_addr drv = U32(b, 8);
    rx_addr node = U32(b, 0x18);
    {
        const rx_addr veh = b1_vehicle(drv);
        float d = F32(node, 0x58) * (F32(veh, 0xc8) - F32(node, 8));
        d = d + F32(node, 0x50) * (F32(veh, 0xc0) - F32(node, 0));
        d = d + F32(node, 0x54) * 0.0f;
        if (d > 0.0f) {                       /* comiss / jbe: unordered (NaN) does not advance */
            SU32(b, 0x14, node);
            SU32(b, 0x18, U32(node, 0x64));
        }
    }
    if (U32(b, 0x18) == 0) {
        const rx_addr sec = U32(U32(U32(b, 0x10), 8), 0);
        SU32(b, 0x10, sec);
        SU32(b, 0x14, U32(sec, 0));
        SU32(b, 0x18, U32(U32(sec, 0), 0x64));
    }
    {
        const rx_addr veh = b1_vehicle(drv);
        SF32(F, 0x20, F32(veh, 0xc0));
        SF32(F, 0x24, F32(veh, 0xc8));
        float hx = F32(veh, 0x130), hz = F32(veh, 0x138);
        const float l2 = hz * hz + hx * hx;
        SF32(F, 0x18, hx);
        SF32(F, 0x1c, hz);
        if (l2 != 0.0f) {
            const float inv = 1.0f /* 0x931f24 */ / sqrtf(l2);
            hx = inv * hx;
            hz = hz * inv;
            SF32(F, 0x18, hx);
            SF32(F, 0x1c, hz);
        }
    }
    const float dist = b1_tuning(0xa04b38, drv);
    const uint32_t state = U32(b, 0x30);
    if (state == 3) {
        float ax = F32(b, 0x28) - F32(F, 0x20);
        float az = F32(b, 0x2c) - F32(F, 0x24);
        const float d2 = az * az + ax * ax;
        SF32(F, 0x28, ax);                    /* the locals keep the unnormalized delta (the original does not store the normalized one) */
        SF32(F, 0x2c, az);
        if (d2 != 0.0f) {
            const float inv = 1.0f /* 0x931f24 */ / sqrtf(d2);
            ax = ax * inv;
            az = az * inv;
        }
        const float dot = az * F32(F, 0x1c) + ax * F32(F, 0x18);
        if (!(0.2f /* 0x92e388 */ > dot)) goto lab;   /* comiss / jbe: unordered keeps state 3 */
        SU32(b, 0x30, 2);
    }
    if (state == 3 || state == 4) {
        RACE_CORE_CALL(2, rx_track_look_ahead(U32(b, 0x18), F + 0x40, F + 0x30, dist));
        SF32(b, 0x28, F32(F, 0x40));
        SF32(b, 0x2c, F32(F, 0x48));
        SF32(F, 0x28, F32(F, 0x30));
        SF32(F, 0x2c, F32(F, 0x38));
    } else if (state == 2) {
        RACE_CORE_CALL(3, rx_track_look_ahead_xz(U32(b, 0x18), b + 0x28, dist, F + 0x28));
    }
lab:;
    const rx_addr rec = 0x9e3468u + 16u * b1_tuning_index(drv);
    const float t1 = F32(rec, 8) / (float)race_pow(1.0 /* 0x977638 */, (double)F32(rec, 0xc));
    const float t2 = F32(rec, 0) / (float)race_pow(1.0, (double)F32(rec, 4));
    const float f14 = t1 - t2;
    float dx = F32(F, 0x20) - F32(b, 0x28), dz = F32(F, 0x24) - F32(b, 0x2c);
    const float d2 = dx * dx + dz * dz;
    SF32(F, 0x14, f14);
    SF32(F, 0x30, dx);
    SF32(F, 0x34, dz);
    if (d2 != 0.0f) {
        const float inv = 1.0f /* 0x931f24 */ / sqrtf(d2);
        dx = dx * inv;
        dz = dz * inv;
    }
    SF32(F, 0x40, dx * f14);
    SF32(F, 0x44, 0.0f);
    SF32(F, 0x48, dz * f14);
    RACE_CORE_CALL(0, rx_race_to_vehicle_frame(b1_vehicle(drv), F + 0x40));
    {
        const rx_addr M = b1_vehicle_matrix_ptr(b1_vehicle(drv));
        float mx = F32(M, 0), mz = F32(M, 8);
        const float l2 = mz * mz + mx * mx;
        SF32(F, 0x30, mx);
        SF32(F, 0x34, mz);
        if (l2 != 0.0f) {
            const float inv = 1.0f /* 0x931f24 */ / sqrtf(l2);
            mx = mx * inv;
            mz = mz * inv;
        }
        const float aimdot = F32(F, 0x2c) * F32(F, 0x1c) + F32(F, 0x28) * F32(F, 0x18);
        if (!(0.98f /* 0x9cd9bc */ > aimdot)) {
            const float lat = fabsf((F32(b, 0x2c) - F32(F, 0x24)) * mz + (F32(b, 0x28) - F32(F, 0x20)) * mx);
            if (!(lat > 3.0f /* 0x92fe48 */)) goto done;   /* comiss / jbe: not above 3 (or NaN) */
        }
        const float hf = rx_race_heading_factor(b + 0x28, F + 0x20, F + 0x18);
        if (b1_vehicle_airborne(b1_vehicle(drv)) == 0) {
            const rx_addr d = U32(b, 8);
            SF32(d, 0x60, F32(F, 0x40) * hf + F32(d, 0x60));
            SF32(d, 0x64, F32(b1_vehicle(d), 0x150) > F32(0, 0x9d78a4) ? F32(0, 0x931da4) : 0.0f);
        }
    }
done:;
    {
        const rx_addr r1 = U32(b, 0x1c);
        uint32_t on;
        if (r1 != 0) on = U32(r1, 0x50);
        else {
            const rx_addr r2 = U32(b, 0x20);
            if (r2 == 0) { RACE_FRAME_END(F); return; }
            on = U32(r2, 0x10);
        }
        if (on == 0) { RACE_FRAME_END(F); return; }   /* still off the track */
    }
    SU32(b, 0x30, 0);
    if (U32(b, 0xc) != 0) b1_behavior_reset(U32(b, 0xc));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00529330, rx_behavior_offtrack_return)

/* 005298b0 BehaviorOfftrack::vf4 (ECX = behaviour, stack dt; RET 4) -> 1 while bringing the rider back (states 2..4). */
uint32_t rx_behavior_offtrack_update(rx_addr b, float dt) {
    (void)dt;
    if (U32(b1_player(b1_driver(b)), 0x10) == 0) return 0;
    switch (U32(b, 0x30)) {
    case 0: rx_behavior_offtrack_watch(b); return 0;
    case 1: rx_behavior_offtrack_aim(b); return 0;
    case 2: case 3: case 4: rx_behavior_offtrack_return(b); return 1;
    default: return 0;
    }
}
RX_FUNCTION(0x005298b0, rx_behavior_offtrack_update)

/* -------------------------------------------------------------------------- the on-track check of BrakeTurn / MonitorCorner */
/* Off (reset) while the rider is wrecking (vehicle +0x280) or off the track: RuleTrackAdvance +0x50 == 0, or without that rule the
 * gate rule's +0x10 == 0. Returns 1 when the behaviour may run. */
static int b1_on_track_or_reset(rx_addr b, rx_addr* rule1) {
    if (U32(b1_vehicle(b1_driver(b)), 0x280) != 0) { b1_behavior_reset(b); return 0; }
    rx_addr r1 = rx_driver_rule_1(b1_driver(b));
    rx_addr rb = rx_driver_offtrack_penalty_rule(b1_driver(b));
    *rule1 = r1;
    if (r1 != 0) {
        if (U32(r1, 0x50) == 0) { b1_behavior_reset(b); return 0; }
    } else if (rb != 0 && U32(rb, 0x10) == 0) { b1_behavior_reset(b); return 0; }
    return 1;
}

/* ----------------------------------------------------------------------------------------------------- BehaviorBrakeTurn */
/* +0x10 state (1..5 handled by 00522fc0 / 00523230 / 005232a0 / 00523300; 2 = wait until the driver clock passes +0x14). */

/* 00522ef0 BehaviorBrakeTurn::vf4 (ECX = behaviour, stack dt; RET 4) -> the state handler's result. */
uint32_t rx_behavior_brake_turn_update(rx_addr b, float dt) {
    (void)dt;
    rx_addr r1;
    if (!b1_on_track_or_reset(b, &r1)) return 0;
    switch (U32(b, 0x10)) {
    case 1: return rx_brake_turn_steer(b);
    case 2:
        if (F32(b1_driver(b), 0x130) > F32(b, 0x14)) SU32(b, 0x10, 3);
        return 0;
    case 3: return rx_brake_turn_clutch(b);
    case 4: return rx_brake_turn_wait_pullback(b);
    case 5: return rx_brake_turn_pullback(b);
    default: return 0;
    }
}
RX_FUNCTION(0x00522ef0, rx_behavior_brake_turn_update)

/* ------------------------------------------------------------------------------------------------ BehaviorBlueYellowFlag */
/* +0x0c state (0 looking, 1 moving aside: 00522a70), +0x10 the side to move to (-1 / 1), +0x14 the driver being let past.
 * Blue flag: a rider with more laps closing from behind within 40 units is let past. */

/* 00522640 (stack behaviour; RET 4): look for a rider about to lap this one: another, not wrecking, behind the rider in its frame
 * (z > 0 in the vehicle frame), the rider moving away from it (velocity . unit(me - it) >= 0.86, the largest such), nearer than 40
 * (1600 squared) and with more laps (+0xf1c). A new one (+0x14 changes): state 1 and the side to move to: away from the track edge
 * (+0x10 / +0x20 edge points of its previous -> next node) nearer to it (-1 when the +0x20 edge is nearer). */
void rx_behavior_blue_flag_find(rx_addr b) {
    const rx_addr old = U32(b, 0x14);
    const int32_t n = I32(U32(b1_player(b1_driver(b)), 0x94), 0x26c);
    float best = 0.0f;
    for (int32_t i = 0; i < n; i++) {
        rx_addr me = b1_driver(b);
        rx_addr other = U32(U32(U32(b1_player(me), 0x94), 0x268), 4 * (uint32_t)i);
        if (me == other) continue;
        rx_addr ov = b1_vehicle(other);
        if (U32(ov, 0x280) != 0) continue;
        rx_addr mv = b1_vehicle(me);
        RX_FRAME_ENTER(F, 0x10);
        SF32(F, 0, F32(mv, 0xc0) - F32(ov, 0xc0));
        SF32(F, 4, F32(mv, 0xc4) - F32(ov, 0xc4));
        SF32(F, 8, F32(mv, 0xc8) - F32(ov, 0xc8));
        rx_race_to_vehicle_frame(mv, F);
        float fz = F32(F, 8);
        RX_FRAME_LEAVE(F, 0x10);
        if (!(fz > 0.0f)) continue;
        mv = b1_vehicle(b1_driver(b));
        ov = b1_vehicle(other);
        float dy = F32(mv, 0xc4) - F32(ov, 0xc4);
        float dz = F32(mv, 0xc8) - F32(ov, 0xc8);
        float dx = F32(mv, 0xc0) - F32(ov, 0xc0);
        float len2 = (dz * dz + dy * dy) + dx * dx;
        if (len2 > 0.0f) {
            float inv = 1.0f / sqrtf(len2);
            dz = dz * inv; dx = dx * inv; dy = dy * inv;
        }
        float dot = (F32(mv, 0x134) * dy + F32(mv, 0x138) * dz) + F32(mv, 0x130) * dx;
        if (!(dot >= 0.86f /* 0x9773bc */)) continue;
        if (!(dot > best)) continue;
        if (!(1600.0f /* 0x9779b4 */ > len2)) continue;
        if (!(I32(other, 0xf1c) > I32(b1_driver(b), 0xf1c))) continue;
        best = dot;
        SU32(b, 0x14, other);           /* (then 007e3be0, an empty debug hook, with sqrt(len2)) */
    }
    rx_addr t = U32(b, 0x14);
    if (t == old) return;
    SU32(b, 0xc, 1);
    rx_addr next = U32(t, 0x1c), prev = U32(t, 0x18), tv = b1_vehicle(t);
    /* nearest point of the +0x10 edge */
    float ax = F32(prev, 0x10), az = F32(prev, 0x18);
    float ex = F32(next, 0x10) - ax, ez = F32(next, 0x18) - az;
    float u = ((F32(tv, 0xc8) - az) * ez + (F32(tv, 0xc0) - ax) * ex) / (ez * ez + ex * ex);
    if (1.0f > u && 0.0f > u) u = 0.0f;
    else if (!(1.0f > u)) u = 1.0f;
    tv = b1_vehicle(t);
    float bx = F32(prev, 0x20), bz = F32(prev, 0x28);
    float lx = (ex * u + ax) - F32(tv, 0xc0);
    float lz = (ez * u + az) - F32(tv, 0xc8);
    float ly = -0.0f - F32(tv, 0xc4);
    float d_left = (lz * lz + ly * ly) + lx * lx;
    /* nearest point of the +0x20 edge */
    float fx = F32(next, 0x20) - bx, fz = F32(next, 0x28) - bz;
    float w = ((F32(tv, 0xc8) - bz) * fz + (F32(tv, 0xc0) - bx) * fx) / (fz * fz + fx * fx);
    if (1.0f > w && 0.0f > w) w = 0.0f;
    else if (!(1.0f > w)) w = 1.0f;
    float ry = -0.0f - F32(tv, 0xc4);
    float rz = (fz * w + bz) - F32(tv, 0xc8);
    float rx_ = (fx * w + bx) - F32(tv, 0xc0);
    float d_right = (rz * rz + ry * ry) + rx_ * rx_;
    SU32(b, 0x10, 0);
    SU32(b, 0x10, d_left > d_right ? 0xffffffffu : 1u);
}
RX_FUNCTION(0x00522640, rx_behavior_blue_flag_find)

/* 00522a70 (ESI = behaviour): state 1, moving aside for the rider at +0x14. The steering bias (driver +0x60) by -0.75 when +0x10 is
 * -1 and +0.75 when it is 1 (0x935aa8; any other side writes nothing), then (007e3be0, an empty debug hook) and the throttle
 * (driver +0x3c0) - 0.2 (0x92e388). Then the direction from this rider's vehicle to the one being let past (x / z, normalized while
 * the squared distance is above 0; the y term is the heading's +0x134 x 0, 0x92e12c): the behaviour resets (+0xc = 0, +0x14 = 0)
 * when the squared distance is above 2025 (45 units, 0x9779b0) or the rider's heading (vehicle +0x130 / +0x138) points at it by
 * more than 0.707 (0x9cd99c; comiss / jbe, so unordered keeps the state). */
void rx_behavior_blue_flag_move(rx_addr b) {
    rx_addr drv = U32(b, 8);
    const uint32_t side = U32(b, 0x10);
    if (side == 0xffffffffu) {
        SF32(drv, 0x60, F32(drv, 0x60) - 0.75f /* 0x935aa8 */);
        /* 007e3be0: an empty function */
    } else if (side == 1) {
        SF32(drv, 0x60, F32(drv, 0x60) + 0.75f /* 0x935aa8 */);
        /* 007e3be0: an empty function */
    }
    SF32(drv, 0x3c0, F32(drv, 0x3c0) - 0.2f /* 0x92e388 */);
    const rx_addr me = b1_vehicle(drv);
    const rx_addr tv = b1_vehicle(U32(b, 0x14));
    const float dx = F32(tv, 0xc0) - F32(me, 0xc0);
    float dz = F32(tv, 0xc8) - F32(me, 0xc8);
    const float d2 = dx * dx + dz * dz;
    float nx = dx, nz = dz;
    const float ny = 0.0f;
    if (d2 > 0.0f) {
        const float inv = 1.0f /* 0x931f24 */ / sqrtf(d2);
        nx = dx * inv;
        nz = dz * inv;
    }
    if (d2 > 2025.0f /* 0x9779b0 */) {
        SU32(b, 0xc, 0);
        SU32(b, 0x14, 0);
        return;
    }
    const float dot = (F32(me, 0x138) * nz + F32(me, 0x134) * ny) + F32(me, 0x130) * nx;
    if (dot > 0.707f /* 0x9cd99c */) {
        SU32(b, 0xc, 0);
        SU32(b, 0x14, 0);
    }
}
RX_FUNCTION(0x00522a70, rx_behavior_blue_flag_move)

/* 005225c0 BehaviorBlueYellowFlag::vf4 (ECX = behaviour, stack dt; RET 4) -> 0. Only while the rider is upright (the up vector's y,
 * vehicle +0x144, at least 0.97 in double): state 0 looks for a rider to let past, state 1 moves aside (00522a70). */
uint32_t rx_behavior_blue_flag_update(rx_addr b, float dt) {
    (void)dt;
    rx_addr veh = b1_vehicle(b1_driver(b));
    float up_y = (F32(veh, 0x148) + F32(veh, 0x140)) * 0.0f /* 0x92e12c */ + F32(veh, 0x144);
    if (0.97 /* 0x9779b8 */ > (double)up_y) return 0;
    uint32_t state = U32(b, 0xc);
    if (state == 0) rx_behavior_blue_flag_find(b);
    else if (state == 1) {
        RACE_CALL_SP(0, rx_behavior_blue_flag_move(b));
    }
    return 0;
}
RX_FUNCTION(0x005225c0, rx_behavior_blue_flag_update)

/* ------------------------------------------------------------------------------------------------- BehaviorMonitorCorner */
/* 00526970 BehaviorMonitorCorner::vf4 (EDI <- ECX = behaviour, stack dt; RET 4) -> 0: by state +0x0c: 0 = watch for a corner
 * ahead (00526a30), 1 / 2 = 00526ab0 / 00526b00. Reset while wrecking or off the track. */
uint32_t rx_behavior_monitor_corner_update(rx_addr b, float dt) {
    (void)dt;
    if (U32(b, 4) == 0) return 0;
    rx_addr r1;
    if (!b1_on_track_or_reset(b, &r1)) return 0;
    switch (U32(b, 0xc)) {
    case 0: rx_behavior_monitor_corner_watch(b); break;
    case 1: rx_behavior_monitor_corner_wait(b); break;
    case 2: rx_behavior_monitor_corner_hold(b); break;
    default: break;
    }
    return 0;
}
RX_FUNCTION(0x00526970, rx_behavior_monitor_corner_update)

/* ------------------------------------------------------------------------------------------------- BehaviorNavigateTrack */
/* 00527300 BehaviorNavigateTrack::vf4 (ECX = behaviour, stack dt; RET 4) -> 0: clears the driver's +0x68, then (when enabled) by
 * the mode of the object at +0x34 (+0x0c; none = 0): 0 = follow the racing line (00527360), 1 / 2 = 005278e0 / 00527fc0. */
uint32_t rx_behavior_navigate_track_update(rx_addr b, float dt) {
    (void)dt;
    SF32(b1_driver(b), 0x68, 0.0f);
    if (U32(b, 4) == 0) return 0;
    rx_addr mode = U32(b, 0x34);
    uint32_t m = mode != 0 ? U32(mode, 0xc) : 0;
    if (m == 0) rx_behavior_navigate_follow(b);
    else if (m == 1) RACE_CORE_CALL(1, rx_behavior_navigate_line(b));
    else if (m == 2) RACE_CORE_CALL(1, rx_behavior_navigate_apex(b));
    return 0;
}
RX_FUNCTION(0x00527300, rx_behavior_navigate_track_update)

/* ------------------------------------------------------------------------------------------------ BehaviorOverrideSpline */
/* 00529bf0 BehaviorOverrideSpline::vf4 (ECX = behaviour, stack dt; RET 4) -> the state's result: by state +0x58: 0 = 00529c40,
 * 1 = 00529ce0, 2 = 00529eb0. */
uint32_t rx_behavior_override_spline_update(rx_addr b, float dt) {
    (void)dt;
    if (U32(b, 4) == 0) return 0;
    switch (U32(b, 0x58)) {
    case 0: return rx_behavior_override_spline_wait(b);
    case 1: return rx_behavior_override_spline_enter(b);
    case 2: return rx_behavior_override_spline_follow(b);
    default: return 0;
    }
}
RX_FUNCTION(0x00529bf0, rx_behavior_override_spline_update)

/* 00529ba0 BehaviorOverrideSpline::vf6 (ECX = behaviour): reset: the state and spline progress fields cleared, the sub-objects
 * +0x60 / +0x64 (if any) and +0x5c told vf7(1). */
typedef void (*b1_void_u32_fn)(rx_addr self, uint32_t a);
static void b1_vf7(rx_addr obj, uint32_t a) {
    rx_addr fn = race_vfn(obj, 7);
    b1_void_u32_fn f = RX_FN(fn, b1_void_u32_fn);
    if (f) f(obj, a); else B1_ORIG(fn, RX_THISCALL, obj, a);
}
void rx_behavior_override_spline_reset(rx_addr b) {
    SU32(b, 0x38, 0); SU32(b, 0x3c, 0); SU32(b, 0x40, 0); SU32(b, 0x50, 0);
    SU32(b, 0x44, 0); SU32(b, 0x54, 0); SU32(b, 0x58, 0); SU32(b, 0x34, 0);
    if (U32(b, 0x60) != 0) b1_vf7(U32(b, 0x60), 1);
    if (U32(b, 0x64) != 0) b1_vf7(U32(b, 0x64), 1);
    b1_vf7(U32(b, 0x5c), 1);
}
RX_FUNCTION(0x00529ba0, rx_behavior_override_spline_reset)

/* 00526b40 (EAX = node, xmm0 = distance, ESI = behaviour) -> EAX: the first node within `distance` of track ahead (each node's
 * segment length counted before its test) flagged as a corner for the rider's vehicle class (node +0x68 + 4 (4 class + kind) & 0x200,
 * the class / kind at vehicle +0x194 / +0x198), or 0. */
rx_addr rx_behavior_corner_find(rx_addr node, float dist, rx_addr b) {
    if (node == 0) return 0;
    while (dist > 0.0f) {
        rx_addr next = U32(node, 0x64);
        dist = dist - F32(node, 0x4f0);
        if (next != 0) {
            rx_addr veh = b1_vehicle(b1_driver(b));
            uint32_t k = U32(veh, 0x198) + U32(veh, 0x194) * 4u;
            if (U32(node, k * 4u + 0x68u) & 0x200) return node;
        }
        node = next;
        if (node == 0) return 0;
    }
    return 0;
}
RX_FUNCTION(0x00526b40, rx_behavior_corner_find)

/* 00526b90 (ECX = node, EDX = the driver's section (+0x14), stack arg = behaviour) -> EAX: the last node of the section's corner
 * run from `node` (nodes whose vehicle-class flag 0x200 is set are followed through +0x64); at a section end the next section
 * (section +8 -> its first node) continues, or 0 when the section says +0x10 == 0. The node flag is at node + 0x68 + 4 *
 * (vehicle +0x198 + 4 * vehicle +0x194). Returns the node before the first unflagged one (the entry node when it is unflagged). */
uint32_t rx_behavior_corner_last(rx_addr node, rx_addr section, rx_addr b) {
    if (node == 0) return 0;
    rx_addr veh = U32(U32(U32(b, 8), 0x74), 0x1ac);
    const uint32_t flags = (U32(veh, 0x198) + U32(veh, 0x194) * 4u) * 4u + 0x68u;
    uint32_t eax = node;
    for (;;) {
        rx_addr e = node;
        for (;;) {
            if (!(U32(e, flags) & 0x200u)) return eax;
            eax = e;
            e = U32(e, 0x64);
            if (!e) break;
        }
        if (U32(section, 0x10) == 0) return 0;
        section = U32(U32(section, 8), 0);
        e = U32(section, 0);
        if (!e) return 0;
        node = e;
    }
}
RX_FUNCTION(0x00526b90, rx_behavior_corner_last)

/* 00526bf0 (EAX = node, ECX = the driver's section, stack arg = behaviour) -> xmm0: the run length of the same corner walk
 * (the node +0x4f0 lengths of the flagged nodes, added in the original's SSE order: total = node + total). */
float rx_behavior_corner_length(rx_addr node, rx_addr section, rx_addr b) {
    float total = 0.0f;
    if (node == 0) return total;
    rx_addr veh = U32(U32(U32(b, 8), 0x74), 0x1ac);
    const uint32_t flags = (U32(veh, 0x198) + U32(veh, 0x194) * 4u) * 4u + 0x68u;
    for (;;) {
        rx_addr e = node;
        for (;;) {
            if (!(U32(e, flags) & 0x200u)) return total;
            total = F32(e, 0x4f0) + total;
            e = U32(e, 0x64);
            if (!e) break;
        }
        if (U32(section, 0x10) == 0) return total;
        section = U32(U32(section, 8), 0);
        e = U32(section, 0);
        if (!e) return total;
        node = e;
    }
}
RX_FUNCTION(0x00526bf0, rx_behavior_corner_length)

/* 005380f0 (EAX = node, ESI = the vehicle class +0x194) -> EAX 1 / -1: the direction of the corner run: the sum of the turn
 * (the cross product's Y of consecutive nodes' direction vectors, x the segment lengths) added when it is >= 0, subtracted
 * otherwise; 1 when the sum is > 0, else -1. */
uint32_t rx_behavior_corner_direction(rx_addr node, uint32_t cls) {
    float sum = 0.0f;
    if (node == 0) return 0xffffffffu;
    rx_addr e = node;
    const uint32_t k = (cls + 7u) * 16u;
    for (;;) {
        rx_addr next = U32(e, 0x64);
        if (!next) break;
        if (!(U32(e, k) & 0x200u)) break;
        if (!(U32(next, k) & 0x200u)) break;
        const float a0 = F32(e, 0x4f0);
        const float ax = F32(e, 0x50) * a0, ay = F32(e, 0x54) * a0, az = F32(e, 0x58) * a0;
        const float b0 = F32(next, 0x4f0);
        const float bx = F32(next, 0x50) * b0, by = F32(next, 0x54) * b0, bz = F32(next, 0x58) * b0;
        float dot = bx * ax + by * ay;
        const float cross = bx * az - bz * ax;
        dot = dot + bz * az;
        if (0.0f <= cross) sum = dot + sum; else sum = sum - dot;
        e = next;
    }
    return sum > 0.0f ? 1u : 0xffffffffu;
}
RX_FUNCTION(0x005380f0, rx_behavior_corner_direction)

/* 00526dd0 (EAX = object with the driver at +0x08, stack int side; RET 4) -> EAX 1 / 0: whether the vehicle is past the driver's
 * current node on that side. The node's flat direction (+0x50 / +0x58) x its length (+0x4f0), the vehicle minus the node's position
 * (+0xc0 / +0xc8 against +0x00 / +0x08), cross product's y (x * z - z * x), x (float)side; comiss with 0 / jbe: 1 only above 0
 * (unordered = 0). Called by HandleBraking's mode 2 (+0x4c's +0x1c is the side) and by LapPacing (b+0x4c's +0x1c when +0xc == 2). */
uint32_t rx_behavior_node_side(rx_addr obj, int32_t side) {
    const rx_addr drv = U32(obj, 8);
    const rx_addr node = U32(drv, 0x18);
    const rx_addr veh = b1_vehicle(drv);
    const float len = F32(node, 0x4f0);
    const float dx = F32(node, 0x50) * len;
    const float dz = F32(node, 0x58) * len;
    const float dz_ = F32(veh, 0xc8) - F32(node, 8);
    const float dx_ = F32(veh, 0xc0) - F32(node, 0);
    const float c = (dx_ * dz - dz_ * dx) * (float)side;
    return c > 0.0f ? 1u : 0u;
}
RX_FUNCTION(0x00526dd0, rx_behavior_node_side)

/* 00526a30 (EDI = behaviour): watching: the next corner within the tuning's distance (0x9e44d8) ahead of the driver's next node
 * (+0x14 = its first node, 0 = none); when there is one: +0x10 the driver's section, +0x18 the corner's last node (00526b90),
 * +0x20 its length (00526bf0), +0x1c its direction (005380f0: 1 / -1, the sign of the summed turn), state 1. */
void rx_behavior_monitor_corner_watch(rx_addr b) {
    rx_addr drv = b1_driver(b);
    rx_addr node = rx_behavior_corner_find(U32(drv, 0x1c), b1_tuning(0x009e44d8, drv), b);
    SU32(b, 0x14, node);
    if (node == 0) return;
    SU32(b, 0x10, U32(drv, 0x14));
    __typeof__(&rx_behavior_corner_last) last = RX_FN(0x00526b90, __typeof__(&rx_behavior_corner_last));
    rx_regs in, o;
    if (last) { SU32(b, 0x18, last(node, U32(drv, 0x14), b)); }
    else {
        in = b1_regs(); o = b1_regs();
        in.r[B1_ECX] = node; in.r[B1_EDX] = U32(drv, 0x14); in.r[B1_EBX] = drv; in.r[B1_ESI] = node; in.r[B1_EDI] = b;
        B1_EX(0x00526b90, &in, &o, b);
        SU32(b, 0x18, o.r[B1_EAX]);
    }
    __typeof__(&rx_behavior_corner_length) clen = RX_FN(0x00526bf0, __typeof__(&rx_behavior_corner_length));
    if (clen) { SF32(b, 0x20, clen(node, U32(b, 0x10), b)); }
    else {
        in = b1_regs(); o = b1_regs();
        in.r[B1_EAX] = node; in.r[B1_ECX] = U32(b, 0x10); in.r[B1_EBX] = drv; in.r[B1_ESI] = node; in.r[B1_EDI] = b;
        B1_EX(0x00526bf0, &in, &o, b);
        SF32(b, 0x20, o.xmm[0].v[0]);
    }
    rx_addr veh = b1_vehicle(drv);
    __typeof__(&rx_behavior_corner_direction) cdir = RX_FN(0x005380f0, __typeof__(&rx_behavior_corner_direction));
    if (cdir) { SU32(b, 0x1c, cdir(node, U32(veh, 0x194))); }
    else {
        in = b1_regs(); o = b1_regs();
        in.r[B1_EAX] = node; in.r[B1_ECX] = b1_player(drv); in.r[B1_EDX] = veh; in.r[B1_ESI] = U32(veh, 0x194);
        in.r[B1_EBX] = drv; in.r[B1_EDI] = b;
        B1_EX0(0x005380f0, &in, &o);
        SU32(b, 0x1c, o.r[B1_EAX]);
    }
    SU32(b, 0xc, 1);
}
RX_FUNCTION(0x00526a30, rx_behavior_monitor_corner_watch)

/* 00526ab0 (EAX = behaviour): state 1, approaching the corner found by watch(): stay in state 1 while the corner is still ahead: the
 * driver has not passed the corner's entry node (behaviour +0x14 vs driver +0x18) and a corner is still within the tuning's distance
 * (0x9e44d8) ahead of the driver's next node (+0x1c, the same search 00526a30 used). Once the entry node is behind it and no corner is
 * in reach, state 2 (00526b00 waits for the run's end). No output: the behaviour keeps steering through NavigateTrack. */
void rx_behavior_monitor_corner_wait(rx_addr b) {
    const rx_addr drv = b1_driver(b);
    if (U32(b, 0x14) != U32(drv, 0x18)) {
        if (rx_behavior_corner_find(U32(drv, 0x1c), b1_tuning(0x009e44d8, drv), b) != 0) return;
    }
    SU32(b, 0xc, 2);
}
RX_FUNCTION(0x00526ab0, rx_behavior_monitor_corner_wait)

/* 00526b00 (ECX = behaviour): state 2, inside the corner: while the driver's current node (+0x18) still carries the vehicle class's
 * corner flag (0x200, node + 0x68 + 4 * (vehicle +0x198 + 4 * vehicle +0x194)) the behaviour waits; at the first unflagged node it
 * resets itself through its own vf6 (the original's `jmp [vtbl+0x18]` tail call: 00526950 clears +0x0c..+0x20, i.e. state 0). */
void rx_behavior_monitor_corner_hold(rx_addr b) {
    const rx_addr drv = b1_driver(b);
    const rx_addr veh = b1_vehicle(drv);
    const uint32_t k = (U32(veh, 0x198) + U32(veh, 0x194) * 4u) * 4u + 0x68u;
    if (U32(U32(drv, 0x18), k) & 0x200u) return;
    b1_behavior_reset(b);
}
RX_FUNCTION(0x00526b00, rx_behavior_monitor_corner_hold)
