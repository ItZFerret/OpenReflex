#pragma once
#include "waypoint_avoid_objects.h"
/* Preserve every prior typed route; bind only the new actual behavior owner.
 * This also enables incremental fixtures using the unchanged V9 registry. */
#define waypoint_behavior_update waypoint_behavior_update_existing
#include "waypoint_driver_calls.h"
#undef waypoint_behavior_update
static inline uint32_t waypoint_behavior_update(rx_addr object,float dt){
 if(U32(U32(object,0),0x10)==0x00520ba0){
  uint32_t result;ACT_CALLR(result,1,rx_waypoint_avoid_objects_update(object,dt));return result;
 }
 return waypoint_behavior_update_existing(object,dt);
}
