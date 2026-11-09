/* bike_remote.c -- see bike_remote.h. Hand transliteration of 00432be0, 004331d0 and the track filters 0042e660 / 0042e6d0 /
 * 0042e720 / 0042e840 / 0042e8e0 / 0042e9f0 / 0042eb20 / 0042ec50 / 0042ee50 / 0042ee90, from the disassembly.
 *
 * All SSE (packed operations in the original's operand order: phys_vec.h's v4_* keep x86's NaN rule); the x87 instructions are
 * fld / fstp copies of floats (bike_x87_copy32: exact, a signaling NaN comes out quiet). Main thread (the vehicle's pre-step). */
#include "bike_remote.h"
#include "bike_math.h"
#include "phys_crt.h"
#include "phys_mech.h"
#include "race.h"
#include "rx_vcall.h"

#define G_ZERO      0x00d61670u   /* the runtime zero vector */
#define G_IDENTITY  0x00d61640u   /* the runtime identity quaternion (0, 0, 0, 1) */

/* ---- small helpers ------------------------------------------------------------------------------------------------------------ */

static float min_one(float r) { return 1.0f > r ? r : 1.0f; }      /* comiss 1.0, r; ja: r, else 1.0 (NaN gives 1.0) */

/* the 4-lane length^2 of the quaternion normalize in 0042ec50: (s3 + s1) + (s2 + s0) of s = q * q */
static float quat_len2(rx_v4 q) {
    const rx_v4 s = v4_mul(q, q);
    return phys_add(phys_add(s.v[3], s.v[1]), phys_add(s.v[2], s.v[0]));
}

/* the frame of `push ebp; mov ebp, esp; and esp, -16; sub esp, size` */
static rx_addr aligned_frame(rx_addr entry, uint32_t size) {
    return ((entry - 4u) & ~15u) - size;
}

/* ---- vector tracks ----------------------------------------------------------------------------------------------------------- */

/* 0042e660: the three keys and the output = zero, times 0 / -0.3 / -0.6, filter time 1 */
rx_addr bike_track_reset(rx_addr track) {
    v4_st(track + 0x00, v4_ld(G_ZERO));
    v4_st(track + 0x10, v4_ld(G_ZERO));
    v4_st(track + 0x20, v4_ld(G_ZERO));
    SF32(track, 0x30, 0.0f);
    SF32(track, 0x34, -0.3f);                  /* 0x9774e0 */
    SF32(track, 0x38, -0.6f);                  /* 0x9774dc */
    v4_st(track + 0x80, v4_ld(G_ZERO));
    SF32(track, 0x3c, 1.0f);                   /* 0x931f24 */
    return track;
}
RX_FUNCTION(0x0042e660, bike_track_reset)

/* 0042e6d0: the same with the keys and the output = *value */
rx_addr bike_track_reset_to(rx_addr value, rx_addr track) {
    v4_st(track + 0x00, v4_ld(value));
    v4_st(track + 0x10, v4_ld(value));
    v4_st(track + 0x20, v4_ld(value));
    SF32(track, 0x30, 0.0f);
    SF32(track, 0x34, -0.3f);
    SF32(track, 0x38, -0.6f);
    v4_st(track + 0x80, v4_ld(value));
    return track;
}
RX_FUNCTION(0x0042e6d0, bike_track_reset_to)

/* 0042e720: a key newer than the newest one shifts in and the cubic through the last three keys is refitted around the middle key:
 * h = time - t1 (t1 = the previous newest time), d0 = (p0 - p1) / (time - t1), d1 = (p1 - p2) / (t1 - t2);
 * c0 = p1, c1 = (d1 + d0) * K [0xd615d0], c3 = ((p0 - p1) * 2 - (c1 + d0) * h) / (3 h^3), c2 = ((d0 - c1) - 3 h^2 c3) / (2 h). */
