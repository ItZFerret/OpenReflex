/* fx_particle.c -- the 2D particle managers (PlatformParticleManager2D: the global one at 0x00c11bd0 and the additive one at
 * 0x00c11d80) and their per-frame update: ParticleEntity::UpdateManagers / Tick, ParticleManager2D_Update 00698510 (submits two
 * Particle_UpdateJob 006925f0 jobs to the engine job pool), the job itself (ageing, curves, drag, integration, colour, the dead
 * list and its compaction), SpawnParticle 00698780, Particle_Copy 00692b50 and the small manager virtuals. notes/fx.md.
 *
 * Manager M (0x1f0 + ... bytes): +0x14 particle array (0x80-byte records), +0x1c capacity, +0x20 disabled, +0x70 emitter asset
 * holder (+0x44 asset data, +0x4c busy flag), +0x8c atlas texture, +0xac live count (snapshot), +0xb0 live count, +0xb4 dead
 * list (index array; entries from 1), +0xb8 dead count + 1, +0xbc next index of the jobs, +0xc4 jobs running, +0xc8 spin lock,
 * +0xcc readers, +0xdc paused, +0xf4 CRITICAL_SECTION.
 * Particle P (0x80): +0x00 position (4 lanes), +0x10 velocity, +0x20 acceleration, +0x38 rotation, +0x3c spin, +0x40 scale,
 * +0x44 birth time, +0x48 life, +0x4c colour (A R G B bytes from the high byte), +0x54 frame, +0x58 frame rate, +0x5c emitter
 * (-> [+0] type holder: +0x10 type index, +0x14 alpha scale). Particle type T (0x1f0 records at asset data +0x10): +0xcc / +0xd0
 * frame range, +0xe4 loop length, +0x10c / +0x110 scale / colour curve offsets (curves: floats at 0xa817b0), +0x12c..+0x15c
 * curve gains (scale +0x12c / +0x130 / +0x13c, colour channels +0x134..+0x154), +0x158 colour keep mask, +0x15c drag. */
#include "fx.h"
#include "fx_util.h"
#include "loop_entity.h"
#include "loop_jobs.h"
#include <stdlib.h>

/* the register model's product backend (fx_util.h; the oracle binds the emulated CPU's registers instead) */
#ifndef RX_ORACLE
/* the register model: the running guest thread's (rx_cpu, w-jobthreads) */
uint32_t fx_reg(int r) { return rx_cpu_cur()->fx_regs[r & 7]; }
void fx_set_reg(int r, uint32_t v) { if (r != FX_ESP) rx_cpu_cur()->fx_regs[r & 7] = v; }
#endif

/* ---- lanes ---- */
static inline rx_v4 v4m(rx_v4 a, rx_v4 b) { rx_v4 r; for (int i = 0; i < 4; i++) r.v[i] = rx_muls(a.v[i], b.v[i]); return r; }
static inline rx_v4 v4a(rx_v4 a, rx_v4 b) { rx_v4 r; for (int i = 0; i < 4; i++) r.v[i] = rx_adds(a.v[i], b.v[i]); return r; }
static inline rx_v4 v4s(float x) { rx_v4 r = {{x, x, x, x}}; return r; }

/* ---- 00692b50 Particle_Copy (ECX = src, EAX = dst): the 0x80-byte record through x87 loads / stores (the w lanes +0x0c / +0x1c /
 * +0x2c are not copied; +0x4c / +0x50 / +0x5c..+0x64 by mov; +0x68 as a qword; +0x70..+0x7c twice, +0x78 first by mov). */
void rx_fx_particle_copy(rx_addr src, rx_addr dst) {
    static const uint8_t f[] = {0x00, 0x04, 0x08, 0x10, 0x14, 0x18, 0x20, 0x24, 0x28, 0x30, 0x34, 0x38, 0x3c, 0x40, 0x44, 0x48};
    for (unsigned i = 0; i < sizeof f; i++) fx_x87_copy(dst + f[i], src + f[i]);
    SU32(dst, 0x4c, U32(src, 0x4c));
    SU32(dst, 0x50, U32(src, 0x50));
    fx_x87_copy(dst + 0x54, src + 0x54);
    fx_x87_copy(dst + 0x58, src + 0x58);
    SU32(dst, 0x5c, U32(src, 0x5c));
    SU32(dst, 0x60, U32(src, 0x60));
    SU32(dst, 0x64, U32(src, 0x64));
    fx_x87_copy64(dst + 0x68, src + 0x68);
    const uint32_t w78 = U32(src, 0x78);
    fx_x87_copy(dst + 0x70, src + 0x70);
    SU32(dst, 0x78, w78);
    fx_x87_copy(dst + 0x74, src + 0x74);
    fx_x87_copy(dst + 0x70, src + 0x70);
    fx_x87_copy(dst + 0x74, src + 0x74);
    fx_x87_copy(dst + 0x78, src + 0x78);
    fx_x87_copy(dst + 0x7c, src + 0x7c);
}
RX_FUNCTION(0x00692b50, rx_fx_particle_copy)

