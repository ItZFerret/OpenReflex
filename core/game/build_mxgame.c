/* build_mxgame.c -- unit 10: the MXGame object (build_mxgame.h): FUN_00755f20, the constructor FUN_00756760 and the members it
 * builds (world configs 00755ff0, the player slots 00552890, the gear list 0053d210, MXGarage 00761330, the MusicPlayer 00675890 /
 * FMODEvent 0080a540, MXHighlightTracking 006d78c0, the "SceneLoading" channel), and the save-game object [0x00d92f10] (00752fd0).
 * Each function is transliterated from its disassembly; the calls keep the original's stack depth (build_mxgame_int.h). */
#include "build_mxgame_int.h"
#include "build_activity.h"
#include "build_mxworld.h"
#include "build_vehicle.h"
#include "build_tables.h"
#include "build_rider.h"
#include "world_misc.h"
#include "world_veh.h"
#include "build_globals_int.h"

/* ---- Quazal allocations (the network library's MemoryManager.cpp) ----
 * 00883ed0 (cdecl manager, size): a block of size rounded up to 4 plus a 4-byte header: through the allocator hook [0x009df5b8]
 * (FUN_00401f90 = ReflexAlignedAlloc once the game init sets it) or msvcr90 malloc; the manager's lock (vf1) / unlock (vf2) around the
 * header store; the header [0x009df5bc], the payload at block + [0x009df5c0] (4). The manager [0x009df5c8] is the static initializer
 * 00910c00's (a Quazal static the globals unit does not build: the product has none, which changes nothing here: the manager only
 * locks). */
static void quazal_manager_lock(rx_addr m, uint32_t slot) {
    /* Quazal::MemoryManager vf1 00883e20 / vf2 00883e40: unless [0x009df580], Enter / LeaveCriticalSection of the mutex +0x18 (00883ac0 /
     * 00883ad0: [mutex] is the critical section) */
    if (U8(0, 0x009df580)) return;
    const rx_addr cs = U32(U32(m, 0x18), 0);
    if (slot == 1) mx_cs_enter(cs); else mx_cs_leave(cs);
}
rx_addr rx_quazal_alloc(rx_addr manager, uint32_t size) {
    MX_FRAME(F, 0, 3);
    const uint32_t n = ((0u - size) & 3u) + size + 4u;
    rx_addr b;
    if (U32(0, 0x009df5b8) != 0) {
        if (U32(0, 0x009df5b8) != 0x00401f90u) mx_unreached(0x00883eed, "Quazal allocator hook other than 00401f90");
        MX_CALLR(b, 1, rx_reflex_aligned_alloc(n));
    } else {
        MX_CALLR(b, 1, rx_crt_malloc(n));
    }
    if (manager) quazal_manager_lock(manager, 1);
    if (U32(0, 0x009df5c0) == 0) SU32(0, 0x009df5c0, 4);
    const rx_addr r = U32(0, 0x009df5c0) + b;
    SU32(b, 0, U32(0, 0x009df5bc));
    if (manager) quazal_manager_lock(manager, 2);
    MX_END(F);
    return r;
}
/* 00883ae0 (ECX = mutex): a critical section in a Quazal block (0x18 bytes, MemoryManager.cpp line 0x4c), +0 its address */
rx_addr rx_quazal_mutex_construct(rx_addr m) {
    MX_FRAME(F, 0, 1);
    rx_addr cs;
    rx_set_sp(F - 0x10);                                        /* push 8; push 0x4c; push 0x917690; push 0x18 (00883e60's unused args) */
    /* 00883e60: the default manager, created by the static initializer 00910c00 (slot 3 of _initterm). That initializer is not
     * translated (unit 12b's Quazal statics): the product has no manager, so the allocation runs without its lock (the same block). */
    const rx_addr mgr = U32(0, 0x009df5c8);
    MX_CALLR(cs, 1, rx_quazal_alloc(mgr, 0x18));              /* push eax (the manager); the size is the pushed 0x18 */
    rx_set_sp(F);
    SU32(m, 0, cs);
    mx_cs_init(cs);
    MX_END(F);
    return m;
}
/* not registered: the Quazal static initializers call 00883ae0 before the default manager exists (00883e60 then creates it:
 * MemoryManager 00883da0 and its profiling timer 008a03c0, network library internals the core does not build) */

/* 00552890 (ESI = the player slots, MXGame + 0x7460; 12 x 0x12a0 constructed later): the slot table's trailer: the Quazal mutex at
 * +0xdfc8 (00883d40: 00883ae0, then +0xdfcc {0, 0x40000000, 0, 0} 008a0220), the counters, the 12 slot indices -1 (+0xdf8c ..) */
