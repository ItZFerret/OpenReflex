// Access to the original game's state for ported code, while it still lives in the original memory layout (libreflexworld's
// Machine). Ported functions read and write through these, so the storage can change later without touching their logic.
#pragma once
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include "vm.h"
#include "x86.h"

namespace reflex_game {

struct Guest {
    reflex_vm::Machine& m;
    uint32_t u32(uint32_t a) const { return uint32_t(m.mem.read(a, 4)); }
    int32_t i32(uint32_t a) const { return int32_t(m.mem.read(a, 4)); }
    uint8_t u8(uint32_t a) const { return uint8_t(m.mem.read(a, 1)); }
    float f32(uint32_t a) const { return x86::flt(u32(a)); }
    x86::V4 v4(uint32_t a) const { return x86::V4::from_bits(u32(a), u32(a + 4), u32(a + 8), u32(a + 12)); }
    void u32(uint32_t a, uint32_t v) { m.mem.write(a, 4, v); }
    void u8(uint32_t a, uint8_t v) { m.mem.write(a, 1, v); }
    void f32(uint32_t a, float v) { m.mem.write(a, 4, x86::bits(v)); }
    void v4(uint32_t a, const x86::V4& v) { for (int i = 0; i < 4; i++) f32(a + 4 * i, v.f[i]); }
    // Calls a function of the game at its original address (ported or not: a ported one runs as the port), with stack arguments,
    // ECX (thiscall) / EAX when >= 0. ESP is restored afterwards (cdecl and stdcall alike). Returns EAX; other results stay in the
    // registers (m.xmm, m.fpu ...). Callee-saved registers are the callee's business, as in the original.
    uint32_t call(uint32_t address, std::initializer_list<uint32_t> args = {}, int64_t ecx = -1, int64_t eax = -1) {
        uint32_t esp = m.r[4], a[16]; int n = 0;
        for (uint32_t v : args) a[n++] = v;
        uint32_t r = m.call_function(address, a, n, ecx, eax);
        m.r[4] = esp;
        return r;
    }
    // xmm registers
    x86::V4 xmm(int i) const { const auto& x = m.xmm[i]; return x86::V4::from_bits(uint32_t(x.lo), uint32_t(x.lo >> 32), uint32_t(x.hi), uint32_t(x.hi >> 32)); }
    void xmm(int i, const x86::V4& v) {
        auto& x = m.xmm[i];
        x.lo = uint64_t(x86::bits(v.f[0])) | uint64_t(x86::bits(v.f[1])) << 32; x.hi = uint64_t(x86::bits(v.f[2])) | uint64_t(x86::bits(v.f[3])) << 32;
    }
};

// The original's stack traffic below ESP, which is dead after the call (Verify does not compare it, the memory hash leaves the
// stacks out). Some original code reads uninitialized stack (e.g. a vec4's unwritten w lane) and copies it into live memory; a port
// whose dead-stack bytes such a reader picks up writes them with this, as the original laid them out (push ebp / aligned frame /
// nested return addresses). Use it only where tools/memdiff.py shows the need, and list the case in notes/dead-stack.md.
struct DeadStack {
    reflex_vm::Machine& m;
    uint32_t esp, ebp;                                   // as at the function's entry (esp points at the return address)
    static DeadStack entry(reflex_vm::Machine& m) { return DeadStack{m, m.r[4], m.r[5]}; }
    void w32(uint32_t a, uint32_t v) { m.mem.write(a, 4, v); }
    void w128(uint32_t a, const uint32_t v[4]) { for (int i = 0; i < 4; i++) m.mem.write(a + 4 * i, 4, v[i]); }
    // push ebp; mov ebp, esp; and esp, -align; sub esp, n: returns the frame base (the new esp)
    uint32_t enter(uint32_t n, uint32_t align = 16) { esp -= 4; w32(esp, ebp); ebp = esp; esp = (esp & ~(align - 1)) - n; return esp; }
    // the stack a nested call sees at its entry (its return address pushed)
    DeadStack call(uint32_t return_address) { DeadStack s{m, esp - 4, ebp}; w32(s.esp, return_address); return s; }
};

}  // namespace reflex_game
