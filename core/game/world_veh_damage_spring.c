/* world_veh_damage_spring.c -- a detachable body part's spring (0047aff0) of the vehicle damage behaviour, and the foley hook it
 * calls at the end stop (00681750).
 *
 * Damage behaviour E (world_veh_damage_hit.c has the layout): +0 the part count, +4 the part table (0x350 bytes per part), +0xc + 4i the
 * part state (2: the part swings on its hinge), +0x130 + 0x10 i the rest orientation (quaternion x y z w), +0x1b0 + 0x10 i the current
 * orientation, +0x230 + 4i the hinge angle, +0x250 + 4i its angular velocity, +0x270 + 0x20 i the previous node position, +0x280 + 0x20 i
 * the current one, +0x370 + 4i the part's node, +0x3fc the vehicle.
 * Part table record R = E[+4] + 0x350 i: +0x310 the hinge axis (x y z), +0x320 the axis in the node's frame, +0x330 / +0x334 the angle
 * limits, +0x338 the rest angle, +0x33c the inertia c, +0x340 the stiffness k, +0x344 the weight of the node acceleration.
 *
 * The step (dt clamped to [0, 0.1] first) is a damped oscillator: with d = angle - rest and q = (-k d - velocity) / c the new angle
 * is rest - cos(theta) sqrt(d^2 + q^2) exp(-k dt), theta = c dt + atan2(q c, c d) in the original's polynomial cosine, and the new
 * velocity the derivative plus the node's acceleration (the node moved since the last step: its world position, twice differenced)
 * projected on the hinge axis. The angle is held between the limits (an impact on the lower limit calls the vehicle's foley hook,
 * the speed reflected by 0xd92d50), and the part node's rotation is the rest orientation times the half-angle rotation about the
 * normalized axis (quaternion product in doubles). Hand transliteration of the disassembly (SSE singles, x87 for the exponent). */
#include "world_util.h"
#include "world_veh_damage.h"
#include "world_node.h"
#include "world_veh.h"
#include "crt_util.h"
#include "anim.h"
#include "fx.h"
#include "crt.h"
#include "phys_crt.h"

/* 00681750 (ESI = the vehicle): the vehicle's slot in the foley manager (0xd92f00: 12 records of 0x1b10 bytes from +0x10, the vehicle first)
 * when it has its sound bound (+0xa84, and its parts +0xa88 / +0xa80 / +0xa8c): the sound object is flagged (+0x908 = 1) and told the
 * record's +0x1c0 (+0x900). The product's vehicle sound is never bound (the audio system is not created: world_util.h AUDIO_BOUNDARY),
 * so there the loop finds no slot of the vehicle and returns. */
void rx_vehicle_foley_end_stop(rx_addr vehicle) {
    const rx_addr mgr = U32(0, 0x00d92f00);
    uint32_t k = 0;
    rx_addr rec = mgr + 0x10;
    for (;; k++, rec += 0x1b10) {
        if (U32(rec, 0) == vehicle) break;
        if (k + 1 >= 0xc) return;
    }
    const rx_addr s = mgr + 0x1b10 * k + 0xa70;
    if (U32(mgr + 0x1b10 * k, 0xa84) == 0 || U32(s, 0x18) == 0 || U32(s, 0x10) == 0 || U32(s, 0x1c) == 0) return;
    SU32(U32(s, 0x1c), 0x908, 1);
    SU32(U32(s, 0x1c), 0x900, U32(s, 0x18) + 0x1c0);
}
RX_FUNCTION(0x00681750, rx_vehicle_foley_end_stop)

/* SSE scalar double arithmetic with the x86 NaN rule (the first operand's NaN quieted, else the second's, else the default NaN) */
static double dnan(double r, double a, double b) {
    if (r == r) return r;
    if (a != a) return rx_bd(rx_db(a) | 0x0008000000000000ull);
    if (b != b) return rx_bd(rx_db(b) | 0x0008000000000000ull);
    return rx_bd(0xfff8000000000000ull);
}
static double dadd(double a, double b) { return dnan(a + b, a, b); }
static double dsub(double a, double b) { return dnan(a - b, a, b); }
static double dmul(double a, double b) { volatile double vb = b; return dnan(a * vb, a, b); }

