#include "replay_clip.h"
#include "rider_cap.h"
/* build_activity_game.c -- construction unit 8: MXGame::Update 00757fb0 (MXGame vtable slot 6), the game's state machine and frame
 * (construction.md section 16.5).
 *   0 start: the init thread (00760400, the MXGame unit's) and the package cache; 1 its end: an empty level config starts the
 *   front end (state 2), else the level is loaded at once (the "start in level" path: 00756060 / 007560f0 / FUN_00759f00 / 0075f890,
 *   state 0xb); 2 running (front end or event); 3 a level was requested (MXGame_RequestLevel): the activity area, the load wait, the
 *   level config's database, its packages (00756060 / 007560f0); 4 the packages loaded -> the world construction FUN_00759f00 as the
 *   loader's "Subscene Creation" job; 5 the job done -> 6; 6 every player loaded: the grid slots (0075f890) and the start (0075e020)
 *   -> 7; 7 the UI's loading page done (005a4160) -> 8; 8 the UI ready (+0x2f4) -> 0xb; 9 / 10 a restart (the event again);
 *   0xb the race world entered (Game_EnterRealWorldAudio) -> 2.
 * Every frame after the state: the engine frame time, the players' deferred work (00552930), the session tick, the activity
 * manager's ticks through the current world's vf5 (MXWorld::vf5 = the world frame, loop unit), the packages / loaders / records /
 * music / Lua jobs / vehicle effects / mud / clip recorder updates of the other units.
 * Boundaries (oracle: the original; product: none or the accepting answer): the renderer's device check, the UI (pages, the
 * front end's switch, the HUD), the network session (Quazal), audio and music, the OS events of the loader thread. Callees of other
 * units are called through their ports when registered, else the original (the product aborts naming it: construction.md 16.7). */
#include "build_activity_util.h"
#include "build_activity_int.h"
#include "build_activity_freeride_run.h"
#include "race.h"
#include "race_misc.h"
#include "world_misc.h"
#include "world_wreck.h"
#include "world_node.h"
#include "loop_world.h"
#include "race_activity.h"
#include "race_activity_freestyle.h"
#include "race_activity_ui.h"
#include "world_veh.h"
#include "world_veh_damage.h"
#include "fx.h"
#include "fx_util.h"
#include "loop_player.h"
#include "loop_engine.h"
#include "race_activity.h"
#include "anim_scenario.h"
#include "build_vehicle.h"
#include "build_asset.h"
#include "career_race_start.h"
#include "../src/rx_x87.h"
#include <string.h>
#include "music_player.h"

/* the original of a function with register arguments (eax, ecx, edx, esi, edi; n stack words) -> EAX */
static uint32_t orig_regs(rx_addr fn, uint32_t eax, uint32_t ecx, uint32_t edx, uint32_t esi, uint32_t edi, int n, const uint32_t* a) {
    rx_regs in, o; memset(&in, 0, sizeof in);
    in.r[0] = eax; in.r[1] = ecx; in.r[2] = edx; in.r[6] = esi; in.r[7] = edi;
    in.r[3] = fx_reg(FX_EBX); in.r[5] = fx_reg(FX_EBP);   /* the frame's EBX / EBP (the original's callees push them: dead stack) */
    rx_call_original_ex(fn, &in, &o, n, a);
    return o.r[0];
}
/* UI / ONLINE BOUNDARY (documented in core/plan/STOP_AUDIT.md and QUEUE.md "w-ctx"): orig_regs for originals the product never runs.
 *  - UI_BOUNDARY_REGS: MXGame::Update's state 0 / 1 (the init thread's start / end: 0053c350, 004dc0a0, 0053c3d0) and the load abort of
 *    the UI's quit (0075a2b0, 007616d0's front-end UI world update). The session API runs one event per session and forces the MXGame
 *    state to 2 then 3 (api/rx_session.c); Unity restarts by creating a new session; no API reaches states 0 / 1 / 9 / 10, the quit
 *    flag +0x15b58 (set by the UI's quit) or the front-end world (+0x745c +0x284 is that world's flag). Evidence key A1.
 *  - ONLINE_BOUNDARY_REGS: the session pause block (402540, 5ee840, 548ad0, 548b40): it runs only when the session's pause state
 *    [0x00da6734] (006e0c50 of the Quazal session) differs from its previous value; the product's ext_regs answers 0 for 006e0c50, so
 *    the state is 0 forever. The online players' load state (0075cfb0) runs only for an online session (the guard `online || +0x1c3c`).
 * Fatal in the product if reached (orig_regs is rx_call_original_ex). */
#define UI_BOUNDARY_REGS(fn, ...) orig_regs((fn), __VA_ARGS__)
#define ONLINE_BOUNDARY_REGS(fn, ...) orig_regs((fn), __VA_ARGS__)
#define UI_BOUNDARY_CALL_F2(fn, conv, a, b) act_call_f2((fn), (conv), (a), (b))
#define ONLINE_BOUNDARY_CALL_FN(fn, conv, ecx, n, a) act_call_fn((fn), (conv), (ecx), (n), (a))
/* not core (UI / network / audio / render / OS): oracle the original, product nothing (0) */
static uint32_t ext_regs(rx_addr fn, uint32_t eax, uint32_t ecx, uint32_t edx, uint32_t esi, uint32_t edi, int n, const uint32_t* a) {
#ifdef RX_ORACLE
    return orig_regs(fn, eax, ecx, edx, esi, edi, n, a);
#else
    (void)fn; (void)eax; (void)ecx; (void)edx; (void)esi; (void)edi; (void)n; (void)a; return 0;
#endif
}
static void session_static(void) {
    if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "the session's static initialiser has not run");
}
/* EBX / ESI / EDI of MXGame::Update at the calls whose callees push them (see the register model in rx_mxgame_update) */
static void mxgame_regs(rx_addr frame_sp, uint32_t ebx, uint32_t esi, uint32_t edi) {
    fx_set_reg(FX_EBP, frame_sp - 4);   /* push ebp; mov ebp, esp: the frame's EBP (earlier original calls with zeroed register sets clobbered it) */
    fx_set_reg(FX_EBX, ebx);
    fx_set_reg(FX_ESI, esi);
    fx_set_reg(FX_EDI, edi);
}
/* FUN_0084ea00 (EDI = the engine [core + 0x10]): the engine's frame bookkeeping (loop unit), ported as rx_engine_frame_mark
 * (build_activity_frame.c) */
static void engine_frame_mark(void) {
    const rx_addr obj = U32(U32(0, 0x00a95844), 0x10);
    ACT_CALL(0, rx_engine_frame_mark(obj));
}
static uint32_t player_mode(void) { uint32_t pm; ACT_CALLR(pm, 0, rx_game_player_mode()); return pm; }
static float dt_of(rx_addr L) { return F32(L, 0xc); }
/* 006de690 (EDI = session; dt): the session tick (the fx unit's port models the original's callee-saved registers from its own
 * adapter's entry, so the oracle build calls the original from here; the product the port) */
