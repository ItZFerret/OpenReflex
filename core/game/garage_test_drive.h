/* garage_test_drive.h -- the garage's TEST DRIVE in the front end's own world (garage_test_drive.c, _re/notes/garage.md section 5). */
#pragma once
#include "rx_game.h"

void rx_ui_set_players_inactive(uint32_t value, rx_addr ui);   /* MXUIManager_SetPlayersInactive 005992f0 (EAX = value; stdcall ui; RET 4) */
void rx_ui_enable_test_drive(rx_addr ui, uint32_t enable);     /* MXUIManager_EnableTestDrive 00599550 (ECX = ui, EAX = enable) */
void rx_garage_test_drive_start(rx_addr garage);               /* MXGarage_TestDriveStart 00762a20 (EAX = the garage, MXGame + 0x15570) */
void rx_garage_test_drive_end(rx_addr garage);                 /* MXGarage_TestDriveEnd 00762cc0 (EAX = the garage) */
void rx_ui_presence(uint32_t controller, uint32_t a, uint32_t b);  /* the online session's rich presence of a local player ([0x00da6cb0] vf12: ONLINE BOUNDARY) */
