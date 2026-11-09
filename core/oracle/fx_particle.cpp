// fx_particle.cpp -- oracle adapters for core/game/fx_particle.c (group "rx_fx").
#include "fx_oracle.h"
extern "C" {
#include "../game/fx.h"
}
using namespace reflex_port;
using rx_oracle::f32;
using rx_oracle::bits;

FX_PORT(0x00692b50, Particle_Copy, 0) { rx_fx_particle_copy(m.r[1], m.r[0]); return m.ret(0); }
FX_PORT(0x006925f0, Particle_UpdateJob, 0) { rx_fx_particle_update_job(m.arg(0)); return m.ret(0); }
FX_PORT(0x00698510, ParticleManager2D_Update, 0) { rx_fx_particle_manager_update(m.r[1], f32(m.arg(0))); return m.ret(4); }
FX_PORT(0x00698780, PlatformParticleManager2D_SpawnParticle, 0) {
    rx_fx_particle_spawn(m.r[1], m.arg(0), m.arg(1)); return m.ret(8);
}
FX_PORT(0x00691ba0, PlatformParticleManager2D_vf7, OUT_EAX) { m.r[0] = rx_fx_particle_manager_vf7(m.r[1]); return m.ret(0); }
FX_PORT(0x00698740, PlatformParticleManager2D_vf5, 0) { m.r[0] = rx_fx_particle_manager_vf5(m.r[1], m.arg(0)); m.r[1] = 0; return m.ret(4); }
FX_PORT(0x00698760, PlatformParticleManager2D_vf4, OUT_EAX) {
    m.r[2] = m.arg(0); m.r[0] = rx_fx_particle_manager_vf4(m.r[1], m.arg(0), m.arg(1), m.arg(2)); m.r[1] = 0; return m.ret(0xc);
}
FX_PORT(0x00691d80, ParticleEmitter_vf4, OUT_EAX) { m.r[0] = rx_fx_emitter_vf4(m.r[1]); return m.ret(0); }
FX_PORT(0x00691d90, ParticleEmitter_vf5, 0) { const uint32_t a = m.arg(0); rx_fx_emitter_vf5(m.r[1], a); m.r[0] = a; return m.ret(4); }
FX_PORT(0x006960c0, ParticleEntity_UpdateManagers, 0) { rx_fx_particle_entity_update_managers(m.r[1], f32(m.arg(0))); return m.ret(4); }
FX_PORT(0x00696100, ParticleEntity_Tick, 0) { rx_fx_particle_entity_tick(m.r[1], f32(m.arg(0))); return m.ret(4); }

// fx_emit.c
FX_PORT(0x006941a0, MatrixLerp_006941a0, 0) { rx_fx_matrix_lerp(m.arg(0), f32(m.arg(1)), m.r[6], m.r[7]); return m.ret(8); }
FX_PORT(0x006944c0, SpawnBuild_006944c0, OUT_EAX) { m.r[0] = rx_fx_spawn_build(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.r[0]); return m.ret(0xc); }
FX_PORT(0x00694ba0, SpawnBuild2_00694ba0, OUT_EAX) { m.r[0] = rx_fx_spawn_build2(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0x10); }
FX_PORT(0x00672fe0, VecAngle_00672fe0, OUT_XMM0) {
    const float r = rx_fx_vec_angle(m.r[6], m.r[7]);
    m.xmm[0].lo = bits(r); m.xmm[0].hi = 0; return m.ret(0);
}
FX_PORT(0x00695b00, EmitterInRange_00695b00, OUT_EAX) { m.r[0] = rx_fx_emitter_in_range(m.arg(0), m.arg(1)); return m.ret(8); }
FX_PORT(0x00695650, EmitterGate_00695650, 0) { rx_fx_emitter_gate(m.r[0], m.r[7]); return m.ret(0); }
FX_PORT(0x00695780, EmitterEmit_00695780, 0) { rx_fx_emitter_emit(m.arg(0)); return m.ret(0); }
FX_PORT(0x00695ab0, ParticleEmitter_vf0, 0) { rx_fx_emitter_vf0(m.r[1], f32(m.arg(0)), m.arg(1), m.arg(2), m.arg(3), m.arg(4)); return m.ret(0x14); }
// 0063c040: the rider's lens-swipe effect vf3 (attached by the idle gesture 0061eac0); its caller reads no register result.
FX_PORT(0x0063c040, AttachableParticleEffect_vf3, 0) { rx_fx_attachable_emitter_vf3(m.r[1], f32(m.arg(0))); return m.ret(4); }
FX_PORT(0x0069b060, VehicleParticleEffectEmitter_vf3, 0) { rx_fx_vehicle_emitter_vf3(m.r[1], f32(m.arg(0))); return m.ret(4); }
// 0069a170: the YRingParticleEmitter's spawn builder (fx_emit.c; reached only when a Y-ring emitter is active). The original
// returns the holder in EAX, but no caller reads it (0069ad60): no declared output.
FX_PORT(0x0069a170, YRingSpawn_0069a170, 0) { rx_yring_spawn(m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0x10); }

