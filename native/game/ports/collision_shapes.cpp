// RavenPhysics collision shapes (0x0084xxxx): bounds, support queries and the composite / static-composite children.
#include "collision_shapes.h"

using namespace collision;
using reflex_port::OUT_EAX;

namespace {


// The exe's quaternion product (SIMD form): l.x * r.wzyx * kQx, l.y * r.zwxy * kQy, l.z * r.yxwz * kQz, l.w * r, summed as
// (z + y) + (w + x). Operand order as the original (NaN propagation).
struct QuatK { X qx, qy, qz; };
inline X quat_mul(const QuatK& k, const X& l, const X& r) {
    X s1 = shuf(r, 0x1b), s2 = shuf(s1, 0xb1), s3 = shuf(s2, 0x1b);
    X x = mulps(mulps(lane(l, 0), s1), k.qx);
    X y = mulps(mulps(lane(l, 1), s2), k.qy);
    X z = mulps(mulps(s3, lane(l, 2)), k.qz);
    return addps(addps(z, y), addps(mulps(lane(l, 3), r), x));
}
// The same product with the operand order of 00848650: z = (r.yxwz * l.z) * kQz, y = kQy * (l.y * r.zwxy), w = l.w * r,
// x = kQx * (l.x * r.wzyx).
inline X quat_mul_kl(const QuatK& k, const X& l, const X& r) {
    X s1 = shuf(r, 0x1b), s2 = shuf(s1, 0xb1), s3 = shuf(s2, 0x1b);
    X x = mulps(k.qx, mulps(lane(l, 0), s1));
    X y = mulps(k.qy, mulps(lane(l, 1), s2));
    X z = mulps(mulps(s3, lane(l, 2)), k.qz);
    return addps(addps(z, y), addps(mulps(lane(l, 3), r), x));
}
// |x| as max(0 - x, x)
inline X absmax(const X& x) { return maxps(subps(X::zero(), x), x); }

// The bound of a rotated shape: four axis vectors a[i] (already absolute) rotated q * a * conj(q) (the conjugate recomputed per
// vector, as the original), the lane-wise max of their absolute values. Then max = position + bound, min = position - bound
// (the position read again after the first store).
void rotated_bounds(Guest& g, const X a[4], uint32_t qp, uint32_t pp, uint32_t out_min, uint32_t out_max) {
    const QuatK k{ld(g, kQx), ld(g, kQy), ld(g, kQz)};
    const X conj = ld(g, kConjSigns), q = ld(g, qp);
    X r[4];
    for (int i = 0; i < 4; i++) r[i] = quat_mul(k, q, quat_mul(k, a[i], mulps(q, conj)));
    X b = maxps(maxps(absmax(r[0]), absmax(r[1])), maxps(absmax(r[2]), absmax(r[3])));
    st(g, out_max, addps(ld(g, pp), b));
    st(g, out_min, subps(ld(g, pp), b));
}

}  // namespace

// 008455f0 CollisionShapeBox::vf2 (thiscall: quaternion*, position*, out min*, out max*; RET 0x10): the world AABB of the box
// (half extents this+0x20): the corner sign patterns (+++, -++, +-+, ++-, signs 1.0 and the runtime -1.0 at 0x9cd9e8) times
// the half extents, absolute, through rotated_bounds.
REFLEX_PORT_FN(0x008455f0, CollisionShapeBox_vf2, "collision", 0) {
    Guest g{m};
    const uint32_t self = m.r[1], qp = m.arg(0), pp = m.arg(1), out_min = m.arg(2), out_max = m.arg(3);
    const X absmask = ld(g, kAbsMask), ext = ld(g, self + 0x20);
    const uint32_t one = g.u32(kOne), m1 = g.u32(kMinusOne);
    const X signs[4] = {{{one, one, one, one}}, {{m1, one, one, one}}, {{one, m1, one, one}}, {{one, one, m1, m1}}};
    X a[4];
    for (int i = 0; i < 4; i++) a[i] = andps(mulps(signs[i], ext), absmask);
    rotated_bounds(g, a, qp, pp, out_min, out_max);
    m.r[0] = out_min; m.r[1] = pp; m.r[2] = out_max;
    return m.ret(0x10);
}

