// lift_rt.h: the one shared runtime header for the native/game lifter (native/game/tools/lift). Every function lifted from
// MXReflex.exe by the lifter is an instruction-level transliteration on guest registers held in C++ locals (eax..edi, esp,
// fl, x0..x7), with the exact semantics of the reference interpreter (libreflexworld's vm.cpp / jit.cpp):
//   - SSE single / double through sse32 / sse64 (x86 NaN rules, the min / max "source on NaN" rule, rcp / rsqrt tables);
//   - x87 through reflex_f80 honouring the control word (precision + rounding);
//   - integer semantics of x86 (wraparound, signed shifts), with flags carried in one EFLAGS word `fl` only where a consumer
//     reads them, or captured per defining instruction (FCmp / fl_sub / fl_add / fl_logic) for the fused-branch style.
// The stack frame stays on the guest stack at the original's addresses (pushes, locals, arguments built for callees), so a
// callee sees the same memory; every call (LIFT_CALL / LIFT_CALLB / LIFT_TAILJMP) syncs the register file and rewrites the
// return-address slot with the original's return address, so even the dead stack reads as the original leaves it.
//
// This header is the union of the group headers the lifter used to emit per group (bike_util.h, engine_util.h, fx_util.h,
// anim_util.h, ui_util.h): the bike SSE / x87 / captured-flag helpers, the fx / anim rcp / rsqrt tables, the anim integer
// division helpers, and the ui materialized-EFLAGS helpers, all in one namespace `lift` with LIFT_* macros.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include "guest.h"
#include "port.h"
#include "f80/f80.h"

