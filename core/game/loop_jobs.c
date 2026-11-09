/* loop_jobs.c -- the worker jobs on host threads (loop_jobs.h). */
#include "loop_jobs.h"
#include "../src/rx_reset.h"
#include <stdlib.h>
#include <string.h>

/* the game thread's guest machine state (rx_mem.h): the stack top 0x7ff00000, the TEB 0x7ffd0000 */
rx_cpu rx_cpu_main = {0x7ff00000u, RX_MAIN_TEB, {0}, {0}, {0}, 0, -1, 0};
static int g_seq_depth;   /* jobs running one after another (nested batches stay so) */
int rx_cpu_par;
/* the host thread's rx_cpu in a parallel batch. Windows: a native TLS slot read straight from the TEB (gs:[0x1480 + 8 slot]; MinGW's
 * _Thread_local is emulated, a call per access); elsewhere _Thread_local. */
#if defined(_WIN32) && !defined(RX_ORACLE)
#include <windows.h>
#include <intrin.h>
static DWORD g_tls_slot RX_RESET_KEEP = TLS_OUT_OF_INDEXES;   /* (kept across session resets, as the pool) */
rx_cpu* rx_cpu_tls_get(void) {
    if (g_tls_slot < 64) return (rx_cpu*)__readgsqword(0x1480 + 8 * g_tls_slot);
    return (rx_cpu*)TlsGetValue(g_tls_slot);
}
static void cpu_tls_set(rx_cpu* c) { TlsSetValue(g_tls_slot, c); }
#else
static _Thread_local rx_cpu* rx_cpu_tls;
rx_cpu* rx_cpu_tls_get(void) { return rx_cpu_tls; }
static void cpu_tls_set(rx_cpu* c) { rx_cpu_tls = c; }
#endif
rx_cpu* rx_cpu_cur_fn(void) { return rx_cpu_cur(); }

#ifdef RX_ORACLE
/* the oracle's workers are the emulator's threads (loop_wake_worker = SetEvent): nothing runs here */
int rx_jobs_threads(void) { return 0; }
int rx_jobs_on(int family) { (void)family; return 0; }
void rx_jobs_run(const rx_job* jobs, int n) { (void)jobs; (void)n; }
void rx_job_ordered(void) {}
void rx_jobs_cs_enter(void) {}
void rx_jobs_cs_leave(void) {}
void rx_jobs_lock_key(rx_addr key) { (void)key; }
void rx_jobs_cs_enter_key(rx_addr cs) { (void)cs; }
void rx_jobs_cs_leave_key(rx_addr cs) { (void)cs; }
void rx_jobs_unlock_key(rx_addr key) { (void)key; }
void rx_job_store_last(rx_addr a, const uint32_t* w, int n) { for (int i = 0; i < n; i++) SU32(a, 4 * i, w[i]); }
rx_addr rx_job_scratch(rx_addr global, uint32_t size) { (void)size; return global; }
void rx_job_scratch_used(rx_addr global, uint32_t bytes) { (void)global; (void)bytes; }
#else
#include <stdatomic.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#endif

/* ---- the worker contexts: context k's 128 KB at 0x7f000000 + 0x20000 k (k < 96: up to 0x7fc00000) (free arena: the CRT images sit at 0x78480000..
 * 0x785bffff, the game thread's stack and TEB at 0x7fec0000..0x7ffdffff), the stack below its TEB page at the top ---- */
#define JOB_CTX_BASE 0x7f000000u
#define JOB_CTX_SIZE 0x20000u
/* rebuilt at every job start (the registers from the game thread's at the batch); one per 256 bytes: the jobs write their stack
 * pointer and register models all the time, and two on one cache line (128 bytes on Apple's cores) would make the cores fight over it */
static union { rx_cpu c; uint8_t pad[256]; } g_ctx_cpu[RX_JOB_CTX_N] __attribute__((aligned(256)));

