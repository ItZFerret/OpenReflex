/* build_mxgame_startup.c -- unit 10: Engine::Startup's game dispatch and MXGame::Startup 00756e10 (build_mxgame.h).
 * MXGame::Startup runs on the main thread inside Engine::Startup (its tail: the entity factory, the three scenes, then vf4). The core
 * runs it as the product's startup orchestrator (rx_globals_run): its own stores and registry reads are transliterated here, its
 * callees are the ported functions of this and other units; what is not core (renderer, network transport, debug overlay, Lua VM) is
 * listed at its call with the reason (core/notes/construction.md section 17). */
#include "build_mxgame_int.h"
#include "build_tables.h"
#include "build_asset.h"
#include "build_pool.h"
#include "build_uiprofile.h"
#include "world_node.h"
#include "race_driver.h"
#include <string.h>

/* a Registry virtual (thiscall registry; stdcall arguments) through the registry's vtable: unit 12a's registered functions */
typedef uint32_t (*reg_fn3)(rx_addr, uint32_t, uint32_t, uint32_t);
static uint32_t reg3(rx_addr reg, uint32_t slot, uint32_t a, uint32_t b, uint32_t c) {
    reg_fn3 f = RX_FN(U32(U32(reg, 0), 4 * slot), reg_fn3);
    if (!f) mx_unreached(U32(U32(reg, 0), 4 * slot), "Registry virtual not ported");
    uint32_t r;
    MX_CALLR(r, 3, f(reg, a, b, c));
    return r;
}
#define REG_FIND(reg, path, key)        reg3((reg), 1, (path), (key), 1)        /* vf1: the record of path\key */
#define REG_BOOLS(reg, rec, dst, n)     reg3((reg), 8, (rec), (dst), (n))       /* vf8 */
#define REG_FLOATS(reg, rec, dst, n)    reg3((reg), 17, (rec), (dst), (n))      /* vf17 */
#define REG_INTS(reg, rec, dst, n)      reg3((reg), 19, (rec), (dst), (n))      /* vf19 */
#define REG_STRING(reg, rec, dst, n)    reg3((reg), 20, (rec), (dst), (n))      /* vf20 */

/* Engine::Startup after the Database (0084e641 .. 0084e6c8): XenonCore vf1 (00821fe0: the entity factory, +0x2c = 0x004046a0), vf5
 * (00822010: scene slot k at +0x1c + 4k) with the Game base's scenes: slot 2 = game + 0xc, slot 0 = game + 4, slot 1 = game + 8 */
void rx_engine_startup_tail(void) {
    const rx_addr ctx = U32(0, 0x00a95844), game = U32(0, 0x00d728d8);
    SU32(ctx, 0x2c, 0x004046a0);
    SU32(ctx, 0x1c + 4 * 2, U32(game, 0xc));
    SU32(ctx, 0x1c + 4 * 0, U32(game, 4));
    SU32(ctx, 0x1c + 4 * 1, U32(game, 8));
}