namespace lift {
using reflex_vm::Machine;
constexpr uint32_t FS_BASE = 0x7ffd0000;   // vm.cpp: fs-relative operands address the TEB here

// ------------------------------------------------------------------ memory
inline uint32_t rd8(Machine& m, uint32_t a) { return uint32_t(m.mem.read(a, 1)); }
inline uint32_t rd16(Machine& m, uint32_t a) { return uint32_t(m.mem.read(a, 2)); }
inline uint32_t rd32(Machine& m, uint32_t a) { return uint32_t(m.mem.read(a, 4)); }
inline uint64_t rd64(Machine& m, uint32_t a) { return m.mem.read(a, 8); }
inline void wr8(Machine& m, uint32_t a, uint32_t v) { m.mem.write(a, 1, v & 0xff); }
inline void wr16(Machine& m, uint32_t a, uint32_t v) { m.mem.write(a, 2, v & 0xffff); }
inline void wr32(Machine& m, uint32_t a, uint32_t v) { m.mem.write(a, 4, v); }
inline void wr64(Machine& m, uint32_t a, uint64_t v) { m.mem.write(a, 8, v); }

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

// packed / scalar single
template <int OP> inline X ps(X a, const X& b) { for (int i = 0; i < 4; i++) a.sl(i, sse32(OP, a.l(i), b.l(i))); return a; }
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
inline uint32_t sext(uint32_t v, int n) { return n == 1 ? uint32_t(int32_t(int8_t(v))) : n == 2 ? uint32_t(int32_t(int16_t(v))) : v; }
inline bool sbb_of(uint32_t a, uint32_t b, uint32_t c) { int64_t r = int64_t(int32_t(a)) - int64_t(int32_t(b)) - int64_t(c); return r < INT32_MIN || r > INT32_MAX; }
// div / idiv r/m32 (EDX:EAX by d), as vm.cpp (division by zero / overflow are faults there too)
inline void udiv32(uint32_t& eax, uint32_t& edx, uint32_t d) {
    if (d == 0) throw reflex_vm::VmError("division by zero");
    uint64_t n = (uint64_t(edx) << 32) | eax, q = n / d, r = n % d;
    if (q > 0xffffffffull) throw reflex_vm::VmError("div overflow");
    eax = uint32_t(q); edx = uint32_t(r);
}
inline void sdiv32(uint32_t& eax, uint32_t& edx, uint32_t d) {
    if (d == 0) throw reflex_vm::VmError("division by zero");
    int64_t n = int64_t((uint64_t(edx) << 32) | eax), sd = int32_t(d);
    if (sd == -1 && n == INT64_MIN) throw reflex_vm::VmError("idiv overflow");
    int64_t q = n / sd, r = n % sd;
    if (q < INT32_MIN || q > INT32_MAX) throw reflex_vm::VmError("idiv overflow");
    eax = uint32_t(q); edx = uint32_t(r);
}

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


// ------------------------------------------------------------------ EFLAGS as vm.cpp keeps them
enum : uint32_t { F_CF = 1, F_PF = 4, F_AF = 0x10, F_ZF = 0x40, F_SF = 0x80, F_DF = 0x400, F_OF = 0x800 };
inline uint32_t f_logic(uint32_t fl, uint32_t res, int n) {
    res &= szmask(n);
    uint32_t f = fl & ~(F_CF | F_PF | F_ZF | F_SF | F_OF);
    if (res == 0) f |= F_ZF;
    if (sgn(res, n)) f |= F_SF;
    if (parity8(res)) f |= F_PF;
    return f;
}
// a - b - carry / a + b + carry (cmp, sub, sbb, neg / add, adc)
inline uint32_t f_sub(uint32_t fl, uint32_t a, uint32_t b, int n, uint32_t carry = 0) {
    uint64_t m = szmask(n); a &= m; b &= m;
    int64_t full = int64_t(a) - int64_t(b) - int64_t(carry); uint32_t res = uint32_t(uint64_t(full) & m);
    uint32_t f = f_logic(fl, res, n);
    if (full < 0) f |= F_CF;
    if ((((a ^ b) & (a ^ res)) >> (8 * n - 1)) & 1) f |= F_OF;
    return f;
}
inline uint32_t f_add(uint32_t fl, uint32_t a, uint32_t b, int n, uint32_t carry = 0) {
    uint64_t m = szmask(n); a &= m; b &= m;
    uint64_t full = uint64_t(a) + b + carry; uint32_t res = uint32_t(full & m);
    uint32_t f = f_logic(fl, res, n);
    if (full > m) f |= F_CF;
    if (((~(a ^ b) & (a ^ res)) >> (8 * n - 1)) & 1) f |= F_OF;
    return f;
}
inline uint32_t f_inc(uint32_t fl, uint32_t a, int n) { return (f_add(fl, a, 1, n) & ~F_CF) | (fl & F_CF); }
inline uint32_t f_dec(uint32_t fl, uint32_t a, int n) { return (f_sub(fl, a, 1, n) & ~F_CF) | (fl & F_CF); }
// shifts by count (masked to 5 bits by the caller; 0 leaves the flags)
inline uint32_t f_shl(uint32_t fl, uint32_t v, unsigned c, int n) {
    if (!c) return fl;
    int bits = 8 * n; uint32_t res = uint32_t((uint64_t(v) << c) & szmask(n)); bool cf = (uint64_t(v) >> (bits - c)) & 1;
    uint32_t f = f_logic(fl, res, n); bool of = (sgn(res, n) ^ cf) != 0;
    return (f & ~(F_CF | F_OF)) | (cf ? F_CF : 0) | ((of && c == 1) ? F_OF : 0);
}
inline uint32_t f_shr(uint32_t fl, uint32_t v, unsigned c, int n) {
    if (!c) return fl;
    v &= szmask(n); uint32_t res = v >> c; bool cf = (v >> (c - 1)) & 1;
    uint32_t f = f_logic(fl, res, n); bool of = sgn(v, n);
    return (f & ~(F_CF | F_OF)) | (cf ? F_CF : 0) | ((of && c == 1) ? F_OF : 0);
}
inline uint32_t f_sar(uint32_t fl, uint32_t v, unsigned c, int n) {
    if (!c) return fl;
    int64_t sv = int64_t(int32_t(sext(v, n)));
    uint32_t res = uint32_t(uint64_t(sv >> c) & szmask(n)); bool cf = (sv >> (c - 1)) & 1;
    uint32_t f = f_logic(fl, res, n);
    return (f & ~(F_CF | F_OF)) | (cf ? F_CF : 0);
}
inline uint32_t f_mulover(uint32_t fl, bool over) { return (fl & ~(F_CF | F_OF)) | (over ? (F_CF | F_OF) : 0); }
// comiss / ucomiss / comisd / ucomisd
inline uint32_t f_comi(uint32_t fl, double a, double b) {
    uint32_t f = fl & ~(F_CF | F_PF | F_ZF | F_SF | F_OF | F_AF);
    if (std::isnan(a) || std::isnan(b)) f |= F_ZF | F_PF | F_CF; else if (a < b) f |= F_CF; else if (a == b) f |= F_ZF;
    return f;
}
inline uint32_t f_comiss(uint32_t fl, uint32_t a, uint32_t b) { return f_comi(fl, double(bf(a)), double(bf(b))); }
inline uint32_t f_comisd(uint32_t fl, uint64_t a, uint64_t b) { return f_comi(fl, bd(a), bd(b)); }
// conditions
inline bool c_o(uint32_t f) { return f & F_OF; }
inline bool c_b(uint32_t f) { return f & F_CF; }
inline bool c_e(uint32_t f) { return f & F_ZF; }
inline bool c_be(uint32_t f) { return f & (F_CF | F_ZF); }
inline bool c_s(uint32_t f) { return f & F_SF; }
inline bool c_p(uint32_t f) { return f & F_PF; }
inline bool c_l(uint32_t f) { return bool(f & F_SF) != bool(f & F_OF); }
inline bool c_le(uint32_t f) { return (f & F_ZF) || c_l(f); }
// comiss outcomes (fused compare + branch): CF = unordered | below, ZF = unordered | equal, PF = unordered
inline bool fb_b(float a, float b) { return !(a >= b); }
inline bool fb_be(float a, float b) { return !(a > b); }
inline bool fb_e(float a, float b) { return !(a < b) && !(a > b); }
inline bool fb_p(float a, float b) { return std::isnan(a) || std::isnan(b); }

// ------------------------------------------------------------------ x87 transcendentals (exactly as vm.cpp execute_fp: the same
// reflex_f80::transcendental with the control word's rounding; the memo there is only a cache of this pure function)
inline int x87_rc(const Machine& m) { return (m.fpu_cw >> 10) & 3; }
// fldpi / fldl2e / fldln2 / fldl2t / fldlg2: op = "pi", "l2e", "ln2", "l2t", "lg2"
inline void x87_ldconst(Machine& m, const char* op) { fpush(m, reflex_f80::transcendental(op, nullptr, nullptr, x87_rc(m))); }
// fpatan / fyl2x / fyl2xp1: st(1) op st(0), pop, result in st(0)
inline void x87_binary(Machine& m, const char* op) {
    F80 a = st(m, 0), b = st(m, 1);
    F80 r = reflex_f80::transcendental(op, &a, &b, x87_rc(m));
    fpop(m); st(m, 0) = r;
}
inline void x87_fpatan(Machine& m) { x87_binary(m, "fpatan"); }
inline void x87_fyl2x(Machine& m) { x87_binary(m, "fyl2x"); }
inline void x87_fyl2xp1(Machine& m) { x87_binary(m, "fyl2xp1"); }
inline void x87_f2xm1(Machine& m) { F80 a = st(m, 0); st(m, 0) = reflex_f80::transcendental("f2xm1", &a, nullptr, x87_rc(m)); }
inline void x87_fsin(Machine& m) { F80 a = st(m, 0); st(m, 0) = reflex_f80::transcendental("fsin", &a, nullptr, x87_rc(m)); m.fpu_sw &= ~0x400; }
inline void x87_fcos(Machine& m) { F80 a = st(m, 0); st(m, 0) = reflex_f80::transcendental("fcos", &a, nullptr, x87_rc(m)); m.fpu_sw &= ~0x400; }
inline void x87_fptan(Machine& m) { F80 a = st(m, 0); st(m, 0) = reflex_f80::transcendental("fptan", &a, nullptr, x87_rc(m)); fpush(m, reflex_f80::from_int(1)); m.fpu_sw &= ~0x400; }

// ------------------------------------------------------------------ x87 loads
inline F80 f80_m32(Machine& m, uint32_t a) { return reflex_f80::from_f32(rd32(m, a)); }
inline F80 f80_m64(Machine& m, uint32_t a) { return reflex_f80::from_f64(rd64(m, a)); }
inline F80 f80_i32(uint32_t v) { return reflex_f80::from_int(int64_t(int32_t(v))); }
inline F80 f80_i16(uint32_t v) { return reflex_f80::from_int(int64_t(int16_t(v))); }
inline F80 f80_i64(uint64_t v) { return reflex_f80::from_int(int64_t(v)); }
inline void fpush_copy(Machine& m, int i) { F80 t = st(m, i); fpush(m, t); }

}  // namespace lift

