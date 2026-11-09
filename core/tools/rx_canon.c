/* rx_canon.c -- see rx_canon.h. A line-by-line C rendering of Boot::write_canonical (boot.cpp, "canonical heap dump") over the
 * core's arena: same node naming, pointer rules, leaf rule, hash and file layout; the roots are the executable's writable sections
 * plus the caller's extra roots (no CRT DLL data, TLS or stacks in the core). */
#include "rx_canon.h"
#include "../src/rx_heap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define DATA_LO 0x009a6000u
#define DATA_HI 0x00dcf76cu
#define TLS_LO  0x00dd0000u
#define TLS_HI  0x00dd0811u
#define STACK_LO 0x7fec0000u
#define STACK_HI 0x7ff00000u

typedef struct { rx_addr a; uint32_t size; uint8_t kind; uint32_t phase, off; uint64_t name, parent; int8_t leaf; } node;
typedef struct { uint8_t kind; uint32_t index, off; } ref;

static node* N; static int NN;
static rx_addr arena_lo, arena_hi;
static uint32_t* order; static int norder;
static int roots_weak;

static uint64_t mix(uint64_t h, uint64_t v) {
    h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2); h *= 0xff51afd7ed558ccdull; return h ^ (h >> 33);
}
static uint32_t u32(rx_addr a) { return rx_rd32(a); }

static ref lookup(uint32_t v) {
    ref none = {0, 0, 0};
    if (v < 0x10000) return none;
    int lo = 0, hi = NN;   /* upper_bound: first node with a > v */
    while (lo < hi) { int mid = (lo + hi) / 2; if (v < N[mid].a) hi = mid; else lo = mid + 1; }
    int text = 1; for (int i = 0; i < 4; i++) { uint8_t c = (uint8_t)(v >> (8 * i)); text = text && c >= 0x20 && c < 0x7f; }
    if (lo > 0) {
        const node* n = &N[lo - 1];
        if (v < n->a + n->size || (v == n->a + n->size && n->size)) {
            if (v != n->a && (text || (n->kind == 2 && (v & 3)))) return none;
            ref r = {1, (uint32_t)(lo - 1), v - n->a}; return r;
        }
    }
    if (text) return none;
    if (v >= arena_lo && v < arena_hi) { ref r = {2, 0, 0}; return r; }
    if (v >= STACK_LO && v < STACK_HI) { ref r = {3, 0, STACK_HI - v}; return r; }
    return none;
}
static void discover(uint32_t v, uint64_t parent, uint32_t off, uint64_t name, uint32_t phase) {
    ref r = lookup(v); if (r.kind != 1 || (parent == 0 && r.off && !roots_weak)) return;
    node* n = &N[r.index]; if (n->phase) return;
    n->phase = phase; n->parent = parent; n->off = off; n->name = name; order[norder++] = r.index;
}
static int leaf(node* n) {
    if (n->leaf >= 0) return n->leaf;
    int l = n->kind == 2;
    if (!l && n->size >= 0x10000 && !(u32(n->a) >= 0x401000 && u32(n->a) < 0x916000)) {
        uint32_t hits = 0, starts = 0;
        for (uint32_t o = 0; o + 4 <= n->size; o += 4) { uint32_t v = u32(n->a + o); if (v < 0x10000) continue; ref r = lookup(v); if (r.kind == 1) { hits++; starts += r.off == 0; } }
        l = hits >= 64 && 4 * starts < hits;
    }
    n->leaf = (int8_t)l; return l;
}
static void scan(int from, uint32_t phase, int weak) {
    for (int q = from; q < norder; q++) {
        node* n = &N[order[q]];
        if (leaf(n)) continue;
        uint64_t nm = n->name;
        for (uint32_t o = 0; o + 4 <= n->size; o += 4) {
            uint32_t v = u32(n->a + o); if (v < 0x10000) continue;
            ref r = lookup(v); if (r.kind != 1 || (r.off && !weak)) continue;
            discover(v, nm, o, mix(nm, o), phase);
        }
    }
}
static const rx_canon_root* g_extra; static int g_nextra;
static void roots1(void) {
    static const uint32_t secs[2][2] = {{DATA_LO, DATA_HI}, {TLS_LO, TLS_HI}};
    for (int s = 0; s < 2; s++)
        for (uint32_t a = secs[s][0]; a + 4 <= secs[s][1]; a += 4) { uint32_t v = u32(a); if (v >= 0x10000) discover(v, 0, a, mix(0x47, a), 1); }
    for (int k = 0; k < g_nextra; k++) if (g_extra[k].addr >= 0x10000) discover(g_extra[k].addr, 0, g_extra[k].key, mix(0x52, g_extra[k].key), 1);
}

