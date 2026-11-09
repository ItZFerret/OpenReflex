/* race_beh1_air.c -- BehaviorAirLeveling (vtable 0x93f9f0) and BehaviorAvoidFlipBack (0x93fce8): the AI in the air.
 * AirLeveling: while airborne, the landing prediction (0x00461980: where / with which ground normal / in how long the bike lands)
 * gives the rider's lean: forward / back (+0x40: 3 x |v . n| signed by the velocity against the landing normal, n the predicted
 * ground normal or the normal of the track segment found at the landing point) and sideways (+0x44: toward the target the line
 * rule gives, scaled by how far the heading is off it, halved for the ATV classes); +0x48 = half the body x axis . n, +0x4c "land
 * now" when the time to land is below the tuning's limit (0xa04c58), +0x54 airborne time, +0x58 the driver clock + 0.1 while the
 * body's roll rate (vf38 x) exceeds 2. The landing prediction is expensive: one airborne AirLeveling per frame (round robin over the
 * global list 0x9e3f68, count 0xa89d04, cursor 0xa93f54; 0051ff80) refreshes it, the others count their time to land down
 * (0051fe00), all under the AI critical section 0xae1c80. Behaviour: +0x0c the line rule (+0x04, +0x0c side, +0x20 target),
 * +0x10 the gate rule, +0x20 / +0x30 / +0x50 the prediction copied for this frame (normal, landing point, time), +0x60 the
 * prediction record {+0x00 behaviour, +0x10 point, +0x20 normal, +0x30 time, +0x34 airborne seen, +0x3c refresh this frame}.
 * AvoidFlipBack: on the ground below 50 units/s, when the body pitches back (up . heading below -0.1736 = 10 degrees) the throttle
 * (driver +0x3c0) is eased off (it drops 1 / s while the pitch grows, recovers 0.5 / s) and the lean (+0x3e0, +0x3d8) is set from
 * the pitch; airborne it follows the AirLeveling's landing state. (core/notes/race.md "AI behaviours") */
#include "race_beh1_extern.h"
#include "rider_cap.h"
#include "../src/rx_os.h"
#include "race_driver.h"
#include "crt.h"

/* ---- calls ---- */
static inline void b1_enter_cs(rx_addr cs) { RX_OS(0x0091600c, cs); }
static inline void b1_leave_cs(rx_addr cs) { RX_OS(0x009160f8, cs); }
/* 00461980 (ECX = time out, EDX = vehicle, args: point out, normal out; RET 8; misc cluster): the landing prediction */
typedef uint32_t (*b1_landing_fn)(rx_addr time_out, rx_addr vehicle, rx_addr point_out, rx_addr normal_out);
static void b1_predict_landing(rx_addr time_out, rx_addr veh, rx_addr point_out, rx_addr normal_out) {
    b1_landing_fn f = RX_FN(0x00461980, b1_landing_fn);
    if (f) { RACE_CORE_CALL(2, f(time_out, veh, point_out, normal_out)); return; }
    rx_regs in = b1_regs();
    in.r[B1_ECX] = time_out; in.r[B1_EDX] = veh;
    B1_EX(0x00461980, &in, 0, point_out, normal_out);
}
/* vehicle vf38 (0x98, thiscall out): the body's angular velocity (16 bytes); vf15 (0x3c, thiscall out): its 4x4 world matrix;
 * vf45 (0xb4): nonzero while ... (thiscall, no arguments) */
