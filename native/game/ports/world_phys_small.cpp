// World group: small accessors of the physics world's generic objects and handle lists (0x00767f80..0x007686ff).
#include "../port.h"
#include "world_util.h"

using namespace world;
using reflex_port::OUT_EAX;
using reflex_port::OUT_XMM0;
using reflex_port::OUT_XMM_ALL;

namespace {
constexpr uint32_t kAbort = 0x00916224;
// 0042bd90 (math group): EAX = handle body {+4 world, +8 index} -> the generic object world +0x1960 + 0xe0 * index.
inline uint32_t object_of(Frame& f, uint32_t body) { return f.call(0x0042bd90, {}, -1, body); }
}  // namespace

// 00767f80: xmm0 = (1 / [EAX], 0, 0, 0) (movss / divss), plain ret.
REFLEX_PORT_FN(0x00767f80, Recip_Eax, "world", OUT_XMM0) {
    Guest g{m};
    xw(g, 0, X{{x86::bits(x86::div(g.f32(0x00931f24), g.f32(m.r[0]))), 0, 0, 0}});
    return m.ret(0);
}

// 00767f90: [arg] = (1 / [ECX+0x10], 1 / [ECX+0x14], 1 / [ECX+0x18], 1 / [ECX+0x18]) (the inverse of a scale vector), ret 4;
// xmm0..2 end as the original's shuffles leave them, EAX = out.
REFLEX_PORT_FN(0x00767f90, Vec3_ReciprocalOf, "world", OUT_EAX | OUT_XMM_ALL) {
    Guest g{m};
    const uint32_t s = m.r[1], out = m.arg(0);
    const float one = g.f32(0x00931f24);
    const uint32_t a = x86::bits(x86::div(one, g.f32(s + 0x14)));
    const uint32_t b = x86::bits(x86::div(one, g.f32(s + 0x18)));
    const uint32_t c = x86::bits(x86::div(one, g.f32(s + 0x10)));
    const X x0{{c, a, b, b}}, x1{{c, b, b, b}}, x2{{c, c, c, c}};
    st(g, out, x0);
    xw(g, 0, x0); xw(g, 1, x1); xw(g, 2, x2);
    return m.ret(4, out);   // EAX = out (callers use it)
}

#ifdef WORLD_HAND_READABLE   // readable port; the literal one in world_phys_lit4.cpp (exact scratch registers for literal callers) is used
// 00768460: sets a generic object's pose through its handle (stdcall handle, rot, pos, vel, angvel; ret 0x14): +0x40 / +0x50 /
// +0x60 / +0x70 from the four vectors, accumulators +0xb0 / +0xc0 cleared to 0xd61670 (nothing for a null handle body).
REFLEX_PORT_FN(0x00768460, DynObj_SetPose, "world", OUT_XMM0) {
    Guest g{m};
    const uint32_t h = m.arg(0);
    Frame f(m, 0, 0);
    const uint32_t body = g.u32(h + 4);
    if (body != 0) {
        const uint32_t O = object_of(f, body);
        st(g, O + 0x40, ld(g, m.arg(1)));
        st(g, O + 0x50, ld(g, m.arg(2)));
        st(g, O + 0x60, ld(g, m.arg(3)));
        st(g, O + 0x70, ld(g, m.arg(4)));
        st(g, O + 0xb0, ld(g, 0x00d61670));
        const X z = ld(g, 0x00d61670);
        st(g, O + 0xc0, z);
        xw(g, 0, z);
    }
    return m.ret(0x14);
}
#endif

#ifdef WORLD_HAND_READABLE   // readable port; the literal one in world_phys_lit4.cpp (exact scratch registers for literal callers) is used
// 007684c0: takes a generic object's accumulated contact results (ESI, EDI = out, stdcall handle, out; ret 8): +0x80 / +0x90 /
// +0xa0 copied out and cleared to 0xd61670; a null handle body gives three zero vectors.
REFLEX_PORT_FN(0x007684c0, DynObj_TakeContactResults, "world", OUT_XMM0) {
    Guest g{m};
    const uint32_t h = m.arg(0), out3 = m.arg(1), out1 = m.r[6], out2 = m.r[7];
    Frame f(m, 0xc, 1);
    const uint32_t body = g.u32(h + 4);
    m.r[3] = out3;
    if (body != 0) {
        const uint32_t O = object_of(f, body);
        st(g, out1, ld(g, O + 0x80));
        st(g, out2, ld(g, O + 0x90));
        st(g, out3, ld(g, O + 0xa0));
        st(g, O + 0x80, ld(g, 0x00d61670));
        st(g, O + 0x90, ld(g, 0x00d61670));
        st(g, O + 0xa0, ld(g, 0x00d61670));
    } else {
        st(g, out1, ld(g, 0x00d61670));
        st(g, out2, ld(g, 0x00d61670));
        st(g, out3, ld(g, 0x00d61670));
    }
    xw(g, 0, ld(g, 0x00d61670));
    f.restore_saved();
    return m.ret(8);
}
#endif