void bike_track_add_key(rx_addr key, float time, rx_addr track) {
    if (F32(track, 0x30) >= time) return;      /* comiss t0, time; jae */
    v4_st(track + 0x20, v4_ld(track + 0x10));
    v4_st(track + 0x10, v4_ld(track + 0x00));
    SU32(track, 0x38, bike_x87_copy32(U32(track, 0x34)));
    const rx_v4 key_value = v4_ld(key);
    SU32(track, 0x34, bike_x87_copy32(U32(track, 0x30)));
    const rx_v4 p1 = v4_ld(track + 0x10);
    const rx_v4 p2 = v4_ld(track + 0x20);
    v4_st(track + 0x00, key_value);
    const float t1 = F32(track, 0x34);
    SF32(track, 0x30, time);
    const float h = phys_sub(time, t1);
    const rx_v4 d0 = v4_div(v4_sub(v4_ld(track + 0x00), p1), v4_sub(v4_splat(time), v4_splat(t1)));
    const float t2 = F32(track, 0x38);
    const rx_v4 d1 = v4_div(v4_sub(p1, p2), v4_sub(v4_splat(t1), v4_splat(t2)));
    v4_st(track + 0x40, p1);
    const rx_v4 c1 = v4_mul(v4_add(d1, d0), v4_ld(0x00d615d0));
    v4_st(track + 0x50, c1);
    const rx_v4 slope = v4_mul(v4_add(c1, d0), v4_splat(h));
    rx_v4 c3 = v4_mul(v4_sub(v4_ld(track + 0x00), v4_ld(track + 0x10)), v4_splat(2.0f));   /* 0x92e330 */
    const float h2x3 = phys_mul(phys_mul(h, 3.0f), h);                                      /* (h * 3) * h (0x92fe48) */
    c3 = v4_sub(c3, slope);
    const float inv_h3x3 = phys_div(1.0f, phys_mul(h2x3, h));
    const float inv_h2 = phys_div(1.0f, phys_mul(h, 2.0f));
    c3 = v4_mul(c3, v4_splat(inv_h3x3));
    v4_st(track + 0x70, c3);
    const rx_v4 c2 = v4_mul(v4_sub(v4_sub(d0, c1), v4_mul(v4_splat(h2x3), v4_ld(track + 0x70))), v4_splat(inv_h2));
    v4_st(track + 0x60, c2);
}
RX_FUNCTION(0x0042e720, bike_track_add_key)

/* 0042e840: the cubic at x = time - t1 (((x c1 + c0) + x^2 c2) + x^3 c3), the output moved toward it by min(5 / filter time, 1):
 * out = (v - out) * a + out, stored and copied to *out_ptr */
void bike_track_evaluate(rx_addr out, float time, rx_addr track) {
    const float x = phys_sub(time, F32(track, 0x34));
    rx_v4 v = v4_add(v4_mul(v4_splat(x), v4_ld(track + 0x50)), v4_ld(track + 0x40));
    const float x2 = phys_mul(x, x);
    v = v4_add(v, v4_mul(v4_splat(x2), v4_ld(track + 0x60)));
    const float x3 = phys_mul(x2, x);
    v = v4_add(v, v4_mul(v4_splat(x3), v4_ld(track + 0x70)));
    const float a = min_one(phys_div(5.0f /* 0x935890 */, F32(track, 0x3c)));
    const rx_v4 previous = v4_ld(track + 0x80);
    v4_st(track + 0x80, v4_add(v4_mul(v4_sub(v, previous), v4_splat(a)), previous));
    v4_st(out, v4_ld(track + 0x80));
}
RX_FUNCTION(0x0042e840, bike_track_evaluate)

/* ---- scalar tracks ----------------------------------------------------------------------------------------------------------- */

/* 0042ee50: the keys and the output = value, times 0 / -0.3 / -0.6 */
void bike_scalar_track_reset_to(rx_addr track, float value) {
    SF32(track, 0x0c, 0.0f);
    SF32(track, 0x10, -0.3f);
    SF32(track, 0x00, value);
    SF32(track, 0x04, value);
    SF32(track, 0x08, value);
    SF32(track, 0x14, -0.6f);
    SF32(track, 0x2c, value);
}
RX_FUNCTION(0x0042ee50, bike_scalar_track_reset_to)

