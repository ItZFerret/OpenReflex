// fx_stunt.cpp -- oracle adapters for core/game/fx_stunt.c (group "rx_fx").
#include "fx_oracle.h"
extern "C" {
#include "../game/fx.h"
#include "../game/fx_stunt_input.h"
}
using namespace reflex_port;
using rx_oracle::f32;
using rx_oracle::bits;

FX_PORT(0x006cd000, StuntKindName_006cd000, OUT_EAX | OUT_ECX) { m.r[0] = rx_fx_stunt_kind_name(m.r[1]); m.r[1] -= 1; return m.ret(0); }
FX_PORT(0x006ca070, StuntKindTime_006ca070, OUT_EAX | OUT_XMM0) {
    m.xmm[0].lo = bits(rx_fx_stunt_kind_time(m.r[0])); m.xmm[0].hi = 0; m.r[0] -= 1; return m.ret(0);
}
FX_PORT(0x006c8570, TrickFind_006c8570, OUT_EAX) { m.r[0] = rx_fx_trick_find(m.arg(0), m.arg(1)); return m.ret(0); }
FX_PORT(0x006cd750, TrickEvent_006cd750, 0) { rx_fx_trick_event(m.arg(0), m.r[1], m.r[2]); return m.ret(4); }
FX_PORT(0x006cd810, StuntEvent_006cd810, 0) { rx_fx_stunt_event(m.arg(0), m.arg(1), m.r[1], m.r[2]); return m.ret(8); }
FX_PORT(0x006ca030, StuntEnd_006ca030, 0) { rx_fx_stunt_end(m.r[0]); return m.ret(0); }
FX_PORT(0x006c9fc0, StuntBegin_006c9fc0, 0) { rx_fx_stunt_begin(m.r[6], m.r[2]); return m.ret(0); }
FX_PORT(0x006c9ed0, StuntWanted_006c9ed0, OUT_EAX) { m.r[0] = rx_fx_stunt_wanted(m.arg(0)); return m.ret(4); }
FX_PORT(0x006ca0c0, StuntUpdate_006ca0c0, 0) { rx_fx_stunt_update(f32(m.arg(0)), m.r[0]); return m.ret(4); }
FX_PORT(0x006c9480, TrickFollowup_006c9480, 0) { rx_fx_trick_followup(m.r[6]); return m.ret(0); }
FX_PORT(0x006c9250, TrickLand_006c9250, 0) { rx_fx_trick_land(m.r[6]); return m.ret(0); }
FX_PORT(0x006c9380, TrickJump_006c9380, 0) { rx_fx_trick_jump(f32(m.arg(0)), m.r[0]); return m.ret(4); }
FX_PORT(0x006c9190, TrickAbort_006c9190, 0) { rx_fx_trick_abort(m.r[6]); return m.ret(0); }
FX_PORT(0x006c9940, TrickGroundOk_006c9940, OUT_EAX) { m.r[0] = rx_fx_trick_ground_ok(m.r[0]); return m.ret(0); }
FX_PORT(0x006c9b00, TrickGround_006c9b00, 0) { rx_fx_trick_ground(m.r[0]); return m.ret(0); }
FX_PORT(0x006c9510, TrickInput_006c9510, 0) { rx_fx_trick_input(m.arg(0), f32(m.arg(1))); return m.ret(8); }
FX_PORT(0x006c9770, TrickUpdate_006c9770, 0) { rx_fx_trick_update(f32(m.arg(0)), m.r[0]); return m.ret(4); }
FX_PORT(0x006cbad0, TricksLanded_006cbad0, OUT_EAX) { m.r[0] = rx_fx_tricks_landed(m.r[7]); return m.ret(0); }
FX_PORT(0x006cc670, StuntVariety_006cc670, 0) { rx_fx_stunt_variety(m.r[6]); return m.ret(0); }
FX_PORT(0x006cc4b0, StuntRatings_006cc4b0, 0) { rx_fx_stunt_ratings(m.r[0]); return m.ret(0); }
FX_PORT(0x006cb9b0, TricksUnique_006cb9b0, OUT_EAX) { m.r[0] = rx_fx_tricks_unique(m.r[7]); return m.ret(0); }
FX_PORT(0x006cc830, StuntRatingE_006cc830, 0) { rx_fx_stunt_rating_e(m.r[0]); return m.ret(0); }
FX_PORT(0x006cc740, StuntRatingA_006cc740, 0) { rx_fx_stunt_rating_a(m.r[0]); return m.ret(0); }
FX_PORT(0x006cc8b0, StuntRefresh_006cc8b0, 0) { rx_fx_stunt_refresh(m.r[6]); return m.ret(0); }
FX_PORT(0x006d83d0, HighlightRecover_006d83d0, OUT_FPU) { rx_fx_highlight_recover(m.r[0], m.r[6]); return m.ret(0); }
FX_PORT(0x006cae70, StuntCombo_006cae70, 0) { rx_fx_stunt_combo(m.r[0]); return m.ret(0); }
FX_PORT(0x006cb720, StuntWreck_006cb720, 0) { rx_fx_stunt_wreck(m.r[6]); return m.ret(0); }
FX_PORT(0x00588b50, StuntScoreDrop_00588b50, 0) { rx_fx_stunt_score_drop(m.arg(0), m.r[0]); return m.ret(4); }
FX_PORT(0x006cab80, StuntScore_006cab80, 0) { rx_fx_stunt_score(f32(m.arg(0)), m.r[0]); return m.ret(4); }
FX_PORT(0x006cc040, StuntComboFinish_006cc040, 0) { rx_fx_stunt_combo_finish(m.r[6]); return m.ret(0); }
FX_PORT(0x006caaf0, StuntComboEnd_006caaf0, 0) { rx_fx_stunt_combo_end(m.r[1]); return m.ret(0); }
FX_PORT(0x006cd870, StuntFinalScore_006cd870, 0) { rx_fx_stunt_final_score(m.r[0], rx_oracle::f32(uint32_t(m.xmm[0].lo))); return m.ret(0); }
FX_PORT(0x006cc0d0, StuntLanded_006cc0d0, 0) { rx_fx_stunt_landed(m.r[1], m.arg(0)); return m.ret(4); }
FX_PORT(0x006cc1f0, StuntComboNew_006cc1f0, OUT_EAX) { m.r[0] = rx_fx_stunt_combo_new(m.r[0], m.r[7]); return m.ret(0); }
FX_PORT(0x006cc230, StuntVehicleEntered_006cc230, 0) { rx_fx_stunt_vehicle_entered(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
FX_PORT(0x006cb500, StuntBest_006cb500, OUT_EAX) { const uint32_t c = U32(m.r[1], 0x430); rx_fx_stunt_best(m.r[1], m.r[2], f32(m.arg(0))); m.r[0] = c; return m.ret(4); }
FX_PORT(0x006cd3f0, TrickMapGet_006cd3f0, OUT_EAX) { m.r[0] = rx_fx_trick_map_get(m.r[7], m.arg(0)); return m.ret(4); }
FX_PORT(0x006cb960, TrickCount_006cb960, OUT_EAX) { m.r[0] = rx_fx_trick_count(m.r[1], m.r[0]); return m.ret(0); }
FX_PORT(0x006cbbe0, StuntBegins_006cbbe0, 0) { rx_fx_stunt_begins(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
FX_PORT(0x006cbdb0, StuntEnds_006cbdb0, 0) { rx_fx_stunt_ends(m.r[1], m.arg(0), m.arg(1)); return m.ret(0xc); }
FX_PORT(0x006cb6a0, StuntComboLost_006cb6a0, 0) { rx_fx_stunt_combo_lost(m.r[1], m.arg(0)); return m.ret(8); }
FX_PORT(0x006ccbc0, TrickTableCount_006ccbc0, 0) { rx_fx_trick_table_count(m.r[0]); return m.ret(0); }
/* the stunt input mappings and the gesture matcher (fx_stunt_input.c, agent "stuntgesture") */
FX_PORT(0x006ca2f0, StuntMappingRecord_006ca2f0, OUT_EAX) {
    m.r[0] = rx_fx_stunt_mapping_record(m.r[1], m.arg(0), m.arg(1)); return m.ret(0);
}
FX_PORT(0x006ca3f0, StuntMappingLoad_006ca3f0, OUT_EAX) { m.r[0] = (uint32_t)rx_fx_stunt_mapping_load(m.arg(0)); return m.ret(0); }
FX_PORT(0x006ca5a0, StuntMappingsInit_006ca5a0, OUT_EAX) { m.r[0] = rx_fx_stunt_mappings_init(nullptr); return m.ret(0); }
FX_PORT(0x006c9aa0, StuntMappingMatch_006c9aa0, OUT_EAX) {
    m.r[0] = (uint32_t)rx_fx_stunt_mapping_match(m.arg(0), m.r[7]); return m.ret(4);
}
/* the gesture read: 00548810's ECX at the return is its callee's residue (a value-getter pointer; no switched caller reads it) */
FX_PORT(0x00548810, StuntGestureDir_00548810, OUT_FPU) { rx_fx_stunt_gesture_dir(m.r[0], m.arg(0)); return m.ret(0); }
FX_PORT(0x006cd2c0, StuntGestureClassify_006cd2c0, OUT_FPU) {
    rx_fx_stunt_gesture_classify(m.r[6], f32(m.arg(0)), f32(m.arg(1)), f32(m.arg(2)), m.arg(3)); return m.ret(0x10);
}
/* the trick start and the hold modes (agent "trickstart") */
FX_PORT(0x006cd050, TrickPerfExists_006cd050, OUT_EAX) { m.r[0] = rx_fx_trick_perf_exists(m.r[1], m.arg(0)); return m.ret(0); }
FX_PORT(0x006cd0f0, TrickAssetHandle_006cd0f0, OUT_EAX) { m.r[0] = (uint32_t)rx_fx_trick_asset_handle(m.r[1], m.arg(0)); return m.ret(0); }
FX_PORT(0x006cd1c0, TrickHoldTime_006cd1c0, OUT_XMM0) {
    const float v = rx_fx_trick_hold_time(m.r[1], m.r[0]);
    m.xmm[0].lo = bits(v); m.xmm[0].hi = 0; return m.ret(0);
}
FX_PORT(0x006c9a40, TrickMinReached_006c9a40, OUT_EAX) { m.r[0] = rx_fx_trick_min_reached(m.r[0]); return m.ret(0); }
FX_PORT(0x006c98c0, TrickStart_006c98c0, 0) { rx_fx_trick_start(m.r[6], m.r[7]); return m.ret(0); }
FX_PORT(0x006c9b60, TrickMode1_006c9b60, 0) { rx_fx_trick_mode1(m.r[6], f32(m.arg(0))); return m.ret(4); }
FX_PORT(0x006c9cc0, TrickMode2_006c9cc0, 0) { rx_fx_trick_mode2(m.r[6], f32(uint32_t(m.xmm[0].lo))); return m.ret(0); }
FX_PORT(0x006c9da0, TrickMode3_006c9da0, 0) { rx_fx_trick_mode3(m.r[6]); return m.ret(0); }
FX_PORT(0x006c9c20, TrickChain_006c9c20, 0) { rx_fx_trick_chain(m.r[0]); return m.ret(0); }
FX_PORT(0x006c9e80, TrickHoldBegin_006c9e80, 0) { rx_fx_trick_hold_begin(m.r[6]); return m.ret(0); }
FX_PORT(0x006c9eb0, TrickHoldEnd_006c9eb0, 0) { rx_fx_trick_hold_end(m.r[6]); return m.ret(0); }
FX_PORT(0x006c9df0, TrickComplete_006c9df0, 0) { rx_fx_trick_complete(m.r[0]); return m.ret(0); }
FX_PORT(0x006cd7b0, TrickSwitchEvent_006cd7b0, 0) { rx_fx_trick_switch_event(m.arg(0), m.r[1], m.r[2]); return m.ret(4); }
FX_PORT(0x006ccef0, StuntChainNote_006ccef0, 0) { rx_fx_stunt_chain_note(m.r[0], m.r[1]); return m.ret(0); }
FX_PORT(0x006cb1b0, StuntBeginEvt_006cb1b0, 0) { rx_fx_stunt_begin_evt(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
FX_PORT(0x006cb3c0, StuntCompleted_006cb3c0, 0) { rx_fx_stunt_completed(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
FX_PORT(0x006cb560, StuntComboFrom_006cb560, 0) { rx_fx_stunt_combo_from(m.r[1], m.arg(0), m.arg(2)); return m.ret(0xc); }
FX_PORT(0x006cb5a0, StuntLandedEvt_006cb5a0, 0) { rx_fx_stunt_landed_evt(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
FX_PORT(0x006cb610, StuntTakeoffEvt_006cb610, 0) { rx_fx_stunt_takeoff_evt(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
FX_PORT(0x006cb660, StuntKickOut_006cb660, 0) { rx_fx_stunt_kickout(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
FX_PORT(0x00581830, StuntAudioBegin_00581830, 0) { rx_fx_stunt_audio_begin(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
FX_PORT(0x00581950, StuntAudioComplete_00581950, 0) { rx_fx_stunt_audio_complete(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
