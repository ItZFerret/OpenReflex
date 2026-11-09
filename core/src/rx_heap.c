/* rx_heap.c -- see rx_heap.h. A bump allocator with a free list per exact size (the core frees rarely: worlds are built once and
 * dropped whole), and the block table in allocation (= address) order. */
#include "rx_heap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__APPLE__) || defined(__linux__)
#include <execinfo.h>
#endif

#define HEAP_LO 0x10000000u
#define HEAP_HI 0x70000000u
typedef struct { rx_addr a; uint32_t size; int live; } block;
static block* g_blocks; static int g_count, g_cap;
static rx_addr g_top = HEAP_LO;
/* RX_HEAP_REDZONE=<bytes> (development, w-rc-core): every new block is followed by that many bytes of the pattern 0xcb (its slack to the
 * 16-byte end too); rx_heap_redzone_check finds the blocks whose tail was written: an array written past its block. Off (0) by default:
 * then nothing here changes an address or a byte. Read at the first allocation of a session. */
static uint32_t g_rz; static int g_rz_read;
static uint32_t redzone(void) {
    if (!g_rz_read) { g_rz_read = 1; const char* e = getenv("RX_HEAP_REDZONE"); g_rz = e ? ((uint32_t)atoi(e) + 15u) & ~15u : 0; }
    return g_rz;
}
static void rz_fill(rx_addr from, rx_addr to) { for (rx_addr x = from; x < to; x++) rx_wr8(x, 0xcb); }

/* Dead blocks by size (w-perfgrow). The reuse rule below is "the lowest-index dead block of this exact size whose address fits the alignment";
 * it was a scan of every block ever made (170 thousand after a level load) on each allocation: 5 % of a frame in the first seconds of a race
 * (the spawn allocates ~1600 blocks a frame), and the same on every allocation later. Now each size keeps the indices of its dead blocks (a
 * hash table of lists) and the rule is applied to that list only: the same block is chosen, so every address is the same as before. */
