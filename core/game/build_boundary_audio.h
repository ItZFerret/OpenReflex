/* build_boundary_audio.h -- the calls of gameplay code into the game's sound engine (FMOD and its worker thread). PRODUCT BOUNDARY, see also build_boundary_audio.c
 * and core/plan/QUEUE.md ("chain", the 2026-10-26 w-fx section). The host plays no sound from the core. In the oracle the originals run (so that Verify of the
 * calling function sees the same memory); in the product the boundary functions do nothing.
 *
 * What the three originals write, from the disassembly (`fn.py`):
 *   00805120 (cdecl paused, wait): the audio system's paused flag [0x00da6878]; for each sound instance (count [0x00a957c8], flags [0x00a81a00 + 20 i], paused
 *            [0x00d679ac + 84 i]) the paused word and the FMOD pause (00807440); with wait == 0 it posts command 1 to the audio worker (Audio_PostCommand) and waits
 *            on an event for the worker's acknowledgement.
 *   00805000 Sound_PauseAll (cdecl pause, force): the flag [0x00da6874] and the instances' pause / fade state (volume words at [0x00d679c0 ..], FMOD 00807170 /
 *            008070b0 / 00807400).
 *   00805200 (cdecl float scale): [0x00a95d70] and FMOD System::set3DSettings (or, off the audio thread, Audio_PostCommand 9).
 * Who reads those words (a scan of every decompiled function that names them, outside 0x804000..0x808000): AudioVolumeMixEntity::vf5 0067d4d0, 0067d810, 0067de30
 * (the music / ambience volume mix), UIFadeLayer::vf47 / vf48 (a UI fade's sound state), 0068b410 / 0068b5b0 (the loc-sound lists) and UILocSound_Play 0068dd40: all
 * of them set sound volumes or UI sound state. Nothing in the simulation (physics, race rules, AI, the scene) reads them, so omitting them in the product changes
 * no game state, only sound the host does not play. */
#pragma once
#include "rx_game.h"

#ifdef RX_ORACLE
/* an audio-object function with register arguments (the FMOD event objects: create 006d05a0's emitter, release 0041c850): the original */
static inline void rx_audio_call_ex(rx_addr fn, const rx_regs* in, int n, const uint32_t* a) {
    rx_regs out;
    rx_call_original_ex(fn, in, &out, n, a);
}
static inline void rx_audio_pause_flags(uint32_t paused, uint32_t wait) {        /* 00805120 */
    const uint32_t a[2] = {paused, wait};
    rx_call_original(0x00805120, RX_CDECL, 0, 2, a);
}
static inline void rx_audio_pause_all(uint32_t pause, uint32_t force) {          /* 00805000 Sound_PauseAll */
    const uint32_t a[2] = {pause, force};
    rx_call_original(0x00805000, RX_CDECL, 0, 2, a);
}
static inline void rx_audio_set_3d_scale(uint32_t scale_bits) {                  /* 00805200 */
    const uint32_t a[1] = {scale_bits};
    rx_call_original(0x00805200, RX_CDECL, 0, 1, a);
}
#else
static inline void rx_audio_call_ex(rx_addr fn, const rx_regs* in, int n, const uint32_t* a) { (void)fn; (void)in; (void)n; (void)a; }
uint32_t rx_audio_boundary_noop2(uint32_t a, uint32_t b);
uint32_t rx_audio_boundary_pause_all(uint32_t pause, uint32_t force);
uint32_t rx_audio_boundary_3d_scale(uint32_t scale_bits);
static inline void rx_audio_pause_flags(uint32_t paused, uint32_t wait) { (void)rx_audio_boundary_noop2(paused, wait); }
static inline void rx_audio_pause_all(uint32_t pause, uint32_t force) { (void)rx_audio_boundary_pause_all(pause, force); }
static inline void rx_audio_set_3d_scale(uint32_t scale_bits) { (void)rx_audio_boundary_3d_scale(scale_bits); }
#endif