typedef void (*b1_out_fn)(rx_addr self, rx_addr out);
static void b1_vehicle_out(rx_addr veh, uint32_t slot, rx_addr out) {
    rx_addr fn = race_vfn(veh, slot);
    b1_out_fn f = RX_FN(fn, b1_out_fn);
    if (f) f(veh, out); else B1_ORIG(fn, RX_THISCALL, veh, out);
}
static uint32_t b1_vehicle_vf45(rx_addr veh) {
    rx_addr fn = race_vfn(veh, 0xb4 / 4);
    b1_u32_fn f = RX_FN(fn, b1_u32_fn);
    return f ? f(veh) : B1_ORIG0(fn, RX_THISCALL, veh);
}
/* 00508c00 (race_driver.c): the line rule's target (x, z) (+0x20 / +0x24) */
static void b1_rule_target(rx_addr out, rx_addr rule) { rx_copy_pair_20(rule, out); }
/* 0050ed70 (EAX = out, ECX = gate rule): two FLD/FSTP dword copies of the
 * target (x,z). Preserve order when out aliases the source, and quiet SNaNs as
 * masked x87 loads do; these are copies, not arithmetic. */
void rx_gate_rule_target(rx_addr out, rx_addr rule) {
    for(uint32_t i=0;i<2;i++) {
        uint32_t bits=U32(rule,0x48+4*i);
        if((bits&0x7f800000u)==0x7f800000u&&(bits&0x007fffffu))bits|=0x00400000u;
        SU32(out,4*i,bits);
    }
}
RX_FUNCTION(0x0050ed70, rx_gate_rule_target)

/* ---------------------------------------------------------------------------------------------------- BehaviorAirLeveling */
/* 0051fea0 (ECX = normal out, EDX = point out, args: prediction record, time out; RET 8): copy the prediction (x87 copies). */
void rx_air_prediction_copy(rx_addr normal_out, rx_addr point_out, rx_addr rec, rx_addr time_out) {
    SF32(point_out, 0, F32(rec, 0x10)); SF32(point_out, 4, F32(rec, 0x14)); SF32(point_out, 8, F32(rec, 0x18));
    SF32(normal_out, 0, F32(rec, 0x20)); SF32(normal_out, 4, F32(rec, 0x24)); SF32(normal_out, 8, F32(rec, 0x28));
    SF32(time_out, 0, F32(rec, 0x30));
}
RX_FUNCTION(0x0051fea0, rx_air_prediction_copy)

/* 0051fe00 (ESI = prediction record, stack dt; RET 4 through LeaveCriticalSection): refresh the prediction when this record was
 * picked for the frame (or on the first airborne frame), else count the time to land down (not below 0); on the ground: 0. */
void rx_air_prediction_update(rx_addr rec, float dt) {
    b1_enter_cs(0x00ae1c80);
    rx_addr veh = b1_vehicle(b1_driver(U32(rec, 0)));
    if (veh == 0 || b1_vehicle_airborne(b1_vehicle(b1_driver(U32(rec, 0)))) == 0) {
        SU8(rec, 0x34, 0);
        SF32(rec, 0x30, 0.0f);
    } else {
        if (U8(rec, 0x34) == 0) { SU8(rec, 0x3c, 1); SU8(rec, 0x34, 1); }
        if (U8(rec, 0x3c) != 0) {
            b1_predict_landing(rec + 0x30, b1_vehicle(b1_driver(U32(rec, 0))), rec + 0x10, rec + 0x20);
        } else {
            float t = F32(rec, 0x30) - dt;
            SF32(rec, 0x30, 0.0f < t || t != t ? t : 0.0f);
        }
    }
    b1_leave_cs(0x00ae1c80);
}
RX_FUNCTION(0x0051fe00, rx_air_prediction_update)

/* 0051ff80: pick this frame's record to refresh: clear every record's flag, then from the cursor on the first whose rider is
 * airborne (the cursor advances past each one looked at, modulo the count). */
