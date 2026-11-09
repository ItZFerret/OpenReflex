/* build_vehicle_pkgcache.c -- construction unit 6 (vehicles, core/notes/construction.md section 15): the package cache of the part
 * loaders (the FactoryPartsLoader [0x00d92ef0] + 4, the FactoryGearLoader [0x00d92ef4] + 8) and the vehicle loader itself. Hand
 * transliterations of the disassembly:
 *   005436d0 construct, 00544a50 the head node, 005440f0 operator[] (00544480 insert(hint) / 00544840 _Insert / 00544b00 + 00544c20
 *   the node, 004ff810 the key compare, 0041bb70 / 0041bbe0 / 00544b90 / 0041bc30 the tree steps), 00543830 add, 00543750 the
 *   database request (0053c740 the DLC path: XenonOnlineContent::vf3 006d3c60 / 006d4220), 00543b30 request, 00543c30 release,
 *   00543990 unmark, 00543cd0 loaded, 00543e50 PackageCache_Update, 00543a20 purge (0041b840 erase), 00540610 the vehicle list's
 *   loader init and 00540000 FactoryPartsLoader_Construct.
 *
 * The cache C (0x38): +0 the container proxy, +0x18 the head node, +0x1c the size, +0x20 the requester value passed to Database vf16,
 * +0x24 "something to unload", +0x28 the unload delay (500 ms, float), +0x2c the purge period (0.25 s), +0x30 its accumulator, +0x34
 * the number of unmarked entries. A node (0x58): +0xc the package name std::string (the key, _stricmp order), +0x28 the database name
 * std::string, +0x44 references, +0x48 GetTickCount of the last use, +0x4c requested, +0x50 marked (an add after the database
 * request), +0x54 colour, +0x55 isnil. Strings are msvcp90 std::string (crt_str_*). The allocator lock (0x00d61704) and the critical
 * sections are not taken (one thread). */
#include "build_vehicle.h"
#include "build_vehicle_util.h"
#include "build_vehicle_tree.h"
#include "build_asset.h"
#include "build_alloc.h"
#include "world_wreck.h"
#include "loop_platform.h"
#include "crt.h"
#include <stdio.h>
#include <stdlib.h>

#define PC_ENGINE   0x00a95844u   /* g_EngineContext (+8 the Database, +0x10 the frame counter object: +0x58) */
#define PC_MXGAME   0x00a946b8u
#define PC_LOADER   0x00d92ef0u
static rx_addr chars(rx_addr s) { return U32(s, 0x18) >= 0x10 ? U32(s, 4) : s + 4; }
static rx_addr database(void) { return U32(U32(0, PC_ENGINE), 8); }

/* ---- the tree (std::map<std::string, entry>) ---- */
/* 004ff810 (ECX = a, EAX = b): _stricmp(a, b) < 0 */
static int str_less(rx_addr a, rx_addr b) { return crt_stricmp(chars(a), chars(b)) < 0; }
/* 00544b00 _Buynode (s0 left, s1 parent, s2 right, s3 &pair; RET 0x10) -> 00544c20: the links, the key and the value copied (copy
 * constructors), the entry's words, red, not nil */
static rx_addr str_make(rx_addr head, rx_addr where, rx_addr val) {
    rx_addr z = rx_reflex_aligned_alloc(0x58);
    if (z) {
        SU32(z, 4, where); SU32(z, 0, head); SU32(z, 8, head);
        crt_str_ctor_copy(z + 0xc, val);
        crt_str_ctor_copy(z + 0x28, val + 0x1c);
        for (uint32_t k = 0; k < 0x10; k += 4) SU32(z + 0x44, k, U32(val + 0x38, k));
        SU8(z, 0x54, 0); SU8(z, 0x55, 0);
    }
    return z;
}
static const vt_tree k_cache = {0x54, 0x038e38e2u, str_less, str_make};
/* the erased node's value: the database name, then the key (0041b840) */
static void node_dtor(rx_addr n) { crt_str_dtor(n + 0x28); crt_str_dtor(n + 0xc); }
/* 0041b840 erase (s0 cache, s1 out, s2 / s3 the iterator; RET 0x10) -> out {proxy, next} */
rx_addr rx_pkgcache_erase(rx_addr cache, rx_addr out, rx_addr node) {
    rx_addr next = vt_erase(&k_cache, cache, node, node_dtor);
    SU32(out, 4, next);
    SU32(out, 0, U32(cache, 0));
    return out;
}

