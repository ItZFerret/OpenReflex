#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/*007c5e80: ECX normal out; stack terrain/point; RET8 belongs to enclosing caller.
 * Exact API retains original entered ESP. Real owner/outputs must be disjoint
 * from its private aligned C0 frame; BODY_PROBE's B20/B30 are above entry. */
uint32_t phys_terrain_height_normal_exact(rx_addr normal,rx_addr terrain,rx_addr point,rx_v4* live_xmm0);
enum rx_height_role {RX_HEIGHT_CELL_SETUP,RX_HEIGHT_CELL_OFFSETS,RX_HEIGHT_SAMPLE_BASE,RX_HEIGHT_SAMPLE_DEFORM};
/* Already entered exact frame; no fabricated arguments/return word. The cdecl
 * setup/window roles retain their caller arguments at expected_sp. */
rx_v4 rx_terrain_height_dependency(enum rx_height_role role,rx_addr expected_sp);
/* Separate texture route admits height-specific actual GPR/RA contracts while
 * reusing the existing typed imported deformation resource owner. */
void rx_terrain_height_texture(rx_addr texture,unsigned slot,rx_addr expected_sp);
void* rx_terrain_height_bind(void* machine);
#ifdef __cplusplus
}
#endif