void rx_air_prediction_schedule(void) {
    b1_enter_cs(0x00ae1c80);
    uint32_t n = U32(0, 0x00a89d04);
    if (n == 0) { b1_leave_cs(0x00ae1c80); return; }
    uint32_t cur = U32(0, 0x00a93f54);
    if (cur > n - 1) { cur = 0; SU32(0, 0x00a93f54, 0); }
    for (uint32_t i = 0; i < n; i++) SU8(U32(rx_rc_tab(0x009e3f68, 4, i), 0), 0x3c, 0);
    const uint32_t start = cur;
    uint32_t found = 0;
    do {
        rx_addr beh = U32(U32(rx_rc_tab(0x009e3f68, 4, cur), 0), 0);
        if (b1_vehicle(b1_driver(beh)) != 0) {
            uint32_t a = b1_vehicle_airborne(b1_vehicle(b1_driver(beh)));
            cur = U32(0, 0x00a93f54);
            n = U32(0, 0x00a89d04);
            if (a != 0) { SU8(U32(rx_rc_tab(0x009e3f68, 4, cur), 0), 0x3c, 1); found++; }
        }
        cur = (cur + 1) % n;
        SU32(0, 0x00a93f54, cur);
    } while (cur != start && found < 1);
    b1_leave_cs(0x00ae1c80);
}
RX_FUNCTION(0x0051ff80, rx_air_prediction_schedule)

