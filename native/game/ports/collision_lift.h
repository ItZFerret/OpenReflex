// Register-level transliteration support for the collision group's large functions (the ones of several kilobytes of LTCG
// SSE code): each instruction of the original becomes one C++ statement on this register file, in the original's order, with
// the original's stack frame kept on the guest stack (same addresses, so the dead stack later code may read uninitialized is
// the original's too). Semantics follow the world runtime's interpreter (vm.cpp): integer flags as set_add / set_logic, SSE
// through x86.h's NaN rules (min / max as the JIT: the second operand unless the comparison holds), x87 through reflex_f80.
#pragma once
#include <cmath>
#include "collision_util.h"
#include "f80/f80.h"

namespace collision {

struct Cpu {
    reflex_vm::Machine& m;
    Guest g;
    uint32_t r[8];
    X x[8];
    bool cf = false, pf = false, af = false, zf = false, sf = false, of = false;
    explicit Cpu(reflex_vm::Machine& mm) : m(mm), g{mm} { load(); }
    void load() {
        for (int i = 0; i < 8; i++) { r[i] = m.r[i]; x[i] = xr(g, i); }
        const uint32_t f = m.flags;
        cf = f & 1; pf = f & 4; af = f & 0x10; zf = f & 0x40; sf = f & 0x80; of = f & 0x800;
    }
    void store() {
        for (int i = 0; i < 8; i++) { m.r[i] = r[i]; xw(g, i, x[i]); }
        m.flags = (m.flags & ~0x8d5u) | (cf ? 1u : 0u) | (pf ? 4u : 0u) | (af ? 0x10u : 0u) | (zf ? 0x40u : 0u) | (sf ? 0x80u : 0u) | (of ? 0x800u : 0u);
    }
    // memory
    uint32_t rd32(uint32_t a) const { return uint32_t(m.mem.read(a, 4)); }
    uint32_t rd16(uint32_t a) const { return uint32_t(m.mem.read(a, 2)); }
    uint32_t rd8(uint32_t a) const { return uint32_t(m.mem.read(a, 1)); }
    void wr32(uint32_t a, uint32_t v) { m.mem.write(a, 4, v); }
    void wr16(uint32_t a, uint32_t v) { m.mem.write(a, 2, v & 0xffff); }
    void wr8(uint32_t a, uint32_t v) { m.mem.write(a, 1, v & 0xff); }
    X rd128(uint32_t a) const { return {{rd32(a), rd32(a + 4), rd32(a + 8), rd32(a + 12)}}; }
    void wr128(uint32_t a, const X& v) { for (int i = 0; i < 4; i++) wr32(a + 4 * i, v.u[i]); }
    X rdss(uint32_t a) const { return {{rd32(a), 0, 0, 0}}; }
    X rdsd(uint32_t a) const { return {{rd32(a), rd32(a + 4), 0, 0}}; }
    void push(uint32_t v) { r[4] -= 4; wr32(r[4], v); }
    uint32_t pop() { uint32_t v = rd32(r[4]); r[4] += 4; return v; }
    // flags (vm.cpp set_logic / set_add)
    static bool parity(uint32_t v) { return (__builtin_popcount(v & 0xff) & 1) == 0; }
    void logic(uint32_t res, int size) {
        const int bits = 8 * size; res &= mask(size);
        cf = false; of = false; zf = res == 0; sf = (res >> (bits - 1)) & 1; pf = parity(res);
    }
    static uint32_t mask(int size) { return size == 4 ? 0xffffffffu : size == 2 ? 0xffffu : 0xffu; }
    uint32_t addf(uint32_t a, uint32_t b, uint32_t carry, int size) {
        const uint64_t mk = mask(size); a &= uint32_t(mk); b &= uint32_t(mk);
        const uint64_t full = uint64_t(a) + b + carry; const uint32_t res = uint32_t(full & mk);
        logic(res, size); cf = full > mk; of = (((~(a ^ b) & (a ^ res)) >> (8 * size - 1)) & 1) != 0; return res;
    }
    uint32_t subf(uint32_t a, uint32_t b, uint32_t borrow, int size) {
        const uint64_t mk = mask(size); a &= uint32_t(mk); b &= uint32_t(mk);
        const int64_t full = int64_t(a) - int64_t(b) - int64_t(borrow); const uint32_t res = uint32_t(uint64_t(full) & mk);
        logic(res, size); cf = full < 0; of = ((((a ^ b) & (a ^ res)) >> (8 * size - 1)) & 1) != 0; return res;
    }
    uint32_t incf(uint32_t a, int size) { bool c = cf; uint32_t v = addf(a, 1, 0, size); cf = c; return v; }
    uint32_t decf(uint32_t a, int size) { bool c = cf; uint32_t v = subf(a, 1, 0, size); cf = c; return v; }
    uint32_t shlf(uint32_t v, unsigned count, int size) {
        count &= 31; if (!count) return v; const int bits = 8 * size; const uint64_t vv = v & mask(size);
        const uint32_t res = uint32_t((vv << count) & mask(size)); const bool c = (vv >> (bits - count)) & 1;
        logic(res, size); const bool o = (((res >> (bits - 1)) & 1) ^ c) != 0; cf = c; of = o && count == 1; return res;
    }
    uint32_t shrf(uint32_t v, unsigned count, int size) {
        count &= 31; if (!count) return v; const int bits = 8 * size; const uint64_t vv = v & mask(size);
        const uint32_t res = uint32_t(vv >> count); const bool c = (vv >> (count - 1)) & 1;
        logic(res, size); const bool o = (vv >> (bits - 1)) & 1; cf = c; of = o && count == 1; return res;
    }
    uint32_t sarf(uint32_t v, unsigned count, int size) {
        count &= 31; if (!count) return v; const int bits = 8 * size;
        const int64_t sv = int64_t(((uint64_t(v) & mask(size)) ^ (1ull << (bits - 1))) - (1ull << (bits - 1)));
        const uint32_t res = uint32_t(uint64_t(sv >> count) & mask(size)); const bool c = (sv >> (count - 1)) & 1;
        logic(res, size); cf = c; of = false; return res;
    }
    uint32_t imulf(int32_t a, int32_t b) { const int64_t p = int64_t(a) * b; const bool o = p < INT32_MIN || p > INT32_MAX; cf = of = o; return uint32_t(p); }
    void comis(float a, float b) {
        const bool u = a != a || b != b;
        zf = u || a == b; pf = u; cf = u || a < b; of = false; sf = false; af = false;
    }
    uint32_t lahf_ah() const { return (cf ? 1u : 0u) | (pf ? 4u : 0u) | (af ? 0x10u : 0u) | (zf ? 0x40u : 0u) | (sf ? 0x80u : 0u) | 2u; }
    // partial registers
    static uint32_t set_lo8(uint32_t r, uint32_t v) { return (r & 0xffffff00u) | (v & 0xff); }
    static uint32_t set_hi8(uint32_t r, uint32_t v) { return (r & 0xffff00ffu) | ((v & 0xff) << 8); }
    static uint32_t set_lo16(uint32_t r, uint32_t v) { return (r & 0xffff0000u) | (v & 0xffff); }
    // calls: the callee runs at the original's ESP (the slot of the return address then gets the original's return address,
    // as the dead stack the original leaves). Returns the callee's eax.
    void call(uint32_t target, uint32_t return_address) {
        const uint32_t esp = r[4];
        store(); m.r[4] = esp;
        m.call_at(target, return_address);   // the real return address in its slot for the whole call (lift_rt.h LIFT_CALL)
        load();
    }
    // jmp to another function (a tail call): it returns to our caller. Returns the next pc of the native.
    uint32_t tail_call(uint32_t target) {
        const uint32_t esp = r[4];
        store(); m.r[4] = esp;
        const uint32_t ra = m.tail_at(target);
        load();
        store();
        return ra;
    }
    uint32_t ret(uint32_t cleanup) { const uint32_t ra = pop(); r[4] += cleanup; store(); return ra; }
    // x87 (vm.cpp execute_fp): stack on the machine, precision / rounding from the control word
    int fbits() const { const int pc = (m.fpu_cw >> 8) & 3; return pc == 0 ? 24 : pc == 2 ? 53 : 64; }
    int frc() const { return (m.fpu_cw >> 10) & 3; }
    reflex_f80::F80& st(int i) { return m.fpu[m.fpu.size() - 1 - size_t(i)]; }
    void fpush(const reflex_f80::F80& v) { m.fpu.push_back(v); }
    void fpop() { m.fpu.pop_back(); }
    void fld32(uint32_t a) { fpush(reflex_f80::from_f32(rd32(a))); }
    void fld64(uint32_t a) { fpush(reflex_f80::from_f64(uint64_t(rd32(a)) | uint64_t(rd32(a + 4)) << 32)); }
    void fst32(uint32_t a) { wr32(a, reflex_f80::to_f32(st(0), frc())); }
    void fst64(uint32_t a) { const uint64_t v = reflex_f80::to_f64(st(0), frc()); wr32(a, uint32_t(v)); wr32(a + 4, uint32_t(v >> 32)); }
    // op: 0 add, 1 mul, 4 sub, 5 subr, 6 div, 7 divr (the x87 reg field)
    reflex_f80::F80 farith(int op, const reflex_f80::F80& a, const reflex_f80::F80& b) {
        const int bits = fbits(), rc = frc();
        switch (op) {
            case 0: return reflex_f80::add(a, b, bits, rc);
            case 1: return reflex_f80::mul(a, b, bits, rc);
            case 4: return reflex_f80::add(a, b, bits, rc, true);
            case 5: return reflex_f80::add(b, a, bits, rc, true);
            case 6: return reflex_f80::div(a, b, bits, rc);
            default: return reflex_f80::div(b, a, bits, rc);
        }
    }
};

// SSE2 doubles (vm.cpp sse64 / conversions)
inline bool nan64(uint64_t b) { return (b & 0x7fffffffffffffffull) > 0x7ff0000000000000ull; }
inline uint64_t lo64(const X& v) { return uint64_t(v.u[0]) | uint64_t(v.u[1]) << 32; }
inline uint64_t hi64(const X& v) { return uint64_t(v.u[2]) | uint64_t(v.u[3]) << 32; }
inline X with_lo64(X v, uint64_t d) { v.u[0] = uint32_t(d); v.u[1] = uint32_t(d >> 32); return v; }
inline X with_hi64(X v, uint64_t d) { v.u[2] = uint32_t(d); v.u[3] = uint32_t(d >> 32); return v; }
inline double dbl(uint64_t b) { double d; std::memcpy(&d, &b, 8); return d; }
inline uint64_t dbits(double d) { uint64_t b; std::memcpy(&b, &d, 8); return b; }
inline uint64_t sse64(int op, uint64_t a, uint64_t b) {
    if (nan64(a)) return a | 0x8000000000000ull;
    if (nan64(b)) return b | 0x8000000000000ull;
    const double x = dbl(a), y = dbl(b); double r;
    switch (op) { case 0: r = x + y; break; case 1: r = x - y; break; case 2: r = x * y; break; default: r = x / y; break; }
    const uint64_t out = dbits(r);
    return nan64(out) ? 0xfff8000000000000ull : out;
}
inline X addsd(const X& a, const X& b) { return with_lo64(a, sse64(0, lo64(a), lo64(b))); }
inline X subsd(const X& a, const X& b) { return with_lo64(a, sse64(1, lo64(a), lo64(b))); }
inline X mulsd(const X& a, const X& b) { return with_lo64(a, sse64(2, lo64(a), lo64(b))); }
inline X divsd(const X& a, const X& b) { return with_lo64(a, sse64(3, lo64(a), lo64(b))); }
inline uint64_t cvt_s2d(uint32_t v) {
    if (x86::isnan_bits(v)) return (uint64_t(v >> 31) << 63) | 0x7ff8000000000000ull | (uint64_t(v & 0x3fffff) << 29);
    return dbits(double(x86::flt(v)));
}
inline uint32_t cvt_d2s(uint64_t v) {
    if (nan64(v)) return uint32_t(((v >> 63) << 31) | 0x7fc00000u | ((v >> 29) & 0x3fffff));
    return x86::bits(float(dbl(v)));
}
// cvtps2pd / cvtpd2ps as the interpreter (host conversions)
inline X cvtps2pd(const X& v) { X r; r = with_lo64(r, dbits(double(v.f(0)))); return with_hi64(r, dbits(double(v.f(1)))); }
inline X cvtpd2ps(const X& v) { return {{x86::bits(float(dbl(lo64(v)))), x86::bits(float(dbl(hi64(v)))), 0, 0}}; }
inline X cvtss2sd(const X& d, const X& s) { return with_lo64(d, cvt_s2d(s.u[0])); }
inline X cvtsd2ss(X d, const X& s) { d.u[0] = cvt_d2s(lo64(s)); return d; }
inline X cvtsi2ss(X d, uint32_t v) { d.u[0] = x86::bits(float(int32_t(v))); return d; }
// cvttss2si / cvtss2si (vm.cpp: out of range or NaN gives 0x80000000)
inline uint32_t cvttss2si(uint32_t b) {
    const double x = double(x86::flt(b));
    if (std::isnan(x) || std::isinf(x)) return 0x80000000u;
    const double t = std::trunc(x);
    return (t < -2147483648.0 || t >= 2147483648.0) ? 0x80000000u : uint32_t(int32_t(t));
}

}  // namespace collision
