/* corebench.c -- a headless timing benchmark of the OPEN REFLEX core (w-winbench; not shipped with the game).
 *
 *   corebench <game files dir or its Archive dir> <track> [frames] [results file]
 *     track: concord (National NAT_Concord, 12 riders) or renne (National NAT_Renne, 12 riders); AI difficulty 95, 3 laps
 *     frames: race frames after the load (default 3600 = 60 s of game time)
 *
 * It loads the event, then runs `frames` race frames with a fixed pad script (throttle held, a short steer every 4 s, see pad())
 * and, per 300 frames, prints the core's cost per frame: the main thread's CPU time and the wall time of rx_session_advance, plus
 * the page faults, and a hash of every rider's bike pose and the game's RNG draw count. The hash is the same on every machine and
 * build when the simulation is bit-identical (a Mac run and a Windows run of the same track and frames print the same hashes).
 *
 * Windows: an in-process sampling profiler. A second thread suspends the main thread every ~1 ms (SuspendThread /
 * GetThreadContext), walks its stack with the modules' own x64 unwind tables (.pdata, RtlVirtualUnwind; no frame pointers needed),
 * and resumes it. At the end it prints the top functions by self and by inclusive samples, symbolised from rxcore.syms /
 * corebench.syms (written next to the exe by the build) and the other modules' export tables. It also measures a few Windows
 * costs on this machine (the arena's first-touch page commit through a vectored exception handler, malloc / free, emulated
 * thread-locals), counts the arena's first-touch faults during the race, and records which kind of core (performance /
 * efficiency) the main thread ran on. Nothing needs installing or admin rights; results go to the file named on the command
 * line (default: corebench_<track>.txt next to the exe) as well as the console.
 *
 * Mac: the same race and timing (thread CPU time, minor faults); no built-in sampler (use `sample`). Build:
 *   clang -O2 -Icore/include core/tools/corebench.c -L<build> -lrxcore -o corebench
 */
#define _GNU_SOURCE
#include "rx_api.h"
#include "rx_rng.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <intrin.h>
#else
#include <dlfcn.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#endif

#ifndef BENCH_VARIANT
#define BENCH_VARIANT "unnamed"
#endif

static FILE* g_out;
static void say(const char* fmt, ...) {
    va_list a;
    va_start(a, fmt); vprintf(fmt, a); va_end(a);
    fflush(stdout);
    if (g_out) { va_start(a, fmt); vfprintf(g_out, fmt, a); va_end(a); fflush(g_out); }
}

/* ---- clocks -------------------------------------------------------------------------------------------------------------- */
#ifdef _WIN32
static HANDLE g_main_thread;
static double qpc_ms(void) { LARGE_INTEGER f, c; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&c); return (double)c.QuadPart * 1000.0 / (double)f.QuadPart; }
static double thread_cpu_ms(void) {   /* kernel + user time of the main thread (100 ns units, updated at the scheduler tick) */
    FILETIME c, e, k, u; GetThreadTimes(g_main_thread, &c, &e, &k, &u);
    const uint64_t kk = ((uint64_t)k.dwHighDateTime << 32) | k.dwLowDateTime, uu = ((uint64_t)u.dwHighDateTime << 32) | u.dwLowDateTime;
    return (double)(kk + uu) / 1e4;
}
static uint64_t thread_cycles(void) { ULONG64 c = 0; QueryThreadCycleTime(g_main_thread, &c); return c; }
static uint64_t page_faults(void) { PROCESS_MEMORY_COUNTERS m; memset(&m, 0, sizeof m); m.cb = sizeof m; K32GetProcessMemoryInfo(GetCurrentProcess(), &m, sizeof m); return m.PageFaultCount; }
#else
static double qpc_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e3 + t.tv_nsec / 1e6; }
static double thread_cpu_ms(void) { struct timespec t; clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t); return t.tv_sec * 1e3 + t.tv_nsec / 1e6; }
static uint64_t thread_cycles(void) { return 0; }
static uint64_t page_faults(void) { struct rusage r; getrusage(RUSAGE_SELF, &r); return (uint64_t)r.ru_minflt + (uint64_t)r.ru_majflt; }
#endif

/* ---- Windows: arena fault counter, core kind, sampler ---------------------------------------------------------------------- */
#ifdef _WIN32
/* Counts first-chance access violations (the core's arena commits its pages on first touch from a vectored handler, rx_mem.c).
 * Installed first in the chain; it only counts and passes the exception on. */
