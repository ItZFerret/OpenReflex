/* loop_engine.c -- the engine frame: Engine::Update 0084ed20 (the whole frame: timing, input, the game's update, the scene
 * tickers, the job pool, the render cameras) and Engine::UpdateTiming 0084ea80, with the worker threads the frame wakes (scene
 * ticker, engine tasks, job pool), run synchronously in the original's order (loop_platform.h).
 *
 * Engine (g_EngineFrameTiming 00d728d0, context + 0x10): +0x08 the game (MXGame), +0x14 frame cap (1/60 s; FLT_MAX = off),
 * +0x18 dt, +0x1c smoothing on, +0x20 ring index, +0x24..+0x34 the last 5 frame times, +0x38 their sum, +0x3c time, +0x40 scaled
 * time (+0x44 scale), +0x48 DeltaSecondsClamp, +0x4c timing tick, +0x54 / +0x58 frame / update counters, +0x68 last counter,
 * +0x70 last FPS counter / +0x80 its tick, +0x84 the FPS display's id, +0x90 / +0x94 task thread go / done events, +0x98 tasks
 * (+0xc0 count). Engine context (g_EngineContext 00a95844): +0x0c a service (slots 3, 30), +0x10 the engine, +0x14 the renderer,
 * +0x1c / +0x20 / +0x24 the three scenes. (_re/notes/physics-timing.md, bootstrap.md, native/game/notes/engine.md) */
#include "loop_engine.h"
#include "fx.h"
#include "fx_util.h"
#include "loop_input.h"
#include "loop_platform.h"
#include "loop_util.h"
#include "loop_extern.h"
#include "anim_ik.h"
#include "build_globals_int.h"
#include "../src/rx_x87.h"
#include "rx_vcall.h"
#include "race_results_table.h"
#include "loop_input_suspend.h"
#include "crt.h"
#include "loop_jobs.h"
#include <stdio.h>
#include <stdlib.h>

/* ---- Engine::UpdateTiming ---- */

/* The main thread's x87 precision: control word 0x027f (53-bit significands; read in the verify worlds). */
#define MAIN_X87_BITS 53
/* The x87 conversion of a 64-bit counter difference (fild of the low 63 bits, fild of the sign bit as an int64, fchs, faddp: the
 * difference as unsigned), at the main thread's precision. */
static rx_f80 x87_neg(rx_f80 a) { a.sign ^= 1; return a; }
static rx_f80 ticks_f80(uint64_t d) {
    const rx_f80 low = rx_f80_from_int((int64_t)(d & 0x7fffffffffffffffull));
    const rx_f80 top = rx_f80_from_int((int64_t)(d & 0x8000000000000000ull));
    return rx_f80_add(low, x87_neg(top), MAIN_X87_BITS, 0);
}
/* seconds between two counter readings: (now - then) / frequency */
static rx_f80 seconds_f80(uint64_t now, uint64_t then, rx_addr freq) {
    return rx_f80_div(ticks_f80(now - then), rx_f80_from_int((int64_t)rx_rd64(freq)), MAIN_X87_BITS, 0);
}

/* 0084ea80 Engine::UpdateTiming (ESI = engine): dt = the counter time since +0x68; with a frame cap (+0x14 != FLT_MAX) it
 * busy-waits on the counter until dt reaches the cap. Smoothing (+0x1c): dt = 0.2 x the sum of the last five frame times.
 * dt = min(dt, +0x48); time += dt, scaled time = scale x time (the old time in 009df490); +0x68 = this frame's counter. Once a
 * second (elapsed x 1000 > 1000) the frame rate goes to the service at context + 0x0c (slot 30) and the FPS clock restarts;
 * +0x4c counts the calls. */
void rx_engine_update_timing(rx_addr engine) {
    /* the original's frame: 8-aligned below the saved EBP; the counter / frequency / FPS locals at their places */
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4) & ~7u;
    rx_set_sp(A - 0x48);
    const rx_addr counter = A - 0x40, freq = A - 0x38, counter2 = A - 0x18, freq2 = A - 0x20, fps = A - 0x24;
    loop_qpc(counter);
    uint64_t now = rx_rd64(counter);
    const uint64_t last = rx_rd64(engine + 0x68);
    loop_qpf(freq);
    float dt = rx_f80_to_f32(seconds_f80(now, last, freq), 0);
    SF32(engine, 0x18, dt);
    const float cap = F32(engine, 0x14);
    if (cap != 3.4028235e38f /* 0x977494 FLT_MAX */ && cap > dt) {
        do {
            loop_qpc(counter2);
            now = rx_rd64(counter2);
            loop_qpf(freq2);
            SF32(engine, 0x18, rx_f80_to_f32(seconds_f80(now, rx_rd64(engine + 0x68), freq2), 0));
        } while (F32(engine, 0x14) > F32(engine, 0x18));
    }
    if (U32(engine, 0x1c) != 0) {   /* the mean of the last five frame times */
        const uint32_t i = U32(engine, 0x20);
        const float t = F32(engine, 0x18);
        SF32(engine, 0x38, F32(engine, 0x38) - F32(engine, 0x24 + 4 * i) + t);
        SF32(engine, 0x24 + 4 * i, t);
        SU32(engine, 0x20, i + 1);
        SF32(engine, 0x18, F32(engine, 0x38) * 0.2f /* 0x92e388 */);
        if (I32(engine, 0x20) >= 5) SU32(engine, 0x20, 0);
    }
    dt = F32(engine, 0x18);
    if (dt >= F32(engine, 0x48)) dt = F32(engine, 0x48);   /* DeltaSecondsClamp (a NaN dt stays) */
    const float time = F32(engine, 0x3c);
    const uint64_t fps_last = rx_rd64(engine + 0x70);
    SF32(engine, 0x18, dt);
    SF32(0, 0x009df490, time);
    SF32(engine, 0x3c, time + dt);
    rx_wr64(engine + 0x68, now);
    SF32(engine, 0x40, F32(engine, 0x44) * (time + dt));
    loop_qpf(freq2);
    const uint32_t tick = U32(engine, 0x4c), frames = tick - U32(engine, 0x80);
    const rx_f80 k = rx_f80_from_f32(F32(0, 0x009cf5b0));                     /* 1000 (ms per second) */
    const rx_f80 elapsed = rx_f80_mul(seconds_f80(now, fps_last, freq2), k, MAIN_X87_BITS, 0);
    rx_f80 n = rx_f80_from_int((int32_t)frames);
    if ((int32_t)frames < 0) n = rx_f80_add(n, rx_f80_from_f32(4294967296.0f /* 0x97731c */), MAIN_X87_BITS, 0);
    SF32(fps, 0, rx_f80_to_f32(rx_f80_mul(rx_f80_div(n, elapsed, MAIN_X87_BITS, 0), k, MAIN_X87_BITS, 0), 0));
    if (rx_f80_cmp(elapsed, k) == 1) {                                         /* fcomip: elapsed > 1000 */
        const rx_addr svc = U32(loop_ctx(), 0xc);
        loop_tc3(loop_vslot(svc, 0x78), svc, U32(engine, 0x84), fps, 1);
        SU32(engine, 0x80, U32(engine, 0x4c));
        rx_wr64(engine + 0x70, now);
        SU32(engine, 0x4c, U32(engine, 0x80) + 1);
    } else {
        SU32(engine, 0x4c, tick + 1);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0084ea80, rx_engine_update_timing)

/* ---- the worker threads the frame wakes ---- */

