// race_beh1.cpp -- oracle adapters for core/game/race_beh1_*.c (the AI behaviours, part 1).
#include "oracle.h"
#include "f80/f80.h"
extern "C" {
#include "../game/race_beh1.h"
}
using namespace reflex_port;
using rx_oracle::f32;
using rx_oracle::bits;

static inline float xmm0f(reflex_vm::Machine& m) { return f32(uint32_t(m.xmm[0].lo)); }
static inline void set_xmm0f(reflex_vm::Machine& m, float v) { m.xmm[0].lo = (m.xmm[0].lo & ~0xffffffffull) | bits(v); }

// ---- BehaviorHandleBraking (race_beh1_brake.c)
RX_PORT(0x005248f0, BehaviorHandleBraking_Type, "rx_race", OUT_EAX) { m.r[0] = rx_behavior_handle_braking_type(m.r[1]); return m.ret(0); }
RX_PORT(0x005248d0, BehaviorHandleBraking_Reset, "rx_race", 0) { rx_behavior_handle_braking_reset(m.r[1]); return m.ret(0); }
// 00524ce0 (EAX = side, stack behaviour; RET 4) -> xmm0
RX_PORT(0x00524ce0, BehaviorHandleBraking_Cast, "rx_race", OUT_XMM0) {
    m.xmm[0].lo = bits(rx_behavior_braking_cast(int(m.r[0]), m.arg(0))); m.xmm[0].hi = 0;   // movss load / sqrtss / divss: upper lanes 0
    return m.ret(4);
}
// 00524bb0 (EDI = behaviour) -> st(0)
RX_PORT(0x00524bb0, BehaviorHandleBraking_Mean, "rx_race", OUT_FPU) {
    const rx_f80 r = rx_behavior_braking_mean(m.r[7]);
    reflex_f80::F80 v; v.kind = r.kind; v.sign = r.sign; v.mant = r.mant; v.exp = r.exp;
    m.fpu.push_back(v);
    return m.ret(0);
}
// 00524c50 (xmm0 = fraction, EDX = behaviour)
RX_PORT(0x00524c50, BehaviorHandleBraking_Apply, "rx_race", 0) { rx_behavior_braking_apply(xmm0f(m), m.r[2]); return m.ret(0); }
// 005249a0 (EAX = behaviour)
RX_PORT(0x005249a0, BehaviorHandleBraking_CastStep, "rx_race", 0) { rx_behavior_braking_cast_step(m.r[0]); return m.ret(0); }
// 00524a40 / 00524af0 (EAX = behaviour): the mode-1 / mode-2 cast cycles, no results
RX_PORT(0x00524a40, BehaviorHandleBraking_CastCenter, "rx_race", 0) { rx_behavior_braking_cast_center(m.r[0]); return m.ret(0); }
RX_PORT(0x00524af0, BehaviorHandleBraking_CastPast, "rx_race", 0) { rx_behavior_braking_cast_past(m.r[0]); return m.ret(0); }
// 00524900 vf4 (ECX = behaviour, stack dt; RET 4) -> EAX
RX_PORT(0x00524900, BehaviorHandleBraking_Update, "rx_race", OUT_EAX) {
    m.r[0] = rx_behavior_handle_braking_update(m.r[1], f32(m.arg(0)));
    return m.ret(4);
}

