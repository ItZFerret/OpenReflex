// Helpers of the "crt" ports: msvcr90.dll 9.00.30729.1 (the VC90 runtime the game loads at 0x78520000) transliterated instruction by
// instruction (scratchpad crtag/tools/crtlift.py, notes/crt.md). The arithmetic, SSE and x87 semantics are the reference
// interpreter's (vm.cpp execute / execute_fp, x87 through reflex_f80); the SSE / memory part is a copy of engine_util.h.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include "../guest.h"
#include "../port.h"
#include "f80/f80.h"
#if defined(__aarch64__)
#include <arm_neon.h>
#endif

namespace crt {
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

inline X cvtdq2pd(uint64_t v) { return X{db(double(int32_t(uint32_t(v)))), db(double(int32_t(uint32_t(v >> 32))))}; }
inline X cvtpd2dq(const X& v, bool trunc) {
    X r; uint64_t l[2] = {v.lo, v.hi};
    for (int k = 0; k < 2; k++) r.sl(k, uint32_t(trunc ? to_i32_trunc(bd(l[k])) : to_i32_near(bd(l[k]))));
    return r;
}
inline uint32_t movmskpd(const X& v) { return uint32_t((v.lo >> 63) | ((v.hi >> 63) << 1)); }
// SSE2 packed integer family (vm.cpp: lanes as little-endian byte arrays)
enum PI { PI_pcmpeqd, PI_pcmpgtd, PI_paddd, PI_psubd, PI_pslld, PI_psrld, PI_psrad, PI_punpckldq,
          PI_pmaxsw, PI_pminsw, PI_pmaxub, PI_pminub, PI_paddb, PI_paddw, PI_psubb, PI_psubw, PI_paddsb, PI_paddsw, PI_paddusb, PI_paddusw,
          PI_psubsb, PI_psubsw, PI_psubusb, PI_psubusw, PI_pcmpeqb, PI_pcmpeqw, PI_pcmpgtb, PI_pcmpgtw, PI_pmullw, PI_pmulhw, PI_pmulhuw,
          PI_pmuludq, PI_pavgb, PI_pavgw, PI_psraw, PI_psrlw, PI_psllw, PI_packsswb, PI_packuswb, PI_packssdw, PI_punpcklbw, PI_punpckhbw,
          PI_punpcklwd, PI_punpckhwd, PI_punpckhdq, PI_psadbw, PI_pmaddwd };
// b: the second operand (an immediate count is passed as X{imm, 0} with imm = true)
inline X pint(int op, const X& a, const X& b, bool imm = false) {
    if (op <= PI_punpckldq) {
        X res; uint64_t c = imm ? (b.lo & 0xff) : b.lo;
        for (int i = 0; i < 4; i++) {
            uint32_t x = a.l(i), y;
            switch (op) {
                case PI_pcmpeqd: y = x == b.l(i) ? 0xffffffffu : 0; break;
                case PI_pcmpgtd: y = int32_t(x) > int32_t(b.l(i)) ? 0xffffffffu : 0; break;
                case PI_paddd: y = x + b.l(i); break;
                case PI_psubd: y = x - b.l(i); break;
                case PI_punpckldq: { uint32_t pick[4] = {a.l(0), b.l(0), a.l(1), b.l(1)}; y = pick[i]; break; }
                case PI_pslld: y = c < 32 ? (x << c) : 0; break;
                case PI_psrld: y = c < 32 ? (x >> c) : 0; break;
                default: y = uint32_t(int32_t(x) >> (c < 31 ? c : 31)); break;
            }
            res.sl(i, y);
        }
        return res;
    }
    uint8_t A[16], B[16], R[16]; std::memcpy(A, &a.lo, 8); std::memcpy(A + 8, &a.hi, 8); std::memcpy(B, &b.lo, 8); std::memcpy(B + 8, &b.hi, 8);
    auto w = [](const uint8_t* x, int i) { uint16_t v; std::memcpy(&v, x + 2 * i, 2); return v; };
    auto sw = [&](int i, uint32_t v) { uint16_t x = uint16_t(v); std::memcpy(R + 2 * i, &x, 2); };
    auto d = [](const uint8_t* x, int i) { uint32_t v; std::memcpy(&v, x + 4 * i, 4); return v; };
    auto sd_ = [&](int i, uint32_t v) { std::memcpy(R + 4 * i, &v, 4); };
    auto sat8s = [](int v) { return uint8_t(v < -128 ? -128 : v > 127 ? 127 : v); };
    auto sat8u = [](int v) { return uint8_t(v < 0 ? 0 : v > 255 ? 255 : v); };
    auto sat16s = [](int v) { return uint16_t(v < -32768 ? -32768 : v > 32767 ? 32767 : v); };
    auto sat16u = [](int v) { return uint16_t(v < 0 ? 0 : v > 65535 ? 65535 : v); };
    uint64_t c = imm ? (b.lo & 0xff) : b.lo;
    std::memset(R, 0, 16);
    switch (op) {
        case PI_pmaxsw: for (int i = 0; i < 8; i++) sw(i, int16_t(w(A, i)) > int16_t(w(B, i)) ? w(A, i) : w(B, i)); break;
        case PI_pminsw: for (int i = 0; i < 8; i++) sw(i, int16_t(w(A, i)) < int16_t(w(B, i)) ? w(A, i) : w(B, i)); break;
        case PI_pmaxub: for (int i = 0; i < 16; i++) R[i] = A[i] > B[i] ? A[i] : B[i]; break;
        case PI_pminub: for (int i = 0; i < 16; i++) R[i] = A[i] < B[i] ? A[i] : B[i]; break;
        case PI_paddb: for (int i = 0; i < 16; i++) R[i] = uint8_t(A[i] + B[i]); break;
        case PI_psubb: for (int i = 0; i < 16; i++) R[i] = uint8_t(A[i] - B[i]); break;
        case PI_paddw: for (int i = 0; i < 8; i++) sw(i, w(A, i) + w(B, i)); break;
        case PI_psubw: for (int i = 0; i < 8; i++) sw(i, w(A, i) - w(B, i)); break;
        case PI_paddsb: for (int i = 0; i < 16; i++) R[i] = sat8s(int8_t(A[i]) + int8_t(B[i])); break;
        case PI_psubsb: for (int i = 0; i < 16; i++) R[i] = sat8s(int8_t(A[i]) - int8_t(B[i])); break;
        case PI_paddusb: for (int i = 0; i < 16; i++) R[i] = sat8u(A[i] + B[i]); break;
        case PI_psubusb: for (int i = 0; i < 16; i++) R[i] = sat8u(int(A[i]) - B[i]); break;
        case PI_paddsw: for (int i = 0; i < 8; i++) sw(i, sat16s(int16_t(w(A, i)) + int16_t(w(B, i)))); break;
        case PI_psubsw: for (int i = 0; i < 8; i++) sw(i, sat16s(int16_t(w(A, i)) - int16_t(w(B, i)))); break;
        case PI_paddusw: for (int i = 0; i < 8; i++) sw(i, sat16u(int(w(A, i)) + w(B, i))); break;
        case PI_psubusw: for (int i = 0; i < 8; i++) sw(i, sat16u(int(w(A, i)) - w(B, i))); break;
        case PI_pcmpeqb: for (int i = 0; i < 16; i++) R[i] = A[i] == B[i] ? 0xff : 0; break;
        case PI_pcmpgtb: for (int i = 0; i < 16; i++) R[i] = int8_t(A[i]) > int8_t(B[i]) ? 0xff : 0; break;
        case PI_pcmpeqw: for (int i = 0; i < 8; i++) sw(i, w(A, i) == w(B, i) ? 0xffff : 0); break;
        case PI_pcmpgtw: for (int i = 0; i < 8; i++) sw(i, int16_t(w(A, i)) > int16_t(w(B, i)) ? 0xffff : 0); break;
        case PI_pmullw: for (int i = 0; i < 8; i++) sw(i, uint32_t(int32_t(int16_t(w(A, i))) * int16_t(w(B, i)))); break;
        case PI_pmulhw: for (int i = 0; i < 8; i++) sw(i, uint32_t((int32_t(int16_t(w(A, i))) * int16_t(w(B, i))) >> 16)); break;
        case PI_pmulhuw: for (int i = 0; i < 8; i++) sw(i, (uint32_t(w(A, i)) * w(B, i)) >> 16); break;
        case PI_pmuludq: { uint64_t lo = uint64_t(d(A, 0)) * d(B, 0), hi = uint64_t(d(A, 2)) * d(B, 2); std::memcpy(R, &lo, 8); std::memcpy(R + 8, &hi, 8); break; }
        case PI_pmaddwd: for (int i = 0; i < 4; i++) sd_(i, uint32_t(int32_t(int16_t(w(A, 2 * i))) * int16_t(w(B, 2 * i)) + int32_t(int16_t(w(A, 2 * i + 1))) * int16_t(w(B, 2 * i + 1)))); break;
        case PI_pavgb: for (int i = 0; i < 16; i++) R[i] = uint8_t((A[i] + B[i] + 1) >> 1); break;
        case PI_pavgw: for (int i = 0; i < 8; i++) sw(i, (uint32_t(w(A, i)) + w(B, i) + 1) >> 1); break;
        case PI_psadbw: { uint64_t s0 = 0, s1 = 0; for (int i = 0; i < 8; i++) { s0 += uint64_t(std::abs(int(A[i]) - B[i])); s1 += uint64_t(std::abs(int(A[i + 8]) - B[i + 8])); } std::memcpy(R, &s0, 8); std::memcpy(R + 8, &s1, 8); break; }
        case PI_psraw: for (int i = 0; i < 8; i++) sw(i, uint32_t(int32_t(int16_t(w(A, i))) >> (c < 15 ? c : 15))); break;
        case PI_psrlw: for (int i = 0; i < 8; i++) sw(i, c < 16 ? (w(A, i) >> c) : 0); break;
        case PI_psllw: for (int i = 0; i < 8; i++) sw(i, c < 16 ? uint32_t(w(A, i)) << c : 0); break;
        case PI_packsswb: for (int i = 0; i < 8; i++) { R[i] = sat8s(int16_t(w(A, i))); R[i + 8] = sat8s(int16_t(w(B, i))); } break;
        case PI_packuswb: for (int i = 0; i < 8; i++) { R[i] = sat8u(int16_t(w(A, i))); R[i + 8] = sat8u(int16_t(w(B, i))); } break;
        case PI_packssdw: for (int i = 0; i < 4; i++) { sw(i, sat16s(int32_t(d(A, i)) < -32768 ? -32768 : int32_t(d(A, i)) > 32767 ? 32767 : int32_t(d(A, i)))); sw(i + 4, sat16s(int32_t(d(B, i)) < -32768 ? -32768 : int32_t(d(B, i)) > 32767 ? 32767 : int32_t(d(B, i)))); } break;
        case PI_punpcklbw: for (int i = 0; i < 8; i++) { R[2 * i] = A[i]; R[2 * i + 1] = B[i]; } break;
        case PI_punpckhbw: for (int i = 0; i < 8; i++) { R[2 * i] = A[i + 8]; R[2 * i + 1] = B[i + 8]; } break;
        case PI_punpcklwd: for (int i = 0; i < 4; i++) { sw(2 * i, w(A, i)); sw(2 * i + 1, w(B, i)); } break;
        case PI_punpckhwd: for (int i = 0; i < 4; i++) { sw(2 * i, w(A, i + 4)); sw(2 * i + 1, w(B, i + 4)); } break;
        case PI_punpckhdq: { uint32_t pick[4] = {d(A, 2), d(B, 2), d(A, 3), d(B, 3)}; for (int i = 0; i < 4; i++) sd_(i, pick[i]); break; }
        default: break;
    }
    X res; std::memcpy(&res.lo, R, 8); std::memcpy(&res.hi, R + 8, 8);
    return res;
}
inline X pshuflw(const X& b, unsigned k) { X r = b; for (int i = 0; i < 4; i++) { uint32_t s = 16 * ((k >> (2 * i)) & 3); r.lo = (r.lo & ~(0xffffull << (16 * i))) | (((b.lo >> s) & 0xffff) << (16 * i)); } return r; }
inline X pshufhw(const X& b, unsigned k) { X r = b; for (int i = 0; i < 4; i++) { uint32_t s = 16 * ((k >> (2 * i)) & 3); r.hi = (r.hi & ~(0xffffull << (16 * i))) | (((b.hi >> s) & 0xffff) << (16 * i)); } return r; }
inline uint32_t pmovmskb(const X& b) { uint32_t m = 0; for (int i = 0; i < 8; i++) { m |= uint32_t((b.lo >> (8 * i + 7)) & 1) << i; m |= uint32_t((b.hi >> (8 * i + 7)) & 1) << (i + 8); } return m; }
inline X psrlq(const X& a, uint64_t c) { return c > 63 ? X{0, 0} : X{a.lo >> c, a.hi >> c}; }
inline X psllq(const X& a, uint64_t c) { return c > 63 ? X{0, 0} : X{a.lo << c, a.hi << c}; }
inline X paddq(const X& a, const X& b) { return X{a.lo + b.lo, a.hi + b.hi}; }
inline X psubq(const X& a, const X& b) { return X{a.lo - b.lo, a.hi - b.hi}; }
inline X psrldq(const X& a, unsigned imm) {
    unsigned c = (imm < 16 ? imm : 16) * 8; unsigned __int128 v = ((unsigned __int128)a.hi << 64) | a.lo; v = c >= 128 ? 0 : (v >> c);
    return X{uint64_t(v), uint64_t(v >> 64)};
}
inline X pslldq(const X& a, unsigned imm) {
    unsigned c = (imm < 16 ? imm : 16) * 8; unsigned __int128 v = ((unsigned __int128)a.hi << 64) | a.lo; v = c >= 128 ? 0 : (v << c);
    return X{uint64_t(v), uint64_t(v >> 64)};
}
inline uint32_t pextrw(const X& v, unsigned k) { k &= 7; uint64_t w = k < 4 ? (v.lo >> (16 * k)) : (v.hi >> (16 * (k - 4))); return uint32_t(w & 0xffff); }
inline X pinsrw(X x, uint32_t w, unsigned k) { k &= 7; uint64_t& lane = k < 4 ? x.lo : x.hi; unsigned s = 16 * (k & 3); lane = (lane & ~(0xffffull << s)) | (uint64_t(w & 0xffff) << s); return x; }
inline X minsd(const X& a, uint64_t b) { return sd<OP_MIN>(a, b); }
inline X maxsd(const X& a, uint64_t b) { return sd<OP_MAX>(a, b); }
inline X minpd(const X& a, const X& b) { return pd<OP_MIN>(a, b); }
inline X maxpd(const X& a, const X& b) { return pd<OP_MAX>(a, b); }
inline X sqrtpd(X a, const X& b) { a.lo = sqrt64(b.lo); a.hi = sqrt64(b.hi); return a; }

// ------------------------------------------------------------------ integer flags (vm.cpp set_add / set_logic and the instruction cases)
enum : uint32_t { F_CF = 1, F_PF = 4, F_ZF = 0x40, F_SF = 0x80, F_OF = 0x800, F_DF = 0x400 };
inline bool parity8(uint32_t v) { return (__builtin_popcount(v & 0xff) & 1) == 0; }
inline uint32_t szmask(int n) { return n == 4 ? 0xffffffffu : n == 2 ? 0xffffu : 0xffu; }
inline bool sub_of(uint32_t a, uint32_t b, int n) { uint32_t r = (a - b) & szmask(n); return (((a ^ b) & (a ^ r)) >> (8 * n - 1)) & 1; }
inline bool add_of(uint32_t a, uint32_t b, int n) { uint32_t r = (a + b) & szmask(n); return ((~(a ^ b) & (a ^ r)) >> (8 * n - 1)) & 1; }
inline bool add_cf(uint32_t a, uint32_t b, int n) { return uint64_t(a & szmask(n)) + uint64_t(b & szmask(n)) > szmask(n); }
inline bool sgn(uint32_t r, int n) { return (r >> (8 * n - 1)) & 1; }
inline uint32_t sext(uint32_t v, int n) { return n == 1 ? uint32_t(int32_t(int8_t(v))) : n == 2 ? uint32_t(int32_t(int16_t(v))) : v; }
// full flag words (CF PF ZF SF OF); fl: the flags before (for the bits an instruction leaves unchanged)
inline uint32_t fl_logic(uint32_t r, int n, uint32_t fl = 0) {
    r &= szmask(n); return (fl & ~(F_CF | F_PF | F_ZF | F_SF | F_OF)) | (r == 0 ? F_ZF : 0) | (sgn(r, n) ? F_SF : 0) | (parity8(r) ? F_PF : 0);
}
inline uint32_t fl_addc(uint32_t a, uint32_t b, uint32_t c, int n, bool sub, uint32_t fl = 0) {   // vm.cpp set_add
    uint64_t m = szmask(n), A = a & m, B = b & m, res; bool cf, of; int bits = 8 * n;
    if (sub) { int64_t full = int64_t(A) - int64_t(B) - int64_t(c); res = uint64_t(full) & m; cf = full < 0; of = ((((A ^ B) & (A ^ res)) >> (bits - 1)) & 1) != 0; }
    else { uint64_t full = A + B + c; res = full & m; cf = full > m; of = (((~(A ^ B) & (A ^ res)) >> (bits - 1)) & 1) != 0; }
    return fl_logic(uint32_t(res), n, fl) | (cf ? F_CF : 0) | (of ? F_OF : 0);
}
inline uint32_t fl_sub(uint32_t a, uint32_t b, int n, uint32_t fl = 0) { return fl_addc(a, b, 0, n, true, fl); }
inline uint32_t fl_add(uint32_t a, uint32_t b, int n, uint32_t fl = 0) { return fl_addc(a, b, 0, n, false, fl); }
inline uint32_t fl_incdec(uint32_t a, int n, bool dec, uint32_t fl) { return (fl_addc(a, 1, 0, n, dec, fl) & ~F_CF) | (fl & F_CF); }
// shifts and rotates by a count (vm.cpp: count masked to 31; 0 leaves the flags)
inline uint32_t fl_shift(int kind, uint32_t v, uint32_t count, int n, uint32_t fl) {   // kind 0 shl, 1 shr, 2 sar, 3 rol, 4 ror
    count &= 31; if (!count) return fl;
    int bits = 8 * n; uint64_t mk = szmask(n), V = v & mk, res; bool cf, of;
    if (kind == 0) { res = (V << count) & mk; cf = (V >> (bits - count)) & 1; fl = fl_logic(uint32_t(res), n, fl); of = (((res >> (bits - 1)) & 1) ^ cf) != 0; }
    else if (kind == 1) { res = V >> count; cf = (V >> (count - 1)) & 1; fl = fl_logic(uint32_t(res), n, fl); of = (V >> (bits - 1)) & 1; }
    else if (kind == 2) { int64_t sv = int64_t((V ^ (1ull << (bits - 1))) - (1ull << (bits - 1))); res = uint64_t(sv >> count) & mk; cf = (sv >> (count - 1)) & 1; fl = fl_logic(uint32_t(res), n, fl); of = false; }
    else {
        unsigned c = count % bits;
        res = kind == 3 ? (((V << c) | (V >> (bits - c))) & mk) : (((V >> c) | (V << (bits - c))) & mk);
        if (c == 0) res = V;
        cf = kind == 3 ? (res & 1) : ((res >> (bits - 1)) & 1); of = false;
    }
    return (fl & ~(F_CF | F_OF)) | (cf ? F_CF : 0) | ((of && count == 1) ? F_OF : 0);
}
inline uint32_t shift_res(int kind, uint32_t v, uint32_t count, int n) {
    count &= 31; if (!count) return v & szmask(n);
    int bits = 8 * n; uint64_t mk = szmask(n), V = v & mk;
    if (kind == 0) return uint32_t((V << count) & mk);
    if (kind == 1) return uint32_t(V >> count);
    if (kind == 2) { int64_t sv = int64_t((V ^ (1ull << (bits - 1))) - (1ull << (bits - 1))); return uint32_t(uint64_t(sv >> count) & mk); }
    unsigned c = count % bits; if (c == 0) return uint32_t(V);
    return uint32_t(kind == 3 ? (((V << c) | (V >> (bits - c))) & mk) : (((V >> c) | (V << (bits - c))) & mk));
}
// shld / shrd (vm.cpp: set_logic of the result, CF set when the last bit out is 1, not cleared otherwise)
inline uint32_t shd_res(bool left, uint32_t a, uint32_t b, uint32_t count, int n) {
    count &= 31; if (!count) return a & szmask(n); int bits = 8 * n; uint64_t mk = szmask(n), A = a & mk, B = b & mk;
    return uint32_t(left ? (((A << count) | (B >> (bits - count))) & mk) : (((A >> count) | (B << (bits - count))) & mk));
}
inline uint32_t fl_shd(bool left, uint32_t a, uint32_t b, uint32_t count, int n, uint32_t fl) {
    count &= 31; if (!count) return fl; int bits = 8 * n; uint64_t mk = szmask(n), A = a & mk;
    bool cf = left ? ((A >> (bits - count)) & 1) : ((A >> (count - 1)) & 1);
    fl = fl_logic(shd_res(left, a, b, count, n), n, fl); return cf ? (fl | F_CF) : fl;
}
inline bool fl_cc(int cc, uint32_t f) {   // cc: o no b ae e ne be a s ns p np l ge le g
    bool cf = f & F_CF, zf = f & F_ZF, sf = f & F_SF, of = f & F_OF, pf = f & F_PF;
    switch (cc) {
        case 0: return of; case 1: return !of; case 2: return cf; case 3: return !cf; case 4: return zf; case 5: return !zf;
        case 6: return cf || zf; case 7: return !cf && !zf; case 8: return sf; case 9: return !sf; case 10: return pf; case 11: return !pf;
        case 12: return sf != of; case 13: return sf == of; case 14: return zf || sf != of; default: return !zf && sf == of;
    }
}
// comisd / ucomisd / comiss (vm.cpp: CF PF ZF SF OF set from the compare) and fcomi (CF PF ZF only)
inline uint32_t fl_fcmp(FCmp c, uint32_t fl, bool sse) {
    fl &= sse ? ~(F_CF | F_PF | F_ZF | F_SF | F_OF | 0x10u) : ~(F_CF | F_PF | F_ZF);
    if (c.un) fl |= F_ZF | F_PF | F_CF; else if (c.lt) fl |= F_CF; else if (c.eq) fl |= F_ZF;
    return fl;
}

// ------------------------------------------------------------------ x87 (as vm.cpp execute_fp)
using reflex_f80::F80;
[[noreturn]] inline void vm_fail(const char* what) { throw reflex_vm::VmError(what); }
inline int fbits(const Machine& m) { int pc = (m.fpu_cw >> 8) & 3; if (pc == 1) vm_fail("x87 reserved precision control"); return pc == 0 ? 24 : pc == 2 ? 53 : 64; }
inline int frc(const Machine& m) { return (m.fpu_cw >> 10) & 3; }
inline F80& st(Machine& m, int i) { if (i >= int(m.fpu.size())) vm_fail("x87 stack underflow"); return m.fpu[m.fpu.size() - 1 - i]; }
inline void fpush(Machine& m, const F80& v) { if (m.fpu.size() >= 8) vm_fail("x87 stack overflow"); m.fpu.push_back(v); }
inline void fpop(Machine& m) { if (m.fpu.empty()) vm_fail("x87 stack underflow"); m.fpu.pop_back(); }
inline F80 fld80(Machine& m, uint32_t a) { uint8_t raw[10]; m.mem.read_bytes(a, raw, 10); return reflex_f80::from_f80(raw); }
inline void fst80(Machine& m, uint32_t a, const F80& v) { uint8_t raw[10]; reflex_f80::to_f80(v, raw); m.mem.write_bytes(a, raw, 10); }
inline uint32_t fst32(Machine& m) { return reflex_f80::to_f32(st(m, 0), frc(m)); }
inline uint64_t fst64(Machine& m) { return reflex_f80::to_f64(st(m, 0), frc(m)); }
inline F80 fint(int64_t v) { return reflex_f80::from_int(v); }
// op: 0 add, 1 mul, 4 sub, 5 subr, 6 div, 7 divr (the x87 opcode's reg field)
inline F80 farith(Machine& m, int op, const F80& a, const F80& b) {
    int bits = fbits(m), rc = frc(m);
    switch (op) {
        case 0: return reflex_f80::add(a, b, bits, rc);
        case 1: return reflex_f80::mul(a, b, bits, rc);
        case 4: return reflex_f80::add(a, b, bits, rc, true);
        case 5: return reflex_f80::add(b, a, bits, rc, true);
        case 6: return reflex_f80::div(a, b, bits, rc);
        case 7: return reflex_f80::div(b, a, bits, rc);
    }
    vm_fail("x87 op");
}
inline FCmp f80cmp(const F80& a, const F80& b) { int c = reflex_f80::compare(a, b); return {c == 2, c == -1, c == 0}; }
// fcom / fucom / ftst / ficom: C0 C2 C3 in the status word
inline void fcom_sw(Machine& m, const F80& a, const F80& b) {
    int c = reflex_f80::compare(a, b); uint16_t sw = m.fpu_sw & ~0x4500;
    if (c == 2) sw |= 0x4500; else if (c < 0) sw |= 0x100; else if (c == 0) sw |= 0x4000;
    m.fpu_sw = sw;
}
inline void fxam(Machine& m) {
    uint16_t sw = m.fpu_sw & ~0x4700;
    if (m.fpu.empty()) sw |= 0x4100;
    else {
        const F80& v = st(m, 0);
        if (v.sign) sw |= 0x200;
        if (v.kind == 2) sw |= 0x100; else if (v.kind == 1) sw |= 0x500; else if (v.mant == 0) sw |= 0x4000; else sw |= 0x400;
    }
    m.fpu_sw = sw;
}
inline uint32_t fnstsw(const Machine& m) { return uint32_t((m.fpu_sw & ~0x3800) | (((8 - m.fpu.size()) % 8) << 11)) & 0xffff; }
inline void fprem(Machine& m, bool nearest) {
    F80 x = st(m, 0), y = st(m, 1); uint16_t sw = m.fpu_sw & ~0x4700;
    int q3 = 0; st(m, 0) = reflex_f80::remainder(x, y, nearest, &q3);
    if (q3 & 4) sw |= 0x100;
    if (q3 & 2) sw |= 0x4000;
    if (q3 & 1) sw |= 0x200;
    m.fpu_sw = sw;
}
inline void fscale(Machine& m) {
    F80 a = st(m, 0), b = st(m, 1);
    if (a.kind != 0 || b.kind != 0) vm_fail("fscale special operand");
    F80 t = reflex_f80::round_to_integer(b, 3); int64_t n = 0;
    if (t.mant) { if (t.exp > 62) vm_fail("fscale range"); n = int64_t(t.mant >> (63 - t.exp)); if (t.sign) n = -n; }
    st(m, 0) = reflex_f80::scale(a, n);
}
// fpatan / fyl2x / fyl2xp1: st(1) = f(st(0), st(1)), popped (vm.cpp memoizes the same pure function)
inline void ftrans2(Machine& m, const char* op) { F80 a = st(m, 0), b = st(m, 1); F80 r = reflex_f80::transcendental(op, &a, &b, frc(m)); fpop(m); st(m, 0) = r; }
inline void ftrans1(Machine& m, const char* op) { F80 a = st(m, 0); st(m, 0) = reflex_f80::transcendental(op, &a, nullptr, frc(m)); }
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
inline void ldmxcsr(Machine& m, uint32_t v) { if (v & 0xe040) vm_fail("unsupported MXCSR"); m.mxcsr = v; }

// ------------------------------------------------------------------ integer instructions (vm.cpp execute, flags in a local word)
// set_add: a - b - c (sub) or a + b + c, masked to n bytes; fl gets CF PF ZF SF OF (other bits kept)
inline uint32_t f_add(uint32_t& fl, uint32_t a, uint32_t b, uint32_t c, int n, bool sub) {
    uint64_t mk = szmask(n), A = a & mk, B = b & mk, res; bool cf, of; int bits = 8 * n;
    if (sub) { int64_t full = int64_t(A) - int64_t(B) - int64_t(c); res = uint64_t(full) & mk; cf = full < 0; of = ((((A ^ B) & (A ^ res)) >> (bits - 1)) & 1) != 0; }
    else { uint64_t full = A + B + c; res = full & mk; cf = full > mk; of = (((~(A ^ B) & (A ^ res)) >> (bits - 1)) & 1) != 0; }
    fl = fl_logic(uint32_t(res), n, fl) | (cf ? F_CF : 0) | (of ? F_OF : 0);
    return uint32_t(res);
}
inline uint32_t f_logic(uint32_t& fl, uint32_t r, int n) { fl = fl_logic(r, n, fl); return r & szmask(n); }
// inc / dec: set_add with CF kept
inline uint32_t f_incdec(uint32_t& fl, uint32_t a, int n, bool dec) { uint32_t cf = fl & F_CF; uint32_t r = f_add(fl, a, 1, 0, n, dec); fl = (fl & ~F_CF) | cf; return r; }
// imul (two / three operand forms): product of the sign-extended operands, CF = OF = overflow of the n-byte result
inline uint32_t f_imul(uint32_t& fl, uint32_t x, uint32_t y, int n) {
    int64_t p = int64_t(int32_t(sext(x, n))) * int64_t(int32_t(sext(y, n)));
    int64_t lo = n == 1 ? INT8_MIN : n == 2 ? INT16_MIN : INT32_MIN, hi = n == 1 ? INT8_MAX : n == 2 ? INT16_MAX : INT32_MAX;
    fl = (fl & ~(F_CF | F_OF)) | ((p < lo || p > hi) ? (F_CF | F_OF) : 0);
    return uint32_t(p) & szmask(n);
}
// one-operand imul / mul / div / idiv on eax:edx (vm.cpp)
inline void f_imul1(uint32_t& fl, uint32_t& eax, uint32_t& edx, uint32_t src, int n) {
    int64_t p;
    if (n == 4) { p = int64_t(int32_t(eax)) * int64_t(int32_t(src)); eax = uint32_t(p); edx = uint32_t(uint64_t(p) >> 32); }
    else if (n == 2) { p = int64_t(int16_t(eax)) * int64_t(int16_t(src)); eax = (eax & 0xffff0000u) | (uint32_t(p) & 0xffff); edx = (edx & 0xffff0000u) | ((uint32_t(p) >> 16) & 0xffff); }
    else { p = int64_t(int8_t(eax)) * int64_t(int8_t(src)); eax = (eax & 0xffff0000u) | (uint32_t(p) & 0xffff); }
    int64_t lo = n == 1 ? INT8_MIN : n == 2 ? INT16_MIN : INT32_MIN, hi = n == 1 ? INT8_MAX : n == 2 ? INT16_MAX : INT32_MAX;
    fl = (fl & ~(F_CF | F_OF)) | ((p < lo || p > hi) ? (F_CF | F_OF) : 0);
}
inline void f_mul1(uint32_t& fl, uint32_t& eax, uint32_t& edx, uint32_t src, int n) {
    bool high;
    if (n == 4) { uint64_t p = uint64_t(eax) * src; eax = uint32_t(p); edx = uint32_t(p >> 32); high = edx != 0; }
    else if (n == 2) { uint32_t p = (eax & 0xffff) * (src & 0xffff); eax = (eax & 0xffff0000u) | (p & 0xffff); edx = (edx & 0xffff0000u) | (p >> 16); high = (p >> 16) != 0; }
    else { uint32_t p = (eax & 0xff) * (src & 0xff); eax = (eax & 0xffff0000u) | (p & 0xffff); high = (p >> 8) != 0; }
    fl = (fl & ~(F_CF | F_OF)) | (high ? (F_CF | F_OF) : 0);
}
inline void f_div1(uint32_t& eax, uint32_t& edx, uint32_t d, int n, bool sgn) {
    if (n == 4) d &= 0xffffffffu; else d &= szmask(n);
    if (d == 0) vm_fail("division by zero");
    if (n == 4) {
        uint64_t nn = (uint64_t(edx) << 32) | eax;
        if (!sgn) { uint64_t q = nn / d, rem = nn % d; if (q > 0xffffffffull) vm_fail("div overflow"); eax = uint32_t(q); edx = uint32_t(rem); }
        else { int64_t sn = int64_t(nn), sd = int32_t(d); int64_t q = sn / sd, rem = sn % sd; if (q < INT32_MIN || q > INT32_MAX) vm_fail("idiv overflow"); eax = uint32_t(q); edx = uint32_t(rem); }
    } else if (n == 2) {
        uint32_t nn = ((edx & 0xffff) << 16) | (eax & 0xffff); int64_t q, rem;
        if (!sgn) { q = nn / d; rem = nn % d; if (q > 0xffff) vm_fail("div overflow"); }
        else { int32_t sn = int32_t(nn), sd = int16_t(d); q = sn / sd; rem = sn % sd; if (q < INT16_MIN || q > INT16_MAX) vm_fail("idiv overflow"); }
        eax = (eax & 0xffff0000u) | (uint32_t(q) & 0xffff); edx = (edx & 0xffff0000u) | (uint32_t(rem) & 0xffff);
    } else {
        uint32_t nn = eax & 0xffff; int64_t q, rem;
        if (!sgn) { q = nn / d; rem = nn % d; if (q > 0xff) vm_fail("div overflow"); }
        else { int32_t sn = int16_t(nn), sd = int8_t(d); q = sn / sd; rem = sn % sd; if (q < INT8_MIN || q > INT8_MAX) vm_fail("idiv overflow"); }
        eax = (eax & 0xffff0000u) | (uint32_t(q) & 0xff) | ((uint32_t(rem) & 0xff) << 8);
    }
}
// string instructions (vm.cpp): size 1 / 2 / 4, DF from fl
inline void s_movs(Machine& m, uint32_t& esi, uint32_t& edi, uint32_t& ecx, uint32_t fl, int size, bool rep) {
    int32_t step = (fl & F_DF) ? -size : size;
    uint32_t count = rep ? ecx : 1;
    uint64_t bytes = uint64_t(count) * uint32_t(size);
    if (rep && step > 0 && count >= 16 && bytes <= (1u << 24) && uint64_t(edi) + bytes <= 0x100000000ull &&
        uint64_t(esi) + bytes <= 0x100000000ull && (uint64_t(edi) + bytes <= esi || uint64_t(esi) + bytes <= edi)) {
        uint8_t stackbuf[4096]; std::unique_ptr<uint8_t[]> heap; uint8_t* buf = stackbuf;
        if (bytes > sizeof stackbuf) { heap.reset(new uint8_t[bytes]); buf = heap.get(); }
        m.mem.read_block(esi, buf, bytes); esi += uint32_t(bytes);
        m.mem.write_block(edi, buf, bytes); edi += uint32_t(bytes);
        count = 0;
    }
    for (uint32_t i = 0; i < count; i++) { m.mem.write(edi, size, m.mem.read(esi, size)); esi += step; edi += step; }
    if (rep) ecx = 0;
}
inline void s_stos(Machine& m, uint32_t& edi, uint32_t& ecx, uint32_t eax, uint32_t fl, int size, bool rep) {
    int32_t step = (fl & F_DF) ? -size : size;
    uint32_t count = rep ? ecx : 1;
    uint64_t bytes = uint64_t(count) * uint32_t(size);
    if (rep && step > 0 && count >= 16 && bytes <= (1u << 24) && uint64_t(edi) + bytes <= 0x100000000ull) {
        uint8_t stackbuf[4096]; std::unique_ptr<uint8_t[]> heap; uint8_t* buf = stackbuf;
        if (bytes > sizeof stackbuf) { heap.reset(new uint8_t[bytes]); buf = heap.get(); }
        for (uint64_t i = 0; i < bytes; i += uint32_t(size)) std::memcpy(buf + i, &eax, size_t(size));
        m.mem.write_block(edi, buf, bytes); edi += uint32_t(bytes);
        count = 0;
    }
    for (uint32_t i = 0; i < count; i++) { m.mem.write(edi, size, eax & szmask(size)); edi += step; }
    if (rep) ecx = 0;
}
// scas / cmps / lods; rep: 0 none, 1 rep / repe, 2 repne
inline void s_cmp(Machine& m, int op, uint32_t& eax, uint32_t& esi, uint32_t& edi, uint32_t& ecx, uint32_t& fl, int size, int rep) {
    int32_t step = (fl & F_DF) ? -size : size;
    if (rep && ecx == 0) return;
    for (;;) {
        if (op == 0) { f_add(fl, eax & szmask(size), uint32_t(m.mem.read(edi, size)), 0, size, true); edi += step; }
        else if (op == 1) { f_add(fl, uint32_t(m.mem.read(esi, size)), uint32_t(m.mem.read(edi, size)), 0, size, true); esi += step; edi += step; }
        else { uint32_t v = uint32_t(m.mem.read(esi, size)); esi += step;
               eax = size == 4 ? v : size == 2 ? ((eax & 0xffff0000u) | v) : ((eax & 0xffffff00u) | v); }
        if (!rep) break;
        ecx--;
        if (ecx == 0) break;
        if (op != 2) { bool zf = (fl & F_ZF) != 0; if ((rep == 1 && !zf) || (rep == 2 && zf)) break; }
    }
}
// bsf / bsr: ZF only; the destination is written only for a nonzero source
inline bool f_bsx(uint32_t& fl, uint32_t v, bool fwd, uint32_t& out) {
    if (!v) { fl |= F_ZF; return false; }
    fl &= ~F_ZF; out = fwd ? uint32_t(__builtin_ctz(v)) : uint32_t(31 - __builtin_clz(v)); return true;
}

// x87 helpers not above
inline uint32_t fpu_rc(const Machine& m) { return (m.fpu_cw >> 10) & 3; }
inline void fcmp_any(Machine& m, uint32_t& fl, const F80& a, const F80& b, bool to_flags) {
    int c = reflex_f80::compare(a, b);
    if (to_flags) { uint32_t f = fl & ~(F_CF | F_PF | F_ZF); if (c == 2) f |= F_CF | F_PF | F_ZF; else if (c < 0) f |= F_CF; else if (c == 0) f |= F_ZF; fl = f; }
    else { uint16_t sw = m.fpu_sw & ~0x4500; if (c == 2) sw |= 0x4500; else if (c < 0) sw |= 0x100; else if (c == 0) sw |= 0x4000; m.fpu_sw = sw; }
}
inline F80 fsx(uint64_t v, int bytes) { int bits = 8 * bytes; uint64_t s = 1ull << (bits - 1); if (bytes >= 8) return reflex_f80::from_int(int64_t(v)); return reflex_f80::from_int(int64_t(((v & ((1ull << bits) - 1)) ^ s) - s)); }

// __SEH_prolog4 (7858cb80) and __SEH_epilog4 (7858cbc5) inlined at their call sites, with the caller's real return address ra:
// the same stack, frame and TEB (fs:[0]) writes and flags as the called originals. (As ports they could not be verified: the
// prolog returns with ESP below its entry, and the epilog reads its own return address.)
inline void seh_prolog4(Machine& m, uint32_t& eax, uint32_t& ebx, uint32_t& esi, uint32_t& edi, uint32_t& esp, uint32_t& ebp,
                        uint32_t& fl, uint32_t ra) {
    esp -= 4; wr32(m, esp, ra);                                         // call
    esp -= 4; wr32(m, esp, 0x7858cbdeu);                                // push _except_handler4
    { uint32_t v = rd32(m, 0x7ffd0000u); esp -= 4; wr32(m, esp, v); }   // push fs:[0]
    eax = rd32(m, esp + 0x10);
    wr32(m, esp + 0x10, ebp);
    ebp = esp + 0x10;
    esp = f_add(fl, esp, eax, 0, 4, true);
    esp -= 4; wr32(m, esp, ebx); esp -= 4; wr32(m, esp, esi); esp -= 4; wr32(m, esp, edi);
    eax = rd32(m, 0x785ba2a8u);
    { uint32_t r = f_logic(fl, rd32(m, ebp - 4) ^ eax, 4); wr32(m, ebp - 4, r); }
    eax = f_logic(fl, eax ^ ebp, 4);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, ebp - 0x18, esp);
    { uint32_t v = rd32(m, ebp - 8); esp -= 4; wr32(m, esp, v); }       // push the return address
    eax = rd32(m, ebp - 4);
    wr32(m, ebp - 4, 0xfffffffeu);
    wr32(m, ebp - 8, eax);
    eax = ebp - 0x10;
    wr32(m, 0x7ffd0000u, eax);
    esp += 4;                                                           // ret
}
inline void seh_epilog4(Machine& m, uint32_t& ecx, uint32_t& ebx, uint32_t& esi, uint32_t& edi, uint32_t& esp, uint32_t& ebp, uint32_t ra) {
    esp -= 4; wr32(m, esp, ra);                                         // call
    ecx = rd32(m, ebp - 0x10);
    wr32(m, 0x7ffd0000u, ecx);
    ecx = rd32(m, esp); esp += 4;
    edi = rd32(m, esp); esp += 4;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    esp -= 4; wr32(m, esp, ecx);
    esp += 4;                                                           // ret
}

