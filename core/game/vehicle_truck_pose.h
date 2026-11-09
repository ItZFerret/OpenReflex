#pragma once
#include "../src/rx_mem.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_truck_publish_rider(rx_addr vehicle,float dt); /*00472d50 ECX=V s0=dt RET4*/
void rx_truck_update_model_pose(rx_addr vehicle,float dt); /*0046c090 ECX=vehicle, s0=dt RET4*/
void rx_truck_update_model_pose_at(rx_addr vehicle,float dt,uint32_t caller_ebx);
#ifdef __cplusplus
}
#endif