/* MXGame::Startup 00756e10 (thiscall game; a1, a2: Engine::Startup's window arguments, passed on to the network session) */
static void mxgame_startup(rx_addr g, fmt_install* in, uint32_t a1, uint32_t a2) {
    MX_FRAME_ALIGN(F, 8, 0x11c, 3);
    SU32(F, 0x124, U32(0, 0x009b45e4) ^ (F + 0xc));             /* the /GS cookie ([esp + 0x118] before the 3 pushes) */
    const rx_addr ctx = U32(0, 0x00a95844);
    /* XenonCore vf2 (the renderer, +0x34) -> vf43 (+0xac) with 0.033f: the renderer's frame time (render: not core) */
    /* the debug key (MapKey 0xd6af18 and 0xd6a3d0 held on pad 0 .. 3): the Database's background cache flag argument */
    uint32_t dbg = 0;
    for (uint32_t pad = 0; pad < 4 && !dbg; pad++) {
        uint32_t a, b = 0;
        MX_CALLR(a, 2, rx_world_keymap_down(0x00d6af18, 1, pad));
        if (a) MX_CALLR(b, 2, rx_world_keymap_down(0x00d6a3d0, 1, pad));
        if (a && b) dbg = 1;
    }
    const rx_addr db = U32(ctx, 8);
    MX_CALL(1, rx_database_cache_enable(db));                   /* vf34 0084cf10 (its argument, dbg, unused) */
    { uint32_t a[1] = {U32(db, 0x83b48)}; mx_os(0x00916090 /* ResetEvent */, 1, a); }   /* vf36 0084cf20 */
    (void)dbg;
    MX_CALL(0, rx_tbl_mxgame_startup_tables(in));               /* FUN_004e6280 (unit 12a) */
    /* the NetworkManager (00756f16 .. 00756fdc): the session/stats static 0xda6ca0 (0xdcf304 bit 0: 006de1f0 + the atexit), its init
     * 006de390 (the "Network" channel 006de4b0; the network objects are the offline boundary), then the network handlers of MXGame
     * (007604b0 / 00760570), its player slots (00552d20) and the DataMiner (007558e0) (build_mxgame_init.c) */
    rx_mxgame_network_startup(g, a1, a2);
    MX_CALL(0, rx_activity_global_channel_create());            /* 0055ed50 */
    MX_CALL(0, rx_vehicle_trigger_channels_create());           /* 00409690 */
    const rx_addr reg = U32(ctx, 0xc);
    SU32(0, 0x00a957c8, 9);
    SU32(0, 0x00a957cc, 0x009c49a0);
    SU32(g, 0x1c, REG_FIND(reg, 0x0096d5d8 /* "MXGame\\Input" */, 0x0096d5cc /* "InvertMouse" */));
    REG_BOOLS(reg, U32(g, 0x1c), g + 0x10, 1);
    MX_CALL(0, rx_stunt_settings_read());                       /* 006c8ee0 */
    MX_CALL(0, rx_stunt_link_settings_read());                  /* 006caa80 (+ 006cc350) */
    MX_CALL(0, rx_environment_settings_read());                 /* 00549b30 */
    {
        const rx_addr rec = REG_FIND(reg, 0x0093e42c /* "MXGame" */, 0x00958ae0 /* "ReplayCameraTargetYOffset" */);
        if (rec) REG_FLOATS(reg, rec, 0x009c4a7c, 1);
    }
    MX_CALL(0, rx_tag_radii_read());                            /* 0051c620 */
    const rx_addr buf = F + 0x18;
    REG_STRING(reg, REG_FIND(reg, 0x0093e42c, 0x0096d5e8 /* "UISceneName" */), buf, 0x80);
    crt_strncpy(g + 0x34, buf, 0x103);
    SU8(g, 0x34 + 0x103, 0);
    SU32(F, 0x14, 0);
    REG_BOOLS(reg, REG_FIND(reg, 0x0093e42c, 0x0096d5f4 /* "StartInRealWorld" */), F + 0x14, 1);
    if (U32(F, 0x14)) {
        REG_STRING(reg, REG_FIND(reg, 0x0093e42c, 0x0096d608 /* "RealWorldSceneName" */), buf, 0x80);
        crt_strncpy(g + 0x39c0, buf, 0x103);
        SU8(g, 0x39c0 + 0x103, 0);
    }
    SU32(g, 0x24, REG_FIND(reg, 0x00952780, 0x0096d61c));
    SU32(g, 0x28, REG_FIND(reg, 0x00952780, 0x0096d628));
    SU32(g, 0x2c, REG_FIND(reg, 0x00952780, 0x0096d638));
    SU32(g, 0x30, REG_FIND(reg, 0x00952780, 0x0096d644));
    SU32(g, 0x20, REG_FIND(reg, 0x0095275c, 0x00952758));
    REG_INTS(reg, REG_FIND(reg, 0x0096d660, 0x0096d650), g + 0x14, 1);
    SU32(F, 0xc, 1);
    REG_BOOLS(reg, REG_FIND(reg, 0x0093e42c, 0x0096d670), F + 0xc, 1);
    SU32(0, 0x00c11bc4, U32(F, 0xc) == 0);
    SU32(F, 0x10, 1);
    REG_BOOLS(reg, REG_FIND(reg, 0x0093e42c, 0x0096d684), F + 0x10, 1);
    SU32(g, 0x7458, U32(F, 0x10));
    if (U32(g, 0x15c6c + 8)) mx_unreached(0x0075724b, "MusicPlayer with a track at startup (00677510)");
    /* (00757660, 007d20b0: see section 17) */
    /* the scenes' render flags (+0x2cb0 .. +0x2cbc): slot 2 off, slot 0 on (no +0x2cbc), slot 1 all on */
    {
        const rx_addr s2 = U32(ctx, 0x24), s0 = U32(ctx, 0x1c), s1 = U32(ctx, 0x20);
        SU32(s2, 0x2cb4, 0); SU32(s2, 0x2cb8, 0); SU32(s2, 0x2cbc, 0);
        SU32(s0, 0x2cb4, 1); SU32(s0, 0x2cb0, 1); SU32(s0, 0x2cb8, 1); SU32(s0, 0x2cbc, 0);
        SU32(s1, 0x2cb4, 1); SU32(s1, 0x2cb0, 1); SU32(s1, 0x2cb8, 1); SU32(s1, 0x2cbc, 1);
    }
    /* the script system (guard bit 0 of 0x00d92f44): 00744180's memory part (the Lua heap, build_pool.c); the VM is not ported */
    if (!(U32(0, 0x00d92f44) & 1)) {
        SU32(0, 0x00d92f44, U32(0, 0x00d92f44) | 1);
        MX_CALL(0, rx_lua_memory_init(0x00d94938));
    }
    /* (the UI manager 00598640 and its vf28 .. vf32: see section 17 - UI / render, not core; its UI settings object is game
     * state that the input layer reads ([0x00d92efc], build_uiprofile.c): the original's `if (g_UISettings == 0)` part is built
     * here) */
    MX_CALL(0, rx_ui_settings_create());                         /* 00598640 -> 005b7cd0 (UISettings_Construct) */
    SU32(g, 0x15b50, 0); SU32(g, 0x15b4c, 0); SU32(g, 0x15b54, 1);
    MX_END(F);
}

void rx_mxgame_startup(fmt_install* in) {
    rx_mxgame_init_set_install(in);
    mxgame_startup(U32(0, 0x00d728d8), in, 0, 0);
}