/* 0042ee90: 0042e720 for one value (its own association of the h powers: (h h) 3 and ((h h) h) 3) */
void bike_scalar_track_add_key(rx_addr track, float time, float value) {
    const float t0 = F32(track, 0x0c);
    if (t0 >= time) return;                    /* comiss t0, time; jae */
    const uint32_t v1_bits = U32(track, 0x04);
    SF32(track, 0x0c, time);
    SU32(track, 0x08, bike_x87_copy32(v1_bits));
    const float h = phys_sub(time, t0);
    SU32(track, 0x04, bike_x87_copy32(U32(track, 0x00)));
    const float v1 = F32(track, 0x04);
    const uint32_t t1_bits = U32(track, 0x10);
    SF32(track, 0x00, value);
    const float dv = phys_sub(value, v1);
    SU32(track, 0x14, bike_x87_copy32(t1_bits));
    const float dt1 = phys_sub(t0, F32(track, 0x14));
    const float d0 = phys_div(dv, h);
    const float dv2 = phys_mul(dv, 2.0f);
    SF32(track, 0x10, t0);
    const float d1 = phys_div(phys_sub(v1, F32(track, 0x08)), dt1);
    const float c1 = phys_mul(phys_add(d1, d0), 0.5f);                                     /* 0x9322cc */
    SF32(track, 0x1c, v1);
    const float hh = phys_mul(h, h);
    const float numerator = phys_sub(dv2, phys_mul(phys_add(c1, d0), h));
    const float hh3 = phys_mul(hh, 3.0f);
    const float hhh3 = phys_mul(phys_mul(hh, h), 3.0f);
    const float h2 = phys_mul(h, 2.0f);
    const float c3 = phys_mul(numerator, phys_div(1.0f, hhh3));
    const float c2 = phys_mul(phys_sub(phys_sub(d0, c1), phys_mul(hh3, c3)), phys_div(1.0f, h2));
    SF32(track, 0x20, c1);
    SF32(track, 0x28, c3);
    SF32(track, 0x24, c2);
}
RX_FUNCTION(0x0042ee90, bike_scalar_track_add_key)

/* the scalar track's filtered value at time (the inlined evaluation in 0042ec50): x = time - t1, ((c1 x + c0) + c2 x^2) + x^3 c3,
 * out = (v - out) * a + out with a = min(5 / filter time, 1) */
static float scalar_track_evaluate(rx_addr track, float time) {
    const float x = phys_sub(time, F32(track, 0x10));
    const float x2 = phys_mul(x, x);
    const float a2 = phys_mul(F32(track, 0x24), x2);
    float v = phys_add(phys_mul(F32(track, 0x20), x), F32(track, 0x1c));
    const float x3 = phys_mul(x2, x);
    const float a3 = phys_mul(x3, F32(track, 0x28));
    v = phys_add(phys_add(v, a2), a3);
    const float a = min_one(phys_div(5.0f, F32(track, 0x18)));
    const float previous = F32(track, 0x2c);
    const float out = phys_add(phys_mul(phys_sub(v, previous), a), previous);
    SF32(track, 0x2c, out);
    return out;
}

/* ---- orientation tracks ------------------------------------------------------------------------------------------------------ */

/* 0042e8e0: the axis track reset (0042e660), the two scalar tracks to 0 (times 0 / -0.3 / -0.6, filter time 1), the quaternion
 * the identity; the axis keys and output = [0xd615c0]. Returns the track. */
rx_addr bike_orient_track_init(rx_addr track) {
    {
        const rx_addr sp = rx_sp();
        rx_set_sp(sp - 8);                     /* push ecx; call */
        bike_track_reset(track);
        rx_set_sp(sp);
    }
    for (uint32_t base = 0x90; base <= 0xc0; base += 0x30) {
        SF32(track, base + 0x00, 0.0f);
        SF32(track, base + 0x04, 0.0f);
        SF32(track, base + 0x08, 0.0f);
        SF32(track, base + 0x0c, 0.0f);
        SF32(track, base + 0x10, -0.3f);
        SF32(track, base + 0x14, -0.6f);
        SF32(track, base + 0x2c, 0.0f);
        SF32(track, base + 0x18, 1.0f);
    }
    v4_st(track + 0xf0, v4_ld(G_IDENTITY));
    v4_st(track + 0x00, v4_ld(0x00d615c0));
    v4_st(track + 0x10, v4_ld(0x00d615c0));
    v4_st(track + 0x20, v4_ld(0x00d615c0));
    SF32(track, 0x30, 0.0f);
    SF32(track, 0x34, -0.3f);
    SF32(track, 0x38, -0.6f);
    v4_st(track + 0x80, v4_ld(0x00d615c0));
    SF32(track, 0x3c, 1.0f);
    SF32(track, 0xa8, 1.0f);
    SF32(track, 0xd8, 1.0f);
    return track;
}
RX_FUNCTION(0x0042e8e0, bike_orient_track_init)