#ifndef RX_ORACLE
/* ---- w-jobthreads2: the scene ticker's entity jobs as parallel jobs (loop_jobs.h) ----
 * In place, an ambient entity's update (slot 19) submits its animation job {entity, dt} and the job pool runs it at once. Here the
 * updates that do nothing but that (the single-loop, spline-loop and crew-chief entities, a scenario entity with no delay to play
 * out, an instanced animation of one of the loops) only queue their job in the pool's ring (the worker not woken); any other
 * update first runs what is queued, and so does the end of the ticker. A run takes the queued jobs off the ring as the worker does
 * (the same ring, counters and records) and runs them in up to 16 parts of consecutive jobs, each part as its own worker thread
 * (contexts RX_JOB_CTX_ENT + k), the jobs of a part one after another on its stack, each with the record copied to its frame as the
 * worker's. The jobs write their own entity's animation state. Shared: the motions' caches and in-use flags (rx_job_store_last /
 * SU32J), the per-entity critical sections (keyed locks), a spline entity's sound events (the audio calls and their low-memory words
 * 0xa0 / 0xa4: rx_job_ordered first) and a following spline entity's leader (the leader's spline time: the follower and the
 * leader both ordered, rx_ent_job_ordered_spline). */
static int g_pool_hold;   /* the scene ticker's deferred entity jobs queue without waking the worker (rx_jobpool_submit) */
/* w-lcgdraws: the frame's vehicle effect jobs (00691bd0 in MXGame::Update, 12 jobs 006979a0 that draw the game LCG for every spawned
 * particle) and whatever is queued behind them wait for the game thread's JobPool_WaitIdle after the scenes' SceneManager_Tick
 * (0084ee86): the original's worker is only woken there (SetEvent; the game thread does not block between the submissions and that
 * wait, so the worker runs when it does), after the scene tickers' PlaceableParticleEffects have drawn theirs. Run at once, the jobs
 * drew before them: every frame's draw sequence differed from the original's (core/plan/QUEUE.md w-lcgdraws). */
static int g_pool_defer;
void rx_jobpool_defer_wakes(void) { g_pool_defer = 1; }
#define ENT_MAX 512
#define ENT_PARTS 16
static struct { rx_addr fn; uint32_t rec[16]; } g_ent[ENT_MAX];
static int g_ent_n, g_ent_lo[ENT_PARTS + 1];
static rx_addr g_ent_lead[ENT_MAX];
static int g_ent_nlead;
static int ent_loop_slot(rx_addr fn) { return fn == 0x006ba250u || fn == 0x006bb340u; }   /* single loop / spline loop vf19 */
static int ent_deferrable(rx_addr e, rx_addr fn) {
    if (ent_loop_slot(fn) || fn == 0x006bd6e0u) return 1;                                    /* + crew chief */
    if (fn == 0x0074ab50u) return !(F32(e, 0x478) > 0.0f);                                   /* scenario: nothing to play first */
    if (fn == 0x004137b0u) { const rx_addr inst = U32(e, 0x9f0); return inst == 0 || ent_loop_slot(loop_vslot(inst, 0x4c)); }
    return 0;
}
static void ent_part_body(rx_addr k) {
    for (int j = g_ent_lo[k]; j < g_ent_lo[k + 1]; j++) {
        const rx_addr sp0 = rx_sp(), A = (sp0 - 4) & ~7u, F = A - 0x40;   /* the worker's frame, as rx_jobpool_worker_run's */
        rx_set_sp(A - 0x50);
        for (uint32_t w = 0; w < 16; w++) SU32(F, 4 * w, g_ent[j].rec[w]);
        void (*job)(rx_addr) = RX_FN(g_ent[j].fn, void (*)(rx_addr));
        const uint32_t a[1] = {F};
        if (job) { const rx_addr s = loop_push(1, a, 0x008819fb); job(F); loop_pop(s); }
        else rx_call_original(g_ent[j].fn, RX_CDECL, 0, 1, a);
        rx_set_sp(sp0);
    }
}
/* the followed spline entities of the run (their jobs are ordered too) */
int rx_ent_job_is_lead(rx_addr e) {
    for (int i = 0; i < g_ent_nlead; i++) if (g_ent_lead[i] == e) return 1;
    return 0;
}
static void ent_jobs_flush(void) {
    const rx_addr pool = 0x00d94ae0u;   /* the job pool (JOBPOOL below) */
    if ((U32(0, 0x00d94ad8) & 1) == 0 || U32(pool, 0x8830) == 0) return;
    /* the worker's run (rx_jobpool_worker_run 0): busy, running + 1, the ring in order (record taken, slot freed, pending - 1) */
    SU32(pool, 0x88dc, 1);
    SU32(pool, 0x8840, U32(pool, 0x8840) + 1);
    g_ent_n = 0;
    for (;;) {
        if (I32(pool, 0x8838) >= 0x200) SU32(pool, 0x8838, 0);
        const uint32_t r = U32(pool, 0x8838);
        const rx_addr fn = U32(pool, 0x8030 + 4 * r);
        if (fn == 0 || g_ent_n == ENT_MAX) break;
        SU32(pool, 0x8838, r + 1);
        for (uint32_t w = 0; w < 16; w++) g_ent[g_ent_n].rec[w] = U32(pool + 0x30 + 0x40 * r, 4 * w);
        g_ent[g_ent_n].fn = fn;
        g_ent_n++;
        SU32(pool, 0x8030 + 4 * r, 0);
        SU32(pool, 0x8830, U32(pool, 0x8830) - 1);
    }
    SU32(pool, 0x8840, U32(pool, 0x8840) - 1);
    SU32(pool, 0x88dc, 0);
    if (g_ent_n == 0) return;
    g_ent_nlead = 0;
    for (int j = 0; j < g_ent_n; j++) {
        const rx_addr e = g_ent[j].rec[0];
        if (g_ent[j].fn == 0x006bb010u && U32(e, 0x384) != 0 && U32(e, 0x39c) != 0) g_ent_lead[g_ent_nlead++] = U32(e, 0x39c);
    }
    const int parts = g_ent_n < ENT_PARTS ? g_ent_n : ENT_PARTS;
    rx_job jobs[ENT_PARTS];
    const rx_addr tls = loop_worker_tls(U32(pool, 0x887c));   /* the pool worker's TLS array */
    for (int k = 0; k <= parts; k++) g_ent_lo[k] = (int)((int64_t)g_ent_n * k / parts);
    for (int k = 0; k < parts; k++) {
        jobs[k].body = ent_part_body; jobs[k].arg = (rx_addr)k; jobs[k].ctx = RX_JOB_CTX_ENT + k; jobs[k].tls = tls;
    }
    rx_jobs_run(jobs, parts);
    g_ent_n = 0; g_ent_nlead = 0;
}
#endif

/* 007d22a0 Scene_TickerThread (scene): one wake of a scene's entity ticker (go +0x2ca4, done +0x2ca8): under the scene's lock
 * (+0x1c) every ticked entity (list +0x2c90..+0x2c94) whose interval (+0x20) has passed since its last tick (+0x24) gets its
 * update (slot 19) with the elapsed scaled time, at most DeltaSecondsClamp, times the scene's time scale (+0x2d14). */