/* a direct call of Particle_Copy at the current ESP (its return address below) */
static void particle_copy_call(rx_addr src, rx_addr dst, rx_addr ret) {
    const rx_addr s = fx_push(0, 0, ret);
    rx_fx_particle_copy(src, dst);
    fx_pop(s);
}

/* _copysign(x, y) called with ESP = sp (the two doubles at sp, sp + 8; return address `ret`): its frame (saved EBP = ebp, the
 * result's two dwords) stays below as the original leaves it. */
static double copysign_call(double x, double y, rx_addr ret, uint32_t ebp) {
    const rx_addr sp = rx_sp();
    const double r = crt_copysign(x, y);
    SU32(sp, -4, ret); SU32(sp, -8, ebp);
    SU32(sp, -0x10, (uint32_t)rx_db(x)); SU32(sp, -0xc, (uint32_t)(rx_db(r) >> 32));
    return r;
}
/* _ftol2_sse of st(0) called at ESP = sp: its frame (return address, saved EBP, the value as a double at (sp - 16) & ~7) */
static int32_t ftol2_sse_call(rx_f80 x, rx_addr ret, uint32_t ebp) {
    const rx_addr sp = rx_sp();
    SU32(sp, -4, ret); SU32(sp, -8, ebp);
    if (U32(0, 0x00dcf760) != 0) rx_wr64((sp - 16) & ~7u, rx_db(rx_f80_to_f64(x, 0)));
    return phys_ftol2_sse(x, FX_X87_BITS);
}

/* the drag constant's static init (guard bit 0 of 0x00db1400, the splat at 0x00db13f0); in a parallel part (w-jobthreads) once,
 * under the jobs' lock, the guard re-read there */
static void drag_init(void) {
    rx_jobs_cs_enter();
    if ((U8(0, 0x00db1400) & 1) == 0) {
        SU8(0, 0x00db1400, U8(0, 0x00db1400) | 1);
        rx_st4(0x00db13f0, v4s(rx_bf(0xba9ef377u) /* 0x9776fc */));
    }
    rx_jobs_cs_leave();
}

/* One particle of Particle_UpdateJob's loop (below): E the job's frame, ebp its saved EBP; a dead particle goes onto the manager's
 * dead list, or into `dead` (a parallel part, w-jobthreads). */
