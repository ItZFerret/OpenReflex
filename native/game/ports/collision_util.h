// Helpers of the "collision" group's ports: xmm values as the guest holds them (four 32-bit lanes, bit-exact lane operations
// with the x86 NaN rules of x86.h) and guest access shortcuts.
#pragma once
#include <cstdint>
#include <cstring>
#include "../guest.h"
#include "../port.h"
#include "../x86.h"

namespace collision {

using reflex_game::Guest;

// An xmm value: four 32-bit lanes, lane 0 lowest.
struct X {
    uint32_t u[4];
    static X splat(uint32_t b) { return {{b, b, b, b}}; }
    static X fsplat(float f) { return splat(x86::bits(f)); }
    static X zero() { return {{0, 0, 0, 0}}; }
    float f(int i) const { return x86::flt(u[i]); }
    void setf(int i, float v) { u[i] = x86::bits(v); }
};

inline X ld(const Guest& g, uint32_t a) { return {{g.u32(a), g.u32(a + 4), g.u32(a + 8), g.u32(a + 12)}}; }
inline void st(Guest& g, uint32_t a, const X& v) { for (int i = 0; i < 4; i++) g.u32(a + 4 * i, v.u[i]); }
inline X ld_ss(const Guest& g, uint32_t a) { return {{g.u32(a), 0, 0, 0}}; }             // movss xmm, m32
inline X ld_sd(const Guest& g, uint32_t a) { return {{g.u32(a), g.u32(a + 4), 0, 0}}; }   // movq / movsd xmm, m64
inline X xr(const Guest& g, int i) {
    const auto& x = g.m.xmm[i];
    return {{uint32_t(x.lo), uint32_t(x.lo >> 32), uint32_t(x.hi), uint32_t(x.hi >> 32)}};
}
inline void xw(Guest& g, int i, const X& v) {
    auto& x = g.m.xmm[i];
    x.lo = uint64_t(v.u[0]) | uint64_t(v.u[1]) << 32; x.hi = uint64_t(v.u[2]) | uint64_t(v.u[3]) << 32;
}

#define COLL_LANES(expr) X r; for (int i = 0; i < 4; i++) r.u[i] = (expr); return r
inline X addps(const X& a, const X& b) { COLL_LANES(x86::bits(x86::add(a.f(i), b.f(i)))); }
inline X subps(const X& a, const X& b) { COLL_LANES(x86::bits(x86::sub(a.f(i), b.f(i)))); }
inline X mulps(const X& a, const X& b) { COLL_LANES(x86::bits(x86::mul(a.f(i), b.f(i)))); }
inline X divps(const X& a, const X& b) { COLL_LANES(x86::bits(x86::div(a.f(i), b.f(i)))); }
inline X sqrtps(const X& a) { COLL_LANES(x86::bits(x86::sqrt(a.f(i)))); }
// minps / maxps a, b: a if the comparison holds, else b (NaNs and equal values give b)
inline uint32_t min_bits(uint32_t a, uint32_t b) { return x86::isnan_bits(a) || x86::isnan_bits(b) ? b : (x86::flt(a) < x86::flt(b) ? a : b); }
inline uint32_t max_bits(uint32_t a, uint32_t b) { return x86::isnan_bits(a) || x86::isnan_bits(b) ? b : (x86::flt(a) > x86::flt(b) ? a : b); }
inline X minps(const X& a, const X& b) { COLL_LANES(min_bits(a.u[i], b.u[i])); }
inline X maxps(const X& a, const X& b) { COLL_LANES(max_bits(a.u[i], b.u[i])); }
inline X andps(const X& a, const X& b) { COLL_LANES(a.u[i] & b.u[i]); }
inline X andnps(const X& a, const X& b) { COLL_LANES(~a.u[i] & b.u[i]); }
inline X orps(const X& a, const X& b) { COLL_LANES(a.u[i] | b.u[i]); }
inline X xorps(const X& a, const X& b) { COLL_LANES(a.u[i] ^ b.u[i]); }
inline X paddd(const X& a, const X& b) { COLL_LANES(a.u[i] + b.u[i]); }
inline X psubd(const X& a, const X& b) { COLL_LANES(a.u[i] - b.u[i]); }
inline X pcmpeqd(const X& a, const X& b) { COLL_LANES(a.u[i] == b.u[i] ? 0xffffffffu : 0u); }
inline X pcmpgtd(const X& a, const X& b) { COLL_LANES(int32_t(a.u[i]) > int32_t(b.u[i]) ? 0xffffffffu : 0u); }
inline X cvttps2dq(const X& a) { COLL_LANES(uint32_t(x86::cvtt(a.f(i)))); }
inline X cvtps2dq(const X& a) { COLL_LANES(uint32_t(x86::cvt(a.f(i)))); }
inline X cvtdq2ps(const X& a) { COLL_LANES(x86::bits(float(int32_t(a.u[i])))); }
// cmpps predicates (unordered: false for lt/le/eq, true for neq/nlt/nle)
inline X cmpltps(const X& a, const X& b) { COLL_LANES(a.f(i) < b.f(i) ? 0xffffffffu : 0u); }
inline X cmpleps(const X& a, const X& b) { COLL_LANES(a.f(i) <= b.f(i) ? 0xffffffffu : 0u); }
inline X cmpeqps(const X& a, const X& b) { COLL_LANES(a.f(i) == b.f(i) ? 0xffffffffu : 0u); }
inline X cmpneqps(const X& a, const X& b) { COLL_LANES(!(a.f(i) == b.f(i)) ? 0xffffffffu : 0u); }
inline X cmpnltps(const X& a, const X& b) { COLL_LANES(!(a.f(i) < b.f(i)) ? 0xffffffffu : 0u); }
inline X cmpnleps(const X& a, const X& b) { COLL_LANES(!(a.f(i) <= b.f(i)) ? 0xffffffffu : 0u); }
#undef COLL_LANES
// shufps a, b, imm: lanes 0, 1 from a, lanes 2, 3 from b
inline X shufps(const X& a, const X& b, int imm) { return {{a.u[imm & 3], a.u[(imm >> 2) & 3], b.u[(imm >> 4) & 3], b.u[(imm >> 6) & 3]}}; }
inline X shuf(const X& a, int imm) { return shufps(a, a, imm); }
inline X lane(const X& a, int i) { return X::splat(a.u[i]); }   // shufps a, a, i * 0x55
inline X unpcklps(const X& a, const X& b) { return {{a.u[0], b.u[0], a.u[1], b.u[1]}}; }
inline X unpckhps(const X& a, const X& b) { return {{a.u[2], b.u[2], a.u[3], b.u[3]}}; }
inline X movlhps(const X& a, const X& b) { return {{a.u[0], a.u[1], b.u[0], b.u[1]}}; }
inline X movhlps(const X& a, const X& b) { return {{b.u[2], b.u[3], a.u[2], a.u[3]}}; }
// movss xmm, xmm: lane 0 from b
inline X movss(const X& a, const X& b) { return {{b.u[0], a.u[1], a.u[2], a.u[3]}}; }
// scalar ops on lane 0 (the other lanes of a kept)
inline X addss(X a, const X& b) { a.setf(0, x86::add(a.f(0), b.f(0))); return a; }
inline X subss(X a, const X& b) { a.setf(0, x86::sub(a.f(0), b.f(0))); return a; }
inline X mulss(X a, const X& b) { a.setf(0, x86::mul(a.f(0), b.f(0))); return a; }
inline X divss(X a, const X& b) { a.setf(0, x86::div(a.f(0), b.f(0))); return a; }
inline X sqrtss(X a, const X& b) { a.setf(0, x86::sqrt(b.f(0))); return a; }
inline X minss(X a, const X& b) { a.u[0] = min_bits(a.u[0], b.u[0]); return a; }
inline X maxss(X a, const X& b) { a.u[0] = max_bits(a.u[0], b.u[0]); return a; }
inline X cmpnless(X a, const X& b) { a.u[0] = !(a.f(0) <= b.f(0)) ? 0xffffffffu : 0u; return a; }
inline X cmpltss(X a, const X& b) { a.u[0] = a.f(0) < b.f(0) ? 0xffffffffu : 0u; return a; }
inline X cmpless(X a, const X& b) { a.u[0] = a.f(0) <= b.f(0) ? 0xffffffffu : 0u; return a; }
inline X cmpnltss(X a, const X& b) { a.u[0] = !(a.f(0) < b.f(0)) ? 0xffffffffu : 0u; return a; }

// comiss / ucomiss a, b: the flag outcomes as conditions (unordered sets ZF PF CF)
struct Cmp {
    bool unord, lt, eq;
    bool a() const { return !unord && !lt && !eq; }    // ja: CF=0 and ZF=0
    bool ae() const { return !unord && !lt; }          // jae: CF=0
    bool b() const { return unord || lt; }             // jb: CF=1
    bool be() const { return unord || lt || eq; }      // jbe: CF=1 or ZF=1
    bool e() const { return unord || eq; }             // je: ZF=1
    bool ne() const { return !(unord || eq); }         // jne
    bool p() const { return unord; }                   // jp
};
inline Cmp comiss(float a, float b) { bool u = a != a || b != b; return {u, !u && a < b, !u && a == b}; }

// Exe data. Read-only constants: 1.0f, -0.0f, 2^23, 0.5f, the height scale (2048) and deformation scale (2). Writable data
// read at run time: -1.0f (0x9cd9e8), the runtime vectors at 0xd616xx, the security cookie.
constexpr uint32_t kOne = 0x931f24, kNegZero = 0x9774a8, kTwo23 = 0x9774f0, kHalf = 0x9322cc;
constexpr uint32_t kHeightScale = 0x92e22c, kDeformScale = 0x92e330, kMinusOne = 0x9cd9e8, kCookie = 0x9b45e4;
constexpr uint32_t kEpsilon6 = 0x9777c4, kInf4 = 0x92dc20, kQNaN4 = 0x92dc30;   // 1e-6f; vec4 +inf; vec4 qNaN
constexpr uint32_t kFltMax = 0x977494, kFltMaxNeg = 0x977550;
constexpr uint32_t kZeroF = 0x92e12c, kColStep = 0xd615c0, kRowStep = 0xd61680;   // 0.0f; runtime vec4s
constexpr uint32_t kZero4 = 0xd61660, kCellsBias = 0xd61690, kUpNormal = 0xd616a0;
// Quaternion product sign patterns, conjugate signs, the xyz mask (0xffffffff x3, 0), -0.5f
constexpr uint32_t kQx = 0x93a1f0, kQy = 0x93a200, kQz = 0x93a210, kConjSigns = 0x93a220, kAbsMask = 0x92db80, kMinusHalf = 0x9774b0;
constexpr uint32_t kInvalidParameter = 0x916224;   // import slot: _invalid_parameter_noinfo (std::vector bounds checks)
// Other groups' helpers (called, not re-implemented).
constexpr uint32_t kVecFromLanes = 0x5452d0;   // [arg] = (xmm3, xmm2, xmm1, xmm0) lane 0s; RET 4
constexpr uint32_t kSplatHelper = 0x41c6c0;    // [arg0] = splat(float arg1); RET 8
constexpr uint32_t kFloor4f = 0x42ccd0;        // xmm0 = floor(xmm0) per lane

// Scratch memory on the guest stack below ESP for the original's frame locals that callees write (ESP is lowered while it
// lives, so nested calls push below it). end() (or the destructor) restores ESP.
struct Frame {
    Guest& g; uint32_t esp0, base; bool done = false;
    Frame(Guest& gg, uint32_t bytes) : g(gg), esp0(gg.m.r[4]) { base = (esp0 - bytes - 16) & ~15u; g.m.r[4] = base; }
    // The original's own frame (push ebp; and esp, -16; sub esp, sub; then `pushes` register pushes): base = its ESP after the
    // prologue, so locals sit at the original's addresses and calls made from base push exactly where the original's do (the
    // callees' frames, and the dead stack they leave, line up with the original's).
    Frame(Guest& gg, uint32_t sub, uint32_t pushes) : g(gg), esp0(gg.m.r[4]) {
        base = (((esp0 - 4) & ~15u) - sub) - 4 * pushes; g.m.r[4] = base;
    }
    uint32_t at(uint32_t off) const { return base + off; }
    void end() { if (!done) { g.m.r[4] = esp0; done = true; } }
    ~Frame() { end(); }
};

// FUN_005452d0 with xmm0..3 = (xi, 0, 0, 0) (movss loads): [out] = (x3, x2, x1, x0).
inline void vec_from_lanes(Guest& g, uint32_t out, uint32_t x0, uint32_t x1, uint32_t x2, uint32_t x3) {
    xw(g, 0, X{{x0, 0, 0, 0}}); xw(g, 1, X{{x1, 0, 0, 0}}); xw(g, 2, X{{x2, 0, 0, 0}}); xw(g, 3, X{{x3, 0, 0, 0}});
    g.call(kVecFromLanes, {out});
}
// FUN_0042ccd0: floor per lane (floats).
inline X call_floor4f(Guest& g, const X& v) { xw(g, 0, v); g.call(kFloor4f); return xr(g, 0); }
// fld m32 / fstp m32: a copy, except that a signaling NaN comes back quiet.
inline uint32_t fld_fstp(uint32_t b) { return x86::isnan_bits(b) ? (b | 0x400000u) : b; }


// The address the original's `and esp, -16` frame gives: callers' stack, aligned (for functions that write to their frame and
// whose frame bytes are not compared, nothing needs it).
inline uint32_t align16(uint32_t a) { return a & ~15u; }

}  // namespace collision
