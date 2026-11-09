// anim_character.cpp -- oracle adapters for core/game/anim_character.c.
#include "anim_oracle.h"
extern "C" {
#include "../game/anim.h"
}
using namespace reflex_port;
static float xmm_f(reflex_vm::Machine& m, int i) { return rx_oracle::f32(uint32_t(m.xmm[i].lo)); }

// 00835430 Character_SampleKeyframes (stack C, xmm1 = dt; RET 4). Void.
ANIM_PORT(0x00835430, Character_SampleKeyframes, "rx_anim", 0) {
    rx_character_sample_keyframes(m.arg(0), xmm_f(m, 1));
    return m.ret(4);
}
// 00838ba0 Character_TransitionBlend (stdcall C, w; RET 8). Void.
ANIM_PORT(0x00838ba0, Character_TransitionBlend, "rx_anim", 0) {
    rx_character_transition_blend(m.arg(0), rx_oracle::f32(m.arg(1)));
    return m.ret(8);
}
// 00835170 (ESI = C): a random start time in the current motion.
ANIM_PORT(0x00835170, Character_RandomMotionTime, "rx_anim", 0) { rx_character_random_motion_time(m.r[6]); return m.ret(0); }
// 00839f40 FloatTrack_Sample (ECX = track, EDI = out, stack time; RET 4). Leaves ECX = the previous key (interpolated) or the keys.
ANIM_PORT(0x00839f40, FloatTrack_Sample, "rx_anim", OUT_ECX) {
    const uint32_t track = m.r[1], esp = m.r[4];
    rx_float_track_sample(track, m.r[7], rx_oracle::f32(m.arg(0)));
    m.r[1] = (U32(track, 4) != 0 && int32_t(U32(track, 8)) > 1) ? U32(esp - 8, 0) : U32(track, 0x10);
    return m.ret(4);
}
// 008435f0 FloatKeys_FindSegment (EAX = keys, EDX = count, xmm0 = time; stack prev / next / fraction out; RET 0xc). EAX = inside.
ANIM_PORT(0x008435f0, FloatKeys_FindSegment, "rx_anim", OUT_EAX) {
    m.r[0] = rx_floatkeys_find_segment(m.r[0], m.r[2], xmm_f(m, 0), m.arg(0), m.arg(1), m.arg(2));
    return m.ret(0xc);
}
// 00835bf0 Character_SampleMotionAt (ECX = slot, stack C, phase; RET 8). Void.
ANIM_PORT(0x00835bf0, Character_SampleMotionAt, "rx_anim", 0) {
    rx_character_sample_motion_at(m.r[1], m.arg(0), rx_oracle::f32(m.arg(1)));
    return m.ret(8);
}
// 008363b0 Character_SampleBlend3 (ECX = slot C, stack C, slot A, slot B, phase A, B, C, w; RET 0x1c). Void.
ANIM_PORT(0x008363b0, Character_SampleBlend3, "rx_anim", 0) {
    rx_character_sample_blend3(m.r[1], m.arg(0), m.arg(1), m.arg(2), rx_oracle::f32(m.arg(3)), rx_oracle::f32(m.arg(4)),
                               rx_oracle::f32(m.arg(5)), rx_oracle::f32(m.arg(6)));
    return m.ret(0x1c);
}
// 00837590 Character_TransitionSample (stdcall C, w; RET 8). Void.
ANIM_PORT(0x00837590, Character_TransitionSample, "rx_anim", 0) {
    rx_character_transition_sample(m.arg(0), rx_oracle::f32(m.arg(1)));
    return m.ret(8);
}
// 00834eb0 CharacterAnim_SetAnimation (EAX = loops; stdcall C, handle; RET 8). EAX = the slot.
ANIM_PORT(0x00834eb0, CharacterAnim_SetAnimation, "rx_anim", OUT_EAX) {
    m.r[0] = rx_character_anim_set_animation(m.arg(0), m.arg(1), m.r[0]);
    return m.ret(8);
}
// 00834d80 CharacterAnim_Release (ESI = C). Void.
ANIM_PORT(0x00834d80, CharacterAnim_Release, "rx_anim", 0) { rx_character_anim_release(m.r[6]); return m.ret(0); }
// 00835000 CharacterAnim_FindSlot (EDI = handle, EAX = C). EAX = the slot or 0.
ANIM_PORT(0x00835000, CharacterAnim_FindSlot, "rx_anim", OUT_EAX) {
    m.r[0] = rx_character_anim_find_slot(m.r[7], m.r[0]);
    return m.ret(0);
}
// 00837510 CharacterAnim_Play (ESI = C, xmm1 = blend time, stack slot; RET 4). Void.
ANIM_PORT(0x00837510, CharacterAnim_Play, "rx_anim", 0) {
    rx_character_anim_play(m.r[6], m.arg(0), xmm_f(m, 1));
    return m.ret(4);
}
// 00839bb0 CharacterEntity_PlaySlot (EAX = entity, ECX = slot). Void.
ANIM_PORT(0x00839bb0, CharacterEntity_PlaySlot, "rx_anim", 0) {
    rx_character_entity_play_slot(m.r[0], m.r[1]);
    return m.ret(0);
}
// 00839be0 CharacterEntity::vf19 (thiscall dt; RET 4). Void.
ANIM_PORT(0x00839be0, CharacterEntity_vf19, "rx_anim", 0) {
    rx_character_entity_animate(m.r[1], rx_oracle::f32(m.arg(0)));
    return m.ret(4);
}
// 00839c30 CharacterEntity_SetLoopAnimation (ESI = entity, EAX = handle, stack loops; RET 4). Void.
ANIM_PORT(0x00839c30, CharacterEntity_SetLoopAnimation, "rx_anim", 0) {
    rx_character_entity_set_loop_animation(m.r[6], m.r[0], m.arg(0));
    return m.ret(4);
}
// 00839b00 CharacterEntity::vf9 (thiscall). Void.
ANIM_PORT(0x00839b00, CharacterEntity_vf9, "rx_anim", 0) { rx_character_entity_release(m.r[1]); return m.ret(0); }
// 0083d480 (thiscall, RET 0xc): empty.
ANIM_PORT(0x0083d480, Animator_Nop3, "rx_anim", 0) { rx_animator_nop3(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }

// 00843350 (EAX = map, EDI = &name cell, stack the byte out; RET 4) / 008433b0 (EDI = map, stack &name cell, &byte; RET 8): the
// bone name -> bone index cache.
ANIM_PORT(0x00843350, BoneCache_Find, "rx_anim", OUT_EAX) { m.r[0] = rx_bone_cache_find(m.r[0], m.r[7], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x008433b0, BoneCache_Insert, "rx_anim", 0) { rx_bone_cache_insert(m.r[7], m.arg(0), m.arg(1)); return m.ret(8); }
// 0082af60 ActionContext::vf0 (scalar deleting destructor; ECX = the object, stack the delete flag; RET 4)
ANIM_PORT(0x0082af60, ActionContext_Dtor, "rx_anim", OUT_EAX) { m.r[0] = rx_action_context_dtor(m.r[1], m.arg(0)); return m.ret(4); }

// 008351c0 CharacterAnim_CapturePose (stdcall C; RET 4). Void.
ANIM_PORT(0x008351c0, CharacterAnim_CapturePose, "rx_anim", 0) { rx_character_anim_capture_pose(m.arg(0)); return m.ret(4); }
