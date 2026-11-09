// Helpers of the "rest" ports (wave 3a: the exe functions still original per race frame, notes/rest.md): a copy of the engine
// group's engine_util.h (itself a copy of bike_util.h; namespace rst, RST_ macros, RST_CALLB for worker thread procedures) plus
// the fx group's rcp / rsqrt and FS_BASE, so the rest group's instruction-level transliterations depend on no other group's header.
// Semantics are the reference interpreter's (vm.cpp sse32 / sse64, x87 through reflex_f80).
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include "../guest.h"
#include "../port.h"
#include "f80/f80.h"
#if defined(__aarch64__)
#include <arm_neon.h>
#endif

namespace rst {
using reflex_vm::Machine;

// ------------------------------------------------------------------ memory
inline uint32_t rd8(Machine& m, uint32_t a) { return uint32_t(m.mem.read(a, 1)); }
inline uint32_t rd16(Machine& m, uint32_t a) { return uint32_t(m.mem.read(a, 2)); }
inline uint32_t rd32(Machine& m, uint32_t a) { return uint32_t(m.mem.read(a, 4)); }
inline uint64_t rd64(Machine& m, uint32_t a) { return m.mem.read(a, 8); }
inline void wr8(Machine& m, uint32_t a, uint32_t v) { m.mem.write(a, 1, v & 0xff); }
inline void wr16(Machine& m, uint32_t a, uint32_t v) { m.mem.write(a, 2, v & 0xffff); }
inline void wr32(Machine& m, uint32_t a, uint32_t v) { m.mem.write(a, 4, v); }
inline void wr64(Machine& m, uint32_t a, uint64_t v) { m.mem.write(a, 8, v); }
constexpr uint32_t FS_BASE = 0x7ffd0000u;   // the world runtime's TEB (fs: segment base, the same on every emulated thread)

// ------------------------------------------------------------------ xmm values
struct X {
    uint64_t lo = 0, hi = 0;
    uint32_t l(int i) const { return uint32_t((i < 2 ? lo : hi) >> (32 * (i & 1))); }
    void sl(int i, uint32_t v) { uint64_t& w = i < 2 ? lo : hi; int s = 32 * (i & 1); w = (w & ~(0xffffffffull << s)) | (uint64_t(v) << s); }
};
inline X ldx(Machine& m, uint32_t a) { return X{m.mem.read(a, 8), m.mem.read(a + 8, 8)}; }
inline void stx(Machine& m, uint32_t a, const X& v) { m.mem.write(a, 8, v.lo); m.mem.write(a + 8, 8, v.hi); }
inline X fromm(const reflex_vm::Xmm& v) { return X{v.lo, v.hi}; }
inline reflex_vm::Xmm tom(const X& v) { reflex_vm::Xmm r; r.lo = v.lo; r.hi = v.hi; return r; }

inline float bf(uint32_t b) { float f; std::memcpy(&f, &b, 4); return f; }
inline uint32_t fb(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
inline double bd(uint64_t b) { double d; std::memcpy(&d, &b, 8); return d; }
inline uint64_t db(double d) { uint64_t b; std::memcpy(&b, &d, 8); return b; }
inline bool nan32(uint32_t b) { return (b & 0x7fffffff) > 0x7f800000; }
inline bool nan64(uint64_t b) { return (b & 0x7fffffffffffffffull) > 0x7ff0000000000000ull; }

enum { OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MIN, OP_MAX };
// one lane, as vm.cpp's sse32 / sse64 (the reference's NaN rules, min / max included)
inline uint32_t sse32(int op, uint32_t a, uint32_t b) {
    if (op == OP_MIN) return bf(a) < bf(b) ? a : b;   // min / max: the source unless the comparison holds (NaN: the source), Intel SDM
    if (op == OP_MAX) return bf(a) > bf(b) ? a : b;
    if (nan32(a)) return a | 0x400000;
    if (nan32(b)) return b | 0x400000;
    float x = bf(a), y = bf(b), r;
    switch (op) {
        case OP_ADD: r = x + y; break;
        case OP_SUB: r = x - y; break;
        case OP_MUL: r = x * y; break;
        case OP_DIV: r = x / y; break;
        case OP_MIN: return x < y ? a : b;
        default: return x > y ? a : b;
    }
    uint32_t out = fb(r);
    return nan32(out) ? 0xffc00000u : out;
}
inline uint64_t sse64(int op, uint64_t a, uint64_t b) {
    if (op == OP_MIN) return bd(a) < bd(b) ? a : b;
    if (op == OP_MAX) return bd(a) > bd(b) ? a : b;
    if (nan64(a)) return a | 0x8000000000000ull;
    if (nan64(b)) return b | 0x8000000000000ull;
    double x = bd(a), y = bd(b), r;
    switch (op) {
        case OP_ADD: r = x + y; break;
        case OP_SUB: r = x - y; break;
        case OP_MUL: r = x * y; break;
        case OP_DIV: r = x / y; break;
        case OP_MIN: return x < y ? a : b;
        default: return x > y ? a : b;
    }
    uint64_t out = db(r);
    return nan64(out) ? 0xfff8000000000000ull : out;
}
inline uint32_t sqrt32(uint32_t a) {
    if (nan32(a)) return a | 0x400000;
    if (a == 0x80000000u) return a;
    if (a >> 31) return 0xffc00000u;
    return fb(std::sqrt(bf(a)));
}
inline uint64_t sqrt64(uint64_t v) {
    if (nan64(v)) return v | 0x8000000000000ull;
    if (v == 0x8000000000000000ull) return v;
    if (v >> 63) return 0xfff8000000000000ull;
    return db(std::sqrt(bd(v)));
}

// rcpss / rcpps / rsqrtss / rsqrtps: the approximation tables the reference uses (vm.cpp rsqrt32 / rcp32)
#include "rsqrt_rcp_tables.inc"
inline uint32_t rsqrt32(uint32_t x) {
    uint32_t s = x >> 31, e = (x >> 23) & 255, m = x & 0x7fffff;
    if (e == 255) return m ? (x | 0x400000) : (s ? 0xffc00000u : 0u);
    if (e == 0) return s ? 0xff800000u : 0x7f800000u;
    if (s) return 0xffc00000u;
    uint32_t ref = (e & 1) ? 127 : 126;
    uint32_t r = (e & 1) ? RSQRT_127[m >> 13] : RSQRT_126[m >> 13];
    int32_t re = int32_t((r >> 23) & 255) + (int32_t(ref) - int32_t(e)) / 2;
    return (uint32_t(re) << 23) | (r & 0x7fffff);
}
inline uint32_t rcp32(uint32_t x) {
    uint32_t s = x >> 31, e = (x >> 23) & 255, m = x & 0x7fffff;
    if (e == 255) return m ? (x | 0x400000) : (s << 31);
    if (e == 0) return (s << 31) | 0x7f800000u;
    uint32_t r = RCP_127[m >> 12];
    int32_t re = int32_t((r >> 23) & 255) + (127 - int32_t(e));
    if (re <= 0) return s << 31;
    return (s << 31) | (uint32_t(re) << 23) | (r & 0x7fffff);
}

// packed / scalar single
template <int OP> inline X ps(X a, const X& b) {
#if defined(__aarch64__)
    if (OP <= OP_DIV) {   // the host's vector unit: IEEE-identical unless a NaN is involved (then the per-lane x86 rule below)
        float32x4_t x = vreinterpretq_f32_u64(vcombine_u64(vcreate_u64(a.lo), vcreate_u64(a.hi)));
        float32x4_t y = vreinterpretq_f32_u64(vcombine_u64(vcreate_u64(b.lo), vcreate_u64(b.hi)));
        float32x4_t r = OP == OP_ADD ? vaddq_f32(x, y) : OP == OP_SUB ? vsubq_f32(x, y) : OP == OP_MUL ? vmulq_f32(x, y) : vdivq_f32(x, y);
        uint32x4_t ok = vandq_u32(vandq_u32(vceqq_f32(x, x), vceqq_f32(y, y)), vceqq_f32(r, r));
        if (vminvq_u32(ok) != 0) { uint64x2_t o = vreinterpretq_u64_f32(r); return X{vgetq_lane_u64(o, 0), vgetq_lane_u64(o, 1)}; }
    }
#endif
    for (int i = 0; i < 4; i++) a.sl(i, sse32(OP, a.l(i), b.l(i)));
    return a;
}
template <int OP> inline X ss(X a, uint32_t b) { a.sl(0, sse32(OP, a.l(0), b)); return a; }
template <int OP> inline X pd(X a, const X& b) { a.lo = sse64(OP, a.lo, b.lo); a.hi = sse64(OP, a.hi, b.hi); return a; }
template <int OP> inline X sd(X a, uint64_t b) { a.lo = sse64(OP, a.lo, b); return a; }
inline X addps(const X& a, const X& b) { return ps<OP_ADD>(a, b); }
inline X subps(const X& a, const X& b) { return ps<OP_SUB>(a, b); }
inline X mulps(const X& a, const X& b) { return ps<OP_MUL>(a, b); }
inline X divps(const X& a, const X& b) { return ps<OP_DIV>(a, b); }
inline X minps(const X& a, const X& b) { return ps<OP_MIN>(a, b); }
inline X maxps(const X& a, const X& b) { return ps<OP_MAX>(a, b); }
inline X addss(const X& a, uint32_t b) { return ss<OP_ADD>(a, b); }
inline X subss(const X& a, uint32_t b) { return ss<OP_SUB>(a, b); }
inline X mulss(const X& a, uint32_t b) { return ss<OP_MUL>(a, b); }
inline X divss(const X& a, uint32_t b) { return ss<OP_DIV>(a, b); }
inline X minss(const X& a, uint32_t b) { return ss<OP_MIN>(a, b); }
inline X maxss(const X& a, uint32_t b) { return ss<OP_MAX>(a, b); }
inline X addsd(const X& a, uint64_t b) { return sd<OP_ADD>(a, b); }
inline X subsd(const X& a, uint64_t b) { return sd<OP_SUB>(a, b); }
inline X mulsd(const X& a, uint64_t b) { return sd<OP_MUL>(a, b); }
inline X divsd(const X& a, uint64_t b) { return sd<OP_DIV>(a, b); }
inline X addpd(const X& a, const X& b) { return pd<OP_ADD>(a, b); }
inline X subpd(const X& a, const X& b) { return pd<OP_SUB>(a, b); }
inline X mulpd(const X& a, const X& b) { return pd<OP_MUL>(a, b); }
inline X divpd(const X& a, const X& b) { return pd<OP_DIV>(a, b); }
inline X sqrtps(X a, const X& b) { for (int i = 0; i < 4; i++) a.sl(i, sqrt32(b.l(i))); return a; }
inline X sqrtss(X a, uint32_t b) { a.sl(0, sqrt32(b)); return a; }
inline X sqrtsd(X a, uint64_t b) { a.lo = sqrt64(b); return a; }
inline X rcpss(X a, uint32_t b) { a.sl(0, rcp32(b)); return a; }
inline X rsqrtss(X a, uint32_t b) { a.sl(0, rsqrt32(b)); return a; }
inline X rcpps(X a, const X& b) { for (int i = 0; i < 4; i++) a.sl(i, rcp32(b.l(i))); return a; }
inline X rsqrtps(X a, const X& b) { for (int i = 0; i < 4; i++) a.sl(i, rsqrt32(b.l(i))); return a; }
inline X andps(const X& a, const X& b) { return X{a.lo & b.lo, a.hi & b.hi}; }
inline X andnps(const X& a, const X& b) { return X{~a.lo & b.lo, ~a.hi & b.hi}; }
inline X orps(const X& a, const X& b) { return X{a.lo | b.lo, a.hi | b.hi}; }
inline X xorps(const X& a, const X& b) { return X{a.lo ^ b.lo, a.hi ^ b.hi}; }
inline X shufps(const X& a, const X& b, unsigned imm) {
    X r; r.sl(0, a.l(imm & 3)); r.sl(1, a.l((imm >> 2) & 3)); r.sl(2, b.l((imm >> 4) & 3)); r.sl(3, b.l((imm >> 6) & 3)); return r;
}
inline X unpcklps(const X& a, const X& b) { X r; r.sl(0, a.l(0)); r.sl(1, b.l(0)); r.sl(2, a.l(1)); r.sl(3, b.l(1)); return r; }
inline X unpckhps(const X& a, const X& b) { X r; r.sl(0, a.l(2)); r.sl(1, b.l(2)); r.sl(2, a.l(3)); r.sl(3, b.l(3)); return r; }
inline X movlhps(const X& a, const X& b) { return X{a.lo, b.lo}; }
inline X movhlps(const X& a, const X& b) { return X{b.hi, a.hi}; }
inline X unpcklpd(const X& a, const X& b) { return X{a.lo, b.lo}; }
inline X unpckhpd(const X& a, const X& b) { return X{a.hi, b.hi}; }
inline X shufpd(const X& a, const X& b, unsigned imm) { return X{(imm & 1) ? a.hi : a.lo, (imm & 2) ? b.hi : b.lo}; }
inline X pshufd(const X& b, unsigned imm) { X r; for (int k = 0; k < 4; k++) r.sl(k, b.l((imm >> (2 * k)) & 3)); return r; }
// movss / movsd: register <- memory zeroes the upper lanes; register <- register replaces lane 0 only
inline X movss_m(uint32_t v) { return X{v, 0}; }
inline X movss_r(X a, const X& b) { a.sl(0, b.l(0)); return a; }
inline X movsd_m(uint64_t v) { return X{v, 0}; }
inline X movsd_r(X a, const X& b) { a.lo = b.lo; return a; }
inline X movd(uint32_t v) { return X{v, 0}; }
// compares (vm.cpp: predicate 0 eq, 1 lt, 2 le, 3 unord, 4 neq, 5 nlt, 6 nle, 7 ord)
inline bool cmp_pred(int pred, double x, double y) {
    bool un = std::isnan(x) || std::isnan(y);
    switch (pred) {
        case 0: return x == y; case 1: return x < y; case 2: return x <= y; case 3: return un;
        case 4: return un || x != y; case 5: return un || !(x < y); case 6: return un || !(x <= y); default: return !un;
    }
}
template <int P> inline X cmpps(X a, const X& b) { for (int i = 0; i < 4; i++) a.sl(i, cmp_pred(P, bf(a.l(i)), bf(b.l(i))) ? 0xffffffffu : 0); return a; }
template <int P> inline X cmpss(X a, uint32_t b) { a.sl(0, cmp_pred(P, bf(a.l(0)), bf(b)) ? 0xffffffffu : 0); return a; }
template <int P> inline X cmppd(X a, const X& b) { a.lo = cmp_pred(P, bd(a.lo), bd(b.lo)) ? ~0ull : 0; a.hi = cmp_pred(P, bd(a.hi), bd(b.hi)) ? ~0ull : 0; return a; }
template <int P> inline X cmpsd(X a, uint64_t b) { a.lo = cmp_pred(P, bd(a.lo), bd(b)) ? ~0ull : 0; return a; }
// conversions (vm.cpp)
inline int32_t to_i32_trunc(double x) { if (std::isnan(x) || std::isinf(x)) return INT32_MIN; double t = std::trunc(x); if (t < -2147483648.0 || t >= 2147483648.0) return INT32_MIN; return int32_t(t); }
inline int32_t to_i32_near(double x) { if (std::isnan(x) || std::isinf(x)) return INT32_MIN; double t = std::nearbyint(x); if (t < -2147483648.0 || t >= 2147483648.0) return INT32_MIN; return int32_t(t); }
inline uint32_t cvttss2si(uint32_t v) { return uint32_t(to_i32_trunc(double(bf(v)))); }
inline uint32_t cvtss2si(uint32_t v) { return uint32_t(to_i32_near(double(bf(v)))); }
inline uint32_t cvttsd2si(uint64_t v) { return uint32_t(to_i32_trunc(bd(v))); }
inline uint32_t cvtsd2si(uint64_t v) { return uint32_t(to_i32_near(bd(v))); }
inline X cvtsi2ss(X a, uint32_t v) { a.sl(0, fb(float(int32_t(v)))); return a; }
inline X cvtsi2sd(X a, uint32_t v) { a.lo = db(double(int32_t(v))); return a; }
inline X cvtss2sd(X a, uint32_t v) {
    a.lo = nan32(v) ? ((uint64_t(v >> 31) << 63) | 0x7ff8000000000000ull | (uint64_t(v & 0x3fffff) << 29)) : db(double(bf(v)));
    return a;
}
inline X cvtsd2ss(X a, uint64_t v) {
    a.sl(0, nan64(v) ? uint32_t(((v >> 63) << 31) | 0x7fc00000u | ((v >> 29) & 0x3fffff)) : fb(float(bd(v))));
    return a;
}
inline X cvtps2pd(uint64_t v) { return X{db(double(bf(uint32_t(v)))), db(double(bf(uint32_t(v >> 32))))}; }
inline X cvtpd2ps(const X& v) { return X{uint64_t(fb(float(bd(v.lo)))) | (uint64_t(fb(float(bd(v.hi)))) << 32), 0}; }
inline X cvtdq2ps(const X& v) { X r; for (int k = 0; k < 4; k++) r.sl(k, fb(float(int32_t(v.l(k))))); return r; }
inline X cvttps2dq(const X& v) { X r; for (int k = 0; k < 4; k++) r.sl(k, uint32_t(to_i32_trunc(bf(v.l(k))))); return r; }
inline X cvtps2dq(const X& v) { X r; for (int k = 0; k < 4; k++) r.sl(k, uint32_t(to_i32_near(bf(v.l(k))))); return r; }
inline uint32_t movmskps(const X& v) { uint32_t r = 0; for (int i = 0; i < 4; i++) r |= (v.l(i) >> 31) << i; return r; }

// comiss / ucomiss and friends: the outcome, as the flags' consumers read it
struct FCmp { bool un, lt, eq; };
inline FCmp fcmp32(uint32_t a, uint32_t b) { float x = bf(a), y = bf(b); bool un = std::isnan(x) || std::isnan(y); return {un, !un && x < y, !un && x == y}; }
inline FCmp fcmp64(uint64_t a, uint64_t b) { double x = bd(a), y = bd(b); bool un = std::isnan(x) || std::isnan(y); return {un, !un && x < y, !un && x == y}; }
// CF = un|lt, ZF = un|eq, PF = un
inline bool fc_b(FCmp c) { return c.un || c.lt; }
inline bool fc_z(FCmp c) { return c.un || c.eq; }
inline uint32_t fc_flags(FCmp c) { return (fc_b(c) ? 1u : 0) | (c.un ? 4u : 0) | (fc_z(c) ? 0x40u : 0); }

// ------------------------------------------------------------------ integer flags (only where a consumer reads them)
inline bool parity8(uint32_t v) { return (__builtin_popcount(v & 0xff) & 1) == 0; }
// sub / cmp a - b (size bytes)
inline uint32_t szmask(int n) { return n == 4 ? 0xffffffffu : n == 2 ? 0xffffu : 0xffu; }
inline bool sub_of(uint32_t a, uint32_t b, int n) { uint32_t r = (a - b) & szmask(n); return (((a ^ b) & (a ^ r)) >> (8 * n - 1)) & 1; }
inline bool add_of(uint32_t a, uint32_t b, int n) { uint32_t r = (a + b) & szmask(n); return ((~(a ^ b) & (a ^ r)) >> (8 * n - 1)) & 1; }
inline bool add_cf(uint32_t a, uint32_t b, int n) { return uint64_t(a & szmask(n)) + uint64_t(b & szmask(n)) > szmask(n); }
inline bool sgn(uint32_t r, int n) { return (r >> (8 * n - 1)) & 1; }
inline uint32_t rotl32(uint32_t v, uint32_t c) { c &= 31; return c ? (v << c) | (v >> (32 - c)) : v; }
inline uint32_t rotr32(uint32_t v, uint32_t c) { c &= 31; return c ? (v >> c) | (v << (32 - c)) : v; }
inline uint32_t sext(uint32_t v, int n) { return n == 1 ? uint32_t(int32_t(int8_t(v))) : n == 2 ? uint32_t(int32_t(int16_t(v))) : v; }

// materialized EFLAGS (CF 1, PF 4, ZF 0x40, SF 0x80, OF 0x800) for flags read on several paths
inline uint32_t fl_logic(uint32_t r, int n) { r &= szmask(n); return (r == 0 ? 0x40u : 0) | (sgn(r, n) ? 0x80u : 0) | (parity8(r) ? 4u : 0); }
inline uint32_t fl_sub(uint32_t a, uint32_t b, int n) {
    a &= szmask(n); b &= szmask(n);
    return fl_logic(a - b, n) | (a < b ? 1u : 0) | (sub_of(a, b, n) ? 0x800u : 0);
}
inline uint32_t fl_add(uint32_t a, uint32_t b, int n) { return fl_logic(a + b, n) | (add_cf(a, b, n) ? 1u : 0) | (add_of(a, b, n) ? 0x800u : 0); }
inline bool fl_cc(int cc, uint32_t f) {   // cc: o no b ae e ne be a s ns p np l ge le g
    bool cf = f & 1, zf = f & 0x40, sf = f & 0x80, of = f & 0x800, pf = f & 4;
    switch (cc) {
        case 0: return of; case 1: return !of; case 2: return cf; case 3: return !cf; case 4: return zf; case 5: return !zf;
        case 6: return cf || zf; case 7: return !cf && !zf; case 8: return sf; case 9: return !sf; case 10: return pf; case 11: return !pf;
        case 12: return sf != of; case 13: return sf == of; case 14: return zf || sf != of; default: return !zf && sf == of;
    }
}

// ------------------------------------------------------------------ x87 (as vm.cpp execute_fp)
using reflex_f80::F80;
inline int fbits(const Machine& m) { int pc = (m.fpu_cw >> 8) & 3; return pc == 0 ? 24 : pc == 2 ? 53 : 64; }
inline int frc(const Machine& m) { return (m.fpu_cw >> 10) & 3; }
inline F80& st(Machine& m, int i) { return m.fpu[m.fpu.size() - 1 - i]; }
inline void fpush(Machine& m, const F80& v) { m.fpu.push_back(v); }
inline void fpop(Machine& m) { m.fpu.pop_back(); }
inline uint32_t fst32(Machine& m) { return reflex_f80::to_f32(st(m, 0), frc(m)); }
inline uint64_t fst64(Machine& m) { return reflex_f80::to_f64(st(m, 0), frc(m)); }
// op: 0 add, 1 mul, 4 sub, 5 subr, 6 div, 7 divr (the x87 opcode's reg field)
inline F80 farith(Machine& m, int op, const F80& a, const F80& b) {
    int bits = fbits(m), rc = frc(m);
    switch (op) {
        case 0: return reflex_f80::add(a, b, bits, rc);
        case 1: return reflex_f80::mul(a, b, bits, rc);
        case 4: return reflex_f80::add(a, b, bits, rc, true);
        case 5: return reflex_f80::add(b, a, bits, rc, true);
        case 6: return reflex_f80::div(a, b, bits, rc);
        default: return reflex_f80::div(b, a, bits, rc);
    }
}
inline FCmp f80cmp(const F80& a, const F80& b) { int c = reflex_f80::compare(a, b); return {c == 2, c == -1, c == 0}; }
inline int64_t fist(Machine& m, int size, int rc) {
    F80 v = st(m, 0); bool valid = v.kind == 0; int64_t n = 0;
    if (valid) {
        F80 ri = reflex_f80::round_to_integer(v, rc);
        if (ri.mant != 0) {
            int shift = 63 - ri.exp;
            if (ri.exp > 63 || (ri.exp == 63 && !(ri.sign && ri.mant == (1ull << 63)))) valid = false;
            else if (ri.exp == 63) n = INT64_MIN;
            if (valid && ri.exp < 63) { uint64_t mag = ri.mant >> shift; n = ri.sign ? -int64_t(mag) : int64_t(mag); }
        }
    }
    if (size == 8) { if (!valid) n = INT64_MIN; }
    else { int64_t lim = int64_t(1) << (8 * size - 1); if (!valid || n < -lim || n >= lim) n = -lim; }
    return n;
}

}  // namespace rst

// Register file of a transliterated function: locals, loaded from the Machine at entry.
#define RST_ENTRY()                                                                                                        \
    using namespace rst;                                                                                                   \
    [[maybe_unused]] uint32_t eax = m.r[0], ecx = m.r[1], edx = m.r[2], ebx = m.r[3], esp = m.r[4], ebp = m.r[5],           \
                              esi = m.r[6], edi = m.r[7];                                                                   \
    [[maybe_unused]] X x0 = fromm(m.xmm[0]), x1 = fromm(m.xmm[1]), x2 = fromm(m.xmm[2]), x3 = fromm(m.xmm[3]),               \
                       x4 = fromm(m.xmm[4]), x5 = fromm(m.xmm[5]), x6 = fromm(m.xmm[6]), x7 = fromm(m.xmm[7])
#define RST_SYNC_OUT()                                                                                                     \
    do { m.r[0] = eax; m.r[1] = ecx; m.r[2] = edx; m.r[3] = ebx; m.r[4] = esp; m.r[5] = ebp; m.r[6] = esi; m.r[7] = edi;    \
         m.xmm[0] = tom(x0); m.xmm[1] = tom(x1); m.xmm[2] = tom(x2); m.xmm[3] = tom(x3);                                     \
         m.xmm[4] = tom(x4); m.xmm[5] = tom(x5); m.xmm[6] = tom(x6); m.xmm[7] = tom(x7); } while (0)
#define RST_SYNC_IN()                                                                                                      \
    do { eax = m.r[0]; ecx = m.r[1]; edx = m.r[2]; ebx = m.r[3]; esp = m.r[4]; ebp = m.r[5]; esi = m.r[6]; edi = m.r[7];    \
         x0 = fromm(m.xmm[0]); x1 = fromm(m.xmm[1]); x2 = fromm(m.xmm[2]); x3 = fromm(m.xmm[3]);                             \
         x4 = fromm(m.xmm[4]); x5 = fromm(m.xmm[5]); x6 = fromm(m.xmm[6]); x7 = fromm(m.xmm[7]); } while (0)
// A call into other code (ported or original): the callee sees the same stack; its return address slot holds 0 while it runs
// (that ends the nested run) and the original's return address afterwards, so even the dead stack matches the original's.
#define RST_CALL(target, ra)                                                                                               \
    do { uint32_t t_ = (target); uint32_t slot_ = esp - 4; RST_SYNC_OUT(); m.call_at(t_, (ra)); RST_SYNC_IN();  \
         (void)slot_; } while (0)
// A tail jump to another function: it returns to our caller.
#define RST_TAILJMP(target)                                                                                                \
    do { uint32_t t_ = (target); RST_SYNC_OUT(); return m.tail_at(t_); } while (0)
#define RST_RET(n) do { RST_SYNC_OUT(); return m.ret(n); } while (0)
#define RST_OUTS (reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX | reflex_port::OUT_XMM_ALL | reflex_port::OUT_FPU)
// A call in a worker thread procedure that may block: the world runtime's wait stand-in ends a blocked thread's run by returning
// without popping the call (ESP still at the return address slot). The thread's run ends there too: the slot gets the original's
// return address and the native returns 0 (the next pc of the stopped thread), with the registers as at the call.
#define RST_CALLB(target, ra)                                                                                              \
    do { uint32_t t_ = (target); uint32_t slot_ = esp - 4; RST_SYNC_OUT(); m.call_at(t_, (ra));                              \
         if (m.r[4] == slot_) { wr32(m, slot_, (ra)); return 0u; }                                                          \
         RST_SYNC_IN(); wr32(m, slot_, (ra)); } while (0)