/* classes.tsv: class \t vtable(hex) ... -> name by vtable (display only; the hash has the vtable, not the name) */
typedef struct { uint32_t vt; char name[200]; } cls_entry;
static cls_entry* C; static int NC;
static int cls_cmp(const void* x, const void* y) { uint32_t a = ((const cls_entry*)x)->vt, b = ((const cls_entry*)y)->vt; return a < b ? -1 : a > b; }
static void load_classes(const char* path) {
    NC = 0; if (!path) return;
    FILE* f = fopen(path, "r"); if (!f) return;
    char line[2048]; int cap = 0;
    if (!fgets(line, sizeof line, f)) { fclose(f); return; }
    while (fgets(line, sizeof line, f)) {
        char* tab = strchr(line, '\t'); if (!tab) continue; *tab = 0;
        uint32_t vt = (uint32_t)strtoul(tab + 1, NULL, 16); if (!vt) continue;
        if (NC == cap) { cap = cap ? cap * 2 : 2048; C = (cls_entry*)realloc(C, sizeof(cls_entry) * (size_t)cap); }
        C[NC].vt = vt; snprintf(C[NC].name, sizeof C[NC].name, "%s", line); NC++;
    }
    fclose(f); qsort(C, (size_t)NC, sizeof *C, cls_cmp);
}
static const char* class_of(const node* n, uint32_t* vt) {
    *vt = n->size >= 4 ? u32(n->a) : 0;
    if (*vt < 0x401000 || *vt >= 0x916000) { *vt = 0; return ""; }
    int lo = 0, hi = NC - 1;
    while (lo <= hi) { int mid = (lo + hi) / 2; if (C[mid].vt == *vt) return C[mid].name; if (C[mid].vt < *vt) lo = mid + 1; else hi = mid - 1; }
    return "";
}

int rx_canon_write(const char* path, const rx_canon_root* extra, int nextra, const char* classes, char* info, size_t ninfo) {
    int nb = rx_heap_blocks(); NN = 0;
    N = (node*)calloc((size_t)(nb ? nb : 1), sizeof(node));
    arena_lo = 0x10000000u; arena_hi = arena_lo;
    for (int i = 0; i < nb; i++) {
        rx_addr a; uint32_t sz; int live; rx_heap_block(i, &a, &sz, &live);
        if (a + sz > arena_hi) arena_hi = a + sz;
        if (!live) continue;
        N[NN].a = a; N[NN].size = sz; N[NN].kind = 1; N[NN].leaf = -1; NN++;
    }
    /* rx_heap's table is in address order already; keep the invariant explicit */
    for (int i = 1; i < NN; i++) if (N[i].a < N[i - 1].a) { fprintf(stderr, "rx_canon: heap table not in address order\n"); return 0; }
    order = (uint32_t*)malloc(sizeof(uint32_t) * (size_t)(NN ? NN : 1)); norder = 0;
    g_extra = extra; g_nextra = nextra; roots_weak = 0;
    roots1(); scan(0, 1, 0);
    roots_weak = 1; roots1(); scan(0, 1, 1);
    int phase1 = norder, reached = norder;
    for (int i = 0; i < NN; i++) if (!N[i].phase) { N[i].phase = 3; order[norder++] = (uint32_t)i; }
    load_classes(classes);
    gzFile f = gzopen(path, "wb1"); if (!f) { fprintf(stderr, "rx_canon: cannot write %s\n", path); return 0; }
#define PUT(p, n) gzwrite(f, (p), (unsigned)(n))
    uint32_t hdr[5] = {0x4e435852u, 1, (uint32_t)norder, (uint32_t)phase1, (uint32_t)reached}; PUT(hdr, 20);
    uint32_t* words = NULL; size_t wcap = 0;
    typedef struct { uint32_t w; uint8_t kind; uint64_t target; uint32_t off; } P;
    P* ptrs = NULL; size_t pcap = 0;
    unsigned long long bytes = 0, nptr = 0;
    for (int q = 0; q < norder; q++) {
        const node* n = &N[order[q]];
        uint32_t vt; const char* cls = class_of(n, &vt);
        int raw = leaf((node*)n);
        uint32_t nw = (n->size + 3) / 4, np = 0;
        if (nw > wcap) { wcap = nw; words = (uint32_t*)realloc(words, 4 * wcap); }
        if (nw > pcap) { pcap = nw; ptrs = (P*)realloc(ptrs, sizeof(P) * pcap); }
        uint64_t h = mix(n->size, vt);
        for (uint32_t w = 0; w < nw; w++) {
            uint32_t v = u32(n->a + 4 * w);
            if (4 * w + 4 > n->size) v &= (1u << (8 * (n->size & 3))) - 1;
            ref r = {0, 0, 0}; if (4 * w + 4 <= n->size && !raw) r = lookup(v);
            if (r.kind == 0) { words[w] = v; h = mix(h, v); continue; }
            words[w] = 0;
            uint64_t target = r.kind == 1 ? N[r.index].name : r.kind == 3 ? r.index : 0;
            ptrs[np].w = w; ptrs[np].kind = r.kind; ptrs[np].target = target; ptrs[np].off = r.off; np++;
            h = mix(mix(mix(h, 0x50000000ull + r.kind), target), r.off);
        }
        uint8_t h2[2] = {(uint8_t)n->phase, n->kind}; PUT(h2, 2);
        PUT(&n->name, 8); PUT(&n->parent, 8); PUT(&n->off, 4); PUT(&n->size, 4); PUT(&vt, 4);
        uint16_t cl = (uint16_t)(strlen(cls) < 200 ? strlen(cls) : 200); PUT(&cl, 2); if (cl) PUT(cls, cl);
        PUT(&h, 8); PUT(&nw, 4); if (nw) PUT(words, 4 * (size_t)nw); PUT(&np, 4);
        for (uint32_t i = 0; i < np; i++) { PUT(&ptrs[i].w, 4); PUT(&ptrs[i].kind, 1); PUT(&ptrs[i].target, 8); PUT(&ptrs[i].off, 4); }
        bytes += n->size; nptr += np;
    }
#undef PUT
    gzclose(f);
    {   /* sidecar <path>.addr: (name, address) per node in file order, for rx_canon_cmp.py (the original's side recovers addresses
           from its world image instead, build_map.resolve) */
        char ap[1024]; snprintf(ap, sizeof ap, "%s.addr", path);
        FILE* af = fopen(ap, "wb");
        if (af) { for (int q = 0; q < norder; q++) { fwrite(&N[order[q]].name, 8, 1, af); fwrite(&N[order[q]].a, 4, 1, af); } fclose(af); }
    }
    if (info && ninfo) snprintf(info, ninfo, "canonical dump: %d nodes (%d from globals / extra roots, %d unreached), %llu bytes, %llu pointers",
                                norder, phase1, norder - reached, bytes, nptr);
    free(words); free(ptrs); free(order); free(N); free(C); N = NULL; C = NULL; NC = 0;
    return 1;
}

