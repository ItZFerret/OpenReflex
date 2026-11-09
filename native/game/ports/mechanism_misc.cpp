// Small RavenPhysics helpers in the mechanism group's address range: axis rotation, 1D noise, segment / plane intersection and
// the averaging of contact frames.
#include "mechanism_kernels.h"

using namespace mech;
using namespace x86;
using reflex_port::OUT_EAX; using reflex_port::OUT_ECX; using reflex_port::OUT_EDX;

// 00852f40 (EAX = out vec3 or 0, ECX = v vec3, arg = unit axis a vec3, XMM1 lane 0 = angle): out = v rotated about a by the angle
// (Rodrigues: proj + (v - proj) cos + (a x (v - proj)) sin), with the exe's polynomial sin / cos of the angle wrapped to [-pi, pi]
// (the sign s = +1 / -1 from angle >= 0, the reduction through 1/2pi and cvttss2si). Returns EAX = 1, or 0 when out is null.
REFLEX_PORT_FN(0x00852f40, Vec3_RotateAboutAxis, "mechanism", OUT_EAX) {
    Guest g{m}; const uint32_t eax = m.r[0], ecx = m.r[1], edx = m.arg(0);
    if (eax == 0) return m.ret(0);
    float angle = g.xmm(1).f[0];
    float s = (angle < g.f32(0x0092e12c) || angle != angle) ? g.f32(0x009cd9e8) : g.f32(0x00931f24);   // comiss / jb: CF on < or unordered
    const uint32_t W0 = g.u32(0x00977498);
    const uint32_t W1 = g.u32(ecx + 0x4);
    const uint32_t W2 = g.u32(0x009774a0);
    const uint32_t W3 = g.u32(0x0097749c);
    const uint32_t W4 = g.u32(0x00935d50);
    const uint32_t W5 = g.u32(0x00935d48);
    const uint32_t W6 = g.u32(0x00935d44);
    const uint32_t W7 = g.u32(0x00935d4c);
    const uint32_t W8 = g.u32(0x00935d40);
    const uint32_t W9 = g.u32(0x00935d3c);
    const uint32_t W10 = g.u32(0x009774a8);
    const uint32_t W11 = g.u32(0x00935d60);
    const uint32_t W12 = g.u32(0x00935d5c);
    const uint32_t W13 = g.u32(0x00935d58);
    const uint32_t W14 = g.u32(0x00935d54);
    const uint32_t W15 = g.u32(0x00935d68);
    const uint32_t W16 = g.u32(0x00935d64);
    const uint32_t W17 = g.u32(ecx + 0x8);
    const uint32_t W18 = g.u32(edx + 0x8);
    const uint32_t W19 = g.u32(ecx);
    const uint32_t W20 = g.u32(edx);
    const uint32_t W21 = g.u32(edx + 0x4);
    const float t1 = add(add(mul(flt(W19), flt(W20)), mul(flt(W1), flt(W21))), mul(flt(W17), flt(W18)));   // v . a
    const float t2 = mul(flt(W18), t1);
    const float t3 = sub(flt(W17), t2);
    const float t4 = mul(flt(W21), t1);
    const float t5 = sub(flt(W1), t4);
    const float t6 = mul(s, angle);
    const float t7 = sub(flt(W0), sub(t6, mul(float(x86::cvtt(mul(t6, flt(W2)))), flt(W3))));
    const float t8 = mul(t7, t7);
    const float t9 = mul(t8, t8);
    const float t10 = mul(mul(add(mul(add(mul(add(mul(t8, flt(W15)), flt(W16)), t9), add(mul(t8, flt(W11)), flt(W12))), t9), add(mul(t8, flt(W13)), flt(W14))), t7), s);
    const float t11 = mul(flt(W20), t1);
    const float t12 = sub(flt(W19), t11);
    const float t13 = sub(flt(W10), add(mul(add(mul(add(mul(t8, flt(W4)), flt(W5)), t9), add(mul(t8, flt(W6)), flt(W7))), t9), add(mul(t8, flt(W8)), flt(W9))));
    g.f32(eax, add(add(mul(sub(mul(flt(W21), t3), mul(flt(W18), t5)), t10), mul(t12, t13)), t11));
    g.f32(eax + 0x4, add(add(mul(sub(mul(flt(W18), t12), mul(flt(W20), t3)), t10), mul(t5, t13)), t4));
    g.f32(eax + 0x8, add(add(mul(sub(mul(flt(W20), t5), mul(flt(W21), t12)), t10), mul(t3, t13)), t2));
    m.r[0] = 1; return m.ret(0);
}

