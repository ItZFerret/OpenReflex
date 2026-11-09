#pragma once
#include "rx_game.h"
typedef struct rx_career_player_dependencies {
 uint32_t (*event_capacity)(rx_addr event); /*005b2750 calls event vf18(+48)*/
 uint32_t (*vehicle_model)(rx_addr list,uint32_t vehicle_id); /*full00541040, including uncreated spec fallback*/
 void (*reset_requests)(uint32_t row); /*004de2f0*/
} rx_career_player_dependencies;
void rx_career_series_ai_ids(rx_addr vector,uint32_t series_id); /*005b7300, caller-owned canonical game vector*/
void rx_career_setup_ai_riders(rx_addr manager,const rx_career_player_dependencies* deps); /*005b2750*/
void rx_career_populate_players(rx_addr manager,const rx_career_player_dependencies* deps); /*005b2a10*/
uint32_t rx_career_player_vehicle_model(uint32_t row,const rx_career_player_dependencies* deps); /*005b0530*/
/*0059e7a0 already exists as rx_player_list_prune in build_activity_setup.c.
 * Reuse it directly; there is deliberately no duplicate pruning owner here. */
