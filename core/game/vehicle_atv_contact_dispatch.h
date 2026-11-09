#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_atv_query_wheel_terrain(rx_addr W,rx_addr S);
void rx_atv_correct_contact_positions(rx_addr S);
void rx_atv_pin_rest_position(rx_addr S);
void rx_atv_update_rolling_orientation(rx_addr S,float dt);
void rx_atv_contact_dispatch(rx_addr S,float dt);
#ifdef __cplusplus
}
#endif