static volatile LONG g_av_count, g_exc_count;
static volatile LONG g_exc_codes[16][2];   /* other exception codes seen during the race: code, count */
static LONG CALLBACK count_av(EXCEPTION_POINTERS* e) {
    const DWORD code = e->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_ACCESS_VIOLATION) InterlockedIncrement(&g_av_count);
    else {
        InterlockedIncrement(&g_exc_count);
        for (int i = 0; i < 16; i++) {
            if (g_exc_codes[i][0] == (LONG)code) { InterlockedIncrement(&g_exc_codes[i][1]); break; }
            if (InterlockedCompareExchange(&g_exc_codes[i][0], (LONG)code, 0) == 0) { InterlockedIncrement(&g_exc_codes[i][1]); break; }
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

/* Processor -> efficiency class (Windows 10+: GetSystemCpuSetInformation; 0 = the most efficient kind, higher = faster cores). */
static unsigned char g_eff[64][64]; static int g_eff_known, g_eff_max;
static void cpu_sets_load(void) {
    typedef BOOL (WINAPI *get_t)(PSYSTEM_CPU_SET_INFORMATION, ULONG, PULONG, HANDLE, ULONG);
    get_t get = (get_t)(void*)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetSystemCpuSetInformation");
    if (!get) return;
    ULONG len = 0; get(NULL, 0, &len, GetCurrentProcess(), 0);
    if (!len) return;
    uint8_t* buf = (uint8_t*)malloc(len);
    if (!buf || !get((PSYSTEM_CPU_SET_INFORMATION)buf, len, &len, GetCurrentProcess(), 0)) { free(buf); return; }
    for (ULONG o = 0; o < len;) {
        SYSTEM_CPU_SET_INFORMATION* i = (SYSTEM_CPU_SET_INFORMATION*)(buf + o);
        if (i->Size == 0) break;
        if (i->Type == CpuSetInformation && i->CpuSet.Group < 64 && i->CpuSet.LogicalProcessorIndex < 64) {
            g_eff[i->CpuSet.Group][i->CpuSet.LogicalProcessorIndex] = i->CpuSet.EfficiencyClass;
            if (i->CpuSet.EfficiencyClass > g_eff_max) g_eff_max = i->CpuSet.EfficiencyClass;
            g_eff_known = 1;
        }
        o += i->Size;
    }
    free(buf);
}
static int current_eff_class(void) {
    PROCESSOR_NUMBER p; GetCurrentProcessorNumberEx(&p);
    return (g_eff_known && p.Group < 64 && p.Number < 64) ? g_eff[p.Group][p.Number] : -1;
}

/* modules: base, size, .pdata, exports (for symbols outside our own two images) */
typedef struct { uintptr_t base, end; const RUNTIME_FUNCTION* pdata; DWORD npdata; char name[64]; } module_t;
static module_t g_mod[256]; static int g_nmod;
static void modules_load(void) {
    HMODULE hm[256]; DWORD need = 0;
    if (!K32EnumProcessModules(GetCurrentProcess(), hm, sizeof hm, &need)) return;
    const int n = (int)(need / sizeof(HMODULE)) < 256 ? (int)(need / sizeof(HMODULE)) : 256;
    for (int i = 0; i < n; i++) {
        MODULEINFO mi; if (!K32GetModuleInformation(GetCurrentProcess(), hm[i], &mi, sizeof mi)) continue;
        module_t* m = &g_mod[g_nmod++];
        m->base = (uintptr_t)mi.lpBaseOfDll; m->end = m->base + mi.SizeOfImage;
        char path[MAX_PATH]; K32GetModuleBaseNameA(GetCurrentProcess(), hm[i], path, sizeof path);
        snprintf(m->name, sizeof m->name, "%s", path);
        const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)m->base;
        const IMAGE_NT_HEADERS64* nt = (const IMAGE_NT_HEADERS64*)(m->base + dos->e_lfanew);
        const IMAGE_DATA_DIRECTORY* d = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
        m->pdata = d->Size ? (const RUNTIME_FUNCTION*)(m->base + d->VirtualAddress) : NULL;
        m->npdata = d->Size / sizeof(RUNTIME_FUNCTION);
    }
}
static const module_t* module_of(uintptr_t pc) { for (int i = 0; i < g_nmod; i++) if (pc >= g_mod[i].base && pc < g_mod[i].end) return &g_mod[i]; return NULL; }
static RUNTIME_FUNCTION* pdata_find(const module_t* m, uintptr_t pc) {   /* lock-free: our own binary search, no loader calls */
    if (!m || !m->pdata) return NULL;
    const DWORD rva = (DWORD)(pc - m->base);
    DWORD lo = 0, hi = m->npdata;
    while (lo < hi) {
        const DWORD mid = (lo + hi) / 2; const RUNTIME_FUNCTION* f = &m->pdata[mid];
        if (rva < f->BeginAddress) hi = mid; else if (rva >= f->EndAddress) lo = mid + 1; else return (RUNTIME_FUNCTION*)f;
    }
    return NULL;
}

#define MAX_DEPTH 48
#define MAX_SAMPLES 400000
static uintptr_t* g_samples;   /* per sample: depth then pcs (outermost last) */
static size_t g_sample_words, g_sample_cap; static unsigned g_nsamples, g_dropped;
static volatile LONG g_sampling, g_sampler_stop;
static uintptr_t g_stack_lo, g_stack_hi;

/* watchdog: if one sample keeps the main thread stopped for more than ~250 ms (the sampler blocked on a lock the stopped thread
 * holds, as seen under Wine), resume the main thread so it can release it; counted and reported */
static volatile LONG g_in_sample, g_watchdog_resumes; static volatile LONG64 g_sample_t0;
static DWORD WINAPI watchdog(LPVOID unused) {
    (void)unused;
    while (!g_sampler_stop) {
        Sleep(50);
        if (g_in_sample && (LONG64)GetTickCount64() - g_sample_t0 > 250) { ResumeThread(g_main_thread); InterlockedIncrement(&g_watchdog_resumes); g_sample_t0 = (LONG64)GetTickCount64(); }
    }
    return 0;
}
static int walk(CONTEXT* c, uintptr_t* pcs) {
    int n = 0;
    while (n < MAX_DEPTH) {
        const uintptr_t pc = (uintptr_t)c->Rip;
        if (!pc) break;
        pcs[n++] = pc;
        const module_t* m = module_of(pc);
        if (!m) break;
        RUNTIME_FUNCTION* f = pdata_find(m, pc);
        if (f) {
            void* hd; DWORD64 ef;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, (DWORD64)m->base, (DWORD64)pc, f, c, &hd, &ef, NULL);
        } else {   /* a leaf function: the return address is on top of the stack */
            if ((uintptr_t)c->Rsp < g_stack_lo || (uintptr_t)c->Rsp + 8 > g_stack_hi) break;
            c->Rip = *(DWORD64*)(uintptr_t)c->Rsp; c->Rsp += 8;
        }
        if ((uintptr_t)c->Rsp < g_stack_lo || (uintptr_t)c->Rsp >= g_stack_hi) break;
    }
    return n;
}
static DWORD WINAPI sampler(LPVOID unused) {
    (void)unused;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    uintptr_t pcs[MAX_DEPTH];
    const DWORD period = getenv("COREBENCH_SAMPLE_MS") ? (DWORD)atoi(getenv("COREBENCH_SAMPLE_MS")) : 1;   /* Wine: suspending is slow, use ~20 */
    while (!g_sampler_stop) {
        Sleep(period);
        if (!g_sampling) continue;
        g_sample_t0 = (LONG64)GetTickCount64(); g_in_sample = 1;
        if (SuspendThread(g_main_thread) == (DWORD)-1) { g_in_sample = 0; continue; }
        CONTEXT c; memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
        int n = 0;
        if (GetThreadContext(g_main_thread, &c)) n = walk(&c, pcs);   /* no allocation or lock while the thread is stopped */
        ResumeThread(g_main_thread);
        g_in_sample = 0;
        if (n == 0) continue;
        if (g_sample_words + (size_t)n + 1 > g_sample_cap) { g_dropped++; continue; }
        g_samples[g_sample_words++] = (uintptr_t)n;
        for (int i = 0; i < n; i++) g_samples[g_sample_words++] = pcs[i];
        g_nsamples++;
    }
    return 0;
}

