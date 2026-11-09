/* build_pool.c -- see build_pool.h. Hand transliteration from the disassembly (0x00743f00, 0x00744180, 0x008235f0..0x00823b53):
 * the original's compares (unsigned jb / ja / jae), its write order and its quirks (the cell range check accepts one cell past the
 * end; a pool pop that finds 0 still advances the top; the chunk split pushes the remainder before unlinking the chunk). */
#include "build_pool.h"
#include "build_alloc.h"
#include "crt.h"

#define CHUNK_USED   0x0c          /* byte: 1 in use, 0 free */
#define MARK_AB_CD   0xcdabu       /* the marker after the used byte (.rdata 0x0092e068 / 0x0092e06a) */
#define MARK_EF      0xefu

static void mark(rx_addr chunk) { SU16(chunk, 0x0d, MARK_AB_CD); SU8(chunk, 0x0f, MARK_EF); }

/* 00823710 */
void rx_pool_heap_init(rx_addr h) {
    SU32(h, 0x08, 0x10);
    SU32(h, 0x00, 0x300000);
    rx_addr blk = rx_crt_aligned_malloc(0x300000, 0x10);   /* under the allocator lock in the original */
    uint32_t al = U32(h, 0x08);
    SU32(h, 0x10, blk);
    uint32_t first = ((blk + al - 1) / al) * al;
    SU32(h, 0x14, first);
    SU32(h, 0x0c, ((al + 0xf) / al) * al);
    uint32_t end = blk + (U32(h, 0x00) - first);
    end -= end % al;
    SU32(h, 0x04, end);
    SU8(first, CHUNK_USED, 0);
    SU32(U32(h, 0x14), 0, U32(h, 0x04) - U32(h, 0x0c));
    SU32(h, 0x18, U32(h, 0x14));
    SU32(U32(h, 0x18), 4, 0);
    SU32(U32(h, 0x18), 8, 0);
    mark(U32(h, 0x14));
}
RX_FUNCTION(0x00823710, rx_pool_heap_init);

/* 00823a50: take a chunk out of the free list */
void rx_pool_heap_unlink(rx_addr h, rx_addr c) {
    if (U32(h, 0x18) == c) {
        rx_addr n = U32(c, 4);
        SU32(h, 0x18, n);
        if (n == 0) { SU32(c, 8, 0); SU32(c, 4, 0); return; }
        SU32(n, 8, 0); SU32(c, 8, 0); SU32(c, 4, 0);
        return;
    }
    rx_addr p = U32(c, 8);
    if (p) SU32(p, 4, U32(c, 4));
    rx_addr n = U32(c, 4);
    if (n) SU32(n, 8, U32(c, 8));
    SU32(c, 8, 0); SU32(c, 4, 0);
}
RX_FUNCTION(0x00823a50, rx_pool_heap_unlink);

/* 008239a0: walk the chunks from the first one; a free chunk swallows the free chunks that follow it (unlinked from the list) */
void rx_pool_heap_coalesce(rx_addr h) {
    rx_addr c = U32(h, 0x14);
    if (!c) return;
    for (;;) {
        if (U8(c, CHUNK_USED) == 0) {
            rx_addr n = U32(c, 0) + c + U32(h, 0x0c);
            if (n - U32(h, 0x14) < U32(h, 0x04) && n != 0 && U8(n, CHUNK_USED) == 0) {
                for (;;) {
                    if (U32(h, 0x18) == n) {
                        rx_addr nn = U32(n, 4);
                        SU32(h, 0x18, nn);
                        if (nn) SU32(nn, 8, 0);
                    } else {
                        rx_addr p = U32(n, 8);
                        if (p) SU32(p, 4, U32(n, 4));
                        rx_addr nn = U32(n, 4);
                        if (nn) SU32(nn, 8, U32(n, 8));
                    }
                    SU32(n, 4, 0); SU32(n, 8, 0);
                    SU32(c, 0, U32(c, 0) + U32(n, 0) + U32(h, 0x0c));
                    n = U32(c, 0) + c + U32(h, 0x0c);
                    if (n - U32(h, 0x14) >= U32(h, 0x04) || n == 0 || U8(n, CHUNK_USED) != 0) break;
                }
            }
        }
        rx_addr n = U32(c, 0) + c + U32(h, 0x0c);
        if (n - U32(h, 0x14) >= U32(h, 0x04) || n == 0) return;
        c = n;
    }
}
RX_FUNCTION(0x008239a0, rx_pool_heap_coalesce);

