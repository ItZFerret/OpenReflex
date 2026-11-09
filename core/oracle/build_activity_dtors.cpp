// build_activity_dtors.cpp -- oracle adapters for core/game/build_activity_dtors.c (group rx_race): the activity destructors and
// area-release virtuals. No reference world ends an activity; the probe core/oracle/dtor_probe.cpp drives them.
#include "race_oracle.h"
extern "C" {
#include "../game/build_activity_dtors.h"
}
using namespace reflex_port;

// 00556640 / 005659c0 / 0055fec0 (thiscall flags; RET 4) -> this
RX_PORT(0x00556640, Activity_Sdtor, "rx_race", OUT_EAX) { m.r[0] = rx_activity_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x005659c0, RaceStub_Sdtor, "rx_race", OUT_EAX) { m.r[0] = rx_race_stub_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0055fec0, FreeRide_Sdtor, "rx_race", OUT_EAX) { m.r[0] = rx_freeride_activity_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
// 0055fee0 (ECX = the activity)
RX_PORT(0x0055fee0, FreeRide_Destroy, "rx_race", 0) { rx_freeride_activity_destroy(m.r[1]); return m.ret(0); }
// 00560080 / 00578b30 (thiscall)
RX_PORT(0x00560080, FreeRide_Vf3, "rx_race", 0) { rx_freeride_activity_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x00578b30, Waypoint_Vf3, "rx_race", 0) { rx_waypoint_activity_vf3(m.r[1]); return m.ret(0); }
// 0058a420 (EAX = channel; stdcall activity; RET 4)
RX_PORT(0x0058a420, Waypoint_UnlistenNetworkObject, "rx_race", 0) { rx_waypoint_unlisten_network_object(m.r[0], m.arg(0)); return m.ret(4); }
// 00555630 Activity::vf43 (thiscall; stack a, b, c; RET 0xc) -> EAX 0
RX_PORT(0x00555630, Activity_StandingsCompare, "rx_race", OUT_EAX) { m.r[0] = uint32_t(rx_activity_standings_compare(m.r[1], m.arg(0), m.arg(1), m.arg(2))); return m.ret(0xc); }
