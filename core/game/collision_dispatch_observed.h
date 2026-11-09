#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/*85ddd0, ECX manifold +7 stack arguments, RET1c. Exact API retains entered
 * ESP; outer caller owns RET. Admitted finite type flags documented in the
 * checkpoint. No pair physics or registered dispatch is duplicated. */
void phys_contact_dispatch_observed(rx_addr manifold,rx_addr a,rx_addr qa,rx_addr pa,rx_addr b,rx_addr qb,rx_addr pb,uint32_t flag);
/* One already-entered pair role, with real RA/args/GPR: the ten typed closures
 * of collision_triangle_ray.c, every other case through the dispatcher's case
 * table (phys_contact_pair_observed). */
void rx_collision_dispatch_pair_observed(rx_addr target,rx_addr return_pc,unsigned nargs,rx_addr expected_sp,uint32_t compare_left,uint32_t compare_right,unsigned compare_width);
void* rx_collision_dispatch_bind(void* machine);
#ifdef __cplusplus
}
#endif
