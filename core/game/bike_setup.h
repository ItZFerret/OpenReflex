/* bike_setup.h -- the motocross simulation's ground placement (bike_setup.c). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_sim_moto_ground_place(rx_addr sim, rx_addr pos, rx_addr dir, rx_addr out_pos, rx_addr out_fwd, rx_addr out_up);   /* 0043d1f0 RavenSimulationMotocross::vf20 (thiscall; RET 0x14) */
#ifdef __cplusplus
}
#endif
