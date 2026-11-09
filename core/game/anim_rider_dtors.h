/* anim_rider_dtors.h -- the vehicle / rider / scenario entity destructors (anim_rider_dtors.c). See the file header. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
rx_addr rx_raven_vehicle_sdtor(rx_addr v, uint32_t flags);              /* 0045d3a0 RavenVehicle::sdtor (thiscall; RET 4) -> this */
rx_addr rx_motocross_vehicle_sdtor(rx_addr v, uint32_t flags);          /* 00462560 RavenVehicleMotocross::sdtor (thiscall; RET 4) -> this */
rx_addr rx_scenario_anim_entity_sdtor(rx_addr e, uint32_t flags);       /* 006b7720 ScenarioAnimEntity::sdtor (thiscall; RET 4) -> this */
void    rx_motocross_sim_destruct(rx_addr sim);                         /* 0043c040 (ECX = the simulation) */
rx_addr rx_motocross_sim_sdtor(rx_addr sim, uint32_t flags);            /* 0043b450 RavenSimulationMotocross::sdtor (thiscall; RET 4) -> this */
rx_addr rx_procedural_animator_sdtor(rx_addr a, uint32_t flags);        /* 006b5820 ProceduralAnimator::sdtor (thiscall; RET 4) -> this */
rx_addr rx_featherstone_animator_sdtor(rx_addr a, uint32_t flags);      /* 006afad0 FeatherstoneAnimator::sdtor (thiscall; RET 4) -> this */
void    rx_switch_animator_destroy(rx_addr a);                          /* 0083c6e0 (ECX = the animator) */
rx_addr rx_switch_animator_sdtor(rx_addr a, uint32_t flags);            /* 0083c6c0 SwitchAnimator::sdtor (thiscall; RET 4) -> this */
rx_addr rx_blend_animator_sdtor(rx_addr a, uint32_t flags);             /* 00833020 BlendAnimator::sdtor (thiscall; RET 4) -> this */
rx_addr rx_additive_animator_sdtor(rx_addr a, uint32_t flags);          /* 00834600 AdditiveAnimator::sdtor (thiscall; RET 4) -> this */
rx_addr rx_motion_animator_sdtor(rx_addr a, uint32_t flags);            /* 0083b3a0 MotionAnimator::sdtor (thiscall; RET 4) -> this */
void    rx_performance_animator_destroy(rx_addr a);                     /* 0083d360 (ECX = the animator) */
rx_addr rx_performance_animator_sdtor(rx_addr a, uint32_t flags);       /* 0083cf70 PerformanceAnimator::sdtor (thiscall; RET 4) -> this */
void    rx_motocross_vehicle_detach(rx_addr v);                         /* 00462960 RavenVehicleMotocross::vf9 (thiscall) */
void    rx_animation_entity_vf9(rx_addr e);                             /* 00410fb0 AnimationEntity::vf9 (thiscall; a thunk to 00401ea0) */
void    rx_camera_entity_destroy(rx_addr e);                            /* 0063da30 (ESI = the camera entity) */
rx_addr rx_camera_entity_sdtor(rx_addr e, uint32_t flags);              /* 00646590 MXCameraEntity::sdtor (thiscall; RET 4) -> this */
rx_addr rx_third_person_camera_sdtor(rx_addr e, uint32_t flags);        /* 00649b70 MXThirdPersonCamera::sdtor (thiscall; RET 4) -> this */
void    rx_camera_entity_vf9(rx_addr e);                                /* 0063db40 MXCameraEntity::vf9 (thiscall) */
void    rx_clip_slots_clear(rx_addr base);                              /* 006b74d0 (EAX = the rider + 0xc940) */
void    rx_clip_slots_clear_tail(rx_addr base);                         /* 006b70b0 (ESI = the rider + 0xc940) */
void    rx_rider_pose_job_release(rx_addr rec);                         /* 006372a0 (ESI = the rider + 0x5b54c) */
void    rx_name_map_release(rx_addr map);                               /* 00618c80 (ESI = the map) */
void    rx_sound_instance_stop(rx_addr s);                              /* 00804e40 (ESI = the sound instance) */
void    rx_rider_sound_set_release(rx_addr set);                        /* 004245f0 (EAX = the rider + 0x5d0) */
void    rx_ik_chain_destroy(rx_addr chain);                             /* 006afc00 (ECX = the chain record) */
void    rx_ik_animator_destroy(rx_addr a);                              /* 006afcf0 (ECX = the animator) */
rx_addr rx_ik_animator_sdtor(rx_addr a, uint32_t flags);                /* 006afcd0 InverseKinematicsAnimator::sdtor (thiscall; RET 4) -> this */
void    rx_expression_animator_destroy(rx_addr a);                      /* 006aca10 (ECX = the animator) */
rx_addr rx_expression_animator_sdtor(rx_addr a, uint32_t flags);        /* 006ac9f0 ExpressionBoneAnimator::sdtor (thiscall; RET 4) -> this */
void    rx_character_buffers_release(rx_addr c);                        /* 0082e040 (ESI = the character data) */
void    rx_character_entity_destroy(rx_addr e);                         /* 0082d100 (ECX = the character entity) */
void    rx_rider_animation_destroy(rx_addr ra);                         /* 00618cd0 (ECX = the rider animation) */
rx_addr rx_rider_animation_sdtor(rx_addr ra, uint32_t flags);           /* 00618c30 RiderAnimation::sdtor (thiscall; RET 4) -> this */
#ifdef __cplusplus
}
#endif
