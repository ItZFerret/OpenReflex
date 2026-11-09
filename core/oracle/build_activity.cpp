// build_activity.cpp -- oracle adapters for construction unit 8 (the activity: event setup, players, drivers, AI, MXGame::Update;
// core/game/build_activity*.c, group rx_build8) and the Rdb additions it needed (row erase / query / delete, build_tables_rdb.c).
// Verified per call inside the boot (REFLEX_PORT=rx_build8 REFLEX_PORT_VERIFY=1 REFLEX_BOOT_SCHED=baton): the core function runs
// on the emulated state, then the original from the same state; memory writes and the declared registers compared. ABI glue only.
#include "oracle.h"
#include "fx_oracle.h"     /* FxBind / AnimBind: the callee-saved register models of the effects and animation code (rx_mxgame_update keeps */
#include "anim_oracle.h"   /* EBX / EBP / ESI / EDI like the original: the ported callees push them into dead stack) */
#include <cstdio>
#include <cstdlib>
extern "C" {
#include "../game/build_tables.h"
#include "../game/build_activity.h"
}
using namespace reflex_port;

#define G "rx_build8"
// ---- Rdb (build_tables_rdb.c) ----
// 004ece50 row erase (stdcall table, row; RET 8)
RX_PORT(0x004ece50, TableEraseRow, G, 0) { rx_tbl_erase_row(m.arg(0), m.arg(1)); return m.ret(8); }
// 004f6e60 Table_SetCellInt (EAX = column name, ESI = table; row, &value; RET 8)
RX_PORT(0x004f6e60, TableSetCellInt, G, 0) { rx_tbl_set_cell_int(m.r[6], m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
// 004eddc0 the rows of a table matching resolved criteria (stdcall table, &criteria, &set; RET 0xc)
RX_PORT(0x004eddc0, TableQuery, G, 0) {
    if (std::getenv("RX_B8_DEBUG")) {   // diagnostics: the criteria (name, column, op, path length) and the out set's table
        uint32_t v = m.arg(1), b = m.mem.read(v + 0xc, 4), e = m.mem.read(v + 0x10, 4);
        std::fprintf(stderr, "query table %08x out table %08x:", m.arg(0), uint32_t(m.mem.read(m.arg(2) + 0x24, 4)));
        for (uint32_t c = b; c < e; c += 0xb0) {
            char name[33] = {}; for (int i = 0; i < 32; i++) name[i] = char(m.mem.read(c + i, 1));
            std::fprintf(stderr, " [%s col %d op %u type %u path %u]", name, int(m.mem.read(c + 0x80, 4)), uint32_t(m.mem.read(c + 0x84, 4)),
                         uint32_t(m.mem.read(c + 0x88, 4)), uint32_t((m.mem.read(c + 0xa8, 4) - m.mem.read(c + 0xa4, 4)) / 4));
        }
        std::fprintf(stderr, "\n");
    }
    rx_tbl_query(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc);
}
// 004ecf00 delete by criteria (stdcall table, &criteria; RET 8) -> EAX
RX_PORT(0x004ecf00, TableDeleteRows, G, OUT_EAX) { m.r[0] = rx_tbl_delete_rows(m.arg(0), m.arg(1)); return m.ret(8); }
// 004ec950 add row (EAX = table; out RowRef, &criteria; RET 8) -> EAX = out
RX_PORT(0x004ec950, TableAddRow, G, OUT_EAX) { m.r[0] = rx_tbl_add_row(m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
// VehicleList_AddRow 004de810 (owner, owner vehicle id; RET 8) / RiderList_AddRow 004dea90 (owner, rider id, gear set; RET 0xc) -> EAX
RX_PORT(0x004de810, VehicleListAddRow, G, OUT_EAX) { m.r[0] = rx_tbl_vehicle_list_add_row(m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x004dea90, RiderListAddRow, G, OUT_EAX) { m.r[0] = rx_tbl_rider_list_add_row(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
// 004f5330 set an int cell (column, row, &value; RET 0xc)
RX_PORT(0x004f5330, TableSetInt, G, 0) { rx_tbl_set_int(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
// ---- the event's table rows (build_activity_setup.c) ----
RX_PORT(0x004dd460, PlayerListAddRow, G, OUT_EAX) { m.r[0] = rx_player_list_add_row(m.arg(0)); return m.ret(4); }
RX_PORT(0x005a1420, PlayerViewports, G, 0) { rx_player_viewports_assign(); return m.ret(0); }
RX_PORT(0x0059e030, EnsureLocalPlayers, G, 0) { rx_ensure_local_players((int32_t)m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0059e310, PlayerRemoveLocal, G, 0) { rx_player_remove_local((int32_t)m.arg(0)); return m.ret(4); }
RX_PORT(0x0059e4e0, PlayerListFreeUnreferenced, G, 0) { rx_player_list_free_unreferenced(); return m.ret(0); }
RX_PORT(0x0059e7a0, PlayerListPrune, G, 0) { rx_player_list_prune(); return m.ret(0); }
// ---- SetupEvent and the AI riders (build_activity_event.c) ----
RX_PORT(0x004ed410, TableFindFirst, G, 0) { rx_tbl_find_first(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x004ed2f0, TableSelect, G, 0) { rx_tbl_select(m.r[1], m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x004dedc0, ActivityTableAddRow, G, OUT_EAX) { m.r[0] = rx_activity_table_add_row(m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0x10); }
RX_PORT(0x004df0f0, ActivityFindRow, G, OUT_EAX) { m.r[0] = (uint32_t)rx_activity_find_row(m.r[1], m.r[0], (int32_t)m.arg(0)); return m.ret(4); }
RX_PORT(0x005a33b0, ActivitySetupRow, G, 0) {
    rx_activity_setup_row(m.r[1], m.r[0], m.arg(0), m.arg(1), m.arg(2), rx_oracle::f32(m.arg(3)), m.arg(4)); return m.ret(0x14);
}
RX_PORT(0x004de3f0, PlayerListHumans, G, OUT_EAX) { m.r[0] = rx_player_list_humans(); return m.ret(0); }
RX_PORT(0x0059e960, AIPlayerAdd, G, OUT_EAX) {
    m.r[0] = rx_ai_player_add(m.r[0], m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4)); return m.ret(0x14);
}
RX_PORT(0x0059ebc0, AIRiderAddOfClass, G, OUT_EAX) { m.r[0] = (uint32_t)rx_ai_rider_add_of_class(m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4)); return m.ret(0x14); }
RX_PORT(0x0059f3e0, AIRosterBuild, G, 0) { rx_ai_roster_build(m.r[0], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4)); return m.ret(0x14); }
RX_PORT(0x0059f2d0, AIRidersSetup, G, 0) { rx_ai_riders_setup(m.arg(0), (int32_t)m.arg(1)); return m.ret(8); }
RX_PORT(0x0059a970, SetupEvent, G, 0) { rx_setup_event(m.r[1]); return m.ret(0); }
// ---- the player slots (build_activity_player.c) ----
RX_PORT(0x006ca750, StuntSystemReset, G, 0) { rx_stunt_system_reset(m.r[6]); return m.ret(0); }
RX_PORT(0x006ca680, StuntSystemConstruct, G, OUT_EAX) { rx_stunt_system_construct(m.r[0]); return m.ret(0); }
RX_PORT(0x005490f0, MXPlayerInit, G, OUT_EAX) { m.r[0] = rx_mxplayer_init(m.arg(0)); return m.ret(4); }
RX_PORT(0x00552ad0, PlayerSlotAlloc, G, OUT_EAX) { m.r[0] = rx_player_slot_alloc(m.r[1]); return m.ret(0); }
RX_PORT(0x00552b20, PlayerFindByName, G, OUT_EAX) { m.r[0] = rx_player_find_by_name(m.r[6], m.arg(0)); return m.ret(4); }
RX_PORT(0x004ddf30, PlayerListSetActivity, G, 0) { rx_player_list_set_activity(m.r[6], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x004de110, PlayerListSetVehicle, G, 0) { rx_player_list_set_vehicle(m.r[6], m.arg(0)); return m.ret(4); }
RX_PORT(0x004de230, PlayerListSetRider, G, 0) { rx_player_list_set_rider(m.r[6], m.arg(0)); return m.ret(4); }
RX_PORT(0x0054a7b0, PlayerFromPlayerList, G, 0) { rx_player_from_player_list(m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0075ba20, PlayersSync, G, OUT_EAX) { m.r[0] = rx_players_sync(m.arg(0)); return m.ret(4); }
// ---- the event-system channels and the activity (build_activity_race.c) ----
RX_PORT(0x00593900, ChannelRegistry, G, OUT_EAX) { m.r[0] = rx_event_channel_registry(); return m.ret(0); }
RX_PORT(0x005930a0, BaseChannelConstruct, G, OUT_EAX) { m.r[0] = rx_event_base_channel_construct(m.r[7], m.arg(0)); return m.ret(4); }
RX_PORT(0x00593520, ChannelRegister, G, 0) { rx_event_channel_register(m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00593950, EventMapInsert, G, OUT_EAX) { m.r[0] = rx_event_map_insert(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x007ce320, EventListHeadAlloc, G, OUT_EAX) { m.r[0] = rx_event_list_head_alloc(); return m.ret(0); }
RX_PORT(0x0055ee60, ActivityManagerInit, G, 0) { rx_activity_manager_init(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x005562c0, ActivityConstruct, G, OUT_EAX) { m.r[0] = rx_activity_construct(m.r[7], m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x005825a0, RaceAudioLogicConstruct, G, OUT_EAX) { m.r[0] = rx_race_audio_logic_construct(m.r[6]); return m.ret(0); }
RX_PORT(0x00565830, RaceStubConstruct, G, OUT_EAX) { m.r[0] = rx_race_stub_construct(m.arg(0), m.r[1], m.r[0], m.arg(1)); return m.ret(8); }
RX_PORT(0x0055ef80, ActivityCreate, G, OUT_EAX) { m.r[0] = rx_activity_create(m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0056be50, ActivitySetMode, G, 0) { rx_activity_set_mode(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00556d90, ActivityVf1, G, 0) { rx_activity_vf1(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00568050, RaceStubVf1, G, 0) { rx_race_stub_vf1(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x007665f0, MXWorldVf3, G, 0) { rx_mxworld_vf3(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00555e60, ActivityAreaName, G, 0) { rx_activity_area_name(m.r[1], m.r[0], m.arg(0), m.arg(1)); return m.ret(0); }
RX_PORT(0x00766250, MXWorldLoadSubscene, G, 0) { rx_mxworld_load_subscene(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00556c30, ActivityVf5, G, 0) { rx_activity_vf5(m.r[1]); return m.ret(0); }
RX_PORT(0x0055a5d0, ActivityTrackName, G, 0) { rx_activity_track_name(m.r[0], m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x007677e0, MXWorldSetTrack, G, 0) { rx_mxworld_set_track(m.r[6], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00556720, ActivityLapPacing, G, 0) { rx_activity_lap_pacing(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0x10); }
RX_PORT(0x00565a30, RaceStubVf5, G, 0) { rx_race_stub_vf5(m.r[1]); return m.ret(0); }
RX_PORT(0x005b9ff0, EventHandlerListPushBack, G, 0) { rx_event_handler_list_push_back(m.arg(0), uint32_t(m.mem.read(m.r[0], 4))); return m.ret(4); }
RX_PORT(0x00592bd0, EventConstruct, G, OUT_EAX) { m.r[0] = rx_event_construct(m.r[6], m.r[0]); return m.ret(0); }
// ---- the world load (build_activity_world.c) ----
RX_PORT(0x00756060, LevelConfigInit, G, 0) { rx_level_config_init(m.arg(0)); return m.ret(4); }
RX_PORT(0x007560f0, LevelLoadRequest, G, 0) { rx_level_load_request(m.r[6], m.arg(0)); return m.ret(4); }
RX_PORT(0x00756250, LevelLoaded, G, OUT_EAX) { m.r[0] = rx_level_loaded_eax(m.r[6], m.r[0]); return m.ret(0); }
RX_PORT(0x007561b0, LevelRelease, G, 0) { rx_level_release(m.r[6]); return m.ret(0); }
RX_PORT(0x00755f80, SceneDirHas, G, OUT_EAX) { m.r[0] = rx_scene_dir_has(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00556140, ActivitySubsceneList, G, 0) { rx_activity_subscene_list(m.arg(0), m.arg(1)); return m.ret(0); }
RX_PORT(0x007cfa10, IntHashInsert, G, 0) { rx_int_hash_insert(m.r[7], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00759f00, WorldConstruct, G, OUT_EAX) { m.r[0] = rx_world_construct(m.arg(0), m.arg(1)); return m.ret(0); }
RX_PORT(0x00767480, MXWorldSetPaused, G, OUT_EAX) { m.r[0] = rx_mxworld_set_paused(m.r[1], m.r[0]); return m.ret(0); }
RX_PORT(0x007669f0, ActivityAreaUpdate, G, OUT_EAX) { m.r[0] = rx_activity_area_update(m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0075c430, GameSetupActivityArea, G, 0) { rx_game_setup_activity_area(m.r[6]); return m.ret(0); }
// ---- StartEvent (build_activity_event.c) ----
RX_PORT(0x005063c0, TableSetCellFloat, G, 0) { rx_tbl_set_cell_float(m.r[6], m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x004dde70, PlayerListRequestActivity, G, 0) { rx_player_list_request_activity(m.r[6], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x005a3590, PlayerListRequestEvent, G, 0) { rx_player_list_request_event(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0); }
RX_PORT(0x005a3750, ActivityRowsKeep, G, 0) { rx_activity_rows_keep(m.arg(0), m.arg(1)); return m.ret(0); }
RX_PORT(0x0075c640, GameRequestLevel, G, 0) { rx_game_request_level(m.r[0]); return m.ret(0); }
RX_PORT(0x0075c8c0, GameResumeFromUI, G, OUT_EAX) { m.r[0] = rx_game_resume_from_ui(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0059bd60, UIRequestLevel, G, 0) { rx_ui_request_level(m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0059ade0, StartEvent, G, 0) { rx_start_event(m.r[1]); return m.ret(0); }
// the event-type registrations (one instantiation per handler signature: EAX = channel; the name as the stack argument (RET 4) or
// fixed in the code (RET 0); the stack depth of its type_info call): generated from the executable. The UI's debug-built ones
// (005f1560 ..: ECX = channel, another code shape) are not core and stay the original.
RX_PORT(0x0041a580, EventType_0041a580, G, 0) { rx_event_type_register_at(m.r[0], m.arg(0), 0x9cbb7c, 1, -136); return m.ret(4); }
RX_PORT(0x0041a6a0, EventType_0041a6a0, G, 0) { rx_event_type_register_at(m.r[0], m.arg(0), 0x9cbbac, 1, -136); return m.ret(4); }
RX_PORT(0x0041a7c0, EventType_0041a7c0, G, 0) { rx_event_type_register_at(m.r[0], 0x936160, 0x9cbbdc, 0, -132); return m.ret(0); }
RX_PORT(0x0041a8d0, EventType_0041a8d0, G, 0) { rx_event_type_register_at(m.r[0], 0x9361f4, 0x9cbc00, 0, -132); return m.ret(0); }
RX_PORT(0x0041a9e0, EventType_0041a9e0, G, 0) { rx_event_type_register_at(m.r[0], m.arg(0), 0x9cbc38, 0, -128); return m.ret(4); }
RX_PORT(0x00586ca0, EventType_00586ca0, G, 0) { rx_event_type_register_at(m.r[0], m.arg(0), 0x9cf3f0, 1, -136); return m.ret(4); }
RX_PORT(0x00590c00, EventType_00590c00, G, 0) { rx_event_type_register_at(m.r[0], m.arg(0), 0x9cbd24, 1, -136); return m.ret(4); }
RX_PORT(0x00590d20, EventType_00590d20, G, 0) { rx_event_type_register_at(m.r[0], 0x941ba8, 0x9d1d0c, 1, -136); return m.ret(0); }
RX_PORT(0x00590e40, EventType_00590e40, G, 0) { rx_event_type_register_at(m.r[0], 0x941bb8, 0x9cf5d4, 1, -136); return m.ret(0); }
RX_PORT(0x00590f60, EventType_00590f60, G, 0) { rx_event_type_register_at(m.r[0], m.arg(0), 0x9cda44, 1, -136); return m.ret(4); }
RX_PORT(0x00591080, EventType_00591080, G, 0) { rx_event_type_register_at(m.r[0], m.arg(0), 0x9cf610, 1, -136); return m.ret(4); }
RX_PORT(0x005911a0, EventType_005911a0, G, 0) { rx_event_type_register_at(m.r[0], m.arg(0), 0x9cf5b4, 1, -136); return m.ret(4); }
RX_PORT(0x005912c0, EventType_005912c0, G, 0) { rx_event_type_register_at(m.r[0], m.arg(0), 0x9cdb7c, 1, -136); return m.ret(4); }
RX_PORT(0x005913e0, EventType_005913e0, G, 0) { rx_event_type_register_at(m.r[0], m.arg(0), 0x9cf660, 1, -136); return m.ret(4); }
RX_PORT(0x00591500, EventType_00591500, G, 0) { rx_event_type_register_at(m.r[0], m.arg(0), 0x9cda18, 1, -136); return m.ret(4); }
RX_PORT(0x00591620, EventType_00591620, G, 0) { rx_event_type_register_at(m.r[0], 0x941ca8, 0x9cf630, 1, -136); return m.ret(0); }
RX_PORT(0x00591740, EventType_00591740, G, 0) { rx_event_type_register_at(m.r[0], m.arg(0), 0x9cd9f8, 1, -136); return m.ret(4); }
RX_PORT(0x00591860, EventType_00591860, G, 0) { rx_event_type_register_at(m.r[0], 0x942870, 0x9ce508, 1, -136); return m.ret(0); }
RX_PORT(0x00591980, EventType_00591980, G, 0) { rx_event_type_register_at(m.r[0], m.arg(0), 0x9cbe2c, 1, -136); return m.ret(4); }
RX_PORT(0x00591aa0, EventType_00591aa0, G, 0) { rx_event_type_register_at(m.r[0], 0x9419b0, 0x9cf7a4, 1, -136); return m.ret(0); }
RX_PORT(0x00591bc0, EventType_00591bc0, G, 0) { rx_event_type_register_at(m.r[0], 0x9416b0, 0x9ce4e4, 1, -136); return m.ret(0); }
RX_PORT(0x00591ce0, EventType_00591ce0, G, 0) { rx_event_type_register_at(m.r[0], 0x9407c4, 0x9ce4b0, 1, -136); return m.ret(0); }
RX_PORT(0x00591e00, EventType_00591e00, G, 0) { rx_event_type_register_at(m.r[0], m.arg(0), 0x9cdaa8, 1, -136); return m.ret(4); }
RX_PORT(0x00591f20, EventType_00591f20, G, 0) { rx_event_type_register_at(m.r[0], 0x941f54, 0x9d1d38, 1, -136); return m.ret(0); }
RX_PORT(0x00592040, EventType_00592040, G, 0) { rx_event_type_register_at(m.r[0], m.arg(0), 0x9cda64, 1, -136); return m.ret(4); }
RX_PORT(0x00592160, EventType_00592160, G, 0) { rx_event_type_register_at(m.r[0], 0x93ef0c, 0x9cda88, 1, -136); return m.ret(0); }
RX_PORT(0x00592280, EventType_00592280, G, 0) { rx_event_type_register_at(m.r[0], 0x93ef90, 0x9cdb00, 1, -136); return m.ret(0); }
RX_PORT(0x005923a0, EventType_005923a0, G, 0) { rx_event_type_register_at(m.r[0], 0x941710, 0x9ce550, 1, -136); return m.ret(0); }
RX_PORT(0x005924c0, EventType_005924c0, G, 0) { rx_event_type_register_at(m.r[0], m.arg(0), 0x9cdad8, 1, -136); return m.ret(4); }
RX_PORT(0x005925e0, EventType_005925e0, G, 0) { rx_event_type_register_at(m.r[0], 0x9421b0, 0x9cf680, 1, -136); return m.ret(0); }
RX_PORT(0x00592700, EventType_00592700, G, 0) { rx_event_type_register_at(m.r[0], 0x93efa4, 0x9cdb30, 1, -136); return m.ret(0); }
RX_PORT(0x00592820, EventType_00592820, G, 0) { rx_event_type_register_at(m.r[0], m.arg(0), 0x9cdb68, 1, -136); return m.ret(4); }
RX_PORT(0x00592940, EventType_00592940, G, 0) { rx_event_type_register_at(m.r[0], 0x9424f4, 0x9cf774, 1, -136); return m.ret(0); }
RX_PORT(0x005c0bb0, EventType_005c0bb0, G, 0) { rx_event_type_register_at(m.r[0], m.arg(0), 0x9d24bc, 1, -136); return m.ret(4); }
RX_PORT(0x005c0cd0, EventType_005c0cd0, G, 0) { rx_event_type_register_at(m.r[0], m.arg(0), 0x9d24d0, 1, -136); return m.ret(4); }
RX_PORT(0x005c0df0, EventType_005c0df0, G, 0) { rx_event_type_register_at(m.r[0], m.arg(0), 0x9cbeb4, 1, -136); return m.ret(4); }
RX_PORT(0x005c0f20, EventType_005c0f20, G, 0) { rx_event_type_register_at(m.r[0], 0x94c62c, 0x9d31f8, 0, -132); return m.ret(0); }
RX_PORT(0x005c1030, EventType_005c1030, G, 0) { rx_event_type_register_at(m.r[0], 0x94c654, 0x9d2500, 0, -132); return m.ret(0); }
RX_PORT(0x005c1140, EventType_005c1140, G, 0) { rx_event_type_register_at(m.r[0], 0x94c68c, 0x9d27d4, 0, -132); return m.ret(0); }
RX_PORT(0x005c1250, EventType_005c1250, G, 0) { rx_event_type_register_at(m.r[0], 0x94c8c8, 0x9d320c, 0, -132); return m.ret(0); }
RX_PORT(0x005c13e0, EventType_005c13e0, G, 0) { rx_event_type_register_at(m.r[0], 0x94c810, 0x9d28c4, 1, -136); return m.ret(0); }
RX_PORT(0x005c1500, EventType_005c1500, G, 0) { rx_event_type_register_at(m.r[0], m.arg(0), 0x9d256c, 1, -136); return m.ret(4); }
RX_PORT(0x005c1620, EventType_005c1620, G, 0) { rx_event_type_register_at(m.r[0], m.arg(0), 0x9d2514, 1, -136); return m.ret(4); }
RX_PORT(0x005c1740, EventType_005c1740, G, 0) { rx_event_type_register_at(m.r[0], m.arg(0), 0x9d2540, 1, -136); return m.ret(4); }
RX_PORT(0x005c1860, EventType_005c1860, G, 0) { rx_event_type_register_at(m.r[0], 0x94c8a8, 0x9d2528, 1, -136); return m.ret(0); }
RX_PORT(0x005c1b40, EventType_005c1b40, G, 0) { rx_event_type_register_at(m.r[0], 0x94d970, 0x9d3224, 0, -132); return m.ret(0); }
RX_PORT(0x006ce210, EventType_006ce210, G, 0) { rx_event_type_register_at(m.r[0], m.arg(0), 0x9cbf38, 0, -128); return m.ret(4); }
RX_PORT(0x006ce320, EventType_006ce320, G, 0) { rx_event_type_register_at(m.r[0], 0x9607b4, 0x9d9a04, 0, -132); return m.ret(0); }
RX_PORT(0x006ce430, EventType_006ce430, G, 0) { rx_event_type_register_at(m.r[0], m.arg(0), 0x9ce768, 0, -128); return m.ret(4); }
RX_PORT(0x006ce540, EventType_006ce540, G, 0) { rx_event_type_register_at(m.r[0], 0x92fc60, 0x9cbda8, 1, -136); return m.ret(0); }
RX_PORT(0x006ce660, EventType_006ce660, G, 0) { rx_event_type_register_at(m.r[0], 0x92fca0, 0x9d9c80, 1, -136); return m.ret(0); }
RX_PORT(0x006d4470, EventType_006d4470, G, 0) { rx_event_type_register_at(m.r[0], m.arg(0), 0x9ce284, 0, -128); return m.ret(4); }
RX_PORT(0x006d5720, EventType_006d5720, G, 0) { rx_event_type_register_at(m.r[0], m.arg(0), 0x9d74fc, 0, -128); return m.ret(4); }
RX_PORT(0x006d5830, EventType_006d5830, G, 0) { rx_event_type_register_at(m.r[0], 0x961170, 0x9d9f54, 0, -132); return m.ret(0); }
RX_PORT(0x00709c80, EventType_00709c80, G, 0) { rx_event_type_register_at(m.r[0], 0x94cba8, 0x9d2a9c, 1, -136); return m.ret(0); }
RX_PORT(0x00709da0, EventType_00709da0, G, 0) { rx_event_type_register_at(m.r[0], 0x96286c, 0x9dc878, 1, -136); return m.ret(0); }
RX_PORT(0x00709ec0, EventType_00709ec0, G, 0) { rx_event_type_register_at(m.r[0], m.arg(0), 0x9cdc7c, 1, -136); return m.ret(4); }
RX_PORT(0x00709fe0, EventType_00709fe0, G, 0) { rx_event_type_register_at(m.r[0], 0x958098, 0x9d7580, 1, -136); return m.ret(0); }
RX_PORT(0x0070a100, EventType_0070a100, G, 0) { rx_event_type_register_at(m.r[0], 0x941d30, 0x9cf8a8, 1, -136); return m.ret(0); }
RX_PORT(0x0070a220, EventType_0070a220, G, 0) { rx_event_type_register_at(m.r[0], 0x941d1c, 0x9cf81c, 1, -136); return m.ret(0); }
RX_PORT(0x0070a340, EventType_0070a340, G, 0) { rx_event_type_register_at(m.r[0], 0x93eed8, 0x9cdd3c, 1, -136); return m.ret(0); }
RX_PORT(0x0070a460, EventType_0070a460, G, 0) { rx_event_type_register_at(m.r[0], 0x93ef78, 0x9cde24, 1, -136); return m.ret(0); }
RX_PORT(0x0070a580, EventType_0070a580, G, 0) { rx_event_type_register_at(m.r[0], 0x942144, 0x9d0140, 1, -136); return m.ret(0); }
RX_PORT(0x0070a6a0, EventType_0070a6a0, G, 0) { rx_event_type_register_at(m.r[0], 0x942230, 0x9d024c, 1, -136); return m.ret(0); }
RX_PORT(0x0070a7c0, EventType_0070a7c0, G, 0) { rx_event_type_register_at(m.r[0], 0x941ff4, 0x9cfcb0, 1, -136); return m.ret(0); }
RX_PORT(0x0070a8e0, EventType_0070a8e0, G, 0) { rx_event_type_register_at(m.r[0], 0x9420fc, 0x9cffcc, 1, -136); return m.ret(0); }
RX_PORT(0x0070aa00, EventType_0070aa00, G, 0) { rx_event_type_register_at(m.r[0], 0x9423c4, 0x9d0820, 1, -136); return m.ret(0); }
RX_PORT(0x0070ab20, EventType_0070ab20, G, 0) { rx_event_type_register_at(m.r[0], 0x942280, 0x9d0420, 1, -136); return m.ret(0); }
RX_PORT(0x0070ac40, EventType_0070ac40, G, 0) { rx_event_type_register_at(m.r[0], 0x94229c, 0x9d0494, 1, -136); return m.ret(0); }
RX_PORT(0x0070ad60, EventType_0070ad60, G, 0) { rx_event_type_register_at(m.r[0], 0x9422b4, 0x9d0520, 1, -136); return m.ret(0); }
RX_PORT(0x0070ae80, EventType_0070ae80, G, 0) { rx_event_type_register_at(m.r[0], 0x942350, 0x9d0710, 1, -136); return m.ret(0); }
RX_PORT(0x0070afa0, EventType_0070afa0, G, 0) { rx_event_type_register_at(m.r[0], m.arg(0), 0x9d2c04, 1, -136); return m.ret(4); }
RX_PORT(0x0070b0c0, EventType_0070b0c0, G, 0) { rx_event_type_register_at(m.r[0], 0x9628cc, 0x9dc8a0, 1, -136); return m.ret(0); }
RX_PORT(0x0070b1e0, EventType_0070b1e0, G, 0) { rx_event_type_register_at(m.r[0], 0x962900, 0x9dcd00, 1, -136); return m.ret(0); }
RX_PORT(0x0070b300, EventType_0070b300, G, 0) { rx_event_type_register_at(m.r[0], 0x94cbb8, 0x9d2ba4, 1, -136); return m.ret(0); }
RX_PORT(0x0070b420, EventType_0070b420, G, 0) { rx_event_type_register_at(m.r[0], 0x962914, 0x9dcac4, 1, -136); return m.ret(0); }
RX_PORT(0x0070b540, EventType_0070b540, G, 0) { rx_event_type_register_at(m.r[0], 0x941d44, 0x9cf940, 1, -136); return m.ret(0); }
RX_PORT(0x0070b660, EventType_0070b660, G, 0) { rx_event_type_register_at(m.r[0], 0x941354, 0x9ce5f4, 1, -136); return m.ret(0); }
RX_PORT(0x0070b780, EventType_0070b780, G, 0) { rx_event_type_register_at(m.r[0], 0x941368, 0x9ce674, 1, -136); return m.ret(0); }
RX_PORT(0x0070b8a0, EventType_0070b8a0, G, 0) { rx_event_type_register_at(m.r[0], 0x962930, 0x9dcd28, 1, -136); return m.ret(0); }
RX_PORT(0x0070b9c0, EventType_0070b9c0, G, 0) { rx_event_type_register_at(m.r[0], 0x941d60, 0x9cf9a4, 1, -136); return m.ret(0); }
RX_PORT(0x0070bae0, EventType_0070bae0, G, 0) { rx_event_type_register_at(m.r[0], 0x962944, 0x9dcd4c, 1, -136); return m.ret(0); }
RX_PORT(0x0070bc00, EventType_0070bc00, G, 0) { rx_event_type_register_at(m.r[0], 0x94cbe8, 0x9d2c68, 1, -136); return m.ret(0); }
RX_PORT(0x00763360, EventType_00763360, G, 0) { rx_event_type_register_at(m.r[0], 0x96d868, 0x9dd6d0, 1, -136); return m.ret(0); }
RX_PORT(0x00763480, EventType_00763480, G, 0) { rx_event_type_register_at(m.r[0], 0x96d890, 0x9dd6f0, 1, -136); return m.ret(0); }
// ---- a player joins (build_activity_driver.c) ----
RX_PORT(0x006c9100, ControlsReset, G, 0) { rx_controls_reset(m.r[0]); return m.ret(0); }
RX_PORT(0x00508c10, BaseDriverConstruct, G, OUT_EAX) { m.r[0] = rx_base_driver_construct(m.r[6], m.r[0]); return m.ret(0); }
RX_PORT(0x0050e1b0, NetworkDriverConstruct, G, OUT_EAX) { m.r[0] = rx_network_driver_construct(m.r[7], m.arg(0)); return m.ret(4); }
RX_PORT(0x0050ed80, RaceDriverConstruct, G, OUT_EAX) { m.r[0] = rx_race_driver_construct(m.r[1], m.r[0]); return m.ret(0); }
RX_PORT(0x00566ea0, RaceStubVf44, G, OUT_EAX) { m.r[0] = rx_race_stub_vf44(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00565e00, RaceStubVf47, G, 0) { rx_race_stub_vf47(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00508f50, DriverBehaviorsReset, G, 0) { rx_driver_behaviors_reset(m.arg(0)); return m.ret(4); }
RX_PORT(0x006cdd50, StuntScoringListen, G, 0) { rx_stunt_scoring_listen(m.arg(0)); return m.ret(4); }
RX_PORT(0x0054d9d0, PlayerJoinActivity, G, 0) { rx_player_join_activity(m.r[6], m.r[0]); return m.ret(0); }
RX_PORT(0x0054cd20, PlayerStartZone, G, 0) { rx_player_start_zone(m.arg(0)); return m.ret(4); }
RX_PORT(0x0055b610, ActivityOnPlayerJoined, G, OUT_EAX) { m.r[0] = rx_activity_on_player_joined(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0050e380, NetworkDriverVf1, G, 0) { rx_network_driver_vf1(m.r[1]); return m.ret(0); }
RX_PORT(0x0050ee40, RaceDriverVf1, G, 0) { rx_race_driver_vf1(m.r[1]); return m.ret(0); }
RX_PORT(0x00757fb0, MXGameUpdate, G, OUT_EAX) { FxBind fb_(m); AnimBind ab_(m); m.r[0] = rx_mxgame_update(m.r[1]); return m.ret(0); }
RX_PORT(0x007668e0, WorldActivitiesStart, G, 0) { rx_world_activities_start(m.arg(0)); return m.ret(4); }
