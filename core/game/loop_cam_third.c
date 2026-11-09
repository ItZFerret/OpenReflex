/* loop_cam_third.c -- MXThirdPersonCamera (vtable 0095a8a4), the chase camera of the race: the per-frame update vf19 and the
 * pieces it is built from (orbit pose around the target, ground / water clearance, the free-look shake).
 *
 * Layout facts used here (camera object `cam`): +0x344 the camera controller, whose +0x1ac is the followed vehicle entity
 * (vf17 slot 0x44 writes its position, 0x94 its orientation, 0xac "is on a rail"); +0x36c a scale object (+0xc a factor);
 * the orbit parameters come in two sets, one per side (index 0 / 1, 4 bytes apart: 0x35c, 0x3b4, 0x3f8, 0x420, 0x42c, 0x438,
 * 0x444, 0x450, 0x468, 0x474); +0x490 is the vector spring {current, target +0x10, velocity +0x20} that lifts the camera out
 * of the ground (rx_cam_vec3_spring). */
#include "crt_atexit.h"
#include "loop_cam.h"
#include "loop_util.h"
#include "world_veh_water.h"

/* 0063e400 (LTCG: EDI = position vec3, in / out): keeps the camera 4 above the ground. The ground probe (007f0050) replaces a copy
 * of the position with the hit; when it hits, the position's height is raised to hit height + 4 if that is higher. The probe
 * object 00d72aac is a function-local static: its first use constructs it (004242f0) and registers its destructor (atexit). */
void rx_cam_ground_clearance(rx_addr pos) {
    RX_FRAME_ENTER(F, 0x30);
    rx_addr probe = F + 0xc, normal = F + 0x20;
    SU32(probe, 0, U32(pos, 0));
    SU32(probe, 4, U32(pos, 4));
    SU32(probe, 8, U32(pos, 8));
    if (!(U8(0, 0x00d72aa8) & 1)) {
        SU32(0, 0x00d72aa8, U32(0, 0x00d72aa8) | 1);
        { const rx_addr sp = rx_sp(); rx_set_sp(sp - 4u); (void)rx_water_heights_init(); rx_set_sp(sp); }   /* 004242f0 */
        RX_ATEXIT(0x00911e90);   /* _atexit */
    }
    if (cam_ground_probe(probe, normal, 0x0063e463) != 0) {
        float lifted = F32(probe, 4) + 4.0f /* 0x9322c0 */;
        if (lifted > F32(pos, 4)) SF32(pos, 4, lifted);
    }
    RX_FRAME_LEAVE(F, 0x30);
}
RX_FUNCTION(0x0063e400, rx_cam_ground_clearance)

/* 00650c60 (LTCG: ECX = camera, EAX = position vec3; stack out vec3, margin; ret 8): the water clearance: four points around the
 * position tested against the world's volumes, the largest rise (+ margin + 1) onto out.y (rx_cam_lift_common, loop_cam_misc.c). */
void rx_cam_lift_above_water(rx_addr cam, rx_addr pos, rx_addr out, float margin) {
    rx_cam_lift_common(cam, pos, out, 0, margin, 0x00650c9a, 0x00650faf);
}
RX_FUNCTION(0x00650c60, rx_cam_lift_above_water)

/* 006502e0 (LTCG: ESI = camera, EAX = view direction vec3; stack side, target vec3, out position, out look-at; ret 0x10): the
 * chase camera's pose around the target. Orbit angles h (pitch) and D (yaw) and distances come from the parameter set of `side`
 * (see the file comment):
 *   position = target - (sin D * cos h, -trig D * cos h)-weighted distance G (x, z) and target.y + E - cos(h + pi/2) * dist (y),
 *   then shifted along the right vector (direction x up) by I; the look-at point is target + |H| * direction (+ E in y), turned
 *   around the vertical by phi = +0x4f0 * scale. Unless `side`, the position is lifted out of the water (650c60, margin 3.0) with
 *   the lift as the target of the vector spring +0x490 (whose current value is added), then lifted out of the water again with the
 *   margin 009d789c and kept above the ground (63e400). */
