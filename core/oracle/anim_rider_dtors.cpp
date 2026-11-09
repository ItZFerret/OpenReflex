// anim_rider_dtors.cpp -- oracle adapters for core/game/anim_rider_dtors.c (group rx_anim): the vehicle / rider / scenario entity
// destructors. No reference world deletes these objects; the probe core/oracle/dtor_probe.cpp drives them.
#include "anim_oracle.h"
extern "C" {
#include "../game/anim_rider_dtors.h"
}
using namespace reflex_port;

// thiscall flags; RET 4 -> this
ANIM_PORT(0x0045d3a0, RavenVehicle_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_raven_vehicle_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x00462560, Motocross_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_motocross_vehicle_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x006b7720, ScenarioAnimEntity_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_scenario_anim_entity_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x00618c30, RiderAnimation_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_rider_animation_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
// 00618cd0 (ECX = the rider animation)
ANIM_PORT(0x00618cd0, RiderAnimation_Destroy, "rx_anim", 0) { rx_rider_animation_destroy(m.r[1]); return m.ret(0); }
// 006b74d0 (EAX = base) / 006b70b0 (ESI = base) / 006372a0 (ESI = record) / 00618c80 (ESI = map)
ANIM_PORT(0x006b74d0, RiderClipSlots_Clear, "rx_anim", 0) { rx_clip_slots_clear(m.r[0]); return m.ret(0); }
ANIM_PORT(0x006b70b0, RiderClipSlots_ClearTail, "rx_anim", 0) { rx_clip_slots_clear_tail(m.r[6]); return m.ret(0); }
ANIM_PORT(0x006372a0, RiderPoseJob_Release, "rx_anim", 0) { rx_rider_pose_job_release(m.r[6]); return m.ret(0); }
ANIM_PORT(0x00618c80, RiderNameMap_Release, "rx_anim", 0) { rx_name_map_release(m.r[6]); return m.ret(0); }
// 00804e40 (ESI = the sound instance) / 004245f0 (EAX = the emitter set)
ANIM_PORT(0x00804e40, SoundInstance_Stop, "rx_anim", 0) { rx_sound_instance_stop(m.r[6]); return m.ret(0); }
ANIM_PORT(0x004245f0, RiderSoundSet_Release, "rx_anim", 0) { rx_rider_sound_set_release(m.r[0]); return m.ret(0); }
// 0082e040 (ESI = the character data) / 0082d100 (ECX = the character entity)
ANIM_PORT(0x0082e040, CharacterBuffers_Release, "rx_anim", 0) { rx_character_buffers_release(m.r[6]); return m.ret(0); }
ANIM_PORT(0x0082d100, CharacterEntity_Destroy, "rx_anim", 0) { rx_character_entity_destroy(m.r[1]); return m.ret(0); }
// 006afc00 (ECX = the chain) / 006afcf0 (ECX = the animator) / 006aca10 (ECX = the animator)
ANIM_PORT(0x006afc00, IkChain_Destroy, "rx_anim", 0) { rx_ik_chain_destroy(m.r[1]); return m.ret(0); }
ANIM_PORT(0x006afcf0, IkAnimator_Destroy, "rx_anim", 0) { rx_ik_animator_destroy(m.r[1]); return m.ret(0); }
ANIM_PORT(0x006aca10, ExpressionAnimator_Destroy, "rx_anim", 0) { rx_expression_animator_destroy(m.r[1]); return m.ret(0); }
// 006afcd0 / 006ac9f0 (thiscall flags; RET 4) -> this
ANIM_PORT(0x006afcd0, IkAnimator_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_ik_animator_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x006ac9f0, ExpressionAnimator_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_expression_animator_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
// 00462960 / 00410fb0 / 0063db40 (thiscall); 0063da30 (ESI = the camera entity)
ANIM_PORT(0x00462960, Motocross_Vf9, "rx_anim", 0) { rx_motocross_vehicle_detach(m.r[1]); return m.ret(0); }
ANIM_PORT(0x00410fb0, AnimationEntity_Vf9, "rx_anim", 0) { rx_animation_entity_vf9(m.r[1]); return m.ret(0); }
ANIM_PORT(0x0063db40, CameraEntity_Vf9, "rx_anim", 0) { rx_camera_entity_vf9(m.r[1]); return m.ret(0); }
ANIM_PORT(0x0063da30, CameraEntity_Destroy, "rx_anim", 0) { rx_camera_entity_destroy(m.r[6]); return m.ret(0); }
// 00646590 / 00649b70 (thiscall flags; RET 4) -> this
ANIM_PORT(0x00646590, CameraEntity_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_camera_entity_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x00649b70, ThirdPersonCamera_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_third_person_camera_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
// 0043c040 (ECX = the simulation); 0043b450 / 006b5820 (thiscall flags; RET 4) -> this
ANIM_PORT(0x0043c040, MotocrossSim_Destruct, "rx_anim", 0) { rx_motocross_sim_destruct(m.r[1]); return m.ret(0); }
ANIM_PORT(0x0043b450, MotocrossSim_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_motocross_sim_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x006b5820, ProceduralAnimator_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_procedural_animator_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x006afad0, FeatherstoneAnimator_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_featherstone_animator_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x0083c6e0, SwitchAnimator_Destroy, "rx_anim", 0) { rx_switch_animator_destroy(m.r[1]); return m.ret(0); }
ANIM_PORT(0x0083c6c0, SwitchAnimator_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_switch_animator_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x00833020, BlendAnimator_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_blend_animator_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x00834600, AdditiveAnimator_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_additive_animator_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x0083b3a0, MotionAnimator_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_motion_animator_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x0083d360, PerformanceAnimator_Destroy, "rx_anim", 0) { rx_performance_animator_destroy(m.r[1]); return m.ret(0); }
ANIM_PORT(0x0083cf70, PerformanceAnimator_Sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_performance_animator_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
