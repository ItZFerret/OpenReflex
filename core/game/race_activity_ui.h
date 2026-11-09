/* race_activity_ui.h -- the UI property store and the UI language table reads the race / activity code makes (race_activity_ui.c). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t rx_ui_property_get_int(rx_addr key, rx_addr ui, rx_addr out);        /* 005dc0c0 UIProperty_GetInt (EAX = key; stdcall ui, out; RET 8) -> found */
uint32_t rx_ui_property_map_find(rx_addr map, rx_addr key_slot, rx_addr out_slot);   /* 005eab30 (ECX = the map; stdcall &key, &out; RET 8) */
uint32_t rx_ui_key_equal(rx_addr stored, rx_addr key);                        /* 005c3440 (ECX = the stored key, EAX = the key) */
/* The session's UI property read of gameplay code: the oracle runs the port over the UI manager's store ([0xa93fcc]); the product has
 * no UI manager and reads the host's event spec (build_ui_props.c). Same registers and stack contract as 005dc0c0 (EAX = key; the
 * stack arguments are pushed by the caller's RACE_CORE_CALL(2, ...)). */
uint32_t rx_ui_session_get_int(rx_addr key, rx_addr out);

rx_addr  rx_ui_language_lookup(rx_addr key, rx_addr holder);                  /* 005e0270 (EAX = the key string, EDI = the language holder) -> the entry */
uint32_t rx_djb2_hash(rx_addr s);                                             /* 005e1190 (cdecl) */
uint32_t rx_int_hash_signed(uint32_t x);                                      /* 005c3340 (ECX) */
uint32_t rx_int_key_equal(uint32_t stored, uint32_t key);                     /* 005c3370 (ECX; cdecl key) */
uint32_t rx_int_map_find(rx_addr map, rx_addr key_slot, rx_addr out_slot);    /* 005ee560 (ECX = the map; stdcall &key, &out; RET 8) */

void rx_free_ride_collectable_found(rx_addr player, uint32_t index);          /* 0054d590 (stdcall; RET 8) */
#ifdef __cplusplus
}
#endif
