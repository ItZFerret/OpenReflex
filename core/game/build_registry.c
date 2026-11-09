/* build_registry.c -- see build_registry.h. Hand transliteration of XMLBuilder (0x00874000 .. 0x0087f800: a library built without
 * optimization, every local through the frame; the transliteration keeps its order of stores), the Registry (0x00823b60 .. 0x00824b50)
 * and the hash tables they use (0x0082ab20 .. 0x0082ad5d, 0x008434f0, 0x0084df50, 0x00880470). Paths the reference boot does not
 * execute abort naming the address. The critical sections the Registry takes around each document end as they began (one thread). */
#include "build_registry.h"
#include "build_alloc.h"
#include "build_xml.h"
#include "../src/fmt_inflate.h"
#include "crt.h"
#include "build_rider.h"
#include "../src/fmt_db.h"
#include "../src/fmt_vfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void unreached(uint32_t addr, const char* what) {
    fprintf(stderr, "rxcore: build_registry: %08x %s: path not in the reference boot (not ported)\n", addr, what);
    abort();
}
static uint32_t gstrlen(rx_addr p) { uint32_t n = 0; while (U8(p, n)) n++; return n; }
static int gstreq(rx_addr a, rx_addr b) {                            /* the inlined strcmp == 0 (005c33c0 ...) */
    for (;; a++, b++) { uint8_t x = U8(a, 0); if (x != U8(b, 0)) return 0; if (!x) return 1; }
}
/* FUN_0078fd90 (ECX = src, EDI = dst, ESI = n): strncpy(dst, src, n - 1), dst[n - 1] = 0 */
static void strncpy_term(rx_addr dst, rx_addr src, uint32_t n) { crt_strncpy(dst, src, n - 1); SU8(dst, n - 1, 0); }
static rx_addr aligned_realloc(rx_addr p, uint32_t n) { return rx_crt_aligned_realloc(p, n, 0x10); }   /* FUN_0078fb90 */
static void aligned_free(rx_addr p) { rx_crt_aligned_free(p); }                                         /* FUN_007105c0 */

/* ======================================================================================================================== */
/* the hash tables (0x20: +0 buckets, +4 max load, +8 count, +0xc bucket array, +0x10 0, +0x14 -1, +0x18 allocator, +0x1c owned;     */
/* nodes 0x10 {key, value, hash, next}); the allocator object (vtable 0x9753b0) is vf1 004eef20 _aligned_malloc 16 / vf2 00775540     */

static rx_addr alloc_obj(uint32_t n) { return rx_crt_aligned_malloc(n, 0x10); }   /* allocator vf1 (StdMemoryAllocator::vf1) */
static void    free_obj(rx_addr p) { rx_reflex_aligned_free(p); }                  /* allocator vf2 (StdMemoryAllocator::vf2) */

/* 0082ab20 (ESI = table, EAX = allocator or 0, EDI = buckets; load, owned; RET 8) / 0082aba0 (100 buckets) */
static void hash_construct(rx_addr t, uint32_t buckets) {
    rx_addr al = rx_reflex_aligned_alloc(4);
    if (al) SU32(al, 0, 0x009753b0);
    SU32(t, 0x18, al); SU8(t, 0x1c, 1);
    SU32(t, 0, buckets); SF32(t, 4, 0.75f /* 0x935aa8 */);
    SU32(t, 0x10, 0); SU32(t, 0x14, 0xffffffffu); SU32(t, 8, 0);
    rx_addr b = alloc_obj(4 * buckets);
    SU32(t, 0xc, b);
    crt_memset(b, 0, 4 * U32(t, 0));
}
/* 0084df50 (table, new bucket count; RET 8) */
static void hash_rehash(rx_addr t, uint32_t n) {
    rx_addr nb = alloc_obj(4 * n);
    crt_memset(nb, 0, 4 * n);
    for (int32_t i = 0; i < I32(t, 0); i++)
        for (rx_addr e = U32(U32(t, 0xc), 4 * (uint32_t)i), nx; e; e = nx) {
            uint32_t k = U32(e, 8) % n;
            nx = U32(e, 0xc);
            SU32(e, 0xc, U32(nb, 4 * k));
            SU32(nb, 4 * k, e);
        }
    free_obj(U32(t, 0xc));
    SU32(t, 0xc, nb); SU32(t, 0, n);
}
static void hash_grow_check(rx_addr t) {                            /* the load test before an insert (comiss count / n > load) */
    float n = (float)I32(t, 0), c = (float)I32(t, 8);
    if (c / n > F32(t, 4)) hash_rehash(t, (uint32_t)rx_cvtt(n * 1.5f /* 0x9322a4 */));
}
/* FUN_005c3380 (EDX = string) */
static uint32_t str_hash(rx_addr s) {
    uint32_t h = 0;
    for (uint8_t c; (c = U8(s, 0)); s++) {
        h = (h << 4) + (uint32_t)(int32_t)(int8_t)c;
        uint32_t t = h & 0xf0000000u;
        if (t) h ^= (t >> 24) ^ t;
    }
    return h;
}
/* the integer hash of 0082ac80 / 0082ac10: of the key's FIRST WORD (the key is a node pointer: `mov edi, [eax]; mov ecx, [edi]`),
 * entries matched by the pointer itself (fixed by unit 10: it hashed the pointer value, so the core's tables had their own bucket
 * layout and core lookups missed in the original's tables) */
