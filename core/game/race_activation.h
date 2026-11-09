/* race_activation.h -- the Free Ride activation areas (race_activation.c): the UI manager's handlers of the vehicle trigger events that
 * mark the area the player is in, what they call, and the career Free Ride's discovery collectables. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ---- the activation areas ("StartActivity" MXTriggers of "<level>FreeRide") ---- */
uint32_t rx_challenge_activity_index(rx_addr name);                                      /* 00596540 (cdecl name) -> 0 .. 0xa, 0xb unknown */
int32_t  rx_challenge_required_class(uint32_t type, uint32_t track, rx_addr spec);       /* 004109d0 (EAX = type, ECX = track; stdcall spec; RET 4) */
uint32_t rx_challenge_vehicle_allowed(uint32_t type, uint32_t track, rx_addr spec, int32_t controller);
                                                                                          /* 00410830 (EAX = type, ECX = track; stdcall spec, controller; RET 8) */
rx_addr  rx_player_table_row(int32_t controller);                                        /* 004e78b0 MXTableHelper_GetPlayerTableRow (stdcall; RET 4) */
rx_addr  rx_vehicle_record_find(rx_addr list, int32_t vehicle_id);                       /* 00542ff0 (EAX = list, EDI = id) */
rx_addr  rx_tbl_rows_by_name(rx_addr table, rx_addr column_name, rx_addr key_slot);      /* 004199d0 (ESI = table, EAX = name; stdcall &key; RET 4) */
void     rx_ui_trigger_enter(rx_addr ui, rx_addr vehicle, rx_addr trigger);              /* 005a3b60 MXUIManager_OnTriggerEnter (thiscall; RET 8) */
void     rx_ui_trigger_exit(rx_addr ui, rx_addr vehicle, rx_addr trigger);               /* 005a3d00 MXUIManager_OnTriggerExit (thiscall; RET 8) */
#ifndef RX_ORACLE
/* The product's stand-in for the UI manager's subscription of the two handlers (MXUIManager::vf42 005999f0 through 005bc930): called
 * once per session after the vehicle trigger channel exists (build_ui_world.c). */
void     rx_activation_listen(void);
/* The last ChallengeRequiredVehicleClass the handlers set (-1 at the session start and after an exit): the AAA notification's help. */
int32_t  rx_activation_required_class(void);
#endif

/* ---- the career Free Ride's discovery collectables (MXActivityFreeRide::vf37 in "discovery mode": vf54 = 1) ---- */
uint32_t rx_player_discovery_found(rx_addr player, uint32_t index);                      /* 0054d640 (stdcall player, index; RET 8) */
void     rx_free_ride_discoveries_setup(rx_addr tracker, rx_addr activity);              /* 00585520 (stdcall tracker, activity; RET 8) */

#ifdef __cplusplus
}
#endif
