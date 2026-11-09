/* loop_entity.c -- the scene's animated ambient entities' updates (slot 19, called by the scene tickers): animation loops,
 * spline animations, scenario actors, crew chiefs, the score tower flags, instanced animations. Most queue their animation as a
 * job (JobPool_Submit with a {entity, dt} record) that the job pool runs at once (loop_engine.c). The job functions themselves
 * belong to the animation subsystem.
 * Entity: +0x10 the scene it lives in (the context's scene +0x20 -> the race world G + 0x5a54, +0x1c -> the front-end world
 * G + 0x20c8). (_re/names/76_ambient.tsv, native/game/notes/rest.md "Ambient updates") */
#include "crt_atexit.h"
#include "loop_entity.h"
#include "alive_gate.h"
#include "fx.h"
#include "fx_util.h"
#include "loop_engine.h"
#include "loop_platform.h"
#include "../src/rx_os.h"
#include "loop_util.h"
#include "loop_extern.h"
#include "anim.h"
#include "anim_scenario.h"
#include "world_flag_holders.h"
#include "../src/rx_x87.h"

/* the world of the entity's scene (0 when it is in neither) */
static rx_addr entity_world(rx_addr e) {
    const rx_addr scene = U32(e, 0x10);
    if (scene == U32(loop_ctx(), 0x20)) return U32(U32(0, 0x00a946b8), 0x5a54);
    if (scene == U32(loop_ctx(), 0x1c)) return U32(U32(0, 0x00a946b8), 0x20c8);
    return 0;
}
/* the world runs: not paused (+0x20014) and the engine not paused (timing +0x0c) */
static int world_running(rx_addr e) {
    return U32(entity_world(e), 0x20014) == 0 && U32(loop_timing(), 0xc) == 0;
}

/* 006925a0 JobPool_Instance -> the engine's job pool 00d94ae0 (constructed on first use) */
rx_addr rx_jobpool_instance(void) {
    if ((U32(0, 0x00d94ad8) & 1) == 0) {
        SU32(0, 0x00d94ad8, U32(0, 0x00d94ad8) | 1);
        { const rx_addr s = loop_push(0, 0, 0x006925bd); rx_jobpool_construct(0x00d94ae0); loop_pop(s); }   /* 00881a70 (ECX = pool) */
        RX_ATEXIT(0x00912b10);   /* _atexit */
    }
    return 0x00d94ae0;
}
RX_FUNCTION(0x006925a0, rx_jobpool_instance)

/* queue the entity's animation job {entity, dt}; the record sits where the original's frame has it (A - 0x40, A = the 8-aligned
 * frame base below the saved EBP: the job pool copies the whole 0x40-byte record from there) */
static void submit_entity_job(rx_addr e, float dt, rx_addr job) {
    const rx_addr sp0 = rx_sp(), rec = ((sp0 - 4) & ~7u) - 0x40;
    rx_set_sp(rec);
    SU32(rec, 0, e);
    SF32(rec, 4, dt);
    rx_jobpool_submit(rx_jobpool_instance(), rec, job);
    rx_set_sp(sp0);
}

/* 006ba250 AnimEntitySingleAnimLoop::vf19 (dt; ret 4): while its world runs, queue AnimEntitySingleAnimLoop_Job 006ba170. */
void rx_anim_entity_single_loop_update(rx_addr e, float dt) {
    if (world_running(e)) submit_entity_job(e, dt, 0x006ba170);
}
RX_FUNCTION(0x006ba250, rx_anim_entity_single_loop_update)

/* 006bb340 AnimEntitySplineAnimLoop::vf19 (dt; ret 4): while its world runs, queue its spline job 006bb010. */
void rx_anim_entity_spline_loop_update(rx_addr e, float dt) {
    if (world_running(e)) submit_entity_job(e, dt, 0x006bb010);
}
RX_FUNCTION(0x006bb340, rx_anim_entity_spline_loop_update)

