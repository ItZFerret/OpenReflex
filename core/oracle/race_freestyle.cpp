// race_freestyle.cpp -- oracle adapters for the Freestyle event mode: core/game/race_activity_freestyle.c, race_freestyle_driver.c,
// race_freestyle_rules.c, race_activity_champ_shared.c (group rx_race), anim_scenario_fmx.c (rx_anim), the championship's intro
// camera (rx_loop), and the construction side build_activity_freestyle.c / the driver unit's vf47 (rx_build8, the factory's group).
// ABI glue only: each adapter takes the original's registers / stack arguments and returns with the original's RET.
#include "race_oracle.h"
extern "C" {
#include "../game/race_activity_freestyle.h"
#include "../game/race_freestyle_driver.h"
#include "../game/race_freestyle_rules.h"
#include "../game/anim_scenario_fmx.h"
#include "../game/build_activity_freestyle.h"
}
using namespace reflex_port;
using rx_oracle::f32;
#define R "rx_race"
#define A "rx_anim"
#define L "rx_loop"
#define B "rx_build8"

// ---- construction (rx_build8) ----
RX_PORT(0x005623b0, FreestyleAudioConstruct, B, OUT_EAX) { m.r[0] = rx_freestyle_audio_construct(m.r[6]); return m.ret(0); }
// 00562280 (ECX = type, EAX = world; stdcall activity; RET 4): the callers read no register after it (live.tsv)
RX_PORT(0x00562280, FreestyleStubConstruct, B, 0) { rx_freestyle_stub_construct(m.arg(0), m.r[1], m.r[0]); return m.ret(4); }
RX_PORT(0x00561910, FreestyleConstruct, B, OUT_EAX) { m.r[0] = rx_freestyle_construct(m.r[6], m.r[1], m.r[0]); return m.ret(0); }
RX_PORT(0x00563740, FreestyleStub_vf47, B, 0) { rx_freestyle_stub_vf47(m.r[1], m.arg(0)); return m.ret(4); }