/* 00883d40 (ECX = object; flag; RET 4): the mutex (00883ae0), then +4 {0, flag, 0, 0} (008a0220) */
static void quazal_locked_construct(rx_addr o, uint32_t flag) {
    MX_FRAME(F, 0, 1);
    MX_CALL(0, rx_quazal_mutex_construct(o));
    SU8(o, 4, 0); SU32(o, 8, flag); SU32(o, 0xc, 0); SU32(o, 0x10, 0);
    MX_END(F);
}
rx_addr rx_player_array_construct(rx_addr a) {
    MX_CALL(1, quazal_locked_construct(a + 0xdfc8, 0x40000000u));
    SU32(a, 0xe00c, 0); SU32(a, 0xe040, 0);
    SU32(a, 0xdf80, 0); SU32(a, 0xdf84, 0); SU32(a, 0xdf88, 0);
    SU8(a, 0xdfbc, 0); SU32(a, 0xdfc0, 0); SU32(a, 0xdfc4, 0); SU32(a, 0xe044, 0);
    for (uint32_t i = 0; i < 12; i++) SU32(a, 0xdf8c + 4 * i, 0xffffffffu);
    return a;
}
RX_FUNCTION(0x00552890, rx_player_array_construct)

/* 00755ff0 (EDI = config): a world config: the subscene name table +0x396c (00843480), the scene name "" +0, the path "Database\\"
 * +0x104 (strncpy 0x103, terminated), +0x2094 / +0x20a4 0, +0x208 / +0x30c "" */
rx_addr rx_world_config_construct(rx_addr c) {
    MX_FRAME(F, 0, 2);
    MX_CALL(0, rx_strtab_construct(c + 0x396c));
    SU8(c, 0, 0);
    crt_strncpy(c + 0x104, 0x0093e434 /* "Database\\" */, 0x103);
    SU8(c, 0x104 + 0x103, 0);
    SU32(c, 0x2094, 0); SU32(c, 0x20a4, 0);
    SU8(c, 0x208, 0); SU8(c, 0x30c, 0);
    MX_END(F);
    return c;
}
RX_FUNCTION(0x00755ff0, rx_world_config_construct)

/* 0053d210 (ESI = the gear list, MXGame + 0x15504): four pointer lists {data, count, capacity, grow, max}: +0 (0x60, grow 10), +0x14
 * (0x18, 0x18), +0x28 (0xc, 0xc), +0x3c (0xc, 0xc) */
rx_addr rx_gear_list_construct(rx_addr l) {
    MX_FRAME(F, 0, 2);
    rx_addr p;
    MX_CALLR(p, 1, rx_reflex_aligned_alloc(mx_size4(0x60)));
    SU32(l, 0, p); SU32(l, 4, 0); SU32(l, 8, 0x60); SU32(l, 0xc, 0xa); SU32(l, 0x10, 0x7fffffffu);
    rx_set_sp(F - 4); MX_CALLR(p, 1, rx_reflex_aligned_alloc(mx_size4(0x18)));   /* the cdecl argument of the first call stays pushed */
    SU32(l, 0x1c, 0x18); SU32(l, 0x24, 0x18); SU32(l, 0x14, p); SU32(l, 0x18, 0); SU32(l, 0x20, 0);
    rx_set_sp(F - 8); MX_CALLR(p, 1, rx_reflex_aligned_alloc(mx_size4(0xc)));
    SU32(l, 0x28, p); SU32(l, 0x2c, 0); SU32(l, 0x30, 0xc); SU32(l, 0x34, 0); SU32(l, 0x38, 0xc);
    rx_set_sp(F - 0xc); MX_CALLR(p, 1, rx_reflex_aligned_alloc(mx_size4(0xc)));
    SU32(l, 0x40, 0); SU32(l, 0x48, 0); SU32(l, 0x3c, p); SU32(l, 0x44, 0xc); SU32(l, 0x4c, 0xc);
    MX_END(F);
    return l;
}
RX_FUNCTION(0x0053d210, rx_gear_list_construct)

/* 00655d00 (ESI = camera): MXUIPartsViewCamera (vtable 0x0095a6dc) on the Camera base 0063d930: the orbit / zoom limits and the view
 * defaults (the constants are .rdata / .data words, read from the image) */
