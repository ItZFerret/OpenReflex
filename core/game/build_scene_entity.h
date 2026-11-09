/* build_scene_entity.h -- the scene Entity base description and the ModelInstance's model binding (build_scene_entity.c). */
#pragma once
#include "rx_game.h"

void rx_entity_describe(rx_addr entity, rx_addr desc);                                          /* 0080ddf0 (stdcall; RET 8) */
void rx_model_instance_set_model(rx_addr inst, rx_addr handle, uint32_t flags, rx_addr overrides);   /* 007f1810 (EAX = handle, ECX = flags; s0 inst, s1 overrides; RET 8) */
void rx_model_instance_release(rx_addr inst);                                                       /* 007f1aa0 (ESI = the instance) */
void rx_camera_entity_add_to_scene(rx_addr cam, rx_addr scene);                              /* 0063dae0 (EAX = camera entity, ESI = scene) */
float    rx_instanced_anim_light_range(rx_addr self);                                                      /* 004045c0 (fastcall ECX; st0 = [ECX + 0x9d4]) */
void     rx_instanced_anim_light_shape(rx_addr self, rx_addr a, rx_addr b, rx_addr c);                    /* 004045d0 (thiscall; RET 0xc) */
uint32_t rx_instanced_anim_vf12_o90(rx_addr self);                                                        /* 00404620 (a tail jump) */
uint32_t rx_instanced_anim_vf11_o90(rx_addr self);                                                        /* 00404590 (a tail jump) */
rx_addr  rx_instanced_anim_vf14(void);                                                                 /* 004045b0 -> 0x009cb510 */
void     rx_character_entity_vf6_o90(rx_addr self, rx_addr ctr, rx_addr ext, rx_addr rad);                /* 00617f60 (thiscall; RET 0xc) */