static uint32_t int_hash(uint32_t k) {
    int32_t c = (int32_t)k;
    c += (int32_t)~((uint32_t)c << 15);
    int32_t a = (c >> 10) ^ c;
    a = (int32_t)((uint32_t)a * 9u);
    a ^= a >> 6;
    a += (int32_t)~((uint32_t)a << 11);
    return (uint32_t)((a >> 16) ^ a);
}
static rx_addr new_entry(rx_addr t, rx_addr key, uint32_t value, uint32_t h, uint32_t bucket) {
    rx_addr e = alloc_obj(0x10);
    if (!e) unreached(0x008435af, "hash node allocation");
    SU32(e, 0xc, 0);
    SU32(e, 0, key); SU32(e, 4, value); SU32(e, 8, h);
    SU32(e, 0xc, U32(U32(t, 0xc), 4 * bucket));
    SU32(U32(t, 0xc), 4 * bucket, e);
    SU32(t, 8, U32(t, 8) + 1);
    return e;
}
/* 008434f0 (EDI = table; &key, &value; RET 8): string keys */
static void hash_insert_str(rx_addr t, rx_addr key, uint32_t value) {
    hash_grow_check(t);
    uint32_t h = str_hash(key), b = h % U32(t, 0);
    for (rx_addr e = U32(U32(t, 0xc), 4 * b); e; e = U32(e, 0xc))
        if (U32(e, 8) == h && gstreq(U32(e, 0), key)) { SU32(e, 4, value); return; }
    new_entry(t, key, value, h, b);
}
/* 0082ac80 (EDI = table; &key, &value; RET 8): pointer keys */
static void hash_insert_int(rx_addr t, uint32_t key, uint32_t value) {
    hash_grow_check(t);
    uint32_t h = int_hash(U32(key, 0)), b = h % U32(t, 0);
    for (rx_addr e = U32(U32(t, 0xc), 4 * b); e; e = U32(e, 0xc))
        if (U32(e, 8) == h && U32(e, 0) == key) unreached(0x0082ad1e, "registry record inserted twice");
    new_entry(t, key, value, h, b);
}
/* 00880470 (ECX = table; &key, &out node, &out value; RET 0xc): 1 found */
static int hash_find_str(rx_addr t, rx_addr key, rx_addr* node) {
    uint32_t h = str_hash(key);
    for (rx_addr e = U32(U32(t, 0xc), 4 * (h % U32(t, 0))); e; e = U32(e, 0xc))
        if (U32(e, 8) == h && gstreq(key, U32(e, 0))) { *node = e; return 1; }
    return 0;
}
/* 0082ac10 (ESI = table, EAX = &key; &out; RET 4) */
static int hash_find_int(rx_addr t, uint32_t key, uint32_t* out) {
    uint32_t h = int_hash(U32(key, 0));
    for (rx_addr e = U32(U32(t, 0xc), 4 * (h % U32(t, 0))); e; e = U32(e, 0xc))
        if (U32(e, 8) == h && U32(e, 0) == key) { *out = U32(e, 4); return 1; }
    return 0;
}

/* the pointer-key functions with the original's ABI (registered; oracle/build_registry.cpp verifies them per call) */
uint32_t rx_registry_hash_find(rx_addr t, rx_addr keyp, rx_addr out) {      /* 0082ac10 (ESI = table, EAX = &key; &out; RET 4) */
    uint32_t v;
    if (!hash_find_int(t, U32(keyp, 0), &v)) return 0;
    SU32(out, 0, v);
    return 1;
}
RX_FUNCTION(0x0082ac10, rx_registry_hash_find)
void rx_registry_hash_insert(rx_addr t, rx_addr keyp, rx_addr valp) {        /* 0082ac80 (EDI = table; &key, &value; RET 8) */
    hash_insert_int(t, U32(keyp, 0), U32(valp, 0));
}
RX_FUNCTION(0x0082ac80, rx_registry_hash_insert)

/* ======================================================================================================================== */
/* XMLBuilder                                                                                                                 */

rx_addr rx_xmlb_construct(rx_addr b) {                               /* 00874300 */
    SU32(b, 0, 0x009751ec);                                           /* 00824fc0: IXMLReader */
    SU32(b, 0, 0x00976cc4);                                           /* 00874450: IXMLBuilder */
    SU32(b, 0, 0x00976edc);
    hash_construct(b + 4, 0x32);                                      /* 0082ab20 (no allocator given: its own) */
    SU32(b, 0x2a14, 0x00975368);                                      /* 00829b30: the zlib state */
    crt_memset(b + 0x2a14 + 8, 0, 0x38);
    SU32(b, 0x2a14 + 4, 0); SU32(b, 0x2a14 + 0x40, 0);
    rx_addr s = b + 0x2a58;                                           /* 0080fe50 */
    SU32(s, 0, 0x00974b60); SU32(s, 4, 0); SU32(s, 8, 0); SU32(s, 0x10, 0); SU32(s, 0x14, 0); SU32(s, 0x18, 0); SU32(s, 0x1c, 0);
    rx_addr w = b + 0x2a7c;                                           /* 0080f850: the compressed stream wrapper */
    SU32(w, 0, 0x00974b48); SU32(w, 4, 0); SU32(w, 8, 0); SU32(w, 0x14, 0); SU32(w, 0x18, 0); SU32(w, 0x1c, 0); SU32(w, 0x20, 0);
    SU32(w, 0x24, 0); SU32(w, 0x28, 0); SU32(w, 0x2c, 0);
    SU32(b, 0x2a0c, 0); SU32(b, 0x2a10, 0);
    for (uint32_t i = 0; i < 0x14; i++) { SU32(b, 0x24 + 4 * i, i); SU32(b, 0x74 + 0x108 * i, 0); }
    SU32(b, 0x1514, 0x14);
    for (uint32_t i = 0; i < 0x14; i++) { SU32(b, 0x1518 + 4 * i, i); SU32(b, 0x1568 + 0x108 * i, 0); }
    SU32(b, 0x2a08, 0x14);
    return b;
}

