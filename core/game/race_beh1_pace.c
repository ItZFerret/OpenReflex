/* race_beh1_pace.c -- BehaviorLapPacing (vtable 0x93f950): the AI's catch-up / slow-down ("rubber band").
 * The driver has a target lap time (+0x10bc, adjusted by the difficulty when the behaviour resets) and the distance left to the
 * finish line (+0x10cc). Each frame (when pacing is on, global 0xae1b9c, after 3 s of the lap and not airborne for more than 0.2 s)
 * the behaviour compares the speed the rider has averaged this lap with the speed needed to finish on time and puts the rider in a
 * pacing state: 1 = too slow (0.9 / 0.95 / 1.0 below), 2 = too fast (1.0 / 1.05 / 1.1 above, by the pacing class +0x1c), 0 = on
 * pace. Too slow raises the vehicle's tuning (RavenVehicle_SetTuning: the engine values +0x40 / +0x44 up to 100, the +0x50 / +0x54
 * values down to 0, in proportion to the speed deficit, capped 50 / 100 / 150 by class); too fast is 00525cd0 (never in the reference
 * worlds). Pacing is suspended (state kept) where the track wants the rider at its line speed (00525930). It never takes control.
 * Behaviour: +0x0c needed speed (distance left / time left), +0x10 average speed this lap, +0x14 why pacing is suspended (1 near the
 * line speed / steering, 2 / 3 node flags 0x4000 / 4 in states 2 / 1), +0x18 the output scale (1), +0x1c pacing class (0 fast lap
 * target, 1, 2: by target - best lap time), +0x20 lap clock, +0x24 a line override object (+0x54 active, +0x3c / +0x40 its nodes),
 * +0x28 wait for the next lap (a section flag 4), +0x30 state, +0x34 previous state, +0x38 tuning column, +0x3c the vehicle's tuning
 * record (+0xa0..+0xbc copied to +0x40..+0x5c: the base tuning), +0x60 airborne time. (core/notes/race.md "AI behaviours") */
#include "loop_player.h"
#include "loop_vehicle.h"
#include "race_beh1_extern.h"

/* 005254e0 BehaviorLapPacing::vf6 (ECX = behaviour): reset. Without a target lap time the behaviour is disabled. The target is
 * adjusted by the skill (player +0x8f4) and level (+0x8f8): skill 0: +0.25 / -0.5 (levels 0 / 2), skill 1: +1.5 / -1, skills 2 / 3:
 * +2 / +1 (levels 0 / 1). Pacing class by target - best (+0x10c0): > 2 -> 2, < -2 -> 0, else 1. The base tuning is the vehicle's
 * record in the game's list (game +0x154d8, count +0x154dc) whose +0x04 is the player's +0x1b0. */