/* 00544a50: the head node (0x58: links 0, black) */
static rx_addr head_node(void) {
    rx_addr n = rx_reflex_aligned_alloc(0x58);
    if (n) SU32(n, 0, 0);
    if (n + 4) SU32(n, 4, 0);
    if (n + 8) SU32(n, 8, 0);
    SU8(n, 0x54, 1); SU8(n, 0x55, 0);
    return n;
}
/* 005436d0 (ESI = cache) */
rx_addr rx_pkgcache_construct(rx_addr c) {
    rx_addr px = rx_reflex_aligned_alloc(4);
    if (px) SU32(px, 0, c); else px = 0;
    SU32(c, 0, px);
    rx_addr h = head_node();
    SU32(c, 0x18, h);
    SU8(h, 0x55, 1);
    SU32(U32(c, 0x18), 4, U32(c, 0x18));
    SU32(U32(c, 0x18), 0, U32(c, 0x18));
    SU32(U32(c, 0x18), 8, U32(c, 0x18));
    SF32(c, 0x28, 500.0f /* 0x9778ec */);
    SF32(c, 0x2c, 0.25f /* 0x92ff84 */);
    SU32(c, 0x1c, 0); SU32(c, 0x24, 0); SU32(c, 0x20, 1);
    SU32(c, 0x30, 0); SU32(c, 0x34, 0);
    return c;
}
RX_FUNCTION(0x005436d0, rx_pkgcache_construct);

/* 005440f0 (s0 = cache, s1 = &key; RET 8) operator[]: &the entry (node + 0x28), inserted when absent (a default entry: the value
 * std::string and four words the original never initializes -- every caller writes them at once) */
rx_addr rx_pkgcache_index(rx_addr c, rx_addr key) {
    rx_addr lb = rx_pkgcache_lower_bound(c, key), node = lb;
    if (lb == U32(c, 0x18) || crt_stricmp(chars(key), chars(lb + 0xc)) < 0) {
        RX_FRAME_ENTER(F, 0x90);
        const rx_addr V = F + 0x1c, P = F + 0x48;   /* the default value; the pair {key, value} */
        crt_str_ctor(V);
        crt_str_ctor_copy(P, key);
        crt_str_ctor_copy(P + 0x1c, V);
        for (uint32_t k = 0; k < 0x10; k += 4) SU32(P + 0x38, k, U32(V + 0x1c, k));
        node = vt_insert_hint(&k_cache, c, P, lb);
        crt_str_dtor(P + 0x1c);
        crt_str_dtor(P);
        crt_str_dtor(V);
        RX_FRAME_LEAVE(F, 0x90);
    }
    return node + 0x28;
}
RX_FUNCTION(0x005440f0, rx_pkgcache_index);

/* the find of a C string name (the callers' std::string temporary, 00544220) -> the node or the head */
static rx_addr find_cstr(rx_addr c, rx_addr name) {
    RX_FRAME_ENTER(F, 0x28);
    crt_str_ctor_cstr(F + 8, name);
    rx_pkgcache_find(F, c, F + 8);
    crt_str_dtor(F + 8);
    rx_addr n = U32(F, 4);
    RX_FRAME_LEAVE(F, 0x28);
    return n;
}

/* ---- the DLC content: 0053c740 (EDI = the path buffer; s0 = MXGame + 0x15554, s1 = the database name, s2 = &the content flag;
 * RET 0xc): "Database\" unless one of the four online content objects (+8 + 4k) has the database (vf3): its path, the flag
 * [0x00a93f58] ---- */
