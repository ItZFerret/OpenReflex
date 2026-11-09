/* build_globals.h -- construction unit 12b: the game's global state and startup, set up by code before a race
 * (core/notes/construction.md section 14).
 *
 * The product reads MXReflex.exe's .rdata / .data sections from the install as the initial data (src/rx_image.h); everything the
 * original sets up by CODE afterwards is ported here, in the original's order:
 *   1. the CRT's static initializers (_initterm(__xc_a, __xc_z): 990 entries of the table 0x009166e4 .. 0x00917660),
 *   2. Engine::Startup (0084e320): key bindings, timing fields, the engine's tables,
 *   3. MXGame::Startup (00756e10) and the game init thread (00760400): the game state a race reads,
 *   4. the controller key maps and the math tables (built by 1 and 2 in the original).
 * Entry: rx_globals_init(in) after rx_mem_init() and rx_image_load(). */
#pragma once
#include "rx_game.h"
#include "../src/fmt_db.h"
#ifdef __cplusplus
extern "C" {
#endif

/* The whole startup (phases below in the original's order). Returns 0. */
int rx_globals_init(fmt_install* in);
/* Up to a named phase, for the tests: "init" (the static initializers: the state at ReflexWinMain), "engine" (+ the engine context, the
 * MXGame object (unit 10) and Engine::Startup's part: the state at Engine::Startup's game dispatch), "startup" (+ the scenes registered and
 * MXGame::Startup), "game" / "all" (+ the init thread's table loads) = rx_globals_init. */
int rx_globals_run(fmt_install* in, const char* phase);

/* ---- phase 3: the game (unit 10, build_mxgame*.c) and the init thread's loads (build_globals.c) ---- */
void rx_globals_game(fmt_install* in);   /* the core's asset loaders registered, then the init thread's table / DLC loads */

/* ---- phase 2: the engine context and Engine::Startup (build_globals_engine.c) ---- */
rx_addr rx_engine_context_create(void);                        /* ReflexWinMain: XenonCore (FUN_00843950) at [0x00a95844] */
void rx_engine_startup(fmt_install* in, uint32_t tick_ms);      /* Engine::Startup up to the game dispatch; tick_ms = GetTickCount() (the random seed) */

/* ---- the downloadable-content finder (build_globals_dlc.c): 006d3f40 / FUN_0053c410 ---- */
/* The names (up to the first '.') of the install's Database\*DLC*.dx9.database files in the game's order (strcmp). Returns the count. */
int  rx_dlc_find(const fmt_vfs* v, char (*names)[64], int max);
/* FUN_0053c410: loads the table / UI databases and packages of the DLC names that contain "Tables" / "MXUI" (rx_tbl_load_dlc_tables). */
void rx_dlc_load(fmt_install* in, rx_addr db);

/* ---- msvcr90's startup data (build_globals_crt.c): after rx_image_load + rx_image_load_crt; rx_globals_init does it ---- */
void rx_crt_startup(void);

/* ---- phase 1: the CRT's static initializers ---- */
#define RX_INITTERM_FIRST 0x009166e4u    /* __xc_a */
#define RX_INITTERM_LAST  0x00917660u    /* __xc_z (exclusive) */
/* _initterm(__xc_a, __xc_z): runs the initializer of every non-null slot in table order through the function registry. An initializer
 * that is not ported (see build_globals.c for the list and the reason of each) is skipped. */
void rx_globals_static_init(void);
/* The slots whose initializer is not ported, in table order (for tools): count; fills out[i] = {slot, function}. */
int rx_globals_unported(uint32_t (*out)[2], int max);

#ifdef __cplusplus
}
#endif