/* symbols: "<rva hex> <name>" lines, sorted (rxcore.syms / corebench.syms next to the exe) */
typedef struct { uint32_t rva; char* name; } sym_t;
typedef struct { const module_t* mod; sym_t* s; int n; } symtab_t;
static symtab_t g_tabs[4]; static int g_ntabs;
static int sym_cmp(const void* a, const void* b) { const sym_t* x = a; const sym_t* y = b; return x->rva < y->rva ? -1 : x->rva > y->rva; }
static void syms_load(const char* dir, const char* file, const char* module) {
    const module_t* m = NULL;
    for (int i = 0; i < g_nmod; i++) if (_strnicmp(g_mod[i].name, module, strlen(module)) == 0) m = &g_mod[i];   /* rxcore.dll (its export name may be rxcore.dll.tmp) */
    char path[MAX_PATH * 2]; snprintf(path, sizeof path, "%s\\%s", dir, file);
    FILE* f = fopen(path, "r");
    if (!f || !m) { if (f) fclose(f); say("symbols: %s not loaded (%s)\n", file, m ? "file missing" : "module missing"); return; }
    symtab_t* t = &g_tabs[g_ntabs++]; t->mod = m; int cap = 1 << 16; t->s = malloc(sizeof(sym_t) * (size_t)cap);
    char line[4096];
    while (fgets(line, sizeof line, f)) {
        unsigned long long rva; char name[4000];
        if (sscanf(line, "%llx %3999s", &rva, name) != 2) continue;
        if (t->n == cap) { cap *= 2; t->s = realloc(t->s, sizeof(sym_t) * (size_t)cap); }
        t->s[t->n].rva = (uint32_t)rva; t->s[t->n].name = _strdup(name); t->n++;
    }
    fclose(f);
    qsort(t->s, (size_t)t->n, sizeof(sym_t), sym_cmp);
    say("symbols: %s %d names for %s\n", file, t->n, module);
}
static const char* export_name(const module_t* m, uintptr_t pc, char* buf, size_t cap) {   /* nearest export at or below pc */
    const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)m->base;
    const IMAGE_NT_HEADERS64* nt = (const IMAGE_NT_HEADERS64*)(m->base + dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY* d = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    const DWORD rva = (DWORD)(pc - m->base);
    const char* best = NULL; DWORD best_rva = 0;
    if (d->Size) {
        const IMAGE_EXPORT_DIRECTORY* ed = (const IMAGE_EXPORT_DIRECTORY*)(m->base + d->VirtualAddress);
        const DWORD* fn = (const DWORD*)(m->base + ed->AddressOfFunctions);
        const DWORD* nm = (const DWORD*)(m->base + ed->AddressOfNames);
        const WORD* ord = (const WORD*)(m->base + ed->AddressOfNameOrdinals);
        for (DWORD i = 0; i < ed->NumberOfNames; i++) {
            const DWORD f = fn[ord[i]];
            if (f <= rva && f >= best_rva && !(f >= d->VirtualAddress && f < d->VirtualAddress + d->Size)) { best_rva = f; best = (const char*)(m->base + nm[i]); }
        }
    }
    if (best) snprintf(buf, cap, "%s!%s", m->name, best); else snprintf(buf, cap, "%s!?", m->name);
    return buf;
}
static const char* symbolise(uintptr_t pc, char* buf, size_t cap) {
    const module_t* m = module_of(pc);
    if (!m) { snprintf(buf, cap, "?%llx", (unsigned long long)pc); return buf; }
    for (int i = 0; i < g_ntabs; i++) if (g_tabs[i].mod == m) {
        const symtab_t* t = &g_tabs[i]; const uint32_t rva = (uint32_t)(pc - m->base);
        int lo = 0, hi = t->n;
        while (lo < hi) { const int mid = (lo + hi) / 2; if (t->s[mid].rva <= rva) lo = mid + 1; else hi = mid; }
        if (lo > 0) { snprintf(buf, cap, "%s", t->s[lo - 1].name); return buf; }
    }
    return export_name(m, pc, buf, cap);
}