/* 0087bd40 (ECX = builder; string; RET 4): the builder's copy of a string (reference counted) */
static rx_addr intern(rx_addr b, rx_addr s) {
    rx_addr e;
    if (hash_find_str(b + 4, s, &e)) { SU32(e, 4, U32(e, 4) + 1); return U32(e, 0); }
    uint32_t n = gstrlen(s) + 1;
    rx_addr c = rx_reflex_aligned_alloc(n);
    strncpy_term(c, s, n);                                            /* 0078fd90 */
    hash_insert_str(b + 4, c, 1);                                     /* 008434f0 */
    return c;
}
/* vf103 / vf118 0087b330 (object, name; RET 8): an interned name at +0 */
static uint32_t set_name(rx_addr b, rx_addr o, rx_addr name) {
    if (gstrlen(name) >= 0x100) unreached(0x0087b372, "a name of 0x100 characters");
    if (U32(o, 0)) unreached(0x0087b37e, "a node renamed");
    SU32(o, 0, intern(b, name));
    return 1;
}
RX_FUNCTION(0x0087b330, set_name)
static rx_addr node_new(void) {                                      /* ReflexAlignedAlloc(0x24) + 00874140 */
    rx_addr n = rx_reflex_aligned_alloc(0x24);
    if (!n) unreached(0x008773ac, "node allocation");
    SU32(n, 0x10, 0); SU32(n, 0x14, 0); SU32(n, 0x18, 0); SU32(n, 0x1c, 0); SU32(n, 0x20, 0); SU32(n, 0, 0); SU32(n, 4, 0);
    SU16(n, 0xa, 0); SU16(n, 8, 0); SU32(n, 4, 0); SU32(n, 0xc, 0);
    return n;
}
rx_addr rx_xmlb_add_node(rx_addr b, rx_addr parent, rx_addr name) { /* vf81 00877360 */
    if (!parent) {
        if (U32(b, 0x2a0c)) unreached(0x00877385, "a second root");
        rx_addr r = node_new();
        SU32(b, 0x2a0c, r); SU32(b, 0x2a10, U32(b, 0x2a0c));
        SU16(U32(b, 0x2a0c), 8, 0);
        set_name(b, U32(b, 0x2a0c), name);                            /* vf103 */
        return U32(b, 0x2a0c);
    }
    SU32(parent, 0x18, U32(parent, 0x18) + 1);
    SU32(parent, 0x14, aligned_realloc(U32(parent, 0x14), 4 * U32(parent, 0x18)));
    rx_addr c = node_new();
    uint32_t k = U32(parent, 0x18) - 1;
    SU32(U32(parent, 0x14), 4 * k, c);
    SU16(c, 8, 0);
    SU32(c, 0x10, parent);
    set_name(b, c, name);
    SU32(b, 0x2a10, c);
    return c;
}
RX_FUNCTION(0x00877360, rx_xmlb_add_node)
/* vf114 008795a0 (node, string; RET 8): a string value */
static uint32_t set_string(rx_addr b, rx_addr n, rx_addr s) {
    if ((int16_t)U16(n, 8) != 1) { aligned_free(U32(n, 4)); SU32(n, 4, 0); SU32(n, 0xc, 0); }
    if (U16(n, 0xa) & 1) unreached(0x008795f2, "a string value of an attribute-flagged node");
    if (U32(n, 4)) unreached(0x0087967a, "a string value over a string");
    SU16(n, 8, 1);
    SU32(n, 4, intern(b, s));
    return 1;
}
RX_FUNCTION(0x008795a0, set_string)
/* vf111 / vf112 / vf116 / vf107 (node, values, count; RET 0xc): typed arrays (5 float, 3 int, 0xb bool: 4 bytes; 0xa vec3: 12) */
static uint32_t set_array(rx_addr n, rx_addr src, uint32_t cnt, uint16_t type, uint32_t esize, uint32_t addr) {
    if ((int16_t)U16(n, 8) == 1) unreached(addr, "a typed value over a string");
    rx_addr p = (int32_t)cnt > I32(n, 0xc) ? aligned_realloc(U32(n, 4), esize * cnt) : U32(n, 4);
    if (!p) unreached(addr, "value allocation");
    SU32(n, 4, p);
    SU16(n, 8, type);
    SU32(n, 0xc, cnt);
    crt_memcpy(U32(n, 4), src, esize * cnt);
    return 1;
}
static uint32_t set_floats(rx_addr b, rx_addr n, rx_addr s, uint32_t c) { (void)b; return set_array(n, s, c, 5, 4, 0x008799ab); }
RX_FUNCTION(0x00879990, set_floats)
static uint32_t set_ints(rx_addr b, rx_addr n, rx_addr s, uint32_t c) { (void)b; return set_array(n, s, c, 3, 4, 0x008797cb); }
RX_FUNCTION(0x008797b0, set_ints)
static uint32_t set_bools(rx_addr b, rx_addr n, rx_addr s, uint32_t c) { (void)b; return set_array(n, s, c, 0xb, 4, 0x008798bb); }
RX_FUNCTION(0x008798a0, set_bools)
static uint32_t set_vec3(rx_addr b, rx_addr n, rx_addr s, uint32_t c) { (void)b; return set_array(n, s, c, 0xa, 0xc, 0x00879d8b); }
RX_FUNCTION(0x00879d70, set_vec3)
/* vf130 0087b3b0 (attribute, string; RET 8) */
static uint32_t set_attr_string(rx_addr b, rx_addr a, rx_addr s) {
    if ((int16_t)U16(a, 0xa) != 1) { aligned_free(U32(a, 4)); SU32(a, 4, 0); SU16(a, 8, 0); }
    if (U32(a, 4)) unreached(0x0087b3f6, "an attribute value over a string");
    SU16(a, 0xa, 1);
    SU32(a, 4, intern(b, s));
    return 1;
}
RX_FUNCTION(0x0087b3b0, set_attr_string)
/* 0087bc40 (ECX = builder; node, name; RET 8): a new attribute */
static rx_addr add_attr(rx_addr b, rx_addr n, rx_addr name) {
    SU32(n, 0x20, U32(n, 0x20) + 1);
    SU32(n, 0x1c, aligned_realloc(U32(n, 0x1c), 0xc * U32(n, 0x20)));
    rx_addr a = U32(n, 0x1c) + 0xc * (U32(n, 0x20) - 1);
    SU32(a, 0, 0); SU32(a, 4, 0); SU32(a, 4, 0); SU16(a, 8, 0); SU16(a, 0xa, 0);
    set_name(b, a, name);                                             /* vf118 */
    return a;
}
/* vf94 00877500 (node, name, value; RET 0xc) */
static rx_addr add_attr_string(rx_addr b, rx_addr n, rx_addr name, rx_addr value) {
    rx_addr a = add_attr(b, n, name);
    set_attr_string(b, a, value);                                     /* vf130 */
    return a;
}
RX_FUNCTION(0x00877500, add_attr_string)

