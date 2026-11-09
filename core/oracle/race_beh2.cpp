// race_beh2.cpp -- oracle adapters for core/game/race_beh2_*.c (the AI behaviours of 0x0052a000-0x00535fff).
#include "oracle.h"
extern "C" {
#include "../game/race_beh2.h"
}
using namespace reflex_port;
using rx_oracle::f32;
using rx_oracle::bits;

// xmm0 = (v, 0, 0, 0): both float returns end with xorps / a movss load.
static inline void set_xmm0(reflex_vm::Machine& m, float v) { m.xmm[0].lo = bits(v); m.xmm[0].hi = 0; }

// --- BehaviorSeperate
// 00531940 BehaviorSeperate::vf4 (ECX = behaviour; RET 4) -> EAX 0.
RX_PORT(0x00531940, BehaviorSeperate_Update, "rx_race", OUT_EAX) { m.r[0] = rx_behavior_seperate_update(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
// 005318f0 BehaviorSeperate::vf6 (ECX) -> EAX 0.
RX_PORT(0x005318f0, BehaviorSeperate_Reset, "rx_race", OUT_EAX) { m.r[0] = rx_behavior_seperate_reset(m.r[1]); return m.ret(0); }
// 00531a70 (EDI = behaviour).
RX_PORT(0x00531a70, Seperate_PickTarget, "rx_race", 0) { rx_seperate_pick_target(m.r[7]); return m.ret(0); }
// 00531c20 (EDI = behaviour).
RX_PORT(0x00531c20, Seperate_ChooseSide, "rx_race", 0) { rx_seperate_choose_side(m.r[7]); return m.ret(0); }
// 005321f0 (ESI = behaviour) -> EAX.
RX_PORT(0x005321f0, Seperate_TargetClear, "rx_race", OUT_EAX) { m.r[0] = uint32_t(rx_seperate_target_clear(m.r[6])); return m.ret(0); }
// 005322c0 (EDI = behaviour) -> EAX.
RX_PORT(0x005322c0, Seperate_KeepPassing, "rx_race", OUT_EAX) { m.r[0] = uint32_t(rx_seperate_keep_passing(m.r[7])); return m.ret(0); }
// 00532910 (EDI = behaviour).
RX_PORT(0x00532910, Seperate_AvoidRiders, "rx_race", 0) { rx_seperate_avoid_riders(m.r[7]); return m.ret(0); }
// 00532dd0 (EAX = behaviour).
RX_PORT(0x00532dd0, Seperate_Follow, "rx_race", 0) { rx_seperate_follow(m.r[0]); return m.ret(0); }
// 00532fd0 (ECX = behaviour).
RX_PORT(0x00532fd0, Seperate_CheckHold, "rx_race", 0) { rx_seperate_check_hold(m.r[1]); return m.ret(0); }
// 00533070 (args: behaviour, other, max time, max move; RET 0x10) -> xmm0.
RX_PORT(0x00533070, Seperate_ContactTime, "rx_race", OUT_XMM0) {
    set_xmm0(m, rx_seperate_contact_time(m.arg(0), m.arg(1), f32(m.arg(2)), f32(m.arg(3))));
    return m.ret(0x10);
}
// 005332f0 (ECX = other, args: behaviour, max time; RET 8) -> xmm0.
RX_PORT(0x005332f0, Seperate_CrossingTime, "rx_race", OUT_XMM0) {
    set_xmm0(m, rx_seperate_crossing_time(m.r[1], m.arg(0), f32(m.arg(1))));
    return m.ret(8);
}
// 00533510 (EDI = vehicle, ESI = out centre, arg 0 = out radius; RET 4).
RX_PORT(0x00533510, Vehicle_BoundCentre, "rx_race", 0) { rx_vehicle_bound_centre(m.r[7], m.r[6], m.arg(0)); return m.ret(4); }

// --- BehaviorTakeLine / BehaviorSplitSection
RX_PORT(0x005344e0, BehaviorTakeLine_Update, "rx_race", OUT_EAX) { m.r[0] = rx_behavior_take_line_update(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00534470, BehaviorTakeLine_Ready, "rx_race", OUT_EAX) { m.r[0] = rx_behavior_take_line_ready(m.r[1]); return m.ret(0); }
RX_PORT(0x00533970, BehaviorSplitSection_Update, "rx_race", OUT_EAX) { m.r[0] = rx_behavior_split_section_update(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }

// --- BehaviorSuckRacing, OverrideSpline scans, Behavior::vf7
RX_PORT(0x00534ff0, BehaviorSuckRacing_Update, "rx_race", OUT_EAX) { m.r[0] = rx_behavior_suck_racing_update(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00534fe0, BehaviorSuckRacing_Type, "rx_race", OUT_EAX) { m.r[0] = rx_behavior_suck_racing_type(m.r[1]); return m.ret(0); }
RX_PORT(0x00534fa0, BehaviorSuckRacing_Reset, "rx_race", OUT_EAX) { m.r[0] = rx_behavior_suck_racing_reset(m.r[1]); return m.ret(0); }
// 00535090 (arg 0 = behaviour; RET 4).
RX_PORT(0x00535090, SuckRacing_Decide, "rx_race", 0) { rx_suck_racing_decide(m.arg(0)); return m.ret(4); }
// 00535480 (ESI = behaviour).
RX_PORT(0x00535480, SuckRacing_Hold, "rx_race", 0) { rx_suck_racing_hold(m.r[6]); return m.ret(0); }
// 005352c0 / 00535380 (ESI = behaviour); 00526c60 (EDI = behaviour) -> xmm0 (upper lanes 0), EDX as the original leaves it.
RX_PORT(0x005352c0, SuckRacing_Mode1, "rx_race", 0) { rx_suck_racing_mode1(m.r[6]); return m.ret(0); }
RX_PORT(0x00535380, SuckRacing_Mode2, "rx_race", 0) { rx_suck_racing_mode2(m.r[6]); return m.ret(0); }
RX_PORT(0x00526c60, Driver_RunDistance, "rx_race", OUT_XMM0 | OUT_EDX) {
    uint32_t edx; const float r = rx_driver_run_distance(m.r[7], &edx);
    m.xmm[0].lo = rx_oracle::bits(r); m.xmm[0].hi = 0; m.r[2] = edx;
    return m.ret(0);
}
// 0052a380 (ESI = behaviour, arg 0 = range; RET 4) -> EAX.
RX_PORT(0x0052a380, OverrideSpline_Scan, "rx_race", OUT_EAX) { m.r[0] = rx_override_spline_scan(m.r[6], f32(m.arg(0))); return m.ret(4); }
// 0052a6d0 (EDI = behaviour) -> EAX.
RX_PORT(0x0052a6d0, OverrideSpline_ScanStart, "rx_race", OUT_EAX) { m.r[0] = rx_override_spline_scan_start(m.r[7]); return m.ret(0); }
// 0052dc40 (ECX = object, arg 0; RET 4) -> EAX = arg.
RX_PORT(0x0052dc40, Behavior_SetEnabled, "rx_race", OUT_EAX) { m.r[0] = rx_behavior_set_enabled(m.r[1], m.arg(0)); return m.ret(4); }
