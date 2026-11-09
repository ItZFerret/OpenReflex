// Register-exact SSE helpers for the math group's ports (and anyone who needs them).
//
// The exe's vector helpers use LTCG register conventions (arguments and results in xmm, scratch registers the callers may see),
// so their ports work on a copy of the guest's eight xmm registers (Regs) with the original's instruction sequence, lane by lane,
// and store all of them back: the native leaves every xmm register exactly as the original does. Floats go through x86:: (NaN
// rule); bit operations on the raw lanes.
#pragma once
#include <cstdint>
#include "../guest.h"
#include "../port.h"
#include "f80/f80.h"

namespace mathx {
using x86::bits;
using x86::flt;

// One xmm register as four raw 32-bit lanes (lane 0 low).
struct X {
    uint32_t u[4];
    float f(int i) const { return flt(u[i]); }
    static X ps(uint32_t a) { return {{a, a, a, a}}; }               // a constant splatted (a 16-byte .rdata vector)
    static X ss(uint32_t a) { return {{a, 0, 0, 0}}; }               // movss xmm, [mem]: upper lanes cleared
    static X v(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { return {{a, b, c, d}}; }
    static X fs(float a) { return ss(bits(a)); }
    uint64_t q(int i) const { return uint64_t(u[2 * i]) | uint64_t(u[2 * i + 1]) << 32; }   // double lanes
    void q(int i, uint64_t v) { u[2 * i] = uint32_t(v); u[2 * i + 1] = uint32_t(v >> 32); }
};

template <class F> inline X lanes(const X& a, const X& b, F f) { X r; for (int i = 0; i < 4; i++) r.u[i] = f(a.u[i], b.u[i]); return r; }
template <class F> inline X lanef(const X& a, const X& b, F f) { X r; for (int i = 0; i < 4; i++) r.u[i] = bits(f(flt(a.u[i]), flt(b.u[i]))); return r; }
template <class F> inline X lane0f(const X& a, const X& b, F f) { X r = a; r.u[0] = bits(f(flt(a.u[0]), flt(b.u[0]))); return r; }

// packed (dest op src)
inline X addps(const X& a, const X& b) { return lanef(a, b, [](float x, float y) { return x86::add(x, y); }); }
inline X subps(const X& a, const X& b) { return lanef(a, b, [](float x, float y) { return x86::sub(x, y); }); }
inline X mulps(const X& a, const X& b) { return lanef(a, b, [](float x, float y) { return x86::mul(x, y); }); }
inline X divps(const X& a, const X& b) { return lanef(a, b, [](float x, float y) { return x86::div(x, y); }); }
inline X maxps(const X& a, const X& b) { return lanef(a, b, [](float x, float y) { return x86::max(x, y); }); }
inline X minps(const X& a, const X& b) { return lanef(a, b, [](float x, float y) { return x86::min(x, y); }); }
inline X sqrtps(const X& a) { X r; for (int i = 0; i < 4; i++) r.u[i] = bits(x86::sqrt(a.f(i))); return r; }
inline X andps(const X& a, const X& b) { return lanes(a, b, [](uint32_t x, uint32_t y) { return x & y; }); }
inline X andnps(const X& a, const X& b) { return lanes(a, b, [](uint32_t x, uint32_t y) { return ~x & y; }); }
inline X orps(const X& a, const X& b) { return lanes(a, b, [](uint32_t x, uint32_t y) { return x | y; }); }
inline X xorps(const X& a, const X& b) { return lanes(a, b, [](uint32_t x, uint32_t y) { return x ^ y; }); }
inline X pcmpeqd(const X& a, const X& b) { return lanes(a, b, [](uint32_t x, uint32_t y) { return x == y ? ~0u : 0u; }); }
inline uint32_t mask(bool c) { return c ? ~0u : 0u; }
inline X cmpeqps(const X& a, const X& b) { return lanes(a, b, [](uint32_t x, uint32_t y) { return mask(flt(x) == flt(y)); }); }
inline X cmpltps(const X& a, const X& b) { return lanes(a, b, [](uint32_t x, uint32_t y) { return mask(flt(x) < flt(y)); }); }
inline X cmpleps(const X& a, const X& b) { return lanes(a, b, [](uint32_t x, uint32_t y) { return mask(flt(x) <= flt(y)); }); }
inline X cmpneqps(const X& a, const X& b) { return lanes(a, b, [](uint32_t x, uint32_t y) { return mask(!(flt(x) == flt(y))); }); }
inline X cmpnltps(const X& a, const X& b) { return lanes(a, b, [](uint32_t x, uint32_t y) { return mask(!(flt(x) < flt(y))); }); }
inline X cmpnleps(const X& a, const X& b) { return lanes(a, b, [](uint32_t x, uint32_t y) { return mask(!(flt(x) <= flt(y))); }); }
// shufps dest, src, imm: lanes 0, 1 from dest, 2, 3 from src
inline X shufps(const X& a, const X& b, int imm) { return {{a.u[imm & 3], a.u[(imm >> 2) & 3], b.u[(imm >> 4) & 3], b.u[(imm >> 6) & 3]}}; }
inline X unpcklps(const X& a, const X& b) { return {{a.u[0], b.u[0], a.u[1], b.u[1]}}; }
inline X unpckhps(const X& a, const X& b) { return {{a.u[2], b.u[2], a.u[3], b.u[3]}}; }
inline X movlhps(const X& a, const X& b) { return {{a.u[0], a.u[1], b.u[0], b.u[1]}}; }
inline X movhlps(const X& a, const X& b) { return {{b.u[2], b.u[3], a.u[2], a.u[3]}}; }

// scalar (lane 0; the upper lanes stay the destination's)
inline X addss(const X& a, const X& b) { return lane0f(a, b, [](float x, float y) { return x86::add(x, y); }); }
inline X subss(const X& a, const X& b) { return lane0f(a, b, [](float x, float y) { return x86::sub(x, y); }); }
inline X mulss(const X& a, const X& b) { return lane0f(a, b, [](float x, float y) { return x86::mul(x, y); }); }
inline X divss(const X& a, const X& b) { return lane0f(a, b, [](float x, float y) { return x86::div(x, y); }); }
inline X maxss(const X& a, const X& b) { return lane0f(a, b, [](float x, float y) { return x86::max(x, y); }); }
inline X minss(const X& a, const X& b) { return lane0f(a, b, [](float x, float y) { return x86::min(x, y); }); }
inline X sqrtss(const X& a, const X& b) { X r = a; r.u[0] = bits(x86::sqrt(b.f(0))); return r; }
inline X addss(const X& a, float b) { return addss(a, X::fs(b)); }
inline X subss(const X& a, float b) { return subss(a, X::fs(b)); }
inline X mulss(const X& a, float b) { return mulss(a, X::fs(b)); }
inline X divss(const X& a, float b) { return divss(a, X::fs(b)); }
inline X movss(const X& a, const X& b) { X r = a; r.u[0] = b.u[0]; return r; }   // register to register
inline X cmpnless(const X& a, const X& b) { X r = a; r.u[0] = mask(!(a.f(0) <= b.f(0))); return r; }
inline X cmpltss(const X& a, const X& b) { X r = a; r.u[0] = mask(a.f(0) < b.f(0)); return r; }
inline X cmpless(const X& a, const X& b) { X r = a; r.u[0] = mask(a.f(0) <= b.f(0)); return r; }
inline X cvtsi2ss(const X& a, int32_t v) { X r = a; r.u[0] = bits(float(v)); return r; }

// comiss / ucomiss outcome (ZF PF CF): unordered sets all three
struct Cmp { bool zf, pf, cf; };
inline Cmp comiss(float a, float b) {
    if (a != a || b != b) return {true, true, true};
    return {a == b, false, a < b};
}
inline uint32_t cmp_flags(const Cmp& c, uint32_t flags) {   // EFLAGS after comiss: ZF PF CF set, OF SF AF cleared
    flags &= ~0x8d5u;
    return flags | (c.cf ? 1u : 0u) | (c.pf ? 4u : 0u) | (c.zf ? 0x40u : 0u);
}

// double lanes (SSE2 scalar double), x86 NaN rule (first NaN operand quieted, else the default NaN)
inline double dbl(uint64_t b) { double d; std::memcpy(&d, &b, 8); return d; }
inline uint64_t dbits(double d) { uint64_t b; std::memcpy(&b, &d, 8); return b; }
inline bool disnan(uint64_t b) { return (b & 0x7fffffffffffffffull) > 0x7ff0000000000000ull; }
inline double dnan(double a, double b) {
    uint64_t x = dbits(a), y = dbits(b);
    return dbl(disnan(x) ? (x | 1ull << 51) : disnan(y) ? (y | 1ull << 51) : 0xfff8000000000000ull);
}
inline double dadd(double a, double b) { double r = a + b; return r != r ? dnan(a, b) : r; }
inline double dsub(double a, double b) { double r = a - b; return r != r ? dnan(a, b) : r; }
inline double dmul(double a, double b) { double r = a * b; return r != r ? dnan(a, b) : r; }
inline double ddiv(double a, double b) { double r = a / b; return r != r ? dnan(a, b) : r; }
inline double f2d(float a) { uint32_t b = bits(a); if (x86::isnan_bits(b)) { uint64_t s = uint64_t(b >> 31) << 63, m = uint64_t((b & 0x7fffffu) | 0x400000u) << 29; return dbl(s | 0x7ff0000000000000ull | m); } return double(a); }
inline float d2f(double a) { uint64_t b = dbits(a); if (disnan(b)) { uint32_t s = uint32_t(b >> 63) << 31, m = uint32_t(((b | 1ull << 51) >> 29) & 0x7fffffu); return flt(s | 0x7f800000u | m); } return float(a); }
// cvtss2sd dest, src (lane 0 of src -> double lane 0, double lane 1 kept); cvtsd2ss (double lane 0 -> float lane 0, rest kept);
// cvtpd2ps (two doubles -> float lanes 0, 1; lanes 2, 3 cleared)
inline X cvtss2sd(const X& a, const X& b) { X r = a; r.q(0, dbits(f2d(b.f(0)))); return r; }
inline X cvtsd2ss(const X& a, const X& b) { X r = a; r.u[0] = bits(d2f(dbl(b.q(0)))); return r; }
inline X cvtpd2ps(const X& b) { return {{bits(d2f(dbl(b.q(0)))), bits(d2f(dbl(b.q(1)))), 0, 0}}; }
template <class F> inline X lane0d(const X& a, const X& b, F f) { X r = a; r.q(0, dbits(f(dbl(a.q(0)), dbl(b.q(0))))); return r; }
inline X addsd(const X& a, const X& b) { return lane0d(a, b, dadd); }
inline X subsd(const X& a, const X& b) { return lane0d(a, b, dsub); }
inline X mulsd(const X& a, const X& b) { return lane0d(a, b, dmul); }
inline X divsd(const X& a, const X& b) { return lane0d(a, b, ddiv); }
inline X sd(uint64_t v) { X r{}; r.q(0, v); return r; }   // movsd xmm, [mem]: upper double cleared


// Guest memory as the transliterated code sees it (movaps / movups / movss / mov ...).
inline X ld(reflex_vm::Machine& m, uint32_t a) { return {{uint32_t(m.mem.read(a, 4)), uint32_t(m.mem.read(a + 4, 4)), uint32_t(m.mem.read(a + 8, 4)), uint32_t(m.mem.read(a + 12, 4))}}; }
inline void st(reflex_vm::Machine& m, uint32_t a, const X& x) { for (int i = 0; i < 4; i++) m.mem.write(a + 4 * i, 4, x.u[i]); }
inline uint32_t ld32(reflex_vm::Machine& m, uint32_t a) { return uint32_t(m.mem.read(a, 4)); }
inline void st32(reflex_vm::Machine& m, uint32_t a, uint32_t v) { m.mem.write(a, 4, v); }
inline uint64_t ld64(reflex_vm::Machine& m, uint32_t a) { return m.mem.read(a, 8); }
inline void st64(reflex_vm::Machine& m, uint32_t a, uint64_t v) { m.mem.write(a, 8, v); }
inline uint32_t ld16(reflex_vm::Machine& m, uint32_t a) { return uint32_t(m.mem.read(a, 2)); }
inline uint32_t ld8(reflex_vm::Machine& m, uint32_t a) { return uint32_t(m.mem.read(a, 1)); }
inline void st16(reflex_vm::Machine& m, uint32_t a, uint32_t v) { m.mem.write(a, 2, v); }
inline void st8(reflex_vm::Machine& m, uint32_t a, uint32_t v) { m.mem.write(a, 1, v); }

// A function's own stack frame as native storage (offsets from the aligned frame base); only for locals that do not escape
// (no address passed to a callee).
// Offsets may be negative down to -0x40 (pushes below the frame base).
template <int N> struct Local {
    static constexpr int kBias = 0x40;
    uint8_t b[N + kBias] = {};
    X ld(int o) const { X x; std::memcpy(x.u, b + kBias + o, 16); return x; }
    void st(int o, const X& x) { std::memcpy(b + kBias + o, x.u, 16); }
    uint32_t ld32(int o) const { uint32_t v; std::memcpy(&v, b + kBias + o, 4); return v; }
    void st32(int o, uint32_t v) { std::memcpy(b + kBias + o, &v, 4); }
    uint64_t ld64(int o) const { uint64_t v; std::memcpy(&v, b + kBias + o, 8); return v; }
    void st64(int o, uint64_t v) { std::memcpy(b + kBias + o, &v, 8); }
};

inline X movsd(const X& a, const X& b) { X r = a; r.u[0] = b.u[0]; r.u[1] = b.u[1]; return r; }
inline X cmpeqss(const X& a, const X& b) { X r = a; r.u[0] = mask(a.f(0) == b.f(0)); return r; }
inline X cmpneqss(const X& a, const X& b) { X r = a; r.u[0] = mask(!(a.f(0) == b.f(0))); return r; }
inline X cmpnltss(const X& a, const X& b) { X r = a; r.u[0] = mask(!(a.f(0) < b.f(0))); return r; }
inline X paddd(const X& a, const X& b) { return lanes(a, b, [](uint32_t x, uint32_t y) { return x + y; }); }
inline X psubd(const X& a, const X& b) { return lanes(a, b, [](uint32_t x, uint32_t y) { return x - y; }); }
inline X pcmpgtd(const X& a, const X& b) { return lanes(a, b, [](uint32_t x, uint32_t y) { return int32_t(x) > int32_t(y) ? ~0u : 0u; }); }
inline X movq(const X& a) { return {{a.u[0], a.u[1], 0, 0}}; }
// cvtps2pd: float lanes 0, 1 -> two doubles
inline X cvtps2pd(const X& a) { X r; r.q(0, dbits(f2d(a.f(0)))); r.q(1, dbits(f2d(a.f(1)))); return r; }
inline X cvtdq2ps(const X& a) { X r; for (int i = 0; i < 4; i++) r.u[i] = bits(float(int32_t(a.u[i]))); return r; }
inline X cvttps2dq(const X& a) { X r; for (int i = 0; i < 4; i++) r.u[i] = uint32_t(x86::cvtt(a.f(i))); return r; }
inline X cvtps2dq(const X& a) { X r; for (int i = 0; i < 4; i++) r.u[i] = uint32_t(x86::cvt(a.f(i))); return r; }

// The guest's xmm registers, copied in at entry and back at exit.
struct Regs {
    X x[8];
    explicit Regs(const reflex_vm::Machine& m) {
        for (int i = 0; i < 8; i++) x[i] = {{uint32_t(m.xmm[i].lo), uint32_t(m.xmm[i].lo >> 32), uint32_t(m.xmm[i].hi), uint32_t(m.xmm[i].hi >> 32)}};
    }
    void store(reflex_vm::Machine& m) const {
        for (int i = 0; i < 8; i++) {
            m.xmm[i].lo = uint64_t(x[i].u[0]) | uint64_t(x[i].u[1]) << 32;
            m.xmm[i].hi = uint64_t(x[i].u[2]) | uint64_t(x[i].u[3]) << 32;
        }
    }
    X& operator[](int i) { return x[i]; }
};

constexpr uint32_t OUT_XMM = reflex_port::OUT_XMM_ALL;
constexpr uint32_t OUT_GPR = reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX;

// The original's stack traffic below the caller's ESP (saved EBP, locals, return addresses of nested calls). Dead once the
// function returns, but not always harmless: later code can read uninitialized stack (padding lanes, unset fields) and copy it
// into live memory, so ports whose dead stack is observed this way write it as the original does.
struct Stack {
    reflex_vm::Machine& m;
    uint32_t esp, ebp;          // as at the function's entry (esp: the return address)
    static Stack entry(reflex_vm::Machine& m) { return Stack{m, m.r[4], m.r[5]}; }
    void w32(uint32_t a, uint32_t v) { m.mem.write(a, 4, v); }
    void w128(uint32_t a, const X& x) { for (int i = 0; i < 4; i++) m.mem.write(a + 4 * i, 4, x.u[i]); }
    // push ebp; mov ebp, esp; and esp, -align; sub esp, n: returns the new esp (the frame base)
    uint32_t enter(uint32_t n, uint32_t align = 16) { esp -= 4; w32(esp, ebp); ebp = esp; esp = (esp & ~(align - 1)) - n; return esp; }
    // the stack a nested call sees at its entry (its return address pushed)
    Stack call(uint32_t return_address) { Stack s{m, esp - 4, ebp}; w32(s.esp, return_address); return s; }
};

// EFLAGS as the transliterated integer code needs them (AF is not kept: LAHF gives it as 0).
struct Flags {
    bool cf = false, zf = false, sf = false, of = false, pf = false;
    bool b() const { return cf; }
    bool ae() const { return !cf; }
    bool e() const { return zf; }
    bool ne() const { return !zf; }
    bool be() const { return cf || zf; }
    bool a() const { return !cf && !zf; }
    bool l() const { return sf != of; }
    bool ge() const { return sf == of; }
    bool le() const { return zf || sf != of; }
    bool g() const { return !zf && sf == of; }
    bool s() const { return sf; }
    bool ns() const { return !sf; }
    bool p() const { return pf; }
    bool np() const { return !pf; }
    bool o() const { return of; }
    bool no() const { return !of; }
    uint32_t lahf() const { return (sf ? 0x80u : 0u) | (zf ? 0x40u : 0u) | (pf ? 4u : 0u) | 2u | (cf ? 1u : 0u); }
};
inline bool parity8(uint32_t r) { return !(__builtin_popcount(r & 0xffu) & 1); }
inline Flags f_logic(uint32_t r) { return {false, r == 0, (r >> 31) != 0, false, parity8(r)}; }
inline Flags f_sub(uint32_t a, uint32_t b, uint32_t r) { return {a < b, r == 0, (r >> 31) != 0, (((a ^ b) & (a ^ r)) >> 31) != 0, parity8(r)}; }
inline Flags f_add(uint32_t a, uint32_t b, uint32_t r) { return {r < a, r == 0, (r >> 31) != 0, ((~(a ^ b) & (a ^ r)) >> 31) != 0, parity8(r)}; }
inline Flags f_adc(uint32_t a, uint32_t b, uint32_t r, bool c) { return {((uint64_t(a) + b + c) >> 32) != 0, r == 0, (r >> 31) != 0, ((~(a ^ b) & (a ^ r)) >> 31) != 0, parity8(r)}; }
inline Flags f_sbb(uint32_t a, uint32_t b, uint32_t r, bool c) { return {uint64_t(a) < uint64_t(b) + c, r == 0, (r >> 31) != 0, (((a ^ b) & (a ^ r)) >> 31) != 0, parity8(r)}; }
inline Flags f_inc(uint32_t, uint32_t r, bool cf) { return {cf, r == 0, (r >> 31) != 0, r == 0x80000000u, parity8(r)}; }
inline Flags f_dec(uint32_t, uint32_t r, bool cf) { return {cf, r == 0, (r >> 31) != 0, r == 0x7fffffffu, parity8(r)}; }
inline Flags f_cmp(uint32_t a, uint32_t b) { return f_sub(a, b, a - b); }
inline Flags f_test(uint32_t a, uint32_t b) { return f_logic(a & b); }
inline Flags f_cmp8(uint32_t a, uint32_t b) { Flags f = f_cmp(a << 24, b << 24); f.pf = parity8(a - b); return f; }
inline Flags f_test8(uint32_t a, uint32_t b) { Flags f = f_logic((a & b) << 24); f.pf = parity8(a & b); return f; }
inline Flags f_cmp16(uint32_t a, uint32_t b) { Flags f = f_cmp(a << 16, b << 16); f.pf = parity8(a - b); return f; }
inline Flags f_test16(uint32_t a, uint32_t b) { Flags f = f_logic((a & b) << 16); f.pf = parity8(a & b); return f; }
inline Flags f_comisd(uint64_t a, uint64_t b) {
    double x = dbl(a), y = dbl(b);
    if (x != x || y != y) return {true, true, false, false, true};
    return {x < y, x == y, false, false, false};
}
inline Flags f_comiss(float a, float b) {
    if (a != a || b != b) return {true, true, false, false, true};
    return {a < b, a == b, false, false, false};
}

// x87 (the machine's stack: st(0) is back()), as the interpreter computes it (reflex_f80)
inline reflex_f80::F80& fst_(reflex_vm::Machine& m, int i) { return m.fpu[m.fpu.size() - 1 - size_t(i)]; }
inline void fpush(reflex_vm::Machine& m, const reflex_f80::F80& v) { m.fpu.push_back(v); }
inline int frc(const reflex_vm::Machine& m) { return (m.fpu_cw >> 10) & 3; }
inline Flags f_fcomi(const reflex_f80::F80& a, const reflex_f80::F80& b) {
    int c = reflex_f80::compare(a, b);
    if (c == 2) return {true, true, false, false, true};
    return {c < 0, c == 0, false, false, false};
}

// The original's integer registers as locals, with its stack in guest memory: for instruction-level transliterations (calls go
// to the original or its port through the machine, with every register synced).
struct Cpu {
    reflex_vm::Machine& m;
    Regs r;
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
    explicit Cpu(reflex_vm::Machine& mm)
        : m(mm), r(mm), eax(mm.r[0]), ecx(mm.r[1]), edx(mm.r[2]), ebx(mm.r[3]), esp(mm.r[4]), ebp(mm.r[5]), esi(mm.r[6]), edi(mm.r[7]) {}
    void sync_out() {
        r.store(m);
        m.r[0] = eax; m.r[1] = ecx; m.r[2] = edx; m.r[3] = ebx; m.r[4] = esp; m.r[5] = ebp; m.r[6] = esi; m.r[7] = edi;
    }
    void sync_in() {
        r = Regs(m);
        eax = m.r[0]; ecx = m.r[1]; edx = m.r[2]; ebx = m.r[3]; esp = m.r[4]; ebp = m.r[5]; esi = m.r[6]; edi = m.r[7];
    }
    void push(uint32_t v) { esp -= 4; m.mem.write(esp, 4, v); }
    uint32_t pop() { uint32_t v = uint32_t(m.mem.read(esp, 4)); esp += 4; return v; }
    // call: the return address slot gets 0 (the machine's return sentinel), ESP after it as the callee leaves it
    void call(uint32_t address) { sync_out(); m.call_function(address, nullptr, 0); sync_in(); }
    uint32_t ret(uint32_t cleanup) { sync_out(); return m.ret(cleanup); }
    uint32_t jump(uint32_t target) { sync_out(); return target; }
    // jmp to another function (a tail call): it runs as a call whose return-address slot is the caller's (0 while it runs), then
    // the native returns to that caller with ESP as the callee left it
    uint32_t tail(uint32_t address) {
        uint32_t ra = pop();
        sync_out(); m.call_function(address, nullptr, 0); sync_in();
        return ra;
    }
};
#define CPU_REFS(k)                                                                                                            \
    [[maybe_unused]] uint32_t &eax = k.eax, &ecx = k.ecx, &edx = k.edx, &ebx = k.ebx, &esp = k.esp, &ebp = k.ebp, &esi = k.esi,    \
                              &edi = k.edi;                                                                                  \
    [[maybe_unused]] X &x0 = k.r[0], &x1 = k.r[1], &x2 = k.r[2], &x3 = k.r[3], &x4 = k.r[4], &x5 = k.r[5], &x6 = k.r[6],          \
                       &x7 = k.r[7];                                                                                         \
    [[maybe_unused]] Flags fl{}

// Calls an original (or another group's) function from a register-level port: the xmm copy goes to the machine before the call
// and comes back after it.
inline void call_regs(reflex_vm::Machine& m, Regs& r, uint32_t address, std::initializer_list<uint32_t> args = {}) {
    r.store(m);
    reflex_game::Guest{m}.call(address, args);
    r = Regs(m);
}


}  // namespace mathx