// ---- MXActivityFreestyleStub / MXActivityFreestyle (rx_race) ----
RX_PORT(0x00562560, FreestyleStub_vf2, R, 0) { rx_freestyle_stub_vf2(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00561a70, Freestyle_StartLine, R, 0) { rx_freestyle_start_line(m.r[1]); return m.ret(0); }
RX_PORT(0x00562800, Freestyle_StartSlots, R, 0) { rx_freestyle_start_slots(m.r[7]); return m.ret(0); }
RX_PORT(0x00561990, Freestyle_vf2, R, 0) { rx_freestyle_vf2(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00562440, FreestyleStub_vf5, R, 0) { rx_freestyle_stub_vf5(m.r[1]); return m.ret(0); }
RX_PORT(0x005626c0, FreestyleStub_vf18, R, 0) { rx_freestyle_stub_vf18(m.r[1]); return m.ret(0); }
RX_PORT(0x00562ac0, FreestyleStub_vf44, R, OUT_EAX) { m.r[0] = rx_freestyle_stub_vf44(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00562d40, FreestyleStub_vf37, R, 0) { rx_freestyle_stub_vf37(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x005620e0, Freestyle_vf52, R, 0) { rx_freestyle_vf52(m.r[1]); return m.ret(0); }
RX_PORT(0x00562220, Freestyle_StuntBegin, R, 0) { rx_freestyle_stunt_begin(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00562250, Freestyle_StuntFailed, R, 0) { rx_freestyle_stunt_failed(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x005621f0, Freestyle_PhysicsStuntBegin, R, 0) { rx_freestyle_physics_stunt_begin(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x00561e30, Freestyle_vf28, R, 0) { rx_freestyle_vf28(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00561dc0, Freestyle_vf38, R, 0) { rx_freestyle_vf38(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x005620d0, Freestyle_vf49, R, 0) { rx_freestyle_vf49(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00562170, Freestyle_vf46, R, 0) { rx_freestyle_vf46(m.r[1], m.arg(0), m.arg(1), f32(m.arg(2))); return m.ret(0xc); }
RX_PORT(0x00562ce0, FreestyleStub_vf23, R, 0) { rx_freestyle_stub_vf23(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00562e00, FreestyleStub_vf25, R, 0) { rx_freestyle_stub_vf25(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00561e10, Freestyle_vf25, R, 0) { rx_freestyle_vf25(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00563d00, FreestyleStub_vf8, R, OUT_EAX) { m.r[0] = rx_freestyle_stub_vf8(m.r[1]); return m.ret(0); }
RX_PORT(0x0055ec10, FreestyleStub_vf9, R, OUT_EAX) { m.r[0] = rx_freestyle_stub_vf9(m.r[1]); return m.ret(0); }
RX_PORT(0x0055ec00, FreestyleStub_vf48, R, OUT_EAX) { m.r[0] = rx_freestyle_stub_vf48(m.r[1]); return m.ret(0); }
RX_PORT(0x00562bd0, FreestyleStub_vf42, R, 0) { rx_freestyle_stub_vf42(m.r[1]); return m.ret(0); }
RX_PORT(0x00562af0, FreestyleStub_vf43, R, OUT_EAX) { m.r[0] = uint32_t(rx_freestyle_stub_vf43(m.r[1], m.arg(0), m.arg(1), m.arg(2))); return m.ret(0xc); }
RX_PORT(0x005626e0, FreestyleStub_vf14, R, 0) { rx_freestyle_stub_vf14(m.r[1], m.arg(0), int32_t(m.arg(1))); return m.ret(8); }
RX_PORT(0x00563820, FreestyleStub_vf19, R, OUT_EAX) { m.r[0] = rx_freestyle_stub_vf19(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0x10); }
RX_PORT(0x00563380, FreestyleStub_vf61, R, OUT_EAX) { m.r[0] = rx_freestyle_stub_vf61(m.r[1]); return m.ret(0); }
RX_PORT(0x00563ce0, FreestyleStub_vf53, R, 0) { rx_freestyle_stub_vf53(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00562f70, FreestyleStub_vf39, R, 0) { rx_freestyle_stub_vf39(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x005633a0, FreestyleStub_vf40, R, 0) { rx_freestyle_stub_vf40(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x006d85e0, Freestyle_Highlight, R, 0) { rx_freestyle_highlight(m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0x10); }
RX_PORT(0x005639a0, Freestyle_AiScores, R, 0) { rx_freestyle_ai_scores(m.arg(0)); return m.ret(4); }
RX_PORT(0x00563090, FreestyleStub_vf41, R, 0) { rx_freestyle_stub_vf41(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00561ef0, Freestyle_vf41, R, 0) { rx_freestyle_vf41(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00563aa0, Freestyle_PostState, R, 0) { rx_freestyle_post_state(m.r[1], m.r[0]); return m.ret(0); }
RX_PORT(0x00563b10, Freestyle_UpdatePost, R, 0) { rx_freestyle_update_post(m.r[0], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00563330, FreestyleStub_vf29, R, 0) { rx_freestyle_stub_vf29(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00561f10, Freestyle_vf29, R, 0) { rx_freestyle_vf29(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00563c50, FreestyleStub_vf58, R, 0) { rx_freestyle_stub_vf58(m.r[1]); return m.ret(0); }
RX_PORT(0x005635f0, FreestyleStub_vf31, R, 0) { rx_freestyle_stub_vf31(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0045d4f0, Vehicle_BodyLinkUnset, R, 0) { rx_vehicle_body_link_unset(m.r[0]); return m.ret(0); }
RX_PORT(0x0061c690, Pad_NetRelease, R, 0) { rx_pad_net_release(m.r[0]); return m.ret(0); }
RX_PORT(0x00563640, FreestyleStub_vf45, R, 0) { rx_freestyle_stub_vf45(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00561f80, Freestyle_GhostCompleted, R, 0) { rx_freestyle_ghost_completed(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x005627b0, FreestyleStub_vf3, R, 0) { rx_freestyle_stub_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x00587b70, Freestyle_UnlistenGhost, R, 0) { rx_freestyle_unlisten_ghost(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00587bd0, Freestyle_UnlistenStunt, R, 0) { rx_freestyle_unlisten_stunt(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00587c30, Freestyle_UnlistenPhysicsStunt, R, 0) { rx_freestyle_unlisten_physics_stunt(m.arg(0)); return m.ret(4); }
RX_PORT(0x00561d30, Freestyle_vf3, R, 0) { rx_freestyle_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x0057dc90, FreestyleChampIntro_vf3, R, 0) { rx_freestyle_champ_intro_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x005623f0, Freestyle_Destroy, R, 0) { rx_freestyle_destroy(m.r[1]); return m.ret(0); }
RX_PORT(0x00562390, FreestyleStub_sdtor, R, OUT_EAX) { m.r[0] = rx_freestyle_stub_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00561960, Freestyle_sdtor, R, OUT_EAX) { m.r[0] = rx_freestyle_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057dab0, FreestyleChampIntro_sdtor, R, OUT_EAX) { m.r[0] = rx_freestyle_champ_intro_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057dbf0, FreestyleChampMain_sdtor, R, OUT_EAX) { m.r[0] = rx_freestyle_champ_main_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057de90, FreestyleChampPractice_sdtor, R, OUT_EAX) { m.r[0] = rx_freestyle_champ_practice_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057daf0, FreestyleChampIntro_vf2, R, 0) { rx_freestyle_champ_intro_vf2(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057dc30, FreestyleChampMain_vf2, R, 0) { rx_freestyle_champ_main_vf2(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057db40, FreestyleChampIntro_vf5, R, 0) { rx_freestyle_champ_intro_vf5(m.r[1]); return m.ret(0); }
RX_PORT(0x0057ded0, FreestyleChampMain_vf5, R, 0) { rx_freestyle_champ_main_vf5(m.r[1]); return m.ret(0); }
RX_PORT(0x0057dbe0, FreestyleChampIntro_vf25, R, 0) { rx_freestyle_champ_intro_vf25(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0057dd00, FreestyleChampMain_vf28, R, 0) { rx_freestyle_champ_main_vf28(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0057dd40, FreestyleChampMain_vf34, R, 0) { rx_freestyle_champ_main_vf34(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057ddd0, FreestyleChampMain_vf35, R, 0) { rx_freestyle_champ_main_vf35(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057df10, FreestyleChampPractice_vf35, R, 0) { rx_freestyle_champ_practice_vf35(m.r[1], m.arg(0)); return m.ret(4); }

// ---- the championship classes' shared slot 38 (rx_race) ----
RX_PORT(0x0057dcf0, Champ_vf38, R, 0) { rx_champ_vf38(m.r[1], m.arg(0)); return m.ret(4); }

// ---- FreestyleDriver (rx_race) ----
RX_PORT(0x0050c900, FreestyleDriver_Construct, R, OUT_EAX) { m.r[0] = rx_freestyle_driver_construct(m.r[1], m.r[0]); return m.ret(0); }
RX_PORT(0x0050c990, FreestyleDriver_sdtor, R, OUT_EAX) { m.r[0] = rx_freestyle_driver_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0050c9c0, FreestyleDriver_vf1, R, 0) { rx_freestyle_driver_vf1(m.r[1]); return m.ret(0); }
RX_PORT(0x0050cae0, FreestyleDriver_vf2, R, 0) { rx_freestyle_driver_vf2(m.r[1]); return m.ret(0); }
RX_PORT(0x0050cbd0, FreestyleDriver_vf3, R, 0) { rx_freestyle_driver_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x0050d1c0, FreestyleDriver_vf6, R, 0) { rx_freestyle_driver_vf6(m.r[1]); return m.ret(0); }
RX_PORT(0x0050d610, FreestyleDriver_Human, R, 0) { rx_freestyle_driver_human(m.r[7], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0050cf00, FreestyleDriver_vf7, R, 0) { rx_freestyle_driver_vf7(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0050db40, FreestyleDriver_vf9, R, 0) { rx_freestyle_driver_vf9(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0050dba0, FreestyleDriver_vf12, R, OUT_EAX) { m.r[0] = rx_freestyle_driver_vf12(m.r[1]); return m.ret(0); }
RX_PORT(0x0050dbc0, FreestyleDriver_vf13, R, OUT_FPU) { m.fpu.push_back(reflex_f80::from_f32(rx_oracle::bits(rx_freestyle_driver_vf13(m.r[1])))); return m.ret(0); }
RX_PORT(0x0050dc70, FreestyleDriver_vf14, R, 0) { rx_freestyle_driver_vf14(m.r[1]); return m.ret(0); }
RX_PORT(0x0050dbf0, FreestyleDriver_vf16, R, 0) { rx_freestyle_driver_vf16(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0050dda0, FreestyleDriver_vf17, R, 0) { rx_freestyle_driver_vf17(m.r[1]); return m.ret(0); }
RX_PORT(0x0050dd20, FreestyleDriver_vf18, R, 0) { rx_freestyle_driver_vf18(m.r[1]); return m.ret(0); }

// ---- the rule and the behaviours (rx_race) ----
RX_PORT(0x006a6e90, CrowdRegion_Contains, R, OUT_EAX) { m.r[0] = rx_crowd_region_contains(m.r[0], m.r[1]); return m.ret(0); }
RX_PORT(0x0051bc50, RuleInCrowd_vf1, R, 0) { rx_rule_in_crowd_vf1(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0051bc70, RuleInCrowd_vf5, R, 0) { rx_rule_in_crowd_vf5(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00519970, Rule_SetEnabled, R, 0) { rx_rule_set_enabled(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0052b620, RoamTrack_vf3, R, 0) { rx_roam_track_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x0052b640, RoamTrack_vf6, R, 0) { rx_roam_track_vf6(m.r[1]); return m.ret(0); }
RX_PORT(0x00530fd0, Behavior_EnableReset, R, 0) { rx_behavior_enable_reset(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x005223c0, AvoidWreck_vf7, R, 0) { rx_avoid_wreck_vf7(m.r[1], m.arg(0)); return m.ret(4); }

// ---- FMXIntroScenario (rx_anim) ----
RX_PORT(0x0074da40, Fmx_RiderClip, A, OUT_EAX) { m.r[0] = rx_fmx_rider_clip(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0074d7a0, Fmx_ChiefClip, A, OUT_EAX) { m.r[0] = rx_fmx_chief_clip(m.r[1], m.r[2]); return m.ret(0); }
RX_PORT(0x0074d800, Fmx_PlaceChief, A, 0) { rx_fmx_place_chief(m.r[7], m.r[6], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0074d890, Fmx_PlaceChiefs, A, 0) { rx_fmx_place_chiefs(m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0074d980, Fmx_PlaceRiders, A, 0) { rx_fmx_place_riders(m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0074c5e0, Cast_ShowLocalRiders, A, 0) { rx_cast_show_local_riders(m.r[6]); return m.ret(0); }
RX_PORT(0x0074dab0, FmxIntro_vf0, A, 0) { rx_fmx_intro_vf0(m.r[1]); return m.ret(0); }
RX_PORT(0x0074dbc0, FmxIntro_vf1, A, 0) { rx_fmx_intro_vf1(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0074d700, FmxIntro_vf3, A, OUT_EAX) { m.r[0] = rx_fmx_intro_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x0074dcf0, FmxIntro_vf6, A, 0) { rx_fmx_intro_vf6(m.r[1]); return m.ret(0); }

// ---- the championship's intro camera (rx_loop) ----
RX_PORT(0x00767760, World_FreestyleIntroCam, L, OUT_EAX) { m.r[0] = rx_world_freestyle_intro_cam(m.r[0]); return m.ret(0); }
RX_PORT(0x0063d580, Entity_CopyConstruct, L, 0) { rx_entity_copy_construct(m.r[0], m.r[2]); return m.ret(0); }
RX_PORT(0x0063d4d0, CameraPath_CopyConstruct, L, OUT_EAX) { m.r[0] = rx_camera_path_copy_construct(m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0063d3e0, AnimatedCamera_Construct, L, OUT_EAX) { m.r[0] = rx_animated_camera_construct(m.r[6], m.arg(0)); return m.ret(4); }
RX_PORT(0x0063d3b0, AnimatedCamera_sdtor, L, OUT_EAX) { m.r[0] = rx_animated_camera_sdtor(m.r[1], m.arg(0)); return m.ret(4); }

// ---- an AI rider's freestyle result (rx_race) ----
// 006ccc70 (cdecl spread) -> xmm0 (lane 1 keeps the high dword of the double 0x9776a0 the original loaded with movsd: 0x3f000000)
RX_PORT(0x006ccc70, Fx_AiTrickDeviation, R, OUT_XMM0) {
    m.xmm[0].lo = uint64_t(rx_oracle::bits(rx_fx_ai_trick_deviation(f32(m.arg(0))))) | (uint64_t(0x3f000000u) << 32);
    m.xmm[0].hi = 0;
    return m.ret(0);
}
RX_PORT(0x006c8640, Fx_TrickRecord, R, OUT_EAX) { m.r[0] = rx_fx_trick_record(m.r[7]); return m.ret(0); }
// 006ccd10 (ESI = scorer, xmm3 = chance; stdcall spread; RET 4)
RX_PORT(0x006ccd10, Fx_AiTrickScores, R, 0) { rx_fx_ai_trick_scores(m.r[6], f32(uint32_t(m.xmm[3].lo)), f32(m.arg(0))); return m.ret(4); }
