/* build_vehicle_parts.c -- construction unit 6 (vehicles, core/notes/construction.md section 15): the vehicle's part models bound
 * into its model instance, and the vehicle loader's package lookup. Hand transliterations of the disassembly:
 *   00541310 VehicleParts_Assemble (ECX = the material map V + 0x6a0, EDX = the vehicle list; s0 = the instance, s1 = the spec; RET 8)
 *   00541690 (one part slot's models listed in the instance's LOD records, the nodes' bounds grown over the part's)
 *   00542150 (part a's models take the materials of part b's material package whose MaterialKey matches)
 *   00542540 (the material map V + 0x6a0: the frame's texture materials by key -> the texture package's)
 *   005429a0 (the MaterialKey of n handles' names)
 *   00773290 / 00773e80 / 00774f60 / 00705de0 std::map<uint, uint>::operator[] and its inserts (MSVC 9 tree: node 0x18 {+0 left,
 *            +4 parent, +8 right, +0xc key, +0x10 value, +0x14 colour (0 red, 1 black), +0x15 isnil}; map {+0 proxy, +0x18 head,
 *            +0x1c size})
 *   00543db0 VehicleLoader_FindPackage, 00544220 / 005449d0 / 004fc5c0 the package cache's find (build_vehicle.h)
 * The shared gear helpers (00543410 / 005434c0 / 005435a0 / 00543fc0 / 00544f70 / 00416f10, "HR_%s", the handles' type tags) are
 * the rider unit's (build_rider.h).
 *
 * Frames: every function keeps the original's frame (entry ESP, /GS cookie, the locals at their offsets, each callee entered at
 * the original's ESP with its return address and stack arguments written) -- 00541690 stores two vectors whose w words it never
 * writes (the frame's old contents) into the model's node bounds.
 *
 * A spec (VehicleList entry, build_vehicle.h): +8 the model record, +0xc + 4k the 17 parts, +0x50 + 4k the pending parts, +0x94 the
 * CrewChief_ShirtColor texture, +0xc0 the high-resolution flag. A part: +0 std::string the material package, +0x1c std::string the
 * model package (size +0x30), +0x38 the part has a high-resolution package ("HR_<name>"). A package: +0x80 its handles (0x1c records:
 * +0 the handle), +0x84 their count, +0x94 loaded. */
#include "build_vehicle.h"
#include "build_vehicle_util.h"
#include "build_vehicle_tree.h"
#include "build_rider.h"
#include "build_asset.h"
#include "crt.h"
#include "loop_platform.h"
#include "anim.h"
#include "build_alloc.h"
#include "phys_vec.h"
#include <stdio.h>
#include <stdlib.h>

#define VP_LOADER       0x00d92ef0u   /* the vehicle loader (+4 its package cache) */
#define VP_LOD_TABLE    0x009c54a8u   /* the 8 LOD distance thresholds */
#define VP_ENGINE       0x00a95844u   /* g_EngineContext (+8 the Database) */
#define TAG_MODEL_LO    0x65646f6du   /* "model" */
#define TAG_MODEL_HI    0x0000006cu
#define TAG_MATERIAL_LO 0x6574616du   /* "material" */
#define TAG_MATERIAL_HI 0x6c616972u
#define TAG_TEXTURE_LO  0x74786574u   /* "texture" */
#define TAG_TEXTURE_HI  0x00657275u

static void vp_fatal(const char* what, uint32_t a) { fprintf(stderr, "rxcore: vehicle parts: %s (%08x)\n", what, a); abort(); }
static void invalid_parameter(void) { vp_fatal("_invalid_parameter_noinfo (an index check failed)", 0); }
static uint32_t lod_count(rx_addr vec) { return (uint32_t)((int32_t)(U32(vec, 0x10) - U32(vec, 0xc)) / 0x1c); }
static rx_addr str_chars(rx_addr s) { return U32(s, 0x18) >= 0x10 ? U32(s, 4) : s + 4; }
static rx_addr handle_name(rx_addr h) { return veh_vcall(h, 8, 0, 0, 0, 0); }   /* vf2 (AssetHandle 004dce30: handle + 4) */
static uint32_t handle_is(rx_addr B, uint32_t ret, rx_addr h, uint32_t lo, uint32_t hi) {   /* vf1 called at the original's ESP */
    veh_call_sp(B, ret, 0, 0, 0, 0);
    uint32_t r = rx_asset_handle_is(h, lo, hi);
    rx_set_sp(B);
    return r;
}
static rx_addr handle_name_at(rx_addr B, uint32_t ret, rx_addr h) {
    veh_call_sp(B, ret, 0, 0, 0, 0);
    rx_addr r = handle_name(h);
    rx_set_sp(B);
    return r;
}
/* 0078fd60 "HR_%s" (EDI = buf, ESI = 0x40): push name, push format, call */
static void hr_name_at(rx_addr B, uint32_t ret, rx_addr buf, rx_addr name) {
    veh_call_sp(B, ret, 2, 0x00940560u, name, 0);
    rx_gear_hr_name(buf, name);
    rx_set_sp(B);
}
static rx_addr find_package_fr(rx_addr e, crt_regs4 c, rx_addr cache, rx_addr name);
/* VehicleLoader_FindPackage of the vehicle loader's cache ([0x00d92ef0] + 4), with the frames the original's call leaves */
static rx_addr find_package_at(rx_addr B, uint32_t ret, rx_addr name) {
    veh_call_sp(B, ret, 0, 0, 0, 0);
    const crt_regs4 c = {0, 0, 0, 0};   /* (the callers' EBX / EBP / ESI: their saves are overwritten before any later read) */
    rx_addr r = find_package_fr(B - 4, c, U32(0, VP_LOADER) + 4, name);
    rx_set_sp(B);
    return r;
}
/* 00543fc0 MaterialKey (EAX = 0x40, ESI = out; push name) */
static void material_key_at(rx_addr B, uint32_t ret, rx_addr out, rx_addr name) {
    veh_call_sp(B, ret, 1, name, 0, 0);
    rx_material_key(out, 0x40, name);
    rx_set_sp(B);
}
static int32_t stricmp_at(rx_addr B, uint32_t ret, rx_addr a, rx_addr b) {
    veh_call_sp(B, ret, 2, a, b, 0);
    int32_t r = crt_stricmp(a, b);
    rx_set_sp(B);
    return r;
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * the package cache's lookup (cache: std::map<std::string, entry>, +0 proxy, +0x18 head; node: +0xc the key std::string, +0x28 the
 * entry (its database name std::string first), +0x55 isnil) */

/* 005449d0 (EAX = cache, s0 = &key std::string; RET 4): lower bound by _stricmp(node key, key) */
rx_addr rx_pkgcache_lower_bound(rx_addr cache, rx_addr key) {
    rx_addr res = U32(cache, 0x18), n = U32(res, 4);
    while (!U8(n, 0x55)) {
        if (crt_stricmp(str_chars(n + 0xc), str_chars(key)) < 0) n = U32(n, 8);
        else { res = n; n = U32(n, 0); }
    }
    return res;
}
/* 00544220 (ESI = cache; s0 = out iterator, s1 = &key; RET 8): find -> {proxy, node} */
rx_addr rx_pkgcache_find(rx_addr out, rx_addr cache, rx_addr key) {
    rx_addr lb = rx_pkgcache_lower_bound(cache, key), head = U32(cache, 0x18);
    rx_addr node = head;
    if (lb != head && crt_stricmp(str_chars(key), str_chars(lb + 0xc)) >= 0) node = lb;
    SU32(out, 4, node);
    SU32(out, 0, U32(cache, 0));
    return out;
}
/* 00543db0 VehicleLoader_FindPackage (EAX = cache, EDI = name): the package of the cached entry (Database vf8 with the entry's
 * database name), 0 when the name is empty or not cached */
