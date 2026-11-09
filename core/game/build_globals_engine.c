/* build_globals_engine.c -- unit 12b: Engine::Startup (0084e320) as far as the core reads its results (core/notes/construction.md section 14).
 *
 * ReflexWinMain builds the engine context (XenonCore, FUN_00843950: [0x00a95844] = [0x00a95960]) and the game, then calls Engine::Startup. What
 * the race and the construction units read of it: the context's Database (+8), Registry (+0xc), vertex declaration cache (+0x14), the input
 * manager (+0x34: XenonCore vf6 00843a10 -> build_input.c) and device (+0x1ec); the registry's key bindings (KeyBind_Core / KeyBind_App); the
 * game's random seed; the 0x80000-byte scratch block [0x00d6daf0]; the engine frame timing object 0x00d728d0 (+0x10 of the context). NOT built
 * (renderer / audio / platform, nothing of the core reads them): the Direct3D device settings (XenonCore +0x3c.. +0x1e8: caps read from the
 * adapter), the audio system (FUN_00804070: [0x00a957d0..]), the debug overlay (FUN_00822330), the graphics settings registry reads
 * (FUN_007adc60), the task thread and its events, QueryPerformanceCounter stamps, the window.
 * Order as in Engine::Startup: Registry, random seed, the input manager (vf6), InputNames (first KeyMap construction), the key bindings, the
 * Database, the scratch block. */
#include "build_globals_int.h"
#include "build_input.h"
#include "build_asset.h"
#include "build_registry.h"
#include "build_world.h"
#include "../src/rx_d3d.h"
#include "loop_platform.h"
#include "loop_hostinput.h"

/* the engine context: FUN_00843950 translated (build_globals_gen.c) */
extern void rx_xenon_core_ctor(uint32_t in_eax);

/* 00816ba0 Keyboard_InitScanTable (EDI = the keyboard device): once (guard byte 0x00da687c): the scan code -> game key id table [0x00a95848] (256
 * bytes: the 101 ids 0x23 .. 0x87 at the DIK codes below, the original's immediate stores in its order) and its inverse [0x00a85b28] (101 words:
 * for id 0x23 + n the first scan code whose entry is that id, else 0); the device's +0x9a8 / +0x9ac and +0x998 .. +0x9a4 are cleared. */