// 00846700 CollisionShapeDynamicCylinder::vf2 (as the box's): radius r = this+0x20, height h = this+0x24; axis vectors
// (r, h*0.5, r, r), (-0.0 - r, h*0.5, r, r), (r, h*-0.5, r, r), (r, h*0.5, -0.0 - r, -0.0 - r), absolute.
REFLEX_PORT_FN(0x00846700, CollisionShapeDynamicCylinder_vf2, "collision", 0) {
    Guest g{m};
    const uint32_t self = m.r[1], qp = m.arg(0), pp = m.arg(1), out_min = m.arg(2), out_max = m.arg(3);
    const X absmask = ld(g, kAbsMask);
    const float r = g.f32(self + 0x20), h = g.f32(self + 0x24), neg0 = g.f32(kNegZero);
    const uint32_t rb = g.u32(self + 0x20), nr = x86::bits(x86::sub(neg0, r));
    const uint32_t hp = x86::bits(x86::mul(h, g.f32(kHalf))), hn = x86::bits(x86::mul(h, g.f32(kMinusHalf)));
    const X v[4] = {{{rb, hp, rb, rb}}, {{nr, hp, rb, rb}}, {{rb, hn, rb, rb}}, {{rb, hp, nr, nr}}};
    X a[4];
    for (int i = 0; i < 4; i++) a[i] = andps(v[i], absmask);
    rotated_bounds(g, a, qp, pp, out_min, out_max);
    m.r[0] = out_min; m.r[1] = pp; m.r[2] = out_max;
    return m.ret(0x10);
}

namespace collision {

void composite_child_rotation(Guest& g, uint32_t comp, uint32_t i, uint32_t q, uint32_t out) {
    vector_check(g, comp + 0x44, i, 4);
    const QuatK k{ld(g, kQx), ld(g, kQy), ld(g, kQz)};
    st(g, out, quat_mul_kl(k, ld(g, q), ld(g, g.u32(comp + 0x44) + i * 16)));
}

void composite_child_position(Guest& g, uint32_t comp, uint32_t i, uint32_t q, uint32_t pos, uint32_t out) {
    vector_check(g, comp + 0x5c, i, 4);
    const QuatK k{ld(g, kQx), ld(g, kQy), ld(g, kQz)};
    const X qv = ld(g, q);
    const X v = andps(ld(g, g.u32(comp + 0x5c) + i * 16), ld(g, kAbsMask));
    const X r = quat_mul(k, qv, quat_mul(k, v, mulps(ld(g, kConjSigns), qv)));
    st(g, out, addps(r, ld(g, pos)));
}

}  // namespace collision

// 00848650 (custom: EDI composite, ECX child index; stack: quaternion*, out*; RET 8): the child's world rotation.
REFLEX_PORT_FN(0x00848650, CompositeChildRotation, "collision", 0) {
    Guest g{m};
    const uint32_t comp = m.r[7], i = m.r[1], q = m.arg(0), out = m.arg(1);
    composite_child_rotation(g, comp, i, q, out);
    m.r[0] = uint32_t((g.i32(comp + 0x48) - g.i32(comp + 0x44)) >> 4); m.r[1] = q;
    return m.ret(8);
}

// 008486f0 (custom: EDI composite, ECX child index; stack: quaternion*, position*, out*; RET 0xc): the child's world
// position (its offset, w masked, rotated, plus the position).
REFLEX_PORT_FN(0x008486f0, CompositeChildPosition, "collision", 0) {
    Guest g{m};
    const uint32_t comp = m.r[7], i = m.r[1], q = m.arg(0), pos = m.arg(1), out = m.arg(2);
    composite_child_position(g, comp, i, q, pos, out);
    m.r[0] = uint32_t((g.i32(comp + 0x60) - g.i32(comp + 0x5c)) >> 4); m.r[1] = q; m.r[2] = pos;
    return m.ret(0xc);
}

