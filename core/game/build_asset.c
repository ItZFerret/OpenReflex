/* build_asset.c -- see build_asset.h. Hand transliteration of the Database's construction path from the disassembly: the hash tables
 * (0084a1f0, 005c3400, 007cfaf0, 007fa250, 0084db60, 0084dea0, 0084dc30, 0084dd30, 0084df50, 0084dfe0, 0084da40 / dcc0 / ddc0), the
 * constructor 0084a2d0, vf24 0084ab30, vf8 0084a890, the loader's database / package requests (Fn_0084b0a0, Database_LoaderThreadProc
 * case 1 + 0084c6b0 + Fn_0084bc70, FUN_0084c550), vf30 0084ad10, vf23 0084aa10, vf31 0084af30, the AssetIO handle constructors
 * (AssetIO::vf1 of the 36 types) and the startup registration (0084d140 + 0041bed0).
 * Strings the original takes from its XMLReader or its callers are host strings here (read from the fmt readers / given by the
 * caller); everything stored in game memory is written as the original writes it (strncpy zero padding, the 0x3f / 0x103 limits). */
#include "build_asset.h"
#include "map_scale.h"
#include "fx_material_binding.h"
#include "build_alloc.h"
#include "build_anim.h"
#include "build_load.h"
#include "loop_platform.h"
#include "build_world.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/rx_os.h"
#ifndef RX_ORACLE
#include "../src/fmt_db.h"
#include "../src/fmt_bxml.h"
#endif

#define STD_ALLOCATOR_VT 0x009753b0u   /* StdMemoryAllocator::vftable */
#define FLOAT_ONE_HALF   0x3f000000u

static void (*g_trace)(const char*) = NULL;
void rx_asset_trace(void (*fn)(const char* line)) { g_trace = fn; }
static void (*g_between)(rx_addr db) = NULL;
void rx_asset_between_loads(void (*fn)(rx_addr db)) { g_between = fn; }

/* ---- strings in game memory ---- */

