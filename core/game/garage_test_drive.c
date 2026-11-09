/* garage_test_drive.c -- the garage's TEST DRIVE (_re/notes/garage.md section 5): the front end's own world UI_Compound (MXGame + 0x20c8,
 * MXUIWorld) with its FreeRide activity (the "Activity List UI World" row: type 8, area 0, mode 4) takes the local players.
 *   GarageHUB:OnStartPressed (Lua): MXGarage:Unload, App.EnableTestDrive( TRUE ) (MXUIManager_EnableTestDrive 00599550), App.SyncDatabaseWithManagers
 *   (Game_SetupActivityArea 0075c430), "GotoLoadingPage" -> the "Test Drive Loading Page", whose TestDriveTransitionPage:Present calls
 *   mxGarage:TestDriveStart( ) (MXGarage_TestDriveStart 00762a20); the world's frame spawns the slot (MXWorld::vf5 -> 00766ec0 ->
 *   PlayerSlot_SpawnStep); GotoInGameHUB. The pause request in a test drive (MXUIManager_OnPauseRequest 00598e40 with ui+0x290 set) ends it:
 *   ui+0x294 = 1, EnableTestDrive( FALSE ) -> "Test Drive Exit Page" -> MXGarage_TestDriveEnd 00762cc0 -> the Garage Page.
 * The UI manager's own words (ui+0x280 / +0x284 the front end's players idle / +0x290 the test drive / +0x294) are the caller's `ui` block: the
 * product has no UI manager (MXGame + 0x745c = 0), its session API passes a block of its own (api/test_drive_session.inc).
 * Boundaries (oracle: the original; product: nothing): the UI's properties and its hub route (UIProperty_SetInt / _SetString, the "GotoHub"
 * event), the local players' rich presence (the online session [0x00da6cb0] vf12 (+0x30)), the split-screen viewports (00757660: render), the
 * garage's own vehicle (007611b0: the garage is never loaded in the product, so its fields are 0), and at the end the UI world's lights /
 * reflection map (007656b0) and the scenes' render records (00757dc0). Evidence: the oracle boot REFLEX_TEST_DRIVE (native/drafts/
 * world_runtime/garage.cpp start_test_drive), core/plan/QUEUE.md "w-testride". */
#include "garage_test_drive.h"
#include "build_activity_util.h"
#include "build_ui_world.h"
#include "build_series_init.h"
#include "career_player_native.h"
#include "crt_atexit.h"
#include "race_driver.h"
#include <string.h>

#define PL_ACTIVE      0x0093def8u   /* "Active" */
#define PL_TYPE        0x00936ef4u   /* "PlayerType" (0 human, 1 computer) */
#define PL_LOCAL       0x0093de90u   /* "PlayerLocal" */
#define PL_CONTROLLER  0x0093ded0u   /* "Controller" */
#define PL_CHANGES     0x00ae0e70u   /* the Player List's change counter (table + 0x68) */

/* ---- boundaries ---- */
/* the original of a register-argument function (oracle), nothing (product) */
static void td_ext(rx_addr fn, uint32_t eax, uint32_t ecx, uint32_t esi, uint32_t edi, int n, const uint32_t* a) {
#ifdef RX_ORACLE
    rx_regs in, o; memset(&in, 0, sizeof in);
    in.r[0] = eax; in.r[1] = ecx; in.r[6] = esi; in.r[7] = edi;
    rx_call_original_ex(fn, &in, &o, n, a);
#else
    (void)fn; (void)eax; (void)ecx; (void)esi; (void)edi; (void)n; (void)a;
#endif
}
/* the session's static (006de1f0 under 0xdcf304 bit 0: rx_stats_static_construct), then the rich presence of a local player: the online session
 * [0x00da6cb0] vf12 (+0x30) (controller, a, b). ONLINE BOUNDARY: the product has no online session (the arena's object is 0). */
static void td_presence(uint32_t controller, uint32_t a, uint32_t b) {
    if (!(U8(0, 0x00dcf304) & 1)) {
        SU32(0, 0x00dcf304, U32(0, 0x00dcf304) | 1);
        ACT_CALL(0, (void)rx_stats_static_construct());
        RX_ATEXIT(0x00913e00);
    }
#ifdef RX_ORACLE
    const rx_addr s = U32(0, 0x00da6cb0);
    const uint32_t args[3] = {controller, a, b};
    rx_call_original(U32(U32(s, 0), 0x30), RX_THISCALL, s, 3, args);
#else
    (void)controller; (void)a; (void)b;
#endif
}
/* the rich presence of a local player (the tutorial mode's too: fx_tutorial_manager.c) */
void rx_ui_presence(uint32_t controller, uint32_t a, uint32_t b) { td_presence(controller, a, b); }
static void td_series_static(void) {
    if (!(U8(0, 0x009e275c) & 1)) {
        SU32(0, 0x009e275c, U32(0, 0x009e275c) | 1);
        ACT_CALL(0, (void)rx_series_manager_construct());
        RX_ATEXIT(0x00911da0);
    }
}

