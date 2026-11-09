#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* EAX vehicle/EDX manager/ESI out, no stack arguments, RET0. */
void rx_clip_damage_copy_positions(rx_addr vehicle,rx_addr manager,rx_addr out);
void rx_clip_damage_copy_states(rx_addr vehicle,rx_addr manager,rx_addr out);
#ifdef __cplusplus
}
#endif
