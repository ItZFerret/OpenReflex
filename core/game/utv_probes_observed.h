#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ECX=self, RET0. Sole canonical UTV kernel; caller models physical RET.
 * Supplement returns actual XMM0 only; other SIMD scratch is not claimed. */
void rx_sim_utv_update_probes_observed(rx_addr self,rx_v4*live_xmm0);
#ifdef __cplusplus
}
#endif