static void parts_view_camera_construct(rx_addr c) {
    MX_FRAME_ALIGN(F, 16, 0, 1);
    MX_CALL(1, rx_camera_entity_construct(c));
    SU32(c, 0, 0x0095a6dc);
    const uint32_t z = 0, k3f0 = U32(0, 0x00935890), kneg10 = U32(0, 0x009d78b8);
    SU32(c, 0x3fc, z); SU32(c, 0x404, z); SU32(c, 0x408, z); SU32(c, 0x40c, z); SU32(c, 0x410, z);
    SU32(c, 0x3d8, U32(0, 0x0097796c));
    SU32(c, 0x3dc, k3f0); SU32(c, 0x3f0, k3f0); SU32(c, 0x418, k3f0);
    SU32(c, 0x3ec, z); SU32(c, 0x414, z); SU32(c, 0x3f4, z); SU32(c, 0x3f8, z); SU32(c, 0x400, z);
    SU32(c, 0x3e0, 0); SU32(c, 0x3e4, 0); SU32(c, 0x3e8, 0);
    SU32(c, 0x3fc, rx_x87_load_f32(U32(c, 0x400)));
    SU32(c, 0x404, z); SU32(c, 0x40c, z);
    SU32(c, 0x408, rx_x87_load_f32(U32(c, 0x40c)));
    SU32(c, 0x410, z);
    const uint32_t k300 = U32(0, 0x00935d28);
    SU32(c, 0x424, k300); SU32(c, 0x428, k300); SU32(c, 0x42c, k300);
    SU32(c, 0x430, U32(0, 0x00977354));
    SU32(c, 0x438, U32(0, 0x0092fd88)); SU32(c, 0x440, U32(0, 0x0092fd88));
    SU32(c, 0x41c, 1);
    SU32(c, 0x434, z); SU32(c, 0x43c, kneg10); SU32(c, 0x444, kneg10);
    static const uint16_t zeros[] = {0x350, 0x354, 0x358, 0x370, 0x374, 0x378};
    for (unsigned i = 0; i < sizeof zeros / sizeof *zeros; i++) SU32(c, zeros[i], z);
    const uint32_t k5 = U32(0, 0x009322d0);
    static const uint16_t zeros2[] = {0x380, 0x384, 0x388, 0x360, 0x364, 0x368, 0x390, 0x394, 0x398, 0x3a0, 0x3a4, 0x3a8, 0x470, 0x474, 0x478};
    for (unsigned i = 0; i < sizeof zeros2 / sizeof *zeros2; i++) SU32(c, zeros2[i], z);
    SU32(c, 0x448, k3f0);
    const uint32_t one = U32(0, 0x00931f24);
    SU32(c, 0x3d4, k5); SU32(c, 0x44c, z); SU32(c, 0x450, 0);
    static const uint16_t row[] = {0x314, 0x318, 0x31c, 0x320, 0x328, 0x32c, 0x330, 0x334, 0x33c, 0x340, 0x344, 0x348};
    SU32(c, 0x310, one);
    for (unsigned i = 0; i < 4; i++) SU32(c, row[i], z);
    SU32(c, 0x324, one);
    for (unsigned i = 4; i < sizeof row / sizeof *row; i++) SU32(c, row[i], z);
    SU32(c, 0x338, one); SU32(c, 0x34c, one);
    SU32(c, 0x420, k5);
    SU32(c, 0x464, z); SU32(c, 0x468, z); SU32(c, 0x460, U32(0, 0x009cd9e8));
    SU32(c, 0x48c, z); SU32(c, 0x490, z);
    const uint32_t half = U32(0, 0x009322cc);
    SU32(c, 0x488, one); SU32(c, 0x494, one);
    SU32(c, 0x3c0, half); SU32(c, 0x3c4, half); SU32(c, 0x3c8, half); SU32(c, 0x3cc, half); SU32(c, 0x480, half); SU32(c, 0x484, half);
    MX_END(F);
}

/* MXGarage_Construct 00761330 (stdcall garage; RET 4): the parts-view camera +0x10, the state fields, the "MXGarage" channel (+0) with
 * its two event types VehicleLoaded (00763360) and RiderLoaded (00763480), the view defaults */
