// loop_cam_misc.cpp -- oracle adapters for core/game/loop_cam_misc.c (group "rx_loop").
#include "oracle.h"
extern "C" {
#include "../game/loop_cam_misc.h"
}
using namespace reflex_port;
using rx_oracle::f32;
using rx_oracle::bits;

// EAX = player index -> xmm0 (movss: upper lanes kept for the 1.0 load, zeroed by xorps for 0)
RX_PORT(0x00548620, Player_Analog7Active, "rx_loop", OUT_XMM0) {
    const float v = rx_player_analog7_active(m.r[0]);
    m.xmm[0].lo = bits(v); m.xmm[0].hi = 0; return m.ret(0); }
// ESI = controller index -> xmm0 (1.0 / -1.0 / 0.0; the movss loads keep the upper lanes)
RX_PORT(0x005486c0, Camera_HControl, "rx_loop", OUT_XMM0) {
    const float v = rx_camera_hcontrol(m.r[6]);
    m.xmm[0].lo = bits(v); m.xmm[0].hi = 0; return m.ret(0); }
RX_PORT(0x00548710, Camera_VControl, "rx_loop", OUT_XMM0) {
    const float v = rx_camera_vcontrol(m.r[6]);
    m.xmm[0].lo = bits(v); m.xmm[0].hi = 0; return m.ret(0); }
RX_PORT(0x00548760, Camera_Dolly, "rx_loop", OUT_XMM0) {
    const float v = rx_camera_dolly(m.r[6]);
    m.xmm[0].lo = bits(v); m.xmm[0].hi = 0; return m.ret(0); }
RX_PORT(0x006481a0, MXStartGateCamera_vf19, "rx_loop", 0) { rx_start_gate_camera_vf19(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00648990, ReplayCamera_Controller, "rx_loop", OUT_EAX) { m.r[0] = rx_replay_camera_controller(); return m.ret(0); }
RX_PORT(0x006489e0, ReplayCamera_FocusPoint, "rx_loop", OUT_EAX) { m.r[0] = rx_replay_focus_point(); return m.ret(0); }
RX_PORT(0x00648aa0, MXReplayCamera_vf19, "rx_loop", 0) { rx_replay_camera_vf19(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00649160, MXReplayTowerCamera_vf19, "rx_loop", 0) { rx_replay_tower_camera_vf19(m.r[1], f32(m.arg(0))); return m.ret(4); }
// ECX = camera, EAX = position; stack out; ret 4
RX_PORT(0x00647950, Cam_LiftAboveWater2, "rx_loop", 0) { rx_cam_lift_above_water2(m.r[1], m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00646ff0, MXQuickCamera_vf19, "rx_loop", 0) { rx_quick_camera_vf19(m.r[1], f32(m.arg(0))); return m.ret(4); }
// ESI = player index; cdecl out (plain ret)
RX_PORT(0x005487d0, Player_Stick7, "rx_loop", 0) { rx_player_stick7(m.r[6], m.arg(0)); return m.ret(0); }
