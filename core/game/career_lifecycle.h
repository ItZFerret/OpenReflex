#pragma once
#include "career_start.h"
#include "career_refresh.h"
typedef struct rx_career_lifecycle_dependencies {
 const rx_career_start_dependencies* start;
 rx_career_vehicle_model_resolver vehicle_model;
 void (*advance_victory)(rx_addr event); /*005aa1c0, actual Single owner*/
 void (*ui_route)(rx_addr name); /*005db4c0 actual owned presentation request*/
 /* Must enqueue an actual Player24 checkpoint for the given controller; a
  * rejected request must fail the operation. Acceptance is not durable-save
  * completion. The host preserves its checkpoint failure reporting. */
 void (*request_save)(uint32_t controller);
} rx_career_lifecycle_dependencies;
void rx_career_bind_manager_channel(rx_addr manager,rx_addr channel); /*005b2690*/
void rx_career_end_series(rx_addr manager,uint32_t save,const rx_career_lifecycle_dependencies* deps); /*005b3fb0*/
void rx_career_commit_initialize(rx_addr manager,const rx_career_lifecycle_dependencies* deps); /*005b4160*/
void rx_career_complete_event(rx_addr manager,const rx_career_lifecycle_dependencies* deps); /*005b41b0*/
void rx_career_complete_series(rx_addr manager,const rx_career_lifecycle_dependencies* deps); /*005b4310*/