rx_addr rx_garage_construct(rx_addr g) {
    MX_FRAME(F, 0, 5);
    MX_CALL(0, parts_view_camera_construct(g + 0x10));
    static const uint16_t z1[] = {0x4d8, 0x508, 0x50c, 0x524, 0x520, 0x528, 0x52c, 0x538};
    for (unsigned i = 0; i < sizeof z1 / sizeof *z1; i++) SU32(g, z1[i], 0);
    SU32(g, 0x530, 0xffffffffu); SU32(g, 0x534, 0xffffffffu); SU32(g, 0x54c, 0xffffffffu);
    SU32(g, 0x540, 0); SU32(g, 0x53c, 0); SU32(g, 0x550, 0xffffffffu); SU32(g, 0x544, 0); SU32(g, 0x548, 0);
    SU32(g, 0x554, 1);
    rx_addr reg;
    MX_CALLR(reg, 0, rx_event_channel_registry());
    SU32(F, 0x10, reg);
    rx_addr ch;
    MX_CALLR(ch, 1, rx_reflex_aligned_alloc(0x5c));
    SU32(F, 0x18, ch);
    if (ch) {
        MX_CALL(1, rx_event_base_channel_construct(ch, 0));
        SU32(ch, 0, 0x00946258);
        rx_addr p;
        MX_CALLR(p, 1, rx_reflex_aligned_alloc(4));
        if (p) SU32(p, 0, ch + 0x40);
        SU32(ch, 0x40, p);
        rx_addr h;
        MX_CALLR(h, 0, rx_event_list_head_alloc());
        SU32(ch, 0x54, h);
        SU32(ch, 0x58, 0);
    }
    MX_CALL(2, rx_event_channel_register(U32(F, 0x10), 0x00965d78 /* "MXGarage" */, U32(F, 0x18)));
    SU32(g, 0, U32(F, 0x18));
    MX_CALL(0, rx_event_type_register_at(U32(g, 0), 0x0096d868 /* "VehicleLoaded" */, 0x009dd6d0, 1, -136));   /* 00763360 */
    MX_CALL(0, rx_event_type_register_at(U32(g, 0), 0x0096d890 /* "RiderLoaded" */, 0x009dd6f0, 1, -136));     /* 00763480 */
    SF32(g, 0x580, 0.0f);
    SU32(g, 0x588, 0xffffffffu); SU32(g, 0x55c, 0); SU32(g, 0x564, 0); SU32(g, 0x560, 0xffffffffu); SU32(g, 0x57c, 0);
    SU8(g, 0x4c4, 0); SU8(g, 0x4d4, 0);
    SU32(g, 0x56c, 0xffffffffu); SU32(g, 0x570, 0xffffffffu); SU32(g, 0x574, 0xffffffffu); SU32(g, 0x578, 0xffffffffu);
    SU32(g, 0x584, 0); SU32(g, 0x568, 0xffffffffu);
    static const uint16_t z2[] = {0x51c, 0x4dc, 0x4e0, 0x4e4, 0x510, 0x514, 0x518, 0x4c0, 0x4b0};
    for (unsigned i = 0; i < sizeof z2 / sizeof *z2; i++) SU32(g, z2[i], 0);
    SU32(g, 0x558, 1);
    const rx_addr cam = g + 0x10;
    SF32(cam, 0xe8, 1.0f);                                      /* 0x931f24 */
    SU32(cam, 0xec, U32(0, 0x00977474));                         /* 85.0f */
    SU32(cam, 0x164, 0); SU32(cam, 0x16c, 0); SU32(cam, 0x168, 0);
    SU32(g, 0x174, 0); SU32(g, 0x17c, 0); SU32(g, 0x178, 0);
    SU32(g, 0xf0, U32(0, 0x00977490));                           /* 0.4887f (28 degrees) */
    MX_END(F);
    return g;
}
RX_FUNCTION(0x00761330, rx_garage_construct)

/* 0080db30: a std::map head node (0x14: left, parent, right 0; +0x10 colour 1 (black), +0x11 isnil 0) */
static rx_addr map14_head_alloc(void) {
    MX_FRAME_ALIGN(F, 1, 0x18, 4);                              /* push ebp; mov ebp, esp; SEH frame (3 words) + sub 0xc; 3 pushes + the cookie */
    rx_addr n;
    MX_CALLR(n, 1, rx_reflex_aligned_alloc(0x14));
    if (n) SU32(n, 0, 0);
    if (n + 4) SU32(n, 4, 0);
    if (n + 8) SU32(n, 8, 0);
    SU8(n, 0x10, 1); SU8(n, 0x11, 0);
    MX_END(F);
    return n;
}
/* 0080a540 (ESI = event; name, a1, a2; RET 0xc): FMODEvent (vtable 0x009739f4): +0x148 a critical section, +0x160 a std::map (proxy,
 * +0x178 head, +0x17c size), the playback fields; with a name 0080a6f0 (not reached: the MusicPlayer's events are nameless) */
