/* anim_scenario_misc.h -- the FreeRide podium / crew chief scenario setup (0074ada0 and its helpers; miscport). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_tree_free_subtree(rx_addr map, rx_addr node);                         /* 0068f300 (ECX = map, stack node; RET 4) */
void rx_crew_chief_set_shirt(rx_addr driver, rx_addr chief);                  /* 006bd760 CrewChief_SetShirt (EAX = the shirt's vehicle owner, stack chief; RET 4) */
void rx_podium_release(rx_addr podium, uint32_t clear);                       /* 0074b180 (EAX = podium; stack flag; RET 4) */
void rx_podium_set_shirts(rx_addr podium);                                    /* 0074c6d0 (EDI = podium) */
void rx_podium_riders_create(rx_addr podium);                                 /* 0074ba40 PodiumRiders_Create (stack podium; RET 4) */
void rx_podium_setup(rx_addr activity, rx_addr podium);                       /* 0074ada0 (EAX = activity, ECX = podium) */
#ifdef __cplusplus
}
#endif
