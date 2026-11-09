/* build_vehicle_place.c -- construction unit 6: placing a vehicle (also its respawn path in race frames): 0045f600 (the vehicle's
 * transform from a position and two axes, validated, then the simulation reset) and RavenSimulationMotocross::vf2 0044a650 (every
 * body at the pose, velocities / forces cleared, the contact / wheel state reset). Hand transliteration from the disassembly; SSE
 * single precision in the original's order (the quaternion products are the inlined bike_quat_mul pattern), the double-precision
 * normalisation of 0045f600 in C doubles with the original's rounding points. Main thread (x87 CW 0x027f): only fld / fstp copies. */
#include "build_vehicle.h"
#include "build_vehicle_util.h"
#include "bike_math.h"
#include "bike_post.h"
#include "bike_sim.h"
#include "bike_remote.h"
#include "bike_slot13.h"
#include "phys_mech.h"
#include "phys_terrain.h"
#include "world.h"
#include "world_misc.h"
#include "loop_vehicle.h"
#include "loop_player.h"
#include "race.h"
#include "crt.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static rx_v4 ld(rx_addr a) { return rx_ld4(a); }
/* rotate(q, v): q * ((v & xyz) * conj(q)), conj as the code computes it: q * (-1, -1, -1, 1) (0093a220) */
static rx_v4 rot(rx_v4 q, rx_v4 v) { return bike_quat_mul(q, bike_quat_mul(v4_xyz(v), v4_mul(q, v4(-1.0f, -1.0f, -1.0f, 1.0f)))); }

/* the RangedFloat setters 0042d670 / 0042d8c0 (value / rate unless FLT_MAX) */
static void ranged_set(rx_addr r, float value, uint32_t rate_bits) {
    if (value != rx_bf(0x7f7fffffu)) SF32(r, 0, value);
    float rate = rx_bf(rate_bits);
    if (rate != rx_bf(0x7f7fffffu)) { SF32(r, 4, rate); SU32(r, 8, 0x3f800000u); }
}
static void ranged3_set(rx_addr out, rx_addr src, uint32_t rate_bits) {
    for (uint32_t i = 0; i < 3; i++) ranged_set(U32(out, 4 * i), F32(src, 4 * i), rate_bits);
}