void rx_behavior_lap_pacing_reset(rx_addr b) {
    rx_addr drv = U32(b, 8);
    float tl = F32(drv, 0x10bc);
    if (tl == 0.0f) { SU32(b, 4, 0); return; }
    rx_addr p = U32(drv, 0x74);
    uint32_t skill = U32(p, 0x8f4), level = U32(p, 0x8f8);
    int set = 1;
    switch (skill) {
    case 0:
        if (level == 0) tl = tl + 0.25f; else if (level == 2) tl = tl - 0.5f; else set = 0;
        break;
    case 1:
        if (level == 0) tl = tl + 1.5f; else if (level == 2) tl = tl - 1.0f; else set = 0;
        break;
    case 2: case 3:
        if (level == 0) tl = tl + 2.0f; else if (level == 1) tl = tl + 1.0f; else set = 0;
        break;
    default: set = 0; break;
    }
    if (set) SF32(drv, 0x10bc, tl);
    SF32(b, 0x20, 0.0f);
    SF32(b, 0x18, 1.0f);
    SF32(b, 0xc, 0.0f);
    SF32(b, 0x10, 0.0f);
    float diff = F32(drv, 0x10bc) - F32(drv, 0x10c0);
    if (diff > 2.0f) SU32(b, 0x1c, 2);
    else if (b1_data(0x009d7b30) /* -2 */ > diff) SU32(b, 0x1c, 0);
    else SU32(b, 0x1c, 1);
    rx_addr sec = U32(U32(b, 8), 0x14);
    if (sec != 0 && (U8(sec, 0x118) & 4)) SU32(b, 0x28, 1);
    rx_addr game = U32(0, 0x00a946b8);
    SU32(b, 0x14, 0);
    SU32(b, 0x34, 0);
    SU32(b, 0x30, 0);
    int32_t n = I32(game, 0x154dc);
    rx_addr list = U32(game, 0x154d8), rec = 0;
    for (int32_t i = 0; i < n; i++) {
        rx_addr e = U32(list, 4 * (uint32_t)i);
        if (e != 0 && U32(e, 4) == U32(U32(drv, 0x74), 0x1b0)) { rec = e; break; }
    }
    SU32(b, 0x3c, rec);
    for (uint32_t k = 0; k < 8; k++) SU32(b, 0x40 + 4 * k, U32(U32(b, 0x3c), 0xa0 + 4 * k));
    SF32(b, 0x60, 0.0f);
}
RX_FUNCTION(0x005254e0, rx_behavior_lap_pacing_reset)

/* 00525790 (EAX = behaviour): at a new lap of a section with flag 4 (driver section +0x118) after waiting for it (+0x28): the lap
 * clock restarts at the time the target pace gives for the distance already covered: (lap length - left) / lap length * target. */
void rx_behavior_lap_pacing_new_lap(rx_addr b) {
    rx_addr drv = U32(b, 8);
    if (U8(U32(drv, 0x14), 0x118) & 4) return;
    if (U32(b, 0x28) == 0) return;
    SF32(b, 0x20, 0.0f);
    SU32(b, 0x28, 0);
    float len = F32(U32(drv, 0x10), 0x1dc);
    SF32(b, 0x20, (len - F32(drv, 0x10cc)) / len * F32(drv, 0x10bc));
}
RX_FUNCTION(0x00525790, rx_behavior_lap_pacing_new_lap)

/* 00525ac0 (EAX = behaviour): the pacing state from average / needed speed: below the class's low limit 1 (too slow), above its
 * high limit 2 (too fast), else 0; the previous state into +0x34. Limits: class 2 [0.9, 1], class 0 [1, 1.1], else [0.95, 1.05]. */
void rx_behavior_lap_pacing_classify(rx_addr b) {
    float r = F32(b, 0x10) / F32(b, 0xc);
    float hi = rx_bf(0x3f866666u) /* 0x9774c8 1.05 */, lo = b1_data(0x009cd9ac) /* 0.95 */;
    uint32_t cls = U32(b, 0x1c);
    if (cls == 2) { hi = 1.0f; lo = b1_data(0x009cd970) /* 0.9 */; }
    else if (cls == 0) { hi = rx_bf(0x3f8ccccdu) /* 0x9774c4 1.1 */; lo = 1.0f; }
    SU32(b, 0x34, U32(b, 0x30));
    if (lo > r) SU32(b, 0x30, 1);
    else if (r > hi) SU32(b, 0x30, 2);
    else SU32(b, 0x30, 0);
}
RX_FUNCTION(0x00525ac0, rx_behavior_lap_pacing_classify)

/* 00525930 (ESI = behaviour) -> EAX: is pacing suspended here (and why, +0x14)? With the segment's line speeds for the vehicle
 * class (node +0xe8 + 4 (4 class + kind), -1 = none) at the line override's nodes (+0x24 when active) or the driver's previous /
 * next node: 1 when a line speed is set and the rider is not both slower than 1.03 x a line speed and steering at most 1 degree;
 * else 2 / 3 when the previous node is flagged 0x4000 / 4 and the state is 2 / 1; else 0. */