#ifdef WORLD_HAND_READABLE   // readable port; the literal one in world_phys_lit4.cpp (exact scratch registers for literal callers) is used
// 00768550: append to a pointer vector unless full (EAX = vector {+0xc begin, +0x10 end, +0x14 capacity}; stdcall value,
// ret 4): 008423d0 (EDI = vector) with the address of the argument slot.
REFLEX_PORT_FN(0x00768550, PtrVector_PushIfRoom, "world", 0) {
    Guest g{m};
    const uint32_t v = m.r[0], slot = m.r[4] + 4;
    Frame f = Frame::flat(m, 2, 0);
    const uint32_t begin = g.u32(v + 0xc);
    const uint32_t cap = begin == 0 ? 0 : uint32_t(int32_t(g.u32(v + 0x14) - begin) >> 2);
    const uint32_t size = uint32_t(int32_t(g.u32(v + 0x10) - begin) >> 2);
    if (size != cap) {
        m.r[7] = v; m.r[1] = begin; m.r[2] = size;
        f.call(0x008423d0, {slot}, begin, slot);
    }
    f.restore_saved();
    return m.ret(4);
}
#endif

#ifdef WORLD_HAND_READABLE   // readable port; the literal one in world_phys_lit4.cpp (exact scratch registers for literal callers) is used
// 00768590: clear every entry equal to the argument in a pointer vector (EDI = vector {+0xc begin, +0x10 end}; stdcall value,
// ret 4). The count is taken once at entry; bounds checks as the original.
REFLEX_PORT_FN(0x00768590, PtrVector_ClearValue, "world", 0) {
    Guest g{m};
    const uint32_t v = m.r[7], val = m.arg(0);
    auto size = [&] { return uint32_t(int32_t(g.u32(v + 0x10) - g.u32(v + 0xc)) >> 2); };
    const int32_t n = int32_t(size());
    for (int32_t i = 0; i < n; i++) {
        if (uint32_t(i) >= size()) g.call(g.u32(kAbort));
        if (g.u32(g.u32(v + 0xc) + 4 * uint32_t(i)) != val) continue;
        if (uint32_t(i) >= size()) g.call(g.u32(kAbort));
        g.u32(g.u32(v + 0xc) + 4 * uint32_t(i), 0);
    }
    return m.ret(4);
}
#endif

// 007686b0: is body A slower than body B (EDX = A, stdcall &world, B; ret 8): |v_A| < |v_B| with the kinematic velocities at
// world +0x1d10 + 0x50 * index (+0x80). EAX = the address arithmetic's upper bytes with AL = the result (sete), as the original
// leaves it; xmm0..2 as well.
REFLEX_PORT_FN(0x007686b0, Kinematic_IsSlower, "world", OUT_EAX | OUT_XMM_ALL) {
    Guest g{m};
    const uint32_t world = g.u32(m.arg(0)), A = m.r[2], B = m.arg(1);
    const X va = ld(g, world + (g.u32(A + 0x80) + 0x5d) * 0x50);
    const uint32_t offb = (g.u32(B + 0x80) + 0x5d) * 0x50;
    const X vb = ld(g, world + offb);
    const X sa = mulps(va, va), sb = mulps(vb, vb);
    const float a2 = x86::add(x86::add(sa.f(0), sa.f(2)), sa.f(1));
    const float b2 = x86::add(x86::add(sb.f(0), sb.f(2)), sb.f(1));
    const X x2 = X::fl(a2, a2, a2, a2), x0 = sqrtps(X::fl(b2, b2, b2, b2));
    const X x1 = cmpltps(sqrtps(x2), x0);
    const uint32_t sum = x1.u[2] + x1.u[1] + x1.u[0];
    xw(g, 0, x0); xw(g, 1, x1); xw(g, 2, x2);
    m.r[1] = x1.u[1]; m.r[2] = sum;
    return m.ret(8, (offb & 0xffffff00u) | (sum == 0xfffffffdu ? 1u : 0u));
}