/* A quaternion q split as the tracks keep it: the axis (q.x, q.y, q.z, q.x) normalized ("or NaN", bike_normalize_masked), its
 * length (q.x^2 + q.z^2) + q.y^2, and q.w. */
static rx_v4 quat_axis(rx_v4 q) {
    const rx_v4 v = v4_perm(q, 0, 1, 2, 0);
    return bike_normalize_masked(v, bike_hsum_012(v4_mul(v, v)));
}
static float quat_axis_length(rx_v4 q) {
    const rx_v4 v = v4_perm(q, 0, 1, 2, 0);
    return phys_sqrt(bike_hsum_021(v4_mul(v, v)));
}

/* 0042e9f0: every part reset to the identity quaternion [0xd61640]. Frame: push ebp; and esp, -16; sub esp, 0x30 (the axis at +0x10,
 * the cookie at +0x2c). */
void bike_orient_track_reset(rx_addr track) {
    const rx_addr entry = rx_sp();
    const rx_addr E = aligned_frame(entry, 0x30);
    rx_set_sp(E);
    SU32(E, 0x2c, U32(0, 0x009b45e4) ^ E);
    const rx_v4 q = v4_ld(G_IDENTITY);
    v4_st(E + 0x10, quat_axis(q));
    const float length = quat_axis_length(q);
    rx_set_sp(E - 8);                          /* push edx; call */
    bike_track_reset_to(E + 0x10, track);
    rx_set_sp(E);
    bike_scalar_track_reset_to(track + 0x90, q.v[3]);
    bike_scalar_track_reset_to(track + 0xc0, length);
    v4_st(track + 0xf0, v4_ld(G_IDENTITY));
    rx_set_sp(E - 4);
    phys_security_check_cookie(U32(E, 0x2c) ^ E);
    rx_set_sp(entry);
}
RX_FUNCTION(0x0042e9f0, bike_orient_track_reset)

/* 0042eb20: a quaternion key: its axis into the vector track, its w and axis length into the scalar tracks. Frame: push ebp;
 * and esp, -16; sub esp, 0x40 (the length at +8, w at +0xc, the axis at +0x20, the cookie at +0x3c). */
void bike_orient_track_add_key(rx_addr track, rx_addr key, float time) {
    const rx_addr entry = rx_sp();
    const rx_addr E = aligned_frame(entry, 0x40);
    rx_set_sp(E);
    SU32(E, 0x3c, U32(0, 0x009b45e4) ^ E);
    const rx_v4 q = v4_ld(key);
    v4_st(E + 0x20, quat_axis(q));
    SF32(E, 0x08, quat_axis_length(q));
    SF32(E, 0x0c, q.v[3]);
    rx_set_sp(E - 8);                          /* push edx; call */
    bike_track_add_key(E + 0x20, time, track);
    rx_set_sp(E);
    bike_scalar_track_add_key(track + 0x90, time, F32(E, 0x0c));
    bike_scalar_track_add_key(track + 0xc0, time, F32(E, 0x08));
    rx_set_sp(E - 4);
    phys_security_check_cookie(U32(E, 0x3c) ^ E);
    rx_set_sp(entry);
}
RX_FUNCTION(0x0042eb20, bike_orient_track_add_key)

/* 0042ec50: the filtered quaternion at time: (length * axis.xyz, w) normalized, kept at +0xf0 and copied to *out. Frame: push ebp;
 * and esp, -16; sub esp, 0x30 (the axis at +0x10, the cookie at +0x2c). */
void bike_orient_track_evaluate(rx_addr track, float time, rx_addr out) {
    const rx_addr entry = rx_sp();
    const rx_addr E = aligned_frame(entry, 0x30);
    rx_set_sp(E);
    SU32(E, 0x2c, U32(0, 0x009b45e4) ^ E);
    rx_set_sp(E - 8);                          /* push edx; call */
    bike_track_evaluate(E + 0x10, time, track);
    rx_set_sp(E);
    const float w = scalar_track_evaluate(track + 0x90, time);
    const float length = scalar_track_evaluate(track + 0xc0, time);
    const rx_v4 v = v4_mul(v4_splat(length), v4_ld(E + 0x10));
    const rx_v4 q = v4(v.v[0], v.v[1], v.v[2], w);
    v4_st(track + 0xf0, bike_normalize_masked(q, quat_len2(q)));
    v4_st(out, v4_ld(track + 0xf0));
    rx_set_sp(E - 4);
    phys_security_check_cookie(U32(E, 0x2c) ^ E);
    rx_set_sp(entry);
}
RX_FUNCTION(0x0042ec50, bike_orient_track_evaluate)