/* ---- the pool (host threads; kept across session resets: the threads outlive a restore of the statics) ---- */
#define MAX_THREADS 16
#define MAX_JOBS 32
typedef struct {
    int mode;                                   /* -1 unread */
    int families;                               /* RX_JOB_FAMILIES */
    int started;                                /* helper threads running */
    _Atomic(const rx_job*) jobs;                /* the batch */
    _Atomic(uint64_t) ticket;                   /* batch id << 32 | job count << 16 | the next job index to take */
    atomic_int finished;                        /* jobs done */
    atomic_uchar done[MAX_JOBS];
    atomic_uint gen;                            /* batch generation (helpers wait for a change) */
    atomic_int quit;
#ifdef _WIN32
    SRWLOCK m; CONDITION_VARIABLE cv;
#else
    pthread_mutex_t m; pthread_cond_t cv;
#endif
} job_pool;
static job_pool g_pool RX_RESET_KEEP = {.mode = -1};
/* rx_job_store_last's per-job logs (below): host memory, grown by the job's own thread, replayed by the batch's caller */
typedef struct { rx_addr a; int n; uint32_t w[4]; } last_rec;
static struct { last_rec* r; int n, cap; } g_log[MAX_JOBS] RX_RESET_KEEP;
static uint32_t g_batch_id RX_RESET_KEEP;
static void logs_apply(int n);
/* the running job's index in a parallel batch (-1: none) and its lock depth: in its rx_cpu (no emulated TLS on MinGW) */
static inline rx_cpu* par_cpu(void) { return rx_cpu_par ? rx_cpu_tls_get() : NULL; }
static inline int cur_job(void) { rx_cpu* const c = par_cpu(); return c ? c->job : -1; }
/* the job whose store log takes rx_job_store_last (parallel or one after another) */
static inline int log_job(void) { return rx_cpu_par ? cur_job() : rx_cpu_main.job; }

int rx_jobs_threads(void) {
    if (g_pool.mode < 0) {
        const char* e = getenv("RX_JOB_THREADS");
        int n;
        if (e && *e) n = atoi(e);
        else {
#ifdef _WIN32
            SYSTEM_INFO si; GetSystemInfo(&si); n = (int)si.dwNumberOfProcessors;
#else
            n = (int)sysconf(_SC_NPROCESSORS_ONLN);
#endif
            if (n > 8) n = 8;
        }
        if (n < 0) n = 0;
        if (n > MAX_THREADS) n = MAX_THREADS;
        const char* f = getenv("RX_JOB_FAMILIES");
        g_pool.families = f && *f ? (int)strtol(f, NULL, 0) : RX_JOB_RIDERS | RX_JOB_RSYNC | RX_JOB_PARTICLES | RX_JOB_ENTITIES | RX_JOB_CAPTURE | RX_JOB_SIMS;
        g_pool.mode = n;
    }
    return g_pool.mode;
}
int rx_jobs_on(int family) { return rx_jobs_threads() > 0 && (g_pool.families & family) != 0; }

static void run_job(const rx_job* j, const rx_cpu* regs, int index) {
    rx_cpu* const c = &g_ctx_cpu[j->ctx].c;
    const rx_addr teb = JOB_CTX_BASE + JOB_CTX_SIZE * (uint32_t)j->ctx + JOB_CTX_SIZE - 0x1000u;
    *c = *regs;                                 /* the register models as the game thread has them at the batch */
    c->teb = teb;
    c->job = index; c->cs_depth = 0;           /* (-1: a nested batch's job, no store log) */
    SU32(teb, 0, 0xffffffffu);                  /* fs:[0]: the end of the SEH chain */
    SU32(teb, 0x2c, j->tls);                    /* fs:[0x2c]: the thread's TLS pointer array */
    /* the worker's stack from its top, as its thread loop calls the body */
    if (rx_cpu_par) {                           /* this host thread's, for the job (rx_sp reads c->sp) */
        c->sp = teb;
        cpu_tls_set(c);
        j->body(j->arg);
        cpu_tls_set(NULL);
    } else if (j->ctx >= RX_JOB_CTX_SIM && j->ctx < RX_JOB_CTX_RIDER2) {      /* (the same, a -DRX_WATCH build's job-guard units: the simulations) */
        const rx_cpu saved = rx_cpu_main;
        const rx_addr saved_sp = rx_g_sp;
        rx_cpu_main = *c;
        rx_g_sp = teb;
        g_seq_depth++;
        RX_WATCH_MARK(10 + index, j->arg);
        j->body(j->arg);
        RX_WATCH_MARK(9, j->arg);
        g_seq_depth--;
        rx_g_sp = saved_sp;
        rx_cpu_main = saved;
    } else {                                    /* one after another on the calling thread: the job's state swapped into the globals */
        const rx_cpu saved = rx_cpu_main;
        const rx_addr saved_sp = rx_g_sp;
        rx_cpu_main = *c;
        rx_g_sp = teb;
        g_seq_depth++;
        j->body(j->arg);
        g_seq_depth--;
        rx_g_sp = saved_sp;
        rx_cpu_main = saved;
    }
}

