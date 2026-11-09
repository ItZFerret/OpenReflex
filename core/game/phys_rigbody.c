/* phys_rigbody.c -- the rider rig's rigid bodies (0x0042xxxx): a small articulated set of bodies the rider animation simulates
 * (P = rider animation + 0x380, passed in ESI / EDI / EDX / ECX by the LTCG callers).
 *   P+0x14 / P+0x6c   linear / angular damping base; P+0x18 + 4 i per-body damping
 *   P+0x7c            body count
 *   linear state, body i at P + 0x80 i:  +0x80 position, +0x90 velocity, +0xa0 mass (vec4), +0xb0 inverse mass (vec4),
 *                                       +0xd0 force, +0xe0 impulse accumulator
 *   angular state, body i at P + 0x70 i: +0xb00 orientation (quaternion), +0xb10 angular velocity (body frame),
 *                                       +0xb30 inverse inertia (body frame), +0xb50 torque (body frame), +0xb60 angular
 *                                       impulse accumulator
 *   saved velocities, body i at P + 0x40 i: +0x1440 linear, +0x1460 angular
 *   rig description: +0x1970 root joint, joints of 0x230 bytes at +0x39e4 (+0 offset record, +8 parent joint (-5: none)),
 *                    offset records of 0x170 bytes at +0x1b60 (local position). */
#include "phys_rigbody.h"

#define LIN(P, i) ((P) + 0x80u * (uint32_t)(i))   /* + 0x80 position, 0x90 velocity, 0xa0 mass, 0xb0 inverse mass ... */
#define ANG(P, i) ((P) + 0x70u * (uint32_t)(i))   /* + 0xb00 orientation, 0xb10 angular velocity ... */

/* 00422550 (EDI = P): saves every body's linear and angular velocity. */
void phys_rigbody_save_velocities(rx_addr P) {
    for (int32_t i = 0; i < I32(P, 0x7c); i++) {
        v4_st(P + 0x1440 + 0x40u * (uint32_t)i, v4_ld(LIN(P, i) + 0x90));
        v4_st(P + 0x1460 + 0x40u * (uint32_t)i, v4_ld(ANG(P, i) + 0xb10));
    }
}
RX_FUNCTION(0x00422550, phys_rigbody_save_velocities)

/* 00422590 (EDI = P): restores the saved velocities. */
void phys_rigbody_restore_velocities(rx_addr P) {
    for (int32_t i = 0; i < I32(P, 0x7c); i++) {
        v4_st(LIN(P, i) + 0x90, v4_ld(P + 0x1440 + 0x40u * (uint32_t)i));
        v4_st(ANG(P, i) + 0xb10, v4_ld(P + 0x1460 + 0x40u * (uint32_t)i));
    }
}
RX_FUNCTION(0x00422590, phys_rigbody_restore_velocities)

/* 00422800 (ESI = P): clears every body's impulse / force and angular impulse / torque (runtime zero vec4 0x00d61670). */
void phys_rigbody_clear_forces(rx_addr P) {
    for (int32_t i = 0; i < I32(P, 0x7c); i++) {
        v4_st(LIN(P, i) + 0xe0, v4_ld(0x00d61670));
        v4_st(LIN(P, i) + 0xd0, v4_ld(0x00d61670));
        v4_st(ANG(P, i) + 0xb60, v4_ld(0x00d61670));
        v4_st(ANG(P, i) + 0xb50, v4_ld(0x00d61670));
    }
}
RX_FUNCTION(0x00422800, phys_rigbody_clear_forces)

/* mass-weighted mean of a per-body vec4 (field +0x80 position / +0x90 velocity): sum(v * mass) / sum(mass), the running sum
 * kept in *out as the original does */
static void mass_weighted_mean(rx_addr P, uint32_t field, rx_addr out) {
    v4_st(out, v4_ld(0x00d61670));
    rx_v4 mass_sum = v4_ld(0x00d61670);
    for (int32_t i = 0; i < I32(P, 0x7c); i++) {
        const rx_v4 mass = v4_ld(LIN(P, i) + 0xa0);
        v4_st(out, v4_add(v4_mul(v4_ld(LIN(P, i) + field), mass), v4_ld(out)));
        mass_sum = v4_add(mass_sum, v4_ld(LIN(P, i) + 0xa0));
    }
    v4_st(out, v4_div(v4_ld(out), mass_sum));
}
/* 00422300 (ESI = P, ECX = out): the rig's center of mass. */
void phys_rigbody_center_of_mass(rx_addr P, rx_addr out) { mass_weighted_mean(P, 0x80, out); }
RX_FUNCTION(0x00422300, phys_rigbody_center_of_mass)
/* 00422350 (ESI = P, ECX = out): the rig's mass-weighted mean velocity. */
void phys_rigbody_mean_velocity(rx_addr P, rx_addr out) { mass_weighted_mean(P, 0x90, out); }
RX_FUNCTION(0x00422350, phys_rigbody_mean_velocity)

