/* build_rider_init.c -- construction unit 7 (riders): RiderAnimation's setup FUN_00619490 (after the constructor: the model and the
 * animation tree, the bone indices, the IK / limb data from RiderPhysics, the property defaults, the vehicle-type event data, the
 * collar / flap wobble records with the game's random numbers, the worker thread slot) and its helpers. Hand transliterations of the
 * disassembly; layouts in build_rider.h. */
#include "build_activity_teardown.h"
#include "build_rider.h"
#include "../src/rx_os.h"
#include "build_rider_util.h"
#include "crt.h"
#include "world_wreck.h"
#include "world_misc.h"
#include "phys_coll_misc.h"
#include "anim.h"
#include "rider_cap.h"

/* ---------------------------------------------------------------------------------------------------------------------------
 * small helpers */

/* 00830160 (ESI = character, s0 name; RET 4): the index of the bone named `name` (_stricmp over the hierarchy +0x240's node names), or -1 */
int32_t rx_rider_bone_index(rx_addr ch, rx_addr name) {
    for (uint32_t k = 0; k < U32(ch, 0x234); k++) {
        rx_addr recs = U32(U32(ch, 0x240), 8);
        rx_addr s = U16(recs, 16 * k + 8) + U32(U32(recs, 16 * k), 0x10);
        if (crt_stricmp(s, name) == 0) return (int32_t)k;
    }
    return -1;
}

/* 0083af10 (EAX = character): the global bone mirror table (0x00d6b5e8: 28 entries of {index, mirrored index, flag}, 16 bytes),
 * filled once (while entry 0 is -1) from the rider hierarchy's names */
void rx_rider_mirror_table_init(rx_addr ch) {
    if ((int32_t)U32(0, 0x00d6b5e8) != -1) return;
    static const struct { uint32_t self, other, flag; } t[] = {
        {0x009581fc, 0, 0},           /* MXRider_Chest */
        {0x009759e0, 0x009759d0, 0},  /* MXRider_LEye / REye */
        {0x0093efb4, 0, 0},           /* MXRider_Head */
        {0x009759f0, 0x0095828c, 0},  /* LArmA / RArmA */
        {0x00975a10, 0x00975a00, 0},  /* LArmB / RArmB */
        {0x00975a30, 0x00975a20, 0},  /* LCollar / RCollar */
        {0x00975a54, 0x00975a40, 0}, {0x00975a7c, 0x00975a68, 0}, {0x00975aa4, 0x00975a90, 0}, {0x00975acc, 0x00975ab8, 0},
        {0x00975af4, 0x00975ae0, 0}, {0x00975b1c, 0x00975b08, 0}, {0x00975b44, 0x00975b30, 0}, {0x00975b6c, 0x00975b58, 0},
        {0x00975b94, 0x00975b80, 0}, {0x00975bbc, 0x00975ba8, 0}, {0x009581c4, 0x00975bd0, 0}, {0x00975bf8, 0x00975be4, 0},
        {0x00975c20, 0x00975c0c, 0}, {0x00975c48, 0x00975c34, 0},   /* LFinger01..14 / RFinger01..14 */
        {0x0095821c, 0x0095822c, 0},  /* LFoot / RFoot */
        {0x0095825c, 0x0095827c, 0},  /* LLegA / RLegA */
        {0x0095824c, 0x0095826c, 0},  /* LLegB / RLegB */
        {0x00975c5c, 0x0095829c, 0},  /* LWrist / RWrist */
        {0x009581ec, 0, 0},           /* Neck */
        {0x0095820c, 0, 1},           /* Root */
        {0x0095823c, 0, 0},           /* SpineA */
        {0x00975c6c, 0, 0},           /* SpineB */
    };
    for (uint32_t k = 0; k < sizeof t / sizeof t[0]; k++) {
        rx_addr e = 0x00d6b5e8 + 16 * k;
        int32_t other = t[k].other ? rx_rider_bone_index(ch, t[k].other) : -1;   /* the original looks the right side up first */
        SU32(e, 0, (uint32_t)rx_rider_bone_index(ch, t[k].self));
        SU32(e, 4, (uint32_t)other);
        SU32(e, 8, t[k].flag);
    }
    SU32(0, 0x00d6b7a0, 0);
}

/* 00851f10 (ECX = set, s0 node; RET 4): erases a std::set<std::string> subtree (right first, then left along the chain), each key's
 * std::string destructor (msvcr90 ~basic_string at [0x9161b4]) and the node freed: the one _Erase of the std::map / set<std::string>
 * nodes, ported in build_activity_teardown.c (rx_channel_map_erase_subtree); entered at the caller's ESP */
void rx_rider_strset_erase(rx_addr set, rx_addr n) {
    rx_channel_map_erase_subtree(set, n);
}
/* an emptied map: erase(root), head links to itself, size 0 */
static void map_clear(rx_addr m) {
    rx_rider_strset_erase(m, U32(U32(m, 0x18), 4));
    SU32(U32(m, 0x18), 4, U32(m, 0x18));
    SU32(m, 0x1c, 0);
    SU32(U32(m, 0x18), 0, U32(m, 0x18));
    SU32(U32(m, 0x18), 8, U32(m, 0x18));
}