/* 0051f480 BehaviorAirLeveling::vf4 (ECX = behaviour, stack dt; RET 4) -> 0 (it only sets the lean values). */
uint32_t rx_behavior_air_leveling_update(rx_addr b, float dt) {
    if (U32(b, 4) == 0) return 0;
    SF32(b, 0x40, 0.0f); SF32(b, 0x44, 0.0f); SF32(b, 0x48, 0.0f);
    SU32(b, 0x4c, 0);
    SF32(b, 0x50, 0.0f);
    const rx_addr veh = b1_vehicle(b1_driver(b));
    /* the original's frame, at its address (push ebp; and esp, -16; sub esp, 0x114; 3 pushes): the landing prediction keeps
     * pointers into the stack, so the calls run at the original's stack depth. S + 0x20 normal / target / projection, + 0x40
     * matrix, + 0xc0 found (section, node), + 0xd0 angular velocity, + 0xe0 query / projection */
    const rx_addr sp0 = rx_sp(), S = ((sp0 - 4u) & ~15u) - 0x120u;
    rx_set_sp(S - 8u);                           /* the pushed dt and the return address */
    rx_air_prediction_update(b + 0x60, dt);
    rx_set_sp(S);
    b1_vehicle_out(veh, 0x98 / 4, S + 0xd0);
    if ((U32(0, 0x00dcf2cc) & 1) == 0) {         /* static local: the roll-rate limit 2.0 */
        SU32(0, 0x00dcf2cc, U32(0, 0x00dcf2cc) | 1);
        SF32(0, 0x00dcf2c8, 2.0f /* 0x92e330 */);
    }
    if (F32(0, 0x00dcf2c8) < F32(S, 0xd0)) SF32(b, 0x58, F32(b1_driver(b), 0x130) + 0.1f /* 0x931d94 */);
    else SF32(b, 0x58, 0.0f);
    if (b1_vehicle_airborne(veh) == 0) {
        SF32(b, 0x54, 0.0f);
        goto out;
    }
    SF32(b, 0x54, dt + F32(b, 0x54));
    rx_air_prediction_copy(b + 0x20, b + 0x30, b + 0x60, b + 0x50);
    b1_vehicle_out(veh, 0x3c / 4, S + 0x40);
    {
        float d = (F32(b, 0x24) * F32(veh, 0x134) + F32(veh, 0x130) * F32(b, 0x20)) + F32(b, 0x28) * F32(veh, 0x138);
        float s = 0.0f > d ? -1.0f : 1.0f;
        SF32(b, 0x40, fabsf(d) * s * 3.0f /* 0x92fe48 */);
    }
    if (U32(0, 0x00ae1b90) != 0) {               /* debug text: "%f" of the time to land, to the player's +0x1b4 display */
        double t = (double)F32(b, 0x50);
        uint64_t tb; memcpy(&tb, &t, 8);
        const uint32_t fa[3] = {0x0093a738u, (uint32_t)tb, (uint32_t)(tb >> 32)};   /* push double; push fmt; call 0078fd60 (EDI = buffer, ESI = 0x10) */
        for (uint32_t i = 0; i < 3; i++) SU32(S - 12u + 4u * i, 0, fa[i]);
        SU32(S - 16u, 0, 0x0051f646);
        rx_set_sp(S - 16u);
        rx_format_buffer(S + 0xe0, 0x10, 0x0093a738u, S - 8u);
        rx_set_sp(S);
        rx_addr disp = U32(b1_player(b1_driver(b)), 0x1b4);
        B1_ORIG(race_vfn(disp, 0x3c / 4), RX_THISCALL, disp, S + 0x80);
    }
    SF32(b, 0x48, ((F32(b, 0x24) * F32(S, 0x44) + F32(b, 0x20) * F32(S, 0x40)) + F32(b, 0x28) * F32(S, 0x48)) * 0.5f);
    if (U32(b, 0xc) == 0) {
        if (b1_tuning(0x00a04c58, b1_driver(b)) > F32(b, 0x50)) SU32(b, 0x4c, 1);
        goto out;
    }
    {
        rx_addr drv = b1_driver(b);
        const rx_addr q = S + 0xe0;
        for (uint32_t k = 0; k < 0x28; k += 4) SU32(q, k, 0);
        SU32(q, 0x20, 1);                         /* follow into the next / previous section */
        SU32(q, 0x24, 0);
        SU32(q, 0x0c, U32(drv, 0x18));
        SU32(q, 0x08, U32(drv, 0x14));
        SU32(q, 0x00, S + 0xc0);
        SU32(q, 0x04, b + 0x30);
        SF32(q, 0x14, b1_data(0x009cd990) /* 460 */);
        SF32(q, 0x18, b1_data(0x009cd990));
        const int found = rx_track_find_segment(q, 0);
        rx_addr a, c;                             /* the segment a -> c of the landing point */
        if (found == 0) {
            drv = b1_driver(b);
            SU32(S, 0xc4, U32(drv, 0x18));
            SU32(S, 0xc0, U32(drv, 0x14));
            float budget = b1_data(0x009cd990);
            rx_track_nearest_node(S + 0xc0, b + 0x30, S + 0x20, &budget);
            rx_addr node = U32(S, 0x24), next = U32(node, 0x64);
            if (next == 0) {
                rx_addr sec = U32(S, 0x20);
                if (U32(sec, 0x10) != 0) goto out;
                next = U32(U32(U32(U32(sec, 8), 0), 0), 0x64);
            }
            float t = rx_track_project_xz(node, next, b + 0x30, S + 0xe0);
            if (0.0f > t) {
                rx_addr prev = U32(node, 0x60);
                c = node;
                if (prev == 0) {
                    rx_addr sec = U32(S, 0x20);
                    if (U32(sec, 0x14) == 0) goto out;
                    prev = U32(U32(U32(U32(sec, 0xc), 0), 4), 0x60);
                }
                a = prev;
            } else {
                rx_addr nn = U32(next, 0x64);
                if (nn == 0) nn = U32(U32(U32(U32(U32(S, 0x20), 8), 0), 0), 0x64);
                if (rx_track_project_xz(next, nn, b + 0x30, S + 0xe0) > 0.0f) goto out;
                a = node; c = next;
            }
            SU32(S, 0x18, a);
        } else {
            rx_addr node = U32(S, 0xc4), next = U32(node, 0x64);
            SU32(S, 0x18, node);
            if (next == 0) next = U32(U32(U32(U32(S, 0xc0), 8), 0), 0);
            a = node; c = next;
        }
        /* the segment's ground normal (from its edge on the rule's side) */
        float ux, uy, uz, vx, vy, vz;
        const float ax = F32(a, 0), ay = F32(a, 4), az = F32(a, 8);
        if (I32(U32(b, 0xc), 0xc) == -1) {
            ux = F32(c, 0) - ax; vx = F32(a, 0x10) - ax;
            uy = F32(c, 4) - ay; vy = F32(a, 0x14) - ay;
            uz = F32(c, 8) - az; vz = F32(a, 0x18) - az;
        } else {
            ux = F32(a, 0x20) - ax; vx = F32(c, 0) - ax;
            uy = F32(a, 0x24) - ay; vy = F32(c, 4) - ay;
            uz = F32(a, 0x28) - az; vz = F32(c, 8) - az;
        }
        float nz = uy * vx - vy * ux;
        float ny = vz * ux - uz * vx;
        float nx = vy * uz - vz * uy;
        float n2 = (ny * ny + nx * nx) + nz * nz;
        if (n2 > 0.0f) {
            float inv = 1.0f / sqrtf(n2);
            nx = inv * nx;
            ny = ny * inv;
            nz = nz * inv;
        }
        float d = (F32(veh, 0x134) * ny + F32(veh, 0x130) * nx) + F32(veh, 0x138) * nz;
        float s = 0.0f > d ? -1.0f : 1.0f;
        SF32(b, 0x40, fabsf(d) * s * 3.0f);
        if (b1_tuning(0x00a04c58, b1_driver(b)) > F32(b, 0x50)) { SU32(b, 0x4c, 1); goto out; }
        /* how far along the rule's line the landing point is: the projections on the +0x10 and +0x20 edges */
        rx_track_project_xz(a + 0x10, c + 0x10, b + 0x30, S + 0xe0);
        rx_track_project_xz(U32(S, 0x18) + 0x20, c + 0x20, b + 0x30, S + 0x20);
        const float p1x = F32(S, 0xe0), p1y = F32(S, 0xe4), p1z = F32(S, 0xe8);
        float wx = F32(S, 0x20) - p1x, wy = F32(S, 0x24) - p1y, wz = F32(S, 0x28) - p1z;
        float lx = F32(b, 0x30) - p1x, ly = F32(b, 0x34) - p1y, lz = F32(b, 0x38) - p1z;
        float f = ((ly * wy + wx * lx) + lz * wz) / ((wy * wy + wx * wx) + wz * wz);
        int32_t side = 0.5f > f ? 1 : -1;
        rx_addr rule = U32(b, 0xc);
        if (U32(rule, 4) == 0) {
            rx_addr g = U32(b, 0x10);
            if (g == 0 || U32(g, 0x50) == 0) goto out;
            rx_gate_rule_target(S + 0x20, g);
        } else {
            b1_rule_target(S + 0x20, rule);
        }
        drv = b1_driver(b);
        rx_addr v = b1_vehicle(drv);
        const float vxp = F32(v, 0xc0), vzp = F32(v, 0xc8);
        float tx = F32(S, 0x20) - vxp, tz = F32(S, 0x24) - vzp;
        float mx = F32(b, 0x30) - vxp, mz = F32(b, 0x38) - vzp;
        float t2 = tz * tz + tx * tx;
        if (t2 != 0.0f) {
            float inv = 1.0f / sqrtf(t2);
            tx = tx * inv;
            tz = tz * inv;
        }
        float m2 = mz * mz + mx * mx;
        if (m2 != 0.0f) {
            float inv = 1.0f / sqrtf(m2);
            mx = inv * mx;
            mz = mz * inv;
        }
        float q_;
        if (found == 0) q_ = F32(b, 0x50) > 0.5f ? 0.0f : 1.0f;
        else {
            q_ = 1.0f - (mz * tz + mx * tx);
            if (!(q_ > 0.0f)) q_ = 0.0f;
        }
        uint32_t cls = U32(b1_vehicle(drv), 0x194);
        if (cls == 1 || cls == 0) SF32(b, 0x44, (float)side * q_);
        else SF32(b, 0x44, (float)side * q_ * 0.5f);
    }
out:
    rx_set_sp(sp0);
    return 0;
}
RX_FUNCTION(0x0051f480, rx_behavior_air_leveling_update)