/* the inlined odd polynomial of the half-angle sine (0x935d54 .. 0x935d68): ((u2 c68 + c64) u4 + (u2 c60 + c5c)) u4 + (u2 c58 + c54) */
static float sin_poly(float u2, float u4) {
    const float c68 = F32(0, 0x00935d68), c64 = F32(0, 0x00935d64), c60 = F32(0, 0x00935d60), c5c = F32(0, 0x00935d5c);
    const float c58 = F32(0, 0x00935d58), c54 = F32(0, 0x00935d54);
    return phys_add(phys_mul(phys_add(phys_mul(phys_add(phys_mul(u2, c68), c64), u4), phys_add(phys_mul(u2, c60), c5c)), u4),
                    phys_add(phys_mul(u2, c58), c54));
}

/* the inlined cosine polynomial (0x935d3c .. 0x935d50) of u = pi - reduce(m): ((u2 c50 + c48) u4 + (u2 c44 + c4c)) u4 + (u2 c40 + c3c) */
static float cos_poly(float u2, float u4) {
    return phys_add(phys_mul(phys_add(phys_mul(phys_add(phys_mul(u2, F32(0, 0x00935d50)), F32(0, 0x00935d48)), u4),
                                      phys_add(phys_mul(u2, F32(0, 0x00935d44)), F32(0, 0x00935d4c))), u4),
                    phys_add(phys_mul(u2, F32(0, 0x00935d40)), F32(0, 0x00935d3c)));
}

/* (1/2 pi, 2 pi, pi) argument reduction as compiled: pi - (m - trunc(m / 2 pi) 2 pi) */
static float reduce_u(float m) {
    const float turns = (float)rx_cvtt(phys_mul(m, F32(0, 0x009774a0)));
    return phys_sub(F32(0, 0x00977498), phys_sub(m, phys_mul(turns, F32(0, 0x0097749c))));
}

