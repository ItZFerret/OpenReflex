/* bike_vehicle.c -- RavenVehicle's step listeners (the vehicle object V; its listener sub-object L = V + 0x90 is what the world
 * calls before / after the physics step). L+1a4 = the simulation S, L+ec = the vehicle's binding handle, L+100 = the player
 * (controller index at +1f0), L+110 = the vehicle's model, L+1f0 = control state (2 = crashed / rider forced off). */
#include "bike_vehicle.h"
#include "bike_sim.h"
#include "bike_remote.h"
#include "phys_misc.h"
#include "loop_vehicle.h"
#include "phys_body.h"
#include "phys_vec.h"
#include "world_rider.h"
#include "rx_vcall.h"

static uint32_t veh_vcall0(rx_addr obj, uint32_t off) {           /* thiscall obj->vtable[off]() */
    return rx_vcall(obj, off, 0, NULL);
}
/* 0042bd90: the indexed 0xe0-byte record of a handle (EAX = handle); returns it in EAX. */
static rx_addr veh_handle_record(rx_addr handle) {
    rx_addr rec;
    PHYS_CALL_SP(0, rec = phys_body_record(handle));
    return rec;
}
static void veh_clear_inputs(rx_addr S) {
    SF32(S, 0x2c, 0.0f); SF32(S, 0x30, 0.0f); SF32(S, 0x34, 0.0f); SF32(S, 0x38, 0.0f);
    SF32(S, 0x1c, 0.0f); SF32(S, 0x20, 0.0f); SF32(S, 0x24, 0.0f); SF32(S, 0x28, 0.0f);
    SU32(S, 0x3c, 0); SU32(S, 0x48, 0); SU32(S, 0x4c, 0); SU32(S, 0x50, 0);
}

/* 0045d510 RavenVehicle::vf0_o90 (ECX = L, stack dt; ret 4): before the physics step. A developer key (input slot 0x31)
 * toggles the vehicle's debug flag (global 009cc8a0, the record's +2c->+14); a crash request S+2e9c forces the rider off;
 * the record flags; the vehicle's own pre-step (V slot 0xf8); then the input by simulation type S+4: 0 local player
 * (debug keys, then the controller through the simulation's slot 23 or 0x004308a0 during a pose override of mode 2, or no
 * input), 1 remote: the received record L+1190 when one is pending L+1184 (00432be0, bike_remote.c), 2 AI (its input block L+1200.. through slot 24). */