// 00853180 (arg = x): 1D gradient noise: on first use (flag 009b71c8) the tables are built by 00853220; then with u = x + 4096,
// i = (int)u, f = u - i, gradients from the permutation 00a89198 into 00a87980: ((f - 1) g1 - f g0) * (3 - 2f) f f + f g0.
// Result in XMM0 lane 0 (upper lanes 0); XMM1..4, EAX, ECX, EDX as the original leaves them.
REFLEX_PORT_FN(0x00853180, Noise1D, "mechanism", OUT_EAX | OUT_ECX | OUT_EDX | reflex_port::OUT_XMM0 | reflex_port::OUT_XMM0 << 1 |
               reflex_port::OUT_XMM0 << 2 | reflex_port::OUT_XMM0 << 3 | reflex_port::OUT_XMM0 << 4) {
    Guest g{m};
    if (g.u32(0x009b71c8) != 0) { g.u32(0x009b71c8, 0); g.call(0x00853220); }
    const float z = flt(0);
    float x0 = add(f32_arg(m, 0), g.f32(0x0093594c));
    float x4 = g.f32(0x0092fe48);
    int32_t i = x86::cvtt(x0);
    uint32_t eax = uint32_t(i) & 0xff;
    float x1 = float(i);
    uint32_t ecx = g.u32(0x00a89198 + eax * 4);
    float x2 = g.f32(0x00a87980 + ecx * 4);
    x0 = sub(x0, x1);
    x1 = x0;
    x0 = sub(x0, g.f32(0x00931f24));
    eax = (eax + 1) & 0xff;
    uint32_t edx = g.u32(0x00a89198 + eax * 4);
    x0 = mul(x0, g.f32(0x00a87980 + edx * 4));
    float x3 = mul(x1, g.f32(0x0092e330));
    x4 = sub(x4, x3);
    x2 = mul(x2, x1);
    x0 = sub(x0, x2);
    x4 = mul(x4, x1);
    x4 = mul(x4, x1);
    x0 = mul(x0, x4);
    x0 = add(x0, x2);
    g.xmm(0, V4{{x0, z, z, z}}); g.xmm(1, V4{{x1, z, z, z}}); g.xmm(2, V4{{x2, z, z, z}}); g.xmm(3, V4{{x3, z, z, z}}); g.xmm(4, V4{{x4, z, z, z}});
    m.r[0] = eax; m.r[1] = ecx; m.r[2] = edx;
    return m.ret(0);
}