/* 006bc040 AnimEntitySplineAnimLoopWithParticles::vf19 (dt; ret 4): not stopped (+0x794): the spline loop's update, then its
 * particle effect's (+0x790, slot 3). */
void rx_anim_entity_spline_particles_update(rx_addr e, float dt) {
    if (U32(e, 0x794) != 0) return;
    rx_anim_entity_spline_loop_update(e, dt);
    const rx_addr fx = U32(e, 0x790);
    if (fx != 0) loop_tc_f(loop_vslot(fx, 0xc), fx, dt);
}
RX_FUNCTION(0x006bc040, rx_anim_entity_spline_particles_update)

/* 0074ab50 ScenarioAnimEntity::vf19 (dt; ret 4): while its world runs: a pending scenario delay (+0x478 > 0) advances first
 * (0074a9f0), then its job ScenarioAnimEntity_Job 0074ab10 is queued. */
void rx_scenario_anim_entity_update(rx_addr e, float dt) {
    if (!world_running(e)) return;
    rx_alive_gate_tick(e);   /* PORT ADDITION (alive_gate.h): an Alive gate's drop clock (host statics only) */
    /* the original's frame: push ebp; and esp, -8; sub esp, 0x44; push esi */
    const rx_addr sp0 = rx_sp();
    rx_set_sp(((sp0 - 4) & ~7u) - 0x48);
    if (F32(e, 0x478) > 0.0f /* 0x92e12c */) { const uint32_t a[1] = {e}; const rx_addr s = loop_push(1, a, 0); rx_scenario_anim_entity_play(e); loop_pop(s); }
    rx_set_sp(sp0);
    submit_entity_job(e, dt, 0x0074ab10);
}
RX_FUNCTION(0x0074ab50, rx_scenario_anim_entity_update)

/* 006bd6e0 CrewChief::vf19 (dt; ret 4): while its world runs and it is not parked (+0x350), queue CrewChief_Job. */
void rx_crew_chief_update(rx_addr e, float dt) {
    if (world_running(e) && U32(e, 0x350) == 0) submit_entity_job(e, dt, 0x006bd6a0);
}
RX_FUNCTION(0x006bd6e0, rx_crew_chief_update)

/* 004137b0 InstancedAnimationEntity::vf19 (dt; ret 4): its instance's update (+0x9f0, slot 19). */
void rx_instanced_animation_update(rx_addr e, float dt) {
    const rx_addr inst = U32(e, 0x9f0);
    if (inst != 0) loop_tc_f(loop_vslot(inst, 0x4c), inst, dt);
}
RX_FUNCTION(0x004137b0, rx_instanced_animation_update)

/* 006c1a00 ScoreTowerFlagHolder::vf19 (dt; ret 4): while its world runs and it is shown (+0x360 == 0): its character's
 * keyframes sampled (Character_SampleKeyframes 00835430: stack the character +0x148, xmm1 = dt); a pending flag change
 * (+0x364 in 1..3) with a motion loaded (+0x16c) is applied (006c1a90: EAX = holder, ECX = 0). */
void rx_score_tower_flag_update(rx_addr e, float dt) {
    /* Frame: push esi (ESI = the holder): the sampler below runs at the original's depth (its frames are dead stack that a later
     * particle template copies: the 128-byte particle records' never-written word, known-differences 1) */
    const rx_addr sp0 = rx_sp();
    const uint32_t esi = fx_reg(FX_ESI);
    SU32(sp0, -4, esi);
    rx_set_sp(sp0 - 4);
    fx_set_reg(FX_ESI, e);
    if (world_running(e) && U32(e, 0x360) == 0) {
        const uint32_t a[1] = {e + 0x148};
        { const rx_addr s = loop_push(1, a, 0x006c1a60); rx_character_sample_keyframes(e + 0x148, dt); loop_pop(s); }   /* 00835430 (xmm1 = dt) */
        if (U32(e, 0x364) - 1u <= 2u && U32(e, 0x16c) != 0) {
            SU32(e, 0x364, 0);
            const rx_addr s = loop_push(0, NULL, 0x006c1a88);   /* the original's `call 006c1a90` (EAX = the tower, ECX = 0) */
            rx_score_tower_apply_state(e, 0);
            loop_pop(s);
        }
    }
    fx_set_reg(FX_ESI, esi);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006c1a00, rx_score_tower_flag_update)

