/* build_ui_world.c -- the front end's UI world: MXUIWorld (vtable 0x0096dcec, 0x22190 bytes, MXGame + 0x20c8) and the part of the game's init thread
 * (FUN_0075ed60) that builds it: the "UIScene" database and level config (MXGame + 0x34: the packages Shared / <level>Base, 00756060 / 007560f0 /
 * 00756250), the UI scene's subscene directory (007562b0) and physics world (0075da90), the world's base subscenes (00766110: the loader job
 * "EnableCoreSubSceneCallback" runs as a plain call), MXUIWorld::vf1 (the world started on the scene, the config, the physics world and the UI
 * activity table 0x00adffe0), the table's FreeRide row (type 8, mode 4: ActivityTable_AddRow 004dedc0), vf3 (the activities: MXWorld::vf3, then the
 * garage: the scene's customize entities by name and the vehicle-type map at +0x22158) and the activities' start (007668e0).
 *   MXUIWorld: +0x22150 the "UICustomizeRiderLocation" entity, +0x22154 the "UIRider_AO_Geo" entity, +0x22158 a std::map<int, {location, AO, scale}>
 *   (proxy +0, head +0x18, size +0x1c; node 0x20: key +0xc, value +0x10 .. +0x18, colour +0x1c, isnil +0x1d), +0x22178 the scene's
 *   "UIAmbientDirectionalLight" entity, +0x2217c 0, +0x22180 the TutorialManager.
 * The TutorialManager (+0x22180, fx_tutorial_manager.c) is built by vf1 and updated by vf5 (w-tutorials; it was a product boundary before). */
#include "build_activity_util.h"
#include "build_activity.h"
#include "build_mxworld.h"
#include "build_world.h"
#include "build_asset.h"
#include "build_rider_util.h"
#include "build_vehicle_tree.h"
#include "build_ui_world.h"
#include "build_activity_int.h"
#include "build_records_init.h"
#include "race_misc.h"
#include "loop_world.h"
#include "anim_extern.h"
#include "fx_tutorial.h"
#include "race_activation.h"
#include "../src/fmt_scene.h"
#include <string.h>

/* 007d25c0 (EAX = scene, ESI = list {+0 data, +4 count, +8 capacity, +0xc growth}; stdcall type; RET 4): under the scene's lock, the entities of the enabled
 * groups whose vf22 (+0x58) is `type`, appended to the list (the count starts at 0; a full list grows by its growth: a new block, the old capacity copied,
 * the old block freed) */
void rx_scene_query_typed(rx_addr s, rx_addr list, uint32_t type) {
    rr_os_cs_enter(s + 4);
    SU32(list, 4, 0);
    for (uint32_t b = 0; b < 0x18; b++) {
        const rx_addr g = s + 0x140 + 0x110 * b;
        if (U32(g, 4) == 0) continue;
        for (int32_t i = 0; i < I32(g, 0); i++) {
            const rx_addr e = U32(U32(g, -4), 4u * (uint32_t)i);
            if (rr_vcall(e, 0x58, 0, NULL) != type) continue;
            if (I32(list, 4) >= I32(list, 8)) {
                const uint32_t cap = U32(list, 0xc) + U32(list, 8);
                const rx_addr nb = rx_reflex_aligned_alloc(rx_mul_sat(cap, 4));
                crt_memcpy(nb, U32(list, 0), U32(list, 8) * 4u);
                rx_reflex_aligned_free(U32(list, 0));
                SU32(list, 8, cap);
                SU32(list, 0, nb);
            }
            SU32(U32(list, 0), 4u * U32(list, 4), U32(U32(g, -4), 4u * (uint32_t)i));
            SU32(list, 4, U32(list, 4) + 1);
        }
    }
    rr_os_cs_leave(s + 4);
}
RX_FUNCTION(0x007d25c0, rx_scene_query_typed)

