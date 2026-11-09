/* race_misc_game.c -- MXGame's frame helpers (MXGame at 0xa946b8). */
#include "crt_atexit.h"
#include "vehicle_audio.h"
#include "race.h"
#include "race_extern.h"
#include "race_misc.h"
#include "build_ui_props.h"
#include "crt.h"
#include "race_driver.h"
#include "build_series_init.h"
#include "fx.h"
#include "world_node.h"
#include "build_activity.h"
#include "phys_coll_shape.h"
#include "race_activity_misc.h"
#include "race_activity_ui.h"
#include "world_wreck.h"
#include "build_alloc.h"
#include "../src/rx_os.h"

/* 00755d30 IntHash_Wang32 (ECX = key) -> EAX: Thomas Wang's 32-bit integer hash. */
uint32_t rx_int_hash_wang32(uint32_t k) {
    k = k + ~(k << 15);
    k = k ^ (k >> 10);
    k = k * 9;
    k = k ^ (k >> 6);
    k = k + ~(k << 11);
    return k ^ (k >> 16);
}
RX_FUNCTION(0x00755d30, rx_int_hash_wang32)

/* 00755d60 (arg = name; cdecl) -> EAX: the index of a game mode name in the table 0x9dd584 (5 names, case-insensitive), or 5. */
int rx_game_mode_index(rx_addr name) {
    for (int i = 0; i < 5; i++)
        if (crt_stricmp(name, U32(0, 0x9dd584 + 4u * (uint32_t)i)) == 0) return i;
    return 5;
}
RX_FUNCTION(0x00755d60, rx_game_mode_index)

/* 008a7310 -> EAX: stat 4's value (+0x24) in the stats table 0x9df584 (FUN_00402420; the shared vf 007a0760 naming it returns 0),
 * 0 if none. */
uint32_t rx_stat4_value(void) {
    uint32_t st;
    RACE_CORE_CALL(2, st = rx_fx_thread_table_get(0x009df584, 4, 0));
    return st == 0 ? 0 : U32(st, 0x24);
}
RX_FUNCTION(0x008a7310, rx_stat4_value)

/* 0075c5f0 (ECX = game) -> EAX: is the active view set's track (+0x5a54 for view set 0x20, +0x20c8 for 0x1c, else none) the
 * game's track +0x5a54? */
uint32_t rx_game_is_view_track(rx_addr game) {
    rx_addr ctx = U32(0, 0xa95844);
    if (U32(U32(ctx, 0x20), 0x34) == 1) return 1;
    if (U32(U32(ctx, 0x1c), 0x34) == 1) return U32(game, 0x20c8) == U32(game, 0x5a54);
    return U32(game, 0x5a54) == 0;
}
RX_FUNCTION(0x0075c5f0, rx_game_is_view_track)

/* 00757f90 MXGameTickDuringSync::vf0 (ECX = tick): while active (+0xc), the game's tick with the stored time (+8; 0067dd10). */
void rx_game_tick_during_sync(rx_addr self) {
    if (U32(self, 0xc) == 0) return;
    uint32_t t = U32(self, 8);
    /* AUDIO BOUNDARY (plan/QUEUE.md "chain round 3", "w-race"): the tick 0067dd10 (cdecl dt) updates only the sound managers (0067b670
     * the collision sound map [0xd94ad0], VehicleAudioManager_UpdateVehicles [0xd92f00], 0068d2c0 the camera sound effects / audio
     * event queue [0xd92f04], 0068de90 the UI localized sounds [0xd94ad4]) and the sound engine's pause state: when the session's
     * "paused" condition (the session block 0xda6cac +0x58 / +0x5c both 0, the input manager's device flag +0x34) differs from
     * [0xd94acc] it stores the new value there and calls 00804f00 (FMOD pause). It reads game state (vehicles' speeds, collision
     * events, the pause conditions) and writes only the sound managers' own objects and 0xd94acc, which no gameplay code reads
     * (0xd94acc is referenced by this function alone: the whole-decompile search finds no other reader). The product has no sound engine: it does not run the tick; the oracle
     * runs the original. */
#ifdef RX_ORACLE
    rx_call_original(0x0067dd10, RX_CDECL, 0, 1, &t);
#else
    /* w-rng: the engine emitters' start line draws the game's LCG inside this tick (00680ec0 -> 00685610 -> 00686ec0 / 006869a0 ->
     * 00686870 / 00686e20): the core runs that part here (game/engine_startline.c via vehicle_audio.c), the rest stays the host's */
    rx_vehicle_audio_tick(rx_bf(t));
#endif
}
RX_FUNCTION(0x00757f90, rx_game_tick_during_sync)