/* 0088fa20 Entity::vf14 -> the entity's transform (+0x30) */
rx_addr rx_entity_transform(rx_addr e) { return e + 0x30; }
RX_FUNCTION(0x0088fa20, rx_entity_transform)

/* The scene tickers' x87 precision (control word 0x027f: 53 bits; their workers' recorded control word). */
#define TICKER_X87_BITS 53
#define TICKER_X87_CW 0x027f

/* MSVCR90 _CIfmod through the exe's thunk 009009c6 (`jmp [0x9163c4]`, no code of its own): st(1) = x, st(0) = y -> fmod(x, y) (exact), by the
 * CRT port (crt_math.c: crt_ci_fmod, 7859ac8a) under the control word of the ticker threads. */
static rx_f80 ci_fmod(rx_f80 x, rx_f80 y) {
    return crt_ci_fmod(x, y, TICKER_X87_CW);
}
/* the game's inline rand() (its own LCG: seed 009cae3c, draw counter 009cae40) */
static uint32_t crowd_rand(void) {
    const uint32_t seed = U32(0, 0x009cae3c) * 0x41c64e6du + 0x3039u;
    SU32(0, 0x009cae3c, seed);
    return (seed >> 16) & 0x7fff;
}

/* 006c6650 BillboardCrowdEntity::vf19 (dt; ret 4): the billboard crowd's cheering. While its world is not paused: the wave phase
 * +0x140 = fmod(dt x 1.5 + phase, 1); every idle billboard (records +0x17c..+0x180, 16 bytes: +0 cheering, +4 time, +8 length,
 * +0xc frame) starts cheering with chance [009c3edc] / 500 (length 0.01..0.11 s, random frame of 500); every billboard's time
 * runs, and it stops past its length. */
void rx_billboard_crowd_update(rx_addr e, float dt) {
    if (U32(entity_world(e), 0x20014) != 0) return;
    const rx_f80 p = rx_f80_add(rx_f80_mul(rx_f80_from_f32(dt), rx_f80_from_f32(1.5f /* 0x9322a4 */), TICKER_X87_BITS, 0),
                                rx_f80_from_f32(F32(e, 0x140)), TICKER_X87_BITS, 0);
    SF32(e, 0x140, rx_f80_to_f32(ci_fmod(p, rx_f80_from_f32(1.0f)), 0));
    const int32_t n = (int32_t)(U32(e, 0x180) - U32(e, 0x17c)) >> 4;
    for (int32_t i = 0; i < n; i++) {
        const rx_addr b = U32(e, 0x17c) + 16u * (uint32_t)i;
        if (U8(b, 0) != 0) continue;
        const int32_t roll = (int32_t)(crowd_rand() % 500u);
        SU32(0, 0x009cae40, U32(0, 0x009cae40) + 1);
        if (roll < 500 - I32(0, 0x009c3edc)) continue;
        SU8(b, 0, 1);
        SU32(0, 0x009cae40, U32(0, 0x009cae40) + 1);
        const float r = (float)((double)(float)crowd_rand() * 3.0517578125e-05 /* 0x9776a0 */);
        SF32(b, 8, r * 0.1f /* 0x931d94 */ + 0.01f /* 0x9322ac */);
        SF32(b, 4, 0.0f);
        const uint32_t frame = crowd_rand() % 500u;
        SU32(0, 0x009cae40, U32(0, 0x009cae40) + 1);
        SU32(b, 0xc, frame);
    }
    for (int32_t i = 0; i < n; i++) {
        const rx_addr b = U32(e, 0x17c) + 16u * (uint32_t)i;
        SF32(b, 4, F32(b, 4) + dt);
        if (F32(b, 4) > F32(b, 8)) SU8(b, 0, 0);
    }
}
RX_FUNCTION(0x006c6650, rx_billboard_crowd_update)