/* 007d2570 (EAX = scene; stdcall name; RET 4) -> the entity registered under the name (the scene's table +0x19f8), else 0; under the scene's lock */
rx_addr rx_scene_find_by_name(rx_addr scene, rx_addr name) {
    ACT_FRAME(L, 4, 2);
    rr_os_cs_enter(scene + 4);
    SU32(L, 0, 0);
    SU32(L, 8, name);
    const uint32_t found = rx_name_hash_find(scene + 0x19f8, L + 8, L);
    rr_os_cs_leave(scene + 4);
    const rx_addr r = found ? U32(L, 0) : 0;
    ACT_END(L);
    return r;
}
RX_FUNCTION(0x007d2570, rx_scene_find_by_name)

/* ---------------------------------------------------------------------------------------------------------------------------
 * MXUIWorld */

/* 00765d30: the std::map's head node (0x20 bytes: links 0, colour byte +0x1c = 1, isnil +0x1d = 0) */
static rx_addr map_head_node(void) {
    const rx_addr n = rx_reflex_aligned_alloc(0x20);
    if (n) { SU32(n, 0, 0); SU32(n, 4, 0); SU32(n, 8, 0); SU8(n, 0x1c, 1); SU8(n, 0x1d, 0); }
    return n;
}

/* 00765090 (ECX = world, EAX = the level config): MXWorld's constructor (00765e20), the vtable, the garage map {proxy, head node (isnil, self linked), size 0},
 * the entity slots cleared and +0x20018 = 1 (the front end's world) */
rx_addr rx_mxuiworld_construct(rx_addr w, rx_addr cfg) {
    rx_mxworld_construct(w, cfg);
    SU32(w, 0, 0x0096dcec);
    const rx_addr map = w + 0x22158;
    const rx_addr px = rx_reflex_aligned_alloc(4);
    if (px) SU32(px, 0, map);
    SU32(map, 0, px);
    const rx_addr head = map_head_node();
    SU32(map, 0x18, head);
    SU8(head, 0x1d, 1);
    SU32(U32(map, 0x18), 4, U32(map, 0x18));
    SU32(U32(map, 0x18), 0, U32(map, 0x18));
    SU32(U32(map, 0x18), 8, U32(map, 0x18));
    SU32(map, 0x1c, 0);
    SU32(w, 0x22180, 0);
    SU32(w, 0x2217c, 0);
    SU32(w, 0x22150, 0);
    SU32(w, 0x22154, 0);
    SU32(w, 0x22178, 0);
    SU32(w, 0x20018, 1);
    return w;
}
RX_FUNCTION(0x00765090, rx_mxuiworld_construct)

/* MXUIWorld::vf1 007651d0 (thiscall scene, config, physics world, table; RET 0x10): MXWorld::vf1, then the TutorialManager (0xc0 bytes,
 * TutorialManager_Construct 0074fd10: its channel, events and handlers, the lesson reset and ShowLessonObjects(-1), which leaves every "TUT<k>"
 * object hidden and the UI scene's 24 TUT placeable effects inert) at +0x22180 (0 when the allocation fails). w-tutorials: the manager is built
 * in the product too (it was a product boundary until then: core/plan/QUEUE.md "w-tutorials"). */
void rx_mxuiworld_start(rx_addr w, rx_addr scene, rx_addr cfg, rx_addr phys, rx_addr table) {
    rx_set_sp(rx_sp() - 4);                                              /* push edi */
    ACT_CALL(4, rx_mxworld_start(w, scene, cfg, phys, table));
    rx_addr m;
    ACT_CALLR(m, 1, rx_reflex_aligned_alloc(0xc0));
    if (m) {
        rx_set_sp(rx_sp() - 4);                                          /* push esi */
        ACT_CALLR(m, 0, rx_tutorial_construct(m));
        rx_set_sp(rx_sp() + 4);
    }
    SU32(w, 0x22180, m);
    rx_set_sp(rx_sp() + 4);
}
RX_FUNCTION(0x007651d0, rx_mxuiworld_start)

