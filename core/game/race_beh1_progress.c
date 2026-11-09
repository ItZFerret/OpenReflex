/* race_beh1_progress.c -- BehaviorFixForwardProgress (vtable 0x93fbbc): the AI's "stuck" recovery.
 * While racing, when the driver asks for throttle (+0x3c0 != 0) but the vehicle's speed^2 (velocity +0x160) stays below the limit
 * (+0x10) for the timeout (+0x0c, deadline +0x20 on the driver clock +0x130), the behaviour remembers the position (+0x40) and
 * escalates: stage 1 (+0x18 = 1) then stage 2 (with the reverse flag driver +0x3d0); each stage drives a recovery manoeuvre
 * (00523a70 / 00523ca0 with a steer side +0x54 / +0x58: toward the track direction, chosen from the rider's side of the line and its
 * facing when +0x60 is set, else alternated) and takes control (returns 1) until it ends. +0x14 / +0x1c / +0x30 / +0x50 are a second
 * manoeuvre set (started elsewhere). (core/notes/race.md "AI behaviours") */
#include "crt_atexit.h"
#include "race_beh1_extern.h"
#include "build_series_init.h"
#include "career_activity.h"
#include "../src/rx_x87.h"

/* The original passes SqrtF's argument through `fld m32; fstp m32` first. In particular, preserve that x87 NaN conversion. */
static float b1_progress_sqrt(float x) {
    return rx_sqrts(rx_f80_to_f32(rx_f80_from_f32(x), 0));
}

/* vehicle vf14 (0x38): a pointer to its forward axis (defined below; 00523ca0 / 00523ed0 call it and ignore the result) */
static rx_addr b1_vehicle_forward(rx_addr veh);

/* 00524050 (EAX = behaviour) -> EAX: the race-type test behind +0x60: the driver's activity kind (+0x23c; from 0x1f on the series
 * manager's, 005b6a30), 1 for kinds 0..4, 7, 0x11, 0x12, 0x13 (the jump table at 0x5240b0 / 0x5240b8), else 0. */
static uint32_t b1_progress_kind(rx_addr b) {
    const rx_addr act = U32(U32(U32(b, 8), 0x74), 0x94);
    uint32_t kind = U32(act, 0x23c);
    if (kind >= 0x1f) {   /* a series event: the manager's type (out of the MX scope; the same inline as race_beh2_extern.h) */
        if ((U32(0, 0x009e275c) & 1) == 0) {
            SU32(0, 0x009e275c, U32(0, 0x009e275c) | 1);
            RACE_CORE_CALL(0, (void)rx_series_manager_construct());     /* MXSeriesManager_Construct */
            RX_ATEXIT(0x00911da0);           /* _atexit */
        }
        RACE_CORE_CALL(1, kind = rx_career_current_event_type(0x009e2188));   /* 005b6a30 (stdcall manager; RET 4) */
    }
    return kind;
}
uint32_t rx_behavior_fix_progress_race_type(rx_addr b) {
    const uint32_t kind = b1_progress_kind(b);
    switch (kind) {
    case 0: case 1: case 2: case 3: case 4: case 7: case 0x11: case 0x12: case 0x13: return 1;
    default: return 0;
    }
}
RX_FUNCTION(0x00524050, rx_behavior_fix_progress_race_type)

/* 00523a70 (ECX = position, EDX = side, stack behaviour; RET 4) -> EAX: the steer-away manoeuvre. The driver's throttle 1, +0x3c4 /
 * +0x3c8 0, +0x3cc / +0x3d8 / +0x3e0 1 and the steering bias +0x60 = (float)-side; then the position - the vehicle's, and by
 * +0x60 (the race-type flag):
 *  - +0x60 != 0: side 0 -> 1 when the squared distance is above 900 (0x9cd9a0); else 1 when it is above 16 (0x9ddfdc) and the
 *    driver's current node's direction (+0x50..0x58) against the vehicle heading (+0x130..0x138) is above 0.9 (0x9cd970).
 *  - +0x60 == 0: 1 above 900; 0 up to 225 (0x9d78c0); otherwise 1 when the vehicle heading points at the normalized position by
 *    less than 0.707 (0x9cd99c). comiss / jbe: unordered fails every test. */