/* the profile report: names interned, self and inclusive (once per sample) counts */
typedef struct { char* name; unsigned self, incl, last; } fn_t;
static fn_t* g_fn; static int g_nfn, g_fncap;
static int g_fnhash[1 << 16];   /* name hash -> index + 1 (open addressing) */
static int fn_index(const char* name) {
    uint32_t h = 2166136261u; for (const char* p = name; *p; p++) { h ^= (uint8_t)*p; h *= 16777619u; }
    size_t k = h & 0xffff;
    while (g_fnhash[k]) { if (strcmp(g_fn[g_fnhash[k] - 1].name, name) == 0) return g_fnhash[k] - 1; k = (k + 1) & 0xffff; }
    if (g_nfn >= 60000) return 0;
    if (g_nfn == g_fncap) { g_fncap = g_fncap ? g_fncap * 2 : 1024; g_fn = realloc(g_fn, sizeof(fn_t) * (size_t)g_fncap); }
    g_fn[g_nfn].name = _strdup(name); g_fn[g_nfn].self = g_fn[g_nfn].incl = 0; g_fn[g_nfn].last = 0;
    g_fnhash[k] = g_nfn + 1;
    return g_nfn++;
}
typedef struct { uintptr_t pc; int fn; } pcmap_t;
static int by_self(const void* a, const void* b) { const fn_t* x = a; const fn_t* y = b; return (int)y->self - (int)x->self; }
static int by_incl(const void* a, const void* b) { const fn_t* x = a; const fn_t* y = b; return (int)y->incl - (int)x->incl; }
static void profile_report(void) {
    say("\nPROFILE %u samples of the main thread during the race (%u dropped)\n", g_nsamples, g_dropped);
    if (!g_nsamples) return;
    /* pc -> function cache (open addressing) */
    const size_t cap = 1u << 20; pcmap_t* map = calloc(cap, sizeof(pcmap_t));
    char buf[4096];
    unsigned stamp = 0;
    for (size_t w = 0; w < g_sample_words;) {
        const int n = (int)g_samples[w++]; stamp++;
        for (int i = 0; i < n; i++) {
            const uintptr_t pc = g_samples[w + (size_t)i];
            size_t h = (size_t)((pc * 0x9E3779B97F4A7C15ull) >> 44) & (cap - 1);
            while (map[h].pc && map[h].pc != pc) h = (h + 1) & (cap - 1);
            if (!map[h].pc) { map[h].pc = pc; map[h].fn = fn_index(symbolise(i ? pc - 1 : pc, buf, sizeof buf)); }
            fn_t* f = &g_fn[map[h].fn];
            if (i == 0) f->self++;
            if (f->last != stamp) { f->incl++; f->last = stamp; }
        }
        w += (size_t)n;
    }
    /* callers of the top self functions: (leaf function, its caller) pairs */
    typedef struct { int leaf, caller; unsigned n; } edge_t;
    edge_t* edges = calloc(1u << 16, sizeof(edge_t));
    for (size_t w = 0; w < g_sample_words;) {
        const int n = (int)g_samples[w++];
        if (n >= 2) {
            int fns[2];
            for (int i = 0; i < 2; i++) { const uintptr_t pc = g_samples[w + (size_t)i]; size_t h = (size_t)((pc * 0x9E3779B97F4A7C15ull) >> 44) & (cap - 1); while (map[h].pc != pc) h = (h + 1) & (cap - 1); fns[i] = map[h].fn; }
            size_t h = ((size_t)fns[0] * 31 + (size_t)fns[1]) & 0xffff;
            while (edges[h].n && (edges[h].leaf != fns[0] || edges[h].caller != fns[1])) h = (h + 1) & 0xffff;
            edges[h].leaf = fns[0]; edges[h].caller = fns[1]; edges[h].n++;
        }
        w += (size_t)n;
    }
    free(map);
    const double tot = g_nsamples;
    {   /* top self functions (by index before sorting) with their 4 biggest callers */
        int order[20]; int no = 0;
        for (int k = 0; k < 20; k++) { int best = -1; for (int i = 0; i < g_nfn; i++) { int used = 0; for (int j = 0; j < no; j++) used |= order[j] == i; if (!used && (best < 0 || g_fn[i].self > g_fn[best].self)) best = i; } if (best < 0) break; order[no++] = best; }
        say("CALLERS of the top self functions\n");
        for (int k = 0; k < no; k++) {
            say("  %s (%u self)\n", g_fn[order[k]].name, g_fn[order[k]].self);
            for (int r = 0; r < 4; r++) { int bi = -1; for (int i = 0; i < 65536; i++) if (edges[i].n && edges[i].leaf == order[k] && (bi < 0 || edges[i].n > edges[bi].n)) bi = i;
                if (bi < 0) break;
                say("      %6u  <- %s\n", edges[bi].n, g_fn[edges[bi].caller].name); edges[bi].n = 0; }
        }
    }
    free(edges);
    qsort(g_fn, (size_t)g_nfn, sizeof(fn_t), by_self);
    say("TOP SELF (samples, %% of all)\n");
    for (int i = 0; i < g_nfn && i < 60; i++) say("  %7u %5.1f%%  %s\n", g_fn[i].self, 100.0 * g_fn[i].self / tot, g_fn[i].name);
    qsort(g_fn, (size_t)g_nfn, sizeof(fn_t), by_incl);
    say("TOP INCLUSIVE (samples, %% of all)\n");
    for (int i = 0; i < g_nfn && i < 80; i++) say("  %7u %5.1f%%  %s\n", g_fn[i].incl, 100.0 * g_fn[i].incl / tot, g_fn[i].name);
}

