// race_minigames.cpp -- oracle adapters for core/game/race_egghunt.c, race_graffiti.c and race_capture.c (group rx_party): the last three
// party modes (EggHunt, Graffiti, Capture), their drivers, rules, audio logic and helpers. No installed level runs them: the test fixtures
// of partylast_probe.cpp give the original and the core the same scene entities so both start the mode (see that file).
#include "race_oracle.h"
extern "C" {
#include "../game/race_minigames.h"
}
using namespace reflex_port;

namespace {
struct MgFpuKeep { reflex_vm::Machine& m; std::vector<reflex_f80::F80> fpu; explicit MgFpuKeep(reflex_vm::Machine& mm) : m(mm), fpu(mm.fpu) {} ~MgFpuKeep() { m.fpu = fpu; } };
}

// ---- EggHunt: the activity ----
RX_PORT(0x00574730, EggHunt_Construct, "rx_party", OUT_EAX) { m.r[0] = rx_egghunt_construct(m.arg(0), m.r[1], m.r[0]); return m.ret(4); }
RX_PORT(0x00574840, EggHunt_Sdtor, "rx_party", OUT_EAX) { m.r[0] = rx_egghunt_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00574860, EggHunt_Destroy, "rx_party", 0) { rx_egghunt_destroy(m.r[1]); return m.ret(0); }
RX_PORT(0x005748f0, EggHunt_Vf2, "rx_party", 0) { rx_egghunt_vf2(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00574b90, EggHunt_Vf3, "rx_party", 0) { rx_egghunt_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x00574cd0, EggHunt_Vf4, "rx_party", 0) { rx_egghunt_vf4(m.r[1]); return m.ret(0); }
RX_PORT(0x00574d20, EggHunt_Vf15, "rx_party", 0) { MgFpuKeep k(m); rx_egghunt_vf15(m.r[1]); return m.ret(0); }
RX_PORT(0x00574d40, EggHunt_Vf16, "rx_party", 0) { rx_egghunt_vf16(m.r[1]); return m.ret(0); }
RX_PORT(0x005757b0, EggHunt_Vf18, "rx_party", 0) { rx_egghunt_vf18(m.r[1]); return m.ret(0); }
RX_PORT(0x00575af0, EggHunt_Vf25, "rx_party", 0) { MgFpuKeep k(m); rx_egghunt_vf25(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00575e00, EggHunt_Vf27, "rx_party", 0) { MgFpuKeep k(m); rx_egghunt_vf27(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00575b60, EggHunt_Vf28, "rx_party", 0) { MgFpuKeep k(m); rx_egghunt_vf28(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x005758e0, EggHunt_Vf40, "rx_party", 0) { rx_egghunt_vf40(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00575eb0, EggHunt_Vf42, "rx_party", 0) { rx_egghunt_vf42(m.r[1]); return m.ret(0); }
RX_PORT(0x005758a0, EggHunt_Vf43, "rx_party", OUT_EAX) { m.r[0] = (uint32_t)rx_egghunt_vf43(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(12); }
RX_PORT(0x00575840, EggHunt_Vf44, "rx_party", OUT_EAX) { m.r[0] = rx_egghunt_vf44(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00575ed0, EggHunt_Vf47, "rx_party", 0) { rx_egghunt_vf47(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00574dd0, EggHunt_PlaceRound, "rx_party", 0) { MgFpuKeep k(m); rx_egghunt_place_round(m.arg(0)); return m.ret(4); }
RX_PORT(0x00575440, EggHunt_SpawnEggs, "rx_party", 0) { rx_egghunt_spawn_eggs(m.r[7]); return m.ret(0); }
RX_PORT(0x00574660, EggHunt_Collect, "rx_party", OUT_EAX) { m.r[0] = rx_egghunt_collect(m.r[6], m.r[7], m.arg(0)); return m.ret(4); }
RX_PORT(0x00589ed0, EggHunt_NetRemovedListen, "rx_party", 0) { rx_egghunt_net_removed_listen(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00589f90, EggHunt_NetPickedListen, "rx_party", 0) { rx_egghunt_net_picked_listen(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058a050, EggHunt_NetRemovedUnlisten, "rx_party", 0) { rx_egghunt_net_removed_unlisten(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058a0b0, EggHunt_NetPickedUnlisten, "rx_party", 0) { rx_egghunt_net_picked_unlisten(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058a110, EggHunt_PlayerWins, "rx_party", 0) { rx_egghunt_player_wins(m.r[6], m.r[1], m.r[2], m.arg(0)); return m.ret(4); }

// ---- EggHunt: the eggs (GreedObjectModel; MXCaptureFlag shares vf16 / vf19 / vf28) ----
RX_PORT(0x0055ec30, GreedModel_Construct, "rx_party", OUT_EAX) { m.r[0] = rx_greed_model_construct(m.r[1]); return m.ret(0); }
RX_PORT(0x00576040, GreedModel_Vf16, "rx_party", 0) { rx_greed_model_vf16(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00576070, GreedModel_Vf19, "rx_party", 0) { rx_greed_model_vf19(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00574630, GreedModel_Vf28, "rx_party", 0) { MgFpuKeep k(m); rx_greed_model_vf28(m.r[1], m.arg(0)); return m.ret(4); }

// ---- EggHunt: the driver ----
RX_PORT(0x00515ff0, EggHuntDriver_Sdtor, "rx_party", OUT_EAX) { m.r[0] = rx_egghunt_driver_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00516030, EggHuntDriver_Vf3, "rx_party", 0) { rx_egghunt_driver_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x005160b0, EggHuntDriver_Vf7, "rx_party", 0) { MgFpuKeep k(m); rx_egghunt_driver_vf7(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }

// ---- EggHunt: the audio logic ----
RX_PORT(0x005848f0, EggHuntAudio_Vf3, "rx_party", 0) { rx_egghunt_audio_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x00584b70, EggHuntAudio_Vf4, "rx_party", 0) { rx_egghunt_audio_vf4(m.r[1]); return m.ret(0); }
RX_PORT(0x0058cff0, EggHuntAudio_ListenRoundStarted, "rx_party", 0) { rx_egghunt_audio_listen_round_started(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058d0b0, EggHuntAudio_ListenActivity, "rx_party", 0) { rx_egghunt_audio_listen_activity(m.r[1], m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0058d170, EggHuntAudio_ListenCollected, "rx_party", 0) { rx_egghunt_audio_listen_collected(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058d230, EggHuntAudio_ListenPlayerWins, "rx_party", 0) { rx_egghunt_audio_listen_player_wins(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058d2f0, EggHuntAudio_UnlistenRoundStarted, "rx_party", 0) { rx_egghunt_audio_unlisten_round_started(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058d350, EggHuntAudio_UnlistenActivity, "rx_party", 0) { rx_egghunt_audio_unlisten_activity(m.r[1], m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0058d3b0, EggHuntAudio_UnlistenCollected, "rx_party", 0) { rx_egghunt_audio_unlisten_collected(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058d410, EggHuntAudio_UnlistenPlayerWins, "rx_party", 0) { rx_egghunt_audio_unlisten_player_wins(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00584e10, EggHuntAudio_Collected, "rx_party", 0) { rx_egghunt_audio_collected(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00584e30, EggHuntAudio_Spawned, "rx_party", 0) { rx_egghunt_audio_spawned(m.r[1], m.arg(0)); return m.ret(4); }

// ---- Capture: the activity ----
RX_PORT(0x00576290, Capture_Construct, "rx_party", OUT_EAX) { m.r[0] = rx_capture_construct(m.arg(0), m.r[1], m.r[0]); return m.ret(4); }
RX_PORT(0x005764e0, Capture_SmokeConstruct, "rx_party", OUT_EAX) { m.r[0] = rx_capture_smoke_construct(m.r[1]); return m.ret(0); }
RX_PORT(0x005764f0, Capture_Destroy, "rx_party", 0) { rx_capture_destroy(m.r[1]); return m.ret(0); }
RX_PORT(0x005764c0, Capture_Sdtor, "rx_party", OUT_EAX) { m.r[0] = rx_capture_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058a170, Capture_NetRemovedListen, "rx_party", 0) { rx_capture_net_removed_listen(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058a230, Capture_NetEventListen, "rx_party", 0) { rx_capture_net_event_listen(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00575fc0, Capture_FlagAttach, "rx_party", 0) { rx_capture_flag_attach(m.r[0], m.r[6], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00576b60, Capture_PlaceStarts, "rx_party", 0) { MgFpuKeep k(m); rx_capture_place_starts(m.arg(0)); return m.ret(4); }
RX_PORT(0x00577ad0, Capture_BasePosition, "rx_party", 0) { rx_capture_base_position(m.r[0], m.r[2], m.arg(0)); return m.ret(4); }
RX_PORT(0x00577b80, Capture_FlagHome, "rx_party", 0) { rx_capture_flag_home(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00577c30, Capture_Score, "rx_party", 0) { rx_capture_score(m.r[6], m.arg(0)); return m.ret(4); }
RX_PORT(0x00578200, Capture_FlagsTouch, "rx_party", 0) { rx_capture_flags_touch(m.r[7]); return m.ret(0); }
RX_PORT(0x005765c0, Capture_Vf2, "rx_party", 0) { rx_capture_vf2(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00576f40, Capture_Vf3, "rx_party", 0) { rx_capture_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x00576ab0, Capture_Vf4, "rx_party", 0) { rx_capture_vf4(m.r[1]); return m.ret(0); }
RX_PORT(0x00577a00, Capture_Vf15, "rx_party", 0) { MgFpuKeep k(m); rx_capture_vf15(m.r[1]); return m.ret(0); }
RX_PORT(0x00577a60, Capture_Vf16, "rx_party", 0) { rx_capture_vf16(m.r[1]); return m.ret(0); }
RX_PORT(0x00577010, Capture_Vf18, "rx_party", 0) { rx_capture_vf18(m.r[1]); return m.ret(0); }
RX_PORT(0x00577d20, Capture_Vf19, "rx_party", OUT_EAX) { MgFpuKeep k(m); m.r[0] = rx_capture_vf19(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0x10); }
RX_PORT(0x00577b60, Capture_Vf27, "rx_party", 0) { MgFpuKeep k(m); rx_capture_vf27(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00577630, Capture_Vf28, "rx_party", 0) { MgFpuKeep k(m); rx_capture_vf28(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x005772b0, Capture_Vf38, "rx_party", 0) { rx_capture_vf38(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00577280, Capture_Vf42, "rx_party", 0) { rx_capture_vf42(m.r[1]); return m.ret(0); }
RX_PORT(0x00577240, Capture_Vf43, "rx_party", OUT_EAX) { m.r[0] = (uint32_t)rx_capture_vf43(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(12); }
RX_PORT(0x005770f0, Capture_Vf44, "rx_party", OUT_EAX) { m.r[0] = rx_capture_vf44(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00577200, Capture_Vf47, "rx_party", 0) { rx_capture_vf47(m.r[1], m.arg(0)); return m.ret(4); }

// ---- Capture: the driver and its rule ----
RX_PORT(0x0058a2f0, CaptureRule_Create, "rx_party", OUT_EAX) { m.r[0] = rx_capture_rule_create(m.r[1]); return m.ret(0); }
RX_PORT(0x005161d0, CaptureDriver_Sdtor, "rx_party", OUT_EAX) { m.r[0] = rx_capture_driver_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0053b290, Driver_RuleCapture, "rx_party", OUT_EAX) { m.r[0] = rx_driver_rule_capture(m.arg(0)); return m.ret(4); }
RX_PORT(0x00516200, CaptureDriver_Vf3, "rx_party", 0) { rx_capture_driver_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x00516490, CaptureDriver_Human, "rx_party", 0) { MgFpuKeep k(m); rx_capture_driver_human(m.arg(0), rx_oracle::f32(m.arg(1))); return m.ret(8); }
RX_PORT(0x00516380, CaptureDriver_Ai, "rx_party", 0) { MgFpuKeep k(m); rx_capture_driver_ai(m.r[6], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x005162a0, CaptureDriver_Vf7, "rx_party", 0) { MgFpuKeep k(m); rx_capture_driver_vf7(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x005164d0, CaptureDriver_Vf9, "rx_party", 0) { rx_capture_driver_vf9(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0051ca80, CaptureRule_Vf1, "rx_party", 0) { rx_capture_rule_vf1(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0051cab0, CaptureRule_Vf5, "rx_party", 0) { MgFpuKeep k(m); rx_capture_rule_vf5(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }

// ---- Graffiti: the activity ----
RX_PORT(0x00582450, GraffitiAudio_Construct, "rx_party", OUT_EAX) { m.r[0] = rx_graffiti_audio_construct(m.r[6]); return m.ret(0); }
RX_PORT(0x00563d90, Graffiti_Construct, "rx_party", OUT_EAX) { m.r[0] = rx_graffiti_construct(m.arg(0), m.r[1], m.r[0]); return m.ret(4); }
RX_PORT(0x00563e10, Graffiti_Sdtor, "rx_party", OUT_EAX) { m.r[0] = rx_graffiti_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00587ef0, Graffiti_NetRemovedListen, "rx_party", 0) { rx_graffiti_net_removed_listen(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00587fb0, Graffiti_NetCapturedListen, "rx_party", 0) { rx_graffiti_net_captured_listen(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00588070, Graffiti_NetRemovedUnlisten, "rx_party", 0) { rx_graffiti_net_removed_unlisten(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x005880d0, Graffiti_NetCapturedUnlisten, "rx_party", 0) { rx_graffiti_net_captured_unlisten(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00408400, GraffitiPoint_Trigger, "rx_party", OUT_EAX) { m.r[0] = rx_graffiti_point_trigger(m.arg(0)); return m.ret(4); }
RX_PORT(0x00408220, GraffitiPoint_Label, "rx_party", 0) { rx_graffiti_point_label(m.r[7]); return m.ret(0); }
RX_PORT(0x00564970, Graffiti_DriverOf, "rx_party", OUT_EAX) { m.r[0] = rx_graffiti_driver_of(m.r[7], m.arg(0)); return m.ret(4); }
RX_PORT(0x00563e60, Graffiti_Vf2, "rx_party", 0) { MgFpuKeep k(m); rx_graffiti_vf2(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00564320, Graffiti_Vf3, "rx_party", 0) { rx_graffiti_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x00564960, Graffiti_Vf15, "rx_party", 0) { rx_graffiti_vf15(m.r[1]); return m.ret(0); }
RX_PORT(0x00564450, Graffiti_Vf18, "rx_party", 0) { rx_graffiti_vf18(m.r[1]); return m.ret(0); }
RX_PORT(0x005646c0, Graffiti_Vf23, "rx_party", 0) { MgFpuKeep k(m); rx_graffiti_vf23(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00564560, Graffiti_Vf40, "rx_party", 0) { rx_graffiti_vf40(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x005648c0, Graffiti_Vf42, "rx_party", 0) { rx_graffiti_vf42(m.r[1]); return m.ret(0); }
RX_PORT(0x005644f0, Graffiti_Vf43, "rx_party", OUT_EAX) { m.r[0] = (uint32_t)rx_graffiti_vf43(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(12); }
RX_PORT(0x0050df10, GraffitiDriver_Construct, "rx_party", OUT_EAX) { m.r[0] = rx_graffiti_driver_construct(m.r[1], m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x005644c0, Graffiti_Vf44, "rx_party", OUT_EAX) { m.r[0] = rx_graffiti_vf44(m.r[1], m.arg(0)); return m.ret(4); }

// ---- Graffiti: the driver and the audio logic ----
RX_PORT(0x0050df80, GraffitiDriver_Vf1, "rx_party", 0) { rx_graffiti_driver_vf1(m.r[1]); return m.ret(0); }
RX_PORT(0x0050dfa0, GraffitiDriver_Vf3, "rx_party", 0) { MgFpuKeep k(m); rx_graffiti_driver_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x0050e030, GraffitiDriver_Vf7, "rx_party", 0) { MgFpuKeep k(m); rx_graffiti_driver_vf7(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0050e120, GraffitiDriver_Vf17, "rx_party", 0) { rx_graffiti_driver_vf17(m.r[1]); return m.ret(0); }
RX_PORT(0x0058b190, GraffitiAudio_ListenCaptured, "rx_party", 0) { rx_graffiti_audio_listen_captured(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058b250, GraffitiAudio_UnlistenCaptured, "rx_party", 0) { rx_graffiti_audio_unlisten_captured(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00582490, GraffitiAudio_Vf29, "rx_party", 0) { rx_graffiti_audio_vf29(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x005824f0, GraffitiAudio_Vf30, "rx_party", 0) { rx_graffiti_audio_vf30(m.r[1]); return m.ret(0); }
RX_PORT(0x00582510, GraffitiAudio_Vf32, "rx_party", 0) { rx_graffiti_audio_vf32(m.r[1]); return m.ret(0); }
RX_PORT(0x00582010, AudioLogic_Vf19, "rx_party", 0) { rx_audio_logic_vf19(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00582520, GraffitiAudio_Captured, "rx_party", 0) { MgFpuKeep k(m); rx_graffiti_audio_captured(m.r[1], m.arg(0)); return m.ret(4); }

// ---- Graffiti: the capture point entity (race_graffiti_point.c; test-fixture points of partylast_probe.cpp) ----
RX_PORT(0x004075c0, GraffitiPoint_Construct, "rx_party", OUT_EAX) { m.r[0] = rx_graffiti_point_construct(m.r[7]); return m.ret(0); }
RX_PORT(0x004076c0, GraffitiPoint_Destroy, "rx_party", 0) { rx_graffiti_point_destroy(m.r[0]); return m.ret(0); }
RX_PORT(0x00407670, GraffitiPoint_Sdtor, "rx_party", OUT_EAX) { m.r[0] = rx_graffiti_point_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0041b170, GraffitiPoint_SdtorRender, "rx_party", OUT_EAX) { m.r[0] = rx_graffiti_point_sdtor_render(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00407780, GraffitiPoint_Vf5, "rx_party", 0) { rx_graffiti_point_vf5(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00407ec0, GraffitiPoint_Vf19, "rx_party", 0) { rx_graffiti_point_vf19(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00407930, GraffitiPoint_OnInit, "rx_party", 0) { rx_graffiti_point_on_init(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00407970, GraffitiPoint_SetHolder, "rx_party", 0) { MgFpuKeep k(m); rx_graffiti_point_set_holder(m.r[6], m.r[0]); return m.ret(0); }
RX_PORT(0x00407a30, GraffitiPoint_Record, "rx_party", OUT_EAX) { m.r[0] = rx_graffiti_point_record(m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0041a530, GraffitiPoint_RecordsGrow, "rx_party", 0) { rx_graffiti_point_records_grow(m.r[6], m.arg(0)); return m.ret(4); }
RX_PORT(0x00407a70, GraffitiPoint_AddRider, "rx_party", 0) { rx_graffiti_point_add_rider(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00407af0, GraffitiPoint_OnEntered, "rx_party", 0) { rx_graffiti_point_on_entered(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00407c00, GraffitiPoint_OnExited, "rx_party", 0) { rx_graffiti_point_on_exited(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00407d10, GraffitiPoint_RecordUpdate, "rx_party", 0) { rx_graffiti_point_record_update(m.r[1], m.r[6]); return m.ret(0); }
RX_PORT(0x00407e00, GraffitiPoint_RemoveRider, "rx_party", 0) { rx_graffiti_point_remove_rider(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00407e60, GraffitiPoint_Prune, "rx_party", 0) { rx_graffiti_point_prune(m.r[0]); return m.ret(0); }
RX_PORT(0x0050e0a0, GraffitiDriver_Captured, "rx_party", 0) { rx_graffiti_driver_captured(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00408090, GraffitiPoint_Capture, "rx_party", 0) { MgFpuKeep k(m); rx_graffiti_point_capture(m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00408110, GraffitiPoint_CaptureVehicle, "rx_party", 0) { MgFpuKeep k(m); rx_graffiti_point_capture_vehicle(m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00407f10, GraffitiPoint_Score, "rx_party", 0) { MgFpuKeep k(m); rx_graffiti_point_score(m.r[0], m.arg(0), (int32_t)m.arg(1)); return m.ret(8); }
RX_PORT(0x00408180, GraffitiPoint_OnScore, "rx_party", 0) { MgFpuKeep k(m); rx_graffiti_point_on_score(m.r[1], m.arg(0), rx_oracle::f32(m.arg(1))); return m.ret(8); }
RX_PORT(0x004081c0, GraffitiPoint_OnBegins, "rx_party", 0) { rx_graffiti_point_on_begins(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(12); }
RX_PORT(0x004081e0, GraffitiPoint_OnFailed, "rx_party", 0) { rx_graffiti_point_on_failed(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00408200, GraffitiPoint_OnKickout, "rx_party", 0) { rx_graffiti_point_on_kickout(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00419220, GraffitiPoint_ListenTrigger, "rx_party", 0) { rx_graffiti_point_listen_trigger(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(12); }
RX_PORT(0x004192f0, GraffitiPoint_ListenInit, "rx_party", 0) { rx_graffiti_point_listen_init(m.arg(0), m.arg(1), m.arg(2)); return m.ret(12); }
RX_PORT(0x004193c0, GraffitiPoint_ListenScore, "rx_party", 0) { rx_graffiti_point_listen_score(m.arg(0), m.arg(1), m.arg(2)); return m.ret(12); }
RX_PORT(0x00419490, GraffitiPoint_ListenBegins, "rx_party", 0) { rx_graffiti_point_listen_begins(m.arg(0), m.arg(1), m.arg(2)); return m.ret(12); }
RX_PORT(0x00419560, GraffitiPoint_ListenFailed, "rx_party", 0) { rx_graffiti_point_listen_failed(m.arg(0), m.arg(1), m.arg(2)); return m.ret(12); }
RX_PORT(0x00419630, GraffitiPoint_ListenKickout, "rx_party", 0) { rx_graffiti_point_listen_kickout(m.arg(0), m.arg(1), m.arg(2)); return m.ret(12); }
RX_PORT(0x00418f80, GraffitiPoint_UnlistenTrigger, "rx_party", 0) { rx_graffiti_point_unlisten_trigger(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(12); }
RX_PORT(0x00418ff0, GraffitiPoint_UnlistenInit, "rx_party", 0) { rx_graffiti_point_unlisten_init(m.arg(0), m.arg(1), m.arg(2)); return m.ret(12); }
RX_PORT(0x00419060, GraffitiPoint_UnlistenScore, "rx_party", 0) { rx_graffiti_point_unlisten_score(m.arg(0), m.arg(1), m.arg(2)); return m.ret(12); }
RX_PORT(0x004190d0, GraffitiPoint_UnlistenBegins, "rx_party", 0) { rx_graffiti_point_unlisten_begins(m.arg(0), m.arg(1), m.arg(2)); return m.ret(12); }
RX_PORT(0x00419140, GraffitiPoint_UnlistenFailed, "rx_party", 0) { rx_graffiti_point_unlisten_failed(m.arg(0), m.arg(1), m.arg(2)); return m.ret(12); }
RX_PORT(0x004191b0, GraffitiPoint_UnlistenKickout, "rx_party", 0) { rx_graffiti_point_unlisten_kickout(m.arg(0), m.arg(1), m.arg(2)); return m.ret(12); }
RX_PORT(0x0041a0d0, EventProxy_PlayerFloat, "rx_party", 0) { MgFpuKeep k(m); rx_event_proxy_call_member_player_float(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x006c57d0, EventProxy_PlayerInt2, "rx_party", 0) { rx_event_proxy_call_member_player_int2(m.r[1], m.arg(0)); return m.ret(4); }