/* ---- the Player List's rows ---- */
static uint32_t pl_rows(void) {
    const rx_addr t = ACT_PLAYER_LIST;
    if (((U32(t, 0x14) - U32(t, 0x10)) >> 2) == 0) return 0;
    return rx_tbl_column_rows(U32(U32(t, 0x10), 0));
}
/* a cell of the row read under the table's lock (the original's EnterCriticalSection 0xae1c44 around the column's cell vector) */
static uint32_t pl_locked_cell(rx_addr name, uint32_t row, uint32_t site) {
    const rx_addr cells = U32(act_column(ACT_PLAYER_LIST, name), 0x40);
    if (row >= (U32(cells, 0x10) - U32(cells, 0xc)) >> 2) act_unreached(site, "row index (_invalid_parameter_noinfo)");
    return U32(U32(cells, 0xc), 4 * row);
}
/* the inlined Table_SetCellInt of the Player List: the change counter when the cell differs, then 004f5330 (column, row, &value) */
static void pl_set(rx_addr name, uint32_t row, rx_addr valp) {
    const uint32_t c = rx_tbl_column_index(ACT_PLAYER_LIST, name);
    if (U32(rx_tbl_cell_word(ACT_PLAYER_LIST, c, row), 0) != U32(valp, 0)) SU32(0, PL_CHANGES, U32(0, PL_CHANGES) + 1);
    if (c >= (U32(ACT_PLAYER_LIST, 0x14) - U32(ACT_PLAYER_LIST, 0x10)) >> 2) crt_invalid_parameter_noinfo();
    ACT_CALL(3, rx_tbl_set_int(U32(U32(ACT_PLAYER_LIST, 0x10), 4 * c), row, valp));
}

/* 005992f0 MXUIManager_SetPlayersInactive (EAX = value; stdcall ui; RET 4): ui+0x284 = value, ui+0x280 = 0 (when it changes); idle (value 1):
 * the split-screen viewports (00757660 (game, 1)), every Player List row inactive (Active 0) and its requests reset (PlayerList_ResetRequests
 * 004de2f0), then the activity area (Game_SetupActivityArea) outside game states 1 / 0xb; active (0): every human row Active 1, the AI rows
 * too unless a series runs (MXSeriesManager +0x190 1 / 2). */
void rx_ui_set_players_inactive(uint32_t value, rx_addr ui) {
    ACT_FRAME_EBP(L, 8, 4, 3);                                       /* push ebp; and esp, -8; push ecx; push ebx / esi / edi */
    const rx_addr local = L + 0xc;
    if (U32(ui, 0x284) == value) { ACT_END(L); return; }
    SU32(ui, 0x284, value);
    SU32(ui, 0x280, 0);
    if (value != 0) {   /* RENDER BOUNDARY: 00757660 (game, 1): the engine's views for one player ("Shared Viewport") */
        const uint32_t a[2] = {U32(0, 0x00a946b8), 1};
        ACT_CALL(0, td_ext(0x00757660, 0, 0, 0, 0, 2, a));
    }
    for (uint32_t r = 0;; r++) {
        if (r >= pl_rows()) break;
        if (U32(ui, 0x284) != 0) {
            SU32(local, 0, 0);
            ACT_CALL(1, pl_set(PL_ACTIVE, r, local));
            ACT_CALL(0, rx_career_player_reset_requests(r, local));   /* PlayerList_ResetRequests (ECX = row, EAX = &local) */
            continue;
        }
        if (act_cell(ACT_PLAYER_LIST, PL_TYPE, r) != 0) {
            td_series_static();
            if (U32(0, 0x009e2318) == 2) continue;
            td_series_static();
            if (U32(0, 0x009e2318) == 1) continue;
        }
        SU32(local, 0, 1);
        ACT_CALL(1, pl_set(PL_ACTIVE, r, local));
    }
    if (U32(ui, 0x284) != 0) {
        const rx_addr g = U32(0, 0x00a946b8);
        if (U32(g, 0x18) != 1 && U32(g, 0x18) != 0xb) ACT_CALL(0, rx_game_setup_activity_area(g));
    }
    ACT_END(L);
}
RX_FUNCTION(0x005992f0, rx_ui_set_players_inactive)