/* ---- the record ---------------------------------------------------------------------------------------------------------------- */

/* the input axes and flags of a record into the simulation (S+1c, S+24 .. S+38 x87 copies; S+48 the flag 0x10000 bit as
 * movsx [+0x46] & 1; S+20 = 1.0 with the reverse flag 0x20000, else 0.0) */
static void apply_record_inputs(rx_addr S, rx_addr record) {
    SU32(S, 0x1c, bike_x87_copy32(U32(record, 0x48)));
    SU32(S, 0x24, bike_x87_copy32(U32(record, 0x4c)));
    SU32(S, 0x28, bike_x87_copy32(U32(record, 0x50)));
    SU32(S, 0x2c, bike_x87_copy32(U32(record, 0x54)));
    SU32(S, 0x30, bike_x87_copy32(U32(record, 0x58)));
    SU32(S, 0x34, bike_x87_copy32(U32(record, 0x5c)));
    SU32(S, 0x38, bike_x87_copy32(U32(record, 0x60)));
    SU32(S, 0x48, (uint32_t)(int32_t)(int16_t)U16(record, 0x46) & 1u);
    SF32(S, 0x20, (U32(record, 0x44) & 0x20000u) != 0 ? 1.0f : 0.0f);
}

/* 00431cf0 rx_net_pose_copy (ECX = source, stack destination; RET 4) at the caller's ESP */
static void copy_record(rx_addr esp, rx_addr from, rx_addr to) {
    rx_set_sp(esp - 8);
    rx_net_pose_copy(from, to);
    rx_set_sp(esp);
}

/* 004331d0: the simulation put on the record (S vf2 (position, orientation), the mechanism's velocity, the root angular velocity,
 * the nodes' slot 7), the record as the last three applied (S+2c50 / 2cc0 / 2d30, their times S+2d00 - 0.3 and S+2d70 - 0.6),
 * its inputs, the ring of 20 records filled with it at times t - 0.3 i, the corrections cleared. Frame: push ebp; and esp, -16;
 * sub esp, 8; push ebx; push esi. */
void bike_remote_jump_to_record(rx_addr record, rx_addr S) {
    const rx_addr entry = rx_sp();
    const rx_addr B = aligned_frame(entry, 8) - 8;
    rx_set_sp(B);
    {
        const uint32_t args[2] = {record, record + 0x10};
        rx_vcall(S, 8, 2, args);                                                    /* vf2: the pose */
    }
    rx_set_sp(B - 4);
    phys_mech_set_velocity(S + 0xd0, record + 0x20);                                /* 0085c4b0 (EDX = M, ESI = the velocity) */
    rx_set_sp(B);
    v4_st(S + 0x740, v4_ld(record + 0x30));
    rx_set_sp(B - 8);                                                               /* push edx (M); call */
    phys_mech_nodes_slot7(S + 0xd0);                                                /* 0085d330 */
    rx_set_sp(B);
    copy_record(B, record, S + 0x2c50);
    copy_record(B, record, S + 0x2cc0);
    copy_record(B, record, S + 0x2d30);
    SF32(S, 0x2d00, phys_sub(F32(S, 0x2d00), 0.3f));                                /* 0x931da4 */
    SF32(S, 0x2d70, phys_sub(F32(S, 0x2d70), 0.6f));                                /* 0x931da8 */
    apply_record_inputs(S, record);
    for (int32_t i = 0; i < 20; i++) {
        const rx_addr key = S + 0x2390 + 0x70u * (uint32_t)i;
        v4_st(key + 0x00, v4_ld(record + 0x00));
        v4_st(key + 0x10, v4_ld(record + 0x10));
        v4_st(key + 0x20, v4_ld(record + 0x20));
        v4_st(key + 0x30, v4_ld(record + 0x30));
        SU32(key, 0x40, bike_x87_copy32(U32(record, 0x40)));
        SU32(key, 0x44, U32(record, 0x44));
        for (uint32_t off = 0x48; off <= 0x60; off += 4) SU32(key, off, bike_x87_copy32(U32(record, off)));
        SF32(key, 0x40, phys_sub(F32(record, 0x40), phys_mul((float)i, 0.3f)));    /* cvtsi2ss i; mulss 0.3 */
    }
    v4_st(S + 0x2da0, v4_ld(G_ZERO));
    v4_st(S + 0x2db0, v4_ld(G_ZERO));
    v4_st(S + 0x2dc0, v4_ld(G_IDENTITY));
    v4_st(S + 0x2dd0, v4_ld(G_ZERO));
    rx_set_sp(entry);
}
RX_FUNCTION(0x004331d0, bike_remote_jump_to_record)

