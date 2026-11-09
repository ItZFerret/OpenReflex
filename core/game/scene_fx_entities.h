/* scene_fx_entities.h -- the FX construction unit's scene classes and world particle effect objects: build_scene.c's class
 * registrations (scene_fx_entities.c) and the world's particle effect rebind (fx_worldparticles.c). notes/fx.md. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

void     rx_scene_fx_register(void);                                        /* build_scene.c: the factory's FX classes */
void     rx_scene_viewports_create(rx_addr game, uint32_t views);           /* 00757660's viewport list (scene_viewports.c) */

/* the scene class virtuals (registered; the StatusBoard's vf5 / vf19 are product-only) */
uint32_t rx_placeable_effect_vf5(rx_addr e, uint32_t reader);
uint32_t rx_placeable_effect_vf7(rx_addr e);
uint32_t rx_placeable_effect_vf9(rx_addr e);
uint32_t rx_fireworks_effect_vf5(rx_addr e, uint32_t reader);
uint32_t rx_triggered_effect_vf5(rx_addr e, uint32_t reader);
uint32_t rx_fireworks_effect_vf7(rx_addr e);
uint32_t rx_triggered_effect_vf7(rx_addr e);
uint32_t rx_particle_entity_vf5(rx_addr e, uint32_t reader);
uint32_t rx_particle_entity_vf7(rx_addr e);
uint32_t rx_particle_entity_vf9(rx_addr e);
uint32_t rx_audio_mix_vf5(rx_addr e, uint32_t reader);
uint32_t rx_audio_mix_tick(rx_addr e, float dt);
uint32_t rx_audio_mix_vf2(rx_addr e);
void     rx_status_board_bounds(rx_addr self, rx_addr center, rx_addr ext, rx_addr radius);
uint32_t rx_status_board_vf9(rx_addr e);

/* the world's particle effect sets (0xc11e90 + k * 0x17c30, MXWorld + 0x21dc4 / +0x21dc8 / +0x21dd4) */
rx_addr  rx_physics_splash_particles_construct(rx_addr block);              /* 00696480 (ESI = the block; RET 4) */
rx_addr  rx_physics_dust_particles_construct(rx_addr block);                /* 006963b0 (ESI = the block; RET 4) */
rx_addr  rx_particle_emitter_construct(rx_addr emitter);                    /* 00694110 (EAX = the emitter) */
void     rx_vehicle_effects_rebind(rx_addr set);                            /* 00697870 (ESI = the effect set) */
void     rx_vehicle_effects_slot_bind(rx_addr veh);                         /* 00691cc0 (EDI = the vehicle): the first free set */
void     rx_vehicle_effect_reset(rx_addr rec);                              /* 006977f0 (EDI = the 0x3f0 record) */
void     rx_vehicle_effect_bind_record(rx_addr rec, rx_addr set, rx_addr desc);   /* 006976a0 (RET 0xc) */
void     rx_particle_controller_release(rx_addr self);                      /* 0069b090 (ECX = the controller) */
void     rx_yring_emitter_vf0(rx_addr S, float dt, rx_addr mgr, rx_addr mat, uint32_t interp, uint32_t index);  /* 0069ad60 (RET 0x14) */

#ifdef __cplusplus
}
#endif
