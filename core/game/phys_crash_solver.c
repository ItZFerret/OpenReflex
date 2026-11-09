/* phys_crash_solver.c -- 00426d00 (stdcall A, dt; RET 8): the rider ragdoll against the bike.
 *
 * The crash path's bike contact: while the ragdoll is being thrown, the rider's bone rotations are slaved to the bike's
 * animation node transforms. Called by 00425640 (phys_rider_crash_step) between the ground contacts and the bone contacts.
 * Frame of the original: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x4d4; push ebx / esi / edi, so the cookie is at
 * L+0x4dc with L = esp after the three pushes, and the locals live at L+0x00 .. L+0x4d8.
 *
 * The original's frame carries:
 *   L+0x370  the per-bone quaternions (vec4, bone b at 0x370 + 16 b; bone 0 is the root body's orientation)
 *   L+0x240  the bike node positions (vec3, stride 0x10, bone 1 + k at 0x240 + 16 k)
 *   L+0x230  the root reference point of 004225d0 (vec3)
 *   L+0x4b0  (reused) the root quaternion copy, later the second angular velocity vector
 *   L+0x4c0  the up-axis splat
 *   L+0x220  the first angular velocity vector
 *   L+0x200  the 0.9 angular-velocity damping splat
 * Every other slot is a compiler temporary; they are ordinary C locals here. The two per-bone arrays stay in the frame
 * because the original reads them back through computed indices (and, for a bone a later node maps before its own element
 * is written, reads the caller's stack residue there).
 *
 * All quaternion products below are the original's: each term a double product of the float operands, summed in the
 * original's order and rounded once to float. The rig stores its quaternions (x, y, z, w).
 */
#include "phys_crash_solver.h"
#include "phys_rider.h"
#include "phys_rigbody.h"
#include "phys_rig.h"
#include "phys_vec.h"

/* the original's 00401000 / 00401040 Float_Abs: andps with the absolute-value mask */
static float abs_f(float x) { return rx_bf(rx_fb(x) & 0x7fffffffu); }
/* 0042c230: sqrtss */
static float sqrt_f(float x) { return phys_sqrt(x); }
/* the original's negation of a float: -0.0 - x (subss) */
static float neg_f(float x) { return phys_sub(-0.0f, x); }

#define ARR_Q(b) (L + 0x370u + 0x10u * (uint32_t)(b))   /* the per-bone quaternion */
#define ARR_P(k) (L + 0x240u + 0x10u * (uint32_t)(k))   /* the bike node position */

/* a scene node's base index: the node record's parent word (+8) plus the record's byte +0xf * 16, relative to the node
 * array's base; -1 for the null record (the original's je on the sum) */
static int32_t node_base_index(rx_addr A, uint32_t node) {
    const rx_addr nodes = U32(U32(A, 0x37c), 0x240);
    const rx_addr rec = U32(nodes, 8) + 16u * node;
    const uint32_t i = (uint32_t)U8(rec, 0xf) * 16u + U32(U32(rec, 0), 8);
    if (i == 0) return -1;
    return (int32_t)(i - U32(nodes, 8)) >> 4;
}

/* q * b (Hamilton, the original's association): x = (qw bx + qx bw) + qy bz - qz by, etc. */
static void qmul(float r[4], const float q[4], const float b[4]) {
    r[0] = (float)((((double)q[3] * (double)b[0] + (double)q[0] * (double)b[3]) + (double)q[1] * (double)b[2]) -
                   (double)q[2] * (double)b[1]);
    r[1] = (float)((((double)q[3] * (double)b[1] + (double)q[1] * (double)b[3]) + (double)q[2] * (double)b[0]) -
                   (double)q[0] * (double)b[2]);
    r[2] = (float)((((double)q[3] * (double)b[2] + (double)q[2] * (double)b[3]) + (double)q[0] * (double)b[1]) -
                   (double)q[1] * (double)b[0]);
    r[3] = (float)((((double)q[3] * (double)b[3] - (double)q[0] * (double)b[0]) - (double)q[1] * (double)b[1]) -
                   (double)q[2] * (double)b[2]);
}
/* the original's "rotate back" product (three lanes): conj(q) applied to v, lane order as the code uses it */
static void qapply3(float r[3], const float q[4], const float v[4]) {
    const float nx = neg_f(q[0]), ny = neg_f(q[1]), nz = neg_f(q[2]);
    r[0] = (float)((((double)v[3] * (double)nx + (double)v[0] * (double)q[3]) + (double)v[1] * (double)nz) -
                   (double)v[2] * (double)ny);
    r[1] = (float)((((double)v[3] * (double)ny + (double)v[1] * (double)q[3]) + (double)v[2] * (double)nx) -
                   (double)v[0] * (double)nz);
    r[2] = (float)((((double)v[3] * (double)nz + (double)v[2] * (double)q[3]) + (double)v[0] * (double)ny) -
                   (double)v[1] * (double)nx);
}