/* 00839890 CharacterEntityTicker construct (cdecl, no arguments) -> EAX = the ticker 0x00d9d3f0: the vtable 0x009761fc, the critical section +0x28
 * (0x00d9d418), the four auto-reset events (created in the order wake +0x10, done +0x1c, kill +0x14, idle +0x18; GetLastError when one fails) and
 * the worker thread (+0x20, id +0x24: FUN_00839710 with the ticker, created suspended, affinity mask 2, priority 2, resumed twice), the entity
 * queue counters (+0x298 / +0x29c) cleared. The system calls go through the imports (the oracle's emulated
 * ones, the product's rx_os stand-ins: one thread, so the worker never runs by itself - rx_character_ticker_run is called in place). Frame: push
 * ebx / esi / edi. */
static uint32_t ticker_os_call(uint32_t slot, rx_addr ret, int n, const uint32_t* a) {
    const uint32_t r = rx_os_import(slot, n, a);
    const rx_addr sp = rx_sp() - 4u * (uint32_t)n;
    for (int i = 0; i < n; i++) SU32(sp, 4u * (uint32_t)i, a[i]);
    SU32(sp, -4, ret);
    return r;
}
rx_addr rx_character_ticker_construct(void) {
    const rx_addr sp0 = rx_sp(), t = 0x00d9d3f0;
    const fx_saved sv = fx_save();
    SU32(sp0, -4, sv.ebx);
    SU32(sp0, -8, sv.esi);
    SU32(sp0, -0xc, sv.edi);
    rx_set_sp(sp0 - 0xc);
    SU32(t, 0, 0x009761fc);
    SU32(t, 0x10, 0);
    SU32(t, 0x14, 0);
    SU32(t, 0x18, 0);
    SU32(t, 0x1c, 0);
    SU32(t, 0x20, 0);
    { const uint32_t a[1] = {t + 0x28}; (void)ticker_os_call(0x00916014, 0x008398c8, 1, a); }        /* InitializeCriticalSection */
    static const struct { uint32_t field, ret_create, ret_error; } ev[4] = {
        {0x10, 0x008398d4, 0x008398e5}, {0x1c, 0x008398eb, 0x008398f6}, {0x14, 0x008398fc, 0x00839907}, {0x18, 0x0083990d, 0x00839918}};
    for (int k = 0; k < 4; k++) {
        const uint32_t a[4] = {0, 0, 0, 0};
        const uint32_t h = ticker_os_call(0x00916038, ev[k].ret_create, 4, a);                       /* CreateEventA(0, 0, 0, 0) */
        SU32(t, ev[k].field, h);
        if (h == 0) (void)ticker_os_call(0x00916078, ev[k].ret_error, 0, 0);                         /* GetLastError */
    }
    {   /* CreateThread(0, 0, FUN_00839710, ticker, CREATE_SUSPENDED, &thread id) */
        const uint32_t a[6] = {0, 0, 0x00839710, t, 4, t + 0x24};
        const uint32_t h = ticker_os_call(0x00916030, 0x00839931, 6, a);
        SU32(t, 0x20, h);
        const uint32_t b[2] = {h, 2};
        (void)ticker_os_call(0x00916028, 0x0083993f, 2, b);                                          /* SetThreadAffinityMask */
        const uint32_t c[2] = {U32(t, 0x20), 2};
        (void)ticker_os_call(0x00916024, 0x0083994d, 2, c);                                          /* SetThreadPriority */
        const uint32_t d[1] = {U32(t, 0x20)};
        (void)ticker_os_call(0x0091602c, 0x0083995c, 1, d);                                          /* ResumeThread */
        if (U32(t, 0x20) != 0) {
            const uint32_t e[1] = {U32(t, 0x20)};
            (void)ticker_os_call(0x0091602c, 0x00839968, 1, e);
        }
    }
    SU32(t, 0x298, 0);
    SU32(t, 0x29c, 0);
    rx_set_sp(sp0);
    return t;
}
RX_FUNCTION(0x00839890, rx_character_ticker_construct)

