/* anim_entity_dtors.h -- the scene entity classes released by the activity area's teardown (Activity::vf6 -> rx_scene_subscene_release: each
 * entity of the area leaves the scene (007d3040), vf9 (released) and vf0 (1) (the scalar deleting destructor)). anim_entity_dtors.c. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
rx_addr rx_entity_sdtor(rx_addr e, uint32_t flags);                  /* 00406ff0 Entity::vf0 (thiscall; RET 4) -> this: AnchorEntity, GeneralStorage, ... */
rx_addr rx_model_entity_sdtor(rx_addr e, uint32_t flags);            /* 00401fd0 ModelEntity::sdtor (the locked free) */
void    rx_scrolling_texture_destroy(rx_addr e);                     /* 00410c10 (ECX = the entity) */
rx_addr rx_scrolling_texture_sdtor(rx_addr e, uint32_t flags);       /* 00410bc0 ScrollingTextureEntity::sdtor */
rx_addr rx_target_practice_collectable_sdtor(rx_addr e, uint32_t flags);   /* 0040e550 MXTargetPracticeCollectable::sdtor */
void    rx_entity_vf9(rx_addr e);                                    /* 00793930 Entity::vf9 (thiscall) */
void    rx_tdf_entity_destroy(rx_addr e);                            /* 004072c0 (ESI = the entity) */
rx_addr rx_tdf_entity_sdtor(rx_addr e, uint32_t flags);              /* 00407110 TDFEntity::sdtor */
rx_addr rx_music_track_list_sdtor(rx_addr e, uint32_t flags);        /* 00407130 MusicTrackListEntity::sdtor */
rx_addr rx_trigger_sdtor(rx_addr e, uint32_t flags);                 /* 00409970 MXTrigger::sdtor (the destroy 00409990 is waypoint_gate.c) */
void    rx_camera_path_destroy(rx_addr e);                           /* 0063e6a0 (ESI = the entity) */
rx_addr rx_camera_path_sdtor(rx_addr e, uint32_t flags);             /* 0063d560 MXCameraPath::sdtor */
void    rx_replay_tower_camera_destroy(rx_addr e);                   /* 00648df0 (EAX = the entity) */
rx_addr rx_replay_tower_camera_sdtor(rx_addr e, uint32_t flags);     /* 00648dd0 MXReplayTowerCamera::sdtor */
void    rx_particle_effect_destroy(rx_addr e);                       /* 006965a0 (ECX = the entity) */
rx_addr rx_fireworks_effect_sdtor(rx_addr e, uint32_t flags);        /* 00692190 FireworksParticleEffect::sdtor */
rx_addr rx_placeable_effect_sdtor(rx_addr e, uint32_t flags);        /* 00696580 PlaceableParticleEffect::sdtor */
rx_addr rx_triggered_effect_sdtor(rx_addr e, uint32_t flags);        /* 00692410 TriggeredPlaceableParticleEffect::sdtor */
void    rx_shadow_map_vf9(rx_addr e);                                /* 0040ad60 ShadowMapEntity::vf9 */
rx_addr rx_spline_loop_sdtor(rx_addr e, uint32_t flags);             /* 006ba5f0 AnimEntitySplineAnimLoop::sdtor */
void    rx_spline_loop_particles_vf9(rx_addr e);                     /* 006bbff0 AnimEntitySplineAnimLoopWithParticles::vf9 */
void    rx_crew_chief_destroy(rx_addr e);                            /* 006bd570 (EAX = the entity) */
rx_addr rx_crew_chief_sdtor(rx_addr e, uint32_t flags);              /* 006bd520 CrewChief::sdtor */
void    rx_collision_entity_destroy(rx_addr e);                      /* 00819050 (EDI = the entity) */
rx_addr rx_collision_entity_sdtor(rx_addr e, uint32_t flags);        /* 00819000 CollisionEntity::sdtor */
void    rx_collision_entity_vf9(rx_addr e);                          /* 00819100 CollisionEntity::vf9 */
#ifdef __cplusplus
}
#endif