/* 0087c000 (ECX = builder; node, index, nodes, ..., strings, ..., data, ...; RET 0x3c): BXML node `idx` into the DOM node */
typedef struct { rx_addr nodes, values, strs, data; } bxml_src;
static void build_node(rx_addr b, rx_addr node, uint32_t idx, const bxml_src* s) {
    rx_addr n = s->nodes + 0x20 * idx;
    if (U16(n, 0xe) & 1) unreached(0x0087c028, "a BXML node with flag 1");
    int32_t t = (int16_t)U16(n, 0xc);
    switch (t) {
    case 1: set_string(b, node, U32(s->strs, 4 * U32(n, 4))); break;                     /* vf114 */
    case 3: set_ints(b, node, s->data + U32(n, 4), U32(n, 8)); break;                    /* vf112 */
    case 5: set_floats(b, node, s->data + U32(n, 4), U32(n, 8)); break;                  /* vf111 */
    case 10: set_vec3(b, node, s->data + U32(n, 4), U32(n, 8)); break;                   /* vf107 */
    case 11: set_bools(b, node, s->data + U32(n, 4), U32(n, 8)); break;                  /* vf116 */
    default: if (t >= 1 && t <= 12) unreached(0x0087c599, "a BXML value type 2 / 4 / 6..9 / 12");
    }
    SU16(node, 0xa, U16(n, 0xe));
    for (uint32_t k = 0; k < U32(n, 0x1c); k++) {
        rx_addr v = s->values + 0xc * (U32(n, 0x18) + k);
        if ((int16_t)U16(v, 0xa) != 1) unreached(0x0087db2e, "a non-string BXML attribute");
        add_attr_string(b, node, U32(s->strs, 4 * U32(v, 0)), U32(s->strs, 4 * U32(v, 4)));   /* vf94 */
    }
    for (uint32_t k = 0; k < U32(n, 0x14); k++) {
        uint32_t ci = U32(n, 0x10) + k;
        rx_addr c = rx_xmlb_add_node(b, node, U32(s->strs, 4 * U32(s->nodes + 0x20 * ci, 0)));   /* vf81 */
        build_node(b, c, ci, s);
    }
}

static int sread(rx_stream* s, rx_addr dst, uint32_t n) {
    if (s->size - s->pos < n) { s->short_read = 1; return 0; }
    rx_stream_read(s, dst, n);
    return 1;
}
/* 00874530 (ECX = builder; stream; RET 4) */
int rx_xmlb_load(rx_addr b, rx_stream* src) {
    if (U32(b, 0x2a0c)) unreached(0x00876bd3, "load into a built document");   /* 00876bc0 */
    RX_FRAME_ENTER(F, 0x40);
    const rx_addr H = F;                                              /* the header, ebp - 0x3c */
    if (!sread(src, H, 0x20)) unreached(0x0087457c, "a short BXML header");
    uint32_t version = U16(H, 4), flags = U16(H, 6);
    uint32_t nstr = U32(H, 8), sbytes = U32(H, 0xc), dbytes = U32(H, 0x10), nvals = U32(H, 0x14), nnodes = U32(H, 0x18);
    rx_stream sub; uint8_t* inflated = NULL; rx_stream* st = src;
    if (flags & 1) {                                                  /* 0080f920: the wrapper at +0x2a7c over the zlib state +0x2a14 */
        uint32_t clen = 0;
        if (src->size - src->pos < 4) unreached(0x008745af, "a short compressed BXML");
        rx_stream_read_host(src, &clen, 4);
        rx_addr buf = rx_crt_aligned_malloc(0x40000, 16);
        uint64_t total = (uint64_t)sbytes + dbytes + (uint64_t)nvals * 12 + (uint64_t)nnodes * 32;
        inflated = (uint8_t*)malloc(total ? (size_t)total : 1);
        if (src->size - src->pos < clen || fmt_zlib_decode(src->data + src->pos, clen, inflated, (size_t)total) < 0)
            unreached(0x008745af, "a broken compressed BXML");
        src->pos += clen;
        rx_stream_init(&sub, inflated, (size_t)total);
        st = &sub;
        /* the wrapper's state after its close (as build_xml.c's reader): the freed chunk pointer at +0xc, its count +0x10 */
        rx_crt_aligned_free(buf);
        SU32(b, 0x2a7c + 0xc, buf); SU32(b, 0x2a7c + 0x10, 1);
    } else unreached(0x008745c4, "an uncompressed BXML");
    rx_addr strs = 0, tab = 0, data = 0, vals = 0, nodes = 0;
    if (sbytes) {
        strs = rx_reflex_aligned_alloc(sbytes);                       /* 00401f90 */
        tab = rx_reflex_aligned_alloc(4 * nstr);
        if (!sread(st, strs, sbytes)) unreached(0x0087460e, "short strings");
        SU32(tab, 0, strs);
        uint32_t k = 1;
        for (uint32_t i = 0; k < nstr; i++) if (U8(strs, i) == 0) { SU32(tab, 4 * k, strs + i + 1); k++; }
    }
    if (dbytes) {
        data = rx_reflex_aligned_alloc(dbytes);
        if (!sread(st, data, dbytes)) unreached(0x008746d6, "short data");
    }
    if (nvals) {
        vals = rx_reflex_aligned_alloc(0xc * nvals);
        if (version < 0x3ea) unreached(0x0087491f, "BXML values before version 0x3ea");
        if (!sread(st, vals, 0xc * nvals)) unreached(0x00874752, "short values");
        if (flags & 4) unreached(0x008747b0, "big-endian BXML");
    }
    if (!nnodes) unreached(0x0087519f, "a BXML without nodes");
    nodes = rx_reflex_aligned_alloc(0x20 * nnodes);
    if (!sread(st, nodes, 0x20 * nnodes)) unreached(0x00874af1, "short nodes");
    if (flags & 4) unreached(0x00874b4f, "big-endian BXML");
    /* the wrapper closes (vf1); CompressedInputStream::vf2 left the size of its last read at +0x2c */
    SU32(b, 0x2a7c + 0x2c, 0x20 * nnodes);
    if (flags & 2) unreached(0x00874f19, "a BXML external data section");
    bxml_src s = {nodes, vals, tab, data};
    rx_addr root = rx_xmlb_add_node(b, 0, U32(tab, 4 * U32(nodes, 0)));
    /* the stream's vf6 is called for an argument the executed path does not use */
    build_node(b, root, 0, &s);
    rx_reflex_aligned_free(0);                                        /* [ebp - 0x60]: 0 */
    aligned_free(nodes); aligned_free(vals); aligned_free(data); aligned_free(strs); aligned_free(tab);   /* 007105c0 */
    free(inflated);
    RX_FRAME_LEAVE(F, 0x40);
    return 1;
}
RX_FUNCTION(0x00874530, rx_xmlb_load)

