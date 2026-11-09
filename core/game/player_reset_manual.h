#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_player_reset_manual(rx_addr player); /*00550400 ESI RET0*/
#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
void rx_player_reset_out_of_bounds(rx_addr player); /*00550a20 ESI RET0*/
#ifdef __cplusplus
}
#endif