/* 006d4220 (EAX = name; s0 = a content record; RET 4): the record's path (+0xc1f) when its database map (+0xf24) has the name */
static rx_addr content_record_path(rx_addr rec, rx_addr name) {
    RX_FRAME_ENTER(F, 0x28);
    crt_str_ctor_cstr(F + 8, name);
    rx_world_string_map_find(rec + 0xf24, F, F + 8);
    crt_str_dtor(F + 8);
    rx_addr r = U32(F, 4) != U32(rec + 0xf24, 0x18) ? rec + 0xc1f : 0;
    RX_FRAME_LEAVE(F, 0x28);
    return r;
}
/* XenonOnlineContent::vf3 006d3c60 (thiscall content, name; RET 4): the first record (vector +0x18 .. +0x1c) that has the name */
rx_addr rx_online_content_find(rx_addr content, rx_addr name) {
    for (uint32_t i = 0; (int32_t)i < (int32_t)((U32(content, 0x1c) - U32(content, 0x18)) & ~3u) >> 2; i++) {
        rx_addr p = content_record_path(U32(U32(content, 0x18), 4 * i), name);
        if (p) return p;
    }
    return 0;
}
RX_FUNCTION(0x006d3c60, rx_online_content_find);
static uint32_t dlc_path(rx_addr path, rx_addr obj, rx_addr name, rx_addr flag) {
    crt_strncpy(path, 0x0093e434 /* "Database\" */, 0x103);
    SU8(path, 0x103, 0);
    SU32(flag, 0, 0);
    for (uint32_t k = 0; k < 4; k++) {
        rx_addr o = U32(obj, 8 + 4 * k);
        if (!o) continue;
        rx_addr p = veh_vcall(o, 0xc, 1, name, 0, 0);
        if (!p) continue;
        crt_strncpy(path, p, 0x103);
        SU32(flag, 0, U32(0, 0x00a93f58));
        SU8(path, 0x103, 0);
        return 1;
    }
    return 0;
}

/* 00543750 (ESI = the database name; s0 = async; RET 4): the database requested unless loaded (vf7) or being loaded (vf13 == 3):
 * vf5 (async) / vf4 (path, name, 1, the content flag) */
void rx_pkgcache_request_db(rx_addr name, int async) {
    RX_FRAME_ENTER(F, 0x110);
    const rx_addr flag = F + 4, path = F + 8;
    SU32(flag, 0, 0);
    for (uint32_t k = 0; k < 10; k++) SU8(path, k, U8(0x0093e434, k));   /* "Database\" + NUL (movq + word) */
    crt_memset(path + 0xa, 0, 0xfa);
    dlc_path(path, U32(0, PC_MXGAME) + 0x15554, name, flag);
    rx_addr db = database();
    if (!veh_vcall(db, 0x1c, 1, name, 0, 0) && veh_vcall(db, 0x34, 1, name, 0, 0) != 3) {
        uint32_t a[4] = {path, name, 1, U32(flag, 0)};
        rx_addr t = U32(U32(db, 0), async ? 0x14 : 0x10);
        void (*f)(rx_addr, uint32_t, uint32_t, uint32_t, uint32_t) = RX_FN(t, void (*)(rx_addr, uint32_t, uint32_t, uint32_t, uint32_t));
        if (f) f(db, a[0], a[1], a[2], a[3]); else rx_call_original(t, RX_THISCALL, db, 4, a);
    }
    RX_FRAME_LEAVE(F, 0x110);
}
RX_FUNCTION(0x00543750, rx_pkgcache_request_db);

/* 00543830 (EDI = cache; s0 = package name, s1 = database name; RET 8): a new entry (the database requested first, async) {database
 * name, refs 0, GetTickCount, not requested, marked}; an existing unmarked one is marked again (+0x34 counted down) */
void rx_pkgcache_add(rx_addr c, rx_addr pkg, rx_addr dbname) {
    rx_addr n = find_cstr(c, pkg);
    if (n == U32(c, 0x18)) {
        rx_pkgcache_request_db(dbname, 1);
        RX_FRAME_ENTER(F, 0x60);
        const rx_addr E = F + 0x20, K = F;   /* the entry {std::string, refs, tick, requested, marked}; the key */
        crt_str_ctor(E);
        crt_str_operator_assign(E, dbname);
        SU32(E, 0x1c, 0);
        SU32(E, 0x20, loop_get_tick_count());
        SU32(E, 0x24, 0);
        SU32(E, 0x28, 1);
        crt_str_ctor_cstr(K, pkg);
        rx_addr v = rx_pkgcache_index(c, K);
        crt_str_assign_sub(v, E, 0, 0xffffffffu);   /* operator=(const string&) */
        SU32(v, 0x1c, U32(E, 0x1c)); SU32(v, 0x20, U32(E, 0x20)); SU32(v, 0x24, U32(E, 0x24)); SU32(v, 0x28, U32(E, 0x28));
        crt_str_dtor(K);
        crt_str_dtor(E);
        RX_FRAME_LEAVE(F, 0x60);
        return;
    }
    if (U32(n, 0x50) == 0) { SU32(c, 0x34, U32(c, 0x34) - 1); SU32(n, 0x50, 1); }
}
RX_FUNCTION(0x00543830, rx_pkgcache_add);

