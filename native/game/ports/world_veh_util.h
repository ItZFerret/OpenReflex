// Helpers of the "world" group's vehicle ports (world_veh*.cpp): an exact register-level model of the original's code.
//
// The ports in world_veh*.cpp are transliterations of the original functions instruction by instruction (generated from the
// disassembly by the scratchpad's wtools/vehlift.py, then reviewed): general registers are the machine's own (m.r[]), xmm registers
// are local X values (written back around calls and at the return), the stack is the guest stack (pushes, locals and the
// arguments of callees at the original's addresses), x87 values live on the machine's x87 stack. Every operation follows the
// interpreter's semantics (vm.cpp), which are the original's: SSE through world_util.h / the double helpers below, x87 through
// reflex_f80. So all registers, the x87 stack and memory end exactly as in the original.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include "../port.h"
#include "world_util.h"
#include "f80/f80.h"

namespace vehx {

using world::X;
using reflex_game::Guest;

// ------------------------------------------------------------------ scalar / double SSE (as vm.cpp's sse32 / sse64)
inline float bf(uint32_t b) { float f; std::memcpy(&f, &b, 4); return f; }
inline uint32_t fb(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
inline double bd(uint64_t b) { double d; std::memcpy(&d, &b, 8); return d; }
inline uint64_t db(double d) { uint64_t b; std::memcpy(&b, &d, 8); return b; }
inline bool nan32(uint32_t b) { return (b & 0x7fffffffu) > 0x7f800000u; }
inline bool nan64(uint64_t b) { return (b & 0x7fffffffffffffffull) > 0x7ff0000000000000ull; }
enum { OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MIN, OP_MAX };
inline uint32_t sse32(int op, uint32_t a, uint32_t b) {
    // min / max: the second operand unless the comparison holds (a NaN in either gives the second, unchanged), as x86
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
inline uint64_t lo64(const X& v) { return uint64_t(v.u[0]) | uint64_t(v.u[1]) << 32; }
inline uint64_t hi64(const X& v) { return uint64_t(v.u[2]) | uint64_t(v.u[3]) << 32; }
inline void setlo64(X& v, uint64_t q) { v.u[0] = uint32_t(q); v.u[1] = uint32_t(q >> 32); }
inline void sethi64(X& v, uint64_t q) { v.u[2] = uint32_t(q); v.u[3] = uint32_t(q >> 32); }
inline X x64(uint64_t lo, uint64_t hi) { X r; setlo64(r, lo); sethi64(r, hi); return r; }

// packed single: all four lanes through sse32 (the interpreter's exact path; same results as its vector fast path)
inline X ps(int op, const X& a, const X& b) { X r; for (int i = 0; i < 4; i++) r.u[i] = sse32(op, a.u[i], b.u[i]); return r; }
inline X ss(int op, const X& a, uint32_t b) { X r = a; r.u[0] = sse32(op, a.u[0], b); return r; }
inline X sd(int op, const X& a, uint64_t b) { X r = a; setlo64(r, sse64(op, lo64(a), b)); return r; }
inline X sqrtps(const X& b) { X r; for (int i = 0; i < 4; i++) r.u[i] = sqrt32(b.u[i]); return r; }
inline X sqrtss(const X& a, uint32_t b) { X r = a; r.u[0] = sqrt32(b); return r; }
// cmpps predicates 0..7 (eq lt le unord neq nlt nle ord)
inline bool cmp_pred(int p, uint32_t a, uint32_t b) {
    float x = bf(a), y = bf(b); bool un = nan32(a) || nan32(b);
    switch (p) {
        case 0: return !un && x == y;
        case 1: return !un && x < y;
        case 2: return !un && x <= y;
        case 3: return un;
        case 4: return un || x != y;
        case 5: return un || !(x < y);
        case 6: return un || !(x <= y);
        default: return !un;
    }
}
inline X cmpps(int p, const X& a, const X& b) { X r; for (int i = 0; i < 4; i++) r.u[i] = cmp_pred(p, a.u[i], b.u[i]) ? 0xffffffffu : 0u; return r; }
inline X cmpss(int p, const X& a, uint32_t b) { X r = a; r.u[0] = cmp_pred(p, a.u[0], b) ? 0xffffffffu : 0u; return r; }

// conversions (vm.cpp)
inline uint64_t cvtss2sd(uint32_t v) {
    if (nan32(v)) return (uint64_t(v >> 31) << 63) | 0x7ff8000000000000ull | (uint64_t(v & 0x3fffff) << 29);
    return db(double(bf(v)));
}
inline uint32_t cvtsd2ss(uint64_t v) {
    if (nan64(v)) return uint32_t(((v >> 63) << 31) | 0x7fc00000u | ((v >> 29) & 0x3fffff));
    volatile float f = float(bd(v)); return fb(f);
}
inline X cvtps2pd(uint64_t v) { return x64(db(double(bf(uint32_t(v)))), db(double(bf(uint32_t(v >> 32))))); }
inline X cvtpd2ps(const X& v) { volatile float a = float(bd(lo64(v))), b = float(bd(hi64(v))); X r{}; r.u[0] = fb(a); r.u[1] = fb(b); return r; }
inline int32_t to_int32_trunc(double x, bool& ok) {
    if (std::isnan(x) || std::isinf(x)) { ok = false; return 0; }
    double t = std::trunc(x);
    if (t < -2147483648.0 || t >= 2147483648.0) { ok = false; return 0; }
    ok = true; return int32_t(t);
}
inline int32_t to_int32_nearest(double x, bool& ok) {
    if (std::isnan(x) || std::isinf(x)) { ok = false; return 0; }
    double t = std::nearbyint(x);
    if (t < -2147483648.0 || t >= 2147483648.0) { ok = false; return 0; }
    ok = true; return int32_t(t);
}
inline uint32_t cvttss2si(uint32_t v) { bool ok; int32_t r = to_int32_trunc(double(bf(v)), ok); return ok ? uint32_t(r) : 0x80000000u; }
inline uint32_t cvtss2si(uint32_t v) { bool ok; int32_t r = to_int32_nearest(double(bf(v)), ok); return ok ? uint32_t(r) : 0x80000000u; }
inline uint32_t cvttsd2si(uint64_t v) { bool ok; int32_t r = to_int32_trunc(bd(v), ok); return ok ? uint32_t(r) : 0x80000000u; }
inline uint32_t cvtsi2ss(uint32_t v) { volatile float f = float(int32_t(v)); return fb(f); }
inline uint64_t cvtsi2sd(uint32_t v) { return db(double(int32_t(v))); }

// ------------------------------------------------------------------ flags (vm.cpp: CF PF ZF SF OF; AF is not modelled)
constexpr uint32_t CF = 1, PF = 4, ZF = 0x40, SF = 0x80, OF = 0x800;
inline uint64_t szmask(int size) { return size == 4 ? 0xffffffffull : size == 2 ? 0xffffull : 0xffull; }
inline bool parity(uint64_t v) { return !__builtin_parity(unsigned(v & 0xff)); }
inline uint32_t f_logic(uint32_t fl, uint64_t res, int size) {
    int bits = 8 * size; res &= szmask(size);
    uint32_t f = fl & ~(CF | PF | ZF | SF | OF);
    if (res == 0) f |= ZF;
    if ((res >> (bits - 1)) & 1) f |= SF;
    if (parity(res)) f |= PF;
    return f;
}
inline uint32_t f_add(uint32_t fl, uint64_t a, uint64_t b, uint64_t carry, int size, bool sub) {
    int bits = 8 * size; uint64_t m = szmask(size), res; bool cf, of;
    a &= m; b &= m;
    if (sub) {
        __int128 full = __int128(a) - __int128(b) - __int128(carry);
        res = uint64_t(full) & m; cf = full < 0;
        of = ((((a ^ b) & (a ^ res)) >> (bits - 1)) & 1) != 0;
    } else {
        unsigned __int128 full = (unsigned __int128)a + b + carry;
        res = uint64_t(full) & m; cf = full > m;
        of = (((~(a ^ b) & (a ^ res)) >> (bits - 1)) & 1) != 0;
    }
    uint32_t f = f_logic(fl, res, size);
    if (cf) f |= CF;
    if (of) f |= OF;
    return f;
}
// comiss / ucomiss / comisd / ucomisd
inline uint32_t f_comi(uint32_t fl, double a, double b) {
    uint32_t f = fl & ~(CF | PF | ZF | SF | OF | 0x10);
    if (std::isnan(a) || std::isnan(b)) f |= ZF | PF | CF; else if (a < b) f |= CF; else if (a == b) f |= ZF;
    return f;
}
inline uint32_t f_comiss(uint32_t fl, uint32_t a, uint32_t b) { return f_comi(fl, bf(a), bf(b)); }
inline uint32_t f_comisd(uint32_t fl, uint64_t a, uint64_t b) { return f_comi(fl, bd(a), bd(b)); }
// conditions by their jcc names
#define VEH_CF (fl & vehx::CF)
#define VEH_ZF (fl & vehx::ZF)
#define VEH_SF ((fl & vehx::SF) != 0)
#define VEH_OF ((fl & vehx::OF) != 0)
#define VEH_PF (fl & vehx::PF)
#define C_o  (VEH_OF)
#define C_no (!VEH_OF)
#define C_b  (VEH_CF)
#define C_ae (!VEH_CF)
#define C_e  (VEH_ZF)
#define C_ne (!VEH_ZF)
#define C_be (VEH_CF || VEH_ZF)
#define C_a  (!VEH_CF && !VEH_ZF)
#define C_s  (VEH_SF)
#define C_ns (!VEH_SF)
#define C_p  (VEH_PF)
#define C_np (!VEH_PF)
#define C_l  (VEH_SF != VEH_OF)
#define C_ge (VEH_SF == VEH_OF)
#define C_le (VEH_ZF || VEH_SF != VEH_OF)
#define C_g  (!VEH_ZF && VEH_SF == VEH_OF)

// ------------------------------------------------------------------ the register context of a lifted port
struct Ctx {
    reflex_vm::Machine& m;
    Guest g;
    X x[8];
    explicit Ctx(reflex_vm::Machine& mm) : m(mm), g{mm} { xin(); }
    void xin() { for (int i = 0; i < 8; i++) x[i] = world::xr(g, i); }
    void xout() { for (int i = 0; i < 8; i++) world::xw(g, i, x[i]); }
    // memory
    uint32_t r32(uint32_t a) const { return uint32_t(m.mem.read(a, 4)); }
    uint32_t r16(uint32_t a) const { return uint32_t(m.mem.read(a, 2)); }
    uint32_t r8(uint32_t a) const { return uint32_t(m.mem.read(a, 1)); }
    uint64_t r64(uint32_t a) const { return uint64_t(r32(a)) | uint64_t(r32(a + 4)) << 32; }
    X r128(uint32_t a) const { return world::ld(g, a); }
    void w32(uint32_t a, uint32_t v) { m.mem.write(a, 4, v); }
    void w16(uint32_t a, uint32_t v) { m.mem.write(a, 2, v & 0xffff); }
    void w8(uint32_t a, uint32_t v) { m.mem.write(a, 1, v & 0xff); }
    void w64(uint32_t a, uint64_t v) { w32(a, uint32_t(v)); w32(a + 4, uint32_t(v >> 32)); }
    void w128(uint32_t a, const X& v) { world::st(g, a, v); }
    // stack
    void push(uint32_t v) { m.r[4] -= 4; w32(m.r[4], v); }
    uint32_t pop() { uint32_t v = r32(m.r[4]); m.r[4] += 4; return v; }
    // call target with the arguments already pushed (ESP as the original's at the call). The callee sees the return address
    // slot holding 0 (Machine::run stops there); afterwards the slot gets the original's return address.
    void call(uint32_t target, uint32_t retaddr) {
        uint32_t sp = m.r[4];
        xout();
        m.call_at(target, retaddr); (void)sp;   // the real return address in its slot for the whole call (lift_rt.h LIFT_CALL)
        xin();
    }
    uint32_t ret(uint32_t cleanup) { xout(); return m.ret(cleanup); }
    // x87 (vm.cpp execute_fp)
    int fbits() const { int pc = (m.fpu_cw >> 8) & 3; if (pc == 1) throw std::runtime_error("x87 reserved precision control"); return pc == 0 ? 24 : pc == 2 ? 53 : 64; }
    int frc() const { return (m.fpu_cw >> 10) & 3; }
    reflex_f80::F80& st(int i) { if (i >= int(m.fpu.size())) throw std::runtime_error("x87 stack underflow"); return m.fpu[m.fpu.size() - 1 - i]; }
    void fpush(const reflex_f80::F80& v) { if (m.fpu.size() >= 8) throw std::runtime_error("x87 stack overflow"); m.fpu.push_back(v); }
    void fpop() { if (m.fpu.empty()) throw std::runtime_error("x87 stack underflow"); m.fpu.pop_back(); }
    void fld32(uint32_t a) { fpush(reflex_f80::from_f32(r32(a))); }
    void fld64(uint32_t a) { fpush(reflex_f80::from_f64(r64(a))); }
    void fst32(uint32_t a, bool pop) { reflex_f80::F80 v = st(0); w32(a, reflex_f80::to_f32(v, frc())); if (pop) fpop(); }
    void fst64(uint32_t a, bool pop) { reflex_f80::F80 v = st(0); w64(a, reflex_f80::to_f64(v, frc())); if (pop) fpop(); }
    void fstst(int i, bool pop) { reflex_f80::F80 v = st(0); st(i) = v; if (pop) fpop(); }
    // st(0) = st(0) op src (op: 0 add, 1 mul, 4 sub, 5 subr, 6 div, 7 divr)
    reflex_f80::F80 fapply(int op, const reflex_f80::F80& a, const reflex_f80::F80& b) {
        int bits = fbits(), rc = frc();
        switch (op) {
            case 0: return reflex_f80::add(a, b, bits, rc);
            case 1: return reflex_f80::mul(a, b, bits, rc);
            case 4: return reflex_f80::add(a, b, bits, rc, true);
            case 5: return reflex_f80::add(b, a, bits, rc, true);
            case 6: return reflex_f80::div(a, b, bits, rc);
            case 7: return reflex_f80::div(b, a, bits, rc);
        }
        throw std::runtime_error("x87 op");
    }
    void farith32(int op, uint32_t a) { st(0) = fapply(op, st(0), reflex_f80::from_f32(r32(a))); }
    void farith64(int op, uint32_t a) { st(0) = fapply(op, st(0), reflex_f80::from_f64(r64(a))); }
    // rep movsd (DF clear)
    void rep_movsd() {
        uint32_t &ecx = m.r[1], &esi = m.r[6], &edi = m.r[7];
        while (ecx) { w32(edi, r32(esi)); esi += 4; edi += 4; ecx--; }
    }
};

// 8 / 16-bit register parts
inline uint32_t lo8(uint32_t r) { return r & 0xff; }
inline uint32_t hi8(uint32_t r) { return (r >> 8) & 0xff; }
inline void set_lo8(uint32_t& r, uint32_t v) { r = (r & ~0xffu) | (v & 0xff); }
inline void set_hi8(uint32_t& r, uint32_t v) { r = (r & ~0xff00u) | ((v & 0xff) << 8); }
inline void set_lo16(uint32_t& r, uint32_t v) { r = (r & ~0xffffu) | (v & 0xffff); }

}  // namespace vehx

// The prologue of a lifted port: the context, the general registers (the machine's) and the xmm registers (local copies).
#define VEH_BEGIN                                                                                                   \
    vehx::Ctx c(m);                                                                                                 \
    uint32_t &eax = m.r[0], &ecx = m.r[1], &edx = m.r[2], &ebx = m.r[3], &esp = m.r[4], &ebp = m.r[5], &esi = m.r[6], \
             &edi = m.r[7];                                                                                         \
    world::X &x0 = c.x[0], &x1 = c.x[1], &x2 = c.x[2], &x3 = c.x[3], &x4 = c.x[4], &x5 = c.x[5], &x6 = c.x[6],     \
             &x7 = c.x[7];                                                                                          \
    uint32_t fl = m.flags;                                                                                          \
    (void)eax; (void)ecx; (void)edx; (void)ebx; (void)esp; (void)ebp; (void)esi; (void)edi; (void)fl;               \
    (void)x0; (void)x1; (void)x2; (void)x3; (void)x4; (void)x5; (void)x6; (void)x7

#define VEH_OUTS (reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX | reflex_port::OUT_XMM_ALL | reflex_port::OUT_FPU)