// 008523b0 (EDI = p0, EDX = p1, arg0 = q, arg1 = out, ESI = plane normal n): segment p0 -> p1 against the plane through q:
// d = p1 - p0 and w = q - p0 through 00401700 into the original's stack locals; den = n . d (0 -> no hit), t = (n . w) / den,
// hit when 0 <= t <= 1: out = p0 + t d, EAX = 1; else EAX = 0. ECX = p0, EDX as left by the calls.
REFLEX_PORT_FN(0x008523b0, Segment_PlaneIntersect, "mechanism", OUT_EAX | OUT_ECX | OUT_EDX) {
    Guest g{m}; const uint32_t ESP0 = m.r[4], p0 = m.r[7], p1 = m.r[2], n = m.r[6], q = m.arg(0), out = m.arg(1);
    const uint32_t A = (ESP0 - 4) & ~15u, L1 = A - 0x20, L2 = A - 0x10;
    uint32_t result = 0;
    m.r[4] = A - 0x40;
    callr(g, 0x00401700, {L1}, In().ECX(p0).EDX(p1));
    float ny = g.f32(n + 4), nx = g.f32(n), nz = g.f32(n + 8);
    float den = add(add(mul(nx, g.f32(L1)), mul(ny, g.f32(L1 + 4))), mul(nz, g.f32(L1 + 8)));
    float zero = g.f32(0x0092e12c);
    bool unordered = den != den || zero != zero;
    if (unordered || den != zero) {
        m.r[4] = A - 0x40;
        callr(g, 0x00401700, {L2}, In().ECX(p0).EDX(q));
        float t = add(add(mul(g.f32(L2 + 8), nz), mul(g.f32(L2 + 4), ny)), mul(g.f32(L2), nx));
        t = div(t, den);
        float one = g.f32(0x00931f24);
        bool below = t < zero || t != t || zero != zero, above = one < t || t != t || one != one;
        if (!below && !above) {
            float dx = g.f32(L1), dy = g.f32(L1 + 4), dz = g.f32(L1 + 8);
            float y = mul(dy, t), x = mul(dx, t), zz = mul(dz, t);
            float ox = add(g.f32(p0), x), oy = add(g.f32(p0 + 4), y), oz = add(g.f32(p0 + 8), zz);
            g.f32(out, ox); g.f32(out + 4, oy); g.f32(out + 8, oz);
            result = 1;
        }
        m.r[2] = q;
    } else {
        m.r[2] = p1;
    }
    m.r[4] = ESP0;
    m.r[0] = result; m.r[1] = p0;
    return m.ret(0);
}

// 0085dcf0 (ESI = contact set: count at +0, entries of 3 vec4 from +10, arg = out of 3 vec4): out = the average of the entries
// (sums in stored order, then divided by the count), out+10 renormalized (packed sqrt / div, zero -> 0, infinite -> the NaN vec4).
REFLEX_PORT_FN(0x0085dcf0, Contacts_Average, "mechanism", OUT_EAX) {
    Guest g{m}; const uint32_t S = m.r[6], out = m.arg(0);
    st(g, out, ld(g, 0x00d61670));
    st(g, out + 0x20, ld(g, 0x00d61670));
    st(g, out + 0x10, ld(g, 0x00d61670));
    V4 cnt = V4::splat(float(g.i32(S)));
    uint32_t i = 0;
    if (g.i32(S) > 0) {
        uint32_t c = S + 0x20;
        do {
            V4 a = ld(g, c + 0x10), b = ld(g, out + 0x20); st(g, out + 0x20, add(a, b));
            a = ld(g, c - 0x10); b = ld(g, out); st(g, out, add(a, b));
            a = ld(g, c); b = ld(g, out + 0x10);
            i++;
            st(g, out + 0x10, add(a, b));
            c += 0x30;
        } while (int32_t(i) < g.i32(S));
        m.r[1] = c;
    } else {
        m.r[1] = g.u32(S);
    }
    st(g, out + 0x20, div(ld(g, out + 0x20), cnt));
    V4 x1 = div(ld(g, out), cnt);
    V4 x0 = ld(g, out + 0x10);
    st(g, out, x1);
    V4 inf = ld(g, 0x0092dc20);
    float n2 = dot3(x0, x0);
    float nn = x86::sqrt(n2);
    V4 nan = ld(g, 0x0092dc30), r;
    for (int k = 0; k < 4; k++) {
        uint32_t d = bits(div(x0.f[k], nn));
        bool finite = cmp_pred(n2, inf.f[k], 4), nonzero = cmp_pred(nn, flt(0), 4);
        r.f[k] = flt(((nonzero ? d : 0u) & (finite ? ~0u : 0u)) | (finite ? 0u : bits(nan.f[k])));
    }
    st(g, out + 0x10, r);
    m.r[0] = out; m.r[2] = i; return m.ret(4);
}
