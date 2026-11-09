// build_activity_pre.cpp -- oracle adapters for core/game/build_activity_pre.c (group rx_build11).
#include "oracle.h"
extern "C" {
#include "../game/build_activity_pre.h"
}
using namespace reflex_port;
using rx_oracle::f32;

// 00770d50 (ECX = the physics world). Void.
RX_PORT(0x00770d50, PhysicsWorldResetBodies, "rx_build11", 0) { rx_physics_world_reset_bodies(m.r[1]); return m.ret(0); }
// 00707320 (stdcall channel, kind, float; RET 0xc). Void.
RX_PORT(0x00707320, SessionQueueTimerEvent, "rx_build11", 0) { rx_session_queue_timer_event(m.arg(0), m.arg(1), f32(m.arg(2))); return m.ret(0xc); }
// 006e4a90 (ESI = the session; stack float; RET 4 at both exits: 006e4b8d / 006e4bca). Void.
// (Found by the challenge restart probe, whose original Pre enter calls it: the adapter used to return with RET 0, leaving the argument.)
RX_PORT(0x006e4a90, SessionPreTimer, "rx_build11", 0) { rx_session_pre_timer(m.r[6], f32(m.arg(0))); return m.ret(4); }
// 00559ac0 / 0047bce0 / 005589c0
RX_PORT(0x00559ac0, SceneViewsReset, "rx_build11", 0) { rx_scene_views_reset(); return m.ret(0); }
RX_PORT(0x0047bce0, DamageManagerReleaseTargets, "rx_build11", 0) { rx_damage_manager_release_targets(m.r[0]); return m.ret(0); }
RX_PORT(0x005589c0, Activity_vf37, "rx_build11", 0) { rx_activity_vf37(m.r[1]); return m.ret(4); }
// 005602f0 MXActivityFreeRide::vf54 (thiscall) -> EAX
RX_PORT(0x005602f0, FreeRide_vf54, "rx_build11", OUT_EAX) { m.r[0] = rx_free_ride_vf54(m.r[1]); return m.ret(0); }
// 00585790 (ESI = the tracker; stdcall activity; RET 4)
RX_PORT(0x00585790, FreeRideCollectablesHide, "rx_build11", 0) { rx_free_ride_collectables_hide(m.r[6], m.arg(0)); return m.ret(4); }
// 00819be0 (stdcall entity; RET 4)
RX_PORT(0x00819be0, EntityBodiesFlagClear, "rx_build11", 0) { rx_entity_bodies_flag_clear(m.arg(0)); return m.ret(4); }
// 00561820 (stdcall activity; RET 4)
RX_PORT(0x00561820, FreeRidePickupsHide, "rx_build11", 0) { rx_free_ride_pickups_hide(m.arg(0)); return m.ret(4); }
// 005603c0 MXActivityFreeRide::vf37 (thiscall old state; RET 4)
RX_PORT(0x005603c0, FreeRide_vf37, "rx_build11", 0) { rx_free_ride_vf37(m.r[1], m.arg(0)); return m.ret(4); }
