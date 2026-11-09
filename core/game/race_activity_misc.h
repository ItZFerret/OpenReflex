/* race_activity_misc.h -- ports of small race / activity / scene helpers found by the census (miscport): their addresses are in
 * the comments, the owners' notes in core/notes/race.md "miscport". */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t rx_vector_clear(rx_addr vec);          /* 004fb0e0 (EAX = std::vector<dword> {+0 proxy, +0xc first, +0x10 last}) -> ECX */
rx_addr rx_ui_language_find(rx_addr name);      /* 005dfc90 (cdecl name; EAX = the language handle or 0) */
void rx_event_activity_player_driver(rx_addr manager, rx_addr activity, rx_addr player, rx_addr driver);   /* 00587630 */
void rx_player_release_session(rx_addr player);                                                  /* 0054db00 (EAX = player) */
void rx_activity_on_player_left(rx_addr activity, rx_addr driver);                                /* 0055c1d0 */
void rx_channel_remove_listener(rx_addr channel, rx_addr key);                                   /* 00592ef0 (EAX = channel; stack key; RET 4) */
void rx_stunt_scoring_unlisten(rx_addr hook);                                                    /* 006cde10 (stdcall hook; RET 4) */
void rx_mxrecords_leave(rx_addr records, uint32_t slot);                                         /* 00764560 (stdcall; RET 8) */
void rx_records_copy_row(rx_addr dst, rx_addr src);                                              /* 00763600 (stdcall; RET 8) */
rx_addr rx_state_map_insert_at(rx_addr map, rx_addr where, rx_addr out, uint32_t add_left, rx_addr val);   /* 005864d0 (EDI = map, EAX = where; stdcall out, left, val; RET 0xc) -> out */
rx_addr rx_state_map_insert_unique(rx_addr map, rx_addr out, rx_addr val);        /* 005862f0 (stdcall map, out, val; RET 0xc) -> out {proxy, node, inserted} */
rx_addr rx_state_map_insert_hint(rx_addr map, rx_addr out, rx_addr val, rx_addr hint_cont, rx_addr hint_node);   /* 00585e90 (stdcall; RET 0x14) -> out {proxy, node} */
void rx_player_row_set_disconnected(uint32_t row);                                               /* 004de700 (ESI = row) */
void rx_players_mark_disconnected(rx_addr players);                                              /* 00553a50 (stdcall players; RET 4) */
void rx_players_remove_left(rx_addr players);                                                    /* 00553c60 (stdcall players; RET 4) */
void rx_player_slot_free(rx_addr players, rx_addr player);                                       /* 00552c60 (EAX = player, ESI = players) */
void rx_player_attrs_delete(uint32_t a0, uint32_t a1);                                /* 004e21d0 (stdcall; RET 8) */
uint32_t rx_player_attr_add(uint32_t a0, uint32_t a1, uint32_t attribute, uint32_t value);   /* 004e2340 (stdcall; RET 0x10) -> the new row */
void rx_free_ride_discoveries(rx_addr tracker, rx_addr activity, rx_addr player);                /* 00585330 (EDI = the tracker; stdcall activity, player; RET 8) */
#ifdef __cplusplus
}
#endif
