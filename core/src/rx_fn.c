/* rx_fn.c -- the function registry (rx_fn.h): open addressing over original addresses. */
#include "rx_fn.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef RX_FN_TRACE
#undef rx_fn   /* the real function is defined here; the macro (rx_fn.h) records the call site of every other caller */
#endif

#define RX_FN_CAP 32768
static struct { rx_addr a; void* fn; const char* name; } g_tab[RX_FN_CAP];
static int g_count;

static unsigned slot_of(rx_addr a) { return (unsigned)((a * 2654435761u) >> 17) & (RX_FN_CAP - 1); }

void rx_fn_register(rx_addr a, void* fn, const char* name) {
    unsigned i = slot_of(a);
    while (g_tab[i].a && g_tab[i].a != a) i = (i + 1) & (RX_FN_CAP - 1);
    if (g_tab[i].a == a && g_tab[i].fn != fn) { fprintf(stderr, "rxcore: %08x registered twice (%s, %s)\n", a, g_tab[i].name, name); abort(); }
    if (!g_tab[i].a) g_count++;
    g_tab[i].a = a; g_tab[i].fn = fn; g_tab[i].name = name;
}
static int find(rx_addr a) {
    unsigned i = slot_of(a);
    while (g_tab[i].a) { if (g_tab[i].a == a) return (int)i; i = (i + 1) & (RX_FN_CAP - 1); }
    return -1;
}
void* rx_fn(rx_addr a) { int i = find(a); return i < 0 ? NULL : g_tab[i].fn; }
const char* rx_fn_name(rx_addr a) { int i = find(a); return i < 0 ? NULL : g_tab[i].name; }
int rx_fn_count(void) { return g_count; }

#ifdef RX_FN_TRACE
/* ---- -DRX_FN_TRACE: the runtime record of the typed paths (w-vtlog) ----------------------------------------------------------------------
 * A fixed open-addressing set of distinct (file, line, type, target, ra[5]); a slot is claimed with a CAS (0 empty, 1 being filled, 2 ready),
 * so the job pool's workers need no lock. ra = the return addresses of the frames above the traced call (frame-pointer walk; the build keeps
 * frame pointers): with the wrapper functions (vcall0, loop_tc0, anim_vcall0 ...) inlined or not, the debug info of the five addresses names
 * the caller that passed the slot. Output, one line per record:
 *   <file>:<line> TAB <type or -> TAB <target hex> TAB <image>+<offset>,<image>+<offset>,<image>+<offset> TAB <hits>
 * file names are as __FILE__ was compiled (relative to core/). */
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <dlfcn.h>

#include "rx_reset.h"
#define TR_CAP (1u << 16)
#define TR_NRA 5      /* frames above the traced call kept: a wrapper nested in a wrapper (anim_vcall0 > sc_vcall0 ...) needs its outermost caller */
typedef struct { _Atomic uint32_t state; uint32_t target; const char* file; const char* type; int line; _Atomic uint32_t hits; uintptr_t ra[TR_NRA]; } tr_slot;
/* RX_RESET_KEEP: the session end copies the statics back to their recorded values (rx_reset.h); the trace must survive it */
static tr_slot g_tr[TR_CAP] RX_RESET_KEEP;
static _Atomic uint32_t g_tr_n RX_RESET_KEEP;