/* the garage map's key (signed) and node */
static int garage_less(rx_addr a, rx_addr b) { return I32(a, 0) < I32(b, 0); }
static rx_addr garage_make(rx_addr head, rx_addr where, rx_addr val) {
    const rx_addr z = rx_reflex_aligned_alloc(0x20);
    if (z) {
        SU32(z, 0, head); SU32(z, 4, where); SU32(z, 8, head);
        SU32(z, 0xc, U32(val, 0)); SU32(z, 0x10, U32(val, 4)); SU32(z, 0x14, U32(val, 8)); SU32(z, 0x18, U32(val, 0xc));
        SU8(z, 0x1c, 0); SU8(z, 0x1d, 0);
    }
    return z;
}
static const vt_tree k_garage_map = {0x1c, 0x7fffffu, garage_less, garage_make};

/* 00765900 (stdcall map, &key; RET 8): the value (+0x10 of the node) of the key, inserted (hint = the lower bound) when absent */
static rx_addr garage_index(rx_addr map, rx_addr keyp) {
    rx_addr head = U32(map, 0x18), lb = head;
    for (rx_addr n = U32(head, 4); !U8(n, 0x1d);) {
        if (I32(n, 0xc) < I32(keyp, 0)) n = U32(n, 8);
        else { lb = n; n = U32(n, 0); }
    }
    rx_addr node = lb;
    if (lb == head || I32(keyp, 0) < I32(lb, 0xc)) {
        RX_FRAME_ENTER(F, 0x10);
        SU32(F, 0, U32(keyp, 0)); SU32(F, 4, 0); SU32(F, 8, 0); SU32(F, 0xc, 0);
        node = vt_insert_hint(&k_garage_map, map, F, lb);
        RX_FRAME_LEAVE(F, 0x10);
    }
    if (node == U32(map, 0x18)) crt_invalid_parameter_noinfo();
    return node + 0x10;
}

/* 00765820 (EAX = the UI world): the scene's type 0xb entities except the world's +0x22178 one get +0x1a8 = 1; that entity (when set) takes +0x2217c as its
 * +0x1cc / +0x198 (when set) and +0x1a8 = 0; the +0x22154 entity is hidden (+0xa2 = 0) */
void rx_mxuiworld_tutorial_mode_on(rx_addr w) {
    ACT_FRAME(L, 0x14, 3);
    const rx_addr list = L + 0x10;
    const rx_addr buf = rx_reflex_aligned_alloc(rx_mul_sat(0x14, 4));
    SU32(L, 0xc, buf);
    SU32(list, 0, buf); SU32(list, 4, 0); SU32(list, 8, 0x14); SU32(list, 0xc, 0x14); SU32(list, 0x10, 0x7fffffff);
    rx_scene_query_typed(U32(w, 0x20138), list, 0xb);
    for (int32_t i = 0; i < I32(list, 4); i++) {
        const rx_addr e = U32(U32(list, 0), 4u * (uint32_t)i);
        if (e != U32(w, 0x22178)) SU32(e, 0x1a8, 1);
    }
    if (U32(w, 0x22178) != 0) {
        if (U32(w, 0x2217c) != 0) { SU32(U32(w, 0x22178), 0x1cc, U32(w, 0x2217c)); SU32(U32(w, 0x22178), 0x198, U32(w, 0x2217c)); }
        SU32(U32(w, 0x22178), 0x1a8, 0);
    }
    if (U32(w, 0x22154) != 0) SU8(U32(w, 0x22154), 0xa2, 0);
    rx_reflex_aligned_free(U32(list, 0));
    ACT_END(L);
}
RX_FUNCTION(0x00765820, rx_mxuiworld_tutorial_mode_on)

