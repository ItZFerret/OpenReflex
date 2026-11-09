/* bike_post.c -- the slots after the pose integration: 16 (wheel node orientations), 17 (wheel spin relative to the chassis,
 * a damped pitch spring), 32 (body-probe vectors). native/game/notes/bike.md. */
#include "bike_post.h"

#define G_AXIS_UP     0x00d616a0

/* 0043ed40 (stack S, dt; ret 8): a damped scalar spring S+8d90 (angle) / S+8d94 (rate), driven by the root-local y of S+160:
 *   rate = ((angle * -75 + g_up * 0) + (-0.0 - local.y)) * dt + rate) * 0.96;  angle = rate * dt + angle
 * (g_up = the bike's up axis . the mechanism gravity S+d0; multiplied by 0 in the original). */
void bike_pitch_spring(rx_addr S, float dt) {
    const rx_v4 q = rx_ld4(S + 0x730);
    const rx_v4 up = bike_rotate(q, rx_ld4(G_AXIS_UP));
    const rx_v4 local = bike_rotate_inv(q, rx_ld4(S + 0x160));
    const float g_up = phys_mul(bike_dot3(up, rx_ld4(S + 0xd0)), 0.0f /* 0092e12c */);
    const float angle = F32(S, 0x8d90);
    float drive = phys_add(phys_add(phys_mul(angle, -75.0f /* 00977890 */), g_up), phys_sub(-0.0f, local.v[1]));
    float rate = phys_mul(phys_add(phys_mul(drive, dt), F32(S, 0x8d94)), 0.9599999785423279f /* 0097788c */);
    SF32(S, 0x8d94, rate);
    SF32(S, 0x8d90, phys_add(phys_mul(rate, dt), angle));
}
RX_FUNCTION(0x0043ed40, bike_pitch_spring)

/* 0043c1a0 RavenSimulationMotocross::vf17 (ECX = S, stack dt; ret 4): the pitch spring, then each wheel's spin relative to
 * the chassis spin: front S+8a30 = (S+880 - chassis spin in the ROOT frame).x (the original rotates back with the root
 * orientation, not the wheel's), rear S+8a34 = (S+920 - chassis spin in the rear wheel's frame).x. */
void bike_slot17(rx_addr S, float dt) {
    bike_pitch_spring(S, dt);
    const rx_v4 q = rx_ld4(S + 0x730);
    const rx_v4 spin = bike_rotate(q, rx_ld4(S + 0x740));
    SF32(S, 0x8a30, v4_sub(rx_ld4(S + 0x880), bike_rotate_inv(q, spin)).v[0]);
    SF32(S, 0x8a34, v4_sub(rx_ld4(S + 0x920), bike_rotate_inv(rx_ld4(S + 0x910), spin)).v[0]);
}
RX_FUNCTION(0x0043c1a0, bike_slot17)

/* 0043e200 RavenSimulationMotocross::vf16 (ECX = S; ret 0): after the pose integration.
 *  - front fork: S+6eb0 = S+8a70 * steering quaternion C+50; S+8a80 = root orientation * S+6eb0;
 *  - rear swing arm: the rear wheel's offset from its rest position S+41d0 - S+3770 in the root frame (x zeroed); pointing
 *    up when it folds over (z < -0.7) -> a fixed direction; scaled to the arm length S+4224 -> the anchor S+77d0 (y from S+4054);
 *    the arm frame (rows: rear x forward-cross, X-axis cross, -direction) -> quaternion S+8a90 (root-relative, then world). */
void bike_slot16(rx_addr S) {
    const rx_v4 q = rx_ld4(S + 0x730), G = rx_ld4(0x00d61670), zero = v4_splat(0.0f);
    const rx_v4 rest = v4_sub(rx_ld4(S + 0x41d0), rx_ld4(S + 0x3770));
    rx_st4(S + 0x6eb0, bike_quat_mul(rx_ld4(S + 0x8a70), rx_ld4(U32(S, 0x5c30) + 0x50)));
    rx_st4(S + 0x8a80, bike_quat_mul(q, rx_ld4(S + 0x6eb0)));
    rx_v4 d = v4_sub(bike_rotate_inv(q, v4_sub(rx_ld4(S + 0x2d0), rx_ld4(S + 0xf0))), rest);
    d.v[0] = G.v[0];
    const rx_v4 n = bike_normalize(d);
    rx_v4 dir = d;
    if (bike_all3(v4_cmplt(v4_splat(-0.699999988079071f /* 0097781c */), v4_lane(n, 2)))) {
        const float m1 = F32(0, 0x009cd9e8);
        if (bike_all3(v4_cmplt(G, v4_lane(n, 1)))) dir = v4(0.0f, 0.0f, m1, m1);
        else dir = v4(0.0f, -0.0f, m1, m1);
    }
    const rx_v4 u = v4_mul(bike_normalize(dir), v4_splat(F32(S, 0x4224)));
    const rx_v4 p = v4_add(u, rest);
    rx_st4(S + 0x77d0, v4(p.v[0], phys_sub(F32(S, 0x4054), F32(S, 0x3770 + 4)), p.v[2], p.v[3]));
    const rx_v4 row2 = v4_sub(zero, bike_normalize(u));
    const rx_v4 row1 = bike_normalize(bike_cross(row2, rx_ld4(0x00d615c0)));
    float row0[3];
    bike_vec3_cross(row0, row1.v, row2.v);                       /* 00401630: row1 x row2 */
    const float l2 = phys_add(phys_add(phys_mul(row0[1], row0[1]), phys_mul(row0[0], row0[0])), phys_mul(row0[2], row0[2]));
    if (l2 > 0.0f) {
        const float inv = phys_div(1.0f, phys_sqrt(l2));               /* SqrtF 0042c230 */
        row0[0] = phys_mul(inv, row0[0]); row0[1] = phys_mul(inv, row0[1]); row0[2] = phys_mul(inv, row0[2]);
    }
    const float m[3][3] = {{row0[0], row0[1], row0[2]}, {row1.v[0], row1.v[1], row1.v[2]}, {row2.v[0], row2.v[1], row2.v[2]}};
    const rx_v4 qa = bike_matrix_to_quat(m);
    rx_st4(S + 0x8a90, qa);
    rx_st4(S + 0x8a90, bike_quat_mul(q, qa));
}
RX_FUNCTION(0x0043e200, bike_slot16)

