/* world_veh_water.c -- the water-height singleton 0x00d72aac (constructed once under bit 1 of 0x00d72aa8 by 004242f0: a std::vector<int> object at 0x00d72aac whose
 * proxy +0 is allocated, its block 16 ints (42b410: filled with the 0 value) then emptied (004fb0e0: the vector cleared)). */
#include "build_alloc.h"
#include "build_rider_util.h"
#include "fx.h"
#include "race_activity_misc.h"

rx_addr rx_vector_int_construct16(rx_addr vec, rx_addr value_p);   /* 0042b410 */
rx_addr rx_water_heights_init(void);                                /* 004242f0 */

/* 0042b410 (stdcall vector, &value; RET 8): the vector's +0xc / +0x10 / +0x14 cleared, a block of 16 ints (0042bf30), begin = end = it, capacity end = it + 0x40, then
 * 16 copies of *value filled in (006c8230) and end = past them. Returns nothing (the SEH frame is the original's) */
rx_addr rx_vector_int_construct16(rx_addr vec, rx_addr value_p) {
    SU32(vec, 0xc, 0); SU32(vec, 0x10, 0); SU32(vec, 0x14, 0);
    rx_addr blk;
    { const rx_addr sp = rx_sp(); rx_set_sp(sp - 4u); blk = rx_fx_vector_alloc(16); rx_set_sp(sp); }
    SU32(vec, 0xc, blk); SU32(vec, 0x10, blk); SU32(vec, 0x14, blk + 0x40);
    SU32(vec, 0x10, rx_fx_fill_dwords(16, blk, value_p));
    return vec;
}
RX_FUNCTION(0x0042b410, rx_vector_int_construct16)

/* 004242f0 (cdecl) -> 0x00d72aac */
rx_addr rx_water_heights_init(void) {
    const rx_addr sp0 = rx_sp(), L = sp0 - 4u;                          /* push ecx: the 0 value at [esp] */
    rx_set_sp(L);
    rx_addr p;
    { const rx_addr sp = rx_sp(); rx_set_sp(sp - 8u); p = rx_reflex_aligned_alloc(4); rx_set_sp(sp); }   /* push 4; call: the callee's entry ESP is L - 8 */
    if (p) SU32(p, 0, 0x00d72ab0);
    SU32(0, 0x00d72ab0, p);
    SU32(L, 0, 0);
    { const rx_addr sp = rx_sp(); rx_set_sp(sp - 12u); (void)rx_vector_int_construct16(0x00d72ab0, L); rx_set_sp(sp); }
    { const rx_addr sp = rx_sp(); rx_set_sp(sp - 4u); (void)rx_vector_clear(0x00d72ab0); rx_set_sp(sp); }
    rx_set_sp(sp0);
    return 0x00d72aac;
}
RX_FUNCTION(0x004242f0, rx_water_heights_init)