/* 00637360 (EAX = checker, ECX = vehicle, s0 rider animation; RET 4): the WreckChecker's owner fields, its preset table (ATV
 * 0x009c4c60, else 0x009c4c00), the MXWorld (the engine scene's +0x34 == 1: MXGame + 0x5a54; else the front end's +0x20c8), and the
 * checker added to the physics world's listeners (+0x1914, 00544cc0) */
void rx_rider_wreck_checker_bind(rx_addr wc, rx_addr veh, rx_addr ra) {
    SU32(wc, 4, ra);
    SU32(wc, 8, veh);
    SU32(wc, 0x10, U32(veh, 0x194) == 1 ? 0x009c4c60 : 0x009c4c00);
    rx_addr eng = U32(0, RR_ENGINE), w;
    if (U32(U32(eng, 0x20), 0x34) == 1) w = U32(U32(0, RR_GAME), 0x5a54);
    else if (U32(U32(eng, 0x1c), 0x34) == 1) w = U32(U32(0, RR_GAME), 0x20c8);
    else w = 0;
    SU32(wc, 0xc, w);
    RX_FRAME_ENTER(F, 4);
    SU32(F, 0, wc);
    rx_world_ptr_vector_push_unique(U32(w, 0x20134) + 0x1914, F);
    RX_FRAME_LEAVE(F, 4);
}

/* 008403b0 (EAX = node set, s0 anim handle, s1 p, s2 record; RET 0xc): an animation's tracks bound to the node set's bones: record
 * +0xcc the set's node count, +4.. the node index of each track (-1 absent), +0 the handle, +0xd0 p */
void rx_rider_bind_anim_tracks(rx_addr ns, rx_addr handle, uint32_t p, rx_addr rec) {
    SU32(rec, 0xcc, U32(ns, 0xc));
    rx_addr anim = U32(handle, 0x44);
    SU32(handle, 0x4c, 1);
    for (int32_t k = 0; k < (int32_t)U32(anim, 0x10); k++) {
        rx_addr r = rx_hierarchy_find_node(ns, U32(anim, 0x14) + 0x20u * (uint32_t)k);
        SU32(rec, 4 + 4u * (uint32_t)k, r ? (uint32_t)((int32_t)(r - U32(ns, 8)) >> 4) : 0xffffffffu);
    }
    SU32(handle, 0x4c, 0);
    SU32(rec, 0, handle);
    SU32(rec, 0xd0, p);
}
RX_FUNCTION(0x008403b0, rx_rider_bind_anim_tracks)

/* Database::vf27 (find or create a handle by name and tag) / vf26 (find) through the engine's Database */
static rx_addr db_vf27(rx_addr name, uint32_t lo, uint32_t hi, uint32_t create) {
    rx_addr db = rr_database();
    uint32_t a[4] = {name, lo, hi, create};
    return rr_vcall(db, 0x6c, 4, a);
}
static rx_addr db_vf26(rx_addr name, uint32_t lo, uint32_t hi) {
    rx_addr db = rr_database();
    uint32_t a[3] = {name, lo, hi};
    return rr_vcall(db, 0x68, 3, a);
}
#define TAG_ANIM_LO 0x6d696e61u   /* "anim" */
#define TAG_PERF_LO 0x66726570u   /* "perf" */
#define TAG_PERF_HI 0x00746164u   /* "dat" */

/* 006b6c80 (EDI = slot, s0 perfdat handle, s1 node set; RET 8): an animation event slot from a performance-data asset: its name, the
 * clip count / list, each clip's anim bound to the vehicle model's nodes (008403b0) and its start time */
void rx_rider_event_slot_bind(rx_addr slot, rx_addr h, rx_addr ns) {
    rx_addr pd = U32(h, 0x44);
    SU32(h, 0x4c, 1);
    SU32(slot, 0, pd);
    rr_zero8(slot + 0xdc0, 0x40);
    rx_addr name = rr_vcall(h, 8, 0, NULL);   /* the handle's name */
    for (uint32_t k = 0;; k++) { uint8_t c = U8(name, k); SU8(slot + 0xdc0, k, c); if (!c) break; }
    rx_addr p = U32(slot, 0);
    SU32(slot, 0xda4, U32(p, 0x10));
    SU32(slot, 0xdac, U32(ns, 0xc));
    int32_t n = (int32_t)U32(p, 0x14);
    SU32(slot, 0xda8, (uint32_t)n);
    for (int32_t k = 0; k < n; k++) {
        rx_addr clip = U32(U32(slot, 0), 0x18) + 0x70u * (uint32_t)k;
        rx_addr a = db_vf27(clip, TAG_ANIM_LO, 0, 1);
        rx_rider_bind_anim_tracks(ns, a, U32(slot, 0xda4), slot + 4 + 0xd8u * (uint32_t)k);
        SF32(slot, 0xd8 + 0xd8u * (uint32_t)k, F32(clip, 0x48) * F32(clip, 0x44) + F32(clip, 0x40));
    }
    float d = F32(U32(slot, 0), 0x24);
    SF32(slot, 0xdb0, d); SF32(slot, 0xdbc, d);
    SU32(h, 0x4c, 0);
    SU32(slot, 0xe00, 1);
}

