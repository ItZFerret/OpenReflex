/* loop_jobs.h -- the original's worker-thread jobs on host threads (w-jobthreads).
 *
 * The original runs the rider animation passes (12 RiderAnimation_WorkerThread threads, one per rider slot), the render sync (4
 * Render_SyncWorkerThread threads) and its job pool's work (2 JobPool_WorkerThread threads: the particle update among others) on
 * their own threads, each with its own stack, TEB and registers. The core used to run them in place on the game thread
 * (loop_wake_worker). Here a batch of such jobs runs with one guest thread per job (rx_cpu, rx_mem.h): the job's own stack region
 * and TEB in the arena (its worker's, the same every frame) and its own register models; the host threads of a small persistent
 * pool take the jobs, and the batch returns when every job is done. Which host thread runs a job does not matter: all a job can see
 * of "its thread" is its rx_cpu, fixed per job. The families: the rider passes (anim_scenario_rider.c), the render sync's four
 * workers (loop_engine.c; the records dealt to them in a fixed pattern), the particle update job's loop in parts of consecutive
 * particles (fx_particle.c; the original's two jobs split it by a shared counter) and the scene ticker's ambient entity jobs
 * (loop_engine.c, w-jobthreads2: queued in the pool's ring while the ticker runs, then run in parts of consecutive jobs).
 *
 * RX_JOB_THREADS (read once): 0 = the old behaviour (loop_wake_worker in place, on the game thread's stack and TEB); 1 = the
 * worker contexts, run one after another on the calling thread; n > 1 = the same on n host threads (the calling one included).
 * Unset: the machine's processor count, at most 8. RX_JOB_FAMILIES: a mask of the families that run as jobs (RX_JOB_* below;
 * default all: 63).
 * Modes 1 and n leave the same memory, byte for byte (core/plan/QUEUE.md w-jobthreads, core/tools/jobthreads_check.sh): a job
 * writes its own state; the shared words the jobs touch are drawn in the batch's order (rx_job_ordered: the game LCG), stored as
 * the last job leaves them (rx_job_store_last / SU32J: caches, search cursors and "in use" flags; no job reads another's during
 * the batch, in mode 1 too), or computed under a lock to values that do not depend on who computes them (rx_jobs_cs_enter,
 * rx_jobs_lock_key, the original's critical sections by address). Mode 0 differs from them only in
 * dead-stack words: the in-place passes leave their frames in the game thread's stack, which later code copies uninitialised. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

/* the worker contexts: the rider animation threads by slot, the four render-sync workers, the particle job's parts (fx_particle.c) */
/* (w-rc-core) RX_JOB_CTX_RIDER2: the rider animation threads of slots 12..31 (rider_cap.h), contexts 0x7fc00000 .. 0x7fe7ffff */
enum { RX_JOB_CTX_RIDER = 0, RX_JOB_CTX_RSYNC = 12, RX_JOB_CTX_PART = 16, RX_JOB_CTX_ENT = 32, RX_JOB_CTX_CAP = 48, RX_JOB_CTX_SIM = 64, RX_JOB_CTX_RIDER2 = 96, RX_JOB_CTX_N = 116 };

typedef struct {
    void (*body)(rx_addr arg);
    rx_addr arg;
    int ctx;          /* RX_JOB_CTX_*: the guest thread it runs as (one job per context in a batch) */
    rx_addr tls;      /* the thread's TLS pointer array (fs:[0x2c]; loop_worker_tls) */
} rx_job;

int  rx_jobs_threads(void);                 /* the RX_JOB_THREADS mode above */
/* the families that run as jobs (RX_JOB_FAMILIES, a mask; unset: all); the others stay in place (loop_wake_worker) */
enum { RX_JOB_RIDERS = 1, RX_JOB_RSYNC = 2, RX_JOB_PARTICLES = 4, RX_JOB_ENTITIES = 8, RX_JOB_CAPTURE = 16, RX_JOB_SIMS = 32 };
int  rx_jobs_on(int family);                /* rx_jobs_threads() > 0 and the family is in the mask */
void rx_jobs_run(const rx_job* jobs, int n);   /* the batch, in the original's wake order; returns when all are done */
/* Inside a job: returns once every earlier job of the batch has finished, so what follows sees and leaves shared state as the
 * one-after-another run does (the game LCG's draws in the wake order). Outside a parallel batch: returns at once. */
void rx_job_ordered(void);
static inline int rx_jobs_parallel(void) { return rx_cpu_par; }   /* a parallel batch is running (rx_mem.h) */
/* n guest words at a (a cache several jobs of a batch write and none reads during it): inside a parallel job they are stored only
 * after the batch, in job order, so they end as the one-after-another run leaves them (the last job's last store), and no job
 * sees another's (or its own) during the batch -- the same in the one-after-another mode 1, so modes 1..n leave the same memory;
 * outside a job a plain store. At most 4 words. */
void rx_job_store_last(rx_addr a, const uint32_t* w, int n);
/* a global scratch buffer (the vehicle classes' terrain triangles) inside a job: the job's private copy, and how much of it the job
 * wrote; copied back in job order after the batch (loop_jobs.c). Outside a job: the global itself, nothing recorded. */
rx_addr rx_job_scratch(rx_addr global, uint32_t size);
void rx_job_scratch_used(rx_addr global, uint32_t bytes);
/* one such word (the resource "in use" flags several jobs set to 1, then 0: they end as the last job leaves them) */
static inline int rx_jobs_in_job(void) { return rx_cpu_par || rx_cpu_main.job >= 0; }   /* running a batch's job (either way) */
static inline void rx_job_wr32(rx_addr a, uint32_t v) { if (rx_jobs_in_job()) rx_job_store_last(a, &v, 1); else rx_wr32(a, v); }
#define SU32J(p, o, v) rx_job_wr32((rx_addr)(p) + (o), (uint32_t)(v))
/* loop_enter_cs / loop_leave_cs inside a parallel job: one host lock for all of the original's critical sections (recursive per
 * thread); nothing elsewhere. */
void rx_jobs_cs_enter(void);
void rx_jobs_cs_leave(void);
/* inside a parallel job: one of 64 spin locks chosen by `key` (a guest address: the node set, ...), not recursive; nothing elsewhere */
void rx_jobs_lock_key(rx_addr key);
void rx_jobs_unlock_key(rx_addr key);

/* loop_platform.c: the TLS pointer array of the worker woken by `go` with the side effects the in-place wake has (the game
 * thread's array made on the first wake; its fs:[0x2c] set to it) -- run on the game thread when the batch is built */
rx_addr loop_worker_tls(uint32_t go);
/* loop_engine.c: the entity a following spline entity of the running entity-job batch follows (its job is ordered) */
int rx_ent_job_is_lead(rx_addr e);
/* world_rider_62.c (w-jobthreads3): the riders' bone-pose captures of the post-step listeners, deferred to their end */
void rx_rider_capture_begin(void);
void rx_rider_capture_flush(void);
/* -DRX_WATCH builds only (development): marks for the driver's dependence check (rx_mem.h rx_watch_access) */
#ifdef RX_WATCH
void rx_watch_mark(int kind, rx_addr arg);
#define RX_WATCH_MARK(k, a) rx_watch_mark((k), (a))
#else
#define RX_WATCH_MARK(k, a) ((void)0)
#endif
/* loop_platform.c's critical sections inside a parallel job: a lock per section address (recursive), see loop_jobs.c */
void rx_jobs_cs_enter_key(rx_addr cs);
void rx_jobs_cs_leave_key(rx_addr cs);

#ifdef __cplusplus
}
#endif
