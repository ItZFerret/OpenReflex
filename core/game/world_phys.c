/* world_phys.c -- the physics world's frame update (0x00769xxx-0x0076dxxx): listeners, the solver dispatch around the vehicle
 * simulations, and the collisions of the world's generic dynamic objects, kinematic bodies and rider bodies.
 *
 * The world (physics context), as far as this code uses it (core/notes/world.md has the full table):
 *   +0x0000  contact events, 100 x 64 bytes (+0 id A, +4 id B, +0x10 point, +0x20 byte type, +0x24 / +0x28 displacement of
 *            A / B, +0x2c output, +0x30 / +0x34 feature ids); +0x1900 the event count
 *   +0x1914  physics listeners (count +0x1918; vfunc 0 before the step, vfunc 1 after, float dt)
 *   +0x1934  contact handlers (vector end +0x1938; vfunc 0 (world)), +0x1940 simulations (count +0x1944; vfunc 1 (dt))
 *   +0x1960  generic dynamic objects (0xe0-byte records, end +0x1964), +0x1980 rider bodies (0x80 bytes, end +0x1984),
 *   +0x19a0  keyed objects (0xe0 bytes, end +0x19a4), +0x19c0 static objects (64 bytes, end +0x19c4)
 *   +0x196c / +0x19ac / +0x19cc broad-phase cursors (generic / keyed / static)
 *   +0x19d4  kinematic body count, +0x19d8 their objects; poses at +0x1d00 + 0x50 k (position, velocity, +0x20 accumulator)
 *   +0xb300  40 x 5 contact-sound timers (0x20 apart), +0xcc00 the contact-handler list, +0xcc9c the terrain */
#include "phys_misc.h"
#include "phys_buoyancy.h"
#include "world.h"
#include "../src/rx_os.h"
#include "crt.h"
#include "world_util.h"
#include "world_veh.h"
#include "world_wreck.h"
#include "world_misc.h"
#include "world_extern.h"
#include "sound_state.h"
#include "loop_jobs.h"
#include <stdlib.h>

/* 00769b80 World_PhysicsUpdate (EAX = world, float dt; RET 4): pre-listeners, the simulation clock (0042ef70, math), the solver
 * dispatch, post-listeners. dt reaches each through fld / fstp (a signalling NaN comes out quiet). */
void rx_world_physics_update(rx_addr world, float dt) {
    world_frame f = world_frame_enter(8, 2, 1);   /* push ebp; and esp, -16; sub esp, 8; push esi; push edi */
    const float dt_pass = rx_bf(world_x87_pass(rx_fb(dt)));
    WORLD_CALL_SP(4, rx_world_pre_listeners(world, dt_pass));
    WORLD_CALL_SP(4, phys_simulation_clock_advance(dt_pass));   /* SimulationClock_Advance */
    WORLD_CALL_SP(4, rx_world_solver_dispatch(world, dt_pass));
    WORLD_CALL_SP(4, rx_world_post_listeners(world, dt_pass));
    world_frame_leave(&f);
}
RX_FUNCTION(0x00769b80, rx_world_physics_update)

/* Every physics listener's vfunc `slot` (thiscall, float dt); the count is re-read each iteration. */
static void call_listeners(rx_addr world, uint32_t slot, float dt) {
    for (int32_t i = 0; i < I32(world, 0x1918); i++) {
        rx_addr listener = U32(U32(world, 0x1914), 4u * (uint32_t)i);
        uint32_t arg = world_x87_pass(rx_fb(dt));
#ifndef RX_ORACLE   /* the product has no original: the listener's registered function (void (this, float dt)) at that address */
        void (*f)(rx_addr, float) = RX_FN(U32(U32(listener, 0), slot), void (*)(rx_addr, float));
        if (f) { const rx_addr sp = rx_sp(); rx_set_sp(sp - 8u); f(listener, rx_bf(arg)); rx_set_sp(sp); continue; }
#endif
        rx_call_original(U32(U32(listener, 0), slot), RX_THISCALL, listener, 1, &arg);
    }
}

/* 007699e0 World_PreListeners (EDI = world, float dt; RET 4): listener vfunc 0, the contact event count reset, the 200 contact
 * sound timers (+0xb300: 40 rows of 5, 0x20 apart) advanced by dt while below 1, then the contact-handler list (+0xcc00). */
void rx_world_pre_listeners(rx_addr world, float dt) {
    world_frame f = world_frame_flat(2, 0);   /* push ecx; push esi */
    call_listeners(world, 0, dt);
    SU32(world, 0x1900, 0);
    for (uint32_t row = 0; row < 40; row++)
        for (uint32_t k = 0; k < 5; k++) {
            rx_addr timer = world + 0xb300 + 0xa0 * row + 0x20 * k;
            float t = F32(timer, 0);
            if (1.0f /* 0x931f24 */ > t) SF32(timer, 0, t + dt);
        }
    WORLD_CALL_SP(4, rx_world_handler_list_compact_and_run(world + 0xcc00, world));
    world_frame_leave(&f);
}
RX_FUNCTION(0x007699e0, rx_world_pre_listeners)

/* 00769ab0 World_PostListeners (ESI = world, float dt; RET 4): listener vfunc 1; when contact events were recorded, every
 * contact handler (+0x1934 vector, bounds-checked) gets vfunc 0 (world); then the kinematic bodies' accumulators (pose +0x20)
 * are cleared. */
void rx_world_post_listeners(rx_addr world, float dt) {
    world_frame f = world_frame_enter(8, 2, 1);
    rx_rider_capture_begin();   /* w-jobthreads3: the riders' pose captures queued while the listeners run (world_rider_62.c) */
    call_listeners(world, 4, dt);
    rx_rider_capture_flush();
    RX_WATCH_MARK(3, world);
#ifndef RX_ORACLE
    rx_sound_record_contacts(world);   /* w-sfx: the events the AudioCollisionManager (a contact handler here in the original) gets */
#endif
    if (U32(world, 0x1900) != 0) {
        for (uint32_t i = 0; i < (uint32_t)(I32(world, 0x1938) - I32(world, 0x1934)) >> 2; i++) {
            /* the size is recomputed with an arithmetic shift for the check */
            world_bounds_check(i, (uint32_t)((I32(world, 0x1938) - I32(world, 0x1934)) >> 2));
            rx_addr handler = U32(U32(world, 0x1934), 4 * i);
            WORLD_ORIG(U32(U32(handler, 0), 0), RX_THISCALL, handler, world);
        }
    }
    for (int32_t k = 0; k < I32(world, 0x19d4); k++) v4_st(world + 0x1d20 + 0x50 * (uint32_t)k, v4_ld(WORLD_ZERO4));
    world_frame_leave(&f);
}
RX_FUNCTION(0x00769ab0, rx_world_post_listeners)

/* a simulation's slot 1 (+4) with the step dt (thiscall, one float): the product runs the registered core function (this, float) */
static void sim_step(rx_addr sim, const uint32_t* dt_arg) {
#ifndef RX_ORACLE
    (void)world_vcall_f(sim, 4, rx_bf(*dt_arg));
#else
    rx_call_original(U32(U32(sim, 0), 4), RX_THISCALL, sim, 1, dt_arg);
#endif
}
/* ---- w-jobthreads4: the simulations' steps as parallel jobs (loop_jobs.h) ----
 * The steps (slot 1: RavenSimulation::vf1 00430eb0 -> UpdateFixedSteps -> the class's integration) come after every collision
 * pass. One job per simulation, in the list's order (contexts RX_JOB_CTX_SIM + i), for the classes whose shared state is known
 * (the census of core/tools/jobthreads_watch_sims.c, core/plan/QUEUE.md w-jobthreads3 / 4): the motocross bike, the ATV, the
 * UTV and the buggy simulation (their slot 8, the pending-tuning reload, identifies them). Shared between the steps: the class's
 * terrain-triangle buffer (rx_job_scratch: a private copy per job, copied back in job order), the pending-tuning flag (SU32J:
 * every step stores 1) and the world's contact-event queue (rx_job_ordered before an impact is appended). Anything else -- another
 * class (the trucks, the remote bike), a tuning XML pending (0x009e3100: the first step applies it), more than 32 -- runs the loop
 * as before. */
static rx_addr g_sims_world;
static uint32_t g_sims_dt;
static void sim_job(rx_addr i) { sim_step(U32(U32(g_sims_world, 0x1940), 4u * i), &g_sims_dt); }
static int sim_class_known(rx_addr sim) {
    const rx_addr vt = U32(sim, 0);
    if (U32(vt, 4) != 0x00430eb0u) return 0;                                  /* RavenSimulation::vf1 */
    const rx_addr s8 = U32(vt, 0x20);
    return s8 == 0x0043b640u || s8 == 0x0048d320u || s8 == 0x004cbf20u || s8 == 0x004b16f0u;   /* MX / ATV / UTV / buggy */
}
static int sims_parallel(rx_addr world, const uint32_t* dt_arg) {
#ifdef RX_ORACLE
    (void)world; (void)dt_arg; return 0;
#else
    const int32_t n = I32(world, 0x1944);
    if (n < 2 || n > 32 || !rx_jobs_on(RX_JOB_SIMS) || rx_jobs_in_job() || U32(0, 0x009e3100) != 0) return 0;
    for (int32_t i = 0; i < n; i++) if (!sim_class_known(U32(U32(world, 0x1940), 4u * (uint32_t)i))) return 0;
    rx_job jobs[32];
    g_sims_world = world; g_sims_dt = *dt_arg;
    for (int32_t i = 0; i < n; i++) {
        jobs[i].body = sim_job; jobs[i].arg = (rx_addr)i; jobs[i].ctx = RX_JOB_CTX_SIM + i; jobs[i].tls = U32(rx_teb(), 0x2c);
    }
    rx_jobs_run(jobs, n);
    if (I32(world, 0x1944) != n) abort();   /* (no step changes the list) */
    return 1;
#endif
}

/* 0076a010 World_SolverDispatch (EAX = world, float dt; RET 4): the collision passes, slot 1 of every simulation with dt (the
 * vehicle fixed-step schedulers), lost kinematic bodies reset, then four quarter steps of the kinematic integration with the
 * water pass between them. The quartered dt is written back into the argument slot, as the original does.
 * With the worker flag 0xa946c4 the simulations are split: up to 12 (a quarter) go to the worker thread through the job
 * table 0xa7f750 (count 0xa946cc, dt 0xa946d0, busy flag 0xa946d4), half of the rest run here before the worker is woken
 * (SetEvent on 0xd61380), the rest after, then this thread waits (Sleep(1)) until the worker clears the busy flag. */
