#pragma once
#include "rx_game.h"
rx_addr rx_tbl_string_row(rx_addr column,rx_addr text); /*004f64a0*/
rx_addr rx_tbl_crit_copy_string(rx_addr criterion,rx_addr name,rx_addr text); /*004f7420*/
uint32_t rx_player_event_status(uint32_t controller,rx_addr name); /*004eaaf0*/
void rx_player_event_status_set(uint32_t controller,rx_addr name,uint32_t value); /*004eabd0*/
void rx_app_pre_activity_rules_page_done(void); /*00742a00*/
#ifndef RX_ORACLE
/* Explicit owned-UI equivalents of the original RulesType property boundary.
 * The host callback only projects ShowRaceRulesPopup; it does not release Pre.
 * This is not a fabricated MXUIManager instance or a raw-register fallback. */
typedef struct rx_owned_rules_host {
 void*context;uint32_t controller;
 uint32_t (*show)(void*,rx_addr activity,rx_addr rules_type);
} rx_owned_rules_host;
uint32_t rx_owned_rules_available(void);
void rx_owned_rules_bind(const rx_owned_rules_host*);
void rx_owned_rules_unbind(void*context);
uint32_t rx_owned_rules_select(rx_addr activity);
void rx_owned_rules_show(rx_addr activity);
#endif
