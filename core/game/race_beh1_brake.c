/* race_beh1_brake.c -- BehaviorHandleBraking (vtable 0x9401a8): the AI's look-ahead braking.
 * Each frame on the ground above ~20 mph the behaviour casts the rider's path ahead (the flat velocity direction turned by the
 * current steering angle, as long as the tuning's look-ahead length) against one track edge, two frames in a row; when both casts hit
 * the same edge the mean hit fraction (hit distance / look-ahead) sets the driver's throttle scale (driver +0x64): full brake
 * pressure scale 0.1 when the hit is far, 1 - (f - B) / C when it is near (per-skill tables). It never takes control (returns 0).
 * Behaviour: +0x04 enabled, +0x08 driver, +0x0c, +0x10 casts this cycle (0..2), +0x14 + 8 i {hit fraction, edge side} of cast i,
 * +0x30 last hit point, +0x40 mean hit fraction (1.01 = none), +0x44 the edge side braking for (0 = none), +0x48 / +0x4c the
 * objects whose +0x04 (a pause flag) / +0x0c (mode: 0 cast, 1 / 2 other modes not used in races) it follows,
 * +0x50 the object whose +0x0c is the edge side to cast at (-1 = the +0x30 edge, 0 = the node centre line, 1 = the +0x40 edge).
 * (core/notes/race.md "AI behaviours") The mode at +0x4c's +0x0c: 0 = the two edge casts below, 1 / 2 = the centre-line cast cycles
 * 00524a40 / 00524af0 (the product's first ride reaches mode 1). */
#include "race_beh1_extern.h"

/* 005248f0 BehaviorHandleBraking::vf5 (shared by 3 classes): the type id 0x11. */
uint32_t rx_behavior_handle_braking_type(rx_addr self) { (void)self; return 0x11; }
RX_FUNCTION(0x005248f0, rx_behavior_handle_braking_type)

/* 005248d0 BehaviorHandleBraking::vf6 (ECX = behaviour): reset (state 1, no braking edge, mean fraction 1.01, no casts). */
void rx_behavior_handle_braking_reset(rx_addr b) {
    SU32(b, 0xc, 1);
    SF32(b, 0x40, rx_bf(0x3f8147aeu) /* 0x9774cc 1.01 */);
    SU32(b, 0x44, 0);
    SU32(b, 0x10, 0);
}
RX_FUNCTION(0x005248d0, rx_behavior_handle_braking_reset)

/* 00524ce0 (EAX = edge side, stack = behaviour; RET 4) -> xmm0: cast the rider's path against an edge of the track.
 * The ray: from the vehicle position (+0xc0), the flat velocity direction (normalized) rotated about the up axis by the steering
 * angle (Rodrigues: c (U x (U x V)) + s (U x V) + U (U . V), inlined sin / cos), times the look-ahead length (tuning 0x9e39c8).
 * It is tested against the edge segments node -> next of up to 30 units of track from the driver's next node (+0x18); the first hit
 * gives the hit point (y = the edge point's y) into behaviour +0x30 and returns the hit's flat distance / look-ahead length.
 * No hit (or no node): 1.01. */
