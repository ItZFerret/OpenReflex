/* build_world.c -- see build_world.h. Hand transliteration of the physics world's constructor FUN_00768830 and its helpers from
 * the disassembly (allocation order, field order and values as the original; the critical sections the allocators take when
 * [0x00da67d8] is set are not needed: the core builds on one thread). */
#include "build_world.h"
#include "build_collision.h"
#include "rider_cap.h"
#include "build_alloc.h"
#include "build_terrain.h"
#include "crt.h"
#include "../src/fmt_db.h"
#include "../src/fmt_bxml.h"
#include <strings.h>
#include "phys_coll_grid.h"
#include "phys_vec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------------------------------------------------------------------
 * MSVC 9 containers */

/* 007ce6a0 (EDI = vector, one dead stack argument; RET 4): _aligned_malloc(4, 4) -> [p] = vector, [vector] = p (0 on failure) */
void rx_vec_proxy_init(rx_addr vec) {
    rx_addr p = rx_crt_aligned_malloc(4, 4);
    if (p) { SU32(p, 0, vec); SU32(vec, 0, p); }
    else SU32(vec, 0, 0);
}
RX_FUNCTION(0x007ce6a0, rx_vec_proxy_init);

/* the constructors' inline proxy: ReflexAlignedAlloc(4) -> [p] = vector, [vector] = p */
void rx_vec_proxy_init16(rx_addr vec) {
    rx_addr p = rx_reflex_aligned_alloc(4);
    if (p) SU32(p, 0, vec);
    SU32(vec, 0, p);
}

uint32_t rx_vec_size(rx_addr vec, uint32_t elem) { return (U32(vec, 0x10) - U32(vec, 0xc)) / elem; }

/* 0042bf30 (ECX = n): ReflexAlignedAlloc(n * 4) (n > 0x3fffffff throws std::length_error: not reachable here) */
static rx_addr alloc_ptrs(uint32_t n) {
    if (n > 0x3fffffffu) { fprintf(stderr, "rxcore: vector of %u pointers (length_error)\n", n); abort(); }
    return rx_reflex_aligned_alloc(n * 4);
}

/* 00418060 _Insert_n(where = end, 1, &value) of a vector of 4-byte elements, the reallocating branch and the in-place branch */
static void vec_insert_end_u32(rx_addr vec, uint32_t value) {
    rx_addr first = U32(vec, 0xc), last = U32(vec, 0x10);
    uint32_t cap = first ? (U32(vec, 0x14) - first) >> 2 : 0;
    uint32_t size = (last - first) >> 2;
    if (0x3fffffffu - size < 1) { fprintf(stderr, "rxcore: vector too long\n"); abort(); }   /* FUN_00851560 (_Xlen) */
    if (cap < size + 1) {
        uint32_t ncap = 0x3fffffffu - (cap >> 1) < cap ? 0 : cap + (cap >> 1);
        if (ncap < size + 1) ncap = size + 1;
        rx_addr nb = alloc_ptrs(ncap);
        rx_addr out = nb;
        for (rx_addr q = first; q < last; q += 4, out += 4) SU32(out, 0, U32(q, 0));   /* 00776d20 [first, where) */
        SU32(out, 0, value); out += 4;                                                /* 006c8230 the new element */
        (void)out;                                                                    /* 00776d20 [where, last): empty */
        uint32_t nsize = ((U32(vec, 0x10) - U32(vec, 0xc)) >> 2) + 1;
        if (U32(vec, 0xc)) rx_reflex_aligned_free(U32(vec, 0xc));
        SU32(vec, 0x14, nb + ncap * 4);
        SU32(vec, 0x10, nb + nsize * 4);
        SU32(vec, 0xc, nb);
    } else {
        /* capacity left (only reached through _Insert_n when push_back's own test failed: not with where = end) */
        SU32(last, 0, value);
        SU32(vec, 0x10, last + 4);
    }
}

/* push_back (FUN_00849660 with EDI = vector, FUN_006b7590 the same): in place while end < capacity end, else _Insert_n at end */
void rx_vec_push_u32(rx_addr vec, uint32_t value) {
    rx_addr first = U32(vec, 0xc);
    uint32_t cap = first ? (U32(vec, 0x14) - first) >> 2 : 0;
    rx_addr last = U32(vec, 0x10);
    if ((last - first) >> 2 < cap) { SU32(last, 0, value); SU32(vec, 0x10, last + 4); return; }
    vec_insert_end_u32(vec, value);
}
/* 00849660 in the original's form (EDI = vector, s0 = &value; RET 4) */
void rx_vec_push_back_ptr(rx_addr vec, rx_addr value_ptr) { rx_vec_push_u32(vec, U32(value_ptr, 0)); }
RX_FUNCTION(0x00849660, rx_vec_push_back_ptr);

/* reserve(n) of an empty vector of elem-byte records, the allocation through `alloc` (the reserve functions 00774250 / 007744d0 /
 * 007745f0 / 00774970 / 00773630 / 00774b10: allocate n * elem, move the old elements (none at construction), free the old block,
 * set capacity end / end / begin). A non-empty vector would need the element moves (copy constructors of the records with their
 * intrusive handles): not reached by construction. */
static void vec_reserve(rx_addr vec, uint32_t n, uint32_t elem, rx_addr (*alloc)(uint32_t bytes)) {
    rx_addr first = U32(vec, 0xc);
    uint32_t cap = first ? (U32(vec, 0x14) - first) / elem : 0;
    if (cap >= n) return;
    if (U32(vec, 0x10) != first) { fprintf(stderr, "rxcore: reserve of a non-empty vector (%08x)\n", vec); abort(); }
    rx_addr nb = alloc(n * elem);
    uint32_t size = (U32(vec, 0x10) - U32(vec, 0xc)) / elem;
    if (U32(vec, 0xc)) rx_reflex_aligned_free(U32(vec, 0xc));
    SU32(vec, 0x14, nb + n * elem);
    SU32(vec, 0x10, nb + size * elem);
    SU32(vec, 0xc, nb);
}
static rx_addr alloc16(uint32_t bytes) { return rx_crt_aligned_malloc(bytes, 16); }   /* 007756b0 / 00775580 / 00820ee0 */
static rx_addr alloc_reflex(uint32_t bytes) { return rx_reflex_aligned_alloc(bytes); } /* 0042bf30 / 00776cc0 */
/* vec_reserve for the other units: kind 0 allocates with _aligned_malloc(bytes, 16) (00820f20, 008494a0), kind 1 with ReflexAlignedAlloc
 * (0042bde0, 008496d0) */
