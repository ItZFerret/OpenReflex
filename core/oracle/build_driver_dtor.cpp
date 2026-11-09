// build_driver_dtor.cpp -- oracle adapters for core/game/build_driver_dtor.c (group rx_build11).
#include "oracle.h"
extern "C" {
#include "../game/build_driver_dtor.h"
}
using namespace reflex_port;

RX_PORT(0x00508ee0, BaseDriver_vf2, "rx_build11", 0) { rx_base_driver_vf2(m.r[1]); return m.ret(0); }
// 0053a410 (ECX = registry, EAX = name; stdcall obj, fn; RET 8). Void.
RX_PORT(0x0053a410, FreeRideUnsubscribe, "rx_build11", 0) { rx_free_ride_unsubscribe(m.r[1], m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0050bd30, FreeRideDriver_vf2, "rx_build11", 0) { rx_free_ride_driver_vf2(m.r[1]); return m.ret(0); }
// 0051bd10 Rule::vf0 (thiscall flags; RET 4) -> EAX = this
RX_PORT(0x0051bd10, Rule_vf0, "rx_build11", OUT_EAX) { m.r[0] = rx_rule_vf0(m.r[1], m.arg(0)); return m.ret(4); }
// 00558470 (stdcall activity; RET 4). Void.
RX_PORT(0x00558470, ActivityEntitiesReset106b, "rx_build11", 0) { rx_activity_entities_reset_106b(m.arg(0)); return m.ret(4); }
// 0055a510 Activity::vf51 (thiscall); ends in a tail jump to 0047c3f0 (the core runs it: part of the verified call)
RX_PORT(0x0055a510, Activity_vf51, "rx_build11", 0) { rx_activity_vf51(m.r[1]); return m.ret(0); }
RX_PORT(0x0053a7a0, NetworkUnsubscribe, "rx_build11", 0) { rx_network_unsubscribe(m.r[1], m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0050e2f0, NetworkDriverDtor, "rx_build11", 0) { rx_network_driver_dtor(m.r[1]); return m.ret(0); }
RX_PORT(0x0050bc90, FreeRideDriver_sdtor, "rx_build11", OUT_EAX) { m.r[0] = rx_free_ride_driver_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