typedef struct { int32_t* idx; int32_t n; } fx_dead_sink;
static void particle_step(rx_addr E, uint32_t ebp, int32_t i, float dt, fx_dead_sink* dead) {
    const rx_addr p = ((uint32_t)i << 7) + U32(E, 0x64);
    const rx_addr holder = U32(U32(p, 0x5c), 0);
    const rx_addr T = U32(holder, 0x10) * 0x1f0u + U32(U32(E, 0x50), 0x10);
    SF32(E, 0x54, F32(holder, 0x14));
    const float birth = F32(p, 0x44), life = F32(p, 0x48);
    const float now = F32(E, 0x5c);
    if (now >= rx_adds(birth, life)) {                   /* dead: onto the dead list */
        if (dead) { dead->idx[dead->n++] = i; return; }   /* (a parallel part: the list is joined in index order after) */
        const rx_addr m = U32(E, 0x28);
        const uint32_t k = fx_interlocked_inc(m + 0xb8, 0x006926ce) - 1;
        SU32(U32(m, 0xb4), 4u * k, (uint32_t)i);
    } else {
        SF32(E, 0x60, F32(T, 0x140)); SF32(E, 0x3c, F32(T, 0x148)); SF32(E, 0x48, F32(T, 0x150)); SF32(E, 0x4c, F32(T, 0x134));
        const float age = rx_subs(now, birth);
        const float rl = rx_rcp(life);
        SF32(E, 0x70, rl); SU32(E, 0x74, 0); SU32(E, 0x78, 0); SU32(E, 0x7c, 0);
        float t = rx_muls(age, rl);
        SF32(E, 0x2c, t);
        if (!(1.0f /* 0x931f24 */ > t)) { t = 1.0f; SF32(E, 0x2c, t); }
        const float s = rx_muls(t, 9.0f /* 0x977700 */);
        SF32(E, 0x58, s);
        const int32_t idx = rx_cvtt(s);
        const double d = (double)s - rx_bd(0x3fdffffffffd4050ull) /* 0x977558: 0.49999999999 */;
        SF64(E, 0x68, d);
        /* sub esp, 0x10: the arguments of _copysign(0.5, d) */
        rx_set_sp(E - 0x10);
        rx_wr64(E - 0x10 + 8, fx_x87_bits64(rx_db(d)));
        SF64(E - 0x10, 0, 0.5 /* 0x9774e8 */);
        const double c = copysign_call(0.5, d, 0x00692783, ebp);
        const rx_f80 rnd = fx_add(rx_f80_from_f64(c), rx_f80_from_f64(F64(E, 0x68)));
        rx_set_sp(E);
        const int32_t n = ftol2_sse_call(rnd, 0x0069278f, ebp);
        /* x87: the colour channels (truncating fistp), the frac and rotation by SSE in between */
        const rx_f80 tt = fx_ld(E + 0x2c);
        rx_f80 a = fx_mul(fx_ld(T + 0x154), tt);
        const float fn = (float)n;
        SU16(E, 0x16, FX_CW);
        a = fx_add(a, fx_ld(E + 0x48));
        SU32(E, 0x18, FX_CW | 0xc00u);
        const float frac = rx_subs(F32(E, 0x58), fn);
        const float rot = rx_subs(F32(p, 0x38), rx_muls(F32(p, 0x3c), dt));
        rx_wr64(E + 0x70, (uint64_t)rx_f80_to_int(a, 3));
        SF32(E, 0x44, frac);
        const uint32_t c0 = U32(E, 0x70);
        a = fx_mul(fx_ld(T + 0x14c), tt);
        SU16(E, 0x16, FX_CW);
        SU32(E, 0x18, FX_CW | 0xc00u);
        a = fx_add(a, fx_ld(E + 0x3c));
        rx_wr64(E + 0x30, (uint64_t)rx_f80_to_int(a, 3));
        const uint32_t c1 = U32(E, 0x30);
        a = fx_mul(tt, fx_ld(T + 0x144));
        SU16(E, 0x16, FX_CW);
        SF32(p, 0x38, rot);
        a = fx_add(a, fx_ld(E + 0x60));
        const rx_v4 acc = rx_ld4(p + 0x20), vel0 = rx_ld4(p + 0x10), pos = rx_ld4(p);
        SU32(E, 0x18, FX_CW | 0xc00u);
        const rx_v4 vel = v4a(v4m(acc, v4s(dt)), vel0);
        rx_wr64(E + 0x18, (uint64_t)rx_f80_to_int(a, 3));
        const uint32_t c2 = U32(E, 0x18);
        if ((U8(0, 0x00db1400) & 1) == 0) drag_init();    /* the drag constant splat (static init) */
        const uint32_t k = U32(T, 0x110) + (uint32_t)idx;
        SU16(E, 0x16, FX_CW);
        const rx_v4 zero = rx_ld4(0x00d61670);
        SU32(E, 0x18, fx_x87_bits(U32(0x00a817b0 + 4u * k, 0)));
        const rx_f80 ck1 = fx_ld(0x00a817b4 + 4u * k);
        const rx_f80 ck = fx_ld(E + 0x18);
        const rx_v4 sq = v4m(vel, vel);
        rx_f80 diff = fx_sub(ck1, ck);
        const float len2 = rx_adds(rx_adds(sq.v[0], sq.v[1]), sq.v[2]);
        diff = fx_mul(diff, fx_ld(E + 0x44));
        rx_f80 alpha = fx_add(ck, diff);
        const rx_v4 l2 = v4a(v4s(len2), v4s(rx_bf(0x3727c5acu) /* 0x9774a4: 1e-5 */));
        alpha = fx_mul(alpha, fx_ld(T + 0x138));
        alpha = fx_add(alpha, fx_ld(E + 0x4c));
        rx_v4 dv = v4m(v4s(F32(T, 0x15c)), l2);
        dv = v4m(dv, rx_ld4(0x00db13f0));
        const rx_f80 ascale = fx_mul(fx_ld(E + 0x54), fx_f(255.0f /* 0x92e104 */));
        SU32(E, 0x18, FX_CW | 0xc00u);
        const uint32_t k2 = U32(T, 0x10c) + (uint32_t)idx;
        rx_v4 rs; for (int l = 0; l < 4; l++) rs.v[l] = rx_rsqrt(l2.v[l]);
        alpha = fx_mul(alpha, ascale);
        dv = v4m(dv, rs);
        dv = v4a(dv, rx_ld4(0x00d61650));
        rx_v4 drag; for (int l = 0; l < 4; l++) drag.v[l] = rx_maxss(zero.v[l], dv.v[l]);
        rx_wr64(E + 0x18, (uint64_t)rx_f80_to_int(alpha, 3));
        const float s0 = F32(0x00a817b0 + 4u * k2, 0), s1 = F32(0x00a817b4 + 4u * k2, 0);
        const uint32_t c3 = U32(E, 0x18);
        float sc = rx_adds(rx_muls(rx_subs(s1, s0), frac), s0);
        sc = rx_adds(rx_muls(sc, F32(T, 0x130)), F32(T, 0x12c));
        SF32(p, 0x40, sc);
        const float sc2 = rx_muls(F32(T, 0x13c), sc);
        const rx_v4 nvel = v4m(drag, vel);
        SF32(p, 0x40, sc2);
        float fr = rx_adds(rx_muls(F32(p, 0x58), dt), F32(p, 0x54));
        const rx_v4 npos = v4a(v4m(nvel, v4s(dt)), pos);
        SF32(p, 0x54, fr);
        const float lo = F32(T, 0xcc);
        const float hi = rx_subs(rx_adds(F32(T, 0xd0), lo), rx_bf(0x38d1b717u) /* 0x9775a4: 1e-4 */);
        SU32(E, 0x18, c3);
        if (fr >= hi) fr = rx_subs(rx_adds(lo, fr), rx_muls(F32(T, 0xe4), hi));
        SF32(p, 0x54, fr);
        if (fr >= hi) fr = hi;
        SF32(p, 0x54, fr);
        /* colour: keep the bytes of the mask, take the curve channels elsewhere */
        const uint32_t m = U32(T, 0x158), nm = ~m;
        uint32_t col = ((uint32_t)U8(p, 0x4e) & m) | (nm & c2) | (c3 << 8);
        const uint32_t b4d = (uint32_t)U8(p, 0x4d) & m, keep = m & U32(p, 0x4c);
        col = (col << 8) | b4d;
        col |= nm & c1;
        col <<= 8;
        col |= keep & 0xffu;
        col |= nm & c0;
        SU32(p, 0x4c, col);
        rx_st4(p + 0x10, nvel);
        rx_st4(p, npos);
    }
}