/* 00543b30 (EAX = cache; s0 = package name, s1 = async; RET 8): a cached package requested: counted when already requested, else
 * loaded (vf16 async with the cache's requester value / vf15) and marked requested with one reference; the tick of the use */
void rx_pkgcache_request(rx_addr c, rx_addr pkg, int async) {
    rx_addr n = find_cstr(c, pkg);
    if (n == U32(c, 0x18)) return;
    if (U32(n, 0x4c)) {
        SU32(n, 0x44, U32(n, 0x44) + 1);
        SU32(n, 0x48, loop_get_tick_count());
        return;
    }
    rx_addr db = database(), dbname = chars(n + 0x28);
    if (async) {
        uint32_t a[4] = {dbname, pkg, 0, U32(c, 0x20)};
        rx_addr t = U32(U32(db, 0), 0x40);
        void (*f)(rx_addr, uint32_t, uint32_t, uint32_t, uint32_t) = RX_FN(t, void (*)(rx_addr, uint32_t, uint32_t, uint32_t, uint32_t));
        if (f) f(db, a[0], a[1], a[2], a[3]); else rx_call_original(t, RX_THISCALL, db, 4, a);
    } else veh_vcall(db, 0x3c, 3, dbname, pkg, 0);
    SU32(n, 0x4c, 1);
    SU32(n, 0x44, 1);
    SU32(n, 0x48, loop_get_tick_count());
}
RX_FUNCTION(0x00543b30, rx_pkgcache_request);

/* 00543c30 (ECX = cache, EAX = package name): one reference released (the last: "something to unload" and the tick) */
void rx_pkgcache_release(rx_addr c, rx_addr pkg) {
    rx_addr n = find_cstr(c, pkg);
    if (n == U32(c, 0x18)) return;
    if (!U32(n, 0x4c)) { SU32(n, 0x44, 0); return; }
    SU32(n, 0x44, U32(n, 0x44) - 1);
    if (U32(n, 0x44) == 0) { SU32(c, 0x24, 1); SU32(n, 0x48, loop_get_tick_count()); }
}
RX_FUNCTION(0x00543c30, rx_pkgcache_release);

/* 00543990 (ECX = cache, EAX = package name): a marked entry unmarked (+0x34 counted) */
void rx_pkgcache_unmark(rx_addr c, rx_addr pkg) {
    rx_addr n = find_cstr(c, pkg);
    if (n == U32(c, 0x18)) return;
    if (U32(n, 0x50) == 1) { SU32(n, 0x50, 0); SU32(c, 0x34, U32(c, 0x34) + 1); }
}
RX_FUNCTION(0x00543990, rx_pkgcache_unmark);

/* 00543cd0 (EAX = cache; s0 = package name; RET 4): 1 when the name is empty or the requested package is loaded (vf10) and no
 * request for it is pending (vf12) */
uint32_t rx_pkgcache_loaded(rx_addr c, rx_addr pkg) {
    if (!pkg || !U8(pkg, 0)) return 1;
    rx_addr n = find_cstr(c, pkg);
    if (n == U32(c, 0x18) || !U32(n, 0x4c)) return 0;
    rx_addr db = database(), dbname = chars(n + 0x28);
    if (!veh_vcall(db, 0x28, 2, dbname, pkg, 0)) return 0;
    return veh_vcall(db, 0x30, 2, dbname, pkg, 0) == 0;
}
RX_FUNCTION(0x00543cd0, rx_pkgcache_loaded);

