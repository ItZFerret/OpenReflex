// race_champ.cpp -- oracle adapters for core/game/race_champ.c (group rx_race): the championship race activity classes' virtuals, the
// stunt-event listener templates, and the base-class virtuals they share. ABI glue only.
#include "race_oracle.h"
extern "C" {
#include "../game/race_champ.h"
}
using namespace reflex_port;

// the x87 stack is balanced by the original (fld dword / fstp dword around the float argument), but the callees of a ported function
// do not use it: restore it after the body (as race_activity.cpp)
struct ChampFpuKeep { reflex_vm::Machine& m; std::vector<reflex_f80::F80> fpu; explicit ChampFpuKeep(reflex_vm::Machine& mm) : m(mm), fpu(mm.fpu) {} ~ChampFpuKeep() { m.fpu = fpu; } };

// ---- destructors (ECX = activity; stack flags; RET 4) -> EAX
RX_PORT(0x0057dfb0, ChampHeat_Sdtor, "rx_race", OUT_EAX) { m.r[0] = rx_champ_heat_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057e050, ChampIntro_Sdtor, "rx_race", OUT_EAX) { m.r[0] = rx_champ_intro_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057e1b0, ChampLCQ_Sdtor, "rx_race", OUT_EAX) { m.r[0] = rx_champ_lcq_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057e310, ChampMain_Sdtor, "rx_race", OUT_EAX) { m.r[0] = rx_champ_main_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057e820, ChampPractice_Sdtor, "rx_race", OUT_EAX) { m.r[0] = rx_champ_practice_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057e880, WpChampIntro_Sdtor, "rx_race", OUT_EAX) { m.r[0] = rx_champ_wp_intro_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057e960, WpChampMain_Sdtor, "rx_race", OUT_EAX) { m.r[0] = rx_champ_wp_main_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057eaa0, WpChampPractice_Sdtor, "rx_race", OUT_EAX) { m.r[0] = rx_champ_wp_practice_sdtor(m.r[1], m.arg(0)); return m.ret(4); }

// ---- the waypoint championship constructors (ESI = activity, ECX = type, EAX = world; the stack is not cleaned) -> EAX
RX_PORT(0x0057ea60, WpChampPractice_Construct, "rx_race", OUT_EAX) { m.r[0] = rx_champ_wp_practice_construct(m.r[6], m.r[1], m.r[0]); return m.ret(0); }
RX_PORT(0x0057e920, WpChampMain_Construct, "rx_race", OUT_EAX) { m.r[0] = rx_champ_wp_main_construct(m.r[6], m.r[1], m.r[0]); return m.ret(0); }

