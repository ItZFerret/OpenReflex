// race_activity_teardown.cpp -- oracle adapters for core/game/race_activity_teardown.c (group rx_race): the race activity's
// teardown. No reference world tears an activity down; the probe core/oracle/race_teardown_probe.cpp drives it.
#include "race_oracle.h"
extern "C" {
#include "../game/race_activity_teardown.h"
}
using namespace reflex_port;

// 005882b0 (ECX = name; stdcall activity, method; RET 8)
RX_PORT(0x005882b0, Race_UnlistenStunt, "rx_race", 0) { rx_race_unlisten_stunt(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
// 00588310 (stdcall activity; RET 4)
RX_PORT(0x00588310, Race_UnlistenPhysicsStunt, "rx_race", 0) { rx_race_unlisten_physics_stunt(m.arg(0)); return m.ret(4); }
// 00588a80 (EAX = channel; stdcall activity, method; RET 8)
RX_PORT(0x00588a80, Race_UnlistenLapCompleted, "rx_race", 0) { rx_race_unlisten_lap_completed(m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
// 00588ae0 (stdcall activity; RET 4)
RX_PORT(0x00588ae0, Race_UnlistenTimedSection, "rx_race", 0) { rx_race_unlisten_timed_section(m.arg(0)); return m.ret(4); }
// 00566c80 MXActivityRaceStub::vf3 (thiscall)
RX_PORT(0x00566c80, RaceStub_Vf3, "rx_race", 0) { rx_race_stub_vf3(m.r[1]); return m.ret(0); }
// 00564c10 MXActivityRace::vf3 (thiscall)
RX_PORT(0x00564c10, Race_Vf3, "rx_race", 0) { rx_race_vf3(m.r[1]); return m.ret(0); }
// 0053a980 (EAX = registry; stdcall driver; RET 4)
RX_PORT(0x0053a980, RaceDriver_UnlistenNetwork, "rx_race", 0) { rx_race_driver_unlisten_network(m.r[0], m.arg(0)); return m.ret(4); }
// 007006e0 (EAX = state)
RX_PORT(0x007006e0, NetPlayerState_Clear, "rx_race", 0) { rx_net_player_state_clear(m.r[0]); return m.ret(0); }
// 0050ef80 RaceDriver::vf2 (thiscall)
RX_PORT(0x0050ef80, RaceDriver_Vf2, "rx_race", 0) { rx_race_driver_vf2(m.r[1]); return m.ret(0); }
// driver destruction
RX_PORT(0x0052aa00, Behavior_Sdtor, "rx_race", OUT_EAX) { m.r[0] = rx_behavior_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0051ff30, TrackedEntry_Remove, "rx_race", 0) { rx_tracked_entry_remove(m.r[6]); return m.ret(0); }
RX_PORT(0x0051f3b0, BehaviorAirLeveling_Sdtor, "rx_race", OUT_EAX) { m.r[0] = rx_behavior_air_leveling_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0053b8b0, FixForward_UnlistenWrecked, "rx_race", 0) { rx_fix_forward_unlisten_wrecked(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00523460, FixForward_Vf2, "rx_race", 0) { rx_fix_forward_vf2(m.r[1]); return m.ret(0); }
RX_PORT(0x00513530, RaceDriver_Sdtor, "rx_race", OUT_EAX) { m.r[0] = rx_race_driver_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x005659e0, RaceStub_Destroy, "rx_race", 0) { rx_race_stub_destroy(m.r[1]); return m.ret(0); }
RX_PORT(0x00564af0, Race_Sdtor, "rx_race", OUT_EAX) { m.r[0] = rx_race_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