#ifndef RX_ORACLE
/* w-jobthreads (loop_jobs.h): the job's particles in parallel parts. The loop of the job that runs (the first of the two the
 * manager submits takes every index: the second finds none) is split into parts of consecutive indices, each run as its own worker
 * thread with the job's frame and locals; a particle's step reads the shared type and curve data and writes only its own record.
 * The dead particles each part finds are joined onto the manager's dead list in index order afterwards, as the one loop puts them,
 * and the index counter +0xbc is left where that loop leaves it (count + 1). Then the job goes on (the compaction) as before. */
#define FX_PART_MIN 192    /* particles per part at least */
#define FX_PARTS_MAX 16
typedef struct { rx_addr mgr, asset, parts; float time, dt; int32_t count, lo, hi; fx_dead_sink dead; } fx_part;
static fx_part g_part[FX_PARTS_MAX];
static int32_t* g_part_dead;
static int32_t g_part_dead_cap;
static void particle_part_body(rx_addr k) {
    fx_part* const P = &g_part[k];
    fx_frame f; fx_frame_enter(&f, 0x74, 16, FX_EBX, FX_ESI, FX_EDI);   /* the job's frame, as Particle_UpdateJob's */
    const rx_addr E = f.E;
    SF32(E, 0x5c, P->time);
    SU32(E, 0x28, P->mgr); SU32(E, 0x24, (uint32_t)P->count); SU32(E, 0x50, P->asset); SF32(E, 0x40, P->dt); SU32(E, 0x64, P->parts);
    for (int32_t i = P->lo; i < P->hi; i++) particle_step(E, f.ebp, i, P->dt, &P->dead);
    fx_frame_leave(&f);
}
/* indices [i, count) of the job (E its frame, at the loop): 1 when done here, 0 when the loop should run them */
static int particles_parallel(rx_addr E, int32_t i, int32_t count) {
    if (count - i < 2 * FX_PART_MIN || !rx_jobs_on(RX_JOB_PARTICLES)) return 0;
    int parts = (count - i) / FX_PART_MIN;
    if (parts > FX_PARTS_MAX) parts = FX_PARTS_MAX;
    if (count > g_part_dead_cap) {
        int32_t* b = (int32_t*)realloc(g_part_dead, sizeof(int32_t) * (size_t)count);
        if (!b) return 0;
        g_part_dead = b; g_part_dead_cap = count;
    }
    rx_job jobs[FX_PARTS_MAX];
    const int32_t span = count - i;
    for (int k = 0; k < parts; k++) {
        fx_part* const P = &g_part[k];
        P->mgr = U32(E, 0x28); P->asset = U32(E, 0x50); P->parts = U32(E, 0x64);
        P->time = F32(E, 0x5c); P->dt = F32(E, 0x40); P->count = count;
        P->lo = i + (int32_t)((int64_t)span * k / parts); P->hi = i + (int32_t)((int64_t)span * (k + 1) / parts);
        P->dead.idx = g_part_dead + P->lo; P->dead.n = 0;
        jobs[k].body = particle_part_body; jobs[k].arg = (rx_addr)k; jobs[k].ctx = RX_JOB_CTX_PART + k;
        jobs[k].tls = U32(rx_teb(), 0x2c);
    }
    rx_jobs_run(jobs, parts);
    const rx_addr m = U32(E, 0x28);
    for (int k = 0; k < parts; k++)
        for (int32_t d = 0; d < g_part[k].dead.n; d++) {   /* the dead list in index order, as the loop builds it */
            const uint32_t at = fx_interlocked_inc(m + 0xb8, 0x006926ce) - 1;
            SU32(U32(m, 0xb4), 4u * at, (uint32_t)g_part[k].dead.idx[d]);
        }
    SU32(m, 0xbc, (uint32_t)count + 1u);   /* the loop's last increment (the index past the end) */
    return 1;
}
#endif

