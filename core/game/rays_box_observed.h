#pragma once
#include "loop_vehicle.h"
#ifdef __cplusplus
extern "C" {
#endif
/*Actual ECXrays/EDXt_rays/EDIset; five stackargs box/q_box/t_box/q_rays/invert.
 *RetainsenteredESP; caller modelsRET14. No nativegamecallbacks/registrations.
 *Observed liveXMM0 is actualpackedreturn, including upperlane zeros/nohit. */
void rx_collision_rays_vs_box_observed(rx_addr rays,rx_addr box,rx_addr q_box,rx_addr t_box,rx_addr q_rays,uint32_t invert,rx_addr t_rays,rx_addr set,rx_v4*live);
#ifdef __cplusplus
}
#endif