void rx_scene_ticker_run(rx_addr scene) {
    RX_FRAME_ENTER(F, 0x18);                 /* the original's frame (callees see its ESP) */
    loop_wait_event(U32(scene, 0x2ca4));
    loop_enter_cs(scene + 0x1c);
    const int32_t n = (int32_t)(U32(scene, 0x2c94) - U32(scene, 0x2c90)) >> 2;
    const float now = F32(loop_timing(), 0x40), clamp = F32(loop_timing(), 0x48);
#ifndef RX_ORACLE
    const int ents = rx_jobs_on(RX_JOB_ENTITIES) && !rx_jobs_parallel();
#endif
    for (int32_t i = 0; i < n; i++) {
        const rx_addr e = U32(U32(scene, 0x2c90), 4u * (uint32_t)i);
        float d = now - F32(e, 0x24);
        if (!(d >= F32(e, 0x20))) continue;
        if (d > clamp) d = clamp;
        SF32(e, 0x24, now);
#ifdef RX_ORACLE
        /* the ticked entity belongs to another unit (entities / anim / fx): the guest runs its update, so an installed port's
         * adapter binds the machine (a direct C call would bypass FX_PORT-style register models) and an uninstalled one stays
         * the original, as in the reference run */
        { const uint32_t a[1] = {rx_fb(F32(scene, 0x2d14) * d)};
          rx_call_original(loop_vslot(e, 0x4c), RX_THISCALL, e, 1, a); }
#else
        if (ents) {   /* w-jobthreads2 (above): queue a deferrable entity's job, run the queue before any other update */
            const rx_addr fn = loop_vslot(e, 0x4c);
            const int defer = ent_deferrable(e, fn);
            if (!defer) ent_jobs_flush();
            g_pool_hold = defer;
            loop_tc_f(fn, e, F32(scene, 0x2d14) * d);
            g_pool_hold = 0;
        } else loop_tc_f(loop_vslot(e, 0x4c), e, F32(scene, 0x2d14) * d);
#endif
    }
#ifndef RX_ORACLE
    if (ents) ent_jobs_flush();
#endif
    loop_leave_cs(scene + 0x1c);
    if (!loop_set_event(U32(scene, 0x2ca8))) loop_get_last_error();
    RX_FRAME_LEAVE(F, 0x18);
}
RX_FUNCTION(0x007d22a0, rx_scene_ticker_run)

/* 0084f320 Engine_TaskThread (engine): one wake (go +0x90): every queued engine task (+0x98, count +0xc0) runs its slot 0, the
 * queue empties, done (+0x94) is signalled. A count of -1 ends the thread (returns 0). */
uint32_t rx_engine_tasks_run(rx_addr engine) {
    RX_FRAME_ENTER(F, 0x10);
    loop_wait_event(U32(engine, 0x90));
    if (I32(engine, 0xc0) == -1) { RX_FRAME_LEAVE(F, 0x10); return 0; }
    for (uint32_t i = 0; i < U32(engine, 0xc0); i++) {
        const rx_addr task = U32(engine, 0x98 + 4 * i);
#ifdef RX_ORACLE
        rx_call_original(loop_vslot(task, 0), RX_THISCALL, task, 0, 0);
#else
        loop_tc0(loop_vslot(task, 0), task);
#endif
    }
    SU32(engine, 0xc0, 0);
    if (!loop_set_event(U32(engine, 0x94))) loop_get_last_error();
    RX_FRAME_LEAVE(F, 0x10);
    return 1;
}
RX_FUNCTION(0x0084f320, rx_engine_tasks_run)
static void engine_tasks_body(rx_addr engine) { rx_engine_tasks_run(engine); }
static void scene_ticker_body(rx_addr scene) { rx_scene_ticker_run(scene); }

/* ---- the job pool (00d94ae0) ----
 * +0x00 lock, +0x18 submit lock, +0x30 the ring of 512 job records (0x40 bytes), +0x8030 their functions (0 = free), +0x8830
 * jobs pending, +0x8834 write index, +0x8838 read index, +0x8840 workers running, +0x8844 worker mask, +0x887c the two workers'
 * wake events, +0x8894 their started events, +0x88c4 quit flags, +0x88dc busy flags. */
#define JOBPOOL 0x00d94ae0u

/* the pool behind its static-init guard (00d94ad8 bit 0: constructed by 00881a70, destroyed at exit by 00912b10) */
static rx_addr jobpool(rx_addr ret) {   /* ret: the return address of the call at the caller's inlined guard */
    if ((U32(0, 0x00d94ad8) & 1) == 0) {
        SU32(0, 0x00d94ad8, U32(0, 0x00d94ad8) | 1);
        { const rx_addr s = loop_push(0, 0, ret); rx_jobpool_construct(JOBPOOL); loop_pop(s); }   /* 00881a70 (ECX = pool) */
        RX_ATEXIT(0x00912b10);   /* _atexit */
    }
    return JOBPOOL;
}

/* wake the first idle worker (in the mask, not busy) */
static void jobpool_wake_idle(rx_addr pool);
#ifdef RX_ORACLE
enum { g_pool_hold = 0, g_pool_defer = 0 };
#endif

/* 00881900 JobPool_WorkerThread (worker index): one wake: run the queued jobs in ring order (each record copied to the worker's
 * frame first, its slot freed, the function called with the copy), then stop. Ends the thread (signals +0x88ac + 4 i) when its
 * quit flag is set. Returns the last job function it ran (0: none). */
rx_addr rx_jobpool_worker_run(uint32_t i) {
    const rx_addr pool = jobpool(0x0088192a);
    rx_addr last = 0;
    loop_reset_event(U32(pool, 0x887c + 4 * i));
    if (!loop_set_event(U32(pool, 0x8894 + 4 * i))) loop_get_last_error();
    if (U32(pool, 0x88c4 + 4 * i) == 0) {
        loop_wait_event(U32(pool, 0x887c + 4 * i));
        SU32(pool, 0x88dc + 4 * i, 1);
        SU32(pool, 0x8840, U32(pool, 0x8840) + 1);           /* InterlockedIncrement: workers running */
        /* the original's frame: 8-aligned below the saved EBP, the job record copy at A - 0x40 (jobs may keep its address) */
        const rx_addr sp0 = rx_sp(), A = (sp0 - 4) & ~7u, F = A - 0x40;
        rx_set_sp(A - 0x50);
        for (;;) {
            loop_enter_cs(pool);
            if (I32(pool, 0x8838) >= 0x200) SU32(pool, 0x8838, 0);
            const uint32_t r = U32(pool, 0x8838);
            const rx_addr fn = U32(pool, 0x8030 + 4 * r);
            if (fn == 0) break;
            SU32(pool, 0x8838, r + 1);
            for (uint32_t k = 0; k < 0x40; k += 4) SU32(F, k, U32(pool + 0x30 + 0x40 * r, k));
            SU32(pool, 0x8030 + 4 * r, 0);
            loop_leave_cs(pool);
#ifdef RX_ORACLE
            /* the job function belongs to another unit (fx / anim / game): the guest runs it, so an installed port's adapter
             * binds the machine and the dead stack the job reads is the original's */
            { const uint32_t a[1] = {F}; rx_call_original(fn, RX_CDECL, 0, 1, a); }
#else
            void (*job)(rx_addr) = RX_FN(fn, void (*)(rx_addr));
            const uint32_t a[1] = {F};
            if (job) { const rx_addr s = loop_push(1, a, 0x008819fb); job(F); loop_pop(s); }
            else rx_call_original(fn, RX_CDECL, 0, 1, a);
#endif
            last = fn;
            SU32(pool, 0x8830, U32(pool, 0x8830) - 1);       /* InterlockedDecrement: jobs pending */
        }
        rx_set_sp(sp0);
        loop_leave_cs(pool);
        SU32(pool, 0x8840, U32(pool, 0x8840) - 1);
        SU32(pool, 0x88dc + 4 * i, 0);
        if (U32(pool, 0x88c4 + 4 * i) == 0) return last;     /* the next wait blocks: the worker's run ends */
    }
    if (!loop_set_event(U32(pool, 0x88ac + 4 * i))) loop_get_last_error();
    return last;
}
RX_FUNCTION(0x00881900, rx_jobpool_worker_run)
static void jobpool_worker_body(rx_addr i) { rx_jobpool_worker_run(i); }