/* ---- the query virtuals (this = builder) ---- */
static rx_addr xb_vf1_root(rx_addr b) { SU32(b, 0x2a10, U32(b, 0x2a0c)); return U32(b, 0x2a0c); }   /* 00877b90 */
RX_FUNCTION(0x00877b90, xb_vf1_root)
static rx_addr xb_vf2_current(rx_addr b) { return U32(b, 0x2a10); }  /* 00877bc0 */
RX_FUNCTION(0x00877bc0, xb_vf2_current)
static uint32_t xb_vf3_set_current(rx_addr b, rx_addr n) { SU32(b, 0x2a10, n); return n; }   /* 00877be0 */
RX_FUNCTION(0x00877be0, xb_vf3_set_current)
static rx_addr xb_vf7_child(rx_addr b, rx_addr n, rx_addr name) {   /* 00877cb0 (node, name; RET 8) */
    if (!n || !U32(n, 0x18)) { SU32(b, 0x2a10, 0); return 0; }
    if (!name) unreached(0x00877dbb, "first child without a name");
    for (uint32_t k = 0; k < U32(n, 0x18); k++) {
        rx_addr c = U32(U32(n, 0x14), 4 * k);
        if (gstreq(U32(c, 0), name)) { SU32(b, 0x2a10, c); return c; }
    }
    SU32(b, 0x2a10, 0);
    return 0;
}
RX_FUNCTION(0x00877cb0, xb_vf7_child)
static rx_addr xb_vf10_next(rx_addr b, uint32_t c) {                 /* 00877f60 (cursor; RET 4) */
    rx_addr cur = b + 0x1568 + 0x108 * c;
    rx_addr n = U32(cur, 0);
    if (I32(cur, 4) >= I32(n, 0x18) - 1) { SU32(cur, 4, U32(n, 0x18)); SU32(b, 0x2a10, 0); return 0; }
    if (U8(cur, 8) == 0) {
        SU32(cur, 4, U32(cur, 4) + 1);
        SU32(b, 0x2a10, U32(U32(n, 0x14), 4 * U32(cur, 4)));
        return U32(b, 0x2a10);
    }
    for (int32_t i = I32(cur, 4) + 1; i < I32(n, 0x18); i++)
        if (gstreq(cur + 8, U32(U32(U32(n, 0x14), 4 * (uint32_t)i), 0))) {
            SU32(cur, 4, (uint32_t)i);
            SU32(b, 0x2a10, U32(U32(n, 0x14), 4 * (uint32_t)i));
            return U32(b, 0x2a10);
        }
    SU32(cur, 4, U32(n, 0x18));                                       /* 00878186: index = the count */
    SU32(b, 0x2a10, 0);
    return 0;
}
RX_FUNCTION(0x00877f60, xb_vf10_next)
static rx_addr xb_vf8_first(rx_addr b, rx_addr n, rx_addr name, rx_addr out) {   /* 00877de0 (node, name, &cursor; RET 0xc) */
    uint32_t c = U32(b, 0x1514 + 4 * U32(b, 0x2a08));
    rx_addr cur = b + 0x1568 + 0x108 * c;
    SU32(cur, 4, 0xffffffffu);
    SU32(cur, 0, n);
    if (name) strncpy_term(cur + 8, name, 0x100); else SU8(cur, 8, 0);
    rx_addr e = xb_vf10_next(b, c);
    if (!e) { SU32(cur, 0, 0); SU32(out, 0, 0xffffffffu); SU32(b, 0x2a10, 0); return 0; }
    SU32(b, 0x2a08, U32(b, 0x2a08) - 1);
    SU32(out, 0, c);
    SU32(b, 0x2a10, e);
    return e;
}
RX_FUNCTION(0x00877de0, xb_vf8_first)
static uint32_t xb_vf9_release(rx_addr b, uint32_t c) {              /* 00877f00 (cursor; RET 4) */
    if (c == 0xffffffffu) return 0;
    SU32(b + 0x108 * c, 0x1568, 0);
    SU32(b, 0x1518 + 4 * U32(b, 0x2a08), c);
    SU32(b, 0x2a08, U32(b, 0x2a08) + 1);
    return 0;
}
RX_FUNCTION(0x00877f00, xb_vf9_release)
static uint32_t xb_vf41_has_value(rx_addr b, rx_addr n) { (void)b; return U16(n, 8) != 0; }   /* 0087a930 */
RX_FUNCTION(0x0087a930, xb_vf41_has_value)

/* the value readers: a typed value copied (count >= n), or a string parsed as a comma list (0087f6c0 floats atof, 0087f580 ints
 * strtol base 0, 0087f620 uints strtoul, 0087f490 bools F/f/0 / T/t/1) */