/* the "<MX|ATV>Vehicle_<suffix>" perfdat name of a clip name (strrchr '_' + 1) into a 0x104 buffer (FUN_0078fd60 = sprintf) */
static void vehicle_perf_name(rx_addr buf, rx_addr clipname, uint32_t vtype) {
    rx_addr us = crt_strchr(clipname, 0x5f) + 1;   /* strchr ([0x916248]) */
    const char* pre = vtype == 0 ? "MX" : vtype == 1 ? "ATV" : "(null)";   /* 0x92fc24 "MX", 0x92fc28 "ATV"; other types: the pointer is 0,
                                                                                * which msvcr90's %s prints as "(null)" */
    char tail[0x104]; rr_gets(us, tail, sizeof tail);
    char s[0x104]; snprintf(s, sizeof s, "%sVehicle_%s", pre, tail);
    for (uint32_t k = 0;; k++) { SU8(buf, k, (uint8_t)s[k]); if (!s[k]) break; }
}

/* 006b7110 (ECX = rider animation, s0 slots (+0xc940), s1 vehicle; RET 8): the vehicle-side event slots of the rider's event lists
 * (+0xaffc: 40-byte names, +0xc454 / +0xc59c: the anim tables' names), each bound to the vehicle model (+0x1328 + 0x10) */
void rx_rider_vehicle_events_init(rx_addr ra, rx_addr s, rx_addr veh) {
    SU32(s, 0x4b4b0, U32(ra, 0xba24));
    SU32(s, 0x4b4b8, U32(ra, 0xc81c));
    SU32(s, 0x4b4b4, U32(ra, 0xc594));
    rr_zero8(s + 0x960, 0x39210);
    rr_zero8(s + 0x3d3b0, 0xe100);
    rr_zero8(s + 0x39b70, 0x3840);
    RX_FRAME_ENTER(F, 0x140);
    rx_addr buf = F + 0x30;   /* the 0x104-byte name buffer of the original's frame */
    for (uint32_t k = 0; k < U32(s, 0x4b4b0); k++) {
        vehicle_perf_name(buf, ra + 0xaffc + 0x28 * k, U32(veh, 0x194));
        if (db_vf26(buf, TAG_PERF_LO, TAG_PERF_HI)) {
            rx_addr h = db_vf27(buf, TAG_PERF_LO, TAG_PERF_HI, 1);
            rx_rider_event_slot_bind(s + 0x960 + 0xe10 * k, h, U32(veh, 0x1328) + 0x10);
        }
    }
    for (uint32_t k = 0; k < U32(s, 0x4b4b4); k++) {
        SU32(F, 0x18, 0);
        SU32(F, 0x28, ra + 0xc454 + 0x28 * k);
        rx_world_string_table_find(U32(ra, 0x144) + 0x248, F + 0x28, F + 0x18);   /* 007fa030: the name's animator */
        vehicle_perf_name(buf, U32(F, 0x18) + 0x9c, U32(veh, 0x194));
        rx_addr h = db_vf27(buf, TAG_PERF_LO, TAG_PERF_HI, 1);
        rx_rider_event_slot_bind(s + 0x39b70 + 0xe10 * k, h, U32(veh, 0x1328) + 0x10);
    }
    for (uint32_t k = 0; k < U32(s, 0x4b4b8); k++) {
        SU32(F, 0x20, 0);
        SU32(F, 0x28, ra + 0xc59c + 0x28 * k);
        rx_world_string_table_find(U32(ra, 0x144) + 0x248, F + 0x28, F + 0x20);
        vehicle_perf_name(buf, U32(F, 0x20) + 0x9c, U32(veh, 0x194));
        if (db_vf26(buf, TAG_PERF_LO, TAG_PERF_HI)) {
            rx_addr h = db_vf27(buf, TAG_PERF_LO, TAG_PERF_HI, 1);
            rx_rider_event_slot_bind(s + 0x3d3b0 + 0xe10 * k, h, U32(veh, 0x1328) + 0x10);
        }
    }
    RX_FRAME_LEAVE(F, 0x140);
    rr_zero8(s, 0x960);
}

/* 0061c6e0 (EAX = vehicle, EDI = rider animation): the lean blend animations of the vehicle type (MX / ATV "<T>Vehicle_RD_LeftRight
 * {Middle,Forward,Back}[Extreme]") bound to the vehicle model's nodes, then the vehicle's three hand / foot node indices (+0x1ac /
 * +0x1b0) and the indices of the animation's tracks named like them */