static rx_cpu g_batch_regs;                     /* the game thread's registers when the batch started (written before the ticket opens) */
static void take_jobs(void) {
    for (;;) {
        /* a claim counts only in the batch it was made in: a thread leaving the last batch late cannot take a new one's job */
        uint64_t t = atomic_load(&g_pool.ticket);
        const int i = (int)(t & 0xffff);
        if (i >= (int)((t >> 16) & 0xffff)) return;
        if (!atomic_compare_exchange_weak(&g_pool.ticket, &t, t + 1)) continue;
        run_job(&atomic_load(&g_pool.jobs)[i], &g_batch_regs, i);
        atomic_store_explicit(&g_pool.done[i], 1, memory_order_release);
        atomic_fetch_add_explicit(&g_pool.finished, 1, memory_order_acq_rel);
    }
}

#ifdef _WIN32
static DWORD WINAPI helper_main(LPVOID p) {
#else
static void* helper_main(void* p) {
#endif
    (void)p;
    unsigned seen = atomic_load(&g_pool.gen);
    for (;;) {
        /* a short spin first: the frame's batches come close together */
        int spins = 0;
        while (atomic_load_explicit(&g_pool.gen, memory_order_acquire) == seen && !atomic_load(&g_pool.quit)) {
            if (++spins < 4000) {
#if defined(__aarch64__)
                __asm__ __volatile__("yield");
#elif defined(__x86_64__) || defined(_M_X64)
                __builtin_ia32_pause();
#endif
                continue;
            }
#ifdef _WIN32
            AcquireSRWLockExclusive(&g_pool.m);
            while (atomic_load(&g_pool.gen) == seen && !atomic_load(&g_pool.quit)) SleepConditionVariableSRW(&g_pool.cv, &g_pool.m, INFINITE, 0);
            ReleaseSRWLockExclusive(&g_pool.m);
#else
            pthread_mutex_lock(&g_pool.m);
            while (atomic_load(&g_pool.gen) == seen && !atomic_load(&g_pool.quit)) pthread_cond_wait(&g_pool.cv, &g_pool.m);
            pthread_mutex_unlock(&g_pool.m);
#endif
        }
        if (atomic_load(&g_pool.quit)) break;
        seen = atomic_load_explicit(&g_pool.gen, memory_order_acquire);
        take_jobs();
    }
    return 0;
}

static void pool_start(int helpers) {
#ifdef _WIN32
    if (g_tls_slot == TLS_OUT_OF_INDEXES) g_tls_slot = TlsAlloc();
    if (g_tls_slot == TLS_OUT_OF_INDEXES) return;   /* (no slot: the batches stay one after another) */
    InitializeSRWLock(&g_pool.m); InitializeConditionVariable(&g_pool.cv);
    for (int i = 0; i < helpers; i++) {
        HANDLE h = CreateThread(NULL, 1u << 20, helper_main, NULL, 0, NULL);
        if (!h) break;
        CloseHandle(h);
        g_pool.started++;
    }
#else
    pthread_mutex_init(&g_pool.m, NULL); pthread_cond_init(&g_pool.cv, NULL);
    for (int i = 0; i < helpers; i++) {
        pthread_t t; pthread_attr_t at; pthread_attr_init(&at); pthread_attr_setstacksize(&at, 1u << 20);
        const int rc = pthread_create(&t, &at, helper_main, NULL);
        pthread_attr_destroy(&at);
        if (rc != 0) break;
        pthread_detach(t);
        g_pool.started++;
    }
#endif
}

/* RX_JOB_STATS=1: at exit, per context family (by the first job's context) the batches, jobs and wall time spent in rx_jobs_run */
#include <stdio.h>
#include <time.h>
static struct { int on; unsigned long long batches[6], jobs[6]; double ms[6]; } g_stats RX_RESET_KEEP = {-1, {0}, {0}, {0}};
static void stats_print(void) {
    static const char* const name[6] = {"rider passes", "render sync", "particle parts", "entity jobs", "pose captures", "simulations"};
    for (int f = 0; f < 6; f++)
        fprintf(stderr, "rx_jobs: %-14s %llu batches, %llu jobs, %.1f ms (%.3f ms per batch)\n", name[f], g_stats.batches[f], g_stats.jobs[f],
                g_stats.ms[f], g_stats.batches[f] ? g_stats.ms[f] / (double)g_stats.batches[f] : 0.0);
}
#if defined(_WIN32) && !defined(RX_ORACLE)   /* QueryPerformanceCounter: mingw's clock_gettime lives in winpthreads, which the DLL must not link */
static double now_ms(void) { LARGE_INTEGER c, f; QueryPerformanceCounter(&c); QueryPerformanceFrequency(&f); return (double)c.QuadPart * 1e3 / (double)f.QuadPart; }
#else
static double now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e3 + t.tv_nsec / 1e6; }
#endif
static void batch_run(const rx_job* jobs, int n);
void rx_jobs_run(const rx_job* jobs, int n) {
    if (g_stats.on < 0) { g_stats.on = getenv("RX_JOB_STATS") != NULL; if (g_stats.on) atexit(stats_print); }
    if (!g_stats.on || n <= 0) { batch_run(jobs, n); return; }
    const int f = jobs[0].ctx >= RX_JOB_CTX_RIDER2 ? 0 : jobs[0].ctx >= RX_JOB_CTX_SIM ? 5 : jobs[0].ctx >= RX_JOB_CTX_CAP ? 4 : jobs[0].ctx >= RX_JOB_CTX_ENT ? 3 : jobs[0].ctx >= RX_JOB_CTX_PART ? 2 : jobs[0].ctx >= RX_JOB_CTX_RSYNC ? 1 : 0;
    const double t0 = now_ms();
    batch_run(jobs, n);
    g_stats.ms[f] += now_ms() - t0; g_stats.batches[f]++; g_stats.jobs[f] += (unsigned long long)n;
}
static void batch_run(const rx_job* jobs, int n) {
    if (n <= 0) return;
    const int mode = rx_jobs_threads();
    g_batch_regs = *rx_cpu_cur();
    if (rx_cpu_par) { for (int i = 0; i < n; i++) jobs[i].body(jobs[i].arg); return; }   /* (inside a parallel job: in its context; no family does this) */
    if (g_pool.started == 0 && mode > 1 && n > 1 && g_seq_depth == 0 && n <= MAX_JOBS) pool_start(mode - 1);
    if (mode <= 1 || n == 1 || g_seq_depth > 0 || n > MAX_JOBS || g_pool.started == 0) {   /* one after another (a nested batch too) */
        const int outer = g_seq_depth == 0 && n <= MAX_JOBS;
        const rx_cpu regs = g_batch_regs;
        for (int i = 0; i < n; i++) run_job(&jobs[i], &regs, outer ? i : -1);
        if (jobs[0].ctx >= RX_JOB_CTX_SIM) RX_WATCH_MARK(8, 0);
        if (outer) logs_apply(n);               /* the cache words the jobs stored, in job order (as the parallel run) */
        return;
    }
    atomic_store(&g_pool.jobs, jobs);
    for (int i = 0; i < n; i++) atomic_store(&g_pool.done[i], 0);
    atomic_store(&g_pool.finished, 0);
    rx_cpu_par = 1;                             /* every thread's guest state is now its rx_cpu_tls (read only inside jobs) */
    ++g_batch_id;
    atomic_store(&g_pool.ticket, ((uint64_t)g_batch_id << 32) | ((uint64_t)n << 16));   /* opens the batch */
#ifdef _WIN32
    AcquireSRWLockExclusive(&g_pool.m);
    atomic_fetch_add_explicit(&g_pool.gen, 1, memory_order_release);
    ReleaseSRWLockExclusive(&g_pool.m);
    WakeAllConditionVariable(&g_pool.cv);
#else
    pthread_mutex_lock(&g_pool.m);
    atomic_fetch_add_explicit(&g_pool.gen, 1, memory_order_release);
    pthread_mutex_unlock(&g_pool.m);
    pthread_cond_broadcast(&g_pool.cv);
#endif
    take_jobs();                                /* the calling thread works too */
    while (atomic_load_explicit(&g_pool.finished, memory_order_acquire) < n) {
#if defined(__aarch64__)
        __asm__ __volatile__("yield");
#elif defined(__x86_64__) || defined(_M_X64)
        __builtin_ia32_pause();
#endif
    }
    rx_cpu_par = 0;
    logs_apply(n);                              /* the cache words the jobs stored, in job order */
}

