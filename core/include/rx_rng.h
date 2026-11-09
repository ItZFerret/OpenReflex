#ifndef RX_RNG_H
#define RX_RNG_H
#include "rx_api.h"
#ifdef __cplusplus
extern "C" {
#endif
/* The game's random number streams (agent w-rng, 2026-10-27). Additive: a new query and struct, RX_SESSION_API_VERSION unchanged.
 *   crt_main / crt_loader   msvcr90 rand()'s per-thread seed (_holdrand, ptd + 0x14) of the main thread and of the level loader thread
 *                           (0 before that thread drew); the core runs the loader's draws as that thread (game/crt_thread.c)
 *   game_seed / game_count  the game's own generator g_GameRandSeed 0x009cb860 and its draw count g_GameRandCount 0x009cb864
 *   ocean_*                 the ocean's Phillips spectrum (Ocean_InitSpectrum 007ea9e0, on the loader thread when the level's first water
 *                           surface creates the ocean): valid, the loader stream's seed before its first draw, the draws it made. The
 *                           renderer's spectrum (Unity ReflexWater.Ocean.Generate(ocean_seed)) then has the game's random phases. */
#define RX_RNG_VERSION 1u
typedef struct rx_rng_state {
    uint32_t size, version, valid;
    uint32_t crt_main, crt_loader;
    uint32_t game_seed, game_count;
    uint32_t ocean_valid, ocean_seed, ocean_draws;
    int32_t reserved[6];
} rx_rng_state;                 /* 64 bytes */
/* Caller sets size and version (1). 1 on bad arguments (output untouched), else 0. Session owner thread, between advances. Reads only. */
RX_API int rx_session_rng(struct rx_session* session, rx_rng_state* out);
#ifdef __cplusplus
}
#endif
#endif