rx_addr rx_fmod_event_construct(rx_addr e, rx_addr name, uint32_t a1, rx_addr a2) {
    MX_FRAME(F, 0, 4);
    (void)a2;
    SU32(e, 0, 0x009739f4); SU32(e, 0x14, 0);
    mx_cs_init(e + 0x148);
    const rx_addr m = e + 0x160;
    rx_addr p;
    MX_CALLR(p, 1, rx_reflex_aligned_alloc(4));
    if (p) SU32(p, 0, m);
    SU32(m, 0, p);
    rx_addr h;
    MX_CALLR(h, 0, map14_head_alloc());
    SU32(m, 0x18, h);
    SU8(h, 0x11, 1);
    SU32(U32(m, 0x18), 4, U32(m, 0x18)); SU32(U32(m, 0x18), 0, U32(m, 0x18)); SU32(U32(m, 0x18), 8, U32(m, 0x18));
    SU32(m, 0x1c, 0);
    SU32(e, 0x118, 0); SU32(e, 0x138, 0); SU32(e, 0x120, 0); SU32(e, 0x124, 0xffffffffu); SU32(e, 0x128, 0xffffffffu);
    SU32(e, 0x180, 0); SU32(e, 4, 0xffffffffu); SU32(e, 8, 0); SU32(e, 0xc, 0);
    for (uint32_t o = 0x184; o <= 0x1a0; o += 4) SU32(e, o, 0);
    SU32(e, 0x130, 0); SF32(e, 0x11c, 1.0f); SU32(e, 0x12c, 0); SU32(e, 0x10, 0);
    if (name) mx_unreached(0x0080a62c, "FMODEvent with a name (0080a6f0)");
    SU8(e, 0x18, 0);
    SU32(e, 0x134, a1);
    SU32(e, 0x144, U32(e, 0x144) & 0xffffffe0u);
    SU32(e, 0x13c, 0); SU32(e, 0x140, 0);
    MX_END(F);
    return e;
}
/* not registered: the audio code constructs named FMODEvents (0080a6f0) everywhere; audio is not core */

/* 00675890 (EDI = player, MXGame + 0x15c6c): the MusicPlayer (vtable 0x0095d140) with three FMODEvents (+0x18, +0x1e0, +0x3a0) */
rx_addr rx_music_player_construct(rx_addr m) {
    MX_FRAME(F, 0, 2);
    SU32(m, 0, 0x0095d140);
    MX_CALL(3, rx_fmod_event_construct(m + 0x18, 0, 0, 0));
    MX_CALL(3, rx_fmod_event_construct(m + 0x1e0, 0, 0, 0));
    MX_CALL(3, rx_fmod_event_construct(m + 0x3a0, 0, 0, 0));
    for (uint32_t o = 0x55c; o <= 0x56c; o += 4) SU32(m, o, 0);
    for (uint32_t o = 0x574; o <= 0x584; o += 4) SU32(m, o, 0);
    SU32(m, 8, 0); SU32(m, 0xc, 0); SU32(m, 0x14, 0); SU32(m, 4, 0);
    SU32(m, 0x1dc, 0x3e);
    MX_END(F);
    return m;
}
RX_FUNCTION(0x00675890, rx_music_player_construct)

/* PCOnlineAchievements_Construct 006d86e0 (EAX = a, 0x34 bytes): three zero words and two Steam callback objects (CCallback: +0
 * vtable, +4 flags, +8 the callback id, +0xc the owner, +0x10 the method): UserStatsReceived (0x0096248c, 006d8750, id 0x44d) at
 * +0xc and UserAchievementStored (0x0096249c, 00679000, id 0x44f) at +0x20, each registered with steam_api's
 * SteamAPI_RegisterCallback (import 0x009166b4; the platform: the oracle calls it, the product has no Steam client and leaves the
 * objects as the reference boot's Steam stand-in does: unchanged) */
