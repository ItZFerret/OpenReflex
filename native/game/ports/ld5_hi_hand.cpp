// ld5_hi (wave 5, 0x800000..0x8fffff): hand-written ports of functions the lifter refuses (notes/ld5_hi.md).
#include "lift_rt.h"

namespace {
constexpr uint32_t LD5_ARITH = 0x8c5u;   // CF PF ZF SF OF: what vm.cpp's set_logic / set_add rewrite (AF and the rest stay)
inline uint32_t ld5_logic(uint32_t fl, uint32_t r) { return (fl & ~LD5_ARITH) | lift::fl_logic(r, 4); }
inline uint32_t ld5_sub(uint32_t fl, uint32_t a, uint32_t b) { return (fl & ~LD5_ARITH) | lift::fl_sub(a, b, 4); }
// CPUID exactly as the reference interpreter answers it (vm.h reflex_vm::cpuid, which vm.cpp M_cpuid calls)
inline void ld5_cpuid(uint32_t& eax, uint32_t& ebx, uint32_t& ecx, uint32_t& edx) { reflex_vm::cpuid(eax, ebx, ecx, edx); }
}  // namespace

// __get_sse2_info (the exe's static CRT, run once by the initializer Fn_008fa76a): pushfd / popfd toggle of the ID flag, CPUID
// leaves 0 and 1, then (only if CPUID.1 EDX bit 26) the SEH-guarded movapd probe FUN_008fa6b8. Returns 1 if SSE2 is usable.
// The lifter refuses pushfd (the whole EFLAGS word); here the word is m.flags as vm.cpp keeps it (pushfd pushes it, popfd keeps
// bits 0x00244dd5 | 2, set_logic / set_add rewrite CF PF ZF SF OF only, inc keeps CF).
//   original __get_sse2_info; register inputs: none; ret 0
REFLEX_PORT_FN(0x008fa708, __get_sse2_info, "ld5_hi", LIFT_OUTS) {
    LIFT_ENTRY();
    LIFT_PUSH(ebp);                                   // mov edi, edi; push ebp
    ebp = esp;                                        // mov ebp, esp
    fl = ld5_sub(fl, esp, 0x18u); esp -= 0x18u;       // sub esp, 0x18
    eax = 0; fl = ld5_logic(fl, eax);                 // xor eax, eax
    LIFT_PUSH(ebx);
    wr32(m, ebp - 4, eax); wr32(m, ebp - 0xc, eax); wr32(m, ebp - 8, eax);
    LIFT_PUSH(ebx);
    LIFT_PUSH(fl);                                    // pushfd
    eax = LIFT_POP();
    ecx = eax;
    eax ^= 0x200000u; fl = ld5_logic(fl, eax);
    LIFT_PUSH(eax);
    fl = (LIFT_POP() & 0x00244dd5u) | 2u;             // popfd
    LIFT_PUSH(fl);                                    // pushfd
    edx = LIFT_POP();
    fl = ld5_sub(fl, edx, ecx); edx -= ecx;
    if (edx == 0) goto L_8fa74d;
    LIFT_PUSH(ecx);
    fl = (LIFT_POP() & 0x00244dd5u) | 2u;             // popfd
    eax = 0; fl = ld5_logic(fl, eax);
    ld5_cpuid(eax, ebx, ecx, edx);
    wr32(m, ebp - 0xc, eax); wr32(m, ebp - 0x18, ebx); wr32(m, ebp - 0x14, edx); wr32(m, ebp - 0x10, ecx);
    eax = 1;
    ld5_cpuid(eax, ebx, ecx, edx);
    wr32(m, ebp - 4, edx); wr32(m, ebp - 8, eax);
L_8fa74d:
    ebx = LIFT_POP();
    fl = ld5_logic(fl, rd32(m, ebp - 4) & 0x4000000u);
    if ((rd32(m, ebp - 4) & 0x4000000u) == 0) goto L_8fa765;
    LIFT_CALL(0x8fa6b8u, 0x8fa75cu);                  // FUN_008fa6b8 (SEH-guarded SSE2 probe)
    fl = ld5_logic(fl, eax);
    if (eax == 0) goto L_8fa765;
    eax = 0; fl = ld5_logic(fl, eax);
    { const uint32_t cf = fl & 1u; fl = (fl & ~LD5_ARITH) | lift::fl_add(eax, 1u, 4); fl = (fl & ~1u) | cf; eax += 1; }   // inc eax
    goto L_8fa767;
L_8fa765:
    eax = 0; fl = ld5_logic(fl, eax);
L_8fa767:
    ebx = LIFT_POP();
    esp = ebp; ebp = LIFT_POP();                      // leave
    LIFT_RET(0);
}