/* 004223a0 (ESI = P, EDI = axis*, stack out*, point*; RET 8): the rig's moment of inertia about the axis through point:
 * sum(mass * r * r), r = |d - (axis . d) axis| with d = position - point (|.|^2 summed (x + z) + y). */
void phys_rigbody_axis_inertia(rx_addr P, rx_addr axis, rx_addr out, rx_addr point) {
    v4_st(out, v4_ld(0x00d61670));
    for (int32_t i = 0; i < I32(P, 0x7c); i++) {
        const rx_v4 a = v4_ld(axis);
        const rx_v4 d = v4_sub(v4_ld(LIN(P, i) + 0x80), v4_ld(point));
        const rx_v4 perp = v4_sub(d, v4_mul(v4_splat(phys_dot3(a, d)), a));
        const rx_v4 p = v4_mul(perp, perp);
        const rx_v4 r = v4_splat(phys_sqrt((p.v[0] + p.v[2]) + p.v[1]));
        v4_st(out, v4_add(v4_mul(v4_mul(v4_ld(LIN(P, i) + 0xa0), r), r), v4_ld(out)));
    }
}
RX_FUNCTION(0x004223a0, phys_rigbody_axis_inertia)

/* 00422180 (EDX = P, ESI = body, EDI = impulse*, stack lever*; RET 4): applies an impulse at a lever arm (body frame):
 * velocity += inverse mass * impulse; t = lever x (conj(q) * impulse * q) (the impulse in the body frame, then the torque);
 * angular velocity += inverse inertia * t; the impulse and t are added to the accumulators (+0xe0 / +0xb60). */
void phys_rigbody_apply_impulse(rx_addr P, int32_t body, rx_addr impulse, rx_addr lever) {
    const rx_addr lin = LIN(P, body), ang = ANG(P, body);
    v4_st(lin + 0x90, v4_add(v4_mul(v4_ld(lin + 0xb0), v4_ld(impulse)), v4_ld(lin + 0x90)));
    const rx_v4 q = v4_ld(ang + 0xb00);
    const rx_v4 local = phys_quat_mul(phys_quat_conj(q), phys_quat_mul(v4_xyz(v4_ld(impulse)), q));
    const rx_v4 torque = phys_cross_ba(local, v4_ld(lever));
    v4_st(ang + 0xb10, v4_add(v4_mul(v4_ld(ang + 0xb30), torque), v4_ld(ang + 0xb10)));
    v4_st(lin + 0xe0, v4_add(v4_ld(impulse), v4_ld(lin + 0xe0)));
    v4_st(ang + 0xb60, v4_add(v4_ld(ang + 0xb60), torque));
}
RX_FUNCTION(0x00422180, phys_rigbody_apply_impulse)

/* 00422700 (ECX = P, stack body, torque*; RET 8): torque (world frame) into the body frame (conj(q) * t * q), added to the
 * body's torque +0xb50. */
void phys_rigbody_add_torque(rx_addr P, int32_t body, rx_addr torque) {
    const rx_addr ang = ANG(P, body);
    const rx_v4 q = v4_ld(ang + 0xb00);
    const rx_v4 local = phys_quat_mul(phys_quat_conj(q), phys_quat_mul(v4_xyz(v4_ld(torque)), q));
    v4_st(ang + 0xb50, v4_add(local, v4_ld(ang + 0xb50)));
}
RX_FUNCTION(0x00422700, phys_rigbody_add_torque)

/* 004241f0 (ECX = vector* in / out, EDX = P, stack body; RET 4): the vector (world frame) into the body frame, stored back, and
 * added to the body's angular velocity. */
void phys_rigbody_add_angular_velocity(rx_addr v, rx_addr P, int32_t body) {
    const rx_addr ang = ANG(P, body);
    const rx_v4 q = v4_ld(ang + 0xb00);
    const rx_v4 local = phys_quat_mul(phys_quat_conj(q), phys_quat_mul(v4_xyz(v4_ld(v)), q));
    v4_st(v, local);
    v4_st(ang + 0xb10, v4_add(local, v4_ld(ang + 0xb10)));
}
RX_FUNCTION(0x004241f0, phys_rigbody_add_angular_velocity)

/* 004225d0 (EDX = out*, stack P; RET 4): the rig's reference point: body 0's position plus, when the root joint (+0x1970) has a
 * parent (its +8 is not -5), the parent's offset (record +0x1b60) rotated by body 0's orientation. */
void phys_rigbody_reference_point(rx_addr out, rx_addr P) {
    const int32_t parent = I32(P + 0x39ec, U32(P, 0x1970) * 0x230);
    if (parent == -5) { v4_st(out, v4_ld(P + 0x80)); return; }
    const rx_v4 q = v4_ld(P + 0xb00);
    const uint32_t record = U32(P + 0x39e4, (uint32_t)parent * 0x230) * 0x170;
    const rx_v4 offset = v4_xyz(v4_ld(P + 0x1b60 + record));
    v4_st(out, phys_quat_mul(q, phys_quat_mul(offset, phys_quat_conj(q))));
    v4_st(out, v4_add(v4_ld(P + 0x80), v4_ld(out)));
}
RX_FUNCTION(0x004225d0, phys_rigbody_reference_point)

