/* phys_mech.c -- RavenPhysics articulated-body mechanism (the vehicle simulation's rigid-body solver): per-node passes, force
 * application, rigid motions, preparation / solve of the articulated-body factorization and small helpers.
 *
 * Mechanism M (= simulation + 0xd0): +0x00 gravity vec4, +0x1c node count (signed), +0xca0 node (constraint) object pointers.
 * Node i (stride 0xa0) has its linear body at M + 0x20 + 0xa0 * i and its angular body 0x640 later. Within a body:
 *   +0x00 pose (linear: position, angular: orientation quaternion)   +0x10 velocity   +0x20 mass / inertia   +0x30 inverses
 *   +0x40 resolved force   +0x50 applied force   +0x60 impulse   +0x70 acceleration
 * (native/game/notes/mechanism.md). The node counts are signed and re-read every iteration, like the original. */
#include "phys_mech.h"
#include "phys_vec.h"
#include "phys_mech_extern.h"
#include "phys_misc.h"
#include "fourwheel_nodes_exact.h"
#include "fx_util.h"
#include <stdlib.h>

/* Call a function of this package the way the original calls it: with `nstack` stack arguments pushed and the return address, so
 * the callee's entry ESP is cur - 4 * nstack - 4. */
#define CALL_AT(cur, nstack, stmt) do { rx_set_sp((cur) - 4u * (nstack) - 4u); stmt; rx_set_sp(cur); } while (0)
#define NODE_STRIDE 0xa0u
#define ANG 0x640u                      /* angular body offset from the linear body */
#define G_ZERO 0x00d61670u              /* runtime vec4 the step clears accumulators with (read every time) */

/* The quaternion sign vectors (.rdata): 0093a1f0 S1, 0093a200 S2, 0093a210 S3, 0093a220 S4 (conjugation). */
static rx_v4 sign_s1(void) { return v4(1.0f, -1.0f, 1.0f, -1.0f); }
static rx_v4 sign_s2(void) { return v4(1.0f, 1.0f, -1.0f, -1.0f); }
static rx_v4 sign_s3(void) { return v4(-1.0f, 1.0f, 1.0f, -1.0f); }
static rx_v4 sign_conj(void) { return v4(-1.0f, -1.0f, -1.0f, 1.0f); }

/* The exe's quaternion product a (x) b in its SSE operation order:
 *   tx = (a.x * b.wzyx) * S1;  ty = (a.y * b.zwxy) * S2;  tz = (b.yxwz * a.z) * S3;  tw = a.w * b + tx;   result = (tz + ty) + tw */
static rx_v4 quat_mul(rx_v4 a, rx_v4 b) {
    rx_v4 rev = v4_perm(b, 3, 2, 1, 0);
    rx_v4 tx = v4_mul(v4_mul(v4_lane(a, 0), rev), sign_s1());
    rx_v4 r2 = v4_perm(rev, 1, 0, 3, 2);
    rx_v4 ty = v4_mul(v4_mul(v4_lane(a, 1), r2), sign_s2());
    rx_v4 r3 = v4_perm(r2, 3, 2, 1, 0);
    rx_v4 tz = v4_mul(v4_mul(r3, v4_lane(a, 2)), sign_s3());
    tz = v4_add(tz, ty);
    rx_v4 tw = v4_add(v4_mul(v4_lane(a, 3), b), tx);
    return v4_add(tz, tw);
}

/* The exe's packed normalization of v given its squared length n2: v / sqrt(n2) per lane when n2 is finite (!= +inf; NaN
 * counts as finite) and the norm is nonzero, 0 when the norm is zero, the NaN vec4 0092dc30 (0x7fc00000) when n2 is +inf. */
static rx_v4 normalize_masked(rx_v4 v, float n2) {
    float n = phys_sqrt(n2);
    int finite = n2 != rx_bf(0x7f800000);                       /* cmpneqps with 0092dc20 (+inf) */
    int nonzero = 0.0f != n;                                    /* cmpneqps 0, n (NaN: true) */
    rx_v4 out;
    for (int k = 0; k < 4; k++) {
        float d = phys_div(v.v[k], n);
        uint32_t bitsd = nonzero ? rx_fb(d) : 0u;
        out.v[k] = rx_bf(finite ? bitsd : 0x7fc00000u);
    }
    return out;
}

/* Quaternion normalization: n2 = (q3^2 + q1^2) + (q2^2 + q0^2) (the exe's packed horizontal sum). */
static rx_v4 quat_normalize(rx_v4 q) {
    rx_v4 s = v4_mul(q, q);
    float n2 = phys_add(phys_add(s.v[3], s.v[1]), phys_add(s.v[2], s.v[0]));
    return normalize_masked(q, n2);
}

/* ---- per-node virtual calls ---- */

/* The node object pointer array (+0xca4 = node 1 ...) walked from node 1: slot `slot` of every non-root node, the count re-read
 * every iteration. Returns the last callee's EAX (0 when no node was visited). */
static uint32_t nodes_vcall_observed(rx_addr M, uint32_t slot, int exact) {
    uint32_t r = exact ? fx_reg(FX_EAX) : 0;
    const rx_addr entry = rx_sp();
    fx_saved saved;
    if (exact) {
        if (slot != 7 || U32(entry, 4) != M) abort();
        saved = fx_save();
        SU32(entry, -4, saved.ebx); fx_set_reg(FX_EBX, M);
        SU32(entry, -8, saved.esi); fx_set_reg(FX_ESI, 1);
        rx_set_sp(entry - 8);
        if (I32(M, 0x1c) > 1) {
            SU32(entry, -12, saved.edi); fx_set_reg(FX_EDI, M + 0xca4);
            rx_set_sp(entry - 12);
        }
    }
    for (int32_t i = 1; i < I32(M, 0x1c); i++) {
        const rx_addr object = U32(M, 0xca4 + 4u * (uint32_t)(i - 1));
        if (exact) {
            const rx_addr vt = U32(object, 0), target = U32(vt, 4 * slot);
            if (target != 0x85ac20) abort(); /* genuine SlidingRotor vf7 only */
            fx_set_reg(FX_ECX, object); fx_set_reg(FX_EAX, vt); fx_set_reg(FX_EDX, target);
            SU32(entry, -16, 0x85d350); rx_set_sp(entry - 16);
            r = phys_rotor_project_velocity_exact(object);
            if (rx_sp() != entry - 16) abort();
            rx_set_sp(entry - 12);
            fx_set_reg(FX_ESI, (uint32_t)i + 1); fx_set_reg(FX_EDI, M + 0xca4 + 4u * (uint32_t)i);
        } else r = phys_vcall0(object, slot);
    }
    if (exact) { rx_set_sp(entry); fx_restore(saved); }
    return r;
}
static uint32_t nodes_vcall(rx_addr M, uint32_t slot) { return nodes_vcall_observed(M, slot, 0); }
/* 0085c6d0 Mechanism_NodesSlot2: slot 2 of every non-root node. */
void phys_mech_nodes_slot2(rx_addr M) { nodes_vcall(M, 2); }
RX_FUNCTION(0x0085c6d0, phys_mech_nodes_slot2)
/* 0085c700 Mechanism_NodesSlot3: slot 3 of every non-root node. */
void phys_mech_nodes_slot3(rx_addr M) { nodes_vcall(M, 3); }
RX_FUNCTION(0x0085c700, phys_mech_nodes_slot3)
/* 0085d300 Mechanism_NodesSlot6: slot 6 of every non-root node; returns the last callee's EAX. */
uint32_t phys_mech_nodes_slot6(rx_addr M) { return nodes_vcall(M, 6); }
RX_FUNCTION(0x0085d300, phys_mech_nodes_slot6)
/* 0085d330 Mechanism_NodesSlot7: slot 7 of every non-root node; returns the last callee's EAX. */
uint32_t phys_mech_nodes_slot7(rx_addr M) { return nodes_vcall(M, 7); }
uint32_t phys_mech_nodes_slot7_exact(rx_addr M) { return nodes_vcall_observed(M, 7, 1); }
RX_FUNCTION(0x0085d330, phys_mech_nodes_slot7)

