/* build_activity.h -- construction unit 8 "activity" (core/notes/construction.md section 14): from an event spec to a running race:
 * the event's table rows (Player / Vehicle / Rider List, the activity row, the AI roster), the activity (MXActivityRace and its
 * entities), the players (MXPlayer slots), the drivers (RaceDriver) and their AI behaviours, and MXGame::Update's loading states
 * that order it all (with the world-load requests).
 *
 * The original takes the event from the UI manager's properties (MXUIManager::SetupEvent / StartEvent read EventType, EventLocation,
 * AIDifficulty, ... set by the front end's Lua). The core takes them from an rx_event_spec (the Unity front end fills it); the UI
 * itself is not core: calls into UI-only code are made through act_ui_* (oracle: the original runs; product: nothing), and only the
 * UI state that gameplay code reads is kept (construction.md 14.3 lists both kinds).
 *
 * Calling conventions of the original are in each prototype's comment (EAX / ECX / ESI / EDI = register arguments, sN = N-th stack
 * argument, RET n = callee cleanup). */
#pragma once
#include "rx_game.h"
#include "../src/rx_x87.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ---- the event spec (what the original's UI properties hold when SetupEvent runs) ---- */
typedef struct rx_event_spec {
    char     event_type[32];       /* UI EventType: "National", "Supercross", "FreeRide", ... (enum names 0x009d2220) */
    int32_t  event_type_id;        /* EventTypeId (table id, another domain than the enum) */
    char     event_activation[32]; /* EventTypeActivation (a Free Ride challenge), "" = none */
    char     location[64];         /* EventLocation (the level: "NAT_Concord") */
    int32_t  location_id;          /* EventLocationId */
    int32_t  track_id;             /* TrackId */
    int32_t  duration;             /* EventDuration (laps / minutes) */
    char     duration_type[16];    /* EventDurationType: "Laps", "Minutes", "" */
    int32_t  riders;               /* EventRiders (players + AI) */
    int32_t  ai_difficulty;        /* AIDifficulty (0..120) */
    int32_t  vehicle_class;        /* VehicleClass */
    char     player_mode[16];      /* PlayerMode: "SinglePlayer" */
    uint16_t player_name[68];      /* the profile's display name (MXProfile_GetDisplayName: localized "Player 1"), UTF-16 */
} rx_event_spec;
void rx_event_spec_default(rx_event_spec* e);   /* the reference event: National NAT_Concord, 12 riders, AI 95 */
void rx_event_set_spec(const rx_event_spec* e); /* the spec the setup reads (copied) */
const rx_event_spec* rx_event_spec_get(void);

/* ---- the event's table rows (build_activity_setup.c) ---- */
uint32_t rx_player_list_add_row(rx_addr player);              /* 004dd460 (stdcall &player record; RET 4) -> the row */
void     rx_player_viewports_assign(void);                    /* 005a1420 */
void     rx_ensure_local_players(int32_t controller, uint32_t activity);   /* EnsureLocalPlayers 0059e030 (stdcall; RET 8) */
/* The profile layer's two leaves the career's post-event code also calls (MXSeriesEventHeat::vf13 005adde0): MXProfile_GetDisplayName
 * 00616840 for the profile of `controller` (oracle: the original; product: the spec's player name, wcsncpy 0x43 + terminator) and
 * WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, n, 0, 0) (oracle: the import; product: the same conversion). */
void     rx_profile_display_name(int32_t controller, rx_addr wide_buffer);
void     rx_wide_to_utf8(rx_addr wide, rx_addr out, uint32_t capacity);
void     rx_player_remove_local(int32_t controller);          /* 0059e310 (stdcall; RET 4) */
void     rx_player_list_free_unreferenced(void);              /* PlayerList_FreeUnreferencedRows 0059e4e0 */
void     rx_player_list_prune(void);                          /* 0059e7a0 */