uint32_t rx_behavior_fix_progress_steer_away(rx_addr b, rx_addr pos, uint32_t side) {
    const rx_addr drv = U32(b, 8);
    SF32(drv, 0x3c0, 1.0f /* 0x931f24 */);
    SF32(drv, 0x3c4, 0.0f);
    SF32(drv, 0x3c8, 0.0f);
    SF32(drv, 0x3cc, 1.0f);
    SF32(drv, 0x3e0, 1.0f);
    SF32(drv, 0x3d8, 1.0f);
    SF32(drv, 0x60, (float)(int32_t)(0u - side));            /* cvtsi2ss of the negated side */
    const rx_addr veh = b1_vehicle(drv);
    const float dx = rx_subs(F32(pos, 0), F32(veh, 0xc0));
    const float dy = rx_subs(F32(pos, 4), F32(veh, 0xc4));
    const float dz = rx_subs(F32(pos, 8), F32(veh, 0xc8));
    const float l2 = rx_adds(rx_adds(rx_muls(dz, dz), rx_muls(dy, dy)), rx_muls(dx, dx));
    if (U32(b, 0x60) != 0) {
        if (side == 0) return l2 > F32(0, 0x009cd9a0) ? 1u : 0u;
        if (!(l2 > F32(0, 0x009ddfdc) /* 16 */)) return 0;
        const rx_addr node = U32(drv, 0x18);
        const float dot = rx_adds(rx_adds(rx_muls(F32(node, 0x58), F32(veh, 0x138)),
                                          rx_muls(F32(node, 0x54), F32(veh, 0x134))),
                                  rx_muls(F32(node, 0x50), F32(veh, 0x130)));
        return dot > F32(0, 0x009cd970) /* 0.9 */ ? 1u : 0u;
    }
    if (l2 > F32(0, 0x009cd9a0) /* 900 */) return 1;
    if (!(l2 > F32(0, 0x009d78c0) /* 225 */)) return 0;
    const float inv = rx_divs(1.0f /* 0x931f24 */, b1_progress_sqrt(l2));
    const float nx = rx_muls(dx, inv), ny = rx_muls(dy, inv), nz = rx_muls(dz, inv);
    const float dot = rx_adds(rx_adds(rx_muls(F32(veh, 0x138), nz), rx_muls(F32(veh, 0x134), ny)),
                              rx_muls(F32(veh, 0x130), nx));
    return F32(0, 0x009cd99c) /* 0.707 */ > dot ? 1u : 0u;
}
RX_FUNCTION(0x00523a70, rx_behavior_fix_progress_steer_away)

/* 00523ca0 (EAX = position, ESI = behaviour, stack side; RET 4) -> EAX: the back-up manoeuvre. The vehicle heading (+0x130 /
 * +0x138) normalized, the position - the vehicle's x / z, the vehicle's vf14 (0x38, its forward axis: the result is not read), the
 * heading . delta (dot1), the delta's squared length normalized into the second dot (dot2 = heading . delta_n), the driver's
 * steering bias +0x60 = (float)side * |dot2|, and the two lazily initialized globals 0xdb4698 = 0.407 (0x93587c) / 0xdb4694 = 20.0
 * (0x935aac) behind flag 0xdb469c. 1 when dot2 < 0.407 and |dot1| > 20; comiss / jbe, so unordered gives 0. */