// 00845480 CollisionShapeCircle::vf2 (thiscall: quaternion*, position*, out min*, out max*; RET 0x10): min = position - r,
// max = position + r (the radius vector this+0x20, position and radius read again for the max).
REFLEX_PORT_FN(0x00845480, CollisionShapeCircle_vf2, "collision", 0) {
    Guest g{m};
    const uint32_t self = m.r[1], pp = m.arg(1), out_min = m.arg(2), out_max = m.arg(3);
    st(g, out_min, subps(ld(g, pp), ld(g, self + 0x20)));
    st(g, out_max, addps(ld(g, pp), ld(g, self + 0x20)));
    m.r[0] = out_min; m.r[2] = out_max;
    return m.ret(0x10);
}

// 00845a70 CollisionShapeBox::vf3: a tail call of the object's vf2 with the same arguments (called here, then RET 0x10).
REFLEX_PORT_FN(0x00845a70, CollisionShapeBox_vf3, "collision", OUT_EAX) {
    Guest g{m};
    const uint32_t self = m.r[1], a0 = m.arg(0), a1 = m.arg(1), a2 = m.arg(2), a3 = m.arg(3);
    g.call(g.u32(g.u32(self) + 8), {a0, a1, a2, a3}, self);
    return m.ret(0x10);
}

// 00848340 CollisionShapeComposite::vf3 (thiscall: quaternion*, position*, out min*, out max*; RET 0x10): the union of the
// children's world bounds (their vf3 at each child's world rotation q * r and position q * offset * conj(q) + position),
// started at (FLT_MAX, -FLT_MAX); nothing written without children. The original's prefetch loop only runs bounds checks
// (its index is not advanced in the main loop); they are kept.
REFLEX_PORT_FN(0x00848340, CollisionShapeComposite_vf3, "collision", OUT_EAX) {
    Guest g{m};
    const uint32_t self = m.r[1], qp = m.arg(0), pp = m.arg(1), out_min = m.arg(2), out_max = m.arg(3);
    const uint32_t n = uint32_t(int32_t(g.u32(self + 0x30) - g.u32(self + 0x2c)) >> 2);
    m.r[0] = out_min;
    if (n == 0) return m.ret(0x10);
    st(g, out_min, X::splat(g.u32(kFltMax)));
    st(g, out_max, X::splat(g.u32(kFltMaxNeg)));
    uint32_t pf = 0;
    for (; pf < n; ) { vector_check(g, self + 0x2c, pf, 2); pf++; if (pf >= 8) break; }
    const QuatK k{ld(g, kQx), ld(g, kQy), ld(g, kQz)};
    Frame f(g, 0x40);
    const uint32_t pos = f.at(0x00), mn = f.at(0x10), rot = f.at(0x20), mx = f.at(0x30);
    for (uint32_t i = 0; i < n; i++) {
        if (pf < n) vector_check(g, self + 0x2c, pf, 2);
        vector_check(g, self + 0x5c, i, 4);
        const X q = ld(g, qp);
        const X v = andps(ld(g, g.u32(self + 0x5c) + i * 16), ld(g, kAbsMask));
        st(g, pos, addps(quat_mul(k, q, quat_mul(k, v, mulps(q, ld(g, kConjSigns)))), ld(g, pp)));
        vector_check(g, self + 0x44, i, 4);
        st(g, rot, quat_mul(k, ld(g, qp), ld(g, g.u32(self + 0x44) + i * 16)));
        vector_check(g, self + 0x2c, i, 2);
        const uint32_t child = g.u32(g.u32(self + 0x2c) + i * 4);
        g.call(g.u32(g.u32(child) + 0xc), {rot, pos, mn, mx}, child);
        st(g, out_min, minps(ld(g, out_min), ld(g, mn)));
        st(g, out_max, maxps(ld(g, out_max), ld(g, mx)));
    }
    f.end();
    m.r[0] = out_max;
    return m.ret(0x10);
}