/* micro-measurements of Windows-only costs on this machine */
static void* g_veh_base; static volatile LONG g_veh_hits;
static LONG CALLBACK commit_on_touch(EXCEPTION_POINTERS* e) {   /* the same steps as the core's arena_page_fault (rx_mem.c) */
    if (e->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || e->ExceptionRecord->NumberParameters < 2) return EXCEPTION_CONTINUE_SEARCH;
    const uintptr_t a = (uintptr_t)e->ExceptionRecord->ExceptionInformation[1], b = (uintptr_t)g_veh_base;
    if (a < b || a >= b + (64u << 20)) return EXCEPTION_CONTINUE_SEARCH;
    MEMORY_BASIC_INFORMATION r;
    if (!VirtualQuery((void*)a, &r, sizeof r) || r.State != MEM_RESERVE) return EXCEPTION_CONTINUE_SEARCH;
    g_veh_hits++;
    return VirtualAlloc((void*)(a & ~(uintptr_t)4095), 4096, MEM_COMMIT, PAGE_READWRITE) ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH;
}
static _Thread_local volatile int g_tls_probe;
static __attribute__((noinline)) void tls_bump(void) { g_tls_probe = g_tls_probe + 1; }   /* one address lookup per call, as in the core's accessors */
static void micro(void) {
    const int pages = 4096;
    /* 1. first touch through the vectored handler (the core's arena) vs a committed region (the kernel's demand-zero fault) */
    g_veh_base = VirtualAlloc(NULL, 64u << 20, MEM_RESERVE, PAGE_NOACCESS);
    void* h = AddVectoredExceptionHandler(1, commit_on_touch);
    double t0 = qpc_ms();
    for (int i = 0; i < pages; i++) ((volatile char*)g_veh_base)[(size_t)i * 4096] = 1;
    const double veh = (qpc_ms() - t0) * 1000.0 / pages;
    RemoveVectoredExceptionHandler(h); VirtualFree(g_veh_base, 0, MEM_RELEASE);
    char* c = VirtualAlloc(NULL, (size_t)pages * 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    t0 = qpc_ms();
    for (int i = 0; i < pages; i++) ((volatile char*)c)[(size_t)i * 4096] = 1;
    const double dz = (qpc_ms() - t0) * 1000.0 / pages;
    VirtualFree(c, 0, MEM_RELEASE);
    /* 2. malloc / free of small blocks (the CRT's heap) */
    void* ptrs[64]; const int rounds = 20000;
    t0 = qpc_ms();
    for (int r = 0; r < rounds; r++) { for (int i = 0; i < 64; i++) ptrs[i] = malloc(16 + (size_t)((i * 37 + r) & 127)); for (int i = 0; i < 64; i++) free(ptrs[i]); }
    const double mf = (qpc_ms() - t0) * 1e6 / (rounds * 64.0);
    /* 3. an emulated thread-local (this exe is built by the same GCC: __emutls_get_address on every access) */
    const int n = 10000000; t0 = qpc_ms();
    for (int i = 0; i < n; i++) tls_bump();
    const double tl = (qpc_ms() - t0) * 1e6 / n;
    say("MICRO first-touch page via vectored handler %.2f us/page (%ld handled), via the kernel (committed) %.2f us/page; malloc+free %.1f ns/pair; emulated thread-local read+write %.1f ns\n",
        veh, (long)g_veh_hits, dz, mf, tl);
}
#endif

#ifndef _WIN32
/* Mac: the core's 4 GiB arena (the one 4 GiB mapping): resident 16 KiB pages and touched 64 KiB / 1 MiB granules (the Windows
 * build commits on first touch; this says how many commits a granule size needs). */
static void arena_report(void) {
    mach_vm_address_t a = 0; mach_vm_size_t sz = 0; natural_t depth = 0;
    for (;;) {
        vm_region_submap_info_data_64_t info; mach_msg_type_number_t cnt = VM_REGION_SUBMAP_INFO_COUNT_64;
        if (mach_vm_region_recurse(mach_task_self(), &a, &sz, &depth, (vm_region_recurse_info_t)&info, &cnt) != KERN_SUCCESS) return;
        if (info.is_submap) { depth++; continue; }
        if (sz >= (1ull << 31) && info.protection == (VM_PROT_READ | VM_PROT_WRITE) && info.share_mode != SM_EMPTY) break;   /* the arena: the 4 GiB read-write anonymous mapping */
        if (sz >= (1ull << 30)) say("ARENA candidate %llx size %llx prot %d share %d\n", (unsigned long long)a, (unsigned long long)sz, info.protection, info.share_mode);
        a += sz;
    }
    sz = 1ull << 32;
    const size_t pg = 16384, n = (size_t)(sz / pg);
    unsigned char* v = malloc(n);
    if (!v || mincore((void*)a, (size_t)sz, (char*)v) != 0) { free(v); return; }
    size_t res = 0, g64 = 0, g256 = 0, g1m = 0;
    for (size_t i = 0; i < n; i++) if (v[i] & 1) res++;
    for (size_t i = 0; i < n; i += 4) { int t = 0; for (size_t j = i; j < i + 4 && j < n; j++) t |= v[j] & 1; g64 += t; }
    for (size_t i = 0; i < n; i += 16) { int t = 0; for (size_t j = i; j < i + 16 && j < n; j++) t |= v[j] & 1; g256 += t; }
    for (size_t i = 0; i < n; i += 64) { int t = 0; for (size_t j = i; j < i + 64 && j < n; j++) t |= v[j] & 1; g1m += t; }
    say("ARENA resident %zu x 16 KiB = %.1f MiB; touched granules: %zu x 64 KiB (%.1f MiB), %zu x 256 KiB (%.1f MiB), %zu x 1 MiB\n",
        res, res * 16.0 / 1024, g64, g64 / 16.0, g256, g256 / 4.0, g1m);
    free(v);
}
#endif

/* ---- the race ----------------------------------------------------------------------------------------------------------- */
static uint64_t fnv(uint64_t h, const void* p, size_t n) { const uint8_t* b = p; for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 0x100000001b3ull; } return h; }

