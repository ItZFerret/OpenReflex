/* race_beh2_line.c -- the lane keepers: BehaviorTakeLine (vtable 0x0093f978) and BehaviorSplitSection (0x0093fc70).
 *
 * Both keep the rider inside a lane of the track segment it is on (driver +0x18 -> +0x1c: the track nodes, +0x00 centre, +0x10
 * left edge, +0x20 right edge). For each of the lane's two borders (TakeLine: the lane chosen by driver +0x314, SplitSection: by
 * +0x310; lane 1 = centre line .. right edge, lane -1 = left edge .. centre line, else the whole track), with r = the rider minus
 * the closest point of the border (flat), they push the steering (driver +0x60) when the rider is outside the border (the sign of
 * r x border) and heading out (the heading's dot with the border normal < 0):
 *     push = -(dot - 0.5) * (A / |r|^a - B / |r|^b) along r / |r|, taken into the vehicle's frame (x),
 * a repulsion with per-driver tuning (TakeLine 0x009e3108 + 16 * index: B, b, A, a; SplitSection 0x009e3a28 ..). TakeLine also
 * needs speed (above 0x009e3968) and a free lane (0050a8b0 against the lane's width 5, once: +0x18), and pauses while the
 * separation behaviour (+0x10) passes or for its hold (+0x1c). Layout TakeLine: +0x04 enabled, +0x08 driver, +0x0c the navigate
 * behaviour (its state +0xc, +0x10), +0x10 BehaviorSeperate, +0x14 lane blocked, +0x18 lane checked, +0x1c hold until, +0x20 why
 * not active (1 slow, 2 passing / hold, 3 blocked). (notes_beh2.md) */
#include "race_beh2_extern.h"

/* The repulsion A / d^a - B / d^b with the four tuning floats at `t` (+0 B, +4 b, +8 A, +0xc a); CRT pow in double. */
static float repulsion(rx_addr t, float dist) {
    const float a = rx_divs(F32(0, t + 8), (float)race_pow((double)dist, (double)F32(0, t + 0xc)));
    const float b = rx_divs(F32(0, t + 0), (float)race_pow((double)dist, (double)F32(0, t + 4)));
    return rx_subs(a, b);
}
/* The push of one border into the steering: k = -0 - (dot - 0.5) * pot along (nx, 0, nz), in the vehicle frame, x added to
 * driver +0x60. */
static void push_steer(rx_addr beh, rx_addr vehicle, rx_addr tmp, float dot, float pot, float nx, float nz) {
    const float k = rx_subs(-0.0f /* 0x9774a8 */, rx_muls(rx_subs(dot, 0.5f /* 0x9322cc */), pot));
    SF32(tmp, 0, rx_muls(nx, k)); SF32(tmp, 4, 0.0f); SF32(tmp, 8, rx_muls(nz, k));
    rx_race_to_vehicle_frame(vehicle, tmp);
    const rx_addr d = U32(beh, 8);
    SF32(d, 0x60, rx_adds(F32(tmp, 0), F32(d, 0x60)));
}

