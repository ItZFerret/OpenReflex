// race_section.cpp -- oracle adapters for core/game/race_section*.c (group rx_race): the Section Race activity (MXActivitySectionRace),
// its SectionRaceDriver, SectionRaceActivityAudioLogic and the SectionRaceZone entity.
#include "race_oracle.h"
extern "C" {
#include "../game/race_section.h"
}
using namespace reflex_port;

namespace {
struct SecFpuKeep { reflex_vm::Machine& m; std::vector<reflex_f80::F80> fpu; explicit SecFpuKeep(reflex_vm::Machine& mm) : m(mm), fpu(mm.fpu) {} ~SecFpuKeep() { m.fpu = fpu; } };
}

// ---- the activity ----
RX_PORT(0x00582e80, SectionAudio_Construct, "rx_race", OUT_EAX) { m.r[0] = rx_section_audio_logic_construct(m.r[6]); return m.ret(0); }
RX_PORT(0x0056a210, SectionRace_Construct, "rx_race", OUT_EAX) { m.r[0] = rx_section_race_construct(m.r[7], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0056a2c0, SectionRace_Sdtor, "rx_race", OUT_EAX) { m.r[0] = rx_section_race_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00588b80, SectionRace_NetRemovedListen, "rx_race", 0) { rx_section_race_net_removed_listen(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00588c40, SectionRace_NetActivityListen, "rx_race", 0) { rx_section_race_net_activity_listen(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00588d00, SectionRace_NetRemovedUnlisten, "rx_race", 0) { rx_section_race_net_removed_unlisten(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00588d60, SectionRace_NetActivityUnlisten, "rx_race", 0) { rx_section_race_net_activity_unlisten(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0056af70, SectionRace_NetObjectRemoved, "rx_race", 0) { rx_section_net_object_removed(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0056a300, SectionRace_Vf2, "rx_race", 0) { rx_section_race_vf2(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0056a520, SectionRace_Vf3, "rx_race", 0) { rx_section_race_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x0056a680, SectionRace_Vf4, "rx_race", 0) { rx_section_race_vf4(m.r[1]); return m.ret(0); }
RX_PORT(0x00571490, PokerRace_Vf16, "rx_race", 0) { rx_poker_race_vf16(m.r[1]); return m.ret(0); }
RX_PORT(0x0056a6b0, SectionRace_Vf18, "rx_race", 0) { rx_section_race_vf18(m.r[1]); return m.ret(0); }
RX_PORT(0x0056a750, SectionRace_Vf23, "rx_race", 0) { SecFpuKeep k(m); rx_section_race_vf23(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0056acf0, SectionRace_Leaders, "rx_race", 0) { rx_section_race_leaders(m.r[2]); return m.ret(0); }
RX_PORT(0x0056ac70, SectionRace_Vf28, "rx_race", 0) { SecFpuKeep k(m); rx_section_race_vf28(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0056a730, SectionRace_Vf38, "rx_race", 0) { rx_section_race_vf38(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0056a930, SectionRace_Vf42, "rx_race", 0) { rx_section_race_vf42(m.r[1]); return m.ret(0); }
RX_PORT(0x0056ae40, SectionRace_CompareBest, "rx_race", OUT_EAX) { m.r[0] = (uint32_t)rx_section_compare_best(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0056adf0, SectionRace_Vf43, "rx_race", OUT_EAX) { m.r[0] = (uint32_t)rx_section_race_compare(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(12); }
RX_PORT(0x0056acc0, SectionRace_Vf44, "rx_race", OUT_EAX) { m.r[0] = rx_section_race_vf44(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0056a720, SectionRace_Vf47, "rx_race", 0) { rx_section_race_vf47(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0056b020, SectionRace_Vf52, "rx_race", 0) { rx_section_race_vf52(); return m.ret(0); }
RX_PORT(0x0056aed0, SectionRace_TrackAudioKind, "rx_race", OUT_EAX) { m.r[0] = rx_section_track_audio_kind(m.r[7], m.r[6]); return m.ret(0); }

// ---- the driver ----
RX_PORT(0x00512db0, SectionDriver_Construct, "rx_race", OUT_EAX) { m.r[0] = rx_section_driver_construct(m.r[6], m.r[0]); return m.ret(0); }
RX_PORT(0x00512e00, SectionDriver_Vf1, "rx_race", 0) { rx_section_driver_vf1(m.r[1]); return m.ret(0); }
RX_PORT(0x00512eb0, SectionDriver_Vf2, "rx_race", 0) { rx_section_driver_vf2(m.r[1]); return m.ret(0); }
RX_PORT(0x00512fc0, SectionDriver_Vf3, "rx_race", 0) { rx_section_driver_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x00513020, SectionDriver_Vf7, "rx_race", 0) { SecFpuKeep k(m); rx_section_driver_vf7(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x005132e0, SectionDriver_Vf17, "rx_race", 0) { rx_section_driver_vf17(m.r[1]); return m.ret(0); }
RX_PORT(0x00513080, SectionDriver_Entered, "rx_race", 0) { rx_section_driver_entered(m.r[1], m.arg(0), (int32_t)m.arg(1), (int32_t)m.arg(2)); return m.ret(12); }
RX_PORT(0x005133f0, SectionDriver_Best, "rx_race", 0) { rx_section_driver_best(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0053ace0, SectionDriver_Listen, "rx_race", 0) { rx_section_driver_listen(m.arg(0)); return m.ret(4); }
RX_PORT(0x0053ada0, SectionDriver_Unlisten, "rx_race", 0) { rx_section_driver_unlisten(m.arg(0)); return m.ret(4); }
RX_PORT(0x0053aea0, SectionDriver_EventDriverInt, "rx_race", 0) { rx_section_event_driver_int(m.r[6], m.r[2], m.arg(0), (int32_t)m.r[1]); return m.ret(4); }
RX_PORT(0x005125c0, RaceDriver_Vf12, "rx_race", OUT_EAX) { m.r[0] = rx_race_driver_vf12(m.r[1]); return m.ret(0); }
RX_PORT(0x005125f0, RaceDriver_Vf13, "rx_race", OUT_FPU) { m.fpu.push_back(reflex_f80::from_f32(rx_oracle::bits(rx_race_driver_vf13(m.r[1])))); return m.ret(0); }

// ---- the audio logic ----
RX_PORT(0x0058ba90, SectionAudio_Listen, "rx_race", 0) { rx_section_audio_listen(m.r[1], m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0058bb50, SectionAudio_Unlisten, "rx_race", 0) { rx_section_audio_unlisten(m.r[1], m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00582ef0, SectionAudio_Vf3, "rx_race", 0) { rx_section_audio_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x00582ff0, SectionAudio_Vf4, "rx_race", 0) { rx_section_audio_vf4(m.r[1]); return m.ret(0); }
RX_PORT(0x005830f0, SectionAudio_Winner, "rx_race", 0) { rx_section_audio_winner(m.r[1], m.arg(0), (int32_t)m.arg(1)); return m.ret(8); }
RX_PORT(0x00583110, SectionAudio_Lost, "rx_race", 0) { rx_section_audio_lost(m.r[1], m.arg(0), (int32_t)m.arg(1)); return m.ret(8); }

// ---- the zone ----
RX_PORT(0x0040aa00, SectionZone_Sdtor, "rx_race", OUT_EAX) { m.r[0] = rx_section_zone_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x004198e0, SectionZone_Post, "rx_race", 0) { rx_section_zone_post(m.r[1], m.r[2], m.arg(0)); return m.ret(4); }
RX_PORT(0x0040ac80, SectionZone_PlayerInside, "rx_race", 0) { rx_section_zone_player_inside(m.r[0], m.r[6]); return m.ret(0); }
RX_PORT(0x0040aae0, SectionZone_Tick, "rx_race", 0) { SecFpuKeep k(m); rx_section_zone_tick(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