/* 00423980 (stack P, xmm0 = dt; RET 4): velocity damping: k = dt * 50 (00977778, double), per body
 * linear *= 1 - (P+0x18[i] + P+0x14) k, angular *= 1 - (P+0x18[i] + P+0x6c) k (in double precision, rounded to float). */
void phys_rigbody_damp(rx_addr P, float dt) {
    const float k = (float)((double)dt * 50.0);
    for (int32_t i = 0; i < I32(P, 0x7c); i++) {
        float f = (float)(1.0 - ((double)F32(P, 0x18 + 4 * i) + (double)F32(P, 0x14)) * (double)k);
        v4_st(LIN(P, i) + 0x90, v4_mul(v4_splat(f), v4_ld(LIN(P, i) + 0x90)));
        f = (float)(1.0 - ((double)F32(P, 0x18 + 4 * i) + (double)F32(P, 0x6c)) * (double)k);
        v4_st(ANG(P, i) + 0xb10, v4_mul(v4_splat(f), v4_ld(ANG(P, i) + 0xb10)));
    }
}
RX_FUNCTION(0x00423980, phys_rigbody_damp)

/* the game's LCG (seed 0x009cb860, draw count 0x009cb864): one draw, uniform in [-0.5, 0.5) (in double: r * 2^-15 - 0.5) */
static float rand_signed_half(void) {
    const uint32_t seed = U32(0, 0x009cb860) * 0x41c64e6du + 0x3039u;
    SU32(0, 0x009cb860, seed);
    const float r = (float)((double)(float)(int32_t)((seed >> 16) & 0x7fff) * rx_bd(0x3f00000000000000ull));   /* 009776a0 */
    return (float)((double)r - 0.5);                                                                            /* 009774e8 */
}
/* 004240e0 (ECX = body, ESI = P, stack scale; RET 4): random spin: angular velocity += (r3, r2, r1, r1) * scale with three
 * draws r1, r2, r3 of the game's random generator (the draw count +3). */
void phys_rigbody_random_spin(rx_addr P, int32_t body, float scale) {
    const float r1 = rand_signed_half(), r2 = rand_signed_half(), r3 = rand_signed_half();
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 3);
    const rx_addr w = ANG(P, body) + 0xb10;
    v4_st(w, v4_add(v4_mul(v4(r3, r2, r1, r1), v4_splat(scale)), v4_ld(w)));
}
RX_FUNCTION(0x004240e0, phys_rigbody_random_spin)

/* 00422450 (EDX = P, ESI = world vector v): adds v to every body's angular velocity in its own frame: w += q^-1 (xyz(v) q). */
void phys_rigbody_add_world_angular_velocity(rx_addr P, rx_addr v) {
    for (int32_t i = 0; i < I32(P, 0x7c); i++) {
        const rx_addr a = ANG(P, i);
        const rx_v4 q = v4_ld(a + 0xb00);
        const rx_v4 local = phys_quat_mul(phys_quat_conj(q), phys_quat_mul(v4_xyz(v4_ld(v)), q));
        v4_st(a + 0xb10, v4_add(local, v4_ld(a + 0xb10)));
    }
}
RX_FUNCTION(0x00422450, phys_rigbody_add_world_angular_velocity)

/* 00423a50 (ESI = P, xmm0 = dt): explicit Euler step of every body: v += (force * inverse mass) dt, w += (torque * inverse
 * inertia) dt, p += v dt, q += (q (w, w_pad)) dt / 2 (the quaternion's w lane of the angular velocity comes from the runtime
 * vec4 0x00d61660 through the stack, as the original builds it). */
void phys_rigbody_integrate(rx_addr P, float dt) {
    const rx_v4 h = v4_splat(dt), half = v4_splat((float)((double)dt * 0.5));
    for (int32_t i = 0; i < I32(P, 0x7c); i++) {
        const rx_addr l = LIN(P, i) + 0x80, a = ANG(P, i) + 0xb00;
        v4_st(l + 0x10, v4_add(v4_mul(v4_mul(v4_ld(l + 0x40), v4_ld(l + 0x30)), h), v4_ld(l + 0x10)));
        v4_st(a + 0x10, v4_add(v4_mul(v4_mul(v4_ld(a + 0x40), v4_ld(a + 0x30)), h), v4_ld(a + 0x10)));
        v4_st(l, v4_add(v4_mul(v4_ld(l + 0x10), h), v4_ld(l)));
        rx_v4 w = v4_ld(a + 0x10);
        w.v[3] = F32(0x00d61660, 0xc);
        const rx_v4 q = v4_ld(a);
        v4_st(a, v4_add(v4_mul(phys_quat_mul(q, w), half), q));
    }
}
RX_FUNCTION(0x00423a50, phys_rigbody_integrate)