/* ---- SetupEvent and the AI riders (build_activity_event.c) ---- */
uint32_t rx_event_type_of(rx_addr name);                      /* 005964b0 (cdecl): the event type enum (0x009d2220 names) */
uint32_t rx_duration_type_of(rx_addr name);                   /* 00596610 (cdecl): Laps / Minutes / ... */
uint32_t rx_activity_mode_of(rx_addr name);                   /* 00596580 (cdecl): Ambient / Exhibition / ... */
uint32_t rx_activity_table_add_row(rx_addr table, uint32_t type, uint32_t area, uint32_t mode);   /* ActivityTable_AddRow 004dedc0 (RET 0x10) */
int32_t  rx_activity_find_row(rx_addr table, uint32_t type, int32_t area);   /* 004df0f0 (ECX = table, EAX = type; area; RET 4) */
void     rx_activity_setup_row(uint32_t type, uint32_t mode, rx_addr ui, uint32_t area, uint32_t laps, float duration, uint32_t spectator);
                                                              /* 005a33b0 (ECX = type, EAX = mode; ui, area, laps, duration, spectator; RET 0x14) */
uint32_t rx_player_list_humans(void);                         /* 004de3f0 */
uint32_t rx_ai_player_add(uint32_t gear, uint32_t rider, uint32_t vehicle, rx_addr name, uint32_t activity, uint32_t area, uint32_t bucket);
                                                              /* MXUIManager_AddAIPlayer 0059e960 (EAX = gear, ECX = rider; 5 stack; RET 0x14) */
int32_t  rx_ai_rider_add_of_class(uint32_t rider, uint32_t cls, uint32_t activity, uint32_t area, uint32_t bucket);   /* 0059ebc0 (RET 0x14) */
void     rx_ai_roster_build(uint32_t difficulty, rx_addr ui, uint32_t count, uint32_t activity, uint32_t area, uint32_t cls);
                                                              /* MXUIManager_BuildAIRoster 0059f3e0 (EAX = difficulty; 5 stack; RET 0x14) */
void     rx_ai_riders_setup(rx_addr ui, int32_t count);       /* MXUIManager_SetupAIRiders 0059f2d0 (RET 8) */
void     rx_setup_event(rx_addr ui);                          /* MXUIManager::SetupEvent 0059a970 (ECX = ui; 0 in the product) */

/* ---- the player slots (build_activity_player.c) ---- */
void     rx_stunt_system_reset(rx_addr s);                    /* 006ca750 (ESI = s) */
void     rx_stunt_system_construct(rx_addr s);                /* 006ca680 (EAX = s) */
rx_addr  rx_mxplayer_init(rx_addr P);                         /* 005490f0 (stdcall P; RET 4) */
rx_addr  rx_player_slot_alloc(rx_addr A);                     /* 00552ad0 (ECX = MXGame + 0x7460) */
rx_addr  rx_player_find_by_name(rx_addr A, rx_addr name);     /* 00552b20 (ESI = A; name; RET 4) */
rx_addr  rx_player_active(rx_addr A, uint32_t index, uint32_t kind);   /* 00552b90 (EDX = A; index, kind; RET 8) */
void     rx_player_list_set_activity(uint32_t row, uint32_t type, uint32_t area);   /* 004ddf30 (ESI = row; type, area; RET 8) */
void     rx_player_list_set_vehicle(uint32_t row, uint32_t vid);   /* 004de110 (ESI = row; id; RET 4) */
void     rx_player_list_set_rider(uint32_t row, uint32_t rid);     /* 004de230 (ESI = row; id; RET 4) */
void     rx_player_from_player_list(rx_addr P, rx_addr ref);  /* PlayerSlot_FromPlayerList 0054a7b0 (stdcall P, &row ref; RET 8) */
uint32_t rx_players_sync(rx_addr game);                       /* 0075ba20 (stdcall game; RET 4): the player slots from the Player List */

/* ---- the controller's key maps (build_activity_controller.c) ---- */
uint32_t rx_controller_build_key_maps(uint32_t controller, uint32_t preset, uint32_t handedness, uint32_t motion);   /* Controller_BuildKeyMaps 00547f30 (cdecl; RET 0) -> the controller's KeyMap block */