static void pad(uint8_t keys[256], const rx_remap_table* r, int frame) {   /* the fixed pad script */
    memset(keys, 0, 256);
    keys[r->keyboard[0] & 255] = 0x80;                                     /* throttle */
    const int phase = frame % 240;
    if (phase >= 120 && phase < 150) keys[r->keyboard[2] & 255] = 0x80;    /* steer left */
    if (phase >= 200 && phase < 220) keys[r->keyboard[3] & 255] = 0x80;    /* steer right */
}

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: corebench <game files dir> <concord|renne> [frames] [results file]\n"); return 2; }
    const char* track = argv[2];
    const int frames = argc > 3 ? atoi(argv[3]) : 3600;
    char root[4096]; snprintf(root, sizeof root, "%s", argv[1]);
    { size_t L = strlen(root); while (L && (root[L - 1] == '/' || root[L - 1] == '\\')) root[--L] = 0; }
    /* accept the "game files" directory itself or its Archive directory */
    { char probe[4200]; snprintf(probe, sizeof probe, "%s/Archive", root); FILE* f = NULL;
      char probe2[4300]; snprintf(probe2, sizeof probe2, "%s/Archive/Common", root);
#ifdef _WIN32
      DWORD a = GetFileAttributesA(probe); if (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY)) snprintf(root, sizeof root, "%s", probe);
#else
      f = fopen(probe, "r"); if (f) { fclose(f); snprintf(root, sizeof root, "%s", probe); }
#endif
      (void)f; (void)probe2; }
    const char* loc; int loc_id;
    if (strcmp(track, "concord") == 0) { loc = "NAT_Concord"; loc_id = 50; }
    else if (strcmp(track, "renne") == 0) { loc = "NAT_Renne"; loc_id = 54; }
    else { fprintf(stderr, "track must be concord or renne\n"); return 2; }

    char outpath[4096];