/* One of the record's vectors (+0x00 position / +0x20 velocity / +0x30 angular velocity) through the three ring records around the
 * simulation's time (keys `key` + 0xe0, + 0x70, + 0: oldest first) into the vector track T at B + 0x30, evaluated at S+2c90 into
 * B + 0x20. The track's times and keys are set to their reset values first (the first time by 0042e660, then inline). */
static void fit_vector(rx_addr B, rx_addr S, rx_addr key, uint32_t field) {
    const rx_addr T = B + 0x30;
    SF32(T, 0x30, 0.0f);
    SF32(T, 0x34, F32(B, 0x18));
    v4_st(T + 0x00, v4_ld(G_ZERO));
    v4_st(T + 0x10, v4_ld(G_ZERO));
    v4_st(T + 0x20, v4_ld(G_ZERO));
    SF32(T, 0x38, F32(B, 0x14));
    v4_st(T + 0x80, v4_ld(G_ZERO));
    rx_set_sp(B - 8);                                                               /* push edx (T); call */
    bike_track_add_key(key + 0xe0 + field, F32(key, 0xe0 + 0x40), T);
    bike_track_add_key(key + 0x70 + field, F32(key, 0x70 + 0x40), T);
    bike_track_add_key(key + 0x00 + field, F32(key, 0x00 + 0x40), T);
    SF32(T, 0x3c, 1.0f);
    bike_track_evaluate(B + 0x20, F32(S, 0x2c90), T);
    rx_set_sp(B);
}

/* 00432be0: a received record (ECX) for the remote simulation S. The first one jumps there (004331d0, S+237c = 1). A record not
 * newer than S+2c90 is dropped. Otherwise it becomes the newest applied (S+2c50, the older two shift), its inputs are taken, and
 * S+2380 / S+2384 = the record is newer than the newest ring record / older than the oldest. The ring index k: 0x11 when older, 0
 * when newer, else the first i < 18 with ring[i].t >= t > ring[i+1].t (t = S+2c90), else -1. Through ring[k + 2], ring[k + 1],
 * ring[k] the position, the orientation (the orientation track on the stack), the velocity and the angular velocity are fitted
 * and evaluated at t; the corrections are the record's values minus them: S+2da0 = S+2c50 - position, S+2dc0 = conj(fitted q) *
 * S+2c60, S+2db0 = S+2c70 - velocity, S+2dd0 = S+2c80 - angular velocity. A position correction over 20 (|S+2da0| > 20) jumps.
 * Frame: push ebp; and esp, -16; sub esp, 0x1d4 (the cookie at +0x1d0); push ebx / esi / edi: B + 0x1c the record, B + 0x14 / +0x18
 * -0.6 / -0.3, B + 0x20 the evaluated vector, B + 0x30 the vector track, B + 0xc0 the fitted quaternion, B + 0xd0 the orientation
 * track. */
