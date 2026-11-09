// Helpers of the "world" group's wreck share (0x0063xxxx-0x0068xxxx, 0x006axxxx-0x0075xxxx): x87 copies, the CRT range-check abort,
// register shortcuts.
#pragma once
#include <cstdint>
#include "../port.h"
#include "world_util.h"

namespace wreck {

using reflex_game::Guest;
using reflex_vm::Machine;
using world::X;

enum { EAX = 0, ECX = 1, EDX = 2, EBX = 3, ESP = 4, EBP = 5, ESI = 6, EDI = 7 };

inline int frc(const Machine& m) { return (m.fpu_cw >> 10) & 3; }
inline int fbits(const Machine& m) { int pc = (m.fpu_cw >> 8) & 3; return pc == 0 ? 24 : pc == 2 ? 53 : 64; }
// fld dword [a]; fstp dword [b]: the value through the 80-bit register (SNaN quieted), stack unchanged.
inline uint32_t fcopy32(const Machine& m, uint32_t bits) { return reflex_f80::to_f32(reflex_f80::from_f32(bits), frc(m)); }

// call dword ptr [0x916224]: the CRT's invalid-parameter abort (std::vector checked iterators). Never fires in practice.
inline void crt_abort(Machine& m) { Guest g{m}; g.call(g.u32(0x916224)); }

// movss xmm, dword [a]: lane 0 loaded, others zero.
inline X ss(const Guest& g, uint32_t a) { return {{g.u32(a), 0, 0, 0}}; }
inline X ss_bits(uint32_t b) { return {{b, 0, 0, 0}}; }
// scalar ops on lane 0 (others from the destination)
inline X addss(X a, const X& b) { a.setf(0, x86::add(a.f(0), b.f(0))); return a; }
inline X subss(X a, const X& b) { a.setf(0, x86::sub(a.f(0), b.f(0))); return a; }
inline X mulss(X a, const X& b) { a.setf(0, x86::mul(a.f(0), b.f(0))); return a; }
inline X divss(X a, const X& b) { a.setf(0, x86::div(a.f(0), b.f(0))); return a; }
inline X sqrtss(X a, const X& b) { a.setf(0, x86::sqrt(b.f(0))); return a; }
inline X maxss(X a, const X& b) { a.u[0] = (x86::isnan_bits(a.u[0]) || x86::isnan_bits(b.u[0])) ? b.u[0] : (a.f(0) > b.f(0) ? a.u[0] : b.u[0]); return a; }
inline X minss(X a, const X& b) { a.u[0] = (x86::isnan_bits(a.u[0]) || x86::isnan_bits(b.u[0])) ? b.u[0] : (a.f(0) < b.f(0) ? a.u[0] : b.u[0]); return a; }
// comiss a, b then ja / jae / jb / jbe (unordered: CF = ZF = PF = 1)
inline bool c_ja(float a, float b) { return a > b; }
inline bool c_jae(float a, float b) { return a >= b; }      // CF = 0
inline bool c_jb(float a, float b) { return !(a >= b); }     // CF = 1: a < b or unordered
inline bool c_jbe(float a, float b) { return !(a > b); }     // CF = 1 or ZF = 1
// ucomiss a, b; lahf: EAX with AH = the flags byte (ZF PF CF, bit 1 set; SF AF cleared).
inline uint32_t lahf_ucomiss(uint32_t eax, float a, float b) {
    uint32_t ah = 0x02;
    if (a != a || b != b) ah |= 0x40 | 0x04 | 0x01;
    else if (a == b) ah |= 0x40;
    else if (a < b) ah |= 0x01;
    return (eax & 0xffff00ffu) | (ah << 8);
}
inline X cmpneqps(const X& a, const X& b) { X r; for (int i = 0; i < 4; i++) r.u[i] = !(a.f(i) == b.f(i)) ? 0xffffffffu : 0u; return r; }
inline X cvtsi2ss(X a, int32_t v) { a.setf(0, float(v)); return a; }

// The guest register file as the original holds it (m.r in place; xmm cached, synced around calls). Ports written in this style
// set the registers exactly where the original does, so callees with custom conventions see what they see in the original.
struct Regs {
    Machine& m;
    Guest g;
    uint32_t &eax, &ecx, &edx, &ebx, &esp, &ebp, &esi, &edi;
    X x[8];
    uint32_t saved[4];
    explicit Regs(Machine& mm) : m(mm), g{mm}, eax(mm.r[0]), ecx(mm.r[1]), edx(mm.r[2]), ebx(mm.r[3]), esp(mm.r[4]), ebp(mm.r[5]),
                                 esi(mm.r[6]), edi(mm.r[7]) {
        load(); saved[0] = ebx; saved[1] = ebp; saved[2] = esi; saved[3] = edi;
    }
    void load() { for (int i = 0; i < 8; i++) x[i] = world::xr(g, i); }
    void store() { for (int i = 0; i < 8; i++) world::xw(g, i, x[i]); }
    // calls addr with ESP = sp (the original's ESP before it pushes the arguments), arguments pushed; registers as they are
    uint32_t call(uint32_t sp, uint32_t addr, std::initializer_list<uint32_t> args = {}) {
        store();
        const uint32_t keep = esp; esp = sp;
        uint32_t a[16]; int n = 0; for (uint32_t v : args) a[n++] = v;
        m.call_function(addr, a, n, -1, -1);
        esp = keep;
        load();
        return eax;
    }
    // jmp addr (tail call from a frameless point, ESP = the entry ESP): the callee sees the caller's return address at [ESP] as in
    // the original (call_function's own return address goes there for the call and the slot is put back afterwards).
    uint32_t tailcall(uint32_t addr) {
        const uint32_t e = esp, keep = g.u32(e);
        call(e + 4, addr);
        g.u32(e, keep);
        return eax;
    }
    // call dword ptr [iat]
    uint32_t icall(uint32_t sp, uint32_t iat, std::initializer_list<uint32_t> args = {}) { return call(sp, g.u32(iat), args); }
    void restore() { ebx = saved[0]; ebp = saved[1]; esi = saved[2]; edi = saved[3]; }
    // returns with callee cleanup n (xmm stored, callee-saved registers restored)
    uint32_t ret(uint32_t n) { store(); restore(); return m.ret(n); }
    uint32_t u32(uint32_t a) const { return g.u32(a); }
    void w32(uint32_t a, uint32_t v) { g.u32(a, v); }
    uint8_t u8(uint32_t a) const { return g.u8(a); }
    void w8(uint32_t a, uint8_t v) { g.u8(a, v); }
    uint32_t u16(uint32_t a) const { return uint32_t(m.mem.read(a, 2)); }
    X ss(uint32_t a) const { return wreck::ss(g, a); }
    X ld(uint32_t a) const { return world::ld(g, a); }
    void st(uint32_t a, const X& v) { world::st(g, a, v); }
    // mov ecx, obj; mov e1, [ecx]; mov e2, [e1 + slot]; call e2 (r1 / r2: the registers the original uses for vtable / target)
    uint32_t vcall(uint32_t sp, uint32_t obj, uint32_t slot, uint32_t& r_vt, uint32_t& r_fn, std::initializer_list<uint32_t> args = {}) {
        ecx = obj; r_vt = g.u32(obj); r_fn = g.u32(r_vt + slot);
        return call(sp, r_fn, args);
    }
};
// sete / setb style byte writes into a register
inline void setlo8(uint32_t& r, uint32_t v) { r = (r & 0xffffff00u) | (v & 0xff); }

}  // namespace wreck