/* 005344e0 BehaviorTakeLine::vf4 (ECX = behaviour; RET 4, unused) -> EAX 0: keep to the lane of driver +0x314 (see above). */
uint32_t rx_behavior_take_line_update(rx_addr beh, float dt) {
    (void)dt;   /* the vf4 argument (unused here) */
    rx_addr nav = U32(beh, 0xc);
    if (nav != 0 && (U32(nav, 0xc) == 0x1b || U32(nav, 0xc) == 3)) { SU32(nav, 0x10, 0); SU32(nav, 0xc, 3); }
    SU32(beh, 0x20, 0);
    rx_addr d = U32(beh, 8);
    if (U32(beh, 4) == 0 || U32(d, 0x314) == 0) {
        SU32(beh, 0x18, 0);
        SU32(beh, 4, 0);
        return 0;
    }
    rx_addr v = beh2_vehicle(d);
    if (beh2_tune_f(0x009e3968, d) > F32(v, 0x150)) { SU32(beh, 0x20, 1); return 0; }
    const rx_addr sep = U32(beh, 0x10);
    if (sep != 0 && (U32(sep, 0x7c) == 2 || U32(sep, 0x7c) == 3 || F32(beh, 0x1c) > F32(d, 0x130))) {
        SU32(beh, 0x20, 2);
        return 0;
    }
    v = beh2_vehicle(d);
    const float mx = F32(v, 0xc0), mz = F32(v, 0xc8);
    float fx = F32(v, 0x130), fz = F32(v, 0x138);
    const float flen2 = fz * fz + fx * fx;
    if (flen2 != 0.0f) {
        const float inv = 1.0f /* 0x931f24 */ / sqrtf(flen2);
        fx = inv * fx;
        fz = fz * inv;
    }
    RX_FRAME_ENTER(F, 0x60);   /* +0x00 left, +0x10 right, +0x20 centre / border 1, +0x30 push 1, +0x40 border 2, +0x50 push 2 */
    /* the last unit offset (the original's stack slot; before any, the right edge's closest point x / y) */
    float nx = 0.0f, nz = 0.0f;
    if (U32(beh, 0x18) == 0) {
        const rx_addr pos = beh2_vehicle(d) + 0xc0, next = U32(d, 0x1c);
        rx_track_project_clamped_xz(U32(d, 0x18) + 0x10, next + 0x10, pos, F);
        rx_track_project_clamped_xz(U32(d, 0x18) + 0x20, next + 0x20, pos, F + 0x10);
        rx_track_project_clamped_xz(U32(d, 0x18), next, pos, F + 0x20);
        const float cx = mx - F32(F, 0x20), cz = mz - F32(F, 0x28);
        float dist = sqrtf(cx * cx + cz * cz);
        const int32_t lane = I32(d, 0x314);
        if (lane == -1 || lane == 1) {
            const rx_addr e = lane == -1 ? F : F + 0x10;
            const float ex = F32(e, 0) - F32(F, 0x20), ez = F32(e, 8) - F32(F, 0x28), ey = F32(e, 4) - F32(F, 0x24);
            dist = sqrtf((ex * ex + ez * ez) + ey * ey) * 0.75f /* 0x935aa8 */ + dist;
        }
        nx = F32(F, 0x10); nz = F32(F, 0x14);
        const int blocked = rx_race_rider_near(d, lane, dist, 5.0f /* 0x935890 */);
        SU32(beh, 0x14, (uint32_t)blocked);
        if (blocked) {
            SU32(beh, 0x20, 3);
            RX_FRAME_LEAVE(F, 0x60);
            return 0;
        }
    }
    nav = U32(beh, 0xc);
    if (nav != 0) { SU32(nav, 0x10, 1); SU32(nav, 0xc, 3); }
    d = U32(beh, 8);
    /* border 1: the centre line in lane 1, else the left edge */
    v = beh2_vehicle(d);
    const uint32_t o1 = I32(d, 0x314) == 1 ? 0 : 0x10;
    rx_track_project_clamped_xz(U32(d, 0x18) + o1, U32(d, 0x1c) + o1, v + 0xc0, F + 0x20);
    float ex = F32(U32(d, 0x1c), o1) - F32(U32(d, 0x18), o1), ez = F32(U32(d, 0x1c), o1 + 8) - F32(U32(d, 0x18), o1 + 8);
    float rx = mx - F32(F, 0x20), rz = mz - F32(F, 0x28);
    float r2 = rx * rx + rz * rz;
    if (r2 != 0.0f) {
        const float inv = 1.0f / sqrtf(r2);
        nx = rx * inv;
        nz = rz * inv;
    }
    const uint32_t t = 0x009e3108 + 16u * beh2_tune(d);
    float pot = repulsion(t, sqrtf(r2));
    {
        const float q = -0.0f /* 0x9774a8 */ - ex;
        const float l2 = ez * ez + q * q;
        float px = ez, pz = q;
        if (l2 != 0.0f) {
            const float inv = 1.0f / sqrtf(l2);
            pz = q * inv;
            px = inv * ez;
        }
        const float dot = pz * fz + px * fx;
        const float cross = rz * ex - rx * ez;
        if (0.0f > cross && 0.0f > dot) push_steer(beh, v, F + 0x30, dot, pot, nx, nz);
    }
    /* border 2: the centre line in lane -1, else the right edge */
    d = U32(beh, 8);
    v = beh2_vehicle(d);
    const uint32_t o2 = I32(d, 0x314) == -1 ? 0 : 0x20;
    rx_track_project_clamped_xz(U32(d, 0x18) + o2, U32(d, 0x1c) + o2, v + 0xc0, F + 0x40);
    ex = F32(U32(d, 0x1c), o2) - F32(U32(d, 0x18), o2);
    ez = F32(U32(d, 0x1c), o2 + 8) - F32(U32(d, 0x18), o2 + 8);
    rx = mx - F32(F, 0x40); rz = mz - F32(F, 0x48);
    r2 = rx * rx + rz * rz;
    if (r2 != 0.0f) {
        const float inv = 1.0f / sqrtf(r2);
        nx = rx * inv;
        nz = rz * inv;
    }
    pot = repulsion(0x009e3108 + 16u * beh2_tune(d), sqrtf(r2));
    {
        const float q = -0.0f - ez;
        const float l2 = ex * ex + q * q;
        float px = ex, pz = q;
        if (l2 != 0.0f) {
            const float inv = 1.0f / sqrtf(l2);
            pz = inv * q;
            px = inv * ex;
        }
        const float dot = px * fz + pz * fx;
        const float cross = rz * ex - rx * ez;
        if (cross > 0.0f && 0.0f > dot) push_steer(beh, v, F + 0x50, dot, pot, nx, nz);
    }
    RX_FRAME_LEAVE(F, 0x60);
    return 0;
}
RX_FUNCTION(0x005344e0, rx_behavior_take_line_update)