/* best fit over the free list (an exact fit stops the search) */
static rx_addr best_fit(rx_addr h, uint32_t size) {
    rx_addr best = 0; uint32_t bsz = 0xffffffffu;
    for (rx_addr c = U32(h, 0x18); c; c = U32(c, 4)) {
        uint32_t s = U32(c, 0);
        if (s >= size && s < bsz) { best = c; bsz = s; if (s == size) break; }
    }
    return best;
}

/* 008237e0 */
rx_addr rx_pool_heap_alloc(rx_addr h, uint32_t n) {
    uint32_t al = U32(h, 0x08);
    uint32_t size = ((n + al - 1) / al) * al;
    rx_addr c = best_fit(h, size);
    if (!c) {
        if (!U32(h, 0x18)) return 0;
        rx_pool_heap_coalesce(h);
        if (!U32(h, 0x18)) return 0;
        c = best_fit(h, size);
        if (!c) return 0;
    }
    uint32_t rest = U32(c, 0);
    SU32(c, 0, size);
    SU8(c, CHUNK_USED, 1);
    uint32_t hdr = U32(h, 0x0c);
    rest -= size;
    rx_addr payload = hdr + c;
    if (rest <= hdr) { SU32(c, 0, rest + size); rx_pool_heap_unlink(h, c); return payload; }
    rx_addr r = size + payload;               /* the remainder becomes a free chunk at the list head */
    SU8(r, CHUNK_USED, 0);
    SU32(r, 0, rest - U32(h, 0x0c));
    mark(r);
    if (U32(h, 0x18) == 0) { SU32(h, 0x18, r); SU32(r, 8, 0); SU32(U32(h, 0x18), 4, 0); }
    else { SU32(r, 8, 0); SU32(r, 4, U32(h, 0x18)); SU32(U32(h, 0x18), 8, r); SU32(h, 0x18, r); }
    rx_pool_heap_unlink(h, c);
    return payload;
}
RX_FUNCTION(0x008237e0, rx_pool_heap_alloc);

/* 00823960 */
void rx_pool_heap_free(rx_addr h, rx_addr p) {
    if (!p) return;
    rx_addr c = p - ((U32(h, 0x08) - 1) & p) - U32(h, 0x0c);
    SU8(c, CHUNK_USED, 0);
    if (U32(h, 0x18) == 0) { SU32(h, 0x18, c); SU32(c, 8, 0); SU32(U32(h, 0x18), 4, 0); return; }
    SU32(c, 8, 0); SU32(c, 4, U32(h, 0x18)); SU32(U32(h, 0x18), 8, c); SU32(h, 0x18, c);
}
RX_FUNCTION(0x00823960, rx_pool_heap_free);

/* 00823900: a new chunk, the old payload copied (min(old chunk size, size)), the old chunk freed */
rx_addr rx_pool_heap_realloc(rx_addr h, rx_addr old, uint32_t size) {
    rx_addr p = rx_pool_heap_alloc(h, size);
    if (!old || !p) return p;
    uint32_t m = U32(h, 0x08) - 1;
    uint32_t osz = U32(old - (m & old) - U32(h, 0x0c), 0);
    p -= m & p;
    crt_memcpy(p, old, osz < size ? osz : size);
    rx_pool_heap_free(h, old);
    return p;
}
RX_FUNCTION(0x00823900, rx_pool_heap_realloc);

/* 00823a90 */
rx_addr rx_pool_cells_construct(rx_addr pool) {
    SU32(pool, 0x00, 0); SU32(pool, 0x04, 1); SU32(pool, 0x08, 0); SU32(pool, 0x0c, 0); SU32(pool, 0x10, 0); SU32(pool, 0x14, 0xffffffffu);
    return pool;
}
RX_FUNCTION(0x00823a90, rx_pool_cells_construct);

/* 00823ab0: the cells and the free stack from the heap at sys + 0xf0; stack[0] = 0, stack[i] = cell i (cell 0 is never handed out),
 * top = 1 */