static void jobpool_wake_idle(rx_addr pool) {
    for (uint32_t i = 0; i < 2; i++) {
        if ((U32(pool, 0x8844) & (1u << i)) && U32(pool, 0x88dc + 4 * i) == 0) {
            loop_wake_worker(U32(pool, 0x887c + 4 * i), jobpool_worker_body, i);
            return;
        }
    }
}

/* 00881f00 JobPool_Submit (pool, job record, function; ret 0xc): queue a 16-dword job: wait while the ring slot is taken (wake a
 * worker, Sleep(1)), copy the record into it, count it pending, and wake an idle worker when fewer than 3 run.
 *
 * Called at the original's stack depth (S = ESP at entry: [S] the return address, [S + 4] pool, +8 job, +0xc function). The
 * original's frame leaves words below S in the dead stack that later frames at the same depth read as uninitialised memory (the
 * particle templates copy them into the particle records): its saved EBX / ESI (the caller's, from the register model), the
 * Enter / LeaveCriticalSection arguments (pool + 0x18 / pool) and return addresses, and its first argument slot overwritten
 * with pool + 0x18. The ring-full wait path is not modelled. */
void rx_jobpool_submit(rx_addr pool, rx_addr job, rx_addr fn) {
    if (fn == 0) return;
#ifndef RX_ORACLE
    if (rx_jobs_parallel()) { fprintf(stderr, "rxcore: JobPool_Submit inside a parallel job (%08x)\n", fn); abort(); }   /* (none of the families does) */
#endif
    const rx_addr S = rx_sp();
    const uint32_t ebx = fx_reg(FX_EBX), esi = fx_reg(FX_ESI);
    int woke = I32(pool, 0x8840) >= 3;
    for (uint32_t i = 0; !woke && i < 2; i++) if ((U32(pool, 0x8844) >> i & 1) && U32(pool, 0x88dc + 4 * i) == 0) woke = 1;
    loop_enter_cs(pool + 0x18);
    loop_enter_cs(pool);
    if (I32(pool, 0x8834) >= 0x200) SU32(pool, 0x8834, 0);
    while (U32(pool, 0x8030 + 4 * U32(pool, 0x8834)) != 0) {   /* ring full */
        jobpool_wake_idle(pool);
        loop_leave_cs(pool);
        loop_sleep(1);
        loop_enter_cs(pool);
    }
    const uint32_t w = U32(pool, 0x8834);
    SU32(pool, 0x8030 + 4 * w, fn);
    for (uint32_t k = 0; k < 0x40; k += 4) SU32(pool + 0x30 + 0x40 * U32(pool, 0x8834), k, U32(job, k));
    SU32(pool, 0x8834, U32(pool, 0x8834) + 1);
    SU32(pool, 0x8830, U32(pool, 0x8830) + 1);               /* InterlockedIncrement: jobs pending */
    loop_leave_cs(pool);
    if (I32(pool, 0x8840) < 3 && !g_pool_hold && !g_pool_defer) jobpool_wake_idle(pool);
    loop_leave_cs(pool + 0x18);
    SU32(S, 4, pool + 0x18);
    SU32(S, -4, ebx); SU32(S, -8, esi);
    SU32(S, -0xc, pool + 0x18); SU32(S, -0x10, woke ? 0x00882079u : 0x00882055u); SU32(S, -0x14, 0x0088200du);
}
RX_FUNCTION(0x00881f00, rx_jobpool_submit)

/* 00881d80 JobPool_Recover (stdcall pool; RET 4): the pool's stall recovery, called by JobPool_WaitIdle every 11th round: under the pool's lock
 * every queued job (the 0x200 ring slots: function +0x8030 + 4 k, record 0x40 bytes at +0x30 + 0x40 k) is run by the caller at once: the record
 * copied onto the stack, the slot cleared, the pending count (+0x8830) decremented, then the function called with the copy (cdecl); then the
 * write / read positions (+0x8834 / +0x8838) are reset. Frame: sub esp, 0x48; push ebx / ebp / esi / edi: the record copy at sp0 - 0x40. */
void rx_jobpool_recover(rx_addr pool) {
    const rx_addr sp0 = rx_sp();
    const rx_addr copy = sp0 - 0x40;
    SU32(sp0, -0x4c, sp0 - 0x4c);
    SU32(sp0, -0x50, 0);
    rx_set_sp(sp0 - 0x58);
    loop_enter_cs(pool);
    rx_addr record = pool + 0x30, slot = pool + 0x8030;
    for (uint32_t k = 0; k < 0x200; k++, record += 0x40, slot += 4) {
        const uint32_t fn = U32(slot, 0);
        if (fn == 0) continue;
        for (uint32_t w = 0; w < 0x40; w += 4) SU32(copy, w, U32(record, w));
        SU32(slot, 0, 0);
        SU32(pool, 0x8830, U32(pool, 0x8830) - 1);                  /* InterlockedDecrement: jobs pending */
#ifdef RX_ORACLE
        /* the job function belongs to another unit (fx / anim / game): the guest runs it, as in the worker (rx_jobpool_worker_run) */
        { const uint32_t a[1] = {copy}; rx_call_original(fn, RX_CDECL, 0, 1, a); }
#else
        void (*job)(rx_addr) = RX_FN(fn, void (*)(rx_addr));
        const uint32_t a[1] = {copy};
        if (job) { const rx_addr s = loop_push(1, a, 0x00881ddc); job(copy); loop_pop(s); }
        else rx_call_original(fn, RX_CDECL, 0, 1, a);
#endif
    }
    SU32(pool, 0x8834, 0);
    SU32(pool, 0x8838, 0);
    loop_leave_cs(pool);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00881d80, rx_jobpool_recover)

/* 00881e20 JobPool_WaitIdle (ESI = pool): under the submit lock, until no job is pending: wake an idle worker, Sleep(1), and every
 * 11th round 00881d80 (the pool's stall recovery). The serialized workers have always finished: no job is pending. */
void rx_jobpool_wait_idle(rx_addr pool) {
#ifndef RX_ORACLE
    g_pool_defer = 0;   /* the game thread waits: the deferred wakes' worker runs now (the loop below) */
#endif
    loop_enter_cs(pool + 0x18);
    SU32(0, 0x00a9598c, 0);
    while (U32(pool, 0x8830) != 0) {
        jobpool_wake_idle(pool);
        loop_sleep(1);
        SU32(0, 0x00a9598c, U32(0, 0x00a9598c) + 1);
        if (I32(0, 0x00a9598c) > 10) {
            const uint32_t a[1] = {pool};
            const rx_addr s = loop_push(1, a, 0x00881ec2);
            rx_jobpool_recover(pool);                                  /* 00881d80 (stdcall pool; RET 4) */
            loop_pop(s);
            SU32(0, 0x00a9598c, 0);
        }
    }
    loop_leave_cs(pool + 0x18);
}
RX_FUNCTION(0x00881e20, rx_jobpool_wait_idle)

/* ---- Engine::Update ---- */