static void session_tick(float dt, rx_addr ses) {
#ifdef RX_ORACLE
    rx_regs in, o; memset(&in, 0, sizeof in); in.r[7] = ses;
    const uint32_t a = rx_fb(dt);
    rx_call_original_ex(0x006de690, &in, &o, 1, &a);
#else
    ACT_CALL(1, rx_fx_session_tick(dt, ses));
#endif
}
static uint32_t fbits(float f) { return rx_fb(f); }
/* Fn_00760470 (stdcall loading screen; xmm0 = progress): the loading screen's progress (UI) */
static void loading_progress(rx_addr g, float p) {
#ifdef RX_ORACLE
    rx_regs in, o; memset(&in, 0, sizeof in); in.xmm[0].v[0] = p;
    const uint32_t a = U32(g, 0x15b10);
    rx_call_original_ex(0x00760470, &in, &o, 1, &a);
#else
    (void)g; (void)p;
#endif
}
/* MXGame_UpdateLoadWait 0075d760 (stdcall game, dt) -> waiting */
static uint32_t load_wait(rx_addr g, float dt) { return act_call_f2(0x0075d760, RX_STDCALL, g, dt); }
/* FUN_0075a2b0 (EDI = game): the load aborted (the UI's world back) */
static void load_abort(rx_addr g) { UI_BOUNDARY_REGS(0x0075a2b0, 0, 0, 0, 0, g, 0, 0); }
/* Fn_00756700 (EDX = the level config) -> st(0): the level's load progress shown on the loading screen: the mean of the packages'
 * progress floats (+0x618, count +0x620 as unsigned) * 0.75 + the float at +0x20a0 * 0.2 + 0.05, all on the x87 at the main
 * thread's 53-bit precision (the callers store it with fstp dword). */
rx_f80 rx_level_load_progress(rx_addr cfg) {
    rx_f80 sum = rx_f80_from_f32(0.0f);
    for (uint32_t i = 0; i < U32(cfg, 0x620); i++) sum = rx_f80_add(sum, rx_f80_from_f32(F32(cfg, 0x618 + 4 * i)), 53, 0);
    rx_f80 count = rx_f80_from_int(I32(cfg, 0x620));
    if (I32(cfg, 0x620) < 0) count = rx_f80_add(count, rx_f80_from_f32(4294967296.0f /* 0x97731c */), 53, 0);
    rx_f80 mean = rx_f80_mul(rx_f80_div(sum, count, 53, 0), rx_f80_from_f32(0.75f /* 0x935aa8 */), 53, 0);
    rx_f80 extra = rx_f80_mul(rx_f80_from_f32(F32(cfg, 0x20a0)), rx_f80_from_f32(0.2f /* 0x92e388 */), 53, 0);
    return rx_f80_add(rx_f80_add(extra, mean, 53, 0), rx_f80_from_f32(0.05f /* 0x9d3bfc */), 53, 0);
}
RX_FUNCTION(0x00756700, rx_level_load_progress)
static float load_progress(rx_addr c) { return rx_f80_to_f32(rx_level_load_progress(c), 0); }

/* the restart / abort path of states 3 .. 0xb: +0x15b58 set by the UI's quit */
static void abort_to(rx_addr g, uint32_t state_on_level) {
    SU32(g, 0x15b50, 1);
    load_abort(g);
    SU32(g, 0x15b58, 0);
    SU32(g, 0x18, state_on_level ? (U8(g, 0x734c) != 0) + 2 : 2);
    loading_progress(g, F32(0, 0x009cd9e8));
}

/* the Player List's Loaded / Ready flags of every local player cleared (state 3): the table's rows of PlayerLocal = 1 (004199d0 /
 * 004fc5c0 / 00417df0: the non-unique index iteration) */
static void players_unload(rx_addr L) {
    (void)L;
    const rx_addr t = ACT_PLAYER_LIST;
    for (uint32_t r = 0; r < rx_tbl_row_count(t); r++) {
        if (act_cell(t, 0x0093de90, r) != 1) continue;          /* "PlayerLocal" */
        static const rx_addr cols[2] = {0x0093de28, 0x0093de38};  /* "Loaded", "Ready" */
        for (int k = 0; k < 2; k++) {
            if (act_cell(t, cols[k], r) != 1) continue;
            SU32(L, 0x4c - 0x2c * k, 0);
            const uint32_t c = rx_tbl_column_index(t, cols[k]);
            if (act_cell(t, cols[k], r) != 0) SU32(t, 0x68, U32(t, 0x68) + 1);
            if (c >= (U32(t, 0x14) - U32(t, 0x10)) >> 2) act_unreached(0x007588c5, "column index (_invalid_parameter_noinfo)");
            ACT_CALL(3, rx_tbl_set_int(U32(U32(t, 0x10), 4 * c), r, L + 0x4c - 0x2c * k));
        }
    }
}

/* ======================================================================================================================== */

