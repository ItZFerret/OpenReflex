/* build_activity_world.c -- construction unit 8: the event's world load, in the original's order (construction.md section 16.4).
 * MXGame::Update state 3 requests the level (the level config MXGame + 0x39c0: 00756060 names its packages Shared / <level>Base and its
 * base subscene; the level database and the packages through the Database: 007560f0), state 4 waits for them (00756250) and starts
 * FUN_00759f00 ("Subscene Creation", on the loader thread in the original; a plain call here): the MXWorld (world unit), the scene's
 * subscene directory (007562b0 with the activity subscene list 00556140), the physics world, the Base subscenes (00766110 ->
 * 00766250), the render culling grid (render, not core), MXWorld::vf1 (world unit) and vf3 (the activities, build_activity_race.c).
 *
 * The level config (0x2690 + bytes at MXGame + 0x39c0; the front end's at + 0x34): +0 the level name, +0x104 the directory
 * ("Database\\"), +0x208 the database name, +0x30c its package, +0x410 package names (0x104 each), +0x618 their load progress (floats),
 * +0x620 the package count, +0x624 the base subscene name "<level>BaseSubscene", +0x82c 1, +0x830 the subscene list (0x104 each) and
 * +0x2090 its count, +0x209c 1 = build the activity subscene list, +0x20a0 the progress, +0x20a4 the scene-from-pack flag, +0x20a8 the
 * scene file's subscene directory (names 0x104 each, at most 0x18; offsets +0x3908, count +0x3968), +0x396c the hash list index ->
 * file offset (an int hash map: buckets +0, load +4, count +8, bucket array +0xc, allocator +0x18). */
#include "build_activity_util.h"
#include "build_mxworld.h"
#include "build_world.h"
#include "build_asset.h"
#include "build_scene.h"
#include "build_ui_world.h"
#include "race_misc.h"
#include "../src/fmt_scene.h"
#include <string.h>
#include <stdarg.h>