/* 0044a650 RavenSimulationMotocross::vf2 (ECX = S; s0 = position*, s1 = orientation*; RET 8) */
void rx_sim_moto_reset(rx_addr S, rx_addr pos_p, rx_addr q_p) {
    const rx_addr sp = rx_sp(), A = (sp - 4) & ~15u, F = A - 0x50;   /* push ebp; and esp, -16; sub esp, 0x44; push ebx, esi, edi */
    rx_set_sp(F);
    rx_addr v = S + 0x1300;
    rx_world_record_vector_erase(v, F + 0x10, U32(v, 0), U32(v, 0xc), U32(v, 0), U32(v, 0x10));
    rx_addr M = S + 0xd0;
    SU32(S, 0x2e98, 1);
    phys_mech_set_velocity(M, VEH_RT_ZERO);
    phys_mech_clear_forces(M);
    const rx_v4 pos = ld(pos_p), q = ld(q_p);
    rx_st4(S + 0xf0, pos); rx_st4(S + 0x730, q); rx_st4(S + 0x190, pos); rx_st4(S + 0x7d0, q);
    {   /* the fork anchor (as 0043b750) */
        rx_v4 s2 = v4_mul(v4_splat(F32(S, 0x4224)), ld(0x00d61680));
        rx_v4 d = v4_sub(v4_sub(ld(S + 0x41d0), ld(S + 0x3770)), s2);
        rx_st4(S + 0x77d0, d);
        rx_v4 e = v4_shuf(v4_splat(F32(S, 0x4054)), ld(S + 0x77d0), 0xe5);
        e.v[0] = ld(S + 0x77d0).v[0];
        rx_st4(S + 0x77d0, e);
    }
    veh_v4(S + 0x7810, VEH_RT_W1); veh_v4(S + 0x8a90, VEH_RT_W1);
    rx_v4 a = v4_add(v4_mul(v4_splat(F32(S, 0x7820)), ld(S + 0x77e0)), ld(S + 0x77d0));
    rx_st4(S + 0x2d0, v4_add(rot(q, a), ld(pos_p)));
    rx_st4(S + 0x910, ld(q_p));
    rx_st4(S + 0x8a80, bike_quat_mul(ld(q_p), ld(S + 0x8a70)));
    rx_st4(S + 0x6e70, v4_sub(ld(S + 0x3dc0), ld(S + 0x3770)));
    rx_st4(S + 0x6eb0, ld(S + 0x8a70));
    rx_v4 b = v4_add(v4_mul(v4_splat(F32(S, 0x6ec0)), ld(S + 0x6e80)), ld(S + 0x6e70));
    rx_st4(S + 0x230, v4_add(rot(ld(q_p), b), ld(pos_p)));
    rx_st4(S + 0x870, ld(q_p));
    veh_v4(U32(S, 0x5c30) + 0x50, VEH_RT_W1);
    veh_vcall(S, 0x40, 0, 0, 0, 0);   /* vf16 */
    phys_mech_nodes_slot6(M);
    for (int32_t i = 0; i < I32(S, 0xec); i++) {
        rx_addr r = S + 0xf0 + 0xa0 * (uint32_t)i;
        rx_st4(r + 0x90, ld(r)); rx_st4(r + 0x80, ld(r));
        rx_st4(r + 0x6d0, ld(r + 0x640)); rx_st4(r + 0x6c0, ld(r + 0x640));
    }
    SU32(S, 0x8888, 1); SU32(S, 0x89d8, 1); SU32(S, 0x8890, 0); SU32(S, 0x89e0, 0); SU32(S, 0x8850, 0); SU32(S, 0x89a0, 0);
    SU32(S, 0x8854, 0); SU32(S, 0x89a4, 0); SU32(S, 0x90, 0); SU32(S, 0x8d98, 1); SU32(S, 0x8780, 0); SU32(S, 0x8784, 0);
    crt_memset(S + 0x1bb8, 0, 0x64);
    ranged_set(S + 0x5c34, 0.0f, veh_x87_copy(U32(S, 0x5cc4)));
    ranged_set(S + 0x5c48, 0.0f, veh_x87_copy(U32(S, 0x5cc4)));
    ranged_set(S + 0x5c5c, 0.0f, veh_x87_copy(U32(S, 0x5cc0)));
    ranged_set(S + 0x5c70, 0.0f, veh_x87_copy(U32(S, 0x3634)));
    ranged3_set(S + 0x8b40, 0x009cb4b0, veh_x87_copy(U32(S, 0x371c)));
    ranged_set(S + 0x94, 0.0f, veh_x87_copy(U32(S, 0x33c4)));
    ranged_set(S + 0x5c84, 0.0f, veh_x87_copy(U32(0, 0x009d3bfc)));
    ranged_set(S + 0x5c98, 0.0f, 0x3e4ccccdu);
    ranged_set(S + 0x5cac, 0.0f, 0x3e800000u);
    ranged3_set(S + 0x8e40, VEH_ZERO3, 0x3dcccccdu);
    ranged3_set(S + 0x8e4c, VEH_ZERO3, 0x3dcccccdu);
    ranged_set(S + 0x8b78, 0.0f, veh_x87_copy(U32(S, 0x5950)));
    ranged_set(S + 0x8b8c, 0.0f, veh_x87_copy(U32(S, 0x5954)));
    ranged_set(S + 0x8ba0, 0.0f, veh_x87_copy(U32(S, 0x5958)));
    ranged_set(S + 0x8bdc, 0.0f, veh_x87_copy(U32(S, 0x59a4)));
    veh_v4(S + 0x8c10, VEH_RT_W1);
    rx_st4(S + 0x8c00, ld(S + 0x730));
    rx_st4(S + 0x8bf0, rot(ld(S + 0x8c00), ld(0x00d615c0)));
    SU32(S, 0x8d90, 0); SU32(S, 0x8d94, 0); SU32(S, 0x8adc, 0); SU32(S, 0x8ae0, 0);
    veh_v4(S + 0x8a40, VEH_RT_ZERO); veh_v4(S + 0x8a50, VEH_RT_ZERO);
    SU32(S, 0x8ab8, veh_x87_copy(U32(S, 0x52ac)));
    SU32(S, 0xbc, 0x3bf5c28fu);   /* [0x977db4] 0.0075f */
    SF32(S, 0x8abc, phys_div(F32(S, 0x505c), rx_bf(0x4118cccdu) /* [0x9774d0] 9.55f */));
    SU32(S, 0x2f00, 0); SU32(S, 0xc0, 0); SU32(S, 0x8a30, 0); SU32(S, 0x8a34, 0);
    SU32(S, 0x2eb4, 0); SU32(S, 0x2eb8, 0);
    veh_v4(S + 0x8b60, VEH_RT_ZERO);
    if (U32(S, 0x2ebc)) {
        rx_world_dynobj_take_contact_results(U32(S, 0x2ebc), F + 0x30, F + 0x20, F + 0x10);   /* ESI, EDI, s1: frame locals */
        rx_world_dynobj_take_contact_results(U32(S, 0x2ebc), F + 0x30, F + 0x20, F + 0x10);
    }
    bike_body_probe_contacts(S);
    SU32(S, 0x14, 0); SU32(S, 0x18, 0); SU32(S, 0x8db8, 0);
    SU32(S, 0x8d40, 0); SU32(S, 0x8d44, 0); SU32(S, 0x8d48, 0); SU32(S, 0x8d50, 0); SU32(S, 0x8d58, 0); SU32(S, 0x8d5c, 0);
    SU32(S, 0x8d60, 0); SU32(S, 0x8d6c, 0);
    SU32(S, 0xac, 1); SU32(S, 0x68, 0);
    SU32(S, 0x8d4c, 0); SU32(S, 0x8d54, 0); SU32(S, 0x8d64, 0); SU32(S, 0x8d68, 0); SU32(S, 0x8d70, 0); SU32(S, 0x8d74, 0); SU32(S, 0x8d78, 0);
    rx_set_sp(sp);
}
RX_FUNCTION(0x0044a650, rx_sim_moto_reset);

