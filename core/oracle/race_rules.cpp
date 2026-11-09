// race_rules.cpp -- oracle adapters for core/game/race_rules.c.
#include "race_oracle.h"
extern "C" {
#include "../game/race.h"
}
using namespace reflex_port;

// 0051a810 (EAX = line, EDI = position, arg 0 = previous position; RET 4) -> EAX.
RX_PORT(0x0051a810, Race_LineCrossed, "rx_race", OUT_EAX) {
    m.r[0] = uint32_t(rx_race_line_crossed(m.r[0], m.r[7], m.arg(0)));
    return m.ret(4);
}
// 0051a7a0 RuleFinishLine::vf5 (ECX = rule; RET 4).
RX_PORT(0x0051a7a0, RuleFinishLine_Update, "rx_race", 0) {
    rx_rule_finish_line_update(m.r[1], rx_oracle::f32(m.arg(0)));
    return m.ret(4);
}
RX_PORT(0x0051a790, RuleFinishLine_Type, "rx_race", OUT_EAX) { m.r[0] = rx_rule_finish_line_type(m.r[1]); return m.ret(0); }
RX_PORT(0x0051b0e0, BehaviorBrakeTurn_Type, "rx_race", OUT_EAX) { m.r[0] = rx_behavior_brake_turn_type(m.r[1]); return m.ret(0); }

// Find-by-type (arg 0 = driver; RET 4) -> EAX.
#define RX_FINDER(addr, Name, fn) RX_PORT(addr, Name, "rx_race", OUT_EAX) { m.r[0] = fn(m.arg(0)); return m.ret(4); }
RX_FINDER(0x0053ae50, Driver_Rule0, rx_driver_rule_0)
RX_FINDER(0x0053ae00, Driver_Rule1, rx_driver_rule_1)
RX_FINDER(0x0053aaa0, Driver_OfftrackPenaltyRule, rx_driver_offtrack_penalty_rule)
RX_FINDER(0x0053baa0, Driver_Behavior1, rx_driver_behavior_1)
RX_FINDER(0x0053a310, Driver_NavigateBehavior, rx_driver_navigate_behavior)
RX_FINDER(0x0053a560, Driver_RoamBehavior, rx_driver_roam_behavior)
RX_FINDER(0x0053ac30, Driver_Behavior3, rx_driver_behavior_3)
RX_FINDER(0x0053abe0, Driver_Behavior5, rx_driver_behavior_5)
RX_FINDER(0x0053a510, Driver_AirLevelingBehavior, rx_driver_air_leveling_behavior)
RX_FINDER(0x0053ab40, Driver_FixProgressBehavior, rx_driver_fix_progress_behavior)
RX_FINDER(0x0053aaf0, Driver_LapPacingBehavior, rx_driver_lap_pacing_behavior)
RX_FINDER(0x0053ab90, Driver_SuckRacingBehavior, rx_driver_suck_racing_behavior)