/* 00892090 (ECX = a, arg = b; RET 4) -> AL: the two handles' first words equal. */
uint32_t rx_handle_equal(rx_addr a, rx_addr b) { return U32(a, 0) == U32(b, 0); }
RX_FUNCTION(0x00892090, rx_handle_equal)

/* 0075ec80 MXGame_PlayerMode -> EAX: the player mode index of the session's UI property 0x94be84 (UIProperty_GetString EAX = the
 * key, ESI = 30, EDI = a zeroed 30-byte buffer; the session 0xa93fcc) by name (00755d60), or 1 without the property. Frame: sub
 * 0x2c; 2 pushes (the buffer at +0xc). */
uint32_t rx_game_player_mode(void) {
#ifdef RX_ORACLE
    RACE_FRAME_PLAIN(F, 0x2c, 2);
    const rx_addr buf = F + 0xc;
    for (uint32_t i = 0; i < 30; i++) SU8(buf, i, 0);
    rx_regs in, o; memset(&in, 0, sizeof in); in.r[0] = 0x0094be84; in.r[6] = 0x1e; in.r[7] = buf;
    uint32_t a = U32(0, 0xa93fcc);
    rx_call_original_ex(0x005dc060, &in, &o, 1, &a);                             /* UIProperty_GetString */
    uint32_t r = 1;
    if (o.r[0] != 0) { int m; RACE_CORE_CALL(1, m = rx_game_mode_index(buf)); r = (uint32_t)m; }
    RACE_FRAME_END(F);
    return r;
#else
    /* product: the spec's PlayerMode replaces UI property 0x94be84 (the UI itself is not core); the table compare reads game
     * memory, so the host string is copied to the game stack first. Empty: "SinglePlayer". */
    const char* mode = rx_event_spec_get()->player_mode;
    if (mode[0] == 0) return 1;
    RX_FRAME_ENTER(buf, 0x10);
    uint32_t i = 0;
    for (; i < 15 && mode[i]; i++) SU8(buf, i, (uint8_t)mode[i]);
    SU8(buf, i, 0);
    int m;
    RACE_CORE_CALL(1, m = rx_game_mode_index(buf));
    RX_FRAME_LEAVE(buf, 0x10);
    return (uint32_t)m;
#endif
}
RX_FUNCTION(0x0075ec80, rx_game_player_mode)

/* 00759d60 (EDI = the game's config reload handles +0x24 .. +0x30): the engine's vf2, then with the debug keys on (0x9b716d clear)
 * each of four key maps (0xd603e8, 0xd5f818, 0xd5f038, 0xd5cff8: vf1 (7, 0)) reloads its config (the config 0xa95844 +0xc vf22
 * (handle)). */
