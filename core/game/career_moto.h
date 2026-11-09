#pragma once
#include "career_single.h"
/* Separate original Moto class, not Single aliases. No public format activation. */
uint32_t rx_career_moto_player_state(uint32_t phase); /*005aeeb0 shared*/
void rx_career_moto_bind(rx_addr event,rx_addr channel,uint32_t attach,const rx_career_single_dependencies* deps);
void rx_career_moto_holeshot(rx_addr event,uint32_t enable,const rx_career_single_dependencies* deps);
void rx_career_moto_gate_result(rx_addr event,uint32_t value,const rx_career_single_dependencies* deps);
void rx_career_moto_advance(rx_addr event,const rx_career_single_dependencies* deps);
void rx_career_moto_post_event(rx_addr event,const rx_career_single_dependencies* deps);
void rx_career_moto_reset(rx_addr event,const rx_career_single_dependencies* deps); /*005ac8f0*/
int32_t rx_career_event_standing_rank(rx_addr event,rx_addr name,uint32_t type,const rx_career_single_dependencies* deps); /*005ac050*/
void rx_career_event_sort_drivers(rx_addr activity,const rx_career_single_dependencies* deps); /*005ac3d0*/
void rx_career_event_set_standing_points(rx_addr name,int32_t points,int32_t series,const rx_career_single_dependencies* deps); /*005ac6c0*/
uint32_t rx_career_moto_event_call(rx_addr event,uint32_t slot,uint32_t argument,const rx_career_single_dependencies* deps);