namespace {
inline X addss(X a, const X& b) { a.u[0] = x86::bits(x86::add(a.f(0), b.f(0))); return a; }
inline bool all3m(const X& m) { return uint32_t(m.u[1] + m.u[2] + m.u[0]) == 0xfffffffdu; }
}  // namespace

// 007705b0: the impulse of one contact on a rigid body, Xenon-style quaternion code (ECX = linear velocity (in / out), EDX =
// inverse inertia (body frame), stdcall: inverse mass, orientation q, centre, angular velocity w (in / out, += the angular
// change), restitution, friction, contact point, normal; ret 0x20). The relative point velocity v + w x r along the normal;
// separating contacts return unchanged. Otherwise the normal impulse (with restitution, over 1/m + n.(I^-1 (r x n)) x r, I^-1
// applied in the body frame through q* . q rotations with the sign masks 0x93a1f0 / 0x93a200 / 0x93a210 / 0x93a220), a friction
// impulse along the tangential velocity when it exceeds 0x9322ac, and the velocity changes. Transliterated register by register
// (all xmm registers end as in the original).
REFLEX_PORT_FN(0x007705b0, RigidBody_ContactImpulse, "world", OUT_XMM_ALL) {
    Guest g{m};
    auto C = [&](uint32_t a) { return ld(g, a); };
    const uint32_t vel = m.r[1], inertia = m.r[2];
    const uint32_t a0 = m.arg(0), a1 = m.arg(1), a2 = m.arg(2), a3 = m.arg(3), a4 = m.arg(4), a5 = m.arg(5), a6 = m.arg(6), a7 = m.arg(7);
    const X kAbs = C(0x0092db80), kS0 = C(0x0093a1f0), kS1 = C(0x0093a200), kS2 = C(0x0093a210), kConj = C(0x0093a220);
    X x0, x1, x2, x3, x4, x5, x6, x7, s10, s20, s30, s40, s50, s60;
    m.r[0] = a3;
    x1 = andps(ld(g, a3), kAbs);
    x3 = ld(g, a6);
    x0 = ld(g, a2);
    x4 = shufps(x1, x1, 0);
    x3 = subps(x3, x0);
    x0 = ld(g, a1);
    x5 = shufps(x1, x1, 0x55);
    x7 = shufps(x1, x1, 0xaa);
    x6 = mulps(x0, kConj);
    x2 = shufps(x6, x6, 0x1b);
    x4 = mulps(x4, x2);
    x2 = shufps(x2, x2, 0xb1);
    x5 = mulps(x5, x2);
    x5 = mulps(x5, kS1);
    x2 = shufps(x2, x2, 0x1b);
    x2 = mulps(x2, x7);
    x2 = mulps(x2, kS2);
    x2 = addps(x2, x5);
    x5 = shufps(x1, x1, 0xff);
    x5 = mulps(x5, x6);
    x6 = kS0;
    x4 = mulps(x4, x6);
    x5 = addps(x5, x4);
    x2 = addps(x2, x5);
    x1 = shufps(x2, x2, 0x1b);
    x4 = shufps(x0, x0, 0);
    x4 = mulps(x4, x1);
    x1 = shufps(x1, x1, 0xb1);
    x5 = shufps(x0, x0, 0x55);
    x5 = mulps(x5, x1);
    x5 = mulps(x5, kS1);
    x1 = shufps(x1, x1, 0x1b);
    x7 = shufps(x0, x0, 0xaa);
    x1 = mulps(x1, x7);
    x1 = mulps(x1, kS2);
    x1 = addps(x1, x5);
    x4 = mulps(x4, x6);
    x5 = shufps(x0, x0, 0xff);
    x5 = mulps(x5, x2);
    x5 = addps(x5, x4);
    x1 = addps(x1, x5);
    x5 = ld(g, vel);
    x1 = shufps(x1, x1, 0xc9);
    x2 = shufps(x3, x3, 0xd2);
    x6 = shufps(x2, x2, 0xd2);
    x7 = shufps(x1, x1, 0xc9);
    x6 = mulps(x6, x7);
    x7 = mulps(x2, x1);
    x7 = subps(x7, x6);
    x6 = kAbs;
    x4 = X::splat(0);
    x7 = andps(x7, x6);
    s20 = x4;
    x7 = addps(x7, x5);
    x4 = subps(x4, x7);
    s50 = x3; s10 = x5; s40 = x4;
    x5 = ld(g, a7);
    x1 = mulps(x5, x4);
    x4 = shufps(x1, x1, 0x99);
    x1 = addss(x1, x4);
    x4 = shufps(x4, x4, 0x55);
    x1 = addss(x1, x4);
    x1 = shufps(x1, x1, 0);
    s60 = x1;
    x1 = cmpltps(x1, C(0x00d61670));
    if (all3m(x1)) {
        x3 = shufps(x3, x3, 0xc9);
        x7 = shufps(x3, x3, 0xc9);
        x1 = shufps(x5, x5, 0xd2);
        x4 = shufps(x1, x1, 0xd2);
        x1 = mulps(x1, x3);
        x4 = mulps(x4, x7);
        x1 = subps(x1, x4);
        x1 = andps(x1, x6); x1 = andps(x1, x6);
        x6 = shufps(x1, x1, 0);
        x4 = shufps(x1, x1, 0xaa);
        x3 = shufps(x0, x0, 0x1b);
        x6 = mulps(x6, x3);
        x6 = mulps(x6, kS0);
        x3 = shufps(x3, x3, 0xb1);
        x7 = x3;
        x3 = shufps(x1, x1, 0x55);
        x3 = mulps(x3, x7);
        x3 = mulps(x3, kS1);
        x7 = shufps(x7, x7, 0x1b);
        x4 = mulps(x4, x7);
        x4 = mulps(x4, kS2);
        x4 = addps(x4, x3);
        x3 = shufps(x1, x1, 0xff);
        x3 = mulps(x3, x0);
        x3 = addps(x3, x6);
        x4 = addps(x4, x3);
        x1 = shufps(x4, x4, 0x1b);
        x3 = mulps(x0, kConj);
        x6 = shufps(x3, x3, 0);
        x6 = mulps(x6, x1);
        s30 = x6;
        x6 = shufps(x3, x3, 0x55);
        x1 = shufps(x1, x1, 0xb1);
        x7 = x1;
        x6 = mulps(x6, x7);
        x6 = mulps(x6, kS1);
        x1 = shufps(x3, x3, 0xaa);
        x7 = shufps(x7, x7, 0x1b);
        x1 = mulps(x1, x7);
        x1 = mulps(x1, kS2);
        x1 = addps(x1, x6);
        x6 = shufps(x3, x3, 0xff);
        x3 = s30;
        x3 = mulps(x3, kS0);
        x6 = mulps(x6, x4);
        x6 = addps(x6, x3);
        x3 = ld(g, inertia);
        x1 = addps(x1, x6);
        x1 = mulps(x1, x3);
        x1 = andps(x1, kAbs);
        x3 = mulps(x0, kConj);
        s30 = x3;
        x3 = shufps(x3, x3, 0x1b);
        x4 = shufps(x1, x1, 0);
        x4 = mulps(x4, x3);
        x3 = shufps(x3, x3, 0xb1);
        x6 = shufps(x1, x1, 0x55);
        x6 = mulps(x6, x3);
        x3 = shufps(x3, x3, 0x1b);
        x7 = shufps(x1, x1, 0xaa);
        x3 = mulps(x3, x7);
        x3 = mulps(x3, kS2);
        x6 = mulps(x6, kS1);
        x4 = mulps(x4, kS0);
        x3 = addps(x3, x6);
        x6 = shufps(x1, x1, 0xff);
        x6 = mulps(x6, s30);
        x6 = addps(x6, x4);
        x3 = addps(x3, x6);
        x1 = shufps(x3, x3, 0x1b);
        x4 = shufps(x0, x0, 0);
        x4 = mulps(x4, x1);
        x4 = mulps(x4, kS0);
        x1 = shufps(x1, x1, 0xb1);
        x6 = shufps(x0, x0, 0x55);
        x6 = mulps(x6, x1);
        x6 = mulps(x6, kS1);
        x1 = shufps(x1, x1, 0x1b);
        x7 = shufps(x0, x0, 0xaa);
        x1 = mulps(x1, x7);
        x1 = mulps(x1, kS2);
        x1 = addps(x1, x6);
        x7 = s20;
        x6 = shufps(x0, x0, 0xff);
        x6 = mulps(x6, x3);
        x3 = shufps(x2, x2, 0xd2);
        x6 = addps(x6, x4);
        x1 = addps(x1, x6);
        x6 = ld(g, a0);
        x1 = shufps(x1, x1, 0xc9);
        x2 = mulps(x2, x1);
        x4 = shufps(x1, x1, 0xc9);
        x3 = mulps(x3, x4);
        x2 = subps(x2, x3);
        x2 = andps(x2, kAbs);
        x2 = mulps(x2, x5);
        x1 = shufps(x2, x2, 0x99);
        x2 = addss(x2, x1);
        x1 = shufps(x1, x1, 0x55);
        x2 = addss(x2, x1);
        x1 = ld(g, a4);
        x3 = shufps(x2, x2, 0);
        x2 = C(0x00d61650);
        x1 = addps(x1, x2);
        x2 = x7;
        x2 = subps(x2, x1);
        x1 = s60;
        x2 = mulps(x2, x1);
        x3 = addps(x3, x6);
        x2 = divps(x2, x3);
        x4 = x7;
        x4 = subps(x4, x2);
        x4 = mulps(x4, x5);
        x5 = mulps(x5, x1);
        x5 = subps(x5, s40);
        x1 = mulps(x5, x5);
        x2 = shufps(x1, x1, 0x66);
        x1 = addss(x1, x2);
        x2 = shufps(x2, x2, 0x55);
        x1 = addss(x1, x2);
        x1 = shufps(x1, x1, 0);
        x2 = sqrtps(x1);
        x1 = C(0x00d61670); s40 = x1;
        x1 = X::splat(g.u32(0x009322ac));
        x1 = cmpltps(x1, x2);
        if (all3m(x1)) {
            x1 = ld(g, a5);
            x7 = subps(x7, x1);
            x7 = mulps(x7, x2);
            x7 = divps(x7, x3);
            x5 = divps(x5, x2);
            x7 = mulps(x7, x5);
        } else {
            x7 = s40;
        }
        x2 = s50;
        x7 = addps(x7, x4);
        x2 = shufps(x2, x2, 0xc9);
        x4 = shufps(x2, x2, 0xc9);
        x1 = shufps(x7, x7, 0xd2);
        x3 = shufps(x1, x1, 0xd2);
        x1 = mulps(x1, x2);
        x2 = kAbs;
        x3 = mulps(x3, x4);
        x1 = subps(x1, x3);
        x1 = andps(x1, x2); x1 = andps(x1, x2);
        x3 = shufps(x1, x1, 0);
        x4 = shufps(x1, x1, 0x55);
        x2 = shufps(x0, x0, 0x1b);
        x3 = mulps(x3, x2);
        x2 = shufps(x2, x2, 0xb1);
        x5 = x2;
        x2 = shufps(x1, x1, 0xaa);
        x4 = mulps(x4, x5);
        x5 = shufps(x5, x5, 0x1b);
        x2 = mulps(x2, x5);
        x5 = kS1;
        x2 = mulps(x2, kS2);
        x4 = mulps(x4, x5);
        x2 = addps(x2, x4);
        x4 = shufps(x1, x1, 0xff);
        x4 = mulps(x4, x0);
        x0 = mulps(x0, kConj);
        s50 = x4;
        x4 = kS0;
        x1 = s50;
        x3 = mulps(x3, x4);
        x1 = addps(x1, x3);
        x2 = addps(x2, x1);
        x6 = mulps(x6, x7);
        x6 = addps(x6, s10);
        x3 = shufps(x0, x0, 0);
        s40 = x4;
        x4 = x0;
        x1 = shufps(x2, x2, 0x1b);
        x3 = mulps(x3, x1);
        x1 = shufps(x1, x1, 0xb1);
        x4 = shufps(x4, x0, 0x55);
        x4 = mulps(x4, x1);
        s50 = x5;
        x5 = kS2;
        st(g, vel, x6);
        x6 = shufps(x1, x1, 0x1b);
        x1 = shufps(x0, x0, 0xaa);
        x6 = mulps(x6, x1);
        x1 = s50;
        x1 = mulps(x1, x4);
        x6 = mulps(x6, x5);
        x6 = addps(x6, x1);
        x1 = shufps(x0, x0, 0xff);
        x0 = s40;
        x0 = mulps(x0, x3);
        x1 = mulps(x1, x2);
        x1 = addps(x1, x0);
        x0 = ld(g, inertia);
        x6 = addps(x6, x1);
        x6 = mulps(x6, x0);
        x0 = ld(g, a3);
        x6 = addps(x6, x0);
        st(g, a3, x6);
    }
    xw(g, 0, x0); xw(g, 1, x1); xw(g, 2, x2); xw(g, 3, x3); xw(g, 4, x4); xw(g, 5, x5); xw(g, 6, x6); xw(g, 7, x7);
    return m.ret(0x20);
}

