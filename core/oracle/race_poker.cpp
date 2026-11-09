// race_poker.cpp -- oracle adapters for core/game/race_poker.c (group rx_party): the PokerRace party mode (MXActivityPokerRace), its
// PokerRaceDriver, RuleOfftrack, RaceDriver's vf16 / vf17 and PokerRaceActivityAudioLogic (vf47: build_activity_driver.c).
#include "race_oracle.h"
extern "C" {
#include "../game/race_poker.h"
}
using namespace reflex_port;

namespace {
struct PokerFpuKeep { reflex_vm::Machine& m; std::vector<reflex_f80::F80> fpu; explicit PokerFpuKeep(reflex_vm::Machine& mm) : m(mm), fpu(mm.fpu) {} ~PokerFpuKeep() { m.fpu = fpu; } };
}

// ---- helpers ----
RX_PORT(0x00571ca0, Poker_DrawCard, "rx_party", OUT_EAX) { m.r[0] = rx_poker_draw_card(m.r[7]); return m.ret(0); }
RX_PORT(0x00557760, Activity_DriverPlace, "rx_party", OUT_EAX) { m.r[0] = (uint32_t)rx_activity_driver_place(m.r[2], m.r[6]); return m.ret(0); }
RX_PORT(0x00589b80, Event_ActivityDriverAt, "rx_party", 0) { rx_event_activity_driver_at(m.r[6], m.r[2], m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00571c20, Poker_TrackAudioKind, "rx_party", OUT_EAX) { m.r[0] = rx_poker_track_audio_kind(m.r[7], m.r[6]); return m.ret(0); }

// ---- the activity ----
RX_PORT(0x00584020, PokerAudio_Construct, "rx_party", OUT_EAX) { m.r[0] = rx_poker_audio_logic_construct(m.r[6]); return m.ret(0); }
RX_PORT(0x00582d50, RaceAudio_Vf32, "rx_party", 0) { rx_race_audio_logic_vf32(m.r[1]); return m.ret(0); }
RX_PORT(0x00571000, Poker_Construct, "rx_party", OUT_EAX) { m.r[0] = rx_poker_race_construct(m.r[7], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00571070, Poker_Sdtor, "rx_party", OUT_EAX) { m.r[0] = rx_poker_race_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x005710a0, Poker_Vf2, "rx_party", 0) { rx_poker_race_vf2(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00571260, Poker_Vf3, "rx_party", 0) { rx_poker_race_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x005713e0, Poker_Vf4, "rx_party", 0) { rx_poker_race_vf4(m.r[1]); return m.ret(0); }
RX_PORT(0x00571420, Poker_Vf18, "rx_party", 0) { rx_poker_race_vf18(m.r[1]); return m.ret(0); }
RX_PORT(0x005715e0, Poker_Vf23, "rx_party", 0) { PokerFpuKeep k(m); rx_poker_race_vf23(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00571980, Poker_Vf27, "rx_party", 0) { PokerFpuKeep k(m); rx_poker_race_vf27(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x005717e0, Poker_Vf28, "rx_party", 0) { PokerFpuKeep k(m); rx_poker_race_vf28(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00571ac0, Poker_Vf29, "rx_party", 0) { PokerFpuKeep k(m); rx_poker_race_vf29(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x005719f0, Poker_Vf34, "rx_party", 0) { rx_poker_race_vf34(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00571500, Poker_Vf38, "rx_party", 0) { rx_poker_race_vf38(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x005715f0, Poker_Vf42, "rx_party", 0) { rx_poker_race_vf42(m.r[1]); return m.ret(0); }
RX_PORT(0x00571b60, Poker_Vf43, "rx_party", OUT_EAX) { m.r[0] = (uint32_t)rx_poker_race_vf43(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(12); }
RX_PORT(0x00571b30, Poker_Vf44, "rx_party", OUT_EAX) { m.r[0] = rx_poker_race_vf44(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x005714b0, Poker_Vf47, "rx_party", 0) { rx_poker_race_vf47(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00571d60, Poker_LapCompleted, "rx_party", 0) { rx_poker_lap_completed(m.r[1], m.arg(0), (int32_t)m.arg(1), m.arg(2), m.arg(3)); return m.ret(16); }
RX_PORT(0x005723c0, Poker_EvaluateDriver, "rx_party", 0) { rx_poker_evaluate_driver(m.arg(0), m.arg(1)); return m.ret(8); }

// ---- the driver ----
RX_PORT(0x005134c0, PokerDriver_Construct, "rx_party", OUT_EAX) { m.r[0] = rx_poker_driver_construct(m.r[6], m.r[0]); return m.ret(0); }
RX_PORT(0x00513560, PokerDriver_Vf1, "rx_party", 0) { rx_poker_driver_vf1(m.r[1]); return m.ret(0); }   /* (w-partyresults) jumps to RaceDriver vf1 / vf2 */
RX_PORT(0x00513570, PokerDriver_Vf2, "rx_party", 0) { rx_poker_driver_vf2(m.r[1]); return m.ret(0); }
RX_PORT(0x00513580, PokerDriver_Vf3, "rx_party", 0) { rx_poker_driver_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x005135e0, PokerDriver_Vf7, "rx_party", 0) { PokerFpuKeep k(m); rx_poker_driver_vf7(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00513630, PokerDriver_Pick, "rx_party", 0) { rx_poker_driver_pick(m.r[6]); return m.ret(0); }
RX_PORT(0x00512bc0, RaceDriver_Vf16, "rx_party", 0) { rx_race_driver_vf16(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00512c30, RaceDriver_Vf17, "rx_party", 0) { rx_race_driver_vf17(m.r[1]); return m.ret(0); }

// ---- RuleOfftrack ----
RX_PORT(0x0051ab70, RuleOfftrack_Vf1, "rx_party", 0) { rx_rule_offtrack_vf1(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0051aba0, RuleOfftrack_Vf4, "rx_party", 0) { rx_rule_offtrack_vf4(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0051abc0, RuleOfftrack_Vf5, "rx_party", 0) { PokerFpuKeep k(m); rx_rule_offtrack_vf5(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }

// ---- the audio logic ----
RX_PORT(0x00584070, PokerAudio_Vf3, "rx_party", 0) { rx_poker_audio_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x00584230, PokerAudio_Vf4, "rx_party", 0) { rx_poker_audio_vf4(m.r[1]); return m.ret(0); }
RX_PORT(0x0058c930, PokerAudio_ListenDriver, "rx_party", 0) { rx_poker_audio_listen_driver(m.r[1], m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0058c9f0, PokerAudio_ListenPlayerWins, "rx_party", 0) { rx_poker_audio_listen_player_wins(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058cab0, PokerAudio_UnlistenDriver, "rx_party", 0) { rx_poker_audio_unlisten_driver(m.r[1], m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0058cb10, PokerAudio_UnlistenPlayerWins, "rx_party", 0) { rx_poker_audio_unlisten_player_wins(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x005843f0, PokerAudio_Flop, "rx_party", 0) { rx_poker_audio_flop(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00584410, PokerAudio_CardAvailable, "rx_party", 0) { rx_poker_audio_card_available(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00584430, PokerAudio_CardSelected, "rx_party", 0) { rx_poker_audio_card_selected(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }

// ---- the listeners (EAX = channel; stdcall act) ----
RX_PORT(0x00589590, Poker_LapListen, "rx_party", 0) { rx_poker_lap_listen(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00589650, Poker_NetRemovedListen, "rx_party", 0) { rx_poker_net_removed_listen(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00589710, Poker_NetCommunityListen, "rx_party", 0) { rx_poker_net_community_listen(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x005897d0, Poker_NetHandListen, "rx_party", 0) { rx_poker_net_hand_listen(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00589890, Poker_NetChoiceListen, "rx_party", 0) { rx_poker_net_choice_listen(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00589950, Poker_LapUnlisten, "rx_party", 0) { rx_poker_lap_unlisten(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x005899b0, Poker_NetRemovedUnlisten, "rx_party", 0) { rx_poker_net_removed_unlisten(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00589a10, Poker_NetCommunityUnlisten, "rx_party", 0) { rx_poker_net_community_unlisten(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00589a70, Poker_NetHandUnlisten, "rx_party", 0) { rx_poker_net_hand_unlisten(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00589ad0, Poker_NetChoiceUnlisten, "rx_party", 0) { rx_poker_net_choice_unlisten(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00571d40, Poker_NetObjectRemoved, "rx_party", 0) { rx_poker_net_object_removed(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }

// ---- the hands and the rule's helpers ----
RX_PORT(0x00572360, Poker_Sort5, "rx_party", 0) { rx_poker_sort5(m.r[1]); return m.ret(0); }
RX_PORT(0x00572720, Poker_RankHand, "rx_party", 0) { rx_poker_rank_hand(m.r[1], m.r[0], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4), m.arg(5)); return m.ret(24); }
RX_PORT(0x0051ae90, RuleOfftrack_Left, "rx_party", 0) { rx_rule_offtrack_left(m.r[2]); return m.ret(0); }
RX_PORT(0x0051af60, RuleOfftrack_PutBack, "rx_party", 0) { rx_rule_offtrack_put_back(m.r[0]); return m.ret(0); }