// msvcp90's __EH_prolog3 (784b9567: eax = handler, [esp + 4] = frame size; cookie at 78506998) and __EH_epilog3 (784b961b),
// inlined at their call sites with the real return address, as the SEH pair above.
inline void eh_prolog3(Machine& m, uint32_t& eax, uint32_t& ebx, uint32_t& esi, uint32_t& edi, uint32_t& esp, uint32_t& ebp,
                       uint32_t& fl, uint32_t ra, uint32_t cookie) {
    esp -= 4; wr32(m, esp, ra);                                         // call
    esp -= 4; wr32(m, esp, eax);                                        // push eax
    { uint32_t v = rd32(m, 0x7ffd0000u); esp -= 4; wr32(m, esp, v); }   // push fs:[0]
    eax = esp + 0xc;
    esp = f_add(fl, esp, rd32(m, esp + 0xc), 0, 4, true);
    esp -= 4; wr32(m, esp, ebx); esp -= 4; wr32(m, esp, esi); esp -= 4; wr32(m, esp, edi);
    wr32(m, eax, ebp);
    ebp = eax;
    eax = rd32(m, cookie);
    eax = f_logic(fl, eax ^ ebp, 4);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, ebp - 0x10, esp);
    { uint32_t v = rd32(m, ebp - 4); esp -= 4; wr32(m, esp, v); }       // push the return address
    wr32(m, ebp - 4, 0xffffffffu);
    eax = ebp - 0xc;
    wr32(m, 0x7ffd0000u, eax);
    esp += 4;                                                           // ret
}
inline void eh_epilog3(Machine& m, uint32_t& ecx, uint32_t& ebx, uint32_t& esi, uint32_t& edi, uint32_t& esp, uint32_t& ebp, uint32_t ra) {
    esp -= 4; wr32(m, esp, ra);                                         // call
    ecx = rd32(m, ebp - 0xc);
    wr32(m, 0x7ffd0000u, ecx);
    ecx = rd32(m, esp); esp += 4;
    edi = rd32(m, esp); esp += 4;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    esp -= 4; wr32(m, esp, ecx);
    esp += 4;                                                           // ret
}