/* calls into other subsystems (the core function when registered, else the original) */
static void scene_pre_tick(rx_addr scene) {        /* 007d21d0 (EDI = scene; rx_anim) */
    const rx_addr s = loop_push(0, 0, 0x0084edb4); rx_scene_manager_tick_late(scene); loop_pop(s);
}
static void stdcall1(rx_addr fn, uint32_t a, rx_addr ret) {      /* a stdcall function of one argument (ret: the call site's) */
    void (*f)(uint32_t) = RX_FN(fn, void (*)(uint32_t));
    const uint32_t v[1] = {a};
    if (f) { const rx_addr s = loop_push(1, v, ret); f(a); loop_pop(s); } else rx_call_original(fn, RX_STDCALL, 0, 1, v);
}
static uint32_t renderer_ready(rx_addr renderer) {   /* 007ae4e0 Renderer_EnsureDeviceReady (render: not ported; the product's
                                                      * renderer is Unity's, always ready) */
#ifdef RX_ORACLE
    const uint32_t v[1] = {renderer};
    return rx_call_original(0x007ae4e0, RX_STDCALL, 0, 1, v) & 0xff;
#else
    (void)renderer; return 1;
#endif
}
/* ---- the render sync (the scene's render container, scene + 0x1a18) ----
 * Two object lists of 0x20-byte records {+0x00 world centre (vec3), +0x10 the entity, +0x14 radius, +0x18 a value}: +0x10b4 (count
 * +0x10b8) the moving objects (vehicles, riders, animated and physics entities), +0x10c8 (count +0x10cc) the terrain and the always
 * synced ones. An entity's renderable is its vf25 (+0x64: entity + 0x90, the vehicles' + 0x94). Gameplay reads what the sync writes:
 * the vehicle model's render copy and its bounds (the AI's BehaviorSeperate target pick, 00531a70 -> 00533510 -> 004302f0), the
 * vehicle's vf61 (its simulation's 0044aec0 on two frames of three), the animated entities' posed node sets and dirty flags. The
 * record's world centre / radius and the culling grid moves (007d9f00 / 007d9fc0, applied by 007da080) feed only the renderer's
 * culling and lights (007e0940, 007e1130): the oracle build runs them, the product does not. */

/* a renderable's virtual (thiscall, integer arguments): the oracle runs it in the guest (an installed port through its adapter),
 * the product calls the registered core function */
static uint32_t rs_vcall(rx_addr obj, uint32_t off, int n, const uint32_t* a) {
#ifdef RX_ORACLE
    return rx_call_original(loop_vslot(obj, off), RX_THISCALL, obj, n, a);
#else
    return rx_vcall(obj, off, n, a);
#endif
}
#ifdef RX_ORACLE
/* 007d9f00 / 007d9fc0 (ESI = grid, EDI = the record (its old centre); stack key, old radius, &centre, radius, value; RET 0x14): a
 * move queued on the culling grid (by entity / by the entity's vf23 id). Render only: oracle. */
static void rs_grid_queue(rx_addr fn, rx_addr grid, rx_addr rec, uint32_t key, uint32_t old_r, rx_addr centre, uint32_t r, uint32_t v) {
    rx_regs in = loop_regs();
    in.r[LOOP_ESI] = grid; in.r[LOOP_EDI] = rec;
    const uint32_t a[5] = {key, old_r, centre, r, v};
    rx_call_original_ex(fn, &in, 0, 5, a);
}
#endif

/* 007e0c50 Render_SyncWorkerJob (cdecl container; the four render-sync workers' body): takes the +0x10b4 records by the shared
 * counter +0x11d0 (InterlockedIncrement), from the last to the first, while below the count +0x11d4. Per record: the entity's
 * renderable (vf25); when it says it changed (vf13: the vehicles and model entities always, the animated ones a dirty flag they
 * clear) its sync (vf14: the vehicle's vf61 and its model's node set copied over the render copy, 004302c0; the animated entities'
 * node sets), its bounds (vf6: centre, extents, radius into the frame) and the centre to world space by its matrix (vf11). The
 * radius kept is the larger of the new one and the record's. Then (oracle) the grid move and the record's new centre / radius.
 * Frame: push ebp; and esp, -16; sub esp, 0x34; push ebx / esi / edi: S = A - 0x40; locals [S + 0x14] the radius kept, +0x18 the
 * bounds' radius, +0x1c the record's offset, +0x20 the centre (vec4), +0x30 the extents. */
void rx_render_sync_job(rx_addr c) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4) & ~15u, S = A - 0x40;
    if (U32(c, 0x11d4) == 0) return;
    rx_set_sp(S);
    for (rx_addr ret = 0x007e0c79;; ret = 0x007e0e77) {
        SU32(S, -4, c + 0x11d0); SU32(S, -8, ret);                    /* InterlockedIncrement(&+0x11d0): its push and return */
        SU32(c, 0x11d0, U32(c, 0x11d0) + 1u);
        const int32_t i = I32(c, 0x11d0) - 1, n = I32(c, 0x11d4);
        if (i >= n || i < 0) break;
        const uint32_t off = (uint32_t)(n - i - 1) << 5;
        SU32(S, 0x1c, off);
        const rx_addr r = rs_vcall(U32(U32(c, 0x10b4) + off, 0x10), 0x64, 0, 0);      /* the entity's vf25: its renderable */
        if (rs_vcall(r, 0x34, 0, 0) == 0) continue;                                    /* vf13: changed */
        rs_vcall(r, 0x38, 0, 0);                                                       /* vf14: the sync */
        { const uint32_t a[3] = {S + 0x20, S + 0x30, S + 0x18}; rs_vcall(r, 0x18, 3, a); }   /* vf6: the bounds */
        const rx_addr m = rs_vcall(r, 0x2c, 0, 0);                                     /* vf11: the matrix */
        const float x = F32(S, 0x20), y = F32(S, 0x24), z = F32(S, 0x28);
        SF32(S, 0x20, ((F32(m, 0x20) * z + F32(m, 0x10) * y) + F32(m, 0) * x) + F32(m, 0x30));
        SF32(S, 0x24, ((F32(m, 0x24) * z + F32(m, 4) * x) + F32(m, 0x14) * y) + F32(m, 0x34));
        SF32(S, 0x28, ((F32(m, 0x28) * z + F32(m, 8) * x) + F32(m, 0x18) * y) + F32(m, 0x38));
        const rx_addr rec = U32(c, 0x10b4) + off;
        SF32(S, 0x14, F32(S, 0x18) >= F32(rec, 0x14) ? F32(S, 0x18) : F32(rec, 0x14));   /* comiss; jb: NaN keeps the record's */
#ifdef RX_ORACLE
        const rx_addr e = U32(rec, 0x10);
        if (rs_vcall(e, 0x5c, 0, 0) != 0)                                              /* vf23: the entity's grid id */
            rs_grid_queue(0x007d9fc0, U32(c, 0x10f0), rec, rs_vcall(e, 0x5c, 0, 0), U32(rec, 0x14), S + 0x20, U32(S, 0x14), U32(rec, 0x18));
        else
            rs_grid_queue(0x007d9f00, U32(c, 0x10f0), rec, e, U32(rec, 0x14), S + 0x20, U32(S, 0x14), U32(rec, 0x18));
        const rx_addr rec2 = U32(c, 0x10b4) + off;
        SU32(rec2, 0, U32(S, 0x20)); SU32(rec2, 4, U32(S, 0x24)); SU32(rec2, 8, U32(S, 0x28));
        SU32(rec2, 0x14, U32(S, 0x14));
#endif
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x007e0c50, rx_render_sync_job)
static void render_sync_worker_body(rx_addr c) { rx_render_sync_job(c); }
#ifndef RX_ORACLE
/* w-jobthreads (loop_jobs.h): the four workers as their own threads, in parallel. Which records a worker takes from the shared
 * counter is a race in the original; here worker k takes the k-th of every four, from the last record to the first, so a record's
 * work runs on the same worker stack in every run. Each pass is Render_SyncWorkerJob's loop body above (its frame, its locals);
 * the counter +0x11d0 is not touched (the caller closes the job with 0x7fffffff, as after the in-place run). The product's record
 * work (vf25 / vf13 / vf14 / vf6 / vf11 of distinct entities) writes the entity's own render state and the worker's frame. */
