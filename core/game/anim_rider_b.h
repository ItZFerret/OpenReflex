/* anim_rider_b.h -- the rider's procedural limb states (core/game/anim_rider_b*.c): the dispatcher 0062aed0 and its modes. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_rider_limb_state_eval(rx_addr st, rx_addr pose);                                                      /* 0062aed0 */
void rx_rider_limb_rest_target(rx_addr st, rx_addr out);                                                      /* 0062ae60 */
void rx_rider_limb_swing_target(rx_addr st, rx_addr out);                                                     /* 0062ab30 */
void rx_rider_limb_reach(rx_addr st, rx_addr pose, rx_addr rec_a, rx_addr rec_b, rx_addr rec_c);                   /* 0062d970 */
void rx_rider_limb_rest(rx_addr st, rx_addr pose, rx_addr rec_a, rx_addr rec_b, rx_addr rec_c);                    /* 0062fa90 */
void rx_rider_limb_yaw_fixed(rx_addr st, rx_addr pose);                                                       /* 0062b0e0 */
void rx_rider_limb_yaw_signed(rx_addr st, rx_addr pose);                                                       /* 0062b3f0 */
void rx_rider_limb_swing_blend_a(rx_addr st, rx_addr pose);                                                   /* 0062b710 */
void rx_rider_limb_swing_blend_b(rx_addr st, rx_addr pose);                                                   /* 0062c840 */
#ifdef __cplusplus
}
#endif
