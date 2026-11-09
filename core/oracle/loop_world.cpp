// loop_world.cpp -- oracle adapters for core/game/loop_world.c (group "rx_loop").
#include "oracle.h"
extern "C" {
#include "../game/loop_world.h"
}
using namespace reflex_port;
using rx_oracle::f32;

RX_PORT(0x00767690, World_DebugToggle, "rx_loop", OUT_FPU) { rx_world_debug_toggle(m.r[6]); return m.ret(0); }
RX_PORT(0x00766ec0, World_UpdatePlayers, "rx_loop", OUT_FPU) { rx_world_update_players(m.arg(0)); return m.ret(4); }
RX_PORT(0x007674b0, MXWorld_vf5, "rx_loop", OUT_EAX | OUT_FPU) { m.r[0] = rx_world_update(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x007678b0, ReplayCameras_Update, "rx_loop", OUT_FPU) { rx_replay_cameras_update(m.arg(0), f32(m.arg(1))); return m.ret(8); }
RX_PORT(0x005933a0, PendingHandlers_Run, "rx_loop", OUT_FPU) { rx_pending_handlers_run(m.arg(0)); return m.ret(4); }
RX_PORT(0x00705640, PendingList_Erase, "rx_loop", OUT_EAX | OUT_FPU) { m.r[0] = rx_pending_list_erase(m.r[7], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
