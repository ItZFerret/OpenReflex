/* phys_rig.h -- the rider rig's physics side (phys_rig.c). */
#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif
void phys_rig_capture_bone_poses(rx_addr rig);      /* 00426a50 (EDI = rider animation) */
int32_t phys_rig_bone_of_node(rx_addr rig, uint32_t node);   /* 004296d0 (EDX = rig, ESI = node) */
void phys_rig_camera_speed(rx_addr rig);            /* 0042ade0 (stack rig; RET 4) */
#ifdef __cplusplus
}
#endif