// 00768750: compacts the world's contact-handler list and runs it (EAX = list (world +0xcc00: a checked vector, container id
// +0, begin +0xc, end +0x10, +0x18 a counter cleared here), stdcall world; ret 4): remove(begin, end, 0) (00776920, value on
// the stack), erase(that, end) (007cf450), then 00776ef0(begin, end, count, world). The checked-iterator aborts as the original.
REFLEX_PORT_FN(0x00768750, HandlerList_CompactAndRun, "world", 0) {
    Guest g{m};
    const uint32_t v = m.r[0], world = m.arg(0);
    Frame f = Frame::flat(m, 4, 0x20);
    const uint32_t sp = f.esp, kValue = sp + 0x14, kRemoved = sp + 0x28, kErased = sp + 0x20;
    const uint32_t end0 = g.u32(v + 0x10);
    g.u32(v + 0x18, 0);
    g.u32(kValue, 0);
    if (g.u32(v + 0xc) > end0) g.call(g.u32(kAbort));
    const uint32_t end1 = g.u32(v + 0x10);
    const uint32_t cont = g.u32(v);
    g.u32(kErased, cont);   // [esp+0x20]: the container id, later erase's output slot
    if (g.u32(v + 0xc) > end1) g.call(g.u32(kAbort));
    const uint32_t begin = g.u32(v + 0xc);
    const uint32_t cont2 = g.u32(v);
    if (begin > g.u32(v + 0x10)) g.call(g.u32(kAbort));
    m.r[3] = end0; m.r[5] = end1; m.r[6] = v; m.r[7] = begin;
    f.call(0x00776920, {kRemoved, kValue, g.u32(v), begin, cont2, end1}, kRemoved, kValue);
    const uint32_t rc = g.u32(kRemoved), rp = g.u32(kRemoved + 4);
    m.r[2] = rc;
    f.call(0x007cf450, {kErased, rc, rp, cont, end0}, rp, kErased);
    const uint32_t e = g.u32(v + 0x10);
    if (g.u32(v + 0xc) > e) g.call(g.u32(kAbort));
    const uint32_t b = g.u32(v + 0xc), c = g.u32(v);
    if (b > g.u32(v + 0x10)) g.call(g.u32(kAbort));
    const uint32_t c2 = g.u32(v);
    if (c == 0 || c != c2) g.call(g.u32(kAbort));
    m.r[3] = c; m.r[5] = b; m.r[6] = c2; m.r[7] = e;
    const uint32_t cnt = uint32_t(int32_t(e - b) >> 2);
    m.r[2] = cnt;
    f.call(0x00776ef0, {b, e, cnt, world}, world);
    f.restore_saved();
    return m.ret(4);
}