/* ---- the player slot's vehicle / rider spawn (build_activity_spawn.c) ---- */
uint32_t rx_player_slot_spawn_step(rx_addr P, rx_addr world);  /* PlayerSlot_SpawnStep 00767200 (EAX = P, stack world; RET 4) -> 1 spec, 2 vehicle, 3 rider entry, 4 rider */
void     rx_player_vehicle_clear(rx_addr P);                   /* 0054a500 (stdcall P; RET 4) */
void     rx_player_spawn_spec(rx_addr P, uint32_t flag);       /* 00549f30 (EAX = P, stack flag; RET 4) */
uint32_t rx_player_vehicle_spawn(rx_addr world, rx_addr P);    /* 0054a050 (EAX = world, stack P; RET 4) -> 1 */
uint32_t rx_player_rider_spawn(rx_addr P, rx_addr world);      /* 0054a320 (EAX = P, ECX = world; RET 0) -> 1 */
void     rx_player_rider_attach(rx_addr P, uint32_t flag);     /* 0054a3f0 (EDI = P, stack flag; RET 4) */
uint32_t rx_part_packages_ready(rx_addr part, uint32_t hr);    /* 0053fa30 (ECX = part, stack hr; RET 4) -> ready */
uint32_t rx_vehicle_spec_ready(rx_addr list, uint32_t id);     /* 00542ac0 (ECX = list, EAX = id; RET 0) -> ready */
uint32_t rx_rider_entry_parts_ready(rx_addr entry);            /* 0053cea0 (stdcall entry; RET 4) -> ready */
uint32_t rx_rider_entry_ready(rx_addr mgr, uint32_t id);       /* 0053e880 (ECX = mgr, EDI = id; RET 0) -> ready */

/* ---- the startup table (build_activity_grid.c) ---- */
uint32_t rx_startup_table_value(uint32_t key);             /* FUN_004df6a0 (EAX = key) -> EAX: the startup table's int of the key's row, or -1 */

/* ---- the event-system channels and the activity (build_activity_race.c) ---- */
rx_addr  rx_event_channel_create(rx_addr name);              /* a new EventSystem::Channel registered under the name (as 0055ee60) */
rx_addr  rx_event_channel_registry(void);                    /* 00593900: the channel registry 0x00dcbeb4 (its static initializer) */
rx_addr  rx_event_base_channel_construct(rx_addr ch, rx_addr parent);   /* 005930a0 (EDI = ch; stdcall parent; RET 4) */
void     rx_event_channel_register(rx_addr registry, rx_addr name, rx_addr ch);   /* 00593520 (EAX = registry; name, ch; RET 8) */
rx_addr  rx_event_map_insert(rx_addr map, rx_addr out, rx_addr pair);   /* 00593950 / 00419d60 map<string, T*>::insert (RET 0xc) */
rx_addr  rx_event_list_head_alloc(void);                     /* 007ce320 */
/* 005b9ff0 (EAX = &handler; list; RET 4): std::list<Handler*>::push_back on an event's handler list (event + 8: proxy, +0x14 head,
 * +0x18 size; node 0xc: next, prev, value) */
void     rx_event_handler_list_push_back(rx_addr list, rx_addr handler);
rx_addr  rx_event_construct(rx_addr ev, rx_addr signature);  /* EventSystem::Event 00592bd0 (ESI = ev, EAX = signature type name) */
/* an event type on a channel (FUN_00590c00 and its instantiations: EAX = channel; stdcall name; RET 4): signature_type = the RTTI
 * type descriptor of the handler signature the instantiation names (0x009cbd24 = void (*)(Activity*), ...) */
void     rx_event_type_register(rx_addr ch, rx_addr name, rx_addr signature_type);
/* the same with the instantiation's stack depth at its type_info call (entry ESP + call_sp, or ((entry - 4) & ~7) + call_sp when its
 * frame is aligned): the CRT's undecoration keeps that stack address */