/* MXUIWorld::vf3 00765230 (thiscall table; RET 4): MXWorld::vf3 (the table's activities), the customize rider entity / the rider AO entity, the garage map
 * {vehicle type -> location entity, AO entity, scale} for the LITE (2), MX (3), ATV (4), UTV (0xb), SB (0xc), PL (0xe), P2 (0xf) vehicles, each AO entity
 * hidden (+0xa2 = 0, +0xac = 0), the "UIAmbientDirectionalLight" entity (+0x22178), then TutorialModeOn */
void rx_mxuiworld_vf3(rx_addr w, rx_addr table) {
    ACT_FRAME_EBP(L, 8, 0x24, 3);
    ACT_CALL(1, rx_mxworld_vf3(w, table));
    const rx_addr sc = U32(w, 0x20138);
    SU32(w, 0x22150, rx_scene_find_by_name(sc, 0x009586f8 /* UICustomizeRiderLocation */));
    SU32(w, 0x22154, rx_scene_find_by_name(sc, 0x0096db14 /* UIRider_AO_Geo */));
    static const struct { int32_t type; uint32_t ao, loc, scale; } g[7] = {
        {2,    0x0096db24, 0x0096db40, 0x3d75c28fu /* 0x977464 */},   /* LITE */
        {3,    0x0096db60, 0x0096db78, 0x3d75c28fu},                  /* MX */
        {4,    0x0096db98, 0x0096dbb4, 0x3d75c28fu},                  /* ATV */
        {0xb,  0x0096dbd4, 0x0096dbf0, 0x3d8f5c29u /* 0x977824 */},   /* UTV */
        {0xc,  0x0096dc10, 0x0096dc28, 0x3da3d70au /* 0x977d90 */},   /* SB */
        {0xe,  0x0096dc48, 0x0096dc60, 0x3da3d70au},                  /* PL */
        {0xf,  0x0096dc80, 0x0096dc98, 0x3da3d70au},                  /* P2 */
    };
    const rx_addr map = w + 0x22158, key = L + 0x10;
    for (int k = 0; k < 7; k++) {
        const rx_addr ao = rx_scene_find_by_name(sc, g[k].ao), loc = rx_scene_find_by_name(sc, g[k].loc);
        SU32(key, 0, (uint32_t)g[k].type);
        const rx_addr v = garage_index(map, key);
        SU32(v, 0, loc); SU32(v, 4, ao); SU32(v, 8, g[k].scale);
    }
    for (rx_addr n = U32(U32(map, 0x18), 0); n != U32(map, 0x18); n = vt_next(&k_garage_map, n)) {
        if (U32(n, 0x14) != 0) { SU8(U32(n, 0x14), 0xa2, 0); SU32(U32(n, 0x14), 0xac, 0); }
    }
    {
        rr_os_cs_enter(U32(w, 0x20138) + 4);
        SU32(L, 0x10, 0);
        SU32(L, 0x14, 0x0096dcb8 /* UIAmbientDirectionalLight */);
        const uint32_t found = rx_name_hash_find(U32(w, 0x20138) + 0x19f8, L + 0x14, L + 0x10);
        rr_os_cs_leave(U32(w, 0x20138) + 4);
        SU32(w, 0x22178, found ? U32(L, 0x10) : 0);
    }
    ACT_CALL(0, rx_mxuiworld_tutorial_mode_on(w));
    ACT_END(L);
}
RX_FUNCTION(0x00765230, rx_mxuiworld_vf3)

/* MXUIWorld::vf5 00765670 (thiscall dt; x87 float): MXWorld::vf5 (the world frame, loop unit), then the TutorialManager's update when there is
 * one (+0x22180: TutorialManager_Update 007500c0) */
