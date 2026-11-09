/* loop_anim.h -- the skeleton animation core (loop_anim.c). Register arguments of the LTCG originals follow the stack ones. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void    rx_skeleton_pose_reset(rx_addr skel, rx_addr pose);                                  /* 0082e250 (EAX, EDX) */
rx_addr rx_skeleton_pose_acquire(rx_addr skel);                                              /* 0082e2d0 (EDI) */
rx_addr rx_skeleton_bone_to_model(rx_addr skel, rx_addr out, uint32_t bone, rx_addr pose, rx_addr vec); /* 0082efb0 (ECX, EDX, EDI) */
void    rx_skeleton_bone_to_model_scalar(rx_addr skel, uint32_t bone, rx_addr pose, rx_addr out); /* 0082e610 (ECX, EAX, EDI) */
void    rx_animkeys_locate(rx_addr ks, rx_addr out, float t);                                /* 0082b930 (stack out; ECX, xmm0) */
void    rx_animkeys_sample_rotation(rx_addr ks, uint32_t track, rx_addr out);                /* 0082b9e0 (ECX, EDX, ESI) */
void    rx_animkeys_sample_translation(rx_addr ks, uint32_t track, rx_addr out, rx_addr at); /* 0082bac0 (stack ks; ECX, EDX, EDI) */
void    rx_motion_sample(rx_addr map, rx_addr pose, rx_addr ks, float t);                   /* 00840040 (stack map; ECX, ESI, xmm0) */
void    rx_skeleton_animate(rx_addr ctx, rx_addr skel);
rx_addr rx_rider_anim_worker_run(uint32_t index, uint32_t* quit);                                          /* 0061cdb0 RiderAnimation_WorkerThread: one wake */                                      /* 0082ff20 (ECX, EDI) */
#ifdef __cplusplus
}
#endif