void rx_cam_orbit_pose(rx_addr cam, rx_addr dir, uint32_t side, rx_addr target, rx_addr out_pos, rx_addr out_look) {
    RX_FRAME_ENTER(F, 0x70);
    const rx_addr POS = F + 0x48, PT2 = F + 0x58, DIFF = F + 0x28;
    const uint32_t s = (side & 0xff) != 0;
#define SIDE(off) F32(cam, (off) + 4u * s)
    float A = SIDE(0x35c);
    float h = (F32(cam, 0x5f4) + F32(cam, 0x4e8)) + A;
    if (U8(cam, 0x5ed) != 0) h = h - SIDE(0x450);
    float B = SIDE(0x468);
    float C = SIDE(0x474);
    float scale = F32(U32(cam, 0x36c), 0xc);
    float D = (SIDE(0x3b4) + B) + (F32(cam, 0x4ec) * scale);
    float E = SIDE(0x3f8);
    float G1 = SIDE(0x438), G2 = SIDE(0x444);
    float H = SIDE(0x420), I = SIDE(0x42c);
#undef SIDE
    float ch = cam_cos(h);
    float dist = (F32(cam, 0x4d0) + C) + F32(cam, 0x368);
    float distG = dist + ((F32(cam, 0x550) * G2) + G1);
    float tx = F32(target, 0), ty = F32(target, 4), tz = F32(target, 8);
    float px = tx - ((cam_sin(D) * ch) * distG);
    float py = (ty + E) - (cam_cos(h + 1.5707964f /* 0x977508 */) * dist);
    float pz = tz - ((-0.0f /* 0x9774a8 */ - (cam_trig(D) * ch)) * distG);
    /* the right vector: direction x (0, 1, 0), normalised */
    float dx = F32(dir, 0), dy = F32(dir, 4), dz = F32(dir, 8);
    float dy0 = dy * 0.0f;
    float rx = dy0 - dz;
    float ry = (dz * 0.0f) - (dx * 0.0f);
    float rz = dx - dy0;
    float len2 = ((ry * ry) + (rx * rx)) + (rz * rz);
    if (len2 > 0.0f) {
        float inv = 1.0f /* 0x931f24 */ / rx_sqrts(len2);
        rx = rx * inv;
        ry = ry * inv;
        rz = rz * inv;
    }
    px = (rx * I) + px;
    py = (ry * I) + py;
    pz = (rz * I) + pz;
    SF32(POS, 0, px); SF32(POS, 4, py); SF32(POS, 8, pz);
    /* the look-at point and its offset from the position, turned around the vertical by phi */
    float a = fabsf(H);
    float ex = ((a * dx) + tx) - px;
    float ez = ((a * dz) + tz) - pz;
    float ly = ((a * dy) + ty) + E;
    float phi = F32(cam, 0x4f0) * scale;
    float sphi = cam_sin(phi), tphi = cam_trig(phi);
    SF32(out_look, 0, px + ((-0.0f - (sphi * ez)) + (-0.0f - (tphi * ex))));
    SF32(out_look, 4, ly);
    SF32(out_look, 8, pz + ((-0.0f - (tphi * ez)) + (sphi * ex)));
    if (!s) {
        rx_cam_lift_above_water(cam, POS, PT2, 3.0f /* 0x92fe48 */);
        SF32(DIFF, 0, F32(PT2, 0) - F32(POS, 0));
        SF32(DIFF, 4, F32(PT2, 4) - F32(POS, 4));
        SF32(DIFF, 8, F32(PT2, 8) - F32(POS, 8));
        rx_cam_copy_vec3_10(cam + 0x490, DIFF);
        SF32(POS, 0, F32(cam + 0x490, 0) + F32(POS, 0));
        SF32(POS, 4, F32(cam + 0x490, 4) + F32(POS, 4));
        SF32(POS, 8, F32(cam + 0x490, 8) + F32(POS, 8));
    }
    float clearance = F32(0, 0x009d789c);
    rx_cam_lift_above_water(cam, POS, PT2, clearance);
    if (F32(PT2, 4) - F32(POS, 4) > clearance) SU32(POS, 4, U32(PT2, 4));
    rx_cam_ground_clearance(POS);
    SU32(out_pos, 0, U32(POS, 0));
    SU32(out_pos, 4, U32(POS, 4));
    SU32(out_pos, 8, U32(POS, 8));
    RX_FRAME_LEAVE(F, 0x70);
}
RX_FUNCTION(0x006502e0, rx_cam_orbit_pose)

