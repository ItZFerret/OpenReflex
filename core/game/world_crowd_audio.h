/* world_crowd_audio.h -- the crowd unit's momentum and frame update (world_crowd_audio.c; w-sfx). */
#pragma once
#include "rx_game.h"
void    rx_crowd_smoother_start(rx_addr s);              /* 0067f1c0 (ECX = smoother) */
void    rx_crowd_smoother_step(rx_addr s, float dt);     /* 0067f210 (EAX = smoother, float dt; RET 4) */
void    rx_crowd_wander(void);                           /* 0067f6d0 */
void    rx_crowd_momentum_tick(float dt);                /* 0067f450 (float dt; RET 4) */
int32_t rx_crowd_type(void);                             /* 00674680 -> EAX */
#ifndef RX_ORACLE
void    rx_crowd_config_init(void);                      /* 00674cd0's values (product, once) */
void    rx_crowd_frame(float dt);                        /* 00674e90 (product) */
#endif