int rx_canon_image(const char* path) {
    /* pages: .data / .tls and every page a live heap block touches */
    int nb = rx_heap_blocks();
    size_t cap = 4096, n = 0; uint32_t* pg = (uint32_t*)malloc(4 * cap);
#define ADDPG(p) do { if (n == cap) { cap *= 2; pg = (uint32_t*)realloc(pg, 4 * cap); } pg[n++] = (p); } while (0)
    for (uint32_t a = DATA_LO & ~0xfffu; a < TLS_HI; a += 0x1000) ADDPG(a);
    for (int i = 0; i < nb; i++) {
        rx_addr a; uint32_t sz; int live; rx_heap_block(i, &a, &sz, &live);
        if (!live) continue;
        for (uint32_t p = a & ~0xfffu; p < a + (sz ? sz : 1); p += 0x1000) if (!n || pg[n - 1] != p) ADDPG(p);
    }
#undef ADDPG
    /* sort + unique (blocks are in address order; .data pages come first and are below the heap) */
    size_t m = 0; for (size_t i = 0; i < n; i++) if (!m || pg[i] > pg[m - 1]) pg[m++] = pg[i];
    FILE* f = fopen(path, "wb"); if (!f) { free(pg); return 0; }
    uint32_t hdr[20] = {0x49575852u, 1, (uint32_t)m, 0}; fwrite(hdr, 4, 20, f);
    size_t base = 80 + 12 * m, at = 0;
    uint8_t page[4096]; uLongf cn; uint8_t* c = (uint8_t*)malloc(compressBound(4096));
    uint32_t* ent = (uint32_t*)calloc(m, 12);
    uint8_t* blob = NULL; size_t bcap = 0;
    for (size_t k = 0; k < m; k++) {
        int z = 1; for (int i = 0; i < 4096; i++) { page[i] = rx_rd8(pg[k] + (uint32_t)i); z = z && !page[i]; }
        ent[3 * k] = pg[k];
        if (z) continue;
        cn = compressBound(4096); compress2(c, &cn, page, 4096, 6);
        const uint8_t* src = cn >= 4096 ? page : c; size_t len = cn >= 4096 ? 4096 : cn;
        if (at + len > bcap) { bcap = (at + len) * 2; blob = (uint8_t*)realloc(blob, bcap); }
        memcpy(blob + at, src, len); ent[3 * k + 1] = (uint32_t)(base + at); ent[3 * k + 2] = (uint32_t)len; at += len;
    }
    fwrite(ent, 12, m, f); if (at) fwrite(blob, 1, at, f); fclose(f);
    free(pg); free(c); free(ent); free(blob);
    return 1;
}