/* 00534470 (ECX = behaviour) -> EAX: does TakeLine want to run (enabled, a lane, not blocked, fast enough, past its hold)? */
uint32_t rx_behavior_take_line_ready(rx_addr beh) {
    if (U32(beh, 4) == 0) return 0;
    const rx_addr d = U32(beh, 8);
    if (U32(d, 0x314) == 0 || U32(beh, 0x14) != 0) return 0;
    if (!(F32(beh2_vehicle(d), 0x150) > beh2_tune_f(0x009e3968, d))) return 0;
    if (!(F32(d, 0x130) > F32(beh, 0x1c))) return 0;
    return 1;
}
RX_FUNCTION(0x00534470, rx_behavior_take_line_ready)

/* The closest point of the border a -> b (node offset o) to (mx, mz), inline in 00533970 (clamped like 005389c0). */
static void border_point(rx_addr a, rx_addr b, uint32_t o, float mx, float mz, float* cx, float* cz) {
    const float ax = F32(a, o), az = F32(a, o + 8);
    const float dx = F32(b, o) - ax, dz = F32(b, o + 8) - az;
    float u = ((mx - ax) * dx + (mz - az) * dz) / (dx * dx + dz * dz);
    if (1.0f /* 0x931f24 */ > u) { if (0.0f > u) u = 0.0f; }
    else u = 1.0f;
    *cx = dx * u + ax;
    *cz = dz * u + az;
}

/* 00533970 BehaviorSplitSection::vf4 (ECX = behaviour; RET 4, unused) -> EAX 0: keep to the lane of driver +0x310 at a split of
 * the track (see above; no speed or free-lane checks). Without a lane it disables itself. The second border's side test uses the
 * first border's direction (as the original). */
