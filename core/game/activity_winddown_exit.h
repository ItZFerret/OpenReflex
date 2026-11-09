#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* 00561ec0: shared Activity slot34; ECX=activity, stack=new state, RET4.
 * Neither argument is read by the original owner. */
void rx_activity_winddown_exit(rx_addr activity, uint32_t new_state);
#ifdef __cplusplus
}
#endif
