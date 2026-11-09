/* build_event_test.c -- the end-to-end driver of construction unit 8 (core/notes/construction.md section 16.6): the core session from
 * an install to a running race, composed from every construction unit's API, then N race frames and the canonical dump.
 *   build_event_test <install root> <out prefix> [frames (600)] [level (NAT_Concord)]
 * Steps (each the API of the unit that owns it):
 *   1. memory, the install, the executable's sections and the CRT data, the startup (unit 12b rx_globals_init: the static
 *      initializers, Engine::Startup, MXGame and MXGame::Startup (unit 10), the init thread's table loads (unit 12a));
 *   2. the event spec (rx_event_spec: the Unity front end's choices; the reference event by default) and its level (fmt_level, the
 *      subscene reads of the world load);
 *   3. the event's setup as the front end's "start event" does it (Boot::start_event of the world runtime): EnsureLocalPlayers,
 *      SetupEvent, StartEvent (unit 8): the level requested (MXGame state 3);
 *   4. MXGame::Update frames (unit 8's state machine: the level load, FUN_00759f00's world (units 3 / 4 / 5 / 6 / 7 / 9), the activity,
 *      the players and drivers, the grid, the race world entered) until the race runs (state 2), then the race frames;
 *   5. <prefix>-canon.bin / -world.bin (rx_canon) and <prefix>-anchors.txt for rx_canon_cmp.py against the original's capture
 *      (a baton boot of the same event with REFLEX_WORLD_CANON / REFLEX_WORLD_OUT after the same number of frames).
 * A function no unit has ported yet stops the product with its address (rx_fn: the coverage list); the driver prints how far it got. */
#include "../src/fmt_db.h"
#include "../src/fmt_scene.h"
#include "../src/rx_image.h"
#include "../src/rx_heap.h"
#include "../game/build_globals.h"
#include "../game/build_vehicle.h"
#include "../game/build_rider.h"
#include "../game/build_alloc.h"
#include "../game/build_asset.h"
#include "../game/build_activity.h"
#include "../game/build_activity_int.h"
#include "../game/build_mxworld.h"
#include "../game/build_alloc.h"
#include "../game/crt.h"
#include "../game/race_misc.h"
#include "rx_canon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <execinfo.h>

static void on_abort(int sig) { void* bt[40]; int n = backtrace(bt, 40); (void)sig; backtrace_symbols_fd(bt, n, 2); _exit(134); }

