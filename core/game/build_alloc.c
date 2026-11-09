/* build_alloc.c -- see build_alloc.h. */
#include "build_alloc.h"
#include "crt.h"
#include "loop_platform.h"
#include <stdlib.h>
#include <stdio.h>

#ifdef RX_ORACLE
/* Oracle build (inside the emulator): allocations go to the game's own heap through msvcr90 (the exe's IAT slots), so a core loader
 * run in the oracle makes the original's blocks (malloc / free too: composite shapes, raw vector blocks). */
static uint32_t crt_call(rx_addr slot, int n, const uint32_t* args) { return rx_call_original(U32(0, slot), RX_CDECL, 0, n, args); }
rx_addr rx_crt_malloc(uint32_t n) { uint32_t a[1] = {n}; return crt_call(0x00916290, 1, a); }   /* MSVCR90 malloc (IAT) */
void rx_crt_free(rx_addr p) { uint32_t a[1] = {p}; crt_call(0x00916294, 1, a); }                   /* MSVCR90 free (IAT) */
rx_addr rx_crt_aligned_malloc(uint32_t size, uint32_t align) { uint32_t a[2] = {size, align}; return crt_call(0x00916358, 2, a); }
rx_addr rx_crt_aligned_realloc(rx_addr p, uint32_t size, uint32_t align) { uint32_t a[3] = {p, size, align}; return crt_call(0x0091641c, 3, a); }
void rx_crt_aligned_free(rx_addr p) { uint32_t a[1] = {p}; crt_call(0x00916350, 1, a); }
#else
#include "../src/rx_heap.h"

#define RX_HEAP_MAX 0x60000000u   /* the core's heap region (rx_heap.c): bigger requests fail like a failed HeapAlloc */

/* Verification aid: RX_BUILD_FILL=<byte> fills new blocks with that byte instead of zeros. The original's fresh blocks hold whatever
 * the heap had there; two core builds with different fills show which bytes the construction never writes (rx_canon_cmp --undef). */
static int g_fill = -2;
rx_addr rx_crt_malloc(uint32_t n) {
    if (n > RX_HEAP_MAX) return 0;
    if (g_fill == -2) { const char* f = getenv("RX_BUILD_FILL"); g_fill = f && *f ? (int)(strtoul(f, NULL, 0) & 0xff) : -1; }
    rx_addr p = rx_heap_alloc(n ? n : 1, 16);
    if (p && g_fill > 0) for (uint32_t k = 0; k < (n ? n : 1); k++) rx_wr8(p + k, (uint8_t)g_fill);
    return p;
}
void rx_crt_free(rx_addr p) { if (p) rx_heap_free(p); }

/* _aligned_offset_malloc_base(size, align, 0): align = max(align, 4) - 1; ptr = malloc(4 + align + size);
 * ret = (ptr + 4 + align) & ~align; [ret - 4] = ptr. (A non-power-of-two align is an error in the CRT: returns 0.) */
rx_addr rx_crt_aligned_malloc(uint32_t size, uint32_t align) {
    if (align & (align - 1)) return 0;
    uint32_t a = (align > 4 ? align : 4) - 1;
    uint64_t want = 4ull + a + size;
    if (want > 0xffffffffull) return 0;
    rx_addr ptr = rx_crt_malloc((uint32_t)want);
    if (!ptr) return 0;
    rx_addr ret = (ptr + 4 + a) & ~a;
    SU32(ret - 4, 0, ptr);
    return ret;
}

void rx_crt_aligned_free(rx_addr p) { if (p) rx_crt_free(U32(p - 4, 0)); }

/* _aligned_offset_realloc_base(p, size, align, 0). NULL -> _aligned_malloc; size 0 -> free, NULL. Otherwise the CRT tries _expand
 * (HeapReAlloc in place) of the old block to 4 + align + size bytes, else mallocs a new one, then moves min(old usable, size) bytes
 * to the aligned position and writes the header (its "same position" shortcut tests (p & ~align), practically never true). Either
 * way the result is one block of 4 + align + size bytes holding header and data; the core always takes a new block. */
rx_addr rx_crt_aligned_realloc(rx_addr p, uint32_t size, uint32_t align) {
    if (!p) return rx_crt_aligned_malloc(size, align);
    if (!size) { rx_crt_aligned_free(p); return 0; }
    rx_addr stptr = U32((p & ~3u) - 4, 0);
    uint32_t a = (align > 4 ? align : 4) - 1;
    uint32_t avail = rx_heap_size(stptr) - (p - stptr);
    uint32_t movesz = avail > size ? size : avail;
    uint64_t want = 4ull + a + size;
    if (want > 0xffffffffull) return 0;
    rx_addr ptr = rx_crt_malloc((uint32_t)want);
    if (!ptr) return 0;
    rx_addr ret = (ptr + 4 + a) & ~a;
    for (uint32_t k = 0; k < movesz; k++) rx_wr8(ret + k, rx_rd8(p + k));
    rx_crt_free(stptr);
    SU32(ret - 4, 0, ptr);
    return ret;
}

#endif

/* 0078fbe0 ReflexAlignedAlloc(size): _aligned_malloc(size, 16) (the allocator lock is not needed: one thread builds) */
rx_addr rx_reflex_aligned_alloc(uint32_t size) { return rx_crt_aligned_malloc(size, 16); }
RX_FUNCTION(0x0078fbe0, rx_reflex_aligned_alloc);