void rx_pool_cells_build(rx_addr pool, rx_addr sys) {
    if (U32(pool, 4) <= 1) return;
    rx_addr heap = sys + 0xf0;
    SU32(pool, 0x0c, rx_pool_heap_alloc(heap, U32(pool, 0) * U32(pool, 4)));
    rx_addr st = rx_pool_heap_alloc(heap, U32(pool, 4) * 4);
    SU32(pool, 0x10, st);
    SU32(st, 0, 0);
    SU32(pool, 0x14, 1);
    for (uint32_t i = 1; i < U32(pool, 4); i++) SU32(U32(pool, 0x10) + 4 * i, 0, U32(pool, 0) * i + U32(pool, 0x0c));
}
RX_FUNCTION(0x00823ab0, rx_pool_cells_build);

/* 00823b20 */
int rx_pool_cells_free(rx_addr pool, rx_addr p) {
    uint32_t n = U32(pool, 4);
    if (n == 1) return 0;
    uint32_t base = U32(pool, 0x0c), cs = U32(pool, 0);
    if (p < cs + base || p > cs * n + base) return 0;
    SU32(pool, 0x14, U32(pool, 0x14) - 1);
    SU32(U32(pool, 0x10) + 4 * U32(pool, 0x14), 0, p);
    return 1;
}
RX_FUNCTION(0x00823b20, rx_pool_cells_free);

/* the pool for a rounded size 0x10..0x80: sys + 0x30 + 0x18 * (size / 16 - 1) (the original indexes ((size >> 4) + 1) * 0x18 from
 * sys - 0x18 + 0x30: the same) */
static rx_addr pool_of(rx_addr sys, uint32_t size) { return sys + 0x30 + 0x18u * ((uint32_t)((int32_t)size >> 4) - 1); }

/* pop a cell: top < count -> stack[top++] (0 when that slot is empty: the top still moves) */
static rx_addr pool_pop(rx_addr pool) {
    uint32_t t = U32(pool, 0x14);
    if (t >= U32(pool, 4)) return 0;
    rx_addr c = U32(U32(pool, 0x10) + 4 * t, 0);
    SU32(pool, 0x14, t + 1);
    return c;
}

/* 00743f00. The original reuses two of its argument slots: the free path counts its unrolled pool loop (2 -> 0) in nsize's slot, the
 * pool realloc path keeps the pool's address in ud's slot (rx_lua_alloc_slots: the adapter writes them like the original) */
rx_lua_alloc_args rx_lua_alloc_slots;
rx_addr rx_lua_alloc(rx_addr ud, rx_addr ptr, uint32_t osize, uint32_t nsize) {
    (void)osize;
    rx_lua_alloc_slots.ud_written = 0; rx_lua_alloc_slots.nsize_written = 0;
    uint32_t size = ((nsize + 0xf) >> 4) << 4;
    if (size == 0) {   /* free: every pool whose range holds ptr takes it back (no early exit), else the heap chunk */
        int found = 0;
        for (int i = 0; i < 8; i++) {
            rx_addr pool = ud + 0x30 + 0x18u * (uint32_t)i;
            uint32_t n = U32(pool, 4);
            if (n == 1) continue;
            uint32_t base = U32(pool, 0x0c), cs = U32(pool, 0);
            if (ptr < base + cs || ptr > cs * n + base) continue;
            SU32(pool, 0x14, U32(pool, 0x14) - 1);
            SU32(U32(pool, 0x10) + 4 * U32(pool, 0x14), 0, ptr);
            found = 1;
        }
        rx_lua_alloc_slots.nsize_written = 1; rx_lua_alloc_slots.nsize = 0;
        if (!found && ptr) {
            rx_addr c = ptr - ((U32(ud, 0xf8) - 1) & ptr) - U32(ud, 0xfc);
            SU8(c, CHUNK_USED, 0);
            if (U32(ud, 0x108) == 0) { SU32(ud, 0x108, c); SU32(c, 8, 0); SU32(U32(ud, 0x108), 4, 0); return 0; }
            SU32(c, 8, 0); SU32(c, 4, U32(ud, 0x108)); SU32(U32(ud, 0x108), 8, c); SU32(ud, 0x108, c);
        }
        return 0;
    }
    if (ptr == 0) {   /* malloc */
        if (size - 0x10 <= 0x70) { rx_addr c = pool_pop(pool_of(ud, size)); if (c) return c; }
        return rx_pool_heap_alloc(ud + 0xf0, size);
    }
    /* realloc: the first pool whose range holds ptr */
    for (int i = 0; i < 8; i++) {
        rx_addr pool = ud + 0x30 + 0x18u * (uint32_t)i;
        uint32_t n = U32(pool, 4);
        if (n == 1) continue;
        uint32_t base = U32(pool, 0x0c), cs = U32(pool, 0);
        if (ptr < base + cs || ptr > cs * n + base) continue;
        uint32_t old = U32(pool, 0);
        rx_lua_alloc_slots.ud_written = 1; rx_lua_alloc_slots.ud = pool;
        if (old == size) return ptr;
        rx_addr q = 0;
        if (size - 0x10 <= 0x70) q = pool_pop(pool_of(ud, size));
        if (!q) { q = rx_pool_heap_alloc(ud + 0xf0, size); if (!q) return 0; }
        crt_memcpy(q, ptr, old < size ? old : size);
        rx_pool_cells_free(pool, ptr);
        return q;
    }
    return rx_pool_heap_realloc(ud + 0xf0, ptr, size);
}
RX_FUNCTION(0x00743f00, rx_lua_alloc);

