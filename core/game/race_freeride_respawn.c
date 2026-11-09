/* 00561650 MXActivityFreeRide::vf19: select a permitted roam node, snap
 * its position to the base terrain, and return its forward axis and world up.
 * Source: decompile plus 00561650..00561815 disassembly. The original keeps
 * the old node if the next section has no first node. No guessed fallback.
 * Frame: EBP, align16, sub0x34, three saved registers; point at F+0x20.
 */
#include "race_activity.h"
#include "race_extern.h"
#include "world_misc.h"
#include "world_board_handlers.h"
#include "phys_terrain.h"
#include "fx.h"

uint32_t rx_free_ride_respawn_point(rx_addr act, rx_addr player, rx_addr pos, rx_addr dir, rx_addr up) {
    RACE_FRAME_EBP(F, 16, 0x34, 3);
    uint32_t index;
    RACE_CORE_CALL(0, index = (uint32_t)rx_world_group_rider_index(act, player));
    const rx_addr driver = U32(U32(act, U32(player, 0x908) ? 0x27c : 0x268), 4 * index);
    rx_addr roam;
    RACE_CORE_CALL(1, roam = rx_driver_roam_behavior(driver));
    if (!roam) { RACE_FRAME_END(F); return 0; }
    rx_addr section = U32(roam, 0x30);
    if (!section) { RACE_FRAME_END(F); return 0; }
    rx_addr node = U32(roam, 0x34);
    const rx_addr vehicle = U32(U32(driver, 0x74), 0x1ac);
    const uint32_t flags = 0x68 + 4 * (U32(vehicle, 0x198) + 4 * U32(vehicle, 0x194));
    if ((U8(node, flags) & 2) && node) {
        rx_addr candidate = node;
        for (;;) {
            if (!(U8(candidate, flags) & 2)) { node = candidate; break; }
            candidate = U32(candidate, 0x64);
            if (candidate) continue;
            if (!U32(section, 0x10)) { RACE_FRAME_END(F); return 0; }
            section = U32(U32(section, 8), 0);
            candidate = U32(section, 0);
            if (!candidate) break;
        }
    }
    if (!section) { RACE_FRAME_END(F); return 0; }
    SU32(roam, 0x30, section); SU32(roam, 0x34, node);
    SU32(F, 0x20, U32(node, 0)); SU32(F, 0x28, U32(node, 8));
    SF32(F, 0x24, rx_adds(F32(node, 4), 5.0f));
    const rx_addr scene = U32(U32(act, 0x234), 0x20138);
    rx_addr registry, terrain;
    RACE_CORE_CALL(0, registry = rx_fx_terrain_registry());
    RACE_CORE_CALL(0, terrain = rx_terrain_for_scene(registry, scene));
    uint32_t edx = F + 0x20;
    RACE_CORE_CALL(1, (void)phys_terrain_base_height_at(terrain, F + 0x20, &edx));
    for (uint32_t i = 0; i < 3; i++) SU32(pos, 4 * i, U32(F, 0x20 + 4 * i));
    /* FLD/FSTP copies quiet signaling NaNs; preserve the sequential stores. */
    for (uint32_t i = 0; i < 3; i++) {
        uint32_t bits = U32(node, 0x50 + 4 * i);
        if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x007fffffu)) bits |= 0x00400000u;
        SU32(dir, 4 * i, bits);
    }
    SU32(up, 0, 0); SU32(up, 4, 0x3f800000); SU32(up, 8, 0);
    RACE_FRAME_END(F); return 1;
}
RX_FUNCTION(0x00561650, rx_free_ride_respawn_point)