static rx_addr g_rs_container;   /* the batch's container (set before the batch, read only during it) */
static void render_sync_part_body(rx_addr k) {
    const rx_addr c = g_rs_container;
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4) & ~15u, S = A - 0x40;
    if (U32(c, 0x11d4) == 0) return;
    rx_set_sp(S);
    const int32_t n = I32(c, 0x11d4);
    for (int32_t i = (int32_t)k; i < n; i += 4) {
        SU32(S, -4, c + 0x11d0); SU32(S, -8, i == (int32_t)k ? 0x007e0c79u : 0x007e0e77u);   /* the InterlockedIncrement call's push and return */
        const uint32_t off = (uint32_t)(n - i - 1) << 5;
        SU32(S, 0x1c, off);
        const rx_addr r = rs_vcall(U32(U32(c, 0x10b4) + off, 0x10), 0x64, 0, 0);      /* the entity's vf25: its renderable */
        if (rs_vcall(r, 0x34, 0, 0) == 0) continue;                                    /* vf13: changed */
        rs_vcall(r, 0x38, 0, 0);                                                       /* vf14: the sync */
        { const uint32_t a[3] = {S + 0x20, S + 0x30, S + 0x18}; rs_vcall(r, 0x18, 3, a); }   /* vf6: the bounds */
        const rx_addr m = rs_vcall(r, 0x2c, 0, 0);                                     /* vf11: the matrix */
        const float x = F32(S, 0x20), y = F32(S, 0x24), z = F32(S, 0x28);
        SF32(S, 0x20, ((F32(m, 0x20) * z + F32(m, 0x10) * y) + F32(m, 0) * x) + F32(m, 0x30));
        SF32(S, 0x24, ((F32(m, 0x24) * z + F32(m, 4) * x) + F32(m, 0x14) * y) + F32(m, 0x34));
        SF32(S, 0x28, ((F32(m, 0x28) * z + F32(m, 8) * x) + F32(m, 0x18) * y) + F32(m, 0x38));
        const rx_addr rec = U32(c, 0x10b4) + off;
        SF32(S, 0x14, F32(S, 0x18) >= F32(rec, 0x14) ? F32(S, 0x18) : F32(rec, 0x14));   /* comiss; jb: NaN keeps the record's */
    }
    rx_set_sp(sp0);
}
#endif

/* 007e0fc0 Render_SyncRenderData (thiscall container, the engine's update counter; RET 4): under the container's two locks (+0x1114,
 * +0x112c) and when it holds anything (+0x10b8 + +0x10cc + +0x10e0, also printed into the "SyncRenderData %d" profile label): the
 * frame kept (+0x11e0), the counter / count of the job (+0x11d0 = 0, +0x11d4 = +0x10b8); every +0x10c8 entity's renderable synced
 * (vf25, vf14: the terrain surface's 007c4fa0, the always-animated entities); then, with work for them, the four render-sync workers
 * woken (each: ResetEvent done +0x1170 + 4 k, SetEvent go +0x1180 + 4 k; they run Render_SyncWorkerJob 007e0c50) and waited for, the
 * job closed (+0x11d0 = 0x7fffffff, +0x11d4 = 0); the culling grid's queued moves applied (007da080, EAX = grid; render: oracle).
 * Frame: sub esp, 0x10c (cookie at +0x108); push ebx / ebp / esi / edi: E = sp0 - 0x11c, the locks at [E + 0x14] / [E + 0x10],
 * the label at E + 0x18. */
void rx_render_sync_data(rx_addr c, uint32_t frame) {
    const rx_addr sp0 = rx_sp(), E0 = sp0 - 0x10c, E = E0 - 0x10;
    SU32(E0, 0x108, U32(0, 0x009b45e4) ^ E0);                          /* the security cookie */
    rx_set_sp(E);
    SU32(E, 0x14, c + 0x1114);
    loop_enter_cs(c + 0x1114);
    SU32(E, 0x10, c + 0x112c);
    loop_enter_cs(c + 0x112c);
    const uint32_t n10 = U32(c, 0x10cc), total = U32(c, 0x10e0) + n10 + U32(c, 0x10b8);
    {   /* sprintf(label, "SyncRenderData %d", total) (IAT 0x916204): the arguments where the original pushes them */
        SU32(E, -4, total); SU32(E, -8, 0x009720f8); SU32(E, -0xc, E + 0x18);
#ifdef RX_ORACLE
        const uint32_t sa[3] = {E + 0x18, 0x009720f8, total};
        rx_call_original(U32(0, 0x00916204), RX_CDECL, 0, 3, sa);
#else
        rx_set_sp(E - 0xc); crt_sprintf(E + 0x18, 0x009720f8, E - 4); rx_set_sp(E);
#endif
    }
    if (total != 0) {
        SU32(c, 0x11d0, 0);
        SU32(c, 0x11d4, U32(c, 0x10b8));
        SU32(c, 0x11e0, frame);
        for (uint32_t k = 0; (int32_t)k < (int32_t)n10; k++) {
            const rx_addr r = rs_vcall(U32(U32(c, 0x10c8) + 0x20 * k, 0x10), 0x64, 0, 0);   /* vf25: the renderable */
#ifndef RX_ORACLE
            /* RENDER BOUNDARY: the terrain surface's vf14 (HFTerrainSurface::vf14 007c4fa0: the deformation readback and the track
             * flushes) is the session's render hook in the product (rx_session.c frame_render_run, later in the same frame; nothing
             * between reads what it writes) */
            if (loop_vslot(r, 0x38) == 0x007c4fa0u) continue;
#endif
            rs_vcall(r, 0x38, 0, 0);                                                         /* vf14: the sync */
        }
        if (U32(c, 0x11dc) + U32(c, 0x11d4) != 0) {
#ifndef RX_ORACLE
            if (rx_jobs_on(RX_JOB_RSYNC)) {   /* w-jobthreads: the four workers as their own threads (above) */
                rx_job jobs[4];
                g_rs_container = c;
                for (uint32_t k = 0; k < 4; k++) {
                    loop_reset_event(U32(c, 0x1170 + 4 * k));
                    jobs[k].body = render_sync_part_body; jobs[k].arg = k; jobs[k].ctx = RX_JOB_CTX_RSYNC + (int)k;
                    jobs[k].tls = loop_worker_tls(U32(c, 0x1180 + 4 * k));
                }
                rx_jobs_run(jobs, 4);
                if (U32(c, 0x11d4) != 0) SU32(c, 0x11d0, U32(c, 0x11d4) + 4u);   /* the counter as the in-place run leaves it */
            } else
#endif
            for (uint32_t k = 0; k < 4; k++) {
                loop_reset_event(U32(c, 0x1170 + 4 * k));
                loop_wake_worker(U32(c, 0x1180 + 4 * k), render_sync_worker_body, c);   /* Render_SyncWorkerThread 007df370 */
            }
            for (uint32_t k = 0; k < 4; k++) loop_wait_event(U32(c, 0x1170 + 4 * k));
            SU32(c, 0x11d0, 0x7fffffffu);
            SU32(c, 0x11d4, 0);
        }
#ifdef RX_ORACLE
        { rx_regs in = loop_regs(); in.r[LOOP_EAX] = U32(c, 0x10f0); rx_call_original_ex(0x007da080, &in, 0, 0, 0); }
#endif
    }
    loop_leave_cs(c + 0x112c);
    loop_leave_cs(c + 0x1114);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x007e0fc0, rx_render_sync_data)