uint32_t rx_mxgame_update(rx_addr g) {
    ACT_FRAME_EBP(L, 8, 0x16c, 3);
    /* The callee-saved registers as the original holds them (fx_util.h): push ebp; mov ebp, esp; mov ebx, ecx. The original
     * callees this frame calls push them, and the dead words they leave are what the particle templates copy into the
     * unused words of the particle records (+0x6c). */
    fx_set_reg(FX_EBP, L_sp - 4);
    fx_set_reg(FX_EBX, g);
    const rx_addr core = U32(0, 0x00a95844);
    {   /* Renderer_EnsureDeviceReady 007ae4e0 (the D3D device; render) */
#ifdef RX_ORACLE
        const uint32_t a = U32(core, 0x14);
        if ((rx_call_original(0x007ae4e0, RX_STDCALL, 0, 1, &a) & 0xff) == 0) { ACT_END(L); return 1; }
#endif
    }
    if (U32(0, 0x009dd598) != 0) SU32(0, 0x009dd598, 0);
    const float dt = F32(U32(core, 0x10), 0x18);
    SF32(L, 0xc, dt);
    SU32(0, 0x009cb26c, g); SF32(0, 0x009cb270, dt); SU32(0, 0x009cb274, 0);
    {   /* the debug pause keys (KeyMap 0xd5b5b8 / 0xd5c4d0, mask 7) */
        int pause = 0;
        if (U8(0, 0x009b716d) == 0) {
            uint32_t k;
            ACT_CALLR(k, 2, rx_world_keymap_down(0x00d5b5b8, 7, 0));
            if (k) pause = 1;
        }
        uint32_t k2;
        ACT_CALLR(k2, 2, rx_world_keymap_down(0x00d5c4d0, 7, 0));
        if (k2) SU8(0, 0x009b716d, U8(0, 0x009b716d) == 0);
        if (pause) SU32(0, 0x00a95964, 1);
    }
    if (U32(0, 0x00a95964) != 0) { engine_frame_mark(); ACT_END(L); return 0; }
    session_static();
    {   /* the session's pause state (the guide / controller overlay: 006e0c50 of the device table [0x00da6cac]) changed: the worlds
         * paused or resumed (00767480), the players' controllers (00548ad0 / 00548b40; input), sounds (audio), the rumble.
         * 006e0c50 (49 bytes; EAX = the device table): 1 (paused) while the session's +0x58 / +0x5c is set, the window focus flag
         * [0x00a9596c] is clear, or the input manager's vf4 ([+0x10]; its byte +0x34) says paused; 0 (running) otherwise.
         * PLATFORM BOUNDARY (ext_regs): the product has no window and no session (the arena reads 0, which the original counts as
         * paused), so it returns 0 (not paused), the state of the running construction boot. Nothing gameplay reads the value: it
         * only selects whether the worlds / controllers / sounds are paused or resumed in this block. */
        const uint32_t st = ext_regs(0x006e0c50, U32(0, 0x00da6cac), 0, 0, 0, 0, 0, 0);
        if (U32(0, 0x00da6734) != st && rx_world_active_sound_table(g) != 0) {
            SU32(0, 0x00da6734, ext_regs(0x006e0c50, U32(rx_fx_session_manager(), 0xc), 0, 0, 0, 0, 0, 0));
            int resume = 1;
            if (U32(0, 0x00da6734) != 0 && ONLINE_BOUNDARY_REGS(0x00402540, U32(rx_fx_session_manager(), 0x10), 0, 0, 0, 0, 0, 0) == 0) {
                resume = 0;
                if (U32(g, 0x5a54)) rx_mxworld_set_paused(U32(g, 0x5a54), 1);
                if (U32(U32(g, 0x745c), 0x284) == 0) rx_mxworld_set_paused(U32(g, 0x20c8), 1);
                ext_regs(0x00805000, 0, 0, 0, 0, 0, 2, (const uint32_t[]){U32(U32(g, 0x20c8), 0x20014), 0});   /* Sound_PauseAll */
            }
            if (resume) {
                if (U32(U32(g, 0x745c), 0x288) == 0) {
                    const rx_addr w = rx_world_active_sound_table(g);
                    const int32_t n = I32(w, 0x21da8);
                    for (int32_t i = 0; i < n; i++) ONLINE_BOUNDARY_REGS(0x005ee840, 0, w + 0x21da4, 0, 0, 0, 1, (const uint32_t[]){(uint32_t)i});
                    if (U32(g, 0x5a54)) rx_mxworld_set_paused(U32(g, 0x5a54), 0);
                    rx_mxworld_set_paused(U32(g, 0x20c8), 0);
                    for (uint32_t i = 0; i < U8(g, 0x1541c); i++) {
                        const int32_t c = I32(rx_player_active(g + 0x7460, i, 3), 0x1f0);
                        if (c > -1) ONLINE_BOUNDARY_REGS(0x00548ad0, 0, 0, 0, 0, 0, 2, (const uint32_t[]){(uint32_t)c, 1});
                    }
                    ext_regs(0x00805000, 0, 0, 0, 0, 0, 2, (const uint32_t[]){0, 0});
                } else {
                    for (uint32_t i = 0; i < U8(g, 0x1541c); i++) {
                        const rx_addr P = rx_player_active(g + 0x7460, i, 3);
                        if (I32(P, 0x1f0) > -1) {
                            ONLINE_BOUNDARY_REGS(0x00548b40, 0, 0, 0, 0, 0, 1, (const uint32_t[]){U32(P, 0x1f0)});
                            ONLINE_BOUNDARY_REGS(0x00548ad0, 0, 0, 0, 0, 0, 2, (const uint32_t[]){U32(P, 0x1f0), 0});
                        }
                    }
                }
            }
            if (U32(0, 0x00da6734) != 0)
                for (uint32_t i = 0; i < U8(g, 0x1541c); i++) {
                    const int32_t c = I32(rx_player_active(g + 0x7460, i, 3), 0x1f0);
                    if (c > -1) { const rx_addr in = act_vcall(core, 8, 0, 0); act_vcall(in, 0xb4, 3, (const uint32_t[]){1, (uint32_t)c, 1}); }
                }
            SU32(U32(core, 0x10), 0x10, U32(0, 0x00da6734));
        }
    }
    SU32(U32(0, 0x00d92f10), 0x67a8, 4);
    switch (U32(g, 0x18)) {
    case 0:   /* the init thread (the MXGame unit's 00760400) */
        SU32(g, 0x18, 1);
        UI_BOUNDARY_REGS(0x0053c350, 0, 0, 0, g + 0x15554, 0, 0, 0);
        ext_regs(U32(0, 0x00916090), 0, 0, 0, 0, 0, 1, (const uint32_t[]){U32(g, 0x161f4)});   /* ResetEvent (OS) */
        ext_regs(U32(0, 0x00916090), 0, 0, 0, 0, 0, 1, (const uint32_t[]){U32(g, 0x161f8)});
        UI_BOUNDARY_REGS(0x004dc0a0, 0, g, 0x00760400, g + 0x161fc, 0, 2, (const uint32_t[]){4, 0});
        ACT_END(L);
        return 1;
    case 1: {   /* the init thread's end */
        engine_frame_mark();
        session_tick(dt, rx_fx_session_manager());
#ifdef RX_ORACLE
        const uint32_t w = rx_call_original(U32(0, 0x00916008), RX_STDCALL, 0, 2, (const uint32_t[]){U32(g, 0x161f4), 1});   /* WaitForSingleObject */
        if (w == 0x102) { ACT_END(L); return 1; }
#endif
        UI_BOUNDARY_REGS(0x0053c3d0, g + 0x15554, 0, 0, 0, 0, 1, (const uint32_t[]){fbits(dt)});
        if (act_vcall(U32(g, 0x745c), 0x9c, 0, 0) != 0) { ACT_END(L); return 1; }
        if (U8(g, 0x39c0) == 0) act_unreached(0x007583b2, "the front end's start (MXGame_SwitchToUIWorld: UI)");
        act_unreached(0x0075843e, "the start in a level (no front end)");
        break;
    }
    case 2:
        SU32(g, 0x15b58, 0);
        break;
    case 3: {   /* a level was requested */
        const uint32_t pm = player_mode();
        SU32(L, 0x10, pm);
        if (U32(g, 0x15b58) != 0) {
            SF32(g, 0x15b38, 0.0f); SF32(g, 0x15b34, 0.0f); SF32(g, 0x15b3c, 0.0f);
            if (pm == 2 || pm == 3) act_unreached(0x007587b8, "an online level request");
        }
        SU32(g, 0x15b58, 0);
        players_unload(L);
        if (U32(g, 0x7454) != 0) act_unreached(0x00758992, "the UI world's switch (MXGame_SwitchToUIWorld)");
        ACT_CALL(0, rx_game_setup_activity_area(g));
        if (pm == 2 || pm == 3) act_unreached(0x007589b2, "an online level load (0075f5a0)");
        if (load_wait(g, dt) != 0) break;
        engine_frame_mark();
        loading_progress(g, 0.0f);
        if (act_call_fn(0x0075f500, RX_STDCALL, 0, 1, (const uint32_t[]){g}) == 0) break;   /* MXGame_CanBeginWorldLoad */
        if (U8(g, 0x734c) != 0) {   /* the pending level becomes the config's name (Fn_00756040: EAX = name, ESI = config) */
#ifndef RX_ORACLE
            rx_level_config_set_name(g + 0x39c0, g + 0x734c);
#else
            orig_regs(0x00756040, g + 0x734c, 0, 0, g + 0x39c0, 0, 0, 0);
#endif
            SU32(g, 0x5a64, U32(g, 0x7450));
            SU8(g, 0x734c, 0);
            SU32(g, 0x7450, 0);
        }
        {   /* the level's database directory "Database\\" (the DLC path 0053c740 may replace it) */
            const rx_addr dir = L + 0x68;
            for (uint32_t i = 0; i < 10; i++) SU8(dir, i, U8(0x0093e434, i));
            for (uint32_t i = 10; i < 0x104; i++) SU8(dir, i, 0);
            SU32(L, 0x1c, 0);
#ifndef RX_ORACLE
            {   /* the registered 0053c740 (build_vehicle_pkgcache.c: dlc_path (the directory buffer, the providers, the config, &out)) */
                uint32_t (*f)(rx_addr, rx_addr, rx_addr, rx_addr) = RX_FN(0x0053c740, uint32_t (*)(rx_addr, rx_addr, rx_addr, rx_addr));
                if (!f) act_unreached(0x0053c740, "the downloadable content lookup");
                ACT_CALL(3, (void)f(dir, g + 0x15554, g + 0x39c0, L + 0x1c));
            }
#else
            orig_regs(0x0053c740, 0, 0, 0, 0, dir, 3, (const uint32_t[]){g + 0x15554, g + 0x39c0, L + 0x1c});
#endif
            SU32(g, 0x5a64, U32(L, 0x1c));
            crt_strncpy(g + 0x3ac4, dir, 0x103);
            SU8(g, 0x3ac4 + 0x103, 0);
            const rx_addr db = U32(core, 8);
            act_vcall(db, 0x14, 4, (const uint32_t[]){g + 0x3ac4, g + 0x39c0, 0, U32(g, 0x5a64)});   /* vf5: the level database (async) */
        }
        ACT_CALL(1, rx_level_config_init(g + 0x39c0));
        ACT_CALL(1, rx_level_load_request(g + 0x39c0, 1));
        {
            const rx_addr ls = U32(0, 0x00d92f10);   /* the save-game / records object: Fn_00753200 (EAX = it) then, for the single player, Fn_00753250 (EAX still it) */
            ACT_CALL(0, rx_records_slots_start(ls));
            if (pm == 0) { ACT_CALL(0, rx_records_slots_attempt(ls)); SU32(ls, 0x67a8, 0); }
            else SU32(ls, 0x67a8, (pm == 2 || pm == 3) ? 2 : 0);
            SU32(g, 0x18, 4);
        }
        break;
    }
    case 4: {   /* the level's packages loading */
        const uint32_t pm = player_mode();
        SU32(U32(0, 0x00d92f10), 0x67a8, (pm == 2 || pm == 3) ? 2 : 0);
        load_wait(g, dt);
        uint32_t done;
        ACT_CALLR(done, 0, rx_level_loaded(g + 0x39c0));
        if (!(done & 0xff)) {
            loading_progress(g, load_progress(g + 0x39c0));
            break;
        }
        if (U32(g, 0x15b58) == 0) {
            SU32(g, 0x18, 5);
            SU32(g, 0x15b54, 0);
            /* the loader's job "Subscene Creation" (Database vf14 0084c0f0): the job record {kind 5, progress, FUN_00759f00 at
             * +0x18c, game at +0x190} in the loader's ring and its wake event. The loader thread (0084c760) runs the function;
             * the product's rx_database_loader_step at the end of this frame is that thread's turn (the original's loader runs
             * between frames). The world is used only once Database vf11 (0084c090) reports the queue empty (state 5), so
             * nothing below may build it in this frame. */
            act_vcall(U32(core, 8), 0x38, 4, (const uint32_t[]){0x0096d6c8, 0x00759f00, g, g + 0x5a60});
        } else {
            ACT_CALL(0, rx_level_release(g + 0x39c0));
            act_vcall(U32(core, 8), 0x18, 2, (const uint32_t[]){g + 0x39c0, U32(U32(core, 0x10), 0x58)});
            SU32(g, 0x15b58, 0);
            SU32(g, 0x18, (U8(g, 0x734c) != 0) + 2);
            loading_progress(g, F32(0, 0x009cd9e8));
        }
        break;
    }
    case 5:   /* the world's construction */
        load_wait(g, dt);
        if (U32(g, 0x15b54) != 0 && act_vcall(U32(core, 8), 0x2c, 0, 0) != 0) {   /* Database vf11: the loader idle */
            if (U32(g, 0x15b58) == 0) {
                SU32(g, 0x18, 6);
                loading_progress(g, 1.0f);
                /* the load is done: the session pause clears (MXGame::Update's 006e0c50 block resume branch: [0xda6734] = 0
                 * and the engine timing pause word = 0, the worlds unpaused; the oracle's state-6 grid pass has pause 0). The
                 * product has no input device table / renderer for 006e0c50 to read, so the transition its block makes is made
                 * here, at the same place (the load's end, before the grid). */
                SU32(0, 0x00da6734, 0);
                ACT_CALL(0, rx_mxworld_set_paused(U32(g, 0x20c8), 0));
            }
            else abort_to(g, 1);
        } else if (U32(g, 0x15b58) == 0) {
            loading_progress(g, load_progress(g + 0x39c0));
        }
        break;
    case 6: {   /* the players loaded -> the race's grid and start */
        const uint32_t pm = player_mode();
        SU32(U32(0, 0x00d92f10), 0x67a8, (pm == 2 || pm == 3) ? 3 : 1);
        load_wait(g, dt);
        if (U32(g, 0x15b58) != 0) { abort_to(g, 1); break; }
#ifndef RX_ORACLE
        if (rx_session_is_networked(U32(rx_fx_session_manager(), 0x10)) != 0)
#else
        if (ONLINE_BOUNDARY_REGS(0x00402540, U32(rx_fx_session_manager(), 0x10), 0, 0, 0, 0, 0, 0) != 0)
#endif
            act_unreached(0x00758da9, "the online players' load state");
        if (act_call_fn(0x0075f2b0, RX_STDCALL, 0, 1, (const uint32_t[]){g}) != 0) {   /* every player loaded */
            uint32_t type = 0x3e, mode = 9;
            {   /* the local player's activity (its Player List row: ActivityType), its mode from the activity table */
                const rx_addr t = ACT_PLAYER_LIST;
                for (uint32_t r = 0; r < rx_tbl_row_count(t); r++)
                    if (act_cell(t, 0x0093de90, r) == 1) { type = act_cell(t, 0x0093dd90, r); break; }
                for (uint32_t r = 0; r < rx_tbl_row_count(ACT_ACTIVITY_TBL); r++)
                    if (act_cell(ACT_ACTIVITY_TBL, 0x0093dd90, r) == type) mode = act_cell(ACT_ACTIVITY_TBL, 0x00936e30, r);
            }
            act_call_fn(0x0075f890, RX_STDCALL, 0, 4, (const uint32_t[]){g, type, mode, 1});
            session_static();
            if (U32(U32(0, 0x00da6cb0), 0x1c4c) != 0) act_unreached(0x00759197, "the online race start");
            SU32(g, 0x18, 7);
            for (uint32_t i = 0; i < U8(g, 0x1541c); i++) {
                rx_addr P;
                ACT_CALLR(P, 2, rx_player_active(g + 0x7460, i, 3));
                const rx_addr v = U32(P, 0x1ac);
                if (!v) continue;
                SU32(P, 0x214, 0);
                const rx_addr pp = U32(v, 0x190);
                if (pp && U32(pp, 4) != 0 && U32(pp, 0x10) == 0) {   /* the local human's controller rumble (input) */
                    const rx_addr in = act_vcall(core, 8, 0, 0);
                    act_vcall(in, 0xc0, 2, (const uint32_t[]){1, U32(pp, 0x1f0)});
                }
            }
            act_call_fn(0x0075e020, RX_CDECL, 0, 0, 0);   /* the race's start */
        }
        if (U32(g, 0x15b44) != 0) act_unreached(0x0075958b, "NetworkNoPlayers");
        break;
    }
    case 7:   /* the UI's loading page */
        if (U32(g, 0x15b58) != 0) { abort_to(g, 0); break; }
        ext_regs(0x005a4160, U32(g, 0x745c), 0, 0, 0, 0, 0, 0);
        SU32(g, 0x18, 8);
        break;
    case 8:   /* the UI ready */
#ifdef RX_ORACLE
        if (U32(U32(g, 0x745c), 0x2f4) != 0) SU32(g, 0x18, 0xb);
#else
        SU32(g, 0x18, 0xb);
#endif
        break;
    case 9:   /* the activity area re-created (Game_StartAAACreation 0075a380): waiting for the job "AAA Creation" (+0x15b04) */
        if (U32(g, 0x15b04) == 0) break;
        ACT_CALL(0, rx_engine_frame_mark(U32(core, 0x10)));                         /* 0084ea00 (EDI = the engine's frame object) */
        ACT_CALL(1, rx_scene_set_active(1, U32(core, 0x20)));                       /* 007d20b0 (EAX = 1; the UI scene) */
        if (U32(g, 0x15b58) != 0) {
            ACT_CALL(1, rx_world_activities_restart(U32(g, 0x5a54)));
            act_unreached(0x00758579, "the event quit while its activity area is re-created (MXGame_SwitchToUIWorld)");
            break;
        }
        if (U32(g, 0x15b08) != 0x3e)   /* a race type to restart (the series' re-creation passes none) */
            act_call_fn(0x0075f890, RX_STDCALL, 0, 4, (const uint32_t[]){g, U32(g, 0x15b08), U32(g, 0x15b0c), 1});
        ACT_CALL(1, rx_world_activities_restart(U32(g, 0x5a54)));                  /* 007665b0 */
        SU32(g, 0x18, 10);
        ACT_CALL(1, (void)rx_players_sync(g));                                      /* 0075ba20 */
        ACT_CALL(1, rx_world_players_spawn(U32(g, 0x5a54)));                       /* 00766e70 */
        break;
    case 10: {   /* the re-created area loading: the players' vehicles and riders, then the series' next race */
        if (U32(g, 0x15b58) != 0) {
            ACT_CALL(0, rx_engine_frame_mark(U32(core, 0x10)));
            act_unreached(0x00758579, "the event quit while its activity area loads (MXGame_SwitchToUIWorld)");
            break;
        }
        if (act_vcall(U32(core, 8), 0x2c, 0, 0) == 0) break;                       /* Database vf11: the loader still busy */
        if (U32(g, 0x15b08) == 8) {
            ACT_CALL(1, (void)rx_game_resume_from_ui(g, 1));                       /* MXGame_ResumeFromUI 0075c8c0 */
            SU32(g, 0x18, 2);
            ACT_CALL(0, rx_game_race_start());                                      /* 0075e020 */
            break;
        }
        ACT_CALL(1, (void)rx_game_resume_from_ui(g, 0));
        if (U32(rx_fx_series_manager(), 0x190) != 2 && U32(rx_fx_series_manager(), 0x190) != 1) {   /* 00413d20: not a series */
            SU32(g, 0x18, 2);
            ACT_CALL(0, rx_game_race_start());
            break;
        }
        if (U32(0, 0x00ae0e70) != U32(g, 0x15b30)) break;
        SU32(L, 0x10, 1);                                                           /* every local player has its vehicle and rider */
        for (uint32_t i = 0; i < U8(g, 0x1541c); i++) {
            rx_addr P;
            ACT_CALLR(P, 2, rx_player_active(g + 0x7460, i, 3));                  /* 00552b90 */
            const rx_addr vehicle = U32(P, 0x1ac);
            if (vehicle) {
                SU8(U32(vehicle, 0x1328), 0x14e, 0);
                act_vcall(vehicle, 0xc4, 1, (const uint32_t[]){0});
                ACT_CALL(0, rx_vehicle_body_link_unset(U32(P, 0x1ac)));          /* 0045d4f0 (EAX = the vehicle) */
            } else {
                SU32(L, 0x10, 0);
            }
            if (U32(P, 0x1b4)) ACT_CALL(0, rx_pad_net_release(U32(P, 0x1b4)));  /* 0061c690 (EAX = the rider) */
            else SU32(L, 0x10, 0);
        }
        if (U32(L, 0x10) == 0) {
            ACT_CALL(0, rx_game_setup_activity_area(g));                            /* Game_SetupActivityArea 0075c430 (ESI = game) */
            ACT_CALL(0, rx_game_race_start());
            break;
        }
        SU32(g, 0x18, 2);
        SU32(L, 0x48, 0);
        uint32_t found;
        ACT_CALLR(found, 2, rx_ui_session_get_int(0x0094d994 /* "NeedSeriesAdvanceEvent" */, L + 0x48));   /* UIProperty_GetInt */
        if (found && U32(L, 0x48) != 0) {
#ifdef RX_ORACLE
            /* the UI asked for the series' next race itself: 005554b0 raises "MXSeriesManager::AdvanceEvent" on the UI manager's
             * channel, 005dc230 erases the property */
            rx_regs in, out;
            memset(&in, 0, sizeof in);
            in.r[7] = 0x0094d6fc;                                                   /* EDI = "MXSeriesManager::AdvanceEvent" */
            const uint32_t channel = U32(U32(0, 0x00a93fcc), 0x98);
            rx_call_original_ex(0x005554b0, &in, &out, 1, &channel);
            const uint32_t key = 0x0094d994;
            rx_call_original(0x005dc230, RX_THISCALL, U32(0, 0x00a93fcc), 1, &key);
#else
            act_unreached(0x00758722, "NeedSeriesAdvanceEvent (a UI page asked for the series' next race)");
#endif
            ACT_CALL(0, rx_game_race_start());
            break;
        }
        act_vcall(U32(rx_fx_series_manager(), 0x18c), 0x24, 0, 0);                 /* the series event's vf9: the gate selection */
        ACT_CALL(0, rx_game_race_start());
        break;
    }
    case 0xb: {   /* the race world entered */
        if (U32(g, 0x15b58) != 0) { abort_to(g, 0); break; }
        engine_frame_mark();
        const rx_addr hud = ext_regs(0x00755e50, 0, 0, 0, 0, 0, 0, 0);   /* PRODUCT BOUNDARY: HUD (UI) */
        ext_regs(0x007a58e0, 0, 0, 0, hud, 0, 0, 0);   /* (render: the scene's post effects) */
        SU32(g, 0x15b4c, 1);
        ACT_CALL(1, rx_game_enter_real_world(g));                     /* Game_EnterRealWorldAudio */
        SU32(g, 0x18, 2);
        {   /*Original00759698..c8: actual series manager, then005b40e0.*/
#ifdef RX_ORACLE
            rx_addr sm=orig_regs(0x00413d20,0,0,0,0,0,0,0);
            if((U32(sm,0x190)==2 ||
                U32(orig_regs(0x00413d20,0,0,0,0,0,0,0),0x190)==1) &&
                U32((sm=orig_regs(0x00413d20,0,0,0,0,0,0,0)),0x18c)!=0)
                (void)orig_regs(0x005b40e0,0,0,0,sm,0,0,0);
#else
            rx_addr sm=rx_fx_series_manager();
            if((U32(sm,0x190)==2 || U32((sm=rx_fx_series_manager()),0x190)==1) &&
                U32((sm=rx_fx_series_manager()),0x18c)!=0)
                ACT_CALL(0,rx_career_composition_begin_race(sm));
#endif
        }
        SU32(g, 0x15b4c, 0);
        break;
    }
    default: break;
    }
    /* ---- the frame ---- */
    SU32(0, 0x00d611c8, U32(U32(core, 0x10), 0x40));
    act_call_fn(0x0081f290, RX_CDECL, 0, 0, 0);                                       /* RumbleFrame 0081f290 (loop unit), the port when registered */
    ACT_CALL(0, rx_players_deferred(g + 0x7460));                                     /* 00552930 */
    session_static();
    session_tick(dt_of(L), 0x00da6ca0);                                               /* 006de690 (EDI = the session) */
    session_static();
    {
        const rx_addr s = U32(0, 0x00da6cb0);
        const int online = U32(s, 0x1c4c) != 0 && U32(s, 0x1b0) != 1;
        if (online || U32(s, 0x1c3c) != 0) {
            rx_addr w = 0;
            if (U32(U32(core, 0x20), 0x34) == 1) w = U32(g, 0x5a54);
            else if (U32(U32(core, 0x1c), 0x34) == 1) w = U32(g, 0x20c8);
            if (w) for (int32_t i = 0; i < I32(w, 0x21da8); i++) {
                const rx_addr a = U32(U32(w, 0x21da4), 4 * (uint32_t)i);
                if (U32(a, 0x23c) == 8 && U32(a, 0x220) == 0) { ONLINE_BOUNDARY_CALL_FN(0x0075cfb0, RX_STDCALL, 0, 1, (const uint32_t[]){g}); break; }
            }
        }
    }
    if (U8(g, 0x15b20) == 0) {   /* the current world's frame (MXWorld::vf5) */
        rx_addr w = 0;
        if (U32(U32(core, 0x20), 0x34) == 1) w = U32(g, 0x5a54);
        else if (U32(U32(core, 0x1c), 0x34) == 1) w = U32(g, 0x20c8);
        if (w) {
            act_vcall_f(w, 0x14, dt_of(L));
            if (ext_regs(0x00760330, g, 0, 0, 0, 0, 0, 0) != 0 && U32(U32(g, 0x745c), 0x288) == 0) {   /* PRODUCT BOUNDARY: UI */
                const int32_t r = (int32_t)ext_regs(0x00599080, 0, 0, 0, 0, 0, 0, 0);
                if (r >= 0) ext_regs(0x00759e20, 0, 0, 0, g, 0, 1, (const uint32_t[]){(uint32_t)r});
            }
        }
    }
    SU8(g, 0x15b20, 0);
    if (U8(0, 0x009b716d) == 0 && U32(0, 0x00d5e340) != 0) {   /* the debug camera toggle keys */
        int ok = 1;
        if (U8(0, 0x00d5e32c) & 2) {
            if (!(U8(0, 0x00d92f48) & 1)) act_unreached(0x00759911, "InputNames_Init (input unit)");
            if (U32(0, 0x00d92f80) == 0 && U32(0, 0x00d5e330) != 0x47) ok = 0;
        }
        if (ok && (U8(0, 0x00d5e32c) & 7) && U32(0, 0x00d5e34c) != 0 && U32(0, 0x00d5e348) != 0) {
            SU32(g, 0x10, U32(g, 0x10) == 0);
            act_vcall(U32(core, 0xc), 0x80, 3, (const uint32_t[]){U32(g, 0x1c), g + 0x10, 1});
        }
    }
    ACT_CALL(0, rx_game_debug_reload(g));                                             /* 00759d60 */
    ACT_CALL(1, rx_package_manager_update(U32(0, 0x00d92ef0), g + 0x154b0, dt_of(L))); /* 00540130 */
    {   /* 0053cc90 (ESI = the loader game + 0x15504, EAX = [0x00d92ef4]; dt) */
        ACT_CALL(1, rx_loader_update(g + 0x15504, U32(0, 0x00d92ef4), dt_of(L)));
    }
    for (uint32_t k = 0; k < 4; k++) {   /* the four updaters (+0x1555c) */
        const rx_addr u = U32(g, 0x1555c + 4 * k);
        if (u) act_vcall_f(u, 8, dt_of(L));
    }
    if (U32(U32(g, 0x745c), 0x284) != 0) UI_BOUNDARY_CALL_F2(0x007616d0, RX_STDCALL, g + 0x15570, dt_of(L));
    ACT_CALL(2, rx_records_update(U32(0, 0x00d92f10), dt_of(L)));                    /* 007532e0 */
    {   /* MXHighlightTracking's Steam stats (not core) */
        const rx_addr ht = U32(U32(0, 0x00d92edc), 4);
        if (ht && U32(ht, 0) != 0) {
#ifdef RX_ORACLE
            const rx_addr st = rx_call_original(U32(0, 0x009166a4), RX_CDECL, 0, 0, 0);
            rx_call_original(U32(U32(st, 0), 0x28), RX_THISCALL, st, 0, 0);
#endif
            SU32(ht, 0, 0);
        }
    }
    SF32(g, 0x15b1c, F32(g, 0x15b1c) + dt_of(L));
    /* music / audio (not core) */
    ext_regs(0x0067f210, 0x009ca584, 0, 0, 0, g + 0x15c6c, 1, (const uint32_t[]){fbits(dt_of(L))});
    ext_regs(0x00676ae0, 0, 0, 0, 0, g + 0x15c6c, 1, (const uint32_t[]){fbits(dt_of(L))});   /* (EDI = the music player) */
    ext_regs(0x00676d20, 0, 0, 0, 0, g + 0x15c6c, 1, (const uint32_t[]){fbits(dt_of(L))});
    ext_regs(0x00676f30, 0, 0, 0, 0, g + 0x15c6c, 0, 0);
#ifndef RX_ORACLE
    rx_music_tick(dt_of(L));   /* w-sfx: the music player's delay / track state (game/music_player.c) */
#endif
#ifndef RX_ORACLE
    /* PRODUCT BOUNDARY: the Lua VM and its per-frame job executor (00779440 / 00744180) are not ported (lead decision:
     * Lua is UI/front-end state; no first-ride gameplay path reads it). The startup's rx_lua_memory_init still built the
     * memory pool; only the VM jobs are skipped. */
#else
    if (!(U8(0, 0x00d92f44) & 1)) act_unreached(0x00759a8e, "the Lua state's static initializer (00744180)");
    {   /* the Lua jobs (00779440 under 0x00d94940; the Lua VM is not ported: lead's decision) */
        const rx_addr lq = U32(0, 0x00d94938);
        orig_regs(0x00779440, 0x19, 5, 0, U32(lq, 0x10), 0, 2, (const uint32_t[]){lq, 0});
    }
#endif
    {   /* the localized packages (0041b410) and the package cache (PackageCache_Update 00543e50: EAX = cache + 4) */
        const rx_addr pc = U32(0, 0x00d72a30);   /* the localized database manager: FUN_00598640, not core (UI / text) */
        if (pc) {   /* the product leaves it null with the front end: no manager, no package cache to update */
            ext_regs(0x0041b410, 0, 0, 0, pc, 0, 1, (const uint32_t[]){pc});   /* PRODUCT BOUNDARY: localized packages (UI/text) */
            ACT_CALL(1, rx_pkgcache_update(pc + 4, dt_of(L)));
        }
    }
    SU32(0, 0x009cb274, 1);
    {   /* the engine's per-frame object list (+0xc0 count < 10: the game's frame object 0x009cb268) */
        const rx_addr e = U32(core, 0x10);
        if (U32(e, 0xc0) < 10) { SU32(e, 0x98 + 4 * U32(e, 0xc0), 0x009cb268); SU32(e, 0xc0, U32(e, 0xc0) + 1); }
    }
#ifdef RX_ORACLE
    {   /* the oracle runs the original: the fx ports' register residue (saved EDI/EBP/...) is what the particle templates copy;
         * the core call is verified as its own port in rx_fx */
        const uint32_t a = rx_fb(dt_of(L));
        mxgame_regs(L_sp, 1, U32(0, 0x00d72a30), g + 0x15c6c);   /* 00759af4: EBX = 1; ESI = the localized database manager; EDI = the music player */
        rx_call_original(0x00691bd0, RX_CDECL, 0, 1, &a);
    }
#else
    mxgame_regs(L_sp, 1, U32(0, 0x00d72a30), g + 0x15c6c);
    rx_jobpool_defer_wakes();   /* w-lcgdraws: its jobs run where the original's worker does, at Engine::Update's JobPool_WaitIdle */
    ACT_CALL(1, rx_fx_vehicle_effects_update_all(dt_of(L)));                          /* 00691bd0 */
#endif
    /* the vehicle damage tick (w-damagetick; product and oracle alike): the VehicleDamageBehaviorManager's entries (registered by
     * 0047bb90 for the four-wheel types 3 / 5 / 7 only; a bike has none) get their parts' spring 0047aff0 / fall 0047b960 and the
     * hit timer (00479970). Gameplay reads it: the swinging parts' nodes of the model hierarchy (+0x1328) feed the render copy's
     * subtree bounds 004302f0 (the AI's BehaviorSeperate target pick), and a released part's body is enabled (+0xb0, the kinematic
     * record read by 0041cea0 and the push kernels). */
    ACT_CALL(0, rx_world_damage_manager_instance());   /* 0047bb40: the game's own function-local static (guard 0xdb45f8) */
    ACT_CALL(2, rx_effect_list_update(0x00db1410, dt_of(L)));                         /* 0047bc60 */
    {
        float c = F32(0, 0x00a89cf8) + dt_of(L);
        SF32(0, 0x00a89cf8, c);
        if (c > 1.0f) SF32(0, 0x00a89cf8, c - 1.0f);
    }
    ACT_CALL(1, rx_pose_overrides_update(dt_of(L)));                                  /* 0047c2a0 */
    {   /* the mud records every 0.1 s */
        float c = F32(0, 0x00a89bc4) + dt_of(L);
        SF32(0, 0x00a89bc4, c);
        if (c > F32(0, 0x00931d94)) {
            for (rx_addr r = 0x00a96d90; r < 0x00a97090; r += 0x40) {   /* 00759bde: ESI = the record, EBX = 1, EDI = the music player */
                mxgame_regs(L_sp, 1, r, g + 0x15c6c);
                if (U32(r, 0x30) != 0) ACT_CALL(0, rx_fx_mud_update(r));
            }
            mxgame_regs(L_sp, 1, 0x00a97090, g + 0x15c6c);   /* 00759bf1: the loop's end value of ESI */
            for (uint32_t k = RX_RC_ORIG; k < rx_mud_n(); k++)   /* (w-rc-core) the side records taken (rider_cap.h) */
                if (U32(rx_mud_rec(k), 0x30) != 0) ACT_CALL(0, rx_fx_mud_update(rx_mud_rec(k)));
            SF32(0, 0x00a89bc4, F32(0, 0x00a89bc4) - F32(0, 0x00931d94));
        }
    }
    if (U32(U32(core, 0x10), 0xc) == 0) {   /* the clip recorder */
        const uint32_t st = U32(0, 0x00a942b4);
        if (st == 1) ACT_CALL(1, rx_clip_recorder_record(dt_of(L)));
        else if (st == 2 && I32(0, 0x009c4070) >= 0) {
            rx_addr clip=0x00d323c0+U32(0,0x009c4070)*0x35d0;
            ACT_CALL(1,rx_replay_playback_tick(clip,rx_f80_to_f32(rx_f80_from_f32(dt_of(L)),0)));
            ACT_CALL(0,rx_replay_fade_out());
            clip=0x00d323c0+U32(0,0x009c4070)*0x35d0;
            if(rx_adds(F32(clip,0x35bc),F32(clip,0x35b0))>F32(clip,0x35b4))
            {
                ACT_CALL(0,rx_replay_next_clip());
                ACT_END(L);
                return 1;
            }
        }
    }
    ACT_END(L);
#ifndef RX_ORACLE
    /* the Database's loader thread (0084c760) runs between frames: its queue turn here, at the frame's end (after MXGame::Update,
     * before the next frame's state machine asks Database vf11 whether the queue is empty). The queue holds the state-4 job
     * "Subscene Creation" (Database vf14 0084c0f0 -> FUN_00759f00); the product's other loader requests are served in their
     * calling functions. */
    rx_database_loader_step(U32(U32(0, 0x00a95844), 8));
#endif
    return 1;
}
RX_FUNCTION(0x00757fb0, rx_mxgame_update)