/* 006cfaf0 the character entity ticker (00d9d3f0: a thread, FUN_00839710, woken by +0x10; +0x40 the queued entities, +0x29c their
 * count, +0x2a0 dt), constructed on first use (00839890, guard 00d9d6c0) */
static rx_addr character_ticker(void) {
    if ((U32(0, 0x00d9d6c0) & 1) == 0) {
        SU32(0, 0x00d9d6c0, U32(0, 0x00d9d6c0) | 1);
        const rx_addr s = loop_push(0, 0, 0x006cfb0e);
        (void)rx_character_ticker_construct();                    /* 00839890 */
        loop_pop(s);
        RX_ATEXIT(0x00912c70);   /* _atexit */
    }
    return 0x00d9d3f0;
}
/* 00839710 (stdcall ticker; the character entity ticker thread's function): one wake of the thread loop (the original blocks on the
 * wake event +0x10 at the top of the loop; the pass that follows a wake): the kill event +0x14 signalled: the thread ends (the
 * ended event +0x18 is set; returns 1); else, when the idle event +0x18 is signalled or at least one entity is queued (+0x298 the
 * next, +0x29c the count; +0x40 the entities, +0x2a0 the dt, +0x2b0 a 0x20-byte record per entity of the batch), the queue drains
 * one entity at a time: the entity's virtual +0x74 (thiscall dt, record, record + 0x10, 0) for the batch, then +0x78 the same, the
 * next index advanced; a signalled idle event empties the queue. Then the done event +0x1c is set (GetLastError on failure) and
 * the thread goes back to waiting (returns 0). The flag "idle signalled" lives in the argument slot ([entry ESP + 4]). The
 * LandAnimalProximity vf29 / vf30 are the callees: NOT REACHED by the verification worlds (no land animals; the threaded
 * animals switch [0xa94324] stays 0): reviewed against the disassembly only. Frame: push ebx, ebp, esi, edi. */
static uint32_t x87_pass_bits(uint32_t b) { return ((b & 0x7fffffffu) > 0x7f800000u) ? (b | 0x400000u) : b; }
uint32_t rx_character_ticker_run(rx_addr t) {
    const rx_addr slot = rx_sp() + 4;
    SU32(slot, 0, 0);
    if (loop_wait_event_timeout(U32(t, 0x14), 0) != 0x102) {
        if (!loop_set_event(U32(t, 0x18))) loop_get_last_error();
        return 1;
    }
    int work;
    if (loop_wait_event_timeout(U32(t, 0x18), 0) != 0x102) { SU32(slot, 0, 1); work = 1; }
    else work = (int32_t)(U32(t, 0x29c) - U32(t, 0x298)) >= 1;
    if (work) {
        int32_t n = (int32_t)(U32(t, 0x29c) - U32(t, 0x298));
        while (n > 0) {
            if (n > 1) n = 1;
            for (uint32_t pass = 0; pass < 2; pass++) {
                for (int32_t i = 0; i < n; i++) {
                    const rx_addr rec = t + 0x2b0 + 0x10u * (uint32_t)i;
                    const rx_addr e = U32(t, 0x40 + 4 * (U32(t, 0x298) + (uint32_t)i));
                    const rx_addr fn = loop_vslot(e, pass ? 0x78 : 0x74);
                    const uint32_t dt = x87_pass_bits(U32(t, 0x2a0));   /* fld / fstp */
                    const uint32_t a[4] = {dt, rec, rec + 0x10, 0};
                    void (*f)(rx_addr, float, uint32_t, uint32_t, rx_addr) = RX_FN(fn, void (*)(rx_addr, float, uint32_t, uint32_t, rx_addr));
                    if (f) { const rx_addr s = loop_push(4, a, pass ? 0x00839816 : 0x008397d6); f(e, rx_bf(dt), rec, rec + 0x10, 0); loop_pop(s); }
                    else rx_call_original(fn, RX_THISCALL, e, 4, a);
                }
            }
            SU32(t, 0x298, U32(t, 0x298) + (uint32_t)n);
            n = (int32_t)(U32(t, 0x29c) - U32(t, 0x298));
        }
        if (U32(slot, 0) != 0) { SU32(t, 0x298, 0); SU32(t, 0x29c, 0); }
    }
    if (!loop_set_event(U32(t, 0x1c))) loop_get_last_error();
    return 0;
}
RX_FUNCTION(0x00839710, rx_character_ticker_run)
static void character_ticker_body(rx_addr ticker) {   /* the ticker thread's pass */
    const uint32_t a[1] = {ticker};
    const rx_addr s = loop_push(1, a, 0);
    rx_character_ticker_run(ticker);
    loop_pop(s);
}