/* 0044ef70 RavenSimulationMotocross::vf32 (ECX = S; ret 0): the twelve root-local body-probe vectors S+14b0..1560 used by
 * the body contacts (0x00435b00) and the contact response: eight fixed points of the bike's shape, two pairs at the rider
 * body (S+230) and the rear wheel (S+2d0) with fixed offsets; on the ground the rider / seat ones rise with the speed (up to
 * 1.5 at 10); then the copy S+1f40.. and, per probe k < S+1318, S+1c20 + 32k = probe - copy, S+1c30 + 32k = probe. */
void bike_slot32(rx_addr S) {
    const rx_v4 zero = v4_splat(0.0f);
    const float m1 = F32(0, 0x009cd9e8), v960 = F32(0, 0x009cd960);
    rx_st4(S + 0x14b0, v4(-1.5f /* 00977850 */, 2.0f, 0.75f, 0.75f));
    rx_st4(S + 0x14d0, v4(1.5f, 2.0f, 0.75f, 0.75f));
    rx_st4(S + 0x14e0, v4(0.5f, 1.2999999523162842f /* 009773f0 */, -3.5f /* 009778d8 */, -3.5f));
    rx_st4(S + 0x14f0, v4(0.0f, 1.0f, 3.0f, 3.0f));
    rx_st4(S + 0x1500, v4(0.0f, -0.800000011920929f /* 009776e4 */, 0.5f, 0.5f));
    rx_st4(S + 0x1510, v4(0.0f, -0.800000011920929f, m1, m1));
    rx_st4(S + 0x1520, v4(0.0f, 0.4000000059604645f, -3.0999999046325684f /* 009778d4 */, -3.0999999046325684f));
    rx_st4(S + 0x14c0, v4(-0.5f /* 009774b0 */, 1.2999999523162842f, -3.5f, -3.5f));
    const rx_v4 off_a = v4(v960, -0.6000000238418579f /* 009774dc */, 0.0f, 0.0f);
    const rx_v4 off_b = v4(-0.44999998807907104f /* 009778d0 */, -0.6000000238418579f, 0.0f, 0.0f);
    const rx_v4 q = rx_ld4(S + 0x730), pos = rx_ld4(S + 0xf0);
    rx_st4(S + 0x1530, v4_add(bike_rotate_inv(q, v4_sub(rx_ld4(S + 0x230), pos)), off_a));
    rx_st4(S + 0x1540, v4_add(bike_rotate_inv(q, v4_sub(rx_ld4(S + 0x230), pos)), off_b));
    rx_st4(S + 0x1550, v4_add(bike_rotate_inv(q, v4_sub(rx_ld4(S + 0x2d0), pos)), off_a));
    rx_st4(S + 0x1560, v4_add(bike_rotate_inv(q, v4_sub(rx_ld4(S + 0x2d0), pos)), off_b));
    if (U32(S, 0x2374) == 0 && U32(S, 0x8db8) == 0) {
        const rx_v4 up = v4_movss(v4_shuf(v4_splat(1.5f), zero, 0xee), zero);       /* (0, 1.5, 0, 0) */
        const rx_v4 v = rx_ld4(S + 0x100);
        const rx_v4 sp = v4_sqrt(v4_splat(bike_hsum_021(v4_mul(v, v))));
        const rx_v4 ten = v4_splat(10.0f /* 0092fd88 */);
        const rx_v4 off = bike_all3(v4_cmplt(ten, sp)) ? up : v4_div(v4_mul(sp, up), ten);
        static const uint16_t raised[6] = {0x1500, 0x1510, 0x1530, 0x1540, 0x1550, 0x1560};
        for (int i = 0; i < 6; i++) rx_st4(S + raised[i], v4_add(off, rx_ld4(S + raised[i])));
    }
    for (int i = 0; i < 12; i++) rx_st4(S + 0x1f40 + 16u * (uint32_t)i, rx_ld4(S + 0x14b0 + 16u * (uint32_t)i));
    for (int32_t k = 0; k < I32(S, 0x1318); k++) {
        rx_addr p = S + 0x14b0 + 16u * (uint32_t)k, d = S + 0x1c30 + 32u * (uint32_t)k;
        rx_st4(d - 0x10, v4_sub(rx_ld4(p), rx_ld4(p + 0xa90)));
        rx_st4(d, rx_ld4(p));
    }
}
RX_FUNCTION(0x0044ef70, bike_slot32)
