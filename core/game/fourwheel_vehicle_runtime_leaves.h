#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t rx_utv_buggy_vehicle_kind(rx_addr v); /*004ad530 RET0*/
uint32_t rx_truck_vehicle_kind(rx_addr v); /*004744a0 RET0*/
uint32_t rx_utv_vehicle_has_nodes(rx_addr v); /*004c0360 RET0*/
uint32_t rx_buggy_vehicle_has_nodes(rx_addr v); /*004a2520 RET0*/
uint32_t rx_truck_vehicle_has_nodes(rx_addr v); /*0046a3b0 RET0*/
uint32_t rx_utv_vehicle_has_id(rx_addr v,uint32_t id); /*004c7e90 RET4*/
uint32_t rx_buggy_vehicle_has_id(rx_addr v,uint32_t id); /*004ad540 RET4*/
uint32_t rx_truck_vehicle_has_id(rx_addr v,uint32_t id); /*00474460 RET4*/
rx_f80 rx_fourwheel_vehicle_zero_value(rx_addr v,uint32_t ignored); /*004c0390 RET4 ST0*/
rx_f80 rx_vehicle_one_value(rx_addr v,uint32_t ignored); /*00430260 RET4 ST0*/
/* Actual typed caller uses raw float bits for threshold and ignored arg. */
uint32_t rx_utv_vehicle_safe_pose(rx_addr v,rx_addr p,rx_addr q,uint32_t threshold,uint32_t ignored);
uint32_t rx_buggy_vehicle_safe_pose(rx_addr v,rx_addr p,rx_addr q,uint32_t threshold,uint32_t ignored);
uint32_t rx_truck_vehicle_safe_pose(rx_addr v,rx_addr p,rx_addr q,uint32_t threshold,uint32_t ignored);
/* Four exact original child roles: V45,V46,world lookup,material. Entered ESP
 * retained until provider models its RET0. No generic target fallback. */
void rx_fourwheel_safe_pose_dependency(unsigned family,unsigned role,rx_addr target,rx_addr expected);
#ifdef __cplusplus
}
#endif
