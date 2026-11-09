// anim_entity_dtors.cpp -- oracle adapters for core/game/anim_entity_dtors.c (group rx_anim): the scene entity classes' destructors and
// release virtuals. The reference worlds never remove an area's entities; core/oracle/dtor_probe.cpp drives them (vf3 / vf6 / vf0 of the live
// Free Ride / Waypoint / race activity, whose area subscene holds the entities).
#include "anim_oracle.h"
extern "C" {
#include "../game/anim_entity_dtors.h"
}
using namespace reflex_port;

// thiscall flags; RET 4 -> this
ANIM_PORT(0x00406ff0, Entity_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_entity_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x00401fd0, ModelEntity_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_model_entity_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x00410bc0, ScrollingTextureEntity_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_scrolling_texture_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x0040e550, MXTargetPracticeCollectable_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_target_practice_collectable_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x00407110, TDFEntity_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_tdf_entity_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x00407130, MusicTrackListEntity_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_music_track_list_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x00409970, MXTrigger_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_trigger_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x0063d560, MXCameraPath_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_camera_path_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x00648dd0, MXReplayTowerCamera_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_replay_tower_camera_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x00692190, FireworksParticleEffect_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_fireworks_effect_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x00696580, PlaceableParticleEffect_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_placeable_effect_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x00692410, TriggeredPlaceableParticleEffect_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_triggered_effect_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x006ba5f0, AnimEntitySplineAnimLoop_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_spline_loop_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x006bd520, CrewChief_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_crew_chief_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x00819000, CollisionEntity_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_collision_entity_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
// ECX = the entity; plain ret
ANIM_PORT(0x0040ad60, ShadowMapEntity_Vf9, "rx_anim", 0) { rx_shadow_map_vf9(m.r[1]); return m.ret(0); }
ANIM_PORT(0x00793930, Entity_Vf9, "rx_anim", 0) { rx_entity_vf9(m.r[1]); return m.ret(0); }
ANIM_PORT(0x006bbff0, AnimEntitySplineAnimLoopWithParticles_Vf9, "rx_anim", 0) { rx_spline_loop_particles_vf9(m.r[1]); return m.ret(0); }
ANIM_PORT(0x00410c10, ScrollingTextureEntity_Destroy, "rx_anim", 0) { rx_scrolling_texture_destroy(m.r[1]); return m.ret(0); }
ANIM_PORT(0x006965a0, ParticleEffect_Destroy, "rx_anim", 0) { rx_particle_effect_destroy(m.r[1]); return m.ret(0); }
ANIM_PORT(0x00819100, CollisionEntity_Vf9, "rx_anim", 0) { rx_collision_entity_vf9(m.r[1]); return m.ret(0); }
// ESI = the entity
ANIM_PORT(0x004072c0, TDFEntity_Destroy, "rx_anim", 0) { rx_tdf_entity_destroy(m.r[6]); return m.ret(0); }
ANIM_PORT(0x0063e6a0, MXCameraPath_Destroy, "rx_anim", 0) { rx_camera_path_destroy(m.r[6]); return m.ret(0); }
// EAX = the entity
ANIM_PORT(0x00648df0, MXReplayTowerCamera_Destroy, "rx_anim", 0) { rx_replay_tower_camera_destroy(m.r[0]); return m.ret(0); }
ANIM_PORT(0x006bd570, CrewChief_Destroy, "rx_anim", 0) { rx_crew_chief_destroy(m.r[0]); return m.ret(0); }
// EDI = the entity
ANIM_PORT(0x00819050, CollisionEntity_Destroy, "rx_anim", 0) { rx_collision_entity_destroy(m.r[7]); return m.ret(0); }