/* 006d06a0 LandAnimalProximity::vf19 (dt; ret 4): while its world runs and it is active (+0x77c == 0): dt kept (+0x878); not
 * updated for 60 frames (+0x340 + 60 < frame +0x54): dt 0 and nothing else. Threaded animals (00a94324): queued on the character
 * entity ticker, which is woken; else its own update (slot 29: dt, +0x870, +0x874, &+0x88c). */
void rx_land_animal_update(rx_addr e, float dt) {
    if (!world_running(e) || U32(e, 0x77c) != 0) return;
    SF32(e, 0x878, dt);
    if (U32(e, 0x340) + 0x3cu < U32(loop_timing(), 0x54)) { SF32(e, 0x878, 0.0f); return; }
    if (U32(0, 0x00a94324) != 0) {
        const rx_addr t = character_ticker();
        SU32(t, 0x40 + 4 * U32(t, 0x29c), e);
        SU32(t, 0x29c, U32(t, 0x29c) + 1);
        SF32(t, 0x2a0, dt);
        loop_wake_worker(U32(t, 0x10), character_ticker_body, t);
        if (U32(0, 0x00a94324) != 0) return;
    }
    const rx_addr fn = loop_vslot(e, 0x74);
    void (*f)(rx_addr, float, uint32_t, uint32_t, rx_addr) = RX_FN(fn, void (*)(rx_addr, float, uint32_t, uint32_t, rx_addr));
    const uint32_t a[4] = {U32(e, 0x878), U32(e, 0x870), U32(e, 0x874), e + 0x88c};
    if (f) { const rx_addr s = loop_push(4, a, 0); f(e, F32(e, 0x878), U32(e, 0x870), U32(e, 0x874), e + 0x88c); loop_pop(s); } else rx_call_original(fn, RX_THISCALL, e, 4, a);
}
RX_FUNCTION(0x006d06a0, rx_land_animal_update)

/* 007e4170 WaterEntity::vf19 (dt; ret 4): a pending reload (+0x98) first (WaterEntity_Reload 007e4090, ESI = entity); the
 * water model's (+0x90, its +0x44 renderable) wave clock gets dt in this frame's half of its double buffer (+0x490 + 4 x (frame &
 * 1), when +0x4a8 is set), with the model locked (+0x4c) meanwhile. */