/* ---- 006925f0 Particle_UpdateJob (cdecl job {+0 manager, +4 count, +8 asset data, +0x10 time, +0x14 dt}; runs on a job pool
 * worker, x87 at 53 bits): takes particle indices from manager +0xbc until `count`; a particle past birth + life goes to the
 * dead list, the others age: t = min((time - birth) * rcp(life), 1), curve position s = 9 t, i = trunc(s), frac = s - n with n =
 * the rounded s - 0.49999999999 (_copysign + _ftol2_sse); colour channels c0..c2 = trunc(gain t + base) (x87), alpha c3 =
 * trunc((curve(i) lerp frac) gain + base) (alpha scale * 255); velocity v' = max(0, drag |v|^2 ... ) * (v + a dt), position +=
 * v' dt; scale from the scale curve; frame += rate dt with its loop. The last job out (jobs running -> 0) compacts: dead
 * particles are overwritten by live ones from the end (Particle_Copy), the counts set, the spin lock released.
 * Frame: push ebp; and esp, -16; sub esp, 0x74; ebx / esi / edi; locals at E = A - 0x80: +0x16 cw, +0x18 c2 / c3, +0x24 count,
 * +0x28 manager, +0x2c t, +0x30 c1, +0x3c / +0x48 / +0x4c / +0x60 type gains, +0x40 dt, +0x44 frac, +0x50 asset data, +0x54
 * alpha scale, +0x58 s, +0x5c time, +0x64 array, +0x68 s - 0.5 (double), +0x70 rcp(life) vector / c0. */
void rx_fx_particle_update_job(rx_addr job) {
    fx_frame f; fx_frame_enter(&f, 0x74, 16, FX_EBX, FX_ESI, FX_EDI);
    const rx_addr E = f.E;
    const rx_addr tls = fx_tls_block();
    const float time = F32(job, 0x10);
    const rx_addr asset = U32(job, 8);
    const int32_t count = I32(job, 4);
    const rx_addr mgr = U32(job, 0);
    SU32(tls, 8, job);
    const rx_addr parts = U32(mgr, 0x14);
    SF32(E, 0x5c, time);
    const float dt = F32(job, 0x14);
    rx_set_sp(E - 4);                                         /* push eax */
    SU32(E, 0x28, mgr); SU32(E, 0x24, (uint32_t)count); SU32(E, 0x50, asset); SF32(E, 0x40, dt); SU32(E, 0x64, parts);
    rx_set_sp(E);
    int32_t i = (int32_t)fx_interlocked_inc(mgr + 0xbc, 0x00692649) - 1;
    SU32(fx_tls_block(), 4, (uint32_t)i);
    (void)tls;
#ifndef RX_ORACLE
    if (i < count && particles_parallel(E, i, count)) i = count;
#endif
    while (i < count) {
        particle_step(E, f.ebp, i, dt, NULL);
        i = (int32_t)fx_interlocked_inc(U32(E, 0x28) + 0xbc, 0x00692a75) - 1;
        if (!(i < I32(E, 0x24))) break;
    }
    int32_t n = I32(E, 0x24);
    if (fx_interlocked_dec(mgr + 0xc4, 0x00692a97) == 0) {     /* the last job out compacts */
        int32_t dead = I32(mgr, 0xb8) - 1;
        const rx_addr arr = U32(mgr, 0x14);
        SU32(0, 0x00a94270, U32(0, 0x00a94270) - (uint32_t)dead);
        SU32(E, 0x30, arr);
        if (dead >= n) { SU32(E, 0x24, 0); n = 0; }
        else if (dead > 0) {
            rx_addr last = ((uint32_t)n << 7) + arr;
            while (n > 0) {
                n--;
                last -= 0x80;
                SU32(E, 0x70, last);
                if (n >= 0 && n < I32(mgr, 0x1c)) {
                    const int32_t d = I32(U32(mgr, 0xb4), 4u * (uint32_t)dead);
                    if (d < n) particle_copy_call(last, ((uint32_t)d << 7) + arr, 0x00692af6);
                }
                if (--dead <= 0) break;
            }
        }
        SU32(mgr, 0xac, (uint32_t)n);
        SU32(mgr, 0xb0, (uint32_t)n);
        if (fx_interlocked_cmpxchg(mgr + 0xc8, 0, 1, 0x00692b22) != 1) {
            do fx_sleep(0, 0x00692b34);
            while (fx_interlocked_cmpxchg(mgr + 0xc8, 0, 1, 0x00692b3b) != 1);
        }
    }
    fx_frame_leave(&f);
}
RX_FUNCTION(0x006925f0, rx_fx_particle_update_job)

