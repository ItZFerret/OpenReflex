// world_flag_holders.cpp -- oracle adapters for core/game/world_flag_holders.c (group rx_world).
#include "oracle.h"
extern "C" {
#include "../game/world_flag_holders.h"
}
using namespace reflex_port;

// 006c45f0 / 006c46b0 (ECX = channel, EAX = name; stack entity, callback, argument; RET 0xc)
RX_PORT(0x006c45f0, EventSubscribeActivityDriver, "rx_world", 0) { rx_event_subscribe_activity_driver(m.r[1], m.r[0], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x006c46b0, EventUnsubscribeActivityDriver, "rx_world", 0) { rx_event_unsubscribe_activity_driver(m.r[1], m.r[0], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
// 006c4580 (ECX = name)
RX_PORT(0x006c4580, EventUnsubscribeGlobal, "rx_world", 0) { rx_event_unsubscribe_global(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x006c18e0, ScoreTower_FirstHuman, "rx_world", 0) { rx_score_tower_first_human(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x006c1910, ScoreTower_ActivityInit, "rx_world", 0) { rx_score_tower_activity_init(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x006c1990, ScoreTower_ActivityUninit, "rx_world", 0) { rx_score_tower_activity_uninit(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x006c1b30, ScoreTower_TwoLaps, "rx_world", 0) { rx_score_tower_two_laps(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x006c1b60, ScoreTower_EndOfRace, "rx_world", 0) { rx_score_tower_end_of_race(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
// 006c1a90 (EAX = the tower, ECX = the state)
RX_PORT(0x006c1a90, ScoreTower_ApplyState, "rx_world", 0) { rx_score_tower_apply_state(m.r[0], m.r[1]); return m.ret(0); }
RX_PORT(0x006c1b80, ScoreTower_SetFlagTexture, "rx_world", 0) { rx_score_tower_set_flag_texture(m.arg(0), m.arg(1)); return m.ret(8); }
// 006c4ab0 / 006c4b70 (ECX = channel, EAX = name; stack entity, callback, argument; RET 0xc)
RX_PORT(0x006c4ab0, EventSubscribeActivityVehicle, "rx_world", 0) { rx_event_subscribe_activity_vehicle(m.r[1], m.r[0], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x006c4b70, EventUnsubscribeActivityVehicle, "rx_world", 0) { rx_event_unsubscribe_activity_vehicle(m.r[1], m.r[0], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x006c30f0, TrackFlag_SetFlagTexture, "rx_world", 0) { rx_track_flag_set_flag_texture(m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x006c2d90, TrackFlag_ActivityInit, "rx_world", 0) { rx_track_flag_activity_init(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x006c2e60, TrackFlag_ActivityUninit, "rx_world", 0) { rx_track_flag_activity_uninit(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x006c2f60, TrackFlag_VehicleWrecked, "rx_world", 0) { rx_track_flag_vehicle_wrecked(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x006c3020, TrackFlag_VehicleLapped, "rx_world", 0) { rx_track_flag_vehicle_lapped(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
