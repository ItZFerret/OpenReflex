// Packed animation quaternions (0x0042xxxx): 0042ce40 slerp of two 8-byte quaternions (16-bit angle + 3 x int16 axis).
// Scalar SSE with float and double polynomials; transliterated instruction by instruction so that every xmm register and
// EAX / ECX / EDX end as in the original (the callers only read the output, but the register state is reproduced anyway).
#include "math_util.h"

using reflex_game::Guest;
using namespace x86;
using namespace mathx;

namespace {
constexpr uint32_t kOne = 0x3f800000, kNegZero = 0x80000000, kPi = 0x40490fdb, k2Pi = 0x40c90fdb, kInv2Pi = 0x3e22f983,
                   kHalfPi = 0x3fc90fdb, kAngleScale = 0x3a490fdb;   // 0x931f24 0x9774a8 0x977498 0x97749c 0x9774a0 0x977508 0x9778fc
// cos (0x935d3c..0x935d50) and sin (0x935d54..0x935d68) polynomials in a^2
const float c3c = flt(0x3f7ffff3), c40 = flt(0xbeffff3d), c44 = flt(0xbab5a662), c48 = flt(0x37cb076b), c4c = flt(0x3d2aa372),
            c50 = flt(0xb46beed0), c54 = flt(0x3f7ffff9), c58 = flt(0xbe2aaa5c), c5c = flt(0x3c0884a2), c60 = flt(0xb94fb563),
            c64 = flt(0x36352cc0), c68 = flt(0xb2aee90a);
// acos polynomial (doubles 0x977788..0x9777b0), 1.0 (0x977638), pi/2 as a double (0x977510)
constexpr uint64_t d88 = 0x3ff46c01a0000000ull, d90 = 0x3f93d7f000000000ull, d98 = 0x3fd98e2fe0000000ull, dA0 = 0x3ff716ac60000000ull,
                   dA8 = 0x3fa9903ac0000000ull, dB0 = 0x3fe212eb00000000ull, dOne = 0x3ff0000000000000ull, dHalfPi = 0x3ff921fb60000000ull;
constexpr uint32_t kFabs = 0x00401000, kFabs2 = 0x00401040;   // fabs xmm0 (another group's helpers)

inline float s(const X& x) { return x.f(0); }

// x0 - 2pi * trunc(x0 / 2pi), with x1 as scratch; returns the truncated quotient (the GPR the original uses)
int32_t wrap_2pi(X& x0, X& x1) {
    x1 = x0;
    x1 = mulss(x1, flt(kInv2Pi));
    int32_t q = cvtt(s(x1));
    x1 = cvtsi2ss(x1, q);
    x1 = mulss(x1, flt(k2Pi));
    x0 = subss(x0, x1);
    return q;
}

struct Unpacked { float axis[3]; Cmp w_sign; int32_t q; };

// One packed quaternion: angle = u16 * pi/4096 (signed by its own sign), a = pi - wrap(angle); w = -cos poly(a), axis * sin poly(a)
// * sign * 1/32767 (the .data scale at 0x9c6400). Register use as both copies in the original (x0..x5, x7; x6 = 0).
Unpacked unpack(Regs& r, uint16_t angle, int32_t ia, int32_t ib, int32_t ic, float neg_one, float axis_scale) {
    X &x0 = r[0], &x1 = r[1], &x2 = r[2], &x3 = r[3], &x4 = r[4], &x5 = r[5], &x6 = r[6], &x7 = r[7];
    Unpacked u;
    x6 = X{};
    x1 = cvtsi2ss(x1, angle);
    x1 = mulss(x1, flt(kAngleScale));
    Cmp c = comiss(s(x1), s(x6));
    x5 = c.cf ? X::fs(neg_one) : X::ss(kOne);
    x4 = X::ss(kPi);
    x0 = x5;
    x0 = mulss(x0, x1);
    u.q = wrap_2pi(x0, x1);
    x4 = subss(x4, x0);
    x1 = x4; x1 = mulss(x1, x4);
    x2 = x1; x2 = mulss(x2, c50); x2 = addss(x2, c48);
    x3 = x1; x3 = mulss(x3, x1);
    x2 = mulss(x2, x3);
    x0 = x1; x0 = mulss(x0, c44); x0 = addss(x0, c4c);
    x2 = addss(x2, x0);
    x2 = mulss(x2, x3);
    x0 = x1; x0 = mulss(x0, c40); x0 = addss(x0, c3c);
    x2 = addss(x2, x0);
    x0 = X::ss(kNegZero);
    x0 = subss(x0, x2);                         // w
    u.w_sign = comiss(s(x0), s(x6));
    x2 = x1; x2 = mulss(x2, c68); x2 = addss(x2, c64);
    x7 = x1;
    x1 = mulss(x1, c58); x1 = addss(x1, c54);
    x7 = mulss(x7, c60); x7 = addss(x7, c5c);
    x2 = mulss(x2, x3);
    x2 = addss(x2, x7);
    x2 = mulss(x2, x3);
    x2 = addss(x2, x1);
    x2 = mulss(x2, x4);
    x2 = mulss(x2, x5);
    x2 = mulss(x2, axis_scale);
    x1 = cvtsi2ss(x1, ia); x1 = mulss(x1, x2); u.axis[0] = s(x1);
    x1 = cvtsi2ss(x1, ib); x1 = mulss(x1, x2); u.axis[1] = s(x1);
    x1 = cvtsi2ss(x1, ic); x1 = mulss(x1, x2); u.axis[2] = s(x1);
    return u;
}

// sin(v) as the original's tail copies compute it: wrap(v + pi/2) (after fabs), a = pi - that, cos poly(a)
}  // namespace