void rx_world_solver_dispatch(rx_addr world, float dt) {
    world_frame f = world_frame_flat(4, 0);   /* push ebx; push ebp; push esi; push edi */
    const rx_addr dt_slot = f.entry + 4;
    WORLD_CALL_SP(4, rx_world_kinematic_collide_water(world));
    WORLD_CALL_SP(0, rx_world_kinematic_collide_static(world));
    WORLD_CALL_SP(4, rx_world_dynobj_collide_static(world));
    WORLD_CALL_SP(4, rx_world_dynobj_collide_kinematic(world));
    WORLD_CALL_SP(4, rx_world_dynobj_collide_all(world));
    WORLD_CALL_SP(0, rx_world_dynobj_collide_water(world));
    WORLD_CALL_SP(0, rx_world_kinematic_collide_keyed(world));
    WORLD_CALL_SP(0, rx_world_rider_collide_keyed(world));
    WORLD_CALL_SP(4, rx_world_dynobj_collide_terrain(world));
    WORLD_CALL_SP(0, rx_world_kinematic_collide_terrain(world));
    const uint32_t dt_arg = world_x87_pass(rx_fb(dt));
    if (U32(0, 0x00a946c4) != 0) {
        const int32_t n = I32(world, 0x1944);
        int32_t jobs = n / 4;
        if (jobs > 12) jobs = 12;
        const int32_t here = n - jobs;
        for (int32_t j = 0; j < jobs; j++) SU32(0, 0x00a7f750 + 4 * (uint32_t)j, U32(U32(world, 0x1940), 4u * (uint32_t)(n - 1 - j)));
        SU32(0, 0x00a946cc, (uint32_t)jobs);
        SF32(0, 0x00a946d0, dt);
        SU32(0, 0x00a946d4, 1);
        const int32_t first = here / 2;
        int32_t i = 0;
        for (; i < first; i++) {
            rx_addr sim = U32(U32(world, 0x1940), 4u * (uint32_t)i);
            sim_step(sim, &dt_arg);
        }
        if (RX_OS(0x00916034, U32(0, 0x00d61380)) == 0)   /* SetEvent */
            RX_OS0(0x00916078);                           /* GetLastError */
        for (; i < here; i++) {
            rx_addr sim = U32(U32(world, 0x1940), 4u * (uint32_t)i);
            sim_step(sim, &dt_arg);
        }
        while (U32(0, 0x00a946d4) != 0) RX_OS(0x00916020, 1);   /* Sleep(1) */
    } else if (!sims_parallel(world, &dt_arg)) {
        int32_t i = 0;
        for (; i < I32(world, 0x1944); i++) {
            rx_addr sim = U32(U32(world, 0x1940), 4u * (uint32_t)i);
            RX_WATCH_MARK(10 + i, sim);   /* (-DRX_WATCH: each simulation's step a unit of the dependence check, w-jobthreads3) */
            sim_step(sim, &dt_arg);
            RX_WATCH_MARK(9, sim);
        }
        RX_WATCH_MARK(8, world);
    }
    WORLD_CALL_SP(0, rx_world_kinematic_reset_lost(world));
    const float quarter = dt * 0.25f /* 0x92ff84 */;
    SF32(dt_slot, 0, quarter);   /* the original keeps the quarter step in its argument slot */
    for (int step = 0; step < 4; step++) {
        if (step > 0) WORLD_CALL_SP(4, rx_world_kinematic_collide_water(world));
        WORLD_CALL_SP(0, rx_world_kinematic_integrate(world, quarter));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0076a010, rx_world_solver_dispatch)

/* 00768750 HandlerList_CompactAndRun (EAX = list, stdcall world; RET 4): the world's contact-handler list (a checked vector:
 * container +0, begin +0xc, end +0x10, +0x18 a counter cleared here): std::remove(begin, end, 0) (00776920), erase(that, end)
 * (007cf450), then the sort loop 00776ef0(begin, end, count, world). The checked-iterator aborts as the original's. */
void rx_world_handler_list_compact_and_run(rx_addr list, rx_addr world) {
    world_frame f = world_frame_flat(4, 0x20);
    const rx_addr value = f.esp + 0x14, removed = f.esp + 0x28, erased = f.esp + 0x20;
    const uint32_t end0 = U32(list, 0x10);
    SU32(list, 0x18, 0);
    SU32(value, 0, 0);
    if (U32(list, 0xc) > end0) crt_invalid_parameter_noinfo();
    const uint32_t end1 = U32(list, 0x10);
    const uint32_t container = U32(list, 0);
    SU32(erased, 0, container);   /* the container id; later erase's result slot */
    if (U32(list, 0xc) > end1) crt_invalid_parameter_noinfo();
    const uint32_t begin = U32(list, 0xc);
    const uint32_t container2 = U32(list, 0);
    if (begin > U32(list, 0x10)) crt_invalid_parameter_noinfo();
    rx_regs in = world_regs();
    in.r[WORLD_EBX] = end0; in.r[WORLD_EBP] = end1; in.r[WORLD_ESI] = list; in.r[WORLD_EDI] = begin;
    WORLD_CALL_SP(0x18, rx_world_ptr_remove(removed, value, U32(list, 0), begin, container2, end1));
    const uint32_t rc = U32(removed, 0), rp = U32(removed, 4);
    in.r[WORLD_ECX] = rp; in.r[WORLD_EDX] = rc; in.r[WORLD_EAX] = erased;
    WORLD_CALL_SP(0x14, (void)phys_vector_erase(list, erased, rp, end0));                        /* vector erase */
    (void)rc; (void)container;
    const uint32_t e = U32(list, 0x10);
    if (U32(list, 0xc) > e) crt_invalid_parameter_noinfo();
    const uint32_t b = U32(list, 0xc), c = U32(list, 0);
    if (b > U32(list, 0x10)) crt_invalid_parameter_noinfo();
    const uint32_t c2 = U32(list, 0);
    if (c == 0 || c != c2) crt_invalid_parameter_noinfo();
    const uint32_t count = (uint32_t)((int32_t)(e - b) >> 2);
    WORLD_CALL_SP(0x10, rx_world_body_sort_loop(b, e, (int32_t)count, world));
    world_frame_leave(&f);
}
RX_FUNCTION(0x00768750, rx_world_handler_list_compact_and_run)

/* ---- the generic dynamic objects (world +0x1960: 0xe0-byte records) ----------------------------------------------------
 * Object: +0x20 material (+0xc0 push limit), +0x2c collision shape (+0x10 / +0x14 flag words), +0x34 id, +0x40 position,
 * +0x50 orientation, +0x60 velocity, +0x70 angular velocity, +0x80 / +0x90 / +0xa0 contact results (displacement, ..., push),
 * +0xb0 / +0xc0 accumulated contact impulse (linear sum, angular direction), +0xd4 "in water" flag. */
/* The contact set the narrow phases fill (count +0, records of 0x30 bytes at +0x10: point, normal, depth; feature ids +0x5b0 /
 * +0x628, 30 each; touching flag +0x6a0). */
static void clear_contact_ids(rx_addr set) {
    for (uint32_t k = 0; k < 30; k++) { SU32(set, 0x5b0 + 4 * k, 0xffffffffu); SU32(set, 0x628 + 4 * k, 0xffffffffu); }
}
static void reset_contact_set(rx_addr set) { SU32(set, 0x6a0, 0); SU32(set, 0, 0); clear_contact_ids(set); }
static rx_addr contact(rx_addr set, int32_t k) { return set + 0x10 + 0x30u * (uint32_t)k; }

/* A contact event (world +0, 64 bytes each, at most 100 per step; count +0x1900): ids, type, output word, the first contact's
 * point and feature ids from `set` (ids at set +ids_off / +ids_off + 0x78, point at set +pt_off), the two displacements. */
static void push_event(rx_addr world, uint32_t id_a, uint32_t id_b, uint32_t out, uint8_t type, rx_addr ids, rx_addr point,
                       uint32_t moved_a, uint32_t moved_b) {
    uint32_t events = U32(world, 0x1900);
    if (events >= 100) return;
    SU32(world, 0x1900, events + 1);
    rx_addr ev = world + events * 64;
    SU32(ev, 0, id_a);
    SU32(ev, 4, id_b);
    SU32(ev, 0x2c, out);
    SU8(ev, 0x20, type);
    SU32(ev, 0x30, U32(ids, 0));
    SU32(ev, 0x34, U32(ids, 0x78));
    SU32(ev, 0x10, U32(point, 0)); SU32(ev, 0x14, U32(point, 4)); SU32(ev, 0x18, U32(point, 8));
    SU32(ev, 0x24, moved_a);
    SU32(ev, 0x28, moved_b);
}
/* A+0xb0 /= n; A+0xc0 normalized (or the NaN pattern). */
static void average_and_normalize(rx_addr A, int32_t n) {
    float c = (float)n;
    v4_st(A + 0xb0, v4_div(v4_ld(A + 0xb0), v4_splat(c)));
    v4_st(A + 0xc0, v4_normalize_or_nan(v4_ld(A + 0xc0)));
}
/* push scaled down to the limit (all of lanes 0..2 of limit < |push|): push * (limit / |push|) */
static rx_v4 clamp_push(rx_v4 push, rx_v4 limit) {
    rx_v4 len = v4_splat(v4_len_xzy(push));
    if (v4_all3(v4_cmplt(limit, len))) push = v4_mul(v4_div(limit, len), push);
    return push;
}

/* 0076bf60 DynObj_CollideAll (stdcall world; RET 4): the generic dynamic objects against each other. For each object A whose
 * shape is not disabled (+0x14 bit 3): its bounds (shape vfunc 3), the broad phase (cursor +0x196c), and every candidate B != A
 * (unless both shapes have +0x10 bit 6): the narrow phase into the contact set, its mean contact (0085dcf0), B's response
 * (0076e460), A's accumulators, the push exchange (min + max of depth * normal over the contacts, * 0.8 * 0.5, clamped by B's
 * material) on +0xa0, and a contact event. Then A's accumulators are averaged (one more contact when A was already moving). */
void rx_world_dynobj_collide_all(rx_addr world) {
    world_frame f = world_frame_enter(0x7a4, 3, 1);
    const rx_addr sp = f.esp;
    const rx_addr box = sp + 0x60, query = sp + 0xa0, pos_a = sp + 0x50, rot_a = sp + 0x40, pos_b = sp + 0xb0, rot_b = sp + 0x80;
    const rx_addr set = sp + 0xf0, mean = sp + 0xc0;
    const rx_v4 half = v4_splat(0.5f /* 0x9322cc */);
    clear_contact_ids(set);
    if (world_obj_count(world) != 0) {
        uint32_t i = 0;
        do {
            world_bounds_check(i, world_obj_count(world));
            const rx_addr A = U32(world, 0x1960) + i * WORLD_OBJ_SIZE;
            const rx_addr shape_a = U32(A, 0x2c);
            if (!((U32(shape_a, 0x14) >> 3) & 1)) {
                v4_st(rot_a, v4_ld(A + 0x40));
                v4_st(pos_a, v4_ld(A + 0x50));
                WORLD_CTX(U32(U32(shape_a, 0), 0xc), &WCTX(.ecx = shape_a, .ebx = world, .esi = i, .edi = A), 0,
                          pos_a, rot_a, box, query);
                const rx_addr cursor = U32(world, 0x196c);
                world_broadphase_query_cells(U32(cursor, 0x30), box, query, WCTX(.ebx = world, .edi = A));
                SU32(cursor, 0, 0);
                const float speed = v4_len_xzy(v4_ld(A + 0xc0));
                int32_t contacts = v4_all3(v4_cmplt(v4_ld(WORLD_ZERO4), v4_splat(speed))) ? 1 : 0;
                for (uint32_t idx = world_next_pair(U32(world, 0x196c)); idx != 0xffffffffu; idx = world_next_pair(U32(world, 0x196c))) {
                    world_bounds_check(idx, world_obj_count(world));
                    const rx_addr B = U32(world, 0x1960) + idx * WORLD_OBJ_SIZE;
                    if (B == A) continue;
                    const rx_addr shape_b = U32(B, 0x2c);
                    if (((U32(shape_a, 0x10) >> 6) & 1) && ((U32(shape_b, 0x10) >> 6) & 1)) continue;
                    v4_st(rot_b, v4_ld(B + 0x40));
                    v4_st(pos_b, v4_ld(B + 0x50));
                    SU32(set, 0x6a0, 0);
                    SU32(set, 0, 0);
                    clear_contact_ids(set);
                    world_contact_dispatch(set, shape_a, pos_a, rot_a, shape_b, pos_b, rot_b, 0,
                                           WCTX(.ebx = B, .esi = 0, .edi = A));
                    if (U32(set, 0x6a0) == 0) continue;
                    const rx_v4 prev_a = v4_ld(A + 0x80), prev_b = v4_ld(B + 0x80);
                    world_contacts_average(set, mean, WCTX(.ebx = B, .edi = A));
                    WORLD_CALL_SP(8, rx_world_dynamic_contact_response(A, B, mean));
                    v4_st(A + 0xb0, v4_add(v4_ld(A + 0xb0), v4_ld(mean)));
                    contacts++;
                    const rx_v4 scale = v4_splat(F32(0, 0x009cc8a8));   /* 0.8 */
                    v4_st(A + 0xc0, v4_add(v4_sub(v4_zero(), v4_ld(mean + 0x10)), v4_ld(A + 0xc0)));
                    const int32_t n = I32(set, 0);
                    rx_v4 lo = v4_ld(WORLD_ZERO4), hi = lo;
                    for (int32_t k = 0; k < n; k++) {
                        const rx_addr p = contact(set, k);
                        const rx_v4 d = v4_mul(v4_ld(p + 0x20), v4_ld(p + 0x10));   /* depth * normal */
                        hi = v4_max(hi, d); lo = v4_min(lo, d);
                    }
                    rx_v4 push = v4_mul(v4_mul(v4_add(lo, hi), scale), half);
                    push = clamp_push(push, v4_ld(U32(B, 0x20) + 0xc0));
                    v4_st(B + 0xa0, v4_add(push, v4_ld(B + 0xa0)));
                    v4_st(A + 0xa0, v4_sub(v4_ld(A + 0xa0), push));
                    const float moved_a = v4_len_xzy(v4_sub(v4_ld(A + 0x80), prev_a));
                    const float moved_b = v4_len_xzy(v4_sub(v4_ld(B + 0x80), prev_b));
                    push_event(world, U32(A, 0x34), U32(B, 0x34), 0, 0, sp + 0x6a0, sp + 0x100, rx_fb(moved_a), rx_fb(moved_b));
                }
                if (contacts > 0) average_and_normalize(A, contacts);
            }
            i++;
        } while (i < world_obj_count(world));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0076bf60, rx_world_dynobj_collide_all)

/* The debug ring of the last ten terrain contacts (0xd613a0, 0x30 bytes each; index 0xd61390, high-water mark 0xd61394). */
static void debug_ring_push(rx_v4 a, rx_v4 b, rx_v4 c) {
    uint32_t i = U32(0, 0x00d61390);
    const rx_addr slot = 0x00d613a0 + i * 0x30;
    SU32(0, 0x00d61390, i + 1);
    v4_st(slot, a); v4_st(slot + 0x10, b); v4_st(slot + 0x20, c);
    i = U32(0, 0x00d61390);
    if ((int32_t)i > I32(0, 0x00d61394)) SU32(0, 0x00d61394, i);
    if ((int32_t)i >= 10) SU32(0, 0x00d61390, 0);
}
/* depth along the normal of a contact relative to `pos`: ((p - pos) * n) summed (x + y) + z */
static float contact_depth(rx_addr p, rx_v4 pos) { return phys_hsum3(v4_mul(v4_sub(v4_ld(p), pos), v4_ld(p + 0x10))); }
/* min / max of depth * normal over the contacts deeper than `thr` (relative to `pos`), starting from thr */
static rx_v4 terrain_push(rx_addr set, int32_t n, rx_v4 pos, rx_v4 thr, rx_v4* lo_out) {
    rx_v4 hi = thr, lo = thr;
    for (int32_t k = 0; k < n; k++) {
        const rx_addr p = contact(set, k);
        const rx_v4 nrm = v4_ld(p + 0x10);
        const rx_v4 d = v4_mul(v4_sub(v4_ld(p), pos), nrm);
        if (v4_all3(v4_cmplt(thr, v4_splat(phys_hsum3(d))))) {
            const rx_v4 depth = v4_mul(v4_ld(p + 0x20), nrm);
            lo = v4_min(lo, depth); hi = v4_max(hi, depth);
        }
    }
    *lo_out = lo;
    return hi;
}

/* 0076d3c0 DynObj_CollideTerrain (stdcall world; RET 4; nothing without a terrain, world +0xcc9c): per object whose shape lacks
 * +0x14 bit 5, the terrain contacts (00667e40, two contact sets: the solid ground and a second set reported as a type 1 event),
 * then for the ground contacts deeper than zero the response 0076f160 (accumulating +0xb0 / +0xc0 as 0076bf60), and the
 * push-out along the contact normals (min + max of depth * normal, * -0.8, clamped by the material) moving +0x40 and +0xa0, with
 * a type 0 event. */
void rx_world_dynobj_collide_terrain(rx_addr world) {
    world_frame f = world_frame_enter(0xda4, 3, 1);
    const rx_addr sp = f.esp, set1 = sp + 0x40, set2 = sp + 0x6f0, out1 = sp + 0x1c, out2 = sp + 0xc;
    if (U32(world, 0xcc9c) != 0) {
        clear_contact_ids(set1); clear_contact_ids(set2);
        if (world_obj_count(world) != 0) {
            uint32_t i = 0;
            do {
                world_bounds_check(i, world_obj_count(world));
                const rx_addr A = U32(world, 0x1960) + i * WORLD_OBJ_SIZE;
                int32_t cnt = 0;
                reset_contact_set(set1); reset_contact_set(set2);
                const rx_addr shape = U32(A, 0x2c);
                SU32(out1, 0, 0); SU32(out2, 0, 0);
                if (!((U32(shape, 0x14) >> 5) & 1))   /* 00667e40 Grid_ReportOverlaps: ECX = shape, EDX = set1 */
                    WORLD_CALL_SP(0x18, rx_world_grid_report_overlaps(shape, set1, U32(U32(world, 0xcc9c), 0x31ec), A + 0x40, A + 0x50,
                                                                      out1, set2, out2));
                if (U32(set2, 0x6a0) != 0) {
                    const float speed = v4_len_xzy(v4_ld(A + 0x60));
                    const float speed2 = v4_len_xzy(v4_ld(A + 0x60));
                    push_event(world, U32(A, 0x34), U32(world, 0xcc9c), U32(out2, 0), 1, set2 + 0x5b0, set2 + 0x10,
                               rx_fb(speed), rx_fb(speed2));
                }
                if (U32(set1, 0x6a0) != 0) {
                    const int32_t n = I32(set1, 0);
                    rx_v4 thr = v4_ld(WORLD_ZERO4);
                    const rx_v4 prev = v4_ld(A + 0x80);
                    if (n > 0) {
                        for (int32_t k = 0; k < n; k++) {
                            const rx_addr p = contact(set1, k);
                            debug_ring_push(v4_ld(p), v4_ld(p + 0x10), v4_ld(p + 0x20));
                            if (v4_all3(v4_cmplt(thr, v4_splat(contact_depth(p, v4_ld(A + 0x40)))))) {
                                WORLD_CALL_SP(4, rx_world_terrain_contact_response(A, p));
                                v4_st(A + 0xb0, v4_add(v4_ld(p), v4_ld(A + 0xb0)));
                                cnt++;
                                v4_st(A + 0xc0, v4_add(v4_sub(v4_zero(), v4_ld(p + 0x10)), v4_ld(A + 0xc0)));
                                thr = v4_ld(WORLD_ZERO4);
                            }
                        }
                        if (cnt > 0) { average_and_normalize(A, cnt); thr = v4_ld(WORLD_ZERO4); }
                    }
                    const rx_v4 scale = v4_splat(F32(0, 0x009cc8a8));
                    rx_v4 lo, hi = thr;
                    lo = thr;
                    if (n > 0) hi = terrain_push(set1, n, v4_ld(A + 0x40), thr, &lo);
                    rx_v4 push = v4_mul(v4_add(lo, hi), v4_sub(v4_zero(), scale));
                    push = clamp_push(push, v4_ld(U32(A, 0x20) + 0xc0));
                    v4_st(A + 0x40, v4_add(v4_ld(A + 0x40), push));
                    v4_st(A + 0xa0, v4_add(v4_ld(A + 0xa0), push));
                    const float moved = v4_len_xzy(v4_sub(v4_ld(A + 0x80), prev));
                    push_event(world, U32(A, 0x34), U32(world, 0xcc9c), U32(out1, 0), 0, set1 + 0x5b0, set1 + 0x10,
                               rx_fb(moved), rx_fb(moved));
                }
                i++;
            } while (i < world_obj_count(world));
        }
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0076d3c0, rx_world_dynobj_collide_terrain)

/* 0076b4a0 DynObj_CollideStatic (stdcall world; RET 4): per object A, its bounds, the static broad phase (cursor +0x19cc), the
 * accumulators +0xb0 / +0xc0 reset, then per enabled static S (64-byte records at world +0x19c0, byte +0x38): the narrow phase,
 * the mean contact, S's response (0076efc0), A's accumulators and the push-out (min + max of depth * normal over the contacts
 * deeper than zero, * -scale, clamped to 0.5; the clamp ratio becomes the scale of every later push in this call, as in the
 * original), moving +0x40 and +0xa0, with an event (other = S +0x30). */
void rx_world_dynobj_collide_static(rx_addr world) {
    world_frame f = world_frame_enter(0x7a4, 3, 1);
    const rx_addr sp = f.esp;
    const rx_addr pos = sp + 0x50, rot = sp + 0x60, box = sp + 0x80, query = sp + 0xa0, pos_s = sp + 0x70, rot_s = sp + 0xb0;
    const rx_addr set = sp + 0xf0, mean = sp + 0xc0;
    const rx_v4 limit = v4_splat(0.5f /* 0x9322cc */);
    rx_v4 scale = v4_splat(1.0f /* 0x931f24 */);
    clear_contact_ids(set);
    if (world_obj_count(world) != 0) {
        uint32_t i = 0;
        do {
            world_bounds_check(i, world_obj_count(world));
            const rx_addr A = U32(world, 0x1960) + i * WORLD_OBJ_SIZE;
            v4_st(rot, v4_ld(A + 0x40));
            v4_st(pos, v4_ld(A + 0x50));
            const rx_addr shape_a = U32(A, 0x2c);
            WORLD_CTX(U32(U32(shape_a, 0), 0xc), &WCTX(.ecx = shape_a, .ebx = world, .esi = i, .edi = A), 0,
                      pos, rot, box, query);
            const rx_addr cursor = U32(world, 0x19cc);
            world_broadphase_query_boxes(U32(cursor, 0x30), box, query, WCTX(.ecx = query, .ebx = world, .edi = A));
            SU32(cursor, 0, 0);
            v4_st(A + 0xb0, v4_ld(WORLD_ZERO4));
            v4_st(A + 0xc0, v4_ld(WORLD_ZERO4));
            int32_t cnt = 0;
            for (uint32_t idx = world_next_pair(U32(world, 0x19cc)); idx != 0xffffffffu; idx = world_next_pair(U32(world, 0x19cc))) {
                world_bounds_check(idx, world_static_count(world));
                const rx_addr S = U32(world, 0x19c0) + idx * 64;
                if (U8(S, 0x38) == 0) continue;
                v4_st(rot_s, v4_ld(S));
                v4_st(pos_s, v4_ld(S + 0x10));
                reset_contact_set(set);
                world_contact_dispatch(set, U32(A, 0x2c), pos, rot, U32(S, 0x28), pos_s, rot_s, 0,
                                       WCTX(.eax = U32(A, 0x2c), .edx = pos, .ebx = world, .esi = S, .edi = A));
                if (U32(set, 0x6a0) == 0) continue;
                const rx_v4 prev_a = v4_ld(A + 0x80);
                world_contacts_average(set, mean, WCTX(.ecx = mean, .edx = pos, .ebx = world, .edi = A));
                WORLD_CALL_SP(4, rx_world_static_contact_response(S, A, mean));
                v4_st(A + 0xb0, v4_add(v4_ld(mean), v4_ld(A + 0xb0)));
                cnt++;
                v4_st(A + 0xc0, v4_add(v4_sub(v4_zero(), v4_ld(mean + 0x10)), v4_ld(A + 0xc0)));
                const int32_t n = I32(set, 0);
                const rx_v4 thr = v4_ld(WORLD_ZERO4);
                rx_v4 lo = thr, hi = thr;
                if (n > 0) hi = terrain_push(set, n, v4_ld(A + 0x40), thr, &lo);
                rx_v4 push = v4_mul(v4_sub(v4_zero(), scale), v4_add(lo, hi));
                const rx_v4 len = v4_splat(v4_len_xzy(push));
                if (v4_all3(v4_cmplt(limit, len))) { const rx_v4 ratio = v4_div(limit, len); push = v4_mul(push, ratio); scale = ratio; }
                v4_st(A + 0x40, v4_add(v4_ld(A + 0x40), push));
                v4_st(A + 0xa0, v4_add(v4_ld(A + 0xa0), push));
                const float moved = v4_len_xzy(v4_sub(v4_ld(A + 0x80), prev_a));
                push_event(world, U32(A, 0x34), U32(S, 0x30), 0, 0, set + 0x5b0, set + 0x10, rx_fb(moved), 0);
            }
            if (cnt > 0) average_and_normalize(A, cnt);
            i++;
        } while (i < world_obj_count(world));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0076b4a0, rx_world_dynobj_collide_static)

/* 0076b9b0 DynObj_CollideKinematic (stdcall world; RET 4): the generic dynamic objects against the keyed objects (world +0x19a0,
 * 0xe0-byte records enabled by byte +0xb0; broad-phase cursor +0x19ac). Per contact pair: the narrow phase, per contact the
 * keyed response 0076e090 (ECX = contact; first a "still moving" test of A's speed against K +0xc0 that clears K's axis lock
 * +0xb4 and its flag +0xd0); a pushable K (+0x84 bit 5) takes the push (min + max of depth * normal, * 0.8 * 0.5, through K's
 * water state 0084fe00, clamped by K's material) off A's +0xa0 and onto its kinematic pose (world +0x1d00 + 0x50 * K +0x80, axis
 * lock applied; copied to K +0x40). An event records both displacements. */
void rx_world_dynobj_collide_kinematic(rx_addr world) {
    world_frame f = world_frame_enter(0x784, 3, 1);
    const rx_addr sp = f.esp;
    const rx_addr pos = sp + 0x60, rot = sp + 0x70, box = sp + 0x90, query = sp + 0xa0, pos_k = sp + 0xc0, rot_k = sp + 0x80;
    const rx_addr set = sp + 0xd0, push_slot = sp + 0x40;
    const rx_v4 half = v4_splat(0.5f /* 0x9322cc */);
    clear_contact_ids(set);
    if (world_obj_count(world) != 0) {
        uint32_t i = 0;
        do {
            world_bounds_check(i, world_obj_count(world));
            const rx_addr A = U32(world, 0x1960) + i * WORLD_OBJ_SIZE;
            v4_st(rot, v4_ld(A + 0x40));
            v4_st(pos, v4_ld(A + 0x50));
            const rx_addr shape_a = U32(A, 0x2c);
            WORLD_CTX(U32(U32(shape_a, 0), 0xc), &WCTX(.ecx = shape_a, .ebx = world, .esi = i, .edi = A), 0,
                      pos, rot, box, query);
            const rx_addr cursor = U32(world, 0x19ac);
            world_broadphase_query_boxes(U32(cursor, 0x30), box, query, WCTX(.ecx = query, .ebx = world, .edi = A));
            SU32(cursor, 0, 0);
            for (uint32_t idx = world_next_pair(U32(world, 0x19ac)); idx != 0xffffffffu; idx = world_next_pair(U32(world, 0x19ac))) {
                world_bounds_check(idx, world_keyed_count(world));
                const rx_addr K = U32(world, 0x19a0) + idx * WORLD_OBJ_SIZE;
                if (U8(K, 0xb0) == 0) continue;
                v4_st(rot_k, v4_ld(K + 0x40));
                v4_st(pos_k, v4_ld(K + 0x50));
                reset_contact_set(set);
                world_contact_dispatch(set, U32(A, 0x2c), pos, rot, U32(K, 0x2c), pos_k, rot_k, 0,
                                       WCTX(.eax = U32(A, 0x2c), .edx = pos, .ebx = world, .esi = K, .edi = A));
                if (U32(set, 0x6a0) == 0) continue;
                const rx_v4 prev_a = v4_ld(A + 0x80);
                rx_v4 last = v4_ld(WORLD_ZERO4);
                rx_v4 prev_k = last;
                if (I32(K, 0x80) >= 0) prev_k = v4_ld(world + 0x1d10 + U32(K, 0x80) * 0x50);
                int32_t n = I32(set, 0);
                if (n > 0) {
                    for (int32_t k = 0; k < n; k++, n = I32(set, 0)) {
                        if (U8(K, 0xd0) != 0 && v4_all3(v4_cmplt(v4_ld(K + 0xc0), v4_splat(v4_len_xzy(v4_ld(A + 0x60)))))) {
                            SU32(K, 0xb4, 0); SU8(K, 0xd0, 0);
                        }
                        WORLD_CALL_SP(8, rx_world_keyed_obj_contact_response(contact(set, k), A, world, K));
                    }
                    last = v4_ld(WORLD_ZERO4);
                }
                if (U8(K, 0x84) & 0x20) {
                    const rx_v4 scale = v4_splat(F32(0, 0x009cc8a8));
                    rx_v4 lo = last, hi = last;
                    for (int32_t k = 0; k < n; k++) {
                        const rx_addr p = contact(set, k);
                        const rx_v4 d = v4_mul(v4_ld(p + 0x20), v4_ld(p + 0x10));
                        hi = v4_max(hi, d); lo = v4_min(lo, d);
                    }
                    v4_st(push_slot, v4_mul(v4_mul(v4_add(lo, hi), scale), half));
                    world_water_filter(world + (U32(K, 0x80) + 0x4d0) * 32, push_slot, WCTX(.edx = push_slot, .ebx = world, .esi = K, .edi = A));
                    rx_v4 push = clamp_push(v4_ld(push_slot), v4_ld(U32(K, 0x20) + 0xc0));
                    v4_st(A + 0xa0, v4_sub(v4_ld(A + 0xa0), push));
                    push = world_apply_lock(push, U32(K, 0xb4), v4_ld(WORLD_ZERO4));
                    const rx_addr kpos = world + 0x1d00 + U32(K, 0x80) * 0x50;
                    v4_st(kpos, v4_add(v4_ld(kpos), push));
                    v4_st(K + 0x40, v4_ld(world + 0x1d00 + U32(K, 0x80) * 0x50));
                    last = v4_ld(WORLD_ZERO4);
                }
                const rx_v4 d_a = v4_sub(v4_ld(A + 0x80), prev_a);
                if (I32(K, 0x80) >= 0) last = v4_ld(world + 0x1d10 + U32(K, 0x80) * 0x50);
                const rx_v4 d_k = v4_sub(last, prev_k);
                const float moved_a = v4_len_xzy(d_a), moved_k = v4_len_xzy(d_k);
                push_event(world, U32(A, 0x34), U32(K, 0x34), 0, 0, set + 0x5b0, set + 0x10, rx_fb(moved_a), rx_fb(moved_k));
            }
            i++;
        } while (i < world_obj_count(world));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0076b9b0, rx_world_dynobj_collide_kinematic)

/* The water body of the current player, as the inlined copies of 00402dc0 search it: the first non-null entry of the list (count
 * at +8) whose +0x44a8 matches the active player's id ([0xa95844]: [[+4] * 4 + 0x1c]); 0 if none. *last_id gets the last id
 * compared (the copies leave it in EBX). */
static rx_addr find_player_water(rx_addr list, uint32_t* last_id) {
    for (uint32_t c = 0; c < U32(list, 8); c++) {
        const rx_addr e = U32(list, 4 * c);
        if (e == 0) continue;
        const rx_addr player = U32(0, 0x00a95844);
        *last_id = U32(player, U32(player, 4) * 4 + 0x1c);
        if (*last_id == U32(e, 0x44a8)) return e;
    }
    return 0;
}

/* The kinematic bodies: world +0x19d8[k] (k < +0x19d4) with pose world +0x1d00 + 0x50 k (position +0, velocity +0x10,
 * accumulator +0x20), orientation world +0x5b80 + 0x50 k (quaternion +0, angular velocity +0x10), water state world +0x9a00
 * + 0x20 k (+0 touching, +0x10 push direction, +0x1900 a float, +0x1910 the impulse). Body: +0x20 material, +0x2c shape,
 * +0x34 render object / id, +0x40 / +0x50 pose copies, +0x80 kinematic index (-1: none), +0x84 state bits (0 collides,
 * 1 integrates, 2 / 3 sleep timers, 4 reset, 5 pushable), +0x90 / +0xa0 timers, +0xb4 axis lock, +0xd0 buoyant. */
static rx_addr kin_body(rx_addr world, int32_t k) { return U32(world, 0x19d8 + 4 * (uint32_t)k); }
static rx_addr kin_pose(rx_addr world, int32_t k) { return world + 0x1d00 + 0x50 * (uint32_t)k; }
static rx_addr kin_water(rx_addr world, int32_t k) { return world + 0x9a00 + 0x20 * (uint32_t)k; }

/* min / max of depth * normal over all the contacts of a set, from the zero vector */
static rx_v4 contacts_push(rx_addr set, int32_t n, rx_v4* lo_out) {
    rx_v4 hi = v4_ld(WORLD_ZERO4), lo = hi;
    for (int32_t j = 0; j < n; j++) {
        const rx_addr p = contact(set, j);
        const rx_v4 d = v4_mul(v4_ld(p + 0x20), v4_ld(p + 0x10));
        hi = v4_max(hi, d); lo = v4_min(lo, d);
    }
    *lo_out = lo;
    return hi;
}

/* 0076ac40 Kinematic_CollideWater (stdcall world; RET 4): per kinematic body in collision (+0x84 bit 0): the narrow phase
 * against the water shape world +0xccb0 (at the origin, identity rotation), the mean contact (a vertical-speed window
 * -3 < v.normal < 3 zeroes the water float +0x1900; the normal goes to +0x1910), the buoyancy per contact 0076dd50, the push
 * (min + max of depth * normal, * 0.8, clamped, axis lock applied) added to the pose (copied to body +0x40), its direction, and
 * an event (other = the player's water body +0xe4). */
void rx_world_kinematic_collide_water(rx_addr world) {
    world_frame f = world_frame_enter(0x784, 3, 1);
    const rx_addr sp = f.esp, rot = sp + 0x50, pos = sp + 0x90, water_rot = sp + 0x60, water_pos = sp + 0x80, set = sp + 0xd0;
    const rx_addr mean = sp + 0xa0;
    v4_st(water_pos, v4_ld(WORLD_ZERO4));
    v4_st(water_rot, v4_ld(WORLD_IDENTITYQ));
    clear_contact_ids(set);
    for (int32_t k = 0; k < I32(world, 0x19d4); k++) {
        const rx_addr O = kin_body(world, k);
        const rx_addr velocity = kin_pose(world, k) + 0x10, water = kin_water(world, k);
        v4_st(rot, v4_ld(O + 0x40));
        v4_st(pos, v4_ld(O + 0x50));
        SU32(water, 0, 0);
        if (!(U8(O, 0x84) & 1)) continue;
        reset_contact_set(set);
        world_contact_dispatch(set, world + 0xccb0, water_rot, water_pos, U32(O, 0x2c), pos, rot, 0,
                               WCTX(.eax = water_rot, .edx = water_pos, .ebx = world, .esi = 0, .edi = O));
        if (U32(set, 0x6a0) == 0) continue;
        const rx_v4 prev = v4_ld(velocity);
        world_contacts_average(set, mean, WCTX(.eax = mean, .edx = velocity, .ebx = world, .edi = O));
        const rx_v4 normal = v4_ld(mean + 0x10);
        const float v = phys_hsum3(v4_mul(v4_ld(velocity), normal));
        if (v > F32(0, 0x009cc380) /* -3 */ && 3.0f /* 0x92fe48 */ > v) SU32(water, 0x1900, 0);
        v4_st(water + 0x1910, normal);
        int32_t n = I32(set, 0);
        if (n > 0) {
            uint32_t c = 0;
            rx_addr p = set + 0x10;
            do {
                WORLD_CALL_SP(8, rx_world_kinematic_water_contact(O, world, p));
                n = I32(set, 0);
                c++; p += 0x30;
            } while ((int32_t)c < n);
        }
        const rx_v4 z = v4_ld(WORLD_ZERO4);
        rx_v4 lo, hi = contacts_push(set, n, &lo);
        rx_v4 push = v4_mul(v4_add(lo, hi), v4_splat(F32(0, 0x009cc8a8)));
        push = clamp_push(push, v4_ld(U32(O, 0x20) + 0xc0));
        push = world_apply_lock(push, U32(O, 0xb4), z);
        v4_st(velocity - 0x10, v4_add(v4_ld(velocity - 0x10), push));
        v4_st(O + 0x40, v4_ld(velocity - 0x10));
        SU32(water, 0, 1);
        v4_st(water + 0x10, v4_normalize_or_nan(push));
        const float moved = v4_len_xzy(v4_sub(v4_ld(velocity), prev));
        if (U32(world, 0x1900) < 100) {
            uint32_t unused;
            const rx_addr body = find_player_water(U32(world, 0xccd0), &unused);
            push_event(world, U32(O, 0x34), U32(body, 0xe4), 0, 0, set + 0x5b0, set + 0x10, rx_fb(moved), 0);
        }
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0076ac40, rx_world_kinematic_collide_water)

/* 0076c510 DynObj_CollideWater (ESI = world; RET 0): the generic objects in water (byte +0xd4): the narrow phase against the
 * water shape, the buoyancy per contact 0076dec0, the push (min + max of depth * normal, * 0.8, clamped) added to +0xa0, and an
 * event (other = the player's water body +0xe4). EBX is one register for the whole function in the original: the per-contact
 * loop counts in it and the water search leaves the last player id in it, and it initializes the next object's contact set
 * (count and flag) and is the narrow phase's last argument. Reproduced. */
void rx_world_dynobj_collide_water(rx_addr world) {
    world_frame f = world_frame_enter(0x738, 2, 1);
    const rx_addr sp = f.esp, rot = sp + 0x30, pos = sp + 0x50, water_rot = sp + 0x40, water_pos = sp + 0x70, set = sp + 0x80;
    v4_st(water_pos, v4_ld(WORLD_ZERO4));
    v4_st(water_rot, v4_ld(WORLD_IDENTITYQ));
    uint32_t ebx = 0;
    clear_contact_ids(set);
    if (world_obj_count(world) != 0) {
        uint32_t i = 0;
        do {
            world_bounds_check(i, world_obj_count(world));
            const rx_addr A = U32(world, 0x1960) + i * WORLD_OBJ_SIZE;
            if (U8(A, 0xd4) != 0) {
                v4_st(rot, v4_ld(A + 0x40));
                v4_st(pos, v4_ld(A + 0x50));
                SU32(set, 0x6a0, ebx); SU32(set, 0, ebx);
                clear_contact_ids(set);
                world_contact_dispatch(set, world + 0xccb0, water_rot, water_pos, U32(A, 0x2c), pos, rot, ebx,
                                       WCTX(.eax = water_rot, .edx = water_pos, .ebx = ebx, .esi = world, .edi = A));
                if (U32(set, 0x6a0) != ebx) {
                    int32_t n = I32(set, 0);
                    const rx_v4 prev_a = v4_ld(A + 0x80);
                    if (n > 0) {
                        rx_addr p = set + 0x10;
                        do {
                            WORLD_CALL_SP(4, rx_world_dynobj_water_contact(world, p, A));   /* 0076dec0 (ECX = world, EDI = A) */
                            n = I32(set, 0);
                            p += 0x30; ebx++;
                        } while ((int32_t)ebx < n);
                    }
                    rx_v4 lo, hi = contacts_push(set, n, &lo);
                    rx_v4 push = v4_mul(v4_add(lo, hi), v4_splat(F32(0, 0x009cc8a8)));
                    push = clamp_push(push, v4_ld(U32(A, 0x20) + 0xc0));
                    v4_st(A + 0xa0, v4_add(push, v4_ld(A + 0xa0)));
                    const float moved = v4_len_xzy(v4_sub(v4_ld(A + 0x80), prev_a));
                    if (U32(world, 0x1900) < 100) {
                        const rx_addr body = find_player_water(U32(world, 0xccd0), &ebx);
                        push_event(world, U32(A, 0x34), U32(body, 0xe4), 0, 0, set + 0x5b0, set + 0x10, rx_fb(moved), 0);
                    }
                }
            }
            i++;
        } while (i < world_obj_count(world));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0076c510, rx_world_dynobj_collide_water)

/* 0076d9a0 Kinematic_CollideTerrain (ESI = world; RET 0): as 0076d3c0 for the kinematic bodies in collision (+0x84 bit 0): the
 * terrain contacts into two sets, an event for the second (type 1, speed = |pose velocity|), and for the ground set the
 * response per contact 0076f2f0 (ECX = body), the push-out (min + max of depth * normal, * -0.8, clamped) added to the pose
 * (copied to body +0x40) and a type 0 event. */
void rx_world_kinematic_collide_terrain(rx_addr world) {
    world_frame f = world_frame_enter(0xdb8, 2, 1);
    const rx_addr sp = f.esp, set1 = sp + 0x50, set2 = sp + 0x700, out1 = sp + 0x28, out2 = sp + 0xc;
    if (U32(world, 0xcc9c) != 0) {
        clear_contact_ids(set1); clear_contact_ids(set2);
        for (int32_t k = 0; k < I32(world, 0x19d4); k++) {
            const rx_addr O = kin_body(world, k);
            const rx_addr velocity = kin_pose(world, k) + 0x10;
            if (!(U8(O, 0x84) & 1)) continue;
            reset_contact_set(set1); reset_contact_set(set2);
            const rx_addr shape = U32(O, 0x2c);
            SU32(out1, 0, 0); SU32(out2, 0, 0);
            if (!((U32(shape, 0x14) >> 5) & 1))
                WORLD_CALL_SP(0x18, rx_world_grid_report_overlaps(shape, set1, U32(U32(world, 0xcc9c), 0x31ec), O + 0x40, O + 0x50,
                                                                  out1, set2, out2));
            if (U32(set2, 0x6a0) != 0) {
                const float speed = v4_len_xzy(v4_ld(velocity));
                push_event(world, U32(O, 0x34), U32(world, 0xcc9c), U32(out2, 0), 1, set2 + 0x5b0, set2 + 0x10,
                           rx_fb(speed), rx_fb(speed));
            }
            if (U32(set1, 0x6a0) != 0) {
                const rx_v4 prev = v4_ld(velocity);
                int32_t n = I32(set1, 0);
                if (n > 0) {
                    uint32_t c = 0;
                    rx_addr p = set1 + 0x10;
                    do {
                        WORLD_CALL_SP(8, rx_world_kinematic_terrain_contact_impulse(O, world, p));
                        n = I32(set1, 0);
                        p += 0x30; c++;
                    } while ((int32_t)c < n);
                }
                const rx_v4 scale = v4_splat(F32(0, 0x009cc8a8));
                rx_v4 lo, hi = contacts_push(set1, n, &lo);
                rx_v4 push = v4_mul(v4_add(lo, hi), v4_sub(v4_zero(), scale));
                push = clamp_push(push, v4_ld(U32(O, 0x20) + 0xc0));
                v4_st(velocity - 0x10, v4_add(v4_ld(velocity - 0x10), push));
                v4_st(O + 0x40, v4_ld(velocity - 0x10));
                const float moved = v4_len_xzy(v4_sub(v4_ld(velocity), prev));
                push_event(world, U32(O, 0x34), U32(world, 0xcc9c), U32(out1, 0), 0, set1 + 0x5b0, set1 + 0x10,
                           rx_fb(moved), rx_fb(moved));
            }
        }
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0076d9a0, rx_world_kinematic_collide_terrain)

/* Quaternion normalization as 0076a360 computes it: |q|^2 = (q3^2 + q1^2) + (q2^2 + q0^2) (two shuffles and adds), then the
 * normalize-or-NaN pattern. */
static rx_v4 normalize_quat(rx_v4 q) {
    const rx_v4 sq = v4_mul(q, q);
    const float len2 = phys_add(phys_add(sq.v[3], sq.v[1]), phys_add(sq.v[2], sq.v[0]));
    const rx_v4 l2 = v4_splat(len2), len = v4_sqrt(l2);
    rx_v4 r = v4_div(q, len);
    const rx_v4 finite = v4_cmpneq(l2, v4_ld(WORLD_INF4));
    r = v4_and(v4_and(r, v4_cmpneq(len, v4_zero())), finite);
    return v4_or(r, v4_andn(finite, v4_ld(WORLD_NAN4)));
}
/* The keyed-object index of a body in world +0x19a0 (vector header world +0x1994: begin +0xc, end +0x10). */
static uint32_t keyed_index(rx_addr world, rx_addr O) {
    const rx_addr hdr = world + 0x1994;
    if ((I32(hdr, 0x10) - I32(hdr, 0xc)) / (int32_t)WORLD_OBJ_SIZE == 0) crt_invalid_parameter_noinfo();
    return (uint32_t)((int32_t)(O - U32(hdr, 0xc)) / (int32_t)WORLD_OBJ_SIZE);
}
/* 0076abb0 Kinematic_ResetAuthoredPose (ECX=world, EDX=body; RET0).
 * Restore the saved pose, publish it through the real entity's vf30, restore
 * axis locks/buoyancy, then move the keyed object's broad-phase box. */
void rx_world_kinematic_reset_authored_pose(rx_addr world, rx_addr O) {
    world_frame f = world_frame_flat(3, 0);
    v4_st(O + 0x40, v4_ld(O + 0x60));
    v4_st(O + 0x50, v4_ld(O + 0x70));
    const rx_addr entity = U32(O, 0x34), vt = U32(entity, 0);
    WORLD_CTX(U32(vt, 0x78), &WCTX(.ecx = entity, .eax = O + 0x40, .edx = O + 0x50,
        .ebx = vt, .esi = world, .edi = O), 0, O + 0x40, O + 0x50);
    SU8(O, 0xd0, U8(O, 0xd1));
    SU32(O, 0xb4, U32(O, 0xb8));
    world_keyed_refresh(keyed_index(world, O), world + 0x1994, WCTX(.ebx = vt, .edi = O));
    world_frame_leave(&f);
}
RX_FUNCTION(0x0076abb0, rx_world_kinematic_reset_authored_pose)

/* the body's render object gets its pose: vfunc 0x78 (position, orientation) */
static void send_pose(rx_addr O, world_ctx c) {
    const rx_addr ro = U32(O, 0x34);
    c.ecx = ro; c.esi = U32(ro, 0); c.eax = O + 0x40; c.edx = O + 0x50;
    WORLD_CTX(U32(U32(ro, 0), 0x78), &c, 0, O + 0x40, O + 0x50);
}

/* 0076a360 Kinematic_Integrate (ECX = world, xmm0 = dt; RET 0): one quarter step of the kinematic bodies. Substeps
 * n = clamp(1 - trunc(dt / -0.001), 1, 10) of h = dt / n. Per integrating body (+0x84 bit 1): the buoyancy setup 0084f700 (when
 * +0xd0), the axis locks 00769e40 / 00769d40, then per substep the accelerations 00769bd0 (locked axes zeroed) integrate the
 * pose (position, velocity) and the orientation (quaternion, angular velocity) explicitly; then the water drag 0084fe00 (when
 * 0xda67c8), the quaternion normalized, the pose copied to +0x40 / +0x50 and sent to the render object, the keyed broad phase
 * refreshed (0042b000), and the sleep test against the material (+0x90 / +0xa0: state 0x27, else 0x23 with the timer +0xa0
 * from +0x80). Timers +0xa0 (state bit 2 -> 0x28) and +0x90 (bit 3 -> 0x30) count down by dt. A second pass, last body first,
 * resets bodies with +0x84 bit 4 (0076a8b0, pose from +0x60 / +0x70, lock from +0xb8, buoyancy from +0xd1). */
void rx_world_kinematic_integrate(rx_addr world, float dt) {
    world_frame f = world_frame_enter(0xa4, 3, 1);
    const rx_addr sp = f.esp, acc = sp + 0x60, ang_acc = sp + 0x70, vel = sp + 0x80, ang_vel = sp + 0x90;
    const uint32_t q = (uint32_t)rx_cvtt(phys_div(dt, -0.001f /* 0x977ba8 */));
    const int32_t steps0 = (int32_t)(1u - q);
    const int32_t steps = steps0 < 1 ? 1 : (steps0 > 10 ? 10 : steps0);
    const float h = phys_div(dt, (float)steps);
    const rx_v4 dt4 = v4_splat(dt), h4 = v4_splat(h);
    for (int32_t k = 0; k < I32(world, 0x19d4); k++) {
        const rx_addr O = kin_body(world, k);
        const rx_addr pose = kin_pose(world, k), orient = world + 0x5b80 + 0x50 * (uint32_t)k, water = kin_water(world, k);
        const rx_addr mat = U32(O, 0x20);
        if (U8(O, 0x84) & 2) {
            if (U8(O, 0xd0) != 0) WORLD_CALL_SP(4, phys_buoyancy_angular_correction(orient, O));
            WORLD_CALL_SP(4, rx_world_kinematic_apply_angular_lock(world, O, (uint32_t)k));
            WORLD_CALL_SP(4, rx_world_kinematic_apply_linear_lock(world, O, (uint32_t)k));
            for (int32_t s = steps; s > 0; s--) {
                WORLD_CALL_SP(0x10, rx_world_kinematic_accelerations((uint32_t)k, acc, ang_vel, world, mat, vel, ang_acc));
                rx_v4 a = v4_ld(acc);
                const uint32_t lock = U32(O, 0xb4);
                if (lock != 0) {
                    const rx_v4 z = v4_ld(WORLD_ZERO4);
                    if (lock & 1) { a.v[0] = z.v[0]; v4_st(acc, a); }
                    if (lock & 2) { a.v[1] = z.v[1]; v4_st(acc, a); }
                    if (lock & 4) { a.v[2] = z.v[2]; a.v[3] = z.v[2]; v4_st(acc, a); }
                }
                v4_st(pose + 0x10, v4_add(v4_mul(a, h4), v4_ld(pose + 0x10)));
                v4_st(pose, v4_add(v4_mul(v4_ld(vel), h4), v4_ld(pose)));
                v4_st(orient + 0x10, v4_add(v4_mul(v4_ld(ang_acc), h4), v4_ld(orient + 0x10)));
                v4_st(orient, v4_add(v4_mul(v4_ld(ang_vel), h4), v4_ld(orient)));
            }
            if (U32(0, 0x00da67c8) != 0) world_water_filter(water, pose + 0x10, WCTX(.eax = pose + 0x10, .ebx = orient, .edi = O));
            v4_st(orient, normalize_quat(v4_ld(orient)));
            v4_st(O + 0x40, v4_ld(pose));
            v4_st(O + 0x50, v4_ld(orient));
            send_pose(O, WCTX(.ebx = orient, .edi = O));
            world_keyed_refresh(keyed_index(world, O), world + 0x1994, WCTX(.ebx = orient, .edi = O));
            const float lin = v4_len2_xyz(v4_ld(pose + 0x10)), ang = v4_len2_xyz(v4_ld(orient + 0x10));
            if (v4_all3(v4_cmplt(v4_splat(lin), v4_ld(mat + 0x90))) && v4_all3(v4_cmplt(v4_splat(ang), v4_ld(mat + 0xa0))))
                SU32(O, 0x84, 0x27);
            else { SU32(O, 0x84, 0x23); v4_st(O + 0xa0, v4_ld(mat + 0x80)); }
        }
        if (U8(O, 0x84) & 4) {
            v4_st(O + 0xa0, v4_sub(v4_ld(O + 0xa0), dt4));
            if (v4_all3(v4_cmplt(v4_ld(O + 0xa0), v4_ld(WORLD_ZERO4))) && U8(O, 0xb1) != 0) {
                v4_st(O + 0x90, v4_ld(mat + 0x70));
                SU32(O, 0x84, 0x28);
            }
        }
        if (U8(O, 0x84) & 8) {
            v4_st(O + 0x90, v4_sub(v4_ld(O + 0x90), dt4));
            if (v4_all3(v4_cmplt(v4_ld(O + 0x90), v4_ld(WORLD_ZERO4))) && U8(O, 0xb2) != 0) SU32(O, 0x84, 0x30);
        }
    }
    const int32_t count = I32(world, 0x19d4);
    for (int32_t j = count - 1; j > -1; j--) {
        const rx_addr O = kin_body(world, j);
        if (!(U8(O, 0x84) & 0x10)) continue;
        WORLD_CALL_SP(4, rx_world_kinematic_deactivate(O, world));
        v4_st(O + 0x40, v4_ld(O + 0x60));
        v4_st(O + 0x50, v4_ld(O + 0x70));
        send_pose(O, WCTX(.ebx = world + 0x19d8 + 4 * (uint32_t)j, .edi = O));
        SU32(O, 0xb4, U32(O, 0xb8));
        SU8(O, 0xd0, U8(O, 0xd1));
        world_keyed_refresh(keyed_index(world, O), world + 0x1994, WCTX(.ebx = world + 0x19d8 + 4 * (uint32_t)j, .edi = O));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0076a360, rx_world_kinematic_integrate)

/* A kinematic body's broad-phase box, read from the keyed grid (+0x41c min / +0x420 max arrays, 16 bytes per object). */
static void keyed_box(rx_addr world, uint32_t idx, rx_addr box, rx_addr query) {
    const rx_addr kgrid = U32(U32(world, 0x19ac), 0x30);
    v4_st(box, v4_ld(U32(kgrid, 0x41c) + idx * 16));
    v4_st(query, v4_ld(U32(kgrid, 0x420) + idx * 16));
}

/* 0076b070 Kinematic_CollideStatic (EDI = world; RET 0): per kinematic body in collision (it is keyed object idx; its box comes
 * from the keyed grid), the static broad phase, then per enabled static S the narrow phase, per contact 0076ee60 (the kinematic
 * body's static contact impulse, rx_world_kinematic_static_contact_impulse; ECX = body, EDX = S), the push (min + max of depth *
 * normal, * -scale, clamped; the clamp ratio becomes the scale of later pushes, as in 0076b4a0), the water drag 0084fe00 when
 * 0xda67cc == 1, added to the pose (copied to +0x40), and an event (other = S +0x30). */
void rx_world_kinematic_collide_static(rx_addr world) {
    world_frame f = world_frame_enter(0x788, 2, 1);
    const rx_addr sp = f.esp, rot = sp + 0x70, pos = sp + 0x90, box = sp + 0x80, query = sp + 0xa0;
    const rx_addr rot_s = sp + 0xc0, pos_s = sp + 0x60, set = sp + 0xd0, push_slot = sp + 0x40;
    rx_v4 scale = v4_splat(F32(0, 0x009cc8a8));
    clear_contact_ids(set);
    for (int32_t k = 0; k < I32(world, 0x19d4); k++) {
        const rx_addr O = kin_body(world, k);
        const rx_addr pose = kin_pose(world, k), water = kin_water(world, k);
        v4_st(rot, v4_ld(O + 0x40));
        v4_st(pos, v4_ld(O + 0x50));
        if (!(U8(O, 0x84) & 1)) continue;
        if (world_keyed_count(world) == 0) crt_invalid_parameter_noinfo();
        const uint32_t idx = (uint32_t)((int32_t)(O - U32(world, 0x19a0)) / (int32_t)WORLD_OBJ_SIZE);
        const rx_addr cursor = U32(world, 0x19cc);
        const rx_addr kgrid = U32(U32(world, 0x19ac), 0x30);
        keyed_box(world, idx, box, query);
        world_broadphase_query_boxes(U32(cursor, 0x30), box, query,
                                     WCTX(.eax = idx * 16, .ecx = U32(kgrid, 0x420), .ebx = cursor, .edi = world));
        SU32(cursor, 0, 0);
        for (uint32_t si = world_next_pair(U32(world, 0x19cc)); si != 0xffffffffu; si = world_next_pair(U32(world, 0x19cc))) {
            world_bounds_check(si, world_static_count(world));
            const rx_addr S = U32(world, 0x19c0) + si * 64;
            if (U8(S, 0x38) == 0) continue;
            v4_st(rot_s, v4_ld(S)); v4_st(pos_s, v4_ld(S + 0x10));
            reset_contact_set(set);
            world_contact_dispatch(set, U32(O, 0x2c), pos, rot, U32(S, 0x28), pos_s, rot_s, 0,
                                   WCTX(.eax = O, .edx = pos, .ebx = 0, .esi = S, .edi = world));
            if (U32(set, 0x6a0) == 0) continue;
            const rx_v4 prev_v = v4_ld(pose + 0x10);
            int32_t n = I32(set, 0);
            if (n > 0) {
                uint32_t c = 0;
                rx_addr p = set + 0x10;
                do {
                    WORLD_CALL_SP(8, rx_world_kinematic_static_contact_impulse(O, S, world, p));
                    n = I32(set, 0);
                    c++; p += 0x30;
                } while ((int32_t)c < n);
            }
            rx_v4 lo, hi = contacts_push(set, n, &lo);
            rx_v4 push = v4_mul(v4_add(lo, hi), v4_sub(v4_zero(), scale));
            const rx_v4 len = v4_splat(v4_len_xzy(push));
            const rx_v4 limit = v4_ld(U32(O, 0x20) + 0xc0);
            if (v4_all3(v4_cmplt(limit, len))) { const rx_v4 ratio = v4_div(limit, len); push = v4_mul(push, ratio); scale = ratio; }
            v4_st(push_slot, push);
            if (U32(0, 0x00da67cc) == 1) {
                world_water_filter(water, push_slot, WCTX(.edx = push_slot, .ebx = pose, .esi = O, .edi = world));
                push = v4_ld(push_slot);
            }
            v4_st(pose, v4_add(v4_ld(pose), push));
            v4_st(O + 0x40, v4_ld(pose));
            const float moved = v4_len_xzy(v4_sub(v4_ld(pose + 0x10), prev_v));
            push_event(world, U32(O, 0x34), U32(S, 0x30), 0, 0, set + 0x5b0, set + 0x10, rx_fb(moved), 0);
        }
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0076b070, rx_world_kinematic_collide_static)

/* 0076cd00 Kinematic_CollideKeyed (EDI = world; RET 0): per kinematic body O in collision (it is keyed object oidx; box from the
 * keyed grid), the keyed broad phase, then per other enabled keyed K (byte +0xb0): the narrow phase, per contact 0076ea00 (the
 * kinematic body's keyed contact impulse, rx_world_kinematic_keyed_contact_impulse; ECX = contact, EDX = K), the push (min + max of
 * depth * normal, * -scale; the water drag 0084fe00 when 0xda67d4 == 1; clamped
 * with the ratio kept as the scale). Then: both kinematic (O +0x80 >= 0, K +0x80 >= 0 and K +0x84 bit 0): half the push to each
 * pose, each with its own axis lock; only O: the whole push to O's pose; only K: K's share from 00769f40 (and, as in the
 * original, K's pose copied into O +0x40). The event records the displacements as the original computes them (K's new pose
 * velocity minus O's previous and minus K's previous). A non-kinematic O stops after its first pair. */
void rx_world_kinematic_collide_keyed(rx_addr world) {
    world_frame f = world_frame_enter(0x7a8, 2, 1);
    const rx_addr sp = f.esp, rot = sp + 0x90, pos = sp + 0xb0, box = sp + 0xa0, query = sp + 0xc0;
    const rx_addr rot_k = sp + 0xe0, pos_k = sp + 0x80, set = sp + 0xf0, push_slot = sp + 0x40;
    rx_v4 scale = v4_splat(F32(0, 0x009cc8a8));
    clear_contact_ids(set);
    for (int32_t k = 0; k < I32(world, 0x19d4); k++) {
        const rx_addr O = kin_body(world, k);
        const rx_addr pose = kin_pose(world, k), water = kin_water(world, k);
        v4_st(rot, v4_ld(O + 0x40));
        v4_st(pos, v4_ld(O + 0x50));
        if (!(U8(O, 0x84) & 1)) continue;
        if (world_keyed_count(world) == 0) crt_invalid_parameter_noinfo();
        const rx_addr kcur = U32(world, 0x19ac);
        const rx_addr kgrid = U32(kcur, 0x30);
        const uint32_t oidx = (uint32_t)((int32_t)(O - U32(world, 0x19a0)) / (int32_t)WORLD_OBJ_SIZE);
        keyed_box(world, oidx, box, query);
        world_broadphase_query_boxes(kgrid, box, query, WCTX(.eax = oidx * 16, .ecx = U32(kgrid, 0x420), .ebx = kcur, .edi = world));
        SU32(kcur, 0, 0);
        for (uint32_t ki = world_next_pair(U32(world, 0x19ac)); ki != 0xffffffffu; ki = world_next_pair(U32(world, 0x19ac))) {
            if (world_keyed_count(world) == 0) crt_invalid_parameter_noinfo();
            if (ki == (uint32_t)((int32_t)(O - U32(world, 0x19a0)) / (int32_t)WORLD_OBJ_SIZE)) continue;
            world_bounds_check(ki, world_keyed_count(world));
            const rx_addr K = U32(world, 0x19a0) + ki * WORLD_OBJ_SIZE;
            if (U8(K, 0xb0) == 0) continue;
            v4_st(rot_k, v4_ld(K + 0x40)); v4_st(pos_k, v4_ld(K + 0x50));
            reset_contact_set(set);
            world_contact_dispatch(set, U32(O, 0x2c), pos, rot, U32(K, 0x2c), pos_k, rot_k, 0,
                                   WCTX(.eax = U32(O, 0x2c), .edx = pos, .ebx = O, .esi = K, .edi = world));
            if (U32(set, 0x6a0) == 0) continue;
            const rx_v4 prev_o = v4_ld(pose + 0x10);
            rx_v4 prev_k = v4_ld(WORLD_ZERO4);
            if (I32(K, 0x80) >= 0) prev_k = v4_ld(world + 0x1d10 + 0x50 * U32(K, 0x80));
            int32_t n = I32(set, 0);
            if (n > 0) {
                uint32_t c = 0;
                rx_addr p = set + 0x10;
                do {
                    WORLD_CALL_SP(8, rx_world_kinematic_keyed_contact_impulse(p, K, world, O));
                    n = I32(set, 0);
                    c++; p += 0x30;
                } while ((int32_t)c < n);
            }
            rx_v4 lo, hi = contacts_push(set, n, &lo);
            rx_v4 push = v4_mul(v4_add(lo, hi), v4_sub(v4_zero(), scale));
            v4_st(push_slot, push);
            if (U32(0, 0x00da67d4) == 1) {
                world_water_filter(water, push_slot, WCTX(.edx = push_slot, .edi = world));
                push = v4_ld(push_slot);
            }
            const rx_v4 len = v4_splat(v4_len_xzy(push));
            const rx_v4 limit = v4_ld(U32(O, 0x20) + 0xc0);
            if (v4_all3(v4_cmplt(limit, len))) {
                const rx_v4 ratio = v4_div(limit, len);
                push = v4_mul(push, ratio); scale = ratio; v4_st(push_slot, push);
            }
            const int o_kin = I32(O, 0x80) >= 0;
            const int k_kin = I32(K, 0x80) >= 0 && (U8(K, 0x84) & 1);
            const rx_v4 z = v4_ld(WORLD_ZERO4);
            if (o_kin) {
                if (k_kin) {
                    const rx_v4 half = v4_mul(v4_splat(0.5f /* 0x9322cc */), push);
                    v4_st(pose, v4_add(v4_ld(pose), world_apply_lock(half, U32(O, 0xb4), z)));
                    v4_st(O + 0x40, v4_ld(pose));
                    const rx_v4 hk = world_apply_lock(half, U32(K, 0xb4), v4_ld(WORLD_ZERO4));
                    const rx_addr kpose = world + 0x1d00 + 0x50 * U32(K, 0x80);
                    v4_st(kpose, v4_sub(v4_ld(kpose), hk));
                    v4_st(K + 0x40, v4_ld(world + 0x1d00 + 0x50 * U32(K, 0x80)));
                } else {
                    v4_st(pose, v4_add(v4_ld(pose), world_apply_lock(push, U32(O, 0xb4), z)));
                    v4_st(O + 0x40, v4_ld(pose));
                }
            } else if (k_kin) {
                WORLD_CALL_SP(0, rx_world_vector_apply_axis_lock(push_slot, K));
                const rx_v4 share = v4_ld(push_slot);
                const rx_addr kpose = world + 0x1d00 + 0x50 * U32(K, 0x80);
                v4_st(kpose, v4_sub(v4_ld(kpose), share));
                v4_st(O + 0x40, v4_ld(world + 0x1d00 + 0x50 * U32(K, 0x80)));
            }
            rx_v4 a = v4_ld(WORLD_ZERO4), b = a;
            if (I32(K, 0x80) >= 0) { a = v4_ld(world + 0x1d10 + 0x50 * U32(K, 0x80)); b = a; }
            const float moved1 = v4_len_xzy(v4_sub(a, prev_o));
            const float moved2 = v4_len_xzy(v4_sub(b, prev_k));
            push_event(world, U32(O, 0x34), U32(K, 0x34), 0, 0, set + 0x5b0, set + 0x10, rx_fb(moved1), rx_fb(moved2));
            if (!o_kin) break;
        }
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0076cd00, rx_world_kinematic_collide_keyed)

/* 0076c880 Rider_CollideKeyed (EDI = world; RET 0): the rider bodies (world +0x1980, 0x80 bytes each; the count is taken at
 * entry) against the keyed objects: bounds (shape vfunc 3), the keyed broad phase, then per enabled K the narrow phase, per
 * contact 0076e6c0 (ECX = contact, EDX = K), and for a pushable K (+0x84 bit 5) the push (min + max of depth * normal, * 0.8 *
 * 0.5; K's water drag 0084fe00 when 0xda67d0 == 1; clamped by K's material) onto K's kinematic pose (copied to K +0x40); an event
 * (K's displacement; the rider's is written as 0). EBX / ESI carry values across iterations as in the original (the contact
 * counter, the last cursor / pair index); they reach the callees. */
void rx_world_rider_collide_keyed(rx_addr world) {
    world_frame f = world_frame_enter(0x778, 2, 1);
    const rx_addr sp = f.esp, pos = sp + 0x40, rot = sp + 0x60, box = sp + 0x80, query = sp + 0xa0;
    const rx_addr rot_k = sp + 0xb0, pos_k = sp + 0x90, set = sp + 0xc0, push_slot = sp + 0x70;
    const rx_v4 half = v4_splat(0.5f /* 0x9322cc */);
    clear_contact_ids(set);
    const uint32_t riders = (uint32_t)((I32(world, 0x1984) - I32(world, 0x1980)) >> 7);
    uint32_t ebx = 0, esi = 0;
    for (uint32_t i = 0; i < riders; i++) {
        world_bounds_check(i, (uint32_t)((I32(world, 0x1984) - I32(world, 0x1980)) >> 7));
        const rx_addr R = U32(world, 0x1980) + i * 0x80;
        v4_st(rot, v4_ld(R + 0x40));
        v4_st(pos, v4_ld(R + 0x50));
        const rx_addr shape = U32(R, 0x2c);
        WORLD_CTX(U32(U32(shape, 0), 0xc), &WCTX(.ecx = shape, .ebx = ebx, .esi = esi, .edi = world), 0, pos, rot, box, query);
        const rx_addr kcur = U32(world, 0x19ac);
        world_broadphase_query_boxes(U32(kcur, 0x30), box, query, WCTX(.ecx = query, .ebx = kcur, .edi = world));
        SU32(kcur, 0, 0);
        ebx = kcur;
        uint32_t ki;
        for (ki = world_next_pair(U32(world, 0x19ac)); ki != 0xffffffffu; ki = world_next_pair(U32(world, 0x19ac))) {
            world_bounds_check(ki, world_keyed_count(world));
            const rx_addr K = U32(world, 0x19a0) + ki * WORLD_OBJ_SIZE;
            if (U8(K, 0xb0) == 0) continue;
            v4_st(rot_k, v4_ld(K + 0x40)); v4_st(pos_k, v4_ld(K + 0x50));
            ebx = 0;
            reset_contact_set(set);
            world_contact_dispatch(set, U32(R, 0x2c), pos, rot, U32(K, 0x2c), pos_k, rot_k, 0,
                                   WCTX(.eax = pos, .edx = U32(R, 0x2c), .ebx = 0, .esi = K, .edi = world));
            if (U32(set, 0x6a0) == 0) continue;
            const int pushable = (U8(K, 0x84) & 0x20) != 0;
            rx_v4 prev_k = v4_ld(WORLD_ZERO4);
            if (pushable) prev_k = v4_ld(world + 0x1d10 + 0x50 * U32(K, 0x80));
            int32_t n = I32(set, 0);
            if (n > 0) {
                rx_addr p = set + 0x10;
                do {
                    WORLD_CALL_SP(8, rx_world_rider_keyed_contact_impulse(p, K, world, R));   /* 0076e6c0 (ECX = contact, EDX = K) */
                    n = I32(set, 0);
                    p += 0x30; ebx++;
                } while ((int32_t)ebx < n);
            }
            if (pushable) {
                const rx_v4 scale = v4_splat(F32(0, 0x009cc8a8));
                rx_v4 lo, hi = contacts_push(set, n, &lo);
                rx_v4 push = v4_mul(v4_mul(v4_add(lo, hi), scale), half);
                v4_st(push_slot, push);
                if (U32(0, 0x00da67d0) == 1) {
                    world_water_filter(world + (U32(K, 0x80) + 0x4d0) * 32, push_slot,
                                       WCTX(.eax = push_slot, .edx = K, .ebx = ebx, .esi = K, .edi = world));
                    push = v4_ld(push_slot);
                }
                push = clamp_push(push, v4_ld(U32(K, 0x20) + 0xc0));
                const rx_addr kpose = world + 0x1d00 + 0x50 * U32(K, 0x80);
                v4_st(kpose, v4_add(v4_ld(kpose), push));
                v4_st(K + 0x40, v4_ld(world + 0x1d00 + 0x50 * U32(K, 0x80)));
            }
            rx_v4 now = v4_ld(WORLD_ZERO4);
            if (U8(K, 0x84) & 0x20) now = v4_ld(world + 0x1d10 + 0x50 * U32(K, 0x80));
            const float moved = v4_len_xzy(v4_sub(now, prev_k));
            push_event(world, U32(R, 0x34), U32(K, 0x34), 0, 0, set + 0x5b0, set + 0x10, 0, rx_fb(moved));
        }
        esi = ki;
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0076c880, rx_world_rider_collide_keyed)

/* NaN or +-inf (0xadfdd0) in lanes 0..2 */
static int not_finite3(rx_v4 v, rx_v4 inf, rx_v4 ninf) {
    const rx_v4 ok = v4_cmpeq(v, v);
    if ((v4_lb(ok, 0) & v4_lb(ok, 2) & v4_lb(ok, 1)) == 0) return 1;
    const rx_v4 hit = v4_or(v4_cmpeq(v, inf), v4_cmpeq(v, ninf));
    return (v4_lb(hit, 2) | v4_lb(hit, 1) | v4_lb(hit, 0)) != 0;
}

/* 0076a1e0 Kinematic_ResetLost (ESI = world; RET 0): the kill height is the player's water level (00402dc0 on the list 0x9e30d4
 * (a function-static, initialized here on first use): its +0xdc; else -10000) minus 500. A kinematic body whose pose velocity or
 * position is NaN / infinite, or whose height is below the kill height, is reset: 0076a8b0 (EDX = body, stack world) and
 * 0076abb0 (ECX = world, EDX = body). */
void rx_world_kinematic_reset_lost(rx_addr world) {
    world_frame f = world_frame_enter(0x38, 2, 1);
    if (!(U8(0, 0x009e30e0) & 1)) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
    rx_addr water;
    WORLD_CALL_SP(0, water = rx_world_find_by_controller(0x009e30d4));   /* 00402dc0 */
    float level = -10000.0f /* 0x977bac */;
    if (water != 0) level = F32(water, 0xdc);
    level = phys_sub(level, 500.0f /* 0x9778ec */);
    const rx_v4 kill = v4_splat(level);
    for (int32_t k = 0; k < I32(world, 0x19d4); k++) {
        const rx_addr pose = kin_pose(world, k);
        const rx_addr O = kin_body(world, k);
        const rx_v4 inf = v4_ld(0x00adfdd0), ninf = v4_sub(v4_zero(), inf);
        int reset = not_finite3(v4_ld(pose + 0x10), inf, ninf);
        if (!reset) {
            const rx_v4 p = v4_ld(pose);
            reset = not_finite3(p, inf, ninf) || v4_all3(v4_cmplt(v4_lane(p, 1), kill));
        }
        if (!reset) continue;
        WORLD_CALL_SP(4, rx_world_kinematic_deactivate(O, world));
        WORLD_CALL_SP(0, rx_world_kinematic_reset_authored_pose(world, O));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0076a1e0, rx_world_kinematic_reset_lost)

/* World_QueryShapeContacts' copy of a contact set into the results (at most 10 in all) */
static uint32_t take_contacts(rx_addr set, uint32_t count, uint32_t type, uint32_t id, rx_addr types, rx_addr ids, rx_addr results) {
    const int32_t n = I32(set, 0);
    if (U32(set, 0x6a0) == 0 || n <= 0) return count;
    for (int32_t k = 0; k < n; k++) {
        if (count >= 10) continue;
        const rx_addr rec = contact(set, k), out = results + 0x30 * count;
        SU32(types, 4 * count, type);
        SU32(ids, 4 * count, id);
        v4_st(out, v4_ld(rec)); v4_st(out + 0x10, v4_ld(rec + 0x10)); v4_st(out + 0x20, v4_ld(rec + 0x20));
        count++;
    }
    return count;
}

/* 007729f0 World_QueryShapeContacts (ECX = int types[10] out, EDX = shape; stdcall world, pos, results, ids; RET 0x10; EAX =
 * the count): what a shape at a position touches in the physics world. Results: 10 contacts of 0x30 bytes (point, normal,
 * depth), initialized to (1e10, 0, -1e10). Order: water (the player's water body, 00402dc0, 007c5950, then the depth test against
 * the shape's radius 008454c0: type 0x10, id water +0xe4), static objects (type 1, id S +0x30), keyed objects (type 2, id +0x34),
 * generic dynamic objects (type 4, id +0x34), the terrain (00667e40, type 8, id the terrain); at most 10 contacts, stopping early
 * once 10 are found. The shape's rotation is the identity 0xd61640. */
uint32_t rx_world_query_shape_contacts(rx_addr types, rx_addr shape, rx_addr world, rx_addr pos, rx_addr results, rx_addr ids) {
    world_frame f = world_frame_enter(0x724, 3, 1);
    const rx_addr sp = f.esp, box = sp + 0x50, query = sp + 0x60, set = sp + 0x70, tmp_a = sp + 0x30, tmp_b = sp + 0x40;
    const rx_v4 far_pt = v4_splat_bits(0x501502f9u /* 1e10, 0x977b88 */), far_depth = v4_splat_bits(0xd01502f9u /* -1e10, 0x977d60 */);
    for (uint32_t k = 0; k < 10; k++) {
        const rx_addr r = results + 0x30 * k;
        v4_st(r, far_pt); v4_st(r + 0x10, v4_ld(WORLD_ZERO4)); v4_st(r + 0x20, far_depth);
    }
    clear_contact_ids(set);
    uint32_t count = 0;
    WORLD_CTX(U32(U32(shape, 0), 8), &WCTX(.ecx = shape, .ebx = world, .esi = pos, .edi = 0), 0, WORLD_IDENTITYQ, pos, box, query);
    if (!(U8(0, 0x009e30e0) & 1)) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
    rx_addr water;
    WORLD_CALL_SP(0, water = rx_world_find_by_controller(0x009e30d4));   /* 00402dc0 */
    SU32(tmp_a, 0, U32(pos, 0)); SU32(tmp_a, 4, U32(pos, 4)); SU32(tmp_a, 8, U32(pos, 8));
    if (water != 0 && world_water_contains(water, tmp_a, WCTX(.ebx = world, .esi = pos)) != 0) {
        const float dot = phys_hsum3(v4_mul(v4_ld(pos), v4_ld(0x00d616a0)));
        const float radius = world_shape_radius(shape, WCTX(.ebx = world, .esi = pos));
        const float depth = phys_sub(dot, radius);
        if (!(F32(tmp_a, 4) < depth)) {
            SU32(types, 0, 0x10);
            SU32(ids, 0, U32(water, 0xe4));
            count = 1;
        }
    }
    /* static objects */
    {
        const rx_addr cursor = U32(world, 0x19cc);
        world_broadphase_query_boxes(U32(cursor, 0x30), box, query, WCTX(.ebx = world, .edi = count));
        SU32(cursor, 0, 0);
    }
    for (uint32_t idx = world_next_pair(U32(world, 0x19cc)); idx != 0xffffffffu; idx = world_next_pair(U32(world, 0x19cc))) {
        world_bounds_check(idx, world_static_count(world));
        const rx_addr S = U32(world, 0x19c0) + idx * 64;
        if (U8(S, 0x38) == 0) continue;
        v4_st(tmp_a, v4_ld(S)); v4_st(tmp_b, v4_ld(S + 0x10));
        reset_contact_set(set);
        world_contact_dispatch(set, shape, WORLD_IDENTITYQ, pos, U32(S, 0x28), tmp_b, tmp_a, 0,
                               WCTX(.eax = shape, .edx = pos, .ebx = world, .esi = S, .edi = count));
        count = take_contacts(set, count, 1, U32(S, 0x30), types, ids, results);
    }
    if (count != 10) {
        const rx_addr kcur = U32(world, 0x19ac);
        world_broadphase_query_boxes(U32(kcur, 0x30), box, query, WCTX(.ebx = world, .edi = count));
        SU32(kcur, 0, 0);
        for (uint32_t idx = world_next_pair(U32(world, 0x19ac)); idx != 0xffffffffu; idx = world_next_pair(U32(world, 0x19ac))) {
            world_bounds_check(idx, world_keyed_count(world));
            const rx_addr K = U32(world, 0x19a0) + idx * WORLD_OBJ_SIZE;
            if (U8(K, 0xb0) == 0) continue;
            v4_st(tmp_b, v4_ld(K + 0x40)); v4_st(tmp_a, v4_ld(K + 0x50));
            reset_contact_set(set);
            world_contact_dispatch(set, shape, WORLD_IDENTITYQ, pos, U32(K, 0x2c), tmp_a, tmp_b, 0,
                                   WCTX(.eax = shape, .edx = pos, .ebx = world, .esi = 0, .edi = count));
            count = take_contacts(set, count, 2, U32(K, 0x34), types, ids, results);
        }
        if (count != 10) {
            const rx_addr gcur = U32(world, 0x196c);
            world_broadphase_query_boxes(U32(gcur, 0x30), box, query, WCTX(.ebx = world, .edi = count));
            SU32(gcur, 0, 0);
            for (uint32_t idx = world_next_pair(U32(world, 0x196c)); idx != 0xffffffffu; idx = world_next_pair(U32(world, 0x196c))) {
                world_bounds_check(idx, world_obj_count(world));
                const rx_addr G = U32(world, 0x1960) + idx * WORLD_OBJ_SIZE;
                v4_st(tmp_b, v4_ld(G + 0x40)); v4_st(tmp_a, v4_ld(G + 0x50));
                reset_contact_set(set);
                world_contact_dispatch(set, shape, WORLD_IDENTITYQ, pos, U32(G, 0x2c), tmp_a, tmp_b, 0,
                                       WCTX(.eax = U32(G, 0x2c), .edx = shape, .ebx = world, .esi = G, .edi = count));
                count = take_contacts(set, count, 4, U32(G, 0x34), types, ids, results);
            }
            if (count != 10) {
                const rx_addr T = U32(world, 0xcc9c);
                if (T != 0 && U32(T, 0x31ec) != 0) {
                    reset_contact_set(set);
                    WORLD_CALL_SP(0x18, rx_world_grid_report_overlaps(shape, set, U32(T, 0x31ec), pos, WORLD_IDENTITYQ, sp + 0x10, 0,
                                                                      sp + 0x14));   /* Grid_ReportOverlaps 00667e40 */
                    count = take_contacts(set, count, 8, U32(world, 0xcc9c), types, ids, results);
                }
            }
        }
    }
    world_frame_leave(&f);
    return count;
}
RX_FUNCTION(0x007729f0, rx_world_query_shape_contacts)

/* 00770e80 Vehicle_CollectNearbyStatics (EDI = list, ECX = query box max; stdcall world, box min; RET 8): the static objects near
 * a vehicle into its list (simulation +0x1300: a vector of 0x50-byte records, begin +0xc, end +0x10). The list is cleared
 * (Vector16_EraseRange 004db5b0), the static broad phase run, and every enabled static S whose shape has +0x10 bit 2 appended
 * while below the capacity (004db570) through 00773d10: S's rotation and position, its broad-phase box, id S +0x30 and S +0x24.
 * The record's last 8 bytes are the original's stale stack (it builds the record in its frame): built at the same address,
 * they are the same bytes. */
void rx_world_vehicle_collect_nearby_statics(rx_addr list, rx_addr query, rx_addr world, rx_addr box) {
    world_frame f = world_frame_enter(0x78, 2, 1);
    const rx_addr sp = f.esp, rec = sp + 0x20;
    const uint32_t end0 = U32(list, 0x10);
    if (U32(list, 0xc) > end0) crt_invalid_parameter_noinfo();
    if (U32(list, 0xc) > U32(list, 0x10)) crt_invalid_parameter_noinfo();
    WORLD_CALL_SP(0x14, (void)rx_world_record_vector_erase(list, sp + 0x18, U32(list, 0), U32(list, 0xc), U32(list, 0), end0));   /* 004db5b0 */
    const rx_addr cursor = U32(world, 0x19cc);
    world_broadphase_query_boxes(U32(cursor, 0x30), box, query, WCTX(.ecx = query, .ebx = world, .edi = list));
    SU32(cursor, 0, 0);
    for (uint32_t idx = world_next_pair(U32(world, 0x19cc)); idx != 0xffffffffu; idx = world_next_pair(U32(world, 0x19cc))) {
        world_bounds_check(idx, world_static_count(world));
        const rx_addr S = U32(world, 0x19c0) + idx * 64;
        if (U8(S, 0x38) == 0) continue;
        if (!((U32(U32(S, 0x28), 0x10) >> 2) & 1)) continue;
        const rx_addr sgrid = U32(U32(world, 0x19cc), 0x30);
        v4_st(rec, v4_ld(S));
        v4_st(rec + 0x10, v4_ld(S + 0x10));
        v4_st(rec + 0x20, v4_ld(U32(sgrid, 0x41c) + idx * 16));
        v4_st(rec + 0x30, v4_ld(U32(sgrid, 0x420) + idx * 16));
        SU32(rec, 0x40, U32(S, 0x30));
        SU32(rec, 0x44, S + 0x24);
        uint32_t cap;
        WORLD_CALL_SP(0, cap = rx_world_record_vector_size(list));   /* 004db570 */
        const uint32_t n = (uint32_t)((int32_t)(U32(list, 0x10) - U32(list, 0xc)) / 0x50);
        if (n >= cap) continue;
        WORLD_CALL_SP(4, rx_world_body_record_vector_push_back(list, rec));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x00770e80, rx_world_vehicle_collect_nearby_statics)