/* 00650250 (LTCG: EAX = camera, EDI = view direction vec3; stack target vec3; ret 4): primes the chase camera's pose with the
 * look-ahead damping off: the followed vehicle's position is fetched (vf17), +0x478 / +0x46c are zeroed, the pose is computed for
 * side 1, +0x46c is restored at half its value and the pose is computed again. The results (two scratch vectors) are not used. */
void rx_cam_orbit_probe(rx_addr cam, rx_addr dir, rx_addr target) {
    RX_FRAME_ENTER(F, 0x40);
    rx_addr entity = U32(U32(cam, 0x344), 0x1ac);
    loop_tc1(loop_vslot(entity, 0x44), entity, F + 0x28);
    float damping = F32(cam, 0x46c);
    SF32(cam, 0x478, 0.0f);
    SF32(cam, 0x46c, 0.0f);
    rx_cam_orbit_pose(cam, dir, 1, target, F + 0x18, F + 0x8);
    SF32(cam, 0x46c, damping * 0.5f /* 0x9322cc */);
    rx_cam_orbit_pose(cam, dir, 1, target, F + 0x18, F + 0x8);
    RX_FRAME_LEAVE(F, 0x40);
}
RX_FUNCTION(0x00650250, rx_cam_orbit_probe)

/* 0064f240 (LTCG: EAX = camera; stack target vec3, view direction vec3; ret 8): the water-ahead probe. The camera's pose (side 1,
 * all look-ahead damping off) gives the view direction; the camera matrix built from it and its position is inverted (cofactors,
 * 1 / determinant), and a point 4 units further ahead of the target each frame (+0x5cc, reset past 009c4750) is tested against the
 * water of the world of the active controller (007c5950). The water depth at the probe, taken through the inverse matrix, gives
 * a slope angle; steep water for more than 20 frames raises the flag +0x5ec and pushes the spring +0x450's target.
 * Counters: +0x5e0 frames, +0x5e4 summed angle, +0x5e8 summed depth, probe point +0x5d0..0x5d8. */

/* the inline 4x4 inverse of the camera matrix rows (right, up, forward, position) built from forward f and position p: the cofactor
 * matrix goes to MAT, its transpose to INV, whose 12 entries (not the last column) are scaled by 1 / determinant. */