// Resumes the original code at pc with the current registers, until the transliterated function's own return (its return address
// slot at entry, esp0, holds 0 meanwhile: the original's ret ends the nested run). Returns the caller's return address.
inline uint32_t resume_original(Machine& m, uint32_t pc, uint32_t esp0) {
    const uint32_t ra = rd32(m, esp0);
    wr32(m, esp0, 0);
    m.run(pc);
    wr32(m, esp0, ra);
    return ra;
}

}  // namespace crt

// Register file of a transliterated function: locals, loaded from the Machine at entry (fl: the flags word, DF included).
#define CRT_ENTRY()                                                                                                        \
    using namespace crt;                                                                                                   \
    [[maybe_unused]] const uint32_t esp0 = m.r[4];                                                                         \
    [[maybe_unused]] uint32_t eax = m.r[0], ecx = m.r[1], edx = m.r[2], ebx = m.r[3], esp = m.r[4], ebp = m.r[5],           \
                              esi = m.r[6], edi = m.r[7];                                                                   \
    [[maybe_unused]] uint32_t fl = m.flags
#define CRT_XMM()                                                                                                          \
    [[maybe_unused]] X x0 = fromm(m.xmm[0]), x1 = fromm(m.xmm[1]), x2 = fromm(m.xmm[2]), x3 = fromm(m.xmm[3]),               \
                       x4 = fromm(m.xmm[4]), x5 = fromm(m.xmm[5]), x6 = fromm(m.xmm[6]), x7 = fromm(m.xmm[7])