static void render_sync(rx_addr scene, uint32_t frame) {   /* 007e0fc0 Render_SyncRenderData of the scene's render container */
    const uint32_t v[1] = {frame};
#ifdef RX_ORACLE
    rx_call_original(0x007e0fc0, RX_THISCALL, scene + 0x1a18, 1, v);
#else
    const rx_addr s = loop_push(1, v, 0x0084ef88); rx_render_sync_data(scene + 0x1a18, frame); loop_pop(s);
#endif
}

/* 0084ed20 Engine::Update (engine; ret 4) -> 1, or 0 when the game's update says stop. The frame: timing, the input devices
 * (context slot 2 -> XenonInputManager::vf43) and key maps (00d92f50), the scenes' pre-tick (007d21d0) and their entity
 * tickers woken, MXGame::Update (game slot 6), the audio frame command (0x17, dt), SceneManager_Tick per scene, the job pool
 * drained, the tickers waited for, the scenes' post-tick (007d2490), the engine task thread run; then per scene (last first) the
 * render sync and the render cameras: each viewport camera of the scene (+0x19b8, count +0x19d0) copied (0x84 bytes) into the
 * scene's render block (00d70f20 - 0x1990 k, records of 0x440 bytes: the Camera built by 006a5b30 at +0x90, +0x00 -> it, +0x14 = 1,
 * +0x270 = dt; the render camera of game-camera.md lives here); the task thread waited for and the context's service updated. */
uint32_t rx_engine_update(rx_addr engine) {
    /* the original's frame: 8-aligned below the saved EBP, the dt local (the audio command's argument) at A - 4 */
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4) & ~7u, F = A - 4;
    rx_set_sp(A - 0x20);
    rx_engine_update_timing(engine);
    const rx_addr im = loop_input_manager();
    loop_tc_f(loop_vslot(im, 0xac), im, F32(engine, 0x18));
    SF32(F, 0, F32(engine, 0x18));
    if ((U32(0, 0x00d92f48) & 1) == 0) {
        SU32(0, 0x00d92f48, U32(0, 0x00d92f48) | 1);
        { const rx_addr s0 = loop_push(0, 0, 0); rx_input_names_init(); loop_pop(s0); }    /* InputNames_Init */
        RX_ATEXIT(0x009127c0);    /* _atexit */
    }
    rx_keymaps_update(0x00d92f50, F32(F, 0));
    for (uint32_t s = 0x1c; s < 0x28; s += 4) {
        const rx_addr scene = U32(loop_ctx(), s);
        if (U32(scene, 0x34) != 0) scene_pre_tick(scene);
    }
    for (uint32_t s = 0x1c; s < 0x28; s += 4) {
        const rx_addr scene = U32(loop_ctx(), s);
        if (U32(scene, 0x34) != 0 && U32(scene, 0x2cac) == 0) {
            SU32(scene, 0x2cac, 1);
            loop_wake_worker(U32(scene, 0x2ca4), scene_ticker_body, scene);
        }
    }
    const rx_addr game = U32(engine, 8);
#ifdef RX_ORACLE
    /* MXGame::Update is the game unit's (core/game/build_activity_game.c): the guest runs it, so its ports (when the filter
     * installs them) and its own cross-unit calls keep the original's behavior; a direct C call here would run another unit's
     * C code unbound (the fx register model, the build activity frame) */
    if (rx_call_original(0x00757fb0, RX_THISCALL, game, 0, 0) == 0) { rx_set_sp(sp0); return 0; }
#else
    if (loop_tc0(loop_vslot(game, 0x18), game) == 0) {                               /* MXGame::Update */
        if (g_pool_defer) { g_pool_defer = 0; jobpool_wake_idle(jobpool(0x0084ee86)); }   /* no wait follows: the deferred jobs run */
        rx_set_sp(sp0); return 0;
    }
#endif
    SF32(F, 0, F32(engine, 0x18));
    if (U32(0, 0x009b7430) == 0) {                   /* Audio_PostCommand(0x17, &dt, 0, 0, 0) (rx_world) */
        const uint32_t a[5] = {0x17, F, 0, 0, 0};
        const rx_addr s = loop_push(5, a, 0x0084ee3d); rx_world_audio_post_command(0x17, F, 0, 0, 0); loop_pop(s);
    }
    for (uint32_t s = 0x1c; s < 0x28; s += 4) {
        const rx_addr scene = U32(loop_ctx(), s);
#ifndef RX_ORACLE
        /* the UI scene's first ticker is the UI manager (UIManager::vf19 005db820): the page that starts in its update and changes game
         * state, the results page's PostEventResults:Start -> App.PopulateResultsTable (its LCG draws and projected times), runs here in
         * the product (race_results_table.c, w-resultsrng; the scene's other ticker 008234f0 draws nothing). The original's UI scene is
         * active during an event; the product's is not (no UI), so its place in the loop is taken whatever its flag. */
        if (s == 0x24) { rx_results_page_frame(); rx_ui_suspend_input_frame(); }   /* + MXUIManager::vf44's SuspendPlayerInput loop */
#endif
        if (U32(scene, 0x34) != 0) stdcall1(0x007d23a0, scene, 0x0084ee64);   /* SceneManager_Tick (rx_anim) */
    }
    rx_jobpool_wait_idle(jobpool(0x0084ee86));
    for (uint32_t s = 0x1c; s < 0x28; s += 4) {
        const rx_addr scene = U32(loop_ctx(), s);
        if (U32(scene, 0x2cac) != 0) {
            SU32(scene, 0x2cac, 0);
            loop_wait_event(U32(scene, 0x2ca8));
        }
    }
    for (uint32_t s = 0x1c; s < 0x28; s += 4) {
        const rx_addr scene = U32(loop_ctx(), s);
        if (U32(scene, 0x34) != 0) stdcall1(0x007d2490, scene, 0x0084eee8);   /* scene post-tick (rx_anim) */
    }
    SU32(0, 0x00d6db00, 0);
    rx_jobpool_wait_idle(jobpool(0x0084ef10));
    loop_wake_worker(U32(engine, 0x90), engine_tasks_body, engine);
    rx_addr block = 0x00d70f20;
    for (uint32_t s = 0x24; block >= 0x00d6dc00; s -= 4, block -= 0x1990) {
        const rx_addr scene = U32(loop_ctx(), s);
        if (U32(scene, 0x34) != 0) {
            const uint32_t frame = U32(engine, 0x58);
            if (renderer_ready(U32(loop_ctx(), 0x14))) {
                stdcall1(0x007d1cd0, 0x00971fa0, 0x0084ef7c);   /* Scene_ProfileScope("Octree") */
                render_sync(scene, frame);
                stdcall1(0x007d1cd0, 0x00971fa0, 0x0084ef92);
            }
        }
        SU32(block, -0xf0, U32(scene, 0x34));
        const uint32_t n = U32(scene, 0x19d0);
        SU32(block, -0xec, n);
        for (uint32_t j = 0; j < n; j++) {
            const rx_addr rec = block + 0x440 * j, cam = U32(scene, 0x19b8 + 4 * j);
            for (uint32_t k = 0; k < 0x84; k += 4) SU32(rec, k, U32(cam, k));
            const uint32_t a[2] = {rec + 0x90, U32(cam, 0)};
            const rx_addr s = loop_push(2, a, 0x0084efde); rx_state_record_assign(rec + 0x90, U32(cam, 0)); loop_pop(s);
            SU32(rec, 0, rec + 0x90);
            SU32(rec, 0x14, 1);
            SU32(rec, 0x270, U32(engine, 0x18));
        }
    }
    loop_wait_event(U32(engine, 0x94));
    const rx_addr svc = U32(loop_ctx(), 0xc);
    loop_tc0(loop_vslot(svc, 0xc), svc);
    rx_set_sp(sp0);
    return 1;
}
RX_FUNCTION(0x0084ed20, rx_engine_update)

