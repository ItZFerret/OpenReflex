/* jobthreads_watch.c (w-jobthreads4) -- the job guard of a -DRX_WATCH build: link it with any driver (jobthreads_hash.c,
 * stop_audit_run.c) and the -DRX_WATCH core objects (core/plan/QUEUE.md w-jobthreads4 gives the commands). Development only.
 *
 * Units are the simulation steps: marks 10 + u / 9 around unit u, 8 the end of a batch (loop_jobs.c's one-after-another run of the
 * simulation jobs, RX_JOB_THREADS=1; or, with RX_JOB_THREADS=0, the solver's own loop in world_phys.c); mark 7 = the unit took its
 * ordered turn (rx_job_ordered: what it does after may depend on the earlier units). Per guest word in a batch: the last writer
 * unit and the reader units. Reported, per kind and code site:
 *   RAW        a unit read a word another unit of the batch wrote, before its ordered turn (a dependence the split would break)
 *   DEADSTACK  a unit read a stack word (0x7f000000..0x7fffffff: the job contexts and the game thread's stack) it had not written
 *   WAR / WAW  a unit wrote a non-stack word another unit read / wrote before its ordered turn (harmless when the split keeps it private, logged or
 *              ordered: the in-place census shows the shared state; under RX_JOB_THREADS=1 only unexpected sharing remains)
 * At exit: one "JOBGUARD" line (the counts) and one "JOBGUARD SITE" line per site (the code address for atos; the slide is printed). */
#include "rx_mem.h"
#include "rx_reset.h"
#include <execinfo.h>
#include <mach-o/dyld.h>
#include <stdio.h>
#include <stdlib.h>

/* the state is RX_RESET_KEEP: a session's end restores the image's statics (rx_reset.c), this module's included when linked in */
int rx_watch_on RX_RESET_KEEP = 1;
#define WSLOTS (1u << 23)
typedef struct { uint32_t w; int8_t wu; uint32_t rmask; } wslot;
static wslot* g_ws RX_RESET_KEEP; static uint32_t* g_used RX_RESET_KEEP; static uint32_t g_nused RX_RESET_KEEP;
static int g_unit RX_RESET_KEEP = -1, g_ordered RX_RESET_KEEP, g_inpass RX_RESET_KEEP;
static unsigned long long g_passes RX_RESET_KEEP, g_units RX_RESET_KEEP, g_ordered_units RX_RESET_KEEP;
enum { K_RAW, K_WAR, K_WAW, K_DEAD, NK };
static const char* const kKind[NK] = {"RAW", "WAR", "WAW", "DEADSTACK"};
#define SITES 4096
static struct { uintptr_t pc; int kind; unsigned long long n; uint32_t a; } g_site[SITES] RX_RESET_KEEP;
static int g_nsite RX_RESET_KEEP;
static wslot* slot(uint32_t w) {
    uint32_t h = (w * 2654435761u) & (WSLOTS - 1);
    for (;;) {
        if (g_ws[h].w == w + 1) return &g_ws[h];
        if (g_ws[h].w == 0) { g_ws[h].w = w + 1; g_ws[h].wu = -1; g_ws[h].rmask = 0; g_used[g_nused++] = h; return &g_ws[h]; }
        h = (h + 1) & (WSLOTS - 1);
    }
}
static void report(int kind, uint32_t w) {
    void* bt[4]; const int n = backtrace(bt, 4);
    const uintptr_t pc = n > 2 ? (uintptr_t)bt[2] : 0;   /* 0 report, 1 rx_watch_access, 2 the accessor's caller (inlined accessor) */
    for (int i = 0; i < g_nsite; i++) if (g_site[i].pc == pc && g_site[i].kind == kind) { g_site[i].n++; return; }
    if (g_nsite < SITES) { g_site[g_nsite].pc = pc; g_site[g_nsite].kind = kind; g_site[g_nsite].n = 1; g_site[g_nsite].a = w * 4; g_nsite++; }
}
void rx_watch_access(rx_addr a, uint32_t n, int write) {
    if (g_unit < 0 || n == 0) return;
    const int u = g_unit;
    for (uint32_t w = a >> 2; w <= (a + n - 1) >> 2; w++) {
        const int stack = w >= (0x7f000000u >> 2) && w < (0x80000000u >> 2);
        wslot* e = slot(w);
        if (!write) {
            if (e->wu >= 0 && e->wu != u) { if (stack) report(K_DEAD, w); else if (!g_ordered) report(K_RAW, w); }
            else if (e->wu < 0 && stack) report(K_DEAD, w);
            e->rmask |= 1u << u;
        } else {
            if (!stack && !g_ordered) {   /* (after the ordered turn the earlier units are done: sequenced) */
                if (e->rmask & ~(1u << u)) report(K_WAR, w);
                if (e->wu >= 0 && e->wu != u) report(K_WAW, w);
            }
            e->wu = (int8_t)u;
            if (stack) e->rmask = 0;
        }
    }
}
void rx_watch_mark(int kind, rx_addr arg) {
    (void)arg;
    if (!g_ws) { g_ws = calloc(WSLOTS, sizeof *g_ws); g_used = malloc(sizeof(uint32_t) * WSLOTS); if (!g_ws || !g_used) abort(); }
    if (kind >= 10) { g_unit = kind - 10 > 31 ? 31 : kind - 10; g_ordered = 0; g_inpass = 1; g_units++; }
    else if (kind == 9) g_unit = -1;
    else if (kind == 7) { if (g_unit >= 0 && !g_ordered) { g_ordered = 1; g_ordered_units++; } }
    else if (kind == 8) {
        if (g_inpass) g_passes++;
        g_inpass = 0; g_unit = -1;
        for (uint32_t i = 0; i < g_nused; i++) g_ws[g_used[i]].w = 0;
        g_nused = 0;
    }
}
__attribute__((destructor)) static void guard_report(void) {
    unsigned long long tot[NK] = {0};
    for (int i = 0; i < g_nsite; i++) tot[g_site[i].kind] += g_site[i].n;
    fprintf(stderr, "JOBGUARD batches %llu units %llu (ordered %llu): RAW %llu DEADSTACK %llu WAR %llu WAW %llu, %d sites; slide %lx\n",
            g_passes, g_units, g_ordered_units, tot[K_RAW], tot[K_DEAD], tot[K_WAR], tot[K_WAW], g_nsite, (unsigned long)_dyld_get_image_vmaddr_slide(0));
    for (int i = 0; i < g_nsite; i++)
        fprintf(stderr, "JOBGUARD SITE %s %llu %lx %08x\n", kKind[g_site[i].kind], g_site[i].n, (unsigned long)g_site[i].pc, g_site[i].a);
}