static void cam_view_matrix_inverse(rx_addr MAT, rx_addr INV, float fx, float fy, float fz, float px, float py, float pz) {
    float t18 = fy * 0.0f;
    float t10 = fz * 0.0f;
    float t8 = fz;
    float t9c = (fy * 0.0f) - fx;
    float f14 = (fx * 0.0f) - (fz * 0.0f);
    float v104 = f14 * fz;
    float t84 = fy * ((fy * 0.0f) - fx);
    float f15 = fz - (fy * 0.0f);
    float tfc = (f14 * fx) - (fy * f15);
    float t4c = pz * 0.0f;
    float t14 = t8 - (pz * 0.0f);
    float t78 = tfc * 0.0f;
    float t70 = (tfc * 0.0f) - t10;
    float tc = t14 * ((f15 * fz) - (((fy * 0.0f) - fx) * fx));
    float t74 = tfc - (pz * 0.0f);
    float f16 = tc - (fy * (tfc - (pz * 0.0f)));
    tc = tc - (fy * (tfc - (pz * 0.0f)));
    float t50 = t9c - t4c;
    t8 = t9c * 0.0f;
    t10 = (t9c * 0.0f) - t10;
    tc = t14 * f14;
    float t2c = f16 + (py * t70);
    float f18 = tc - (fy * t50);
    tc = tc - (fy * t50);
    tc = t8 - t78;
    float t1c = f18 + (py * t10);
    t8 = (t74 * f14) - (t50 * ((f15 * fz) - (((fy * 0.0f) - fx) * fx)));
    float t20 = t8 + (py * tc);
    t8 = (t70 * f14) - (t10 * ((f15 * fz) - (((fy * 0.0f) - fx) * fx)));
    t78 = t8 + (fy * tc);
    float inv_det = 1.0f / ((((t2c * f15) - (t1c * ((fy * ((fy * 0.0f) - fx)) - (f14 * fz)))) + (t20 * fx)) - (t78 * px));
    float m0 = t2c;
    float m1 = -0.0f - (((t14 * ((fy * ((fy * 0.0f) - fx)) - (f14 * fz))) - (t74 * fx)) + (t70 * px));
    float t28 = py * 0.0f;
    t2c = fy - (py * 0.0f);
    float f19 = (f15 * fz) - (((fy * 0.0f) - fx) * fx);
    float t7c = f19 * 0.0f;
    t4c = (f19 * 0.0f) - t18;
    float ta8 = f19 - (py * 0.0f);
    float m2 = ((t2c * ((fy * ((fy * 0.0f) - fx)) - (f14 * fz))) - ((f19 - (py * 0.0f)) * fx)) + (t4c * px);
    t8 = fy * pz;
    float f20 = t8 - (py * fz);
    t8 = pz * f19;
    float tb0 = f20;
    float tac = t8 - (py * tfc);
    t8 = f19 * fz;
    t8 = t8 - (fy * tfc);
    float m4 = -0.0f - t1c;
    float m3 = -0.0f - (((tb0 * ((fy * ((fy * 0.0f) - fx)) - (f14 * fz))) - (tac * fx)) + (t8 * px));
    float m5 = ((t14 * f15) - (t50 * fx)) + (t10 * px);
    t14 = f14 * 0.0f;
    t18 = (f14 * 0.0f) - t18;
    float t30 = f14 - t28;
    float f24 = -0.0f - (((t2c * f15) - ((f14 - t28) * fx)) + (t18 * px));
    t28 = pz * f14;
    float m6 = f24;
    t1c = v104 - t84;
    float f25 = t28 - (py * t9c);
    t28 = t28 - (py * t9c);
    t84 = (tb0 * f15) - (f25 * fx);
    float m7 = ((tb0 * f15) - (f25 * fx)) + (px * t1c);
    float m8 = t20;
    t20 = t14 - t7c;
    float m9 = -0.0f - (((t74 * f15) - (t50 * ((fy * ((fy * 0.0f) - fx)) - (f14 * fz)))) + (tc * px));
    float f26 = (ta8 * f15) - (t30 * ((fy * ((fy * 0.0f) - fx)) - (f14 * fz)));
    t30 = (ta8 * f15) - (t30 * ((fy * ((fy * 0.0f) - fx)) - (f14 * fz)));
    float m10 = f26 + (px * t20);
    float f28 = (fy * ((fy * 0.0f) - fx)) - (f14 * fz);
    float m11 = -0.0f - (((tac * f15) - (f28 * t28)) + (px * ((tfc * f14) - (f19 * t9c))));
    float m12 = -0.0f - t78;
    float m13 = ((t70 * f15) - (t10 * f28)) + (tc * fx);
    float m14 = -0.0f - (((t4c * f15) - (t18 * f28)) + (t20 * fx));
    float m15 = ((t8 * f15) - (f28 * t1c)) + (((tfc * f14) - (f19 * t9c)) * fx);
    const float m[16] = {m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15};
    for (int i = 0; i < 16; i++) SF32(MAT, 4 * i, m[i]);
    cam_matrix_transpose(MAT, INV, 0x0064faa6);
    static const uint32_t scaled[12] = {0x00, 0x04, 0x08, 0x10, 0x14, 0x18, 0x20, 0x24, 0x28, 0x30, 0x34, 0x38};
    for (int i = 0; i < 12; i++) SF32(INV, scaled[i], F32(INV, scaled[i]) * inv_det);
}

