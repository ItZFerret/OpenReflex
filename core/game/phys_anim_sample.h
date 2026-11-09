/* phys_anim_sample.h -- pose samplers (phys_anim_sample.c). */
#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t phys_pose_sample(rx_addr slot, float time, rx_addr pose);                       /* 00840440 (stdcall; RET 0xc) */
uint32_t phys_pose_sample_blend(rx_addr slot_a, rx_addr slot_b, float time_a, float time_b, float w, rx_addr pose);  /* 00840b70 (RET 0x18) */
#ifdef __cplusplus
}
#endif