/* _stricmp in the "C" locale (msvcr90 without setlocale: only A..Z fold) */
static int fold(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static int g_icmp(rx_addr a, rx_addr b) {
    for (;; a++, b++) { int x = fold(U8(a, 0)), y = fold(U8(b, 0)); if (x != y || !x) return x - y; }
}
static int g_icmp_host(rx_addr a, const char* b) {
    for (;; a++, b++) { int x = fold(U8(a, 0)), y = fold((uint8_t)*b); if (x != y || !x) return x - y; }
}
/* strncpy(dst, src, n): up to n bytes, the rest of the n zero */
static void g_strncpy_host(rx_addr d, const char* s, uint32_t n) {
    uint32_t k = 0;
    for (; k < n && s[k]; k++) SU8(d, k, (uint8_t)s[k]);
    for (; k < n; k++) SU8(d, k, 0);
}
static void g_strncpy(rx_addr d, rx_addr s, uint32_t n) {
    uint32_t k = 0;
    for (; k < n && U8(s, k); k++) SU8(d, k, U8(s, k));
    for (; k < n; k++) SU8(d, k, 0);
}
/* strncat(dst, src, n) */
static void g_strncat_host(rx_addr d, const char* s, uint32_t n) {
    while (U8(d, 0)) d++;
    uint32_t k = 0;
    for (; k < n && s[k]; k++) SU8(d, k, (uint8_t)s[k]);
    SU8(d, k, 0);
}
static void g_memset(rx_addr d, uint8_t v, uint32_t n) { for (uint32_t k = 0; k < n; k++) SU8(d, k, v); }

/* the type-tag folding of Fn_0084b0a0: the table at 0x009b77c8 (Windows-1252 lower case) */
static uint8_t fold1252(uint8_t c) {
    if ((c >= 'A' && c <= 'Z') || (c >= 0xc0 && c <= 0xde && c != 0xd7)) return (uint8_t)(c + 32);
    if (c == 0x8a || c == 0x8c || c == 0x8e) return (uint8_t)(c + 16);
    if (c == 0x9f) return 0xff;
    return c;
}
/* strncpy(tag, type, 8), folded: the 8-byte key */
static void make_tag(const char* type, uint32_t* lo, uint32_t* hi) {
    uint8_t b[8] = {0};
    for (int k = 0; k < 8 && type[k]; k++) b[k] = (uint8_t)type[k];
    for (int k = 0; k < 8 && b[k]; k++) b[k] = fold1252(b[k]);
    *lo = (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
    *hi = (uint32_t)b[4] | (uint32_t)b[5] << 8 | (uint32_t)b[6] << 16 | (uint32_t)b[7] << 24;
}
uint32_t rx_asset_tag_lo(const char* type) { uint32_t lo, hi; make_tag(type, &lo, &hi); return lo; }
uint32_t rx_asset_tag_hi(const char* type) { uint32_t lo, hi; make_tag(type, &lo, &hi); return hi; }

/* ---- StdMemoryAllocator (the tables' allocator: vf1 004eef20 = ReflexAlignedAlloc, vf2 00775540 = ReflexAlignedFree) ---- */
static rx_addr alloc_vf1(rx_addr a, uint32_t n) {
    if (U32(a, 0) != STD_ALLOCATOR_VT) { fprintf(stderr, "rxcore: table allocator %08x is not a StdMemoryAllocator\n", a); abort(); }
    return rx_reflex_aligned_alloc(n);
}
static void alloc_vf2(rx_addr a, rx_addr p) { (void)a; rx_reflex_aligned_free(p); }

/* ---- hash tables ---- */

static void htab_init(rx_addr t, uint32_t n) {
    rx_addr a = rx_reflex_aligned_alloc(4);
    if (a) SU32(a, 0, STD_ALLOCATOR_VT);
    SU32(t, 0x18, a);
    SU8(t, 0x1c, 1);
    SU32(t, 0x00, n);
    SU32(t, 0x04, FLOAT_ONE_HALF);
    SU32(t, 0x10, 0);
    SU32(t, 0x14, 0xffffffffu);
    SU32(t, 0x08, 0);
    rx_addr b = alloc_vf1(U32(t, 0x18), 4 * n);
    SU32(t, 0x0c, b);
    g_memset(b, 0, 4 * U32(t, 0));
}
void rx_htab_init_50(rx_addr t) { htab_init(t, 50); }
RX_FUNCTION(0x0084ddc0, rx_htab_init_50);
void rx_htab_init_5(rx_addr t) { htab_init(t, 5); }
RX_FUNCTION(0x0084dcc0, rx_htab_init_5);
void rx_htab_init_500(rx_addr t) { htab_init(t, 500); }
RX_FUNCTION(0x0084da40, rx_htab_init_500);

/* 0084a1f0: Wang-style mix of each word (arithmetic shifts), summed */
static uint32_t mix(uint32_t x) {
    uint32_t e = ~(x << 15) + x;
    e = (uint32_t)((int32_t)e >> 10) ^ e;
    e = e * 9;
    e ^= (uint32_t)((int32_t)e >> 6);
    e += ~(e << 11);
    return (uint32_t)((int32_t)e >> 16) ^ e;
}
uint32_t rx_tag_hash(uint32_t lo, uint32_t hi) { return mix(hi) + mix(lo); }
RX_FUNCTION(0x0084a1f0, rx_tag_hash);

/* 005c3400: h = h * 16 + tolower(c) (movsx: signed chars), the top nibble folded in */
static uint32_t hash_step(uint32_t h, int8_t c) {
    int32_t lc = (c >= 'A' && c <= 'Z') ? c + 32 : c;   /* tolower, C locale */
    h = (h << 4) + (uint32_t)lc;
    uint32_t top = h & 0xf0000000u;
    if (top) h ^= (top >> 24) ^ top;
    return h;
}
uint32_t rx_string_hash_nocase(rx_addr s) { uint32_t h = 0; for (; U8(s, 0); s++) h = hash_step(h, (int8_t)U8(s, 0)); return h; }
RX_FUNCTION(0x005c3400, rx_string_hash_nocase);
static uint32_t hash_host(const char* s) { uint32_t h = 0; for (; *s; s++) h = hash_step(h, (int8_t)*s); return h; }

/* the load check before an insert: count / n > max load (float compare, comiss) -> (int)(n * 1.5f) buckets */
static int over_load(rx_addr t) { float n = (float)I32(t, 0), c = (float)I32(t, 8); return c / n > F32(t, 4); }
static uint32_t grown(rx_addr t) { return (uint32_t)(int32_t)((float)I32(t, 0) * 1.5f /* 0x009322a4 */); }

static void rehash(rx_addr t, uint32_t n, uint32_t hash_off, uint32_t next_off) {
    rx_addr nb = alloc_vf1(U32(t, 0x18), 4 * n);
    g_memset(nb, 0, 4 * n);
    for (int32_t i = 0; i < I32(t, 0); i++) {
        rx_addr e = U32(U32(t, 0xc), 4u * (uint32_t)i);
        while (e) {
            uint32_t b = U32(e, hash_off) % n;
            rx_addr next = U32(e, next_off);
            SU32(e, next_off, U32(nb, 4 * b));
            SU32(nb, 4 * b, e);
            e = next;
        }
    }
    alloc_vf2(U32(t, 0x18), U32(t, 0xc));
    SU32(t, 0xc, nb);
    SU32(t, 0, n);
}
void rx_tagtab_rehash(rx_addr t, uint32_t n) { rehash(t, n, 0xc, 0x10); }
RX_FUNCTION(0x0084dfe0, rx_tagtab_rehash);
void rx_strtab_rehash(rx_addr t, uint32_t n) { rehash(t, n, 8, 0xc); }
RX_FUNCTION(0x0084df50, rx_strtab_rehash);

int rx_tagtab_find(rx_addr t, uint32_t lo, uint32_t hi, uint32_t* value) {
    uint32_t h = rx_tag_hash(lo, hi);
    for (rx_addr e = U32(U32(t, 0xc), 4 * (h % U32(t, 0))); e; e = U32(e, 0x10))
        if (U32(e, 0xc) == h && U32(e, 0) == lo && U32(e, 4) == hi) { *value = U32(e, 8); return 1; }
    return 0;
}
RX_FUNCTION(0x0084dea0, rx_tagtab_find);

void rx_tagtab_insert(rx_addr t, uint32_t lo, uint32_t hi, uint32_t value) {
    if (over_load(t)) rx_tagtab_rehash(t, grown(t));
    uint32_t h = rx_tag_hash(lo, hi), b = h % U32(t, 0);
    for (rx_addr e = U32(U32(t, 0xc), 4 * b); e; e = U32(e, 0x10))
        if (U32(e, 0xc) == h && U32(e, 0) == lo && U32(e, 4) == hi) { SU32(e, 8, value); return; }
    rx_addr e = alloc_vf1(U32(t, 0x18), 0x18);
    if (e) SU32(e, 0x10, 0);
    SU32(e, 0, lo); SU32(e, 4, hi); SU32(e, 0xc, h); SU32(e, 8, value);
    SU32(e, 0x10, U32(U32(t, 0xc), 4 * b));
    SU32(U32(t, 0xc), 4 * b, e);
    SU32(t, 8, U32(t, 8) + 1);
}
RX_FUNCTION(0x0084db60, rx_tagtab_insert);

int rx_tagtab_remove(rx_addr t, uint32_t lo, uint32_t hi) {
    uint32_t h = rx_tag_hash(lo, hi);
    rx_addr slot = U32(t, 0xc) + 4 * (h % U32(t, 0));
    rx_addr e = U32(slot, 0);
    for (; e; e = U32(e, 0x10)) if (U32(e, 0xc) == h && U32(e, 0) == lo && U32(e, 4) == hi) break;
    if (!e) return 0;
    if (U32(slot, 0) == e) SU32(slot, 0, U32(e, 0x10));
    else { rx_addr p = U32(slot, 0); while (U32(p, 0x10) != e) p = U32(p, 0x10); SU32(p, 0x10, U32(e, 0x10)); }
    alloc_vf2(U32(t, 0x18), e);
    SU32(t, 8, U32(t, 8) - 1);
    return 1;
}
RX_FUNCTION(0x0084dc30, rx_tagtab_remove);

int rx_strtab_find(rx_addr t, rx_addr key, uint32_t* value) {
    uint32_t h = rx_string_hash_nocase(key);
    for (rx_addr e = U32(U32(t, 0xc), 4 * (h % U32(t, 0))); e; e = U32(e, 0xc))
        if (U32(e, 8) == h && g_icmp(U32(e, 0), key) == 0) { *value = U32(e, 4); return 1; }
    return 0;
}
/* 007fa250 is registered by its race-frame port (race_misc_util.c rx_name_hash_find, the same lookup): not again here */
static int strtab_find_host(rx_addr t, const char* key, uint32_t* value) {
    uint32_t h = hash_host(key);
    for (rx_addr e = U32(U32(t, 0xc), 4 * (h % U32(t, 0))); e; e = U32(e, 0xc))
        if (U32(e, 8) == h && g_icmp_host(U32(e, 0), key) == 0) { *value = U32(e, 4); return 1; }
    return 0;
}

void rx_strtab_insert(rx_addr t, rx_addr key, uint32_t value) {
    if (over_load(t)) rx_strtab_rehash(t, grown(t));
    uint32_t h = rx_string_hash_nocase(key), b = h % U32(t, 0);
    for (rx_addr e = U32(U32(t, 0xc), 4 * b); e; e = U32(e, 0xc))
        if (U32(e, 8) == h && g_icmp(U32(e, 0), key) == 0) { SU32(e, 4, value); return; }
    rx_addr e = alloc_vf1(U32(t, 0x18), 0x10);
    if (e) SU32(e, 0xc, 0);
    SU32(e, 0, key); SU32(e, 4, value); SU32(e, 8, h);
    SU32(e, 0xc, U32(U32(t, 0xc), 4 * b));
    SU32(U32(t, 0xc), 4 * b, e);
    SU32(t, 8, U32(t, 8) + 1);
}
RX_FUNCTION(0x007cfaf0, rx_strtab_insert);
/* 0084df50 (stdcall table, n; RET 8) with the original's frame, for the insert below: push ebx, ebp (EBP = the table), esi (ESI = 4n);
 * the allocator's vf1 004eef20 (push 4n; return 0084df6a: the lock, push esi; push 0x10, size; call [_aligned_malloc], return
 * 004eef42; the lock left, return 004eef5b); memset (push 4n, 0, nb; return 0084df75); the move (push edi; the bucket index in the
 * table argument's slot); vf2 00775540 (push old; return 0084dfc9: the lock, push p; call [_aligned_free], return 0077555f; the
 * leave a tail jump). In the oracle _aligned_malloc / memset run the originals with the original's registers. */
static void strtab_rehash_at(rx_addr R, crt_regs4 r, rx_addr t, uint32_t n) {
    const int lock = U8(0, 0x00da67d8) != 0;
    SU32(R, 4, t); SU32(R, 8, n);
    SU32(R, -4, r.ebx); SU32(R, -8, r.ebp); SU32(R, -0xc, r.esi);
    const rx_addr a = U32(t, 0x18);
    if (U32(a, 0) != STD_ALLOCATOR_VT) { fprintf(stderr, "rxcore: table allocator %08x is not a StdMemoryAllocator\n", a); abort(); }
    const uint32_t size = 4u * n;
    SU32(R, -0x10, size); SU32(R, -0x14, 0x0084df6a);
    const rx_addr E4 = R - 0x14;
    if (lock) { const rx_addr sp = rx_sp(); rx_set_sp(E4); loop_enter_cs(0x00d61704); rx_set_sp(sp); SU32(E4, -4, 0x00d61704); SU32(E4, -8, 0x004eef34); }
    SU32(E4, -4, size); SU32(E4, -8, 0x10); SU32(E4, -0xc, size); SU32(E4, -0x10, 0x004eef42);
    rx_addr nb;
    {
        const rx_addr sp = rx_sp(); rx_set_sp(E4 - 4);
#ifdef RX_ORACLE
        const uint32_t args[2] = {size, 0x10};
        nb = rx_call_original_regs(U32(0, 0x00916358), r.ebx, t, size, r.edi, 2, args);
#else
        nb = rx_crt_aligned_malloc(size, 16);
#endif
        rx_set_sp(sp);
        SU32(E4, -0x10, 0x004eef42);
    }
    if (lock) { const rx_addr sp = rx_sp(); rx_set_sp(E4 - 4); loop_leave_cs(0x00d61704); rx_set_sp(sp); SU32(E4, -8, 0x00d61704); SU32(E4, -0xc, 0x004eef5b); }
    /* memset(nb, 0, 4n) */
    SU32(R, -0x10, size); SU32(R, -0x14, 0); SU32(R, -0x18, nb); SU32(R, -0x1c, 0x0084df75);
    {
        const rx_addr sp = rx_sp(); rx_set_sp(R - 0xc);
#ifdef RX_ORACLE
        const uint32_t args[3] = {nb, 0, size};
        (void)rx_call_original_regs(U32(0, 0x00916298), nb, t, size, r.edi, 3, args);
#else
        g_memset(nb, 0, size);
#endif
        rx_set_sp(sp);
        SU32(R, -0x1c, 0x0084df75);
    }
    SU32(R, 4, 0);
    uint32_t esi = size;
    if (I32(t, 0) > 0) {
        SU32(R, -0x10, r.edi);
        for (int32_t i = 0; i < I32(t, 0); i++) {
            rx_addr e = U32(U32(t, 0xc), 4u * (uint32_t)i);
            while (e) {
                esi = e;
                const uint32_t b = U32(e, 8) % n;
                const rx_addr next = U32(e, 0xc);
                SU32(e, 0xc, U32(nb, 4 * b));
                SU32(nb, 4 * b, e);
                e = next;
            }
            SU32(R, 4, (uint32_t)i + 1);
        }
    }
    /* vf2: push old; call (return 0084dfc9) */
    const rx_addr old = U32(t, 0xc);
    SU32(R, -0x10, old); SU32(R, -0x14, 0x0084dfc9);
    const rx_addr E5 = R - 0x14;
    if (lock) { const rx_addr sp = rx_sp(); rx_set_sp(E5); loop_enter_cs(0x00d61704); rx_set_sp(sp); SU32(E5, -4, 0x00d61704); SU32(E5, -8, 0x00775554); }
    SU32(E5, -4, old); SU32(E5, -8, 0x0077555f);
    { const crt_regs4 f = {nb, t, esi, r.edi}; crt_aligned_free_at(E5 - 8, f, old); }
    if (lock) { const rx_addr sp = rx_sp(); rx_set_sp(E5 + 8); loop_leave_cs(0x00d61704); rx_set_sp(sp); SU32(E5, 4, 0x00d61704); SU32(E5, 0, 0x0084dfc9); }   /* (mov [esp + 4], cs; jmp: the return slot stays) */
    SU32(t, 0xc, nb);
    SU32(t, 0, n);
}

/* 007cfaf0 with the original's frame (build_asset.h): push ecx, ebx (EBX = &key), ebp, esi; [e - 4] = &count; the rehash when
 * over its load (0084df50: push count, table; return 007cfb25); String_HashNoCase (return 007cfb2c; the hash over the &key
 * argument slot); per same-hash node _stricmp (return 007cfb53; EBP = the bucket, ESI = the node); a new node from the
 * allocator's vf1 004eef20 (push 0x10; return 007cfb71: the lock, push esi; push 0x10, size; call [_aligned_malloc], return
 * 004eef42 (the original in the oracle, with the original's registers); the lock left, return 004eef5b) */
void rx_strtab_insert_at(rx_addr e, crt_regs4 r, uint32_t ecx, rx_addr t, rx_addr keyp, rx_addr valp) {
    SU32(e, 4, keyp); SU32(e, 8, valp);
    SU32(e, -4, ecx); SU32(e, -8, r.ebx); SU32(e, -0xc, r.ebp); SU32(e, -0x10, r.esi);
    SU32(e, -4, t + 8);
    if (over_load(t)) {
        const uint32_t n = grown(t);
        SU32(e, -0x14, n); SU32(e, -0x18, t); SU32(e, -0x1c, 0x007cfb25);
        const crt_regs4 rr = {r.ebx, r.ebp, r.esi, t};
        strtab_rehash_at(e - 0x1c, rr, t, n);
    }
    const rx_addr key = U32(keyp, 0);
    SU32(e, -0x14, 0x007cfb2c);
    const crt_regs4 hr = {keyp, r.ebp, r.esi, t};
    const uint32_t h = rx_hash_nocase_at(e - 0x14, hr, key), b = h % U32(t, 0);
    SU32(e, 4, h);
    rx_addr n = U32(U32(t, 0xc), 4 * b);
    for (; n; n = U32(n, 0xc)) {
        if (U32(n, 8) != h) continue;
        SU32(e, -0x14, U32(keyp, 0)); SU32(e, -0x18, U32(n, 0));
        const crt_regs4 c = {keyp, b, n, t};
        if (rx_stricmp_call_at(e - 0x1c, 0x007cfb53, c, U32(n, 0), U32(keyp, 0)) == 0) { SU32(n, 4, U32(valp, 0)); return; }
    }
    const rx_addr a = U32(t, 0x18);
    if (U32(a, 0) != STD_ALLOCATOR_VT) { fprintf(stderr, "rxcore: table allocator %08x is not a StdMemoryAllocator\n", a); abort(); }
    /* push 0x10; call [vf1] (return 007cfb71) */
    SU32(e, -0x14, 0x10); SU32(e, -0x18, 0x007cfb71);
    const rx_addr E4 = e - 0x18;
    if (U8(0, 0x00da67d8) != 0) {
        const rx_addr sp = rx_sp(); rx_set_sp(E4); loop_enter_cs(0x00d61704); rx_set_sp(sp);
        SU32(E4, -4, 0x00d61704); SU32(E4, -8, 0x004eef34);
    }
    SU32(E4, -4, 0); SU32(E4, -8, 0x10); SU32(E4, -0xc, 0x10); SU32(E4, -0x10, 0x004eef42);   /* push esi (0: the chain's end); push 0x10, size */
    rx_addr p;
    {
        const rx_addr sp = rx_sp(); rx_set_sp(E4 - 4);
#ifdef RX_ORACLE
        const uint32_t args[2] = {0x10, 0x10};
        p = rx_call_original_regs(U32(0, 0x00916358), keyp, b, 0, t, 2, args);
#else
        p = rx_crt_aligned_malloc(0x10, 16);
#endif
        rx_set_sp(sp);
        SU32(E4, -0x10, 0x004eef42);
    }
    if (U8(0, 0x00da67d8) != 0) {
        const rx_addr sp = rx_sp(); rx_set_sp(E4 - 4); loop_leave_cs(0x00d61704); rx_set_sp(sp);   /* (add esp, 8; push; call) */
        SU32(E4, -8, 0x00d61704); SU32(E4, -0xc, 0x004eef5b);
    }
    if (p) SU32(p, 0xc, 0);
    SU32(p, 0, key); SU32(p, 4, U32(valp, 0)); SU32(p, 8, h);
    SU32(p, 0xc, U32(U32(t, 0xc), 4 * b));
    SU32(U32(t, 0xc), 4 * b, p);
    SU32(t, 8, U32(t, 8) + 1);
}

/* 0084dd30 (EDI = the table; stdcall: the ADDRESS of the key slot, which holds the string pointer; RET 4) -> EAX 1 when the entry was
 * removed (its node freed through the allocator vf2, the count - 1), else 0 */
int rx_strtab_remove(rx_addr t, rx_addr key_slot) {
    const rx_addr key = U32(key_slot, 0);
    uint32_t h = rx_string_hash_nocase(key);
    rx_addr slot = U32(t, 0xc) + 4 * (h % U32(t, 0));
    rx_addr e = U32(slot, 0);
    for (; e; e = U32(e, 0xc)) if (U32(e, 8) == h && g_icmp(U32(e, 0), key) == 0) break;
    if (!e) return 0;
    if (U32(slot, 0) == e) SU32(slot, 0, U32(e, 0xc));
    else { rx_addr p = U32(slot, 0); while (U32(p, 0xc) != e) p = U32(p, 0xc); SU32(p, 0xc, U32(e, 0xc)); }
    alloc_vf2(U32(t, 0x18), e);
    SU32(t, 8, U32(t, 8) - 1);
    return 1;
}
RX_FUNCTION(0x0084dd30, rx_strtab_remove);

/* ---- AssetIO types (the 36 the game registers) ---- */

typedef struct {
    const char* tag;        /* the type tag (AssetIO::vf2 / the handles' vf1) */
    rx_addr io;             /* the AssetIO object (static in .data; material: 0 = the heap singleton at [0x00da67fc]) */
    rx_addr guard;          /* its static-init guard (bit 0) */
    uint32_t io_vt;         /* the AssetIO vtable */
    uint32_t handle_size;   /* AssetIO::vf1: ReflexAlignedAlloc(size) */
    uint32_t handle_vt;     /* the handle class */
    rx_asset_loader loader; /* the core's AssetIO::vf3, if any */
} io_type;

/* in the order of the startup registration (the trace of the reference boot: core/notes/construction.md section 8) */
static io_type g_types[] = {
    {"subscene", 0x00da6834, 0x00da6838, 0x00972854, 0x50, 0x00972870, NULL},
    {"material", 0,          0x00da6800, 0x00971f4c, 0x50, 0x0095b4f4, NULL},
    {"texture",  0x00da67e8, 0x00da67f8, 0x00971c24, 0x50, 0x009719c8, NULL},
    {"surface",  0x00da67f0, 0x00da67e4, 0x00971c08, 0x50, 0x00971a60, NULL},
    {"model",    0x00da682c, 0x00da681c, 0x009728c0, 0x50, 0x00972898, NULL},
    {"morph",    0x00da67dc, 0x00da67ec, 0x00971af0, 0x50, 0x00971a30, NULL},
    {"anim",     0x00da68a4, 0x00da689c, 0x00975ecc, 0x50, 0x00975e94, NULL},
    {"collis",   0x00da6884, 0x00da6880, 0x00974f38, 0x50, 0x0096dd60, NULL},
    {"sound",    0x00da686c, 0x00da6860, 0x00973bb8, 0x50, 0x00973a28, NULL},
    {"soundbnk", 0x00da6864, 0x00da685c, 0x00973bd4, 0x50, 0x00973a60, NULL},
    {"sounddat", 0x00da6868, 0x00da6858, 0x00973bf0, 0x50, 0x00973a00, NULL},
    {"bxml",     0x00da6894, 0x00da6898, 0x00975738, 0x58, 0x0097567c, NULL},
    {"shader",   0x00da67e0, 0x00da67f4, 0x00971bec, 0x50, 0x00971aa4, NULL},
    {"action",   0x00da68ac, 0x00da68b0, 0x00975e50, 0x50, 0x00975db4, NULL},
    {"cell",     0x00da6848, 0x00da6854, 0x00972d24, 0x50, 0x00972cdc, NULL},
    {"water",    0x00da6810, 0x00da6828, 0x00972a88, 0x50, 0x00972a60, NULL},
    {"particle", 0x00da6818, 0x00da6820, 0x00972970, 0x50, 0x00972948, NULL},
    {"texatlas", 0x00d94904, 0x00d94a78, 0x009507f8, 0x50, 0x00972d50, NULL},
    {"icongeom", 0x00da6850, 0,          0x00972d88, 0x50, 0x00972bf8, NULL},
    {"banner",   0x00da680c, 0x00da6814, 0x00972918, 0x50, 0x009728f0, NULL},
    {"perfdat",  0x00da68a0, 0x00da68a8, 0x00976204, 0x50, 0x00975ffc, NULL},
    {"packdata", 0x00da6824, 0x00da6830, 0x00972bb8, 0x50, 0x00972aa4, NULL},
    {"localiz",  0x00d72aa4, 0x00d72aa0, 0x0093a06c, 0x50, 0x009503dc, NULL},
    {"grass",    0x00d72a74, 0x00d72a6c, 0x0093a088, 0x50, 0x0095b6a4, NULL},
    {"tree",     0x00d72a68, 0x00d72a58, 0x0093a0a4, 0x50, 0x0095b52c, NULL},
    {"forest",   0x00d72a5c, 0x00d72a70, 0x0093a0c0, 0x50, 0x0095b56c, NULL},
    {"frstmtl",  0x00d72a64, 0x00d72a60, 0x0093a0dc, 0x50, 0x0095b66c, NULL},
    {"trncol",   0x00d72a38, 0x00d72a40, 0x0093a0f8, 0x50, 0x0096252c, NULL},
    {"terrain",  0x00d72a3c, 0x00d72a44, 0x0093a114, 0x50, 0x009624f4, NULL},
    {"adv",      0x00d72a4c, 0x00d72a48, 0x0093a130, 0x50, 0x00960bb4, NULL},
    {"tbldata",  0x00d72a90, 0x00d72a94, 0x0093a184, 0x50, 0x0093eb94, NULL},
    {"script",   0x00d72a9c, 0x00d72a98, 0x0093a14c, 0x50, 0x00965774, NULL},
    {"uicmpnt",  0x00d72a7c, 0x00d72a78, 0x0093a168, 0x50, 0x00951864, NULL},
    {"tdf",      0x00d72a8c, 0x00d72a88, 0x0093a1a0, 0x50, 0x009402a8, NULL},
    {"bink",     0x00d72a80, 0x00d72a84, 0x0093a1bc, 0x50, 0x00946270, NULL},
    {"partattr", 0x00d72a54, 0x00d72a50, 0x0093a1d8, 0x50, 0x0095df0c, NULL},
};
#define NTYPES ((int)(sizeof g_types / sizeof g_types[0]))

static io_type* type_by_io_vt(uint32_t vt) { for (int i = 0; i < NTYPES; i++) if (g_types[i].io_vt == vt) return &g_types[i]; return NULL; }
static io_type* type_by_handle_vt(uint32_t vt) { for (int i = 0; i < NTYPES; i++) if (g_types[i].handle_vt == vt) return &g_types[i]; return NULL; }

/* the core's AssetIO::vf3 ports (one per type as the loader units land) */
static rx_addr anim_vf3(rx_stream* s, rx_addr name, uint32_t lo, uint32_t hi) { (void)name; (void)lo; (void)hi; return rx_anim_asset_io_create(s); }
void rx_asset_register_core_loaders(void) {
    rx_asset_set_loader(0x00975ecc, anim_vf3);   /* AnimAssetIO::vf3 0082cc70 (build_anim.c) */
    rx_load_register();                          /* the asset loaders of build_load.c (units 2b-2g) */
}

void rx_asset_set_loader(rx_addr io_vtable, rx_asset_loader fn) {
    io_type* t = type_by_io_vt(io_vtable);
    if (!t) { fprintf(stderr, "rxcore: rx_asset_set_loader: no AssetIO with vtable %08x\n", io_vtable); abort(); }
    t->loader = fn;
}

/* AssetIO::vf1 of each type: ReflexAlignedAlloc(size), +0x44 / +0x48 / +0x4c zero, the handle vtable (BXMLAssetIO::vf1 00822280
 * also keeps the tag at +0x50 / +0x54; IconGeometry / TextureAtlas / UIComponent first run the AssetHandle constructor 0084a2a0,
 * the same final fields) */
rx_addr rx_asset_io_new_handle(rx_addr io, uint32_t lo, uint32_t hi) {
    io_type* t = type_by_io_vt(U32(io, 0));
    if (!t) { fprintf(stderr, "rxcore: AssetIO %08x (vtable %08x) unknown\n", io, U32(io, 0)); abort(); }
    rx_addr h = rx_reflex_aligned_alloc(t->handle_size);
    if (!h) return 0;
    SU32(h, 0x44, 0); SU32(h, 0x48, 0); SU32(h, 0x4c, 0);
    SU32(h, 0, t->handle_vt);
    if (t->handle_size == 0x58) { SU32(h, 0x50, lo); SU32(h, 0x54, hi); }
    return h;
}

/* the handle's type tag (its vf1: a constant per class; BXMLAssetHandle::vf1 008221a0 returns +0x50 / +0x54) */
static void handle_tag(rx_addr h, uint32_t* lo, uint32_t* hi) {
    io_type* t = type_by_handle_vt(U32(h, 0));
    if (!t) { fprintf(stderr, "rxcore: handle %08x has an unknown class (vtable %08x)\n", h, U32(h, 0)); abort(); }
    if (t->handle_size == 0x58) { *lo = U32(h, 0x50); *hi = U32(h, 0x54); return; }
    make_tag(t->tag, lo, hi);
}

/* ---- the Database ---- */

static void critical_section_init(rx_addr cs) {   /* InitializeCriticalSection as the reference boot leaves it: {0, -1, 0, 0, 0, 0} */
    SU32(cs, 0, 0); SU32(cs, 4, 0xffffffffu); SU32(cs, 8, 0); SU32(cs, 0xc, 0); SU32(cs, 0x10, 0); SU32(cs, 0x14, 0);
}

/* Fn_0084d980 (ECX = v): a std::vector's container proxy (_Container_base: a 4-byte block pointing back), begin / end / end of
 * storage 0 */
static void vector_construct(rx_addr v) {
    rx_addr p = rx_reflex_aligned_alloc(4);
    if (p) { SU32(p, 0, v); SU32(v, 0, p); } else SU32(v, 0, 0);
    SU32(v, 0xc, 0); SU32(v, 0x10, 0); SU32(v, 0x14, 0);
}

rx_addr rx_database_construct(rx_addr db) {
    SU32(db, 0, 0x0097695c);   /* Database::vftable */
    rx_htab_init_50(db + 4);
    rx_htab_init_5(db + 0x24);
    rx_htab_init_50(db + 0x44);
    vector_construct(db + 0x64); vector_construct(db + 0x7c);
    SU32(db, 0xe8, 0); SU32(db, 0xf8, 0); SU32(db, 0xfc, 0);
    g_memset(db + 0x62b24, 0, 0x60);
    g_memset(db + 0x83b20, 0, 0x18);
    SU32(db, 0x83b38, 0); SU32(db, 0x83b3c, 0); SU32(db, 0x83b48, 0);
    g_memset(db + 0x94, 0, 0x40);
    g_memset(db + 0xd4, 0, 0x11);
    g_strncpy_host(db + 0x94, "None", 0x3f);
    g_strncpy_host(db + 0xd4, "None", 0x10);
    g_memset(db + 0x104, 0, 0x62a20);
    /* CreateEventA "DatabaseThreadEvent" -> +0xf8 (an OS handle: not made by the core) */
    SU32(db, 0xec, 0); SU32(db, 0xf0, 0); SU32(db, 0xf4, 0);
    critical_section_init(db + 0x62b24); critical_section_init(db + 0x62b3c);
    critical_section_init(db + 0x62b54); critical_section_init(db + 0x62b6c);
    SU8(db, 0x62b84, 0);
    /* FUN_0084ce80: the cache thread's state (its events: OS handles) */
    critical_section_init(db + 0x83b20);
    SU32(db, 0x82b88, 0); SU32(db, 0x83b44, 0); SU32(db, 0x83b18, 0); SU32(db, 0x83b1c, 0);
    g_memset(db + 0x82b8c, 0, 0xf8c);
    /* CreateThread(Database_LoaderThreadProc) -> +0xfc, its id at +0x100: the core loads on the calling thread */
    return db;
}
RX_FUNCTION(0x0084a2d0, rx_database_construct);

rx_addr rx_database_create(void) {
    rx_addr db = rx_reflex_aligned_alloc(RX_DATABASE_SIZE);
    if (db) rx_load_set_database(db);   /* the loaders look handles up in it ([[0xa95844] + 8] in the original) */
    return db ? rx_database_construct(db) : 0;
}

/* vf24 0084ab30: the type entry (CRITICAL_SECTION + a 500-bucket handle table) under the tag at +0x44, the AssetIO at +4 */
void rx_database_register_io(rx_addr db, rx_addr io, uint32_t lo, uint32_t hi) {
    rx_addr e = rx_reflex_aligned_alloc(0x38);
    if (e) {
        g_memset(e, 0, 0x18);
        rx_htab_init_500(e + 0x18);
        critical_section_init(e);
    }
    rx_tagtab_insert(db + 0x44, lo, hi, e);
    rx_tagtab_insert(db + 4, lo, hi, io);
}
RX_FUNCTION(0x0084ab30, rx_database_register_io);

void rx_database_register_all(rx_addr db) {
    for (int i = 0; i < NTYPES; i++) {
        io_type* t = &g_types[i];
        rx_addr io = t->io;
        if (!io) {   /* FUN_00793a20: XenonMaterialAssetIO, a 4-byte heap singleton */
            if (!(U32(0, t->guard) & 1)) {
                SU32(0, t->guard, U32(0, t->guard) | 1);
                rx_addr p = rx_reflex_aligned_alloc(4);
                if (p) SU32(p, 0, t->io_vt);
                SU32(0, 0x00da67fc, p);
            }
            io = U32(0, 0x00da67fc);
        } else {     /* a static AssetIO (TextureAtlas / IconGeometry: their UI members are not built) */
            if (t->guard && !(U32(0, t->guard) & 1)) SU32(0, t->guard, U32(0, t->guard) | 1);
            SU32(io, 0, t->io_vt);
        }
        uint32_t lo, hi; make_tag(t->tag, &lo, &hi);
        rx_database_register_io(db, io, lo, hi);
    }
}

void rx_database_request(rx_addr db) {
    /* a queued loader job (Database vf14) is still pending: the original's loader thread serves the ring in order, so a request the
     * product serves at once must not advance the loader's cursor past it (the record it would have posted is not written). */
    if (U32(db, 0xec) != U32(db, 0xf4)) return;
    SU32(db, 0xf4, (U32(db, 0xf4) + 1) % 1000);
    SU32(db, 0xf0, U32(db, 0xf4));
    SU32(db, 0xec, (U32(db, 0xec) + 1) % 1000);
}

/* vf34 0084cf10: the background cache on */
void rx_database_cache_enable(rx_addr db) { SU32(db, 0x82b88, 1); }
RX_FUNCTION(0x0084cf10, rx_database_cache_enable);

/* vf36 0084cf20 (thiscall): ResetEvent(the database's event handle +0x83b48) */
uint32_t rx_database_reset_event(rx_addr db) { return RX_OS(0x00916090, U32(db, 0x83b48)); }
RX_FUNCTION(0x0084cf20, rx_database_reset_event);

/* vf22 0084ce00 (thiscall db, handle, frame; RET 8): the handle queued for its removal: pushed (00849660 on the argument's own slot) into the
 * pending list of the frame's parity (+0x64 / +0x7c, two vectors of 0x18 bytes) under the CRITICAL_SECTION at +0x62b3c. The original's
 * Engine::Frame 0084f260 drains the list of the rendered frame's parity after the render (Database vf33 0084cc80: vf31 (handle, -1), the
 * asset's reference, then vf32 (handle) for each, the list cleared). The vehicle damage parts' model assets are queued here when their
 * entry is released (00479dd0). Frame: push ebx / esi / edi. */
void rx_database_queue_remove(rx_addr db, rx_addr handle, uint32_t frame) {
    const rx_addr sp0 = rx_sp();
    SU32(sp0, 4, handle); SU32(sp0, 8, frame);                             /* the caller's pushed arguments */
    rx_set_sp(sp0 - 12);
    (void)RX_OS(0x0091600c, db + 0x62b3c);                                 /* EnterCriticalSection */
    {
        const rx_addr sp = rx_sp();
        SU32(sp, -4, sp0 + 4); SU32(sp, -8, 0x0084ce2a);
        rx_set_sp(sp - 8);
        rx_vec_push_back_ptr(db + 0x64 + 0x18u * (frame & 1u), sp0 + 4);
        rx_set_sp(sp);
    }
    (void)RX_OS(0x009160f8, db + 0x62b3c);                                 /* LeaveCriticalSection */
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0084ce00, rx_database_queue_remove);

/* vf38 0084cf50 (under the CRITICAL_SECTION at +0x83b20): the first free of 5 slots of 0x31c bytes gets (directory, database
 * pattern) -- the high-resolution databases the cache thread streams ("HR_*"); only while the cache is on */
void rx_database_cache_add(rx_addr db, const char* dir, const char* pattern) {
    if (!U32(db, 0x82b88)) return;
    uint32_t i = 0;
    while (i < 5 && U32(db, 0x82ea4 + 0x31c * i) != 0) i++;
    rx_addr slot = db + 0x31c * i;
    g_strncpy_host(slot + 0x82c90, pattern, 0x103); SU8(slot, 0x82d93, 0);
    SU32(slot, 0x82ea4, 1);
    g_strncpy_host(slot + 0x82b8c, dir, 0x103); SU8(slot, 0x82c8f, 0);
    if (U32(db, 0x83b1c) == 0) SU32(db, 0x83b1c, 1);
}

rx_addr rx_database_find_db(rx_addr db, const char* name) {
    uint32_t rec = 0;
    return strtab_find_host(db + 0x24, name, &rec) ? rec : 0;
}

/* vf8 0084a890 */
rx_addr rx_database_find_package(rx_addr db, const char* dbname, const char* pkg) {
    rx_addr rec = rx_database_find_db(db, dbname);
    if (!rec) return 0;
    for (uint32_t i = 0; i < U32(rec, 0x14c); i++) {
        rx_addr p = U32(rec, 0x148) + 0xa0 * i;
        if (g_icmp_host(p, pkg) == 0) return p;
    }
    return 0;
}

static rx_addr type_entry(rx_addr db, uint32_t lo, uint32_t hi) {
    uint32_t e = 0;
    if (!rx_tagtab_find(db + 0x44, lo, hi, &e)) { fprintf(stderr, "rxcore: asset type %.8s is not registered\n", (const char*)&lo); abort(); }
    return e;
}
static rx_addr find_io(rx_addr db, uint32_t lo, uint32_t hi) { uint32_t io = 0; return rx_tagtab_find(db + 4, lo, hi, &io) ? io : 0; }

rx_addr rx_database_find_handle(rx_addr db, const char* type, const char* name) {
    uint32_t lo, hi, h = 0; make_tag(type, &lo, &hi);
    uint32_t e = 0;
    if (!rx_tagtab_find(db + 0x44, lo, hi, &e)) return 0;
    return strtab_find_host(e + 0x18, name, &h) ? h : 0;
}

/* the handle of (type, name), created by the type's AssetIO::vf1 when the type's table does not have it (the name truncated to 63) */
static rx_addr handle_get(rx_addr db, uint32_t lo, uint32_t hi, const char* name, int create) {
    char nm[0x40]; strncpy(nm, name, 0x3f); nm[0x3f] = 0;
    rx_addr e = type_entry(db, lo, hi);
    uint32_t h = 0;
    if (!strtab_find_host(e + 0x18, nm, &h) && create) {
        rx_addr io = find_io(db, lo, hi);
        h = rx_asset_io_new_handle(io, lo, hi);
        g_strncpy_host(h + 4, nm, 0x3f); SU8(h, 0x43, 0);   /* AssetHandle::vf6 00768400 */
        rx_strtab_insert(e + 0x18, h + 4, h);              /* key: AssetHandle_Name (vf2) = +4 */
    }
    return h;
}

/* ---- asset loads ---- */

static void trace_asset(rx_addr db, rx_addr h, int built) {
    if (!g_trace) return;
    char line[256], nm[0x41], tg[9];
    for (int k = 0; k < 0x40; k++) nm[k] = (char)U8(h, 4 + k); nm[0x40] = 0;
    for (int k = 0; k < 8; k++) tg[k] = (char)U8(db, 0xd4 + k); tg[8] = 0;
    snprintf(line, sizeof line, "ASSET %s %s serial=%u built=%d", nm, tg, U32(db, 0xe8) - 1, built);
    g_trace(line);
}

#ifndef RX_ORACLE
/* XenonGrassAsset_Parse 0066cd30 (GrassAssetIO::vf3 00668aa0; the level loader thread), the part the product needs (w-rng): the grass
 * assets are the renderer's (Unity reads them), but the parse steps the game's LCG (g_GameRandSeed 0x9cb860 / g_GameRandCount 0x9cb864)
 * once for every blade while it builds the blades' vertices (0066d9f3 / 0066dd0x: the new seed is not even read: the leftover of a
 * rand() whose value went unused), in groups of 100 and the rest: the blade count (the header's +0xc, the asset's +0x64) draws. NAT_Renne /
 * NAT_RenneB (the only grass levels): 79 draws in the oracle's boot (REFLEX_WATCH on 0x9cb864, writes from 0066cd30). */
static void grass_parse_draws(const rx_stream* s) {
    if (s->pos + 0x100 > s->size) return;
    uint32_t blades;
    memcpy(&blades, s->data + s->pos + 0xc, 4);
    for (uint32_t i = 0; i < blades; i++) {
        SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
        SU32(0, 0x009cb860, U32(0, 0x009cb860) * 0x41c64e6du + 0x3039u);
    }
}
#endif

/* vf30 0084ad10 */
void rx_database_load_asset(rx_addr db, rx_addr h, rx_stream* s, rx_addr pkg) {
    g_strncpy(db + 0x94, h + 4, 0x3f);
    uint32_t lo, hi; handle_tag(h, &lo, &hi);
    SU32(db, 0xd4, lo); SU32(db, 0xd8, hi); SU8(db, 0xdc, 0);   /* strncpy(+0xd4, &tag, 8) */
    rx_addr asset = U32(h, 0x44);
    SU32(h, 0x4c, 1);
    if (pkg == 0xffffffffu && asset) {   /* reloading from a stream: the old asset is destroyed first (AssetIO::vf4) */
        fprintf(stderr, "rxcore: Database::vf30 reload of %08x: AssetIO::vf4 not ported\n", h); abort();
    }
    int built = 0;
    if (U32(h, 0x44)) SU32(asset, 8, U32(asset, 8) + 1);
    else {
        rx_addr io = find_io(db, lo, hi);
        if (!io) return;                    /* an unregistered type: nothing loaded, no serial */
        io_type* t = type_by_io_vt(U32(io, 0));
        SU8(0, 0x00a956fb, 1);              /* the "loading an asset" flag */
        /* Host-only actual serialized CPU shader binding. No material/GPU object
         * is synthesized; original handle+44 remains governed by real loader. */
#ifndef RX_ORACLE
        if (lo == 0x6574616du && hi == 0x6c616972u && s->pos <= s->size)
            rx_fx_material_binding_capture(h, s->data + s->pos, s->size - s->pos);
#endif
#ifndef RX_ORACLE
        if (t && !t->loader && !strcmp(t->tag, "grass")) grass_parse_draws(s);   /* w-rng: XenonGrassAsset_Parse's draws */
#endif
        asset = t && t->loader ? t->loader(s, h + 4, lo, hi) : 0;
        if (asset) SU32(asset, 8, U32(asset, 8) + 1);
        SU8(0, 0x00a956fb, 0);
        if (asset) SU32(asset, 0xc, pkg);
        built = asset != 0;
    }
    SU32(h, 0x4c, 0);
    uint32_t serial = U32(db, 0xe8);
    SU32(db, 0xe8, serial + 1);
    if (asset) SU32(asset, 4, serial);
    SU32(h, 0x44, asset);                   /* AssetHandle::vf5 004dce40 */
    trace_asset(db, h, built || (asset != 0));
}
RX_FUNCTION(0x0084ad10, rx_database_load_asset);

/* vf23 0084aa10 */
rx_addr rx_database_add_asset(rx_addr db, const char* name, rx_addr asset, uint32_t lo, uint32_t hi) {
    rx_addr h = handle_get(db, lo, hi, name, 1);
    SU32(h, 0x48, U32(h, 0x48) + 1);        /* AssetHandle::vf7 */
    if (asset) { SU32(asset, 8, U32(asset, 8) + 1); SU32(asset, 0xc, 0xffffffffu); }
    uint32_t serial = U32(db, 0xe8);
    SU32(db, 0xe8, serial + 1);
    if (asset) SU32(asset, 4, serial);
    SU32(h, 0x44, asset);
    if (g_trace) { char line[160]; snprintf(line, sizeof line, "NEWASSET %s serial=%u built=%d", name, serial, asset != 0); g_trace(line); }
    return h;
}

/* AssetIO::vf4 007d3ee0 (shared by the AssetIO classes; thiscall io, stack: the asset; RET 4): a non-null asset is deleted through its virtual
 * destructor (vtable slot 0, flag 1) */
void rx_asset_io_destroy(rx_addr io, rx_addr asset) {
    (void)io;
    if (!asset) return;
    uint32_t dtor = U32(U32(asset, 0), 0);
    uint32_t (*f)(rx_addr, uint32_t) = RX_FN(dtor, uint32_t (*)(rx_addr, uint32_t));
    if (!f) { fprintf(stderr, "rxcore: asset %08x: destructor %08x (vtable %08x slot 0) is not ported\n", asset, dtor, U32(asset, 0)); abort(); }
    f(asset, 1);
}
RX_FUNCTION(0x007d3ee0, rx_asset_io_destroy)

/* vf31 0084af30 (thiscall db, handle, package; RET 8): the handle's asset loses a reference; at 0 its AssetIO (asset vtable slot 2) deletes it
 * (AssetIO vtable slot 4 = rx_asset_io_destroy) and the handle forgets it (AssetHandle::vf5 004dce40); otherwise an asset of that package is
 * marked as no longer owned by it */
void rx_database_release_asset(rx_addr db, rx_addr h, rx_addr pkg) {
    (void)db;
    rx_addr a = U32(h, 0x44);                              /* AssetHandle::vf4 */
    if (!a) return;
    SU32(a, 8, U32(a, 8) - 1);
    if (U32(a, 8) == 0) {
        uint32_t get_io = U32(U32(a, 0), 8);
        rx_addr (*gio)(rx_addr) = RX_FN(get_io, rx_addr (*)(rx_addr));
        if (!gio) { fprintf(stderr, "rxcore: Database::vf31: asset %08x: vf2 %08x (vtable %08x) is not ported\n", a, get_io, U32(a, 0)); abort(); }
        rx_addr io = gio(a);
        if (io) {
            uint32_t vf4 = U32(U32(io, 0), 4 * 4);
            void (*d)(rx_addr, rx_addr) = RX_FN(vf4, void (*)(rx_addr, rx_addr));
            if (!d) { fprintf(stderr, "rxcore: Database::vf31: AssetIO::vf4 %08x (vtable %08x) is not ported\n", vf4, U32(io, 0)); abort(); }
            d(io, a);
        }
        SU32(h, 0x44, 0);                                  /* AssetHandle::vf5(0) */
    } else if (U32(a, 0xc) == pkg) SU32(a, 0xc, 0xffffffffu);
}
/* registered (w-frametail): Database vf33 0084cc80 reaches it through the vtable (slot 31) */
RX_FUNCTION(0x0084af30, rx_database_release_asset);

void rx_database_unload_package(rx_addr db, const char* dbname, const char* pkg) {
    rx_addr p = rx_database_find_package(db, dbname, pkg);
    if (!p) return;
    for (uint32_t k = 0; k < U32(p, 0x84); k++) {
        rx_addr h = U32(U32(p, 0x80) + 0x1c * k, 0);
        if (h) rx_database_release_asset(db, h, p);
    }
    SU32(p, 0x94, 0);
}

#ifndef RX_ORACLE

/* a numeric attribute as the loader reads it: type 4 (uint) directly, a string through FUN_008295e0 (a comma list's first int);
 * anything else leaves the field (0 here: the core's blocks are zero-filled) */
static uint32_t attr_u32(const fmt_bxml* x, int node, const char* name) {
    int a = node >= 0 ? fmt_bxml_find_attr(x, node, name) : -1;
    if (a < 0) return 0;
    fmt_bxml_value v;
    if (fmt_bxml_attr_value(x, a, &v) != 0) return 0;
    if (v.kind == FMT_BX_UINT && v.count) return fmt_bx_u32(&v, 0);
    if (v.kind == FMT_BX_STRING && v.str) return (uint32_t)strtol(v.str, NULL, 10);
    return 0;
}
static const char* attr_str(const fmt_bxml* x, int node, const char* name) {
    const char* s = fmt_bxml_attr_str(x, node, name);
    return s ? s : "";
}
static int child_count(const fmt_bxml* x, int node) { return node >= 0 ? fmt_bxml_child_count(x, node) : 0; }

/* Fn_0084b0a0 */
rx_addr rx_database_load(rx_addr db, struct fmt_install* in, const char* name, const char* dir, uint32_t p5, uint32_t p6) {
    rx_addr have = rx_database_find_db(db, name);
    if (have) return have;
    /* the file: <dir><name>.dx9.database (the vfs is case and separator insensitive; a leading ".\" is the current directory) */
    char path[0x200];
    const char* d = dir;
    if (d[0] == '.' && (d[1] == '\\' || d[1] == '/')) d += 2;
    snprintf(path, sizeof path, "%s%s.dx9.database", d, name);
    fmt_buf b = {0};
    if (fmt_vfs_read_file(fmt_install_vfs(in), path, &b) != 0) return 0;
    fmt_bxml* x = fmt_bxml_parse(b.data, b.size);
    fmt_buf_free(&b);
    if (!x) return 0;
    int root = 0;
    int handles = fmt_bxml_find_child(x, root, "Handles"), packages = fmt_bxml_find_child(x, root, "Packages");
    uint32_t nh = (uint32_t)child_count(x, handles), np = (uint32_t)child_count(x, packages), na = 0;
    for (int k = 0; packages >= 0 && fmt_bxml_find_child_n(x, packages, "Package", k) >= 0; k++)
        na += (uint32_t)child_count(x, fmt_bxml_find_child_n(x, packages, "Package", k));
    rx_addr rec = rx_reflex_aligned_alloc((nh + np * 0x28 + na * 7) * 4 + 0x160);
    rx_addr arr = rec + 0x160;
    g_strncpy_host(rec, name, 0x3f); SU8(rec, 0x3f, 0);
    g_strncpy_host(rec + 0x40, dir, 0x103); SU8(rec, 0x143, 0);
    SU32(rec, 0x154, nh); SU32(rec, 0x14c, np); SU32(rec, 0x158, 0); SU32(rec, 0x15c, p5); SU32(rec, 0x144, p6);
    SU32(rec, 0x150, arr);
    SU32(rec, 0x148, arr + 4 * U32(rec, 0x154));
    rx_addr assets = arr + 4 * U32(rec, 0x154) + 0xa0 * U32(rec, 0x14c);
    /* the handles: each one found or created, counted */
    for (int k = 0, slot = 0; handles >= 0; k++) {
        int a = fmt_bxml_find_child_n(x, handles, "Asset", k);
        if (a < 0) break;
        uint32_t lo, hi; make_tag(attr_str(x, a, "type"), &lo, &hi);
        rx_addr h = handle_get(db, lo, hi, attr_str(x, a, "name"), 1);
        SU32(h, 0x48, U32(h, 0x48) + 1);   /* AssetHandle::vf7 */
        SU32(U32(rec, 0x150), 4 * (uint32_t)slot, h);
        slot++;
    }
    /* the packages and their asset records (each asset's handle looked up, not counted) */
    for (int k = 0; packages >= 0; k++) {
        int pn = fmt_bxml_find_child_n(x, packages, "Package", k);
        if (pn < 0) break;
        rx_addr p = U32(rec, 0x148) + 0xa0 * (uint32_t)k;
        g_strncpy_host(p, attr_str(x, pn, "name"), 0x3f); SU8(p, 0x3f, 0);
        g_strncpy_host(p + 0x40, attr_str(x, pn, "file"), 0x3f); SU8(p, 0x7f, 0);
        g_strncat_host(p + 0x40, ".dx9.package", 0x40);
        SU32(p, 0x88, attr_u32(x, pn, "blockOffset"));
        SU32(p, 0x8c, attr_u32(x, pn, "heapOffset"));
        SU32(p, 0x90, attr_u32(x, pn, "offset"));
        SU32(p, 0x94, 0);
        SU32(p, 0x80, assets);
        SU32(p, 0x9c, rec);
        uint32_t n = (uint32_t)child_count(x, pn);
        assets += 0x1c * n;
        SU32(p, 0x84, n);
        for (int j = 0;; j++) {
            int an = fmt_bxml_find_child_n(x, pn, "Asset", j);
            if (an < 0) break;
            rx_addr r = U32(p, 0x80) + 0x1c * (uint32_t)j;
            uint32_t lo, hi; make_tag(attr_str(x, an, "type"), &lo, &hi);
            SU32(r, 0, 0);
            SU32(r, 0, handle_get(db, lo, hi, attr_str(x, an, "name"), 0));
            int blk = fmt_bxml_find_child(x, an, "Block"), heap = fmt_bxml_find_child(x, an, "Heap");
            SU32(r, 8, attr_u32(x, blk, "offset"));
            SU32(r, 4, attr_u32(x, blk, "size"));
            SU32(r, 0x10, attr_u32(x, heap, "offset"));
            SU32(r, 0xc, attr_u32(x, heap, "size"));
            SU32(r, 0x14, 0); SU32(r, 0x18, 0);
            int c = fmt_bxml_find_child(x, an, "Compress");
            if (c >= 0) {
                int ea = fmt_bxml_find_attr(x, c, "enabled");
                fmt_bxml_value v;
                if (ea >= 0 && fmt_bxml_attr_value(x, ea, &v) == 0) {
                    if (v.kind == FMT_BX_BOOL && v.count) SU32(r, 0x14, fmt_bx_u32(&v, 0));
                    else if (v.kind == FMT_BX_STRING && v.str) SU32(r, 0x14, (!strcmp(v.str, "1") || !strcmp(v.str, "true")) ? 1 : 0);
                }
                const char* codec = fmt_bxml_attr_str(x, c, "codec");
                if (codec) {
                    if (!fmt_stricmp(codec, "Zlib")) SU32(r, 0x18, 0);
                    else if (!fmt_stricmp(codec, "LZX")) SU32(r, 0x18, 1);
                    else if (!fmt_stricmp(codec, "SPUZlib")) SU32(r, 0x18, 2);
                }
            }
        }
    }
    fmt_bxml_free(x);
    rx_strtab_insert(db + 0x24, rec, rec);   /* key: the record's name at +0 */
    return rec;
}

/* Database_LoaderThreadProc case 1 (0084c7xx) with FUN_0084c6b0 and Fn_0084bc70 */
int rx_database_load_package(rx_addr db, struct fmt_install* in, const char* dbname, const char* pkg, rx_addr progress, uint32_t p98) {
    rx_addr p = rx_database_find_package(db, dbname, pkg);
    if (!p) return -1;
    if (U32(p, 0x94)) return 0;
    /* FUN_0084c6b0: the texture reduction flag from the database name (the front end's databases are never reduced) */
    static const char* const keep[9] = {"mxui", "mxui_dlc001", "mxui_dlc003", "mxui_dlc004", "mxui_dlc005", "mxui_streaming", "mxuipatch",
                                         "enginedependencies", "uiscene"};
    char low[0x200]; snprintf(low, sizeof low, "%s", dbname);
    for (char* c = low; *c; c++) *c = (char)fold((uint8_t)*c);
    g_strncpy_host(0x00da6e18, low, (uint32_t)strlen(low) + 1);   /* strcpy_s + _strlwr_s into the global buffer */
    SU8(0, 0x00a956fa, 1);
    for (int i = 0; i < 9; i++) if (strstr(low, keep[i])) { SU8(0, 0x00a956fa, 0); break; }
    /* Fn_0084bc70: the package's assets in record order; the bytes of asset k are the fmt reader's asset k of that package */
    const fmt_database* fd = fmt_install_db(in, dbname);
    const fmt_package* fp = fd ? fmt_db_package(fd, pkg) : NULL;
    const int ms_was = rx_ms_level_begin(dbname);   /* MAP SCALE (port): the level database's assets load scaled (map_scale.h) */
    if (!fp || (uint32_t)fp->nassets != U32(p, 0x84)) { fprintf(stderr, "rxcore: package %s/%s: the fmt reader does not match\n", dbname, pkg); abort(); }
    if (progress) SU32(progress, 0, 0);
    int n = 0;
    for (uint32_t k = 0; k < U32(p, 0x84); k++) {
        if (g_between) g_between(db);
        rx_addr r = U32(p, 0x80) + 0x1c * k;
        rx_addr h = U32(r, 0);
        const fmt_asset* fa = &fp->assets[k];
        fmt_buf b = {0};
        if (fmt_install_read(in, fa, &b) != 0) {
            fprintf(stderr, "rxcore: read %s/%s %s: %s\n", dbname, pkg, fa->name, fmt_error());
            /* (w-fb-customtrack) the session's error names the file (rx_session.c describe_fault): a custom track's bad resource says what it is */
            snprintf(rx_stop_note, sizeof rx_stop_note, "rxcore: read %s.%s (%s): %s", fa->name, fa->type, dbname, fmt_error());
            abort();
        }
        rx_stream s; rx_stream_init(&s, b.data, b.size);
        if (h) { rx_database_load_asset(db, h, &s, p); n++; }
        fmt_buf_free(&b);
        if (progress) SF32(progress, 0, (float)k / (float)U32(p, 0x84));   /* both unsigned ints converted to float */
    }
    rx_ms_level_end(ms_was);
    if (progress) SU32(progress, 0, 0x3f800000u);
    SU32(p, 0x94, 1);
    SU8(0, 0x00a956fa, 0);
    SU32(p, 0x98, p98);
    return n;
}

#else  /* the oracle build (inside the emulator): the file-reading entry points are not used there */

rx_addr rx_database_load(rx_addr db, struct fmt_install* in, const char* name, const char* dir, uint32_t p5, uint32_t p6) {
    (void)db; (void)in; (void)name; (void)dir; (void)p5; (void)p6; fprintf(stderr, "rxcore: rx_database_load in the oracle build\n"); abort();
}
int rx_database_load_package(rx_addr db, struct fmt_install* in, const char* dbname, const char* pkg, rx_addr progress, uint32_t p98) {
    (void)db; (void)in; (void)dbname; (void)pkg; (void)progress; (void)p98; fprintf(stderr, "rxcore: rx_database_load_package in the oracle build\n"); abort();
}

#endif

/* ---- Database virtuals the requesting code calls (construction unit 7, riders; construction.md section 8.8) ----
 * Registered so vtable calls of the core's requesters land here. vf8 / vf26 / vf27 are transliterations (the type entry's
 * CRITICAL_SECTION is not taken: the core builds on one thread). vf4 / vf15 post a loader request (0084c190) and Sleep(1) until the
 * loader thread has served it (vf7 / vf10); the product serves it at once (rx_database_load / rx_database_load_package with the
 * install of rx_database_set_install, then the request ring advanced as the loader does: rx_database_request). The oracle build runs
 * the original (the loader thread is the emulator's). */
static struct fmt_install* g_req_install;
void rx_database_set_install(struct fmt_install* in) { g_req_install = in; }
static void game_str(rx_addr s, char* out, size_t n) { size_t k = 0; for (; k + 1 < n && U8(s, k); k++) out[k] = (char)U8(s, k); out[k] = 0; }

/* vf8 0084a890 (thiscall db, database name, package name; RET 8): the package record (0xa0) or 0 (_stricmp) */
rx_addr rx_database_vf_find_package(rx_addr db, rx_addr dbname, rx_addr pkg) {
    uint32_t rec = 0;
    if (!rx_strtab_find(db + 0x24, dbname, &rec) || !rec) return 0;
    for (uint32_t i = 0; i < U32(rec, 0x14c); i++) {
        rx_addr p = U32(rec, 0x148) + 0xa0 * i;
        if (g_icmp(p, pkg) == 0) return p;
    }
    return 0;
}
RX_FUNCTION(0x0084a890, rx_database_vf_find_package);
/* vf26 0084ac20 (thiscall db, name, tag lo, tag hi; RET 0xc): 1 when the type's handle table has the name (0084dab0) */
uint32_t rx_database_vf_has_handle(rx_addr db, rx_addr name, uint32_t lo, uint32_t hi) {
    uint32_t e = 0, h = 0;
    if (!rx_tagtab_find(db + 0x44, lo, hi, &e)) return 0;
    return (uint32_t)rx_strtab_find(e + 0x18, name, &h);
}
RX_FUNCTION(0x0084ac20, rx_database_vf_has_handle);
/* vf27 0084ac70 (thiscall db, name, tag lo, tag hi, flag (unused); RET 0x10): the handle of the name, 0 when none (the not-found path
 * copies the tag to a local for nothing) */
rx_addr rx_database_vf_handle(rx_addr db, rx_addr name, uint32_t lo, uint32_t hi, uint32_t flag) {
    (void)flag;
    uint32_t e = 0, h = 0;
    if (!rx_tagtab_find(db + 0x44, lo, hi, &e)) return 0;
    return rx_strtab_find(e + 0x18, name, &h) ? h : 0;
}
RX_FUNCTION(0x0084ac70, rx_database_vf_handle);

/* Database::vf1 0084a7a0 (thiscall db) -> 1 when the calling thread is the database's own (+0x100, GetCurrentThreadId IAT 0x91607c), else 0.
 * Read off the disassembly (w-vtunported: rx_fx_package_request's db_vcall slot 4). */
uint32_t rx_database_is_owner_thread(rx_addr db) {
    return rx_os_import(0x0091607c, 0, 0) == U32(db, 0x100) ? 1u : 0u;
}
RX_FUNCTION(0x0084a7a0, rx_database_is_owner_thread);
/* vf4 0084b020 (thiscall db, directory, database name, p3, p4; RET 0x10): load a database and wait (request 3: +0x10c directory,
 * +0x210 name, +0x250 *&p3, +0x254 *&p4) */
void rx_database_vf_load_db(rx_addr db, rx_addr dir, rx_addr name, uint32_t p3, uint32_t p4) {
#ifdef RX_ORACLE
    uint32_t a[4] = {dir, name, p3, p4}; rx_call_original(0x0084b020, RX_THISCALL, db, 4, a);
#else
    char d[0x40], n[0x40]; game_str(dir, d, sizeof d); game_str(name, n, sizeof n);
    if (!g_req_install) { fprintf(stderr, "rxcore: Database::vf4 %s without an install (rx_database_set_install)\n", n); abort(); }
    rx_database_load(db, g_req_install, n, d, p3, p4);
    rx_database_request(db);
#endif
}
RX_FUNCTION(0x0084b020, rx_database_vf_load_db);
/* vf15 0084a7f0 (thiscall db, database name, package name, p98; RET 0xc): load a package and wait (request 1: +0x210 database,
 * +0x250 package, +0x108 p98, no progress float) */
void rx_database_vf_load_package(rx_addr db, rx_addr dbname, rx_addr pkg, uint32_t p98) {
#ifdef RX_ORACLE
    uint32_t a[3] = {dbname, pkg, p98}; rx_call_original(0x0084a7f0, RX_THISCALL, db, 3, a);
#else
    char d[0x40], p[0x40]; game_str(dbname, d, sizeof d); game_str(pkg, p, sizeof p);
    if (!g_req_install) { fprintf(stderr, "rxcore: Database::vf15 %s/%s without an install (rx_database_set_install)\n", d, p); abort(); }
    rx_database_load_package(db, g_req_install, d, p, 0, p98);
    rx_database_request(db);
#endif
}
RX_FUNCTION(0x0084a7f0, rx_database_vf_load_package);

/* ---- the asynchronous requests and the request ring's queries (construction unit 6, vehicles; construction.md section 8.8) ----
 * The ring: 1000 records of 0x194 bytes at db + 0x104 + 0x194 * i (+0 the type: 1 load package, 2 unload package, 3 load database,
 * 4 / 5 / 6 others; +4 / +8 the parameters, +0x10c / +0x14c / +0x150 the names), +0xec the next record the loader thread serves,
 * +0xf4 the next free one, +0xf8 the loader thread's wake event. vf5 / vf16 / vf17 post a record (0084c190) and return; the product
 * serves them at once (as vf4 / vf15). vf7 / vf10 / vf12 / vf13 are transliterations (the CRITICAL_SECTION +0x62b24 / +0x62b54 is not
 * taken; vf12 / vf13 end with SetEvent of the wake event, loop_set_event). */
/* vf5 0084b070 (thiscall db, directory, database name, p3, p4; RET 0x10): load a database (request 3), no wait */
void rx_database_vf_load_db_async(rx_addr db, rx_addr dir, rx_addr name, uint32_t p3, uint32_t p4) {
#ifdef RX_ORACLE
    uint32_t a[4] = {dir, name, p3, p4}; rx_call_original(0x0084b070, RX_THISCALL, db, 4, a);
#else
    rx_database_vf_load_db(db, dir, name, p3, p4);
#endif
}
RX_FUNCTION(0x0084b070, rx_database_vf_load_db_async);
/* vf16 0084a7c0 (thiscall db, database name, package name, p2, requester; RET 0x10): load a package (request 1: +0x108 p2, +0x10c
 * the requester value), no wait */
void rx_database_vf_load_package_async(rx_addr db, rx_addr dbname, rx_addr pkg, uint32_t p2, uint32_t requester) {
#ifdef RX_ORACLE
    uint32_t a[4] = {dbname, pkg, p2, requester}; rx_call_original(0x0084a7c0, RX_THISCALL, db, 4, a);
#else
    (void)requester;
    rx_database_vf_load_package(db, dbname, pkg, p2);
#endif
}
RX_FUNCTION(0x0084a7c0, rx_database_vf_load_package_async);
/* vf17 0084cde0 (thiscall db, database name, package name, frame; RET 0xc): unload a package (request 2: +0x10c the frame) */
void rx_database_vf_unload_package(rx_addr db, rx_addr dbname, rx_addr pkg, uint32_t frame) {
#ifdef RX_ORACLE
    uint32_t a[3] = {dbname, pkg, frame}; rx_call_original(0x0084cde0, RX_THISCALL, db, 3, a);
#else
    (void)frame;
    char d[0x40], p[0x40]; game_str(dbname, d, sizeof d); game_str(pkg, p, sizeof p);
    rx_database_unload_package(db, d, p);
    rx_database_request(db);
#endif
}
RX_FUNCTION(0x0084cde0, rx_database_vf_unload_package);
/* vf7 0084bc20 (thiscall db, database name; RET 4): 1 when the database is loaded (its record in the name table +0x24, 007fa250) */
uint32_t rx_database_vf_has_db(rx_addr db, rx_addr name) {
    uint32_t rec = 0;
    rx_strtab_find(db + 0x24, name, &rec);
    return rec != 0;
}
RX_FUNCTION(0x0084bc20, rx_database_vf_has_db);
/* vf10 0084a860 (thiscall db, database name, package name; RET 8): the package's loaded flag (+0x94; vf8 through the vtable), 0 when
 * there is no such package */
uint32_t rx_database_vf_package_loaded(rx_addr db, rx_addr dbname, rx_addr pkg) {
    rx_addr t = U32(U32(db, 0), 0x20);
    rx_addr (*vf8)(rx_addr, rx_addr, rx_addr) = RX_FN(t, rx_addr (*)(rx_addr, rx_addr, rx_addr));
    uint32_t a[2] = {dbname, pkg};
    rx_addr p = vf8 ? vf8(db, dbname, pkg) : rx_call_original(t, RX_THISCALL, db, 2, a);
    return p ? U32(p, 0x94) : 0;
}
RX_FUNCTION(0x0084a860, rx_database_vf_package_loaded);
/* the inline strcmp of the ring scans: 0 when equal */
static int ring_strcmp(rx_addr a, rx_addr b) {
    for (uint32_t k = 0;; k++) {
        uint8_t x = U8(a, k), y = U8(b, k);
        if (x != y) return x < y ? -1 : 1;
        if (!x) return 0;
    }
}
/* vf12 0084c3a0 (thiscall db, database name, package name; RET 8): the type of the last pending request of that package (records
 * +0xec .. +0xf4: +0x14c package, +0x10c database), 0 when none */
uint32_t rx_database_vf_package_pending(rx_addr db, rx_addr dbname, rx_addr pkg) {
    uint32_t r = 0;
    for (uint32_t i = U32(db, 0xec), end = U32(db, 0xf4); i != end; i = (uint32_t)((int32_t)(i + 1) % 1000)) {
        rx_addr rec = db + 0x194 * i;
        if (ring_strcmp(rec + 0x250, pkg) == 0 && ring_strcmp(rec + 0x210, dbname) == 0) r = U32(rec, 0x104);
    }
    loop_set_event(U32(db, 0xf8));
    return r;
}
RX_FUNCTION(0x0084c3a0, rx_database_vf_package_pending);
/* vf13 0084c490 (thiscall db, database name; RET 4): the type of the last pending request naming that database (+0x10c of the
 * record: the name of a load-database request, the database of a package request), 0 when none */
uint32_t rx_database_vf_db_pending(rx_addr db, rx_addr name) {
    uint32_t r = 0;
    for (uint32_t i = U32(db, 0xec), end = U32(db, 0xf4); i != end; i = (uint32_t)((int32_t)(i + 1) % 1000)) {
        rx_addr rec = db + 0x194 * i;
        if (ring_strcmp(rec + 0x210, name) == 0) r = U32(rec, 0x104);
    }
    loop_set_event(U32(db, 0xf8));
    return r;
}
RX_FUNCTION(0x0084c490, rx_database_vf_db_pending);

/* FUN_0084c5b0 (EDX = db): the first ring slot from +0xf4 whose kind is 0, or 0xffffffff when all 1000 are busy */
uint32_t rx_database_ring_free(rx_addr db) {
    uint32_t i = U32(db, 0xf4);
    for (uint32_t n = 0; n < 1000; n++) {
        if (U32(db, 0x104 + 0x194u * i) == 0) return i;
        i = (i + 1) % 1000;
    }
    return 0xffffffffu;
}

/* ---- Database::vf14 0084c0f0 (the loader's job request) and the loader thread's step -------------------------------
 * vf14 (thiscall db, name, fn, arg, progress; RET 0x10) is how MXGame::Update's state 4 starts FUN_00759f00
 * ("Subscene Creation"): it takes the first free ring slot from +0xf4, zeroes it, writes kind 5, the progress pointer at
 * +4, the function at +0x18c and its argument at +0x190, advances +0xf4 and signals the loader thread (+0xf8). The name
 * argument is not read by the original (checked in the disassembly). The loader thread (Database_LoaderThreadProc
 * 0084c760) snapshots +0xf4 into +0xf0, serves the records from +0xec, clears each one and advances +0xec. Case 5 calls
 * the record's function with its argument and the progress pointer (a scratch slot when that is 0).
 * The product has no second thread: each request the product can serve at once (packages / databases) is served in the
 * calling function and its ring record is advanced past (rx_database_request); a state-4 job is queued here and served by
 * rx_database_loader_step at the end of the frame, the point between frames where the original's loader thread runs. */
uint32_t rx_database_vf_submit_job(rx_addr db, rx_addr name, rx_addr fn, uint32_t arg, rx_addr progress) {
#ifdef RX_ORACLE
    /* the oracle build runs the original vf14 (the emulated loader thread serves the record; as vf4 / vf15 do) */
    const uint32_t a[4] = {name, fn, arg, progress};
    return rx_call_original(0x0084c0f0, RX_THISCALL, db, 4, a);
#else
    (void)name;
    const uint32_t i = rx_database_ring_free(db);          /* FUN_0084c5b0: from +0xf4 to the first slot whose kind is 0 */
    if (i == 0xffffffffu) return 0;                        /* the original leaves the record unwritten (returns 0) */
    const rx_addr rec = db + 0x104 + 0x194u * i;
    for (uint32_t k = 0; k < 0x194; k += 4) SU32(rec, k, 0);
    SU32(rec, 0, 5);
    SU32(rec, 4, progress);
    SU32(rec, 0x18c, fn);
    SU32(rec, 0x190, arg);
    SU32(db, 0xf4, (i + 1) % 1000);
    (void)loop_set_event(U32(db, 0xf8));                   /* SetEvent (the loader thread's wake event) */
    return 0;
#endif
}
RX_FUNCTION(0x0084c0f0, rx_database_vf_submit_job);

/* one turn of Database_LoaderThreadProc's queue (product): +0xf0 = +0xf4, then every record from +0xec to +0xf0 served and
 * cleared (the original's case 5: fn(arg, progress ? progress : a scratch stack slot)); its own case 1..4 records cannot be
 * pending in the product (they are served at once and advanced past). */
void rx_database_loader_step(rx_addr db) {
    if (!db) return;
    SU32(db, 0xf0, U32(db, 0xf4));
    while (U32(db, 0xec) != U32(db, 0xf0)) {
        const uint32_t i = U32(db, 0xec);
        const rx_addr rec = db + 0x104 + 0x194u * i;
        const uint32_t kind = U32(rec, 0);
        if (kind == 5) {
            const rx_addr fn = U32(rec, 0x18c), arg = U32(rec, 0x190), prog = U32(rec, 4) ? U32(rec, 4) : rx_sp() - 4;
            uint32_t (*f)(uint32_t, rx_addr) = RX_FN(fn, uint32_t (*)(uint32_t, rx_addr));
            if (!f) { fprintf(stderr, "rxcore: the Database loader job %08x is not registered\n", fn); abort(); }
            const rx_addr s = rx_sp();
            rx_set_sp(s - 12);                                 /* the original's two pushed arguments + return address */
            f(arg, prog);
            rx_set_sp(s);
        } else if (kind != 0) {
            fprintf(stderr, "rxcore: the Database loader record %u has kind %u (a request the product serves at once)\n", i, kind);
            abort();
        }
        for (uint32_t k = 0; k < 0x194; k += 4) SU32(rec, k, 0);
        SU32(db, 0xec, (i + 1) % 1000);
    }
}

