#pragma once
#include "rx_game.h"
/* Required native composition. None of these may be satisfied by an empty host
 * callback or a generic call-original fallback. A provider must reject unsupported
 * event classes/slots explicitly. Slots are vtable byte offsets divided by four. */
typedef struct rx_career_start_dependencies {
 uint32_t (*event_call)(rx_addr event,uint32_t slot,uint32_t argument);
 void (*ui_int)(rx_addr key,uint32_t value);
 void (*ui_string)(rx_addr key,rx_addr value);
 uint32_t (*ui_get_int)(rx_addr key,uint32_t initial);
 void (*clear_nonhuman_players)(void);                    /*0059e7a0*/
 void (*setup_ai_riders)(rx_addr manager);                /*005b2750*/
 void (*populate_players)(rx_addr manager);               /*005b2a10, substantive3169B*/
 void (*seed_standings)(rx_addr manager,int32_t selector);/*005b4d30*/
 void (*reset_player_requests)(uint32_t row);             /*004de2f0*/
 void (*manager_refresh)(rx_addr manager,uint32_t value); /*005b4210*/
 void (*build_route)(rx_addr output,uint32_t capacity,uint32_t type,uint32_t mode,uint32_t player_mode,rx_addr suffix); /*005e3000*/
 uint32_t (*compare_update_route)(rx_addr route);         /*005a4350*/
 /* Must deliver the real configured-level request, including original level
  * selection/player reconciliation semantics; it is NOT a presentation no-op.
  * Return only after the host has accepted/copied the request; failure is fatal. */
 void (*request_configured_level)(rx_addr ui,uint32_t changed); /*0059bd60*/
} rx_career_start_dependencies;
rx_addr rx_career_event_for_format(rx_addr manager,uint32_t format); /*005b6900*/
void rx_career_select_event(rx_addr manager,uint32_t event_type,uint32_t format,const rx_career_start_dependencies* deps); /*005b6950 EAX=format,RET8*/
void rx_career_initialize_series(rx_addr manager,const rx_career_start_dependencies* deps); /*005b3b50 RET4*/
void rx_career_setup_event_players(rx_addr manager,const rx_career_start_dependencies* deps); /*005b6ab0 RET4*/
void rx_career_start_event(rx_addr manager,const rx_career_start_dependencies* deps); /*005b7210 RET4*/