void rx_game_debug_reload(rx_addr g) {
    RACE_FRAME_PLAIN(F, 0, 1);
    const rx_addr eng = U32(0, 0xa95844);
    RACE_CORE_CALL(0, phys_xenon_core_vf2(eng));   /* 008439f0 XenonCore::vf2: the input-manager getter (registered port) */
    const rx_addr cfg = U32(U32(0, 0xa95844), 0xc);
    static const rx_addr keys[4] = {0x00d603e8, 0x00d5f818, 0x00d5f038, 0x00d5cff8};
    for (int i = 0; i < 4; i++) {
        if (U8(0, 0x9b716d) != 0) break;
        uint32_t pressed;
        RACE_CORE_CALL(2, pressed = rx_world_keymap_latched(keys[i], 7, 0));
        if (pressed != 0) race_thiscall(race_vfn(cfg, 22), cfg, 1, U32(g, 0x24 + 4 * i), 0, 0, 0);
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00759d60, rx_game_debug_reload)

/* ---- MXRecords (the career statistics, 0xa946c0) ------------------------------------------------------------------------------ */

static void series_manager_guard(void) {
    if ((U8(0, 0x9e275c) & 1) == 0) {
        SU32(0, 0x9e275c, U32(0, 0x9e275c) | 1);
        RACE_CORE_CALL(0, (void)rx_series_manager_construct());   /* MXSeriesManager_Construct */
        RX_ATEXIT(0x00911da0);      /* _atexit */
    }
}

/* A play-time counter: + dt (in the original's operand order), its "unsaved" flag cleared; the event counters pin a negative
 * total (overflow) at FLT_MAX. */
static void add_time(rx_addr t, rx_addr flag, float dt, int dt_first, int pin) {
    const float v = dt_first ? rx_adds(dt, F32(t, 0)) : rx_adds(F32(t, 0), dt);
    SF32(t, 0, v);
    SU32(flag, 0, 0);
    if (pin && 0.0f > F32(t, 0)) SF32(t, 0, 3.4028234663852886e+38f /* 0x977494 */);
}

/* 007556d0 (stdcall manager (0xda6ca0), a, b, c; RET 0x10): posts a "NetworkPlayerMotocardData" event to the session manager's
 * queue (+0x40) under the manager's lock (+0x28): an event record { the type id pair of the name (the manager's map +8: StringMap_Find
 * of a std::string built from 0x962944), a 12-byte payload block {a, b, c}, its size 0xc }, 16-aligned allocations; the original keeps the
 * record in its first argument slot while it appends it (FUN_005b9ff0). Frame: sub 0x2c; 4 pushes (the lock at +0x10, the iterator at
 * +0x14, the string at +0x1c). */
void rx_post_motocard_data(rx_addr mgr, uint32_t a, uint32_t b, uint32_t c) {
    const rx_addr args = rx_sp() + 4;
    RACE_FRAME_PLAIN(F, 0x2c, 4);
    const rx_addr lock = mgr + 0x28, it = F + 0x14, str = F + 0x1c;
    SU32(F, 0x10, lock);
    uint32_t cs = lock;
    rx_os_import(0x0091600c, 1, &cs);                              /* EnterCriticalSection */
    rx_addr rec = rx_reflex_aligned_alloc(0x10);                   /* 0078fbe0 */
    if (rec != 0) {
        SU32(rec, 0, 0);
        SU32(rec, 4, 0);
    }
    SU32(args, 0, rec);
    crt_str_ctor_cstr(str, 0x00962944 /* "NetworkPlayerMotocardData" */);
    RACE_CORE_CALL(2, rx_world_string_map_find(mgr + 8, it, str));   /* 00702ad0 (ESI = the map) */
    const uint32_t key0 = U32(it, 0), key1 = U32(it, 4);
    crt_str_dtor(str);
    SU32(rec, 0, key0);
    SU32(rec, 4, key1);
    uint32_t heap = 0x00d61704;
    if (U8(0, 0x00da67d8) != 0) rx_os_import(0x0091600c, 1, &heap);   /* EnterCriticalSection (the allocator lock) */
    rx_addr payload;
    RACE_CORE_CALL(2, payload = crt_aligned_malloc(0xc, 0x10));    /* _aligned_malloc (IAT 0x916358) */
    if (U8(0, 0x00da67d8) != 0) rx_os_import(0x009160f8, 1, &heap);   /* LeaveCriticalSection */
    SU32(rec, 8, payload);
    SU32(rec, 0xc, 0xc);
    SU32(payload, 0, a);
    SU32(payload, 4, b);
    SU32(payload, 8, c);
    RACE_CORE_CALL(1, rx_event_handler_list_push_back(mgr + 0x40, U32(args, 0)));   /* 005b9ff0 (EAX = the record's slot) */
    cs = U32(F, 0x10);
    rx_os_import(0x009160f8, 1, &cs);                              /* LeaveCriticalSection */
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x007556d0, rx_post_motocard_data)

/* 007532e0 (stdcall records, dt; RET 8): the career play-time counters: the total (+0x18); for each of the four local players
 * playing (+0x3c + 4 * k; player blocks of 0x1138 bytes from +0x68, their unsaved flags of 0x89c bytes from +0x453c) the time in
 * the current mode (+0x67a8: 0 .. 3 -> +0x60 / +0x68 / +0x70 / +0x78), and in a career event (+0x1c) the event time (+0x80),
 * online / career time (+0x128; the series modes 0x9e2318 1 / 2 or the UI property "InCareerMode") and the replay time (+0x130,
 * clip playback 0xa942b4 == 2). Then the pending saves: a finished profile save (+0x68dc, state +0xc 2: FUN_004e21d0 and every
 * entry of the list +0x68f8 (FUN_004e2340); FUN_007556d0 (0xda6ca0, +0x6918, +0x691c, ok)) and load (+0x68d8; ok with +0x68e0:
 * every unsaved flag set) are deleted (vf0 (1)); with no load pending a queued request (+0x6914 entries at +0x6900, current
 * +0x6904) runs (FUN_00753c20 (records, slot, the slot's player +0x2c)). The original walks the player flags with its first
 * argument slot (written here too). */
void rx_records_update(rx_addr rec, float dt) {
    const rx_addr args = rx_sp() + 4;
    RACE_FRAME_PLAIN(F, 0xc, 4);
    SF32(rec, 0x18, rx_adds(dt, F32(rec, 0x18)));
    rx_addr flags = rec + 0x453c, t = rec + 0x68;
    SU32(args, 0, rec + 0x3c);
    SU32(F, 0x18, 4);
    for (uint32_t k = 0; k < 4; k++, flags += 0x89c, t += 0x1138, SU32(args, 0, U32(args, 0) + 4)) {
        if (U32(U32(args, 0), 0) == 0) continue;
        switch (U32(rec, 0x67a8)) {
        case 0: add_time(t - 8, flags - 4, dt, 0, 0); break;
        case 1: add_time(t, flags, dt, 1, 0); break;
        case 2: add_time(t + 8, flags + 4, dt, 0, 0); break;
        case 3: add_time(t + 0x10, flags + 8, dt, 0, 0); break;
        default: break;
        }
        if (U32(rec, 0x1c) == 0) continue;
        SU32(F, 0x14, 0);
        /* UIProperty_GetInt("InCareerMode") 005dc0c0: the oracle's UI store; the product's property is absent (build_ui_props.c) */
        RACE_CORE_CALL(2, (void)rx_ui_session_get_int(0x00941f6c, F + 0x14));
        series_manager_guard();
        int career = U32(0, 0x9e2318) == 2;
        if (!career) { series_manager_guard(); career = U32(0, 0x9e2318) == 1 || U32(F, 0x14) != 0; }
        if (career) add_time(t + 0xc0, flags + 0x60, dt, 1, 1);
        add_time(t + 0x18, flags + 0xc, dt, 0, 1);
        if (U32(0, 0xa942b4) == 2) add_time(t + 0xc8, flags + 0x64, dt, 1, 1);
    }
    const rx_addr save = U32(rec, 0x68dc);
    if (save != 0 && U32(save, 0xc) != 1) {
        uint32_t ok = 0;
        if (U32(save, 0xc) == 2) {
            uint32_t a[2] = {U32(rec, 0x6918), U32(rec, 0x691c)};
            RACE_CORE_CALL(2, rx_player_attrs_delete(a[0], a[1]));                 /* 004e21d0 (race_activity_misc.c) */
            for (rx_addr n = U32(U32(rec, 0x68f8), 0); n != U32(rec, 0x68f8); n = U32(n, 0)) {
                uint32_t b[4] = {U32(rec, 0x6918), U32(rec, 0x691c), U32(n, 0xc), U32(n, 0x10)};
                RACE_CORE_CALL(4, (void)rx_player_attr_add(b[0], b[1], b[2], b[3]));   /* 004e2340 */
            }
            ok = 1;
        }
        race_static_dcf304();
        RACE_CORE_CALL(4, rx_post_motocard_data(U32(0, 0xda6ca0), U32(rec, 0x6918), U32(rec, 0x691c), ok));   /* 007556d0 */
        const rx_addr s2 = U32(rec, 0x68dc);
        if (s2 != 0) { race_thiscall(race_vfn(s2, 0), s2, 1, 1, 0, 0, 0); SU32(rec, 0x68dc, 0); }
    }
    const rx_addr load = U32(rec, 0x68d8);
    if (load != 0 && U32(load, 0xc) != 1) {
        if (U32(rec, 0x68e0) != 0 && U32(load, 0xc) == 2) for (uint32_t i = 0; i < 0x89c; i++) SU32(rec, 0x4530 + 4 * i, 1);
        const rx_addr l2 = U32(rec, 0x68d8);
        if (l2 != 0) { race_thiscall(race_vfn(l2, 0), l2, 1, 1, 0, 0, 0); SU32(rec, 0x68d8, 0); }
    }
    if (U32(rec, 0x68d8) == 0 && U32(rec, 0x6914) != 0) {
        const uint32_t slot = U32(rec, 0x6904);
        const rx_addr p = U32(rec, 0x2c + 4 * slot);
#ifdef RX_ORACLE
        /* 00753c20: the online PlayerMetrics upload (Quazal: the session's connected state 3, the metrics objects of the network library); the
         * oracle runs the original. PRODUCT BOUNDARY (network): no online session, the product does nothing (core/plan/QUEUE.md, miscport) */
        if (p != 0) { uint32_t a[3] = {rec, slot, p}; rx_call_original(0x00753c20, RX_STDCALL, 0, 3, a); }
#else
        (void)p;
#endif
        SU32(rec, 0x6904, U32(rec, 0x6900 + 4 * U32(rec, 0x6914)));
        SU32(rec, 0x6914, U32(rec, 0x6914) - 1);
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x007532e0, rx_records_update)
