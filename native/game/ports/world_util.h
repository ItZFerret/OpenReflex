// Helpers of the "world" group's ports: 128-bit SSE values as the guest holds them (bit-exact lane operations, the AltiVec-style
// byte permute the Xenon-derived code emulates through the stack), and small guest access shortcuts.
#pragma once
#include <cstdint>
#include <cstring>
#include "../guest.h"
#include "../x86.h"

namespace world {

using reflex_game::Guest;

// An xmm value: four 32-bit lanes, lane 0 lowest.
struct X {
    uint32_t u[4];
    static X splat(uint32_t b) { return {{b, b, b, b}}; }
    static X fl(float a, float b, float c, float d) { return {{x86::bits(a), x86::bits(b), x86::bits(c), x86::bits(d)}}; }
    float f(int i) const { return x86::flt(u[i]); }
    void setf(int i, float v) { u[i] = x86::bits(v); }
};

inline X ld(const Guest& g, uint32_t a) { return {{g.u32(a), g.u32(a + 4), g.u32(a + 8), g.u32(a + 12)}}; }
inline void st(Guest& g, uint32_t a, const X& v) { for (int i = 0; i < 4; i++) g.u32(a + 4 * i, v.u[i]); }
inline X xr(const Guest& g, int i) {
    const auto& x = g.m.xmm[i];
    return {{uint32_t(x.lo), uint32_t(x.lo >> 32), uint32_t(x.hi), uint32_t(x.hi >> 32)}};
}
inline void xw(Guest& g, int i, const X& v) {
    auto& x = g.m.xmm[i];
    x.lo = uint64_t(v.u[0]) | uint64_t(v.u[1]) << 32; x.hi = uint64_t(v.u[2]) | uint64_t(v.u[3]) << 32;
}

#define WORLD_LANES(expr) X r; for (int i = 0; i < 4; i++) r.u[i] = (expr); return r
inline X addps(const X& a, const X& b) { WORLD_LANES(x86::bits(x86::add(a.f(i), b.f(i)))); }
inline X subps(const X& a, const X& b) { WORLD_LANES(x86::bits(x86::sub(a.f(i), b.f(i)))); }
inline X mulps(const X& a, const X& b) { WORLD_LANES(x86::bits(x86::mul(a.f(i), b.f(i)))); }
inline X divps(const X& a, const X& b) { WORLD_LANES(x86::bits(x86::div(a.f(i), b.f(i)))); }
inline X sqrtps(const X& a) { WORLD_LANES(x86::bits(x86::sqrt(a.f(i)))); }
// minps / maxps: the second operand unless the comparison holds (so a NaN in either operand gives the second unchanged), as
// x86 and the interpreter (vm.cpp sse32 since 2026-10-18) do.
inline X minps(const X& a, const X& b) { WORLD_LANES(a.f(i) < b.f(i) ? a.u[i] : b.u[i]); }
inline X maxps(const X& a, const X& b) { WORLD_LANES(a.f(i) > b.f(i) ? a.u[i] : b.u[i]); }
inline X andps(const X& a, const X& b) { WORLD_LANES(a.u[i] & b.u[i]); }
inline X andnps(const X& a, const X& b) { WORLD_LANES(~a.u[i] & b.u[i]); }
inline X orps(const X& a, const X& b) { WORLD_LANES(a.u[i] | b.u[i]); }
inline X xorps(const X& a, const X& b) { WORLD_LANES(a.u[i] ^ b.u[i]); }
inline X pcmpeqd(const X& a, const X& b) { WORLD_LANES(a.u[i] == b.u[i] ? 0xffffffffu : 0u); }
// cmpps predicates (unordered: false for lt/le/eq, true for neq/nlt/nle)
inline X cmpltps(const X& a, const X& b) { WORLD_LANES(a.f(i) < b.f(i) ? 0xffffffffu : 0u); }
inline X cmpleps(const X& a, const X& b) { WORLD_LANES(a.f(i) <= b.f(i) ? 0xffffffffu : 0u); }
inline X cmpeqps(const X& a, const X& b) { WORLD_LANES(a.f(i) == b.f(i) ? 0xffffffffu : 0u); }
inline X cmpnltps(const X& a, const X& b) { WORLD_LANES(!(a.f(i) < b.f(i)) ? 0xffffffffu : 0u); }
inline X cmpnleps(const X& a, const X& b) { WORLD_LANES(!(a.f(i) <= b.f(i)) ? 0xffffffffu : 0u); }
#undef WORLD_LANES
// shufps a, b, imm: lanes 0, 1 from a, lanes 2, 3 from b
inline X shufps(const X& a, const X& b, int imm) { return {{a.u[imm & 3], a.u[(imm >> 2) & 3], b.u[(imm >> 4) & 3], b.u[(imm >> 6) & 3]}}; }
inline X unpcklps(const X& a, const X& b) { return {{a.u[0], b.u[0], a.u[1], b.u[1]}}; }
inline X unpckhps(const X& a, const X& b) { return {{a.u[2], b.u[2], a.u[3], b.u[3]}}; }
inline X movlhps(const X& a, const X& b) { return {{a.u[0], a.u[1], b.u[0], b.u[1]}}; }
inline X movhlps(const X& a, const X& b) { return {{b.u[2], b.u[3], a.u[2], a.u[3]}}; }
// movss xmm, xmm: lane 0 from b
inline X movss(const X& a, const X& b) { return {{b.u[0], a.u[1], a.u[2], a.u[3]}}; }

// The byte permute (vperm as the exe emulates it, a 16-step loop over the stack): byte i of the result is byte
// (ctrl[i] ^ 3) & 0x1f of the 32 bytes lo:hi (memory order; the xor makes the big-endian byte numbers little-endian).
inline X perm(const X& lo, const X& hi, const X& ctrl) {
    uint8_t src[32], c[16], out[16];
    std::memcpy(src, lo.u, 16); std::memcpy(src + 16, hi.u, 16); std::memcpy(c, ctrl.u, 16);
    for (int i = 0; i < 16; i++) out[i] = src[(c[i] ^ 3) & 0x1f];
    X r; std::memcpy(r.u, out, 16); return r;
}
// The low byte the permute loop leaves in DL (callers do not read it; kept for completeness).
inline uint8_t perm_last_byte(const X& lo, const X& hi, const X& ctrl) {
    uint8_t src[32], c[16];
    std::memcpy(src, lo.u, 16); std::memcpy(src + 16, hi.u, 16); std::memcpy(c, ctrl.u, 16);
    return src[(c[15] ^ 3) & 0x1f];
}

}  // namespace world

