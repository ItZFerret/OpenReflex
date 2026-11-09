#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Finite vf15 call sites only. No registration or original-code fallback. */
void rx_family_response_clamp_frame(rx_addr simulation, rx_addr expected_sp);
void rx_family_response_velocity_frame(rx_addr mechanism, rx_addr vector, rx_addr expected_sp);
#ifdef __cplusplus
}
#endif