// ---- the STL algorithms the handler list uses (checked iterators: {container, pointer} pairs; aborts as the original) ----

#ifdef WORLD_HAND_READABLE   // readable port; the literal one in world_phys_lit4.cpp (exact scratch registers for literal callers) is used
// 00776920: std::remove(first, last, value) (cdecl result*, &value, first.c, first.p, last.c, last.p; EAX = result*). The found
// position is written back into first.p's argument slot, as the original does; the rest is 007774f0 (remove_copy).
REFLEX_PORT_FN(0x00776920, PtrRemove, "world", OUT_EAX) {
    Guest g{m};
    const uint32_t E = m.r[4];
    const uint32_t res = m.arg(0), val = m.arg(1), fc = m.arg(2), fp = m.arg(3), lc = m.arg(4), lp = m.arg(5);
    Frame f = Frame::flat(m, 4, 0xc);
    uint32_t p = fp;
    if (p != lp) { const uint32_t v = g.u32(val); while (g.u32(p) != v) { p += 4; if (p == lp) break; } }
    g.u32(E + 0xc, fc); g.u32(E + 0x10, p);
    if (fc == 0 || fc != lc) g.call(g.u32(kAbort));
    if (p == lp) { g.u32(res + 4, p); g.u32(res, fc); f.restore_saved(); return m.ret(0, res); }
    g.u32(f.esp + 0x14, fc);
    if (fc == 0) g.call(g.u32(kAbort));
    if (p >= g.u32(g.u32(fc) + 0x10)) g.call(g.u32(kAbort));
    g.u8(f.esp + 0x14, 0);
    const uint32_t tag = g.u32(f.esp + 0x14);
    m.r[3] = res; m.r[5] = g.u32(kAbort); m.r[6] = p + 4; m.r[7] = g.u32(fc);
    f.call(0x007774f0, {res, p + 4, lp, val, tag, fc, p}, val, tag);
    f.restore_saved();
    return m.ret(0, res);
}
#endif

