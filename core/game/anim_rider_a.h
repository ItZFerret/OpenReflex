/* anim_rider_a.h -- the RiderCharacter's procedural pose (core/game/anim_rider_a*.c): the RiderAnimation worker's node wobble /
 * nod, the head bone transform, the lean controller (ProceduralAnimator +0x58) and the two-limb IK controller (ProceduralAnimator
 * +0x54) with its two-bone solver. Layouts: core/notes/anim.md. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

/* anim_rider_a.c */
void rx_rider_anim_head_transform(rx_addr self, rx_addr pos_out, rx_addr quat_out);                          /* 00623bd0 */
void rx_rider_anim_noise_wobble(rx_addr self, rx_addr axis0, float rate, float dt, float amount,
                                rx_addr axis1, rx_addr axis2);                                                  /* 00623c80 */
void rx_rider_anim_nod(rx_addr self);                                                                           /* 00623f30 */
void rx_rider_lean_eval(rx_addr lean, rx_addr pose);                                                            /* 00629910 */
void rx_rider_lean_rotate_bone(rx_addr quat, rx_addr lean);                                                     /* 00629a60 */

/* anim_rider_a_ik.c */
void rx_rider_limbs_eval(rx_addr limbs, rx_addr ch, rx_addr pose);                                              /* 006259c0 */
void rx_rider_limb_solve(rx_addr target, rx_addr limbs, rx_addr parent, rx_addr bone, float weight,
                         rx_addr s0, rx_addr s1, rx_addr s2);                                                   /* 00626740 */

#ifdef __cplusplus
}
#endif