float rx_behavior_braking_cast(int side, rx_addr b) {
    const rx_addr drv = b1_driver(b);
    rx_addr node = U32(drv, 0x18);
    rx_addr veh = b1_vehicle(drv);
    float dist = 30.0f /* 0x931d98 */;
    float hit[3] = {0.0f, 0.0f, 0.0f};
    float vz = F32(veh, 0x138), vx = F32(veh, 0x130);
    float ray[3] = {vx, 0.0f, vz};                       /* the original's frame +0x30 (V, then the ray end) */
    float len2 = (vx * vx + 0.0f * 0.0f) + vz * vz;
    if (len2 > 0.0f) {
        float inv = 1.0f /* 0x931f24 */ / sqrtf(len2);
        ray[0] = inv * ray[0];
        ray[1] = inv * 0.0f;
        ray[2] = vz * inv;
    }
    const float up[3] = {0.0f, 1.0f, 0.0f};
    float a = b1_vehicle_steer_angle(b1_vehicle(drv));
    float s = b1_sign(a);
    float t = b1_trig_reduce(s * a);
    float t2 = t * t, t4 = t2 * t2;
    float c = -0.0f /* 0x9774a8 */ - b1_cos_poly(t2, t4);
    float sn = b1_sin_poly(t2, t4) * t * s;
    float w[3], x[3];
    b1_cross(ray, up, w);                                /* EAX = V, ECX = U */
    b1_cross(up, w, x);                                  /* EAX = U, ECX = W */
    float d = (ray[0] * 0.0f + ray[1]) + ray[2] * 0.0f; /* U . V */
    float px = d * 0.0f, pz = d * 0.0f;
    float L = b1_tuning(0x009e39c8, drv);
    rx_addr pos = b1_vehicle(drv) + 0xc0;
    float rx_ = (x[0] * c + w[0] * sn + px) * L;
    float ry = (x[1] * c + w[1] * sn + d) * L;
    float rz = (x[2] * c + w[2] * sn + pz) * L;
    ray[0] = F32(pos, 0) + rx_;
    ray[1] = F32(pos, 4) + ry;
    ray[2] = F32(pos, 8) + rz;

    if (side == -1 || side == 0 || side == 1) {
        const uint32_t off = side == -1 ? 0x30 : side == 0 ? 0x00 : 0x40;
        RX_FRAME_ENTER(F, 0x20);                         /* F + 0x00 hit, F + 0x10 the ray end */
        SF32(F, 0, hit[0]); SF32(F, 4, hit[1]); SF32(F, 8, hit[2]);
        SF32(F, 0x10, ray[0]); SF32(F, 0x14, ray[1]); SF32(F, 0x18, ray[2]);
        for (;;) {
            rx_addr next;
            if (node == 0 || (next = U32(node, 0x64)) == 0) break;
            dist = dist - F32(node, 0x4f0);
            uint32_t r = (uint32_t)rx_race_segments_cross_xz(node + off, next + off, b1_vehicle(drv) + 0xc0, F + 0x10, F);
            if (r != 0) break;
            node = next;
            if (!(dist > 0.0f)) goto done;
        }
        if (dist > 0.0f && node != 0 && U32(node, 0x64) != 0) SF32(F, 4, F32(node, off + 4));
    done:
        hit[0] = F32(F, 0); hit[1] = F32(F, 4); hit[2] = F32(F, 8);
        RX_FRAME_LEAVE(F, 0x20);
    }
    float h2 = (hit[0] * hit[0] + hit[1] * hit[1]) + hit[2] * hit[2];
    if (h2 != 0.0f && node != 0 && U32(node, 0x64) != 0) {
        rx_addr v = b1_vehicle(drv);
        float dx = hit[0] - F32(v, 0xc0);
        float dz = hit[2] - F32(v, 0xc8);
        float f = sqrtf(dz * dz + dx * dx) / b1_tuning(0x009e39c8, drv);
        SF32(b, 0x30, hit[0]);
        SF32(b, 0x34, hit[1]);
        SF32(b, 0x38, hit[2]);
        return f;
    }
    return rx_bf(0x3f8147aeu) /* 0x9774cc 1.01 */;
}
RX_FUNCTION(0x00524ce0, rx_behavior_braking_cast)

/* 00524bb0 (EDI = behaviour) -> st(0): the mean hit fraction of the casts (+0x14 + 8 i, count +0x10), summed in pairs as the
 * original (even entries, odd entries in SSE, then on the x87: odd + even + the last odd one, / the count as unsigned, at the
 * main thread's 53-bit precision). */
