/* anim_scenario_finish.h -- the post-race presentation scenarios (anim_scenario_finish.c):
 *   RaceFinishScenario  (vtable 0x009433d4; the race activity's +0x114260): the finish replay and the podium;
 *   FMXFinishScenario   (vtable 0x0094414c; the Freestyle activity's +0x114250): the same for a Freestyle event;
 *   FMXResultsScenario  (vtable 0x0094418c; the Freestyle activity's +0x129420): the results podium.
 * The other slots of these vtables are shared with the Waypoint / results scenarios (waypoint_finish_scenario.c,
 * race_finish_select.c, race_results_scenario.c) or BaseScenario. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* the podium's riders and vehicles (the level cast = world + 0x20150; its +0x40 is the activity) */
void     rx_podium_riders_set_visible(rx_addr cast, uint8_t visible, uint32_t kind);  /* 0074c590 (EDI = cast, ESI = kind; stack visible; RET 4) */
void     rx_podium_vehicles_set_visible(rx_addr cast, uint32_t visible, uint32_t kind); /* 0074c620 (stdcall cast, visible, kind; RET 0xc) */
/* RaceFinishScenario */
void     rx_race_finish_vf0(rx_addr sc);                     /* 0074a1e0 RaceFinishScenario::vf0 (thiscall): the start */
void     rx_race_finish_vf1(rx_addr sc, uint32_t step);      /* 0074a340 RaceFinishScenario::vf1 (thiscall; RET 4): begin a step */
void     rx_race_finish_vf6(rx_addr sc);                     /* 0074a540 RaceFinishScenario::vf6 (thiscall): the end */
/* FMXFinishScenario */
void     rx_fmx_finish_debug_standings(uint32_t step, rx_addr sc); /* 0074dd40 (EAX = step, EDI = scenario) */
void     rx_fmx_finish_vf0(rx_addr sc);                      /* 0074df30 FMXFinishScenario::vf0 (thiscall) */
void     rx_fmx_finish_vf1(rx_addr sc, uint32_t step);       /* 0074e050 FMXFinishScenario::vf1 (thiscall; RET 4) */
uint32_t rx_fmx_finish_vf3(rx_addr sc);                      /* 0074de60 FMXFinishScenario::vf3 (thiscall) -> EAX: the next step */
void     rx_fmx_finish_vf6(rx_addr sc);                      /* 0074e140 FMXFinishScenario::vf6 (thiscall) */
uint32_t rx_fmx_finish_vf13(rx_addr sc);                     /* 0074e2a0 FMXFinishScenario::vf13 (thiscall) -> EAX: the previous step */
/* FMXResultsScenario */
void     rx_fmx_results_hide_cards(rx_addr sc);              /* 0074fc70 (EAX = scenario) */
void     rx_fmx_results_vf0(rx_addr sc);                     /* 0074fae0 FMXResultsScenario::vf0 (thiscall) */
#ifdef __cplusplus
}
#endif
