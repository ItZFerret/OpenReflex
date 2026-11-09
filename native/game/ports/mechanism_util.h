// Helpers for the "mechanism" group (RavenPhysics articulated-body solver): xmm-lane operations exactly as the exe's SSE code,
// guest vec4 access, guest stack frames for locals that callees write, and calls with EDX inputs.
#pragma once
#include <cstdint>
#include <initializer_list>
#include "../guest.h"
#include "../port.h"

namespace mech {

using reflex_game::Guest;
using x86::V4;

// ---- lanes (an xmm register: lane 0 = low 32 bits)
inline uint32_t lb(const V4& v, int i) { return x86::bits(v.f[i]); }
inline V4 vbits(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { return V4::from_bits(a, b, c, d); }
// shufps a, b, imm
inline V4 shuf(const V4& a, const V4& b, int imm) { return {{a.f[imm & 3], a.f[(imm >> 2) & 3], b.f[(imm >> 4) & 3], b.f[(imm >> 6) & 3]}}; }
inline V4 bcast(const V4& a, int i) { return V4::splat(a.f[i]); }   // shufps a, a, i*0x55
inline V4 perm(const V4& a, int i0, int i1, int i2, int i3) { return {{a.f[i0], a.f[i1], a.f[i2], a.f[i3]}}; }
// movss a, b (register form): lane 0 from b
inline V4 movss(const V4& a, const V4& b) { V4 r = a; r.f[0] = b.f[0]; return r; }
// movss a, [mem] / a scalar into lane 0 with zeros above
inline V4 scalar(float x) { return {{x, x86::flt(0), x86::flt(0), x86::flt(0)}}; }
inline V4 unpcklps(const V4& a, const V4& b) { return {{a.f[0], b.f[0], a.f[1], b.f[1]}}; }
inline V4 unpckhps(const V4& a, const V4& b) { return {{a.f[2], b.f[2], a.f[3], b.f[3]}}; }
inline V4 movlhps(const V4& a, const V4& b) { return {{a.f[0], a.f[1], b.f[0], b.f[1]}}; }
inline V4 movhlps(const V4& a, const V4& b) { return {{b.f[2], b.f[3], a.f[2], a.f[3]}}; }
inline V4 zero() { return V4::from_bits(0, 0, 0, 0); }
// scalar-lane forms (addss / subss / mulss / divss a, b): lane 0 computed, lanes 1..3 of a kept
inline V4 addss(const V4& a, const V4& b) { V4 r = a; r.f[0] = x86::add(a.f[0], b.f[0]); return r; }
inline V4 subss(const V4& a, const V4& b) { V4 r = a; r.f[0] = x86::sub(a.f[0], b.f[0]); return r; }
inline V4 mulss(const V4& a, const V4& b) { V4 r = a; r.f[0] = x86::mul(a.f[0], b.f[0]); return r; }
inline V4 divss(const V4& a, const V4& b) { V4 r = a; r.f[0] = x86::div(a.f[0], b.f[0]); return r; }
// bitwise
inline V4 andps(const V4& a, const V4& b) { return vbits(lb(a, 0) & lb(b, 0), lb(a, 1) & lb(b, 1), lb(a, 2) & lb(b, 2), lb(a, 3) & lb(b, 3)); }
inline V4 andnps(const V4& a, const V4& b) { return vbits(~lb(a, 0) & lb(b, 0), ~lb(a, 1) & lb(b, 1), ~lb(a, 2) & lb(b, 2), ~lb(a, 3) & lb(b, 3)); }
inline V4 orps(const V4& a, const V4& b) { return vbits(lb(a, 0) | lb(b, 0), lb(a, 1) | lb(b, 1), lb(a, 2) | lb(b, 2), lb(a, 3) | lb(b, 3)); }
inline V4 xorps(const V4& a, const V4& b) { return vbits(lb(a, 0) ^ lb(b, 0), lb(a, 1) ^ lb(b, 1), lb(a, 2) ^ lb(b, 2), lb(a, 3) ^ lb(b, 3)); }
// packed sqrt (sqrtps)
inline V4 sqrtps(const V4& a) { return {{x86::sqrt(a.f[0]), x86::sqrt(a.f[1]), x86::sqrt(a.f[2]), x86::sqrt(a.f[3])}}; }
// packed min / max (minps / maxps: the second operand unless the comparison holds)
inline V4 minps(const V4& a, const V4& b) { return {{x86::min(a.f[0], b.f[0]), x86::min(a.f[1], b.f[1]), x86::min(a.f[2], b.f[2]), x86::min(a.f[3], b.f[3])}}; }
inline V4 maxps(const V4& a, const V4& b) { return {{x86::max(a.f[0], b.f[0]), x86::max(a.f[1], b.f[1]), x86::max(a.f[2], b.f[2]), x86::max(a.f[3], b.f[3])}}; }
// cmpps predicates: 0 eq, 1 lt, 2 le, 3 unord, 4 neq, 5 nlt, 6 nle, 7 ord
inline bool cmp_pred(float a, float b, int p) {
    bool un = a != a || b != b;
    switch (p & 7) {
        case 0: return !un && a == b;
        case 1: return !un && a < b;
        case 2: return !un && a <= b;
        case 3: return un;
        case 4: return un || a != b;
        case 5: return un || !(a < b);
        case 6: return un || !(a <= b);
        default: return !un;
    }
}
inline V4 cmpps(const V4& a, const V4& b, int p) {
    V4 r; for (int i = 0; i < 4; i++) r.f[i] = x86::flt(cmp_pred(a.f[i], b.f[i], p) ? 0xffffffffu : 0u); return r;
}

// The exe's horizontal sums of a product P (lane 0 of the result; the other lanes are those of P):
// dot3: movaps t,P; shufps t,P,0x99; addss P,t; shufps t,t,0x55; addss P,t  ->  (P0 + P1) + P2
inline float hsum3(const V4& p) { return x86::add(x86::add(p.f[0], p.f[1]), p.f[2]); }
inline float dot3(const V4& a, const V4& b) { return hsum3(x86::mul(a, b)); }
// dot4: P = a*b; movaps t,x; shufps t,P,0x40; addps t,P; shufps P,t,0x30; addps P,t -> lane 2 = (P1 + P3) + (P0 + P2)
inline float dot4(const V4& a, const V4& b) {
    V4 p = x86::mul(a, b);
    return x86::add(x86::add(p.f[1], p.f[3]), x86::add(p.f[0], p.f[2]));
}

// ---- guest memory
inline float f32_arg(reflex_vm::Machine& m, int i) { return x86::flt(m.arg(i)); }
inline V4 ld(const Guest& g, uint32_t a) { return g.v4(a); }
inline void st(Guest& g, uint32_t a, const V4& v) { g.v4(a, v); }
inline void st_bits(Guest& g, uint32_t a, const V4& v) { for (int i = 0; i < 4; i++) g.u32(a + 4 * i, lb(v, i)); }

// A guest stack frame for locals whose address is passed to callees (they live below ESP like the original's; the dead stack is
// not compared). Restores ESP when it goes out of scope: close the scope before m.ret().
struct Frame {
    reflex_vm::Machine& m; uint32_t esp0; uint32_t base;
    Frame(reflex_vm::Machine& mm, uint32_t size) : m(mm), esp0(mm.r[4]) { base = (esp0 - size - 64) & ~15u; m.r[4] = base; }
    ~Frame() { m.r[4] = esp0; }
    uint32_t at(uint32_t off) const { return base + off; }
};

// Guest::call with EDX (and ECX / EAX) inputs.
inline uint32_t call_regs(Guest& g, uint32_t address, std::initializer_list<uint32_t> args, int64_t ecx, int64_t edx, int64_t eax = -1) {
    if (edx >= 0) g.m.r[2] = uint32_t(edx);
    return g.call(address, args, ecx, eax);
}
// A call with register inputs of an LTCG convention (any of EAX ECX EDX EBX ESI EDI; -1 = unchanged). The callee-saved EBX / ESI /
// EDI are restored afterwards (they are the caller's: the original sets them for the call and its own epilogue restores them).
struct In {
    int64_t eax = -1, ecx = -1, edx = -1, ebx = -1, esi = -1, edi = -1;
    In& EAX(uint32_t v) { eax = v; return *this; }
    In& ECX(uint32_t v) { ecx = v; return *this; }
    In& EDX(uint32_t v) { edx = v; return *this; }
    In& EBX(uint32_t v) { ebx = v; return *this; }
    In& ESI(uint32_t v) { esi = v; return *this; }
    In& EDI(uint32_t v) { edi = v; return *this; }
};
inline uint32_t callr(Guest& g, uint32_t address, std::initializer_list<uint32_t> args, In in) {
    auto& r = g.m.r;
    uint32_t ebx = r[3], esi = r[6], edi = r[7];
    if (in.edx >= 0) r[2] = uint32_t(in.edx);
    if (in.ebx >= 0) r[3] = uint32_t(in.ebx);
    if (in.esi >= 0) r[6] = uint32_t(in.esi);
    if (in.edi >= 0) r[7] = uint32_t(in.edi);
    uint32_t v = g.call(address, args, in.ecx, in.eax);
    r[3] = ebx; r[6] = esi; r[7] = edi;
    return v;
}
// Virtual call: slot `slot` of the vtable of the object at `obj` (thiscall).
inline uint32_t vcall(Guest& g, uint32_t obj, int slot, std::initializer_list<uint32_t> args = {}) {
    return g.call(g.u32(g.u32(obj) + 4 * slot), args, obj);
}


}  // namespace mech
