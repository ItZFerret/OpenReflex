/* build_boundary_audio.c -- PRODUCT BOUNDARY (core/plan/QUEUE.md "chain"): the game's sound engine (FMOD / the sound instance tables, 0x00a957c8 / 0x00d679ac ...). The
 * host plays no sound from the core: these calls of gameplay code (pause / resume, listener settings, cue triggers) have no effect in the product and nothing in the
 * simulation reads their results (the game's sound tables are read only by the sound code). Registered in the product build only (the oracle runs the originals). Each
 * entry says what it is and why its result is not read.
 *   00805120 (cdecl a, b: the audio system's paused flag [0x00da6878] and the instances' flag tables)  00805000 Sound_PauseAll (cdecl pause, force: the instances' pause)
 *   00674e90 (cdecl float: the world's sound zones)  00805200 (cdecl float: the 3D scale) */
#include "rx_game.h"
#include "build_boundary_audio.h"
#include "world_crowd_audio.h"

#ifndef RX_ORACLE
/* 00674e90 (cdecl float dt): the world's sound zones' update: the ambient zone databases (00674f20 loads "Database\\<zone>" packages through the engine's package vf), the
 * zone ramp block 0x00c023a8 (its state machine 0067f450, set up by core/game/world_cam_ramp.c) and the zone objects' vf19: the ambience of the listener only */
/* w-sfx: these are the crowd's (not the ambience's): the crowd sound packages, the crowd momentum block and the crowd emitters' vf19;
 * the product runs the crowd's game state (adoption, momentum, its draws of the game's LCG) in world_crowd_audio.c rx_crowd_frame. */
uint32_t rx_audio_boundary_zones(uint32_t dt_bits) { rx_crowd_frame(rx_bf(dt_bits)); return 0; }
RX_FUNCTION(0x00674e90, rx_audio_boundary_zones)
/* 00805200 (cdecl float: the audio system's 3D scale / doppler setting; stores [0x00a95d70] and calls FMOD set3DSettings) */
uint32_t rx_audio_boundary_3d_scale(uint32_t scale_bits) { (void)scale_bits; return 0; }
RX_FUNCTION(0x00805200, rx_audio_boundary_3d_scale)
uint32_t rx_audio_boundary_noop2(uint32_t a, uint32_t b) { (void)a; (void)b; return 0; }
RX_FUNCTION(0x00805120, rx_audio_boundary_noop2)
uint32_t rx_audio_boundary_pause_all(uint32_t pause, uint32_t force) { (void)pause; (void)force; return 0; }
RX_FUNCTION(0x00805000, rx_audio_boundary_pause_all)
#endif