uint32_t rx_behavior_split_section_update(rx_addr beh, float dt) {
    (void)dt;   /* the vf4 argument (unused here) */
    rx_addr d = U32(beh, 8);
    const rx_addr nav = U32(beh, 0xc);
    if (U32(d, 0x310) == 0) {
        if (nav != 0 && (U32(nav, 0xc) == 0x1b || U32(nav, 0xc) == 4)) { SU32(nav, 0x10, 0); SU32(nav, 0xc, 4); }
        SU32(beh, 4, 0);
        return 0;
    }
    if (nav != 0) { SU32(nav, 0x10, 1); SU32(nav, 0xc, 4); }
    d = U32(beh, 8);
    rx_addr v = beh2_vehicle(d);
    const float mx = F32(v, 0xc0), mz = F32(v, 0xc8);
    float fx = F32(v, 0x130), fz = F32(v, 0x138);
    const float flen2 = fx * fx + fz * fz;
    if (flen2 != 0.0f) {
        const float inv = 1.0f /* 0x931f24 */ / sqrtf(flen2);
        fx = inv * fx;
        fz = fz * inv;
    }
    RX_FRAME_ENTER(F, 0x20);   /* +0x00 push 1, +0x10 push 2 */
    float nx = 0.0f, nz = 0.0f;   /* the original's stack slots (uninitialised before the first offset) */
    /* border 1: the centre line in lane 1, else the left edge */
    v = beh2_vehicle(d);
    const uint32_t o1 = I32(d, 0x310) == 1 ? 0 : 0x10;
    float cx, cz;
    border_point(U32(d, 0x18), U32(d, 0x1c), o1, F32(v, 0xc0), F32(v, 0xc8), &cx, &cz);
    const float ex1 = F32(U32(d, 0x1c), o1) - F32(U32(d, 0x18), o1);
    const float ez1 = F32(U32(d, 0x1c), o1 + 8) - F32(U32(d, 0x18), o1 + 8);
    float rx = mx - cx, rz = mz - cz;
    float r2 = rx * rx + rz * rz;
    if (r2 != 0.0f) {
        const float inv = 1.0f / sqrtf(r2);
        nx = rx * inv;
        nz = rz * inv;
    }
    float pot = repulsion(0x009e3a28 + 16u * beh2_tune(d), sqrtf(r2));
    {
        const float q = -0.0f /* 0x9774a8 */ - ex1;
        const float l2 = q * q + ez1 * ez1;
        float px = ez1, pz = q;
        if (l2 != 0.0f) {
            const float inv = 1.0f / sqrtf(l2);
            pz = q * inv;
            px = inv * ez1;
        }
        const float dot = px * fx + pz * fz;
        const float cross = rz * ex1 - rx * ez1;
        if (0.0f > cross && 0.0f > dot) push_steer(beh, v, F, dot, pot, nx, nz);
    }
    /* border 2: the centre line in lane -1, else the right edge */
    d = U32(beh, 8);
    v = beh2_vehicle(d);
    const uint32_t o2 = I32(d, 0x310) == -1 ? 0 : 0x20;
    border_point(U32(d, 0x18), U32(d, 0x1c), o2, F32(v, 0xc0), F32(v, 0xc8), &cx, &cz);
    const float ex2 = F32(U32(d, 0x1c), o2) - F32(U32(d, 0x18), o2);
    const float ez2 = F32(U32(d, 0x1c), o2 + 8) - F32(U32(d, 0x18), o2 + 8);
    rx = mx - cx; rz = mz - cz;
    r2 = rz * rz + rx * rx;
    if (r2 != 0.0f) {
        const float inv = 1.0f / sqrtf(r2);
        nx = rx * inv;
        nz = rz * inv;
    }
    pot = repulsion(0x009e3a28 + 16u * beh2_tune(d), sqrtf(r2));
    {
        const float q = -0.0f - ez2;
        const float l2 = ex2 * ex2 + q * q;
        float px = ex2, pz = q;
        if (l2 != 0.0f) {
            const float inv = 1.0f / sqrtf(l2);
            pz = inv * q;
            px = inv * ex2;
        }
        const float dot = pz * fx + px * fz;
        const float cross = rz * ex1 - rx * ez1;
        if (cross > 0.0f && 0.0f > dot) push_steer(beh, v, F + 0x10, dot, pot, nx, nz);
    }
    RX_FRAME_LEAVE(F, 0x20);
    return 0;
}
RX_FUNCTION(0x00533970, rx_behavior_split_section_update)