void bike_vehicle_pre_step(rx_addr L, float dt) {
    (void)dt;
    {
        rx_addr game = U32(0, 0x00a95844);
        rx_addr input = veh_vcall0(game, 8);
        uint32_t args[2] = {0x31, 0};
        if (rx_vcall(input, 0x7c, 2, args) != 0) {
            rx_addr h = U32(L, 0xec);
            if (U32(0, 0x009cc8a0) != 0) { if (h) SU32(U32(veh_handle_record(h), 0x2c), 0x14, 0xffffffffu); }
            else if (h) SU32(U32(veh_handle_record(h), 0x2c), 0x14, 0);
            SU32(0, 0x009cc8a0, U32(0, 0x009cc8a0) == 0 ? 1 : 0);
        }
    }
    if (U32(U32(L, 0x1a4), 0x2e9c) != 0) {                 /* crash request: rider off */
        SU32(L, 0x1f0, 2);
        SU32(U32(L, 0x1a4), 0x2374, 1);
        PHYS_CALL_SP(8, rx_world_rider_end_wreck(U32(L, 0x110), 4));   /* (model, 4); ret 8 */
    }
    if (U32(L, 0xec) != 0) SU8(veh_handle_record(U32(L, 0xec)), 0xd4, 0);
    if (U32(L, 0xec) != 0) { rx_addr r = U32(veh_handle_record(U32(L, 0xec)), 0x2c) + 0x14; SU32(r, 0, U32(r, 0) | 4); }
    veh_vcall0(L - 0x90, 0xf8);

    const rx_addr S = U32(L, 0x1a4);
    switch (U32(S, 4)) {
    case 0: {                                                 /* local player */
        bike_handle_debug_keys(S);
        rx_addr p = U32(L, 0x100);
        if (p == 0) return;
        rx_addr S2 = U32(L, 0x1a4);
        if (U32(L, 0x1f0) == 0 && U32(S2, 0x2de0) == 0 && U32(S2, 0x2de4) != 3) {
            if (U8(p, 0x21c) == 0 || U32(0, 0x00a7a994) != U32(p, 0x1f0) || U32(0, 0x00a9418c) != 0) {
                uint32_t arg = U32(p, 0x1f0);
                rx_vcall(S2, 0x5c, 1, &arg);   /* slot 23: read the controller */
                return;
            }
        }
        rx_addr S3 = U32(L, 0x1a4);
        if (U32(S3, 0x2de0) != 0 && U32(S3, 0x2de4) == 2) { bike_read_controller_input(U32(p, 0x1f0), S3 + 0x1c); return; }
        veh_clear_inputs(S3);
        return;
    }
    case 1:                                                   /* remote */
        if (U32(L, 0x1184) != 0) {
            PHYS_CALL_SP(4, bike_remote_apply_record(L + 0x1190, S));   /* 00432be0 (ECX = the received record, stack S) */
            SU32(L, 0x1184, 0);
        }
        return;
    case 2:                                                   /* AI */
        if (U32(L, 0x100) == 0) return;
        if (U32(L, 0x1f0) != 0 || U32(S, 0x2de0) != 0) { veh_clear_inputs(S); return; }
        {
            const rx_addr sp = rx_sp(), B = (sp - 0x50) & ~15u;
            rx_set_sp(B);
            bike_init_input_block(B);
            SF32(B, 0x00, F32(L, 0x1200)); SF32(B, 0x08, F32(L, 0x1204)); SF32(B, 0x0c, F32(L, 0x1208));
            SF32(B, 0x10, F32(L, 0x121c)); SF32(B, 0x14, F32(L, 0x1220)); SF32(B, 0x18, F32(L, 0x1214));
            SF32(B, 0x1c, F32(L, 0x1218)); SU32(B, 0x2c, U32(L, 0x1210)); SF32(B, 0x04, F32(L, 0x120c));
            SU32(B, 0x24, 0); SU32(B, 0x30, 0); SU32(B, 0x34, 0);
            uint32_t arg = B;
            rx_vcall(S, 0x60, 1, &arg);    /* slot 24: set the input block */
            rx_set_sp(sp);
        }
        return;
    default:
        return;
    }
}
RX_FUNCTION(0x0045d510, bike_vehicle_pre_step)

/* 0045d820 RavenVehicle::vf1_o90 (ECX = L, stack dt; ret 4): after the physics step: clears the drag scale S+2f00, the
 * vehicle's "settled" flags L+1a8 / 1ac (set while not resting, not in a pose override of mode 2 and not constrained S+5c),
 * the vehicle update (V slot 0x74, dt), the distance travelled L+c4 += |L+30 - L+40|, then the previous position. */
void bike_vehicle_post_step(rx_addr L, float dt) {
    const rx_addr S = U32(L, 0x1a4);
    SF32(S, 0x2f00, 0.0f);
    rx_addr S2 = U32(L, 0x1a4);
    uint32_t free = U32(S2, 0x90) == 0 && !(U32(S2, 0x2de0) != 0 && U32(S2, 0x2de4) == 2) && U32(S2, 0x5c) == 0;
    SU32(L, 0x1a8, free); SU32(L, 0x1ac, free);
    {
        const rx_addr V = L - 0x90;
        uint32_t arg = bike_x87_copy32(rx_fb(dt));
#ifndef RX_ORACLE   /* the product: the registered function (this, float dt) */
        void (*f74)(rx_addr, float) = RX_FN(U32(U32(V, 0), 0x74), void (*)(rx_addr, float));
        if (f74) { const rx_addr sp74 = rx_sp(); rx_set_sp(sp74 - 8u); f74(V, rx_bf(arg)); rx_set_sp(sp74); } else
#endif
        rx_call_original(U32(U32(V, 0), 0x74), RX_THISCALL, V, 1, &arg);
    }
    float dx = phys_sub(F32(L, 0x30), F32(L, 0x40)), dy = phys_sub(F32(L, 0x34), F32(L, 0x44)), dz = phys_sub(F32(L, 0x38), F32(L, 0x48));
    float d = phys_sqrt(phys_add(phys_add(phys_mul(dy, dy), phys_mul(dz, dz)), phys_mul(dx, dx)));
    SF32(L, 0xc4, phys_add(d, F32(L, 0xc4)));
    SF32(L, 0x40, bike_x87_copyf(F32(L, 0x30)));
    SF32(L, 0x44, bike_x87_copyf(F32(L, 0x34)));
    SF32(L, 0x48, bike_x87_copyf(F32(L, 0x38)));
}
RX_FUNCTION(0x0045d820, bike_vehicle_post_step)