uint32_t rx_behavior_lap_pacing_suspended(rx_addr b) {
    rx_addr o = U32(b, 0x24), na, nb;
    if (o != 0 && U32(o, 0x54) != 0) { na = U32(o, 0x3c); nb = U32(o, 0x40); }
    else { na = U32(U32(b, 8), 0x18); nb = U32(U32(b, 8), 0x1c); }
    rx_addr veh = b1_vehicle(U32(b, 8));
    uint32_t k = (U32(veh, 0x198) + U32(veh, 0x194) * 4u) * 4u + 0xe8u;
    float sa = F32(na + k, 0), sb = F32(nb + k, 0);
    float sa3 = sa * b1_data(0x009cf5a8) /* 0.03 */, sb3 = sb * b1_data(0x009cf5a8);
    uint32_t slow = 0;
    /* fmul m32 / fstp m32 at the thread's 53-bit precision */
    float deg = rx_f80_to_f32(rx_f80_mul(b1_vehicle_steer_angle_x87(veh), rx_f80_from_f32(rx_bf(0x42652ee1u) /* 0x977840 57.29578 */),
                                         53, 0), 0);
    uint32_t steering = fabsf(deg) > 1.0f ? 1u : 0u;
    rx_addr drv = U32(b, 8);
    float speed = F32(b1_vehicle(drv), 0x150);
    if ((sa3 + sa > speed || sb3 + sb > speed) && steering == 0) slow = 1;
    if ((sa > b1_data(0x009cd9e8) /* -1 */ || sb > b1_data(0x009cd9e8)) && slow == 0) { SU32(b, 0x14, 1); return 1; }
    rx_addr v = b1_vehicle(drv);
    uint32_t flags = U32(na, (U32(v, 0x198) + U32(v, 0x194) * 4u) * 4u + 0x68u);
    if ((flags & 0x4000) && U32(b, 0x30) == 2) { SU32(b, 0x14, 2); return 1; }
    v = b1_vehicle(drv);
    flags = U32(na, (U32(v, 0x198) + U32(v, 0x194) * 4u) * 4u + 0x68u);
    if ((flags & 4) && U32(b, 0x30) == 1) { SU32(b, 0x14, 3); return 1; }
    SU32(b, 0x14, 0);
    return 0;
}
RX_FUNCTION(0x00525930, rx_behavior_lap_pacing_suspended)

/* 00525b30 (EAX = behaviour): too slow: the vehicle's tuning from the base tuning (+0x40..+0x5c) raised by the speed deficit
 * d = min(needed - average, 50 / 100 / 150 by class 1 / 0 / 2) x the tuning column's scale (0x9c55b0[class + 8 column]):
 * [0] / [1] = min(base + trunc(d / f x scale), 100) with f = 1.75 / 1 ([0], fast / slow rider: speed > 40) and 1 / 1.75 ([1]),
 * [4] = max(base - trunc(d x scale x 0.5), 0), [5] = max(base - trunc(d x scale x -0.476), 0), the rest unchanged;
 * RavenVehicle_SetTuning 0045f270 (ESI = new tuning, EDI = vehicle, stack base tuning; RET 4). */