uint32_t rx_behavior_fix_progress_back_up(rx_addr b, rx_addr pos, uint32_t side) {
    const rx_addr drv = U32(b, 8);
    const float px = F32(pos, 0), pz = F32(pos, 8);
    const rx_addr veh = b1_vehicle(drv);
    float hx = F32(veh, 0x130), hz = F32(veh, 0x138);
    const float hl2 = rx_adds(rx_muls(hz, hz), rx_muls(hx, hx));
    if (hl2 != 0.0f) {
        const float inv = rx_divs(1.0f /* 0x931f24 */, b1_progress_sqrt(hl2));
        hx = rx_muls(hx, inv);
        hz = rx_muls(hz, inv);
    }
    const float dx = rx_subs(px, F32(veh, 0xc0)), dz = rx_subs(pz, F32(veh, 0xc8));
    (void)b1_vehicle_forward(veh);               /* vf14 0x38: called, its result unused */
    const float dot1 = rx_adds(rx_muls(dz, hz), rx_muls(dx, hx));
    const float d2 = rx_adds(rx_muls(dz, dz), rx_muls(dx, dx));
    float nx = dx, nz = dz;
    if (d2 != 0.0f) {
        const float inv = rx_divs(1.0f /* 0x931f24 */, b1_progress_sqrt(d2));
        nx = rx_muls(dx, inv);
        nz = rx_muls(dz, inv);
    }
    const float dot2 = rx_adds(rx_muls(nz, hz), rx_muls(nx, hx));
    SF32(drv, 0x60, rx_muls((float)(int32_t)side, fabsf(dot2)));   /* cvtsi2ss(side) x |dot2| */
    uint32_t fl = U32(0, 0x00db469c);
    if (!(fl & 1)) {
        SF32(0, 0x00db4698, F32(0, 0x0093587c) /* 0.407 */);
        fl |= 1;
        SU32(0, 0x00db469c, fl);
    }
    if (!(fl & 2)) {
        SF32(0, 0x00db4694, F32(0, 0x00935aac) /* 20.0 */);
        fl |= 2;
        SU32(0, 0x00db469c, fl);
    }
    if (!(F32(0, 0x00db4698) > dot2)) return 0;
    if (!(fabsf(dot1) > F32(0, 0x00db4694))) return 0;
    return 1;
}
RX_FUNCTION(0x00523ca0, rx_behavior_fix_progress_back_up)

/* 00523ed0 (ESI = behaviour) -> EAX: the side of the second manoeuvre from the object at +0x5c (its direction +0x50 / +0x58 and
 * edge point +0x20 / +0x28): s = (edge.z * (veh.z - dir.z) + edge.x * (veh.x - dir.x)), the x / z offset normalized into
 * t = (dz_n * edge.x - edge.z * dx_n) after the vehicle's vf14 call. The driver's +0x54 / +0x58 = 1 when (s > 0 and t < 0) or
 * (not s > 0 and t > 0), else -1; comiss / jbe, so unordered on s takes the second test and unordered on t gives -1. */
uint32_t rx_behavior_fix_progress_side(rx_addr b) {
    const rx_addr node = U32(b, 0x5c);
    const float ex = F32(node, 0x20), ez = F32(node, 0x28);
    const rx_addr drv = U32(b, 8);
    const rx_addr veh = b1_vehicle(drv);
    const float dx = rx_subs(F32(veh, 0xc0), F32(node, 0x50));
    const float dz = rx_subs(F32(veh, 0xc8), F32(node, 0x58));
    const float s = rx_adds(rx_muls(ez, dz), rx_muls(ex, dx));
    const float d2 = rx_adds(rx_muls(dz, dz), rx_muls(dx, dx));
    float nx = dx, nz = dz;
    if (d2 > 0.0f) {
        const float inv = rx_divs(1.0f /* 0x931f24 */, b1_progress_sqrt(d2));
        nx = rx_muls(dx, inv);
        nz = rx_muls(dz, inv);
    }
    const float t = rx_subs(rx_muls(nz, ex), rx_muls(ez, nx));
    (void)b1_vehicle_forward(veh);               /* vf14 0x38: called, its result unused */
    SU32(b, 0x54, 0);
    SU32(b, 0x58, 0);
    const uint32_t r = (s > 0.0f) ? (t < 0.0f ? 1u : 0xffffffffu) : (t > 0.0f ? 1u : 0xffffffffu);
    SU32(b, 0x54, r);
    SU32(b, 0x58, r);
    return r;
}
RX_FUNCTION(0x00523ed0, rx_behavior_fix_progress_side)