static void steam_register(rx_addr cb, uint32_t id) {
#ifdef RX_ORACLE
    if (U8(cb, 4) & 1) { uint32_t a[1] = {cb}; rx_call_original(U32(0, 0x009166c4), RX_CDECL, 0, 1, a); }   /* SteamAPI_UnregisterCallback */
    uint32_t a[2] = {cb, id};
    rx_call_original(U32(0, 0x009166b4), RX_CDECL, 0, 2, a);
#else
    (void)cb; (void)id;
#endif
}
rx_addr rx_online_achievements_construct(rx_addr a) {
    SU32(a, 0, 0); SU32(a, 4, 0); SU32(a, 8, 0);
    rx_addr cb = a + 0xc;
    SU8(cb, 4, 0); SU32(cb, 8, 0); SU32(cb, 0, 0x0096248c); SU32(cb, 0xc, a); SU32(cb, 0x10, 0x006d8750);
    if (a) { SU32(cb, 0xc, a); SU32(cb, 0x10, 0x006d8750); steam_register(cb, 0x44d); }      /* 006d8870 (EDI = a, ESI = cb) */
    cb = a + 0x20;
    SU8(cb, 4, 0); SU32(cb, 8, 0); SU32(cb, 0, 0x0096249c); SU32(cb, 0xc, a); SU32(cb, 0x10, 0x00679000);
    if (a) { SU32(cb, 0xc, a); SU32(cb, 0x10, 0x00679000); steam_register(cb, 0x44f); }      /* 006d88b0 */
    return a;
}
RX_FUNCTION(0x006d86e0, rx_online_achievements_construct)
/* PCOnlineAchievements_RequestStats 006d8780 (stdcall a; RET 4): SteamUser() / SteamUserStats() -> RequestCurrentStats into a + 4.
 * The platform: the oracle calls steam_api (the reference boot's stand-in returns no user: nothing is written); the product has no
 * Steam client. */
static void achievements_request_stats(rx_addr a) {   /* called at the caller's ESP before its argument push */
#ifdef RX_ORACLE
    rx_call_original(0x006d8780, RX_STDCALL, 0, 1, &a);
#else
    (void)a;
#endif
}

/* MXHighlightTracking_Construct 006d78c0 (ESI = h, 200 bytes): vtable 0x0096202c, +4 the achievements object (0x34), its stats
 * requested, +8 .. +0xc4 the 48 highlight progress slots -1 */
rx_addr rx_highlight_tracking_construct(rx_addr h) {
    SU32(h, 0, 0x0096202c);
    const rx_addr E = rx_sp();
    rx_addr a;
    MX_CALLR(a, 1, rx_reflex_aligned_alloc(0x34));
    if (a) MX_CALLR(a, 0, rx_online_achievements_construct(a));
    SU32(h, 4, a);
    rx_set_sp(E - 4); achievements_request_stats(a); rx_set_sp(E);   /* push edi; push eax; call (the oracle's call pushes a) */
    for (uint32_t i = 0; i < 0x30; i++) SU32(h, 8 + 4 * i, 0xffffffffu);
    return h;
}
RX_FUNCTION(0x006d78c0, rx_highlight_tracking_construct)