void     rx_event_type_register_at(rx_addr ch, rx_addr name, rx_addr signature_type, int aligned, int32_t call_sp);
void     rx_activity_manager_init(rx_addr mgr, rx_addr world, rx_addr level);   /* 0055ee60 (stdcall; RET 0xc) */
rx_addr  rx_activity_construct(rx_addr A, uint32_t type, rx_addr world);   /* Activity 005562c0 (EDI = A, ECX = type; world; RET 4) */
rx_addr  rx_race_audio_logic_construct(rx_addr l);           /* RaceActivityAudioLogic 005825a0 (ESI = l) */
rx_addr  rx_race_stub_construct(rx_addr A, uint32_t type, rx_addr world, rx_addr audio);   /* 00565830 (ECX = type, EAX = world; A, audio; RET 8) */
rx_addr  rx_activity_create(rx_addr mgr, uint32_t type);     /* the activity factory 0055ef80 (stdcall; RET 8) */
void     rx_activity_set_mode(rx_addr A, uint32_t mode);     /* Activity::vf22 0056be50 (thiscall; RET 4) */
void     rx_activity_vf1(rx_addr A, rx_addr ref);            /* Activity::vf1 00556d90 (thiscall &row ref; RET 4) */
void     rx_race_stub_vf1(rx_addr A, rx_addr ref);           /* MXActivityRaceStub::vf1 00568050 (thiscall &row ref; RET 4) */
void     rx_mxworld_vf3(rx_addr w, rx_addr table);
struct fmt_level;
void     rx_event_set_level(struct fmt_level* lv);            /* the level the event's subscenes are read from (product) */
struct fmt_level* rx_event_level(void);
void     rx_activity_area_name(rx_addr level, uint32_t type, rx_addr buf, uint32_t area);   /* 00555e60 (ECX = level, EAX = type; cdecl) */
void     rx_scene_request_subscene(rx_addr path, rx_addr config, rx_addr name, uint32_t slot);   /* 007564f0 (ECX = path; RET 0xc) */
void     rx_mxworld_load_subscene(rx_addr w, rx_addr name);   /* 00766250 (ECX = world; RET 4) */
void     rx_activity_vf5(rx_addr A);                          /* Activity::vf5 00556c30 (ECX) */
void     rx_activity_track_name(rx_addr buf, uint32_t type, rx_addr A);   /* 0055a5d0 (EAX = buf, ECX = type; A; RET 4) */
void     rx_mxworld_set_track(rx_addr w, rx_addr name, rx_addr audio);   /* 007677e0 (ESI = world; RET 8) */
void     rx_activity_lap_pacing(rx_addr level, rx_addr A, uint32_t kind, uint32_t area, uint32_t from_file);   /* 00556720 (ECX = level; RET 0x10) */
void     rx_race_stub_vf5(rx_addr A);                         /* MXActivityRaceStub::vf5 00565a30 (ECX) */           /* MXWorld::vf3 007665f0 (thiscall the activity table; RET 4) */

/* ---- the track definition, .tdf07 (build_activity_track.c) ---- */
uint32_t rx_track_load(rx_addr level, rx_addr block, uint32_t kind, uint32_t area);   /* FUN_00536ef0 (ECX = level; block, kind, area; RET 0xc) -> EAX = loaded */
void     rx_track_asset_name(rx_addr level, uint32_t kind, rx_addr out, uint32_t area);   /* 00536dc0 (ESI = level, ECX = kind, EDI = out; area; RET 0) */
void     rx_track_file_path(rx_addr level, uint32_t kind, rx_addr out, uint32_t area);    /* 00536d00 (same; "AI\\<name>.tdf07") */
uint32_t rx_track_has_asset(uint32_t kind, rx_addr level, uint32_t area);   /* 00536e80 (EDX = level, ECX = kind; area; RET 0) -> EAX: the Database has it */
void     rx_track_block_reset(rx_addr block);                               /* 00538090 (EAX = block) */
uint32_t rx_track_read_tag(rx_addr stream, rx_addr tag_out, rx_addr len_out);   /* 00539550 (EAX = stream, EDI = len; tag; RET 4) */
void     rx_track_read_vec3(rx_addr stream, rx_addr dst);                   /* 005395d0 (EDI = stream, ESI = dst; RET 4) */
void     rx_track_read_u32_array(rx_addr stream, rx_addr dst);              /* 00539680 (EDI = stream; dst; RET 4) */
void     rx_track_read_grid(rx_addr stream, rx_addr dst);                   /* 00539720 (EDI = stream; dst; RET 4) */
void     rx_track_read_record30(rx_addr stream, rx_addr dst, rx_addr holder);   /* 00539820 (ECX = stream, ESI = dst; holder; RET 4) */
void     rx_buffered_stream_construct(rx_addr s);                           /* 00810600 (EAX = s): BufferedFileInputStream */
void     rx_buffered_stream_destroy(rx_addr s);                             /* 00810680 (ECX = s) */
uint32_t rx_memory_stream_read(rx_addr s, rx_addr dst, uint32_t n);         /* 008109b0 (ECX = s; dst, n; RET 8) */
void     rx_memory_stream_skip(rx_addr s, uint32_t n);                      /* 00810a10 (ECX = s; n; RET 4) */
void     rx_memory_stream_close(rx_addr s);                                 /* 00810960 (ECX = s) */
uint32_t rx_file_stream_open(rx_addr s, rx_addr path, uint32_t a2, uint32_t a3);   /* 008104d0 (ECX = s; path, a2, a3; RET 0xc) */

