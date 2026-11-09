/* build_ui_world.h -- the front end's UI world (MXUIWorld, game + 0x20c8), built the way the game's init thread builds it (build_ui_world.c). */
#pragma once
#include "rx_game.h"
struct fmt_install;
struct fmt_level;

void     rx_scene_query_typed(rx_addr scene, rx_addr list, uint32_t type);                  /* 007d25c0 (EAX = scene, ESI = list {data, count, capacity, grow}; stdcall type; RET 4) */
rx_addr  rx_scene_find_by_name(rx_addr scene, rx_addr name);                                /* 007d2570 (EAX = scene; stdcall name; RET 4) */
rx_addr  rx_mxuiworld_construct(rx_addr world, rx_addr config);                             /* 00765090 (ECX = world, EAX = config) */
void     rx_mxuiworld_start(rx_addr world, rx_addr scene, rx_addr config, rx_addr physics, rx_addr table);   /* MXUIWorld::vf1 007651d0 (RET 0x10) */
void     rx_mxuiworld_vf3(rx_addr world, rx_addr table);                                    /* 00765230 (RET 4) */
uint32_t rx_mxuiworld_vf5(rx_addr world, float dt);                                         /* 00765670 */
void     rx_mxuiworld_tutorial_mode_on(rx_addr world);                                      /* 00765820 (EAX = world) */
/* the init thread's UI world part (0075ed60 from FUN_00756060 on): the UI level's config, packages and scene, the MXUIWorld and its FreeRide activity;
 * `level` is the install's "UIScene" level (the subscene file), which the scene directory / base subscene readers use while this runs */
int      rx_ui_world_create(rx_addr game, struct fmt_install* install, struct fmt_level* level);
/* The UI manager's event channel (the original's [0xa93fcc] + 0x98, the EventSystem::Channel "UI" its constructor 005da6cb builds).
 * The product has no UI manager (a documented boundary), so the channel itself is constructed by rx_ui_world_create exactly as that
 * constructor step does, with the events the game's own code raises on it (their original signature types) and no handlers (the UI's
 * handlers are what MXUIManager::vf42 005999f0 subscribes). In the oracle this is the manager's own channel. */
rx_addr  rx_ui_event_channel(void);