void rx_job_ordered(void) {
    RX_WATCH_MARK(7, 0);                        /* (-DRX_WATCH: what follows in the job is ordered) */
    if (!rx_cpu_par) return;
    const int me = cur_job();
    if (me < 0) return;
    for (int j = 0; j < me; j++)
        while (!atomic_load_explicit(&g_pool.done[j], memory_order_acquire)) {
#ifdef _WIN32
            SwitchToThread();
#else
            sched_yield();
#endif
        }
}

/* rx_job_store_last: inside a parallel job the stores go to the job's log; after the batch the logs are applied in job order (each in
 * its own order), so the words end as the one-after-another run leaves them, with no lock in the jobs. */
void rx_job_store_last(rx_addr a, const uint32_t* w, int n) {
    const int me = log_job();
    if (me < 0 || me >= MAX_JOBS || n > 4) { for (int i = 0; i < n; i++) SU32(a, 4 * i, w[i]); return; }
    __typeof__(&g_log[0]) L = &g_log[me];
    if (L->n == L->cap) {
        const int cap = L->cap ? 2 * L->cap : 256;
        last_rec* r = (last_rec*)realloc(L->r, sizeof(last_rec) * (size_t)cap);
        if (!r) abort();
        L->r = r; L->cap = cap;
    }
    last_rec* const e = &L->r[L->n++];
    e->a = a; e->n = n;
    for (int i = 0; i < n; i++) e->w[i] = w[i];
}
/* rx_job_scratch: a job's private copy of a global scratch buffer, at the bottom of its context (below its stack), copied in on the
 * job's first use (the words no job writes keep their value); rx_job_scratch_used: the job wrote [0, bytes) of it. After the batch
 * the copies go back in job order, each over what its job wrote, so the global ends as the one-after-another run leaves it. */