void rx_rider_lean_anims_init(rx_addr veh, rx_addr ra) {
    static const uint32_t mx[6] = {0x0095855c, 0x00958580, 0x009585a0, 0x009585c8, 0x009585e8, 0x0095860c};
    static const uint32_t atv[6] = {0x00958628, 0x00958650, 0x00958670, 0x00958698, 0x009586b8, 0x009586dc};
    static const uint32_t rec[6] = {0x5ace0, 0x5adb4, 0x5ae88, 0x5af5c, 0x5b030, 0x5b104};
    uint32_t t = U32(veh, 0x194);
    if (t == 0 || t == 1) {
        SU32(ra, 0x5b1f8, 1);
        for (int k = 0; k < 6; k++) {
            rx_addr h = db_vf27(t == 0 ? mx[k] : atv[k], TAG_ANIM_LO, 0, 1);
            rx_rider_bind_anim_tracks(U32(veh, 0x1328) + 0x10, h, 0, ra + rec[k]);
        }
    }
    if (!U32(ra, 0x5b1f8)) return;
    SU32(ra, 0x5b1dc, U32(veh, 0x1b0));
    SU32(ra, 0x5b1e0, U32(veh, 0x1ac));
    SU32(ra, 0x5b1ec, 2);
    SU32(ra, 0x5b1f0, 3);
    rx_addr h0 = U32(ra, 0x5ace0);
    rx_addr anim = U32(h0, 0x44);
    SU32(h0, 0x4c, 1);
    for (int j = 0; j < 2; j++) {
        rx_addr slot = ra + 0x5b1e4 + 4u * (uint32_t)j;
        int32_t n = (int32_t)U32(anim, 0x10);
        rx_addr node = U32(slot - 8, 0);   /* the vehicle node record (+0x1b0 / +0x1ac) */
        rx_addr nm = U16(node, 8) + U32(U32(node, 0), 0x10);
        rx_addr tn = U32(anim, 0x14);
        for (int32_t k = 0; k < n; k++, tn += 0x20)
            if (crt_strcmp(nm, tn) == 0) { SU32(slot, 0, (uint32_t)k); break; }
    }
    SU32(U32(ra, 0x5ace0), 0x4c, 0);
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * the property defaults (inline RangedFloat / ClampedFloat sets of 00619490) */

/* the smoothing step of a ranged float at b (+0 min, +4 max, +8 rate, +0xc dt, +0x10 value): value += min(rate, dt) / rate * (T - value) */
static float smooth_to(rx_addr b, float T) {
    float r = F32(b, 8), x0 = F32(b, 0xc);
    if (!(r > x0)) x0 = r;
    float v = F32(b, 0x10);
    x0 = x0 / r * (T - v) + v;
    SF32(b, 0x10, x0);
    return x0;
}
/* the second clamp form (comiss v, max; jb / comiss min, v; jae): (clamp - min) / (max - min) */
static float norm_b(float v, float lo, float hi) {
    if (!(v < hi)) v = hi;
    float x1 = lo;
    if (!(lo >= v)) x1 = v;
    return (x1 - lo) / (hi - lo);
}
static void set_listeners(rx_addr lst, rx_addr slot, float v) { SF32(slot, 0, v); rx_world_float_listeners_set(lst, slot); }
/* a ranged property (min, max at b, the smoothing towards T, the normalised value to the listeners at b + 0x14) */
static void ranged_set(rx_addr b, float lo, float hi, float T, rx_addr slot) {
    SF32(b, 0, lo); SF32(b, 4, hi);
    float v = smooth_to(b, T);
    set_listeners(b + 0x14, slot, norm_b(v, F32(b, 0), F32(b, 4)));
}
/* a clamped float without listeners: min / max (0 / 1 here), the value clamped to [min, cap] and stored through the pointer at +8 */
static void clamped_out(rx_addr b, float cap) {
    float v = F32(b, 4);
    if (!(cap >= v)) v = cap;
    float lo = F32(b, 0);
    if (!(lo < v)) v = lo;
    rx_addr p = U32(b, 8);
    if (p) SF32(p, 0, v);
}
/* a ranged float without listeners: the smoothing towards T and the clamped value through the pointer at +0x14 */
static void ranged_out(rx_addr b, float T) {
    float v = smooth_to(b, T);
    float hi = F32(b, 4), lo = F32(b, 0);
    if (!(v < hi)) v = hi;
    if (!(lo < v)) v = lo;
    rx_addr p = U32(b, 0x14);
    if (p) SF32(p, 0, v);
}
/* the wrapped float form (0x538 / 0x4c0 ...: min, max at b, value 0 wrapped into [min, max]) to the listeners */
static void wrapped_zero(rx_addr b, rx_addr lst, rx_addr slot) {
    float v = 0.0f, hi = 1.0f;
    SF32(b, 0, 0.0f); SF32(b, 4, hi);
    if (v > hi) v = v - hi;
    else { float lo = F32(b, 0); if (lo > v) v = (v - lo) + hi; }
    set_listeners(lst, slot, v);
}

/* the game's random number (g_GameRandSeed 0x009cb860, count 0x009cb864): seed = seed * 0x41c64e6d + 0x3039, (seed >> 16) & 0x7fff */
static uint32_t game_rand(void) {
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
    uint32_t s = U32(0, 0x009cb860) * 0x41c64e6du + 0x3039u;
    SU32(0, 0x009cb860, s);
    return (s >> 16) & 0x7fff;
}

/* a wobble record {phase, ..} of the collar / flap nodes: the bone, phase = base + offset, the amplitudes */
static void wobble(rx_addr ra, uint32_t at, rx_addr name, uint32_t off_bits, const uint32_t v[6]) {
    SU32(ra, at + 4, (uint32_t)rx_rider_bone_index(U32(ra, 0x144), name));
    if (off_bits) SF32(ra, at, F32(ra, 0x5b300) + rx_bf(off_bits));
    for (int k = 0; k < 6; k++) if (v[k] != 0xffffffffu) SU32(ra, at + 0x10 + 4u * (uint32_t)k, v[k]);
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * the worker slot's OS objects (product: placeholders; the product runs the worker pass in place, anim_scenario_rider.c) */
static uint32_t os_create_event(uint32_t manual) {
#ifdef RX_ORACLE
    uint32_t a[4] = {0, manual, 0, 0};
    uint32_t h = rx_os_import(0x00916038, 4, a);
    if (!h) rx_os_import(0x00916078, 0, NULL);
    return h;
#else
    static uint32_t next = 0x00e48000;   /* placeholder handles (the OS layer, queue item 10) */
    (void)manual; next += 4; return next;
#endif
}
static void os_worker_thread(uint32_t slot, uint32_t cpu) {
    rx_addr th = rx_rc_tab(0x00af6c38, 8, slot);   /* (w-rc-core) slots 12..31 in side storage */
#ifdef RX_ORACLE
    uint32_t a[6] = {0, 0, 0x0061cdb0 /* RiderAnimation_WorkerThread */, slot, 4, th + 4};
    uint32_t h = rx_os_import(0x00916030, 6, a);   /* CreateThread (suspended) */
    SU32(th, 0, h);
    uint32_t b[2] = {h, 1u << (cpu & 0x1f)};
    rx_os_import(0x00916028, 2, b);                /* SetThreadAffinityMask */
    uint32_t c[2] = {U32(th, 0), slot + 1};
    rx_os_import(0x00916024, 2, c);                /* SetThreadPriority */
    uint32_t d = U32(th, 0);
    rx_os_import(0x0091602c, 1, &d);               /* ResumeThread */
    if (U32(th, 0)) { d = U32(th, 0); rx_os_import(0x0091602c, 1, &d); }
#else
    (void)cpu;
    SU32(th, 0, 0x00e4c000 + 4 * slot);   /* placeholder thread handle / id: no thread in the product */
    SU32(th, 4, 0x2000 + slot);
#endif
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * 00619490 (EAX = the hierarchy handle; stdcall ra, scene, tree handle, vehicle; RET 0x10) */
void rx_rider_animation_init(rx_addr hier, rx_addr ra, rx_addr scene, rx_addr tree, rx_addr veh) {
    for (uint32_t o = 0x5a0; o <= 0x5b6; o += 2) if (o != 0x5aa && o != 0x5b2) SU16(ra, o, 0xffff);
    SU32(ra, 0xaf10, 0); SU32(ra, 0xaf28, 0); SU32(ra, 0xaf2c, 0); SU32(ra, 0x5b520, 0); SU32(ra, 0x5b524, 0);
    SU32(ra, 0xaf30, 0); SU32(ra, 0xaf34, 1); SU32(ra, 0xaf38, 0); SU32(ra, 0xaf54, 0); SU32(ra, 0x5b2e8, 0); SU32(ra, 0x5bb54, 0);
    SF32(ra, 0x5b2d8, 0);
    rx_rider_entity_set_model(hier, ra, scene, tree);   /* 006b5de0 (EDI = the hierarchy handle) */
    rx_rider_mirror_table_init(U32(ra, 0x144));
    SU32(ra, 0x2b4, hier);
    SU32(ra, 0x70, 0);
    SU16(ra, 0xb0, U16(ra, 0xb0) | 1);
    rx_rider_wreck_checker_bind(ra + 0x5b54c, veh, ra);
    rx_rider_ik_setup(ra + 0x5d0, U32(ra, 0x144), ra);   /* 004246c0 */
    SU8(ra, 0x5b578, 0);
    map_clear(ra + 0xaf7c);
    map_clear(ra + 0xaf9c);
    map_clear(ra + 0xafbc);
    map_clear(ra + 0xafdc);
    SU32(ra, 0xba24, 0);
    rr_zero8(ra + 0xaffc, 0xa28);
    SU32(ra, 0xc450, 0);
    rr_zero8(ra + 0xba28, 0xa28);
    SU32(ra, 0xc594, 0); SU32(ra, 0xc598, 0);
    rr_zero8(ra + 0xc454, 0x140);
    SU32(ra, 0xc81c, 0); SU32(ra, 0xc820, 0); SU32(ra, 0xc824, 0);
    rr_zero8(ra + 0xc59c, 0x280);
    SU32(ra, 0xc924, 0); SU32(ra, 0xc91c, 0); SU32(ra, 0xc920, 0);
    rr_zero8(ra + 0xc8cc, 0x50);
    SU32(ra, 0xc8c8, 0);
    rr_zero8(ra + 0xc828, 0xa0);
    SU32(ra, 0x5b2f0, 0xffffffffu);
    SU32(ra, 0x5bab8, U32(veh, 0x194));
    SU32(ra, 0x5b540, U32(U32(veh, 0x190), 0x1f0));
    SU32(ra, 0xaf1c, veh);
    SU32(ra, 0xc928, 0); SU32(ra, 0xc92c, 0); SU32(ra, 0xc930, 0); SU32(ra, 0xc934, 0);
    if (U32(ra, 0x5bab8) <= 1) {
        rx_rider_attach_plugs(veh, ra);                    /* 0061ad70 */
        SU32(ra, 0xaf08, 0);
        rx_rider_vehicle_events_init(ra, ra + 0xc940, veh);
        rx_rider_lean_anims_init(veh, ra);
    } else {
        SU32(ra, 0xaf08, 1);
        SU8(ra, 0xaf0c, 1);
        rx_rider_truck_setup(veh, ra);                     /* 0061aa10 (trucks / buggies / UTVs) */
    }
    RX_FRAME_ENTER(F, 0x10);
    rx_addr slot = F + 0xc;
    /* +0x378: min -1 ([0x9cd9e8]), max 1, rate 0.25, the value towards 0 */
    SU32(ra, 0x378, U32(0, 0x009cd9e8));
    SF32(ra, 0x37c, 1.0f);
    SF32(ra, 0x380, 0.25f);
    {
        float r = F32(ra, 0x380), x0 = F32(ra, 0x384);
        if (!(r > x0)) x0 = r;
        float v = F32(ra, 0x388);
        x0 = x0 / r * (0.0f - v) + v;
        SF32(ra, 0x388, x0);
        float hi = F32(ra, 0x37c), lo = F32(ra, 0x378);
        if (!(x0 < hi)) x0 = hi;
        if (!(lo < x0)) x0 = lo;
        set_listeners(ra + 0x38c, slot, (x0 - lo) / (hi - lo));
    }
    if (U32(ra, 0x57c)) SU8(U32(ra, 0x57c), 0, 1);
    if (U32(ra, 0x580)) SU16(U32(ra, 0x580), 0, U16(ra, 0x5a0));
    SF32(ra, 0x590, 0); SF32(ra, 0x594, 1.0f);
    if (U32(ra, 0x564)) SU8(U32(ra, 0x564), 0, 1);
    if (U32(ra, 0x568)) SU16(U32(ra, 0x568), 0, U16(ra, 0x5ac));
    SF32(ra, 0x584, 0); SF32(ra, 0x588, 1.0f);
    wrapped_zero(ra + 0x538, ra + 0x548, slot);
    if (!U32(ra, 0xaf08)) {
        ranged_set(ra + 0x2f4, 0.0f, 1.0f, 0.0f, slot);
        ranged_set(ra + 0x320, 0.0f, 1.0f, 0.0f, slot);
        ranged_set(ra + 0x34c, 0.0f, 1.0f, 1.0f, slot);
        SU32(ra, 0x328, U32(0, 0x009c4754));
        SU32(ra, 0x2fc, U32(0, 0x009c4754));
        SF32(ra, 0x354, 0.25f);
        SU32(F, 0xc, ra + 0x320);
        phys_vector_push_back(ra + 0x2dc, F + 0xc);
        SU32(F, 0xc, ra + 0x2f4);
        phys_vector_push_back(ra + 0x2dc, F + 0xc);
        SU32(ra, 0x2d0, U32(0, 0x009c4754));
        {   /* clamped floats 0 .. 1 with the value 0 (min 0, max 1): the result max(0, min(1, 0)) */
            static const uint32_t at[2] = {0x464, 0x484};
            for (int k = 0; k < 2; k++) {
                float lo = 0.0f, hi = 1.0f;
                SF32(ra, at[k], lo); SF32(ra, at[k] + 4, hi);
                float v = hi; if (!(lo >= hi)) v = lo;   /* comiss 0, max; jae */
                float r = (lo >= v) ? lo : v;            /* comiss 0, v; jb */
                set_listeners(ra + at[k] + 8, slot, r);
            }
        }
        wrapped_zero(ra + 0x4c0, ra + 0x4d0, slot);
        wrapped_zero(ra + 0x4e8, ra + 0x4f8, slot);
        wrapped_zero(ra + 0x510, ra + 0x520, slot);
        SF32(ra, 0x3bc, 0); SF32(ra, 0x3c0, 1.0f);
        SF32(ra, 0x3a4, 0); SF32(ra, 0x3a8, 1.0f);
        SF32(ra, 0x3e0, 0); SF32(ra, 0x3e4, 1.0f);
        SF32(ra, 0x3ec, 0); SF32(ra, 0x3f0, 1.0f);
        SF32(ra, 0x3f8, 0); SF32(ra, 0x3fc, 1.0f);
        SF32(ra, 0x3c8, 0); SF32(ra, 0x3cc, 1.0f);
        SF32(ra, 0x3d4, 0.2f);
        SU32(ra, 0x3d8, U32(0, 0x009c4b4c));
        clamped_out(ra + 0x3bc, 0.0f);
        clamped_out(ra + 0x3c8, 0.0f);
        clamped_out(ra + 0x3d4, 0.5f);
        ranged_out(ra + 0x3a4, 0.5f);
        clamped_out(ra + 0x3e0, 0.5f);
        clamped_out(ra + 0x3ec, 0.5f);
        clamped_out(ra + 0x3f8, 0.5f);
        SU32(ra, 0x3ac, U32(0, 0x009c4754));
        if (U32(ra, 0x5bab8) == 0) {
            static const uint32_t at[4] = {0x404, 0x41c, 0x434, 0x44c};
            for (int k = 0; k < 4; k++) {
                SF32(ra, at[k], 0); SF32(ra, at[k] + 4, 1.0f);
                ranged_out(ra + at[k], 0.5f);
                SF32(ra, at[k] + 8, 0.25f);
            }
        }
        {   /* the rider entry of the game's rider manager (MXGame + 0x15518 / count + 0x1551c) with this rider id */
            rx_addr g = U32(0, RR_GAME), e = 0;
            for (uint32_t k = 0; k < U32(g, 0x1551c); k++) {
                rx_addr c = U32(U32(g, 0x15518), 4 * k);
                if (U32(c, 4) == U32(ra, 0x5b51c)) { e = c; break; }
            }
            rx_addr gear = U32(e, 0x20);
            if (gear) SU32(ra, 0x5b1fc, (int32_t)U32(gear, 0x30) > 0);
        }
        SU32(ra, 0x5b6d4, 0);
        SU32(ra, 0x5b6d8, (uint32_t)rx_rider_bone_index(U32(ra, 0x144), 0x009581c4));   /* "MXRider_LFinger11" */
        {   /* the lens-swipe effect: vf1(the scene's effect set, "Lens_Swipe_Effect") */
            rx_addr fx = U32(ra, 0x5b6dc);
            uint32_t a[2] = {U32(0, 0x00a94264 + 4 * U32(scene, 0x2cc4)), 0x009581d8};
            rr_vcall(fx, 4, 2, a);
        }
        SU32(ra, 0x5b6e0, 0);
    }
    RX_FRAME_LEAVE(F, 0x10);
    SF32(ra, 0xaf20, 0); SF32(ra, 0xaf24, 0);
    rx_addr ch = U32(ra, 0x144);
    SU32(ra, 0xaee4, (uint32_t)rx_rider_bone_index(ch, 0x0093efb4));   /* Head */
    SU32(ra, 0xaee8, (uint32_t)rx_rider_bone_index(ch, 0x009581ec));   /* Neck */
    SU32(ra, 0xaeec, (uint32_t)rx_rider_bone_index(ch, 0x009581fc));   /* Chest */
    SU32(ra, 0xaef0, (uint32_t)rx_rider_bone_index(ch, 0x0095820c));   /* Root */
    SU32(ra, 0xaef4, (uint32_t)rx_rider_bone_index(ch, 0x0095821c));   /* LFoot */
    SU32(ra, 0xaef8, (uint32_t)rx_rider_bone_index(ch, 0x0095822c));   /* RFoot */
    SU32(ra, 0x5b5a0, U32(ra, 0xaee4));
    SU32(ra, 0x5b5a4, U32(ra, 0xaee8));
    SU32(ra, 0x5b5e0, U32(veh, 0x190));
    SU32(ra, 0x5b6f4, (uint32_t)rx_rider_bone_index(ch, 0x0095823c));  /* SpineA */
    SU32(ra, 0x5b6ec, U32(veh, 0x190));
    SU32(ra, 0x5b780, U32(ra, 0xaef4));
    SU32(ra, 0x5b784, (uint32_t)rx_rider_bone_index(ch, 0x0095824c));  /* LLegB */
    SU32(ra, 0x5b788, (uint32_t)rx_rider_bone_index(ch, 0x0095825c));  /* LLegA */
    SU32(ra, 0x5b78c, 2);
    SU32(ra, 0x5b81c, U32(veh, 0x190));
    SU32(ra, 0x5b870, U32(ra, 0xaef8));
    SU32(ra, 0x5b874, (uint32_t)rx_rider_bone_index(ch, 0x0095826c));  /* RLegB */
    SU32(ra, 0x5b878, (uint32_t)rx_rider_bone_index(ch, 0x0095827c));  /* RLegA */
    SU32(ra, 0x5b87c, 3);
    SU32(ra, 0x5b90c, U32(veh, 0x190));
    SU32(ra, 0x5b948, (uint32_t)rx_rider_bone_index(ch, 0x0095828c));  /* RArmA */
    SU32(ra, 0x5b94c, (uint32_t)rx_rider_bone_index(ch, 0x0095829c));  /* RWrist */
    SU32(ra, 0x5b968, U32(veh, 0x190));
    /* the collar / flap wobble records (0x30 each from +0x5b300): the bone, the phase (a random base 0..100 + offset), amplitudes */
    SU32(ra, 0x5b304, (uint32_t)rx_rider_bone_index(ch, 0x009582ac));  /* LCollar_Flap */
    {
        uint32_t r = game_rand();
        float base = (float)((double)(float)(int32_t)r * 0x1p-15 /* [0x9776a0] */) * 100.0f /* [0x93005c] */;
        SF32(ra, 0x5b300, base);
    }
    const uint32_t A = U32(0, 0x009d78c4), B = U32(0, 0x009d3bfc), C = U32(0, 0x009cd960), N = 0xffffffffu;
    const uint32_t f015 = 0x3e19999a, f02 = 0x3e4ccccd, f0 = 0, f011 = 0x3de147ae, f01 = 0x3dcccccd, f025 = 0x3e800000, f03 = 0x3e99999a;
    { const uint32_t v[6] = {f015, f02, A, N, f0, f02}; for (int k = 0; k < 6; k++) if (v[k] != N) SU32(ra, 0x5b310 + 4u * (uint32_t)k, v[k]); SU32(ra, 0x5b328, f015); }
    { const uint32_t v[6] = {f015, f02, A, N, f0, f02}; wobble(ra, 0x5b330, 0x009582c4, 0x3e19999a /* 0.15 */, v); SU32(ra, 0x5b358, f015); }
    { const uint32_t v[6] = {f015, f02, f011, N, f015, f01}; wobble(ra, 0x5b360, 0x009582dc, U32(0, 0x009d78d4), v); SU32(ra, 0x5b388, f01); }
    { const uint32_t v[6] = {f015, f02, f025, N, f015, f01}; wobble(ra, 0x5b390, 0x009582f0, 0x3ebd70a4 /* [0x977ec8] 0.37 */, v); SU32(ra, 0x5b3b8, f01); }
    { const uint32_t v[6] = {B, f02, f025, N, B, f01}; wobble(ra, 0x5b3c0, 0x00958304, 0x3f0ccccd /* [0x9776f0] 0.55 */, v); SU32(ra, 0x5b3e8, f01); }
    { const uint32_t v[6] = {B, f02, f015, N, B, f01}; wobble(ra, 0x5b3f0, 0x00958318, 0x3f1eb852 /* [0x977ec4] 0.62 */, v); SU32(ra, 0x5b418, f01); }
    { const uint32_t v[6] = {f03, f01, C, N, f03, f01}; wobble(ra, 0x5b420, 0x0095832c, 0x3e6147ae /* [0x97746c] 0.22 */, v); SU32(ra, 0x5b448, f02); }
    { const uint32_t v[6] = {f03, f01, C, N, f03, f01}; wobble(ra, 0x5b450, 0x00958340, 0x3eb851ec /* [0x977ec0] 0.36 */, v); SU32(ra, 0x5b478, f02); }
    { const uint32_t v[6] = {f01, f01, f03, N, f01, f01}; wobble(ra, 0x5b480, 0x00958354, 0x3f451eb8 /* [0x977d0c] 0.77 */, v); SU32(ra, 0x5b4a8, f015); }
    { const uint32_t v[6] = {f03, f01, f02, N, f0, f01}; wobble(ra, 0x5b4b0, 0x00958364, 0x3fea3d71 /* [0x977ebc] 1.83 */, v); SU32(ra, 0x5b4d8, f015); }
    { const uint32_t v[6] = {f03, f01, f02, N, f0, f01}; wobble(ra, 0x5b4e0, 0x00958374, 0x3fa60419 /* [0x977eb8] 1.297 */, v); SU32(ra, 0x5b508, f015); }
    SU32(ra, 0x5b510, (uint32_t)rx_rider_bone_index(ch, 0x00958384));  /* MXRider_Blink */
    SF32(ra, 0x5b518, 1.0f);
    SF32(ra, 0x5b514, 0);
    SU32(ra, 0x2c8, 0);
    /* the worker's processor: the next of five ([0x00a94084] cycles 0..4 over the table 0x009c4b58) */
    uint32_t n = U32(0, 0x00a94084);
    uint32_t cpu = U32(0, 0x009c4b58 + 4 * n);
    SU32(ra, 0x2c0, cpu);
    SU32(0, 0x00a94084, n + 1 < 5 ? n + 1 : 0);
    /* the first free of the twelve worker slots (0x00a9408c); the original stores at the loop index even when none is free */
    /* (w-rc-core) slots 12..31 in side storage (rider_cap.h rx_rc_tab): the 13th rider's slot 12 instead of the original's -1 */
    int32_t slot_i = -1; uint32_t i = 0;
    for (; i < RX_RC_MAX; i++) if (!U32(rx_rc_tab(0x00a9408c, 4, i), 0)) { slot_i = (int32_t)i; break; }
    if (slot_i < 0) rr_fatal("no free rider animation worker slot (32 taken)", i);
    SU32(ra, 0x2bc, (uint32_t)slot_i);
    SU32(rx_rc_tab(0x00a9408c, 4, i), 0, ra);
    SU32(ra, 0x2c4, 1);
    if (!U32(rx_rc_tab(0x00af6c38, 8, (uint32_t)slot_i), 4)) {
        uint32_t s = (uint32_t)slot_i;
        SU32(rx_rc_tab(0x00af6c08, 4, s), 0, os_create_event(0));
        SU32(rx_rc_tab(0x00a940bc, 4, s), 0, rx_rc_tab(0x00af6c08, 4, s));
        SU32(rx_rc_tab(0x00af6b80, 4, s), 0, os_create_event(0));
        SU32(rx_rc_tab(0x00af6b50, 4, s), 0, os_create_event(1));
        os_worker_thread(s, cpu);
    }
}

/* Four-wheel rider setup0061aa10 is implemented beside its walker in build_rider_plugs.c. */