/* 0047aff0 (stdcall E, part index i, dt; RET 0xc). Frame: push ebp; and esp, -16; sub esp, 0x64; push ebx / esi / edi. */
void rx_world_damage_part_spring(rx_addr e, uint32_t i, float dt) {
    world_frame f = world_frame_enter(0x64, 3, 1);
    const rx_addr S = f.esp;
    const rx_addr node = U32(e, 0x370 + 4 * i);
    if (node == 0) goto done;
    const float negz = F32(0, 0x009774a8);   /* -0.0 */
    {   /* dt clamped to [0, 0.1] (a NaN stays) */
        const float limit = F32(0, 0x00931d94);
        if (dt < limit || dt != dt) { if (!(0.0f < dt || dt != dt)) dt = 0.0f; }
        else dt = limit;
        SF32(f.ebp, 0x10, dt);
    }
    const rx_addr T = U32(e, 4);
    const rx_addr R = T + i * 0x350;
    const float angle = F32(e, 0x230 + 4 * i), vel = F32(e, 0x250 + 4 * i);
    const float rest = F32(R, 0x338), c = F32(R, 0x33c), k = F32(R, 0x340);
    const float d = phys_sub(angle, rest);
    const float num = phys_sub(phys_sub(negz, phys_mul(k, d)), vel);
    const float q = phys_div(num, c);
    SU32(S, 0x2c, i * 0x350); SF32(S, 0x30, rest); SF32(S, 0x24, k); SF32(S, 0x18, c); SF32(S, 0x20, num); SF32(S, 0x28, d);
    float r;
    {   /* SqrtF (cdecl; one pushed argument) */
        const float sum = phys_add(phys_mul(d, d), phys_mul(q, q));
        SF32(S, -4, sum);
        WORLD_CALL_SP(4, r = phys_sqrtf(sum));
    }
    SF32(S, 0x1c, r);
    float at;
    {   /* FUN_0041c4b0 = atan2f(y, x), cdecl, two pushed arguments */
        const float cd = phys_mul(c, d);
        SF32(S, -4, cd); SF32(S, -8, num);
        WORLD_CALL_SP(8, at = rx_fx_atan2f(num, cd));
    }
    SF32(S, 0x28, at);
    const float theta = phys_add(phys_mul(c, dt), at);
    SF32(S, 0x34, theta);
    float abs_theta;
    WORLD_CALL_SP(0, abs_theta = rx_world_float_abs(theta));
    SF32(S, 0x38, abs_theta);
    /* exp(-(k dt)): the product of two floats is exact in the x87; _CIexp takes the double */
    const float damp = (float)crt_exp_x87(-((double)k * (double)dt));
    SF32(S, 0x28, damp);
    const float u1 = reduce_u(abs_theta), x1 = phys_mul(u1, u1), x2 = phys_mul(x1, x1);
    SF32(S, 0x20, x1); SF32(S, 0x14, x2);
    const float cos_a = cos_poly(x1, x2);
    const float newangle = phys_sub(rest, phys_mul(phys_mul(cos_a, r), damp));
    SF32(S, 0x14, newangle);
    float abs_y;
    WORLD_CALL_SP(0, abs_y = rx_world_float_abs(phys_add(theta, F32(0, 0x00977508))));
    const float uy = reduce_u(abs_y), y1 = phys_mul(uy, uy), y2 = phys_mul(y1, y1);
    SF32(S, 0x34, y1); SF32(S, 0x30, y2);
    const float uz = reduce_u(abs_theta), z1 = phys_mul(uz, uz), z2 = phys_mul(z1, z1);
    SF32(S, 0x3c, z1); SF32(S, 0x20, z2);
    /* the node's acceleration: the position twice differenced */
    const rx_addr prev = e + 0x270 + 0x20 * i, cur = e + 0x280 + 0x20 * i;
    const float o0 = F32(cur, 0), o1 = F32(cur, 4), o2 = F32(cur, 8);
    const float v0 = phys_div(phys_sub(o0, F32(prev, 0)), dt), v1 = phys_div(phys_sub(o1, F32(prev, 4)), dt), v2 = phys_div(phys_sub(o2, F32(prev, 8)), dt);
    SU32(S, 0x38, prev); SF32(S, 0x40, v0); SF32(S, 0x44, v1); SF32(S, 0x48, v2);
    rx_world_node_get_world_position(node, S + 0x50);
    const float P0 = F32(S, 0x50), P1 = F32(S, 0x54), P2 = F32(S, 0x58);
    const float w0 = phys_sub(P0, o0), w1 = phys_sub(P1, o1), w2 = phys_sub(P2, o2);
    SU32(prev, 0, world_x87_pass(rx_fb(o0))); SU32(prev, 4, world_x87_pass(rx_fb(o1))); SU32(prev, 8, world_x87_pass(rx_fb(o2)));
    const float a0 = phys_div(phys_sub(phys_div(w0, dt), v0), dt), a1 = phys_div(phys_sub(phys_div(w1, dt), v1), dt), a2 = phys_div(phys_sub(phys_div(w2, dt), v2), dt);
    SF32(cur, 0, P0); SF32(cur, 4, P1); SF32(cur, 8, P2);
    SF32(S, 0x40, a0); SF32(S, 0x44, a1); SF32(S, 0x48, a2);
    WORLD_CALL_SP(4, rx_world_node_evaluate_world(node, 0));
    float V, A = newangle;
    {
        const rx_addr M = U32(U32(node, 0), 0x1c) + (uint32_t)U8(node, 0xb) * 0x40u;
        const float a_x = F32(R, 0x320), a_y = F32(R, 0x324), a_z = F32(R, 0x328);
        const float r0 = phys_add(phys_add(phys_mul(F32(M, 0x10), a_y), phys_mul(F32(M, 0), a_x)), phys_mul(F32(M, 0x20), a_z));
        const float r1 = phys_add(phys_add(phys_mul(F32(M, 4), a_x), phys_mul(F32(M, 0x14), a_y)), phys_mul(F32(M, 0x24), a_z));
        const float r2 = phys_add(phys_add(phys_mul(F32(M, 8), a_x), phys_mul(F32(M, 0x18), a_y)), phys_mul(F32(M, 0x28), a_z));
        const float c0 = phys_sub(phys_mul(a2, r1), phys_mul(a1, r2));
        const float c1 = phys_sub(phys_mul(r2, a0), phys_mul(a2, r0));
        const float c2 = phys_sub(phys_mul(a1, r0), phys_mul(r1, a0));
        SF32(S, 0x54, c1); SF32(S, 0x58, c2);
        const float py = cos_poly(y1, y2), pz = cos_poly(z1, z2);
        const float osc = phys_sub(negz, phys_mul(phys_sub(phys_mul(py, phys_mul(r, c)), phys_mul(pz, phys_mul(r, k))), damp));
        const float dot = phys_mul(phys_add(phys_add(phys_mul(F32(R, 0x314), c1), phys_mul(F32(R, 0x310), c0)), phys_mul(F32(R, 0x318), c2)), F32(R, 0x344));
        V = phys_sub(osc, dot);
    }
    {
        const float lo = F32(R, 0x330), hi = F32(R, 0x334), refl = F32(0, 0x00d92d50);
        if (lo > A) {
            A = lo;
            SF32(S, 0x14, A);
            if (0.0f > V) V = phys_sub(negz, phys_mul(refl, V));
            WORLD_CALL_SP(0, rx_vehicle_foley_end_stop(U32(e, 0x3fc)));
        }
        if (A > hi) {
            A = hi;
            SF32(S, 0x14, A);
            if (V > 0.0f) V = phys_sub(negz, phys_mul(refl, V));
        }
    }
    SF32(e, 0x250 + 4 * i, V); SF32(e, 0x230 + 4 * i, A);
    /* the hinge axis normalized (|axis|^2 summed y, x, then z); a zero axis leaves the accelerations in its place */
    {
        const float ax = F32(R, 0x310), ay = F32(R, 0x314), az = F32(R, 0x318);
        const float len2 = phys_add(phys_add(phys_mul(ay, ay), phys_mul(ax, ax)), phys_mul(az, az));
        float nx, ny, nz;
        SF32(S, 0x60, ax); SF32(S, 0x64, ay); SF32(S, 0x68, az); SF32(S, 0x3c, len2);
        if (len2 > 0.0f) {
            float len;
            SF32(S, -4, len2);
            WORLD_CALL_SP(4, len = phys_sqrtf(len2));
            const float inv = phys_div(F32(0, 0x00931f24), len);
            nx = phys_mul(ax, inv); ny = phys_mul(ay, inv); nz = phys_mul(az, inv);
            SF32(S, 0x40, nx); SF32(S, 0x44, ny);
        } else { nx = a0; ny = a1; nz = a2; }
        {
            const float h = phys_mul(A, F32(0, 0x009322cc));
            const float s = (h < 0.0f || h != h) ? F32(0, 0x009cd9e8) : F32(0, 0x00931f24);
            SF32(S, 0x2c, s);
            const float u = reduce_u(phys_mul(s, h)), u2 = phys_mul(u, u), u4 = phys_mul(u2, u2);
            const float W = phys_sub(negz, cos_poly(u2, u4));
            const float sv = phys_mul(phys_mul(sin_poly(u2, u4), u), s);
            const float b = phys_mul(nx, sv), cc = phys_mul(ny, sv), dd = phys_mul(nz, sv);
            const rx_addr q0 = e + 0x130 + 0x10 * i;
            const float qx = F32(q0, 0), qy = F32(q0, 4), qz = F32(q0, 8), qw = F32(q0, 0xc);
            SF32(S, 0x1c, qx); SF32(S, 0x18, qy); SF32(S, 0x14, qz);
            const double Wd = W, bd = b, cd = cc, dd_ = dd, qxd = qx, qyd = qy, qzd = qz, qwd = qw;
            const float ow = (float)dsub(dsub(dsub(dmul(Wd, qwd), dmul(qxd, bd)), dmul(qyd, cd)), dmul(qzd, dd_));
            const float ox = (float)dsub(dadd(dadd(dmul(qwd, bd), dmul(Wd, qxd)), dmul(qzd, cd)), dmul(qyd, dd_));
            const float oy = (float)dsub(dadd(dadd(dmul(qyd, Wd), dmul(cd, qwd)), dmul(dd_, qxd)), dmul(qzd, bd));
            const float oz = (float)dsub(dadd(dadd(dmul(qzd, Wd), dmul(dd_, qwd)), dmul(qyd, bd)), dmul(cd, qxd));
            SF32(S, 0x4c, ow); SF32(S, 0x40, ox); SF32(S, 0x44, oy); SF32(S, 0x48, oz);
            const rx_addr out = e + 0x1b0 + 0x10 * i;
            SF32(out, 0, ox); SF32(out, 4, oy); SF32(out, 8, oz); SF32(out, 0xc, ow);
            WORLD_CALL_SP(4, rx_node_set_local_rotation(U32(e, 0x370 + 4 * i), out));
        }
    }
done:
    world_frame_leave(&f);
}
RX_FUNCTION(0x0047aff0, rx_world_damage_part_spring)
