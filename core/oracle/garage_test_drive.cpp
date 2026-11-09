// garage_test_drive.cpp -- oracle adapters (group rx_testdrive) for core/game/garage_test_drive.c: the garage's TEST DRIVE in the front end's
// own world (MXUIManager_SetPlayersInactive / EnableTestDrive, MXGarage_TestDriveStart / End). The boot that reaches them:
// REFLEX_EVENT=testdrive REFLEX_TEST_DRIVE=1 [REFLEX_TEST_DRIVE_END=<frame>] (native/drafts/world_runtime/garage.cpp).
#include "oracle.h"
extern "C" {
#include "../game/garage_test_drive.h"
}
using namespace reflex_port;
#define G "rx_testdrive"

// 005992f0 (EAX = value; stdcall ui; RET 4)
RX_PORT(0x005992f0, UISetPlayersInactive, G, 0) { rx_ui_set_players_inactive(m.r[0], m.arg(0)); return m.ret(4); }
// 00599550 (ECX = ui, EAX = enable)
RX_PORT(0x00599550, UIEnableTestDrive, G, 0) { rx_ui_enable_test_drive(m.r[1], m.r[0]); return m.ret(0); }
// 00762a20 / 00762cc0 (EAX = the garage)
RX_PORT(0x00762a20, GarageTestDriveStart, G, 0) { rx_garage_test_drive_start(m.r[0]); return m.ret(0); }
RX_PORT(0x00762cc0, GarageTestDriveEnd, G, 0) { rx_garage_test_drive_end(m.r[0]); return m.ret(0); }