/* ---- 00698510 ParticleManager2D_Update (thiscall manager, dt; RET 4): with an emitter asset and not disabled: under the
 * manager's critical section, wait for readers (+0xcc) and take the spin lock (+0xc8); snapshot the live count, reset the job
 * index and the dead list. Unless the global pause [0xc11bc4] is set (then release and return): dt clamped to 0.03333; the
 * global rate [0x9c46b8] = [0x9c46bc]; when not paused (+0xdc, the timing's +0xc, the level world's +0x20014) two
 * Particle_UpdateJob jobs {manager, live - 1, asset data, time, dt; +0x20 = 2} are submitted (the last one releases the lock),
 * else the lock is released here. Frame: push ebp; and esp, -16; sub esp, 0x44; ebx / esi / edi; E = A - 0x50: +0xc the
 * critical section, +0x10 the job record. */
void rx_fx_particle_manager_update(rx_addr mgr, float dt) {
    fx_frame f; fx_frame_enter(&f, 0x44, 16, FX_EBX, FX_ESI, FX_EDI);
    fx_set_reg(FX_ESI, mgr);
    const rx_addr E = f.E;
    if (U32(mgr, 0x70) != 0 && U32(mgr, 0x20) == 0) {
        SU32(0, 0x00d9d3d4, U32(0, 0x00d9d3d4) + 1);
        const rx_addr cs = mgr + 0xf4;
        fx_set_reg(FX_EDI, cs);
        rx_set_sp(E - 4); SU32(E, 0xc, cs); rx_set_sp(E);
        fx_enter_cs(cs, 0x00698549);
        while (U32(mgr, 0xcc) != 0) {
            fx_leave_cs(cs, 0x0069856a);
            fx_sleep(0, 0x00698572);
            fx_enter_cs(cs, 0x00698579);
        }
        fx_set_reg(FX_EBX, U32(0, 0x0091609c));                  /* ebx = &InterlockedCompareExchange */
        fx_set_reg(FX_EDI, mgr + 0xc8);
        if (fx_interlocked_cmpxchg(mgr + 0xc8, 1, 0, 0x006985a6) != 0) {
            do fx_sleep(0, 0x006985b8);
            while (fx_interlocked_cmpxchg(mgr + 0xc8, 1, 0, 0x006985bf) != 0);
        }
        SU32(mgr, 0xac, U32(mgr, 0xb0));
        SU32(mgr, 0xbc, 0);
        SU32(mgr, 0xb8, 1);
        fx_set_reg(FX_EBX, 1);
        if (U32(0, 0x00c11bc4) != 0) {
            if (fx_interlocked_cmpxchg(mgr + 0xc8, 0, 1, 0x00698601) != 1) {
                do fx_sleep(0, 0x00698614);
                while (fx_interlocked_cmpxchg(mgr + 0xc8, 0, 1, 0x0069861b) != 1);
            }
            fx_leave_cs(U32(E, 0xc), 0x0069862b);
            fx_frame_leave(&f);
            return;
        }
        const rx_addr timing = fx_timing();
        float d = dt;
        const float time = F32(timing, 0x40);
        const float dmax = rx_bf(0x3d08850au) /* 0x977804: 0.03333 */;
        if (d > dmax) d = dmax;
        SU32(0, 0x009c46b8, U32(0, 0x009c46bc));
        int run = 0;
        if (U32(mgr, 0xdc) == 0 && U32(timing, 0xc) == 0) {
            const rx_addr lw = fx_level_world(U32(0, FX_G_GAME), 0x00698685, mgr);
            run = U32(lw, 0x20014) == 0;
        }
        if (run) {
            const rx_addr holder = U32(mgr, 0x70);
            SU32(holder, 0x4c, 1);
            const uint32_t live = U32(mgr, 0xac);
            const rx_addr data = U32(holder, 0x44);
            SU32(0, 0x00a9428c, U32(0, 0x00a9428c) + live);
            SU32(mgr, 0xc4, 2);
            const uint32_t cnt = U32(mgr, 0xac) - 1;
            const rx_addr rec = E + 0x10;
            SU32(rec, 0, mgr); SU32(rec, 8, data); SF32(rec, 0x14, d); SF32(rec, 0x10, time); SU32(rec, 0x20, 2); SU32(rec, 4, cnt);
            for (uint32_t j = 2; j > 0; j--) {
                fx_set_reg(FX_EDI, j);
                const uint32_t a[2] = {rec, 0x006925f0};
                const rx_addr s = fx_push(2, a, 0x006986e1);
                const rx_addr pool = rx_jobpool_instance();
                rx_set_sp(s - 8);
                fx_jobpool_submit(pool, rec, 0x006925f0, 0x006986e7);
                fx_pop(s);
            }
            fx_set_reg(FX_EDI, 0);
            SU32(U32(mgr, 0x70), 0x4c, 0);
        } else {
            if (fx_interlocked_cmpxchg(mgr + 0xc8, 0, 1, 0x006986ff) != 1) {
                do fx_sleep(0, 0x0069870c);
                while (fx_interlocked_cmpxchg(mgr + 0xc8, 0, 1, 0x00698713) != 1);
            }
        }
        const rx_addr c2 = U32(E, 0xc);
        SU32(U32(mgr, 0x70), 0x4c, 0);
        fx_leave_cs(c2, 0x0069872d);
    }
    fx_frame_leave(&f);
}
RX_FUNCTION(0x00698510, rx_fx_particle_manager_update)