static uint32_t parse_list(rx_addr dst, uint32_t n, rx_addr s, int kind) {
    char buf[0x400]; uint32_t L = 0;
    for (; L + 1 < sizeof buf && U8(s, L); L++) buf[L] = (char)U8(s, L);
    buf[L] = 0;
    const char* p = buf;
    for (uint32_t i = 0; i < n; i++) {
        if (!*p) unreached(0x0087f6f0, "a short value list");
        switch (kind) {
        case 5: SF32(dst, 4 * i, (float)atof(p)); break;             /* fstp dword of msvcr90 atof */
        case 3: SU32(dst, 4 * i, (uint32_t)strtol(p, NULL, 0)); break;
        case 4: SU32(dst, 4 * i, (uint32_t)strtoul(p, NULL, 0)); break;
        default:
            if (*p == 'F' || *p == 'f' || *p == '0') SU32(dst, 4 * i, 0);
            else if (*p == 'T') SU32(dst, 4 * i, 1);
            else unreached(0x0087f506, "a bool other than F / f / 0 / T");
        }
        const char* c = strchr(p, ',');
        if (!c) { if (i != n - 1) unreached(0x0087f72d, "a short value list"); break; }
        unreached(0x0087f733, "a value list of more than one value");
    }
    return 1;
}
static uint32_t typed_copy(rx_addr n, rx_addr dst, uint32_t cnt) {
    if (I32(n, 0xc) < (int32_t)cnt) unreached(0x00878e02, "a value array shorter than asked");
    crt_memcpy(dst, U32(n, 4), 4 * cnt);
    return 1;
}
static uint32_t xb_vf51_floats(rx_addr b, rx_addr n, rx_addr dst, uint32_t cnt) {   /* 00878dd0 */
    (void)b;
    int32_t t = (int16_t)U16(n, 8);
    if (t == 1) return parse_list(dst, cnt, U32(n, 4), 5);
    if (t == 5) return typed_copy(n, dst, cnt);
    unreached(0x00878df5, "floats of another type"); return 0;
}
RX_FUNCTION(0x00878dd0, xb_vf51_floats)
static uint32_t xb_vf52_ints(rx_addr b, rx_addr n, rx_addr dst, uint32_t cnt) {     /* 00878d40 */
    (void)b;
    int32_t t = (int16_t)U16(n, 8);
    if (t == 1) return parse_list(dst, cnt, U32(n, 4), 3);
    if (t == 3) return typed_copy(n, dst, cnt);
    unreached(0x00878d6b, "ints of type 0 or another type"); return 0;
}
RX_FUNCTION(0x00878d40, xb_vf52_ints)
static uint32_t xb_vf53_uints(rx_addr b, rx_addr n, rx_addr dst, uint32_t cnt) {    /* 00878be0 */
    (void)b;
    if ((int16_t)U16(n, 8) == 1) return parse_list(dst, cnt, U32(n, 4), 4);
    unreached(0x00878bff, "uints of a typed value"); return 0;
}
RX_FUNCTION(0x00878be0, xb_vf53_uints)
static uint32_t xb_vf54_string(rx_addr b, rx_addr n, rx_addr dst, uint32_t cnt) {   /* 00878b60 */
    (void)b;
    if ((int16_t)U16(n, 8) != 1) unreached(0x00878b95, "the string of a typed value");
    strncpy_term(dst, U32(n, 4), cnt);
    return 1;
}
RX_FUNCTION(0x00878b60, xb_vf54_string)
static rx_addr xb_vf56_cstr(rx_addr b, rx_addr n) {                  /* 00878bb0 */
    (void)b;
    if ((int16_t)U16(n, 8) != 1) unreached(0x00878bd3, "the string of a typed value");
    return U32(n, 4);
}
RX_FUNCTION(0x00878bb0, xb_vf56_cstr)
static uint32_t xb_vf57_bools(rx_addr b, rx_addr n, rx_addr dst, uint32_t cnt) {    /* 00878c60 */
    (void)b;
    int32_t t = (int16_t)U16(n, 8);
    if (t == 1) return parse_list(dst, cnt, U32(n, 4), 0xb);          /* 0087f490 */
    if (t == 0xb) { if (I32(n, 0xc) < (int32_t)cnt) return 0; crt_memcpy(dst, U32(n, 4), 4 * cnt); return 1; }
    unreached(0x00878d03, "bools of another type"); return 0;
}
RX_FUNCTION(0x00878c60, xb_vf57_bools)
/* 008294c0 (EAX = p): blanks (space, \n, \r, \t) skipped */
static rx_addr skip_blanks(rx_addr p) { for (uint8_t c; (c = U8(p, 0)) == ' ' || c == '\n' || c == '\r' || c == '\t'; p++) {} return p; }
static float atof_game(rx_addr p) { char t[64]; uint32_t i = 0; for (; i < 63 && U8(p, i); i++) t[i] = (char)U8(p, i); t[i] = 0; return (float)atof(t); }
static rx_addr strchr_game(rx_addr p, uint8_t c) { for (;; p++) { uint8_t x = U8(p, 0); if (x == c) return p; if (!x) return 0; } }
/* 0087f900 (dst, n, string; cdecl): n vec3 from "x,y,z,x,y,z..." (atof each, blanks after the commas skipped, 16-byte stride, lane 3
 * untouched): 1 when all n were read; the last vector may end without a comma (atof: the host's, as parse_list's: see 12.4) */
static uint32_t parse_vec3_str(rx_addr dst, uint32_t cnt, rx_addr s) {
    rx_addr p = s;
    for (uint32_t k = 0; k < cnt; k++) {
        if (!U8(p, 0)) return 0;
        SF32(dst + 16 * k, 0, atof_game(p));
        if (!(p = strchr_game(p, ','))) return 0;
        p = skip_blanks(p + 1);
        if (!U8(p, 0)) return 0;
        SF32(dst + 16 * k, 4, atof_game(p));
        if (!(p = strchr_game(p, ','))) return 0;
        p = skip_blanks(p + 1);
        if (!U8(p, 0)) return 0;
        SF32(dst + 16 * k, 8, atof_game(p));
        rx_addr c = strchr_game(p, ',');
        if (!c) { if (k != cnt - 1) return 0; p = 0; }
        else p = skip_blanks(c + 1);
    }
    return 1;
}
static uint32_t xb_vf48_vec3(rx_addr b, rx_addr n, rx_addr dst, uint32_t cnt) {     /* 00878f50 */
    (void)b;
    if ((int16_t)U16(n, 8) == 1) return parse_vec3_str(dst, cnt, U32(n, 4));     /* 0087f900 */
    unreached(0x00878f50, "vec3 of a typed value"); return 0;
}
RX_FUNCTION(0x00878f50, xb_vf48_vec3)

/* ======================================================================================================================== */
/* the Registry                                                                                                               */

/* 008240c0 (ECX = registry; builder; RET 4): the nodes with a value under the current node */
static uint32_t count_values(rx_addr b) {
    rx_addr cur = xb_vf2_current(b);
    uint32_t n = 0;
    RX_FRAME_ENTER(F, 4);
    rx_addr e = xb_vf8_first(b, cur, 0, F);
    for (; e; e = xb_vf10_next(b, U32(F, 0))) {
        if (xb_vf41_has_value(b, e)) n++;
        n += count_values(b);
    }
    xb_vf9_release(b, U32(F, 0));
    RX_FRAME_LEAVE(F, 4);
    return n;
}
/* 00823b60 (EDI = entry): the next record of the entry's last block */
static rx_addr new_record(rx_addr E) {
    if (I32(E, 8) >= I32(E, 0xc)) unreached(0x00823b69, "a second record block");
    rx_addr arr = U32(E, 4);
    rx_addr blk = U32(U32(arr, 0), 4 * U32(arr, 4) - 4);
    rx_addr r = blk + 0xc * U32(E, 8);
    SU32(E, 8, U32(E, 8) + 1);
    return r;
}
/* 00824140 (ECX = registry; builder, entry; RET 8): a record per valued node, mapped by node */
static void add_records(rx_addr reg, rx_addr b, rx_addr E) {
    rx_addr cur = xb_vf2_current(b);
    RX_FRAME_ENTER(F, 4);
    rx_addr e = xb_vf8_first(b, cur, 0, F);
    for (; e; e = xb_vf10_next(b, U32(F, 0))) {
        if (xb_vf41_has_value(b, e)) {
            rx_addr r = new_record(E);
            SU32(r, 0, U32(E, 0)); SU32(r, 4, e); SU8(r, 8, 0);
            hash_insert_int(reg + 0x18, e, r);                        /* 0082ac80 */
        }
        add_records(reg, b, E);
    }
    xb_vf9_release(b, U32(F, 0));
    RX_FRAME_LEAVE(F, 4);
}
/* an array {data, count, capacity, growth, 0x7fffffff} push (the inline pushes; growth: 007cff20, capacity += growth) */
static void array_push(rx_addr a, uint32_t v, uint32_t addr) {
    (void)addr;
    const uint32_t cap = U32(a, 8);
    if (I32(a, 4) >= (int32_t)cap) rx_iarr_reserve(a, cap + U32(a, 0xc));   /* 007cff20 */
    SU32(U32(a, 0), 4 * U32(a, 4), v);
    SU32(a, 4, U32(a, 4) + 1);
}
/* 00823f70 (registry, builder; RET 8): the document's entry and records */
static void add_document(rx_addr reg, rx_addr b) {
    rx_addr E = rx_reflex_aligned_alloc(0x10);
    xb_vf1_root(b);
    uint32_t n = count_values(b);
    if (!n) n = 0x19;
    rx_addr cs = rx_reflex_aligned_alloc(0x1c);
    if (!cs) unreached(0x00823fce, "allocation");
    for (uint32_t k = 0; k < 0x18; k += 4) SU32(cs, k, 0);
    SU32(cs, 4, 0xffffffffu);                                         /* InitializeCriticalSection as the reference boot leaves it */
    SU32(E, 0, cs);
    SU32(cs, 0x18, b);
    rx_addr arr = rx_reflex_aligned_alloc(0x14);
    if (!arr) unreached(0x00824020, "allocation");
    SU32(arr, 0, rx_reflex_aligned_alloc(rx_mul_sat(5, 4)));
    SU32(arr, 4, 0); SU32(arr, 8, 5); SU32(arr, 0xc, 5); SU32(arr, 0x10, 0x7fffffff);
    SU32(E, 4, arr); SU32(E, 0xc, n); SU32(E, 8, 0);
    rx_addr blk = rx_reflex_aligned_alloc(rx_mul_sat(n, 0xc));
    array_push(U32(E, 4), blk, 0x00824057);
    xb_vf1_root(b);
    add_records(reg, b, E);
    array_push(reg + 4, E, 0x00824094);
}

