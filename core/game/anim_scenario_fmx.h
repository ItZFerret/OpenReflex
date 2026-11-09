/* anim_scenario_fmx.h -- the Freestyle intro scenario FMXIntroScenario (vtable 0x0094410c), anim_scenario_fmx.c. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
rx_addr  rx_fmx_rider_clip(rx_addr player, uint32_t k);                             /* 0074da40 (ECX = player; stack k; RET 4) -> EAX */
rx_addr  rx_fmx_chief_clip(uint32_t step, rx_addr player);                          /* 0074d7a0 (ECX = step, EDX = player) -> EAX */
void     rx_fmx_place_chief(rx_addr player, rx_addr chief, rx_addr sc, rx_addr clip); /* 0074d800 (EDI = player, ESI = chief; stdcall sc, clip; RET 8) */
void     rx_fmx_place_chiefs(rx_addr sc, uint32_t step);                            /* 0074d890 (stdcall; RET 8) */
void     rx_fmx_place_riders(rx_addr sc, uint32_t step);                            /* 0074d980 (stdcall; RET 8) */
void     rx_cast_show_local_riders(rx_addr sm);                                     /* 0074c5e0 (ESI = the level cast) */
void     rx_fmx_intro_vf0(rx_addr sc);                                              /* 0074dab0 FMXIntroScenario::vf0 */
void     rx_fmx_intro_vf1(rx_addr sc, uint32_t step);                               /* 0074dbc0 FMXIntroScenario::vf1 */
uint32_t rx_fmx_intro_vf3(rx_addr sc);                                              /* 0074d700 FMXIntroScenario::vf3 */
void     rx_fmx_intro_vf6(rx_addr sc);                                              /* 0074dcf0 FMXIntroScenario::vf6 */
#ifdef __cplusplus
}
#endif
