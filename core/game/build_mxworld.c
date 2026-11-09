#include "view_camera.h"
/* build_mxworld.c -- see build_mxworld.h. Hand transliteration of 00765e20, 0074abe0 (with the command registry insert 00817f70 /
 * 00818200 / 008185d0 / 00818530 / 00818580), 0063d2c0 / 0063d930 / 0063e490 / 007909f0 / 0075c530, 00536ca0 and MXWorld::vf1
 * 00765f80 from the disassembly. */
#include "build_mxworld.h"
#include "build_scene_entity.h"
#include "build_activity.h"
#include "build_alloc.h"
#include "build_world.h"
#include "anim.h"
#include "crt.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copy_words(rx_addr dst, rx_addr src, uint32_t n) { for (uint32_t k = 0; k < n; k += 4) SU32(dst, k, U32(src, k)); }
static void zero(rx_addr dst, uint32_t n) { for (uint32_t k = 0; k < n; k++) SU8(dst, k, 0); }
static uint32_t x87_copy(uint32_t b) {   /* fld / fstp dword: the bits, a signalling NaN quieted */
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu) && !(b & 0x00400000u)) b |= 0x00400000u;
    return b;
}
/* msvcr90 _stricmp (C locale) of two game strings */
static int stricmp_gg(rx_addr a, rx_addr b) {
    for (uint32_t k = 0;; k++) {
        int x = tolower(U8(a, k)), y = tolower(U8(b, k));
        if (x != y) return x - y;
        if (!x) return 0;
    }
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * the script command registry [0x00d6925c] (std::map<const char*, fn> ordered by _stricmp; node 0x18 {left, parent, right, key,
 * value, color +0x14 (0 red), isnil +0x15}; the head at [0x00d6925c], the size at [0x00d69260]) */
#define CMD_HEAD 0x00d6925cu
#define CMD_SIZE 0x00d69260u
static int cnil(rx_addr n) { return U8(n, 0x15) != 0; }
static void cmd_lrotate(rx_addr w) {   /* 00818530 (ECX = node) */
    rx_addr p = U32(w, 8);
    SU32(w, 8, U32(p, 0));
    if (!cnil(U32(p, 0))) SU32(U32(p, 0), 4, w);
    SU32(p, 4, U32(w, 4));
    rx_addr head = U32(0, CMD_HEAD);
    if (w == U32(head, 4)) SU32(head, 4, p);
    else if (w == U32(U32(w, 4), 0)) SU32(U32(w, 4), 0, p);
    else SU32(U32(w, 4), 8, p);
    SU32(p, 0, w); SU32(w, 4, p);
}
static void cmd_rrotate(rx_addr w) {   /* 00818580 (ECX = node) */
    rx_addr p = U32(w, 0);
    SU32(w, 0, U32(p, 8));
    if (!cnil(U32(p, 8))) SU32(U32(p, 8), 4, w);
    SU32(p, 4, U32(w, 4));
    rx_addr head = U32(0, CMD_HEAD);
    if (w == U32(head, 4)) SU32(head, 4, p);
    else if (w == U32(U32(w, 4), 8)) SU32(U32(w, 4), 8, p);
    else SU32(U32(w, 4), 0, p);
    SU32(p, 8, w); SU32(w, 4, p);
}
/* 00818200 _Insert(addleft, where, value) (EAX = where): the node 008185d0 (ReflexAlignedAlloc(0x18)) and the red-black fix-up */
static rx_addr cmd_insert_at(int addleft, rx_addr where, rx_addr key, rx_addr value) {
    if (U32(0, CMD_SIZE) >= 0x1ffffffeu) { fprintf(stderr, "rxcore: command map too long\n"); abort(); }
    rx_addr head = U32(0, CMD_HEAD);
    rx_addr n = rx_reflex_aligned_alloc(0x18);
    if (n) { SU32(n, 0, head); SU32(n, 4, where); SU32(n, 8, head); SU32(n, 0xc, key); SU32(n, 0x10, value); SU8(n, 0x14, 0); SU8(n, 0x15, 0); }
    SU32(0, CMD_SIZE, U32(0, CMD_SIZE) + 1);
    if (where == head) { SU32(head, 4, n); SU32(head, 0, n); SU32(head, 8, n); }
    else if (addleft) { SU32(where, 0, n); if (where == U32(head, 0)) SU32(head, 0, n); }
    else { SU32(where, 8, n); if (where == U32(head, 8)) SU32(head, 8, n); }
    for (rx_addr p = n; U8(U32(p, 4), 0x14) == 0;) {
        rx_addr par = U32(p, 4), gp = U32(par, 4);
        if (par == U32(gp, 0)) {
            rx_addr u = U32(gp, 8);
            if (U8(u, 0x14) == 0) { SU8(par, 0x14, 1); SU8(u, 0x14, 1); SU8(U32(U32(p, 4), 4), 0x14, 0); p = U32(U32(p, 4), 4); }
            else {
                if (p == U32(par, 8)) { p = par; cmd_lrotate(p); }
                SU8(U32(p, 4), 0x14, 1); SU8(U32(U32(p, 4), 4), 0x14, 0); cmd_rrotate(U32(U32(p, 4), 4));
            }
        } else {
            rx_addr u = U32(gp, 0);
            if (U8(u, 0x14) == 0) { SU8(par, 0x14, 1); SU8(u, 0x14, 1); SU8(U32(U32(p, 4), 4), 0x14, 0); p = U32(U32(p, 4), 4); }
            else {
                if (p == U32(par, 0)) { p = par; cmd_rrotate(p); }
                SU8(U32(p, 4), 0x14, 1); SU8(U32(U32(p, 4), 4), 0x14, 0); cmd_lrotate(U32(U32(p, 4), 4));
            }
        }
    }
    SU8(U32(U32(0, CMD_HEAD), 4), 0x14, 1);
    return n;
}
/* 008181a0 (a static initializer, run from the CRT's initializer table at 0090fe76: unit 12's startup): the registry's proxy and
 * head node (00706df0) */
void rx_command_registry_init(void) {
    rx_addr p = rx_reflex_aligned_alloc(4);
    if (p) SU32(p, 0, 0x00d69244);
    SU32(0, 0x00d69244, p);
    rx_addr h = rx_reflex_aligned_alloc(0x18);
    SU32(h, 0, 0); SU32(h, 4, 0); SU32(h, 8, 0); SU8(h, 0x14, 1); SU8(h, 0x15, 0);
    SU32(0, CMD_HEAD, h);
    SU8(h, 0x15, 1);
    SU32(U32(0, CMD_HEAD), 4, U32(0, CMD_HEAD)); SU32(U32(0, CMD_HEAD), 0, U32(0, CMD_HEAD)); SU32(U32(0, CMD_HEAD), 8, U32(0, CMD_HEAD));
    SU32(0, CMD_SIZE, 0);
}

/* 00817f70 insert(pair) (unique): descent by _stricmp(key, node) < 0, then the predecessor test (006909c0 decrement) */
static void cmd_insert(rx_addr key, rx_addr value) {
    rx_addr head = U32(0, CMD_HEAD);
    if (!head) { fprintf(stderr, "rxcore: the script command registry [0x00d6925c] is not built (startup)\n"); abort(); }
    rx_addr t = U32(head, 4), where = head;
    int left = 1;
    while (!cnil(t)) { where = t; left = stricmp_gg(key, U32(t, 0xc)) < 0; t = left ? U32(t, 0) : U32(t, 8); }
    rx_addr it = where;
    if (left) {
        if (it == U32(head, 0)) { cmd_insert_at(1, where, key, value); return; }
        if (cnil(it)) it = U32(it, 8);
        else if (!cnil(U32(it, 0))) { it = U32(it, 0); while (!cnil(U32(it, 8))) it = U32(it, 8); }
        else { rx_addr p; while (!cnil(p = U32(it, 4)) && it == U32(p, 0)) it = p; if (!cnil(it)) it = p; }
    }
    if (stricmp_gg(U32(it, 0xc), key) < 0) cmd_insert_at(left, where, key, value);
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * the MXWorld's parts */

/* 0074abe0 */
rx_addr rx_camera_director_construct(rx_addr d) {
    cmd_insert(0x0096cc5c /* "PlayScenario" */, 0x0074c790);
    SU32(0, 0x00a7f74c, d);
    static const float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    for (int k = 0; k < 16; k++) SF32(d, 4u * (uint32_t)k, m[k]);
    SU32(d, 0x290, 0); SU32(d, 0x172c, 0);
    zero(d + 0x294, 0x4b0);
    SU32(d, 0x744, 0);
    zero(d + 0x748, 0xfa0);
    SU32(d, 0x16e8, 0);
    zero(d + 0x16ec, 0x40);
    zero(d + 0x50, 0x240);
    SU32(d, 0x1798, 0); SU32(d, 0x1794, 0); SU32(d, 0x179c, 0); SU32(d, 0x40, 0); SU32(d, 0x44, 0);
    SU32(0, 0x00a7f748, 0);
    return d;
}

/* 007909f0 (EAX = node): the camera node's identity frame and lens defaults */
static void camera_node_construct(rx_addr n) {
    static const uint16_t z[] = {0x10, 0x14, 0x18, 0x24, 0x28, 0x30, 0x38, 0x40, 0x44};
    for (unsigned k = 0; k < sizeof z / sizeof *z; k++) SF32(n, z[k], 0.0f);
    SU32(n, 0, 0x0095a2d8);
    SF32(n, 0x20, 1.0f); SF32(n, 0x34, 1.0f); SF32(n, 0x48, 1.0f);
    static const uint16_t src[] = {0x10, 0x14, 0x18, 0x20, 0x24, 0x28, 0x30, 0x34, 0x38, 0x40, 0x44, 0x48};
    for (unsigned k = 0; k < sizeof src / sizeof *src; k++) SU32(n, 0x190u + src[k], x87_copy(U32(n, src[k])));
    SU32(n, 0x50, 0x3f490fdbu /* 0x9776e0: pi / 4 */); SU32(n, 0x54, 0x3faaaaabu /* 0x9776dc: 4 / 3 */); SF32(n, 0x58, 2.0f /* 0x92e330 */);
    SF32(n, 0x5c, 15000.0f /* 0x977564 */); SF32(n, 0x60, 10.0f /* 0x92fd88 */); SF32(n, 0x64, 7.5f /* 0x97745c */);
    SU32(n, 0x68, 1);
    SU32(n, 0xd4, 0); SU32(n, 0xd0, 0); SU32(n, 0xdc, 0); SU32(n, 0xd8, 0);
}
/* Typed reuse of the existing complete007909f0 kernel; no duplicate math. */
rx_addr rx_view_camera_construct(rx_addr camera) { camera_node_construct(camera); return camera; }
/* Original XenonCore::vf10 00843aa0: own a0x1e0 Camera, tail-call its ctor. */
rx_addr rx_view_camera_create(void) { rx_addr camera=rx_reflex_aligned_alloc(0x1e0); return camera?rx_view_camera_construct(camera):0; }

/* the Entity base (the inline constructor of 0063d930 / 0063e490) */
static void entity_base(rx_addr e) {
    SU32(e, 0, 0x0095a2e4); SU32(e, 0x18, 0); SU32(e, 0x10, 0);
    SF32(e, 0x24, 0.0f); SF32(e, 0x20, 0.0f); SU32(e, 0x70, 0);
    copy_words(e + 0x30, 0x009cb510, 0x40);
    SU32(e, 0x1c, 0); SU32(e, 0x7c, 0); SF32(e, 0x78, 0.0f); SU32(e, 0x74, 0); SU32(e, 0x28, 0); SU32(e, 0x2c, 0);
}
/* 0075c530: the current MXWorld by the game state */
rx_addr rx_mxworld_current(void) {
    rx_addr g = U32(0, 0x00a946b8);
    switch (U32(g, 0x18)) {
    case 1: return U32(g, 0x20c8);
    case 2: case 3: {
        rx_addr core = U32(0, 0x00a95844);
        if (U32(U32(core, 0x20), 0x34) == 1) return U32(g, 0x5a54);
        if (U32(U32(core, 0x1c), 0x34) == 1) return U32(g, 0x20c8);
        return 0;
    }
    default: return U32(g, 0x5a54);
    }
}
/* 0063d930 (stdcall camera): the Camera entity base: the node at +0x90, the name string at +0x2d8, the camera list */
static void camera_entity_construct(rx_addr c) {
    entity_base(c);
    SU32(c, 0, 0x0095a35c);
    camera_node_construct(c + 0x90);
    SU8(c, 0x2b0, 0);
    SF32(c, 0x2c0, 0.0f); SF32(c, 0x2c4, 0.0f); SF32(c, 0x2c8, 0.0f);
    SU32(c, 0x2d0, 0); SU32(c, 0x2d4, 0);
    crt_str_ctor_cstr(c + 0x2d8, 0x0093e6d0);
    SU32(c, 0x2f8, 0); SU8(c, 0x2fc, 0); SU32(c, 0x2f4, 0);
    rx_addr last = U32(0, 0x00a94188);
    if (!last) { SU32(0, 0x00a94184, c); SU32(0, 0x00a94188, c); SU32(c, 0x2d4, 0); SU32(c, 0x2d0, 0); return; }
    SU32(last, 0x2d0, c);
    SU32(c, 0x2d4, U32(0, 0x00a94188));
    SU32(0, 0x00a94188, c);
    SU32(c, 0x2d0, 0);
}
/* 0063d930 exported and registered (stdcall camera; RET 4 -> EAX = camera): unit 10's MXGarage builds the first camera of the game */
rx_addr rx_camera_entity_construct(rx_addr c) { camera_entity_construct(c); return c; }
RX_FUNCTION(0x0063d930, rx_camera_entity_construct)
/* 0063e490 (stdcall entity): the camera target entity, linked into the current world's +0x21de8 / +0x21dec list */
void rx_camera_target_construct(rx_addr t) {
    entity_base(t);
    SU32(t, 0, 0x0095a3f4);
    SU32(t, 0x90, 0); SU32(t, 0x94, 0); SF32(t, 0x120, 0.0f);
    zero(t + 0x9c, 0x40);
    zero(t + 0xdc, 0x40);
    rx_addr w = rx_mxworld_current();
    SU32(t, 0x98, w);
    if (!w) return;
    rx_addr tail = U32(w, 0x21dec);
    if (!tail) { SU32(w, 0x21de8, t); SU32(U32(t, 0x98), 0x21dec, t); SU32(t, 0x94, 0); SU32(t, 0x90, 0); return; }
    SU32(tail, 0x90, t);
    SU32(t, 0x94, U32(U32(t, 0x98), 0x21dec));
    SU32(U32(t, 0x98), 0x21dec, t);
    SU32(t, 0x90, 0);
}
/* 0063d2c0 */
rx_addr rx_world_camera_construct(rx_addr c) {
    camera_entity_construct(c);
    SU32(c, 0, 0x0095a46c);
    rx_camera_target_construct(c + 0x310);
    SF32(c, 0x448, 1.0f); SF32(c, 0x44c, 0.0f); SU32(c, 0x450, 0x3d072b02u /* 0x977818: 0.033 */);
    SU32(c, 0x444, 0); SU32(c, 0x4a0, 0);
    SU32(c, 0x440, 1); SU32(c, 0x70, 1);
    static const float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    for (int k = 0; k < 16; k++) SF32(c, 0x460 + 4u * (uint32_t)k, m[k]);
    return c;
}

/* 00536ca0 (ESI = block): [0x009e3104] guards an InitializeCriticalSection(0x00ae1cf0) (OS object, not built), fields, 0x1e0 zero */
static void track_block_construct(rx_addr b) {
    if (!U32(0, 0x009e3104)) SU32(0, 0x009e3104, 1);
    SU32(b, 0x320, 0); SU32(b, 8, 0xf); SU32(b, 0x2f4, 0); SU32(b, 0x2f8, 0); SU32(b, 0x2fc, 0); SU32(b, 4, 0); SU32(b, 0, 0);
    zero(b + 0x10, 0x1e0);
}

/* 00765e20 */
rx_addr rx_mxworld_construct(rx_addr w, rx_addr config) {
    SU32(w, 0, 0x0096dd34);   /* MXWorld::vftable */
    rx_camera_director_construct(w + 0x20150);
    rx_world_camera_construct(w + 0x218f0);
    rx_addr a = rx_reflex_aligned_alloc(rx_mul_sat(10, 4));
    SU32(w, 0x21dac, 10); SU32(w, 0x21db4, 10); SU32(w, 0x21da4, a); SU32(w, 0x21da8, 0); SU32(w, 0x21db0, 0);
    SU32(w, 0x21dd8, 0); SU32(w, 0x21ddc, 0); SU32(w, 0x21de8, 0); SU32(w, 0x21dec, 0); SU32(w, 0x21df4, 0); SU32(w, 0x21df8, 0);
    track_block_construct(w + 0x21e10);
    SU32(w, 0x22144, 0); SU32(w, 0x22148, 1); SU32(w, 0x20018, 0);
    SU32(w, 0x21dc4, 0); SU32(w, 0x21dc8, 0); SU32(w, 0x21dcc, 0); SU32(w, 0x21dd0, 0); SU32(w, 0x21dd4, 0);
    SU8(w, 0x21de5, 0); SU32(w, 0x21de0, 0); SU32(w, 0x20138, 0); SU32(w, 0x20134, 0);
    SU8(0, 0x00a9434f, 0);
    SU32(w, 0x22140, 0);
    SU32(w, 0x10, U32(config, 0x20a4));
    return w;
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * MXWorld::vf1 */

/* a call of another unit's function: the core's when registered, else the original (oracle; fatal in the product) */
static uint32_t ext0(rx_addr fn) {
    uint32_t (*f)(void) = RX_FN(fn, uint32_t (*)(void));
    return f ? f() : rx_call_original(fn, RX_CDECL, 0, 0, NULL);
}
/* 007d2920 (EAX = scene; stack: type, out*, max, start): the scene's entities of a type (the scene unit's query) */
static int32_t scene_query(rx_addr scene, uint32_t type, rx_addr out) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 20u);
    SU32(sp0 - 20u, 0, 0); SU32(sp0 - 16u, 0, type); SU32(sp0 - 12u, 0, out); SU32(sp0 - 8u, 0, 1); SU32(sp0 - 4u, 0, 0);
    const int32_t n = (int32_t)rx_scene_manager_collect_typed(scene, type, out, 1, 0);
    rx_set_sp(sp0);
    return n;
}

/* vf1 00765f80 */
void rx_mxworld_start(rx_addr w, rx_addr scene, rx_addr name, rx_addr phys, rx_addr table) {
    (void)table;
    SU32(w, 0x20138, scene);
    SU32(w, 0x20134, phys);
    RX_FRAME_ENTER(F, 4);
    SU32(F, 0, U32(0, 0x00d94ad0));                           /* the audio collision manager (audio, not core) */
#ifndef RX_ORACLE
    /* PRODUCT BOUNDARY (audio): the audio collision manager at [0xd94ad0] is created by the unported audio session; in the
     * product it is absent, so its contact-handler registration goes with the same boundary (a null entry would be called by
     * the world's post-listeners). The oracle registers the original's manager. */
    if (U32(F, 0) != 0)
#endif
    rx_vec_push_u32(phys + 0x1928, U32(F, 0));                /* 0080cdf0 */
    SU32(F, 0, ext0(0x0047bb40));                             /* the VehicleDamageBehaviorManager singleton (unit vehicle) */
    rx_vec_push_u32(U32(w, 0x20134) + 0x1928, U32(F, 0));
    RX_FRAME_LEAVE(F, 4);
    for (uint32_t k = 0;; k++) { uint8_t c = U8(name, k); SU8(w + 0x20030, k, c); if (!c) break; }
    SF64(w, 0x20020, 0.0);
    SU32(w, 0x21dc0, 1); SU32(w, 0x20014, 0); SU32(w, 0x21df0, 0); SU32(w, 0x21de0, 0); SU8(w, 0x21de4, 0); SU32(w, 0x21dfc, 0);
    SF32(w, 0x21e00, 100000000.0f /* 0x977808 */);
    SU32(w, 0x21960, 0);
    {   /* FUN_0063dae0 (EAX = the camera entity, ESI = scene): the scene adds it (the scene unit's 0080ddf0 / 007d2f70) */
        rx_camera_entity_add_to_scene(w + 0x218f0, scene);   /* 0063dae0: build_scene_entity.c */
    }
    SF32(w, 0x219d8, 0.5f /* 0x9322cc */);
    SU32(w, 0x21a54, 0); SU32(w, 0x21a5c, 0); SU32(w, 0x21a58, 0);
    SU32(w, 0x21e10, w);
    rx_track_load(w + 0x20030, w + 0x21e10, 8, 0);   /* FUN_00536ef0 (.tdf07 loader; unit 11) */
    RX_FRAME_ENTER(G, 4);
    /* FUN_00767b40 (ESI = world): the entity of type 0x107a */
    SU32(G, 0, 0);
    if (scene_query(U32(w, 0x20138), 0x107a, G) >= 1) { SU32(w, 0x22144, U32(U32(G, 0), 0x90)); SU32(w, 0x22148, U32(U32(G, 0), 0x94)); }
    else { SU32(w, 0x22144, 0); SU32(w, 0x22148, 1); }
    SU32(G, 0, 0);
    int32_t n = scene_query(U32(w, 0x20138), 0x1019, G);
    SU32(w, 0x22140, n < 1 ? 0 : U32(G, 0));
    SU32(G, 0, 0);
    if (scene_query(U32(w, 0x20138), 0x104d, G) >= 1) SU32(w, 0x2214c, U32(G, 0));
    else SU32(w, 0x2214c, 0x009cb2f0);   /* the default mud manager */
    RX_FRAME_LEAVE(G, 4);
    SU32(w, 0x2013c, 0xffffffff);
    if (U32(0, 0x009c47bc) != 0xffffffff) SU32(0, 0x009c47c0, 0xffffffff);
    ext0(0x00674d90);
}