// ---- the stunt listener templates
// 0058a9a0 (ECX = event name; stdcall self, method; RET 8) / 0058aa60 (stdcall self; RET 4)
RX_PORT(0x0058a9a0, ChampStunt_Listen, "rx_race", 0) { rx_champ_stunt_listen(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0058aa60, ChampPhysicsStunt_Listen, "rx_race", 0) { rx_champ_physics_stunt_listen(m.arg(0)); return m.ret(4); }
// 0058ab20 (ECX = event name; stdcall self, method; RET 8) / 0058ab80 (stdcall self; RET 4)
RX_PORT(0x0058ab20, ChampStunt_Unlisten, "rx_race", 0) { rx_champ_stunt_unlisten(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0058ab80, ChampPhysicsStunt_Unlisten, "rx_race", 0) { rx_champ_physics_stunt_unlisten(m.arg(0)); return m.ret(4); }
// 0058a940 / 0058a420 (EAX = channel; stdcall activity; RET 4)
RX_PORT(0x0058a940, ChampLap_Unlisten, "rx_race", 0) { rx_champ_unlisten_lap(m.r[0], m.arg(0)); return m.ret(4); }
// the main event's stunt handlers (ECX = activity; stack player, descriptor [, type])
RX_PORT(0x0057e7c0, ChampMain_StuntBegin, "rx_race", 0) { rx_champ_main_stunt_begin(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0057e7f0, ChampMain_StuntFailed, "rx_race", 0) { rx_champ_main_stunt_failed(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0057e790, ChampMain_PhysicsStuntBegin, "rx_race", 0) { rx_champ_main_physics_stunt_begin(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }

// ---- vf2 (ECX = activity; stack arg; RET 4)
RX_PORT(0x0057e090, ChampIntro_Vf2, "rx_race", 0) { rx_champ_intro_vf2(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057e1f0, ChampHeat_Vf2, "rx_race", 0) { rx_champ_heat_vf2(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057e350, ChampMain_Vf2, "rx_race", 0) { rx_champ_main_vf2(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057e8c0, WpChampIntro_Vf2, "rx_race", 0) { rx_champ_wp_intro_vf2(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057e9b0, WpChampMain_Vf2, "rx_race", 0) { rx_champ_wp_main_vf2(m.r[1], m.arg(0)); return m.ret(4); }

// ---- vf3 / vf5 (ECX = activity)
RX_PORT(0x0057e210, ChampHeat_Vf3, "rx_race", 0) { rx_champ_heat_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x0057e390, ChampMain_Vf3, "rx_race", 0) { rx_champ_main_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x0057e8f0, WpChampIntro_Vf3, "rx_race", 0) { rx_champ_wp_intro_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x0057e0b0, ChampHeat_Vf5, "rx_race", 0) { rx_champ_heat_vf5(m.r[1]); return m.ret(0); }
RX_PORT(0x0057e410, ChampLCQ_Vf5, "rx_race", 0) { rx_champ_lcq_vf5(m.r[1]); return m.ret(0); }
RX_PORT(0x0057e8e0, WpChampIntro_Vf5, "rx_race", 0) { rx_champ_wp_intro_vf5(m.r[1]); return m.ret(0); }

// ---- the base-class and shared virtuals
RX_PORT(0x0055eb70, RaceStub_Vf9, "rx_race", OUT_EAX) { m.r[0] = rx_race_stub_vf9(m.r[1]); return m.ret(0); }
RX_PORT(0x005693d0, Champ_PlaceSwap, "rx_race", 0) { rx_champ_place_swap(m.r[1], m.arg(0), int32_t(m.arg(1))); return m.ret(8); }
RX_PORT(0x00557b40, Activity_Vf16, "rx_race", 0) { rx_activity_vf16(m.r[1]); return m.ret(0); }
RX_PORT(0x0054e190, Player_RestartReset, "rx_race", 0) { rx_player_restart_reset(m.r[0]); return m.ret(0); }
RX_PORT(0x0057eaf0, Champ_Vf15, "rx_race", 0) { rx_champ_vf15(m.r[1]); return m.ret(0); }
RX_PORT(0x0057e270, Champ_Vf17, "rx_race", 0) { rx_champ_vf17(m.r[1]); return m.ret(0); }
RX_PORT(0x0055d120, Activity_Vf20, "rx_race", 0) { ChampFpuKeep k(m); rx_activity_vf20(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057a3e0, WaypointRace_Vf20, "rx_race", 0) { ChampFpuKeep k(m); rx_waypoint_activity_vf20(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00578cc0, WaypointRace_Vf16, "rx_race", 0) { rx_waypoint_activity_vf16(m.r[1]); return m.ret(0); }
RX_PORT(0x0057eb60, WpChampPractice_Vf16, "rx_race", 0) { rx_champ_wp_practice_vf16(m.r[1]); return m.ret(0); }
RX_PORT(0x0057e9c0, WpChampMain_Vf16, "rx_race", 0) { rx_champ_wp_main_vf16(m.r[1]); return m.ret(0); }
RX_PORT(0x0057e9d0, WpChampMain_Vf42, "rx_race", 0) { rx_champ_wp_main_vf42(m.r[1]); return m.ret(0); }

// ---- the float-argument virtuals (ECX = activity; stack dt; RET 4)
RX_PORT(0x0057e1a0, ChampIntro_Vf25, "rx_race", 0) { ChampFpuKeep k(m); rx_champ_intro_vf25(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0057e910, WpChampIntro_Vf25, "rx_race", 0) { ChampFpuKeep k(m); rx_champ_wp_intro_vf25(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0057e000, ChampHeat_Vf28, "rx_race", 0) { ChampFpuKeep k(m); rx_champ_heat_vf28(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0057e2d0, ChampLCQ_Vf28, "rx_race", 0) { ChampFpuKeep k(m); rx_champ_lcq_vf28(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0057e860, ChampPractice_Vf28, "rx_race", 0) { ChampFpuKeep k(m); rx_champ_practice_vf28(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0057e4f0, ChampMain_Vf28, "rx_race", 0) { ChampFpuKeep k(m); rx_champ_main_vf28(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0057e9e0, WpChampMain_Vf28, "rx_race", 0) { ChampFpuKeep k(m); rx_champ_wp_main_vf28(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00559a00, Activity_Vf29, "rx_race", 0) { ChampFpuKeep k(m); rx_activity_vf29(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0057c010, ChampHeat_Vf29, "rx_race", 0) { ChampFpuKeep k(m); rx_champ_heat_vf29(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0057e6b0, ChampMain_Vf29, "rx_race", 0) { ChampFpuKeep k(m); rx_champ_main_vf29(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }

// ---- the rest (ECX = activity; stack arg; RET 4 unless noted)
RX_PORT(0x0057e700, ChampMain_Vf34, "rx_race", 0) { rx_champ_main_vf34(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057e720, Activity_Vf35, "rx_race", 0) { ChampFpuKeep k(m); rx_champ_vf35(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057e9f0, WpChamp_Vf35, "rx_race", 0) { ChampFpuKeep k(m); rx_champ_wp_vf35(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057e900, WpChamp_Vf37, "rx_race", 0) { rx_champ_wp_vf37(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057dff0, ChampHeat_Vf38, "rx_race", 0) { rx_champ_heat_vf38(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057e4b0, ChampMain_Vf38, "rx_race", 0) { rx_champ_main_vf38(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00557570, Activity_Vf45, "rx_race", 0) { rx_activity_vf45(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00567ac0, RaceStub_Vf45, "rx_race", 0) { ChampFpuKeep k(m); rx_race_stub_vf45(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0057ea30, Champ_Vf52, "rx_race", 0) { rx_champ_vf52(m.r[1]); return m.ret(0); }
RX_PORT(0x0057e760, ChampMain_Vf52, "rx_race", 0) { rx_champ_main_vf52(m.r[1]); return m.ret(0); }