rx_addr rx_registry_construct(rx_addr r) {                           /* 00823bd0 */
    SU32(r, 0, 0x0097580c);
    rx_addr a = rx_reflex_aligned_alloc(rx_mul_sat(5, 4));
    SU32(r, 0xc, 5); SU32(r, 0x10, 5);
    SU32(r, 4, a); SU32(r, 8, 0); SU32(r, 0x14, 0x7fffffff);
    hash_construct(r + 0x18, 0x64);                                   /* 0082aba0 */
    rx_addr c = rx_reflex_aligned_alloc(rx_mul_sat(0x19, 4));
    SU32(r, 0x38, c); SU32(r, 0x3c, 0); SU32(r, 0x40, 0x19); SU32(r, 0x44, 0x19); SU32(r, 0x48, 0x7fffffff);
    rx_addr b = rx_reflex_aligned_alloc(0x2aac);
    if (!b) unreached(0x00823c66, "allocation");
    rx_xmlb_construct(b);
    rx_xmlb_add_node(b, 0, 0x0093dab4);                               /* vf81(0, "Registry") */
    add_document(r, b);
    return r;
}

void rx_registry_load_file(fmt_install* in, rx_addr reg, rx_addr name) {   /* 00823e90 */
    char path[256]; uint32_t L = 0;
    for (; L + 1 < sizeof path && U8(name, L); L++) path[L] = (char)U8(name, L);
    path[L] = 0;
    fmt_buf fb = {0};                                                 /* the CRTFileInputStream (fopen / fread / fclose) */
    if (fmt_vfs_read_file(fmt_install_vfs(in), path, &fb) != 0) unreached(0x00823ece, "a registry file missing");
    rx_stream s; rx_stream_init(&s, fb.data, fb.size);
    rx_addr b = rx_reflex_aligned_alloc(0x2aac);
    if (!b) unreached(0x00823eef, "allocation");
    rx_xmlb_construct(b);
    rx_xmlb_load(b, &s);                                              /* 00874530 */
    add_document(reg, b);                                             /* 00823f70 */
    fmt_buf_free(&fb);
}

rx_addr rx_registry_startup(fmt_install* in) {
    RX_FRAME_ENTER(F, 0x480);
    const rx_addr reader = F + 0x20, cur = F + 0x10;
    rx_xml_reader_construct(reader);                                  /* XMLReader_Construct(0) */
    rx_addr reg = rx_reflex_aligned_alloc(0x4c);
    if (reg) rx_registry_construct(reg);
    fmt_buf fb = {0};                                                 /* FUN_00825260("Engine.bxml") */
    if (fmt_vfs_read_file(fmt_install_vfs(in), "Engine.bxml", &fb) != 0) unreached(0x0084e3a3, "Engine.bxml missing");
    rx_stream s; rx_stream_init(&s, fb.data, fb.size);
    rx_xml_reader_load(reader, &s);
    fmt_buf_free(&fb);
    void* first = rx_fn(0x008280b0), *next = rx_fn(0x008281c0), *rel = rx_fn(0x00828190);
    if (!first || !next || !rel) unreached(0x0084e3ca, "the XMLReader cursor virtuals");
    rx_addr root = U32(reader, 0x38);
    SU32(reader, 0x3c, root);
    for (rx_addr e = ((rx_addr (*)(rx_addr, rx_addr, rx_addr, rx_addr))first)(reader, root, 0x0093dab4, cur); e;
         e = ((rx_addr (*)(rx_addr, uint32_t))next)(reader, U32(cur, 0)))
        rx_registry_load_file(in, reg, U32(e, 8));                    /* 00823e90(node value) */
    ((uint32_t (*)(rx_addr, uint32_t))rel)(reader, U32(cur, 0));
    rx_xml_reader_destroy(reader);                                    /* at the end of Engine::Startup (0084e864) */
    RX_FRAME_LEAVE(F, 0x480);
    return reg;
}