void rx_vec_reserve_empty(rx_addr vec, uint32_t n, uint32_t elem, int kind) { vec_reserve(vec, n, elem, kind == 0 ? alloc16 : alloc_reflex); }

/* ---------------------------------------------------------------------------------------------------------------------------
 * global registries */

/* 00773cd0: the vector of physics worlds at 0x00db460c (proxy, empty); run once under the static guard [0x00dcbe54] bit 1 */
void rx_phys_worlds_init(void) {
    if (U32(0, 0x00dcbe54) & 1) return;
    SU32(0, 0x00dcbe54, U32(0, 0x00dcbe54) | 1);
    rx_addr p = rx_reflex_aligned_alloc(4);
    if (p) SU32(p, 0, RX_PHYS_WORLDS);
    SU32(0, RX_PHYS_WORLDS, p);
    SU32(0, RX_PHYS_WORLDS + 0xc, 0); SU32(0, RX_PHYS_WORLDS + 0x10, 0); SU32(0, RX_PHYS_WORLDS + 0x14, 0);
    /* _atexit(0x913f80): the destructor at exit (nothing to record) */
}

/* 007731f0 (cdecl scene): the physics world of a scene (the registry's guard first, as the original) */
rx_addr rx_phys_world_of_scene(rx_addr scene) {
    rx_phys_worlds_init();
    uint32_t n = (U32(0, 0x00db461c) - U32(0, 0x00db4618)) >> 2;
    for (uint32_t i = 0; i < n; i++) {
        rx_addr w = U32(U32(0, 0x00db4618), 4 * i);
        if (U32(w, 0xcca4) == scene) return w;
    }
    return 0;
}
RX_FUNCTION(0x007731f0, rx_phys_world_of_scene);