// ---- race_beh1_small.c
#define B1_VF4(addr, Name, fn) \
    RX_PORT(addr, Name, "rx_race", OUT_EAX) { m.r[0] = fn(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x005223b0, BehaviorAvoidWreck_Type, "rx_race", OUT_EAX) { m.r[0] = rx_behavior_avoid_wreck_type(m.r[1]); return m.ret(0); }
RX_PORT(0x00522390, BehaviorAvoidWreck_Reset, "rx_race", 0) { rx_behavior_avoid_wreck_reset(m.r[1]); return m.ret(0); }
B1_VF4(0x005223e0, BehaviorAvoidWreck_Update, rx_behavior_avoid_wreck_update)
RX_PORT(0x00529140, BehaviorOfftrack_Type, "rx_race", OUT_EAX) { m.r[0] = rx_behavior_offtrack_type(m.r[1]); return m.ret(0); }
RX_PORT(0x00529150, BehaviorOfftrack_Watch, "rx_race", 0) { rx_behavior_offtrack_watch(m.r[0]); return m.ret(0); }
// 00529190 (ESI = behaviour)
RX_PORT(0x00529190, BehaviorOfftrack_Aim, "rx_race", 0) { rx_behavior_offtrack_aim(m.r[6]); return m.ret(0); }
// 00529330 (ECX = behaviour, stack behaviour; RET 4)
RX_PORT(0x00529330, BehaviorOfftrack_Return, "rx_race", 0) { rx_behavior_offtrack_return(m.arg(0)); return m.ret(4); }
B1_VF4(0x005298b0, BehaviorOfftrack_Update, rx_behavior_offtrack_update)
B1_VF4(0x00522ef0, BehaviorBrakeTurn_Update, rx_behavior_brake_turn_update)
// 00522640 (stack behaviour; RET 4)
RX_PORT(0x00522640, BehaviorBlueYellowFlag_Find, "rx_race", 0) { rx_behavior_blue_flag_find(m.arg(0)); return m.ret(4); }
// 00522a70 (ESI = behaviour)
RX_PORT(0x00522a70, BehaviorBlueYellowFlag_Move, "rx_race", 0) { rx_behavior_blue_flag_move(m.r[6]); return m.ret(0); }
B1_VF4(0x005225c0, BehaviorBlueYellowFlag_Update, rx_behavior_blue_flag_update)
// 00526b40 (EAX = node, xmm0 = distance, ESI = behaviour) -> EAX
RX_PORT(0x00526b40, BehaviorMonitorCorner_Find, "rx_race", OUT_EAX) {
    m.r[0] = rx_behavior_corner_find(m.r[0], xmm0f(m), m.r[6]);
    return m.ret(0);
}
// 00526dd0 (EAX = object with the driver at +8, stack side; RET 4) -> EAX
RX_PORT(0x00526dd0, BehaviorNodeSide, "rx_race", OUT_EAX) {
    m.r[0] = rx_behavior_node_side(m.r[0], int32_t(m.arg(0)));
    return m.ret(4);
}
// 00526a30 (EDI = behaviour)
RX_PORT(0x00526a30, BehaviorMonitorCorner_Watch, "rx_race", 0) { rx_behavior_monitor_corner_watch(m.r[7]); return m.ret(0); }
// 00526ab0 (EAX = behaviour) / 00526b00 (ECX = behaviour): states 1 / 2; no results the caller reads (00526970 returns 0 itself)
RX_PORT(0x00526ab0, BehaviorMonitorCorner_Wait, "rx_race", 0) { rx_behavior_monitor_corner_wait(m.r[0]); return m.ret(0); }
RX_PORT(0x00526b00, BehaviorMonitorCorner_Hold, "rx_race", 0) { rx_behavior_monitor_corner_hold(m.r[1]); return m.ret(0); }
B1_VF4(0x00526970, BehaviorMonitorCorner_Update, rx_behavior_monitor_corner_update)
B1_VF4(0x00527300, BehaviorNavigateTrack_Update, rx_behavior_navigate_track_update)
B1_VF4(0x00529bf0, BehaviorOverrideSpline_Update, rx_behavior_override_spline_update)
RX_PORT(0x00529ba0, BehaviorOverrideSpline_Reset, "rx_race", 0) { rx_behavior_override_spline_reset(m.r[1]); return m.ret(0); }

// ---- race_beh1_pace.c
RX_PORT(0x005254e0, BehaviorLapPacing_Reset, "rx_race", 0) { rx_behavior_lap_pacing_reset(m.r[1]); return m.ret(0); }
RX_PORT(0x00525790, BehaviorLapPacing_NewLap, "rx_race", 0) { rx_behavior_lap_pacing_new_lap(m.r[0]); return m.ret(0); }
RX_PORT(0x00525ac0, BehaviorLapPacing_Classify, "rx_race", 0) { rx_behavior_lap_pacing_classify(m.r[0]); return m.ret(0); }
// 00525930 (ESI = behaviour) -> EAX
RX_PORT(0x00525930, BehaviorLapPacing_Suspended, "rx_race", OUT_EAX) { m.r[0] = rx_behavior_lap_pacing_suspended(m.r[6]); return m.ret(0); }
RX_PORT(0x00525b30, BehaviorLapPacing_SpeedUp, "rx_race", 0) { rx_behavior_lap_pacing_speed_up(m.r[0]); return m.ret(0); }
// 005256d0 (EAX = behaviour, xmm1 = gap).
RX_PORT(0x005256d0, BehaviorLapPacing_Gap, "rx_race", 0) { rx_behavior_lap_pacing_gap(m.r[0], rx_oracle::f32(uint32_t(m.xmm[1].lo))); return m.ret(0); }
// 00525d60 (xmm0 = fraction, arg 0 = behaviour; RET 4).
RX_PORT(0x00525d60, BehaviorLapPacing_Start, "rx_race", 0) { rx_behavior_lap_pacing_start(m.arg(0), rx_oracle::f32(uint32_t(m.xmm[0].lo))); return m.ret(4); }
B1_VF4(0x005257e0, BehaviorLapPacing_Update, rx_behavior_lap_pacing_update)

// ---- race_beh1_bounds.c
// 00521b40 / 00521ee0 (EAX = behaviour, args: edge start, edge end; RET 8) -> xmm0 (upper lanes 0: movss loads)
RX_PORT(0x00521b40, BehaviorAvoidBoundaries_Push, "rx_race", OUT_XMM0) {
    m.xmm[0].lo = bits(rx_behavior_boundary_push(m.r[0], m.arg(0), m.arg(1))); m.xmm[0].hi = 0;
    return m.ret(8);
}
RX_PORT(0x00521ee0, BehaviorAvoidBoundaries_PushMirrored, "rx_race", OUT_XMM0) {
    m.xmm[0].lo = bits(rx_behavior_boundary_push_mirrored(m.r[0], m.arg(0), m.arg(1))); m.xmm[0].hi = 0;
    return m.ret(8);
}
B1_VF4(0x00522280, BehaviorAvoidBoundaries_Update, rx_behavior_avoid_boundaries_update)

// ---- race_beh1_progress.c
B1_VF4(0x00523570, BehaviorFixForwardProgress_Update, rx_behavior_fix_progress_update)
// 00524050 (EAX = behaviour) -> EAX: the race-type test behind +0x60
RX_PORT(0x00524050, BehaviorFixForwardProgress_RaceType, "rx_race", OUT_EAX) {
    m.r[0] = rx_behavior_fix_progress_race_type(m.r[0]);
    return m.ret(0);
}
// 00523a70 (ECX = position, EDX = side, stack behaviour; RET 4) -> EAX: the steer-away manoeuvre
RX_PORT(0x00523a70, BehaviorFixForwardProgress_SteerAway, "rx_race", OUT_EAX) {
    m.r[0] = rx_behavior_fix_progress_steer_away(m.arg(0), m.r[1], m.r[2]);
    return m.ret(4);
}
// 00523ca0 (EAX = position, ESI = behaviour, stack side; RET 4) -> EAX: the back-up manoeuvre
RX_PORT(0x00523ca0, BehaviorFixForwardProgress_BackUp, "rx_race", OUT_EAX) {
    m.r[0] = rx_behavior_fix_progress_back_up(m.r[6], m.r[0], m.arg(0));
    return m.ret(4);
}
// 00523ed0 / 005240d0 (ESI = behaviour) -> EAX: the second manoeuvre's side / the type-6 proximity test
RX_PORT(0x00523ed0, BehaviorFixForwardProgress_Side, "rx_race", OUT_EAX) { m.r[0] = rx_behavior_fix_progress_side(m.r[6]); return m.ret(0); }
RX_PORT(0x005240d0, BehaviorFixForwardProgress_Near, "rx_race", OUT_EAX) { m.r[0] = rx_behavior_fix_progress_near(m.r[6]); return m.ret(0); }

// ---- race_beh1_nav.c
// 00528d50 (EDI = behaviour, args when_airborne, force; RET 8)
RX_PORT(0x00528d50, BehaviorNavigateTrack_PickSide, "rx_race", 0) {
    rx_behavior_navigate_pick_side(m.r[7], m.arg(0), m.arg(1));
    return m.ret(8);
}
// 00528940 (EDI = behaviour, EDX = node, stack side; RET 4) -> xmm0 (upper lanes: xorps / sqrtss of a movss-loaded value: 0)
RX_PORT(0x00528940, BehaviorNavigateTrack_LineOffset, "rx_race", OUT_XMM0) {
    m.xmm[0].lo = bits(rx_behavior_navigate_line_offset(m.r[7], m.r[2], int32_t(m.arg(0)))); m.xmm[0].hi = 0;
    return m.ret(4);
}
// 00527360 (EAX = behaviour)
RX_PORT(0x00527360, BehaviorNavigateTrack_Follow, "rx_race", 0) { rx_behavior_navigate_follow(m.r[0]); return m.ret(0); }
// 00528bd0 (EAX = corner direction, EDX = node, ESI = behaviour; args side, random; RET 8) -> xmm0
RX_PORT(0x00528bd0, BehaviorNavigateTrack_CornerOffset, "rx_race", OUT_XMM0) {
    m.xmm[0].lo = bits(rx_behavior_navigate_corner_offset(m.r[6], m.r[2], int32_t(m.r[0]), int32_t(m.arg(0)), f32(m.arg(1))));
    m.xmm[0].hi = 0;
    return m.ret(8);
}
// 00528a70 (EAX = the side argument, EDX = node, ESI = behaviour; args side, random; RET 8) -> xmm0
RX_PORT(0x00528a70, BehaviorNavigateTrack_ApexOffset, "rx_race", OUT_XMM0) {
    m.xmm[0].lo = bits(rx_behavior_navigate_apex_offset(m.r[6], m.r[2], int32_t(m.r[0]), int32_t(m.arg(0)), f32(m.arg(1))));
    m.xmm[0].hi = 0;
    return m.ret(8);
}
// 005278e0 / 00527fc0 (stack behaviour; RET 4)
RX_PORT(0x005278e0, BehaviorNavigateTrack_Line, "rx_race", 0) { rx_behavior_navigate_line(m.arg(0)); return m.ret(4); }
RX_PORT(0x00527fc0, BehaviorNavigateTrack_Apex, "rx_race", 0) { rx_behavior_navigate_apex(m.arg(0)); return m.ret(4); }
RX_PORT(0x00529c40, BehaviorOverrideSpline_Wait, "rx_race", OUT_EAX) { m.r[0] = rx_behavior_override_spline_wait(m.r[0]); return m.ret(0); }
RX_PORT(0x00529ce0, BehaviorOverrideSpline_Enter, "rx_race", OUT_EAX) { m.r[0] = rx_behavior_override_spline_enter(m.r[0]); return m.ret(0); }
RX_PORT(0x00529eb0, BehaviorOverrideSpline_Follow, "rx_race", OUT_EAX) { m.r[0] = rx_behavior_override_spline_follow(m.r[0]); return m.ret(0); }

// ---- race_beh1_air.c
// 0050ed70 (EAX = out, ECX = gate rule): two x87 copies.
RX_PORT(0x0050ed70, GateRule_Target, "rx_race", 0) { rx_gate_rule_target(m.r[0], m.r[1]); return m.ret(0); }

// 0051fea0 (ECX = normal out, EDX = point out, args: record, time out; RET 8)
RX_PORT(0x0051fea0, BehaviorAirLeveling_CopyPrediction, "rx_race", 0) {
    rx_air_prediction_copy(m.r[1], m.r[2], m.arg(0), m.arg(1));
    return m.ret(8);
}
// 0051fe00 (ESI = record, stack dt; returns through LeaveCriticalSection: RET 4, the argument slot = the critical section)
RX_PORT(0x0051fe00, BehaviorAirLeveling_UpdatePrediction, "rx_race", 0) {
    rx_air_prediction_update(m.r[6], f32(m.arg(0)));
    m.mem.write(m.r[4] + 4, 4, 0x00ae1c80u);
    return m.ret(4);
}
RX_PORT(0x0051ff80, BehaviorAirLeveling_SchedulePrediction, "rx_race", 0) { rx_air_prediction_schedule(); return m.ret(0); }
B1_VF4(0x0051f480, BehaviorAirLeveling_Update, rx_behavior_air_leveling_update)
B1_VF4(0x00520050, BehaviorAvoidFlipBack_Update, rx_behavior_avoid_flip_back_update)
