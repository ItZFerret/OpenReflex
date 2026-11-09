/* build_mxworld.h -- construction unit "world" (core/notes/construction.md, unit 3): the MXWorld object of the event
 * (0x22150 bytes, MXGame + 0x5a54) made by FUN_00759f00 (MXGame::Update state 3): its constructor FUN_00765e20 and MXWorld::vf1
 * 00765f80 (the world started on its scene and physics world).
 *
 * MXWorld (vtable 0x0096dd34): +0x10 the config's +0x20a4 (scene-from-file flag), +0x20014 paused (the world update runs only at 0),
 * +0x20018 front end (0 for the event), +0x20020 (double) 0, +0x20030 the level name, +0x20134 the physics world, +0x20138 the
 * scene, +0x2013c -1, +0x20150 the camera director (0x17a0; [0x00a7f74c] = it; registers the script command "PlayScenario"
 * (0x0074c790) in the command registry [0x00d6925c]), +0x218f0 the world camera entity (Camera, vtable 0x0095a46c; its node at
 * +0x90, a std::string at +0x2d8, the camera list [0x00a94184] / [0x00a94188]; at +0x310 a camera target entity (0x0095a3f4) linked
 * into the current MXWorld's +0x21de8 / +0x21dec list), +0x219d8 0.5, +0x21da4 a small array {data (10 pointers), 0, 10, 0, 10},
 * +0x21dc0 1, +0x21e00 [0x977808], +0x21e10 a 0x320-byte block (FUN_00536ca0; FUN_00536ef0 in vf1), +0x22140 the scene's entity
 * of type 0x1019 (or 0), +0x22144 / +0x22148 from the type 0x107a entity (+0x90 / +0x94; 0 / 1 without one), +0x2214c the type
 * 0x104d entity (the mud manager) or the default mud manager 0x009cb2f0.
 * vf1 also registers two physics contact handlers in the physics world's handler list (+0x1928): the audio collision manager
 * [0x00d94ad0] (audio, not core: whatever the global holds) and the VehicleDamageBehaviorManager singleton FUN_0047bb40 (unit
 * vehicle). Scene services vf1 calls (the scene unit's: FUN_0063dae0's 0080ddf0 / 007d2f70, the entity queries 007d2920) and the
 * functions of other units (0047bb40, 00536ef0, 00674d90) go through the function registry (RX_FN) and, while unported, the
 * original (rx_call_original: oracle only). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void    rx_command_registry_init(void);                                 /* 008181a0 (static initializer: startup) */
rx_addr rx_mxworld_construct(rx_addr world, rx_addr config);            /* 00765e20 (EDI = world; stdcall config; RET 4) */
rx_addr rx_camera_director_construct(rx_addr d);                        /* 0074abe0 (ESI = director) */
rx_addr rx_camera_entity_construct(rx_addr cam);                        /* 0063d930 (stdcall camera; RET 4): the Camera entity base */
rx_addr rx_world_camera_construct(rx_addr cam);                         /* 0063d2c0 (ESI = camera) */
void    rx_mxworld_start(rx_addr world, rx_addr scene, rx_addr name, rx_addr physworld, rx_addr table);   /* vf1 00765f80 (RET 0x10) */
/* 0075c530: the current MXWorld by the game state (G +0x18: 1 -> +0x20c8; 2 / 3 by the core's world flags; else +0x5a54) */
rx_addr rx_mxworld_current(void);
/* 0063e490 (stdcall entity): the camera target entity, linked into the current world's +0x21de8 / +0x21dec list */
void rx_camera_target_construct(rx_addr t);

#ifdef __cplusplus
}
#endif
