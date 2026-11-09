// race_activity_misc.cpp -- oracle adapters for core/game/race_activity_misc.c (group rx_race).
#include "race_oracle.h"
extern "C" {
#include "../game/race_activity_misc.h"
}
using namespace reflex_port;

// 004fb0e0 (EAX = vector) -> ECX
RX_PORT(0x004fb0e0, Vector_Clear, "rx_race", OUT_ECX) { m.r[1] = rx_vector_clear(m.r[0]); return m.ret(0); }
// 005dfc90 (cdecl name) -> EAX: the language handle
RX_PORT(0x005dfc90, UI_LanguageFind, "rx_race", OUT_EAX) { m.r[0] = rx_ui_language_find(m.arg(0)); return m.ret(0); }
// 00587630 (ESI = manager, ECX = player, EDX = driver, stack activity; RET 4)
RX_PORT(0x00587630, Event_PlayerLeft3, "rx_race", 0) { rx_event_activity_player_driver(m.r[6], m.arg(0), m.r[1], m.r[2]); return m.ret(4); }
// 0054db00 (EAX = player)
RX_PORT(0x0054db00, Player_ReleaseSession, "rx_race", 0) { rx_player_release_session(m.r[0]); return m.ret(0); }
// 0055c1d0 (stdcall activity, driver; RET 8)
RX_PORT(0x0055c1d0, Activity_OnPlayerLeft, "rx_race", 0) { rx_activity_on_player_left(m.arg(0), m.arg(1)); return m.ret(8); }
// 00592ef0 (EAX = channel, stack key; RET 4)
RX_PORT(0x00592ef0, Channel_RemoveListener, "rx_race", 0) { rx_channel_remove_listener(m.r[0], m.arg(0)); return m.ret(4); }
// 006cde10 (stdcall hook; RET 4)
RX_PORT(0x006cde10, StuntScoring_Unlisten, "rx_race", 0) { rx_stunt_scoring_unlisten(m.arg(0)); return m.ret(4); }
// 005864d0 (EDI = map, EAX = where; stdcall out, left, val; RET 0xc) -> EAX = out
RX_PORT(0x005864d0, StateMap_InsertAt, "rx_race", OUT_EAX) { m.r[0] = rx_state_map_insert_at(m.r[7], m.r[0], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
// 005862f0 (stdcall map, out, val; RET 0xc) -> EAX = out
RX_PORT(0x005862f0, StateMap_InsertUnique, "rx_race", OUT_EAX) { m.r[0] = rx_state_map_insert_unique(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
// 00585e90 (stdcall map, out, val, hint container, hint node; RET 0x14) -> EAX = out
RX_PORT(0x00585e90, StateMap_InsertHint, "rx_race", OUT_EAX) { m.r[0] = rx_state_map_insert_hint(m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4)); return m.ret(0x14); }
// 004de700 (ESI = row)
RX_PORT(0x004de700, PlayerRow_SetDisconnected, "rx_race", 0) { rx_player_row_set_disconnected(m.r[6]); return m.ret(0); }
// 00553a50 (stdcall players; RET 4)
RX_PORT(0x00553a50, Players_MarkDisconnected, "rx_race", 0) { rx_players_mark_disconnected(m.arg(0)); return m.ret(4); }
// 00553c60 (stdcall players; RET 4)
RX_PORT(0x00553c60, Players_RemoveLeft, "rx_race", 0) { rx_players_remove_left(m.arg(0)); return m.ret(4); }
// 00552c60 (EAX = player, ESI = players)
RX_PORT(0x00552c60, Player_SlotFree, "rx_race", 0) { rx_player_slot_free(m.r[6], m.r[0]); return m.ret(0); }
// 004e21d0 (stdcall high, low; RET 8)
RX_PORT(0x004e21d0, PlayerAttrs_Delete, "rx_race", 0) { rx_player_attrs_delete(m.arg(0), m.arg(1)); return m.ret(8); }
// 004e2340 (stdcall high, low, attribute, value; RET 0x10) -> EAX (the liveness table also lists ECX: no caller in the core reads it)
RX_PORT(0x004e2340, PlayerAttr_Add, "rx_race", OUT_EAX) { m.r[0] = rx_player_attr_add(m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0x10); }
// 00764560 (stdcall records, slot; RET 8)
RX_PORT(0x00764560, MXRecords_Leave, "rx_race", 0) { rx_mxrecords_leave(m.arg(0), m.arg(1)); return m.ret(8); }
// 00763600 (stdcall &dst ref, &src ref; RET 8)
RX_PORT(0x00763600, MXRecords_CopyRow, "rx_race", 0) { rx_records_copy_row(m.arg(0), m.arg(1)); return m.ret(8); }
// 00585330 (EDI = tracker; stdcall activity, player; RET 8)
RX_PORT(0x00585330, FreeRide_Discoveries, "rx_race", 0) { rx_free_ride_discoveries(m.r[7], m.arg(0), m.arg(1)); return m.ret(8); }