rx_f80 rx_behavior_braking_mean(rx_addr b) {
    uint32_t n = U32(b, 0x10), i = 0;
    float even = 0.0f, odd = 0.0f, last = 0.0f;
    if ((int32_t)n >= 2) {
        uint32_t pairs = ((n - 2) >> 1) + 1;
        for (uint32_t k = 0; k < pairs; k++) {
            even = F32(b, 0x14 + 16 * k) + even;
            odd = F32(b, 0x1c + 16 * k) + odd;
        }
        i = pairs * 2;
    }
    if (i < n) last = F32(b, 0x14 + 8 * i);
    rx_f80 s = rx_f80_add(rx_f80_from_f32(odd), rx_f80_from_f32(even), 53, 0);
    s = rx_f80_add(s, rx_f80_from_f32(last), 53, 0);
    rx_f80 c = rx_f80_from_int((int32_t)n);
    if ((int32_t)n < 0) c = rx_f80_add(c, rx_f80_from_f32(4294967296.0f /* 0x97731c */), 53, 0);
    return rx_f80_div(s, c, 53, 0);
}
RX_FUNCTION(0x00524bb0, rx_behavior_braking_mean)

/* 00524c50 (xmm0 = mean hit fraction, EDX = behaviour): the throttle scale (driver +0x64) for a hit at that fraction: nothing when
 * the fraction is >= 1 (no hit), 0.1 when it is at least the tuning's far limit (0x9e3ba8), else 1 - (f - B) / C (0x9e4358,
 * 0xa04898). */
void rx_behavior_braking_apply(float f, rx_addr b) {
    if (!(1.0f > f)) return;
    rx_addr drv = b1_driver(b);
    if (f >= b1_tuning(0x009e3ba8, drv)) {               /* comiss; jb (unordered: the formula) */
        SF32(drv, 0x64, 0.1f /* 0x931d94 */);
        return;
    }
    SF32(drv, 0x64, 1.0f - (f - b1_tuning(0x009e4358, drv)) / b1_tuning(0x00a04898, drv));
}
RX_FUNCTION(0x00524c50, rx_behavior_braking_apply)

/* 005249a0 (EAX = behaviour): one cast at the edge side of +0x50's +0x0c; after the second cast, when both were at the same side,
 * brake for that side with their mean fraction, else no braking edge (1.01, side 0); then apply the braking edge if there is one. */
void rx_behavior_braking_cast_step(rx_addr b) {
    int32_t side = I32(U32(b, 0x50), 0xc);
    float f = rx_behavior_braking_cast(side, b);
    SF32(b, 0x14 + 8 * U32(b, 0x10), f);
    SU32(b, 0x18 + 8 * U32(b, 0x10), (uint32_t)side);
    SU32(b, 0x10, U32(b, 0x10) + 1);
    if (U32(b, 0x10) == 2) {
        uint32_t first = U32(b, 0x18), same = 1;
        for (uint32_t i = 0, n = U32(b, 0x10); n != 0; i++, n--) if (first != U32(b, 0x18 + 8 * i)) same = 0;
        if (same != 0) {
            SF32(b, 0x40, rx_f80_to_f32(rx_behavior_braking_mean(b), 0));   /* fstp */
            SU32(b, 0x44, first);
        } else {
            SF32(b, 0x40, rx_bf(0x3f8147aeu) /* 0x9774cc 1.01 */);
            SU32(b, 0x44, 0);
        }
        SU32(b, 0x10, 0);
    }
    if (U32(b, 0x44) != 0) rx_behavior_braking_apply(F32(b, 0x40), b);
}
RX_FUNCTION(0x005249a0, rx_behavior_braking_cast_step)

/* The tail all three cast modes share: record the cast's hit fraction and the side it was cast at (+0x14 + 8 i / +0x18 + 8 i, count
 * +0x10); at the second cast take the mean fraction when both sides are the same, else no edge (1.01, side 0); finally apply the
 * edge when there is one (and, in modes 1 / 2, also for a cast recorded at side 0: the original's `|| ebx == 0`). */