// fx_vehicle.c
FX_PORT(0x00697460, VehicleEffectGate_00697460, OUT_EAX) { m.r[0] = rx_fx_vehicle_effect_gate(m.r[1], m.r[0]); return m.ret(0); }
FX_PORT(0x00693560, VehicleEffectRate_00693560, 0) {
    rx_fx_vehicle_effect_rate(m.arg(0), m.arg(1), f32(m.arg(2)), f32(m.arg(3)), m.arg(4), m.arg(5), m.r[0], m.r[6]); return m.ret(0x18);
}
FX_PORT(0x00696d80, VehicleEffectUpdate_00696d80, 0) { rx_fx_vehicle_effect_update(f32(m.arg(0)), m.r[0], m.r[7]); return m.ret(4); }
FX_PORT(0x006979a0, VehicleEffectsJob_006979a0, 0) { rx_fx_vehicle_effects_job(m.arg(0)); return m.ret(0); }
FX_PORT(0x00697a00, VehicleEffectsUpdate_00697a00, 0) { rx_fx_vehicle_effects_update(m.r[6], f32(m.arg(0))); return m.ret(4); }
FX_PORT(0x00691bd0, VehicleEffectsUpdateAll_00691bd0, 0) { rx_fx_vehicle_effects_update_all(f32(m.arg(0))); return m.ret(0); }

// fx_effect.c
FX_PORT(0x00678980, SoundEmitter_00678980, 0) { rx_fx_sound_emitter_update(m.r[0]); return m.ret(0); }
FX_PORT(0x00696940, PlaceableParticleEffect_vf19, 0) { rx_fx_placeable_effect_tick(m.r[1], f32(m.arg(0))); return m.ret(4); }
FX_PORT(0x00692340, FireworksParticleEffect_vf19, 0) { rx_fx_fireworks_effect_tick(m.r[1], f32(m.arg(0))); return m.ret(4); }
FX_PORT(0x00692520, TriggeredPlaceableParticleEffect_vf19, 0) { rx_fx_triggered_effect_tick(m.r[1], f32(m.arg(0))); return m.ret(4); }
FX_PORT(0x006922c0, FireworksStart_006922c0, 0) { rx_fx_fireworks_start(f32(m.arg(0)), m.r[6]); return m.ret(4); }
// 00678bd0 (EAX = the sound event; plain ret)
FX_PORT(0x00678bd0, Fx_SoundStart_00678bd0, 0) { rx_fx_sound_start(m.r[0]); return m.ret(0); }
FX_PORT(0x006968e0, PlaceableParticleEffect_vf5_o90, 0) { rx_fx_placeable_effect_enable(m.r[1], m.arg(0)); return m.ret(4); }