/* ======================================================================================================================== */
/* the race world entered (state 0xb)                                                                                         */

/* 007668e0 (stdcall world; RET 4): every activity of the world started (vf2 ("PostEventOverlay")); a mode-2 activity marked
 * +0x10 = 2 */
void rx_world_activities_start(rx_addr w) {
    ACT_FRAME(L, 0, 4);
    const int32_t n = I32(w, 0x21da8);
    for (int32_t i = 0; i < n; i++) {
        const rx_addr a = U32(U32(w, 0x21da4), 4 * (uint32_t)i);
        act_vcall(a, 8, 1, (const uint32_t[]){0x0096dd14 /* "PostEventOverlay" */});
        if (U32(a, 0x238) == 2) SU32(a, 0x10, 2);
    }
    ACT_END(L);
}
RX_FUNCTION(0x007668e0, rx_world_activities_start)

/* Game_EnterRealWorldAudio 0075a3f0 (stdcall game; RET 4): the switch from the front end's world to the event's: the reverb off
 * and the sound groups (audio), the scenes' render-sync objects reset (00757dc0: render), "InUIWorld" (0) on the tutorial channel
 * [0x00a946a8], the UI's InRealWorld / EventTypeActivation (UI), game + 0x7454 = 1, the scenes' active flags (007d20b0: the event
 * scene on, the UI's off), the engine's scene index (+4) 1, the UI world's activities ended and its players left (00766930), the
 * overlay / HDR (render), each player's vehicle moved to the event world (vf31) and its damage parts' bodies after it (0047bb40
 * / 0047bc20), the world's activities started (007668e0), the event scene's render / update flags (+0x2cb0, +0x2cb8, the
 * render-sync objects' +4), the engine frame mark, the 2D particle managers cleared (fx), the event scene's ambient zone flagged,
 * the session's online bookkeeping (network), "AutoGenerate" (UI), the activity area (0075c430), Database vf36, the ambient
 * reverb (audio), the vehicle sounds (audio) */
