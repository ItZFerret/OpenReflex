// Register-level transliteration runtime of the world group's "node" ports: the original's registers, flags, x87 stack and
// stack frame are the machine's own (m.r / m.flags / m.xmm / m.fpu, the guest stack), so a port written against these helpers
// leaves exactly the state the original leaves (including the dead stack and every scratch register). Semantics follow the
// interpreter (vm.cpp): flags CF PF ZF SF OF, SSE NaN rules of sse32 / sse64 (min / max: the source unless the comparison holds), x87 through
// reflex_f80 with the control word's precision and rounding.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include "../port.h"
#include "f80/f80.h"

namespace xc {

using reflex_vm::Machine;
using reflex_vm::Xmm;
namespace F = reflex_f80;

constexpr uint32_t CF = 1, PF = 4, ZF = 0x40, SF = 0x80, OF = 0x800;

// ---- memory
inline uint32_t rd32(Machine& m, uint32_t a) { return uint32_t(m.mem.read(a, 4)); }
inline uint32_t rd16(Machine& m, uint32_t a) { return uint32_t(m.mem.read(a, 2)); }
inline uint32_t rd8(Machine& m, uint32_t a) { return uint32_t(m.mem.read(a, 1)); }
inline uint64_t rd64(Machine& m, uint32_t a) { return m.mem.read(a, 8); }
inline void wr32(Machine& m, uint32_t a, uint32_t v) { m.mem.write(a, 4, v); }
inline void wr16(Machine& m, uint32_t a, uint32_t v) { m.mem.write(a, 2, v & 0xffff); }
inline void wr8(Machine& m, uint32_t a, uint32_t v) { m.mem.write(a, 1, v & 0xff); }
inline void wr64(Machine& m, uint32_t a, uint64_t v) { m.mem.write(a, 8, v); }
inline Xmm rd128(Machine& m, uint32_t a) { return Xmm{m.mem.read(a, 8), m.mem.read(a + 8, 8)}; }
inline void wr128(Machine& m, uint32_t a, const Xmm& v) { m.mem.write(a, 8, v.lo); m.mem.write(a + 8, 8, v.hi); }

// ---- stack
inline void push(Machine& m, uint32_t v) { m.r[4] -= 4; wr32(m, m.r[4], v); }
inline uint32_t pop(Machine& m) { uint32_t v = rd32(m, m.r[4]); m.r[4] += 4; return v; }
// call target (ported or original) as the original does: the return address slot keeps ret_addr afterwards (the machine runs the
// callee with a 0 sentinel there); ESP ends as the callee leaves it (stdcall cleanup included).
inline void call(Machine& m, uint32_t target, uint32_t ret_addr) {
    m.call_at(target, ret_addr);   // the real return address in its slot for the whole call (lift_rt.h LIFT_CALL)
}

// a jump out of the function (tail call, or a shared tail elsewhere): runs target on the current state until it returns
// through the native's return address slot (entry_sp); returns that return address (the machine continues there).
inline uint32_t tail(Machine& m, uint32_t target, uint32_t entry_sp) {
    const uint32_t ra = rd32(m, entry_sp);
    wr32(m, entry_sp, 0);
    m.run(target);
    wr32(m, entry_sp, ra);
    return ra;
}

// ---- sub-registers
inline uint32_t r16(const Machine& m, int i) { return m.r[i] & 0xffff; }
inline uint32_t r8l(const Machine& m, int i) { return m.r[i] & 0xff; }
inline uint32_t r8h(const Machine& m, int i) { return (m.r[i] >> 8) & 0xff; }
inline void s16(Machine& m, int i, uint32_t v) { m.r[i] = (m.r[i] & 0xffff0000u) | (v & 0xffff); }
inline void s8l(Machine& m, int i, uint32_t v) { m.r[i] = (m.r[i] & 0xffffff00u) | (v & 0xff); }
inline void s8h(Machine& m, int i, uint32_t v) { m.r[i] = (m.r[i] & 0xffff00ffu) | ((v & 0xff) << 8); }

// ---- flags (as Machine::set_logic / set_add)
inline uint64_t mask(int size) { return size == 8 ? ~0ull : (1ull << (8 * size)) - 1; }
inline bool parity(uint64_t v) { return (__builtin_popcount(unsigned(v & 0xff)) & 1) == 0; }
inline void logic(Machine& m, uint64_t res, int size) {
    const int bits = 8 * size; res &= mask(size);
    uint32_t f = m.flags & ~(CF | PF | ZF | SF | OF);
    if (res == 0) f |= ZF;
    if ((res >> (bits - 1)) & 1) f |= SF;
    if (parity(res)) f |= PF;
    m.flags = f;
}
inline uint64_t addf(Machine& m, uint64_t a, uint64_t b, uint64_t carry, int size, bool sub) {
    const int bits = 8 * size; const uint64_t mk = mask(size); uint64_t res; bool cf, of;
    a &= mk; b &= mk;
    if (sub) {
        __int128 full = __int128(a) - __int128(b) - __int128(carry);
        res = uint64_t(full) & mk; cf = full < 0;
        of = ((((a ^ b) & (a ^ res)) >> (bits - 1)) & 1) != 0;
    } else {
        unsigned __int128 full = (unsigned __int128)a + b + carry;
        res = uint64_t(full) & mk; cf = full > mk;
        of = (((~(a ^ b) & (a ^ res)) >> (bits - 1)) & 1) != 0;
    }
    logic(m, res, size);
    if (cf) m.flags |= CF;
    if (of) m.flags |= OF;
    return res;
}
inline bool fcf(const Machine& m) { return m.flags & CF; }
inline bool fzf(const Machine& m) { return m.flags & ZF; }
inline bool fsf(const Machine& m) { return m.flags & SF; }
inline bool fof(const Machine& m) { return m.flags & OF; }
inline bool fpf(const Machine& m) { return m.flags & PF; }
// shifts (count already masked to 0..31; 0: no change)
inline uint64_t shift(Machine& m, int kind, uint64_t v, unsigned count, int size) {   // kind 0 shl, 1 shr, 2 sar, 3 rol, 4 ror
    const int bits = 8 * size; v &= mask(size);
    if (!count) return v;
    uint64_t res; bool cf, of;
    if (kind == 0) { res = (v << count) & mask(size); cf = (v >> (bits - count)) & 1; logic(m, res, size); of = (((res >> (bits - 1)) & 1) ^ cf) != 0; }
    else if (kind == 1) { res = v >> count; cf = (v >> (count - 1)) & 1; logic(m, res, size); of = (v >> (bits - 1)) & 1; }
    else if (kind == 2) {
        int64_t sv = int64_t((v ^ (1ull << (bits - 1))) - (1ull << (bits - 1)));
        res = uint64_t(sv >> count) & mask(size); cf = (sv >> (count - 1)) & 1; logic(m, res, size); of = false;
    } else {
        unsigned c = count % bits;
        res = kind == 3 ? (((v << c) | (v >> (bits - c))) & mask(size)) : (((v >> c) | (v << (bits - c))) & mask(size));
        if (c == 0) res = v;
        cf = kind == 3 ? (res & 1) : ((res >> (bits - 1)) & 1); of = false;
    }
    m.flags = (m.flags & ~(CF | OF)) | (cf ? CF : 0) | ((of && count == 1) ? OF : 0);
    return res;
}
inline void imul_flags(Machine& m, int64_t p, int size) {
    int64_t lo = size == 1 ? INT8_MIN : size == 2 ? INT16_MIN : INT32_MIN, hi = size == 1 ? INT8_MAX : size == 2 ? INT16_MAX : INT32_MAX;
    bool over = p < lo || p > hi;
    m.flags = (m.flags & ~(CF | OF)) | (over ? (CF | OF) : 0);
}
inline int64_t sx(uint64_t v, int n) { return n == 1 ? int64_t(int8_t(v)) : n == 2 ? int64_t(int16_t(v)) : int64_t(int32_t(uint32_t(v))); }

// ---- SSE (vm.cpp's sse32 / sse64)
inline float bf(uint32_t b) { float f; std::memcpy(&f, &b, 4); return f; }
inline uint32_t fb(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
inline double bd(uint64_t b) { double d; std::memcpy(&d, &b, 8); return d; }
inline uint64_t db(double d) { uint64_t b; std::memcpy(&b, &d, 8); return b; }
inline bool nan32(uint32_t b) { return (b & 0x7fffffff) > 0x7f800000; }
inline bool nan64(uint64_t b) { return (b & 0x7fffffffffffffffull) > 0x7ff0000000000000ull; }
enum { OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MIN, OP_MAX };
inline uint32_t sse32(int op, uint32_t a, uint32_t b) {
    // min / max: the second (source) operand unless the comparison holds (a NaN gives the source unchanged), as x86
    if (op == OP_MIN) return bf(a) < bf(b) ? a : b;
    if (op == OP_MAX) return bf(a) > bf(b) ? a : b;
    if (nan32(a)) return a | 0x400000;
    if (nan32(b)) return b | 0x400000;
    volatile float x = bf(a), y = bf(b), r;
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
    volatile double x = bd(a), y = bd(b), r;
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
    volatile float x = bf(a); volatile float r = std::sqrt(x); return fb(r);
}
inline uint64_t sqrt64(uint64_t v) {
    if (nan64(v)) return v | 0x8000000000000ull;
    if (v == 0x8000000000000000ull) return v;
    if (v >> 63) return 0xfff8000000000000ull;
    volatile double d = std::sqrt(bd(v)); return db(d);
}
inline uint32_t L(const Xmm& v, int i) { return uint32_t((i < 2 ? v.lo : v.hi) >> (32 * (i & 1))); }
inline void SL(Xmm& v, int i, uint32_t x) { uint64_t& w = i < 2 ? v.lo : v.hi; int s = 32 * (i & 1); w = (w & ~(0xffffffffull << s)) | (uint64_t(x) << s); }
inline Xmm ps(int op, Xmm a, const Xmm& b) { for (int i = 0; i < 4; i++) SL(a, i, sse32(op, L(a, i), L(b, i))); return a; }
inline Xmm pd(int op, Xmm a, const Xmm& b) { a.lo = sse64(op, a.lo, b.lo); a.hi = sse64(op, a.hi, b.hi); return a; }
inline Xmm shufps(const Xmm& a, const Xmm& b, unsigned imm) {
    Xmm r; SL(r, 0, L(a, imm & 3)); SL(r, 1, L(a, (imm >> 2) & 3)); SL(r, 2, L(b, (imm >> 4) & 3)); SL(r, 3, L(b, (imm >> 6) & 3)); return r;
}
inline Xmm unpck(const Xmm& a, const Xmm& b, int off) { Xmm r; SL(r, 0, L(a, off)); SL(r, 1, L(b, off)); SL(r, 2, L(a, off + 1)); SL(r, 3, L(b, off + 1)); return r; }
inline bool cmp_pred(int pred, double x, double y) {
    bool un = std::isnan(x) || std::isnan(y);
    switch (pred) {
        case 0: return x == y; case 1: return x < y; case 2: return x <= y; case 3: return un;
        case 4: return un || x != y; case 5: return un || !(x < y); case 6: return un || !(x <= y); default: return !un;
    }
}
inline Xmm cmpps(int pred, Xmm a, const Xmm& b, int lanes) {
    for (int i = 0; i < lanes; i++) SL(a, i, cmp_pred(pred, bf(L(a, i)), bf(L(b, i))) ? 0xffffffffu : 0);
    return a;
}
inline void comis(Machine& m, double a, double b) {
    uint32_t f = m.flags & ~(CF | PF | ZF | SF | OF | 0x10);
    if (std::isnan(a) || std::isnan(b)) f |= ZF | PF | CF; else if (a < b) f |= CF; else if (a == b) f |= ZF;
    m.flags = f;
}
inline int32_t i32_trunc(double x, bool& ok) {
    if (std::isnan(x) || std::isinf(x)) { ok = false; return 0; }
    double t = std::trunc(x);
    if (t < -2147483648.0 || t >= 2147483648.0) { ok = false; return 0; }
    ok = true; return int32_t(t);
}
inline int32_t i32_near(double x, bool& ok) {
    if (std::isnan(x) || std::isinf(x)) { ok = false; return 0; }
    double t = std::nearbyint(x);
    if (t < -2147483648.0 || t >= 2147483648.0) { ok = false; return 0; }
    ok = true; return int32_t(t);
}
inline uint32_t cvt_si(double x, bool trunc) { bool ok; int32_t v = trunc ? i32_trunc(x, ok) : i32_near(x, ok); return ok ? uint32_t(v) : 0x80000000u; }
inline uint64_t cvtss2sd(uint32_t v) {
    if (nan32(v)) return (uint64_t(v >> 31) << 63) | 0x7ff8000000000000ull | (uint64_t(v & 0x3fffff) << 29);
    return db(double(bf(v)));
}
inline uint32_t cvtsd2ss(uint64_t v) {
    if (nan64(v)) return uint32_t(((v >> 63) << 31) | 0x7fc00000u | ((v >> 29) & 0x3fffff));
    volatile float f = float(bd(v)); return fb(f);
}
inline uint32_t cvtsi2ss(uint32_t v) { volatile float f = float(int32_t(v)); return fb(f); }

// ---- x87
inline int fbits(const Machine& m) { int pc = (m.fpu_cw >> 8) & 3; return pc == 0 ? 24 : pc == 2 ? 53 : 64; }
inline int frc(const Machine& m) { return (m.fpu_cw >> 10) & 3; }
inline F::F80& st(Machine& m, int i) { return m.fpu[m.fpu.size() - 1 - i]; }
inline void fpush(Machine& m, const F::F80& v) { m.fpu.push_back(v); }
inline void fpop(Machine& m) { m.fpu.pop_back(); }
// op as the x87 D8 reg field: 0 add, 1 mul, 4 sub, 5 subr, 6 div, 7 divr
inline F::F80 fop(Machine& m, int op, const F::F80& a, const F::F80& b) {
    const int bits = fbits(m), rc = frc(m);
    switch (op) {
        case 0: return F::add(a, b, bits, rc);
        case 1: return F::mul(a, b, bits, rc);
        case 4: return F::add(a, b, bits, rc, true);
        case 5: return F::add(b, a, bits, rc, true);
        case 6: return F::div(a, b, bits, rc);
        default: return F::div(b, a, bits, rc);
    }
}
inline int64_t fist(const F::F80& v, int rc, int size) {
    bool valid = v.kind == 0; int64_t n = 0;
    if (valid) {
        F::F80 ri = F::round_to_integer(v, rc);
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
inline void fcom(Machine& m, const F::F80& a, const F::F80& b, bool to_eflags) {
    int c = F::compare(a, b);
    if (to_eflags) {
        uint32_t f = m.flags & ~(CF | PF | ZF);
        if (c == 2) f |= CF | PF | ZF; else if (c < 0) f |= CF; else if (c == 0) f |= ZF;
        m.flags = f;
    } else {
        uint16_t sw = m.fpu_sw & ~0x4500;
        if (c == 2) sw |= 0x4500; else if (c < 0) sw |= 0x100; else if (c == 0) sw |= 0x4000;
        m.fpu_sw = sw;
    }
}
inline uint32_t fnstsw(const Machine& m) { return (m.fpu_sw & ~0x3800) | uint32_t(((8 - m.fpu.size()) % 8) << 11); }

}  // namespace xc