/* 00599550 MXUIManager_EnableTestDrive (ECX = ui, EAX = enable): when it changes: ui+0x290 = enable, the UI's TestDriveEnabled; off: the
 * hub route "GotoGarageVehiclePage" posted with GotoHub (UI); the players idle when off / active when on (005992f0); the local human players'
 * rich presence (on: (controller, 9, 1), (controller, 0, 7) for PlayerLocal != 0; off: (controller, 0, 0) for PlayerLocal == 1). */
void rx_ui_enable_test_drive(rx_addr ui, uint32_t enable) {
    ACT_FRAME_EBP(L, 8, 4, 3);                                       /* push ebp; and esp, -8; push ecx; push ebx / esi / edi */
    if (U32(ui, 0x290) == enable) { ACT_END(L); return; }
    SU32(ui, 0x290, enable);
    {   /* UI BOUNDARY: UIProperty_SetInt (ECX = ui; "TestDriveEnabled", enable) */
        const uint32_t a[2] = {0x0094c358, enable};
        ACT_CALL(0, (void)act_ui_call(0x005dbfb0, RX_THISCALL, ui, 2, a));
    }
    if (U32(ui, 0x290) == 0) {
        /* UI BOUNDARY: UIProperty_SetString (EDI = "UIRoutingMessage" 0x0094cc74, ESI = ui; "GotoGarageVehiclePage"), then the "GotoHub" event
         * (005db500) raised on the UI manager's channel (ui+0x98; 004020c0): the page graph's route to the Test Drive Exit Page */
        const uint32_t a[1] = {0x0094c36c};
        ACT_CALL(0, td_ext(0x005dbf50, 0, 0, ui, 0x0094cc74, 1, a));
#ifdef RX_ORACLE
        rx_regs in, o; memset(&in, 0, sizeof in); in.r[6] = ui;          /* 005db500 (ESI = ui; name; RET 4) */
        const uint32_t name[1] = {0x009424ec};
        rx_call_original_ex(0x005db500, &in, &o, 1, name);
        const rx_addr ev = o.r[0];
        if (ev) {
            const rx_addr channel = U32(ui, 0x98);
            SU32(L, 0xc, ev);
            uint32_t node = 0x009e1a7c;
            rx_call_original(U32(0, 0x00916388), RX_THISCALL, 0x009cdb68, 1, &node);   /* type_info::_name_internal_method */
            memset(&in, 0, sizeof in); in.r[0] = channel; in.r[3] = channel; in.r[6] = ui; in.r[7] = ev;
            const uint32_t b[2] = {L + 0xc, 4};
            rx_call_original_ex(0x004020c0, &in, &o, 2, b);
        }
#endif
    }
    ACT_CALL(1, rx_ui_set_players_inactive(U32(ui, 0x290) == 0, ui));
    const int on = U32(ui, 0x290) != 0;
    for (uint32_t r = 0;; r++) {
        if (r >= pl_rows()) break;
        const uint32_t type = act_cell(ACT_PLAYER_LIST, PL_TYPE, r), loc = act_cell(ACT_PLAYER_LIST, PL_LOCAL, r);
        if (type != 0 || (on ? loc == 0 : loc != 1)) continue;
        const uint32_t controller = act_cell(ACT_PLAYER_LIST, PL_CONTROLLER, r);
        if (on) { td_presence(controller, 9, 1); td_presence(controller, 0, 7); }
        else td_presence(controller, 0, 0);
    }
    ACT_END(L);
}
RX_FUNCTION(0x00599550, rx_ui_enable_test_drive)

/* the garage's own vehicle shown / hidden (007611b0 (show; ESI = garage + 0x520; RET 4)) when the garage has one. The product never loads the
 * garage (MXGarage +0x524 / +0x528 stay 0): reaching it there means a garage the product does not have. */
static void td_garage_vehicle(rx_addr garage, uint32_t show) {
    const rx_addr v = U32(garage, 0x524);
    if (v == 0 || (U32(v, 0xc4) == 0 && U32(garage, 0x528) == 0)) return;
#ifdef RX_ORACLE
    rx_regs in, o; memset(&in, 0, sizeof in); in.r[6] = garage + 0x520;
    const uint32_t a[1] = {show};
    rx_call_original_ex(0x007611b0, &in, &o, 1, a);
#else
    (void)show;
    act_unreached(0x007611b0, "the garage's vehicle (the product never loads the garage)");
#endif
}

