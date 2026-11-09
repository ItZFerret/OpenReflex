#pragma once
#include "career_single.h"
#define RX_MOTO_OBSERVATION_VERSION 1u
#define RX_MOTO_OBSERVATION_ROWS 12u
#define RX_MOTO_OBSERVATION_NAME 128u
typedef struct { int32_t row,player_type,player_state,score_one,score_two; float sort_value; char name[RX_MOTO_OBSERVATION_NAME]; } rx_moto_score_row;
typedef struct { uint32_t size,version,series_id,sub_event,phase,next_phase,post_complete,row_count; rx_moto_score_row rows[RX_MOTO_OBSERVATION_ROWS]; } rx_moto_observation;
/* Game-internal copied actual rows, zero mutation. NOT a public session API. */
int rx_career_moto_observe(rx_addr event,rx_moto_observation*out);