// 0042ce40: EAX = packed quaternion a, ESI = packed quaternion b, EDI = float[4] out (x, y, z, w), stack arg t (cdecl).
// Unpacks both (each w made non-negative by flipping the whole quaternion), takes the shortest arc (b flipped when the dot is
// negative), the angle from an acos polynomial in double precision (via sqrt(1 - d^2) above 0.7071), and the sine weights from
// the cos polynomial at angle + pi/2; at |d| >= 1 the weights are 1 and 0.
REFLEX_PORT_FN(0x0042ce40, QuatSlerp_Packed, "math", 0) { // outputs: callers read only [EDI] (its registers partly come from fabs, another group's helper)
    Guest g{m};
    Regs r(m);
    X &x0 = r[0], &x1 = r[1], &x2 = r[2], &x3 = r[3], &x4 = r[4], &x5 = r[5], &x6 = r[6], &x7 = r[7];
    const uint32_t pa = m.r[0], pb = m.r[6], out = m.r[7];
    const float t = flt(m.arg(0));
    const float neg_one = g.f32(0x9cd9e8), axis_scale = g.f32(0x9c6400), cos45 = g.f32(0x9ddfc8);   // .data
    uint32_t eax, ecx, edx;
    float s0c, s10, s14, s18, s1c, s20, s24, s28, s2c, s30, s34, s38, s3c;   // the original's stack slots

    // quaternion a
    Unpacked a = unpack(r, uint16_t(m.mem.read(pa, 2)), int16_t(m.mem.read(pa + 2, 2)), int16_t(m.mem.read(pa + 4, 2)), int16_t(m.mem.read(pa + 6, 2)), neg_one, axis_scale);
    ecx = uint32_t(int32_t(int16_t(m.mem.read(pa + 2, 2)))); edx = uint32_t(int32_t(int16_t(m.mem.read(pa + 4, 2)))); eax = uint32_t(int32_t(int16_t(m.mem.read(pa + 6, 2))));
    s20 = a.axis[0]; s24 = a.axis[1]; s28 = a.axis[2];
    x1 = a.w_sign.cf ? X::fs(neg_one) : X::ss(kOne);
    s0c = s(x1);
    call_regs(m, r, kFabs);
    x1 = X::fs(s20);
    x6 = X{};
    s2c = s(x0);
    x0 = X::fs(s0c);
    x1 = mulss(x1, x0); s20 = s(x1);
    x1 = x0; x1 = mulss(x1, s24);
    x0 = mulss(x0, s28);
    s24 = s(x1);
    s28 = s(x0);

    // quaternion b
    Unpacked b = unpack(r, uint16_t(m.mem.read(pb, 2)), int16_t(m.mem.read(pb + 2, 2)), int16_t(m.mem.read(pb + 4, 2)), int16_t(m.mem.read(pb + 6, 2)), neg_one, axis_scale);
    eax = uint32_t(int32_t(int16_t(m.mem.read(pb + 2, 2)))); ecx = uint32_t(int32_t(int16_t(m.mem.read(pb + 4, 2)))); edx = uint32_t(int32_t(int16_t(m.mem.read(pb + 6, 2))));
    s30 = b.axis[0]; s34 = b.axis[1]; s38 = b.axis[2];
    x1 = b.w_sign.cf ? X::fs(neg_one) : X::ss(kOne);
    s0c = s(x1);
    call_regs(m, r, kFabs);
    x1 = X::fs(s0c);
    x2 = X::fs(s34);
    x3 = X::fs(s38);
    x4 = x0;
    x0 = X::fs(s30);
    x0 = mulss(x0, x1);
    x3 = mulss(x3, x1);
    x2 = mulss(x2, x1);
    s30 = s(x0);
    x0 = mulss(x0, s20);
    x1 = x2; x1 = mulss(x1, s24);
    x0 = addss(x0, x1);
    s38 = s(x3);
    x3 = mulss(x3, s28);
    x1 = x4; x1 = mulss(x1, s2c);
    x0 = addss(x0, x3);
    x0 = addss(x0, x1);                         // the dot
    Cmp c = comiss(s(x0), 0.0f);
    s3c = s(x4);
    s34 = s(x2);
    x1 = c.cf ? X::fs(neg_one) : X::ss(kOne);
    s10 = s(x1);                                // b's sign for the shortest arc
    call_regs(m, r, kFabs);
    s0c = s(x0);                                // |dot|
    call_regs(m, r, kFabs2);
    x2 = sd(dOne);
    x4 = x0;
    c = comiss(s(x4), cos45);
    s18 = s(x4);
    if (!c.cf) {                                // |dot| >= 0.7071: the angle from sqrt(1 - d^2)
        x0 = cvtss2sd(x0, X::fs(s0c));
        x1 = x0;
        x1 = mulsd(x1, x0);
        x0 = cvtsd2ss(x0, x1);
        x0 = cvtss2sd(x0, x0);
        x1 = x2;
        x1 = subsd(x1, x0);
        x0 = cvtsd2ss(x0, x1);
        call_regs(m, r, kFabs2);
        x0 = sqrtss(x0, x0);                    // 0042c230 (sqrt of its stack argument)
        x4 = X::fs(s18);
        x2 = sd(dOne);
    } else {
        x0 = x4;
    }
    c = comiss(s(x4), cos45);
    x6 = sd(dB0);
    x1 = cvtss2sd(x1, x0);
    x3 = x1;
    x3 = mulsd(x3, x1);
    x1 = cvtsd2ss(x1, x3);
    x3 = cvtss2sd(x3, x1);
    x5 = x3;
    x5 = mulsd(x5, x3);
    x3 = cvtsd2ss(x3, x5);
    x5 = cvtss2sd(x5, x1);
    x5 = mulsd(x5, sd(dA8));
    x6 = subsd(x6, x5);
    x5 = cvtpd2ps(x6);
    x5 = cvtss2sd(x5, x5);
    x6 = cvtss2sd(x6, x3);
    x5 = mulsd(x5, x6);
    x6 = cvtss2sd(x6, x1);
    x6 = mulsd(x6, sd(dA0));
    x7 = x2;
    x7 = subsd(x7, x6);
    x6 = cvtsd2ss(x6, x7);
    x7 = sd(d98);
    x6 = cvtss2sd(x6, x6);
    x5 = addsd(x5, x6);
    x6 = x2;
    x6 = divsd(x6, x5);
    x5 = cvtpd2ps(x6);
    x6 = cvtss2sd(x6, x1);
    x6 = mulsd(x6, sd(d90));
    x7 = subsd(x7, x6);
    x1 = cvtss2sd(x1, x1);
    x1 = mulsd(x1, sd(d88));
    x2 = subsd(x2, x1);
    x6 = cvtpd2ps(x7);
    x6 = cvtss2sd(x6, x6);
    x0 = cvtss2sd(x0, x0);
    x3 = cvtss2sd(x3, x3);
    x6 = mulsd(x6, x3);
    x1 = cvtpd2ps(x2);
    x1 = cvtss2sd(x1, x1);
    x6 = addsd(x6, x1);
    x6 = mulsd(x6, x0);
    x0 = cvtpd2ps(x6);
    x0 = cvtss2sd(x0, x0);
    x5 = cvtss2sd(x5, x5);
    x5 = mulsd(x5, x0);
    x0 = sd(dHalfPi);
    x1 = cvtsd2ss(x1, x5);
    if (!c.cf) {                                // asin branch: pi/2 - it
        x1 = cvtss2sd(x1, x1);
        x2 = x0;
        x2 = subsd(x2, x1);
        x1 = cvtpd2ps(x2);
    }
    x2 = X::fs(s0c);
    if (comiss(s(x2), 0.0f).cf) {
        x2 = X::ss(kNegZero);
        x2 = subss(x2, x1);
        x1 = x2;
    }
    x1 = cvtss2sd(x1, x1);
    x0 = subsd(x0, x1);
    x0 = cvtpd2ps(x0);                          // the angle
    s18 = s(x0);
    x0 = addss(x0, flt(kHalfPi));
    call_regs(m, r, kFabs);
    eax = uint32_t(wrap_2pi(x0, x1));
    x1 = X::ss(kPi);
    x1 = subss(x1, x0);
    x0 = x1; x0 = mulss(x0, x1);
    x2 = x0; x2 = mulss(x2, c50); x2 = addss(x2, c48);
    x1 = x0; x1 = mulss(x1, x0);
    x3 = x0; x3 = mulss(x3, c44);
    x0 = mulss(x0, c40);
    x3 = addss(x3, c4c);
    x0 = addss(x0, c3c);
    x2 = mulss(x2, x1);
    x2 = addss(x2, x3);
    x3 = X::ss(kOne);
    x2 = mulss(x2, x1);
    x1 = X::fs(s0c);
    c = comiss(s(x1), s(x3));
    x2 = addss(x2, x0);
    x0 = x3;
    x0 = divss(x0, x2);
    s14 = s(x0);                                // 1 / sin(angle)
    x0 = X::fs(s18);
    x2 = x0;
    x2 = mulss(x2, t);
    s1c = s(x2);                                // angle * t
    if (!c.cf) {
        x1 = x3;
    } else {                                    // a's weight sin(angle - angle * t) / sin(angle)
        x0 = subss(x0, x2);
        x0 = addss(x0, flt(kHalfPi));
        call_regs(m, r, kFabs);
        ecx = uint32_t(wrap_2pi(x0, x1));
        x1 = X::ss(kPi);
        x1 = subss(x1, x0);
        x0 = x1; x0 = mulss(x0, x1);
        x1 = x0; x1 = mulss(x1, c50); x1 = addss(x1, c48);
        x2 = x0; x2 = mulss(x2, x0);
        x1 = mulss(x1, x2);
        x3 = x0; x3 = mulss(x3, c44); x3 = addss(x3, c4c);
        x0 = mulss(x0, c40); x0 = addss(x0, c3c);
        x1 = addss(x1, x3);
        x3 = X::ss(kOne);
        x1 = mulss(x1, x2);
        x2 = X::fs(s1c);
        x1 = addss(x1, x0);
        x1 = mulss(x1, s14);
    }
    x0 = X::fs(s0c);
    c = comiss(s(x0), s(x3));
    s18 = s(x1);
    if (!c.cf) {
        x0 = X{};
    } else {                                    // b's weight sin(angle * t) / sin(angle), signed
        x0 = x2;
        x0 = addss(x0, flt(kHalfPi));
        call_regs(m, r, kFabs);
        edx = uint32_t(wrap_2pi(x0, x1));
        x1 = X::ss(kPi);
        x1 = subss(x1, x0);
        x1 = mulss(x1, x1);
        x0 = x1; x0 = mulss(x0, c50); x0 = addss(x0, c48);
        x2 = x1; x2 = mulss(x2, x1);
        x3 = x1; x3 = mulss(x3, c44); x3 = addss(x3, c4c);
        x1 = mulss(x1, c40); x1 = addss(x1, c3c);
        x0 = mulss(x0, x2);
        x0 = addss(x0, x3);
        x0 = mulss(x0, x2);
        x0 = addss(x0, x1);
        x0 = mulss(x0, s10);
        x0 = mulss(x0, s14);
        x1 = X::fs(s18);
    }
    x3 = x1; x3 = mulss(x3, s2c);
    x2 = x0; x2 = mulss(x2, s3c);
    x2 = addss(x2, x3);
    g.f32(out + 0xc, s(x2));
    x3 = x1; x3 = mulss(x3, s20);
    x2 = x0; x2 = mulss(x2, s30);
    x2 = addss(x2, x3);
    g.f32(out, s(x2));
    x3 = x1; x3 = mulss(x3, s24);
    x1 = mulss(x1, s28);
    x2 = x0; x2 = mulss(x2, s34);
    x2 = addss(x2, x3);
    g.f32(out + 4, s(x2));
    x2 = X::fs(s38);
    x2 = mulss(x2, x0);
    x2 = addss(x2, x1);
    g.f32(out + 8, s(x2));
    r.store(m);
    m.r[0] = eax; m.r[1] = ecx; m.r[2] = edx;
    return m.ret(0);
}