static void braking_after_cast(rx_addr b, float f, uint32_t side) {
    SF32(b, 0x14 + 8 * U32(b, 0x10), f);
    SU32(b, 0x18 + 8 * U32(b, 0x10), side);
    SU32(b, 0x10, U32(b, 0x10) + 1);
    if (U32(b, 0x10) == 2) {
        const uint32_t first = U32(b, 0x18);
        uint32_t same = 1;
        for (uint32_t i = 0, n = U32(b, 0x10); n != 0; i++, n--) if (first != U32(b, 0x18 + 8 * i)) same = 0;
        if (same != 0) {
            SF32(b, 0x40, rx_f80_to_f32(rx_behavior_braking_mean(b), 0));   /* fstp */
            SU32(b, 0x44, first);
        } else {
            SF32(b, 0x40, rx_bf(0x3f8147aeu) /* 0x9774cc 1.01 */);
            SU32(b, 0x44, 0);
        }
        SU32(b, 0x10, 0);
    }
    if (U32(b, 0x44) != 0 || side == 0) rx_behavior_braking_apply(F32(b, 0x40), b);
}

/* 00524a40 (EAX = behaviour): the cast cycle of mode 1 (+0x4c's +0x0c == 1): cast at the +0x50 edge side as 005249a0 does, but when
 * the mode object's own +0x1c side is the side being cast, cast at the centre line instead (side 0, recorded as 0: the tail then
 * applies the fraction even without a common edge, i.e. it brakes for the centre). */
void rx_behavior_braking_cast_center(rx_addr b) {
    const int32_t side = I32(U32(b, 0x50), 0xc);
    if (I32(U32(b, 0x4c), 0x1c) == side) {
        const float f = rx_behavior_braking_cast(0, b);
        braking_after_cast(b, f, 0);
    } else {
        const float f = rx_behavior_braking_cast(side, b);
        braking_after_cast(b, f, (uint32_t)side);
    }
}
RX_FUNCTION(0x00524a40, rx_behavior_braking_cast_center)

/* 00524af0 (EAX = behaviour): the cast cycle of mode 2 (+0x4c's +0x0c == 2): 00526dd0 tells whether the vehicle is past the side
 * +0x4c's +0x1c of the driver's current node (the mode object's own driver); if it is, cast at the centre line (side 0, recorded 0),
 * else at the +0x50 edge side. Same tail as mode 1. */
void rx_behavior_braking_cast_past(rx_addr b) {
    const rx_addr m = U32(b, 0x4c);
    if (rx_behavior_node_side(m, I32(m, 0x1c)) != 0) {
        const float f = rx_behavior_braking_cast(0, b);
        braking_after_cast(b, f, 0);
    } else {
        const int32_t side = I32(U32(b, 0x50), 0xc);
        const float f = rx_behavior_braking_cast(side, b);
        braking_after_cast(b, f, (uint32_t)side);
    }
}
RX_FUNCTION(0x00524af0, rx_behavior_braking_cast_past)

/* 00524900 BehaviorHandleBraking::vf4 (ECX = behaviour, stack dt; RET 4) -> 0 (never takes control).
 * Off (reset) while airborne, below 29.3332 units/s (20 mph) or while +0x48's +0x04 is set; else by +0x4c's mode: 0 = the edge
 * casts (005249a0), 1 / 2 = the centre-line cycles 00524a40 / 00524af0. */
uint32_t rx_behavior_handle_braking_update(rx_addr b, float dt) {
    (void)dt;
    if (U32(b, 4) == 0) return 0;
    rx_addr veh = b1_vehicle(b1_driver(b));
    if (b1_vehicle_airborne(veh) != 0 || b1_data(0x009cd9b8) /* 29.3332 */ > F32(b1_vehicle(b1_driver(b)), 0x150) ||
        U32(U32(b, 0x48), 4) != 0) {
        b1_behavior_reset(b);
        return 0;
    }
    uint32_t mode = U32(U32(b, 0x4c), 0xc);
    if (mode == 0) rx_behavior_braking_cast_step(b);
    else if (mode == 1) rx_behavior_braking_cast_center(b);
    else if (mode == 2) rx_behavior_braking_cast_past(b);
    return 0;
}
RX_FUNCTION(0x00524900, rx_behavior_handle_braking_update)
