// race_net.cpp -- oracle adapters for core/game/race_net.c (the network player of the NetworkDriver) and the debug camera toggle.
// No reference world reaches these: the probe (race_net_probe.cpp, RX_RACE_PROBE=1) drives them through the verified path.
#include "race_oracle.h"
extern "C" {
#include "../game/race.h"
}
using namespace reflex_port;
using rx_oracle::f32;

// 0089c8a0 (cdecl out) -> EAX = out.
RX_PORT(0x0089c8a0, Net_CurrentStation, "rx_race", OUT_EAX) { m.r[0] = rx_net_current_station(m.arg(0)); return m.ret(0); }
// 00884950 (ECX = reference) -> EAX (AL = the result, the rest the last station query's out pointer).
RX_PORT(0x00884950, Net_RefValid, "rx_race", OUT_EAX) { m.r[0] = rx_net_ref_valid(m.r[1]); return m.ret(0); }
// 005088c0 (ECX = net).
RX_PORT(0x005088c0, Net_Stamp, "rx_race", 0) { rx_net_stamp(m.r[1]); return m.ret(0); }
// 00508b90 (EAX = player, arg 0 = slot; RET 4) -> EAX.
RX_PORT(0x00508b90, Net_SnapshotBind, "rx_race", OUT_EAX) { m.r[0] = rx_net_snapshot_bind(m.r[0], m.arg(0)); return m.ret(4); }
// 0050bc70 (EAX = net, arg 0 = player; RET 4) -> EAX = 1.
RX_PORT(0x0050bc70, Net_BindPlayer, "rx_race", OUT_EAX) { m.r[0] = rx_net_bind_player(m.r[0], m.arg(0)); return m.ret(4); }
// 00700310 (ESI = snapshot) -> EAX = snapshot.
RX_PORT(0x00700310, Net_SnapshotInit, "rx_race", OUT_EAX) { m.r[0] = rx_net_snapshot_init(m.r[6]); return m.ret(0); }
// 006fed10 (EAX = snapshot, ECX = net player).
RX_PORT(0x006fed10, Net_SnapshotControls, "rx_race", 0) { rx_net_snapshot_controls(m.r[0], m.r[1]); return m.ret(0); }
// 006feb00 (EAX = net player, ECX = snapshot) -> EAX.
RX_PORT(0x006feb00, Net_PlayerSnapshot, "rx_race", OUT_EAX) { m.r[0] = rx_net_player_snapshot(m.r[0], m.r[1]); return m.ret(0); }
// 00431cf0 (ECX = source, arg 0 = destination; RET 4).
RX_PORT(0x00431cf0, Net_PoseCopy, "rx_race", 0) { rx_net_pose_copy(m.r[1], m.arg(0)); return m.ret(4); }
// 0045d4d0 (EAX = vehicle), 005084d0 (ECX = vehicle), 0061c6b0 (EAX = pad block), 0054e110 (ESI = player).
RX_PORT(0x0045d4d0, Vehicle_BodyLinkClear, "rx_race", 0) { rx_vehicle_body_link_clear(m.r[0]); return m.ret(0); }
RX_PORT(0x005084d0, Vehicle_NetControlsReset, "rx_race", 0) { rx_vehicle_net_controls_reset(m.r[1]); return m.ret(0); }
RX_PORT(0x0061c6b0, Pad_NetHold, "rx_race", 0) { rx_pad_net_hold(m.r[0]); return m.ret(0); }
RX_PORT(0x0054e110, Player_NetPauseFlag, "rx_race", 0) { rx_player_net_pause_flag(m.r[6]); return m.ret(0); }
// 0050e7e0 (EDI = driver, arg 0 = snapshot; RET 4).
RX_PORT(0x0050e7e0, Net_PlayerReset, "rx_race", 0) { rx_net_player_reset(m.r[7], m.arg(0)); return m.ret(4); }
// 006fe210 (arg 0 = network player, arg 1 = dt; RET 8) -> EAX.
RX_PORT(0x006fe210, Net_PlayerUpdate, "rx_race", OUT_EAX) { m.r[0] = rx_net_player_update(m.arg(0), f32(m.arg(1))); return m.ret(8); }
// 00757f40 (arg 0 = player; RET 4).
RX_PORT(0x00757f40, Game_DebugCameraToggle, "rx_race", 0) { rx_game_debug_camera_toggle(m.arg(0)); return m.ret(4); }