rx_addr rx_vehloader_find_package(rx_addr cache, rx_addr name) {
    if (!name || !U8(name, 0)) return 0;
    const rx_addr sp0 = rx_sp(), B = sp0 - 0x34;   /* sub esp, 0x2c; push ebx, esi */
    rx_set_sp(B);
    const rx_addr str = B + 0x14, it = B + 0xc;
    veh_call_sp(B, 0x00543dd3, 1, name, 0, 0);
    crt_str_ctor_cstr(str, name);
    veh_call_sp(B, 0x00543de2, 2, it, str, 0);
    rx_pkgcache_find(it, cache, str);
    veh_call_sp(B, 0x00543dec, 0, 0, 0, 0);
    crt_str_dtor(str);
    rx_set_sp(B);
    rx_addr r = 0;
    if (U32(it, 4) != U32(cache, 0x18)) {
        rx_addr pair = U32(it, 4) + 0xc;   /* 004fc5c0 */
        rx_addr db = U32(U32(0, VP_ENGINE), 8);
        rx_addr dbname = str_chars(pair + 0x1c);
        veh_call_sp(B, U32(pair + 0x1c, 0x18) >= 0x10 ? 0x00543e30u : 0x00543e42u, 2, dbname, name, 0);
        r = veh_vcall(db, 0x20, 2, dbname, name, 0);   /* Database vf8 (0084a890) */
    }
    rx_set_sp(sp0);
    return r;
}
RX_FUNCTION(0x00543db0, rx_vehloader_find_package);

/* ---- the same lookup with the original's frames ----------------------------------------------------------------------------
 * part_attach (00541690) stores two of its frame's never-written words into the instance's node bounds (the w lanes of the centre
 * and half extents, construction.md 15.2 (a)); the original's values there are what the deeper calls before it left: this
 * lookup's std::string, its _stricmp calls, the package database's name hash. These versions write every stack word the original's
 * code writes (e = the call's return slot; c = the caller's EBX EBP ESI EDI): the frames below the call, at the original's addresses. */
#define VP_STRICMP_IAT 0x009162bcu   /* the exe's _stricmp import (EBP holds it in the loops) */
/* push b; push a; call [_stricmp] (the return slot ret_slot) */
int32_t rx_stricmp_call_at(rx_addr ret_slot, uint32_t ret, crt_regs4 r, rx_addr a, rx_addr b) {
    SU32(ret_slot, 0, ret); SU32(ret_slot, 4, a); SU32(ret_slot, 8, b);
    return crt_stricmp_at(ret_slot, r, a, b);
}
/* 005449d0 (EAX = cache; s0 = &key std::string; RET 4) */
static rx_addr pkgcache_lower_bound_fr(rx_addr e, crt_regs4 r, rx_addr cache, rx_addr key) {
    const rx_addr head = U32(cache, 0x18);
    SU32(e, -4, r.ebx); SU32(e, -8, r.esi);
    rx_addr n = U32(head, 4), res = head;
    if (U8(n, 0x55)) return head;
    SU32(e, -0xc, r.ebp); SU32(e, -0x10, r.edi);
    const uint32_t ebp = U32(0, VP_STRICMP_IAT);
    while (!U8(n, 0x55)) {
        const crt_regs4 c = {res, ebp, n, key + 4};
        if (rx_stricmp_call_at(e - 0x1c, 0x00544a15u, c, str_chars(n + 0xc), str_chars(key)) < 0) n = U32(n, 8);
        else { res = n; n = U32(n, 0); }
    }
    return res;
}
/* 00544220 (ESI = cache; s0 = out iterator, s1 = &key; RET 8) */
static rx_addr pkgcache_find_fr(rx_addr e, crt_regs4 r, rx_addr out, rx_addr cache, rx_addr key) {
    SU32(e, -0x1c, r.ebx); SU32(e, -0x20, r.ebp); SU32(e, -0x24, r.edi);
    SU32(e, -0x28, key); SU32(e, -0x2c, 0x00544232u);
    const crt_regs4 l = {r.ebx, key, cache, r.edi};
    const rx_addr lb = pkgcache_lower_bound_fr(e - 0x2c, l, cache, key);
    SU32(e, -0x10, lb);
    const rx_addr head = U32(cache, 0x18), proxy = U32(cache, 0);
    if (!cache || !proxy) invalid_parameter();
    SU32(e, -0x14, proxy);
    rx_addr node = head;
    if (lb != head) {
        const rx_addr lc = str_chars(lb + 0xc);
        const crt_regs4 c = {head, key, cache, lc};
        if (rx_stricmp_call_at(e - 0x30, 0x00544283u, c, str_chars(key), lc) >= 0) node = lb;
    }
    if (node == head) { SU32(e, -8, head); SU32(e, -0xc, proxy); }
    SU32(out, 4, node); SU32(out, 0, proxy);
    return out;
}
/* 005c3400 String_HashNoCase (EAX = string): push esi, edi; push ebx (EBX = [tolower]) and per byte push c; call (return
 * 005c3418) tolower (msvcr90 7855e8c1: push ebp) */
uint32_t rx_hash_nocase_at(rx_addr e, crt_regs4 r, rx_addr str) {
    SU32(e, -4, r.esi); SU32(e, -8, r.edi);
    if (!U8(str, 0)) return 0;
    SU32(e, -0xc, r.ebx);
    for (rx_addr p = str; U8(p, 0); p++) {
        SU32(e, -0x10, (uint32_t)(int32_t)(int8_t)U8(p, 0)); SU32(e, -0x14, 0x005c3418u); SU32(e, -0x18, r.ebp);
    }
    return rx_string_hash_nocase(str);
}
/* 007fa250 (EAX = the name hash map, EDI = &key pointer; s0 = value out; RET 4): push ebx, ebp, esi; the hash (return 007fa25c);
 * each bucket node of the same hash: push key, node key; call [_stricmp] (return 007fa27f) */