#define CRT_OUT()                                                                                                          \
    do { m.r[0] = eax; m.r[1] = ecx; m.r[2] = edx; m.r[3] = ebx; m.r[4] = esp; m.r[5] = ebp; m.r[6] = esi; m.r[7] = edi;    \
         m.flags = fl; } while (0)
#define CRT_IN()                                                                                                           \
    do { eax = m.r[0]; ecx = m.r[1]; edx = m.r[2]; ebx = m.r[3]; esp = m.r[4]; ebp = m.r[5]; esi = m.r[6]; edi = m.r[7];    \
         fl = m.flags; } while (0)
#define CRT_OUTX()                                                                                                         \
    do { CRT_OUT(); m.xmm[0] = tom(x0); m.xmm[1] = tom(x1); m.xmm[2] = tom(x2); m.xmm[3] = tom(x3);                          \
         m.xmm[4] = tom(x4); m.xmm[5] = tom(x5); m.xmm[6] = tom(x6); m.xmm[7] = tom(x7); } while (0)
#define CRT_INX()                                                                                                          \
    do { CRT_IN(); x0 = fromm(m.xmm[0]); x1 = fromm(m.xmm[1]); x2 = fromm(m.xmm[2]); x3 = fromm(m.xmm[3]);                  \
         x4 = fromm(m.xmm[4]); x5 = fromm(m.xmm[5]); x6 = fromm(m.xmm[6]); x7 = fromm(m.xmm[7]); } while (0)
// A call into other code (ported or original): the callee sees the same stack; its return address slot holds 0 while it runs
// (that ends the nested run) and the original's return address afterwards (when the callee popped it).
#define CRT_CALL(OUT, IN, target, ra)                                                                                      \
    do { uint32_t t_ = (target); uint32_t s_ = esp - 4; OUT(); m.call_at(t_, (ra)); IN(); (void)s_;                          \
         } while (0)
// A tail jump to another function: it returns to our caller.
#define CRT_TAIL(OUT, target)                                                                                              \
    do { uint32_t t_ = (target); OUT(); return m.tail_at(t_); } while (0)
#define CRT_RET(OUT, n) do { OUT(); return m.ret(n); } while (0)
#define CRT_RESUME(OUT, pc) do { OUT(); return crt::resume_original(m, (pc), esp0); } while (0)
#define CRT_OUTS (reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX | reflex_port::OUT_XMM_ALL | reflex_port::OUT_FPU)
