// world_scene_handlers.cpp -- oracle adapters for core/game/world_scene_handlers.c (group rx_world).
#include "oracle.h"
extern "C" {
#include "../game/world_scene_handlers.h"
}
using namespace reflex_port;

// thiscall bale handlers (ECX = the bale; stack vehicle, trigger; RET 8)
RX_PORT(0x006c14a0, PopUpBale_VehicleExited, "rx_world", 0) { rx_popup_bale_vehicle_exited(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x006c0b60, PopDownBale_VehicleExited, "rx_world", 0) { rx_popdown_bale_vehicle_exited(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
// the clip recorder's stunt listeners (ECX = 0x00d32360, unused; stack player, stunt; RET 8)
RX_PORT(0x006ab290, ClipRecorder_StuntBegin, "rx_world", 0) { rx_clip_event_stunt_begin(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x006ab380, ClipRecorder_StuntHoldBegin, "rx_world", 0) { rx_clip_event_stunt_hold_begin(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x006ab410, ClipRecorder_StuntHoldEnd, "rx_world", 0) { rx_clip_event_stunt_hold_end(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x006ab4a0, ClipRecorder_StuntCompleted, "rx_world", 0) { rx_clip_event_stunt_completed(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }

// the listener-removal templates (006c4450 / 006c4330: EAX = channel; 006c3c70: ECX = channel, EAX = name; 006c4a40: ECX = name; 006c3930: none; stack entity, callback, argument; RET 0xc)
RX_PORT(0x006c4450, EventUnregisterVehicleExitedUp, "rx_world", 0) { rx_event_unregister_vehicle_exited_up(m.r[0], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x006c4330, EventUnregisterVehicleExitedDown, "rx_world", 0) { rx_event_unregister_vehicle_exited_down(m.r[0], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x006c3c70, EventUnregisterActivity, "rx_world", 0) { rx_event_unregister_activity(m.r[1], m.r[0], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x006c4a40, EventUnregisterGlobalC, "rx_world", 0) { rx_event_unregister_global_c(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x006c3930, EventUnregisterFirstHuman, "rx_world", 0) { rx_event_unregister_first_human(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
// unloading (vf9, thiscall) and the tower / holder listener removal (ESI = the entity)
RX_PORT(0x006c0f50, PopUpBale_vf9, "rx_world", 0) { rx_popup_bale_vf9(m.r[1]); return m.ret(0); }
RX_PORT(0x006c0730, PopDownBale_vf9, "rx_world", 0) { rx_popdown_bale_vf9(m.r[1]); return m.ret(0); }
RX_PORT(0x006c1890, ScoreTower_Unlisten, "rx_world", 0) { rx_score_tower_unlisten(m.r[6]); return m.ret(0); }
RX_PORT(0x006c1830, ScoreTower_vf9, "rx_world", 0) { rx_score_tower_vf9(m.r[1]); return m.ret(0); }
RX_PORT(0x006c2d40, TrackFlag_Unlisten, "rx_world", 0) { rx_track_flag_unlisten(m.r[6]); return m.ret(0); }
RX_PORT(0x006c2ce0, TrackFlag_vf9, "rx_world", 0) { rx_track_flag_vf9(m.r[1]); return m.ret(0); }
RX_PORT(0x006b7b60, ThirtySecondBoard_vf9, "rx_world", 0) { rx_thirty_second_board_vf9(m.r[1]); return m.ret(0); }

// the profile statistics: 005411f0 (ECX = table, EDI = key -> EAX), 0050de10 (EAX = a FreestyleDriver -> xmm0), 007538c0 (thiscall record, stack activity; RET 4)
RX_PORT(0x005411f0, StatTable_Find, "rx_world", OUT_EAX) { m.r[0] = rx_stat_table_find(m.r[1], m.r[7]); return m.ret(0); }
RX_PORT(0x0050de10, FreestyleDriver_BestScore, "rx_world", OUT_XMM0) { m.xmm[0].lo = rx_oracle::bits(rx_freestyle_driver_best_score(m.r[0])); m.xmm[0].hi = 0; return m.ret(0); }
RX_PORT(0x007538c0, DataMiner_ActivityEnd, "rx_world", 0) { rx_dataminer_activity_end(m.r[1], m.arg(0)); return m.ret(4); }