void rx_water_entity_update(rx_addr e, float dt) {
    /* BOUNDARY (not reached): WaterEntity_Reload runs for a pending reload (+0x98), set only by the asset hot-reload of a water model; the product loads its assets once (0 of 106 bytes executed in 1700 frames of the original's reference world). */
    if (U32(e, 0x98) != 0) { rx_regs in = loop_regs(); in.r[LOOP_ESI] = e; loop_ex(0x007e4090, in, 0, 0); }
    const rx_addr model = U32(e, 0x90);
    SU32(model, 0x4c, 1);
    const rx_addr r = U32(model, 0x44);
    if (U32(r, 0x4a8) != 0) SF32(r, 0x490 + 4 * (U32(loop_timing(), 0x58) & 1), dt);
    SU32(U32(e, 0x90), 0x4c, 0);
}
RX_FUNCTION(0x007e4170, rx_water_entity_update)

/* 00662dd0 ForestSystem::vf19 (dt; ret 4): while its world is not paused, the developer keys (unless 009b716d) toggle the
 * forest's debug switches +0x31c4 / +0x31c8 / +0x31cc (key maps 00d60688, 00d5db20, 00d5fc08: KeyMap::vf1(7, 0) pressed); with
 * +0x31cc on, the forest's model (+0x9c) is locked and released (+0x4c). */
void rx_forest_update(rx_addr e, float dt) {
    (void)dt;
    if (U32(entity_world(e), 0x20014) != 0) return;
    static const rx_addr keys[3] = {0x00d60688, 0x00d5db20, 0x00d5fc08};
    for (uint32_t k = 0; k < 3; k++) {
        if (U8(0, 0x009b716d) != 0) break;
        if (loop_tc2(0x00811420, keys[k], 7, 0) != 0) SU32(e, 0x31c4 + 4 * k, U32(e, 0x31c4 + 4 * k) == 0);
    }
    if (U32(e, 0x31cc) != 0) {
        SU32(U32(e, 0x9c), 0x4c, 1);
        SU32(U32(e, 0x9c), 0x4c, 0);
    }
}
RX_FUNCTION(0x00662dd0, rx_forest_update)

/* 007c9110 HFTerrainEntity::vf25 -> its terrain (+0x90), bound first when it is pending (+0x4590): the terrain's slot 15 with the
 * height field (+0x158), its scene (+0x4538 = +0x10) and owner (+0x174) set. 007c90a0 HFTerrainEntity::vf19 (dt; ret 4) binds the
 * same way. */
static void terrain_bind(rx_addr e) {
    if (U32(e, 0x4590) == 0) return;
    loop_tc1(loop_vslot(e + 0x90, 0x3c), e + 0x90, U32(e, 0x158));
    SU32(e, 0x4538, U32(e, 0x10));
    SU32(e, 0x174, e);
    SU32(e, 0x4590, 0);
}
void rx_terrain_entity_update(rx_addr e, float dt) { (void)dt; terrain_bind(e); }
RX_FUNCTION(0x007c90a0, rx_terrain_entity_update)
rx_addr rx_terrain_entity_terrain(rx_addr e) { terrain_bind(e); return e + 0x90; }
RX_FUNCTION(0x007c9110, rx_terrain_entity_terrain)

/* 006bd6a0 CrewChief_Job (stdcall: the job record {the crew chief, dt}; RET 0): under the crew chief's lock (+0x358) its
 * character's keyframes sampled (+0x148; xmm1 = the record's dt). Frame: push esi; push edi. */
void rx_crew_chief_job(rx_addr job) {
    const rx_addr entry0 = rx_sp(), E = entry0 - 8u;
    rx_set_sp(E);
    const rx_addr e = U32(job, 0);
    const float dt = F32(job, 4);
    loop_enter_cs(e + 0x358);
    { const uint32_t a[1] = {e + 0x148}; const rx_addr s = rx_sp(); rx_set_sp(s - 8u);
      SU32(s - 4, 0, a[0]); rx_character_sample_keyframes(e + 0x148, dt); rx_set_sp(s); }
    loop_leave_cs(e + 0x358);
    rx_set_sp(entry0);
}
RX_FUNCTION(0x006bd6a0, rx_crew_chief_job)
