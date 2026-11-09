#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Original007c6160 thiscall query pointer, five stack arguments RET14h.
 * Exact API retains original entry ESP. Caller owns RET modeling. */
void phys_terrain_triangles_exact(rx_addr query,rx_addr terrain,rx_addr out,rx_addr count,int32_t capacity,rx_addr extent);
/* Private imported texture boundary only; actual caller args+RA already laid
 * out, expectedSP=entry+20 LockRect /entry+8 UnlockRect. No terrain math. */
void* rx_terrain_triangles_bind(void* machine); /* oracle imported-owner context only */
void rx_terrain_texture_exact(rx_addr texture,unsigned slot,rx_addr expected_sp);
#ifdef __cplusplus
}
#endif