void rx_lua_memory_init(rx_addr sys) {
    /* InitializeCriticalSection(sys + 8) (OS state) */
    SU32(sys, 0x24, 0); SU32(sys, 0x28, 0); SU32(sys, 0x20, 0);
    for (int i = 0; i < 8; i++) rx_pool_cells_construct(sys + 0x30 + 0x18u * (uint32_t)i);
    for (uint32_t o = 0x110; o < 0x138; o += 4) SU32(sys, o, 0);   /* 0x00d94a48 .. 0x00d94a6c */
    rx_pool_heap_init(sys + 0xf0);
    static const uint32_t cell[8] = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80 };
    static const uint32_t count[8] = { 0x5dd, 0x2711, 0x1f41, 0x5dd, 0xbb9, 0x1f5, 0x191, 0x191 };
    /* the original's store order: 0x5dd into pools 0 and 3 first, then cell size / count / 0x10 per pool */
    SU32(sys, 0x34, 0x5dd); SU32(sys, 0x7c, 0x5dd);
    for (int i = 0; i < 8; i++) {
        rx_addr pool = sys + 0x30 + 0x18u * (uint32_t)i;
        SU32(pool, 0, cell[i]);
        if (i != 0 && i != 3) SU32(pool, 4, count[i]);
        SU32(pool, 8, 0x10);
    }
    for (int i = 0; i < 8; i++) rx_pool_cells_build(sys + 0x30 + 0x18u * (uint32_t)i, sys);
}

/* ---- SimpleMemoryAllocator ---- */

/* 00823600 (ESI = self; the caller allocated 0x14 bytes) */
rx_addr rx_simple_allocator_construct(rx_addr s) {
    SU32(s, 0, 0x009753c4);   /* SimpleMemoryAllocator::vftable */
    rx_addr blk = rx_crt_aligned_malloc(0x19000, 0x10);
    SU32(s, 4, blk); SU32(s, 8, blk); SU32(s, 0xc, 0x19000); SU32(s, 0x10, 0);
    return s;
}
RX_FUNCTION(0x00823600, rx_simple_allocator_construct);

/* vf1 008236d0 */
rx_addr rx_simple_allocator_alloc(rx_addr s, uint32_t size) {
    uint32_t sz = (size + 0x13) & ~0xfu;
    uint32_t used = U32(s, 0x10) + sz;
    if (used >= U32(s, 0xc)) return 0;
    rx_addr p = U32(s, 8);
    SU32(s, 0x10, used);
    SU32(s, 8, sz + p);
    return p;
}
RX_FUNCTION(0x008236d0, rx_simple_allocator_alloc);

/* vf3 00823700 */
void rx_simple_allocator_reset(rx_addr s) { SU32(s, 8, U32(s, 4)); SU32(s, 0x10, 0); }
RX_FUNCTION(0x00823700, rx_simple_allocator_reset);

/* 00823680 */
void rx_simple_allocator_destruct(rx_addr s) {
    rx_addr blk = U32(s, 4);
    SU32(s, 0, 0x009753c4);
    rx_crt_aligned_free(blk);
    SU32(s, 0, 0x0097539c);   /* IMemoryAllocator::vftable */
}
RX_FUNCTION(0x00823680, rx_simple_allocator_destruct);

/* vf0 00823660 */
rx_addr rx_simple_allocator_sdtor(rx_addr s, uint32_t flags) {
    rx_simple_allocator_destruct(s);
    if (flags & 1) rx_reflex_aligned_free(s);
    return s;
}
RX_FUNCTION(0x00823660, rx_simple_allocator_sdtor);