/* 005240d0 (ESI = behaviour) -> EAX: 1 when the activity is a type-6 one (the series kind from 0x1f on) and the object at +0x5c
 * (its direction +0x50 / +0x54 / +0x58) is less than 10000 (0x931e98) from the vehicle; comiss / jbe: unordered gives 0. */
uint32_t rx_behavior_fix_progress_near(rx_addr b) {
    if (b1_progress_kind(b) != 6) return 0;
    const rx_addr node = U32(b, 0x5c);
    const rx_addr veh = b1_vehicle(U32(b, 8));
    const float dx = rx_subs(F32(veh, 0xc0), F32(node, 0x50));
    const float dy = rx_subs(F32(veh, 0xc4), F32(node, 0x54));
    const float dz = rx_subs(F32(veh, 0xc8), F32(node, 0x58));
    const float d2 = rx_adds(rx_adds(rx_muls(dz, dz), rx_muls(dx, dx)), rx_muls(dy, dy));
    return F32(0, 0x00931e98) /* 10000 */ > d2 ? 1u : 0u;
}
RX_FUNCTION(0x005240d0, rx_behavior_fix_progress_near)

/* vehicle vf14 (0x38): a pointer to its forward axis (thiscall, no arguments) */
static rx_addr b1_vehicle_forward(rx_addr veh) {
    rx_addr fn = race_vfn(veh, 0x38 / 4);
    b1_u32_fn f = RX_FN(fn, b1_u32_fn);
    return f ? f(veh) : B1_ORIG0(fn, RX_THISCALL, veh);
}