/* 004331d0 (apply a replay record) is bike_remote.c's bike_remote_jump_to_record (merged 2026-10-26; w-fx had a second port). */

static int finite_f(float x) { return isfinite((double)x); }   /* _finite of the float widened to double */

/* 0045f600 (ECX = axis b (the up axis), s0 = the vehicle V, s1 = position, s2 = axis a (forward); RET 0xc): the orientation from the
 * two axes (third axis c = b x a (Vec3_Cross), normalised), its quaternion (the largest of the four diagonal combinations), normalised
 * in double precision; the 3x4 world matrix V+0xf0.., position V+0xc0 / +0x120, axes V+0x130 / +0x140; when every value is finite,
 * the quaternion's length is in [0.something, 1.05] and the position within 10000: the simulation reset (S vf2), the vehicle's
 * vf33 and 0045f590, vf60(0), vf62, the replay state record, V+0xd0 = the position, and the 25 terrain-track slots cut. */
void rx_vehicle_place(rx_addr V, rx_addr pos, rx_addr a, rx_addr bvec) {
    const rx_addr sp = rx_sp(), A = (sp - 4) & ~15u, F = A - 0xc0;   /* push ebp; and esp, -16; sub esp, 0xb4; push ebx, esi, edi */
    rx_set_sp(F);
    veh_cookie(A - 0xb4, 0xb0);
    SU32(V, 0xc0, veh_x87_copy(U32(pos, 0))); SU32(V, 0xc4, veh_x87_copy(U32(pos, 4))); SU32(V, 0xc8, veh_x87_copy(U32(pos, 8)));
    /* c = b x a (00401630: EAX = a, ECX = b) */
    float bb[3] = {F32(bvec, 0), F32(bvec, 4), F32(bvec, 8)}, aa[3] = {F32(a, 0), F32(a, 4), F32(a, 8)}, c[3];
    bike_vec3_cross(c, bb, aa);
    const rx_addr cx = F + 0x20;   /* the cross product's buffer on the frame */
    SF32(cx, 0, c[0]); SF32(cx, 4, c[1]); SF32(cx, 8, c[2]);
    float l2 = phys_add(phys_add(phys_mul(c[1], c[1]), phys_mul(c[0], c[0])), phys_mul(c[2], c[2]));
    float x = c[0];
    if (l2 > 0.0f) {
        const float inv = phys_div(1.0f, phys_sqrt(bike_x87_copyf(l2)));
        x = phys_mul(inv, c[0]);
        c[1] = phys_mul(inv, c[1]); c[2] = phys_mul(inv, c[2]);
        SF32(cx, 4, c[1]); SF32(cx, 8, c[2]);
    }
    /* the four diagonal combinations with m00 = c.x (normalised), m11 = b.y, m22 = a.z */
    const float b1 = F32(bvec, 4), a2 = F32(a, 8), k = 0.25f;
    float t0 = phys_add(x, 1.0f), t1 = phys_sub(1.0f, x);
    const float w = phys_mul(phys_add(phys_add(b1, t0), a2), k);          /* xmm5: (b1 + (x + 1)) + a2 */
    const float qx = phys_mul(phys_sub(phys_sub(t0, b1), a2), k);         /* xmm0: ((x + 1) - b1) - a2 */
    const float qy = phys_mul(phys_sub(phys_add(t1, b1), a2), k);         /* xmm2: ((1 - x) + b1) - a2 */
    const float qz = phys_mul(phys_add(phys_sub(t1, b1), a2), k);         /* xmm1: ((1 - x) - b1) + a2 */
    float m = qx > qy ? qx : qy;
    if (!(m > qz)) m = qz;
    if (!(m > w)) m = w;
    if (m == w) {
        const float s = phys_sqrt(bike_x87_copyf(w)), r = phys_div(k, s);
        SF32(V, 0xec, s);
        SF32(V, 0xe0, phys_mul(phys_sub(F32(bvec, 8), F32(a, 4)), r));
        SF32(V, 0xe4, phys_mul(phys_sub(F32(a, 0), c[2]), r));
        SF32(V, 0xe8, phys_mul(phys_sub(c[1], F32(bvec, 0)), r));
    } else {
        float m2 = qy > qz ? qy : qz;
        if (!(m2 > qx)) m2 = qx;
        if (m2 == qx) {
            const float s = phys_sqrt(bike_x87_copyf(qx)), r = phys_div(k, s);
            SF32(V, 0xe0, s);
            SF32(V, 0xec, phys_mul(phys_sub(F32(bvec, 8), F32(a, 4)), r));
            SF32(V, 0xe4, phys_mul(phys_add(F32(bvec, 0), c[1]), r));
            SF32(V, 0xe8, phys_mul(phys_add(F32(a, 0), c[2]), r));
        } else if (qy > qz) {
            const float s = phys_sqrt(bike_x87_copyf(qy)), r = phys_div(k, s);
            SF32(V, 0xe4, s);
            SF32(V, 0xec, phys_mul(phys_sub(F32(a, 0), c[2]), r));
            SF32(V, 0xe0, phys_mul(phys_add(F32(bvec, 0), c[1]), r));
            SF32(V, 0xe8, phys_mul(phys_add(F32(a, 4), F32(bvec, 8)), r));
        } else {
            const float s = phys_sqrt(bike_x87_copyf(qz)), r = phys_div(k, s);
            SF32(V, 0xe8, s);
            SF32(V, 0xec, phys_mul(phys_sub(c[1], F32(bvec, 0)), r));
            SF32(V, 0xe0, phys_mul(phys_add(F32(a, 0), c[2]), r));
            SF32(V, 0xe4, phys_mul(phys_add(F32(a, 4), F32(bvec, 8)), r));
        }
    }
    {   /* |q|^2 in doubles (cvtps2pd: x x + w w + y y + z z), rounded to float */
        const double dx = F32(V, 0xe0), dw = F32(V, 0xec), dy = F32(V, 0xe4), dz = F32(V, 0xe8);
        const float n2 = (float)(((dx * dx + dw * dw) + dy * dy) + dz * dz);
        if (n2 != 0.0f) {   /* ucomiss n2, 0; jnp: ordered equal skips to the zero quaternion */
            const float inv = phys_div(1.0f, phys_sqrt(bike_x87_copyf(n2)));
            const double di = inv;
            SF32(V, 0xec, (float)((double)F32(V, 0xec) * di));
            SF32(V, 0xe0, (float)((double)F32(V, 0xe0) * di));
            SF32(V, 0xe4, (float)((double)F32(V, 0xe4) * di));
            SF32(V, 0xe8, (float)(di * (double)F32(V, 0xe8)));
        } else { SU32(V, 0xe0, 0); SU32(V, 0xe4, 0); SU32(V, 0xe8, 0); SU32(V, 0xec, 0); }
    }
    {   /* the rotation rows from the quaternion */
        const float qz_ = F32(V, 0xe8), qx_ = F32(V, 0xe0), qy_ = F32(V, 0xe4), qw = F32(V, 0xec), two = 2.0f;
        const float x2 = phys_mul(qx_, two), xx = phys_mul(qx_, x2), y2 = phys_mul(qy_, two), z2 = phys_mul(qz_, two);
        const float wx = phys_mul(qw, x2), yy = phys_mul(qy_, y2), xy = phys_mul(qy_, x2), zz = phys_mul(qz_, z2);
        const float wz = phys_mul(qw, z2), xz = phys_mul(qz_, x2), yz = phys_mul(qz_, y2), wy = phys_mul(qw, y2);
        SF32(V, 0xf0, phys_sub(1.0f, phys_add(zz, yy)));
        SF32(V, 0x100, phys_sub(xy, wz));
        SF32(V, 0xf4, phys_add(xy, wz));
        SF32(V, 0x104, phys_sub(1.0f, phys_add(zz, xx)));
        SF32(V, 0x114, phys_sub(yz, wx));
        SF32(V, 0xf8, phys_sub(xz, wy));
        SU32(V, 0xfc, 0); SU32(V, 0x10c, 0); SU32(V, 0x11c, 0);
        SF32(V, 0x108, phys_add(yz, wx));
        SF32(V, 0x110, phys_add(xz, wy));
        SF32(V, 0x118, phys_sub(1.0f, phys_add(xx, yy)));
    }
    SU32(V, 0x120, veh_x87_copy(U32(V, 0xc0))); SU32(V, 0x124, veh_x87_copy(U32(V, 0xc4)));
    SU32(V, 0x12c, 0x3f800000u);
    SU32(V, 0x128, veh_x87_copy(U32(V, 0xc8)));
    SU32(V, 0x130, veh_x87_copy(U32(a, 0))); SU32(V, 0x134, veh_x87_copy(U32(a, 4))); SU32(V, 0x138, veh_x87_copy(U32(a, 8)));
    SU32(V, 0x140, veh_x87_copy(U32(bvec, 0))); SU32(V, 0x144, veh_x87_copy(U32(bvec, 4)));
    int bad = 0;
    SU32(V, 0x148, veh_x87_copy(U32(bvec, 8)));
    const rx_addr lpos = F + 0x30, lq = F + 0x20;
    veh_v4(lpos, V + 0xc0); veh_v4(lq, V + 0xe0);
    const float q0 = F32(V, 0xe0), q1 = F32(V, 0xe4), q2 = F32(V, 0xe8), q3 = F32(V, 0xec);
    if (q0 != q0 || q1 != q1 || q2 != q2 || q3 != q3 || !finite_f(q0) || !finite_f(q1) || !finite_f(q2) || !finite_f(q3)) bad = 1;
    else {
        float n = phys_sqrt(phys_add(phys_add(phys_add(phys_mul(q0, q0), phys_mul(q1, q1)), phys_mul(q2, q2)), phys_mul(q3, q3)));
        if (n > rx_bf(0x3f866666u) /* [0x9774c8] 1.05f */) bad = 1;
        else if (F32(0, 0x009cd9ac) > n) bad = 1;
    }
    const float p0 = F32(V, 0xc0), p1 = F32(V, 0xc4), p2 = F32(V, 0xc8);
    if (p0 != p0 || p1 != p1 || p2 != p2 || !finite_f(p0) || !finite_f(p1) || !finite_f(p2)) goto out;
    if (bike_fabs(p0) > 10000.0f || bike_fabs(p1) > 10000.0f || bike_fabs(p2) > 10000.0f || bad) goto out;
    veh_v4(V + 0x200, lpos); veh_v4(V + 0x210, lq);
    SU32(V, 0x180, 1);
    {
        veh_vcall(U32(V, 0x234), 8, 2, lpos, lq, 0);   /* S vf2 */
        veh_vcall(V, 0x84, 0, 0, 0, 0);                  /* vf33 */
        rx_vehicle_release_controls(V);
        veh_vcall_f(V, 0xf0, 0.0f);                      /* vf60 */
        veh_vcall(V, 0xf8, 0, 0, 0, 0);                  /* vf62 */
        rx_addr S = U32(V, 0x234);
        if (U32(S, 4) == 0) {
            bike_get_replay_state(S, V + 0x1220);
            if (U32(V, 0x190)) {   /* 0054feb0 (EAX = axis a, ESI = axis b; s0 = the player slot, s1 = position): the replay
                                      recorder's teleport record (the activity / replay unit's) */
                const rx_addr sp0 = rx_sp();
                rx_set_sp(sp0 - 12u);
                SU32(sp0 - 12u, 0, 0); SU32(sp0 - 8u, 0, U32(V, 0x190)); SU32(sp0 - 4u, 0, pos);
                rx_player_post_vectors(a, bvec, U32(V, 0x190), pos);
                rx_set_sp(sp0);
            }
        } else if (U32(S, 4) == 1) {   /* a replayed simulation: its state record re-applied (004331d0, EDI = the record, s0 = S) */
            const rx_addr rec = F + 0x40;
            rx_veh_body_rec_init(rec);
            bike_get_replay_state(S, rec);
            PHYS_CALL_SP(4, bike_remote_jump_to_record(rec, S));   /* 004331d0 (bike_remote.c) */
        }
    }
    SU32(V, 0xd0, veh_x87_copy(U32(pos, 0))); SU32(V, 0xd4, veh_x87_copy(U32(pos, 4))); SU32(V, 0xd8, veh_x87_copy(U32(pos, 8)));
    if (!(U32(0, 0x009e30e0) & 1)) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
    {
        rx_addr tr = U32(0, 0x00a9577c);
        for (uint32_t i = 0; i < 0x19; i++) {
            rx_addr slot = V + 0x11b0 + 4 * i;
            int32_t h = I32(slot, 0);
            if (h < 0) continue;
            int32_t idx = h ^ 0x4000;
            if (idx < I32(tr, 0xc) && idx >= 0) {
                phys_tracks_cut_strip(tr, idx);
                SU32(U32(tr, 0x24) + 0xc0 * (uint32_t)idx, 0x98, 1);
                SU32(tr, 0x28, U32(tr, 0x28) - 1);
            }
            SU32(slot, 0, 0xffffffffu);
        }
    }
out:
    rx_set_sp(sp);
}
RX_FUNCTION(0x0045f600, rx_vehicle_place);
