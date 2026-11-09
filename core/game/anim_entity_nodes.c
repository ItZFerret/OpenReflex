/* anim_entity_nodes.c -- the renderable sub-objects' (entity + 0x90) render-sync slots (the render sync's job, loop_engine.c
 * rx_render_sync_job, calls vf13 "changed", vf14 "sync", vf11 "matrix"):
 * 0082d1a0 AnimationEntity::vf14_o90 (ECX = the entity's +0x90 sub-object): the bind node set (+0x130) is filled from
 * the posed one (+0xf0): NodeSet_CopyFrom 007dd930 with ESI = this + 0x130 (destination), EDI = this + 0xf0 (source), after the original's
 * push esi / push edi. */
#include "anim.h"
#include "anim_extern.h"
#include "anim_entity_nodes.h"

void rx_animation_entity_reset_nodes(rx_addr self) {
    ANIM_CALL_FRAME(8);
    rx_nodeset_copy_from(self + 0x130, self + 0xf0);
    ANIM_CALL_FRAME_END(8);
}
RX_FUNCTION(0x0082d1a0, rx_animation_entity_reset_nodes)

/* 0055fd90 (the entities' vf14 at the +0x90 sub-object, shared by 16 classes: CollisionEntity, ...; fastcall ECX = the sub-object): the entity's matrix
 * (this - 0x60 = entity + 0x30, 64 bytes) copied to this + 0x30 (entity + 0xc0), as rep movsd with the saved esi / edi */
void rx_entity_copy_matrix_o90(rx_addr self) {
    ANIM_CALL_FRAME(8);
    for (uint32_t k = 0; k < 0x10; k++) SU32(self + 0x30, 4 * k, U32(self - 0x60, 4 * k));
    ANIM_CALL_FRAME_END(8);
}
RX_FUNCTION(0x0055fd90, rx_entity_copy_matrix_o90)

/* 0082d1c0 AnimationEntity::vf13_o90 / 006180e0 CharacterEntity::vf13_o90 (thiscall): the pose-changed flag (+0x124 / +0x1b4), read
 * and cleared */
uint32_t rx_animation_entity_take_changed(rx_addr self) { const uint32_t f = U32(self, 0x124); SU32(self, 0x124, 0); return f; }
RX_FUNCTION(0x0082d1c0, rx_animation_entity_take_changed)
uint32_t rx_character_entity_take_changed(rx_addr self) { const uint32_t f = U32(self, 0x1b4); SU32(self, 0x1b4, 0); return f; }
RX_FUNCTION(0x006180e0, rx_character_entity_take_changed)

/* 00617da0 AnimationEntity::vf11_o90 / 00618050 CharacterEntity::vf11_o90 (thiscall): the root node's matrix: the node record
 * (+0x130 / +0x1c0) -> its hierarchy's local matrices (+0x18) + 0x40 x the record's index byte (+0xb) */
rx_addr rx_animation_entity_root_matrix_o90(rx_addr self) { const rx_addr n = U32(self, 0x130); return U32(U32(n, 0), 0x18) + ((uint32_t)U8(n, 0xb) << 6); }
RX_FUNCTION(0x00617da0, rx_animation_entity_root_matrix_o90)
rx_addr rx_character_entity_root_matrix_o90(rx_addr self) { const rx_addr n = U32(self, 0x1c0); return U32(U32(n, 0), 0x18) + ((uint32_t)U8(n, 0xb) << 6); }
RX_FUNCTION(0x00618050, rx_character_entity_root_matrix_o90)