void rx_behavior_lap_pacing_speed_up(rx_addr b) {
    float lim = b1_data(0x009d78dc) /* 50 */;
    uint32_t cls = U32(b, 0x1c);
    rx_addr drv = U32(b, 8);
    if (cls == 2) lim = b1_data(0x009d7b34) /* 150 */;
    else if (cls == 0) lim = 100.0f /* 0x93005c */;
    uint32_t vclass = U32(b1_vehicle(drv), 0x194);
    float d = F32(b, 0xc) - F32(b, 0x10);
    float scale = F32(0x009c55b0, 4u * (vclass + U32(b, 0x38) * 8u));
    if (d > lim) d = lim;
    rx_addr veh = b1_vehicle(drv);
    float speed = F32(veh, 0x150);
    const float fast = b1_data(0x009cc37c) /* 40 */, f175 = b1_data(0x009cd9a4) /* 1.75 */;
    int32_t t0 = rx_cvtt(d / (speed > fast ? f175 : 1.0f) * scale) + I32(b, 0x40);
    if (t0 > 100) t0 = 100;
    int32_t t1 = rx_cvtt(d / (speed > fast ? 1.0f : f175) * scale) + I32(b, 0x44);
    if (t1 > 100) t1 = 100;
    float ds = d * scale;
    int32_t t4 = I32(b, 0x50) - rx_cvtt(ds * 0.5f /* 0x9322cc */);
    if (t4 < 0) t4 = 0;
    int32_t t5 = I32(b, 0x54) - rx_cvtt(ds * rx_bf(0xbef3cf3eu) /* 0x97799c -0.4761905 */);
    if (t5 < 0) t5 = 0;
    RX_FRAME_ENTER(F, 0x20);
    SU32(F, 0x00, (uint32_t)t0);
    SU32(F, 0x04, (uint32_t)t1);
    SU32(F, 0x08, U32(b, 0x48));
    SU32(F, 0x0c, U32(b, 0x4c));
    SU32(F, 0x10, (uint32_t)t4);
    SU32(F, 0x14, (uint32_t)t5);
    SU32(F, 0x18, U32(b, 0x58));
    SU32(F, 0x1c, U32(b, 0x5c));
    RACE_CALL_SP(4, rx_vehicle_set_tuning(veh, F, b + 0x40));   /* 0045f270 (EDI = veh, ESI = cur; stack def) */
    RX_FRAME_LEAVE(F, 0x20);
}
RX_FUNCTION(0x00525b30, rx_behavior_lap_pacing_speed_up)

/* 005256d0 (EAX = behaviour, xmm1 = gap to the best human lap; no result): the AI's time base +0x10bc (driver) moves by the gap
 * clamped to +-5 and, by the difficulty tier (+0x8f4: 1 = once, 2 = twice, 3 = three times), by a third of the vehicle's current
 * time-table step (the activity's +0xbc4 / +0xbd0 pair of the vehicle's row +0x198 + 4 * +0x194, 0x14 apart): tier 3 adds it
 * thrice, 2 twice, 1 once, others not at all. */
void rx_behavior_lap_pacing_gap(rx_addr b, float gap) {
    const rx_addr d = U32(b, 8), player = U32(d, 0x74), veh = U32(player, 0x1ac);
    const uint32_t row = U32(veh, 0x198) + U32(veh, 0x194) * 4;
    const rx_addr act = U32(player, 0x94), rec = act + 0x14 * row;
    const float step = rx_muls(rx_subs(F32(rec, 0xbd0), F32(rec, 0xbc4)), 0.33333334f /* 0x92e214 */);
    if (gap > 5.0f /* 0x935890 */) gap = 5.0f;
    else if (-5.0f /* 0x97739c */ > gap) gap = -5.0f;
    float acc = rx_adds(gap, F32(d, 0x10bc));
    SF32(d, 0x10bc, acc);
    const uint32_t tier = U32(player, 0x8f4);
    if (tier == 3) { acc = rx_adds(acc, step); SF32(d, 0x10bc, acc); }
    if (tier == 3 || tier == 2) { acc = rx_adds(step, F32(d, 0x10bc)); SF32(d, 0x10bc, acc); }
    if (tier == 3 || tier == 2 || tier == 1) SF32(d, 0x10bc, rx_adds(step, F32(d, 0x10bc)));
}
RX_FUNCTION(0x005256d0, rx_behavior_lap_pacing_gap)