/* The packed asin fit (the rational fit at 0x935b70's cousins, as 00420160 uses it): the input is the cosine-like dot;
 * the result is the angle for the corrective torque. The intermediate roundings are the original's cvtpd2ps / cvtsd2ss. */
static float crash_angle(float dot) {
    const float t = abs_f(dot);                                     /* 00401040 Float_Abs_Thunk */
    const int big = (t >= 0.707099974f /* 0x9ddfc8 */);             /* comiss + jb */
    float x;
    if (!big) {
        x = t;
    } else {
        const float d2 = (float)((double)dot * (double)dot);        /* cvtsd2ss of dot^2 */
        const float q = (float)(1.0 - (double)d2);                  /* 0x977638 = 1.0 */
        x = sqrt_f(abs_f(q));
    }
    {
        const float y = (float)((double)x * (double)x);
        const float y2 = (float)((double)y * (double)y);
        const float t1 = (float)(0.5648093223571777 - (double)y * 0.04992850869894028);      /* 0x9777b0 / 0x9777a8 */
        const float t2 = (float)(1.0 - (double)y * 1.4430354833602905);                      /* 0x9777a0 */
        const double den = (double)t1 * (double)y2 + (double)t2;
        const float inv = (float)(1.0 / den);
        const float t3 = (float)(0.3993034064769745 - (double)y * 0.0193784236907959);       /* 0x977798 / 0x977790 */
        const float t4 = (float)(1.0 - (double)y * 1.2763687372207642);                      /* 0x977788 */
        const float num = (float)(((double)t3 * (double)y2 + (double)t4) * (double)x);
        float r = (float)((double)inv * (double)num);
        if (big) r = (float)(1.5707963705062866 - (double)r);                                 /* 0x977510 */
        if (!(dot >= 0.0f)) r = -0.0f - r;                                                   /* comiss + jb */
        return (float)(1.5707963705062866 - (double)r);
    }
}

/* the torque scale: |angle| / (pi/2) * 6, clamped to [0, 6] (the original's two comiss pairs) */
static float crash_torque_scale(float angle) {
    float v = (float)((double)abs_f(angle) / 1.5707963705062866);
    v = (float)((double)v * 6.0);
    if (!(v > 0.0f)) v = 0.0f;
    else if (!(6.0f /* 0x92fe4c */ > v)) v = 6.0f;
    return v;
}

