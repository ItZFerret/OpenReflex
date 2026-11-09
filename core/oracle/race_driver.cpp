// race_driver.cpp -- oracle adapters for core/game/race_driver*.c.
#include "race_oracle.h"
extern "C" {
#include "../game/race.h"
#include "../game/race_driver.h"
}
using namespace reflex_port;
static void xmm0_only(reflex_vm::Machine& m, float r) { m.xmm[0].lo = rx_oracle::bits(r); m.xmm[0].hi = 0; }

RX_PORT(0x00508b60, Float_Get, "rx_race", OUT_XMM0) { xmm0_only(m, rx_float_get(m.r[0])); return m.ret(0); }
RX_PORT(0x00508b70, Float_Set4, "rx_race", 0) { rx_float_set4(m.r[0], rx_oracle::f32(uint32_t(m.xmm[0].lo))); return m.ret(0); }
RX_PORT(0x00508c00, CopyPair20, "rx_race", 0) { rx_copy_pair_20(m.r[1], m.r[0]); return m.ret(0); }
RX_PORT(0x00508a90, Field_b58, "rx_race", OUT_EAX) { m.r[0] = rx_field_b58(m.r[0]); return m.ret(0); }
RX_PORT(0x005089a0, DebugKeyMap_Get, "rx_race", OUT_EAX) { m.r[0] = rx_debug_keymap_get(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
// 0050deb0 (ECX = a, EDX = b, arg 0 = out; cdecl) -> EAX = out.
RX_PORT(0x0050deb0, Race_ProjectOn, "rx_race", OUT_EAX) { m.r[0] = rx_race_project_on(m.r[1], m.r[2], m.arg(0)); return m.ret(0); }
// 00512620 (ECX = driver) -> xmm0; EAX = the progress node.
RX_PORT(0x00512620, Driver_DistanceToLine, "rx_race", OUT_EAX | OUT_XMM0) {
    xmm0_only(m, rx_driver_distance_to_line(m.r[1])); m.r[0] = U32(m.r[1], 0x90); return m.ret(0);
}
// 00512870 (ESI = driver) -> xmm0. EAX = player, ECX = activity (or the lap-array cursor), EDX = laps on the summing path.
RX_PORT(0x00512870, Driver_TotalTime, "rx_race", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM0) {
    const uint32_t d = m.r[6], p = U32(d, 0x74), a = U32(p, 0x94);
    xmm0_only(m, rx_driver_total_time(d));
    m.r[0] = p; m.r[1] = a;
    if (U32(a, 0xfe8) == 0) {
        const int32_t n = I32(d, 0xf1c);
        m.r[2] = uint32_t(n);
        if (n >= 2) m.r[1] = d + 0xf28 + 8 * ((uint32_t(n - 2) >> 1) + 1);
    }
    return m.ret(0);
}
// 00512c80 (ECX = driver, EAX = the riders left behind it; plain RET): the driver is eliminated
RX_PORT(0x00512c80, Driver_Eliminate, "rx_race", 0) { rx_driver_eliminate(m.r[1], m.r[0]); return m.ret(0); }
RX_PORT(0x00512910, Driver_StandingsTime, "rx_race", OUT_XMM0) { xmm0_only(m, rx_driver_standings_time(m.r[0])); return m.ret(0); }
// 00512690 (EAX = driver, arg 0 = position; RET 4) -> xmm0 (upper lanes 0).
RX_PORT(0x00512690, Driver_Progress, "rx_race", OUT_XMM0) { xmm0_only(m, rx_driver_progress(m.r[0], m.arg(0))); return m.ret(4); }
// 00512d00 (ESI = a, EDI = b) -> EAX.
RX_PORT(0x00512d00, Driver_HasLapped, "rx_race", OUT_EAX) { m.r[0] = uint32_t(rx_driver_has_lapped(m.r[6], m.r[7])); return m.ret(0); }
// 00512140 RaceDriver::vf9 (ECX = driver, arg 0 = rule; RET 4).
RX_PORT(0x00512140, RaceDriver_OnRule, "rx_race", 0) { rx_race_driver_on_rule(m.r[1], m.arg(0)); return m.ret(4); }
// The drivers' updates leave the x87 stack as they found it (values the original pops: kept off the emulated stack here).
struct RaceFpuKeep { reflex_vm::Machine& m; std::vector<reflex_f80::F80> fpu; explicit RaceFpuKeep(reflex_vm::Machine& mm) : m(mm), fpu(mm.fpu) {} ~RaceFpuKeep() { m.fpu = fpu; } };
RX_PORT(0x00509370, BaseDriver_Update, "rx_race", 0) { RaceFpuKeep k(m); rx_base_driver_update(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0050e510, NetworkDriver_Update, "rx_race", 0) { RaceFpuKeep k(m); rx_network_driver_update(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00511760, Ai_StartLineControls, "rx_race", 0) { rx_ai_start_line_controls(m.r[0]); return m.ret(0); }
RX_PORT(0x0050fef0, Ai_RaceStart, "rx_race", 0) { rx_ai_race_start(m.r[6]); return m.ret(0); }
RX_PORT(0x0050fd30, RaceDriver_Countdown, "rx_race", 0) { rx_race_driver_countdown(m.r[1]); return m.ret(0); }
RX_PORT(0x0050f620, RaceDriver_Update, "rx_race", 0) { RaceFpuKeep k(m); rx_race_driver_update(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0050f5d0, RaceDriver_vf4, "rx_race", 0) { RaceFpuKeep k(m); rx_race_driver_vf4(m.r[1]); return m.ret(0); }
RX_PORT(0x0054e280, Player_FinishReset, "rx_race", 0) { RaceFpuKeep k(m); rx_player_finish_reset(m.r[6]); return m.ret(0); }
RX_PORT(0x0050bfa0, FreeRide_RulesUpdate, "rx_race", 0) { rx_free_ride_rules_update(m.arg(0)); return m.ret(4); }
RX_PORT(0x0050c070, FreeRideDriver_AIUpdate, "rx_race", 0) { RaceFpuKeep k(m); rx_free_ride_ai_update(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0050c500, FreeRide_AISteer, "rx_race", 0) { RaceFpuKeep k(m); rx_free_ride_steer(m.arg(0)); return m.ret(4); }
RX_PORT(0x0050c7f0, FreeRide_AITimer, "rx_race", 0) { RaceFpuKeep k(m); rx_free_ride_timer(m.r[1]); return m.ret(0); }
RX_PORT(0x0050ab00, FreeRide_AIDebugDraw, "rx_race", 0) { RaceFpuKeep k(m); rx_free_ride_debug_draw(m.r[6]); return m.ret(0); }
// 0051de00 (EAX = driver) -> xmm0; xmm1 passes through (the caller's random draw).
RX_PORT(0x0051de00, AiParam_de00, "rx_race", OUT_XMM0 | (OUT_XMM0 << 1)) {
    m.xmm[0].lo = rx_oracle::bits(rx_ai_param_de00(m.r[0])); m.xmm[0].hi = 0;
    return m.ret(0);
}
// 0050b590 (EAX = driver, EDI = the node's threshold record; RET 0) -> EAX.
RX_PORT(0x0050b590, Driver_StartLean, "rx_race", OUT_EAX) { RaceFpuKeep k(m); m.r[0] = rx_ai_start_lean(m.r[0], m.r[7]); return m.ret(0); }
RX_PORT(0x00508970, DebugKeyMap_Latched, "rx_race", OUT_EAX) { m.r[0] = rx_debug_keymap_latched(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00509c20, Driver_WreckControlsOff, "rx_race", 0) { rx_ai_wreck_controls_off(m.r[0]); return m.ret(0); }
// 0050ad00 (ESI = driver). orig_frame = the original's aligned frame ((entry ESP - 4) & -16) - 0x60.
RX_PORT(0x0050ad00, Driver_LeanStart, "rx_race", 0) {
    RaceFpuKeep k(m);
    rx_ai_lean_start(m.r[6], ((m.r[4] - 4u) & ~15u) - 0x60u);
    return m.ret(0);
}
RX_PORT(0x0050fac0, RaceDriver_AIDelayedStart, "rx_race", 0) { RaceFpuKeep k(m); rx_ai_delayed_start(m.r[6]); return m.ret(0); }
RX_PORT(0x0050bf30, FreeRideDriver_Update, "rx_race", 0) { RaceFpuKeep k(m); rx_free_ride_driver_update(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
// 00511430 (args driver, dt; RET 8). The original's frame: ((entry ESP - 4) & -16) - 0x34 - 3 pushes.
RX_PORT(0x00511430, HumanDriver_Rules, "rx_race", 0) {
    rx_human_driver_rules(m.arg(0), rx_oracle::f32(m.arg(1)), ((m.r[4] - 4) & ~15u) - 0x34 - 0xc);
    return m.ret(8);
}
RX_PORT(0x00511f00, Driver_UpdateHighlight, "rx_race", 0) { rx_driver_update_highlight(m.r[0]); return m.ret(0); }
// 00511f80 (arg 0 = driver, ESI = out; RET 4) -> EAX. Frame ((entry ESP - 4) & -16) - 0x48 - 2 pushes.
RX_PORT(0x00511f80, Driver_PredictSide, "rx_race", OUT_EAX) {
    m.r[0] = uint32_t(rx_driver_predict_side(m.arg(0), m.r[6], ((m.r[4] - 4) & ~15u) - 0x48 - 8));
    return m.ret(4);
}
// 005118c0 (args driver, dt; RET 8).
RX_PORT(0x005118c0, Ai_TrackNodeReactions, "rx_race", 0) { rx_ai_track_node_reactions(m.arg(0), rx_oracle::f32(m.arg(1))); return m.ret(8); }
// 00510010 (args driver, dt; RET 8).
RX_PORT(0x00510010, Ai_RaceUpdate, "rx_race", 0) { RaceFpuKeep k(m); rx_ai_race_update(m.arg(0), rx_oracle::f32(m.arg(1))); return m.ret(8); }

// 0050b330 (ECX = driver, EDX = the control outputs, xmm2 = dt).
RX_PORT(0x0050b330, Ai_ControlSmooth, "rx_race", 0) { rx_ai_control_smooth(m.r[1], m.r[2], rx_oracle::f32(uint32_t(m.xmm[2].lo))); return m.ret(0); }
// 00790100 (EAX = out buffer, xmm0 = seconds, EDI = with hundredths).
RX_PORT(0x00790100, LapTimeText, "rx_race", 0) { rx_lap_time_text(m.r[0], rx_oracle::f32(uint32_t(m.xmm[0].lo)), m.r[7]); return m.ret(0); }
// 00509940 (ESI = driver).
RX_PORT(0x00509940, Ai_DebugOverlay, "rx_race", 0) { rx_ai_debug_overlay(m.r[6]); return m.ret(0); }