// Register file of a transliterated function: locals, loaded from the Machine at entry (fl: EFLAGS).
#define LIFT_ENTRY()                                                                                                       \
    using namespace lift;                                                                                                  \
    [[maybe_unused]] uint32_t eax = m.r[0], ecx = m.r[1], edx = m.r[2], ebx = m.r[3], esp = m.r[4], ebp = m.r[5],           \
                              esi = m.r[6], edi = m.r[7], fl = m.flags;                                                     \
    [[maybe_unused]] X x0 = fromm(m.xmm[0]), x1 = fromm(m.xmm[1]), x2 = fromm(m.xmm[2]), x3 = fromm(m.xmm[3]),               \
                       x4 = fromm(m.xmm[4]), x5 = fromm(m.xmm[5]), x6 = fromm(m.xmm[6]), x7 = fromm(m.xmm[7])
#define LIFT_SYNC_OUT()                                                                                                    \
    do { m.r[0] = eax; m.r[1] = ecx; m.r[2] = edx; m.r[3] = ebx; m.r[4] = esp; m.r[5] = ebp; m.r[6] = esi; m.r[7] = edi;    \
         m.flags = fl; m.xmm[0] = tom(x0); m.xmm[1] = tom(x1); m.xmm[2] = tom(x2); m.xmm[3] = tom(x3);                       \
         m.xmm[4] = tom(x4); m.xmm[5] = tom(x5); m.xmm[6] = tom(x6); m.xmm[7] = tom(x7); } while (0)