/* ---- clearing and setting ---- */

/* 0085c9d0 Mechanism_ClearForces: every node's resolved force, applied force and impulse (both bodies) = the runtime vec4
 * 0xd61670, in the original's store order: linear +0x60, +0x50, angular +0x60, +0x50, linear +0x40, angular +0x40. */
void phys_mech_clear_forces(rx_addr M) {
    for (int32_t i = 0; i < I32(M, 0x1c); i++) {
        rx_addr lin = M + 0x20 + NODE_STRIDE * (uint32_t)i, ang = lin + ANG;
        v4_st(lin + 0x60, v4_ld(G_ZERO));
        v4_st(lin + 0x50, v4_ld(G_ZERO));
        v4_st(ang + 0x60, v4_ld(G_ZERO));
        v4_st(ang + 0x50, v4_ld(G_ZERO));
        v4_st(lin + 0x40, v4_ld(G_ZERO));
        v4_st(ang + 0x40, v4_ld(G_ZERO));
    }
}
RX_FUNCTION(0x0085c9d0, phys_mech_clear_forces)

/* 0085c500 Mechanism_Translate: every node's position += *vec (translation of the whole mechanism; vec re-read each node). */
void phys_mech_translate(rx_addr M, rx_addr vec) {
    for (int32_t i = 0; i < I32(M, 0x1c); i++) {
        rx_addr lin = M + 0x20 + NODE_STRIDE * (uint32_t)i;
        rx_v4 pos = v4_ld(lin), d = v4_ld(vec);
        v4_st(lin, v4_add(pos, d));
    }
}
RX_FUNCTION(0x0085c500, phys_mech_translate)

/* 0085c4b0 Mechanism_SetVelocity: every node's linear velocity = *vec; its linear acceleration, angular velocity and angular
 * acceleration = the runtime vec4 0xd61670. */
void phys_mech_set_velocity(rx_addr M, rx_addr vec) {
    for (int32_t i = 0; i < I32(M, 0x1c); i++) {
        rx_addr lin = M + 0x20 + NODE_STRIDE * (uint32_t)i, ang = lin + ANG;
        v4_st(lin + 0x10, v4_ld(vec));
        v4_st(lin + 0x70, v4_ld(G_ZERO));
        v4_st(ang + 0x10, v4_ld(G_ZERO));
        v4_st(ang + 0x70, v4_ld(G_ZERO));
    }
}
RX_FUNCTION(0x0085c4b0, phys_mech_set_velocity)

/* 0085c730 Mechanism_SetNodeLinearVelocity: node `index`'s linear velocity = *vec. Returns index * 0xa0 (EAX). */
uint32_t phys_mech_set_node_linear_velocity(rx_addr M, rx_addr vec, uint32_t index) {
    uint32_t off = index * NODE_STRIDE;
    v4_st(M + off + 0x30, v4_ld(vec));
    return off;
}
RX_FUNCTION(0x0085c730, phys_mech_set_node_linear_velocity)

/* 0085c750 Mechanism_SetNodeAngularVelocity: node `index`'s angular velocity = *vec. Returns index * 0xa0 (EAX). */
uint32_t phys_mech_set_node_angular_velocity(rx_addr M, rx_addr vec, uint32_t index) {
    uint32_t off = index * NODE_STRIDE;
    v4_st(M + off + 0x670, v4_ld(vec));
    return off;
}
RX_FUNCTION(0x0085c750, phys_mech_set_node_angular_velocity)

/* ---- force composition and pose integration ---- */

/* 0085ca40 Mechanism_ComposeForces (EDI = M, stack arg dt): per node, linear resolved force = mass * gravity + applied force;
 * angular resolved torque = applied torque - omega x (I omega) with I omega = inertia * omega componentwise (the cross product
 * through the skew matrix kernel 00857ff0 and the 3x3 product 00854490, on the original's stack locals); then both resolved
 * forces += impulse * (1 / dt). The locals (Matrix3x3 object at f0+0x28, I omega at f0+0x18, the product at f0+0x08) are at the
 * original's frame: f0 = ((esp - 4) & ~15) - 0x78 after the prologue, esp = f0 - 8 at the two kernel calls. */