#ifdef WORLD_HAND_READABLE   // readable port; the literal one in world_phys_lit4.cpp (exact scratch registers for literal callers) is used
// 007774f0: std::remove_copy(first, last, dest, value) for pointers (cdecl result*, first, last, &value, (unused), dest.c,
// dest.p; EAX = result*); dest.p's argument slot advances as the original's does.
REFLEX_PORT_FN(0x007774f0, PtrRemoveCopy, "world", OUT_EAX) {
    Guest g{m};
    const uint32_t E = m.r[4];
    const uint32_t res = m.arg(0), last = m.arg(2), val = m.arg(3), dc = m.arg(5);
    for (uint32_t p = m.arg(1); p != last; p += 4) {
        if (g.u32(p) == g.u32(val)) continue;
        const uint32_t dp = g.u32(E + 0x1c);
        if (dc == 0) g.call(g.u32(kAbort));
        if (dp >= g.u32((dc ? g.u32(dc) : 0) + 0x10)) g.call(g.u32(kAbort));
        g.u32(E + 0x1c, dp + 4);
        if (dc == 0) g.call(g.u32(kAbort));
        if (dp >= g.u32(g.u32(dc) + 0x10)) g.call(g.u32(kAbort));
        g.u32(dp, g.u32(p));
    }
    g.u32(res, g.u32(E + 0x18));
    g.u32(res + 4, g.u32(E + 0x1c));
    m.r[1] = g.u32(E + 0x18); m.r[2] = g.u32(E + 0x1c);
    return m.ret(0, res);
}
#endif