void bike_remote_apply_record(rx_addr record, rx_addr S) {
    const rx_addr entry = rx_sp();
    const rx_addr A = aligned_frame(entry, 0x1d4);
    const rx_addr B = A - 12;
    rx_set_sp(B);
    SU32(A, 0x1d0, U32(0, 0x009b45e4) ^ A);
    SU32(B, 0x1c, record);

    if (U32(S, 0x237c) == 0) {
        rx_set_sp(B - 8);                                                           /* push ebx (S); call */
        bike_remote_jump_to_record(record, S);
        rx_set_sp(B);
        SU32(S, 0x237c, 1);
    } else if (!(F32(S, 0x2c90) >= F32(record, 0x40))) {                           /* comiss S+2c90, t; jae: dropped */
        copy_record(B, S + 0x2cc0, S + 0x2d30);
        copy_record(B, S + 0x2c50, S + 0x2cc0);
        copy_record(B, record, S + 0x2c50);
        apply_record_inputs(S, record);
        const float t_record = F32(record, 0x40);
        const uint32_t newer = t_record > F32(S, 0x23d0);
        SU32(S, 0x2380, newer);
        const uint32_t older = F32(S, 0x2b40) > t_record;
        SU32(S, 0x2384, older);
        int32_t k = -1;
        if (older) {
            k = 0x11;
        } else if (newer) {
            k = 0;
        } else {
            const float t = F32(S, 0x2c90);
            for (int32_t i = 0; i < 0x12; i++) {
                const rx_addr ring_time = S + 0x23d0 + 0x70u * (uint32_t)i;
                if (F32(ring_time, 0) < t || F32(ring_time, 0) != F32(ring_time, 0) || t != t) continue;   /* comiss; jb */
                if (t > F32(ring_time, 0x70)) { k = i; break; }                                          /* comiss; ja */
            }
        }

        /* 00432d4c: the orientation track and the vector track */
        rx_set_sp(B - 4);
        bike_orient_track_init(B + 0xd0);
        rx_set_sp(B - 8);
        bike_track_reset(B + 0x30);
        rx_set_sp(B);
        const rx_addr key = S + 0x2390 + (uint32_t)k * 0x70u;                      /* ring[k] (k = -1 reads below the ring) */
        SF32(B, 0x18, phys_sub(0.0f, 0.3f));                                        /* xorps; subss 0x931da4 */
        SF32(B, 0x14, phys_sub(0.0f, 0.6f));                                        /* 0x931da8 */

        /* the position */
        fit_vector(B, S, key, 0x00);
        v4_st(S + 0x2da0, v4_sub(v4_ld(S + 0x2c50), v4_ld(B + 0x20)));

        /* 00432e2e: the orientation */
        rx_set_sp(B - 4);
        bike_orient_track_reset(B + 0xd0);
        for (uint32_t back = 0x70u * 3u; back > 0; back -= 0x70u) {                 /* ring[k + 2], ring[k + 1], ring[k] */
            const rx_addr ring = key + back - 0x70u;
            const float time = bike_x87_copyf(F32(ring, 0x40));                    /* fld / fstp to the argument */
            rx_set_sp(B - 12);
            bike_orient_track_add_key(B + 0xd0, ring + 0x10, time);
        }
        SF32(B, 0x10c, 1.0f);
        SF32(B, 0x178, 1.0f);
        SF32(B, 0x1a8, 1.0f);
        rx_set_sp(B - 4);
        bike_orient_track_evaluate(B + 0xd0, F32(S, 0x2c90), B + 0xc0);
        rx_set_sp(B);
        {
            const rx_v4 conj = v4_mul(v4_ld(B + 0xc0), v4(-1.0f, -1.0f, -1.0f, 1.0f));   /* 0x93a220 */
            v4_st(S + 0x2dc0, bike_quat_mul(conj, v4_ld(S + 0x2c60)));
        }

        /* the velocity and the angular velocity */
        fit_vector(B, S, key, 0x20);
        v4_st(S + 0x2db0, v4_sub(v4_ld(S + 0x2c70), v4_ld(B + 0x20)));
        fit_vector(B, S, key, 0x30);
        v4_st(S + 0x2dd0, v4_sub(v4_ld(S + 0x2c80), v4_ld(B + 0x20)));

        /* 004330b0: a position correction over 20 jumps to the record */
        {
            const rx_v4 d = v4_ld(S + 0x2da0);
            const float length = phys_sqrt(bike_hsum_021(v4_mul(d, d)));
            const rx_v4 far = v4_cmplt(v4_splat(20.0f /* 0x935aac */), v4_splat(length));
            SU32(B, 0x20, v4_lb(far, 0)); SU32(B, 0x24, v4_lb(far, 1));
            SU32(B, 0x28, v4_lb(far, 2)); SU32(B, 0x2c, v4_lb(far, 3));
            if (bike_all3(far)) {
                rx_set_sp(B - 8);
                bike_remote_jump_to_record(U32(B, 0x1c), S);
                rx_set_sp(B);
            }
        }
    }
    rx_set_sp(B + 8);
    phys_security_check_cookie(U32(A, 0x1d0) ^ A);
    rx_set_sp(entry);
}
RX_FUNCTION(0x00432be0, bike_remote_apply_record)