// 00848800 CompositeSetChildPosition (custom: ESI composite, ECX world position*, EDX body position*; stack: child index,
// body quaternion*; RET 8): child offset (composite+0x5c) = conj(q) * ((world - body) & xyz) * q.
REFLEX_PORT_FN(0x00848800, CompositeSetChildPosition, "collision", OUT_EAX) {
    Guest g{m};
    const uint32_t comp = m.r[6], world = m.r[1], body = m.r[2], i = m.arg(0), qp = m.arg(1);
    const QuatK k{ld(g, kQx), ld(g, kQy), ld(g, kQz)};
    const X q = ld(g, qp);
    const X v = andps(subps(ld(g, world), ld(g, body)), ld(g, kAbsMask));
    const X r1 = quat_mul(k, v, q);
    const X r = quat_mul(k, mulps(ld(g, kConjSigns), q), r1);
    vector_check(g, comp + 0x5c, i, 4);
    st(g, g.u32(comp + 0x5c) + i * 16, r);
    m.r[0] = uint32_t((g.i32(comp + 0x60) - g.i32(comp + 0x5c)) >> 4);
    return m.ret(8);
}

// 00848910 CompositeSetChildRotation (custom: ESI composite, EDX child index, ECX world rotation*; stack: body quaternion*;
// RET 4): child rotation (composite+0x44) = conj(q) * world rotation.
REFLEX_PORT_FN(0x00848910, CompositeSetChildRotation, "collision", OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX) {
    Guest g{m};
    const uint32_t comp = m.r[6], rot = m.r[1], i = m.r[2], qp = m.arg(0);
    const QuatK k{ld(g, kQx), ld(g, kQy), ld(g, kQz)};
    const X r = quat_mul_kl(k, mulps(ld(g, qp), ld(g, kConjSigns)), ld(g, rot));
    vector_check(g, comp + 0x44, i, 4);
    st(g, g.u32(comp + 0x44) + i * 16, r);
    m.r[0] = qp; m.r[2] = uint32_t((g.i32(comp + 0x48) - g.i32(comp + 0x44)) >> 4);
    return m.ret(4);
}

// 00845a80 CollisionShapeBox_Corners (thiscall: out[8] vec4*; RET 4): the eight corners of the box in local space, the half
// extents h (this+0x20, read again for each) times the sign patterns A = (-1,1,1,1), B = (1,-1,1,1), C = (1,1,-1,-1)
// (-1 the runtime 0x9cd9e8): h, C h, B h, B h C, A h, A h C, A h B, A h B C.
REFLEX_PORT_FN(0x00845a80, CollisionShapeBox_Corners, "collision", 0) {
    Guest g{m};
    const uint32_t self = m.r[1], out = m.arg(0);
    const uint32_t one = g.u32(kOne), m1 = g.u32(kMinusOne);
    const X A = {{m1, one, one, one}}, Bv = {{one, m1, one, one}}, C = {{one, one, m1, m1}};
    auto h = [&] { return ld(g, self + 0x20); };
    st(g, out, h());
    st(g, out + 0x10, mulps(C, h()));
    st(g, out + 0x20, mulps(Bv, h()));
    st(g, out + 0x30, mulps(mulps(Bv, h()), C));
    st(g, out + 0x40, mulps(A, h()));
    st(g, out + 0x50, mulps(mulps(A, h()), C));
    st(g, out + 0x60, mulps(mulps(A, h()), Bv));
    const X hl = h(), last = mulps(mulps(mulps(A, hl), Bv), C);
    st(g, out + 0x70, last);
    xw(g, 0, C); xw(g, 1, last); xw(g, 2, hl); xw(g, 3, Bv); xw(g, 4, hl);
    m.r[0] = out;
    return m.ret(4);
}

