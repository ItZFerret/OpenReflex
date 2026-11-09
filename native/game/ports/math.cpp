// Vector math helpers of MXReflex.exe (0x0042xxxx): the packed SSE helpers (sin, atan, atan2, quaternion slerp, floor) with
// their LTCG register conventions. Every port here leaves all eight xmm registers as the original does (see math_util.h), so
// callers that keep values in registers across the call, or read a scratch register, see the same state.
#include "math_util.h"

using reflex_game::Guest;
using namespace x86;
using namespace mathx;

namespace {
// Odd Taylor coefficients of sin to x^23 (the exe's table at 0x92daf4: -1/3!, 1/5!, ...).
const uint32_t kSinCoeff[11] = {0xbe2aaaab, 0x3c088889, 0xb9500d01, 0x3638ef1d, 0xb2d7322b, 0x2f309231,
                                0xab573f9f, 0x274a963c, 0xa317a4da, 0x1eb8dc78, 0x9a3b0da1};
const float kInv2Pi = flt(0x3e22f983), k2Pi = flt(0x40c90fdb);   // 0x92dc00, 0x92dbf0

// .rdata vectors and scalars of the atan / atan2 / slerp helpers
constexpr uint32_t kOne = 0x3f800000, kMinusOne = 0xbf800000, kHalfPi = 0x3fc90fdb, kPi = 0x40490fdb;

// 0042c240 sin per lane (xmm0 in / out; xmm1..3 left as the original leaves them).
// Range reduction x - 2pi * round(x / 2pi) (lanes with |x / 2pi| >= 2^23 or NaN unrounded), then the odd polynomial, multiplied
// and added in the original's order.
void vec4_sin(Regs& r, Stack) {
    V4 x = V4::from_bits(r[0].u[0], r[0].u[1], r[0].u[2], r[0].u[3]), rr, y, acc;
    for (int i = 0; i < 4; i++) {
        float t = mul(kInv2Pi, x.f[i]);
        uint32_t abs_bits = bits(t) & 0x7fffffffu;
        bool small = int32_t(0x4b000000) > int32_t(abs_bits);
        float rounded = float(cvt(t));
        rr.f[i] = small ? rounded : t;
        y.f[i] = sub(x.f[i], mul(rr.f[i], k2Pi));
    }
    V4 y2 = mul(y, y), p = mul(y2, y);                       // y^2, y^3
    acc = add(mul(V4::splat(flt(kSinCoeff[0])), p), y);
    for (int k = 1; k <= 9; k++) {                           // y^5 .. y^21
        p = mul(p, y2);
        acc = add(mul(V4::splat(flt(kSinCoeff[k])), p), acc);
    }
    V4 y23 = mul(p, y2);
    V4 x0 = add(mul(y23, V4::splat(flt(kSinCoeff[10]))), acc);
    auto put = [](const V4& v) { return X::v(bits(v.f[0]), bits(v.f[1]), bits(v.f[2]), bits(v.f[3])); };
    r[0] = put(x0); r[1] = X::ps(kSinCoeff[10]); r[2] = put(acc); r[3] = put(p);   // xmm3: y^21
}

// 0042c650 atan per lane (xmm0 in / out, xmm1..7 scratch). a = |x| or 1 / |x| (|x| > 1), reduced by pi/6 when a > tan(pi/12)
// ((a * (sqrt3 - 1) + a - 1) / (a + sqrt3)), then a rational polynomial in a^2 (a when |a| <= 2^-12), plus the offsets
// (pi/6, pi/2 - , ...), the sign, and +-pi/2 for |x| >= 2^126 (the constant 0x7e800000).
void vec4_atan(Regs& r, Stack st) {
    X &x0 = r[0], &x1 = r[1], &x2 = r[2], &x3 = r[3], &x4 = r[4], &x5 = r[5], &x6 = r[6], &x7 = r[7];
    X s00, s10, s20, s30, s40, s50;   // the original's stack slots
    x3 = X::ps(kOne);
    x2 = x3;
    x6 = X::ps(kMinusOne);
    x5 = X{};
    x1 = subps(x5, x0);
    x1 = maxps(x1, x0);                         // |x|
    x3 = divps(x3, x1);                         // 1 / |x|
    x2 = cmpltps(x2, x1);                       // |x| > 1
    x3 = andps(x3, x2);
    x4 = andnps(x2, x1);
    x3 = orps(x3, x4);                          // a
    x4 = X::ss(0x3fddb3d7);                     // sqrt 3
    s00 = X::ps(kHalfPi);
    x1 = cmpltps(X::ps(0x3e8930a3), x3);        // a > tan(pi / 12)
    x7 = x1;
    x1 = mulps(X::ps(0x3f3b67af), x3);          // sqrt 3 - 1
    x1 = addps(x1, x3);
    x1 = addps(x1, x6);
    x4 = shufps(x4, x4, 0);
    x4 = addps(x4, x3);
    x1 = divps(x1, x4);
    x1 = andps(x1, x7);
    x4 = andnps(x7, x3);
    x1 = orps(x1, x4);                          // the reduced argument
    x4 = andps(s00, x2);
    s10 = x7;
    x3 = andnps(x2, x5);
    x3 = orps(x3, x4);
    x4 = andnps(s10, x3);
    x3 = x4;
    s40 = x3;
    x3 = X::ps(0x3f060a92);                     // pi / 6
    s20 = x1;
    x4 = andnps(x2, x3);
    x3 = andps(X::ps(0x3f860a92), x2);          // pi / 3
    x4 = orps(x4, x3);
    x3 = X::ss(0xc107e9fb);
    x4 = andps(x4, x7);
    s50 = x5;
    x5 = subps(x5, x1);
    x5 = maxps(x5, x1);
    s30 = x4;
    x4 = mulps(x1, x1);                         // t^2
    x1 = X::ps(0x39800000);
    x5 = cmpleps(x5, x1);                       // |t| <= 2^-12
    x1 = X::ps(0xbf566bd7);
    x3 = shufps(x3, x3, 0);
    x1 = mulps(x1, x4); x1 = addps(x1, x3);
    x3 = X::ps(0xc1a40bfe);
    x1 = mulps(x1, x4); x1 = addps(x1, x3);
    x3 = X::ps(0xc15b0533);
    x1 = mulps(x1, x4); x1 = addps(x1, x3);
    x3 = X::ss(0x4170624f); x3 = shufps(x3, x3, 0);
    s10 = x5;
    x1 = mulps(x1, x4);
    x3 = addps(x3, x4);
    x7 = X::ss(0x426e5052);
    x3 = mulps(x3, x4);
    x7 = shufps(x7, x7, 0);
    x3 = addps(x3, x7);
    x7 = X::ss(0x42ac5090);
    x3 = mulps(x3, x4);
    x7 = shufps(x7, x7, 0);
    x3 = addps(x3, x7);
    x3 = mulps(x3, x4);
    x4 = X::ps(0x422443e6);
    x3 = addps(x3, x4);
    x4 = s20;
    x1 = divps(x1, x3);
    x3 = s10;
    x1 = mulps(x1, x4);
    x1 = addps(x1, x4);
    x3 = andnps(x3, x1);
    x5 = andps(x5, x4);
    x3 = orps(x3, x5);
    x1 = mulps(x3, x6);
    x1 = andps(x1, x2);
    x2 = andnps(x2, x3);
    x3 = X::ss(0x7e800000);
    x1 = orps(x1, x2);
    x2 = orps(s30, s40);
    x1 = addps(x1, x2);
    x2 = cmpleps(x0, s50);                      // x <= 0
    x3 = shufps(x3, x3, 0);
    x7 = mulps(x1, x6);
    x4 = x3;
    x3 = cmpleps(x3, x0);                       // x >= 2^126
    x4 = mulps(x4, x6);
    x6 = mulps(x6, s00);
    x7 = andps(x7, x2);
    x2 = andnps(x2, x1);
    x1 = X::ps(kHalfPi);
    x0 = cmpleps(x0, x4);                       // x <= -2^126
    x5 = x3;
    x4 = x0;
    x7 = orps(x7, x2);
    x5 = andnps(x5, x7);
    x3 = andps(x3, x1);
    x5 = orps(x5, x3);
    x0 = andnps(x0, x5);
    x6 = andps(x6, x4);
    x0 = orps(x0, x6);
    uint32_t f = st.enter(0x60);
    st.w128(f, s00); st.w128(f + 0x10, s10); st.w128(f + 0x20, s20); st.w128(f + 0x30, s30); st.w128(f + 0x40, s40); st.w128(f + 0x50, s50);
}

// 0042c880 atan2 per lane: xmm0 = y, xmm1 = x in, xmm0 = atan2(y, x) out (xmm1..7 scratch). atan(y / x) plus the quadrant offset,
// with the infinity and zero cases selected by masks.
void vec4_atan2(Regs& r, Stack st) {
    X &x0 = r[0], &x1 = r[1], &x2 = r[2], &x3 = r[3], &x4 = r[4], &x5 = r[5], &x6 = r[6], &x7 = r[7];
    const X kZero{}, kInf = X::ps(0x7f800000), kAbs = X::ps(0x7fffffff), kAll = X::ps(0xffffffff), kSign = X::ps(0x80000000);
    X s00, s10, s20, s30, s40, s50, s60;
    x5 = kZero;
    x3 = x0;
    x0 = kAbs;
    s00 = x3;
    x2 = andps(x0, x3);
    x6 = x1;
    x1 = kSign;
    x2 = cmpeqps(x2, kInf);                     // |y| == inf
    x0 = andps(x0, x6);
    x0 = cmpeqps(x0, kInf);                     // |x| == inf
    x4 = andps(x1, x3);                         // sign of y
    x3 = X::ss(kPi); x3 = shufps(x3, x3, 0);
    x3 = orps(x3, x4);
    x1 = andps(x1, x6);
    x1 = pcmpeqd(x1, x5);                       // x >= +0 (sign clear)
    x7 = andnps(x1, x3);
    x5 = cmpeqps(x5, x6);                       // x == 0
    x3 = andps(x1, x4);
    x3 = orps(x3, x7);
    s30 = x3;
    x3 = X::ss(kHalfPi); x3 = shufps(x3, x3, 0);
    x3 = orps(x3, x4);
    s60 = x7;
    s10 = x3;
    x7 = andnps(x5, kAll);
    s40 = x6;
    x6 = x0;
    x0 = X::ss(0x4016cbe4); x0 = shufps(x0, x0, 0);   // 3pi / 4
    x0 = orps(x0, x4);
    s20 = x7;
    x7 = x3;
    x3 = andnps(x1, x0);
    x0 = X::ss(0x3f490fdb); x0 = shufps(x0, x0, 0);   // pi / 4
    x0 = orps(x0, x4);
    x0 = andps(x0, x1);
    x3 = orps(x3, x0);
    x7 = andps(x7, x5);
    x5 = kZero;
    x5 = cmpeqps(x5, s00);                      // y == 0
    x7 = orps(x7, s20);
    x3 = andps(x3, x6);
    x0 = andnps(x6, s10);
    x3 = orps(x3, x0);
    x0 = s30;
    x4 = andnps(x5, x7);
    x5 = andps(x5, x0);
    x4 = orps(x4, x5);
    s50 = x1;
    x1 = andnps(x2, x4);
    x3 = andps(x3, x2);
    x3 = orps(x3, x1);
    x4 = andps(x3, x2);
    x2 = andnps(x2, x0);
    x0 = divps(s00, s40);                       // y / x
    x4 = orps(x4, x2);
    x4 = andps(x4, x6);
    x6 = andnps(x6, x3);
    x4 = orps(x4, x6);
    s20 = x4;
    x4 = pcmpeqd(x4, kAll);                     // no special case: atan(y / x) + offset
    s30 = x4;
    uint32_t f = st.enter(0x70);
    st.w128(f, s00); st.w128(f + 0x10, s10); st.w128(f + 0x20, s20); st.w128(f + 0x30, s30); st.w128(f + 0x40, s40); st.w128(f + 0x50, s50);
    st.w128(f + 0x60, s60);
    vec4_atan(r, st.call(0x0042c9cb));
    x1 = andps(kZero, s50);
    x1 = orps(x1, s60);
    x0 = addps(x0, x1);
    x1 = s30;
    x0 = andps(x0, x1);
    x1 = andnps(x1, s20);
    x0 = orps(x0, x1);
}

// 0042ca00 quaternion slerp: xmm0 = q0, xmm1 = q1, xmm2 lane 0 = t in; xmm0 = slerp(q0, q1, t) out (shortest arc: q1's weight
// takes the sign of the dot; lerp when |dot| >= 0.99999).
void quat_slerp(Regs& r, Stack st) {
    X &x0 = r[0], &x1 = r[1], &x2 = r[2], &x3 = r[3], &x4 = r[4], &x5 = r[5];
    X s00, s10, s20, s30, s40, s50;
    x4 = X::ps(kOne);
    x5 = X::ps(kMinusOne);
    s50 = x0;
    x0 = mulps(x0, x1);
    s40 = x1;
    x1 = shufps(x1, x0, 0x40);
    x1 = addps(x1, x0);
    x0 = shufps(x0, x1, 0x30);
    x0 = addps(x0, x1);
    x0 = shufps(x0, x0, 0xaa);                  // the dot in every lane
    x3 = x0;
    x1 = X{};
    x3 = cmpltps(x3, x1);
    x1 = x3;
    x5 = andps(x5, x3);
    x3 = X::ps(0x3f7fff58);                     // 0.99999
    x1 = andnps(x1, x4);
    x1 = orps(x1, x5);                          // sign of the dot
    s30 = x1;
    x1 = mulps(x1, x0);                         // |dot|
    x0 = cmpltps(x1, x3);
    x3 = X::v(0xffffffff, 0xffffffff, 0, 0);
    x2 = shufps(x2, x2, 0xb1);
    x2 = andps(x2, x3);
    x3 = X::v(0x80000000, 0, 0, 0);
    s20 = x0;
    x0 = mulps(x1, x1);
    x2 = xorps(x2, x3);
    x3 = X::v(kOne, 0, 0, 0);
    x4 = subps(x4, x0);
    x0 = sqrtps(x4);                            // sin of the angle
    x2 = addps(x2, x3);                         // (1 - t, t, 0, 0)
    s10 = x0;
    s00 = x2;
    uint32_t f = st.enter(0x60);
    st.w128(f, s00); st.w128(f + 0x10, s10); st.w128(f + 0x20, s20); st.w128(f + 0x30, s30); st.w128(f + 0x40, s40); st.w128(f + 0x50, s50);
    vec4_atan2(r, st.call(0x0042caa3));         // the angle
    x0 = mulps(x0, s00);
    vec4_sin(r, st.call(0x0042caac));
    x1 = divps(x0, s10);
    x0 = s20;
    x1 = andps(x1, x0);
    x0 = andnps(x0, s00);
    x1 = orps(x1, x0);                          // the weights
    x0 = shufps(x1, x1, 0x55);
    x0 = mulps(x0, s30);
    x0 = mulps(x0, s40);
    x2 = shufps(x1, x1, 0);
    x2 = mulps(x2, s50);
    x0 = addps(x0, x2);
}

// 0042ccd0 floor per lane (|x| < 2^23): round to nearest with the 2^23 trick (sign-matched), minus 1 where that rounded up.
// xmm0 in / out; xmm1..6 as the original leaves them.
void vec4_floor(Regs& r, Stack st) {
    X &x0 = r[0], &x1 = r[1], &x2 = r[2], &x3 = r[3], &x4 = r[4], &x5 = r[5], &x6 = r[6];
    const X in = x0;
    X out{};
    x1 = X::ss(0x80000000);
    x2 = X::ss(0x4b000000);
    x6 = X::ss(kOne);
    x5 = X::ss(in.u[0]);
    for (int i = 0; i < 3; i++) {
        x4 = x2;
        x3 = andps(x1, x5);
        x4 = orps(x4, x3);
        x0 = x5;
        x0 = addss(x0, x4);
        x0 = subss(x0, x4);
        x4 = subss(x0, x5);
        if (i < 2) x5 = X::ss(in.u[i + 1]);
        x4 = cmpnless(x4, x3);
        if (i == 2) x3 = X::ss(in.u[3]);
        x4 = andps(x4, x6);
        x0 = subss(x0, x4);
        out.u[i] = x0.u[0];
    }
    x1 = andps(x1, x3);
    x2 = orps(x2, x1);
    x0 = x3;
    x0 = addss(x0, x2);
    x0 = subss(x0, x2);
    x2 = subss(x0, x3);
    x2 = cmpnless(x2, x1);
    x2 = andps(x2, x6);
    x0 = subss(x0, x2);
    out.u[3] = x0.u[0];
    x0 = out;
    uint32_t f = st.enter(0x20);
    st.w128(f, in); st.w128(f + 0x10, out);
}

template <void (*F)(Regs&, Stack)> uint32_t run_regs(reflex_vm::Machine& m) {
    Regs r(m);
    F(r, Stack::entry(m));
    r.store(m);
    return m.ret(0);
}
}  // namespace

REFLEX_PORT_FN(0x0042c240, Vec4Sin_RangeReduced, "math", OUT_XMM) { return run_regs<vec4_sin>(m); }
REFLEX_PORT_FN(0x0042c650, Vec4Atan, "math", OUT_XMM) { return run_regs<vec4_atan>(m); }
REFLEX_PORT_FN(0x0042c880, Vec4Atan2, "math", OUT_XMM) { return run_regs<vec4_atan2>(m); }
REFLEX_PORT_FN(0x0042ca00, QuatSlerp_Simd, "math", OUT_XMM) { return run_regs<quat_slerp>(m); }
REFLEX_PORT_FN(0x0042ccd0, Vec4Floor, "math", OUT_XMM) { return run_regs<vec4_floor>(m); }