uint32_t rx_name_hash_find_at(rx_addr e, crt_regs4 r, rx_addr map, rx_addr keyp, rx_addr out) {
    SU32(e, -4, r.ebx); SU32(e, -8, r.ebp); SU32(e, -0xc, r.esi);
    SU32(e, -0x10, 0x007fa25cu);
    const crt_regs4 h = {r.ebx, r.ebp, map, keyp};
    const uint32_t hash = rx_hash_nocase_at(e - 0x10, h, U32(keyp, 0));
    const uint32_t ebp = U32(0, VP_STRICMP_IAT);
    for (rx_addr n = U32(U32(map, 0xc), 4 * (hash % U32(map, 0))); n != 0; n = U32(n, 0xc)) {
        if (U32(n, 8) != hash) continue;
        const crt_regs4 c = {hash, ebp, n, keyp};
        SU32(e, -0x10, U32(keyp, 0)); SU32(e, -0x14, U32(n, 0));
        if (rx_stricmp_call_at(e - 0x18, 0x007fa27fu, c, U32(n, 0), U32(keyp, 0)) == 0) { SU32(out, 0, U32(n, 4)); return 1; }
    }
    return 0;
}
/* Database::vf8 0084a890 (thiscall db, database name, package name; RET 8) */
static rx_addr database_find_package_fr(rx_addr e, crt_regs4 r, rx_addr db, rx_addr dbname, rx_addr pkg) {
    SU32(e, -4, db); SU32(e, -8, r.ebx); SU32(e, -0xc, r.ebp); SU32(e, -0x10, r.esi); SU32(e, -0x14, r.edi);
    const rx_addr cs = db + 0x62b54, out = e - 4;
    (void)dbname;   /* (read through EDI = &the pushed argument, e + 4)  */
    SU32(out, 0, 0);
    { const rx_addr sp = rx_sp(); rx_set_sp(e - 0x14); loop_enter_cs(cs); rx_set_sp(sp); }
    SU32(e, -0x18, cs); SU32(e, -0x1c, 0x0084a8acu);             /* push ebx; call [EnterCriticalSection] */
    SU32(e, -0x18, out); SU32(e, -0x1c, 0x0084a8bdu);            /* push &rec; EDI = &database name; call 007fa250 */
    const crt_regs4 f = {cs, r.ebp, db, e + 4};
    (void)rx_name_hash_find_at(e - 0x1c, f, db + 0x24, e + 4, out);
    { const rx_addr sp = rx_sp(); rx_set_sp(e - 0x14); loop_leave_cs(cs); rx_set_sp(sp); }
    SU32(e, -0x18, cs); SU32(e, -0x1c, 0x0084a8c4u);             /* push ebx; call [LeaveCriticalSection] (ESP back to e - 0x14) */
    const rx_addr rec = U32(out, 0);
    if (!rec) return 0;
    for (uint32_t i = 0; i < U32(rec, 0x14c); i++) {             /* push ebp (the package name), the record; call [_stricmp] */
        const rx_addr p = U32(rec, 0x148) + 0xa0 * i;
        const crt_regs4 c = {0xa0 * i, pkg, rec, i};
        SU32(e, -0x18, pkg);
        if (rx_stricmp_call_at(e - 0x20, 0x0084a8f0u, c, p, pkg) == 0) return p;
    }
    return 0;
}
/* 00543db0 with the frames (e = its return slot; c = the caller's registers) */
static rx_addr find_package_fr(rx_addr e, crt_regs4 c, rx_addr cache, rx_addr name) {
    if (!name || !U8(name, 0)) return 0;
    SU32(e, -0x30, c.ebx); SU32(e, -0x34, c.esi);                /* sub esp, 0x2c; push ebx, esi; ESI = cache */
    const rx_addr str = e - 0x20, it = e - 0x28;
    const crt_regs4 r = {c.ebx, c.ebp, cache, name};
    SU32(e, -0x38, name); SU32(e, -0x3c, 0x00543dd3u);           /* push edi; call [std::string(const char*)] */
    crt_str_ctor_cstr_at(e - 0x3c, r, str, name);
    SU32(e, -0x38, str); SU32(e, -0x3c, it); SU32(e, -0x40, 0x00543de2u);   /* push &str, &it; call 00544220 */
    pkgcache_find_fr(e - 0x40, r, it, cache, str);
    SU32(e, -0x38, 0x00543decu);                                 /* call [~basic_string] */
    crt_str_dtor_at(e - 0x38, r, str);
    const rx_addr head = U32(cache, 0x18);
    if (U32(it, 0) == 0 || U32(it, 0) != U32(cache, 0)) invalid_parameter();
    if (U32(it, 4) == head) return 0;
    SU32(e, -0x38, 0x00543e12u); SU32(e, -0x3c, name);           /* call 004fc5c0 (ESI = &it): push edi */
    const rx_addr pair = U32(it, 4) + 0xc;
    const rx_addr db = U32(U32(0, VP_ENGINE), 8);
    const rx_addr dbname = str_chars(pair + 0x1c);
    SU32(e, -0x38, name); SU32(e, -0x3c, dbname);                /* push edi, the database name; call [vf8] */
    SU32(e, -0x40, U32(pair + 0x1c, 0x18) >= 0x10 ? 0x00543e30u : 0x00543e42u);
    if (U32(U32(db, 0), 0x20) != 0x0084a890u) { rx_set_sp(e - 0x40); const rx_addr p = veh_vcall(db, 0x20, 2, dbname, name, 0); rx_set_sp(e); return p; }
    const crt_regs4 v = {head, c.ebp, it, name};
    return database_find_package_fr(e - 0x40, v, db, dbname, name);
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * std::map<uint, uint> (the material map V + 0x6a0): node 0x18 {+0xc key, +0x10 value, +0x14 colour, +0x15 isnil} */
static int uint_less(rx_addr a, rx_addr b) { return U32(a, 0) < U32(b, 0); }
/* 008185d0 _Buynode (ECX = &value; s0 head, s1 where, s2 head; RET 0xc): ReflexAlignedAlloc(0x18) */
static rx_addr uint_make(rx_addr head, rx_addr where, rx_addr val) {
    rx_addr z = rx_reflex_aligned_alloc(0x18);
    if (z) {
        SU32(z, 0, head); SU32(z, 4, where); SU32(z, 8, head);
        SU32(z, 0xc, U32(val, 0)); SU32(z, 0x10, U32(val, 4));
        SU8(z, 0x14, 0); SU8(z, 0x15, 0);
    }
    return z;
}
static const vt_tree k_uint_map = {0x14, 0x1ffffffeu, uint_less, uint_make};
/* 008185d0 _Buynode with its frames (b = its return slot; ECX = &value, s0 head, s1 where, s2 head; RET 0xc): push ebp; the EH
 * record {fs:[0], handler 009020d0, state -1}; sub esp, 8; push ebx, esi, edi; push cookie ^ ebp; push 0x18; call
 * ReflexAlignedAlloc 0078fbe0 (return 00818604): push esi; [the allocator lock 0xd61704 around it when 0xda67d8 is set]; push 0x10,
 * size; call [_aligned_malloc] (return 0078fc14; the original's own frames below in the oracle) */
static rx_addr uint_make_fr(rx_addr b, const vt_frame* f, rx_addr head, rx_addr where, rx_addr val) {
    const rx_addr ebp = b - 4;
    SU32(b, -4, f->ebp); SU32(b, -8, 0xffffffffu); SU32(b, -0xc, 0x009020d0u); SU32(b, -0x10, U32(rx_teb(), 0));
    SU32(b, -0x1c, f->ebx); SU32(b, -0x20, where); SU32(b, -0x24, f->edi);
    SU32(b, -0x28, U32(0, 0x009b45e4) ^ ebp);
    const uint32_t fs0 = U32(rx_teb(), 0);
    SU32(rx_teb(), 0, ebp - 0xc);
    SU32(ebp, -0x10, b - 0x28);
    SU32(b, -0x2c, 0x18); SU32(b, -0x30, 0x00818604u);
    const rx_addr r = b - 0x30;
    SU32(r, -4, val);                                            /* push esi (ESI = &value) */
    const int lock = U8(0, 0x00da67d8) != 0;
    if (lock) { SU32(r, -8, 0x00d61704u); SU32(r, -0xc, 0x0078fbf7u); SU32(r, -0xc, 0x0078fc07u); }
    SU32(r, -8, 0x10); SU32(r, -0xc, 0x18);
    const rx_addr sp = rx_sp();
    rx_set_sp(r - 4);
    const rx_addr z = rx_reflex_aligned_alloc(0x18);
    rx_set_sp(sp);
    SU32(r, -0x10, 0x0078fc14u);
    if (lock) { SU32(r, -8, f->edi); SU32(r, -0xc, 0x00d61704u); SU32(r, -0x10, 0x0078fc30u); SU32(r, -0x10, 0x0078fc40u); }
    SU32(ebp, -0x14, z); SU32(ebp, -4, 0);
    if (z) {
        SU32(z, 0, head); SU32(z, 4, where); SU32(z, 8, head);
        SU32(z, 0xc, U32(val, 0)); SU32(z, 0x10, U32(val, 4));
        SU8(z, 0x14, 0); SU8(z, 0x15, 0);
    }
    SU32(ebp, -4, 0xffffffffu);
    SU32(rx_teb(), 0, fs0);
    return z;
}
/* (operator[]'s hint is the lower bound: its predecessor is below the key; anything else is a broken tree) */
static void vt_fatal_uint_map(rx_addr n) { vp_fatal("std::map<uint, uint> hint insert: the hint's predecessor is not below the key", n); }
#define MAP_NIL(n) U8((n), 0x15)

/* 00773290 (s0 = map, s1 = &key; RET 8) operator[]: &value (inserted as 0 when absent). Entered at its return slot (rx_sp()), with
 * the frames of the original's insert: 00773e80 (map, out, &value, hint proxy, hint node; RET 0x14) on the lower bound as hint,
 * which for a key below the hint takes one of four _Insert (00705de0) calls (empty map / before the leftmost / after the rightmost /
 * after the hint's predecessor or before the hint), each with its own return address; the predecessor through the checked
 * iterator's -- (006909c0, ESI = the iterator). */
rx_addr rx_uint_map_index(rx_addr m, rx_addr keyp) {
    const rx_addr e = rx_sp();
    const rx_addr head = U32(m, 0x18), proxy = U32(m, 0);
    rx_addr lb = head;
    for (rx_addr n = U32(head, 4); !MAP_NIL(n);) {
        if (U32(n, 0xc) < U32(keyp, 0)) n = U32(n, 8);
        else { lb = n; n = U32(n, 0); }
    }
    SU32(e, -0x10, head);                                        /* [esp + 0x14] = the end node */
    if (!proxy) invalid_parameter();
    rx_addr node = lb;
    const uint32_t key = U32(keyp, 0);
    if (lb == head || key < U32(lb, 0xc)) {
        const rx_addr val = e - 0x14, out = e - 0xc, e2 = e - 0x3c, ebp = U32(0, 0x00916224);
        SU32(e, -0x28, lb); SU32(e, -0x2c, proxy); SU32(e, -0x30, val); SU32(e, -0x34, out); SU32(e, -0x38, m);
        SU32(val, 0, key); SU32(val, 4, 0); SU32(e2, 0, 0x00773314u);
        /* 00773e80: sub esp, 0x18; push ebx (map), ebp (the _invalid_parameter_noinfo import), esi (the proxy), edi (the hint) */
        SU32(e2, -0x1c, m); SU32(e2, -0x20, ebp); SU32(e2, -0x24, proxy); SU32(e2, -0x28, lb);
        rx_set_sp(e2);
        rx_addr where; uint32_t left, ret;
        if (U32(m, 0x1c) == 0) { where = head; left = 1; ret = 0x00773ea7u; }
        else if (lb == U32(head, 0)) { where = lb; left = 1; ret = 0x00773ef4u; }
        else if (lb == head) { where = U32(head, 8); left = 0; ret = 0x00773f3du; }
        else {
            SU32(e2, -0x14, proxy); SU32(e2, -0x10, lb);         /* the iterator {proxy, hint}; --it (006909c0: push edi = &value) */
            SU32(e2, -0x2c, 0x00773f65u); SU32(e2, -0x30, val);
            const rx_addr p = vt_prev(&k_uint_map, lb);
            SU32(e2, -0x10, p);
            if (!(U32(p, 0xc) < key)) vt_fatal_uint_map(lb);
            if (MAP_NIL(U32(p, 8))) { where = p; left = 0; ret = 0x00773fffu; }
            else { where = lb; left = 1; ret = 0x00773f88u; }
        }
        SU32(e2, -0x2c, val); SU32(e2, -0x30, left); SU32(e2, -0x34, out); SU32(e2, -0x38, ret);   /* push &value, add_left, out; call */
        rx_set_sp(e2 - 0x38);
        /* 00705de0: EBX = the map, EBP = out, ESI as each path leaves it, EDI = the map */
        const uint32_t esi = U32(m, 0x1c) == 0 ? proxy : ret == 0x00773ef4u ? lb : ret == 0x00773f3du ? head : e2 - 0x14;
        const vt_frame f = {e2 - 0x38, m, out, esi, m, uint_make_fr};
        node = vt_insert_at_fr(&k_uint_map, m, where, left, val, &f);
        SU32(out, 0, proxy); SU32(out, 4, node);
        rx_set_sp(e);
    }
    if (node == head) invalid_parameter();
    return node + 0x10;
}
RX_FUNCTION(0x00773290, rx_uint_map_index);

/* ---------------------------------------------------------------------------------------------------------------------------
 * the material tables of a package: the material (or texture) handles (at most 20) and their MaterialKeys (computed 20 times, as
 * the original's loop does) */
typedef struct { rx_addr mats, keyptr, keybuf; } mat_tab;   /* frame offsets: handles, key pointers, key buffers (0x40 each) */
static int32_t mat_tab_fill(rx_addr B, mat_tab t, rx_addr pkg, uint32_t lo, uint32_t hi, uint32_t ret_vf1) {
    int32_t n = 0;
    for (uint32_t j = 0; j < U32(pkg, 0x84); j++) {
        rx_addr h = U32(U32(pkg, 0x80) + 0x1c * j, 0);
        if (handle_is(B, ret_vf1, h, lo, hi) && n < 0x14) SU32(B + t.mats, 4 * (uint32_t)n++, h);
    }
    return n;
}

/* 00542150 (stdcall list, spec, a, b; RET 0x10): part a's model entries take the material of part b's material package with the
 * same MaterialKey */
static void parts_override_materials(rx_addr list, rx_addr spec, uint32_t a, uint32_t b) {
    const rx_addr sp0 = rx_sp(), E = sp0 - 0x694, B = E - 0x10;
    rx_set_sp(B);
    veh_cookie(E, 0x68c);
    (void)list;
    if (!U32(spec, 0xc + 4 * b)) { rx_set_sp(sp0); return; }
    rx_addr pa = U32(spec, 0xc + 4 * a);
    SU32(B, 0x10, str_chars(pa + 0x1c));
    if (U32(spec, 0xc0) && U32(pa, 0x38)) { hr_name_at(B, 0x005421cc, B + 0xd8, U32(B, 0x10)); SU32(B, 0x10, B + 0xd8); }
    rx_addr pb = U32(spec, 0xc + 4 * b), nb = 0;
    if (pb) {
        nb = str_chars(pb);
        if (nb && U32(spec, 0xc0) && U32(pb, 0x38)) { hr_name_at(B, 0x00542218, B + 0x158, nb); nb = B + 0x158; }
    }
    const mat_tab t = {0x38, 0x88, 0x198};
    veh_call_sp(B, 0x0054222e, 3, B + 0x3c, 0, 0x4c);
    SU32(B, 0x38, 0);
    crt_memset(B + 0x3c, 0, 0x4c);
    rx_set_sp(B);
    for (uint32_t i = 0; i < 0x14; i++) SU32(B + t.keyptr, 4 * i, B + t.keybuf + 0x40 * i);
    int32_t n = 0;
    SU32(B, 0x14, 0);
    if (nb && U8(nb, 0)) {
        rx_addr pkg = find_package_at(B, 0x00542272, nb);
        if (pkg && U32(pkg, 0x94)) {
            n = mat_tab_fill(B, t, pkg, TAG_MATERIAL_LO, TAG_MATERIAL_HI, 0x005422a6);
            SU32(B, 0x14, (uint32_t)n);
            for (int r = 0; r < 0x14; r++)
                for (int32_t i = 0; i < n; i++)
                    material_key_at(B, 0x00542303, U32(B + t.keyptr, 4 * (uint32_t)i), handle_name_at(B, 0x005422f1, U32(B + t.mats, 4 * (uint32_t)i)));
        }
    }
    rx_addr na = U32(B, 0x10);
    if (!na || !U8(na, 0)) { rx_set_sp(sp0); return; }
    rx_addr pkgA = find_package_at(B, 0x00542332, na);
    SU32(B, 0x28, pkgA);
    if (!pkgA || !U32(pkgA, 0x94) || n <= 0) { rx_set_sp(sp0); return; }
    for (uint32_t j = 0; j < U32(pkgA, 0x84); j++) {
        SU32(B, 0x24, j); SU32(B, 0x20, 0x1c * j);
        rx_addr h = U32(U32(pkgA, 0x80) + 0x1c * j, 0);
        if (!handle_is(B, 0x00542382, h, TAG_MODEL_LO, TAG_MODEL_HI)) continue;
        h = U32(U32(pkgA, 0x80) + 0x1c * j, 0);
        rx_addr M = U32(h, 0x44);
        SU32(B, 0x30, h); SU32(h, 0x4c, 1); SU32(B, 0x34, M);
        if (M) {
            for (uint32_t lod = 0; lod < U32(M, 0x118); lod++) {
                SU32(B, 0x2c, lod); SU32(B, 0x1c, 0x1c * lod);
                if (lod >= lod_count(M + 0x124)) invalid_parameter();
                rx_addr R = U32(M, 0x130) + 0x1c * lod;
                for (uint32_t e = 0; e < (uint32_t)((int32_t)(U32(R, 0x14) - U32(R, 0x10)) >> 4); e++) {
                    SU32(B, 0x10, e); SU32(B, 0x18, 0x10 * e);
                    rx_addr En = U32(R, 0x10) + 0x10 * e;
                    rx_addr mat = U32(En, 0xc);
                    material_key_at(B, 0x00542458, B + 0x118, handle_name_at(B, 0x00542446, mat));
                    for (int32_t i = 0; i < (int32_t)U32(B, 0x14); i++)
                        if (stricmp_at(B, 0x00542486, B + 0x118, U32(B + t.keyptr, 4 * (uint32_t)i)) == 0) { mat = U32(B + t.mats, 4 * (uint32_t)i); break; }
                    SU32(En, 0xc, mat);
                }
            }
        }
        SU32(h, 0x4c, 0);
    }
    rx_set_sp(sp0);
}

/* 005429a0 (EAX = handles; s0 = key pointers, s1 = n; RET 8): the MaterialKey of each handle's name */
static void keys_of(rx_addr B, rx_addr handles, rx_addr keyptrs, int32_t n) {
    /* push ebp; push edi; push ebx; push esi below the entry ESP B */
    const rx_addr C = B - 0x10;
    for (int32_t i = 0; i < n; i++) {
        rx_addr h = U32(handles, 4 * (uint32_t)i);
        rx_addr name = handle_name_at(C, 0x005429bd, h);
        material_key_at(C, 0x005429cb, U32(keyptrs, 4 * (uint32_t)i), name);
    }
}
/* 00542540 (stdcall list, spec, map; RET 0xc): the frame part's (+0x10) model entries' materials whose texture-package key (part
 * +0x40's material package: its texture handles) matches: map[the material's texture id] = the texture */
static void parts_texture_map(rx_addr list, rx_addr spec, rx_addr map) {
    const rx_addr sp0 = rx_sp(), E = sp0 - 0x6a8, B = E - 0x10;
    rx_set_sp(B);
    veh_cookie(E, 0x6a4);
    (void)list;
    SU32(B, 0x34, map);
    if (!U32(spec, 0x40) || !map) { rx_set_sp(sp0); return; }
    rx_addr pf = U32(spec, 0x10);
    SU32(B, 0x10, str_chars(pf + 0x1c));
    if (U32(spec, 0xc0) && U32(pf, 0x38)) { hr_name_at(B, 0x005425c3, B + 0xf0, U32(B, 0x10)); SU32(B, 0x10, B + 0xf0); }
    rx_addr pt = U32(spec, 0x40), nt = 0;
    if (pt) {
        nt = str_chars(pt);
        if (nt && U32(spec, 0xc0) && U32(pt, 0x38)) { hr_name_at(B, 0x0054260d, B + 0x170, nt); nt = B + 0x170; }
    }
    const mat_tab t = {0x50, 0xa0, 0x1b0};
    veh_call_sp(B, 0x00542623, 3, B + 0x54, 0, 0x4c);
    SU32(B, 0x50, 0);
    crt_memset(B + 0x54, 0, 0x4c);
    rx_set_sp(B);
    for (uint32_t i = 0; i < 0x14; i++) SU32(B + t.keyptr, 4 * i, B + t.keybuf + 0x40 * i);
    int32_t n = 0;
    SU32(B, 0x20, 0);
    if (nt && U8(nt, 0)) {
        rx_addr pkg = find_package_at(B, 0x00542661, nt);
        if (pkg && U32(pkg, 0x94)) {
            n = mat_tab_fill(B, t, pkg, TAG_TEXTURE_LO, TAG_TEXTURE_HI, 0x00542692);
            SU32(B, 0x20, (uint32_t)n);
            for (int r = 0; r < 0x14; r++) {
                rx_addr C = veh_call_sp(B, 0x005426e2, 2, B + t.keyptr, (uint32_t)n, 0);
                keys_of(C, B + t.mats, B + t.keyptr, n);
                rx_set_sp(B);
            }
        }
    }
    rx_addr nf = U32(B, 0x10);
    if (!nf || !U8(nf, 0)) { rx_set_sp(sp0); return; }
    rx_addr pkgF = find_package_at(B, 0x00542709, nf);
    SU32(B, 0x38, pkgF);
    if (!pkgF || !U32(pkgF, 0x94) || n <= 0) { rx_set_sp(sp0); return; }
    for (uint32_t j = 0; j < U32(pkgF, 0x84); j++) {
        SU32(B, 0x28, j); SU32(B, 0x30, 0x1c * j);
        rx_addr h = U32(U32(pkgF, 0x80) + 0x1c * j, 0);
        if (!handle_is(B, 0x00542756, h, TAG_MODEL_LO, TAG_MODEL_HI)) continue;
        h = U32(U32(pkgF, 0x80) + 0x1c * j, 0);
        rx_addr M = U32(h, 0x44);
        SU32(B, 0x3c, h); SU32(h, 0x4c, 1); SU32(B, 0x44, M);
        if (M) {
            for (uint32_t lod = 0; lod < U32(M, 0x118); lod++) {
                SU32(B, 0x18, lod); SU32(B, 0x14, 0x1c * lod);
                if (lod >= lod_count(M + 0x124)) invalid_parameter();
                rx_addr R = U32(M, 0x130) + 0x1c * lod;
                SU32(B, 0x40, R);
                for (uint32_t e = 0; e < (uint32_t)((int32_t)(U32(R, 0x14) - U32(R, 0x10)) >> 4); e++) {
                    SU32(B, 0x2c, e); SU32(B, 0x1c, 0x10 * e);
                    rx_addr mh = U32(U32(R, 0x10) + 0x10 * e, 0xc);   /* the entry's material handle */
                    SU32(mh, 0x4c, 1);
                    rx_addr mat = U32(mh, 0x44);
                    SU32(B, 0x48, mat);
                    if (mat && (int32_t)U32(mat, 0x18) > 0) {
                        for (int32_t s = 0; s < (int32_t)U32(mat, 0x18); s++) {
                            SU32(B, 0x24, (uint32_t)s); SU32(B, 0x10, mat + 0x1c + 4 * (uint32_t)s);
                            rx_addr th = U32(mat + 0x1c, 4 * (uint32_t)s);   /* the material's texture handles */
                            material_key_at(B, 0x0054286a, B + 0x130, handle_name_at(B, 0x00542858, th));
                            for (int32_t i = 0; i < (int32_t)U32(B, 0x20); i++) {
                                if (stricmp_at(B, 0x00542896, B + 0x130, U32(B + t.keyptr, 4 * (uint32_t)i)) != 0) continue;
                                rx_addr tex = U32(th, 0x44), nh = U32(B + t.mats, 4 * (uint32_t)i);
                                SU32(th, 0x4c, 1); SU32(nh, 0x4c, 1);
                                SU32(B, 0x4c, U32(U32(tex, 0x10), 4 * U32(tex, 0x18)));   /* the texture's id */
                                uint32_t v = U32(nh, 0x44);
                                veh_call_sp(B, 0x005428c9, 2, map, B + 0x4c, 0);
                                SU32(rx_uint_map_index(map, B + 0x4c), 0, v);
                                rx_set_sp(B);
                                SU32(th, 0x4c, 0); SU32(nh, 0x4c, 0);
                            }
                        }
                    }
                }
            }
        }
        SU32(h, 0x4c, 0);
    }
    rx_set_sp(sp0);
}

/* 00541690 (ECX = spec, EDX = the threshold set; s0 = the instance, s1 = the part slot, s2 = the material part slot; RET 0xc): the
 * part's model packages (+0 then +0x1c; "HR_" names when the spec is high resolution): each model's LODs, per instance LOD record
 * the LOD covers (00544f70 of the thresholds), every entry whose node the instance has: the node's bounds grown over the part
 * node's (scaled by the ratio of the two matrices' row lengths), the entry {part, node index, handle, material (the material part's
 * by MaterialKey)} appended (00416f10) */
static void part_attach(rx_addr spec, rx_addr set, rx_addr inst, uint32_t slot, uint32_t mslot) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4) & ~15u, E = A - 0x784, B = E - 0xc;
    rx_set_sp(B);
    veh_cookie(E, 0x780);
    SU32(B, 0x2c, inst); SU32(B, 0xa4, spec); SU32(B, 0x9c, set);
    if (!U32(spec, 0xc + 4 * slot)) { rx_set_sp(sp0); return; }
    rx_addr pm = U32(spec, 0xc + 4 * mslot), mname = 0;
    if (pm) {
        mname = str_chars(pm);
        if (mname && U32(spec, 0xc0) && U32(pm, 0x38)) { hr_name_at(B, 0x00541716, B + 0x248, mname); mname = B + 0x248; }
    }
    veh_call_sp(B, 0x00541735, 3, B + 0x12c, 0, 0x4c);
    SU32(B, 0x128, 0);
    crt_memset(B + 0x12c, 0, 0x4c);
    rx_set_sp(B);
    for (uint32_t i = 0; i < 0x14; i++) SU32(B + 0x178, 4 * i, B + 0x288 + 0x40 * i);
    int32_t nmat = 0;
    SU32(B, 0x6c, 0);
    rx_addr mpkg = 0;
    if (mname && U8(mname, 0)) {
        mpkg = find_package_at(B, 0x00541774, mname);
        if (mpkg && U32(mpkg, 0x94)) {
            for (uint32_t j = 0; j < U32(mpkg, 0x84); j++) {
                SU32(B, 0x14, j);
                rx_addr h = U32(U32(mpkg, 0x80) + 0x1c * j, 0);
                if (handle_is(B, 0x005417b5, h, TAG_MATERIAL_LO, TAG_MATERIAL_HI) && nmat < 0x14)
                    SU32(B + 0x128, 4 * (uint32_t)nmat++, U32(U32(mpkg, 0x80) + 0x1c * j, 0));
            }
            SU32(B, 0x6c, (uint32_t)nmat);
            for (uint32_t r = 0x14; r; r--) {
                SU32(B, 0x14, r);
                for (int32_t i = 0; i < nmat; i++)
                    material_key_at(B, 0x00541830, U32(B + 0x178, 4 * (uint32_t)i), handle_name_at(B, 0x0054181e, U32(B + 0x128, 4 * (uint32_t)i)));
            }
            SU32(B, 0x14, 0);
        }
    }
    SU32(B, 0x68, mpkg);
    for (uint32_t off = 0; off < 0x38; off += 0x1c) {
        SU32(B, 0x60, off);
        rx_addr P = U32(spec, 0xc + 4 * slot);
        rx_addr nm = str_chars(P + off);
        if (U32(spec, 0xc0) && U32(P, 0x38)) { hr_name_at(B, 0x0054189f, B + 0x1c8, nm); nm = B + 0x1c8; }
        else if (!nm) continue;
        if (!U8(nm, 0)) continue;
        rx_addr pkg = find_package_at(B, 0x005418c2, nm);
        SU32(B, 0x94, pkg);
        if (!pkg || !U32(pkg, 0x94)) continue;
        for (uint32_t j = 0; j < U32(pkg, 0x84); j++) {
            SU32(B, 0x50, j); SU32(B, 0x58, 0x1c * j);
            rx_addr h = U32(U32(pkg, 0x80) + 0x1c * j, 0);
            if (!handle_is(B, 0x0054190a, h, TAG_MODEL_LO, TAG_MODEL_HI)) continue;
            h = U32(U32(pkg, 0x80) + 0x1c * j, 0);
            SU32(B, 0x8c, h);
            SU32(h, 0x4c, 1);
            rx_addr M = U32(h, 0x44);
            SU32(B, 0x18, M);
            if (M && U32(M, 0x118)) {
                SU32(B, 0x20, 0);
                for (uint32_t lod = 0; lod < U32(M, 0x118); lod = U32(B, 0x88)) {
                    SU32(B, 0x14, 0x1c * lod);
                    /* the int array of the instance LODs this LOD covers {ReflexAlignedAlloc(10 * 4), 0, 10, 10, 0x7fffffff} */
                    const rx_addr vec = B + 0xa8;
                    veh_call_sp(B, 0x00541980, 1, 0x28, 0, 0);
                    rx_addr d = rx_reflex_aligned_alloc(0x28);
                    rx_set_sp(B);
                    SU32(vec, 0, d); SU32(vec, 4, 0); SU32(vec, 8, 10); SU32(vec, 0xc, 10); SU32(vec, 0x10, 0x7fffffffu);
                    int32_t lo = (int32_t)U32(0, VP_LOD_TABLE + 4 * (lod < 7 ? lod : 7)), hi;
                    uint32_t nx = lod + 1;
                    SU32(B, 0x88, nx);
                    if (nx < U32(M, 0x118)) {
                        hi = (int32_t)U32(0, VP_LOD_TABLE + 4 * (nx < 7 ? nx : 7)) - 1;
                        if (hi == 0) hi = 0x7fffffff;
                    } else hi = 500;
                    veh_call_sp(B, 0x00541a0f, 3, set + 4, (uint32_t)lo, (uint32_t)hi);
                    rx_iarr_indices_in_range(vec, set + 4, lo, hi);
                    rx_set_sp(B);
                    for (uint32_t i = 0; i < U32(vec, 4); i++) {
                        SU32(B, 0x24, i);
                        uint32_t ri = U32(U32(vec, 0), 4 * i);
                        if (U32(B, 0x20) >= lod_count(M + 0x124)) invalid_parameter();
                        rx_addr Ml = U32(M, 0x130) + U32(B, 0x14);
                        SU32(B, 0x98, Ml);
                        if (ri >= lod_count(inst + 0x124)) invalid_parameter();
                        rx_addr R = U32(inst, 0x130) + 0x1c * ri;
                        SU32(B, 0xa0, R);
                        for (uint32_t e = 0; e < (uint32_t)((int32_t)(U32(Ml, 0x14) - U32(Ml, 0x10)) >> 4); e++) {
                            SU32(B, 0x28, e); SU32(B, 0x1c, 0x10 * e);
                            rx_addr En = U32(Ml, 0x10) + 0x10 * e;
                            SU32(B, 0x90, En);
                            int32_t ni = (int32_t)U32(En, 4);
                            if (ni < 0) ni = 0;
                            rx_addr N = ((uint32_t)ni << 4) + U32(M, 0x18);
                            SU32(B, 0x48, N);
                            rx_addr nname = U32(U32(N, 0), 0x10) + U16(N, 8);
                            veh_call_sp(B, 0x00541b35, 1, nname, 0, 0);
                            rx_addr r = rx_hierarchy_find_node(inst + 0x10, nname);
                            rx_set_sp(B);
                            if (!r) continue;
                            int32_t idx = (int32_t)(r - U32(inst + 0x10, 8)) >> 4;
                            SU32(B, 0x5c, (uint32_t)idx);
                            if (idx < 0) continue;
                            rx_addr mat = U32(En, 0xc);
                            SU32(B, 0x4c, mat);
                            if (U32(B, 0x68)) {
                                material_key_at(B, 0x00541b78, B + 0x208, handle_name_at(B, 0x00541b66, mat));
                                for (int32_t k = 0; k < (int32_t)U32(B, 0x6c); k++)
                                    if (stricmp_at(B, 0x00541ba2, B + 0x208, U32(B + 0x178, 4 * (uint32_t)k)) == 0) { SU32(B, 0x4c, U32(B + 0x128, 4 * (uint32_t)k)); break; }
                            }
                            /* the bounds: N (the part's node) and N2 (the instance's) matrices' row lengths, the 0x50 bound records
                             * (+0 centre, +0x10 half extents, +0x40 radius) */
                            rx_addr N2 = U32(inst + 0x10, 8) + ((uint32_t)idx << 4);
                            rx_addr Nh = U32(N, 0), N2h = U32(N2, 0);
                            SU32(B, 0x84, Nh);
                            veh_copy(B + 0xd0, U32(Nh, 0x18) + ((uint32_t)U8(N, 0xb) << 6), 0x40);
                            #define ROWLEN(o) phys_sqrt(phys_add(phys_add(phys_mul(F32(B, 0xd8 + (o)), F32(B, 0xd8 + (o))), \
                                phys_mul(F32(B, 0xd4 + (o)), F32(B, 0xd4 + (o)))), phys_mul(F32(B, 0xd0 + (o)), F32(B, 0xd0 + (o)))))
                            float s0 = ROWLEN(0), s1 = ROWLEN(0x10), s2 = ROWLEN(0x20);
                            veh_copy(B + 0xd0, U32(N2h, 0x18) + ((uint32_t)U8(N2, 0xb) << 6), 0x40);
                            float f0 = phys_div(s0, ROWLEN(0));
                            SF32(B, 0x54, f0);
                            float f1 = phys_div(s1, ROWLEN(0x10));
                            SF32(B, 0xbc, f1);
                            float f2 = phys_div(s2, ROWLEN(0x20));
                            #undef ROWLEN
                            const rx_addr Bd = U32(N2h, 0x20) + (uint32_t)U8(N2, 0xb) * 0x50;
                            const rx_addr Cd = U32(Nh, 0x20) + (uint32_t)U8(N, 0xb) * 0x50;
                            float maxx = phys_add(F32(Bd, 0x10), F32(Bd, 0));
                            SF32(B, 0xc0, phys_sub(F32(Bd, 0), F32(Bd, 0x10)));
                            SF32(B, 0x64, f2);
                            float maxy = phys_add(F32(Bd, 0x14), F32(Bd, 4));
                            SF32(B, 0xc4, phys_sub(F32(Bd, 4), F32(Bd, 0x14)));
                            float cx = phys_mul(F32(Cd, 0), f0);
                            SU32(B, 0x34, U32(Cd, 0x14));
                            float maxz = phys_add(F32(Bd, 0x18), F32(Bd, 8));
                            float minz0 = phys_sub(F32(Bd, 8), F32(Bd, 0x18));
                            SU32(B, 0x38, U32(Cd, 0x18));
                            float cz = phys_mul(F32(Cd, 8), f2);
                            float cy = phys_mul(F32(Cd, 4), f1);
                            SF32(B, 0x74, cy);
                            float ey = phys_mul(F32(B, 0x34), f1);
                            SF32(B, 0x78, cz);
                            float ez = phys_mul(F32(B, 0x38), f2);
                            SF32(B, 0xc8, minz0);
                            float ex = phys_mul(F32(Cd, 0x10), f0);
                            SF32(B, 0x38, ez);
                            float t1 = phys_add(ey, F32(B, 0x74));
                            SF32(B, 0x34, ey);
                            float t2 = phys_add(ez, F32(B, 0x78));
                            float t0 = phys_add(ex, cx);
                            SF32(B, 0x30, ex);
                            if (!(maxx > t0)) maxx = t0;
                            if (!(maxy > t1)) maxy = t1;
                            if (!(maxz > t2)) maxz = t2;
                            float u1 = phys_sub(F32(B, 0x74), F32(B, 0x34));
                            float u0 = phys_sub(cx, F32(B, 0x30));
                            float u2 = phys_sub(F32(B, 0x78), F32(B, 0x38));
                            float minx = F32(B, 0xc0); if (!(u0 > minx)) minx = u0;
                            float miny = F32(B, 0xc4); if (!(u1 > miny)) miny = u1;
                            float minz = F32(B, 0xc8); if (!(u2 > minz)) minz = u2;
                            float ncx = phys_mul(phys_add(minx, maxx), 0.5f /* 0x9322cc */);
                            float nex = phys_sub(maxx, ncx);
                            float ncy = phys_mul(phys_add(miny, maxy), 0.5f);
                            SF32(B, 0x70, ncx); SF32(B, 0x74, ncy);
                            float ncz = phys_mul(phys_add(minz, maxz), 0.5f);
                            SF32(B, 0x78, ncz);
                            veh_v4(Bd, B + 0x70);                 /* movaps: w = [B + 0x7c] (the frame's old word) */
                            float ney = phys_sub(maxy, ncy), nez = phys_sub(maxz, ncz);
                            SF32(B, 0x30, nex); SF32(B, 0x34, ney); SF32(B, 0x38, nez);
                            veh_v4(Bd + 0x10, B + 0x30);          /* w = [B + 0x3c] */
                            SF32(Bd, 0x40, phys_sqrt(phys_add(phys_add(phys_mul(nez, nez), phys_mul(ney, ney)), phys_mul(nex, nex))));
                            SU8(N2, 0xc, U8(N2, 0xc) & 0xfb);
                            /* the entry {part, node index, handle, material} (B + 0x118) */
                            SU32(B, 0x118, U32(En, 0)); SU32(B, 0x124, U32(B, 0x4c)); SU32(B, 0x11c, (uint32_t)idx); SU32(B, 0x120, U32(En, 8));
                            veh_call_sp(B, 0x0054202a, 1, B + 0x118, 0, 0);
                            rx_lod_entries_push_back(R + 4, B + 0x118);
                            rx_set_sp(B);
                            Ml = U32(B, 0x98);
                        }
                    }
                    veh_call_sp(B, 0x0054209a, 1, U32(vec, 0), 0, 0);
                    crt_aligned_free(U32(vec, 0));   /* _aligned_free under the allocator's lock */
                    rx_set_sp(B);
                    SU32(B, 0x14, U32(B, 0x14) + 0x1c);
                    SU32(B, 0x20, U32(B, 0x88));
                }
            }
            SU32(U32(B, 0x8c), 0x4c, 0);
        }
    }
    rx_set_sp(sp0);
}