/* ==== 0045d910 the vehicle's debug view (EDI = vehicle V, stack dt; ret 4) ====================================================
 * Called by every RavenVehicle*::vf29 for local simulations. A developer key (input 0x64 through the engine context 00a95844)
 * cycles the global mode 00a9418c through 0..4 and resets the smoothing (0042d8c0 on the position filters V+a0, the angle
 * filter V+ac). By mode, from the vehicle's position P (slot 0x78) and orientation q (slot 0x90):
 *   1  the view quaternion G (00af6cd0, x y z w) = the frame of the reference axis K (009cb4b0) around q's orientation,
 *      the view position (00c01b90) = P;
 *   2  G turns toward that frame by a filtered fraction of the angle between them (at most 10 rad/s); the position follows
 *      P through the filters V+a0 (0042d7f0);
 *   3  G = the fixed 009cb550, the position filtered as in 2;
 *   0, 4  nothing. */
#define DBG_MODE   0x00a9418cu
#define DBG_G      0x00af6cd0u
#define DBG_POS    0x00c01b90u
#define DBG_K      0x009cb4b0u                  /* the reference axis (data) */

/* the frame quaternion of an orientation q relative to the axis K: the rotation matrix's cofactor column A, n = A x K
 * normalized, rows (K x n, K, n) -> quaternion (0045d972 / 0045de9e compute it the same way) */
static rx_v4 dbg_frame_quat(float x, float y, float z, float w, float row0_yz[2]) {
    const float x2 = phys_mul(x, 2.0f), y2 = phys_mul(y, 2.0f), z2 = phys_mul(z, 2.0f);   /* 0092e330 */
    const float xx2 = phys_mul(x, x2), wx2 = phys_mul(w, x2), wz2 = phys_mul(w, z2);
    const float a = phys_sub(1.0f, phys_add(phys_mul(z, z2), xx2));                 /* R11 */
    const float b = phys_sub(1.0f, phys_add(phys_mul(y, y2), xx2));                 /* R22 */
    const float f = phys_sub(phys_mul(x, y2), wz2);                                 /* R01 */
    const float e = phys_add(phys_mul(x, z2), phys_mul(w, y2));                     /* R02 */
    const float yz2 = phys_mul(y, z2);
    const float c = phys_add(yz2, wx2), d = phys_sub(yz2, wx2);                     /* R21, R12 */
    const float A1 = phys_sub(phys_mul(b, a), phys_mul(c, d));
    const float A2 = phys_sub(phys_mul(c, e), phys_mul(b, f));
    const float A3 = phys_sub(phys_mul(d, f), phys_mul(a, e));
    const float K0 = F32(0, DBG_K), K1 = F32(0, DBG_K + 4), K2 = F32(0, DBG_K + 8);
    float n[3] = { phys_sub(phys_mul(K2, A2), phys_mul(K1, A3)), phys_sub(phys_mul(A3, K0), phys_mul(K2, A1)),
                   phys_sub(phys_mul(K1, A1), phys_mul(A2, K0)) };
    const float ln = phys_add(phys_add(phys_mul(n[1], n[1]), phys_mul(n[0], n[0])), phys_mul(n[2], n[2]));
    if (ln > 0.0f) {
        const float inv = phys_div(1.0f, phys_sqrt(ln));                            /* SqrtF 0042c230 */
        n[0] = phys_mul(n[0], inv); n[1] = phys_mul(n[1], inv); n[2] = phys_mul(n[2], inv);
    }
    const float K[3] = { K0, K1, K2 };
    float u[3];
    bike_vec3_cross(u, K, n);                                                       /* 00401630: K x n */
    const float lu = phys_add(phys_add(phys_mul(u[1], u[1]), phys_mul(u[0], u[0])), phys_mul(u[2], u[2]));
    if (lu > 0.0f) {
        const float inv = phys_div(1.0f, phys_sqrt(lu));
        u[0] = phys_mul(inv, u[0]); u[1] = phys_mul(inv, u[1]); u[2] = phys_mul(inv, u[2]);
    }
    if (row0_yz) { row0_yz[0] = u[1]; row0_yz[1] = u[2]; }
    const float M[3][3] = {{u[0], u[1], u[2]}, {K0, K1, K2}, {n[0], n[1], n[2]}};
    return bike_matrix_to_quat(M);
}
/* the angle polynomials of this function (00935d3c.. cosine, 00935d54.. sine), on x already wrapped by the caller's rules */
static float dbg_cos_core(float u2) {
    const float u4 = phys_mul(u2, u2);
    const float k6 = rx_bf(0xb46beed0u), k4 = rx_bf(0x37cb076bu), k3 = rx_bf(0x3d2aa372u);
    const float k2 = rx_bf(0xbab5a662u), k1 = rx_bf(0xbeffff3du), k0 = rx_bf(0x3f7ffff3u);
    return phys_add(phys_mul(phys_add(phys_mul(phys_add(phys_mul(u2, k6), k4), u4), phys_add(phys_mul(u2, k2), k3)), u4),
                    phys_add(phys_mul(u2, k1), k0));
}
static float dbg_wrap(float x) {                 /* x - trunc(x / 2 pi) 2 pi */
    return phys_sub(x, phys_mul((float)rx_cvtt(phys_mul(x, 0.15915493667125702f /* 009774a0 */)), 6.2831854820251465f /* 0097749c */));
}
/* sin(a) as the code gets it: the cosine polynomial of |a + pi/2| */
static float dbg_sin(float a) {
    const float u = phys_sub(3.1415927410125732f /* 00977498 */, dbg_wrap(bike_fabs(phys_add(a, 1.5707963705062866f /* 00977508 */))));
    return dbg_cos_core(phys_mul(u, u));
}
static float dbg_signed_asin(float c) {
    const float a = bike_asin_approx(bike_fabs(c), c);
    return c >= 0.0f ? a : phys_sub(-0.0f, a);
}
/* 0042d7f0 (LTCG: ESI = the position filters (three pointers to {value, time constant, factor}), EDX = the target vec3, EAX = the output vec3,
 * xmm4 = dt; plain ret) -> EAX = out: each axis moves its filter value towards the target by min(dt, time constant) / time constant of the
 * way (the factor is kept at +8), and the new value is the output. A NaN time constant counts as the smaller (comiss / jbe). */