/* ---- 00698780 PlatformParticleManager2D::SpawnParticle (vf1; thiscall manager, emitter, template*; RET 8): with an emitter
 * asset: wait for the spin lock to be free, claim slot +0xb0 (full: give it back), copy the template there, set its emitter,
 * wrap its start frame into the asset's frame count (+0x44 -> +0x1c). Frame: push edi; push ebx; push esi (E = sp0 - 0xc). */
void rx_fx_particle_spawn(rx_addr mgr, rx_addr emitter, rx_addr tmpl) {
    const rx_addr sp0 = rx_sp();
    SU32(sp0 - 4, 0, fx_reg(FX_EDI));                          /* push edi */
    if (U32(mgr, 0x70) == 0) return;
    const rx_addr E = sp0 - 0xc;
    SU32(sp0 - 8, 0, fx_reg(FX_EBX)); SU32(sp0 - 0xc, 0, fx_reg(FX_ESI));   /* push ebx; push esi */
    rx_set_sp(E);
    if (U32(mgr, 0xc8) != 0) {
        SU32(E - 4, 0, fx_reg(FX_EBP));                       /* push ebp */
        rx_set_sp(E - 4);
        do fx_sleep(0, 0x006987b4);
        while (U32(mgr, 0xc8) != 0);
        rx_set_sp(E);
    }
    const int32_t idx = (int32_t)fx_interlocked_inc(mgr + 0xb0, 0x006987d6) - 1;
    if (idx >= I32(mgr, 0x1c)) {
        fx_interlocked_dec(mgr + 0xb0, 0x006987e3);
        rx_set_sp(sp0);
        return;
    }
    const rx_addr p = ((uint32_t)idx << 7) + U32(mgr, 0x14);
    particle_copy_call(tmpl, p, 0x006987fa);
    SU32(p, 0x5c, emitter);
    const rx_addr data = U32(U32(mgr, 0x70), 0x44);
    SU32(U32(mgr, 0x70), 0x4c, 1);
    const int32_t frames = I32(data, 0x1c);
    SU32(U32(mgr, 0x70), 0x4c, 0);
    const float fr = F32(p, 0x54);
    const int32_t fi = rx_cvtt(fr);
    if (fi > frames) {
        const int32_t q = fi / frames;
        SF32(p, 0x54, rx_subs(fr, rx_muls((float)q, (float)frames)));
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00698780, rx_fx_particle_spawn)

/* ---- 00691ba0 PlatformParticleManager2D::vf7: manager +0xa8 != 1 */
uint32_t rx_fx_particle_manager_vf7(rx_addr mgr) { return U32(mgr, 0xa8) != 1; }
RX_FUNCTION(0x00691ba0, rx_fx_particle_manager_vf7)

/* ---- 00698740 PlatformParticleManager2D::vf5 (stdcall out*; RET 4): out +0x4 / +0x8 / +0xc / +0x10 = 0 (EAX = out, ECX = 0) */
rx_addr rx_fx_particle_manager_vf5(rx_addr self, rx_addr out) {
    (void)self;
    SU32(out, 0xc, 0); SU32(out, 8, 0); SU32(out, 4, 0); SU32(out, 0x10, 0);
    return out;
}
RX_FUNCTION(0x00698740, rx_fx_particle_manager_vf5)

/* ---- 00698760 PlatformParticleManager2D::vf4 (stdcall a, b, out*; RET 0xc): out = {a, 0, 0, b}; returns 1 (ECX = 0, EDX = a) */
uint32_t rx_fx_particle_manager_vf4(rx_addr self, uint32_t a, uint32_t b, rx_addr out) {
    (void)self;
    SU32(out, 0xc, b); SU32(out, 8, 0); SU32(out, 0, a); SU32(out, 4, 0);
    return 1;
}
RX_FUNCTION(0x00698760, rx_fx_particle_manager_vf4)

/* ---- 00691d80 (shared vf4 of the particle effect emitters / controllers): this + 0x10 */
rx_addr rx_fx_emitter_vf4(rx_addr self) { return self + 0x10; }
RX_FUNCTION(0x00691d80, rx_fx_emitter_vf4)

/* ---- 00691d90 (shared vf5: enable(on); RET 4): +0x2c = on; switching on from off sets +0x30 (restart) */
void rx_fx_emitter_vf5(rx_addr self, uint32_t on) {
    if (on == 0) { SU32(self, 0x2c, 0); return; }
    const uint32_t was = U32(self, 0x2c);
    SU32(self, 0x2c, on);
    if (was == 0) SU32(self, 0x30, 1);
}
RX_FUNCTION(0x00691d90, rx_fx_emitter_vf5)

/* ---- 006960c0 ParticleEntity::UpdateManagers (vf21; thiscall entity, dt; RET 4): ParticleManager2D_Update of both managers
 * (+0xc0 the global one, +0xc4 the additive one), dt passed through fld / fstp. Frame: push ebp; and esp, -8; push ecx; push
 * esi; the argument slot below. */
void rx_fx_particle_entity_update_managers(rx_addr ent, float dt) {
    const fx_saved in = fx_save();
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4) & ~7u;
    SU32(sp0 - 4, 0, in.ebp); fx_set_reg(FX_EBP, sp0 - 4);   /* push ebp; mov ebp, esp */
    const float d = rx_bf(fx_x87_bits(rx_fb(dt)));
    SU32(A - 4, 0, ent);                                      /* push ecx */
    SU32(A - 8, 0, in.esi);                                   /* push esi */
    fx_set_reg(FX_ESI, ent);
    rx_set_sp(A - 8);
    uint32_t a[1] = {rx_fb(d)};
    rx_addr s = fx_push(1, a, 0x006960dc);
    rx_fx_particle_manager_update(U32(ent, 0xc0), d);
    fx_pop(s);
    s = fx_push(1, a, 0x006960ee);
    rx_fx_particle_manager_update(U32(ent, 0xc4), d);
    fx_pop(s);
    rx_set_sp(sp0);
    fx_restore(in);
}
RX_FUNCTION(0x006960c0, rx_fx_particle_entity_update_managers)