const char* rx_fn_trace_ints(int total) {
    static const char* const nm[] = {"rx_ints/0", "rx_ints/1", "rx_ints/2", "rx_ints/3", "rx_ints/4", "rx_ints/5", "rx_ints/6", "rx_ints/7", "rx_ints/N"};
    return nm[total >= 0 && total <= 7 ? total : 8];
}
static int tr_valid(uintptr_t p) { return p > 0x100000 && !(p & 7) && p < ((uintptr_t)1 << 47); }
__attribute__((noinline)) void* rx_fn_traced(rx_addr a, const char* file, int line, const char* type) {
    void* fn = rx_fn(a);
    uintptr_t ra[TR_NRA] = {0};
    uintptr_t* fp = (uintptr_t*)__builtin_frame_address(0);
    for (int k = 0; k < TR_NRA && tr_valid((uintptr_t)fp); k++) {
        ra[k] = fp[1];
        uintptr_t* nx = (uintptr_t*)fp[0];
        if (nx <= fp) break;
        fp = nx;
    }
    uintptr_t h = (uintptr_t)file * 31u + (uintptr_t)line * 0x9e3779b1u + (uintptr_t)type * 17u + (uintptr_t)a * 0x85ebca6bu + ra[0] * 7u + ra[1] * 13u + ra[2] * 3u + ra[3] * 5u + ra[4] * 11u;
    h ^= h >> 15; h *= 0x2c1b3c6du; h ^= h >> 12;
    for (uint32_t i = (uint32_t)h & (TR_CAP - 1), n = 0; n < TR_CAP; i = (i + 1) & (TR_CAP - 1), n++) {
        tr_slot* s = &g_tr[i];
        uint32_t st = atomic_load_explicit(&s->state, memory_order_acquire);
        if (st == 0) {
            uint32_t z = 0;
            if (atomic_compare_exchange_strong(&s->state, &z, 1)) {
                s->file = file; s->line = line; s->type = type; s->target = a; for (int k = 0; k < TR_NRA; k++) s->ra[k] = ra[k];
                atomic_store_explicit(&s->hits, 1, memory_order_relaxed);
                atomic_store_explicit(&s->state, 2, memory_order_release);
                atomic_fetch_add(&g_tr_n, 1);
                return fn;
            }
            st = atomic_load_explicit(&s->state, memory_order_acquire);
        }
        while (st == 1) st = atomic_load_explicit(&s->state, memory_order_acquire);
        if (s->file == file && s->line == line && s->type == type && s->target == a && !memcmp(s->ra, ra, sizeof ra)) {
            atomic_fetch_add_explicit(&s->hits, 1, memory_order_relaxed);
            return fn;
        }
    }
    return fn;      /* full: further new records are lost (TR_CAP is far above the number a run produces) */
}

static void tr_addr(FILE* f, uintptr_t p) {
    Dl_info di;
    if (p && dladdr((void*)p, &di) && di.dli_fname) {
        const char* b = strrchr(di.dli_fname, '/');
        fprintf(f, "%s+0x%lx", b ? b + 1 : di.dli_fname, (unsigned long)(p - (uintptr_t)di.dli_fbase));
    } else fprintf(f, "-");
}
void rx_fn_trace_dump(const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) return;
    for (uint32_t i = 0; i < TR_CAP; i++) {
        tr_slot* s = &g_tr[i];
        if (atomic_load_explicit(&s->state, memory_order_acquire) != 2) continue;
        fprintf(f, "%s:%d\t%s\t%08x\t", s->file, s->line, s->type ? s->type : "-", s->target);
        for (int k = 0; k < TR_NRA; k++) { if (k) fputc(',', f); tr_addr(f, s->ra[k]); }
        fprintf(f, "\t%u\n", atomic_load(&s->hits));
    }
    fclose(f);
}
static void tr_dump_env(void) { const char* p = getenv("RX_FN_TRACE_OUT"); if (p && *p) rx_fn_trace_dump(p); }
static void tr_on_signal(int sig) {
    tr_dump_env();
    if (sig == SIGUSR1) return;
    signal(sig, SIG_DFL);
    raise(sig);
}
__attribute__((constructor)) static void tr_init(void) {
    if (!getenv("RX_FN_TRACE_OUT")) return;
    atexit(tr_dump_env);
    signal(SIGUSR1, tr_on_signal);
    signal(SIGSEGV, tr_on_signal); signal(SIGBUS, tr_on_signal); signal(SIGABRT, tr_on_signal); signal(SIGTERM, tr_on_signal);
}
#endif