/* 00541310 VehicleParts_Assemble (ECX = the material map, EDX = the vehicle list; s0 = the instance, s1 = the spec; RET 8) */
void rx_vehicle_parts_assemble(rx_addr map, rx_addr list, rx_addr inst, rx_addr spec) {
    const rx_addr sp0 = rx_sp(), E = sp0 - 0x74, B = E - 0x10;   /* sub esp, 0x74; push ebx, ebp, esi, edi */
    rx_set_sp(B);
    veh_cookie(E, 0x6c);
    SU32(B, 0x14, list); SU32(B, 0x18, inst); SU32(B, 0x1c, map);
    /* GetTickCount (start; unused) */
    for (uint32_t k = 0; k < 0x11; k++)   /* the pending parts (0053fa30: their packages loaded) become current */
        if (U32(spec, 0x50 + 4 * k)) vp_fatal("a pending part (0053fa30 / 005400c0 / 00540450 / 00544c80 / 0053f9f0) not ported", spec);
    const rx_addr set = B + 0x20;
    veh_call_sp(B, 0x005413ed, 0, 0, 0, 0);
    rx_lod_set_init(set);
    rx_set_sp(B);
    for (uint32_t k = 0; k < 0x11; k++) {
        SU32(B, 0x10, 0x11 - k);
        rx_addr P = U32(spec, 0xc + 4 * k);
        if (!P || !U32(P, 0x30)) continue;
        rx_addr nm = str_chars(P + 0x1c), pkg = 0;
        if (U32(spec, 0xc0) && U32(P, 0x38)) { hr_name_at(B, 0x00541436, B + 0x38, nm); nm = B + 0x38; }
        else if (!nm) nm = 0;
        if (nm && U8(nm, 0)) pkg = find_package_at(B, 0x00541455, nm);
        veh_call_sp(B, 0x00541464, 2, set, pkg, 0);
        rx_lod_set_add_package(set, pkg);
        rx_set_sp(B);
    }
    veh_call_sp(B, 0x0054147d, 2, set, inst, 0);
    rx_lod_set_apply(set, inst);
    rx_set_sp(B);
    uint32_t dist = 0;
    switch (U32(U32(spec, 8), 0xc)) {   /* the factory index */
    case 0: case 1: dist = 900; break;
    case 2: dist = 2000; break;
    case 3: case 4: case 5: case 6: case 7: dist = 0x514; break;
    }
    SU32(inst, 0x120, dist);
    static const struct { uint32_t need, a, b, ret; } ovr[5] = {
        {0x10, 1, 1, 0x005414bb}, {0x10, 1, 0xb, 0x005414cc}, {0x10, 1, 0xc, 0x005414dd}, {0x14, 2, 1, 0x005414ee}, {0x1c, 4, 5, 0x005414ff}};
    for (int i = 0; i < 5; i++) {
        if (!U32(spec, ovr[i].need)) { if (ovr[i].need == 0x10) i = 2; continue; }   /* the first three need the frame part */
        const rx_addr sp = B - 0x14;   /* push b, a, spec, list */
        SU32(sp, 0, ovr[i].ret); SU32(sp, 4, list); SU32(sp, 8, spec); SU32(sp, 0xc, ovr[i].a); SU32(sp, 0x10, ovr[i].b);
        rx_set_sp(sp);
        parts_override_materials(list, spec, ovr[i].a, ovr[i].b);
        rx_set_sp(B);
    }
    if (U32(spec, 0x10)) {
        veh_call_sp(B, 0x00541511, 3, list, spec, map);
        parts_texture_map(list, spec, map);
        rx_set_sp(B);
    }
    for (uint32_t k = 0; k < 0x11; k++) {
        veh_call_sp(B, 0x00541523, 3, inst, k, k);
        part_attach(spec, set, inst, k, k);
        rx_set_sp(B);
    }
    if (!U32(spec, 0xc0) && U32(spec, 0x10)) {   /* the crew chief's shirt texture of the frame's material package */
        rx_addr nm = str_chars(U32(spec, 0x10));
        if (nm && U8(nm, 0)) {
            rx_addr pkg = find_package_at(B, 0x0054156d, nm);
            if (pkg && U32(pkg, 0x94))
                for (uint32_t j = 0; j < U32(pkg, 0x84); j++) {
                    rx_addr h = U32(U32(pkg, 0x80) + 0x1c * j, 0);
                    if (!handle_is(B, 0x005415a5, h, TAG_TEXTURE_LO, TAG_TEXTURE_HI)) continue;
                    rx_addr name = handle_name_at(B, 0x005415c9, U32(U32(pkg, 0x80) + 0x1c * j, 0));
                    veh_call_sp(B, 0x005415d7, 3, name, 0x0094044c /* "CrewChief_ShirtColor" */, 0x14);
                    int32_t c = crt_strnicmp(name, 0x0094044c, 0x14);
                    rx_set_sp(B);
                    if (c == 0) SU32(spec, 0x94, U32(U32(pkg, 0x80) + 0x1c * j, 0));
                }
        }
    }
    /* GetTickCount (end; unused); the threshold set's array freed (_aligned_free under the allocator's lock) */
    veh_call_sp(B, 0x00541630, 1, U32(set, 4), 0, 0);
    crt_aligned_free(U32(set, 4));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00541310, rx_vehicle_parts_assemble);

/* the registry: the transliterations above that are whole original functions (their conventions in the comments) */
RX_FUNCTION(0x00541690, part_attach);
RX_FUNCTION(0x00542150, parts_override_materials);
RX_FUNCTION(0x00542540, parts_texture_map);
RX_FUNCTION(0x008185d0, uint_make);