/* ---- 00696100 ParticleEntity::Tick (vf19; thiscall entity, dt; RET 4): hands the emitter asset (+0xb0) to both managers'
 * +0x70 under their critical sections and the atlas texture (+0xb8) to their +0x8c. Frame: push ecx / ebx / ebp / esi / edi;
 * the ecx slot (sp0 - 4) ends holding the asset. */
void rx_fx_particle_entity_tick(rx_addr ent, float dt) {
    (void)dt;
    const rx_addr sp0 = rx_sp();
    SU32(sp0 - 4, 0, ent);
    SU32(sp0 - 8, 0, fx_reg(FX_EBX)); SU32(sp0 - 0xc, 0, fx_reg(FX_EBP));
    SU32(sp0 - 0x10, 0, fx_reg(FX_ESI)); SU32(sp0 - 0x14, 0, fx_reg(FX_EDI));
    rx_set_sp(sp0 - 0x14);
    const uint32_t asset = U32(ent, 0xb0);
    rx_addr m = U32(ent, 0xc0);
    fx_enter_cs(m + 0xf4, 0x00696120);
    SU32(m, 0x70, asset);
    fx_leave_cs(m + 0xf4, 0x0069612c);
    m = U32(ent, 0xc4);
    SU32(sp0 - 4, 0, U32(ent, 0xb0));
    fx_enter_cs(m + 0xf4, 0x00696149);
    SU32(m, 0x70, U32(sp0 - 4, 0));
    fx_leave_cs(m + 0xf4, 0x00696153);
    SU32(U32(ent, 0xc0), 0x8c, U32(ent, 0xb8));
    SU32(U32(ent, 0xc4), 0x8c, U32(ent, 0xb8));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00696100, rx_fx_particle_entity_tick)