/* fld dword / fstp dword: the bits, a signalling NaN quieted */
static uint32_t x87_copy(uint32_t b) { return ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu)) ? (b | 0x400000u) : b; }
static const char* host_str(rx_addr s) {
    static char b[4][512]; static int k;
    char* d = b[k++ & 3];
    uint32_t i = 0;
    for (; i < 511; i++) { d[i] = (char)U8(s, i); if (!d[i]) break; }
    d[i] = 0;
    return d;
}
/* FUN_0078fd60 (_vsnprintf into a fixed buffer, terminated) */
static void w_format(rx_addr dst, uint32_t size, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
static void w_format(rx_addr dst, uint32_t size, const char* fmt, ...) {
    char buf[1024];
    va_list ap; va_start(ap, fmt); int n = vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    if (n < 0) n = 0;
    uint32_t len = (uint32_t)n < size ? (uint32_t)n : size;
    for (uint32_t i = 0; i < len; i++) SU8(dst, i, (uint8_t)buf[i]);
    if (n > 0 && (uint32_t)n < size) SU8(dst, (uint32_t)n, 0); else SU8(dst, size - 1, 0);
}

/* a Database virtual (the engine's Database [[0x00a95844] + 8]): the core's when registered, else the original */
static rx_addr database(void) { return U32(U32(0, 0x00a95844), 8); }
static uint32_t db_call(uint32_t slot, int n, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const rx_addr db = database(), fn = U32(U32(db, 0), slot);
    void* f = RX_FN_INTS(fn, n + 1);
    uint32_t r = 0;
    if (f) {
        switch (n) {
        case 2: ACT_CALLR(r, 2, ((uint32_t (*)(rx_addr, uint32_t, uint32_t))f)(db, a0, a1)); break;
        case 3: ACT_CALLR(r, 3, ((uint32_t (*)(rx_addr, uint32_t, uint32_t, uint32_t))f)(db, a0, a1, a2)); break;
        case 4: ACT_CALLR(r, 4, ((uint32_t (*)(rx_addr, uint32_t, uint32_t, uint32_t, uint32_t))f)(db, a0, a1, a2, a3)); break;
        default: act_unreached(fn, "a Database virtual with an unexpected argument count");
        }
        return r;
    }
    const uint32_t a[4] = {a0, a1, a2, a3};
    return rx_call_original(fn, RX_THISCALL, db, n, a);
}
/* the void-returning ones (vf4 / vf15 / vf16 / vf17 / vf6) */
static void db_vcall(uint32_t slot, int n, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const rx_addr db = database(), fn = U32(U32(db, 0), slot);
    void* f = RX_FN_INTS(fn, n + 1);
    if (f) {
        switch (n) {
        case 2: ACT_CALL(2, ((void (*)(rx_addr, uint32_t, uint32_t))f)(db, a0, a1)); break;
        case 3: ACT_CALL(3, ((void (*)(rx_addr, uint32_t, uint32_t, uint32_t))f)(db, a0, a1, a2)); break;
        case 4: ACT_CALL(4, ((void (*)(rx_addr, uint32_t, uint32_t, uint32_t, uint32_t))f)(db, a0, a1, a2, a3)); break;
        default: act_unreached(fn, "a Database virtual with an unexpected argument count");
        }
        return;
    }
    const uint32_t a[4] = {a0, a1, a2, a3};
    rx_call_original(fn, RX_THISCALL, db, n, a);
}

/* ======================================================================================================================== */
/* the level's load requests                                                                                                */

/* 00756060 (stdcall config; RET 4): the level's packages "Shared" and "<level>Base" (progress 0), its base subscene name, the activity
 * subscene list to be built (+0x209c) */
void rx_level_config_init(rx_addr c) {
    crt_strncpy(c + 0x410, 0x0096d55c /* "Shared" */, 0x103);
    SU8(c, 0x410 + 0x103, 0);
    w_format(c + 0x514, 0x104, "%sBase", host_str(c));
    SF32(c, 0x618, 0.0f); SF32(c, 0x61c, 0.0f);
    SU32(c, 0x620, 2);
    w_format(c + 0x624, 0x104, "%sBaseSubscene", host_str(c));
    SU32(c, 0x82c, 1);
    SU32(c, 0x209c, 1);
}
RX_FUNCTION(0x00756060, rx_level_config_init)

/* 007560f0 (ESI = config; stdcall async; RET 4): the config's database (vf4 "Database\\" + name, then its package vf16) when named,
 * then each package: async -> vf16 (the progress float), else vf15 and the progress 1.0; the total progress +0x20a0 = 0 */
void rx_level_load_request(rx_addr c, uint32_t async) {
    ACT_FRAME(L, 0, 2);                                                /* push ebp / edi (ebx is pushed after the database's calls) */
    if (U8(c, 0x208) != 0) {
        db_vcall(0x10, 4, 0x0093e434 /* "Database\\" */, c + 0x208, 0, 0);
        db_vcall(0x40, 4, c + 0x208, c + 0x30c, 0, 0);
    }
    ACT_FRAME(R, 0, 1);                                                /* push ebx (00756140) */
    for (uint32_t i = 0; i < U32(c, 0x620); i++) {
        if (async) db_vcall(0x40, 4, c, c + 0x410 + 0x104 * i, c + 0x618 + 4 * i, 0);
        else { db_vcall(0x3c, 3, c, c + 0x410 + 0x104 * i, 0, 0); SF32(c, 0x618 + 4 * i, 1.0f); }
    }
    ACT_END(R);
    SF32(c, 0x20a0, 0.0f);
    ACT_END(L);
}
RX_FUNCTION(0x007560f0, rx_level_load_request)

/* 00756250 (ESI = config) -> AL: every package of the config known (vf9) and loaded (vf10). rx_level_loaded_eax: the whole EAX as
 * the original leaves it (AL over the last virtual's result, or eax_in when there are no packages) */
uint32_t rx_level_loaded_eax(rx_addr c, uint32_t eax_in) {
    ACT_FRAME(L, 0, 2);
    uint32_t eax = eax_in, r = 1;
    for (uint32_t i = 0; i < U32(c, 0x620); i++) {
        eax = db_call(0x24, 2, c, c + 0x410 + 0x104 * i, 0, 0);
        if (!eax) { r = 0; break; }
        eax = db_call(0x28, 2, c, c + 0x410 + 0x104 * i, 0, 0);
        if (!eax) { r = 0; break; }
    }
    ACT_END(L);
    return (eax & ~0xffu) | r;
}
uint32_t rx_level_loaded(rx_addr c) { return rx_level_loaded_eax(c, 0) & 0xff; }
RX_FUNCTION(0x00756250, rx_level_loaded)

/* 007561b0 (ESI = config): the config's database package and database released (vf17 / vf6 at the engine's frame [+0x10] + 0x58, the
 * name cleared), then every loaded package (vf17) */
void rx_level_release(rx_addr c) {
    ACT_FRAME(L, 0, 2);
    const uint32_t frame = U32(U32(U32(0, 0x00a95844), 0x10), 0x58);
    if (U8(c, 0x208) != 0) {
        db_vcall(0x44, 3, c + 0x208, c + 0x30c, frame, 0);
        db_vcall(0x18, 2, c + 0x208, frame, 0, 0);
        SU8(c, 0x208, 0);
    }
    for (uint32_t i = 0; i < U32(c, 0x620); i++)
        if (db_call(0x28, 2, c, c + 0x410 + 0x104 * i, 0, 0)) db_vcall(0x44, 3, c, c + 0x410 + 0x104 * i, frame, 0);
    ACT_END(L);
}
RX_FUNCTION(0x007561b0, rx_level_release)

/* ======================================================================================================================== */
/* the scene file's subscene directory                                                                                      */

/* 00755f80 (EAX = directory (config + 0x20a8, count at + 0x18c0); stdcall name; RET 4): the name in the directory (strcmp) */
uint32_t rx_scene_dir_has(rx_addr dir, rx_addr name) {
    for (uint32_t i = 0; i < U32(dir, 0x18c0); i++) if (crt_strcmp(dir + 0x104 * i, name) == 0) return 1;
    return 0;
}
RX_FUNCTION(0x00755f80, rx_scene_dir_has)

/* 00556140 (cdecl config, directory): the activity subscene list (+0x830): for each activity type 0 .. 0x1e its area-0 subscene name
 * (00555e60) kept when the directory has it or the Database has a 'subscene' asset of that name (vf26); the area variants 1, 2, ...
 * of the types 6 / 0x15 / 0x16 / 0x18 / 0x1b while the directory has them; last "<level>Base"; the count +0x2090 */
void rx_activity_subscene_list(rx_addr c, rx_addr dir) {
    const rx_addr arg0 = rx_sp() + 4;   /* the original keeps its scratch in its first argument's slot (the in-directory flag byte, then
                                           the area counter): written like the original, the caller's slot holds them after the call */
    ACT_FRAME(L, 0, 5);
    uint32_t k = 0;
    for (uint32_t type = 0; type < 0x1f; type++) {
        rx_addr slot = c + 0x830 + 0x104 * k;
        ACT_CALL(2, rx_activity_area_name(c, type, slot, 0));
        SU8(arg0, 0, 0);
        if (dir && rx_scene_dir_has(dir, slot)) SU8(arg0, 0, 1);
        uint32_t has = db_call(0x68, 3, slot, 0x73627573, 0x656e6563 /* 'subscene' */, 0);
        if (has == 0 && U8(arg0, 0) == 0) continue;
        k++;
        if (type == 6 || type == 0x15 || type == 0x16 || type == 0x18 || type == 0x1b) {
            slot = c + 0x830 + 0x104 * k;
            SU32(arg0, 0, 1);
            ACT_CALL(2, rx_activity_area_name(c, type, slot, 1));
            while (dir && rx_scene_dir_has(dir, slot)) {
                k++;
                SU32(arg0, 0, U32(arg0, 0) + 1);
                slot = c + 0x830 + 0x104 * k;
                ACT_CALL(2, rx_activity_area_name(c, type, slot, U32(arg0, 0)));
            }
        }
    }
    w_format(c + 0x830 + 0x104 * k, 0x104, "%sBase", host_str(c));
    SU32(c, 0x2090, k + 1);
    ACT_END(L);
}
RX_FUNCTION(0x00556140, rx_activity_subscene_list)

/* 007cfa10 (EDI = int hash map; stdcall &key, &value; RET 8): insert or replace (Wang's hash 00755d30; grown by 1.5 (0084df50) when
 * count / buckets passes the load; nodes of 0x10 {key, value, hash, next} from the map's allocator vf1) */
void rx_int_hash_insert(rx_addr h, rx_addr key_p, rx_addr value_p) {
    const float load = (float)I32(h, 8) / (float)I32(h, 0);
    if (load > F32(h, 4)) rx_strtab_rehash(h, (uint32_t)(int32_t)((float)I32(h, 0) * 1.5f));
    const uint32_t key = U32(key_p, 0), hv = rx_int_hash_wang32(key), b = hv % U32(h, 0);
    for (rx_addr e = U32(U32(h, 0xc), 4 * b); e; e = U32(e, 0xc))
        if (U32(e, 8) == hv && U32(e, 0) == key) { SU32(e, 4, U32(value_p, 0)); return; }
    const rx_addr alloc = U32(h, 0x18), afn = U32(U32(alloc, 0), 4);
    rx_addr (*af)(rx_addr, uint32_t) = RX_FN(afn, rx_addr (*)(rx_addr, uint32_t));
    rx_addr e;
    if (af) ACT_CALLR(e, 1, af(alloc, 0x10));
    else { uint32_t a = 0x10; e = rx_call_original(afn, RX_THISCALL, alloc, 1, &a); }
    if (e) SU32(e, 0xc, 0);
    SU32(e, 0, key); SU32(e, 8, hv); SU32(e, 4, U32(value_p, 0));
    SU32(e, 0xc, U32(U32(h, 0xc), 4 * b));
    SU32(U32(h, 0xc), 4 * b, e);
    SU32(h, 8, U32(h, 8) + 1);
}
RX_FUNCTION(0x007cfa10, rx_int_hash_insert)

/* 007562b0 (stdcall config): the scene file "<dir><level>.dx9.scene" ("....scene.<ext>" from the pack): its subscene directory (at most
 * 0x18 names and offsets), the activity subscene list (00556140) when asked (+0x209c == 1), then per directory entry the config
 * subscenes of that name indexed to its file offset (+0x396c). The original reads the file's header; the product reads the same table
 * with fmt_scene (the oracle runs the original: file I/O). */
void rx_scene_directory_read(rx_addr c) {
#ifdef RX_ORACLE
    rx_call_original(0x007562b0, RX_STDCALL, 0, 1, &c);
#else
    ACT_FRAME(L, 0x240, 4);
    fmt_scene* sc = rx_event_level() ? fmt_level_scene((fmt_level*)rx_event_level()) : NULL;
    if (!sc) act_unreached(0x0075634d, "the level's scene file (rx_event_set_level)");
    uint32_t n = (uint32_t)fmt_scene_subscene_count(sc);
    if (n > 0x18) n = 0x18;
    SU32(c, 0x3968, n);
    for (uint32_t i = 0; i < n; i++) {
        const char* nm = fmt_scene_subscene_name(sc, (int)i);
        for (uint32_t k = 0; k < 0x104; k++) { uint8_t ch = (uint8_t)nm[k]; SU8(c + 0x20a8 + 0x104 * i, k, ch); if (!ch) { for (uint32_t z = k; z < 0x104; z++) SU8(c + 0x20a8 + 0x104 * i, z, 0); break; } }
        SU32(c, 0x3908 + 4 * i, fmt_scene_subscene_offset(sc, (int)i));
    }
    if (U32(c, 0x209c) == 1) ACT_CALL(2, rx_activity_subscene_list(c, c + 0x20a8));
    for (uint32_t j = 0; j < U32(c, 0x3968); j++) {
        for (uint32_t i = 0; i < U32(c, 0x2090); i++) {
            if (crt_strcmp(c + 0x20a8 + 0x104 * j, c + 0x830 + 0x104 * i) != 0) continue;
            SU32(L, 0x14, i);
            ACT_CALL(2, rx_int_hash_insert(c + 0x396c, L + 0x14, c + 0x3908 + 4 * j));
        }
    }
    ACT_END(L);
#endif
}
RX_FUNCTION(0x007562b0, rx_scene_directory_read)

/* 00766110 (stdcall world, event; RET 8): the config's subscenes named "...Base..." loaded: for the event into its scene (00766250), else (the
 * front end) the scene's own loader 007d19f0 after the audio listeners' flags; then an audio-thread round trip (Audio_PostCommand 1 and
 * a wait: audio, not core). The oracle runs the original (its audio wait switches threads). */
void rx_mxworld_load_base(rx_addr w, uint32_t event) {
#ifdef RX_ORACLE
    const uint32_t a[2] = {w, event};
    rx_call_original(0x00766110, RX_STDCALL, 0, 2, a);
#else
    const rx_addr game = U32(0, 0x00a946b8), c = U32(w, 0x20018) ? game + 0x34 : game + 0x39c0;
    for (uint32_t i = 0; i < U32(c, 0x2090); i++) {
        const rx_addr nm = c + 0x830 + 0x104 * i;
        if (!crt_strstr(nm, 0x0094fc98 /* "Base" */)) continue;
        if (!event) act_unreached(0x00766175, "the front end's Base subscenes (007d19f0)");
        ACT_CALL(1, rx_mxworld_load_subscene(w, nm));
    }
#endif
}
RX_FUNCTION(0x00766110, rx_mxworld_load_base)

/* ======================================================================================================================== */
/* FUN_00759f00: the event's world ("Subscene Creation")                                                                    */

/* render (the scene's culling grid, scene + 0x1a18 + 0x10f0: not core): oracle the original, product nothing */
#ifdef RX_ORACLE
static void w_render_call(rx_addr fn, const rx_regs* in, int n, const uint32_t* a) { rx_regs o; rx_call_original_ex(fn, in, &o, n, a); }
#else
static void w_render_call(rx_addr fn, const rx_regs* in, int n, const uint32_t* a) { (void)fn; (void)in; (void)n; (void)a; }
#endif

/* FUN_007d25c0 (EAX = scene, ESI = the list {data, count, capacity, grow}; stdcall type; RET 4): the scene's entities of a type */
static void scene_query(rx_addr scene, rx_addr list, uint32_t type) {
    ACT_CALL(1, rx_scene_query_typed(scene, list, type));
}

/* 00759f00 (cdecl game, &progress) -> 0: the MXWorld (00765e20) at game + 0x5a54 on the level config game + 0x39c0, the scene file's
 * directory (007562b0), the physics world of the engine's scene ([0x00a95844] + 0x20; 0075da90), the Base subscenes (00766110), the
 * culling grid from the scene's entity of type 0xd (its bounds) or of the terrain (0x1027: its height map's bounds) or the type 0x10
 * entity (render), MXWorld::vf1 (scene, config, physics world, the activity table), MXWorld::vf3 (the activities), game + 0x15b54 = 1
 * (the world is ready); the progress 0, 0.25, 0.5, 0.75, 1 */
uint32_t rx_world_construct(rx_addr game, rx_addr progress) {
    ACT_FRAME_EBP(L, 16, 0x94, 3);
    SF32(progress, 0, 0.0f);
    rx_addr w = rx_reflex_aligned_alloc(0x22150);
    if (w) ACT_CALLR(w, 1, rx_mxworld_construct(w, game + 0x39c0));
    SU32(game, 0x5a54, w);
    ACT_CALL(1, rx_scene_directory_read(game + 0x39c0));
    const rx_addr core = U32(0, 0x00a95844), scene = U32(core, 0x20);
    rx_addr pw;
    ACT_CALLR(pw, 1, rx_physics_world_create(scene));
    SU32(L, 0x34, pw);
    ACT_CALL(2, rx_mxworld_load_base(U32(game, 0x5a54), 1));
    /* the scene's type 0xd entities: the list {data, count, capacity 2, grow 2, limit} at L + 0x20 */
    const rx_addr l0 = L + 0x20;
    SU32(l0, 0, rx_reflex_aligned_alloc(2 * 4));
    SU32(l0, 8, 2); SU32(l0, 0xc, 2); SU32(l0, 4, 0); SU32(l0, 0x10, 0x7fffffff);
    scene_query(U32(core, 0x20), l0, 0xd);
    if (U32(l0, 4) != 0) {
        const rx_addr e = U32(U32(l0, 0), 0);
        if (U32(e, 0x9c) != 0) {   /* 00755de0: the entity's bounds (+0xb0 -> L + 0x40, +0xc0 -> L + 0x90) */
            for (uint32_t k = 0; k < 3; k++) { SU32(L, 0x40 + 4 * k, x87_copy(U32(e, 0xb0 + 4 * k))); SU32(L, 0x90 + 4 * k, x87_copy(U32(e, 0xc0 + 4 * k))); }
        }
        for (uint32_t k = 0; k < 3; k++) SU32(L, 0x40 + 4 * k, U32(0, 0x00a95d50 + 4 * k));
        rx_regs in; memset(&in, 0, sizeof in); in.r[7] = U32(core, 0x20) + 0x1a18;
        const uint32_t args[4] = {L + 0x90, L + 0x40, e, U32(e, 0x90)};
        w_render_call(0x007e14a0, &in, 4, args);   /* the culling grid from the entity */
    } else SU32(U32(core, 0x20), 0x2b64, 0);
    SF32(progress, 0, 0.25f);
    /* the terrain (type 0x1027), else the type 0x10 entity: the list {data, count, capacity 1, grow 1, limit} at L + 0xc */
    const rx_addr l1 = L + 0xc;
    SU32(l1, 0, rx_reflex_aligned_alloc(1 * 4));
    SU32(l1, 4, 0); SU32(l1, 8, 1); SU32(l1, 0xc, 1); SU32(l1, 0x10, 0x7fffffff);
    scene_query(U32(core, 0x20), l1, 0x1027);
    if (U32(l1, 4) != 0) {
        const rx_addr h = U32(U32(U32(l1, 0), 0), 0xb8), hm = U32(h, 0x44);   /* the height map asset */
        SU32(h, 0x4c, 1);
        for (uint32_t k = 0; k < 3; k++) {
            const float lo = F32(hm, 0x2040 + 4 * k), hi = F32(hm, 0x2050 + 4 * k);
            const float ext = (hi - lo) * 0.5f;
            SF32(L, 0x80 + 4 * k, ext); SF32(L, 0x50 + 4 * k, lo + ext);
        }
        if (U32(l0, 4) == 0) {
            rx_regs in; memset(&in, 0, sizeof in); in.r[7] = U32(core, 0x20) + 0x1a18;
            const uint32_t args[2] = {L + 0x50, L + 0x80};
            w_render_call(0x007e1130, &in, 2, args);   /* the culling grid over the terrain */
        }
        SU32(h, 0x4c, 0);
    } else {
        scene_query(U32(core, 0x20), l1, 0x10);
        if (U32(l1, 4) != 0) {
            const rx_addr e = U32(U32(l1, 0), 0);
            SU32(L, 0x38, U32(e, 0x4560)); SU32(L, 0x3c, U32(e, 0x4564));   /* 00755e30 */
            for (uint32_t k = 0; k < 3; k++) SU32(L, 0x60 + 4 * k, U32(0, 0x00a95d50 + 4 * k));
            SU32(L, 0x70, U32(L, 0x38)); SF32(L, 0x74, 0.0f); SU32(L, 0x78, U32(L, 0x3c));
            if (U32(l0, 4) == 0) {
                rx_regs in; memset(&in, 0, sizeof in); in.r[7] = U32(core, 0x20) + 0x1a18;
                const uint32_t args[2] = {L + 0x60, L + 0x70};
                w_render_call(0x007e1130, &in, 2, args);
            }
        }
    }
    SF32(progress, 0, 0.5f);
    SF32(progress, 0, 0.75f);
    /* MXWorld::vf1 (scene, config, physics world, the activity table) and vf3 (the activities): the world unit's / build_activity_race.c */
    ACT_CALL(4, rx_mxworld_start(U32(game, 0x5a54), U32(core, 0x20), game + 0x39c0, pw, ACT_ACTIVITY_TBL));
    ACT_CALL(1, rx_mxworld_vf3(U32(game, 0x5a54), ACT_ACTIVITY_TBL));
    SU32(game, 0x15b54, 1);
    SF32(progress, 0, 1.0f);
    rx_reflex_aligned_free(U32(l1, 0));
    rx_reflex_aligned_free(U32(l0, 0));
    ACT_END(L);
    (void)scene;
    return 0;
}
RX_FUNCTION(0x00759f00, rx_world_construct)

/* 00767480 (ECX = world, EAX = paused) -> changed: the engine's pause flag ([0x00a95844] + 0x10 + 0xc) and the world's (+0x20014) */
uint32_t rx_mxworld_set_paused(rx_addr w, uint32_t v) {
    SU32(U32(U32(0, 0x00a95844), 0x10), 0xc, v);
    if (U32(w, 0x20014) == v) return 0;
    SU32(w, 0x20014, v);
    return 1;
}
RX_FUNCTION(0x00767480, rx_mxworld_set_paused)