/* ---- Engine::Frame ---- */

/* The render of a frame (Engine::Render 0084f060, stdcall engine). RENDER BOUNDARY: the product's renderer is Unity, which draws
 * after rx_session_advance from the exported state; what the original's render pass does to game state that the core keeps (the
 * render-thread work the session runs in its place: terrain readback / tracks flush, the drawn-frame stamps) is the session's render
 * hook, called exactly where the render runs. The oracle build runs the original. */
static void (*g_render_hook)(void* ctx) = NULL;
static void* g_render_ctx = NULL;
void rx_engine_set_render_hook(void (*fn)(void* ctx), void* ctx) { g_render_hook = fn; g_render_ctx = ctx; }
static void engine_render(rx_addr engine) {
#ifdef RX_ORACLE
    const uint32_t a[1] = {engine};
    rx_call_original(0x0084f060, RX_STDCALL, 0, 1, a);
#else
    (void)engine;
    if (g_render_hook) g_render_hook(g_render_ctx);
#endif
}

/* 0084f260 Engine::Frame (EAX = engine) -> Engine::Update's result. Device not ready (Renderer_EnsureDeviceReady 007ae4e0 of the
 * renderer [ctx + 0x14]): only UpdateTiming, 1. Else the two frame events (+0xd8 / +0xdc, the device-reset handshake with the window
 * thread) waited for, Engine::Update, the update counter +0x58 + 1, the render (when the device is still ready), the Database's
 * end-of-frame removal vf33 (+0x84) with the render counter +0x54 -- the pending list the frame's Update queued into (vf22 with +0x58
 * before the increment) -- then +0x54 + 1 and the two events set again (GetLastError when SetEvent fails). Between frames +0x54 ==
 * +0x58 == the number of rendered frames since the engine's init (0084e170 clears both). Frame: push esi; push ebx / ebp / edi.
 * Not adapted in the oracle: the boot's frame-stop native owns 0084f260 (the main thread parks there between frames). */
uint32_t rx_engine_frame(rx_addr engine) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);                                                   /* push esi */
    if (!renderer_ready(U32(loop_ctx(), 0x14))) {
        { const rx_addr s = loop_push(0, 0, 0x0084f27a); rx_engine_update_timing(engine); loop_pop(s); }
        rx_set_sp(sp0);
        return 1;
    }
    rx_set_sp(sp0 - 0x10);                                                /* push ebx / ebp / edi */
    loop_wait_event(U32(engine, 0xd8));
    loop_wait_event(U32(engine, 0xdc));
    uint32_t r;
    { const uint32_t a[1] = {engine}; const rx_addr s = loop_push(1, a, 0x0084f2a6); r = rx_engine_update(engine); loop_pop(s); }
    SU32(engine, 0x58, U32(engine, 0x58) + 1u);
    if (renderer_ready(U32(loop_ctx(), 0x14))) {
        const uint32_t a[1] = {engine}; const rx_addr s = loop_push(1, a, 0x0084f2c8); engine_render(engine); loop_pop(s);
    }
    {
        const rx_addr db = U32(loop_ctx(), 8);
        const uint32_t frame = U32(engine, 0x54);
        rx_vcall(db, 0x84, 1, &frame);                                    /* Database::vf33 0084cc80 */
    }
    SU32(engine, 0x54, U32(engine, 0x54) + 1u);
    if (loop_set_event(U32(engine, 0xd8)) == 0) loop_get_last_error();
    if (loop_set_event(U32(engine, 0xdc)) == 0) loop_get_last_error();
    rx_set_sp(sp0);
    return r;
}
RX_FUNCTION(0x0084f260, rx_engine_frame)

/* RENDER BOUNDARY (product): the drawn-frame stamp of the character entities. The original's render pass draws a character entity
 * through its renderable's vf2 (CharacterEntity::vf2_o90 00839b60, the +0x90 sub-object of 15 classes: Model_SubmitSkinned, then the
 * sub-object's +0x2b0 = the render counter [engine + 0x54]); AnimEntitySingleAnimLoop_Job 006ba170 samples its clip only while that
 * stamp + 5 >= the counter (the off-screen LOD). Unity draws the product's frame from the exported state with its own culling, which
 * the core does not see, so every character entity of the active scenes' in-use buckets (+0x38 + 0x110 b: +0x104 entities, +0x108
 * count, +0x10c in use) counts as drawn in each frame's render: the stamp the original's vf2_o90 writes for an entity on screen.
 * (Before the frame tail the counters stayed 0 and the test always passed: the same result.) */
static const uint32_t k_character_renderable_vt[] = {      /* the +0x90 vtables whose slot 2 is 00839b60 (_re/out/vtables.tsv) */
    0x0095f49cu /* ThirtySecondBoard */, 0x0095f62cu /* AnimEntityProximity */, 0x0095f6f4u /* AnimEntitySingleAnimLoop */,
    0x0095f7bcu /* AnimEntitySingleAnimLoopWithParticles */, 0x0095f8bcu /* AnimEntitySplineAnimLoop */,
    0x0095f984u /* AnimEntitySplineAnimLoopWithParticles */, 0x0095fa4cu /* AnimEntitySplineWithStopLogic */,
    0x0095fb14u /* AnimEntitySplineWithStopLogicWithParticles */, 0x0095fbdcu /* AnimEntityTrigger */,
    0x00960114u /* ScoreTowerFlagHolder */, 0x00960294u /* TrackSideFlagHolder */, 0x00960cecu /* AirAnimalSoaring */,
    0x00960e6cu /* LandAnimalProximity */, 0x0096cf64u /* ScenarioAnimEntity */, 0x009761b4u /* CharacterEntity */,
};
uint32_t rx_engine_render_stamps(rx_addr engine) {
    const uint32_t frame = U32(engine, 0x54);
    uint32_t stamped = 0;
    for (uint32_t s = 0x1c; s < 0x28; s += 4) {
        const rx_addr scene = U32(loop_ctx(), s);
        if (scene == 0 || U32(scene, 0x34) == 0) continue;
        for (uint32_t b = 0; b < 0x18; b++) {
            const rx_addr bucket = scene + 0x38 + 0x110 * b;
            if (U32(bucket, 0x10c) == 0) continue;
            const rx_addr arr = U32(bucket, 0x104);
            for (int32_t i = 0; i < I32(bucket, 0x108); i++) {
                const rx_addr e = U32(arr, 4u * (uint32_t)i);
                if (e == 0) continue;
                const uint32_t vt = U32(e, 0x90);
                for (size_t k = 0; k < sizeof k_character_renderable_vt / sizeof k_character_renderable_vt[0]; k++)
                    if (vt == k_character_renderable_vt[k]) { SU32(e, 0x340, frame); stamped++; break; }
            }
        }
    }
    return stamped;
}