namespace world {

// The original's stack frame at the same guest addresses. Buffers the original passes to callees live there (callees write and
// read them at the same addresses), and each call sees the ESP it sees in the original (so the callee's own frame and any
// uninitialized reads of it land on the same bytes). Locals no callee sees stay C++ locals.
//   Frame f(m, 0x7a4, 3);   // push ebp; mov ebp, esp; and esp, -16; sub esp, 0x7a4; then 3 pushes
//   f.esp + X is [esp + X] after the prologue.
struct Frame {
    reflex_vm::Machine& m;
    uint32_t entry, esp;
    uint32_t saved[4];   // ebx, ebp, esi, edi at entry (restored by leave())
    Frame(reflex_vm::Machine& mm, uint32_t size, int pushes, bool align16 = true) : m(mm), entry(mm.r[4]) {
        uint32_t ebp = entry - 4;
        esp = (align16 ? (ebp & ~15u) : ebp) - size - 4u * uint32_t(pushes);
        saved[0] = m.r[3]; saved[1] = m.r[5]; saved[2] = m.r[6]; saved[3] = m.r[7];
    }
    // Frame without "push ebp / and esp": the callee-saved pushes and sub esp only (esp = entry - 4 * pushes - size).
    static Frame flat(reflex_vm::Machine& mm, int pushes, uint32_t size) {
        Frame f(mm, 0, 0, false); f.esp = f.entry - 4u * uint32_t(pushes) - size; return f;
    }
    // Calls addr with ESP = sp (the original's ESP before it pushes the arguments), ECX / EAX when >= 0.
    uint32_t call_at(uint32_t sp, uint32_t addr, std::initializer_list<uint32_t> args = {}, int64_t ecx = -1, int64_t eax = -1) {
        uint32_t keep = m.r[4]; m.r[4] = sp;
        Guest g{m}; uint32_t r = g.call(addr, args, ecx, eax);
        m.r[4] = keep; return r;
    }
    uint32_t call(uint32_t addr, std::initializer_list<uint32_t> args = {}, int64_t ecx = -1, int64_t eax = -1) { return call_at(esp, addr, args, ecx, eax); }
    void restore_saved() { m.r[3] = saved[0]; m.r[5] = saved[1]; m.r[6] = saved[2]; m.r[7] = saved[3]; }
};

}  // namespace world