uint32_t rx_mxuiworld_vf5(rx_addr w, float dt) {
    const rx_addr e = rx_sp();
    rx_set_sp(e - 8);                                                    /* push esi / edi */
    uint32_t r;
    ACT_CALLR(r, 1, rx_world_update(w, dt));                             /* push ecx; fstp [esp] (dt): MXWorld::vf5 (RET 4) */
    const rx_addr m = U32(w, 0x22180);
    if (m != 0) ACT_CALL(2, rx_tutorial_update(m, dt));                 /* push ecx (dt); push esi (m): TutorialManager_Update (RET 8) */
    rx_set_sp(e);
    return r;
}
RX_FUNCTION(0x00765670, rx_mxuiworld_vf5)

/* ---------------------------------------------------------------------------------------------------------------------------
 * the init thread's UI world part */

static const char* host_str(rx_addr s) {
    static char b[512];
    uint32_t i = 0;
    for (; i < 511; i++) { b[i] = (char)U8(s, i); if (!b[i]) break; }
    b[i] = 0;
    return b;
}

/* ---- the UI manager's event channel ([0xa93fcc] + 0x98) --------------------------------------------------------------- */
/* 005da6cb .. 005da739 (the UIManager base constructor): an EventSystem::Channel (0x5c bytes, base constructor 005930a0, vtable
 * 0x00946258, proxy +0x40, head +0x54 from 007ce320, size +0x58) registered in the channel registry (00593520) under "UI"
 * (00593900). The events the game raises on it are the ones MXUIManager::vf42 (005999f0) subscribes the UI's handlers to; this
 * stand-in registers the same event objects with their original signature type_infos and no handlers (the product has no UI).
 * PRODUCT ONLY (the oracle's UI manager builds its own). */
static rx_addr g_ui_event_channel;
#ifdef RX_ORACLE
rx_addr rx_ui_event_channel(void) { return U32(U32(0, 0x00a93fcc), 0x98); }
#else
rx_addr rx_ui_event_channel(void) { return g_ui_event_channel; }
static void ui_event_channel_create(void) {
    if (g_ui_event_channel) return;
    const rx_addr ch = rx_event_channel_create(0x0094bf04 /* "UI" (the channel registry key the constructor uses) */);
    g_ui_event_channel = ch;
    if (!ch) return;
    /*005999f0/00599a17: original UI constructor registers RestartActivity
     * through00586ca0, whose signature type_info is009cf3f0. Base career
     * vf7/vf8 subscribe/unsubscribe it on this same registry.*/
    rx_event_type_register(ch, 0x0094c3ac /*RestartActivity*/, 0x009cf3f0);
    /*Actual career fade signatures from005bdaa0 and005bd9e0. Registration
     * supplies channels only: no completion is synthesized or acknowledged.*/
    rx_event_type_register(ch, 0x0094d65c /*FadeOut*/, 0x009d2590); /*void(float,uint32)*/
    rx_event_type_register(ch, 0x0094d174 /*FadeComplete*/, 0x009cdb68); /*void(const char*)*/
    rx_event_type_register(ch, 0x009420d0 /* "HoleshotActivated" */, 0x009cbeb4);   /* void(MXPlayer*)    (005c0df0's type_info) */
    rx_event_type_register(ch, 0x00941f90 /* "FreeRide Started" */, 0x009cf3f0);   /* void()             (00586ca0) */
    rx_event_type_register(ch, 0x00941e9c /* "ShowPageWipe" */, 0x009cdb68);       /* (the name string)  (00592820) */
    rx_event_type_register(ch, 0x00941d7c /* "PageWipeTransition" */, 0x009cdb68); /* (the name string)  (00592820) */
    /*00599bc7/00599bcc: same authentic string signature as In transition.*/
    rx_event_type_register(ch, 0x0094c5ec /* "PageWipeComplete" */, 0x009cdb68);
}
#endif

/* FUN_0075ed60 from the UI level on (the init thread runs it on its own thread in the original; a plain call in order here) -> 1, 0 when the UI level's
 * packages never finish loading */