/* 00523570 BehaviorFixForwardProgress::vf4 (ECX = behaviour, stack dt; RET 4) -> 1 while a manoeuvre runs. */
uint32_t rx_behavior_fix_progress_update(rx_addr b, float dt) {
    (void)dt;
    rx_addr drv = b1_driver(b), p = b1_player(drv);
    if (U32(U32(p, 0x94), 0x21c) != 2 || U32(b, 4) == 0) return 0;
    rx_addr veh = U32(p, 0x1ac);
    if (U32(veh, 0x280) != 0) { SF32(b, 0x20, 0.0f); return 0; }
    if (F32(drv, 0x3c0) != 0.0f) {
        float vx = F32(veh, 0x160), vy = F32(veh, 0x164), vz = F32(veh, 0x168);
        float s2 = (vx * vx + vy * vy) + vz * vz;
        if (!(F32(b, 0x10) > s2)) {
            SF32(b, 0x20, 0.0f);
        } else {
            float deadline = F32(b, 0x20);
            if (deadline == 0.0f) {
                SF32(b, 0x20, F32(drv, 0x130) + F32(b, 0xc));
            } else if (F32(drv, 0x130) > deadline) {
                SF32(b, 0x40, F32(veh, 0xc0));
                SF32(b, 0x44, F32(veh, 0xc4));
                SF32(b, 0x48, F32(veh, 0xc8));
                rx_addr d = b1_driver(b);
                SF32(b, 0x20, F32(d, 0x130) + F32(b, 0xc));
                if (U32(b, 0x18) == 1) { SU32(b, 0x18, 2); SU32(d, 0x3d0, 1); }
                else SU32(b, 0x18, 1);
                if (U32(b, 0x60) != 0) {
                    rx_addr d2 = b1_driver(b), node = U32(d2, 0x18), v = b1_vehicle(d2);
                    float dot = (F32(node, 0x58) * F32(v, 0x138) + F32(node, 0x54) * F32(v, 0x134)) + F32(node, 0x50) * F32(v, 0x130);
                    if (b1_data(0x009cd99c) /* 0.707 */ > dot) {
                        float dy = F32(v, 0xc4) - F32(node, 4);
                        float dz = F32(v, 0xc8) - F32(node, 8);
                        float dx = F32(v, 0xc0) - F32(node, 0);
                        float l2 = (dz * dz + dy * dy) + dx * dx;
                        if (l2 > 0.0f) {
                            float inv = 1.0f / sqrtf(l2);
                            dx = dx * inv;
                            dz = dz * inv;
                        }
                        float side = dz * F32(node, 0x50) - F32(node, 0x58) * dx;
                        rx_addr fwd = b1_vehicle_forward(b1_vehicle(d2));
                        float fx = F32(fwd, 0), fy = F32(fwd, 4), fz = F32(fwd, 8);
                        float f2 = (fz * fz + fy * fy) + fx * fx;
                        if (f2 > 0.0f) {
                            float inv = 1.0f / sqrtf(f2);
                            fx = inv * fx;
                            fy = fy * inv;
                            fz = fz * inv;
                        }
                        rx_addr n = U32(b1_driver(b), 0x18);
                        float face = (F32(n, 0x58) * fz + F32(n, 0x54) * fy) + F32(n, 0x50) * fx;
                        uint32_t s;
                        if (side > 0.0f) s = b1_data(0x009cd9dc) /* -0.1 */ > face ? 0xffffffffu : 1u;
                        else s = face > 0.1f /* 0x931d94 */ ? 1u : 0xffffffffu;
                        SU32(b, 0x54, s);
                        SU32(b, 0x58, s);
                    } else {
                        SU32(b, 0x54, 0);
                    }
                } else {
                    uint32_t near_;
                    B1_CALL_SP(near_, 0, rx_behavior_fix_progress_near(b));
                    if (near_ != 0) {
                        B1_CALL_SP(near_, 0, rx_behavior_fix_progress_side(b));
                    } else if (U32(b, 0x18) == 1) {
                        SU32(b, 0x58, I32(b, 0x58) == -1 ? 1u : 0xffffffffu);
                    } else {
                        SU32(b, 0x54, I32(b, 0x54) == -1 ? 1u : 0xffffffffu);
                    }
                }
            }
        }
    }
    switch (U32(b, 0x18)) {
    case 1: {
        uint32_t done;
        B1_CALL_SP(done, 4, rx_behavior_fix_progress_steer_away(b, b + 0x40, U32(b, 0x58)));
        if (done == 0) return 1;
        SU32(b1_driver(b), 0x3d0, 1);
        SF32(b1_driver(b), 0x3cc, 0.0f);
        B1_CALL_SP(done, 0, rx_behavior_fix_progress_near(b));
        if (U32(b, 0x60) == 0 && done == 0) {
            SU32(b, 0x18, 2);
            SF32(b, 0x20, F32(b1_driver(b), 0x130) + F32(b, 0xc));
            return 1;
        }
        SU32(b, 0x18, 0); SU32(b, 0x54, 0); SU32(b, 0x58, 0);
        SF32(b, 0x20, 0.0f);
        SF32(b1_driver(b), 0x3cc, 0.0f);
        return 1;
    }
    case 2: {
        uint32_t done;
        B1_CALL_SP(done, 4, rx_behavior_fix_progress_back_up(b, b + 0x40, U32(b, 0x54)));
        if (done == 0) return 1;
        SU32(b, 0x18, 0); SU32(b, 0x54, 0); SU32(b, 0x58, 0);
        SF32(b, 0x20, 0.0f);
        SF32(b1_driver(b), 0x3cc, 0.0f);
        return 1;
    }
    default:
        break;
    }
    switch (U32(b, 0x14)) {
    case 1: {
        uint32_t done;
        B1_CALL_SP(done, 4, rx_behavior_fix_progress_steer_away(b, b + 0x30, U32(b, 0x50)));
        if (done == 0) return 1;
        SF32(b1_driver(b), 0x3cc, 0.0f);
        SU32(b, 0x14, 2);
        SF32(b, 0x1c, F32(b1_driver(b), 0x130) + 10.0f /* 0x92fd88 */);
        return 1;
    }
    case 2: {
        uint32_t done;
        B1_CALL_SP(done, 4, rx_behavior_fix_progress_back_up(b, b + 0x30, U32(b, 0x50)));
        if (done == 0) return 1;
        SU32(b, 0x14, 0); SU32(b, 0x50, 0);
        SF32(b, 0x1c, 0.0f);
        SF32(b1_driver(b), 0x3cc, 0.0f);
        return 1;
    }
    default:
        return 0;
    }
}
RX_FUNCTION(0x00523570, rx_behavior_fix_progress_update)