#ifdef _WIN32
    char exedir[MAX_PATH]; GetModuleFileNameA(NULL, exedir, sizeof exedir);
    { char* sl = strrchr(exedir, '\\'); if (sl) *sl = 0; }
    if (argc > 4) snprintf(outpath, sizeof outpath, "%s", argv[4]); else snprintf(outpath, sizeof outpath, "%s\\corebench_%s.txt", exedir, track);
#else
    if (argc > 4) snprintf(outpath, sizeof outpath, "%s", argv[4]); else snprintf(outpath, sizeof outpath, "corebench_%s.txt", track);
#endif
    g_out = fopen(outpath, "w");
    say("corebench variant %s, track %s, %d race frames, install %s\n", BENCH_VARIANT, loc, frames, root);
#ifdef _WIN32
    { int r[4]; char brand[49]; memset(brand, 0, sizeof brand);
      __cpuid(r, (int)0x80000000); if ((unsigned)r[0] >= 0x80000004u) for (int i = 0; i < 3; i++) { __cpuid(r, (int)(0x80000002u + (unsigned)i)); memcpy(brand + 16 * i, r, 16); }
      OSVERSIONINFOW v; memset(&v, 0, sizeof v); v.dwOSVersionInfoSize = sizeof v;
      typedef LONG (WINAPI *rgv_t)(OSVERSIONINFOW*); rgv_t rgv = (rgv_t)(void*)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetVersion"); if (rgv) rgv(&v);
      SYSTEM_INFO si; GetSystemInfo(&si);
      say("cpu %s, %lu logical processors, Windows %lu.%lu.%lu\n", brand, (unsigned long)si.dwNumberOfProcessors, (unsigned long)v.dwMajorVersion, (unsigned long)v.dwMinorVersion, (unsigned long)v.dwBuildNumber); }
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_main_thread, 0, FALSE, DUPLICATE_SAME_ACCESS);
    { ULONG_PTR lo, hi; GetCurrentThreadStackLimits(&lo, &hi); g_stack_lo = lo; g_stack_hi = hi; }
    cpu_sets_load();
    micro();
#else
    { struct utsname u; uname(&u); say("host %s %s %s\n", u.sysname, u.release, u.machine); }
#endif

    double t0 = qpc_ms();
    struct rx_session* s = rx_session_create(root);
    if (!s) { say("RESULT create failed: %s\n", rx_session_last_error(NULL)); return 1; }
    rx_event_spec e; rx_session_event_default(&e);
    snprintf(e.event_type, sizeof e.event_type, "National"); e.event_type_id = 10;
    snprintf(e.location, sizeof e.location, "%s", loc); e.location_id = loc_id;
    e.track_id = 0; e.vehicle_class = 2; e.duration = 3; snprintf(e.duration_type, sizeof e.duration_type, "Laps");
    e.riders = 12; e.ai_difficulty = 95; e.event_activation[0] = 0;
    if (rx_session_event_load(s, &e) != 0) { say("RESULT event load failed: %s\n", rx_session_last_error(s)); return 1; }
    int loadf = 0;
    while (rx_session_get_state(s) == RX_SESSION_LOADING && loadf < 3000) { if (rx_session_advance(s) != 0) break; loadf++; }
    say("loaded in %d frames, %.1f s\n", loadf, (qpc_ms() - t0) / 1000.0);
    if (rx_session_get_state(s) == RX_SESSION_ERROR) { say("RESULT load error: %s\n", rx_session_last_error(s)); return 1; }

    rx_remap_table remap; memset(&remap, 0, sizeof remap); rx_remap_defaults(&remap);
    uint8_t keys[256];
    static float pose[50 * 16];
    uint64_t hash = 0xcbf29ce484222325ull;
#ifdef _WIN32
    g_sample_cap = (size_t)MAX_SAMPLES * 8; g_samples = (uintptr_t*)malloc(g_sample_cap * sizeof(uintptr_t));
    modules_load();
    syms_load(exedir, "rxcore.syms", "rxcore.dll");
    syms_load(exedir, "corebench.syms", "corebench.exe");
    void* avh = AddVectoredExceptionHandler(1, count_av);
    const int sample_on = !getenv("COREBENCH_SAMPLE") || atoi(getenv("COREBENCH_SAMPLE")) != 0;   /* COREBENCH_SAMPLE=0: timing only */
    HANDLE sth = sample_on ? CreateThread(NULL, 0, sampler, NULL, 0, NULL) : NULL;
    HANDLE wdh = sample_on ? CreateThread(NULL, 0, watchdog, NULL, 0, NULL) : NULL;
    unsigned eff_frames[16]; memset(eff_frames, 0, sizeof eff_frames);
    if (g_eff_known) say("core kinds: efficiency classes 0..%d (higher = faster cores)\n", g_eff_max);
#endif
    say("%-6s %9s %9s %10s %9s %8s %18s %s\n", "frames", "cpu ms/f", "wall ms/f", "cycles/f", "faults/f", "AV/f", "hash", "kinds");
    double sum_cpu = 0, sum_wall = 0; int counted = 0;
    double c0 = thread_cpu_ms(), w0 = qpc_ms(); uint64_t y0 = thread_cycles(), f0 = page_faults();
