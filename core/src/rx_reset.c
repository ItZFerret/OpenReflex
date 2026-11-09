/* rx_reset.c -- see rx_reset.h. */
#include "rx_reset.h"
#include "rx_x87.h"

#if RX_RESET_AVAILABLE && !defined(_WIN32)
#include <dlfcn.h>
#include <mach-o/getsect.h>
#include <mach-o/loader.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { uint8_t* addr; size_t size; uint8_t* copy; } region;
static const struct { const char* seg; const char* sect; } k_sections[] = {
    {"__DATA", "__data"}, {"__DATA", "__bss"}, {"__DATA", "__common"},
};
#define NREGIONS (int)(sizeof k_sections / sizeof k_sections[0])

/* outside the covered sections, or the restore would wipe its own record */
static region g_regions[NREGIONS] RX_RESET_KEEP;
static int g_captured RX_RESET_KEEP;

int rx_reset_available(void) { return 1; }

int rx_reset_capture(void) {
    if (g_captured) return 1;
    Dl_info info;
    if (!dladdr((void*)&rx_reset_capture, &info) || !info.dli_fbase) return 0;
    const struct mach_header_64* mh = (const struct mach_header_64*)info.dli_fbase;
    rx_x87_warm();   /* its constants are C++ statics built on first use (fixed storage, no heap): record them built */
    region r[NREGIONS];
    memset(r, 0, sizeof r);
    for (int i = 0; i < NREGIONS; i++) {
        unsigned long size = 0;
        uint8_t* p = getsectiondata(mh, k_sections[i].seg, k_sections[i].sect, &size);
        if (!p || !size) continue;
        r[i].addr = p; r[i].size = (size_t)size;
        r[i].copy = (uint8_t*)malloc(size);
        if (!r[i].copy) { for (int k = 0; k < i; k++) free(r[k].copy); return 0; }
    }
    for (int i = 0; i < NREGIONS; i++) if (r[i].addr) memcpy(r[i].copy, r[i].addr, r[i].size);
    memcpy(g_regions, r, sizeof r);
    g_captured = 1;
    return 1;
}

static long differ(const uint8_t* a, const uint8_t* b, size_t n) {
    long d = 0;
    for (size_t i = 0; i < n; i++) d += a[i] != b[i];
    return d;
}

long rx_reset_dirty(void) {
    if (!g_captured) return -1;
    long d = 0;
    for (int i = 0; i < NREGIONS; i++) if (g_regions[i].addr) d += differ(g_regions[i].addr, g_regions[i].copy, g_regions[i].size);
    return d;
}

/* RX_RESET_DEBUG=1: lists the ranges a restore changes (offset in the section, length, nearest exported symbol if any). Static
 * symbols are not in dladdr's table: match the offsets against `nm -m` of the image. */
static void report(int i) {
    const region* r = &g_regions[i];
    size_t k = 0;
    while (k < r->size) {
        if (r->addr[k] == r->copy[k]) { k++; continue; }
        size_t e = k;
        while (e < r->size && r->addr[e] != r->copy[e]) e++;
        fprintf(stderr, "rx_reset: %s,%s +0x%zx (%p) %zu byte(s) differ\n", k_sections[i].seg, k_sections[i].sect, k, (void*)(r->addr + k), e - k);
        k = e;
    }
}

long rx_reset_restore(void) {
    if (!g_captured) return -1;
    long d = 0;
    const int debug = getenv("RX_RESET_DEBUG") != NULL;
    for (int i = 0; i < NREGIONS; i++) {
        if (!g_regions[i].addr) continue;
        if (debug) report(i);
        d += differ(g_regions[i].addr, g_regions[i].copy, g_regions[i].size);
        memcpy(g_regions[i].addr, g_regions[i].copy, g_regions[i].size);
    }
    return d;
}

#elif !defined(_WIN32)
int  rx_reset_available(void) { return 0; }
int  rx_reset_capture(void) { return 0; }
long rx_reset_restore(void) { return -1; }
long rx_reset_dirty(void) { return -1; }
#endif