// 0051d400 RuleTrackAdvance::vf5 (ECX = rule, arg 0 = dt; RET 4).
RX_PORT(0x0051d400, RuleTrackAdvance_Update, "rx_race", 0) {
    rx_rule_track_advance_update(m.r[1], rx_oracle::f32(m.arg(0)));
    return m.ret(4);
}
// 005501d0 (EAX = player) -> EAX.
RX_PORT(0x005501d0, Player_IsResetting, "rx_race", OUT_EAX) { m.r[0] = uint32_t(rx_player_is_resetting(m.r[0])); return m.ret(0); }
// 00519990 RuleBackwards::vf5 (ECX = rule, arg 0 = dt; RET 4).
RX_PORT(0x00519990, RuleBackwards_Update, "rx_race", 0) {
    race_check_pc53(m, "00519990"); rx_rule_backwards_update(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
// 00519ce0 (ECX = query) -> xmm0 (upper lanes 0).
RX_PORT(0x00519ce0, RuleBackwards_Distance, "rx_race", OUT_XMM0) {
    const float r = rx_rule_backwards_distance(m.r[1]);
    m.xmm[0].lo = rx_oracle::bits(r); m.xmm[0].hi = 0;
    return m.ret(0);
}
// 00519cb0 (EAX = rule).
RX_PORT(0x00519cb0, RuleBackwards_Reset, "rx_race", 0) { rx_rule_backwards_reset(m.r[0]); return m.ret(0); }
// 0053b670 (arg 0 = driver; RET 4) -> EAX.
RX_FINDER(0x0053b670, Driver_TrackAdvanceRule, rx_driver_track_advance_rule)
// 0051d890 (EAX = rule).
RX_PORT(0x0051d890, RuleTrackAdvance_SectionChanged, "rx_race", 0) { rx_rule_track_advance_section_changed(m.r[0]); return m.ret(0); }
// 0051b130 RuleOfftrackTimePenalty::vf5 (ECX = rule, arg 0 = dt; RET 4).
RX_PORT(0x0051b130, RuleOfftrack_Update, "rx_race", 0) { rx_rule_offtrack_update(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
static void xmm0_only(reflex_vm::Machine& m, float r) { m.xmm[0].lo = rx_oracle::bits(r); m.xmm[0].hi = 0; }
// 0051b690 (EAX = a, ESI = b, ECX = position near b, args section, position near a; RET 8) -> xmm0.
RX_PORT(0x0051b690, Track_DistanceBetween, "rx_race", OUT_XMM0) {
    xmm0_only(m, rx_track_distance_between(m.r[0], m.r[6], m.r[1], m.arg(0), m.arg(1)));
    return m.ret(8);
}
// 0051b890 (ESI = b, EAX = position near a, ECX = section, args rule, position near b, a; RET 0xc) -> xmm0.
RX_PORT(0x0051b890, Offtrack_ShortcutPenalty, "rx_race", OUT_XMM0) {
    xmm0_only(m, rx_offtrack_shortcut_penalty(m.arg(0), m.arg(1), m.arg(2), m.r[6], m.r[0], m.r[1]));
    return m.ret(0xc);
}
// 0051b9e0 (EAX = from, ECX = position, args rule, section, to; RET 0xc) -> EAX.
RX_PORT(0x0051b9e0, Offtrack_CountMarkers, "rx_race", OUT_EAX) {
    m.r[0] = uint32_t(rx_offtrack_count_markers(m.arg(0), m.r[0], m.r[1], m.arg(1), m.arg(2)));
    return m.ret(0xc);
}
RX_PORT(0x0051b080, Offtrack_Reset, "rx_race", 0) { rx_offtrack_reset(m.r[7]); return m.ret(0); }
RX_PORT(0x0051b8f0, Offtrack_LeftTrack, "rx_race", 0) { rx_offtrack_left_track(m.r[0]); return m.ret(0); }
RX_PORT(0x0051b010, RuleOfftrack_ResetState, "rx_race", OUT_ECX) { rx_rule_offtrack_reset_state(m.r[0]); m.r[1] = 0; return m.ret(0); }
RX_PORT(0x00552870, Player_IsLocalHuman, "rx_race", OUT_EAX) { m.r[0] = uint32_t(rx_player_is_local_human(m.r[0])); return m.ret(0); }
// 00519fa0 (EAX = position, EDI = out point or 0) -> EAX.
RX_PORT(0x00519fa0, Cannonball_Inside, "rx_race", OUT_EAX) { m.r[0] = uint32_t(rx_cannonball_inside(m.r[0], m.r[7])); return m.ret(0); }
RX_PORT(0x0051a300, RuleCannonball_Update, "rx_race", 0) { rx_rule_cannonball_update(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_FINDER(0x00554c90, Driver_Rule7, rx_driver_rule_7)
// 0051a110 (EAX = position, arg 0 = out point or 0; cdecl) -> EAX.
RX_PORT(0x0051a110, Cannonball_Exit, "rx_race", OUT_EAX) { m.r[0] = uint32_t(rx_cannonball_exit(m.r[0], m.arg(0))); return m.ret(0); }
// 0051a690 (EDI = rule, EBP = the caller's).
RX_PORT(0x0051a690, Cannonball_Enter, "rx_race", 0) { rx_cannonball_enter(m.r[7], m.r[5]); return m.ret(0); }
// 00519ed0 (ECX = fade state) -> EAX.
RX_PORT(0x00519ed0, Rule_FadeLayer, "rx_race", OUT_EAX) { m.r[0] = rx_fade_layer(m.r[1]); return m.ret(0); }
// 0051d8c0 (ESI = rule) -> xmm0 (upper lanes 0).
RX_PORT(0x0051d8c0, RuleTrackAdvance_Distance, "rx_race", OUT_XMM0) { xmm0_only(m, rx_track_advance_distance(m.r[6])); return m.ret(0); }
// 0054f150 (ECX = player, EDI = point).
RX_PORT(0x0054f150, Player_Launch, "rx_race", 0) { rx_player_launch(m.r[1], m.r[7]); return m.ret(0); }

// race_sound.c: the rules' sound calls.
// 0080ae10 (ESI = event), 0080ade0 (EAX = event, EBP = the caller's), 0080b410 (ESI = event, xmm0 = volume, EBP), 00807130 (EAX = channel,
// xmm0 = volume), 00807170 (ECX = channel, arg 0 = fade time; RET 4), 00581350 / 00581490 (cdecl out).
RX_PORT(0x0080ae10, Sound_StopNow, "rx_race", 0) { rx_sound_stop_now(m.r[6]); return m.ret(0); }
RX_PORT(0x0080ade0, Sound_Stop, "rx_race", 0) { rx_sound_stop(m.r[0], m.r[5]); return m.ret(0); }
RX_PORT(0x0080b410, Sound_SetVolume, "rx_race", 0) { rx_sound_set_volume(m.r[6], rx_oracle::f32(uint32_t(m.xmm[0].lo)), m.r[5]); return m.ret(0); }
RX_PORT(0x00807130, Channel_SetVolume, "rx_race", 0) { rx_channel_set_volume(m.r[0], rx_oracle::f32(uint32_t(m.xmm[0].lo))); return m.ret(0); }
RX_PORT(0x00807170, Channel_Fade, "rx_race", 0) { rx_channel_fade(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00581350, Sound_WorldName, "rx_race", 0) { rx_sound_world_name(m.arg(0)); return m.ret(0); }
RX_PORT(0x00581490, Sound_WorldPath, "rx_race", 0) { rx_sound_world_path(m.arg(0)); return m.ret(0); }