#ifdef WORLD_HAND_READABLE   // readable port; the literal one in world_phys_lit4.cpp (exact scratch registers for literal callers) is used
// 00777db0: insertion sort of body pointers by speed (cdecl first, last, comparator (by value: &world); the comparator is
// 007686b0 called with the address of the comparator's argument slot), moving runs with memmove_s (IAT 0x916200).
REFLEX_PORT_FN(0x00777db0, BodySort_Insertion, "world", 0) {
    Guest g{m};
    const uint32_t E = m.r[4];
    const uint32_t first = m.arg(0), last = m.arg(1), pred = E + 0xc;
    Frame f = Frame::flat(m, 4, 8);
    const uint32_t sp = f.esp;
    auto slower = [&](uint32_t a, uint32_t b, uint32_t ebx, uint32_t esi) {
        m.r[2] = a; m.r[3] = ebx; m.r[6] = esi; m.r[7] = a;
        return (f.call(0x007686b0, {pred, b}, pred) & 0xff) != 0;
    };
    if (first != last && first + 4 != last) {
        g.u32(sp + 0x14, 4 - first);
        for (uint32_t ebp = first + 4; ebp != g.u32(E + 8); ebp += 4) {
            const uint32_t val = g.u32(ebp);
            g.u32(sp + 0x10, val);
            m.r[5] = ebp;
            if (slower(val, g.u32(first), ebp, first)) {
                const int32_t cnt = int32_t(g.u32(sp + 0x14) + ebp - 4) >> 2;
                if (cnt > 0) {
                    const uint32_t bytes = uint32_t(cnt) * 4;
                    f.call(g.u32(0x00916200), {ebp - bytes + 4, bytes, first, bytes}, ebp - bytes + 4, bytes);
                }
                g.u32(first, val);
            } else {
                uint32_t hole = ebp, cur = ebp - 4;
                if (slower(val, g.u32(cur), ebp, cur)) {
                    uint32_t moved = g.u32(cur);
                    for (;;) {
                        g.u32(hole, moved);
                        const uint32_t next = g.u32(cur - 4);
                        hole = cur; cur -= 4;
                        if (!slower(val, next, hole, cur)) break;
                        moved = next;
                    }
                }
                g.u32(hole, val);
            }
        }
    }
    f.restore_saved();
    return m.ret(0);
}
#endif