void phys_mech_compose_forces(rx_addr M, float dt) {
    const rx_addr sp0 = rx_sp();
    const rx_addr f0 = ((sp0 - 4) & ~15u) - 0x78;
    const rx_addr T = f0 + 0x28, Iw = f0 + 0x18, W = f0 + 0x08;
    float inv = phys_div(1.0f /* 0x931f24 */, dt);
    rx_v4 inv4 = v4_splat(inv);
    rx_set_sp(f0 - 8);
    SU32(T, 4, 3);
    SU32(T, 8, 3);
    SU32(T, 0, 0x0093ca04);                 /* Matrix3x3 vtable */
    for (int32_t i = 0; i < I32(M, 0x1c); i++) {
        rx_addr lin = M + 0x20 + NODE_STRIDE * (uint32_t)i, ang = lin + ANG;
        rx_v4 g = v4_ld(M), mass = v4_ld(lin + 0x20);
        v4_st(lin + 0x40, v4_add(v4_mul(mass, g), v4_ld(lin + 0x50)));
        v4_st(ang + 0x40, v4_ld(ang + 0x50));
        rx_v4 inertia = v4_ld(ang + 0x20), omega = v4_ld(ang + 0x10);
        v4_st(Iw, v4_mul(inertia, omega));
        CALL_AT(f0 - 8, 1, phys_matrix3x3_skew(ang + 0x10, T));
        CALL_AT(f0 - 8, 1, phys_mat3_mulvec(W, Iw, T));
        rx_v4 tq = v4_ld(ang + 0x40), cross = v4_ld(W);
        v4_st(ang + 0x40, v4_sub(tq, cross));
    }
    for (int32_t i = 0; i < I32(M, 0x1c); i++) {
        rx_addr lin = M + 0x20 + NODE_STRIDE * (uint32_t)i, ang = lin + ANG;
        rx_v4 imp = v4_ld(lin + 0x60), res = v4_ld(lin + 0x40);
        v4_st(lin + 0x40, v4_add(v4_mul(imp, inv4), res));
        imp = v4_ld(ang + 0x60); res = v4_ld(ang + 0x40);
        v4_st(ang + 0x40, v4_add(v4_mul(imp, inv4), res));
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0085ca40, phys_mech_compose_forces)

/* 0085d100 Mechanism_IntegratePoses (ECX = M, XMM6 lane 0 = dt): position += velocity * dt; orientation q += (q (x) omega) *
 * (dt / 2) with omega = (angular velocity xyz, w = lane 3 of the runtime vec4 0xd61660) and the product in the exe's order
 * (quat_mul); then every orientation is normalized (quat_normalize). 0x9322cc is 0.5f. */
void phys_mech_integrate_poses(rx_addr M, float dt) {
    rx_v4 dt4 = v4_splat(dt), half = v4_splat(phys_mul(dt, 0.5f /* 0x9322cc */));
    for (int32_t i = 0; i < I32(M, 0x1c); i++) {
        rx_addr lin = M + 0x20 + NODE_STRIDE * (uint32_t)i, ang = lin + ANG;
        rx_v4 vel = v4_ld(lin + 0x10), pos = v4_ld(lin);
        v4_st(lin, v4_add(v4_mul(vel, dt4), pos));
        rx_v4 g = v4_ld(0x00d61660), w = v4_ld(ang + 0x10);
        rx_v4 omega = v4(w.v[0], w.v[1], w.v[2], g.v[3]);
        rx_v4 q = v4_ld(ang);
        rx_v4 dq = v4_mul(quat_mul(q, omega), half);
        v4_st(ang, v4_add(dq, q));
    }
    for (int32_t i = 0; i < I32(M, 0x1c); i++) {
        rx_addr ang = M + 0x20 + NODE_STRIDE * (uint32_t)i + ANG;
        v4_st(ang, quat_normalize(v4_ld(ang)));
    }
}
RX_FUNCTION(0x0085d100, phys_mech_integrate_poses)

/* ---- forces at a point and rigid motions ---- */

/* 0085c770 Mechanism_AddForceAtPoint (ECX = force vec4, EDX = M, ESI = point vec4, stack arg = node index): the force F is added
 * to the node's linear applied force; the torque (r x F') & xyz is added to its angular applied torque, where F' = conj(q) (x)
 * (F & xyz (x) q) is the force rotated into the body frame (products in the exe's order, S4 the conjugation signs) and the
 * cross product uses the rotated (r, F') vectors (1 2 0) / (2 0 1) shuffles. Returns M + index * 0xa0 (EAX).
 * (The original's tail writes the force to its dead stack when the debug flag 00da68bc is set: nothing live.) */
uint32_t phys_mech_add_force_at_point(rx_addr force, rx_addr M, rx_addr point, uint32_t index) {
    const rx_addr p = index * NODE_STRIDE + M;
    rx_v4 F = v4_ld(force), mask = v4_ld(0x0092db80), q = v4_ld(p + 0x660);
    rx_v4 Fm = v4_and(F, mask);
    rx_v4 t = quat_mul(Fm, q);
    rx_v4 qc = v4_mul(sign_conj(), q);
    rx_v4 u = quat_mul(qc, t);
    rx_v4 r = v4_ld(point);
    rx_v4 r1 = v4_perm(r, 1, 2, 0, 3);
    rx_v4 u1 = v4_perm(u, 2, 0, 1, 3);
    rx_v4 u2 = v4_perm(u1, 2, 0, 1, 3);
    rx_v4 c1 = v4_mul(u1, r1);
    rx_v4 r2 = v4_perm(r1, 1, 2, 0, 3);
    rx_v4 lin = v4_ld(p + 0x70);
    v4_st(p + 0x70, v4_add(lin, F));
    rx_v4 ang = v4_ld(p + 0x6b0);
    rx_v4 cr = v4_and(v4_sub(c1, v4_mul(u2, r2)), mask);
    v4_st(p + 0x6b0, v4_add(ang, cr));
    return p;
}
RX_FUNCTION(0x0085c770, phys_mech_add_force_at_point)

/* 0085c8b0 Mechanism_AddBodyForce (ECX = M, stack index, force; RET 8): the force is local to the body. Mask its w lane,
 * rotate it to world space as q (x) ((F & xyz) (x) conj(q)), and add all four resulting lanes to the linear applied force.
 * The aligned frame saves only EBP and has no locals or callees. Returns M + index * 0xa0, the original's EAX. */
uint32_t phys_mech_add_body_force(rx_addr M, uint32_t index, rx_addr force) {
    const rx_addr p = M + index * NODE_STRIDE;
    const rx_v4 q = v4_ld(p + 0x660);
    const rx_v4 qc = v4_mul(sign_conj(), q);
    const rx_v4 f = v4_and(v4_ld(force), v4_ld(0x0092db80));
    const rx_v4 local = quat_mul(f, qc);
    const rx_v4 world = quat_mul(q, local);
    v4_st(p + 0x70, v4_add(world, v4_ld(p + 0x70)));
    return p;
}
RX_FUNCTION(0x0085c8b0, phys_mech_add_body_force)

/* 0085c530 Mechanism_Rotate (EDX = M, ESI = rotation quaternion R): rotate the whole mechanism about the root node's position:
 * every node's orientation q = q (x) R, and position = root + R (x) ((position - root) & xyz (x) conj(R)); the root position is
 * re-read for every node (so the root itself is rotated in place first) and R is re-read after the orientation store. */
void phys_mech_rotate(rx_addr M, rx_addr rot) {
    const rx_addr root = M + 0x20;
    for (int32_t i = 0; i < I32(M, 0x1c); i++) {
        rx_addr lin = M + 0x20 + NODE_STRIDE * (uint32_t)i, ang = lin + ANG;
        rx_v4 q = v4_ld(ang), R = v4_ld(rot);
        rx_v4 rootpos = v4_ld(root);
        v4_st(ang, quat_mul(q, R));
        R = v4_ld(rot);
        rx_v4 pos = v4_ld(lin);
        rx_v4 d = v4_and(v4_sub(pos, rootpos), v4_ld(0x0092db80));
        rx_v4 Rc = v4_mul(R, sign_conj());
        rx_v4 v2 = quat_mul(d, Rc);
        v4_st(lin, v4_add(quat_mul(R, v2), rootpos));
    }
}
RX_FUNCTION(0x0085c530, phys_mech_rotate)

/* ---- small helpers ---- */

/* 00852f40 Vec3_RotateAboutAxis (EAX = out vec3 or 0, ECX = v vec3, stack arg = unit axis a vec3, XMM1 lane 0 = angle): out = v
 * rotated about a by the angle (Rodrigues: proj + (v - proj) cos + (a x (v - proj)) sin, proj = a (a . v)), with the exe's
 * polynomial sin / cos of the angle wrapped to [-pi, pi]: s = -1 when angle < 0 (or NaN; the -1 constant 009cd9e8 is writable,
 * read each time) else +1; r = s * angle - 2 pi * trunc(s * angle / 2 pi); x = pi - r. Returns 1, or 0 when out is null (nothing
 * is written). */
uint32_t phys_vec3_rotate_about_axis(rx_addr out, rx_addr v, rx_addr axis, float angle) {
    if (out == 0) return 0;
    const float zero = 0.0f;                                     /* 0092e12c */
    float s = (angle < zero || angle != angle) ? F32(0, 0x009cd9e8) : 1.0f /* 0x931f24 */;
    float vx = F32(v, 0), vy = F32(v, 4), vz = F32(v, 8);
    float ax = F32(axis, 0), ay = F32(axis, 4), az = F32(axis, 8);
    float sa = phys_mul(s, angle);
    float turns = (float)rx_cvtt(phys_mul(sa, rx_bf(0x3e22f983) /* 0x9774a0: 1 / 2pi */));
    float r = phys_sub(sa, phys_mul(turns, rx_bf(0x40c90fdb) /* 0x97749c: 2 pi */));
    float x = phys_sub(rx_bf(0x40490fdb) /* 0x977498: pi */, r);
    float x2 = phys_mul(x, x), x4 = phys_mul(x2, x2);
    /* sin(x) part: x * s * poly, and the cos part: -0 - poly (coefficients at 0x935d3c..0x935d68) */
    float sinpoly = phys_add(phys_mul(phys_add(phys_mul(phys_add(phys_mul(x2, rx_bf(0xb2aee90a)), rx_bf(0x36352cc0)), x4),
                                               phys_add(phys_mul(x2, rx_bf(0xb94fb563)), rx_bf(0x3c0884a2))), x4),
                             phys_add(phys_mul(x2, rx_bf(0xbe2aaa5c)), rx_bf(0x3f7ffff9)));
    float sn = phys_mul(phys_mul(sinpoly, x), s);
    float cospoly = phys_add(phys_mul(phys_add(phys_mul(phys_add(phys_mul(x2, rx_bf(0xb46beed0)), rx_bf(0x37cb076b)), x4),
                                               phys_add(phys_mul(x2, rx_bf(0xbab5a662)), rx_bf(0x3d2aa372))), x4),
                             phys_add(phys_mul(x2, rx_bf(0xbeffff3d)), rx_bf(0x3f7ffff3)));
    float cs = phys_sub(rx_bf(0x80000000) /* 0x9774a8: -0 */, cospoly);
    float dot = phys_add(phys_add(phys_mul(vx, ax), phys_mul(vy, ay)), phys_mul(vz, az));
    float px = phys_mul(ax, dot), py = phys_mul(ay, dot), pz = phys_mul(az, dot);
    float dx = phys_sub(vx, px), dy = phys_sub(vy, py), dz = phys_sub(vz, pz);
    float cx = phys_sub(phys_mul(ay, dz), phys_mul(az, dy));
    float cy = phys_sub(phys_mul(az, dx), phys_mul(ax, dz));
    float cz = phys_sub(phys_mul(ax, dy), phys_mul(ay, dx));
    float ox = phys_add(phys_add(phys_mul(cx, sn), phys_mul(dx, cs)), px);
    float oy = phys_add(phys_add(phys_mul(cy, sn), phys_mul(dy, cs)), py);
    float oz = phys_add(phys_add(phys_mul(cz, sn), phys_mul(dz, cs)), pz);
    SF32(out, 0, ox);
    SF32(out, 4, oy);
    SF32(out, 8, oz);
    return 1;
}
RX_FUNCTION(0x00852f40, phys_vec3_rotate_about_axis)

/* 00853220 (no arguments): builds the noise tables with the exe's inlined linear congruential generator (seed 0x009cb860, call
 * counter 0x009cb864: x = x * 0x41c64e6d + 0x3039, the value (x >> 16) & 0x7fff; the counter counts the draws): per entry i of 256,
 * six draws in order, each as float((draw & 0x1ff) - 256) * 0.00390625 (0x9777d0): the 1D gradient G1[i] (0x00a87980), the 2D
 * gradient G2[i] (0x00a88188, two floats; normalized afterwards: 1 / sqrt(y^2 + x^2)), the 3D gradient G3[i] (0x00a86168, three
 * floats; normalized: 1 / sqrt((x^2 + y^2) + z^2)); the permutation P[i] = i (0x00a89198). Then P is shuffled (i from 255 to 1:
 * swap P[i] and P[draw & 0xff]) and the tables are extended past their end by 258 entries (rep movsd for G1 and P at +0x400, fld /
 * fstp for G2 at +0x800 and G3 at +0xc00): the lookups run to index 257. */
void phys_noise_init_tables(void) {
    const float scale = 0.00390625f /* 0x9777d0 */;
    uint32_t seed = U32(0, 0x009cb860), draws = U32(0, 0x009cb864);
#define NOISE_DRAW() (seed = seed * 0x41c64e6du + 0x3039u, draws++, (int32_t)(((seed >> 16) & 0x7fffu) & 0x1ffu) - 0x100)
    for (uint32_t i = 0; i < 256; i++) {
        SU32(0, 0x00a89198u + 4 * i, i);
        SF32(0, 0x00a87980u + 4 * i, phys_mul((float)NOISE_DRAW(), scale));
        const rx_addr g2 = 0x00a88188u + 8 * i, g3 = 0x00a86168u + 12 * i;
        SF32(g2, 0, phys_mul((float)NOISE_DRAW(), scale));
        SF32(g2, 4, phys_mul((float)NOISE_DRAW(), scale));
        {
            const float x = F32(g2, 0), y = F32(g2, 4);
            const float inv = phys_div(1.0f /* 0x931f24 */, phys_sqrt(phys_add(phys_mul(y, y), phys_mul(x, x))));
            SF32(g2, 0, phys_mul(inv, x)); SF32(g2, 4, phys_mul(inv, y));
        }
        SF32(g3, 0, phys_mul((float)NOISE_DRAW(), scale));
        SF32(g3, 4, phys_mul((float)NOISE_DRAW(), scale));
        SF32(g3, 8, phys_mul((float)NOISE_DRAW(), scale));
        {
            const float x = F32(g3, 0), y = F32(g3, 4), z = F32(g3, 8);
            const float inv = phys_div(1.0f, phys_sqrt(phys_add(phys_add(phys_mul(x, x), phys_mul(y, y)), phys_mul(z, z))));
            SF32(g3, 0, phys_mul(inv, x)); SF32(g3, 4, phys_mul(inv, y)); SF32(g3, 8, phys_mul(inv, z));
        }
    }
    SU32(0, 0x009cb860, seed); SU32(0, 0x009cb864, draws);
    for (uint32_t c = 255; c != 0; c--) {
        const uint32_t held = U32(0, 0x00a89198u + 4 * c);
        (void)NOISE_DRAW();
        const uint32_t j = (seed >> 16) & 0xffu;
        SU32(0, 0x00a89198u + 4 * c, U32(0, 0x00a89198u + 4 * j));
        SU32(0, 0x00a89198u + 4 * j, held);
    }
    SU32(0, 0x009cb860, seed); SU32(0, 0x009cb864, draws);
#undef NOISE_DRAW
    for (uint32_t k = 0; k < 0x102; k++) SU32(0, 0x00a87d80u + 4 * k, U32(0, 0x00a87980u + 4 * k));
    for (uint32_t k = 0; k < 0x102; k++) SU32(0, 0x00a89598u + 4 * k, U32(0, 0x00a89198u + 4 * k));
    for (uint32_t k = 0; k < 258; k++) {
        SU32(0, 0x00a88988u + 8 * k, U32(0, 0x00a88188u + 8 * k));
        SU32(0, 0x00a8898cu + 8 * k, U32(0, 0x00a8818cu + 8 * k));
        for (uint32_t w = 0; w < 3; w++) SU32(0, 0x00a86d68u + 12 * k + 4 * w, U32(0, 0x00a86168u + 12 * k + 4 * w));
    }
}
RX_FUNCTION(0x00853220, phys_noise_init_tables)

/* 00853180 Noise1D (stack arg x, result XMM0 lane 0): 1D gradient noise. The first use (flag 009b71c8 set) clears the flag and
 * builds the tables (00853220); with u = x + 4096, i = (int) u (truncated), f = u - i, the gradients g0 = G[P[i & 255]] and
 * g1 = G[P[(i + 1) & 255]] (permutation 00a89198, gradients 00a87980, both writable: read each time):
 *   ((f - 1) g1 - g0 f) * ((3 - 2 f) f f) + g0 f. */
float phys_noise_1d(float x) {
    if (U32(0, 0x009b71c8) != 0) {
        SU32(0, 0x009b71c8, 0);
        phys_noise_init_tables();
    }
    float u = phys_add(x, 4096.0f /* 0x93594c */);
    int32_t i = rx_cvtt(u);
    uint32_t k0 = (uint32_t)i & 0xff;
    float fi = (float)i;
    float g0 = F32(0, 0x00a87980u + 4u * U32(0, 0x00a89198u + 4u * k0));
    float f = phys_sub(u, fi);
    float fm1 = phys_sub(f, 1.0f /* 0x931f24 */);
    uint32_t k1 = (k0 + 1u) & 0xff;
    float g1 = F32(0, 0x00a87980u + 4u * U32(0, 0x00a89198u + 4u * k1));
    float a = phys_mul(fm1, g1);
    float t = phys_mul(f, 2.0f /* 0x92e330 */);
    float smooth = phys_sub(3.0f /* 0x92fe48 */, t);
    float gf = phys_mul(g0, f);
    float num = phys_sub(a, gf);
    smooth = phys_mul(phys_mul(smooth, f), f);
    return phys_add(phys_mul(num, smooth), gf);
}
RX_FUNCTION(0x00853180, phys_noise_1d)

/* 008523b0 Segment_PlaneIntersect (EDI = p0, EDX = p1, stack args q and out, ESI = plane normal n): intersect the segment p0 -> p1
 * with the plane through q: d = p1 - p0 and w = q - p0 (00401700 into the original's stack locals), den = (n.x d.x + n.y d.y) +
 * n.z d.z; no hit when den == 0 (NaN counts as nonzero); t = (((w.z n.z) + (w.y n.y)) + (w.x n.x)) / den; a hit when 0 <= t <= 1:
 * out = p0 + d t componentwise, returns 1; else returns 0. The locals sit in the original's frame: f0 = ((esp - 4) & ~15) - 0x3c;
 * d at f0 + 0x1c, w at f0 + 0x2c, esp = f0 - 4 at the two calls. */
uint32_t phys_segment_plane_intersect(rx_addr p0, rx_addr p1, rx_addr q, rx_addr out, rx_addr n) {
    const rx_addr sp0 = rx_sp();
    const rx_addr f0 = ((sp0 - 4) & ~15u) - 0x3c;
    const rx_addr D = f0 + 0x1c, Wv = f0 + 0x2c;
    uint32_t result = 0;
    rx_set_sp(f0 - 4);
    CALL_AT(f0 - 4, 1, phys_vec3_sub(p0, p1, D));
    float nx = F32(n, 0), ny = F32(n, 4), nz = F32(n, 8);
    float den = phys_add(phys_add(phys_mul(nx, F32(D, 0)), phys_mul(ny, F32(D, 4))), phys_mul(nz, F32(D, 8)));
    if (den != 0.0f /* 0092e12c; NaN: true */) {
        rx_addr w = Wv;
        CALL_AT(f0 - 4, 1, phys_vec3_sub(p0, q, Wv));
        float t = phys_add(phys_add(phys_mul(F32(w, 8), nz), phys_mul(F32(w, 4), ny)), phys_mul(F32(w, 0), nx));
        t = phys_div(t, den);
        if (!(t < 0.0f || t != t) && !(1.0f /* 0x931f24 */ < t || t != t)) {
            float dx = F32(D, 0), dy = F32(D, 4), dz = F32(D, 8);
            float y = phys_mul(dy, t), xx = phys_mul(dx, t), z = phys_mul(dz, t);
            float ox = phys_add(F32(p0, 0), xx), oy = phys_add(F32(p0, 4), y), oz = phys_add(F32(p0, 8), z);
            SF32(out, 0, ox);
            SF32(out, 4, oy);
            SF32(out, 8, oz);
            result = 1;
        }
    }
    rx_set_sp(sp0);
    return result;
}
RX_FUNCTION(0x008523b0, phys_segment_plane_intersect)

/* 0085dcf0 Contacts_Average (ESI = contact set S: count at +0, entries of three vec4 from +0x10 with stride 0x30; stack arg = out
 * of three vec4): out+0x00 = (sum of entry vec 0) / count, out+0x20 = (sum of entry vec 2) / count, out+0x10 = the normalized sum
 * of entry vec 1 (the exe's packed normalization of its xyz length: normalize_masked). The sums start from the runtime vec4
 * 0xd61670 and are accumulated in stored order. Returns out. */
uint32_t phys_contacts_average(rx_addr S, rx_addr out) {
    v4_st(out, v4_ld(G_ZERO));
    v4_st(out + 0x20, v4_ld(G_ZERO));
    v4_st(out + 0x10, v4_ld(G_ZERO));
    rx_v4 cnt = v4_splat((float)I32(S, 0));
    for (int32_t i = 0; i < I32(S, 0); i++) {
        rx_addr e = S + 0x10 + 0x30u * (uint32_t)i;
        rx_v4 a = v4_ld(e + 0x20), b = v4_ld(out + 0x20);
        v4_st(out + 0x20, v4_add(a, b));
        a = v4_ld(e); b = v4_ld(out);
        v4_st(out, v4_add(a, b));
        a = v4_ld(e + 0x10); b = v4_ld(out + 0x10);
        v4_st(out + 0x10, v4_add(a, b));
    }
    v4_st(out + 0x20, v4_div(v4_ld(out + 0x20), cnt));
    rx_v4 x1 = v4_div(v4_ld(out), cnt);
    rx_v4 x0 = v4_ld(out + 0x10);
    v4_st(out, x1);
    v4_st(out + 0x10, normalize_masked(x0, phys_dot3(x0, x0)));
    return out;
}
RX_FUNCTION(0x0085dcf0, phys_contacts_average)

/* ---- preparation of the articulated-body factorization, the solve and their matrix dispatchers ----
 * Node (constraint) objects (pointers at M + 0xca0 + 4 i): +0x0c parent index, +0x10 child count, +0x14 child indices, +0x30 /
 * +0x34 linear / angular body, +0x38 .. +0x60 matrix objects (+0x44 the node's articulated inertia K, +0x48 its inverse, +0x4c /
 * +0x50 / +0x54 / +0x58 / +0x5c / +0x60 coupling matrices), +0x40 / +0x64 / +0x68 vector objects (rhs, bias b, solution z);
 * virtual slots 5 / 8 / 9 (byte offsets 0x14 / 0x20 / 0x24). Every function here runs at the original's ESP (the frames are
 * computed from the entry ESP like the prologues do), so the callees see the original's stack positions. */
#define NODES 0xca0u
static rx_addr node_at(rx_addr M, int32_t i) { return U32(M, NODES + 4u * (uint32_t)i); }


void phys_mat_coupling(rx_addr A, rx_addr B, rx_addr C);          /* 00855740 */
void phys_mat_mulvec_sub(rx_addr A, rx_addr x, rx_addr Y);        /* 00857590 */
void phys_mat_mulvec(rx_addr A, rx_addr x, rx_addr y);            /* 00853900 */
void phys_mat6_mulvec_sub(rx_addr A, rx_addr x, rx_addr Y);       /* 00857b30 */

/* out = inverse(mat), by the row count of mat (3, 4, 5 or 6; other counts do nothing). Used three times by 0085cb70. */
static void invert_by_rows(rx_addr mat, rx_addr out) {
    switch (U32(mat, 4)) {
        case 3: CALL_AT(rx_sp(), 1, phys_mat3_invert(out, mat)); break;
        case 4: CALL_AT(rx_sp(), 0, phys_mat4_invert(out, mat)); break;
        case 5: CALL_AT(rx_sp(), 2, phys_mat5_invert(out, mat)); break;
        case 6: CALL_AT(rx_sp(), 1, phys_mat6_invert(mat, out)); break;
        default: break;
    }
}

/* The diagonal of a node's articulated inertia K from its bodies: rows +0x20 / +0x30 / +0x40 = mass * (00d615c0 / 00d616a0 /
 * 00d61680), rows +0xe0 / +0xf0 / +0x100 = inertia * the same, after the off-diagonal blocks +0x50 / +0x90 are cleared (slot 1).
 * (mass and inertia are read before the clears.) The three runtime vec4s are read every time. */
static void fill_inertia(rx_addr node) {
    rx_addr lin = U32(node, 0x30), K = U32(node, 0x44);
    rx_v4 mass = v4_ld(lin + 0x20);
    rx_addr ang = U32(node, 0x34);
    rx_v4 inertia = v4_ld(ang + 0x20);
    phys_vcall0(K + 0x50, 1);
    phys_vcall0(K + 0x90, 1);
    v4_st(K + 0x20, v4_mul(mass, v4_ld(0x00d615c0)));
    v4_st(K + 0x30, v4_mul(mass, v4_ld(0x00d616a0)));
    v4_st(K + 0x40, v4_mul(mass, v4_ld(0x00d61680)));
    v4_st(K + 0xe0, v4_mul(inertia, v4_ld(0x00d615c0)));
    v4_st(K + 0xf0, v4_mul(inertia, v4_ld(0x00d616a0)));
    v4_st(K + 0x100, v4_mul(inertia, v4_ld(0x00d61680)));
}

/* K(node) -= child.slot8() for every child, in stored order (count and index re-read). */
static void subtract_children(rx_addr M, rx_addr node) {
    for (int32_t k = 0; k < I32(node, 0x10); k++) {
        rx_addr child = node_at(M, I32(node, 0x14 + 4 * (uint32_t)k));
        rx_addr K = U32(node, 0x44);
        uint32_t c = phys_vcall0(child, 8);
        phys_vcall2(K, 4, K, c);
    }
}

/* 0085cb70 Mechanism_Prepare (stack arg M): preparation of the factorization. Pass 1, descending over the non-root nodes: the
 * inertia diagonal, then +0x3c slot 2 -> +0x4c (transpose), +0x54 slot 1 (clear), +0x5c slot 3 (copy of +0x38), +0x5c slot 2 ->
 * +0x60 (transpose); then the root's inertia diagonal. Pass 2, descending over the non-root nodes: K -= the children's
 * contributions, +0x48 = inverse(K), +0x50 = transpose(+0x4c), coupling (+0x48, +0x4c, +0x50), +0x54 -= node.slot9(),
 * +0x58 = inverse(+0x54), coupling (+0x58, +0x5c, +0x60). Finally the root: K -= children, +0x48 = inverse(K).
 * Frame: esp = ((entry esp - 4) & ~15) - 0x40 (sub 0x34 and three pushes). */
void phys_mech_prepare(rx_addr M) {
    const rx_addr sp0 = rx_sp();
    const rx_addr cur = ((sp0 - 4) & ~15u) - 0x40;
    rx_set_sp(cur);
    for (int32_t i = I32(M, 0x1c) - 1; i > 0; i--) {
        rx_addr node = node_at(M, i);
        fill_inertia(node);
        phys_vcall1(U32(node, 0x3c), 2, U32(node, 0x4c));
        phys_vcall0(U32(node, 0x54), 1);
        phys_vcall1(U32(node, 0x5c), 3, U32(node, 0x38));
        phys_vcall1(U32(node, 0x5c), 2, U32(node, 0x60));
    }
    fill_inertia(node_at(M, 0));
    for (int32_t i = I32(M, 0x1c) - 1; i > 0; i--) {
        rx_addr node = node_at(M, i);
        subtract_children(M, node);
        invert_by_rows(U32(node, 0x44), U32(node, 0x48));
        phys_vcall1(U32(node, 0x4c), 2, U32(node, 0x50));
        CALL_AT(cur, 0, phys_mat_coupling(U32(node, 0x48), U32(node, 0x4c), U32(node, 0x50)));
        rx_addr S = U32(node, 0x54);
        uint32_t c = phys_vcall0(node, 9);
        phys_vcall2(S, 4, S, c);
        invert_by_rows(U32(node, 0x54), U32(node, 0x58));
        CALL_AT(cur, 0, phys_mat_coupling(U32(node, 0x58), U32(node, 0x5c), U32(node, 0x60)));
    }
    rx_addr root = node_at(M, 0);
    subtract_children(M, root);
    invert_by_rows(U32(root, 0x44), U32(root, 0x48));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0085cb70, phys_mech_prepare)

/* 00857b30 Mat6_MulVecSub (ECX = A: 6x6 of 3x3 blocks at +0x10 / +0x50 / +0x90 / +0xd0, stack arg = x: Vec6, ESI = Y: Vec6):
 * Y0 -= A00 x0 + A01 x1; Y1 -= A10 x0 + A11 x1 through four 3x3 matrix-vector products (00854490) on the stack temporaries
 * t1..t4 at ((entry esp - 4) & ~15) - 0x30 / - 0x50 / - 0x40 / - 0x20 (the original's frame; esp = that frame's - 0x60). */
void phys_mat6_mulvec_sub(rx_addr A, rx_addr x, rx_addr Y) {
    const rx_addr sp0 = rx_sp();
    const rx_addr base = (sp0 - 4) & ~15u;
    const rx_addr t1 = base - 0x30, t2 = base - 0x50, t3 = base - 0x40, t4 = base - 0x20;
    rx_set_sp(base - 0x60);
    const rx_addr cur = base - 0x60;
    CALL_AT(cur, 1, phys_mat3_mulvec(t1, x + 0x10, A + 0x10));
    CALL_AT(cur, 1, phys_mat3_mulvec(t2, x + 0x20, A + 0x50));
    CALL_AT(cur, 1, phys_mat3_mulvec(t3, x + 0x10, A + 0x90));
    CALL_AT(cur, 1, phys_mat3_mulvec(t4, x + 0x20, A + 0xd0));
    rx_v4 s = v4_add(v4_ld(t1), v4_ld(t2));
    v4_st(Y + 0x10, v4_sub(v4_ld(Y + 0x10), s));
    s = v4_add(v4_ld(t3), v4_ld(t4));
    v4_st(Y + 0x20, v4_sub(v4_ld(Y + 0x20), s));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00857b30, phys_mat6_mulvec_sub)

/* 00857590 Mat_MulVecSub (ECX = A, EDX = x, stack arg = Y): Y -= A x by A's shape (rows x cols): 3 x 6, 4 x 6, 5 x 6, and for 6
 * rows 6 x 3 / 6 x 4 / 6 x 5 / 6 x 6, each through its own kernel (the 6 x 5 and 6 x 6 and 5 x 6 shapes through Mat6_MulVecSub);
 * other shapes do nothing. Frame: two pushes (esp = entry - 8). */
void phys_mat_mulvec_sub(rx_addr A, rx_addr x, rx_addr Y) {
    const rx_addr sp0 = rx_sp();
    const rx_addr cur = sp0 - 8;
    const uint32_t rows = U32(A, 4), cols = U32(A, 8);
    rx_set_sp(cur);
    switch (rows) {
        case 3: if (cols == 6) CALL_AT(cur, 1, phys_mat3x6_mulvec_sub(Y, x, A)); break;
        case 4: if (cols == 6) CALL_AT(cur, 1, phys_mat4x6_mulvec_sub(Y, x, A)); break;
        case 5: if (cols == 6) CALL_AT(cur, 1, phys_mat6_mulvec_sub(A, x, Y)); break;
        case 6:
            switch (cols) {
                case 3: CALL_AT(cur, 1, phys_mat6x3_mulvec_sub(Y, x, A)); break;
                case 4: CALL_AT(cur, 1, phys_mat6x4_mulvec_sub(Y, x, A)); break;
                case 5: case 6: CALL_AT(cur, 1, phys_mat6_mulvec_sub(A, x, Y)); break;
                default: break;
            }
            break;
        default: break;
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00857590, phys_mat_mulvec_sub)

/* 00853900 Mat_MulVec (EAX = A, EDX = x, ESI = y): y = A x by A's shape: 3 x 3 (00854520), 3 x 4 (008545b0), 3 x 6 (00853d80),
 * 4 x 3 (00853ca0), 4 x 4 (00853bd0), 4 x 6 (00853a30), 5 x 5 / 5 x 6 (008542c0), 6 x 3 (00853fa0), 6 x 4 (00853e80),
 * 6 x 5 / 6 x 6 (008540a0); other shapes do nothing. The 3 x 6 and 4 x 6 kernels take (x, y) in (ECX, EDX), the others (y, x).
 * Frame: one push (esp = entry - 4). */
void phys_mat_mulvec(rx_addr A, rx_addr x, rx_addr y) {
    const rx_addr sp0 = rx_sp();
    const uint32_t rows = U32(A, 4), cols = U32(A, 8);
    rx_set_sp(sp0 - 4);
    const rx_addr cur = sp0 - 4;
    switch (rows) {
        case 3:
            if (cols == 3) CALL_AT(cur, 1, phys_mat3x3_mulvec(y, x, A));
            else if (cols == 4) CALL_AT(cur, 1, phys_mat3x4_mulvec(y, x, A));
            else if (cols == 6) CALL_AT(cur, 1, phys_mat3x6_mulvec(x, y, A));
            break;
        case 4:
            if (cols == 3) CALL_AT(cur, 1, phys_mat4x3_mulvec(y, x, A));
            else if (cols == 4) CALL_AT(cur, 1, phys_mat4_mulvec(y, x, A));
            else if (cols == 6) CALL_AT(cur, 1, phys_mat4x6_mulvec(x, y, A));
            break;
        case 5: if (cols == 5 || cols == 6) CALL_AT(cur, 1, phys_mat5x6_mulvec(y, x, A)); break;
        case 6:
            if (cols == 3) CALL_AT(cur, 1, phys_mat6x3_mulvec(y, x, A));
            else if (cols == 4) CALL_AT(cur, 1, phys_mat6x4_mulvec(y, x, A));
            else if (cols == 5 || cols == 6) CALL_AT(cur, 1, phys_mat6_mulvec(y, x, A));
            break;
        default: break;
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00853900, phys_mat_mulvec)

/* 0085cf80 Mechanism_Solve (stack arg M): the solve with the factorization. Backward, over the non-root nodes descending and then
 * the node left over (index min(count - 1, 0)): bias b (+0x64) cleared (slot 0), b -= child.C (+0x60) * child.z (+0x68) for
 * every child, z (+0x68) = node +0x40 (slot 1), z -= (+0x50) b (not for the last node). The root: b = (+0x48) b. Forward, over the
 * non-root nodes ascending: z = (+0x58) z, z -= (+0x5c) parent.b, b = (+0x48) b, b -= (+0x4c) z, then node slot 5.
 * Frame: sub 8 and four pushes (esp = entry - 0x18). */
static void solve_children(rx_addr M, rx_addr node, rx_addr cur) {
    for (int32_t k = 0; k < I32(node, 0x10); k++) {
        rx_addr child = node_at(M, I32(node, 0x14 + 4 * (uint32_t)k));
        CALL_AT(cur, 1, phys_mat_mulvec_sub(U32(child, 0x60), U32(child, 0x68), U32(node, 0x64)));
    }
}
void phys_mech_solve(rx_addr M) {
    const rx_addr sp0 = rx_sp();
    const rx_addr cur = sp0 - 0x18;
    rx_set_sp(cur);
    int32_t i = I32(M, 0x1c) - 1;
    for (; i > 0; i--) {
        rx_addr node = node_at(M, i);
        phys_vcall0(U32(node, 0x64), 0);
        solve_children(M, node, cur);
        phys_vcall1(U32(node, 0x68), 1, U32(node, 0x40));
        CALL_AT(cur, 1, phys_mat_mulvec_sub(U32(node, 0x50), U32(node, 0x64), U32(node, 0x68)));
    }
    rx_addr last = node_at(M, i);
    phys_vcall0(U32(last, 0x64), 0);
    solve_children(M, last, cur);
    rx_addr root = node_at(M, 0);
    CALL_AT(cur, 0, phys_mat_mulvec(U32(root, 0x48), U32(root, 0x64), U32(root, 0x64)));
    for (int32_t k = 1; k < I32(M, 0x1c); k++) {
        rx_addr node = node_at(M, k);
        rx_addr parent = node_at(M, I32(node, 0xc));
        CALL_AT(cur, 0, phys_mat_mulvec(U32(node, 0x58), U32(node, 0x68), U32(node, 0x68)));
        CALL_AT(cur, 1, phys_mat_mulvec_sub(U32(node, 0x5c), U32(parent, 0x64), U32(node, 0x68)));
        CALL_AT(cur, 0, phys_mat_mulvec(U32(node, 0x48), U32(node, 0x64), U32(node, 0x64)));
        CALL_AT(cur, 1, phys_mat_mulvec_sub(U32(node, 0x4c), U32(node, 0x68), U32(node, 0x64)));
        phys_vcall0(node, 5);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0085cf80, phys_mech_solve)

/* 00853740 Mat3_Mul (ECX = B: Matrix3x3 object, EDI = A, ESI = out): out = A * B through B's transpose (slot 2) into a
 * Matrix3x3 local at the frame's bottom: out row j = (A_j . T0, A_j . T1, A_j . T2, A_j . T2) with 3-element dots ((x + y) + z),
 * all stored after all rows are computed (out may alias A). Frame: esp = ((entry esp - 4) & ~15) - 0x80, T at esp. */
void phys_mat3_mul(rx_addr B, rx_addr A, rx_addr out) {
    const rx_addr sp0 = rx_sp();
    const rx_addr T = ((sp0 - 4) & ~15u) - 0x80;
    rx_set_sp(T);
    SU32(T, 4, 3);
    SU32(T, 8, 3);
    SU32(T, 0, 0x0093ca04);                 /* Matrix3x3 vtable */
    phys_vcall1(B, 2, T);
    rx_v4 T0 = v4_ld(T + 0x10), T1 = v4_ld(T + 0x20), T2 = v4_ld(T + 0x30);
    float d[3][3];
    for (int j = 0; j < 3; j++) {
        rx_v4 a = v4_ld(A + 0x10 + 0x10u * (uint32_t)j);
        d[j][0] = phys_dot3(a, T0);
        d[j][1] = phys_dot3(a, T1);
        d[j][2] = phys_dot3(a, T2);
    }
    for (int j = 0; j < 3; j++) v4_st(out + 0x10 + 0x10u * (uint32_t)j, v4(d[j][0], d[j][1], d[j][2], d[j][2]));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00853740, phys_mat3_mul)

/* 00858fe0 Mat_Congruence (ECX = A, EDX = B, stack args C and D): C = B * D * B^T-style congruence products selected by C's row
 * count (3..6) and, for 3 and 6, A's row count: the shape-specific kernels with a temporary block matrix L built at the original's
 * stack address (a Matrix3x3 object T for 3 x 3). Only the shapes the vehicles use matter. Frame: esp E = ((entry esp - 4) & ~15)
 * - 0x170 (sub 0x164 and three pushes); T = E + 0x10, L = E + 0x50. */
void phys_mat_congruence(rx_addr A, rx_addr B, rx_addr C, rx_addr D) {
    const rx_addr sp0 = rx_sp();
    const rx_addr E = ((sp0 - 4) & ~15u) - 0x170;
    const rx_addr T = E + 0x10, L = E + 0x50;
    const uint32_t arows = U32(A, 4);
    rx_set_sp(E);
    switch (U32(C, 4)) {
        case 3:
            if (arows == 3) {
                SU32(T, 4, 3); SU32(T, 8, 3); SU32(T, 0, 0x0093ca04);
                CALL_AT(E, 1, phys_mat3_mul_abt(T, B, D));
                CALL_AT(E, 1, phys_mat3_mul_abt(C, T, B));
            } else if (arows == 6) {
                CALL_AT(E, 0, phys_matrix3x6_construct(L));
                CALL_AT(E, 1, phys_mat3x6_mul_abt(L, D, B));
                CALL_AT(E, 1, phys_mat3x3_mul_abt_sum(C, L, B));
            }
            break;
        case 4:
            CALL_AT(E, 0, phys_matrix4x6_construct(L));
            CALL_AT(E, 1, phys_mat4x6_mul_abt(L, D, B));
            CALL_AT(E, 1, phys_mat4x4_mul_abt_sum(C, L, B));
            break;
        case 5:
            CALL_AT(E, 0, phys_matrix5x6_construct(L));
            CALL_AT(E, 1, phys_mat6_mul_abt(B, L, D));
            CALL_AT(E, 1, phys_mat6_mul_abt(L, C, B));
            break;
        case 6:
            switch (arows) {
                case 3:
                    CALL_AT(E, 0, phys_matrix6x3_construct(L));
                    CALL_AT(E, 1, phys_mat3_mul_blocks(B, D, L));
                    CALL_AT(E, 2, phys_mat6_mul_3t_blocks(C, L, B));
                    break;
                case 4:
                    CALL_AT(E, 0, phys_matrix6x4_construct(L));
                    CALL_AT(E, 1, phys_mat3x4_mul_blocks(B, D, L));
                    CALL_AT(E, 2, phys_mat6_mul_3x4t_blocks(C, L, B));
                    break;
                case 5:
                    CALL_AT(E, 0, phys_matrix6x5_construct(L));
                    CALL_AT(E, 1, phys_mat6_mul_abt(B, L, D));
                    CALL_AT(E, 1, phys_mat6_mul_abt(L, C, B));
                    break;
                case 6:
                    CALL_AT(E, 0, phys_matrix6x6_construct(L));
                    CALL_AT(E, 1, phys_mat6_mul_abt(B, L, D));
                    CALL_AT(E, 1, phys_mat6_mul_abt(L, C, B));
                    break;
                default: break;
            }
            break;
        default: break;
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00858fe0, phys_mat_congruence)

/* 00855740 Mat_Coupling (EAX = A, ECX = B, EDI = C): the coupling products of the preparation by A's shape: 3 rows 00857220, 4 rows
 * 00856b40, 5 rows (and 6 x 5 / 6 x 6) C-product B = A * C^T (00854f00) then C = transpose(B) (B's slot 2), 6 x 3 00855810,
 * 6 x 4 00855ee0. Frame: one push (esp = entry - 4). */
void phys_mat_coupling(rx_addr A, rx_addr B, rx_addr C) {
    const rx_addr sp0 = rx_sp();
    const uint32_t arows = U32(A, 4);
    rx_set_sp(sp0 - 4);
    int via = 0;
    switch (arows) {
        case 3: CALL_AT(sp0 - 4, 1, phys_coupling3(A, B, C)); break;
        case 4: CALL_AT(sp0 - 4, 1, phys_coupling4(B, A, C)); break;
        case 5: via = 1; break;
        case 6:
            switch (U32(B, 8)) {
                case 3: CALL_AT(sp0 - 4, 1, phys_coupling6x3(A, B, C)); break;
                case 4: CALL_AT(sp0 - 4, 1, phys_coupling6x4(A, B, C)); break;
                case 5: case 6: via = 1; break;
                default: break;
            }
            break;
        default: break;
    }
    if (via) {
        CALL_AT(sp0 - 4, 1, phys_mat6_mul_abt(A, B, C));
        phys_vcall1(B, 2, C);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00855740, phys_mat_coupling)

/* 00401700 Vec3_Sub (ECX = a, EDX = b, stack arg = out vec3): out = b - a componentwise (scalar), stored x, y, z. Returns out. */
rx_addr phys_vec3_sub(rx_addr a, rx_addr b, rx_addr out) {
    SF32(out, 0, phys_sub(F32(b, 0), F32(a, 0)));
    SF32(out, 4, phys_sub(F32(b, 4), F32(a, 4)));
    SF32(out, 8, phys_sub(F32(b, 8), F32(a, 8)));
    return out;
}
RX_FUNCTION(0x00401700, phys_vec3_sub)