/* 00762a20 MXGarage_TestDriveStart (EAX = the garage, MXGame + 0x15570): the garage's vehicle hidden (007611b0 (0)) and its +0x55c model
 * (+0xa2 = 0), the UI world's tutorial mode (00765820), +0x4b0 = 2, the Player List pruned to the local humans (0059e7a0), the local players'
 * request of the FreeRide activity (005a3590 (8, 0, 0)), each local human row Active = 1 with its rich presence, then Game_SetupActivityArea:
 * the UI world's FreeRide activity takes the players (their vehicles and riders spawn in the world's next frames). */
void rx_garage_test_drive_start(rx_addr garage) {
    ACT_FRAME_EBP(L, 8, 4, 3);                                       /* push ebp; and esp, -8; push ecx; push ebx / esi / edi */
    const rx_addr local = L + 0xc;
    ACT_CALL(1, td_garage_vehicle(garage, 0));
    if (U32(garage, 0x55c) != 0) SU8(U32(garage, 0x55c), 0xa2, 0);
    ACT_CALL(0, rx_mxuiworld_tutorial_mode_on(U32(U32(0, 0x00a946b8), 0x20c8)));
    SU32(garage, 0x4b0, 2);
    ACT_CALL(0, rx_player_list_prune());
    ACT_CALL(3, rx_player_list_request_event(8, 0, 0));
    for (uint32_t r = 0;; r++) {
        if (r >= pl_rows()) break;
        const uint32_t type = pl_locked_cell(PL_TYPE, r, 0x00762b28);
        const uint32_t loc = pl_locked_cell(PL_LOCAL, r, 0x00762b90);
        if (type != 0 || loc == 0) continue;
        SU32(local, 0, 1);
        ACT_CALL(1, pl_set(PL_ACTIVE, r, local));
        const uint32_t controller = act_cell(ACT_PLAYER_LIST, PL_CONTROLLER, r);
        td_presence(controller, 9, 1);
        td_presence(controller, 0, 7);
    }
    ACT_CALL(0, rx_game_setup_activity_area(U32(0, 0x00a946b8)));
    ACT_END(L);
}
RX_FUNCTION(0x00762a20, rx_garage_test_drive_start)

/* 00762cc0 MXGarage_TestDriveEnd (EAX = the garage): the garage's vehicle shown (007611b0 (1)), the scenes' render records (00757dc0), the
 * +0x55c model shown (+0xa2 = 1), the UI world's tutorial mode off (007656b0: its lights and the reflection map), each local human row that is
 * active (PlayerLocal 1) inactive with its requests reset and its rich presence, then Game_SetupActivityArea: the players leave the UI world's
 * FreeRide activity. */
void rx_garage_test_drive_end(rx_addr garage) {
    ACT_FRAME_EBP(L, 8, 4, 3);
    const rx_addr local = L + 0xc;
    ACT_CALL(1, td_garage_vehicle(garage, 1));
    ACT_CALL(0, td_ext(0x00757dc0, 0, 0, 0, 0, 0, 0));                /* RENDER BOUNDARY: the scenes' entity render records reset */
    if (U32(garage, 0x55c) != 0) SU8(U32(garage, 0x55c), 0xa2, 1);
    ACT_CALL(0, td_ext(0x007656b0, U32(U32(0, 0x00a946b8), 0x20c8), 0, 0, 0, 0, 0));   /* RENDER BOUNDARY: the UI world's lights / reflection map */
    for (uint32_t r = 0;; r++) {
        if (r >= pl_rows()) break;
        const uint32_t type = pl_locked_cell(PL_TYPE, r, 0x00762db8);
        SU32(local, 0, type);
        const uint32_t loc = pl_locked_cell(PL_LOCAL, r, 0x00762e20);
        if (type != 0 || loc != 1) continue;
        SU32(local, 0, 0);
        ACT_CALL(1, pl_set(PL_ACTIVE, r, local));
        ACT_CALL(0, rx_career_player_reset_requests(r, local));
        const uint32_t controller = act_cell(ACT_PLAYER_LIST, PL_CONTROLLER, r);
        td_presence(controller, 0, 0);
    }
    ACT_CALL(0, rx_game_setup_activity_area(U32(0, 0x00a946b8)));
    ACT_END(L);
}
RX_FUNCTION(0x00762cc0, rx_garage_test_drive_end)