static const struct { uint8_t scan, id; } k_scan_ids[101] = {
    {0x1e, 0x23}, {0x30, 0x24}, {0x2e, 0x25}, {0x20, 0x26}, {0x12, 0x27}, {0x21, 0x28}, {0x22, 0x29}, {0x23, 0x2a},
    {0x17, 0x2b}, {0x24, 0x2c}, {0x25, 0x2d}, {0x26, 0x2e}, {0x32, 0x2f}, {0x31, 0x30}, {0x18, 0x31}, {0x19, 0x32},
    {0x10, 0x33}, {0x13, 0x34}, {0x1f, 0x35}, {0x14, 0x36}, {0x16, 0x37}, {0x2f, 0x38}, {0x11, 0x39}, {0x2d, 0x3a},
    {0x15, 0x3b}, {0x2c, 0x3c}, {0x0b, 0x3d}, {0x02, 0x3e}, {0x03, 0x3f}, {0x04, 0x40}, {0x05, 0x41}, {0x06, 0x42},
    {0x07, 0x43}, {0x08, 0x44}, {0x09, 0x45}, {0x0a, 0x46}, {0x29, 0x47}, {0x0c, 0x48}, {0x0d, 0x49}, {0x1a, 0x4a},
    {0x1b, 0x4b}, {0x2b, 0x4c}, {0x35, 0x4d}, {0x27, 0x4e}, {0x28, 0x4f}, {0x33, 0x50}, {0x34, 0x51}, {0x3b, 0x52},
    {0x3c, 0x53}, {0x3d, 0x54}, {0x3e, 0x55}, {0x3f, 0x56}, {0x40, 0x57}, {0x41, 0x58}, {0x42, 0x59}, {0x43, 0x5a},
    {0x44, 0x5b}, {0x57, 0x5c}, {0x58, 0x5d}, {0x38, 0x5e}, {0xb8, 0x5f}, {0x1d, 0x60}, {0x9d, 0x61}, {0x2a, 0x62},
    {0x36, 0x63}, {0x0e, 0x64}, {0x39, 0x65}, {0x1c, 0x66}, {0x01, 0x67}, {0x0f, 0x68}, {0x3a, 0x69}, {0xb7, 0x6a},
    {0x46, 0x6b}, {0xc5, 0x6c}, {0xd2, 0x6d}, {0xd3, 0x6e}, {0xc7, 0x6f}, {0xcf, 0x70}, {0xc9, 0x71}, {0xd1, 0x72},
    {0x45, 0x73}, {0x52, 0x74}, {0x4f, 0x75}, {0x50, 0x76}, {0x51, 0x77}, {0x4b, 0x78}, {0x4c, 0x79}, {0x4d, 0x7a},
    {0x47, 0x7b}, {0x48, 0x7c}, {0x49, 0x7d}, {0xb5, 0x7e}, {0x37, 0x7f}, {0x4a, 0x80}, {0x4e, 0x81}, {0x53, 0x82},
    {0x9c, 0x83}, {0xc8, 0x84}, {0xd0, 0x85}, {0xcb, 0x86}, {0xcd, 0x87},
};
/* Pure copy of the same scan IDs used by keyboard startup; no arena or device. */
void rx_keyboard_scan_defaults(int32_t out[256]) {
    for (int i = 0; i < 256; ++i) out[i] = 0;
    for (int i = 0; i < 101; ++i) out[k_scan_ids[i].scan] = k_scan_ids[i].id;
}
uint32_t rx_keyboard_init_scan_table(uint32_t dev) {
    SU32(dev, 0x9a8, 0); SU32(dev, 0x9ac, 0);
    if (!U8(0, 0x00da687c)) {
        SU8(0, 0x00da687c, 1);
        crt_memset(0x00a95848, 0, 0x100);
        for (int i = 0; i < 101; i++) SU8(0, 0x00a95848 + k_scan_ids[i].scan, k_scan_ids[i].id);
    }
    uint32_t id = 0x23;
    for (rx_addr e = 0x00a85b28; e < 0x00a85cbc; e += 4, id++)
        for (uint32_t sc = 0; sc < 0x100; sc++) {
            uint8_t c = U8(0, 0x00a95848 + sc);
            if (c != 0 && c == id) { SU32(0, e, sc); break; }
        }
    SU32(dev, 0x998, 0); SU32(dev, 0x99c, 0); SU32(dev, 0x9a0, 0); SU32(dev, 0x9a4, 0);
    return dev;
}

/* msvcr90 srand(seed): the per-thread data's _holdrand (ptd + 0x14) */
static void crt_srand(uint32_t seed) { SU32(crt_ptd(), 0x14, seed); }

/* ReflexWinMain: g_EngineContext = XenonCore (0x1f0 bytes, FUN_00843950), before the game object (unit 10's rx_mxgame_create) */
rx_addr rx_engine_context_create(void) {
    rx_addr ctx = rx_reflex_aligned_alloc(0x1f0);
    rx_xenon_core_ctor(ctx);
    SU32(0, 0x00a95844, ctx);
    SU32(ctx, 0x10, 0x00d728d0);                              /* the engine (frame timing) object */
    return ctx;
}