/* ---- MXActivityFreeRide (build_activity_freeride.c) ---- */
rx_addr  rx_free_ride_construct(rx_addr A, uint32_t type, rx_addr world);   /* 0055fdb0 (EAX = world, ECX = type; A; RET 4) */
void     rx_free_ride_jump_tracker_init(rx_addr t);                         /* 005857f0 (EAX = the tracker, activity + 0xff080) */

/* ---- the world load (build_activity_world.c) ---- */
void     rx_level_config_init(rx_addr c);                     /* 00756060 (stdcall config; RET 4) */
void     rx_level_load_request(rx_addr c, uint32_t async);    /* 007560f0 (ESI = config; stdcall async; RET 4) */
uint32_t rx_level_loaded(rx_addr c);                          /* 00756250 (ESI = config) -> AL */
uint32_t rx_level_loaded_eax(rx_addr c, uint32_t eax_in);     /* the same with the whole EAX the original leaves */
void     rx_level_release(rx_addr c);                         /* 007561b0 (ESI = config) */
uint32_t rx_scene_dir_has(rx_addr dir, rx_addr name);         /* 00755f80 (EAX = directory; RET 4) */
void     rx_activity_subscene_list(rx_addr c, rx_addr dir);   /* 00556140 (cdecl) */
void     rx_int_hash_insert(rx_addr h, rx_addr key_p, rx_addr value_p);   /* 007cfa10 (EDI = map; RET 8) */
void     rx_scene_directory_read(rx_addr c);                  /* 007562b0 (stdcall config; RET 4) */
void     rx_mxworld_load_base(rx_addr w, uint32_t event);     /* 00766110 (stdcall; RET 8) */
uint32_t rx_world_construct(rx_addr game, rx_addr progress);  /* FUN_00759f00 (cdecl game, &progress) -> 0 */
uint32_t rx_mxworld_set_paused(rx_addr w, uint32_t v);        /* 00767480 (ECX = world, EAX = value) -> changed */
uint32_t rx_activity_area_update(rx_addr w, rx_addr t);       /* 007669f0 (stdcall world, table; RET 8) -> changed */
void     rx_game_setup_activity_area(rx_addr g);              /* Game_SetupActivityArea 0075c430 (ESI = game) */

/* ---- StartEvent and the level request (build_activity_event.c) ---- */
void     rx_tbl_set_cell_float(rx_addr t, rx_addr name, uint32_t row, rx_addr valp);   /* 005063c0 (ESI = table, EAX = name; RET 8) */
void     rx_player_list_request_activity(uint32_t row, uint32_t type, uint32_t area);  /* 004dde70 (ESI = row; RET 8) */
void     rx_player_list_request_event(uint32_t type, uint32_t area, uint32_t all);     /* 005a3590 (cdecl) */
void     rx_activity_rows_keep(uint32_t type, uint32_t area);                          /* 005a3750 (cdecl) */
void     rx_game_request_level(rx_addr name);                 /* MXGame_RequestLevel 0075c640 (EAX = name) */
uint32_t rx_game_resume_from_ui(rx_addr g, uint32_t hide_ui); /* MXGame_ResumeFromUI 0075c8c0 (EAX = game; RET 4) */
void     rx_ui_request_level(rx_addr ui, uint32_t page);      /* UI_RequestConfiguredLevel 0059bd60 (stdcall; RET 8) */
void     rx_start_event(rx_addr ui);                          /* MXUIManager::StartEvent 0059ade0 (ECX = ui) */

