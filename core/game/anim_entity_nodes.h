/* anim_entity_nodes.h -- the renderable sub-objects' render-sync slots (anim_entity_nodes.c). */
#pragma once
#include "rx_game.h"
void rx_animation_entity_reset_nodes(rx_addr self);   /* 0082d1a0 AnimationEntity::vf14_o90 (thiscall) */
void rx_entity_copy_matrix_o90(rx_addr self);       /* 0055fd90 (fastcall) */
uint32_t rx_animation_entity_take_changed(rx_addr self);     /* 0082d1c0 AnimationEntity::vf13_o90 (thiscall) */
uint32_t rx_character_entity_take_changed(rx_addr self);     /* 006180e0 CharacterEntity::vf13_o90 (thiscall) */
rx_addr  rx_animation_entity_root_matrix_o90(rx_addr self);  /* 00617da0 AnimationEntity::vf11_o90 (thiscall) */
rx_addr  rx_character_entity_root_matrix_o90(rx_addr self);  /* 00618050 CharacterEntity::vf11_o90 (thiscall) */
