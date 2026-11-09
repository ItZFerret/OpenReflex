/* build_mxgame.h -- construction unit 10: the MXGame object and its startup (core/notes/construction.md section 17).
 *
 * The original (ReflexWinMain 008814a0): the engine context (XenonCore, unit 12b) -> FUN_00755f20 {MXGame (0x16210 bytes) = the
 * constructor FUN_00756760 on _aligned_malloc'd memory (00755da0), [0x00a946b8]; the save-game object [0x00d92f10] = FUN_00752fd0}
 * -> MXGame::vf3 (the name "MXGame") -> Engine::Startup (0084e320: Registry, input, Database ...; its tail registers the entity factory
 * and the three scenes of the Game base with XenonCore vf1 / vf5, then calls MXGame::Startup 00756e10 = vf4).
 *
 * MXGame layout (what the race reads; the rest in build_mxgame.c's comments):
 *   +0 vtable 0x0096d7dc, +4 / +8 / +0xc the three SceneManagers of the Game base 00882080 (slot 0 front end, 1 the event's world, 2 the
 *   UI), +0x10 MXGame\Input InvertMouse, +0x14 1, +0x18 the game state, +0x1c the registry node of MXGame\Input, +0x20 .. +0x30 registry
 *   handles, +0x34 / +0x39c0 the two world configs (00755ff0: scene name +0, path +0x104, the 0x20-byte hash table +0x396c), +0x5a54 /
 *   +0x5a58 the event MXWorld / its activity table 0x00ae1b18, +0x20c8 / +0x20cc the UI world / 0x00adffe0, +0x7454.. players
 *   (+0x7460 the 12 player slots 00552890), +0x745c the MXUIManager, +0x154b0 the vehicle list (unit 6), +0x15504 the gear list
 *   (0053d210), +0x15554 the package cache, +0x15570 MXGarage, +0x15b10 the "SceneLoading" channel, +0x15c6c the MusicPlayer,
 *   +0x161f4 / +0x161f8 the loading-thread events.
 *
 * SceneManager (0x2d20, FUN_007d14e0): +4 / +0x1c critical sections, +0x38 0x18 subscene records (0x110), +0x19d4 / +0x19f8 two
 * string hash tables (00843480), +0x1a18 the render container (007df410: light / renderable lists, the four render-sync workers,
 * +0x10f0 the culling grid 007dfba0 / 007d6510), +0x2bfc .. +0x2cac entity vectors / lists, +0x2c9c the ticker thread, +0x2cc4 the
 * scene kind, +0x2cd0 the identity matrix, +0x2d14 1.0f. */
#pragma once
#include "rx_game.h"
#include "../src/fmt_db.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ---- the startup sequence (build_globals.c calls these in the original's order) ---- */
rx_addr rx_mxgame_create(void);                     /* FUN_00755f20: MXGame (ctor 00756760) at [0x00a946b8] and [0x00d92f10] (00752fd0) */
void    rx_engine_startup_tail(void);               /* Engine::Startup after the Database: XenonCore vf1 (entity factory 004046a0), vf5 x3 (scenes) */
void    rx_mxgame_startup(fmt_install* in);         /* MXGame::Startup 00756e10 (vf4) */

/* ---- MXGame (build_mxgame.c) ---- */
rx_addr rx_mxgame_construct(rx_addr g);             /* FUN_00756760 (stdcall g; RET 4) */
rx_addr rx_world_config_construct(rx_addr c);       /* 00755ff0 (EDI = config) */
rx_addr rx_player_array_construct(rx_addr a);       /* 00552890 (ESI = MXGame + 0x7460) */
rx_addr rx_gear_list_construct(rx_addr l);          /* 0053d210 (ESI = MXGame + 0x15504) */
rx_addr rx_garage_construct(rx_addr g);             /* MXGarage_Construct 00761330 (stdcall garage; RET 4) */
rx_addr rx_music_player_construct(rx_addr m);       /* 00675890 (EDI = player) */
rx_addr rx_fmod_event_construct(rx_addr e, rx_addr name, uint32_t a1, rx_addr a2);   /* 0080a540 (ESI = event; name, a1, a2; RET 0xc) */
rx_addr rx_highlight_tracking_construct(rx_addr h); /* MXHighlightTracking_Construct 006d78c0 (ESI = h) */
rx_addr rx_online_achievements_construct(rx_addr a);   /* PCOnlineAchievements_Construct 006d86e0 (EAX = a) */
rx_addr rx_save_object_construct(rx_addr s);        /* 00752fd0 (stdcall s; RET 4): the object [0x00d92f10] */
rx_addr rx_mxgame_new(void);                        /* 00755f20 */
/* Quazal allocations as the original makes them (MemoryManager.cpp, the default manager [0x009df5c8]): 00883ed0 */
rx_addr rx_quazal_alloc(rx_addr manager, uint32_t size);
rx_addr rx_quazal_mutex_construct(rx_addr m);       /* 00883ae0 (ECX = m): a critical section in a Quazal block */