#ifdef _WIN32
    LONG a0 = g_av_count; g_sampling = 1;
#endif
    double wall_acc = 0;
    int f;
    for (f = 1; f <= frames; f++) {
        pad(keys, &remap, f);
        rx_session_set_input(s, keys, NULL, 0);
        const double ta = qpc_ms();
        const int rc = rx_session_advance(s);
        wall_acc += qpc_ms() - ta;
#ifdef _WIN32
        { const int k = current_eff_class(); if (k >= 0 && k < 16) eff_frames[k]++; }
        g_sampling = 0;
#endif
        if (rc != 0 || rx_session_get_state(s) == RX_SESSION_ERROR) { say("RESULT error at race frame %d: %s\n", f, rx_session_last_error(s)); break; }
        { int n = rx_session_bike_pose(s, pose, 50); if (n > 0) hash = fnv(hash, pose, (size_t)n * 16 * sizeof(float));
          for (int sl = 0; sl < 16; sl++) { n = rx_session_bike_pose_slot(s, sl, pose, 50); if (n > 0) hash = fnv(hash, pose, (size_t)n * 16 * sizeof(float)); } }
#ifdef _WIN32
        g_sampling = 1;
#endif
        if (f % 300 == 0) {
            rx_rng_state g; memset(&g, 0, sizeof g); g.size = sizeof g; if (rx_session_rng(s, &g) == 0) hash = fnv(hash, &g.game_count, sizeof g.game_count);
            const double c1 = thread_cpu_ms(); const uint64_t y1 = thread_cycles(), f1 = page_faults();
            const double cpu = (c1 - c0) / 300.0, wall = wall_acc / 300.0;
            char kinds[128] = "";
#ifdef _WIN32
            const LONG a1 = g_av_count;
            { size_t o = 0; for (int k = 0; k <= g_eff_max && k < 16; k++) o += (size_t)snprintf(kinds + o, sizeof kinds - o, "%sclass%d:%u", o ? " " : "", k, eff_frames[k]); memset(eff_frames, 0, sizeof eff_frames); }
            say("%-6d %9.3f %9.3f %10.0f %9.1f %8.1f %018llx %s\n", f, cpu, wall, (double)(y1 - y0) / 300.0, (double)(f1 - f0) / 300.0, (double)(a1 - a0) / 300.0, (unsigned long long)hash, kinds);
            a0 = a1;
#else
            { static unsigned long long* tls_calls; static unsigned long long tls0; static int looked;   /* a measurement build's counting emutls (w-winbench) */
              if (!looked) { looked = 1; tls_calls = (unsigned long long*)dlsym(RTLD_DEFAULT, "rxbench_emutls_calls"); }
              if (tls_calls) { snprintf(kinds, sizeof kinds, "emutls %.0f/frame", (double)(*tls_calls - tls0) / 300.0); tls0 = *tls_calls; }
              static unsigned long long* m_calls; static unsigned long long m0;
              if (looked == 1) { looked = 2; m_calls = (unsigned long long*)dlsym(RTLD_DEFAULT, "rxbench_malloc_calls"); }
              if (m_calls) { const size_t o = strlen(kinds); snprintf(kinds + o, sizeof kinds - o, " allocs %.0f/frame", (double)(*m_calls - m0) / 300.0); m0 = *m_calls; } }
            say("%-6d %9.3f %9.3f %10.0f %9.1f %8s %018llx %s\n", f, cpu, wall, 0.0, (double)(f1 - f0) / 300.0, "-", (unsigned long long)hash, kinds);
#endif
            if (f > 300) { sum_cpu += cpu; sum_wall += wall; counted++; }   /* the first block (the start line) is left out of the mean */
            c0 = thread_cpu_ms(); w0 = qpc_ms(); y0 = thread_cycles(); f0 = page_faults(); wall_acc = 0;
        }
    }
    (void)w0;
#ifdef _WIN32
    g_sampling = 0; g_sampler_stop = 1; if (sth) WaitForSingleObject(sth, 2000); if (wdh) WaitForSingleObject(wdh, 2000);
    if (g_watchdog_resumes) say("SAMPLER watchdog resumed the main thread %ld times (a sample blocked)\n", (long)g_watchdog_resumes);
    RemoveVectoredExceptionHandler(avh);
#endif
#ifdef _WIN32
    if (g_exc_count) { say("EXCEPTIONS other than access violations during the race: %ld (", (long)g_exc_count); for (int i = 0; i < 16 && g_exc_codes[i][0]; i++) say("%s%08lx x%ld", i ? ", " : "", (unsigned long)g_exc_codes[i][0], (long)g_exc_codes[i][1]); say(")\n"); }
#endif
    if (counted) say("MEAN frames 301..%d: cpu %.3f ms/frame, wall %.3f ms/frame; final hash %018llx; game time %.3f\n", f - 1, sum_cpu / counted, sum_wall / counted, (unsigned long long)hash, rx_session_game_time(s));
#ifdef _WIN32
    profile_report();
#else
    arena_report();
#endif
    rx_session_destroy(s);
    say("DONE\n");
    if (g_out) fclose(g_out);
    return 0;
}
