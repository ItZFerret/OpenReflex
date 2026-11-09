/* world_veh_util.h -- helpers of the vehicle share of the world code (core/game/world_veh*.c): the shared world helpers
 * (world_util.h: frames at the original's stack addresses, world_ctx calls, vector ops with x86's NaN rule) plus the idioms of
 * the vehicle functions. core/notes/world.md (veh). */
#pragma once
#include <math.h>
#include "world_util.h"
#include "world_veh_extern.h"

/* ---- the inlined quaternion product of the vehicle / rider code (sign tables 0x93a1f0 / 0x93a200 / 0x93a210):
 *   A0 = (a.x * b.wzyx) * (1,-1,1,-1)  A1 = (a.y * b.zwxy) * (1,1,-1,-1)  A2 = (b.yxwz * a.z) * (-1,1,1,-1)  A3 = a.w * b
 *   a * b = (A2 + A1) + (A3 + A0)   (the multiplications by the sign tables are exact either way round) */
static inline rx_v4 veh_quat_mul(rx_v4 a, rx_v4 b) {
    const rx_v4 A0 = v4_mul(v4_mul(v4_lane(a, 0), v4_perm(b, 3, 2, 1, 0)), v4(1.0f, -1.0f, 1.0f, -1.0f));
    const rx_v4 A1 = v4_mul(v4_mul(v4_lane(a, 1), v4_perm(b, 2, 3, 0, 1)), v4(1.0f, 1.0f, -1.0f, -1.0f));
    const rx_v4 A2 = v4_mul(v4_mul(v4_perm(b, 1, 0, 3, 2), v4_lane(a, 2)), v4(-1.0f, 1.0f, 1.0f, -1.0f));
    const rx_v4 A3 = v4_mul(v4_lane(a, 3), b);
    return v4_add(v4_add(A2, A1), v4_add(A3, A0));
}
/* |q|^2 of the 4-lane normalize: p = q * q, (p3 + p1) + (p2 + p0) (shufps 0xee / 0x40 / 0xf0 pattern) */
static inline float veh_len2_4(rx_v4 q) { const rx_v4 p = v4_mul(q, q); return phys_add(phys_add(p.v[3], p.v[1]), phys_add(p.v[2], p.v[0])); }
/* q / |q| in all four lanes, 0 where |q| = 0, the NaN pattern 0x92dc30 where |q|^2 is infinite */
static inline rx_v4 veh_normalize_len2(rx_v4 q, float len2) {
    const rx_v4 l2 = v4_splat(len2), len = v4_sqrt(l2);
    const rx_v4 finite = v4_cmpneq(l2, v4_ld(WORLD_INF4));
    const rx_v4 r = v4_and(v4_and(v4_cmpneq(v4_zero(), len), v4_div(q, len)), finite);
    return v4_or(r, v4_andn(finite, v4_ld(WORLD_NAN4)));
}
static inline rx_v4 veh_normalize4(rx_v4 q) { return veh_normalize_len2(q, veh_len2_4(q)); }

/* ---- the rotation matrix of a unit quaternion (x, y, z, w) as the scalar code builds it (x2 = x * 2 ..., xx = x * x2 ...):
 *   (1 - (zz + yy), xy + wz, xz - wy) / (xy - wz, 1 - (zz + xx), yz + wx) / (xz + wy, yz - wx, 1 - (yy + xx)) */
typedef struct { float m[3][3]; } veh_rot;
static inline veh_rot veh_quat_rows(float x, float y, float z, float w) {
    const float two = 2.0f /* 0x92e330 */, one = 1.0f /* 0x931f24 */;
    const float x2 = phys_mul(x, two), y2 = phys_mul(y, two), z2 = phys_mul(z, two);
    const float zz = phys_mul(z, z2), xx = phys_mul(x, x2), yy = phys_mul(y, y2);
    const float wx = phys_mul(w, x2), wy = phys_mul(w, y2), wz = phys_mul(w, z2);
    const float xy = phys_mul(y, x2), yz = phys_mul(z, y2), xz = phys_mul(z, x2);
    veh_rot r;
    r.m[0][0] = phys_sub(one, phys_add(zz, yy)); r.m[0][1] = phys_add(xy, wz); r.m[0][2] = phys_sub(xz, wy);
    r.m[1][0] = phys_sub(xy, wz); r.m[1][1] = phys_sub(one, phys_add(zz, xx)); r.m[1][2] = phys_add(yz, wx);
    r.m[2][0] = phys_add(xz, wy); r.m[2][1] = phys_sub(yz, wx); r.m[2][2] = phys_sub(one, phys_add(yy, xx));
    return r;
}

/* push ebp; mov ebp, esp; and esp, -8; then `pushes` pushes (the frames of the x87 / double functions) */
static inline world_frame veh_frame_align8(int pushes) {
    world_frame f; f.entry = rx_sp(); f.ebp = f.entry - 4;
    f.esp = (f.ebp & ~7u) - 4u * (uint32_t)pushes;
    rx_set_sp(f.esp); return f;
}

/* the inlined rotation of a vector by a unit quaternion: q * (v.xyz, 0) * conj(q), conj = (-1, -1, -1, 1) * q (0x93a220) */
static inline rx_v4 veh_rotate(rx_v4 q, rx_v4 v) {
    const rx_v4 conj = v4_mul(v4(-1.0f, -1.0f, -1.0f, 1.0f), q);
    return veh_quat_mul(q, veh_quat_mul(v4_xyz(v), conj));
}

/* MSVCR90 _CIpow through the exe's thunk 0x009009d2 (x87 st(1) = x, st(0) = y; result st(0), an extended value) */
static inline rx_f80 veh_ci_pow(rx_f80 x, rx_f80 y) { return crt_ci_pow(x, y, CRT_CW_MAIN); }
