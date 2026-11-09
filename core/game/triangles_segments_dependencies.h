#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Caller has already pushed six actual cdecl arguments and real return word.
 * Returns actual EAX, retaining actual EDX/GPR/x87 and consumed dead arguments.
 * Expected is after callee RET0, BEFORE caller ADD ESP18. */
uint32_t rx_tri_segment_rays_dependency(rx_addr expected_sp);
#ifdef __cplusplus
}
#endif