rx_addr bike_vehicle_position_filter(rx_addr filters, rx_addr target, rx_addr out, float dt) {
    for (uint32_t axis = 0; axis < 3; axis++) {
        const rx_addr f = U32(filters, 4 * axis);
        const float tau = F32(f, 4);
        const float step = tau > dt ? dt : tau;
        const float inverse = rx_divs(1.0f /* 0x931f24 */, tau);
        const float current = F32(f, 0);
        const float factor = rx_muls(step, inverse);
        const float value = rx_adds(rx_muls(rx_subs(F32(target, 4 * axis), current), factor), current);
        SF32(f, 8, factor);
        SF32(f, 0, value);
        SF32(out, 4 * axis, value);
    }
    return out;
}
RX_FUNCTION(0x0042d7f0, bike_vehicle_position_filter)
static rx_addr dbg_filter_position(rx_addr filters, rx_addr target, rx_addr out, float dt) {
    rx_addr r;
    PHYS_CALL_SP(0, r = bike_vehicle_position_filter(filters, target, out, dt));
    return r;
}

void bike_vehicle_debug_view(rx_addr V, float dt) {
    const rx_addr entry_sp = rx_sp();
    const rx_addr A = (entry_sp - 4) & ~15u, E = A - 0xa0;    /* the original's frame: ESP after its prologue (push esi) */
    rx_set_sp(E);
    { uint32_t arg = E + 0x70; rx_vcall(V, 0x78, 1, &arg); }   /* the position */
    { uint32_t arg = E + 0x80; rx_vcall(V, 0x90, 1, &arg); }   /* the orientation */
    rx_st4(E + 0x60, rx_ld4(E + 0x70));
    const rx_v4 q = rx_ld4(E + 0x80);
    const uint32_t mode = U32(0, DBG_MODE);
    if (mode == 1) {
        rx_st4(DBG_G, dbg_frame_quat(q.v[0], q.v[1], q.v[2], q.v[3], NULL));
        for (int i = 0; i < 3; i++) SF32(0, DBG_POS + 4 * i, F32(E, 0x60 + 4 * i));
    } else if (mode == 2) {
        /* the target frame, normalized in double. loc40 / 44 / 48 = what the original leaves at E+0x40..0x48 (the frame's
         * row 0 y / z there, then the turn angle at +0x40): read back below when the turn axis degenerates */
        float row0[2];
        rx_v4 t = dbg_frame_quat(q.v[0], q.v[1], q.v[2], q.v[3], row0);
        float loc40;
        const float loc44 = row0[0], loc48 = row0[1];
        {
            const double l2d = (((double)t.v[3] * t.v[3] + (double)t.v[0] * t.v[0]) + (double)t.v[1] * t.v[1]) + (double)t.v[2] * t.v[2];
            const float l2 = (float)l2d;
            if (l2 == 0.0f) t = v4(0.0f, 0.0f, 0.0f, 0.0f);
            else {
                const float inv = phys_div(1.0f, phys_sqrt(l2));
                t = v4((float)((double)t.v[0] * inv), (float)((double)t.v[1] * inv), (float)((double)t.v[2] * inv),
                       (float)((double)t.v[3] * inv));
            }
        }
        /* r = conj(G) * t (double) */
        const float Gx = F32(0, DBG_G), Gy = F32(0, DBG_G + 4), Gz = F32(0, DBG_G + 8), Gw = F32(0, DBG_G + 12);
        {
            const double m0 = phys_sub(-0.0f, Gx), m1 = phys_sub(-0.0f, Gy), m2 = phys_sub(-0.0f, Gz), gw = Gw;
            const double tx = t.v[0], ty = t.v[1], tz = t.v[2], tw = t.v[3];
            const float rw = (float)(((gw * tw - m0 * tx) - m1 * ty) - m2 * tz);
            const float rx = (float)(((tw * m0 + gw * tx) + m1 * tz) - m2 * ty);
            const float ry = (float)(((gw * ty + m1 * tw) + m2 * tx) - tz * m0);
            const float rz = (float)(((gw * tz + m2 * tw) + ty * m0) - m1 * tx);
            /* the turn: angle and axis */
            const float half = (float)(1.5707963705062866 /* 00977510 */ - (double)dbg_signed_asin(rw));
            loc40 = half;
            float ang = half, ax, ay, az;
            if (half == 0.0f) {
                ax = F32(0, 0x009cb4a0); ay = F32(0, 0x009cb4a4); az = F32(0, 0x009cb4a8);
            } else {
                const float s = dbg_sin(half);
                ang = phys_mul(half, 2.0f);
                loc40 = ang;
                const float inv = phys_div(1.0f, s);
                ax = phys_mul(inv, rx); ay = phys_mul(inv, ry); az = phys_mul(inv, rz);
            }
            const float la = phys_add(phys_add(phys_mul(ay, ay), phys_mul(ax, ax)), phys_mul(az, az));
            if (la > 0.0f) {
                const float inv = phys_div(1.0f, phys_sqrt(la));
                ax = phys_mul(ax, inv); ay = phys_mul(ay, inv); az = phys_mul(az, inv);
            }
            if (ang > 3.1415927410125732f) ang = phys_sub(ang, 6.2831854820251465f);
            else if (-3.1415927410125732f /* 00977530 */ > ang) ang = phys_add(ang, 6.2831854820251465f);
            const float rate = phys_div(ang, dt);
            if (bike_fabs(rate) > 10.0f /* 0092fd88 */) ang = phys_mul(phys_mul(phys_div(10.0f, bike_fabs(rate)), rate), dt);
            float n0, n1, n2;
            const float ln = phys_add(phys_add(phys_mul(ay, ay), phys_mul(ax, ax)), phys_mul(az, az));
            if (ln > 0.0f) {
                const float inv = phys_div(1.0f, phys_sqrt(ln));
                n0 = phys_mul(inv, ax); n1 = phys_mul(ay, inv); n2 = phys_mul(az, inv);
            } else {
                n0 = loc40; n1 = loc44; n2 = loc48;               /* the original reads its stale locals here */
            }
            /* D = (n sin(ang / 2), cos(ang / 2)) */
            const float h = phys_mul(ang, 0.5f /* 009322cc */);
            const float sgn = h >= 0.0f ? 1.0f : F32(0, 0x009cd9e8);
            const float x = phys_mul(sgn, h);
            const float uu = phys_sub(3.1415927410125732f, dbg_wrap(x)), u2 = phys_mul(uu, uu), u4 = phys_mul(u2, u2);
            const float cs = phys_sub(-0.0f, dbg_cos_core(u2));
            const float sp = phys_mul(phys_mul(phys_add(phys_mul(phys_add(phys_mul(phys_add(phys_mul(u2, rx_bf(0xb2aee90au) /* 00935d68 */),
                rx_bf(0x36352cc0u) /* 00935d64 */), u4), phys_add(phys_mul(u2, rx_bf(0xb94fb563u) /* 00935d60 */), rx_bf(0x3c0884a2u) /* 00935d5c */)), u4),
                phys_add(phys_mul(u2, rx_bf(0xbe2aaa5cu) /* 00935d58 */), rx_bf(0x3f7ffff9u) /* 00935d54 */)), uu), sgn);
            const double dx = phys_mul(n0, sp), dy = phys_mul(n1, sp), dz = phys_mul(n2, sp), dw = cs;
            const double gx = Gx, gy = Gy, gz = Gz, gw2 = Gw;
            const float Pw = (float)(((dw * gw2 - dx * gx) - dy * gy) - dz * gz);
            const float Px = (float)(((dw * gx + gw2 * dx) + dz * gy) - dy * gz);
            const float Py = (float)(((dw * gy + dy * gw2) + gz * dx) - dz * gx);
            const float Pz = (float)(((dw * gz + dz * gw2) + dy * gx) - gy * dx);
            /* blend G toward P by the filtered angle (V+ac), slerp weights from the polynomial sines */
            float fr = phys_lag_filter_update(V + 0xac, dt, bike_fabs(ang));
            if (fr > 1.0f) fr = 1.0f;
            const float dot = bike_fabs(phys_add(phys_add(phys_add(phys_mul(Py, Gy), phys_mul(Px, Gx)), phys_mul(Pz, Gz)), phys_mul(Gw, Pw)));
            const float om = (float)(1.5707963705062866 - (double)dbg_signed_asin(dot));
            const float inv_s = phys_div(1.0f, dbg_sin(om));
            const float fom = phys_mul(fr, om);
            float k1 = 1.0f, k2 = 0.0f;
            if (!(dot >= 1.0f)) {
                k1 = phys_mul(dbg_sin(phys_sub(om, fom)), inv_s);
                k2 = phys_mul(dbg_sin(fom), inv_s);
            }
            float nw = phys_add(phys_mul(k1, Gw), phys_mul(k2, Pw));
            float nx = phys_add(phys_mul(k2, Px), phys_mul(k1, Gx));
            float nz = phys_add(phys_mul(k2, Pz), phys_mul(k1, Gz));
            float ny = phys_add(phys_mul(k2, Py), phys_mul(k1, Gy));
            const float l2 = (float)((((double)nx * nx + (double)nw * nw) + (double)ny * ny) + (double)nz * nz);
            if (l2 == 0.0f) rx_st4(DBG_G, v4(0.0f, 0.0f, 0.0f, 0.0f));
            else {
                const float inv = phys_div(1.0f, phys_sqrt(l2));
                rx_st4(DBG_G, v4((float)((double)inv * nx), (float)((double)inv * ny), (float)((double)inv * nz), (float)((double)inv * nw)));
            }
        }
        const rx_addr p = dbg_filter_position(V + 0xa0, E + 0x60, E + 0x20, dt);
        for (int i = 0; i < 3; i++) SF32(0, DBG_POS + 4 * i, F32(p, 4 * i));
    } else if (mode == 3) {
        const rx_addr p = dbg_filter_position(V + 0xa0, E + 0x60, E + 0x40, dt);
        for (int i = 0; i < 3; i++) SF32(0, DBG_POS + 4 * i, F32(p, 4 * i));
        rx_st4(DBG_G, rx_ld4(0x009cb550));
    }
    /* the developer key */
    const rx_addr ctx = U32(0, 0x00a95844);
    const rx_addr input = rx_vcall(ctx, 0x08, 0, NULL);
    const uint32_t key[2] = { 0x64, 0 };
    if (rx_vcall(input, 0x7c, 2, key) != 0) {
        uint32_t m = U32(0, DBG_MODE) + 1;
        SU32(0, DBG_MODE, (int32_t)m > 4 ? 0u : m);
        rx_vehicle_set_limits(DBG_POS, V + 0xa0, rx_bf(0x7f7fffffu));          /* 0042d8c0, FLT_MAX: keep the time constants */
        phys_lag_filter_set(V + 0xac, 0.0f, rx_bf(0x7f7fffffu));                 /* 0042d670 */
    }
    rx_set_sp(entry_sp);
}
static void veh_debug_view(rx_addr V, float dt) { bike_vehicle_debug_view(V, dt); }
RX_FUNCTION(0x0045d910, veh_debug_view)
