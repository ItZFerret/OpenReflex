/* world_veh_damage.c -- the VehicleDamageBehaviorManager's element (0x410 bytes; 12 in the manager singleton 0xdb1410 from +0x10, 0047bb40 constructs them by
 * the vector constructor iterator): 004798e0, with Fn_00844100 (a collision shape handle, vtable 0x9765a8). Element: +0 / +4 0, +0x2c + 4 i 0, +0x4c the
 * eight 8-byte shape handles, +0x90 + 16 i the vectors (the constant 0xd61670), +0x110 + 4 i 0, +0x3b0 + 4 i 2, +0x3d0 + i 0 (bytes), +0x3d8 + 4 i 0.0f
 * (i < 8), +0x3fc the target 0, +0x400 0. Used by world_veh_raven.c's update (0047bd10, 0047a970). */
#include "rx_game.h"
#include "../src/rx_x86.h"
#include "fx.h"
#include "world_util.h"
#include "world_veh_damage.h"

/* 00844100 (ECX = the handle): +4 = 0, +0 = the vtable */
rx_addr rx_collision_shape_handle_construct(rx_addr h) {
    SU32(h, 4, 0);
    SU32(h, 0, 0x009765a8);
    return h;
}
RX_FUNCTION(0x00844100, rx_collision_shape_handle_construct)

/* 004798e0 (ECX = the element) */
rx_addr rx_damage_behavior_construct(rx_addr b) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~15u;
    rx_set_sp(A - 8u - 8u);                                   /* sub esp, 8; push ebx; push esi */
    WORLD_CALL_SP(16, rx_fx_vector_ctor_iterator(b + 0x4c, 8, 8, 0x00844100));   /* 004022d0, the registered port */
    SU32(b, 0x3fc, 0); SU32(b, 0, 0); SU32(b, 4, 0); SU32(b, 0x400, 0);
    for (uint32_t i = 0; i < 8; i++) {
        SU32(b, 0x2c + 4 * i, 0);
        SF32(b, 0x3d8 + 4 * i, 0.0f);
        SU32(b, 0x110 + 4 * i, 0);
        rx_st4(b + 0x90 + 16 * i, rx_ld4(0x00d61670));
        SU32(b, 0x3b0 + 4 * i, 2);
        SU8(b, 0x3d0 + i, 0);
    }
    rx_set_sp(sp0);
    return b;
}
RX_FUNCTION(0x004798e0, rx_damage_behavior_construct)