#ifdef WORLD_HAND_READABLE   // readable port; the literal one in world_phys_lit4.cpp (exact scratch registers for literal callers) is used
// 00776ef0: the introsort loop of std::sort over body pointers (cdecl first, last, ideal, comparator): while more than 32
// elements, 00777580 (median partition into a {first, second} pair on the stack) and recursion on the smaller side; ideal
// exhausted: heap sort (00777d20 make_heap when more than one element, 00777d60 sort_heap; EAX = last, EDI = first); the
// remainder by insertion sort 00777db0.
REFLEX_PORT_FN(0x00776ef0, BodySort_Loop, "world", 0) {
    Guest g{m};
    uint32_t first = m.arg(0), last = m.arg(1);
    int32_t ideal = int32_t(m.arg(2));
    const uint32_t pred = m.arg(3);
    Frame f = Frame::flat(m, 4, 0xc);
    const uint32_t sp = f.esp, kMid = sp + 0x10;
    int32_t n = int32_t(last - first) >> 2;
    bool heap = false;
    while (n > 32) {
        if (ideal <= 0) { heap = true; break; }
        m.r[3] = last; m.r[6] = uint32_t(ideal); m.r[7] = first;
        f.call(0x00777580, {kMid, first, last, pred}, kMid);
        const uint32_t mid_first = g.u32(kMid), mid_second = g.u32(kMid + 4);
        ideal = ideal / 2 + (ideal / 2) / 2;
        const int32_t right = int32_t(last - mid_second) & ~3, left = int32_t(mid_first - first) & ~3;
        m.r[5] = mid_second; m.r[6] = uint32_t(ideal);
        if (left < right) {
            m.r[3] = last; m.r[7] = first;
            f.call(0x00776ef0, {first, mid_first, uint32_t(ideal), pred});
            first = mid_second;
        } else {
            m.r[3] = last; m.r[7] = first;
            f.call(0x00776ef0, {mid_second, last, uint32_t(ideal), pred});
            last = mid_first;
        }
        n = int32_t(last - first) >> 2;
    }
    if (heap) {
        m.r[3] = last; m.r[6] = uint32_t(ideal); m.r[7] = first;
        if ((int32_t(last - first) & ~3) > 4) f.call(0x00777d20, {pred}, -1, last);
        m.r[3] = last; m.r[7] = first;
        f.call(0x00777d60, {pred}, -1, last);
    } else if (n > 1) {
        m.r[3] = last; m.r[7] = first;
        f.call(0x00777db0, {first, last, pred});
    }
    f.restore_saved();
    return m.ret(0);
}
#endif

// 00777110: std::uninitialized_fill_n for the 0x50-byte nearby-object records (cdecl dest, n, value*): 004dbfa0 (EAX = dest,
// ECX = value) per record. The original's SEH frame (fs:[0] linked in and restored) has no effect after the call and is not
// reproduced; its argument slots dest / n advance as the original's do (they are the caller's memory). ECX = the security
// cookie ([0x9b45e4] ^ EBP) the epilogue pops into it, EAX = the last copy's result (or the frame address with n = 0).
REFLEX_PORT_FN(0x00777110, BodyRecord_FillN, "world", OUT_EAX | reflex_port::OUT_ECX) {
    Guest g{m};
    const uint32_t E = m.r[4];
    uint32_t dest = m.arg(0), n = m.arg(1);
    const uint32_t val = m.arg(2);
    Frame f = Frame::flat(m, 9, 0);
    const uint32_t cookie = g.u32(0x009b45e4) ^ (E - 4);   // ECX ends with the frame cookie the epilogue pops
    uint32_t eax = E - 0x10;                  // lea eax, [ebp - 0xc]
    while (n != 0) {
        m.r[3] = val; m.r[5] = E - 4; m.r[6] = n; m.r[7] = dest;
        eax = f.call(0x004dbfa0, {}, val, dest);
        n--; g.u32(E + 8, n);
        dest += 0x50; g.u32(E + 4, dest);
    }
    f.restore_saved();
    m.r[1] = cookie;
    return m.ret(0, eax);
}