/* FUN_00756760 (stdcall game; RET 4): the MXGame constructor */
rx_addr rx_mxgame_construct(rx_addr g) {
    MX_FRAME(F, 0, 4);
    MX_CALL(0, rx_game_base_construct(g));                      /* Game::Game 00882080 (ESI = game) */
    SU32(g, 0, 0x0096d7dc);                                     /* MXGame::vftable */
    MX_CALL(0, rx_world_config_construct(g + 0x34));
    MX_CALL(0, rx_world_config_construct(g + 0x39c0));
    MX_CALL(0, rx_player_array_construct(g + 0x7460));
    MX_CALL(0, rx_vehicle_list_construct(g + 0x154b0));
    MX_CALL(0, rx_gear_list_construct(g + 0x15504));
    for (uint32_t o = 0x15554; o <= 0x15568; o += 4) SU32(g, o, 0);   /* the package cache's fields (005436d0 at game init) */
    MX_CALL(1, rx_garage_construct(g + 0x15570));
    MX_CALL(0, rx_music_player_construct(g + 0x15c6c));
    SU32(g, 0x161f4, 0); SU32(g, 0x161f8, 0); SU32(g, 0x161fc, 0);
    SF32(g, 0x15b1c, 0.0f);
    SU8(g, 0x15b68, 0); SU32(g, 0x15b64, 0); SU32(g, 0x15b30, 0xffffffffu);
    SU32(g, 0x7454, 0); SU32(g, 0x745c, 0);
    for (uint32_t o = 0x1c; o <= 0x30; o += 4) SU32(g, o, 0);
    SU8(g, 0x15b20, 0); SU32(g, 0x15b24, 0); SU32(g, 0x15b28, 0); SU32(g, 0x15b2c, 0xffffffffu);
    SU8(g, 0x734c, 0); SU32(g, 0x7450, 0); SU32(g, 0x15b44, 0); SU32(g, 0x15b58, 0); SU32(g, 0x15b00, 0); SU32(g, 0x15b60, 0);
    SU32(g, 0x14, 1);
    SF32(g, 0x15b40, 0.0f);
    MX_CALL(0, rx_tbl_create_schemas());                        /* 00756887 .. 00756936 (unit 12a) */
    if (U32(0, 0x00d92edc) == 0) {
        rx_addr h;
        MX_CALLR(h, 1, rx_reflex_aligned_alloc(200));
        if (h) MX_CALLR(h, 0, rx_highlight_tracking_construct(h));
        SU32(0, 0x00d92edc, h);
    }
    SU32(g, 0x161f4, mx_event_create(1, 0, 0x0096d59c /* "LoadingThreadEvent" */));
    SU32(g, 0x161f8, mx_event_create(1, 0, 0x0096d5b0 /* "LoadingThreadEarlyOutEvent" */));
    /* the "SceneLoading" channel (the registry kept in the argument slot), its void(float) event "Loading Progress", raised once
     * with 0.0f (the payload in the argument slot) */
    rx_addr reg;
    MX_CALLR(reg, 0, rx_event_channel_registry());
    SU32(F_sp, 4, reg);
    rx_addr ch;
    MX_CALLR(ch, 1, rx_reflex_aligned_alloc(0x5c));
    if (ch) {
        MX_CALL(1, rx_event_base_channel_construct(ch, 0));
        SU32(ch, 0, 0x00946258);
        rx_addr p;
        MX_CALLR(p, 1, rx_reflex_aligned_alloc(4));
        if (p) SU32(p, 0, ch + 0x40);
        SU32(ch + 0x40, 0, p);
        rx_addr h;
        MX_CALLR(h, 0, rx_event_list_head_alloc());
        SU32(ch, 0x54, h);
        SU32(ch, 0x58, 0);
    }
    MX_CALL(2, rx_event_channel_register(U32(F_sp, 4), 0x00952900 /* "SceneLoading" */, ch));
    SU32(g, 0x15b10, ch);
    MX_CALL(1, rx_channel_add_float_event(ch, 0x009527f0 /* "Loading Progress" */));
    SF32(F_sp, 4, 0.0f);
    crt_type_info_name(0x009d00dc, 0x009e1a7c);                 /* the signature's name (cached; unused): push node; call (CRT convention) */
    MX_CALL(2, rx_world_event_dispatch(U32(g, 0x15b10), 0x009527f0, F_sp + 4, 4));
    SU32(g, 0x5a58, 0x00ae1b18);
    SU32(g, 0x20cc, 0x00adffe0);
    SU32(g, 0x18, 0);
    MX_CALL(0, rx_stunt_channel_create());
    MX_CALL(0, rx_stunt_scoring_channel_create());
    MX_END(F);
    return g;
}
RX_FUNCTION(0x00756760, rx_mxgame_construct)

/* the save-game object's "Game Loaded" / "Game Save Notify" handlers (00755490 / 00755550: EAX = the StorageNotification channel,
 * stdcall object; RET 4): member EventHandlerProxies (build_mxgame_init.c) */
static void save_on_game_loaded(rx_addr ch, rx_addr obj) {
    rx_mx_handler_register(ch, 0x0094c83c /* "Game Loaded" */, 0x14, 0x0096d470, obj, 0x00754d10, 0x009d24d0, 2);
}
static void save_on_save_notify(rx_addr ch, rx_addr obj) {
    rx_mx_handler_register(ch, 0x0094c7f0 /* "Game Save Notify" */, 0x14, 0x0096d490, obj, 0x00754430, 0x009d24bc, 2);
}

/* 00704d40: a Quazal list head (0x14 block, next = prev = itself) */
static rx_addr quazal_list_head(void) {
    MX_FRAME_ALIGN(F, 1, 0x14, 4);
    rx_set_sp(F - 0x10);                                        /* push 7; push 0; push "Unknown"; push 0x14 (the size) */
    rx_addr n;
    MX_CALLR(n, 1, rx_quazal_alloc(U32(0, 0x009df5c8), 0x14));
    rx_set_sp(F);
    if (n) SU32(n, 0, n);
    if (n + 4) SU32(n + 4, 0, n);
    MX_END(F);
    return n;
}
/* 00702820 (ESI = list): a Quazal std::list (its proxy block {owner}, +0x14 head, +0x18 size) */
static void quazal_list_construct(rx_addr l) {
    rx_set_sp(rx_sp() - 0x10);
    rx_addr p;
    MX_CALLR(p, 1, rx_quazal_alloc(U32(0, 0x009df5c8), 4));
    rx_set_sp(rx_sp() + 0x10);
    if (p) { SU32(p, 0, l); SU32(l, 0, p); }
    else SU32(l, 0, 0);
    rx_addr h;
    MX_CALLR(h, 0, quazal_list_head());
    SU32(l, 0x14, h);
    SU32(l, 0x18, 0);
}

