/* build_globals.c -- see build_globals.h. */
#include "build_globals.h"
#include "build_tables.h"
#include "build_globals_int.h"
#include "build_asset.h"
#include "build_load.h"
#include "build_mxgame.h"
#include "build_uiprofile.h"
#include "build_world.h"
#include <stdio.h>
#include <string.h>

typedef void (*rx_init_fn)(void);

/* 00903b10: the static allocator table (FUN_004dc0f0, guard 0x00d92ed8) -- build_tables_rdb.c's rx_tbl_alloc_init */
static void rx_init_00903b10(void) { rx_tbl_alloc_init(); }
RX_FUNCTION(0x00903b10, rx_init_00903b10)

static uint32_t g_unported[RX_INITTERM_LAST / 4 - RX_INITTERM_FIRST / 4][2];
static int g_nunported;

int rx_globals_unported(uint32_t (*out)[2], int max) {
    int n = g_nunported < max ? g_nunported : max;
    for (int i = 0; i < n; i++) { out[i][0] = g_unported[i][0]; out[i][1] = g_unported[i][1]; }
    return g_nunported;
}

void rx_globals_static_init(void) {
    g_nunported = 0;
    for (uint32_t slot = RX_INITTERM_FIRST; slot < RX_INITTERM_LAST; slot += 4) {
        uint32_t fn = U32(0, slot);
        if (!fn) continue;
        if (fn == 0x00903b20u) {                     /* 12a: the table objects' initializers 00903b20 .. 00904160 (34 slots) */
            rx_tbl_static_init();
            slot += 4u * 33u;
            continue;
        }
        rx_init_fn f = RX_FN(fn, rx_init_fn);
        if (!f) { g_unported[g_nunported][0] = slot; g_unported[g_nunported][1] = fn; g_nunported++; continue; }
        f();
    }
}

/* MXGame::Startup's table startup (FUN_004e6280) and the game init thread's table / content loads (0075ed60: MXRegistry_Load 004e6c30, then FUN_0053c410's
 * DLC finder). The engine context (rx_engine_startup) holds the Database (+8) and the Registry (+0xc). NOT here (see construction.md section 14.7): the
 * rest of MXGame's constructor and of MXGame::Startup (event channels, Lua, localization, debug overlay), the init thread's audio / UI loads. */
void rx_globals_game(fmt_install* in) {
    rx_addr ctx = U32(0, 0x00a95844);
    rx_addr db = U32(ctx, 8), registry = U32(ctx, 0xc);
    rx_tbl_mxregistry_load(in, db, registry);           /* MXRegistry_Load */
    rx_dlc_load(in, db);                                /* FUN_0053c410 */
}

/* The original's order (core/notes/construction.md 14.1 / 17): ReflexWinMain creates the engine context, then the game object
 * (00755f20: the MXGame constructor, with unit 12a's schema calls inside it, and the save object), then Engine::Startup, whose tail
 * registers the entity factory and the game's scenes and calls MXGame::Startup (FUN_004e6280's tables inside it); the init thread
 * then loads MXRegistry / the DLC tables. */
int rx_globals_run(fmt_install* in, const char* phase) {
    if (!strcmp(phase, "none")) return 0;
    rx_crt_startup();                                   /* msvcr90's DllMain (the DLL data was loaded by rx_image_load_crt) */
    rx_globals_static_init();
    if (!strcmp(phase, "init")) return 0;
    rx_engine_context_create();                         /* XenonCore */
    rx_mxgame_create();                                 /* 00755f20 (unit 10) */
    rx_engine_startup(in, 0);
    if (!strcmp(phase, "engine")) return 0;
    rx_database_set_install(in);
    rx_asset_register_core_loaders();                   /* every ported AssetIO::vf3 (the tbldata loader among them): core setup */
    {   /* Engine::Startup's EngineDependencies (0084e2xx +269 / +285): the engine's bxml / model / collis / shader /
         * texture assets that later subsystems look up by name (the vehicle parameters' mx*_vehicle.bxml among them).
         * The core loaders must be registered first (above); the original loads it inside Engine::Startup. */
        const rx_addr db = U32(U32(0, 0x00a95844), 8);
        rx_database_load(db, in, "EngineDependencies", "Database\\", 0, 0);
        rx_database_load_package(db, in, "EngineDependencies", "EngineDependencies", 0, 0);
        rx_phys_props_set_install(in);                  /* EngineDependencies/PhysicsProperties for the physics helpers */
    }
    rx_engine_startup_tail();                           /* the factory and the scenes (unit 10) */
    rx_mxgame_startup(in);                              /* MXGame::Startup 00756e10 (unit 10) */
    if (!strcmp(phase, "startup")) return 0;
    rx_globals_game(in);
    /* the front end's profile route (the product has no front end): the shipping guest profile calls
     * UI_SetPrimaryPlayerDeviceIndex(0) after MXRegistry_Load and before any event (build_uiprofile.c) */
    rx_ui_profile_bootstrap();
    return 0;
}

int rx_globals_init(fmt_install* in) { return rx_globals_run(in, "all"); }