/* 00844110 (ESI = shape, ECX = type) */
void rx_collision_shape_construct(rx_addr s, uint32_t type) {
    SU32(s, 0x04, 0);
    SU32(s, 0x08, type);
    SU32(s, 0x00, 0x00976584);   /* RavenPhysics::CollisionShape::vftable */
    SU32(s, 0x0c, 1);
    SU32(s, 0x10, 1);
    SU32(s, 0x14, 0);
    SU32(s, 0x18, 0);
    SU32(s, 0x1c, 0);
    /* EnterCriticalSection(0x00d6d8f8); push_back(&shape) on the shape list; LeaveCriticalSection */
    rx_vec_push_u32(RX_SHAPE_LIST, s);
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * broadphase */

static void st_v4(rx_addr dst, rx_addr src) { for (uint32_t k = 0; k < 16; k += 4) SU32(dst, k, U32(src, k)); }

/* 00882370 */
rx_addr rx_bp_grid_construct(rx_addr g, uint32_t nx, uint32_t ny, uint32_t capacity, uint32_t per_cell, uint32_t overflow) {
    SU32(g, 0x408, ny);
    SU32(g, 0x404, nx);
    SU32(g, 0x40c, capacity);
    SU32(g, 0x418, overflow);
    SU32(g, 0x414, per_cell);
    SU32(g, 0x400, 0);
    st_v4(g + 0x430, 0x00d61670);   /* the runtime zero vector (movaps from .data) */
    st_v4(g + 0x440, 0x00d61670);
    st_v4(g + 0x450, 0x00d61670);
    st_v4(g + 0x460, 0x00d61670);
    SU32(g, 0x410, U32(g, 0x404) * U32(g, 0x408));
    SU32(g, 0x478, 0);
    SU32(g, 0x41c, rx_crt_aligned_malloc(rx_mul_sat(U32(g, 0x40c), 16), 16));
    SU32(g, 0x420, rx_crt_aligned_malloc(rx_mul_sat(U32(g, 0x40c), 16), 16));
    SU32(g, 0x470, rx_reflex_aligned_alloc(rx_mul_sat(U32(g, 0x410) * U32(g, 0x414), 4)));
    SU32(g, 0x474, rx_reflex_aligned_alloc(rx_mul_sat(U32(g, 0x410), 4)));
    for (int32_t i = 0; i < I32(g, 0x410); i++) SU32(U32(g, 0x474), 4u * (uint32_t)i, 0);
    SU32(g, 0x47c, rx_reflex_aligned_alloc(rx_mul_sat(U32(g, 0x418), 4)));
    return g;
}
RX_FUNCTION(0x00882370, rx_bp_grid_construct);

/* 0084a0d0 */
rx_addr rx_bp_cursor_construct(rx_addr c, uint32_t nx, uint32_t ny, uint32_t capacity, uint32_t per_cell, uint32_t overflow) {
    SU32(c, 0, 0);
    st_v4(c + 0x10, 0x00d61670);
    st_v4(c + 0x20, 0x00d61670);
    rx_addr g = rx_crt_aligned_malloc(0x480, 16);
    SU32(c, 0x30, g ? rx_bp_grid_construct(g, nx, ny, capacity, per_cell, overflow) : 0);
    return c;
}
RX_FUNCTION(0x0084a0d0, rx_bp_cursor_construct);

/* 00773950 (and its fixed-argument forms 00773740 (nx, ny, capacity, 250, 250) / 00773820 (nx, ny, capacity, per_cell, 20)) */
void rx_bp_cursor_create(rx_addr owner, uint32_t nx, uint32_t ny, uint32_t capacity, uint32_t per_cell, uint32_t overflow) {
    rx_addr c = rx_crt_aligned_malloc(0x40, 16);
    SU32(owner, 0x18, c ? rx_bp_cursor_construct(c, nx, ny, capacity, per_cell, overflow) : 0);
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * physics properties */

/* std::operator<(basic_string, basic_string) (msvcp90): memcmp of the common length, then the shorter first */
static int str_less_host(const uint8_t* a, uint32_t na, const uint8_t* b, uint32_t nb) {
    uint32_t n = na < nb ? na : nb;
    int c = memcmp(a, b, n);
    return c < 0 || (c == 0 && na < nb);
}
/* a game basic_string<char> (0x1c bytes: +4 the buffer or (capacity >= 0x10) its pointer, +0x14 size, +0x18 capacity) */
static uint32_t gstr_read(rx_addr s, uint8_t* out, uint32_t max) {
    uint32_t n = U32(s, 0x14);
    rx_addr p = U32(s, 0x18) >= 0x10 ? U32(s, 4) : s + 4;
    for (uint32_t k = 0; k < n && k < max; k++) out[k] = U8(p, k);
    return n < max ? n : max;
}

/* 00850bf0 / StringMap_Find 00702ad0 / 008519a0: lower bound in the red-black tree (head node at map + 0x18, root = head + 4,
 * nil nodes have byte +0x2d set; key at node + 0xc, value at node + 0x28), then "found unless key < node" */
uint32_t rx_phys_props_find(rx_addr registry, const char* name) {
    rx_addr map = registry + 0x30;
    rx_addr head = U32(map, 0x18);
    if (!head) return 0;
    const uint8_t* k = (const uint8_t*)name; uint32_t nk = (uint32_t)strlen(name);
    uint8_t buf[256];
    rx_addr best = head, node = U32(head, 4);
    while (!U8(node, 0x2d)) {
        uint32_t nn = gstr_read(node + 0xc, buf, sizeof buf);
        if (str_less_host(buf, nn, k, nk)) node = U32(node, 8);
        else { best = node; node = U32(node, 0); }
    }
    if (best == head) return 0;
    uint32_t nn = gstr_read(best + 0xc, buf, sizeof buf);
    if (str_less_host(k, nk, buf, nn)) return 0;
    return U32(best, 0x28);
}

/* ---- the registry's construction (static guard bit 1 of [0x00dce7d4]: FUN_00850010 and its loader FUN_008501a0) ---- */
static struct fmt_install* g_install;
void rx_phys_props_set_install(struct fmt_install* in) { g_install = in; }
struct fmt_install* rx_phys_props_install(void) { return g_install; }   /* w-sfx: the install the globals were built from (the crowd unit's config) */

/* a std::map<std::string, int> of game memory (MSVC 9 _Tree): head node at map + 0x18, size + 0x1c; node 0x30 {left, parent,
 * right, key (basic_string, +0xc), value (+0x28), color (+0x2c: 0 red, 1 black), isnil (+0x2d)} */
static int key_less(const char* a, rx_addr node) {   /* std::operator<(string, string) of a key with a node's key */
    uint8_t buf[256]; uint32_t n = gstr_read(node + 0xc, buf, sizeof buf);
    return str_less_host((const uint8_t*)a, (uint32_t)strlen(a), buf, n);
}
static int node_less(rx_addr node, const char* b) {
    uint8_t buf[256]; uint32_t n = gstr_read(node + 0xc, buf, sizeof buf);
    return str_less_host(buf, n, (const uint8_t*)b, (uint32_t)strlen(b));
}
static int isnil(rx_addr n) { return U8(n, 0x2d) != 0; }
/* 00851f50 _Lrotate / 005bb590 _Rrotate (stack: the map) */
static void tree_lrotate(rx_addr map, rx_addr w) {
    rx_addr p = U32(w, 8);
    SU32(w, 8, U32(p, 0));
    if (!isnil(U32(p, 0))) SU32(U32(p, 0), 4, w);
    SU32(p, 4, U32(w, 4));
    rx_addr head = U32(map, 0x18);
    if (w == U32(head, 4)) SU32(head, 4, p);
    else if (w == U32(U32(w, 4), 0)) SU32(U32(w, 4), 0, p);
    else SU32(U32(w, 4), 8, p);
    SU32(p, 0, w);
    SU32(w, 4, p);
}
static void tree_rrotate(rx_addr map, rx_addr w) {
    rx_addr p = U32(w, 0);
    SU32(w, 0, U32(p, 8));
    if (!isnil(U32(p, 8))) SU32(U32(p, 8), 4, w);
    SU32(p, 4, U32(w, 4));
    rx_addr head = U32(map, 0x18);
    if (w == U32(head, 4)) SU32(head, 4, p);
    else if (w == U32(U32(w, 4), 8)) SU32(U32(w, 4), 8, p);
    else SU32(U32(w, 4), 0, p);
    SU32(p, 8, w);
    SU32(w, 4, p);
}
/* 00586700 _Insert(addleft, where, value) (EDI = map, EAX = where): the node (00851b80: ReflexAlignedAlloc(0x30), the key copied
 * with basic_string's copy constructor), linked, then the red-black fix-up; returns the node */
static rx_addr tree_insert_at(rx_addr map, int addleft, rx_addr where, const char* key, uint32_t value) {
    rx_addr head = U32(map, 0x18);
    rx_addr n = rx_reflex_aligned_alloc(0x30);
    if (n) {
        SU32(n, 0, head); SU32(n, 4, where); SU32(n, 8, head);
        /* basic_string(const basic_string&) of the key (a transient std::string of the original): crt_str_ctor_cstr from a
         * frame copy of the C string */
        uint32_t len = (uint32_t)strlen(key);
        RX_FRAME_ENTER(F, 0x100);
        for (uint32_t k = 0; k <= len; k++) SU8(F, k, (uint8_t)key[k]);
        crt_str_ctor_cstr(n + 0xc, F);
        RX_FRAME_LEAVE(F, 0x100);
        SU32(n, 0x28, value);
        SU8(n, 0x2c, 0); SU8(n, 0x2d, 0);
    }
    SU32(map, 0x1c, U32(map, 0x1c) + 1);
    if (where == head) { SU32(head, 4, n); SU32(head, 0, n); SU32(head, 8, n); }
    else if (addleft) { SU32(where, 0, n); if (where == U32(head, 0)) SU32(head, 0, n); }
    else { SU32(where, 8, n); if (where == U32(head, 8)) SU32(head, 8, n); }
    for (rx_addr p = n; U8(U32(p, 4), 0x2c) == 0;) {
        rx_addr par = U32(p, 4), gp = U32(par, 4);
        if (par == U32(gp, 0)) {
            rx_addr u = U32(gp, 8);
            if (U8(u, 0x2c) == 0) { SU8(par, 0x2c, 1); SU8(u, 0x2c, 1); SU8(U32(U32(p, 4), 4), 0x2c, 0); p = U32(U32(p, 4), 4); }
            else {
                if (p == U32(par, 8)) { p = par; tree_lrotate(map, p); }
                SU8(U32(p, 4), 0x2c, 1); SU8(U32(U32(p, 4), 4), 0x2c, 0); tree_rrotate(map, U32(U32(p, 4), 4));
            }
        } else {
            rx_addr u = U32(gp, 0);
            if (U8(u, 0x2c) == 0) { SU8(par, 0x2c, 1); SU8(u, 0x2c, 1); SU8(U32(U32(p, 4), 4), 0x2c, 0); p = U32(U32(p, 4), 4); }
            else {
                if (p == U32(par, 0)) { p = par; tree_rrotate(map, p); }
                SU8(U32(p, 4), 0x2c, 1); SU8(U32(U32(p, 4), 4), 0x2c, 0); tree_lrotate(map, U32(U32(p, 4), 4));
            }
        }
    }
    SU8(U32(U32(map, 0x18), 4), 0x2c, 1);
    return n;
}
/* 00593950 insert(value) (unique): the descent, the predecessor test */
static rx_addr tree_insert_unique(rx_addr map, const char* key, uint32_t value) {
    rx_addr head = U32(map, 0x18), t = U32(head, 4), where = head;
    int left = 1;
    while (!isnil(t)) { where = t; left = key_less(key, t); t = left ? U32(t, 0) : U32(t, 8); }
    rx_addr it = where;
    if (left) {
        if (it == U32(head, 0)) return tree_insert_at(map, 1, where, key, value);
        /* --it (0063bf50) */
        if (isnil(it)) it = U32(it, 8);
        else if (!isnil(U32(it, 0))) { it = U32(it, 0); while (!isnil(U32(it, 8))) it = U32(it, 8); }
        else { rx_addr p; while (!isnil(p = U32(it, 4)) && it == U32(p, 0)) it = p; if (!isnil(it)) it = p; }
    }
    if (node_less(it, key)) return tree_insert_at(map, left, where, key, value);
    return it;
}
static rx_addr tree_next(rx_addr it) {   /* TreeIterator_Increment 00593dc0 */
    if (!isnil(U32(it, 8))) { it = U32(it, 8); while (!isnil(U32(it, 0))) it = U32(it, 0); return it; }
    rx_addr p;
    while (!isnil(p = U32(it, 4)) && it == U32(p, 8)) it = p;
    return p;
}
static rx_addr tree_prev(rx_addr head, rx_addr it) {   /* 0063bf50 */
    if (isnil(it)) return U32(it, 8);
    if (!isnil(U32(it, 0))) { it = U32(it, 0); while (!isnil(U32(it, 8))) it = U32(it, 8); return it; }
    rx_addr p;
    while (!isnil(p = U32(it, 4)) && it == U32(p, 0)) it = p;
    (void)head;
    return isnil(it) ? it : p;
}
/* 00850f30 insert(hint, value) */
static rx_addr tree_insert_hint(rx_addr map, rx_addr hint, const char* key, uint32_t value) {
    rx_addr head = U32(map, 0x18);
    if (U32(map, 0x1c) == 0) return tree_insert_at(map, 1, head, key, value);
    if (hint == U32(head, 0)) {
        if (key_less(key, hint)) return tree_insert_at(map, 1, hint, key, value);
    } else if (hint == head) {
        if (node_less(U32(head, 8), key)) return tree_insert_at(map, 0, U32(head, 8), key, value);
    } else if (key_less(key, hint)) {
        rx_addr prev = tree_prev(head, hint);
        if (node_less(prev, key)) {
            if (isnil(U32(prev, 8))) return tree_insert_at(map, 0, prev, key, value);
            return tree_insert_at(map, 1, hint, key, value);
        }
    } else if (node_less(hint, key)) {
        rx_addr next = tree_next(hint);
        if (next == head || key_less(key, next)) {
            if (isnil(U32(hint, 8))) return tree_insert_at(map, 0, hint, key, value);
            return tree_insert_at(map, 1, next, key, value);
        }
    }
    return tree_insert_unique(map, key, value);
}
/* 00850cd0 operator[](key): lower bound (008519a0); a new (key, 0) element inserted at the hint when absent; the value's address */
static rx_addr tree_index(rx_addr map, const char* key) {
    rx_addr head = U32(map, 0x18), best = head, n = U32(head, 4);
    while (!isnil(n)) { if (node_less(n, key)) n = U32(n, 8); else { best = n; n = U32(n, 0); } }
    if (best == head || key_less(key, best)) best = tree_insert_hint(map, best, key, 0);
    return best + 0x28;
}

/* the PhysicsProperties document (the BXMLAsset's XMLReader interface of the original; the same document through fmt_bxml) */
static float child_float(const fmt_bxml* d, int node, const char* name, float def) {   /* reader vf 0x1c + vf 0xcc */
    int c = fmt_bxml_find_child(d, node, name);
    if (c < 0) return def;
    fmt_bxml_value v;
    if (fmt_bxml_node_value(d, c, &v) != 0) return 0.0f;
    if (v.kind == FMT_BX_FLOAT) return fmt_bx_f32(&v, 0, 0);
    if (v.kind == FMT_BX_STRING) return (float)strtod(v.str, NULL);   /* ParseCommaFloatList 00829640: atof, fstp dword */
    return 0.0f;   /* the original leaves its local unwritten: not reached by the file */
}
static void attr_string(const fmt_bxml* d, int node, const char* name, char out[0x40]) {   /* vf 0xe8 + vf 0x13c: strncpy 0x3f */
    const char* s = fmt_bxml_attr_str(d, node, name);
    if (!s) return;
    strncpy(out, s, 0x3f); out[0x3f] = 0;
}

/* 008507f0: one material record (0xd0: +0 name, then nine splat parameters, three of them squared) */
static void material_read(const fmt_bxml* d, int node, rx_addr m) {
    const char* s = fmt_bxml_attr_str(d, node, "name");
    if (s) { uint32_t k = 0; for (; k < 0x3f && s[k]; k++) SU8(m, k, (uint8_t)s[k]); for (uint32_t j = k; j < 0x3f; j++) SU8(m, j, 0); SU8(m, 0x3f, 0); }
    float v;
    v = child_float(d, node, "Density", 1.0f /* 0x931f24 */);                         for (uint32_t k = 0; k < 16; k += 4) SF32(m, 0x40 + k, v);
    v = child_float(d, node, "LinearVelocityDamping", 0.00999999978f /* 0x9322ac */);  for (uint32_t k = 0; k < 16; k += 4) SF32(m, 0x50 + k, v);
    v = child_float(d, node, "AngularVelocityDamping", 0.0399999991f /* 0x9776a8 */);  for (uint32_t k = 0; k < 16; k += 4) SF32(m, 0x60 + k, v);
    v = child_float(d, node, "ResetTime", F32(0, 0x009cd9e8));                         for (uint32_t k = 0; k < 16; k += 4) SF32(m, 0x70 + k, v);
    v = child_float(d, node, "DisableTime", 2.0f /* 0x92e330 */);                      for (uint32_t k = 0; k < 16; k += 4) SF32(m, 0x80 + k, v);
    v = child_float(d, node, "MinimumDisableSpeed", 1.0f); v = v * v;                   for (uint32_t k = 0; k < 16; k += 4) SF32(m, 0x90 + k, v);
    v = child_float(d, node, "MinimumDisableAngularRate", 1000000.0f /* 0x977658 */); v = v * v;   for (uint32_t k = 0; k < 16; k += 4) SF32(m, 0xa0 + k, v);
    v = child_float(d, node, "WakeUpSpeed", 2.0f); v = v * v;                           for (uint32_t k = 0; k < 16; k += 4) SF32(m, 0xb0 + k, v);
    v = child_float(d, node, "CollisionMaximumDepenetration", 0.200000003f /* 0x92e388 */);   for (uint32_t k = 0; k < 16; k += 4) SF32(m, 0xc0 + k, v);
}

/* 0084fe70 (ESI = registry; stack a, b): the contact pair's index in the triangular table, clamped to [0, count] */
uint32_t rx_phys_props_contact_index(rx_addr reg, int32_t a, int32_t b) {
    if (a > b) { int32_t t = a; a = b; b = t; }   /* FUN_00613420 swap */
    int32_t n2 = (int32_t)(((U32(reg, 0x10) - U32(reg, 0xc)) / 0xd0) * 2);
    int32_t idx = (int32_t)((uint32_t)((n2 - a - 1) * a) >> 1) + b;
    int32_t cnt = (int32_t)((U32(reg, 0x28) - U32(reg, 0x24)) / 12);
    if (idx < 0) return 0;
    return (uint32_t)(idx > cnt ? cnt : idx);
}

/* 00850010 + 008501a0 */
static void phys_props_build(void) {
    rx_addr r = RX_PHYS_PROPS;
    rx_vec_proxy_init(r);                                 /* 008515c0: _aligned_malloc(4, 4) */
    SU32(r, 0xc, 0); SU32(r, 0x10, 0); SU32(r, 0x14, 0);
    rx_vec_proxy_init16(r + 0x18); SU32(r, 0x24, 0); SU32(r, 0x28, 0); SU32(r, 0x2c, 0);
    rx_vec_proxy_init16(r + 0x30);
    for (uint32_t m = 0x30; m <= 0x50; m += 0x20) {      /* the two maps' head nodes (00690910) */
        rx_addr h = rx_reflex_aligned_alloc(0x30);
        SU32(h, 0, 0); SU32(h, 4, 0); SU32(h, 8, 0); SU8(h, 0x2c, 1); SU8(h, 0x2d, 0);
        SU32(r, m + 0x18, h);
        SU8(h, 0x2d, 1);
        SU32(U32(r, m + 0x18), 4, U32(r, m + 0x18)); SU32(U32(r, m + 0x18), 0, U32(r, m + 0x18)); SU32(U32(r, m + 0x18), 8, U32(r, m + 0x18));
        SU32(r, m + 0x1c, 0);
        if (m == 0x30) rx_vec_proxy_init16(r + 0x50);
    }
    /* 008501a0: the PhysicsProperties BXML (Database::vf27 'bxml', its XMLReader) */
    rx_addr h = rx_asset_handle("bxml", "PhysicsProperties");
    SU32(h, 0x4c, 1);
    const fmt_database* db = g_install ? fmt_install_db(g_install, "EngineDependencies") : NULL;
    const fmt_asset* a = db ? fmt_db_find(db, "PhysicsProperties", "bxml") : NULL;
    fmt_buf b = {0};
    fmt_bxml* d = NULL;
    if (a && fmt_install_read(g_install, a, &b) == 0) d = fmt_bxml_parse(b.data, b.size);
    if (!d) { fprintf(stderr, "rxcore: PhysicsProperties: no document (rx_phys_props_set_install)\n"); abort(); }
    int root = 0, nphy = 0, ncon = 0;
    for (int k = 0; k < fmt_bxml_child_count(d, root); k++) {
        const char* t = fmt_bxml_tag(d, fmt_bxml_child(d, root, k));
        if (!strcmp(t, "Phy")) nphy++; else if (!strcmp(t, "Contact")) ncon++;
    }
    /* 00850d80: the materials resized to the count of Phy nodes, copies of an uninitialised stack record (00851630:
     * _aligned_malloc(n * 0xd0, 16)); every field the file gives is written below */
    if (nphy) {
        rx_addr mb = rx_crt_aligned_malloc((uint32_t)nphy * 0xd0, 16);
        SU32(r, 0xc, mb); SU32(r, 0x10, mb + (uint32_t)nphy * 0xd0); SU32(r, 0x14, mb + (uint32_t)nphy * 0xd0);
    }
    for (int k = 0; k < fmt_bxml_child_count(d, root); k++) {
        int c = fmt_bxml_child(d, root, k);
        if (strcmp(fmt_bxml_tag(d, c), "Phy")) continue;
        const char* ids = fmt_bxml_attr_str(d, c, "id");                   /* 00850600: vf 0x138, strtoul(s, NULL, 0) */
        uint32_t id = ids ? (uint32_t)strtoul(ids, NULL, 0) : (uint32_t)fmt_bxml_attr_i32(d, c, "id", 0);
        uint32_t n = (U32(r, 0x10) - U32(r, 0xc)) / 0xd0;
        uint32_t idx = id < n ? id : n;
        if (idx >= n) { fprintf(stderr, "rxcore: PhysicsProperties: id %u out of range\n", id); abort(); }
        rx_addr m = U32(r, 0xc) + idx * 0xd0;
        material_read(d, c, m);
        char name[0x41]; for (uint32_t j = 0; j < 0x40; j++) name[j] = (char)U8(m, j); name[0x40] = 0;
        SU32(tree_index(r + 0x30, name), 0, idx);                           /* map[std::string(name)] = idx (00850cd0) */
    }
    /* 00850e60: the contacts resized to the count of Contact nodes, zeroed (00776cc0: ReflexAlignedAlloc(n * 12)) */
    if (ncon) {
        rx_addr cb = rx_reflex_aligned_alloc(rx_mul_sat((uint32_t)ncon, 12));
        for (uint32_t k = 0; k < (uint32_t)ncon * 12; k += 4) SU32(cb, k, 0);
        SU32(r, 0x24, cb); SU32(r, 0x28, cb + (uint32_t)ncon * 12); SU32(r, 0x2c, cb + (uint32_t)ncon * 12);
    }
    for (int k = 0; k < fmt_bxml_child_count(d, root); k++) {
        int c = fmt_bxml_child(d, root, k);
        if (strcmp(fmt_bxml_tag(d, c), "Contact")) continue;
        char an[0x40] = "", bn[0x40] = "";                                 /* 00850670 (uninitialised when absent) */
        attr_string(d, c, "a", an); attr_string(d, c, "b", bn);
        int32_t ia = 0, ib = 0;
        uint32_t n = (U32(r, 0x10) - U32(r, 0xc)) / 0xd0;
        for (uint32_t i = 0; i < n; i++) {
            char nm[0x41]; for (uint32_t j = 0; j < 0x40; j++) nm[j] = (char)U8(U32(r, 0xc) + i * 0xd0, j); nm[0x40] = 0;
            if (!strncasecmp(an, nm, 0x40)) ia = (int32_t)i;               /* _strnicmp(.., 0x40) */
            if (!strncasecmp(bn, nm, 0x40)) ib = (int32_t)i;
        }
        uint32_t ci = rx_phys_props_contact_index(r, ia, ib);
        rx_addr e = U32(r, 0x24) + 12 * ci;                                 /* 00850b00 */
        SF32(e, 0, child_float(d, c, "Restitution", 1.0f));
        SF32(e, 4, child_float(d, c, "Friction", 1.0f));
        SF32(e, 8, child_float(d, c, "SpringConstant", 1.0f));
    }
    fmt_bxml_free(d); fmt_buf_free(&b);
    SU32(h, 0x4c, 0);
}

rx_addr rx_phys_props_get(void) {
    if (!(U32(0, 0x00dce7d4) & 1)) {
        SU32(0, 0x00dce7d4, U32(0, 0x00dce7d4) | 1);
        phys_props_build();
        /* _atexit(0x914ee0) */
    }
    return RX_PHYS_PROPS;
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * the physics world */

/* the MXWorld that owns a scene (the inline test of 00768830): the event's [game + 0x5a54] when the scene is the engine context's
 * +0x20 scene, the front end's [game + 0x20c8] for its +0x1c scene */
static rx_addr owner_world(rx_addr scene) {
    rx_addr ctx = U32(0, 0x00a95844), game = U32(0, 0x00a946b8);
    if (scene == U32(ctx, 0x20)) return U32(game, 0x5a54);
    if (scene == U32(ctx, 0x1c)) return U32(game, 0x20c8);
    return 0;
}

/* the game's small growable array {data, count, capacity, growth, max} with a first block of 5 pointers */
static void small_array5(rx_addr a) {
    SU32(a, 0, rx_reflex_aligned_alloc(rx_mul_sat(5, 4)));
}

/* an object vector + its cursor: proxy, zero +0xc..+0x1c, reserve */
static void object_vector(rx_addr v, uint32_t n, uint32_t elem) {
    rx_vec_proxy_init(v);
    SU32(v, 0xc, 0); SU32(v, 0x10, 0); SU32(v, 0x14, 0); SU32(v, 0x18, 0); SU32(v, 0x1c, 0);
    vec_reserve(v, n, elem, alloc16);
}

/* 00773c20 (ESI = four vectors of 12-byte records): proxies, then reserve 1000 of the first three (00774b10) */
static void record_vectors(rx_addr v) {
    for (uint32_t k = 0; k < 4; k++) {
        rx_addr e = v + 0x18 * k;
        rx_addr p = rx_reflex_aligned_alloc(4);
        if (p) SU32(p, 0, e);
        SU32(e, 0, p);
        SU32(e, 0xc, 0); SU32(e, 0x10, 0); SU32(e, 0x14, 0);
    }
    for (uint32_t k = 0; k < 3; k++) vec_reserve(v + 0x18 * k, 1000, 12, alloc_reflex);
}

/* 00706df0: a std::map head node (0x18 bytes: left / parent / right 0, +0x14 color 1 (black), +0x15 isnil 0) */
static rx_addr map_head_node(void) {
    rx_addr n = rx_reflex_aligned_alloc(0x18);
    SU32(n, 0, 0); SU32(n, 4, 0); SU32(n, 8, 0);
    SU8(n, 0x14, 1);
    SU8(n, 0x15, 0);
    return n;
}

/* 00768830 */
rx_addr rx_physics_world_construct(rx_addr w, rx_addr scene) {
    SU32(w, 0x1900, 0);
    SU8(w, 0x1910, U32(owner_world(scene), 0x20018) == 0);
    /* listeners */
    small_array5(w + 0x1914);
    SU32(w, 0x1918, 0); SU32(w, 0x191c, 5); SU32(w, 0x1920, 5); SU32(w, 0x1924, 0x7fffffff);
    /* contact handlers (std::vector) */
    rx_vec_proxy_init16(w + 0x1928);
    SU32(w, 0x1934, 0); SU32(w, 0x1938, 0); SU32(w, 0x193c, 0);
    /* simulations */
    small_array5(w + 0x1940);
    SU32(w, 0x1950, 0x7fffffff); SU32(w, 0x1944, 0); SU32(w, 0x1948, 5); SU32(w, 0x194c, 5);
    /* the four object vectors (sizes by the front-end flag) */
    int ev = U8(w, 0x1910) != 0;
    object_vector(w + 0x1954, ev ? 0x12c : 0xf, 0xe0);      /* 00774250 */
    object_vector(w + 0x1974, ev ? 0xc8 : 0xa, 0x80);       /* 007744d0 */
    object_vector(w + 0x1994, ev ? 0x7d0 : 0x96, 0xe0);     /* 007745f0 */
    object_vector(w + 0x19b4, ev ? 0x4e20 : 0x64, 0x40);    /* 00774970 */
    SU32(w, 0x19d4, 0);
    /* `vector constructor iterator' 004022d0: 200 water states (00767fe0), 200 sound timers (Fn_00768820) */
    for (uint32_t k = 0; k < 200; k++) { rx_addr e = w + 0x9a00 + 0x20 * k; SU32(e, 0, 0); st_v4(e + 0x10, 0x00d616a0); }
    for (uint32_t k = 0; k < 200; k++) SF32(w + 0xb300 + 0x20 * k, 0, 1.0f /* 0x931f24 */);
    /* the contact-handler list */
    rx_vec_proxy_init16(w + 0xcc00);
    SU32(w, 0xcc0c, 0); SU32(w, 0xcc10, 0); SU32(w, 0xcc14, 0);
    vec_reserve(w + 0xcc00, 200, 4, alloc_reflex);          /* 00773630 */
    SU32(w, 0xcc18, 0);
    record_vectors(w + 0xcc1c);
    rx_vec_proxy_init16(w + 0xcc7c);
    rx_addr head = map_head_node();
    SU32(w, 0xcc94, head);
    SU8(head, 0x15, 1);
    SU32(U32(w, 0xcc94), 4, U32(w, 0xcc94));
    SU32(U32(w, 0xcc94), 0, U32(w, 0xcc94));
    SU32(U32(w, 0xcc94), 8, U32(w, 0xcc94));
    SU32(w, 0xcc98, 0);
    SU32(w, 0xcca4, scene);
    SU32(w, 0xcca8, 0);
    /* std::string("Terrain"); the physics property registry (static guard); its id */
    SU32(w, 0xccac, rx_phys_props_find(rx_phys_props_get(), "Terrain"));
    /* the embedded CollisionShapeHFTerrain */
    rx_addr s = w + 0xccb0;
    rx_collision_shape_construct(s, 0x200);
    SU32(s, 0, 0x009765e4);   /* RavenPhysics::CollisionShapeHFTerrain::vftable */
    if (!(U32(0, 0x009e30e0) & 1)) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
    SU32(s, 0x20, RX_TERRAIN_LIST);
    SU32(s, 0xc, 0x301);
    int ev2 = U32(owner_world(U32(w, 0xcca4)), 0x20018) == 0;
    SU32(w, 0xcc9c, 0);
    SU8(w, 0x1910, ev2);
    SU32(w, 0x19b0, w); SU32(w, 0x19d0, w); SU32(w, 0x1970, w); SU32(w, 0x1990, w);
    if (ev2) {
        rx_bp_cursor_create(w + 0x1994, 0x1e, 0x1e, 2000, 0x28, 0xc8);     /* keyed objects */
        rx_bp_cursor_create(w + 0x19b4, 0x1e, 0x1e, 20000, 500, 0xc8);     /* static objects */
        /* (w-rc-core) the grids' box arrays hold `capacity` ids (the records' indices); an event of 13+ players (rider_cap.h) has more
         * dynamic records than the original's 300 (32 riders: past 300, the box arrays were written past their blocks): 3 x */
        const uint32_t more = rx_rc_more() ? 3u : 1u;
        rx_bp_cursor_create(w + 0x1954, 0x1e, 0x1e, 0x12c * more, 0xfa, 0xfa);    /* 00773740: generic objects */
        rx_bp_cursor_create(w + 0x1974, 0x1e, 0x1e, 0xc8 * more, 0x14, 0x14);     /* 00773820: rider bodies */
    } else {
        rx_bp_cursor_create(w + 0x1994, 10, 10, 0x96, 0x96, 0x96);
        rx_bp_cursor_create(w + 0x19b4, 10, 10, 100, 0x19, 0x14);
        rx_bp_cursor_create(w + 0x1954, 10, 10, 0xfa, 0xfa, 0xfa);
        rx_bp_cursor_create(w + 0x1974, 10, 10, 10, 5, 0x14);
    }
    SU32(w, 0xccc0, 4);
    SU32(w, 0xcca0, 0);
    rx_phys_worlds_init();
    rx_vec_push_u32(RX_PHYS_WORLDS, w);                       /* 006b7590 */
    /* [0x00a946c4] set: the physics worker thread (CreateEvent / CreateThread 00769fa0): not supported by the core */
    if (U32(0, 0x00a946c4) && !U32(0, 0x00d61388)) { fprintf(stderr, "rxcore: physics worker thread requested (0xa946c4)\n"); abort(); }
    return w;
}
RX_FUNCTION(0x00768830, rx_physics_world_construct);

/* 0075da90 */
rx_addr rx_physics_world_create(rx_addr scene) {
    rx_addr w = rx_crt_aligned_malloc(0xcce0, 16);
    return w ? rx_physics_world_construct(w, scene) : 0;
}
RX_FUNCTION(0x0075da90, rx_physics_world_create);

/* ---------------------------------------------------------------------------------------------------------------------------
 * the grids' extents */

/* 00882cf0 */
void rx_bp_grid_clear(rx_addr g) {
    for (int32_t c = 0; c < I32(g, 0x410); c++) {
        for (int32_t k = 0; k < I32(g, 0x414); k++) SU32(U32(g, 0x470), 4u * (uint32_t)(I32(g, 0x414) * c + k), 0xffffffff);
        SU32(U32(g, 0x474), 4u * (uint32_t)c, 0);
    }
    for (int32_t k = 0; k < I32(g, 0x418); k++) SU32(U32(g, 0x47c), 4u * (uint32_t)k, 0xffffffff);
    SU32(g, 0x478, 0);
}

/* 00882630 */
void rx_bp_grid_set_extent(rx_addr g, rx_addr mn, rx_addr mx) {
    rx_bp_grid_clear(g);
    st_v4(g + 0x430, mn);
    st_v4(g + 0x440, mx);
    v4_st(g + 0x450, v4_sub(v4_ld(g + 0x440), v4_ld(g + 0x430)));
    const float nx = (float)I32(g, 0x404), ny = (float)I32(g, 0x408);
    v4_st(g + 0x450, v4_div(v4_ld(g + 0x450), v4(nx, 1.0f /* 0x931f24 */, ny, ny)));
    v4_st(g + 0x460, v4_div(v4_ld(0x00d61650), v4_ld(g + 0x450)));   /* the runtime (1, 1, 1, 1) */
}

/* the union of a vector's grid boxes into (mn, mx) (007739c0 for 0xe0-byte objects, 00773b50 for statics) */
static void grid_boxes_union(rx_addr v, uint32_t elem, rx_addr mn, rx_addr mx) {
    if (!U32(v, 0x18)) return;
    uint32_t n = (U32(v, 0x10) - U32(v, 0xc)) / elem;
    for (uint32_t i = 0; i < n; i++) {
        rx_addr g = U32(U32(v, 0x18), 0x30);
        v4_st(mn, v4_min(v4_ld(mn), v4_ld(U32(g, 0x41c) + 16 * i)));
        v4_st(mx, v4_max(v4_ld(mx), v4_ld(U32(g, 0x420) + 16 * i)));
    }
}
/* 007737b0 / 00773bd0 / 00773890 (EDI = vector, EDX = min, ECX = max): the extent, the cells emptied again, every object inserted */
static void grid_rebuild(rx_addr v, uint32_t elem, rx_addr mn, rx_addr mx) {
    if (!U32(v, 0x18)) return;
    rx_bp_grid_set_extent(U32(U32(v, 0x18), 0x30), mn, mx);
    rx_bp_grid_clear(U32(U32(v, 0x18), 0x30));
    for (uint32_t i = 0; i < (U32(v, 0x10) - U32(v, 0xc)) / elem; i++) phys_grid_insert(U32(U32(v, 0x18), 0x30), i);
}

/* 00769030 */
void rx_physics_world_finalize(rx_addr w, rx_addr forest) {
    rx_modtrack_add_collision(w);
    if (forest && !U32(w, 0xcc9c)) {   /* FUN_00772940 */
        fprintf(stderr, "rxcore: the forest's collision (FUN_00667cc0) is the forest unit's\n"); abort();
    }
    RX_FRAME_ENTER(F, 0x60);
    const rx_addr maxA = F + 0x10, minA = F + 0x20, maxB = F + 0x30, minB = F + 0x40, minAB = F + 0x50;
    v4_st(minA, v4_splat(1000000.0f /* 0x977658 */)); v4_st(minB, v4_splat(1000000.0f));
    v4_st(maxA, v4_splat(-1000000.0f /* 0x9777d8 */)); v4_st(maxB, v4_splat(-1000000.0f));
    grid_boxes_union(w + 0x1994, 0xe0, minB, maxB);                      /* the keyed objects */
    v4_st(minAB, v4_min(v4_ld(minA), v4_ld(minB)));
    v4_st(minA, v4_max(v4_ld(maxA), v4_ld(maxB)));                       /* F + 0x20 now holds the max */
    grid_boxes_union(w + 0x19b4, 0x40, minB, maxB);                      /* the statics, accumulated */
    v4_st(minAB, v4_min(v4_ld(minAB), v4_ld(minB)));
    v4_st(minA, v4_max(v4_ld(minA), v4_ld(maxB)));
    const rx_addr mn = minAB, mx = minA;
    grid_rebuild(w + 0x1994, 0xe0, mn, mx);   /* FUN_00769370 */
    grid_rebuild(w + 0x19b4, 0x40, mn, mx);
    grid_rebuild(w + 0x1974, 0x80, mn, mx);
    grid_rebuild(w + 0x1954, 0xe0, mn, mx);
    RX_FRAME_LEAVE(F, 0x60);
}
