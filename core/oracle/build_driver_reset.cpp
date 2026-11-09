// build_driver_reset.cpp -- oracle adapters for core/game/build_driver_reset.c (group rx_build11).
#include "oracle.h"
extern "C" {
#include "../game/build_driver_reset.h"
}
using namespace reflex_port;

// 0051f150 (EAX = the kind) -> EAX
RX_PORT(0x0051f150, ActivityKindFlag, "rx_build11", OUT_EAX) { m.r[0] = rx_activity_kind_flag(m.r[0]); return m.ret(0); }
// 00508ad0 (EAX = the player)
RX_PORT(0x00508ad0, DriverResetTimers, "rx_build11", 0) { rx_driver_reset_timers(m.r[0]); return m.ret(0); }
// 00409320 MudRecord_Clear (EDX = the vehicle)
RX_PORT(0x00409320, MudRecordClear, "rx_build11", 0) { rx_mud_record_clear(m.r[2]); return m.ret(0); }
// 00508f90 BaseDriver::vf3 / 0050e400 NetworkDriver::vf3 / 0050bd80 FreeRideDriver::vf3 (thiscall)
RX_PORT(0x00508f90, BaseDriver_vf3, "rx_build11", 0) { rx_base_driver_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x0050e400, NetworkDriver_vf3, "rx_build11", 0) { rx_network_driver_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x0050bd80, FreeRideDriver_vf3, "rx_build11", 0) { rx_free_ride_driver_vf3(m.r[1]); return m.ret(0); }
// 005092b0 / 005092f0 (stdcall driver; RET 4)
RX_PORT(0x005092b0, DriverBehaviorsVf6, "rx_build11", 0) { rx_driver_behaviors_vf6(m.arg(0)); return m.ret(4); }
RX_PORT(0x005092f0, DriverRulesVf6, "rx_build11", 0) { rx_driver_rules_vf6(m.arg(0)); return m.ret(4); }
// 0053a4c0 (stdcall driver; RET 4) -> EAX
RX_PORT(0x0053a4c0, DriverBehaviorF, "rx_build11", OUT_EAX) { m.r[0] = rx_driver_behavior_f(m.arg(0)); return m.ret(4); }

extern "C" { void rx_records_construct(uint32_t o); }
// 007635b0 (EAX = the object); ECX = -1, EDX = 0 at the return are not read
RX_PORT(0x007635b0, RecordsConstruct, "rx_build11", 0) { rx_records_construct(m.r[0]); return m.ret(0); }
