#pragma once
#include "rx_game.h"
void    rx_series_rewards_reset(rx_addr block, uint32_t progress);   /* 005b14d0 (ESI = the block, EAX = the progress) */
void    rx_series_progress_reset(rx_addr progress);                   /* 005b0620 (EAX = the progress) */
rx_addr rx_series_rewards_construct(rx_addr block);                   /* 005b1440 (ESI = the block) */
rx_addr rx_series_manager_construct(void);                            /* MXSeriesManager_Construct 005b2370 */