// 00845f20 CollisionShapeBox_ContainsPoint (custom: ECX point*; stack: box*; RET 4): 1 if -h <= p <= h on x, y, z (h the
// half extents box+0x20; cmpleps lanes 0..2 summed), else 0.
REFLEX_PORT_FN(0x00845f20, CollisionShapeBox_ContainsPoint, "collision", OUT_EAX) {
    Guest g{m};
    const X h = ld(g, m.arg(0) + 0x20), p = ld(g, m.r[1]);
    const X a = cmpleps(p, h);
    uint32_t s = a.u[2] + a.u[1] + a.u[0], r = 0;
    if (s == 0xfffffffdu) {
        const X b = cmpleps(subps(X::zero(), h), p);
        if (b.u[2] + b.u[1] + b.u[0] == s) r = 1;
        m.r[1] = b.u[1]; m.r[2] = b.u[2] + b.u[1] + b.u[0];
    } else {
        m.r[2] = a.u[1];
    }
    m.r[0] = r;
    return m.ret(4);
}

// 008439f0 XenonCore::vf2: eax = this+0x34.
REFLEX_PORT_FN(0x008439f0, XenonCore_vf2, "collision", OUT_EAX) {
    Guest g{m};
    m.r[0] = g.u32(m.r[1] + 0x34);
    return m.ret(0);
}

// 008454c0 (custom: EAX shape): xmm0 = the shape's vector +0x20 (radius / half extents).
REFLEX_PORT_FN(0x008454c0, CollisionShape_Vec20, "collision", reflex_port::OUT_XMM0) {
    Guest g{m};
    xw(g, 0, ld(g, m.r[0] + 0x20));
    return m.ret(0);
}

// 00843fc0 CollisionShapeSphere::vf4 (thiscall: scale; RET 4): saves the radius vector (+0x20 to +0x30), scales it.
REFLEX_PORT_FN(0x00843fc0, CollisionShapeSphere_vf4, "collision", OUT_EAX) {
    Guest g{m};
    const uint32_t self = m.r[1];
    st(g, self + 0x30, ld(g, self + 0x20));
    const X s = X::splat(g.u32(m.r[4] + 4)), r = ld(g, self + 0x20), v = mulps(s, r);
    st(g, self + 0x20, v);
    xw(g, 0, v); xw(g, 1, r);
    m.r[0] = self;
    return m.ret(4);
}

// 00843ff0 CollisionShapeSphere::vf5: restores the radius vector (+0x30 to +0x20).
REFLEX_PORT_FN(0x00843ff0, CollisionShapeSphere_vf5, "collision", OUT_EAX) {
    Guest g{m};
    const uint32_t self = m.r[1];
    const X v = ld(g, self + 0x30);
    st(g, self + 0x20, v);
    xw(g, 0, v);
    m.r[0] = self;
    return m.ret(0);
}

// 00844050 CollisionShapeDynamicCylinder::vf4 (thiscall: scale; RET 4): saves radius / height (+0x20 / +0x24 to +0x28 /
// +0x2c), scales them.
REFLEX_PORT_FN(0x00844050, CollisionShapeDynamicCylinder_vf4, "collision", OUT_EAX) {
    Guest g{m};
    const uint32_t self = m.r[1];
    X x0 = ld_ss(g, self + 0x20), x1 = ld_ss(g, self + 0x24);
    const X x2 = ld_ss(g, m.r[4] + 4);
    g.u32(self + 0x28, x0.u[0]); g.u32(self + 0x2c, x1.u[0]);
    x0 = mulss(x0, x2); x1 = mulss(x1, x2);
    g.u32(self + 0x20, x0.u[0]); g.u32(self + 0x24, x1.u[0]);
    xw(g, 0, x0); xw(g, 1, x1); xw(g, 2, x2);
    return m.ret(4);
}

