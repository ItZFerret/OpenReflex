/* anim_scene_misc.h -- the scene's light / event-volume registry removal (007e0190) and its helpers (miscport). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t rx_event_cell_remove_c(rx_addr cell, rx_addr ev);                                           /* 007da9c0 */
uint32_t rx_event_cell_remove_d(rx_addr cell, rx_addr ev);                                           /* 007dab70 */
uint32_t rx_event_grid_remove_c(rx_addr g, uint32_t id, float radius, rx_addr pos);                  /* 007da290 (EDI = grid) */
uint32_t rx_event_grid_remove_d(rx_addr g, uint32_t id, float radius, rx_addr pos);                  /* 007da350 (EDI = grid) */
uint32_t rx_array_ptr_find(rx_addr arr, rx_addr keyp);                                               /* 007f9db0 (ECX = array, stack &key) */
void rx_array40_remove(uint32_t index, rx_addr arr);                                                 /* 007f9fd0 (ECX = index, EDX = array) */
void rx_lights_remove_entity(rx_addr lights, rx_addr e);                                             /* 007e0190 (EAX = lights; stack entity) */
#ifdef __cplusplus
}
#endif
