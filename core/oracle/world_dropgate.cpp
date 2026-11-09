// world_dropgate.cpp -- oracle adapters for core/game/world_dropgate.c (group rx_world).
#include "oracle.h"
extern "C" {
#include "../game/world_dropgate.h"
}
using namespace reflex_port;

RX_PORT(0x006bdb50, DropGate_ActivityInit, "rx_world", 0) { rx_drop_gate_activity_init(m.r[1], m.arg(0)); return m.ret(4); }
// 006c3d90 (EAX = the channel; stack entity, callback, argument; RET 0xc)
RX_PORT(0x006c3d90, EventUnregisterCountdownStep, "rx_world", 0) { rx_event_unregister_countdown_step(m.r[0], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x006bdb90, DropGate_ActivityUninit, "rx_world", 0) { rx_drop_gate_activity_uninit(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x006bed90, DropGate_CountdownStep, "rx_world", 0) { rx_drop_gate_countdown_step(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
// 006bdbe0 (EAX = the activity, ECX = the gate)
RX_PORT(0x006bdbe0, DropGate_Reset, "rx_world", 0) { rx_drop_gate_reset(m.r[1], m.r[0]); return m.ret(0); }