// 00844080 CollisionShapeDynamicCylinder::vf5: restores radius / height (fld / fstp copies: a signaling NaN comes back quiet).
REFLEX_PORT_FN(0x00844080, CollisionShapeDynamicCylinder_vf5, "collision", OUT_EAX) {
    Guest g{m};
    const uint32_t self = m.r[1];
    g.u32(self + 0x20, fld_fstp(g.u32(self + 0x28)));
    g.u32(self + 0x24, fld_fstp(g.u32(self + 0x2c)));
    return m.ret(0);
}

// 00846bb0 CollisionShapeCylinder_ContainsPoint (custom: ECX point*; stack: cylinder*; RET 4): 1 if x^2 + z^2 < r^2 and
// -h/2 < y < h/2 (radius +0x20, height +0x24; cmpltps, lanes 0..2 summed), else 0.
REFLEX_PORT_FN(0x00846bb0, CollisionShapeCylinder_ContainsPoint, "collision", OUT_EAX) {
    Guest g{m};
    const uint32_t cyl = m.arg(0);
    const X p = ld(g, m.r[1]);
    const X r = ld_ss(g, cyl + 0x20), hh = X::splat(mulss(ld_ss(g, cyl + 0x24), ld_ss(g, kHalf)).u[0]);
    const X z = lane(p, 2), x = lane(p, 0);
    const X d2 = addps(mulps(z, z), mulps(x, x));
    const X r2 = X::splat(mulss(r, r).u[0]), y = lane(p, 1);
    uint32_t res = 0;
    const X a = cmpltps(d2, r2);
    const uint32_t s = a.u[2] + a.u[1] + a.u[0];
    m.r[2] = a.u[1];
    if (s == 0xfffffffdu) {
        const X b = cmpltps(y, hh);
        const uint32_t sb = b.u[2] + b.u[1] + b.u[0];
        m.r[1] = b.u[1]; m.r[2] = sb;
        if (sb == s) {
            const X c = cmpltps(subps(X::zero(), hh), y);
            const uint32_t sc = c.u[2] + c.u[1] + c.u[0];
            m.r[1] = sc;
            if (sc == 0xfffffffdu) res = 1;
        }
    }
    m.r[0] = res;
    return m.ret(4);
}

// 008454d0 (custom: EDX circle-like shape, ECX direction out*; stack: v* in / out; RET 4): v = r * v / (|v| + 1e-6) (r the
// shape's vector +0x20, |v| over x, y, z), stored back; the direction v / |v| goes out, (NaN) where |v| is infinite or zero.
REFLEX_PORT_FN(0x008454d0, CollisionShape_ScaleToRadius, "collision", 0) {
    Guest g{m};
    const uint32_t shape = m.r[2], out = m.r[1], vp = m.arg(0);
    X x1 = mulps(ld(g, vp), ld(g, vp));
    X x0 = shufps(x1, x1, 0x66);
    x1 = addss(x1, x0);
    x0 = shuf(x0, 0x55);
    x1 = addss(x1, x0);
    x1 = addps(sqrtps(shuf(x1, 0)), X::splat(g.u32(kEpsilon6)));
    X s = mulps(divps(ld(g, shape + 0x20), x1), ld(g, vp));
    x1 = mulps(s, s);
    X x2 = shufps(x1, x1, 0x99);
    x1 = addss(x1, x2);
    st(g, vp, s);
    X x3 = ld(g, kInf4);
    x2 = shuf(x2, 0x55);
    x1 = shuf(addss(x1, x2), 0);
    x2 = sqrtps(x1);
    x0 = divps(s, x2);
    x1 = cmpneqps(x1, x3);
    x3 = X::zero();
    x2 = cmpneqps(x2, x3);
    x0 = andps(andps(x0, x2), x1);
    x2 = ld(g, kQNaN4);
    x1 = andnps(x1, x2);
    x0 = orps(x0, x1);
    st(g, out, x0);
    xw(g, 0, x0); xw(g, 1, x1); xw(g, 2, x2); xw(g, 3, x3);
    m.r[0] = vp;
    return m.ret(4);
}