/* 00543a20 (EDI = cache): the unmarked, unreferenced, unrequested entries erased (from the first again after each) */
static void purge(rx_addr c) {
    if ((int32_t)U32(c, 0x34) <= 0) return;
    rx_addr n = U32(U32(c, 0x18), 0);
    while (n != U32(c, 0x18)) {
        if (U32(n, 0x50) == 0 && U32(n, 0x44) == 0 && U32(n, 0x4c) == 0) {
            SU32(c, 0x34, U32(c, 0x34) - 1);
            RX_FRAME_ENTER(F, 8);
            rx_pkgcache_erase(c, F, n);
            RX_FRAME_LEAVE(F, 8);
            n = U32(U32(c, 0x18), 0);
        } else n = vt_next(&k_cache, n);
    }
}
/* 00543e50 PackageCache_Update (EAX = cache; s0 = dt; RET 4): when something may unload, the requested unreferenced entries idle
 * longer than the delay are unloaded (Database vf17 with the frame number); every 0.25 s the purge */
void rx_pkgcache_update(rx_addr c, float dt) {
    if (U32(c, 0x24) == 1) {
        SU32(c, 0x24, 0);
        uint32_t now = loop_get_tick_count();
        for (rx_addr n = U32(U32(c, 0x18), 0); n != U32(c, 0x18); n = vt_next(&k_cache, n)) {
            if (U32(n, 0x44) != 0 || U32(n, 0x4c) == 0) continue;
            uint32_t idle = now - U32(n, 0x48);
            if ((double)idle > (double)F32(c, 0x28)) {   /* fild (+ 2^32 when negative); fcompi with the float delay */
                SU32(n, 0x4c, 0);
                rx_addr db = database();
                uint32_t frame = U32(U32(U32(0, PC_ENGINE), 0x10), 0x58);
                veh_vcall(db, 0x44, 3, chars(n + 0x28), chars(n + 0xc), frame);
            } else SU32(c, 0x24, 1);
        }
    }
    float acc = rx_adds(F32(c, 0x30), dt);
    SF32(c, 0x30, acc);
    if (acc > F32(c, 0x2c)) { purge(c); SU32(c, 0x30, 0); }
}
RX_FUNCTION(0x00543e50, rx_pkgcache_update);

/* ---- the loader ---- */
/* 00540000 FactoryPartsLoader_Construct (EDI = loader): the vtable, the cache, the Database's background cache patterns of the
 * high-resolution part databases ("Database\" HR_Rider* / HR_ATV* / HR_MX* / HR_*: vf38) */
rx_addr rx_parts_loader_construct(rx_addr L) {
    SU32(L, 0, 0x00940788);
    rx_pkgcache_construct(L + 4);
    static const rx_addr pats[4] = {0x009406e0, 0x009406ec, 0x009406f4, 0x009406fc};
    for (int k = 0; k < 4; k++) {
        char pat[0x20]; uint32_t i = 0;
        for (; i + 1 < sizeof pat && U8(pats[k], i); i++) pat[i] = (char)U8(pats[k], i);
        pat[i] = 0;
        rx_database_cache_add(database(), "Database\\", pat);
    }
    return L;
}
/* 00540610 (s0 = the vehicle list; RET 4): the list's VehicleHierarchies table (+0x3c) and the parts loader [0x00d92ef0] (once) */
void rx_vehicle_list_init_loader(rx_addr list) {
    rx_addr t = rx_tbl_find(0x00ae0910, 0x00940704 /* "VehicleHierarchies" */);
    SU32(list, 0x3c, t);
    if (U32(0, PC_LOADER)) return;
    rx_addr L = rx_reflex_aligned_alloc(0x3c);
    SU32(0, PC_LOADER, L ? rx_parts_loader_construct(L) : 0);
}
RX_FUNCTION(0x00540610, rx_vehicle_list_init_loader);

/* the registry: the transliterations above that are whole original functions (their conventions in the comments) */
RX_FUNCTION(0x004ff810, str_less);
RX_FUNCTION(0x0041b840, rx_pkgcache_erase);
RX_FUNCTION(0x00544a50, head_node);
RX_FUNCTION(0x006d4220, content_record_path);
RX_FUNCTION(0x00543a20, purge);
RX_FUNCTION(0x00540000, rx_parts_loader_construct);
RX_FUNCTION(0x0053c740, dlc_path);
RX_FUNCTION(0x00544b00, str_make);
