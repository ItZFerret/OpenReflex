#pragma once
#include "rx_game.h"
/* Explicit gameplay property/package/loading boundary. No callback may report
 * package/level success before the real operation succeeds. This contract does
 * not activate Career and is not an exhibition event-spec fallback. */
typedef struct rx_career_route_dependencies {
 /*Product has no legacy UI object. This required alternative must validate a
  * real supported owned presentation route and accept it before returning1.
  * No default callback/always-success registry is supplied here.*/
 uint32_t (*compare_owned_route)(rx_addr route);
 uint32_t (*get_int)(rx_addr key,uint32_t initial);
 void (*get_string)(rx_addr ui,rx_addr key,rx_addr output,uint32_t capacity);
 /*005dbcb0/005dbda0: actual localized/raw database existence lookup and
  * package-cache request/release/unmark, not a string-only host route flag.*/
 uint32_t (*acquire_route)(rx_addr database_name,rx_addr route,rx_addr ui,uint32_t wait);
 void (*release_route)(rx_addr database_name,rx_addr route,rx_addr ui);
 /*Native session adapter must open exact fmt_level and rx_event_set_level
  * without rebuilding PlayerList/ActivityTable or replacing career properties.
  * Zero means failure: request MUST stop, never claim successful loading.*/
 uint32_t (*prepare_level)(rx_addr location);
 void (*session_metadata)(rx_addr ui); /*0059cd40; explicit until offline/session policy is proved*/
} rx_career_route_dependencies;
void rx_career_build_route(rx_addr output,uint32_t capacity,uint32_t kind,uint32_t activity_mode,uint32_t player_mode,rx_addr prefix,const rx_career_route_dependencies* deps); /*005e3000*/
uint32_t rx_career_compare_route(rx_addr ui,rx_addr route,const rx_career_route_dependencies* deps); /*005a4350 ESI=ui*/
void rx_career_prepare_gameplay_routes(rx_addr ui,const rx_career_route_dependencies* deps); /*005a43e0*/
void rx_career_request_level(rx_addr ui,uint32_t prepare_gameplay,const rx_career_route_dependencies* deps); /*0059bd60*/
