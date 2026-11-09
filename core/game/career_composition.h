#pragma once
#include "career_session_host.h"
#include "career_lifecycle.h"
#include "career_single.h"
#ifndef RX_ORACLE
/* Product-owned request acceptance, not a placeholder UI or durable-save claim.
 * Callbacks execute synchronously on the session worker; copy guest strings
 * before returning if queued. Every zero return rejects/fails the operation. */
typedef struct rx_career_host {
 void* context;
 uint32_t (*ui_event)(void*context,rx_addr name);
 uint32_t (*ui_route)(void*context,rx_addr name);
 uint32_t (*ui_goto_hub)(void*context);
 uint32_t (*route_accept)(void*context,rx_addr route);
 uint32_t (*request_checkpoint)(void*context,uint32_t controller);
} rx_career_host;
/*Call after a genuine native session/UI event channel exists, before career
 * properties are seeded. No default properties, roster, rewards or statuses.*/
void rx_career_composition_install(const rx_career_host*host);
/*Must run BEFORE session teardown/static restore; this clears host pointers
 * and Single dependency binding. Does not end/commit/save an active series.*/
void rx_career_composition_dispose(void);
rx_addr rx_career_composition_load(uint32_t controller,uint32_t series_id,uint32_t owner);
/* 1 when the loaded series' format runs on a composed series event class (Single or Moto) and its Series row agrees. */
int rx_career_composition_format_supported(rx_addr manager);
void rx_career_composition_select_vehicle(uint32_t excluded_type);
void rx_career_composition_initialize(void);
int rx_career_composition_start_session(struct rx_session*session);
void rx_career_composition_complete_event(void);
void rx_career_composition_complete_series(void);
void rx_career_composition_end(uint32_t save);
const rx_career_single_dependencies* rx_career_composition_single_dependencies(void);
#endif
