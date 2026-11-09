/* bike_force.h -- the force stage (slot 11) of the motocross simulation (bike_force.c): gears, engine, axle torques, suspension,
 * wheel drag. S = the simulation object (RavenSimulationMotocross). */
#pragma once
#include "bike_math.h"
#ifdef __cplusplus
extern "C" {
#endif
float   bike_engine_coupling_spring(rx_addr S, float factor, float dt);  /* 00446a40 BikeEngine_CouplingSpring (returns the root-local Y of S+160) */
void    bike_wheels_apply_axle_torques(rx_addr S);                      /* 00447cd0 BikeWheels_ApplyAxleTorques */
int32_t bike_gear_select_downshift(rx_addr S);                          /* 004460f0 BikeGear_SelectDownshift (returns the best ratio index, -1 when gated) */
int32_t bike_gear_auto_select(rx_addr S);                               /* 00445e80 BikeGear_AutoSelect (same) */
void    bike_wheel_drag_forces(rx_addr S);                              /* 0043efa0 BikeWheel_DragForces */
void    bike_engine_drive_forces(rx_addr S, float dt);                  /* 00446340 BikeEngine_DriveForces */
void    bike_suspension_spring_damper_forces(rx_addr S);                /* 0043c590 BikeSuspension_SpringDamperForces */
void    bike_force_stage(rx_addr S, float dt);                          /* 0043e100 RavenSimulationMotocross::vf11 */
#ifdef __cplusplus
}
#endif