/* FUN_007902d0 (ESI = state {+0 flag, +4 token, +8 rest}, EDI = delimiters): the next token */
static rx_addr next_token(rx_addr st, rx_addr delims) {
    rx_addr rest = U32(st, 8);
    if (!rest) return 0;
    if (U32(st, 0)) {
        if (crt_strchr(delims, (int8_t)U8(rest, 0))) unreached(0x007902f1, "a path with a leading separator");
        if (!U8(U32(st, 8), 0)) unreached(0x0079030e, "an empty path rest");
    }
    SU32(st, 4, rest);
    rx_addr d = crt_strpbrk(rest, delims);
    SU32(st, 8, d);
    if (d) { SU8(d, 0, 0); SU32(st, 8, d + 1); if (!U8(d + 1, 0)) SU32(st, 8, 0); }
    return U32(st, 4);
}
rx_addr rx_registry_vf1_find(rx_addr reg, rx_addr path, rx_addr key, uint32_t a3) {   /* 008241f0 (path, key, ?; RET 0xc) */
    (void)a3;
    RX_FRAME_ENTER(F, 0x240);
    const rx_addr st = F + 0x2c, buf = F + 0x38;
    rx_addr result = 0;
    int32_t count = I32(reg, 8);
    for (int32_t i = 0; i < count; i++) {
        rx_addr E = U32(U32(reg, 4), 4 * (uint32_t)i);
        rx_addr b = U32(U32(E, 0), 0x18);
        rx_addr node = xb_vf1_root(b);
        strncpy_term(buf, path, 0x200);
        SU32(st, 0, 1);
        SU32(st, 8, gstrlen(buf) ? buf : 0);
        for (rx_addr tok = next_token(st, 0x00974650); tok; tok = next_token(st, 0x00974650)) {   /* "\" */
            node = xb_vf7_child(b, node, tok);
            if (!node) break;
        }
        if (!node) continue;
        rx_addr leaf = xb_vf7_child(b, node, key);
        if (!leaf || !xb_vf41_has_value(b, leaf)) continue;
        uint32_t rec = key;                                           /* 0082ac10 writes only when found */
        hash_find_int(reg + 0x18, leaf, &rec);
        result = rec;
        RX_FRAME_LEAVE(F, 0x240);
        return result;
    }
    RX_FRAME_LEAVE(F, 0x240);
    return 0;
}
RX_FUNCTION(0x008241f0, rx_registry_vf1_find)
static uint32_t reg_vf2_changed(rx_addr reg, rx_addr rec) { (void)reg; return U8(rec, 8); }   /* 008222e0 */
RX_FUNCTION(0x008222e0, reg_vf2_changed)
void rx_registry_vf3_clear_changed(rx_addr reg) {                    /* 00824390 */
    for (int32_t i = 0; i < I32(reg, 0x3c); i++) SU8(U32(U32(reg, 0x38), 4 * (uint32_t)i), 8, 0);
    SU32(reg, 0x3c, 0);
}
RX_FUNCTION(0x00824390, rx_registry_vf3_clear_changed)
static rx_addr rec_builder(rx_addr rec) { return U32(U32(rec, 0), 0x18); }
static rx_addr reg_vf6_cstr(rx_addr reg, rx_addr rec) { (void)reg; return xb_vf56_cstr(rec_builder(rec), U32(rec, 4)); }   /* 00824440 */
RX_FUNCTION(0x00824440, reg_vf6_cstr)
static uint32_t reg_vf7_int(rx_addr reg, rx_addr rec) {               /* 00824480: one int (its own argument slot) */
    (void)reg;
    RX_FRAME_ENTER(F, 4);
    xb_vf52_ints(rec_builder(rec), U32(rec, 4), F, 1);
    uint32_t v = U32(F, 0);
    RX_FRAME_LEAVE(F, 4);
    return v;
}
RX_FUNCTION(0x00824480, reg_vf7_int)
static uint32_t reg_vf8_bools(rx_addr reg, rx_addr rec, rx_addr d, uint32_t n) { (void)reg; return xb_vf57_bools(rec_builder(rec), U32(rec, 4), d, n); }   /* 00824580 */
RX_FUNCTION(0x00824580, reg_vf8_bools)
static uint32_t reg_vf9_bool(rx_addr reg, rx_addr rec) {               /* 008244c0 */
    (void)reg;
    RX_FRAME_ENTER(F, 4);
    xb_vf57_bools(rec_builder(rec), U32(rec, 4), F, 1);
    uint32_t v = U32(F, 0);
    RX_FRAME_LEAVE(F, 4);
    return v;
}
RX_FUNCTION(0x008244c0, reg_vf9_bool)
static uint32_t reg_vf14_vec3(rx_addr reg, rx_addr rec, rx_addr d, uint32_t n) { (void)reg; return xb_vf48_vec3(rec_builder(rec), U32(rec, 4), d, n); }   /* 00824700 */
RX_FUNCTION(0x00824700, reg_vf14_vec3)
uint32_t rx_registry_vf17_floats(rx_addr reg, rx_addr rec, rx_addr d, uint32_t n) { (void)reg; return xb_vf51_floats(rec_builder(rec), U32(rec, 4), d, n); }   /* 00824640 */
RX_FUNCTION(0x00824640, rx_registry_vf17_floats)
static uint32_t reg_vf18_uints(rx_addr reg, rx_addr rec, rx_addr d, uint32_t n) { (void)reg; return xb_vf53_uints(rec_builder(rec), U32(rec, 4), d, n); }   /* 00824600 */
RX_FUNCTION(0x00824600, reg_vf18_uints)
static uint32_t reg_vf19_ints(rx_addr reg, rx_addr rec, rx_addr d, uint32_t n) { (void)reg; return xb_vf52_ints(rec_builder(rec), U32(rec, 4), d, n); }   /* 008245c0 */
RX_FUNCTION(0x008245c0, reg_vf19_ints)
uint32_t rx_registry_vf20_string(rx_addr reg, rx_addr rec, rx_addr d, uint32_t n) { (void)reg; return xb_vf54_string(rec_builder(rec), U32(rec, 4), d, n); }   /* 00824540 */
RX_FUNCTION(0x00824540, rx_registry_vf20_string)
/* vf30 00824af0 / vf35 008249a0 (record, values, n; RET 0xc): floats / bools set, the record marked changed and listed (+0x38) */
static void mark_changed(rx_addr reg, rx_addr rec) {
    SU8(rec, 8, 1);
    array_push(reg + 0x38, rec, 0x00824b2d);
}
static uint32_t reg_vf30_set_floats(rx_addr reg, rx_addr rec, rx_addr s, uint32_t n) {
    set_floats(rec_builder(rec), U32(rec, 4), s, n); mark_changed(reg, rec); return 1;
}
RX_FUNCTION(0x00824af0, reg_vf30_set_floats)
static uint32_t reg_vf35_set_bools(rx_addr reg, rx_addr rec, rx_addr s, uint32_t n) {
    set_bools(rec_builder(rec), U32(rec, 4), s, n); mark_changed(reg, rec); return 1;
}
RX_FUNCTION(0x008249a0, reg_vf35_set_bools)
