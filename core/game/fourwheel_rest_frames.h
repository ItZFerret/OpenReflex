#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* True base vf25/vf26 are shared by the three actual four-wheel vtables.
 * These scoped typed observers preserve entered SP; caller models RET0.
 * Canonical bike_sim owns every semantic decision, through narrow hooks. */
uint32_t rx_fourwheel_enter_rest_frame(rx_addr simulation);
uint32_t rx_fourwheel_leave_rest_frame(rx_addr simulation);
/* Exception-safe oracle adapter saves/restores this context binding. No
 * arbitrary product context: wrapper installs its own actual frame record. */
void* rx_fourwheel_rest_frames_bind(void* previous);
/* Canonical-source observer hooks. Only active in these two typed wrappers. */
int rx_fourwheel_rest_frames_active(void);
uint32_t rx_fourwheel_rest_frames_vcall(rx_addr self,uint32_t slot,int argc,const uint32_t* args);
void rx_fourwheel_rest_frames_speed(float speed,float elapsed);
void rx_fourwheel_rest_frames_impulse(int above_limit);
void rx_fourwheel_rest_frames_stopped(void);
rx_addr rx_fourwheel_rest_frames_normal_output(void);
void rx_fourwheel_rest_frames_normal_result(int above_limit);
#ifdef __cplusplus
}
#endif