/* 0078fc50 ReflexAlignedFree(p): _aligned_free(p) (= StdMemoryAllocator::vf2 00775540) */
void rx_reflex_aligned_free(rx_addr p) { rx_crt_aligned_free(p); }
RX_FUNCTION(0x0078fc50, rx_reflex_aligned_free);

/* 0078fc50 with the original's frame, for callers that keep the original's stack (the package unload's asset destructors on the
 * database thread leave it as dead stack there): e = the call's return slot, r = EBX / EBP / ESI / EDI at the call. push esi; the
 * allocator lock ([0x00da67d8] set: EnterCriticalSection twice through ESI = [0x0091600c], returns 0078fc67 / 0078fc77); push p;
 * call [_aligned_free] (return 0078fc82) with _aligned_free's and free's frames (crt_frames.c); the lock left twice through ESI =
 * [0x009160f8] (returns 0078fc9b / 0078fcab). */
static void alloc_cs(rx_addr e, int enter, rx_addr ret) {   /* push 0xd61704; call: the argument at e + 4, the return address at e */
    const rx_addr sp = rx_sp();
    rx_set_sp(e + 8);
    if (enter) loop_enter_cs(0x00d61704); else loop_leave_cs(0x00d61704);
    SU32(e, 4, 0x00d61704); SU32(e, 0, ret);
    rx_set_sp(sp);
}
void rx_reflex_aligned_free_at(rx_addr e, crt_regs4 r, rx_addr p) {
    uint32_t esi = r.esi;
    SU32(e, -4, r.esi);
    if (U8(0, 0x00da67d8) != 0) {
        esi = U32(0, 0x0091600c);
        alloc_cs(e - 0xc, 1, 0x0078fc67);
        if (U8(0, 0x00da67d8) != 0) alloc_cs(e - 0xc, 1, 0x0078fc77);
    }
    SU32(e, -8, p); SU32(e, -0xc, 0x0078fc82);
    const crt_regs4 m = {r.ebx, r.ebp, esi, r.edi};
    crt_aligned_free_at(e - 0xc, m, p);
    if (U8(0, 0x00da67d8) != 0) {
        alloc_cs(e - 0xc, 0, 0x0078fc9b);
        if (U8(0, 0x00da67d8) != 0) alloc_cs(e - 0xc, 0, 0x0078fcab);
    }
}

/* 0078fbe0 with the original's frame (the counterpart of rx_reflex_aligned_free_at): push esi; the lock entered twice through
 * ESI = [0x0091600c] (returns 0078fbf7 / 0078fc07); push 0x10, size; call [_aligned_malloc] (return 0078fc14; oracle: the
 * original, with the original's registers, so its frames match; product: the core's heap); the lock left twice through EDI =
 * [0x009160f8] after push edi (returns 0078fc30 / 0078fc40). */
rx_addr rx_reflex_aligned_alloc_at(rx_addr e, crt_regs4 r, uint32_t size) {
    uint32_t esi = r.esi;
    SU32(e, -4, r.esi);
    if (U8(0, 0x00da67d8) != 0) {
        esi = U32(0, 0x0091600c);
        alloc_cs(e - 0xc, 1, 0x0078fbf7);
        if (U8(0, 0x00da67d8) != 0) alloc_cs(e - 0xc, 1, 0x0078fc07);
    }
    SU32(e, -8, 0x10); SU32(e, -0xc, size); SU32(e, -0x10, 0x0078fc14);
    const rx_addr sp = rx_sp();
    rx_set_sp(e - 4);
#ifdef RX_ORACLE
    const uint32_t a[2] = {size, 0x10};
    const rx_addr p = rx_call_original_regs(U32(0, 0x00916358), r.ebx, r.ebp, esi, r.edi, 2, a);
#else
    (void)esi;
    const rx_addr p = rx_crt_aligned_malloc(size, 16);
#endif
    rx_set_sp(sp);
    SU32(e, -0x10, 0x0078fc14);
    if (U8(0, 0x00da67d8) != 0) {
        SU32(e, -8, r.edi);
        alloc_cs(e - 0x10, 0, 0x0078fc30);
        if (U8(0, 0x00da67d8) != 0) alloc_cs(e - 0x10, 0, 0x0078fc40);
    }
    return p;
}

rx_addr rx_reflex_alloc_array(uint32_t n, uint32_t elem) {
    uint32_t bytes = rx_mul_sat(n, elem);
    uint32_t size = bytes > 0xfffffffbu ? 0xffffffffu : bytes + 4;
    rx_addr p = rx_reflex_aligned_alloc(size);
    if (!p) return 0;
    SU32(p, 0, n);
    return p + 4;
}

/* 008220e0 IMemoryAllocator::vf0 (scalar deleting destructor; ECX = the object, stack the delete flag; RET 4): the vtable
 * 0x0097539c written back; with bit 0 the object freed (0078fc50). EAX = the object. Frame: push esi. */
rx_addr rx_memory_allocator_dtor(rx_addr self, uint32_t flag) {
    const rx_addr entry0 = rx_sp(), E = entry0 - 4u;           /* push esi */
    rx_set_sp(E);
    SU32(self, 0, 0x0097539c);
    if (flag & 1) rx_reflex_aligned_free(self);
    rx_set_sp(entry0);
    return self;
}
RX_FUNCTION(0x008220e0, rx_memory_allocator_dtor)