int rx_ui_world_create(rx_addr g, struct fmt_install* in, struct fmt_level* level) {
    const rx_addr cfg = g + 0x34, ctx = U32(0, 0x00a95844), db = U32(ctx, 8);
    rx_event_set_level(level);
    rx_records_create();                                                 /* g_MXRecords: MXUIManager's constructor makes it (no UI manager here) */
#ifndef RX_ORACLE
    ui_event_channel_create();                                           /* the manager constructor's "UI" event channel (no UI manager here) */
    rx_activation_listen();                                              /* MXUIManager::vf42's vehicle trigger handlers (race_activation.c) */
#endif
    rx_database_load(db, in, host_str(cfg), "Database\\", 0, 0);        /* the UI level's database: Database vf5 ("Database\\", MXGame + 0x34) */
    ACT_CALL(1, rx_level_config_init(cfg));                              /* 00756060 */
    ACT_CALL(1, rx_level_load_request(cfg, 1));                          /* 007560f0 */
    uint32_t done = 0;
    for (int i = 0; i < 10000 && !done; i++) { ACT_CALLR(done, 0, rx_level_loaded(cfg)); }   /* 00756250 */
    if (!done) return 0;
    rx_addr w = rx_reflex_aligned_alloc(0x22190);
    if (w) w = rx_mxuiworld_construct(w, cfg);
    SU32(g, 0x20c8, w);
    rx_scene_set_active(0, U32(ctx, 0x20));                              /* 0075ac50: the event scene off, the UI scene on, the engine's scene index 0 */
    rx_scene_set_active(1, U32(ctx, 0x1c));
    SU32(ctx, 4, 0);
    ACT_CALL(1, rx_scene_directory_read(cfg));                           /* 007562b0 */
    rx_addr pw;
    ACT_CALLR(pw, 1, rx_physics_world_create(U32(ctx, 0x1c)));           /* 0075da90 */
    ACT_CALL(2, rx_mxworld_load_base(w, 1));                             /* the job "EnableCoreSubSceneCallback": 0075ed10 -> 00766110 (world, 1) */
    rx_mxuiworld_start(w, U32(ctx, 0x1c), cfg, pw, 0x00adffe0);          /* vf1 */
    ACT_CALL(4, (void)rx_activity_table_add_row(0x00adffe0, 8, 0, 4));   /* the FreeRide row of "Activity List UI World" */
    rx_mxuiworld_vf3(w, 0x00adffe0);                                     /* vf3 */
    ACT_CALL(1, rx_world_activities_start(w));                           /* 007668e0 */
    /* the front end's pause: the engine timing's pause word is 1 while the UI world is the one in view (oracle GRIDSTATE at the
     * front end: pause 1, the UI world's +0x20014 = 0 -- the pause comes from MXGame::Update's session-pause block 006e0c50, which
     * writes the timing word only while no world exists yet; MXGame::vf10's own world pause is not the one the original has here).
     * MXGame_Update case 5 clears it at the load's end, as the block's resume does (build_activity_game.c). */
    SU32(U32(U32(0, 0x00a95844), 0x10), 0xc, 1);
#ifndef RX_ORACLE
    /* w-rng: the front end's start. MXGame::Update's state 1 (the init thread's end) switches to the UI world (MXGame_SwitchToUIWorld
     * 0075ac90), whose grid pass 0075f890(game, 8 FreeRide, 4, assign 1) at 0075ae48 makes one draw of the game's LCG (the mini-game
     * variant: count 0 -> 1 before the event's AI roster in the oracle's boot, RX_RNG_LOG). The product has no front end and never ran
     * it, so every later draw of the game's generator was one behind. The grid pass is the game's (the UI world's Player List rows, the
     * activity table's MiniGameStartZone, [0x00da6738]); the rest of the switch is the front end's (the UI properties, the audio, the
     * particles' clear, the pause) and stays outside the core. */
    ACT_CALL(4, rx_game_grid_pass(g, 8, 4, 1));
#endif
    return 1;
}