/* 00525d60 (xmm0 = fraction, stack behaviour; RET 4): the start of a race: pacing is held at a fraction of the base tuning:
 * for 0 < fraction <= 1 the vehicle's tuning gets [0] and [4] = min(trunc(fraction x 100), 100) (the others from the base tuning
 * +0x40..+0x5c) and the behaviour's +0x2c becomes 0; otherwise the base tuning itself is applied. RavenVehicle_SetTuning 0045f270
 * (ESI = new tuning, EDI = vehicle, stack base tuning; RET 4). Frame: sub esp, 0x20; push ebx esi edi (the new tuning at +0xc). */
void rx_behavior_lap_pacing_start(rx_addr b, float fraction) {
    RACE_FRAME_PLAIN(F, 0x20, 3);
    rx_addr veh;
    if (0.0f >= fraction || fraction > 1.0f /* 0x931f24 */) {
        veh = b1_vehicle(U32(b, 8));
        RACE_CALL_SP(4, rx_vehicle_set_tuning(veh, b + 0x40, b + 0x40));
        RACE_FRAME_END(F);
        return;
    }
    int32_t n = rx_cvtt(rx_muls(fraction, 100.0f /* 0x93005c */));
    if (n > 100) n = 100;
    const rx_addr R = F + 0xc;
    SU32(R, 0x00, (uint32_t)n);
    SU32(R, 0x04, U32(b, 0x44));
    SU32(R, 0x08, U32(b, 0x48));
    SU32(R, 0x0c, U32(b, 0x4c));
    SU32(R, 0x10, (uint32_t)n);
    SU32(R, 0x14, U32(b, 0x54));
    SU32(R, 0x18, U32(b, 0x58));
    SU32(R, 0x1c, U32(b, 0x5c));
    veh = b1_vehicle(U32(b, 8));
    RACE_CALL_SP(4, rx_vehicle_set_tuning(veh, R, b + 0x40));
    SU32(b, 0x2c, 0);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00525d60, rx_behavior_lap_pacing_start)

/* 005257e0 BehaviorLapPacing::vf4 (ECX = behaviour, stack dt; RET 4) -> 0. */
uint32_t rx_behavior_lap_pacing_update(rx_addr b, float dt) {
    float t = dt + F32(b, 0x20);
    SF32(b, 0x20, t);
    if (U32(0, 0x00ae1b9c) == 0 || U32(b, 4) == 0 || U32(U32(U32(b, 8), 0x10), 0x1ec) != 0 || U32(b, 0x28) != 0 ||
        3.0f /* 0x92fe48 */ > t)
        goto off;
    if (b1_vehicle_airborne(b1_vehicle(U32(b, 8))) != 0) {
        float a = dt + F32(b, 0x60);
        SF32(b, 0x60, a);
        if (a > 0.2f /* 0x92e388 */) goto off;
    } else {
        SF32(b, 0x60, 0.0f);
    }
    rx_addr drv = U32(b, 8);
    float lap = F32(b, 0x20);
    SF32(b, 0x18, 1.0f);
    float left = F32(drv, 0x10bc) - lap;
    if (1.0f > left) left = 1.0f;
    SF32(b, 0xc, F32(drv, 0x10cc) / left);
    SF32(b, 0x10, (F32(U32(drv, 0x10), 0x1dc) - F32(drv, 0x10cc)) / lap);
    rx_behavior_lap_pacing_classify(b);
    if (rx_behavior_lap_pacing_suspended(b) != 0) goto off;
    rx_addr nav = rx_driver_navigate_behavior(U32(b, 8));
    if (nav != 0 && U32(nav, 0x4c) != 0) goto off;
    switch (U32(b, 0x30)) {
    case 1: rx_behavior_lap_pacing_speed_up(b); break;
    case 2: RACE_CALL_SP(0, rx_activity_apply_tuning(b)); break;
    default: break;
    }
    return 0;
off:
    SF32(b, 0x18, 1.0f);
    return 0;
}
RX_FUNCTION(0x005257e0, rx_behavior_lap_pacing_update)