#define SCR_PER_JOB 2
#define SCR_SPACE 0x5000u                       /* the bottom 20 KB of a 128 KB context (the stack starts at 0x1f000 above) */
static struct { rx_addr global, priv; uint32_t size, used; } g_scr[MAX_JOBS][SCR_PER_JOB];
static int g_scr_n[MAX_JOBS];
rx_addr rx_job_scratch(rx_addr global, uint32_t size) {
    const int me = log_job();
    if (me < 0 || me >= MAX_JOBS) return global;
    for (int i = 0; i < g_scr_n[me]; i++) if (g_scr[me][i].global == global) return g_scr[me][i].priv;
    rx_addr off = 0;
    for (int i = 0; i < g_scr_n[me]; i++) off += (g_scr[me][i].size + 15u) & ~15u;
    if (g_scr_n[me] == SCR_PER_JOB || off + size > SCR_SPACE) abort();   /* (the families' buffers fit) */
    const rx_addr priv = rx_teb() - (JOB_CTX_SIZE - 0x1000u) + off;     /* the context's base */
    rx_move(priv, global, size);
    g_scr[me][g_scr_n[me]].global = global; g_scr[me][g_scr_n[me]].priv = priv;
    g_scr[me][g_scr_n[me]].size = size; g_scr[me][g_scr_n[me]].used = 0;
    g_scr_n[me]++;
    return priv;
}
void rx_job_scratch_used(rx_addr global, uint32_t bytes) {
    const int me = log_job();
    if (me < 0 || me >= MAX_JOBS) return;
    for (int i = 0; i < g_scr_n[me]; i++)
        if (g_scr[me][i].global == global) { if (bytes > g_scr[me][i].used) g_scr[me][i].used = bytes < g_scr[me][i].size ? bytes : g_scr[me][i].size; return; }
}
static void logs_apply(int n) {
    for (int j = 0; j < n; j++) {
        for (int k = 0; k < g_log[j].n; k++) {
            const last_rec* const e = &g_log[j].r[k];
            for (int i = 0; i < e->n; i++) SU32(e->a, 4 * i, e->w[i]);
        }
        g_log[j].n = 0;
        for (int i = 0; i < g_scr_n[j]; i++) if (g_scr[j][i].used) rx_move(g_scr[j][i].global, g_scr[j][i].priv, g_scr[j][i].used);
        g_scr_n[j] = 0;
    }
}