typedef struct { uint32_t size; int n, cap; int* idx; } deadlist;
static deadlist* g_dead; static uint32_t g_dead_cap, g_dead_used;   /* open addressing on the size; g_dead_cap a power of two */
static deadlist* dead_find(uint32_t size, int create) {
    if (!g_dead_cap) { if (!create) return NULL; g_dead_cap = 1024; g_dead = (deadlist*)calloc(g_dead_cap, sizeof *g_dead); }
    for (uint32_t i = (size * 2654435761u) & (g_dead_cap - 1);; i = (i + 1) & (g_dead_cap - 1)) {
        if (g_dead[i].idx && g_dead[i].size == size) return &g_dead[i];
        if (!g_dead[i].idx && !g_dead[i].cap) {
            if (!create) return NULL;
            if ((g_dead_used + 1) * 2 > g_dead_cap) {   /* grow, rehash */
                deadlist* old = g_dead; uint32_t oc = g_dead_cap; g_dead_cap *= 2; g_dead = (deadlist*)calloc(g_dead_cap, sizeof *g_dead);
                for (uint32_t k = 0; k < oc; k++) if (old[k].cap) { uint32_t j = (old[k].size * 2654435761u) & (g_dead_cap - 1); while (g_dead[j].cap) j = (j + 1) & (g_dead_cap - 1); g_dead[j] = old[k]; }
                free(old);
                return dead_find(size, create);
            }
            g_dead_used++; g_dead[i].size = size; g_dead[i].cap = 4; g_dead[i].n = 0; g_dead[i].idx = (int*)malloc(4 * sizeof(int));
            return &g_dead[i];
        }
    }
}
static int find(rx_addr a) {
    int lo = 0, hi = g_count - 1;
    while (lo <= hi) { int mid = (lo + hi) / 2; if (g_blocks[mid].a == a) return mid; if (g_blocks[mid].a < a) lo = mid + 1; else hi = mid - 1; }
    return -1;
}
rx_addr rx_heap_alloc(uint32_t size, uint32_t align) {
    if (!align) align = 16;
#if defined(__APPLE__) || defined(__linux__)
    if (redzone()) {   /* RX_HEAP_TRACE_SIZE=<hex size> (development, with the redzone): the host stack of every allocation of that size */
        static long want = -2;
        if (want == -2) { const char* e = getenv("RX_HEAP_TRACE_SIZE"); want = e ? strtol(e, NULL, 16) : -1; }
        if (want >= 0 && (uint32_t)want == size) { void* bt[16]; fprintf(stderr, "rxcore: heap alloc 0x%x:\n", size); backtrace_symbols_fd(bt, backtrace(bt, 16), 2); }
    }
#endif
    if (size == 0) size = 1;
    deadlist* dl = dead_find(size, 0);   /* reuse a freed block of the same size and alignment: the lowest index */
    if (dl && dl->n) {
        int best = -1;
        for (int j = 0; j < dl->n; j++) {
            const int i = dl->idx[j];
            if ((g_blocks[i].a & (align - 1)) == 0 && (best < 0 || i < dl->idx[best])) best = j;
        }
        if (best >= 0) {
            const int i = dl->idx[best];
            dl->idx[best] = dl->idx[--dl->n];
            g_blocks[i].live = 1;
            memset(rx_base + g_blocks[i].a, 0, size);
            return g_blocks[i].a;
        }
    }
    rx_addr a = (g_top + align - 1) & ~(align - 1);
    if ((uint64_t)a + size > HEAP_HI) { fprintf(stderr, "rxcore: heap exhausted (%u bytes)\n", size); abort(); }
    g_top = a + ((size + 15u) & ~15u);
    if (redzone()) { g_top += g_rz; rz_fill(a + size, g_top); }
    if (g_count == g_cap) { g_cap = g_cap ? g_cap * 2 : 4096; g_blocks = (block*)realloc(g_blocks, sizeof(block) * (size_t)g_cap); }
    g_blocks[g_count].a = a; g_blocks[g_count].size = size; g_blocks[g_count].live = 1; g_count++;
    memset(rx_base + a, 0, size);
    return a;
}
rx_addr rx_heap_alloc_fill(uint32_t size, uint32_t align, uint8_t fill) {
    rx_addr a = rx_heap_alloc(size, align);
    if (fill) for (uint32_t k = 0; k < size; k++) rx_wr8(a + k, fill);
    return a;
}
int rx_heap_resize(rx_addr a, uint32_t new_size) {
    int i = find(a);
    if (i < 0 || !g_blocks[i].live) return 0;
    uint32_t old = g_blocks[i].size, room = (old + 15u) & ~15u;
    if (i == g_count - 1) {   /* the last block: move the top */
        if ((uint64_t)a + new_size > HEAP_HI) return 0;
        g_top = a + ((new_size + 15u) & ~15u);
        if (g_rz) { g_top += g_rz; rz_fill(a + new_size, g_top); }
    } else if (new_size > room) return 0;
    for (uint32_t k = old; k < new_size; k++) rx_wr8(a + k, 0);
    if (g_rz && i != g_count - 1) rz_fill(a + new_size, a + room + g_rz);
    g_blocks[i].size = new_size ? new_size : 1;
    return 1;
}
void rx_heap_free(rx_addr a) {
    int i = find(a);
    if (i < 0 || !g_blocks[i].live) return;
    g_blocks[i].live = 0;
    deadlist* dl = dead_find(g_blocks[i].size, 1);
    if (dl->n == dl->cap) { dl->cap *= 2; dl->idx = (int*)realloc(dl->idx, (size_t)dl->cap * sizeof(int)); }
    dl->idx[dl->n++] = i;
}
uint32_t rx_heap_size(rx_addr a) { int i = find(a); return i >= 0 && g_blocks[i].live ? g_blocks[i].size : 0; }
int rx_heap_blocks(void) { return g_count; }
void rx_heap_block(int i, rx_addr* a, uint32_t* size, int* live) { *a = g_blocks[i].a; *size = g_blocks[i].size; *live = g_blocks[i].live; }
int rx_heap_redzone_check(int report) {
    if (!g_rz) return -1;
    int bad = 0;
    for (int i = 0; i < g_count; i++) {
        const rx_addr a = g_blocks[i].a, from = a + g_blocks[i].size, to = a + ((g_blocks[i].size + 15u) & ~15u) + g_rz;
        for (rx_addr x = from; x < to; x++) if (rx_rd8(x) != 0xcb) {
            if (bad < report) fprintf(stderr, "rxcore: heap redzone: block %08x size 0x%x (%s) written at +0x%x (%02x)\n", a, g_blocks[i].size,
                                      g_blocks[i].live ? "live" : "free", x - a, rx_rd8(x));
            bad++; break;
        }
    }
    return bad;
}
void rx_heap_reset(void) {
    free(g_blocks); g_blocks = NULL; g_count = g_cap = 0; g_top = HEAP_LO;
    for (uint32_t k = 0; k < g_dead_cap; k++) free(g_dead[k].idx);
    free(g_dead); g_dead = NULL; g_dead_cap = g_dead_used = 0;
    g_rz_read = 0; g_rz = 0;
}   /* the table is host memory: a session's end releases it */