/* ---- a player joins: the driver, its rules and behaviours (build_activity_driver.c) ---- */
void     rx_controls_reset(rx_addr c);                        /* 006c9100 (EAX = the controls, P + 0xb58) */
rx_addr  rx_base_driver_construct(rx_addr d, rx_addr player); /* BaseDriver 00508c10 (ESI = d, EAX = player) */
rx_addr  rx_network_driver_construct(rx_addr d, rx_addr player);   /* NetworkDriver 0050e1b0 (EDI = d; player; RET 4) */
rx_addr  rx_race_driver_construct(rx_addr d, rx_addr player); /* RaceDriver 0050ed80 (ECX = d, EAX = player) */
rx_addr  rx_race_stub_vf44(rx_addr A, rx_addr player);        /* MXActivityRaceStub::vf44 00566ea0 (thiscall; RET 4) */
void     rx_race_stub_vf47(rx_addr A, rx_addr d);             /* MXActivityRaceStub::vf47 00565e00 (thiscall; RET 4) */
void     rx_driver_behaviors_reset(rx_addr d);                /* 00508f50 (stdcall; RET 4) */
void     rx_stunt_scoring_listen(rx_addr s);                  /* 006cdd50 (stdcall; RET 4) */
void     rx_player_join_activity(rx_addr P, rx_addr A);       /* 0054d9d0 (ESI = P, EAX = A) */
void     rx_player_start_zone(rx_addr P);                     /* 0054cd20 (stdcall; RET 4) */
rx_addr  rx_activity_on_player_joined(rx_addr P, rx_addr A);
void     rx_network_driver_vf1(rx_addr d);                    /* NetworkDriver::vf1 0050e380 (ECX) */
void     rx_race_driver_vf1(rx_addr d);                       /* RaceDriver::vf1 0050ee40 (ECX) */  /* Activity_OnPlayerJoined 0055b610 (ECX = P; A; RET 4) -> driver */

/* ---- the activity area re-created in the event world: game states 9 / 10 (build_activity_aaa.c) ---- */
void     rx_game_start_aaa_creation(uint32_t activity_type, uint32_t activity_mode);   /* Game_StartAAACreation 0075a380 (EAX = type, ECX = mode) */
uint32_t rx_game_aaa_creation_job(rx_addr game);   /* 0075a3d0: the Database loader job "AAA Creation" (cdecl game) -> 0 */
void     rx_world_activities_suspend(rx_addr world);           /* 007663d0 (stdcall world; RET 4) */
void     rx_world_recreate_activities(rx_addr world);          /* 00766420 (stdcall world; RET 4) */
void     rx_world_activities_restart(rx_addr world);           /* 007665b0 (stdcall world; RET 4) */
void     rx_world_players_spawn(rx_addr world);                /* 00766e70 (stdcall world; RET 4) */

/* ---- MXGame::Update (build_activity_game.c) ---- */
uint32_t rx_mxgame_update(rx_addr g);
rx_f80   rx_level_load_progress(rx_addr cfg);              /* Fn_00756700 (EDX = the level config) -> st(0) */
void     rx_world_activities_start(rx_addr w);                /* 007668e0 (stdcall world; RET 4) */
void     rx_game_enter_real_world(rx_addr g);                 /* Game_EnterRealWorldAudio 0075a3f0 (stdcall game; RET 4) */                         /* MXGame::Update 00757fb0 (ECX = game) -> 1 (0 while debug-paused) */

#ifdef __cplusplus
}
#endif
