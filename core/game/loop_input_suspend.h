/* loop_input_suspend.h -- a player's input suspended while a UI page holds it (loop_input_suspend.c; w-tutcore).
 *
 * The UI pages that take the controls (the MotoSkills popups and the lesson list, the pause menu: App.SetUIVariableInt( "SuspendPlayerInput",
 * TRUE / FALSE )) set the UI manager's property "SuspendPlayerInput"; MXUIManager::vf44 00598cf0 (the UI manager's update, from UIManager::vf19
 * in the UI scene's tick) calls Player_SetInputSuspended( player, 1 ) for every local controller's player while it is set, and the property's
 * change callback MXUIManager_OnSuspendPlayerInputChanged 005a3b00 calls it with 0 when it is cleared. Player_SetInputSuspended keeps the
 * eleven input groups' enabled state in +0x11cc and switches them off while +0x11c8 is set (every setter of the groups then only marks the
 * group's bit in +0x11cc), and switches the pad's vibration off / back on. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

void     rx_player_set_input_suspended(rx_addr player, uint32_t suspended);   /* 0054f540 Player_SetInputSuspended (stdcall; RET 8) */
uint32_t rx_player_group0_enabled(uint32_t pad);       /* 00548d20 (ESI = pad) -> EAX: the steer / lean pad maps both enabled */
uint32_t rx_player_group3_enabled(uint32_t pad);       /* 00548d60 (EAX = pad) -> EAX: the wreck steer / lean maps both enabled */
uint32_t rx_player_group2_enabled(uint32_t pad);       /* 00548da0 (ESI = pad) -> EAX: the KeyMaps +0x148 / +0x1134 / +0x11d8 enabled */
uint32_t rx_player_group5_enabled(uint32_t pad);       /* 00548e00 (ESI = pad) -> EAX: the ten KeyMaps +0x33a0 .. +0x4630 enabled */
uint32_t rx_input_set_vibration_paused(rx_addr im, uint32_t device, uint32_t ctl, uint32_t paused);   /* 00815170 XenonInputManager::vf45 (RET 0xc) */
void     rx_ui_on_suspend_input_changed(rx_addr ui, rx_addr args);          /* 005a3b00 MXUIManager_OnSuspendPlayerInputChanged (ECX = ui; RET 4) */

/* UI BOUNDARY: UIProperty_GetInt( ui, "SuspendPlayerInput" ) (005dc0c0: EAX = name; stack ui, &value; RET 8). Oracle: the original. Product: the
 * property the host's pages set (rx_ui_suspend_input_set). */
uint32_t rx_ui_get_suspend_input(rx_addr ui, rx_addr out);

#ifndef RX_ORACLE
/* ---- PRODUCT ONLY (the host draws the pages; the core has no UI manager) ---- */
/* The pages' App.SetUIVariableInt( "SuspendPlayerInput", value ): the property; a change calls 005a3b00 as the property's change callback does
 * (UIProperty_SetInt runs the callbacks at once). */
void     rx_ui_suspend_input_set(rx_addr ui, uint32_t value);
uint32_t rx_ui_suspend_input(void);
void     rx_ui_suspend_input_reset(void);                                   /* a new session: the property 0 */
/* MXUIManager::vf44's SuspendPlayerInput loop (00598df4 .. 00598e22), at the UI manager's place in the frame (the UI scene's tick,
 * loop_engine.c): every local controller's player suspended while the property is set. */
void     rx_ui_suspend_input_frame(void);
#endif

#ifdef __cplusplus
}
#endif