void rx_engine_startup(fmt_install* in, uint32_t tick_ms) {
    const fmt_vfs* vfs = fmt_install_vfs(in);
    const rx_addr ctx = U32(0, 0x00a95844);                   /* rx_engine_context_create */
    /* THE DISPLAY MODE ReflexWinMain applies before Engine::Startup: Display_SetMode with the GraphicsConfig_Load 00883500 /
     * GraphicsConfig_FindKey 00883820 values of "graphics.cfg" (+0xc of each key). The reference boot's install resolves
     * ScreenWidth 1280 / ScreenHeight 720 (TextureQuality 2 is already the XenonCore ctor's +0x64). STAND-IN: the product
     * has no graphics.cfg parser yet (core/plan/QUEUE.md "build6 vehicles": port GraphicsConfig_Load at engine startup);
     * the values here are the player's install file, so the engine sees the display the reference does. The animated
     * camera's fov converts the path's horizontal angle through width / height (0063e850), so the intro fly-by's fov
     * depends on it (640 x 480 from the ctor would be 33 % wider). The ctor's own defaults are in rx_xenon_core_ctor. */
    SU32(ctx, 0x44, 1280);                                    /* display +0x08 width */
    SU32(ctx, 0x48, 720);                                     /* display +0x0c height */
    /* Engine::Startup: the Registry (Engine.bxml and the registries it lists), stored in the context (+0xc) */
    SU32(ctx, 0xc, rx_registry_startup(in));
    /* srand(GetTickCount()); g_GameRandSeed = GetTickCount(); g_GameRandCount = 0 */
    crt_srand(tick_ms);
    SU32(0, 0x009cb860, tick_ms);
    SU32(0, 0x009cb864, 0);
    /* XenonCore vf6 (00843a10 at 0084e45f): the input manager, the context's +0x34 (it builds the keyboard scan tables through
     * Keyboard_Create 00817090 -> Keyboard_InitScanTable 00816ba0 before InputNames / the key bindings read them) */
    rx_xenon_core_vf6(ctx);
    /* the input names (guard 0x00d92f48, first use in Engine::Startup) and the key bindings of the registry 0x00d92f50 */
    if (!(U32(0, 0x00d92f48) & 1)) { SU32(0, 0x00d92f48, U32(0, 0x00d92f48) | 1); rx_input_names_init(); RX_ATEXIT(0x009127c0); }
    rx_keybind_load(vfs, RX_KEYMAP_REGISTRY, "KeyBind_Core.bxml");
    rx_keybind_load(vfs, RX_KEYMAP_REGISTRY, "KeyBind_App.bxml");
    /* the asset Database (FUN_0084e4d0): the context's +8 */
    rx_addr db = rx_database_create();
    rx_database_register_all(db);
    SU32(ctx, 8, db);
    /* the renderer's vertex declaration cache (+0x14: a vector at +0x18 inside a 0x7f0-byte object) and the device (+0x1ec) */
    rx_addr decls = rx_reflex_aligned_alloc(0x7f0);
    rx_vec_proxy_init16(decls + 0x18); SU32(decls, 0x24, 0); SU32(decls, 0x28, 0); SU32(decls, 0x2c, 0);
    SU32(ctx, 0x14, decls);
    SU32(ctx, 0x1ec, rx_d3d_device_create());
    /* [0x00d6daf0] = a 0x80000-byte scratch block (the old one freed): cursor and end */
    SU32(0, 0x00d6db00, 0);
    if (U32(0, 0x00d6daf0)) rx_reflex_aligned_free(U32(0, 0x00d6daf0));
    rx_addr blk = rx_reflex_aligned_alloc(0x80000);
    SU32(0, 0x00d6daf0, blk); SU32(0, 0x00d6daf4, blk + 0x7ffff); SU32(0, 0x00d6daf8, blk);
    /* the engine frame timing's settings (0084e52c..0084e56a, 0084e641): the FPS display's registry id (+0x84), DeltaSecondsClamp from the Registry's
     * "Game" group (+0x48: 0.1 s), Engine::vf0(60.0) = the frame cap 1 / 60 s (+0x14); the constructor left both at FLT_MAX */
    {
        const rx_addr reg = U32(ctx, 0xc), eng = 0x00d728d0;
        SU32(eng, 0x84, rx_registry_vf1_find(reg, 0x0095275c, 0x00952758, 1));
        const rx_addr clamp = rx_registry_vf1_find(reg, 0x00976afc, 0x00976ae8, 1);
        if (clamp) rx_registry_vf17_floats(reg, clamp, eng + 0x48, 1);
        rx_engine_set_frame_rate(eng, 60.0f /* 0x42700000 */);
    }
    /* the engine frame timing's counter stamps (0084e705..0084e734: QueryPerformanceCounter into +0x60, +0x68 (the last frame's counter
     * Engine::UpdateTiming measures its first dt from) and +0x70 (the FPS clock), time +0x3c / +0x40 = 0, tick +0x4c / +0x80 = 0) */
    {
        const rx_addr eng = 0x00d728d0;
        const rx_addr c = rx_sp() - 8;
        loop_qpc(c);
        const uint64_t t = rx_rd64(c);
        rx_wr64(eng + 0x60, t); rx_wr64(eng + 0x68, t); rx_wr64(eng + 0x70, t);
        SF32(eng, 0x3c, 0.0f); SF32(eng, 0x40, 0.0f);
        SU32(eng, 0x4c, 0); SU32(eng, 0x80, 0);
    }
}