/* -------------------------------------------------------------------------------------------------- BehaviorAvoidFlipBack */
/* +0x0c the last pitch (up . heading), +0x10 the throttle it allows. */

/* 00520050 BehaviorAvoidFlipBack::vf4 (ECX = behaviour, stack dt; RET 4) -> 0. */
uint32_t rx_behavior_avoid_flip_back_update(rx_addr b, float dt) {
    rx_addr drv = b1_driver(b);
    if (U32(drv, 0x32c) == 2) {
        if (b1_vehicle_airborne(b1_vehicle(drv)) != 0) SF32(b1_driver(b), 0x3c0, 0.2f /* 0x92e388 */);
        return 0;
    }
    rx_addr veh = b1_vehicle(drv);
    float vx = F32(veh, 0x130), vz = F32(veh, 0x138);
    float len2 = vz * vz + vx * vx;
    float hx, hy, hz;
    if (len2 > 0.0f) {
        float inv = 1.0f / sqrtf(len2);
        hx = inv * vx;
        hy = rx_muls(inv, 0.0f /* 0x92e12c */);
        hz = vz * inv;
    } else {
        /* the original reads its heading y from a frame slot it never wrote (stale stack: the original's esp + 0x24) */
        hx = vx; hz = vz;
        hy = F32(0, ((rx_sp() - 4u) & ~15u) - 0xcu);
    }
    veh = b1_vehicle(drv);
    const float pitch = (F32(veh, 0x148) * hz + F32(veh, 0x144) * hy) + F32(veh, 0x140) * hx;
    const float thr = F32(drv, 0x3c0);
    if (b1_data(0x009d78dc) /* 50 */ > F32(veh, 0x150) && b1_vehicle_vf45(b1_vehicle(drv)) == 0) {
        if (!(-0.1736f /* 0x9779cc */ > pitch) && U32(U32(b1_vehicle(b1_driver(b)), 0x234), 0x2eb8) == 0) {
            if (pitch > 0.1736f /* 0x9779c8 */) {
                SF32(b, 0x10, thr);
                SF32(b, 0xc, pitch);
            }
            return 0;
        }
        if (b1_vehicle_airborne(b1_vehicle(b1_driver(b))) != 0) {
            SF32(b, 0x10, 0.1f /* 0x931d94 */);
            SF32(b1_driver(b), 0x3c0, 0.1f);
        } else {
            float last = F32(b, 0xc), change;
            if (last == 0.0f) {
                change = pitch - 0.1736f;
                SF32(b, 0x10, thr);
            } else {
                change = pitch - last;
            }
            if (0.0f > change) {
                float x = F32(b, 0x10) - dt;
                SF32(b, 0x10, x);
                if (0.1f > x) SF32(b, 0x10, 0.1f);
            } else {
                SF32(b, 0x10, dt * 0.5f + F32(b, 0x10));
            }
            SF32(b1_driver(b), 0x3c0, F32(b, 0x10));
        }
        rx_addr d = b1_driver(b);
        SF32(b, 0xc, pitch);
        SF32(d, 0x3e0, pitch * rx_bf(0xc0086b85u) /* 0x9779c4 -2.1315625 */ - rx_bf(0x3ebd833eu) /* 0x9779c0 0.37014192 */);
        SF32(d, 0x3d8, F32(d, 0x3e0));
        return 0;
    }
    if (b1_vehicle_airborne(b1_vehicle(b1_driver(b))) != 0) {
        rx_addr al = rx_driver_air_leveling_behavior(b1_driver(b));
        if (al != 0) {
            rx_addr d = b1_driver(b);
            if (F32(al, 0x54) > rx_ai_param_e130(d) * 0.001f /* 0x97743c */ &&
                F32(al, 0x50) > rx_ai_param_e180(d) * 0.001f)
                SF32(d, 0x3c0, 0.0f > F32(al, 0x40) ? 1.0f : 0.2f);
        }
    }
    SF32(b, 0x10, 0.1f);
    SF32(b, 0xc, 0.0f);
    return 0;
}
RX_FUNCTION(0x00520050, rx_behavior_avoid_flip_back_update)