/* 0064f240 rest: see above. */
void rx_cam_water_ahead(rx_addr cam, rx_addr target, rx_addr dir) {
    RX_FRAME_ENTER(F, 0x158);
    const rx_addr POS = F + 0x34, LOOK = F + 0x94, MAT = F + 0x114, INV = F + 0xb4;
    const rx_addr SPRING = cam + 0x450;
    /* the controller of the active mode's player table */
    rx_addr ctx = U32(0, 0x00a95844), owner = 0;
    if (U32(U32(ctx, 0x20), 0x34) == 1) owner = U32(U32(0, 0x00a946b8), 0x5a54);
    else if (U32(U32(ctx, 0x1c), 0x34) == 1) owner = U32(U32(0, 0x00a946b8), 0x20c8);
    uint32_t controller = U32(owner, 0x20138);
    uint32_t saved0 = U32(cam, 0x450), saved1 = U32(cam, 0x454), saved2 = U32(cam, 0x458);
    SF32(SPRING, 0, 0.0f);
    SF32(SPRING, 4, 0.0f);
    SF32(SPRING, 8, 0.0f);
    rx_cam_orbit_pose(cam, dir, 1, target, POS, LOOK);
    float py = F32(POS, 4), pz = F32(POS, 8);
    float dx = F32(LOOK, 0) - F32(POS, 0);
    float dy = F32(LOOK, 4) - py;
    float dz = F32(LOOK, 8) - pz;
    float fx = dx, fy = dy, fz = dz;
    float len2 = ((dy * dy) + (dx * dx)) + (dz * dz);
    if (len2 > 0.0f) {
        float inv = 1.0f / rx_sqrts(len2);
        fx = dx * inv;
        fy = dy * inv;
        fz = dz * inv;
    }
    float px = F32(POS, 0);
    cam_view_matrix_inverse(MAT, INV, fx, fy, fz, px, py, pz);
    SU32(SPRING, 0, saved0);
    SU32(SPRING, 4, saved1);
    SU32(SPRING, 8, saved2);
    /* the forward direction on the ground plane */
    float gx = fx, gz = fz;
    float glen2 = (fx * fx) + (fz * fz);
    if (glen2 != 0.0f) {
        float inv = 1.0f / rx_sqrts(glen2);
        gx = inv * fx;
        gz = inv * fz;
    }
    SU32(POS, 0, U32(target, 0));
    SU32(POS, 4, U32(target, 4));
    SU32(POS, 8, U32(target, 8));
    /* the world of the controller: the first registry entry with its id (+0x44a8) */
    uint32_t count;
    if (!(U8(0, 0x009e30e0) & 1)) {
        SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1);
        SU32(0, 0x009e30dc, 0);
        count = 0;
    } else {
        count = U32(0, 0x009e30dc);
    }
    rx_addr world = 0;
    for (uint32_t k = 0; k < count; k++) {
        rx_addr e = U32(0, 0x009e30d4 + 4 * k);
        if (e != 0 && controller == U32(e, 0x44a8)) { world = e; break; }
    }
    for (uint32_t pass = 0; pass < 2; pass++) {
        if (F32(cam, 0x5cc) == 0.0f) {
            cam_water_contains(world, POS, 0x0064fcef);
            SU32(cam, 0x5d0, U32(POS, 0));
            SU32(cam, 0x5d4, U32(POS, 4));
            SU32(cam, 0x5d8, U32(POS, 8));
        }
        float ahead = F32(cam, 0x5cc) + 4.0f /* 0x9322c0 */;
        SF32(cam, 0x5cc, ahead);
        int out_of_range = ahead > F32(0, 0x009c4750);
        float prev_y = 0.0f;
        if (!out_of_range) {
            prev_y = F32(cam, 0x5d4);
            float x = (ahead * gx) + F32(target, 0);
            SF32(cam, 0x5d0, x);
            SF32(cam, 0x5d4, prev_y + 8.0f /* 0x9322b8 */);
            SF32(cam, 0x5d8, (ahead * gz) + F32(target, 8));
            out_of_range = F32(world, 0x44d8) > x || x > F32(world, 0x44e0) || F32(world, 0x44dc) > F32(cam, 0x5d8) ||
                           F32(cam, 0x5d8) > F32(world, 0x44e4);
        }
        if (out_of_range) {   /* the probe left the world: start over */
            if (U8(cam, 0x5ec) == 0) SF32(cam, 0x454, 0.0f);
            SF32(cam, 0x5cc, 0.0f);
            SF32(cam, 0x5e4, 0.0f);
            SF32(cam, 0x5e8, 0.0f);
            SU32(cam, 0x5e0, 0);
            SU8(cam, 0x5ec, 0);
            break;
        }
        cam_water_contains(world, cam + 0x5d0, 0x0064fde3);
        float depth_step = (F32(cam, 0x5d4) - prev_y) / (U8(cam, 0x5ec) ? 4.0f /* 0x9322c0 */ : 2.0f /* 0x92e330 */);
        if (!(depth_step > F32(0, 0x009cd9a8))) {
            SF32(cam, 0x5e8, 0.0f);
        } else {
            float depth = F32(cam, 0x5e8) + depth_step;
            SF32(cam, 0x5e8, depth);
            if (depth > 10.0f /* 0x92fd88 */) {   /* far too deep: reset the probe */
                SF32(cam, 0x5cc, 0.0f);
                SF32(cam, 0x5e4, 0.0f);
                SF32(cam, 0x5e8, 0.0f);
                SU32(cam, 0x5e0, 0);
                SU8(cam, 0x5ec, 0);
                break;
            }
        }
        /* the probe point through the inverse matrix, normalised */
        float qy = F32(cam, 0x5d4) + F32(cam, 0x558);
        float qx = F32(cam, 0x5d0) + 0.0f;
        float qz = F32(cam, 0x5d8) + 0.0f;
        float a1 = (((qy * F32(INV, 0x14)) + (F32(INV, 0x04) * qx)) + (qz * F32(INV, 0x24))) + F32(INV, 0x34);
        float a0 = (((qy * F32(INV, 0x10)) + (qx * F32(INV, 0x00))) + (qz * F32(INV, 0x20))) + F32(INV, 0x30);
        float a2 = (((qy * F32(INV, 0x18)) + (F32(INV, 0x08) * qx)) + (qz * F32(INV, 0x28))) + F32(INV, 0x38);
        float alen2 = ((a1 * a1) + (a0 * a0)) + (a2 * a2);
        float ny = a1;
        if (alen2 > 0.0f) ny = (1.0f / rx_sqrts(alen2)) * a1;
        /* arcsin(ny) as a rational approximation in doubles rounded to floats at each step (|ny| >= 009ddfc8: through the cosine) */
        float ay = fabsf(ny);
        float r;
        if (ay >= F32(0, 0x009ddfc8)) {
            double d = (double)ny;
            float one_minus = (float)(1.0 /* 0x977638 */ - (double)(float)(d * d));
            r = rx_sqrts(fabsf(one_minus));
        } else {
            r = ay;
        }
        double rd = (double)r;
        float r2 = (float)(rd * rd);
        double R2 = (double)r2;
        float r4 = (float)(R2 * R2);
        double R4 = (double)r4;
        double A1 = (double)(float)(0.5648093223571777 /* 0x9777b0 */ - (R2 * 0.04992850869894028 /* 0x9777a8 */));
        double den = (A1 * R4) + (double)(float)(1.0 - (R2 * 1.4430354833602905 /* 0x9777a0 */));
        float q = (float)(1.0 / den);
        double num = ((double)(float)(0.3993034064769745 /* 0x977798 */ - (R2 * 0.0193784236907959 /* 0x977790 */)) * R4) +
                     (double)(float)(1.0 - (R2 * 1.2763687372207642 /* 0x977788 */));
        float t = (float)((double)q * (double)(float)(num * rd));
        if (ay >= F32(0, 0x009ddfc8)) t = (float)(1.5707963705062866 /* 0x977510 */ - (double)t);
        float angle = (ny >= 0.0f) ? t : (-0.0f - t);
        if (angle > F32(cam, 0x554)) {
            SU32(cam, 0x5e0, U32(cam, 0x5e0) + 1);
            uint32_t frames = U32(cam, 0x5e0);
            SF32(cam, 0x5e4, angle + F32(cam, 0x5e4));
            if (frames > 0x14) {
                float push = (angle - F32(cam, 0x554)) * 1.6f /* 0x9774bc */;
                SU8(cam, 0x5ec, 1);
                if (push > F32(SPRING, 4)) SF32(SPRING, 4, push);
            }
        } else {
            SU32(cam, 0x5e0, 0);
            SF32(cam, 0x5e4, 0.0f);
        }
    }
    RX_FRAME_LEAVE(F, 0x158);
}
RX_FUNCTION(0x0064f240, rx_cam_water_ahead)
