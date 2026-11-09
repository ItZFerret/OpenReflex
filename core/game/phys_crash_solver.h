/* phys_crash_solver.h -- the rider ragdoll against the bike: 00426d00, the crash solver the crash world (REFLEX_DRIVE=7) and
 * the product's first race frames reach. Called from 00425640 (phys_rider_crash_step). */
#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif
/* 00426d00 (stdcall A, dt; RET 8): the ragdoll's bone rotations slaved to the bike's node transforms during a crash. */
void phys_crash_bike_solver(rx_addr A, float dt);
#ifdef __cplusplus
}
#endif