static const char* g_step = "start";
static void step(const char* s) { g_step = s; fprintf(stderr, "build_event_test: %s\n", s); }
static void on_exit_report(void) { fprintf(stderr, "build_event_test: last step: %s\n", g_step); }

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: build_event_test <install> <out prefix> [frames] [level]\n"); return 2; }
    if (getenv("RX_BT")) signal(SIGABRT, on_abort);
    atexit(on_exit_report);
    const int frames = argc > 3 ? atoi(argv[3]) : 600;
    rx_event_spec spec;
    rx_event_spec_default(&spec);
    if (argc > 4) snprintf(spec.location, sizeof spec.location, "%s", argv[4]);

    step("memory and install");
    if (!rx_mem_init()) { fprintf(stderr, "arena reservation failed\n"); return 1; }
    fmt_install* in = fmt_install_open(argv[1]);
    if (!in) { fprintf(stderr, "install: %s\n", fmt_error()); return 1; }
    if (rx_image_load(fmt_install_vfs(in)) != 0 || rx_image_load_crt(fmt_install_vfs(in)) != 0) { fprintf(stderr, "image: %s\n", fmt_error()); return 1; }

    step("startup (unit 12b: static initializers, Engine::Startup, MXGame, the init thread's loads)");
    rx_globals_init(in);
    rx_database_set_install(in);

    /* ---- TEST-DRIVER STAND-IN: the engine's job pool ---- 0084ea00 (the engine frame mark, build_activity_frame.c) waits for the
     * job pool at 0x00d94ae0 to be idle; the pool lives behind its function-local static guard (0x00d94ad8 bit 0, constructed by
     * 00881a70, 488 bytes, not ported yet: loop unit). The driver builds the single-threaded construction state: an empty pool (no
     * jobs at +0x8830, no workers), which is what the product's construction phase (no submitted jobs) needs. Replace with the
     * port of 00881a70 (or its lazy construction inside rx_engine_frame_mark) when the loop unit lands it. */
    if (!(U8(0, 0x00d94ad8) & 1)) {
        crt_memset(0x00d94ae0, 0, 0x88e4);
        SU32(0x00d94ae0, 0x8844, 3);          /* the constructor's two workers (the product runs their bodies inline) */
        SU8(0, 0x00d94ad8, U8(0, 0x00d94ad8) | 1);
    }

    /* the session's "Network" channel [0x00da6ca0]: built by the startup (MXGame::Startup -> 006de4b0, build_mxgame_init.c) */

    /* ---- TEST-DRIVER STAND-IN: the game init thread's loaders (0075ed60, not ported; build_globals_game reproduces only its
     * table loads). 0075ed60:
     *   - when [0x00d92ef4] is 0, allocates 0x40 (0078fbe0) and constructs the FactoryGearLoader (0053cbd0, EDI = it) into it: the
     *     rider entry / part path (0053e8c0 -> part_get -> 00543830) reads its package cache ([0x00d92ef4] + 8); without it the
     *     map is null and the cache walk dereferences a null key;
     *   - calls 00540610 with MXGame + 0x154b0: the VehicleHierarchies table (+0x3c) and the parts loader [0x00d92ef0] the vehicle
     *     spec path (00542b20 -> 00543830) needs (otherwise that "package cache" lands at address 4 and the walk never ends).
     * Both calls are the original's, in its order. Replace with the port of 0075ed60 when the startup unit lands it. */
    if (U32(0, 0x00a946b8) != 0 && U32(0, 0x00d92ef4) == 0) {
        const rx_addr gl = rx_reflex_aligned_alloc(0x40);
        SU32(0, 0x00d92ef4, gl ? rx_gear_loader_construct(gl) : 0);
    }
    if (U32(0, 0x00a946b8) != 0 && U32(0, 0x00d92ef0) == 0) rx_vehicle_list_init_loader(U32(0, 0x00a946b8) + 0x154b0);
    /* 0075ed60's game databases (the same pair the session API loads): Animations (the rider's animation trees / model hierarchy)
     * and RiderPhysics (the ragdoll / IK bxml "MXWreckingRagdoll" 004246c0 reads). The product has no loader thread: load them
     * synchronously, as EngineDependencies above. */
    if (U32(0, 0x00a946b8) != 0) {
        const rx_addr db = U32(U32(0, 0x00a95844), 8);
        rx_database_load(db, in, "Animations", "Database\\", 0, 0);
        rx_database_load_package(db, in, "Animations", "Animations", 0, 0);
        rx_database_load(db, in, "RiderPhysics", "Database\\", 0, 0);
        rx_database_load_package(db, in, "RiderPhysics", "RiderPhysics", 0, 0);
    }

    if (getenv("RX_EVENT_DIAG")) {
        fprintf(stderr, "diag channels: Session=%08x Stunts=%08x StuntScoring=%08x ActivityGlobal=%08x VehicleTrigger=%08x\n",
                U32(0, 0x00da6ca0), U32(0, 0x00a94304), U32(0, 0x00a94318), U32(0, 0x00a93f74), U32(0, 0x00a89bc8));
    }

    if (getenv("RX_EVENT_DIAG")) {
        const rx_addr ch = U32(0, 0x00a94304), map = ch + 8, head = U32(map, 0x18);
        fprintf(stderr, "diag: ch=%08x map=%08x head=%08x head[0]=%08x head[4]=%08x head[8]=%08x size=%u isnil=%u\n",
                ch, map, head, U32(head, 0), U32(head, 4), U32(head, 8), U32(map, 0x1c), U8(head, 0x2d));
        {
            static const rx_addr chs[4] = {0x00a94304, 0x00a94318, 0x00da6ca0, 0x00a89bc8};
            static const rx_addr nms[8] = {0x0095ba28, 0x0093ee98, 0x0093eeb0, 0x00941354, 0x00941368, 0x00935e84, 0x00960770, 0x009407c4};
            for (int ci = 0; ci < 4; ci++) {
                const rx_addr c = U32(0, chs[ci]), m = c + 8, h = U32(m, 0x18);
                fprintf(stderr, "  chan %d %08x map=%08x head=%08x size=%u headnil=%u\n", ci, c, m, h, U32(m, 0x1c), U8(h, 0x2d));
                for (int ni = 0; ni < 8; ni++) {
                    crt_str_operator_assign(0x00af6660, nms[ni]);
                    rx_addr x = U32(h, 4); int steps = 0;
                    while (U8(x, 0x2d) == 0 && steps < 40) { if (crt_str_less(x + 0xc, 0x00af6660)) x = U32(x, 8); else x = U32(x, 0); steps++; }
                    if (steps >= 40) fprintf(stderr, "    name %08x CYCLE after %d steps\n", nms[ni], steps);
                }
            }
        }
        crt_str_operator_assign(0x00af6660, 0x0095ba28);
        {   /* lower_bound's walk for "StuntBegin", bounded */
            rx_addr result = head, x = U32(head, 4);
            for (int i = 0; i < 24 && U8(x, 0x2d) == 0; i++) {
                const uint32_t lt = crt_str_less(x + 0xc, 0x00af6660);
                fprintf(stderr, "  lb %2d x=%08x less=%u keyptr=%08x keyres=%u -> %s\n", i, x, lt, U32(x+0xc,4), U32(x+0xc,0x18), lt ? "right" : "left");
                if (lt) x = U32(x, 8); else { result = x; x = U32(x, 0); }
            }
            fprintf(stderr, "  lb end result=%08x x=%08x isnil=%u\n", result, x, U8(x, 0x2d));
        }
        for (rx_addr x = U32(head, 4); U8(x, 0x2d) == 0;) {
            static int guard = 0;
            if (guard++ > 40) break;
            char k[64]; for (int j = 0; j < 63; j++) k[j] = (char)U8(x + 0xc, j);
            k[63] = 0;
            fprintf(stderr, "  node %08x L=%08x R=%08x P=%08x nil=%u sz=%u '%s'\n", x, U32(x,0), U32(x,8), U32(x,4), U8(x,0x2d), U32(x+0xc,0x18), k);
            x = U32(x, 8);
        }
        { const rx_addr d = U32(0x00af6660, 0x18) >= 0x10 ? U32(0x00af6660, 4) : 0x00af6664;
          char kb[40]; for (int j = 0; j < 39; j++) kb[j] = (char)U8(d, j); kb[39] = 0;
          fprintf(stderr, "diag: global key str 0af6660: ptr=%08x size=%u res=%u data='%s'\n", U32(0x00af6660,4), U32(0x00af6660,0x14), U32(0x00af6660,0x18), kb); }
    }

    step("the event spec and its level");
    rx_event_set_spec(&spec);
    fmt_level* lv = fmt_level_open(in, spec.location);
    if (!lv) { fprintf(stderr, "level %s: %s\n", spec.location, fmt_error()); return 1; }
    rx_event_set_level(lv);
    const rx_addr game = U32(0, 0x00a946b8), ui = U32(0, 0x00a93fcc);
    if (!game) { fprintf(stderr, "no MXGame after the startup\n"); return 1; }
    /* the front end is not core: the game is in its running state (2) when the event starts, as after the front end's start */
    if (U32(game, 0x18) != 2) SU32(game, 0x18, 2);

    /* ---- TEST-DRIVER STAND-IN: the front end's world ---- The original runs StartEvent from the front end, in its UI world:
     * game+0x20c8 = the UI MXWorld (session started +0x20018 = 1, its activity table's revision +0x2013c = [0x00adffe0]+0x68),
     * game+0x5a54 = 0, the UI scene ctx+0x1c active (+0x34 = 1) and the event scene ctx+0x20 inactive (007d20b0). 0075c5f0 then
     * answers 0 ("not viewing the event track"), the path StartEvent 0059ade0 and Game_SetupActivityArea 0075c430 take. The
     * product driver skips the front end, so it builds that state here (values from the reference baton boot; see the session
     * notes). THE SESSION API MUST PROVIDE THE SAME STATE LATER (before StartEvent): game+0x20c8 = the UI world with +0x20018 =
     * 1 and +0x2013c = the UI table's +0x68, game+0x5a54 = 0, and both scenes' active flags through 007d20b0. */
    {
        const rx_addr ctx = U32(0, 0x00a95844);
        const rx_addr cfg = rx_reflex_aligned_alloc(0x20a8);      /* a zero level config (the constructor reads only +0x20a4) */
        const rx_addr fe_world = rx_reflex_aligned_alloc(0x22150);
        if (!cfg || !fe_world) { fprintf(stderr, "the front-end world stand-in: out of memory\n"); return 1; }
        crt_memset(cfg, 0, 0x20a8);
        crt_memset(fe_world, 0, 0x22150);
        rx_mxworld_construct(fe_world, cfg);
        SU32(fe_world, 0x20018, 1);                               /* the front end's session has started */
        SU32(fe_world, 0x2013c, U32(0x00adffe0, 0x68));           /* the UI activity table's revision: no activity area update */
        SU32(fe_world, 0x20138, U32(ctx, 0x1c));                  /* the UI world's scene is the UI scene (game+4); no physics world */
        SU32(game, 0x20c8, fe_world);
        SU32(game, 0x5a54, 0);
        rx_scene_set_active(1, U32(ctx, 0x1c));                   /* the UI scene on, the event scene off (007d20b0) */
        rx_scene_set_active(0, U32(ctx, 0x20));
    }
    if (getenv("RX_EVENT_DIAG")) {   /* the front-end state this driver stands in for (0075c5f0 must answer 0) */
        const rx_addr ctx = U32(0, 0x00a95844);
        char lv[0x104];
        for (uint32_t i = 0; i < sizeof lv; i++) lv[i] = (char)U8(game, 0x39c0 + i);
        fprintf(stderr, "diag: game+18=%u 20c8=%08x 5a54=%08x 15b54=%u level='%s'\n",
                U32(game, 0x18), U32(game, 0x20c8), U32(game, 0x5a54), U32(game, 0x15b54), lv);
        fprintf(stderr, "diag: ctx=%08x +1c=%08x [1c+34]=%08x +20=%08x [20+34]=%08x | view=%u\n",
                ctx, U32(ctx, 0x1c), U32(U32(ctx, 0x1c), 0x34), U32(ctx, 0x20), U32(U32(ctx, 0x20), 0x34),
                rx_game_is_view_track(game));
    }
    step("the event's setup (EnsureLocalPlayers, SetupEvent, StartEvent)");
    rx_ensure_local_players(0, 0);
    rx_ensure_local_players(0, 0);
    rx_setup_event(ui);
    if (getenv("RX_EVENT_DIAG") && U8(game, 0x1541c) > 0) {   /* the state 00767200 will be called with */
        const rx_addr P = rx_player_active(game + 0x7460, 0, 3);
        fprintf(stderr, "diag: P=%08x +10=%u +94=%08x +1ac=%08x +1b0=%08x +1b4=%08x +1b8=%08x +1c0=%08x +1c4=%08x +1d0=%08x +1d4=%08x +1d8=%08x\n",
                P, U32(P, 0x10), U32(P, 0x94), U32(P, 0x1ac), U32(P, 0x1b0), U32(P, 0x1b4), U32(P, 0x1b8),
                U32(P, 0x1c0), U32(P, 0x1c4), U32(P, 0x1d0), U32(P, 0x1d4), U32(P, 0x1d8));
    }
    rx_start_event(ui);
    if (U32(game, 0x18) != 3) { fprintf(stderr, "the level was not requested (MXGame state %u)\n", U32(game, 0x18)); return 1; }

    step("the load (MXGame::Update states 3 .. 0xb)");
    int f = 0;
    for (; f < 10000 && U32(game, 0x18) != 2; f++) rx_mxgame_update(game);
    if (U32(game, 0x18) != 2) { fprintf(stderr, "the race did not start (state %u after %d frames)\n", U32(game, 0x18), f); return 1; }
    fprintf(stderr, "build_event_test: race started after %d load frames\n", f);

    step("the race frames");
    for (int k = 0; k < frames; k++) rx_mxgame_update(game);

    step("the canonical dump");
    char path[1024], info[256] = "";
    rx_canon_root roots[4]; int nroots = 0;
    roots[nroots].addr = game; roots[nroots].key = (uint32_t)nroots; nroots++;
    snprintf(path, sizeof path, "%s-canon.bin", argv[2]);
    if (!rx_canon_write(path, roots, nroots, getenv("RX_CANON_CLASSES"), info, sizeof info)) return 1;
    snprintf(path, sizeof path, "%s-world.bin", argv[2]);
    if (!rx_canon_image(path)) return 1;
    snprintf(path, sizeof path, "%s-anchors.txt", argv[2]);
    FILE* a = fopen(path, "w");
    if (a) { fprintf(a, "G:00a946b8\nG:00a93f74\nG:00ae0e08\nG:00ae1b18\n"); fclose(a); }
    printf("event built: %s, %d race frames; heap blocks %d; %s\n", spec.location, frames, rx_heap_blocks(), info);
    return 0;
}