void rx_game_enter_real_world(rx_addr g) {
#ifdef RX_ORACLE
    /* mostly UI / audio / render / network boundaries (the UI's by-value argument block of "AutoGenerate" included): the oracle runs
     * the original; its core callees (007668e0, 0075c430, the activities' vf2 ...) are verified as their own ports */
    rx_call_original(0x0075a3f0, RX_STDCALL, 0, 1, &g);
    return;
#endif
    ACT_FRAME_EBP(L, 8, 0x5c, 3);
    const rx_addr core = U32(0, 0x00a95844);
    ext_regs(0x00805780, 0, 0, 0, 0, 0, 3, (const uint32_t[]){0x14, 0x009b7438, 0});   /* Audio_PostCommandWait (the reverb off) */
    ext_regs(0x00805120, 0, 0, 0, 0, 0, 2, (const uint32_t[]){1, 1});
    ext_regs(0x00757dc0, 0, 0, 0, 0, 0, 0, 0);
    if (U32(0, 0x00a946a8) != 0) {
        const uint32_t payload[1] = {0};
        race_raise_event(U32(0, 0x00a946a8), 0x0096d074 /* "InUIWorld" */, 0x009d24d0, payload, 1);
    }
    ext_regs(0x005dbfb0, 0, U32(0, 0x00a93fcc), 0, 0, 0, 2, (const uint32_t[]){0x0094c9bc /* "InRealWorld" */, 1});
    ext_regs(0x005dbf50, 0, 0, 0, U32(0, 0x00a93fcc), 0x0094c320 /* "EventTypeActivation" */, 1, (const uint32_t[]){0x00935ddc});
    SU32(g, 0x7454, 1);
    ACT_CALL(1, rx_scene_set_active(1, U32(core, 0x20)));   /* FUN_007d20b0 (EAX = active; stdcall scene) */
    ACT_CALL(1, rx_scene_set_active(0, U32(core, 0x1c)));
    SU32(core, 4, 1);
    ACT_CALL(1, rx_world_players_leave(U32(g, 0x20c8)));                  /* 00766930 */
    ext_regs(0x0075aa40, 0, 0, 0, 0, U32(g, 0x20c8), 0, 0);
    for (uint32_t i = 0; i < U8(g, 0x1541c); i++) {
        rx_addr P;
        ACT_CALLR(P, 2, rx_player_active(g + 0x7460, i, 3));
        if (!P) continue;
        const rx_addr v = U32(P, 0x1ac), w = U32(g, 0x5a54);
        if (!v || U32(v, 0x188) == w) continue;
        SF32(P, 0x10bc, 0.0f);
        act_vcall(v, 0x7c, 1, (const uint32_t[]){w});                  /* the vehicle into the event world */
        rx_addr m;
        ACT_CALLR(m, 0, rx_world_damage_manager_instance());           /* 0047bb40 */
        ACT_CALL(1, rx_damage_move_vehicle_parts(m, U32(P, 0x1ac)));    /* 0047bc20: the part bodies follow the vehicle */
    }
    ext_regs(0x0075a870, 0, 0, 0, 0, 0, 1, (const uint32_t[]){U32(g, 0x5a54)});
    ACT_CALL(1, rx_world_activities_start(U32(g, 0x5a54)));
    {
        const rx_addr sc = U32(core, 0x20);
        SU32(sc, 0x2cb0, 1); SU32(sc, 0x2cb8, 1);
        for (uint32_t k = 0; k < U32(U32(core, 0x20), 0x19d0); k++) {
            const rx_addr o = U32(U32(U32(core, 0x20), 0x19b8 + 4 * k), 0x60);
            if (o) SU32(o, 4, 1);
        }
    }
    engine_frame_mark();
    ext_regs(0x0069a0c0, 0, 0, 0, 0, 0x00c11bd0, 0, 0);   /* ParticleManager2D_Clear (EDI = the manager; fx, render) */
    ext_regs(0x0069a0c0, 0, 0, 0, 0, 0x00c11d80, 0, 0);
    {   /* the ambient zone of the event scene (0x009e30d4 list, count [0x009e30dc]: +0x44a8 the scene, +0x44b8 = 1) */
        const rx_addr w = U32(g, 0x5a54);
        if (w && U32(w, 0x20138)) {
            if (!(U8(0, 0x009e30e0) & 1)) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
            for (uint32_t k = 0; k < U32(0, 0x009e30dc); k++) {
                const rx_addr z = U32(0, 0x009e30d4 + 4 * k);
                if (z && U32(w, 0x20138) == U32(z, 0x44a8)) { SU32(z, 0x44b8, 1); break; }
            }
        }
    }
    session_static();
    if (U32(U32(0, 0x00da6cb0), 0x1c4c) != 0 && U32(U32(0, 0x00da6cb0), 0x1b0) != 1) act_unreached(0x0075a674, "the online world switch");
    /* "AutoGenerate" (UI: 005c3590 / 005dc290 on a by-value argument block): not core */
    ACT_CALL(0, rx_game_setup_activity_area(U32(0, 0x00a946b8)));
    act_vcall(U32(core, 8), 0x90, 0, 0);                               /* Database vf36 */
    {
        rx_addr p = 0x009b7438;
        const rx_addr z = ext_regs(U32(U32(U32(0, 0x00a95830), 0), 8), 0, U32(0, 0x00a95830), 0, 0x009b7438, 0, 0, 0);
        if (z) p = z + 0x98;
        if (!z || p) ext_regs(0x00805780, 0, 0, 0, 0, 0, 3, (const uint32_t[]){0x14, p, 0});
    }
    for (uint32_t k = 1; k <= 0xc; k++) {   /* the vehicle sounds (audio) */
        const rx_addr s = U32(U32(0, 0x00d92f00), 0x1b10 * k);
        if (s) ext_regs(0x006826d0, 0, 0, 0, 0, 0, 1, (const uint32_t[]){s});
    }
    ACT_END(L);
}
RX_FUNCTION(0x0075a3f0, rx_game_enter_real_world)