/* ---- the scenes (build_mxgame_scene.c) ---- */
rx_addr rx_game_base_construct(rx_addr g);          /* Game::Game 00882080 (ESI = game) */
rx_addr rx_scene_manager_construct(rx_addr s, uint32_t kind, uint32_t capacity, uint32_t grow);   /* 007d14e0 (stdcall; RET 0x10) */
rx_addr rx_render_container_construct(rx_addr c);  /* 007df410 (EDI = container) */
void    rx_scene_octree_init(rx_addr scene, uint32_t capacity);   /* 007d1d50 (stdcall scene, capacity; RET 8) */
void    rx_render_grid_create(rx_addr c, uint32_t capacity, rx_addr lo, rx_addr hi);   /* 007dfba0 (EAX = container; RET 0xc) */
void    rx_render_grid_init(rx_addr grid, uint32_t edx, rx_addr lo, rx_addr hi, uint32_t capacity, rx_addr container);
                                                    /* 007d6510 (ESI = grid, EDX; lo, hi, capacity, container; cdecl) */
rx_addr rx_strtab_construct(rx_addr t);             /* 00843480 (ESI = t): the 0x20-byte string hash table */
void    rx_vecp_reserve_ecx(rx_addr v, uint32_t n); /* 005ba080 (ECX = vector; n; RET 4) */

/* ---- event channels of the game (build_mxgame_event.c) ---- */
void    rx_stunt_channel_create(void);              /* 006c8fd0: "Stunts" [0x00a94304] and its 12 event types */
void    rx_stunt_scoring_channel_create(void);      /* 006cb870: "StuntScoring" [0x00a94318] and its 8 event types */
void    rx_channel_add_float_event(rx_addr ch, rx_addr name);   /* 005ed6e0 / 005f2460 (ECX = channel; name; RET 4): void(float) */
rx_addr rx_saved_game_manager_construct(void);      /* SavedGameManager_Construct 005a61b0: the static object 0x00dce458 */

/* ---- MXGame::Startup's callees (build_mxgame_init.c) ---- */
void    rx_activity_global_channel_create(void);    /* 0055ed50: "ActivityManagerGlobalEvents" [0x00a93f74] */
void    rx_vehicle_trigger_channels_create(void);   /* 00409690: "VehicleTrigger" [0x00a89bc8] / [0x00a89bcc], the trigger list [0x00a95d74] */
void    rx_activity_loaded_handler_add(rx_addr fn); /* 00419700 (stdcall function; RET 4) */
void    rx_stunt_settings_read(void);               /* 006c8ee0: MXGame\Stunts */
void    rx_stunt_link_settings_read(void);          /* 006caa80 (+ the judging parameters 006cc350) */
void    rx_judging_parameters_load(void);           /* 006cc350: FMXJudgingParameters.bxml */
void    rx_environment_settings_read(void);         /* 00549b30: mud splatter, race line, player reset */
void    rx_tag_radii_read(void);                    /* 0051c620: MXGame\Tag */
/* the NetworkManager (the static 0x00da6ca0) and the network handlers of MXGame::Startup 00756f16 .. 00756fdc */
void    rx_mxgame_network_startup(rx_addr game, uint32_t a1, uint32_t a2);   /* the static, 006de390 (composition), 007604b0 .. 007558e0 */
void    rx_netmgr_channel_init(rx_addr mgr);        /* 006de4b0 (ESI = manager): the "Network" channel at +0, its 40 event types, 007071a0 / 00707260 */
void    rx_netmgr_listen_event_details(rx_addr ch, rx_addr mgr);   /* 007071a0 (EAX = channel; stdcall manager; RET 4) */
void    rx_netmgr_listen_object_added(rx_addr ch, rx_addr mgr);    /* 00707260 (EAX = channel; stdcall manager; RET 4) */
void    rx_mxgame_listen_session_state(rx_addr ch, rx_addr game);  /* 007604b0 (EAX = channel; stdcall game; RET 4) */
void    rx_mxgame_listen_timer_events(rx_addr ch, rx_addr game);   /* 00760570 (EAX = channel; stdcall game; RET 4) */
void    rx_mxgame_on_net_timer(rx_addr game, uint32_t kind, float seconds);   /* 0075cf50 (ECX = game; kind, float; RET 8): the handler */
void    rx_player_manager_listen(rx_addr ch, rx_addr name, rx_addr obj, rx_addr method);   /* 00554eb0 (ECX = ch, EAX = name; RET 8) */
void    rx_player_manager_net_listen(rx_addr players);             /* 00552d20 (ESI = MXGame + 0x7460) */
void    rx_data_miner_listen_login(rx_addr ch, rx_addr miner);     /* 007558e0 (EAX = channel; stdcall miner; RET 4) */

#ifdef __cplusplus
}
#endif