/* 00752fd0 (stdcall s; RET 4): the save-game / records object [0x00d92f10] (0x6920): the two stream objects (FileInputStream vtable
 * 0x00974ca4 at +0x67ac, FileOutputStream 0x00974d00 at +0x68c8), the request list +0x68e4 (00702820), the counters, the 0x44e0 bytes
 * of per-player records zeroed from +0x50 and the 0x89c "unsaved" flags at +0x4530 set; the SavedGameManager (once) and the two
 * handlers on its channel */
rx_addr rx_save_object_construct(rx_addr s) {
    MX_FRAME(F, 0, 4);
    SU32(s, 0x67b8, 0); SU32(s, 0x67bc, 0); SU8(s, 0x67c4, 0); SU32(s, 0x67b0, 0);
    SU32(s, 0x67ac, 0x00974ca4);
    SU32(s, 0x68c8, 0x00974d00); SU32(s, 0x68cc, 0);
    MX_CALL(0, quazal_list_construct(s + 0x68e4));
    SU32(s, 0x6914, 0); SU32(s, 0x68d8, 0); SU32(s, 0x68e0, 0); SU32(s, 0x68dc, 0);
    for (uint32_t o = 0; o <= 0x14; o += 4) SU32(s, o, 0);
    SU32(s, 0x1c, 0); SU32(s, 0x67a8, 4); SU32(s, 0x20, 0);
    SU32(s, 0x67a4, U32(0, 0x009322cc));                         /* 0.5f */
    SU32(s, 0x38, 0); SU32(s, 0x34, 0); SU32(s, 0x30, 0); SU32(s, 0x2c, 0);
    MX_AT(F, 0); crt_memset(s + 0x50, 0, 0x44e0);
    for (uint32_t i = 0; i < 0x89c; i++) SU32(s, 0x4530 + 4 * i, 1);
    for (int k = 0; k < 2; k++) {
        if (!(U32(0, 0x00dcf2c4) & 1)) {
            SU32(0, 0x00dcf2c4, U32(0, 0x00dcf2c4) | 1);
            MX_CALL(0, rx_saved_game_manager_construct());
            MX_AT(F, 0); RX_ATEXIT(0x00912790);
        }
        if (k == 0) MX_CALL(1, save_on_game_loaded(U32(0, 0x00dce628), s));
        else MX_CALL(1, save_on_save_notify(U32(0, 0x00dce628), s));
    }
    MX_END(F);
    return s;
}
RX_FUNCTION(0x00752fd0, rx_save_object_construct)

/* 00755da0: _aligned_malloc(0x16210, 16) under the allocator lock (push esi before the call) */
static rx_addr mxgame_alloc(void) {
    MX_FRAME(F, 0, 0);
    if (U8(0, 0x00da67d8)) mx_cs_enter(0x00d61704);
    rx_set_sp(F - 4);
    const rx_addr p = rx_crt_aligned_malloc(0x16210, 0x10);
    rx_set_sp(F);
    if (U8(0, 0x00da67d8)) mx_cs_leave(0x00d61704);
    MX_END(F);
    return p;
}
/* 00755f20: MXGame at [0x00a946b8] (00755da0: _aligned_malloc(0x16210) under the allocator lock; the constructor), then the
 * save-game object [0x00d92f10] (0x6920) when absent */
rx_addr rx_mxgame_new(void) {
    MX_FRAME(F, 0, 0);
    if (U32(0, 0x00a946b8) == 0) {
        rx_addr g;
        MX_CALLR(g, 0, mxgame_alloc());
        if (g) MX_CALLR(g, 1, rx_mxgame_construct(g));
        SU32(0, 0x00a946b8, g);
        if (U32(0, 0x00d92f10) == 0) {
            rx_addr s;
            MX_CALLR(s, 1, rx_reflex_aligned_alloc(0x6920));
            if (s) MX_CALLR(s, 1, rx_save_object_construct(s));
            SU32(0, 0x00d92f10, s);
        }
    }
    MX_END(F);
    return U32(0, 0x00a946b8);
}
RX_FUNCTION(0x00755f20, rx_mxgame_new)

/* ReflexWinMain's part: MXGame (00755f20) at [0x00d728d8] (the engine's game) and MXGame::vf3 (00756e00: the name "MXGame" into the
 * engine frame object's slot, the first argument &tStack_30.time = a local of ReflexWinMain: nothing persists) */
rx_addr rx_mxgame_create(void) {
    rx_addr g;
    MX_CALLR(g, 0, rx_mxgame_new());
    SU32(0, 0x00d728d8, g);
    return g;
}
