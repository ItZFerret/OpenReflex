// anim_entity_nodes.cpp -- oracle adapter for core/game/anim_entity_nodes.c.
#include "anim_oracle.h"
extern "C" {
#include "../game/anim_entity_nodes.h"
}
using namespace reflex_port;

// 0082d1a0 AnimationEntity::vf14_o90 (thiscall). Void.
ANIM_PORT(0x0082d1a0, AnimationEntity_vf14_o90, "rx_anim", 0) { rx_animation_entity_reset_nodes(m.r[1]); return m.ret(0); }
// 0055fd90 vf14 of the +0x90 sub-object (fastcall ECX; rep movsd leaves ECX = 0). Void.
ANIM_PORT(0x0055fd90, Entity_vf14_o90_copy, "rx_anim", 0) { rx_entity_copy_matrix_o90(m.r[1]); m.r[1] = 0; return m.ret(0); }
// the render sync's vf13 / vf11 of the +0x90 sub-object (thiscall): EAX = the flag / the matrix; ECX = the node record (vf11) as left
ANIM_PORT(0x0082d1c0, AnimationEntity_vf13_o90, "rx_anim", OUT_EAX) { m.r[0] = rx_animation_entity_take_changed(m.r[1]); return m.ret(0); }
ANIM_PORT(0x006180e0, CharacterEntity_vf13_o90, "rx_anim", OUT_EAX) { m.r[0] = rx_character_entity_take_changed(m.r[1]); return m.ret(0); }
ANIM_PORT(0x00617da0, AnimationEntity_vf11_o90, "rx_anim", OUT_EAX | OUT_ECX) {
    const uint32_t s = m.r[1]; m.r[0] = rx_animation_entity_root_matrix_o90(s); m.r[1] = U32(U32(s, 0x130), 0); return m.ret(0);
}
ANIM_PORT(0x00618050, CharacterEntity_vf11_o90, "rx_anim", OUT_EAX | OUT_ECX) {
    const uint32_t s = m.r[1]; m.r[0] = rx_character_entity_root_matrix_o90(s); m.r[1] = U32(U32(s, 0x1c0), 0); return m.ret(0);
}
