// Literal transliterations (world group): one C++ statement per original instruction, generated from the disassembly
// (scratchpad tool lit2cpp.py) for long straight-line SIMD kernels and checked by Verify. Registers are locals; the guest stack is
// used literally (esp / ebp, push / pop write guest memory at the original's addresses), so the dead stack matches the original
// and callees see the same frame. Calls run the target through Machine::call_function with the registers synced.
#pragma once
#include "world_util.h"

#pragma clang diagnostic ignored "-Wunused-label"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-but-set-variable"

#define LIT_BEGIN                                                                                                         \
    reflex_game::Guest g{m};                                                                                              \
    const uint32_t lit_entry = m.r[4];                                                                                    \
    uint32_t eax = m.r[0], ecx = m.r[1], edx = m.r[2], ebx = m.r[3], esp = m.r[4], ebp = m.r[5], esi = m.r[6], edi = m.r[7]; \
    world::X x0 = world::xr(g, 0), x1 = world::xr(g, 1), x2 = world::xr(g, 2), x3 = world::xr(g, 3), x4 = world::xr(g, 4), \
             x5 = world::xr(g, 5), x6 = world::xr(g, 6), x7 = world::xr(g, 7);                                            \
    uint32_t CMP_A = 0, CMP_B = 0; float CF_A = 0, CF_B = 0;
#define LIT_SYNC_OUT                                                                                                      \
    m.r[0] = eax; m.r[1] = ecx; m.r[2] = edx; m.r[3] = ebx; m.r[4] = esp; m.r[5] = ebp; m.r[6] = esi; m.r[7] = edi;     \
    world::xw(g, 0, x0); world::xw(g, 1, x1); world::xw(g, 2, x2); world::xw(g, 3, x3);                                  \
    world::xw(g, 4, x4); world::xw(g, 5, x5); world::xw(g, 6, x6); world::xw(g, 7, x7);
#define LIT_SYNC_IN                                                                                                       \
    eax = m.r[0]; ecx = m.r[1]; edx = m.r[2]; ebx = m.r[3]; esp = m.r[4]; ebp = m.r[5]; esi = m.r[6]; edi = m.r[7];     \
    x0 = world::xr(g, 0); x1 = world::xr(g, 1); x2 = world::xr(g, 2); x3 = world::xr(g, 3);                              \
    x4 = world::xr(g, 4); x5 = world::xr(g, 5); x6 = world::xr(g, 6); x7 = world::xr(g, 7);
#define LIT_CALL(target) do { LIT_SYNC_OUT m.call_function((target), nullptr, 0); LIT_SYNC_IN } while (0)
// at the original's ret: registers out (ESP as at entry; Machine::ret pops the return address and the callee cleanup)
#define LIT_END(cleanup) L_return: LIT_SYNC_OUT m.r[4] = lit_entry; return m.ret(cleanup);
