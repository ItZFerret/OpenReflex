#pragma once
#include "rx_game.h"
void rx_career_practice_binding(rx_addr channel, rx_addr event, uint32_t attach); /*005be540/6b0*/
void rx_career_advance_binding(rx_addr channel, rx_addr event, rx_addr name, rx_addr callback, uint32_t attach); /*005be600/710*/
void rx_career_event_binding(rx_addr channel, rx_addr event, rx_addr name, rx_addr callback, uint32_t attach); /*005be250/3c0*/
void rx_career_holeshot_binding(rx_addr channel, rx_addr event, uint32_t attach); /*005be770/830*/
void rx_career_base_binding(rx_addr event, rx_addr channel, uint32_t attach); /*005a8950/89b0*/
void rx_career_subscribe_fade(rx_addr event); /*005bd9e0 actual callerFadeComplete*/
void rx_career_emit_fade_out(void); /*005bdaa0*/
void rx_career_unsubscribe_page_wipe(rx_addr channel, rx_addr event); /*005bdb00*/
void rx_career_event_fade_complete(rx_addr event); /*005a9dd0*/
void rx_career_subscribe_page_wipe(rx_addr event);
void rx_career_emit_series_page_wipe(void);
void rx_career_event_practice_callback(rx_addr event,uint32_t value); /*005c3300*/

void rx_career_proxy_call_member_void(rx_addr proxy,rx_addr payload); /*005bfee0*/
/* career_moto_events.c: one member-proxy subscription (attach) or its removal; vtable / type = the proxy class and its RTTI type. */
void rx_career_member_proxy_bind(rx_addr registry, rx_addr name, rx_addr object, rx_addr callback, rx_addr vtable, rx_addr type,
                                 uint32_t attach);
