#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_career_progress_load(rx_addr progress,uint32_t controller,uint32_t series_id,uint32_t owner);
void rx_career_progress_commit(rx_addr progress);
void rx_career_progress_resolve_event(rx_addr progress);
void rx_career_progress_set_vehicle(rx_addr progress,uint32_t excluded_type);
uint32_t rx_career_vehicle_class_for_types(rx_addr vehicle_types);
uint32_t rx_career_series_format(rx_addr text);
uint32_t rx_career_series_ai_format(rx_addr text);
uint32_t rx_career_activity_type_from_name(rx_addr text);
uint32_t rx_career_event_activity_type(uint32_t event_type_id);
#ifdef __cplusplus
}
#endif
