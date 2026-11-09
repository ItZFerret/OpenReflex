#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_career_rewards_calculate(rx_addr rewards,uint32_t controller,int32_t zero_based_place);
void rx_career_series_highlights(rx_addr tracking,uint32_t controller,int32_t series,int32_t place);
void rx_career_rewards_apply(rx_addr rewards,uint32_t controller); /* 005b20b0, stack rewards/controller, RET8 */
#ifdef __cplusplus
}
#endif