#define LIFT_SYNC_IN()                                                                                                     \
    do { eax = m.r[0]; ecx = m.r[1]; edx = m.r[2]; ebx = m.r[3]; esp = m.r[4]; ebp = m.r[5]; esi = m.r[6]; edi = m.r[7];    \
         fl = m.flags; x0 = fromm(m.xmm[0]); x1 = fromm(m.xmm[1]); x2 = fromm(m.xmm[2]); x3 = fromm(m.xmm[3]);               \
         x4 = fromm(m.xmm[4]); x5 = fromm(m.xmm[5]); x6 = fromm(m.xmm[6]); x7 = fromm(m.xmm[7]); } while (0)
// A call into other code (ported or original, an import / stand-in, or an exe jmp [IAT] thunk): the callee sees the same stack and
// every register it leaves is taken over. Its return address slot holds the original's return address for the whole call
// (Machine::call_at: the nested run ends when the call returns there, stop_pc / stop_esp; the JIT never chains past it), so a
// thread that blocks inside the callee (the boot's baton scheduler parks it) shows the original's stack, e.g. in a world image
// written meanwhile, and the dead stack below ESP afterwards reads as the original leaves it. (Before 2026-10-18 only import
// calls did this; other calls held 0 in the slot while running: 0 return addresses in blocked threads' stacks in world images.)
namespace lift {
inline bool import_target(Machine& m, uint32_t t) {
    if (t >= 0x916000u) return m.import_names.count(t) != 0;   // import sentinels: world runtime 0xfe000000.., boot 0xec000000..
    return t >= 0x401000u && t < 0x916000u && m.mem.mapped(t) && uint32_t(m.mem.read(t, 2)) == 0x25ffu;
}
// Helpers that return to their caller with ESP below the call's slot (they allocate the caller's frame): the nested run stops at
// the return address whatever ESP is (port.h OUT_ESP_BELOW for Verify).
// (Machine::call_at knows them: __SEH_prolog4, __EH_prolog3_catch, __alloca_probe.)
inline void call_import(Machine& m, uint32_t target, uint32_t ra) { m.call_at(target, ra); }
inline uint32_t tail_jump(Machine& m, uint32_t target) { return m.tail_at(target); }
}  // namespace lift
#define LIFT_CALL(target, ra)                                                                                              \
    do { uint32_t t_ = (target), sp_ = esp; LIFT_SYNC_OUT(); m.call_at(t_, (ra)); (void)sp_;                                                       \
         LIFT_SYNC_IN(); } while (0)
// A tail jump to another function: it returns to our caller (its return address stays in place).
#define LIFT_TAILJMP(target)                                                                                               \
    do { uint32_t t_ = (target); LIFT_SYNC_OUT(); return lift::tail_jump(m, t_); } while (0)
#define LIFT_RET(n) do { LIFT_SYNC_OUT(); return m.ret(n); } while (0)
// A call in a worker thread procedure that may block: the world runtime's wait stand-in ends a blocked thread's run by
// returning pc 0 without popping the call (ESP still at the return-address slot). The thread's run ends there too: the slot
// gets the original's return address and the native returns 0 (the next pc of the stopped thread), registers as at the call.
#define LIFT_CALLB(target, ra)                                                                                             \
    do { uint32_t t_ = (target); uint32_t slot_ = esp - 4; LIFT_SYNC_OUT();                                                  \
         m.call_at(t_, (ra));                                                                                                 \
         if (m.r[4] == slot_) { wr32(m, slot_, (ra)); return 0u; }                                                         \
         LIFT_SYNC_IN(); wr32(m, slot_, (ra)); } while (0)
#define LIFT_BAD(pc) throw reflex_vm::VmError("lift port: unexpected control flow at " #pc)
#define LIFT_OUTS (reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX | reflex_port::OUT_XMM_ALL | reflex_port::OUT_FPU)
#define LIFT_PUSH(v) do { uint32_t pv_ = (v); esp -= 4; wr32(m, esp, pv_); } while (0)
#define LIFT_POP() (esp += 4, rd32(m, esp - 4))
