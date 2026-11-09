/* loop_entity.h -- ambient entity updates (loop_entity.c). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
rx_addr  rx_character_ticker_construct(void);                                   /* 00839890 (cdecl) -> the ticker 0x00d9d3f0 */
uint32_t rx_character_ticker_run(rx_addr ticker);                         /* 00839710: one wake of the character ticker thread (1: it ended) */
rx_addr rx_jobpool_instance(void);                                       /* 006925a0 JobPool_Instance */
void    rx_anim_entity_single_loop_update(rx_addr e, float dt);          /* 006ba250 AnimEntitySingleAnimLoop::vf19 */
void    rx_anim_entity_spline_loop_update(rx_addr e, float dt);          /* 006bb340 AnimEntitySplineAnimLoop::vf19 */
void    rx_anim_entity_spline_particles_update(rx_addr e, float dt);     /* 006bc040 AnimEntitySplineAnimLoopWithParticles::vf19 */
void    rx_scenario_anim_entity_update(rx_addr e, float dt);             /* 0074ab50 ScenarioAnimEntity::vf19 */
void    rx_crew_chief_update(rx_addr e, float dt);                       /* 006bd6e0 CrewChief::vf19 */
void    rx_instanced_animation_update(rx_addr e, float dt);              /* 004137b0 InstancedAnimationEntity::vf19 */
void    rx_score_tower_flag_update(rx_addr e, float dt);                 /* 006c1a00 ScoreTowerFlagHolder::vf19 */
rx_addr rx_entity_transform(rx_addr e);                                  /* 0088fa20 Entity::vf14 */
void    rx_billboard_crowd_update(rx_addr e, float dt);                  /* 006c6650 BillboardCrowdEntity::vf19 */
void    rx_land_animal_update(rx_addr e, float dt);                      /* 006d06a0 LandAnimalProximity::vf19 */
void    rx_water_entity_update(rx_addr e, float dt);                     /* 007e4170 WaterEntity::vf19 */
void    rx_forest_update(rx_addr e, float dt);                           /* 00662dd0 ForestSystem::vf19 */
void    rx_terrain_entity_update(rx_addr e, float dt);                   /* 007c90a0 HFTerrainEntity::vf19 */
rx_addr rx_terrain_entity_terrain(rx_addr e);                            /* 007c9110 HFTerrainEntity::vf25 */
void rx_crew_chief_job(rx_addr job);                                    /* 006bd6a0 (stdcall job; RET 0) */

#ifdef __cplusplus
}
#endif
