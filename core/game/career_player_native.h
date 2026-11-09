#pragma once
#include "rx_game.h"
uint32_t rx_career_vehicle_model_id(rx_addr list,uint32_t vehicle_id); /*full00541040, unique spec-owner extension*/
void rx_career_player_request_vehicle(uint32_t row,uint32_t id); /*004de0b0*/
void rx_career_player_request_rider(uint32_t row,uint32_t id); /*004de1d0*/
void rx_career_player_reset_requests(uint32_t row,rx_addr zero_out); /*004de2f0 ECX=row EAX=out,RET0*/
void rx_career_reset_player_requests(uint32_t row); /*typed callback convenience, same kernel*/
