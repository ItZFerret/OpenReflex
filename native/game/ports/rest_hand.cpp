// Group "rest" (wave 3a, notes/rest.md): the hand-written ports of the group (what the rest lifter cannot transliterate).
#include <stdexcept>
#include <utility>
#include "rest_util.h"

// 008fa230 __alloca_probe: EAX = bytes to allocate on the caller's stack. Touches every page between the current stack page and the
// new top (test [page], eax), moves ESP down by EAX (clamped at 0 on underflow, as sbb / not / and does) and returns to the caller
// with the return address copied to the new top: [new esp] = return address, EAX = return address, ECX preserved (push / pop).
//   push ecx; lea ecx, [esp + 4]; sub ecx, eax; sbb eax, eax; not eax; and ecx, eax; mov eax, esp; and eax, 0xfffff000
//   L: cmp ecx, eax; jb probe; mov eax, ecx; pop ecx; xchg esp, eax; mov eax, [eax]; mov [esp], eax; ret
//   probe: sub eax, 0x1000; test [eax], eax; jmp L
// The math agent's port (notes/math.md) was exact in On mode; it is kept here because the rest group owns the remaining frame code.
REFLEX_PORT_FN(0x008fa230, alloca_probe, "rest", reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX | reflex_port::OUT_ESP_BELOW) {
    using namespace rst;
    const uint32_t esp0 = m.r[4], ecx0 = m.r[1], size = m.r[0];
    uint32_t esp = esp0 - 4;
    wr32(m, esp, ecx0);                                         // push ecx
    uint32_t ecx = esp + 4;                                     // lea ecx, [esp + 4]
    const bool borrow = ecx < size;                             // sub ecx, eax (CF)
    ecx -= size;
    uint32_t eax = borrow ? 0xffffffffu : 0u;                   // sbb eax, eax
    eax = ~eax;                                                 // not eax
    ecx &= eax;                                                 // and ecx, eax
    eax = esp & 0xfffff000u;                                    // mov eax, esp; and eax, 0xfffff000
    while (ecx < eax) {                                         // cmp ecx, eax; jb
        eax -= 0x1000u;                                         // sub eax, 0x1000
        (void)rd32(m, eax);                                     // test dword ptr [eax], eax (the page probe: a read)
    }
    eax = ecx;                                                  // mov eax, ecx
    m.r[1] = rd32(m, esp); esp += 4;                            // pop ecx
    std::swap(esp, eax);                                        // xchg esp, eax
    eax = rd32(m, eax);                                         // mov eax, dword ptr [eax] (the return address)
    wr32(m, esp, eax);                                          // mov dword ptr [esp], eax
    m.r[0] = eax; m.r[4] = esp;
    return m.ret(0);                                            // ret
}