void phys_crash_bike_solver(rx_addr A, float dt) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x4d4 - 12;
    rx_set_sp(L);
    SU32(L, 0x4dc, U32(0, 0x009b45e4) ^ (L + 0xc));
    if (U32(A, 0xa8c0) == 0) goto done;
    if (U8(U32(A, 0x37c), 0x204) == 0) goto done;
    if (!((U32(A, 0x374) == 0 && U8(A, 0x88) == 0) || U32(A, 0x98) != 0)) goto done;
    if (U32(A, 0xa8a0) != 0) goto done;
    {
        const rx_addr P = A + 0x380;
        const int32_t count = I32(A, 0x3fc);
        const rx_addr bike = U32(U32(A, 0x37c), 0x1f8);
        /* the root reference point (004225d0) and the rig's gravity */
        rx_set_sp(L - 4);
        SU32(L - 4, 0, P);
        phys_rigbody_reference_point(L + 0x4b0, P);
        rx_set_sp(L);
        SF32(L, 0x230, F32(L, 0x4b0));
        SF32(L, 0x234, F32(L, 0x4b4));
        SF32(L, 0x238, F32(L, 0x4b8));
        if (U8(0, 0x00d72ad0) == 0) {                               /* 0xd72ad0: the debug gravity override */
            SF32(A, 0x380, 0.0f);                                   /* 0x92e12c */
            SF32(A, 0x384, -32.2f);                                 /* 0x977688 */
            SF32(A, 0x388, 0.0f);
            SF32(A, 0x38c, 0.0f);
        } else {
            v4_st(A + 0x410, v4_ld(0x00d61670));
            v4_st(A + 0xe90, v4_ld(0x00d61670));
            v4_st(P, v4_ld(0x00d61670));
        }
        v4_st(L + 0x4b0, v4_ld(A + 0xe80));                         /* the root orientation (kept for the stale w lane) */
        v4_st(L + 0x370, v4_ld(A + 0xe80));                         /* bone 0 */
        v4_st(L + 0x200, v4_splat(F32(0, 0x009cd970)));             /* 0.9 */

        /* 1. per bike node: compose its rotation onto the rider bone it maps to (the array at L+0x370); the node's
         *    position goes to L+0x240. The already stored bone quaternion is the left factor (root for bone 0). */
        for (int32_t k = 1; k < count; k++) {
            const uint32_t node = (uint32_t)node_base_index(A, U32(A, 0xa544 + 4u * (uint32_t)(k - 1)));
            const int32_t bone = phys_rig_bone_of_node(A, node);
            const rx_addr rec = bike + 0x30 + 0x30u * (uint32_t)(k - 1);
            const rx_addr po = ARR_P(k - 1);
            float q[4], b[4], r[4];
            SF32(po, 0, F32(rec, 0x10));
            SF32(po, 4, F32(rec, 0x14));
            SF32(po, 8, F32(rec, 0x18));
            q[0] = F32(ARR_Q(bone), 0); q[1] = F32(ARR_Q(bone), 4);
            q[2] = F32(ARR_Q(bone), 8); q[3] = F32(ARR_Q(bone), 0xc);
            b[0] = F32(rec, 0); b[1] = F32(rec, 4); b[2] = F32(rec, 8); b[3] = F32(rec, 0xc);
            qmul(r, q, b);
            {
                const rx_addr o = L + 0x380 + 0x10u * (uint32_t)(k - 1);
                SF32(o, 0, r[0]); SF32(o, 4, r[1]); SF32(o, 8, r[2]); SF32(o, 0xc, r[3]);
                {
                    /* the length in double: ((w^2 + x^2) + y^2) + z^2, through the frame as the original reads it */
                    double s = (double)F32(o, 0xc) * (double)F32(o, 0xc);
                    s += (double)F32(o, 0) * (double)F32(o, 0);
                    s += (double)F32(o, 4) * (double)F32(o, 4);
                    s += (double)F32(o, 8) * (double)F32(o, 8);
                    {
                        const float len2 = (float)s;
                        if (len2 == 0.0f) {
                            SF32(o, 0, 0.0f); SF32(o, 4, 0.0f); SF32(o, 8, 0.0f); SF32(o, 0xc, 0.0f);
                        } else {
                            const float inv = 1.0f / sqrt_f(len2);
                            /* the original's stores: the y / z / w lanes are scaled, x keeps its raw value and the
                             * length lands in the next element's first lane */
                            SF32(L + 0x390 + 0x10u * (uint32_t)(k - 1), 0, (float)((double)inv * (double)len2));
                            SF32(o, 4, (float)((double)inv * (double)F32(o, 4)));
                            SF32(o, 8, (float)((double)inv * (double)F32(o, 8)));
                            SF32(o, 0xc, (float)((double)inv * (double)F32(o, 0xc)));
                        }
                    }
                }
            }
        }

        /* 2. the node -> bone map validated (0xd72ad1); no result is stored */
        if (U8(0, 0x00d72ad1) != 0 && count > 1) {
            for (int32_t k = 1; k < count; k++) {
                const int32_t node = node_base_index(A, U32(A, 0xa544 + 4u * (uint32_t)(k - 1)));
                int32_t j = 0;
                for (; j < count; j++) if (I32(A, 0xa540 + 4u * (uint32_t)j) == node) break;
                (void)j;
            }
        }

        /* 3. per bone pair: the angles between the bike node's rotated X / Z axes and the rider bone's, applied as
         *    corrective world angular velocities to the body, then the body's angular velocity is damped by 0.9 */
        for (int32_t i = 1; i < count; i++) {
            const rx_addr qa_p = A + 0xe80 + 0x70u * (uint32_t)i;    /* this body */
            const rx_addr rec = bike + 0x30 + 0x30u * (uint32_t)(i - 1);
            const int32_t node = node_base_index(A, U32(A, 0xa544 + 4u * (uint32_t)(i - 1)));
            int32_t bone;
            {
                int32_t j = 0;
                for (; j < count; j++) if (I32(A, 0xa540 + 4u * (uint32_t)j) == node) break;
                bone = (j < count) ? j : -1;
            }
            {
                const rx_addr qb_p = A + 0xe80 + 0x70u * (uint32_t)bone;
                float qa[4], qb[4], b[4], r[4];
                const float X[4] = { 1.0f /* 0x9cb4a0 */, 0.0f /* 0x9cb4a4 */, 0.0f /* 0x9cb4a8 */, 0.0f /* 0x9cb4ac */ };
                const float Z[4] = { 0.0f /* 0x9cb4c0 */, 0.0f /* 0x9cb4c4 */, 1.0f /* 0x9cb4c8 */, 0.0f /* 0x9cb4cc */ };
                float uX[4], nX[3], vX[4], mX[3];
                float uZ[4], nZ[3], vZ[4], mZ[3];
                float cX[3], cZ[3], p1[4], p2[4], w1[3], w2[3];
                float dot1, dot2, a1, a2;
                qa[0] = F32(qa_p, 0); qa[1] = F32(qa_p, 4); qa[2] = F32(qa_p, 8); qa[3] = F32(qa_p, 0xc);
                qb[0] = F32(qb_p, 0); qb[1] = F32(qb_p, 4); qb[2] = F32(qb_p, 8); qb[3] = F32(qb_p, 0xc);
                b[0] = F32(rec, 0); b[1] = F32(rec, 4); b[2] = F32(rec, 8); b[3] = F32(rec, 0xc);
                {   /* r = conj(qb) * qa */
                    float cq[4];
                    cq[0] = neg_f(qb[0]); cq[1] = neg_f(qb[1]); cq[2] = neg_f(qb[2]); cq[3] = qb[3];
                    qmul(r, cq, qa);
                }
                /* the X axis group: nX = r's world X axis, mX = the bike node's world X axis */
                qmul(uX, r, X);
                qapply3(nX, r, uX);
                qmul(vX, b, X);
                qapply3(mX, b, vX);
                /* the Z axis group */
                qmul(uZ, r, Z);
                qapply3(nZ, r, uZ);
                qmul(vZ, b, Z);
                qapply3(mZ, b, vZ);
                /* cX = cross(nX, mX), cZ = cross(nZ, mZ) (the original's component order) */
                cX[0] = mX[2] * nX[1] - mX[1] * nX[2];
                cX[1] = mX[0] * nX[2] - mX[2] * nX[0];
                cX[2] = mX[1] * nX[0] - mX[0] * nX[1];
                cZ[0] = mZ[2] * nZ[1] - mZ[1] * nZ[2];
                cZ[1] = mZ[0] * nZ[2] - mZ[2] * nZ[0];
                cZ[2] = mZ[1] * nZ[0] - mZ[0] * nZ[1];
                /* the bike-node products of the cross vectors (the original's lane pattern) */
                p1[3] = (float)((((double)qb[3] * 0.0 - (double)cX[0] * (double)qb[0]) - (double)cX[1] * (double)qb[1]) -
                                (double)qb[2] * (double)cX[2]);
                p1[0] = (float)((((double)cX[0] * (double)qb[3] + (double)qb[0] * 0.0) + (double)qb[1] * (double)cX[2]) -
                                (double)cX[1] * (double)qb[2]);
                p1[1] = (float)((((double)cX[1] * (double)qb[3] + (double)qb[1] * 0.0) + (double)cX[0] * (double)qb[2]) -
                                (double)qb[0] * (double)cX[2]);
                p1[2] = (float)((((double)qb[3] * (double)cX[2] + (double)qb[2] * 0.0) + (double)cX[1] * (double)qb[0]) -
                                (double)cX[0] * (double)qb[1]);
                p2[3] = (float)((((double)qb[3] * 0.0 - (double)qb[0] * (double)cZ[0]) - (double)qb[1] * (double)cZ[1]) -
                                (double)qb[2] * (double)cZ[2]);
                p2[0] = (float)((((double)qb[3] * (double)cZ[0] + (double)qb[0] * 0.0) + (double)qb[1] * (double)cZ[2]) -
                                (double)qb[2] * (double)cZ[1]);
                p2[1] = (float)((((double)qb[3] * (double)cZ[1] + (double)qb[1] * 0.0) + (double)qb[2] * (double)cZ[0]) -
                                (double)qb[0] * (double)cZ[2]);
                p2[2] = (float)((((double)qb[3] * (double)cZ[2] + (double)qb[2] * 0.0) + (double)qb[0] * (double)cZ[1]) -
                                (double)qb[1] * (double)cZ[0]);
                qapply3(w1, qb, p1);
                qapply3(w2, qb, p2);
                /* the dots are single precision in the original (mulss / addss) */
                dot1 = (mX[1] * nX[1] + mX[0] * nX[0]) + mX[2] * nX[2];
                dot2 = (mZ[1] * nZ[1] + mZ[0] * nZ[0]) + mZ[2] * nZ[2];
                a1 = crash_angle(dot1);
                a2 = crash_angle(dot2);
                a1 = crash_torque_scale(a1);
                a2 = crash_torque_scale(a2);
                /* touching or settled: 0.75 in double, rounded */
                if (U32(A, 0xa834) != 0 || U8(A, 0x88) != 0) {
                    a2 = (float)((double)a2 * 0.75 /* 0x977b78 */);
                    a1 = (float)((double)a1 * 0.75);
                }
                {   /* the two world-frame angular velocity vectors: angle * axis * dt * 30.0 */
                    const float k = F32(0, 0x00931d98);                 /* 30.0 */
                    SF32(L, 0x220, a1 * w1[0] * dt * k);
                    SF32(L, 0x224, a1 * w1[1] * dt * k);
                    SF32(L, 0x228, a1 * w1[2] * dt * k);
                    SF32(L, 0x4c0, w2[0] * a2 * dt * k);
                    SF32(L, 0x4c4, w2[1] * a2 * dt * k);
                    SF32(L, 0x4c8, w2[2] * a2 * dt * k);
                    rx_set_sp(L - 4);
                    SU32(L - 4, 0, (uint32_t)i);
                    phys_rigbody_add_angular_velocity(L + 0x220, P, i);
                    SU32(L - 4, 0, (uint32_t)i);
                    phys_rigbody_add_angular_velocity(L + 0x4c0, P, i);
                    rx_set_sp(L);
                }
                /* the body's angular velocity damped by 0.9 */
                v4_st(A + 0xe90 + 0x70u * (uint32_t)i, v4_mul(v4_ld(A + 0xe90 + 0x70u * (uint32_t)i), v4_ld(L + 0x200)));
            }
        }

        /* 4. settled / touching: the root body's world angular velocity with its up component, then a linear damp */
        if ((U32(A, 0x374) != 0 || U8(A, 0x88) != 0) && U32(A, 0x98) == 0) {
            const float qx = F32(A, 0xe80), qy = F32(A, 0xe84), qz = F32(A, 0xe88), qw = F32(A, 0xe8c);
            const float wx = F32(A, 0xe90), wy = F32(A, 0xe94), wz = F32(A, 0xe98);
            const float nx = neg_f(qx), ny = neg_f(qy), nz = neg_f(qz);
            float px, py, pz, pw;
            float vx, vy, vz;
            /* p = P1(q, w) */
            pw = (float)((((double)qw * 0.0 - (double)qx * (double)wx) - (double)qy * (double)wy) - (double)wz * (double)qz);
            px = (float)((((double)qw * (double)wx + (double)qx * 0.0) + (double)wz * (double)qy) - (double)qz * (double)wy);
            py = (float)((((double)qw * (double)wy + (double)qy * 0.0) + (double)qz * (double)wx) - (double)wz * (double)qx);
            pz = (float)((((double)wz * (double)qw + (double)qz * 0.0) + (double)qx * (double)wy) - (double)qy * (double)wx);
            /* the world-frame angular velocity */
            vx = (float)((((double)qw * (double)px + (double)pw * (double)nx) + (double)nz * (double)py) -
                         (double)ny * (double)pz);
            vy = (float)((((double)qw * (double)py + (double)ny * (double)pw) + (double)pz * (double)nx) -
                         (double)nz * (double)px);
            vz = (float)((((double)qw * (double)pz + (double)nz * (double)pw) + (double)ny * (double)px) -
                         (double)py * (double)nx);
            /* the up axis (A+0xa844) splatted; grounded and steep: a quarter of the up component */
            {
                const float up_y = F32(A, 0xa844);
                v4_st(L + 0x4c0, v4_splat(up_y));
                if (U32(A, 0xa834) != 0 && (double)abs_f(up_y) > 0.85 /* 0x977b70 */)
                    vy = (float)((double)vy * 0.25 /* 0x977868 */);
            }
            /* the body-frame angular velocity: P1(conj(q), v) */
            {
                const float rw = (float)((((double)qw * 0.0 - (double)nx * (double)vx) - (double)ny * (double)vy) -
                                         (double)nz * (double)vz);
                const float rx = (float)((((double)qw * (double)vx + (double)nx * 0.0) + (double)ny * (double)vz) -
                                         (double)nz * (double)vy);
                const float ry = (float)((((double)qw * (double)vy + (double)ny * 0.0) + (double)nz * (double)vx) -
                                         (double)nx * (double)vz);
                const float rz = (float)((((double)qw * (double)vz + (double)nz * 0.0) + (double)nx * (double)vy) -
                                         (double)ny * (double)vx);
                SF32(A, 0xe90, (float)((((double)qw * (double)rx + (double)qx * (double)rw + (double)qz * (double)ry) -
                                        (double)qy * (double)rz)));
                SF32(A, 0xe94, (float)((((double)qy * (double)rw + (double)qw * (double)ry + (double)qx * (double)rz) -
                                        (double)qz * (double)rx)));
                SF32(A, 0xe98, (float)((((double)qz * (double)rw + (double)qw * (double)rz + (double)qy * (double)rx) -
                                        (double)qx * (double)ry)));
                SF32(A, 0xe9c, F32(L, 0x4bc));                       /* the frame's stale w lane */
            }
            {   /* the linear velocity damp while grounded and slow in x / z */
                float lvx = F32(A, 0x410), lvz = F32(A, 0x418);
                if (U32(A, 0xa834) != 0 && (double)abs_f(F32(L, 0x4c4)) > 0.9 /* 0x977b68 */ &&
                    abs_f(lvx) < 2.0f /* 0x92e330 */ && abs_f(lvz) < 2.0f) {
                    lvx = (float)((double)lvx * 0.4 /* 0x977b60 */);
                    lvz = (float)((double)lvz * 0.4);
                }
                SF32(A, 0x410, lvx);
                SF32(A, 0x418, lvz);
                SF32(A, 0x41c, F32(L, 0x3c));                        /* the frame's stale w lane */
            }
        }
    }
done:
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00426d00, phys_crash_bike_solver)
