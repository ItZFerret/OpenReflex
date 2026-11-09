// scene_fx_entities.cpp -- oracle adapters for the FX construction unit (core/game/scene_fx_entities.c, fx_worldparticles.c,
// build_particle.c). Group rx_fxconstruct. The product-only UI ports (StatusBoard vf5 / vf19) are not registered here: the oracle
// runs the originals.
#include "oracle.h"
#include "f80/f80.h"
extern "C" {
#include "../game/scene_fx_entities.h"
#include "../game/build_particle.h"
}
using namespace reflex_port;
using rx_oracle::f32;

/* ---- the constructors ---- */
RX_PORT(0x00694110, ParticleEmitterConstruct, "rx_fxconstruct", OUT_EAX) { m.r[0] = rx_particle_emitter_construct(m.r[0]); return m.ret(0); }
RX_PORT(0x00696480, PhysicsObjectSplashParticles, "rx_fxconstruct", OUT_EAX) { m.r[0] = rx_physics_splash_particles_construct(m.r[6]); return m.ret(0); }
RX_PORT(0x006963b0, PhysicsObjectDustParticles, "rx_fxconstruct", OUT_EAX) { m.r[0] = rx_physics_dust_particles_construct(m.r[6]); return m.ret(0); }

/* ---- PlaceableParticleEffect and its variants ---- */
// RX_PORT(0x00696670, PlaceableParticleEffect_vf5, "rx_fxconstruct", 0) { rx_placeable_effect_vf5(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00696810, PlaceableParticleEffect_vf7, "rx_fxconstruct", 0) { rx_placeable_effect_vf7(m.r[1]); return m.ret(0); }
RX_PORT(0x00696870, PlaceableParticleEffect_vf9, "rx_fxconstruct", 0) { rx_placeable_effect_vf9(m.r[1]); return m.ret(0); }
// RX_PORT(0x006921c0, FireworksParticleEffect_vf5, "rx_fxconstruct", 0) { rx_fireworks_effect_vf5(m.r[1], m.arg(0)); return m.ret(4); }
// RX_PORT(0x00692440, TriggeredPlaceableParticleEffect_vf5, "rx_fxconstruct", 0) { rx_triggered_effect_vf5(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00692240, FireworksParticleEffect_vf7, "rx_fxconstruct", 0) { rx_fireworks_effect_vf7(m.r[1]); return m.ret(0); }
RX_PORT(0x00692450, TriggeredPlaceableParticleEffect_vf7, "rx_fxconstruct", 0) { rx_triggered_effect_vf7(m.r[1]); return m.ret(0); }

/* ---- ParticleEntity ---- */
// RX_PORT(0x00695de0, ParticleEntity_Load, "rx_fxconstruct", 0) { rx_particle_entity_vf5(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00695da0, ParticleEntity_vf7, "rx_fxconstruct", 0) { rx_particle_entity_vf7(m.r[1]); return m.ret(0); }
RX_PORT(0x00696040, ParticleEntity_vf9, "rx_fxconstruct", 0) { rx_particle_entity_vf9(m.r[1]); return m.ret(0); }

/* ---- AudioVolumeMixEntity ---- */
// RX_PORT(0x0067d4d0, AudioVolumeMixEntity_vf5, "rx_fxconstruct", 0) { rx_audio_mix_vf5(m.r[1], m.arg(0)); return m.ret(4); }
// RX_PORT(0x0067d7f0, AudioVolumeMixEntity_vf19, "rx_fxconstruct", 0) { rx_audio_mix_tick(m.r[1], f32(m.arg(0))); return m.ret(4); }
// RX_PORT(0x0067d7d0, AudioVolumeMixEntity_vf2, "rx_fxconstruct", 0) { rx_audio_mix_vf2(m.r[1]); return m.ret(0); }

/* ---- ThemeStatusBoard / viewport bounds ---- */
RX_PORT(0x004167d0, StatusBoardEntity_vf6_o90, "rx_fxconstruct", 0) { rx_status_board_bounds(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x00416430, StatusBoardEntity_vf9, "rx_fxconstruct", 0) { rx_status_board_vf9(m.r[1]); return m.ret(0); }

/* ---- the world's vehicle particle effect sets ---- */
RX_PORT(0x006977f0, VehicleEffectReset, "rx_fxconstruct", 0) { rx_vehicle_effect_reset(m.r[7]); return m.ret(0); }
RX_PORT(0x006976a0, VehicleEffectBindRecord, "rx_fxconstruct", 0) { rx_vehicle_effect_bind_record(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x0069b090, ParticleControllerRelease, "rx_fxconstruct", 0) { rx_particle_controller_release(m.r[1]); return m.ret(0); }
RX_PORT(0x00697870, VehicleEffectsRebind, "rx_fxconstruct", 0) { rx_vehicle_effects_rebind(m.r[6]); return m.ret(0); }
RX_PORT(0x0069ad60, YRingParticleEmitter_vf0, "rx_fxconstruct", 0) {
    rx_yring_emitter_vf0(m.r[1], f32(m.arg(0)), m.arg(1), m.arg(2), m.arg(3), m.arg(4)); return m.ret(0x14);
}
