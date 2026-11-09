// loop_entity.cpp -- oracle adapters for core/game/loop_entity.c (group "rx_loop"): thiscall (entity, dt), ret 4.
#include "oracle.h"
#include "fx_oracle.h"   /* FxBind: 006c1a00's push esi (loop_entity.c) */
#include <cstdio>
extern "C" {
#include "../game/loop_entity.h"
}
using namespace reflex_port;
using rx_oracle::f32;

// 00839710 (the character ticker thread, stdcall ticker): one wake. Blocks again at 00839732 (WaitForSingleObject(wake, -1)) with ESP = entry - 0x1c,
// EBX = 0, ESI = the ticker, EDI = the import's address, EBP = &record + 0x10 when an entity was drained, else as on entry. Not reached by any world.
RX_PORT_RAW(0x00839710, CharacterTicker_Thread, "rx_loop", OUT_EAX) {
    const uint32_t entry = m.r[4], t = m.arg(0), r3 = m.r[3], r5 = m.r[5], r6 = m.r[6], r7 = m.r[7];
    const bool queued = int32_t(m.mem.read(t + 0x29c, 4) - m.mem.read(t + 0x298, 4)) >= 1;
    if (rx_character_ticker_run(t)) { m.r[3] = r3; m.r[5] = r5; m.r[6] = r6; m.r[7] = r7; m.r[0] = 0; return m.ret(0); }
    const uint32_t esp = entry - 0x1c, wake = uint32_t(m.mem.read(t + 0x10, 4));
    m.mem.write(esp, 4, 0x00839734u); m.mem.write(esp + 4, 4, wake); m.mem.write(esp + 8, 4, 0xffffffffu);
    m.r[3] = 0; m.r[5] = queued ? t + 0x2c0 : r5; m.r[6] = t; m.r[7] = uint32_t(m.mem.read(0x00916008, 4));
    m.r[4] = esp; m.r[0] = wake;
    return 0;
}
RX_PORT(0x00839890, CharacterTicker_Construct, "rx_loop", OUT_EAX) { m.r[0] = rx_character_ticker_construct(); return m.ret(0); }
RX_PORT(0x006925a0, JobPool_Instance, "rx_loop", OUT_EAX) { m.r[0] = rx_jobpool_instance(); return m.ret(0); }
RX_PORT(0x006ba250, AnimEntitySingleAnimLoop_vf19, "rx_loop", OUT_FPU) { rx_anim_entity_single_loop_update(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x006bb340, AnimEntitySplineAnimLoop_vf19, "rx_loop", OUT_FPU) { rx_anim_entity_spline_loop_update(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x006bc040, AnimEntitySplineAnimLoopWithParticles_vf19, "rx_loop", OUT_FPU) { rx_anim_entity_spline_particles_update(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0074ab50, ScenarioAnimEntity_vf19, "rx_loop", OUT_FPU) { rx_scenario_anim_entity_update(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x006bd6e0, CrewChief_vf19, "rx_loop", OUT_FPU) { rx_crew_chief_update(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x004137b0, InstancedAnimationEntity_vf19, "rx_loop", OUT_FPU) { rx_instanced_animation_update(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x006c1a00, ScoreTowerFlagHolder_vf19, "rx_loop", OUT_FPU) { FxBind fb_(m); rx_score_tower_flag_update(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0088fa20, Entity_vf14, "rx_loop", OUT_EAX) { m.r[0] = rx_entity_transform(m.r[1]); return m.ret(0); }
RX_PORT(0x006c6650, BillboardCrowdEntity_vf19, "rx_loop", OUT_FPU) {
    static int seen; if (!seen++) std::fprintf(stderr, "billboard cw %04x\n", m.fpu_cw);
    rx_billboard_crowd_update(m.r[1], f32(m.arg(0))); return m.ret(4);
}
RX_PORT(0x006d06a0, LandAnimalProximity_vf19, "rx_loop", OUT_FPU) { rx_land_animal_update(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x007e4170, WaterEntity_vf19, "rx_loop", OUT_FPU) { rx_water_entity_update(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00662dd0, ForestSystem_vf19, "rx_loop", OUT_FPU) { rx_forest_update(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x007c90a0, HFTerrainEntity_vf19, "rx_loop", OUT_FPU) { rx_terrain_entity_update(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x007c9110, HFTerrainEntity_vf25, "rx_loop", OUT_EAX) { m.r[0] = rx_terrain_entity_terrain(m.r[1]); return m.ret(0); }
// 006bd6a0 CrewChief_Job (stdcall: the job record; RET 0)
RX_PORT(0x006bd6a0, CrewChief_Job, "rx_loop", 0) { rx_crew_chief_job(m.arg(0)); return m.ret(0); }
