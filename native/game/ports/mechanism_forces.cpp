// RavenPhysics mechanism (mechanism group): applying forces at points and rigid motions of the whole mechanism.
#include "mechanism_kernels.h"

using namespace mech;
using namespace x86;
using reflex_port::OUT_EAX; using reflex_port::OUT_ECX;

namespace {
// The exe's quaternion product a (x) b in its operation order: bcast(a.x) * b.wzyx * S1 (0093a1f0), bcast(a.y) * b.zwxy * S2
// (0093a200), b.yxwz * bcast(a.z) * S3 (0093a210), bcast(a.w) * b; summed (z + y) + (w + x). Constants read from guest memory.
V4 quat_mul(Guest& g, const V4& a, const V4& b) {
    V4 rev = perm(b, 3, 2, 1, 0);
    V4 tx = mul(mul(bcast(a, 0), rev), ld(g, 0x0093a1f0));
    V4 r2 = perm(rev, 1, 0, 3, 2);
    V4 ty = mul(mul(bcast(a, 1), r2), ld(g, 0x0093a200));
    V4 r3 = perm(r2, 3, 2, 1, 0);
    V4 tz = mul(mul(r3, bcast(a, 2)), ld(g, 0x0093a210));
    tz = add(tz, ty);
    V4 tw = add(mul(bcast(a, 3), b), tx);
    return add(tz, tw);
}
}

// 0085c770 (ECX = force vec4, EDX = M, ESI = point vec4, arg = node index): apply a force at a point of a node: linear applied
// force (+70) += F; angular applied (+6b0) += (r x F') & mask, where F' is the masked force rotated into the body frame by the
// node's orientation (conj(q) (x) (F (x) q) in the exe's product order, 0093a220 the conjugation signs). EAX = M + i * a0.
// (The original's tail writes the force to its dead stack when the debug flag 00da68bc is set: nothing live.)
REFLEX_PORT_FN(0x0085c770, Mechanism_AddForceAtPoint, "mechanism", OUT_EAX | OUT_ECX) {
    Guest g{m}; const uint32_t fp = m.r[1], M = m.r[2], rp = m.r[6];
    const uint32_t p = m.arg(0) * 0xa0 + M;
    V4 F = ld(g, fp), mask = ld(g, 0x0092db80), q = ld(g, p + 0x660);
    V4 Fm = andps(F, mask);
    V4 t = quat_mul(g, Fm, q);
    V4 qc = mul(ld(g, 0x0093a220), q);
    V4 u = quat_mul(g, qc, t);
    V4 r = ld(g, rp);
    V4 r1 = perm(r, 1, 2, 0, 3);                         // shufps c9
    V4 u1 = perm(u, 2, 0, 1, 3);                         // shufps d2
    V4 u2 = perm(u1, 2, 0, 1, 3);
    V4 c1 = mul(u1, r1);
    V4 r2 = perm(r1, 1, 2, 0, 3);
    V4 lin = ld(g, p + 0x70);
    st(g, p + 0x70, add(lin, F));
    V4 ang = ld(g, p + 0x6b0);
    V4 cr = andps(sub(c1, mul(u2, r2)), mask);
    st(g, p + 0x6b0, add(ang, cr));
    m.r[0] = p; return m.ret(4);
}

// 0085c530 (EDX = M, ESI = rotation quaternion R): rotate the whole mechanism about the root's position: every node's
// orientation q = R (x) q style product (exe order), position = root + rotate(R, (position - root) & mask) (the root position
// is re-read every node, so the root itself is rotated in place first). ECX = count / EAX = last pointer.
REFLEX_PORT_FN(0x0085c530, Mechanism_Rotate, "mechanism", OUT_EAX | OUT_ECX) {
    Guest g{m}; const uint32_t M = m.r[2], esi = m.r[6], edi = M + 0x20;
    uint32_t i = 0, eax = m.r[0];
    if (g.i32(M + 0x1c) > 0) {
        eax = edi;
        do {
            const V4 L0 = ld(g, eax + 0x640);
            const V4 L1 = ld(g, esi);
            const V4 L2 = ld(g, edi);
            const V4 L3 = ld(g, 0x0093a210);
            const V4 L4 = ld(g, 0x0093a200);
            const V4 L5 = ld(g, 0x0093a1f0);
            st(g, eax + 0x640, add(add(mul(mul(perm(L1, 1, 0, 3, 2), bcast(L0, 2)), L3), mul(L4, mul(bcast(L0, 1), perm(L1, 2, 3, 0, 1)))),
                                   add(mul(bcast(L0, 3), L1), mul(L5, mul(bcast(L0, 0), perm(L1, 3, 2, 1, 0))))));
            const V4 L6 = ld(g, esi);
            const V4 L7 = ld(g, eax);
            const V4 L8 = ld(g, 0x0092db80);
            const V4 L9 = ld(g, 0x0093a220);
            const V4 L10 = ld(g, 0x0093a1f0);
            const V4 L11 = ld(g, 0x0093a200);
            const V4 L12 = ld(g, 0x0093a210);
            const V4 L13 = ld(g, 0x0093a210);
            const V4 L14 = ld(g, 0x0093a1f0);
            const V4 v1 = mul(L6, L9);
            const float t1 = flt(bits(sub(L7.f[2], L2.f[2])) & bits(L8.f[2]));
            const float t2 = flt(bits(sub(L7.f[1], L2.f[1])) & bits(L8.f[1]));
            const float t3 = flt(bits(sub(L7.f[3], L2.f[3])) & bits(L8.f[3]));
            const float t4 = flt(bits(sub(L7.f[0], L2.f[0])) & bits(L8.f[0]));
            const V4 v2 = add(add(mul(mul(v1, V4::splat(t1)), perm(L12, 1, 0, 3, 2)), mul(mul(V4::splat(t2), perm(v1, 3, 2, 1, 0)), perm(L11, 1, 0, 3, 2))),
                              add(mul(V4::splat(t3), perm(v1, 1, 0, 3, 2)), mul(mul(V4::splat(t4), perm(v1, 2, 3, 0, 1)), perm(L10, 1, 0, 3, 2))));
            st(g, eax, add(add(add(mul(mul(v2, bcast(L6, 2)), L13), mul(mul(bcast(L6, 1), perm(v2, 3, 2, 1, 0)), L11)),
                               add(mul(bcast(L6, 3), perm(v2, 1, 0, 3, 2)), mul(mul(bcast(L6, 0), perm(v2, 2, 3, 0, 1)), L14))), L2));
            i++; eax += 0xa0;
        } while (int32_t(i) < g.i32(M + 0x1c));
    }
    m.r[1] = i; m.r[0] = eax; return m.ret(0);
}