/* the jobs' lock: a spin lock owned by the running job (its index + 1; recursive), the critical sections being a few hundred
 * instructions (an OS mutex hands off through the kernel when contended, which costs more than the sections) */
static atomic_int g_cs_owner;
void rx_jobs_cs_enter(void) {
    rx_cpu* const c = par_cpu();               /* (no thread-local read outside a parallel batch) */
    if (!c || c->job < 0) return;
    const int me = c->job + 1;
    if (atomic_load_explicit(&g_cs_owner, memory_order_relaxed) == me) { c->cs_depth++; return; }
    for (int spins = 0;; spins++) {
        int free_ = 0;
        if (atomic_compare_exchange_weak_explicit(&g_cs_owner, &free_, me, memory_order_acquire, memory_order_relaxed)) break;
        if (spins > 1000) {
#ifdef _WIN32
            SwitchToThread();
#else
            sched_yield();
#endif
        }
    }
    c->cs_depth = 1;
}
void rx_jobs_cs_leave(void) {
    rx_cpu* const c = par_cpu();
    if (!c || c->job < 0 || c->cs_depth == 0) return;
    if (--c->cs_depth == 0) atomic_store_explicit(&g_cs_owner, 0, memory_order_release);
}
static struct { atomic_int v; char pad[124]; } g_key_lock[64] __attribute__((aligned(128)));
static unsigned key_slot(rx_addr key) { return (key * 2654435761u) >> 26; }
void rx_jobs_lock_key(rx_addr key) {
    if (cur_job() < 0) return;
    atomic_int* const l = &g_key_lock[key_slot(key)].v;
    for (int spins = 0;; spins++) {
        int free_ = 0;
        if (atomic_compare_exchange_weak_explicit(l, &free_, 1, memory_order_acquire, memory_order_relaxed)) return;
        if (spins > 1000) {
#ifdef _WIN32
            SwitchToThread();
#else
            sched_yield();
#endif
        }
    }
}
/* the original's critical sections inside a parallel job: one recursive spin lock per section address (256 of them, by address;
 * two sections on one lock only serialize more) */
static struct { atomic_int owner; int depth; char pad[120]; } g_cs_lock[256] __attribute__((aligned(128)));
void rx_jobs_cs_enter_key(rx_addr cs) {
    rx_cpu* const c = par_cpu();
    if (!c || c->job < 0) return;
    __typeof__(&g_cs_lock[0]) L = &g_cs_lock[(cs * 2654435761u) >> 24];
    const int me = c->job + 1;
    if (atomic_load_explicit(&L->owner, memory_order_relaxed) == me) { L->depth++; return; }
    for (int spins = 0;; spins++) {
        int free_ = 0;
        if (atomic_compare_exchange_weak_explicit(&L->owner, &free_, me, memory_order_acquire, memory_order_relaxed)) break;
        if (spins > 1000) {
#ifdef _WIN32
            SwitchToThread();
#else
            sched_yield();
#endif
        }
    }
    L->depth = 1;
}
void rx_jobs_cs_leave_key(rx_addr cs) {
    rx_cpu* const c = par_cpu();
    if (!c || c->job < 0) return;
    __typeof__(&g_cs_lock[0]) L = &g_cs_lock[(cs * 2654435761u) >> 24];
    if (atomic_load_explicit(&L->owner, memory_order_relaxed) != c->job + 1) return;
    if (--L->depth == 0) atomic_store_explicit(&L->owner, 0, memory_order_release);
}
void rx_jobs_unlock_key(rx_addr key) {
    if (cur_job() < 0) return;
    atomic_store_explicit(&g_key_lock[key_slot(key)].v, 0, memory_order_release);
}
#endif
