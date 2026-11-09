/* career_heat.h -- MXSeriesEventHeat (vtable 0x0094e28c), the series event of the Heat format (Series rows 41-46): a practice
 * qualifier (optional), the heat, the last chance qualifier (LCQ) and the main event (career_heat.c, career_heat_events.c).
 *
 * The event object is the series manager's static 0x009e231c (manager + 0x194). Its fields are the base MXSeriesEvent's
 * (career_single.h): +4 the sub-event (0 for the whole event in the career: the constructor's .bss zero), +0xc the phase of the
 * current race, +0x10 the next phase, +0x14 the race's activity (set by the results overlay), +0x28 "advance" pending, +0x30 the event
 * is over, +0xb0 the limit kind (laps), +0xec / +0xf0 the holeshot player / subscription.
 * Phases: 1 the practice qualifier, 3 the heat, 4 the LCQ, 5 the main event (8 = a race is set up). PlayerState of a phase (vf1):
 * 1 / 3 -> 4 (the heat's riders; the other heat is 5), 4 -> 6 (the LCQ; 7 the local human in it), 5 -> 8 (the main event);
 * 9 / 10 = out (10 the local human). */
#pragma once
#include "career_single.h"

uint32_t rx_career_heat_player_state(uint32_t phase);                                  /* vf1  005ad840 (RET 4) */
void rx_career_heat_holeshot(rx_addr event, uint32_t enable, const rx_career_single_dependencies* deps);   /* vf2 005aee00 */
void rx_career_heat_bind(rx_addr event, rx_addr channel, uint32_t attach, const rx_career_single_dependencies* deps);   /* vf7 005ad880 / vf8 005ad8f0 */
void rx_career_heat_gate_result(rx_addr event, uint32_t practice, const rx_career_single_dependencies* deps);   /* vf10 005ad960 */
void rx_career_heat_advance(rx_addr event, const rx_career_single_dependencies* deps);      /* vf12 005adb00 */
void rx_career_heat_post_event(rx_addr event, const rx_career_single_dependencies* deps);   /* vf13 005adde0 */
uint32_t rx_career_heat_capacity(rx_addr event, const rx_career_single_dependencies* deps); /* vf18 005aedd0 */

/* The virtual slot `slot` of the Heat event, as rx_career_single_event_call dispatches the Single and Moto events. */
uint32_t rx_career_heat_event_call(rx_addr event, uint32_t slot, uint32_t argument, const rx_career_single_dependencies* deps);

/* career_aaa_loading.c: MXUIAAALoadingPage::vf4 005a8030 (thiscall page; the product passes 0: the page is the presenter's) */
void rx_aaa_loading_page_enter(rx_addr page);
