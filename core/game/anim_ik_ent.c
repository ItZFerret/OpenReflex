/* anim_ik_ent.c -- the animated scene entities of 0x006b (AnimEntity* loops: spline-driven and single-animation props,
 * ThirtySecondBoard, DropGate, LeaderBoard, CrewChief): their jobs and the samplers they use.
 *
 * AnimEntitySplineAnimLoop (vtable 0095f83c; ...WithParticles 0095f904, ...WithStopLogic 0095f9cc) moves along a spline
 * animation: object +0x370 (+0x44 its track set: +0x20 rotation track count, +0x24 -> rotation track; +0x18 position track
 * count, +0x1c -> position track; +0x4c busy flag while sampled), character data at +0x148 (Character_SampleKeyframes),
 * spline time +0x364, length +0x368, speed +0x36c, animation speed +0x374, the three row scales +0x378 / +0x37c / +0x380,
 * follow mode +0x384 (leader +0x39c, lag +0x388), pause at the end +0x38c (pausing +0x390, pause length +0x394, left +0x398),
 * sound events +0x3ac (count) / +0x3b0 (0x1c-byte records: time, sound id, ?, min distance, max distance, ...).
 * Rotation track: +0x00 -> keys (12 bytes: time, packed rotation u16 angle / int16 x y z), +0x04 cached segment, +0x0c enabled,
 * +0x10 key count. Position track: +0x00 -> keys (16 bytes: time, x y z), +0x08 interpolated, +0x0c key count. */
#include "anim_ik.h"
#include "anim.h"
#include "build_xml.h"
#include "build_alloc.h"
#include "crt.h"
#include "phys_terrain.h"
#include <string.h>
#include "loop_cam_entity.h"
#include "loop_engine.h"
#include "loop_entity.h"
#include "anim_math.h"
#include "phys_vec.h"
#include "loop_jobs.h"

/* ---- callees outside the cluster ---- */

/* MSVCR90 calls (IAT) through the core's CRT (crt.h), with the original's argument pushes and return address on the dead stack
 * (rx_sp() = ESP before the pushes). */
static int32_t ent_stricmp(rx_addr a, rx_addr b, rx_addr ret) {          /* _stricmp (0x9162bc) */
    const rx_addr sp = rx_sp();
    SU32(sp - 4, 0, b); SU32(sp - 8, 0, a); SU32(sp - 12, 0, ret);
    rx_set_sp(sp - 12);
    const int32_t r = crt_stricmp(a, b);
    rx_set_sp(sp);
    return r;
}
static uint32_t ent_atoi(rx_addr s, rx_addr ret) {   /* atoi (0x916220) */
    const rx_addr sp = rx_sp();
    SU32(sp - 4, 0, s); SU32(sp - 8, 0, ret);
    return (uint32_t)crt_atoi(s);
}
static int32_t ent_sprintf1(rx_addr buf, rx_addr fmt, uint32_t arg, rx_addr ret) {   /* sprintf (0x916204), one argument */
    const rx_addr sp = rx_sp();
    SU32(sp - 4, 0, arg); SU32(sp - 8, 0, fmt); SU32(sp - 12, 0, buf); SU32(sp - 16, 0, ret);
    rx_set_sp(sp - 16);                            /* the callee runs below its arguments */
    const int32_t r = crt_sprintf(buf, fmt, sp - 4);
    rx_set_sp(sp);
    return r;
}

/* 00835430 Character_SampleKeyframes (rx_anim: me; stack: character data; xmm1 = time step; RET 4). */
static void character_sample_keyframes(rx_addr data, float step, float xmm0, rx_addr ret, uint32_t ebx, uint32_t ebp) {
    (void)xmm0; (void)ebx; (void)ebp;
    const rx_addr sp0 = ik_enter(1, &data, ret);
    rx_character_sample_keyframes(data, step);
    rx_set_sp(sp0);
}
/* 0042ce40 QuatSlerp_Packed (math; EAX = packed key a, ESI = packed key b, EDI = quaternion out; cdecl float t). */
static void quat_slerp_packed(rx_addr a, rx_addr b, rx_addr out, uint32_t t_bits, rx_addr ret, uint32_t ebx, uint32_t ebp) {
    (void)ebx; (void)ebp;
    const rx_addr sp0 = ik_enter(1, &t_bits, ret);
    phys_quat_slerp_packed(a, b, out, rx_bf(t_bits));
    rx_set_sp(sp0);
}

/* ---- the spline sampler ---- */

/* 006bb6e0 (stack: spline entity, time, matrix out; RET 0xc): the entity's spline pose at `time` as a 4x4 matrix (rows 0x10
 * apart; translation row +0x30, [3][3] = 1). Rotation: the cached segment of the rotation track (searched from the cache
 * forward / backward; a fast reset when the time is before the quarter key or past the 3/4 key), the two packed keys slerped
 * (0042ce40) -- or, at the track's ends, the outer key decoded as is; position: the position track's segment lerped
 * (004dbce0), its last key at or before the time when not interpolated (004dbe00), else the 0xa95d50 vector. With no rotation
 * track the quaternion is the stale stack the original reads. The matrix's z row is negated (-(row 1 of the rotation)). */
void rx_spline_entity_sample(rx_addr ent, float t, rx_addr out) {
    ANIM_FRAME_ENTER(A, 0x80);                     /* sub esp, 0x74; push ebx; push esi; push edi */
    const rx_addr ebp = A_sp0 - 4, q = A - 0x60, pos = A - 0x50;
    const rx_addr obj = U32(ent, 0x370);
    SU32J(obj, 0x4c, 1);
    const rx_addr ts = U32(obj, 0x44);
    SU32(A - 0x68, 0, ts);
    if (I32(ts, 0x20) > 0) {
        const rx_addr rt = U32(ts, 0x24);
        const int32_t n = I32(rt, 0x10);
        if (U32(rt, 0x0c) != 0 && n >= 1) {
            int interp = 1;
            SU32(A - 0x6c, 0, 1);
            rx_addr a, b;
            if (n <= 2) {
                const rx_addr keys = U32(rt, 0);
                float frac;
                if (n == 2) { b = keys + 0xc; frac = phys_div(phys_sub(t, F32(keys, 0)), phys_sub(F32(b, 0), F32(keys, 0))); }
                else { b = keys; frac = 0.0f; }
                a = keys;
                SF32(A - 0x70, 0, frac);
            } else {
                const int32_t last = n - 1, quarter = last >> 2, mid = (last >> 1) + quarter;
                SU32(A - 0x70, 0, (uint32_t)last);
                /* the cached segment (+4) is the track's (shared): read once, stored back as the batch's last job leaves it */
                int32_t i = I32(rt, 4);
                if (i > mid && F32(U32(rt, 0), 12u * (uint32_t)quarter) > t) i = 1;
                else if (i < quarter && t > F32(U32(rt, 0), 12u * (uint32_t)mid)) i = last;
                if (i < 1) i = 1;
                if (t > F32(U32(rt, 0), 12u * (uint32_t)i)) {
                    do {
                        i++;
                        if (i >= n) { i = last; interp = 0; break; }
                    } while (t > F32(U32(rt, 0), 12u * (uint32_t)i));
                }
                if (interp) interp = (int)U32(A - 0x6c, 0);
                const rx_addr keys = U32(rt, 0);
                while (F32(keys, 12u * (uint32_t)i - 12u) > t) {
                    i--;
                    if (i < 1) { i = 1; interp = 0; break; }
                }
                rx_job_wr32(rt + 4, (uint32_t)i);
                b = keys + 12u * (uint32_t)i;
                a = b - 12u;
                SF32(A - 0x70, 0, phys_div(phys_sub(t, F32(a, 0)), phys_sub(F32(b, 0), F32(a, 0))));
            }
            if (interp) {
                SU32(A - 0x84, 0, ik_x87_bits(U32(A - 0x70, 0)));
                rx_set_sp(A - 0x84);
                quat_slerp_packed(a + 4, b + 4, q, ik_x87_bits(U32(A - 0x70, 0)), 0x006bb834, b + 4, ebp);
                rx_set_sp(A - 0x80);
            } else {
                /* the key itself, decoded as anim_packed_quat does, with the original's frame stores and Float_Abs call */
                float ang = (float)U16(b, 4) * 0.0007669904f;                    /* 0x9778fc: pi / 4096 */
                const float sg = anim_sign(ang);
                ang = rx_muls(ang, sg);
                const float red = anim_trig_reduce(ang);
                const float t2 = red * red, t4 = t2 * t2;
                const float c = rx_subs(-0.0f, anim_cos_poly(t2, t4));
                const float k = rx_muls(rx_muls(anim_sin_poly(t2, t4) * red, sg), 3.051851e-05f);   /* 0x9c6400 */
                const float x = (float)(int16_t)U16(b, 6) * k, y = (float)(int16_t)U16(b, 8) * k, z = (float)(int16_t)U16(b, 10) * k;
                SF32(q, 0, x); SF32(q, 4, y); SF32(q, 8, z);
                const float flip = anim_sign(c);
                SF32(A - 0x6c, 0, flip);
                SU32(A - 0x84, 0, 0x006bbabc); SU32(A - 0x88, 0, ebp);   /* Float_Abs 00401000 */
                SF32(q, 0xc, fabsf(c));
                SF32(q, 0, rx_muls(x, flip)); SF32(q, 4, rx_muls(y, flip)); SF32(q, 8, rx_muls(z, flip));
            }
        }
    }
    /* position */
    SU32(pos, 0, U32(0, 0x00a95d50)); SU32(pos, 4, U32(0, 0x00a95d54)); SU32(pos, 8, U32(0, 0x00a95d58));
    const rx_addr ts2 = U32(A - 0x68, 0);
    if (I32(ts2, 0x18) > 0) {
        const rx_addr pt = U32(ts2, 0x1c);
        const int32_t count = I32(pt, 0x0c);
        if (U32(pt, 8) != 0 && count > 1) {
            const uint32_t inside = anim_keytrack_find_segment(U32(pt, 0), (uint32_t)count, t, A - 0x6c, A - 0x68, A - 0x70);
            SU32(A - 0x90, 0, 0x006bb8a9);
            const float f = F32(A - 0x70, 0);
            const rx_addr p = U32(A - 0x6c, 0), nx = U32(A - 0x68, 0);
            const rx_addr base = inside ? p : nx;   /* inside: prev + (next - prev) f; clamped: next + (next - prev) f */
            SF32(pos, 0, phys_add(phys_mul(phys_sub(F32(nx, 4), F32(p, 4)), f), F32(base, 4)));
            SF32(pos, 4, phys_add(phys_mul(phys_sub(F32(nx, 8), F32(p, 8)), f), F32(base, 8)));
            SF32(pos, 8, phys_add(phys_mul(phys_sub(F32(nx, 12), F32(p, 12)), f), F32(base, 12)));
        } else {
            const rx_addr keys = U32(pt, 0);
            const uint32_t idx = anim_keytrack_upper(keys, (uint32_t)count, t);
            SU32(A - 0x84, 0, 0x006bbb68);
            const rx_addr k = keys + 16u * idx;
            SU32(pos, 0, U32(k, -12)); SU32(pos, 4, U32(k, -8)); SU32(pos, 8, U32(k, -4));
        }
    }
    /* the rotation matrix (2x terms as the original groups them) */
    const float qx = F32(q, 0), qy = F32(q, 4), qz = F32(q, 8), qw = F32(q, 0xc);
    const float y2 = phys_mul(qy, 2.0f), x2 = phys_mul(qx, 2.0f), z2 = phys_mul(qz, 2.0f);
    const float yy = phys_mul(y2, qy), xx = phys_mul(x2, qx);
    const float zz = phys_mul(qz, z2);
    SF32(A - 0x68, 0, zz);
    const float wx = phys_mul(x2, qw);
    SF32(A - 0x70, 0, wx);
    const float wy = phys_mul(y2, qw), wz = phys_mul(z2, qw);
    SU32J(U32(ent, 0x370), 0x4c, 0);
    SF32(A - 0x6c, 0, wy);
    const float xy = phys_mul(x2, qy), xz = phys_mul(qz, x2), yz = phys_mul(qz, y2);
    const float m01 = phys_add(xy, wz);
    SF32(A - 0x3c, 0, m01);
    SF32(out, 0x00, phys_sub(1.0f, phys_add(yy, zz)));
    SF32(out, 0x04, m01);
    SF32(A - 0x64, 0, wz);
    const float m10 = phys_sub(xy, wz);
    SF32(A - 0x30, 0, m10);
    SF32(out, 0x20, phys_sub(-0.0f, m10));
    SF32(out, 0x28, phys_sub(-0.0f, phys_add(wx, yz)));
    SU32(out, 0x30, U32(pos, 0));
    SF32(out, 0x08, phys_sub(xz, wy));
    SU32(out, 0x34, U32(pos, 4));
    SF32(out, 0x0c, 0.0f);
    SF32(out, 0x10, phys_add(xz, wy));
    SF32(out, 0x14, phys_sub(yz, wx));
    SF32(out, 0x18, phys_sub(1.0f, phys_add(yy, xx)));
    SF32(out, 0x1c, 0.0f);
    SF32(out, 0x24, phys_sub(-0.0f, phys_sub(1.0f, phys_add(xx, zz))));
    SF32(out, 0x2c, 0.0f);
    SU32(out, 0x38, U32(pos, 8));
    SF32(out, 0x3c, 1.0f);
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x006bb6e0, rx_spline_entity_sample)

/* ---- the spline loop's job ---- */

/* An entity's virtual call, thiscall with one argument (the original: the classes' slots are not in the cluster). */
static void ent_vcall1(rx_addr ent, uint32_t slot_off, uint32_t arg, rx_addr ret) {
    ik_call(U32(U32(ent, 0), slot_off), RX_THISCALL, ent, 1, &arg, ret);
}
/* The sound calls of a spline event (audio, 0x0080xxxx, not in the cluster), with the registers the original has: they run
 * on a null sound object (EDI / EAX / ESI = 0), so they read and write low memory (0xa0 / 0xa4 are written here too). */
static void sound_event_play(float id, rx_addr ret, uint32_t ebx, uint32_t ebp, uint32_t esi) {     /* 008078d0 (xmm0, EDI = 0) */
    rx_regs r = anim_regs(); r.xmm[0].v[0] = id; r.r[ANIM_EBX] = ebx; r.r[ANIM_EBP] = ebp; r.r[ANIM_ESI] = esi;
    ik_call_ex(0x008078d0, &r, 0, 0, 0, ret);
}
static void sound_set_distances(uint32_t min_bits, uint32_t max_bits, rx_addr ret, uint32_t ebx, uint32_t ebp, uint32_t esi) {
    rx_regs r = anim_regs(); r.r[ANIM_EBX] = ebx; r.r[ANIM_EBP] = ebp; r.r[ANIM_ESI] = esi;   /* 00807dc0 (EAX = 0; RET 8) */
    uint32_t a[2] = {min_bits, max_bits};
    ik_call_ex(0x00807dc0, &r, 0, 2, a, ret);
}
static void sound_set_position(rx_addr pos, uint32_t mode, uint32_t value_bits, rx_addr ret, uint32_t ebx, uint32_t ebp) {
    rx_regs r = anim_regs(); r.r[ANIM_EAX] = pos; r.r[ANIM_EBX] = ebx; r.r[ANIM_EBP] = ebp;    /* 008077c0 (EAX, ESI = 0; RET 8) */
    uint32_t a[2] = {mode, value_bits};
    ik_call_ex(0x008077c0, &r, 0, 2, a, ret);
}

/* 006bb010 (cdecl job: [arg] = spline entity, [arg + 4] = dt): AnimEntitySplineAnimLoop's update. Samples its character
 * animation (Character_SampleKeyframes, step = dt * +0x374; or, on the first update with +0x16c and a clip +0x1f4, restarts it),
 * fires the sound events whose time lies in (previous, current) animation time (+0x188), advances the spline time (follow
 * mode: the leader's time - lag; pausing at the end), wraps it at the length, samples the spline (006bb6e0), scales the rows
 * by +0x378 / +0x37c / +0x380 and hands the matrix to the entity (vtable slot 16). */
void rx_spline_entity_job(rx_addr job) {
    ANIM_FRAME_ENTER(A, 0x70);                     /* sub esp, 0x64; push ebx; push esi; push edi */
    const rx_addr ebp = A_sp0 - 4, m = A - 0x40;
    const rx_addr ent = U32(job, 0);
    const float dt = F32(job, 4);
    SF32(A - 0x54, 0, dt);
    SF32(A - 0x60, 0, 0.0f);
    rx_addr clip;
    if (U32(ent, 0x16c) != 0 && (clip = U32(ent, 0x1f4)) != 0) {
        SU32(ent, 0x16c, 0); SU32(ent, 0x170, 0); SU32(ent, 0x174, clip); SF32(ent, 0x184, 0.0f);
    } else {
        SU32(A - 0x60, 0, U32(ent, 0x188));
        character_sample_keyframes(ent + 0x148, phys_mul(F32(ent, 0x374), dt), dt, 0x006bb08b, ent, ebp);
    }
    SU32(A - 0x5c, 0, 0);
    if (I32(ent, 0x3ac) > 0) {
        rx_addr ev = ent + 0x3b0;
        SU32(A - 0x58, 0, ev);
        int32_t i = 0;
        do {
            const float et = F32(ev, 0);
            if (et > F32(A - 0x60, 0) && F32(ent, 0x188) > et) {
                rx_job_ordered();   /* the audio calls and their low-memory words: in the batch's order (w-jobthreads2) */
                ent_vcall1(ent, 0x44, A - 0x50, 0x006bb0de);                /* the sound's position */
                sound_event_play(F32(ev, 4), 0x006bb0ea, ent, ebp, ev);
                SU32(0, 0xa0, ik_x87_bits(U32(ev, 0xc)));
                const float mx = F32(ev, 0x10);
                SF32(0, 0xa4, mx);
                if (F32(0, 0xa0) == mx) SF32(0, 0xa4, phys_add(mx, 0.25f));  /* 0x92ff84 */
                sound_set_distances(ik_x87_bits(U32(0, 0xa0)), ik_x87_bits(U32(0, 0xa4)), 0x006bb13e, ent, ebp, ev);
                sound_set_position(A - 0x50, 5, 0xbf800000u /* -1.0 (0x9cd9e8) */, 0x006bb155, ent, ebp);
                ev = U32(A - 0x58, 0);
            }
            i++;
            ev += 0x1c;
            SU32(A - 0x5c, 0, (uint32_t)i);
            SU32(A - 0x58, 0, ev);
        } while (i < I32(ent, 0x3ac));
    }
    /* the spline time (a follower reads its leader's: in a parallel entity-job batch both are ordered, w-jobthreads2) */
#ifndef RX_ORACLE
    if (rx_jobs_parallel() && ((U32(ent, 0x384) != 0 && U32(ent, 0x39c) != 0) || rx_ent_job_is_lead(ent))) rx_job_ordered();
#endif
    const uint32_t pause_at_end = U32(ent, 0x38c);
    rx_addr lead;
    if (pause_at_end != 0 && U32(ent, 0x390) != 0) {
        const float left = phys_sub(F32(ent, 0x398), dt);
        SF32(ent, 0x398, left);
        if (0.0f > left) SU32(ent, 0x390, 0);
    } else {
        float x;
        if (U32(ent, 0x384) != 0 && (lead = U32(ent, 0x39c)) != 0 && F32(lead, 0x364) > F32(ent, 0x364))
            x = phys_sub(F32(lead, 0x364), F32(ent, 0x388));
        else x = phys_add(phys_mul(F32(ent, 0x36c), dt), F32(ent, 0x364));
        SF32(ent, 0x364, x);
    }
    const float x = F32(ent, 0x364), len = F32(ent, 0x368);
    if (x > len) {
        float w;
        if (U32(ent, 0x384) != 0 && (lead = U32(ent, 0x39c)) != 0) w = phys_sub(F32(lead, 0x364), F32(ent, 0x388));
        else w = phys_sub(x, len);
        SF32(ent, 0x364, w);
        if (pause_at_end) { SU32(ent, 0x390, 1); SU32(ent, 0x398, ik_x87_bits(U32(ent, 0x394))); }
    }
    /* sample: push out, time (x87), entity; return address */
    const uint32_t tb = ik_x87_bits(U32(ent, 0x364));
    SU32(A - 0x74, 0, m); SU32(A - 0x78, 0, tb); SU32(A - 0x7c, 0, ent); SU32(A - 0x80, 0, 0x006bb26e);
    rx_set_sp(A - 0x80);
    rx_spline_entity_sample(ent, rx_bf(tb), m);
    rx_set_sp(A - 0x70);
    for (int r = 0; r < 3; r++) {
        const float sc = F32(ent, 0x378 + 4 * r);
        SF32(m, 0x10 * r + 0, phys_mul(sc, F32(m, 0x10 * r + 0)));
        SF32(m, 0x10 * r + 4, phys_mul(sc, F32(m, 0x10 * r + 4)));
        SF32(m, 0x10 * r + 8, phys_mul(sc, F32(m, 0x10 * r + 8)));
    }
    ent_vcall1(ent, 0x40, m, 0x006bb330);
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x006bb010, rx_spline_entity_job)

/* ---- AnimEntitySingleAnimLoop ---- */

/* 00835bf0 (rx_anim: me; ECX = clip; stack: character data, phase (x87); RET 8): the clip sampled at the phase. */
static void clip_sample_at(rx_addr clip, rx_addr data, uint32_t phase_bits, rx_addr ret) {
    const uint32_t a[2] = {data, phase_bits};
    const rx_addr sp0 = ik_enter(2, a, ret);
    rx_character_sample_motion_at(clip, data, rx_bf(phase_bits));
    rx_set_sp(sp0);
}

/* 006ba170 AnimEntitySingleAnimLoop_Job (cdecl job: [arg] = entity, [arg + 4] = dt): the loop phase +0x358 advances by
 * dt / (the clip's length, [[+0x144] + 0x44] + 0x4c) * speed +0x354 (marking [+0x144] + 0x4c busy meanwhile) and wraps at 1;
 * when the entity is due (its update frame +0x340 + 5 not below the scene's frame [[0xa95844] + 0x10] + 0x54, or forced +0x744)
 * and visible (+0x740), the clip +0x1f4 is sampled into the character data +0x148 at the phase (00835bf0). */
void rx_single_anim_loop_job(rx_addr job) {
    const rx_addr ent = U32(job, 0);
    rx_addr o = U32(ent, 0x144);
    SU32J(o, 0x4c, 1);
    const float step = phys_mul(phys_div(F32(job, 4), F32(U32(o, 0x44), 0x4c)), F32(ent, 0x354));
    SU32J(U32(ent, 0x144), 0x4c, 0);
    const float ph = phys_add(F32(ent, 0x358), step);
    SF32(ent, 0x358, ph);
    if (ph > 1.0f) SF32(ent, 0x358, phys_sub(ph, 1.0f));
    if ((U32(ent, 0x340) + 5u >= U32(U32(U32(0, 0x00a95844), 0x10), 0x54) || U32(ent, 0x744) != 0) && U32(ent, 0x740) != 0)
        clip_sample_at(U32(ent, 0x1f4), ent + 0x148, ik_x87_bits(U32(ent, 0x358)), 0x006ba21a);
}
RX_FUNCTION(0x006ba170, rx_single_anim_loop_job)

/* ---- 006a5b30: a state record's assignment ---- */

/* 0063ddd0 (EAX = destination, ECX = source; leaf, not in the cluster): the record's +0x70 member assigned (0x60 bytes). */
static void member_assign_0063ddd0(rx_addr dst, rx_addr src, rx_addr ret, uint32_t ebx, uint32_t ebp) {
    (void)ebx; (void)ebp;
    const rx_addr sp0 = ik_enter(0, 0, ret);
    rx_camera_copy_block(src, dst);
    rx_set_sp(sp0);
}

/* 006a5b30 (stdcall (destination, source); RET 8; returns the destination in EAX, ECX = 0 after its rep movsd): operator= of a
 * 0x1dc-byte state record (used by OnFieldCrowd and Engine::Update): the floats +0x10..+0x64 through the x87 (+0x1c, +0x2c,
 * +0x3c, +0x4c padding skipped), +0x68 raw, the +0x70 member (0063ddd0), +0xd0..+0xdc raw, the three 4x4 matrices +0xe0 /
 * +0x120 / +0x160 raw (rep movsd), the floats +0x1a0..+0x1d8 through the x87 (+0x1ac, +0x1bc, +0x1cc skipped). */
rx_addr rx_state_record_assign(rx_addr dst, rx_addr src) {
    static const uint8_t f1[] = {0x10, 0x14, 0x18, 0x20, 0x24, 0x28, 0x30, 0x34, 0x38, 0x40, 0x44, 0x48, 0x50, 0x54, 0x58, 0x5c, 0x60, 0x64};
    const rx_addr sp = rx_sp();                    /* push ebx; push ebp; push esi; push edi: the callee runs at sp - 0x10 */
    for (unsigned i = 0; i < sizeof f1; i++) SU32(dst, f1[i], ik_x87_bits(U32(src, f1[i])));
    SU32(dst, 0x68, U32(src, 0x68));
    rx_set_sp(sp - 0x10);
    member_assign_0063ddd0(dst + 0x70, src + 0x70, 0x006a5bb9, dst, src);
    rx_set_sp(sp);
    for (uint32_t o = 0xd0; o < 0x1a0; o += 4) SU32(dst, o, U32(src, o));
    for (uint32_t o = 0x1a0; o < 0x1dc; o += 4) if ((o & 0xf) != 0xc) SU32(dst, o, ik_x87_bits(U32(src, o)));
    return dst;
}
RX_FUNCTION(0x006a5b30, rx_state_record_assign)

/* ---- ThirtySecondBoard (vtable 0095f41c): the "30 seconds" board carried to the start line ----
 * +0x10 owner (world / player id), +0x480 job state (0 idle: hold timer +0x4f4 runs down; 1 walk to point B; 4 walk to point
 * A, timer running), +0x484 move state (0 at A, 2 at B, 5 = done: no more setup), +0x498 / +0x49c walk speeds (to B / to A),
 * +0x4a0 hidden, +0x4b0 facing at the point (vec3), +0x4c0 point A, +0x4d0 point B (vec3), +0x4f8 / +0x4fc / +0x500 the
 * matrix row scales. The entity's character data +0x148 (Character_SampleKeyframes), clip progress +0x170 / +0x18c / +0x194
 * / +0x198. Ground snapping through the terrain object found in the static table 0x9e30d4 (count 0x9e30dc, guard bit 0x9e30e0
 * bit 0) whose +0x44a8 matches the owner. */

/* x87 at the animation threads' 53-bit precision: a * a + b * b, stored as a float. */
static float x87_sumsq2(float a, float b) {
    rx_f80 x = rx_f80_from_f32(a), y = rx_f80_from_f32(b);
    return rx_f80_to_f32(rx_f80_add(rx_f80_mul(x, x, 53, 0), rx_f80_mul(y, y, 53, 0), 53, 0), 0);
}
/* SqrtF 0042c230 called at stack pointer esp (argument, return address and saved ebp on the dead stack). */
static float ent_sqrtf(rx_addr esp, uint32_t arg_bits, rx_addr ret, rx_addr ebp) {
    SU32(esp - 4, 0, arg_bits); SU32(esp - 8, 0, ret); SU32(esp - 12, 0, ebp);
    return rx_sqrts(rx_bf(arg_bits));
}
/* Vec3_Cross 00401630 (math; EAX = a, ECX = b; cdecl out): out = (a.z b.y - a.y b.z, a.x b.z - b.x a.z, b.x a.y - a.x b.y). */
static void vec3_cross(rx_addr a, rx_addr b, rx_addr out, rx_addr ret) {
    const rx_addr sp0 = ik_enter(1, &out, ret);
    rx_world_vec3_cross(out, b, a);
    rx_set_sp(sp0);
}
/* 007c5950 (terrain; thiscall ECX = terrain object or 0; stack: point; RET 4): the point snapped onto the ground. */
static void ground_snap(rx_addr terrain, rx_addr point, rx_addr ret, uint32_t ebx, uint32_t ebp, uint32_t esi, uint32_t edi) {
    (void)ebx; (void)ebp; (void)esi; (void)edi;
    uint32_t edx;
    const rx_addr sp0 = ik_enter(1, &point, ret);
    phys_terrain_base_height_at(terrain, point, &edx);
    rx_set_sp(sp0);
}
/* The terrain object of the owner +0x10 in the static table (first use: the table emptied). */
static rx_addr board_terrain(rx_addr self) {
    uint32_t n;
    if (!(U32(0, 0x009e30e0) & 1)) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); n = 0; SU32(0, 0x009e30dc, 0); }
    else n = U32(0, 0x009e30dc);
    for (uint32_t i = 0; i < n; i++) {
        const rx_addr c = U32(0, 0x009e30d4 + 4 * i);
        if (c && U32(self, 0x10) == U32(c, 0x44a8)) return c;
    }
    return 0;
}

/* 006b85e0 (ESI = board, xmm0 = dt; stack: to B; RET 4): one walking step of the board toward point B (to B != 0) or A:
 * step = walk speed * dt * (clip progress (+0x194 - +0x18c) / +0x198, or 1); arriving (step > distance in xz): snapped onto
 * the point, facing +0x4b0, move state 2 (B) or 0 (A); else moved by the step along the xz direction (the stale direction of the
 * previous call when the distance is 0) and snapped onto the ground. The new matrix (rows: (facing x up) * +0x4f8, up *
 * +0x4fc, facing * +0x500, position) goes to the entity (vtable slot 16). */
void rx_board_walk_step(rx_addr self, float dt, uint32_t to_b) {
    ANIM_FRAME_ENTER(A, 0xb0);                     /* sub esp, 0xa8; push ebx; push edi */
    const rx_addr ebp = A_sp0 - 4, M = A - 0x80, up = A - 0x10, pt = A - 0xa0;
    float speed = phys_mul(F32(self, 0x49c), dt);
    if (to_b) speed = phys_mul(F32(self, 0x498), dt);
    float f;
    if (U32(self, 0x170) != 0) f = phys_div(phys_sub(F32(self, 0x194), F32(self, 0x18c)), F32(self, 0x198));
    else f = 1.0f;
    const float step = phys_mul(f, speed);
    rx_set_sp(A - 0xb4);
    SF32(A - 0x84, 0, step);
    rx_set_sp(A - 0xb0);
    ent_vcall1(self, 0x3c, M, 0x006b8650);         /* the current matrix */
    SU32(A - 0x30, 0, U32(M, 0x20)); SU32(A - 0x2c, 0, U32(M, 0x24)); SU32(A - 0x28, 0, U32(M, 0x28));
    SF32(up, 0, 0.0f); SF32(up, 4, 1.0f); SF32(up, 8, 0.0f);
    const float tx = F32(self, to_b ? 0x4d0 : 0x4c0), tz = F32(self, to_b ? 0x4d8 : 0x4c8);
    const float dz = phys_sub(tz, F32(M, 0x38)), dx = phys_sub(tx, F32(M, 0x30));
    SF32(A - 0x98, 0, dz); SF32(A - 0xa0, 0, dx);
    const rx_addr terrain = board_terrain(self);
    const float d2 = x87_sumsq2(dz, dx);
    SF32(A - 0x88, 0, d2);
    const float dist = ent_sqrtf(A - 0xb0, rx_fb(d2), 0x006b873f, ebp);
    float r1y, r1z;
    if (step > dist) {
        const uint32_t p = to_b ? 0x4d0 : 0x4c0;
        SU32(pt, 0, U32(self, p)); SU32(pt, 4, U32(self, p + 4));
        SU32(self, 0x484, to_b ? 2 : 0);
        SU32(pt, 8, U32(self, p + 8));
        ground_snap(terrain, pt, 0x006b87c9, to_b, ebp, self, terrain);
        rx_set_sp(A - 0xb0);
        vec3_cross(self + 0x4b0, up, A - 0x40, 0x006b87e2);
        const float fx = F32(self, 0x4b0), fy = F32(self, 0x4b4), fz = F32(self, 0x4b8);
        SU32(M, 0x30, U32(pt, 0)); SU32(M, 0x34, U32(pt, 4)); SU32(M, 0x38, U32(pt, 8)); SF32(M, 0x3c, 1.0f);
        const float s0 = F32(self, 0x4f8);
        SF32(M, 0x00, phys_mul(s0, F32(A - 0x40, 0))); SF32(M, 0x08, phys_mul(s0, F32(A - 0x38, 0))); SF32(M, 0x04, phys_mul(s0, F32(A - 0x3c, 0)));
        SF32(M, 0x0c, 0.0f); SF32(M, 0x1c, 0.0f); SF32(M, 0x2c, 0.0f);
        const float s1 = F32(self, 0x4fc);
        SF32(M, 0x10, phys_mul(s1, 0.0f)); SF32(M, 0x18, phys_mul(s1, 0.0f));
        const float s2 = F32(self, 0x500);
        SF32(M, 0x20, phys_mul(s2, fx));
        r1y = phys_mul(s2, fy); r1z = phys_mul(s2, fz);
    } else {
        float nx, ny, nz;
        if (d2 > 0.0f) {
            const float inv = phys_div(1.0f, ent_sqrtf(A - 0xb0, ik_x87_bits(rx_fb(d2)), 0x006b88ce, ebp));
            nx = phys_mul(inv, dx); ny = phys_mul(inv, 0.0f); nz = phys_mul(dz, inv);
        } else { nx = F32(A - 0x20, 0); ny = F32(A - 0x1c, 0); nz = F32(A - 0x18, 0); }   /* stale: the previous cross */
        const float mx = phys_sub(-0.0f, nx), my = phys_sub(-0.0f, ny), mz = phys_sub(-0.0f, nz);
        const float px = phys_add(phys_mul(nx, step), F32(M, 0x30));
        const float py = phys_add(phys_mul(ny, step), F32(M, 0x34));
        const float pz = phys_add(phys_mul(nz, step), F32(M, 0x38));
        SF32(A - 0x40, 0, mx); SF32(A - 0x3c, 0, my); SF32(A - 0x38, 0, mz);
        SF32(A - 0x30, 0, mx); SF32(A - 0x2c, 0, my); SF32(A - 0x28, 0, mz);
        SF32(pt, 0, px); SF32(pt, 4, py); SF32(pt, 8, pz);
        ground_snap(terrain, pt, 0x006b89a1, to_b, ebp, self, terrain);
        rx_set_sp(A - 0xb0);
        vec3_cross(A - 0x30, up, A - 0x20, 0x006b89bc);
        SU32(M, 0x30, U32(pt, 0)); SU32(M, 0x34, U32(pt, 4)); SU32(M, 0x38, U32(pt, 8)); SF32(M, 0x3c, 1.0f);
        const float s0 = F32(self, 0x4f8);
        SF32(M, 0x00, phys_mul(s0, F32(A - 0x20, 0))); SF32(M, 0x08, phys_mul(s0, F32(A - 0x18, 0))); SF32(M, 0x04, phys_mul(s0, F32(A - 0x1c, 0)));
        SF32(M, 0x0c, 0.0f); SF32(M, 0x1c, 0.0f); SF32(M, 0x2c, 0.0f);
        const float s1 = F32(self, 0x4fc);
        SF32(M, 0x10, phys_mul(s1, 0.0f)); SF32(M, 0x18, phys_mul(s1, 0.0f));
        const float s2 = F32(self, 0x500);
        SF32(M, 0x20, phys_mul(s2, F32(A - 0x40, 0)));
        r1y = phys_mul(s2, F32(A - 0x3c, 0)); r1z = phys_mul(s2, F32(A - 0x38, 0));
    }
    SU32(M, 0x14, U32(self, 0x4fc));
    SF32(M, 0x24, r1y); SF32(M, 0x28, r1z);
    ent_vcall1(self, 0x40, M, 0x006b8aa2);
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x006b85e0, rx_board_walk_step)

/* 006b7c70 (cdecl job: [arg] = board, [arg + 4] = dt): ThirtySecondBoard's job. Samples its character animation (step dt),
 * rebuilds its matrix upright (rows: (forward x up) * +0x4f8, up * +0x4fc, normalized forward * +0x500, the position kept),
 * then by job state: 0 the hold timer +0x4f4 runs down, 1 a walking step toward B, 4 timer and a step toward A. */
void rx_board_job(rx_addr job) {
    ANIM_FRAME_ENTER(A, 0x80);                     /* sub esp, 0x7c; push esi */
    const rx_addr ebp = A_sp0 - 4, M = A - 0x40, up = A - 0x60, fwd = A - 0x70;
    const rx_addr self = U32(job, 0);
    const float dt = F32(job, 4);
    rx_set_sp(A - 0x84);
    SF32(A - 0x78, 0, dt);
    rx_set_sp(A - 0x80);
    character_sample_keyframes(self + 0x148, dt, 0.0f, 0x006b7c96, 0, ebp);
    SF32(up, 0, 0.0f); SF32(up, 4, 1.0f); SF32(up, 8, 0.0f);
    ent_vcall1(self, 0x3c, M, 0x006b7cc1);
    const float fx = F32(M, 0x20), fy = F32(M, 0x24), fz = F32(M, 0x28);
    SF32(A - 0x68, 0, fz); SF32(A - 0x6c, 0, fy);
    const float l2 = phys_add(phys_add(phys_mul(fz, fz), phys_mul(fy, fy)), phys_mul(fx, fx));
    SF32(fwd, 0, fx); SF32(A - 0x74, 0, l2);
    if (l2 > 0.0f) {
        const float inv = phys_div(1.0f, ent_sqrtf(A - 0x80, ik_x87_bits(rx_fb(l2)), 0x006b7d1b, ebp));
        SF32(fwd, 0, phys_mul(inv, fx)); SF32(fwd, 4, phys_mul(inv, fy)); SF32(fwd, 8, phys_mul(inv, fz));
    }
    rx_set_sp(A - 0x80);
    vec3_cross(fwd, up, A - 0x50, 0x006b7d69);
    const float rx_ = F32(A - 0x50, 0), ry = F32(A - 0x4c, 0), rz = F32(A - 0x48, 0);
    const float dx = F32(fwd, 0), dy = F32(fwd, 4), dz = F32(fwd, 8);
    SF32(M, 0x00, rx_); SF32(M, 0x10, 0.0f); SF32(M, 0x18, 0.0f); SF32(M, 0x0c, 0.0f); SF32(M, 0x1c, 0.0f); SF32(M, 0x2c, 0.0f);
    SF32(M, 0x14, 1.0f); SF32(M, 0x04, ry); SF32(M, 0x08, rz);
    SF32(M, 0x20, dx); SF32(M, 0x24, dy); SF32(M, 0x28, dz); SF32(M, 0x3c, 1.0f);
    const float s0 = F32(self, 0x4f8);
    SF32(M, 0x00, phys_mul(s0, rx_)); SF32(M, 0x04, phys_mul(F32(self, 0x4f8), ry)); SF32(M, 0x08, phys_mul(F32(self, 0x4f8), rz));
    SF32(M, 0x10, phys_mul(F32(self, 0x4fc), 0.0f)); SU32(M, 0x14, U32(self, 0x4fc)); SF32(M, 0x18, phys_mul(F32(self, 0x4fc), 0.0f));
    SF32(M, 0x20, phys_mul(F32(self, 0x500), dx)); SF32(M, 0x24, phys_mul(F32(self, 0x500), dy)); SF32(M, 0x28, phys_mul(F32(self, 0x500), dz));
    ent_vcall1(self, 0x40, M, 0x006b7e98);
    const uint32_t state = U32(self, 0x480);
    if (state == 0) SF32(self, 0x4f4, phys_sub(F32(self, 0x4f4), dt));
    else if (state == 1 || state == 4) {
        if (state == 4) SF32(self, 0x4f4, phys_sub(F32(self, 0x4f4), dt));
        SU32(A - 0x84, 0, state == 1 ? 1 : 0); SU32(A - 0x88, 0, state == 1 ? 0x006b7edf : 0x006b7ecd);
        rx_set_sp(A - 0x88);
        rx_board_walk_step(self, dt, state == 1);
        rx_set_sp(A - 0x80);
    }
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x006b7c70, rx_board_job)

/* ---- 006b6a30: the animation alias table ---- */

/* 006b6a30 (fastcall ECX = flag byte out or 0, EDX = name to avoid or 0; cdecl: name; returns EAX): looks a name up in the
 * alias table (sorted, [0xd5a9e8] entries, count [0xd5a9ec]; entry: name[0x104] lower case, +0x104 variant count, +0x108 ->
 * variant names, +0x110 -> per-variant flag bytes or 0) and returns one of its variants, picked with the game's random generator
 * (seed 0x9cb860 = seed * 0x41c64e6d + 0x3039, counter 0x9cb864) when there are several; the variant equal (_stricmp) to the
 * name to avoid is replaced by the previous one (or, the first, by another random one). The name is copied with "%s" into a
 * 260-byte frame buffer (0078fd60) and lower-cased through the table 0x9b77c8. The result is also left in 0xa942ec. Unknown
 * name: 0 (and the flag 0). */
static uint32_t game_rand15(void) {
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
    const uint32_t seed = U32(0, 0x009cb860) * 0x41c64e6du + 0x3039u;
    SU32(0, 0x009cb860, seed);
    return (seed >> 16) & 0x7fff;
}
uint32_t rx_anim_alias_pick(rx_addr flag_out, rx_addr avoid, rx_addr name) {
    const rx_addr S = rx_sp();                     /* sub esp, 0x114 (no alignment): frame below the return address */
    const rx_addr buf = S - 0x108;
    ik_frame_cookie(S - 4, S - 0x114);
    SU32(S - 0x110, 0, flag_out); SU32(S - 0x10c, 0, avoid); SU32(S - 0x114, 0, 0);
    {   /* 0078fd60 (EDI = buffer, ESI = size; cdecl format, ...): vsnprintf "%s" (0x9419e4), terminated */
        const uint32_t a[2] = {0x009419e4, name};
        rx_set_sp(S - 0x124);
        const rx_addr sp1 = ik_enter(2, a, 0x006b6a73);
        rx_format_buffer(buf, 0x104, 0x009419e4, rx_sp() + 8);
        rx_set_sp(sp1);
    }
    for (rx_addr p = buf; U8(p, 0) != 0; p++) SU8(p, 0, U8(0x009b77c8, U8(p, 0)));
    rx_addr entry = 0;
    int32_t hi = I32(0, 0x00d5a9ec) - 1, lo = 0;
    const rx_addr table = U32(0, 0x00d5a9e8);
    while (hi >= 0 && lo <= hi) {
        const int32_t mid = (hi + lo) / 2;
        const rx_addr e = U32(table, 4u * (uint32_t)mid);
        int cmp = 0;
        for (uint32_t k = 0;; k++) {
            const uint8_t a = U8(buf, k), b = U8(e, k);
            if (a != b) { cmp = a < b ? -1 : 1; break; }
            if (a == 0) break;
        }
        if (cmp < 0) hi = mid - 1;
        else if (cmp > 0) lo = mid + 1;
        else { entry = e; goto found; }
    }
    if (U32(S - 0x110, 0)) SU8(U32(S - 0x110, 0), 0, 0);
    goto done;
found:
    if (!entry) { if (U32(S - 0x110, 0)) SU8(U32(S - 0x110, 0), 0, 0); goto done; }
    {
        const int32_t n = I32(entry, 0x104);
        int32_t v = 0;
        if (n > 1) v = rx_cvtt((float)((double)(float)game_rand15() * 3.0517578125e-05) * (float)n);   /* 0x9776a0 */
        const rx_addr av = U32(S - 0x10c, 0);
        if (av) {
            uint32_t a[2] = {av, U32(U32(entry, 0x108), 4u * (uint32_t)v)};
            rx_set_sp(S - 0x124);
            const uint32_t same = (uint32_t)ent_stricmp(a[0], a[1], 0x006b6b8d);
            if (same == 0) {
                if (v > 0) v -= 1;
                else {
                    const int32_t n1 = I32(entry, 0x104) - 1;
                    v = 1 - rx_cvtt((float)((double)(float)game_rand15() * -3.0517578125e-05) * (float)n1);  /* 0x977708 */
                }
            }
        }
        SU32(S - 0x114, 0, U32(U32(entry, 0x108), 4u * (uint32_t)v));
        const rx_addr out = U32(S - 0x110, 0);
        if (out) {
            const rx_addr flags = U32(entry, 0x110);
            SU8(out, 0, flags ? U8(flags, (uint32_t)v) : 0);
        }
    }
done:
    SU32(0, 0x00a942ec, U32(S - 0x114, 0));
    SU32(S - 0x118, 0, 0x006b6b11);                /* __security_check_cookie's return address */
    rx_set_sp(S);
    return U32(S - 0x114, 0);
}
RX_FUNCTION(0x006b6a30, rx_anim_alias_pick)

/* ---- the alias table's loader (006b68e0 "AnimationSets", product construction) ----
 * The original's init thread (0075ed60) loads the "AnimationSets" bxml at 006b68e0 and parses it into the table above: one
 * 0x114-byte entry per <Set> (name at +0, lower case, 0x104 bytes; +0x104 the <Anim> count; +0x108 their name pointers;
 * +0x10c the names block; +0x110 one flag byte per name, or 0), sorted by _stricmp on the set name (006b68e0). The per-set
 * parser is 006b64a0 (the "Set" element), the table walk 006b6890. This is the same parse with the core's reader queries: the
 * intro scenario's CameraAnimation handler (0055d290) resolves its AnimName through this table, so without it the fly-by's
 * camera path is never set (an event name like "CameraStartLine" is a set of per-level clips). Product construction only:
 * the oracle boot's original init thread builds the same table. */
void rx_anim_sets_load(const struct fmt_vfs* vfs) {
    const rx_addr sp0 = rx_sp(), F = sp0 - 0x530u;
    rx_set_sp(F);
    const rx_addr rd = F + 0x10u;
    rx_xml_reader_construct(rd);
    if (!rx_xml_reader_load_file(vfs, rd, "AnimationSets.bxml")) { rx_xml_reader_destroy(rd); rx_set_sp(sp0); return; }
    const rx_addr root = rx_xml_root(rd);
    char setname[0x104], names[0x800];
    uint8_t flags[1000];
    for (rx_addr set = rx_xml_find_child(rd, root, "Set"); set; set = rx_xml_next_child(rd, set)) {
        const rx_addr an = rx_xml_find_attr(rd, set, "Name");
        if (an == 0 || !rx_xml_attr_string(rd, an, setname, sizeof setname)) continue;
        uint32_t n = 0, off = 0;
        int any_flag = 0, overflow = 0;
        for (rx_addr el = rx_xml_find_child(rd, set, "Anim"); el; el = rx_xml_next_child(rd, el)) {
            char nm[0x104];
            const rx_addr a = rx_xml_find_attr(rd, el, "Name");
            if (a == 0 || !rx_xml_attr_string(rd, a, nm, sizeof nm)) continue;
            const uint32_t len = (uint32_t)strlen(nm);
            if (off + len >= sizeof names) { overflow = 1; break; }      /* 0x800 (0x6b65e8) */
            if (n + 1 >= 1000) { overflow = 1; break; }                  /* 0x3e8 (0x6b661f) */
            memcpy(names + off, nm, len + 1);
            off += len + 1;
            flags[n] = 0;
            uint32_t b = 0;
            rx_addr fl = rx_xml_find_attr(rd, el, "HasTimeline");
            if (fl && rx_xml_attr_bools(rd, fl, F + 0x500u, 1) && U32(F, 0x500u)) { flags[n] |= 1; any_flag = 1; }
            fl = rx_xml_find_attr(rd, el, "HasVehicleAnim");
            if (fl && rx_xml_attr_bools(rd, fl, F + 0x500u, 1) && U32(F, 0x500u)) { flags[n] |= 2; any_flag = 1; }
            fl = rx_xml_find_attr(rd, el, "IsLooping");
            if (fl && rx_xml_attr_bools(rd, fl, F + 0x500u, 1) && U32(F, 0x500u)) { flags[n] |= 4; any_flag = 1; }
            (void)b;
            n++;
        }
        if (overflow || n == 0) continue;
        const rx_addr e = rx_reflex_aligned_alloc(0x114u);
        if (!e) continue;
        uint32_t k = 0;
        for (; k + 1 < sizeof setname && setname[k]; k++) SU8(e, k, (uint8_t)setname[k]);
        SU8(e, k, 0);
        for (rx_addr p = e; U8(p, 0) != 0; p++) SU8(p, 0, U8(0x009b77c8, U8(p, 0)));   /* 0078fdb0: lower case */
        SU32(e, 0x104, n);
        const rx_addr block = rx_reflex_aligned_alloc(off);
        const rx_addr parr = rx_reflex_aligned_alloc(4u * n);
        SU32(e, 0x10c, block);
        SU32(e, 0x108, parr);
        for (k = 0; k < off; k++) SU8(block, k, (uint8_t)names[k]);
        rx_addr p = block;
        for (k = 0; k < n; k++) { SU32(parr, 4u * k, p); while (U8(p, 0) != 0) p++; p++; }
        if (any_flag) {
            const rx_addr fl = rx_reflex_aligned_alloc(n);
            SU32(e, 0x110, fl);
            for (k = 0; k < n; k++) SU8(fl, k, flags[k]);
        } else {
            SU32(e, 0x110, 0);
        }
        if (I32(0, 0x00d5a9ec) >= I32(0, 0x00d5a9f0)) {                /* 006b7650: +0x32 (50) entries */
            const int32_t cap = I32(0, 0x00d5a9f0), ncap = cap + I32(0, 0x00d5a9f4);
            const rx_addr nt = rx_reflex_aligned_alloc(4u * (uint32_t)ncap);
            for (int32_t i = 0; i < cap; i++) SU32(nt, 4u * (uint32_t)i, U32(U32(0, 0x00d5a9e8), 4u * (uint32_t)i));
            rx_reflex_aligned_free(U32(0, 0x00d5a9e8));
            SU32(0, 0x00d5a9e8, nt);
            SU32(0, 0x00d5a9f0, (uint32_t)ncap);
        }
        SU32(U32(0, 0x00d5a9e8), 4u * U32(0, 0x00d5a9ec), e);
        SU32(0, 0x00d5a9ec, U32(0, 0x00d5a9ec) + 1u);
    }
    rx_xml_reader_destroy(rd);
    rx_set_sp(sp0);
    /* 006b68e0's sort: the entries by _stricmp on their names (the lookup 006b6a30 binary-searches them) */
    const rx_addr tab = U32(0, 0x00d5a9e8);
    const int32_t cnt = I32(0, 0x00d5a9ec);
    for (int32_t i = 0; i < cnt - 1; i++)
        for (int32_t j = i + 1; j < cnt; j++)
            if (crt_stricmp(U32(tab, 4u * (uint32_t)i), U32(tab, 4u * (uint32_t)j)) > 0) {
                const uint32_t t = U32(tab, 4u * (uint32_t)i);
                SU32(tab, 4u * (uint32_t)i, U32(tab, 4u * (uint32_t)j));
                SU32(tab, 4u * (uint32_t)j, t);
            }
    SU32(0, 0x00a942ec, 0);
}

/* ---- ThirtySecondBoard setup and update ---- */

/* 00837510 (rx_anim: me; ESI = character data, xmm1 = blend time; stack: clip; RET 4): plays the clip. */
static void character_play_clip(rx_addr data, rx_addr clip, float blend, rx_addr ret, uint32_t ebx, uint32_t ebp, uint32_t edi) {
    (void)ebx; (void)ebp; (void)edi;
    const rx_addr sp0 = ik_enter(1, &clip, ret);
    rx_character_anim_play(data, clip, blend);
    rx_set_sp(sp0);
}
/* The board's matrix at a point facing f (vec3 address): rows (f x up) * +0x4f8, (0, s1, 0) with s1 = +0x4fc (the zeros
 * as s1 * 0), f * +0x500, the point. `cross` is the frame vec3 holding f x up. */
static void board_matrix(rx_addr self, rx_addr M, rx_addr cross, rx_addr f, float px, float py, float pz) {
    SF32(M, 0x30, px); SF32(M, 0x34, py); SF32(M, 0x38, pz); SF32(M, 0x3c, 1.0f);
    const float s0 = F32(self, 0x4f8);
    SF32(M, 0x00, phys_mul(s0, F32(cross, 0))); SF32(M, 0x08, phys_mul(s0, F32(cross, 8))); SF32(M, 0x04, phys_mul(s0, F32(cross, 4)));
    SF32(M, 0x0c, 0.0f); SF32(M, 0x1c, 0.0f); SF32(M, 0x2c, 0.0f);
    const float s1 = F32(self, 0x4fc);
    SF32(M, 0x10, phys_mul(s1, 0.0f)); SF32(M, 0x18, phys_mul(s1, 0.0f)); SU32(M, 0x14, U32(self, 0x4fc));
    const float s2 = F32(self, 0x500);
    const float fx = F32(f, 0), fy = F32(f, 4), fz = F32(f, 8);
    SF32(M, 0x20, phys_mul(s2, fx)); SF32(M, 0x24, phys_mul(s2, fy)); SF32(M, 0x28, phys_mul(s2, fz));
}

/* 006b7fa0 (EAX = board; not reached in the five worlds): (re)starts the board's job from its move state: job state = move
 * state, move state = 5, busy (+0xa2, +0x70); by the job state: 0 snapped onto point A, position +0x60, clip +0x488 (blend
 * 0.25); 1 at point A with clip +0x490 queued; 2 at B facing +0x4b0 with clip +0x488; 3 the same and the show-board clip
 * ("30SecondGirlShowBoard" through the alias table, resource type 'anim') started on the character (or clip +0x48c queued);
 * 4 half way between A and B facing along A - B, clip +0x494 queued. */
void rx_board_setup(rx_addr self) {
    ANIM_FRAME_ENTER(A, 0xa0);                     /* sub esp, 0x94; push ebx; push esi; push edi */
    const rx_addr ebp = A_sp0 - 4, M = A - 0x80, up = A - 0x10, cross = A - 0x90;
    SU32(self, 0x480, U32(self, 0x484));
    SU32(self, 0x484, 5);
    SF32(up, 0, 0.0f); SF32(up, 4, 1.0f); SF32(up, 8, 0.0f);
    SU8(self, 0xa2, 1);
    SU32(self, 0x70, 1);
    const rx_addr terrain = board_terrain(self);
    uint32_t queued;
    switch (U32(self, 0x480)) {
    case 0:
        ground_snap(terrain, self + 0x4c0, 0x006b8067, 0, ebp, self + 0x4c0, self);
        SU32(self, 0x60, ik_x87_bits(U32(self, 0x4c0))); SU32(self, 0x64, ik_x87_bits(U32(self, 0x4c4)));
        SU32(self, 0x68, ik_x87_bits(U32(self, 0x4c8)));
        character_play_clip(self + 0x148, U32(self, 0x488), 0.25f, 0x006b8092, 0, ebp, self);
        SU32(self, 0x70, 0);
        SU8(self, 0xa2, 0);
        ANIM_FRAME_LEAVE(A);
        return;
    case 1:
        SU32(self, 0x60, ik_x87_bits(U32(self, 0x4c0))); SU32(self, 0x64, ik_x87_bits(U32(self, 0x4c4)));
        SU32(self, 0x68, ik_x87_bits(U32(self, 0x4c8)));
        queued = U32(self, 0x490);
        SF32(self, 0x184, 0.0f);
        break;
    case 2:
    case 3: {
        const int show = U32(self, 0x480) == 3;
        vec3_cross(self + 0x4b0, up, cross, show ? 0x006b820f : 0x006b80ea);
        board_matrix(self, M, cross, self + 0x4b0, F32(self, 0x4d0), F32(self, 0x4d4), F32(self, 0x4d8));
        ent_vcall1(self, 0x40, M, show ? 0x006b82fa : 0x006b81d5);
        if (!show) {
            character_play_clip(self + 0x148, U32(self, 0x488), 0.25f, 0x006b81ef, 0, ebp, self);
            ANIM_FRAME_LEAVE(A);
            return;
        }
        SU32(A - 0xa4, 0, 0x0095f110);
        rx_set_sp(A - 0xa4); SU32(A - 0xa8, 0, 0x006b8308); rx_set_sp(A - 0xa8);
        const uint32_t alias = rx_anim_alias_pick(0, 0, 0x0095f110);   /* "30SecondGirlShowBoard" */
        rx_set_sp(A - 0xa0);
        const rx_addr engine = U32(U32(0, 0x00a95844), 8);
        uint32_t a[4] = {alias, 0x6d696e61u /* 'anim' */, 0, 0};
        const rx_addr res = ik_call(U32(U32(engine, 0), 0x6c), RX_THISCALL, engine, 4, a, 0x006b8323);   /* resource by name */
        if (res) {
            const rx_addr data = self + 0x148;
            rx_addr sp1 = ik_enter(0, 0, 0x006b833a);                    /* 00835000: the clip's instance, if loaded */
            uint32_t clip = rx_character_anim_find_slot(res, data);
            rx_set_sp(sp1);
            if (clip == 0) {
                const uint32_t a2[2] = {data, res};
                sp1 = ik_enter(2, a2, 0x006b8349);                        /* 00834eb0 CharacterAnim_SetAnimation (EAX = 0 loops) */
                clip = rx_character_anim_set_animation(data, res, 0);
                rx_set_sp(sp1);
            }
            SU32(data, 0x24, 0); SU32(data, 0x28, 0); SU32(data, 0x2c, clip); SF32(data, 0x3c, 0.0f);
        } else {
            SU32(self, 0x16c, 0); SU32(self, 0x170, 0); SU32(self, 0x174, U32(self, 0x48c)); SF32(self, 0x184, 0.0f);
        }
        ANIM_FRAME_LEAVE(A);
        return;
    }
    case 4: {
        const float mx = phys_mul(phys_add(F32(self, 0x4d0), F32(self, 0x4c0)), 0.5f);
        const float mz = phys_mul(phys_add(F32(self, 0x4c8), F32(self, 0x4d8)), 0.5f);
        const float my = phys_mul(phys_add(F32(self, 0x4c4), F32(self, 0x4d4)), 0.5f);
        SF32(cross, 0, mx); SF32(cross, 8, mz); SF32(cross, 4, my);
        const float dz = phys_sub(F32(self, 0x4c8), F32(self, 0x4d8));
        const float dy = phys_sub(F32(self, 0x4c4), F32(self, 0x4d4));
        const float dx = phys_sub(F32(self, 0x4c0), F32(self, 0x4d0));
        SF32(A - 0x38, 0, dz); SF32(A - 0x28, 0, dz); SF32(A - 0x3c, 0, dy); SF32(A - 0x2c, 0, dy);
        const float l2 = phys_add(phys_add(phys_mul(dz, dz), phys_mul(dy, dy)), phys_mul(dx, dx));
        SF32(A - 0x40, 0, dx); SF32(A - 0x30, 0, dx); SF32(A - 0x14, 0, l2);
        if (l2 > 0.0f) {
            const float inv = phys_div(1.0f, ent_sqrtf(A - 0xa0, ik_x87_bits(rx_fb(l2)), 0x006b846d, ebp));
            SF32(A - 0x30, 0, phys_mul(inv, dx)); SF32(A - 0x2c, 0, phys_mul(dy, inv)); SF32(A - 0x28, 0, phys_mul(dz, inv));
        }
        vec3_cross(A - 0x30, up, A - 0x40, 0x006b84c3);
        board_matrix(self, M, A - 0x40, A - 0x30, mx, my, mz);
        ent_vcall1(self, 0x40, M, 0x006b85a0);
        queued = U32(self, 0x494);
        SF32(self, 0x184, 0.0f);
        break;
    }
    default:
        ANIM_FRAME_LEAVE(A);
        return;
    }
    SU32(self, 0x174, queued); SU32(self, 0x170, 0); SU32(self, 0x16c, 0);
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x006b7fa0, rx_board_setup)

/* 006b7f00 ThirtySecondBoard::vf19 (thiscall (dt); RET 4): unless the owner's scene is paused (its player object +0x20014: the
 * game's player 1 [0xa946b8]+0x5a54 when the owner is [0xa95844]+0x20, player 2 +0x20c8 when it is +0x1c, else address 0),
 * the world is paused ([[0xa95844]+0x10]+0xc) or the board hidden (+0x4a0): set up once (move state != 5: 006b7fa0) and submit
 * its job (006b7c70, record (board, dt) in the frame) to the job pool. */
void rx_thirty_second_board_update(rx_addr self, float dt) {
    ANIM_FRAME8_ENTER(A, 0x48);                    /* and esp, -8; sub esp, 0x44; push esi */
    const rx_addr g = U32(0, 0x00a95844), owner = U32(self, 0x10);
    rx_addr player = 0;
    if (owner == U32(g, 0x20)) player = U32(U32(0, 0x00a946b8), 0x5a54);
    else if (owner == U32(g, 0x1c)) player = U32(U32(0, 0x00a946b8), 0x20c8);
    if (U32(player, 0x20014) == 0 && U32(U32(g, 0x10), 0xc) == 0 && U32(self, 0x4a0) == 0) {
        if (U32(self, 0x484) != 5) {
            SU32(A - 0x4c, 0, 0x006b7f67);
            rx_set_sp(A - 0x4c); rx_board_setup(self); rx_set_sp(A - 0x48);
        }
        const rx_addr rec = A - 0x40;
        SU32(A - 0x4c, 0, 0x006b7c70); SU32(A - 0x50, 0, rec);
        SU32(rec, 0, self); SF32(rec, 4, dt);
        rx_set_sp(A - 0x50);
        rx_addr sp1 = ik_enter(0, 0, 0x006b7f85);                                   /* JobPool_Instance */
        const rx_addr pool = rx_jobpool_instance();
        rx_set_sp(sp1);
        const uint32_t a[3] = {pool, rec, 0x006b7c70};
        rx_set_sp(A - 0x48);
        sp1 = ik_enter(3, a, 0x006b7f8b);                                           /* JobPool_Submit (rx_loop) */
        rx_jobpool_submit(pool, rec, 0x006b7c70);
        rx_set_sp(sp1);
    }
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x006b7f00, rx_thirty_second_board_update)

/* ---- the entity classes' vf22: the type id (mov eax, id; ret) ---- */
uint32_t rx_anim_entity_single_anim_loop_type(rx_addr self) { (void)self; return 0x1050; }   /* AnimEntitySingleAnimLoop */
RX_FUNCTION(0x006b9ac0, rx_anim_entity_single_anim_loop_type)
uint32_t rx_anim_entity_spline_anim_loop_type(rx_addr self) { (void)self; return 0x1052; }   /* AnimEntitySplineAnimLoop */
RX_FUNCTION(0x006ba560, rx_anim_entity_spline_anim_loop_type)
uint32_t rx_anim_entity_spline_particles_type(rx_addr self) { (void)self; return 0x1059; }   /* ...WithParticles */
RX_FUNCTION(0x006bbdd0, rx_anim_entity_spline_particles_type)
uint32_t rx_crew_chief_type(rx_addr self) { (void)self; return 0x103b; }                     /* CrewChief */
RX_FUNCTION(0x006bd480, rx_crew_chief_type)
uint32_t rx_thirty_second_board_type(rx_addr self) { (void)self; return 0x103a; }            /* ThirtySecondBoard */
RX_FUNCTION(0x006b76c0, rx_thirty_second_board_type)
uint32_t rx_drop_gate_type(rx_addr self) { (void)self; return 0x1033; }                      /* DropGate */
RX_FUNCTION(0x006bd900, rx_drop_gate_type)
uint32_t rx_leader_board_type(rx_addr self) { (void)self; return 0x1037; }                   /* LeaderBoard */
RX_FUNCTION(0x006bf530, rx_leader_board_type)

/* ---- DropGate (vtable 0095fc24): the start gate that drops and comes back up ----
 * +0x10 owner, +0x160 -> its collision part (+4 -> body with the part array at +0x19b4 (+0xc data, 64-byte records; size from
 * +0x19c0 / +0x19c4), +8 the part's index: record +0x38 = 0 switches the gate's collision off once it is down), +0x1a0 state
 * (1 dropping, 2 coming back, 3 dropping again, 4 down), +0x1f0 the down angle, +0x1f4 the drop step, +0x1f8 the drop angle,
 * +0x1fc the back-swing angle, +0x200 its step, +0x204 its angle. The entity matrix lives at +0x30 (4x4). */

/* 006be330 (EAX = angle, ECX = negate, xmm6 = target angle, xmm2 = step; stack: gate; RET 4): advances the angle by the step;
 * reaching the target it is clamped there. The gate's matrix (vtable slot 15) is turned about its x axis by the angle moved
 * (negated when ECX != 0) -- R = (1 0 0 0; 0 c s 0; 0 -s c 0; 0 0 0 1) times the matrix, the zero products kept -- and written
 * to +0x30. Returns 1 when the target was reached. The two paths sum the products in different orders (as here). */
uint32_t rx_drop_gate_swing(rx_addr gate, rx_addr angle, uint32_t negate, float target, float step) {
    ANIM_FRAME_ENTER(A, 0x120);                    /* sub esp, 0x118; push esi; push edi */
    const rx_addr ebp = A_sp0 - 4, M = A - 0xf0, O = A - 0x50;
#define T(x) (A - 0x120 + (x))
    const float cur = F32(angle, 0), now = phys_add(cur, step);
    SF32(angle, 0, now);
    const int reached = !(now < target || now != now || target != target);   /* comiss; jb */
    float a = reached ? phys_sub(target, cur) : step;
    if (negate) a = phys_sub(-0.0f, a);
    const float sgn = a >= 0.0f ? 1.0f : -1.0f;
    if (reached) SF32(T(0x14), 0, sgn);
    const float m = phys_mul(sgn, a);
    const float turns = (float)rx_cvtt(phys_mul(m, K_INV_2PI));
    const float red = phys_sub(K_PI, phys_sub(m, phys_mul(turns, K_2PI)));
    const float t2 = phys_mul(red, red), t4 = phys_mul(t2, t2);
    float p = phys_mul(phys_add(phys_mul(t2, K_C6), K_C5), t4);
    p = phys_mul(phys_add(p, phys_add(phys_mul(t2, K_C4), K_C3)), t4);
    p = phys_add(p, phys_add(phys_mul(t2, K_C2), K_C1));
    const float c = phys_sub(-0.0f, p);
    float q = phys_mul(phys_add(phys_mul(t2, K_S6), K_S5), t4);
    q = phys_mul(phys_add(q, phys_add(phys_mul(t2, K_S4), K_S3)), t4);
    q = phys_add(q, phys_add(phys_mul(t2, K_S2), K_S1));
    if (reached) SF32(angle, 0, target);
    const float s = phys_mul(phys_mul(q, red), sgn), ns = phys_sub(-0.0f, s);
    SF32(T(0xb8), 0, c); SF32(T(0xb4), 0, ns); SF32(T(0xa8), 0, s); SF32(T(0xa4), 0, c);
    ent_vcall1(gate, 0x3c, M, reached ? 0x006be4ed : 0x006be9e4);
#define MM(r, k) F32(M, 0x10 * (r) + 4 * (k))
    const float z = 0.0f;
    float mz[4][4];
    for (int r = 0; r < 4; r++) for (int k = 0; k < 4; k++) mz[r][k] = phys_mul(MM(r, k), z);
    float o[16];
    for (int k = 0; k < 4; k++) {
        /* row 1: c * m1 + s * m2; row 2: -s * m1 + c * m2 */
        const float r1 = phys_add(phys_mul(MM(2, k), s), phys_mul(MM(1, k), c));
        const float r2 = phys_add(phys_mul(MM(2, k), c), phys_mul(MM(1, k), ns));
        if (reached) {
            o[k] = phys_add(phys_add(phys_add(mz[3][k], mz[1][k]), mz[2][k]), MM(0, k));
            if (k == 0) { o[4] = phys_add(r1, phys_add(mz[0][0], mz[3][0])); o[8] = phys_add(r2, phys_add(mz[0][0], mz[3][0])); }
        } else {
            o[k] = phys_add(phys_add(phys_add(mz[1][k], mz[2][k]), mz[3][k]), MM(0, k));
            if (k == 0) { o[4] = phys_add(phys_add(r1, mz[0][0]), mz[3][0]); o[8] = phys_add(phys_add(r2, mz[0][0]), mz[3][0]); }
        }
        if (k > 0) { o[4 + k] = phys_add(phys_add(r1, mz[0][k]), mz[3][k]); o[8 + k] = phys_add(phys_add(r2, mz[0][k]), mz[3][k]); }
        o[12 + k] = phys_add(phys_add(phys_add(mz[0][k], mz[1][k]), mz[2][k]), MM(3, k));
    }
    /* the frame temporaries the original leaves (final values) */
    if (reached) {
        SF32(T(0x14), 0, mz[3][1]); SF32(T(0x18), 0, phys_mul(MM(2, 0), c)); SF32(T(0x1c), 0, mz[3][3]); SF32(T(0x20), 0, mz[3][2]);
        SF32(T(0x24), 0, mz[2][3]); SF32(T(0x28), 0, mz[0][0]); SF32(T(0x2c), 0, phys_add(mz[0][0], mz[3][0]));
        SF32(T(0x74), 0, mz[1][1]); SF32(T(0x78), 0, mz[2][0]); SF32(T(0x7c), 0, mz[2][1]); SF32(T(0x80), 0, mz[1][2]);
        SF32(T(0x84), 0, mz[2][2]); SF32(T(0x88), 0, mz[1][3]); SF32(T(0x8c), 0, mz[1][0]);
    } else {
        SF32(T(0x14), 0, mz[0][1]); SF32(T(0x18), 0, mz[3][3]); SF32(T(0x1c), 0, mz[0][0]); SF32(T(0x20), 0, mz[3][0]);
        SF32(T(0x24), 0, mz[0][3]); SF32(T(0x28), 0, mz[3][1]); SF32(T(0x2c), 0, mz[3][2]);
        SF32(T(0x74), 0, mz[1][2]); SF32(T(0x78), 0, mz[2][2]); SF32(T(0x7c), 0, mz[2][1]); SF32(T(0x80), 0, mz[1][1]);
        SF32(T(0x84), 0, mz[2][0]); SF32(T(0x88), 0, mz[1][0]); SF32(T(0x8c), 0, mz[1][3]); SF32(T(0x11c), 0, mz[2][3]);
    }
    for (int i = 0; i < 16; i++) SF32(O, 4 * i, o[i]);
    for (int i = 0; i < 16; i++) SU32(gate, 0x30 + 4 * i, U32(O, 4 * i));
#undef MM
#undef T
    (void)ebp;
    ANIM_FRAME_LEAVE(A);
    return reached ? 1 : 0;
}
RX_FUNCTION(0x006be330, rx_drop_gate_swing)

/* 006be1c0 DropGate::vf19 (thiscall (dt, unused); RET 4): unless the owner's scene or the world is paused (as the board's),
 * by state: 1 drop to +0x1f0 (then the collision part off, state 2), 2 swing back by +0x1fc (then the drop angle restarts at
 * +0x1fc, state 3), 3 drop again (state 4). */
void rx_drop_gate_update(rx_addr self, float dt) {
    (void)dt;
    ANIM_FRAME8_ENTER(A, 0x10);                    /* and esp, -8; push ecx; push ebx; push esi; push edi */
    const rx_addr g = U32(0, 0x00a95844), owner = U32(self, 0x10);
    rx_addr player = 0;
    if (owner == U32(g, 0x20)) player = U32(U32(0, 0x00a946b8), 0x5a54);
    else if (owner == U32(g, 0x1c)) player = U32(U32(0, 0x00a946b8), 0x20c8);
    if (U32(player, 0x20014) == 0 && U32(U32(g, 0x10), 0xc) == 0) {
        const uint32_t state = U32(self, 0x1a0);
        if (state == 1 || state == 2 || state == 3) {
            const int back = state == 2;
            const float target = back ? phys_sub(F32(self, 0x1f0), F32(self, 0x1fc)) : F32(self, 0x1f0);
            const float step = F32(self, back ? 0x200 : 0x1f4);
            const rx_addr ang = self + (back ? 0x204 : 0x1f8);
            const rx_addr ret = state == 1 ? 0x006be2dd : state == 2 ? 0x006be295 : 0x006be254;
            SU32(A - 0x14, 0, self); SU32(A - 0x18, 0, ret);
            rx_set_sp(A - 0x18);
            const uint32_t done = rx_drop_gate_swing(self, ang, back ? 0 : 1, target, step);
            rx_set_sp(A - 0x10);
            if (done) {
                if (state == 1) {
                    const rx_addr part = U32(self, 0x160);
                    if (part) {
                        const rx_addr body = U32(part, 4);
                        const uint32_t idx = U32(part, 8);
                        if (idx >= (uint32_t)((int32_t)(U32(body, 0x19c4) - U32(body, 0x19c0)) >> 6)) ik_invalid_parameter(0x006be310);
                        SU8(U32(body, 0x19b4 + 0xc), 64u * idx + 0x38, 0);
                    }
                    SU32(self, 0x1a0, 2);
                } else if (state == 2) {
                    SU32(self, 0x1a0, 3);
                    SU32(self, 0x1f8, ik_x87_bits(U32(self, 0x1fc)));
                } else SU32(self, 0x1a0, 4);
            }
        }
    }
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x006be1c0, rx_drop_gate_update)

/* ---- LeaderBoard (vtable 0095fd94): the track-side board with the five leaders' numbers and the lap ----
 * +0x10 owner, +0x1a0 the five shown names (4-byte slots), +0x1b4 the digit textures (11: 0..9, 10 = blank), +0x1e0 -> its model
 * (+0x4c busy while edited; +0x44 -> mesh set: +0x118 group count, +0x130 / +0x134 groups (0x1c bytes: +0x10 / +0x14 its parts,
 * 16 bytes each, +0xc the part: vtable slot 2 = its name, +0x44 -> material (+0x1c texture), +0x4c lock)), +0x1e4 update
 * period (frames), +0x1e8 frame counter. The digit parts are named "LPos<n>_Num1..3" (position n's rider number) and
 * "LapNum_01" / "LapNum_02" (lap tens / units). */

/* A part's name (thiscall vtable slot 2) compared (_stricmp) with s; the original pushes s before the virtual call. */
static int part_name_is(rx_addr part, rx_addr s, rx_addr ret_name, rx_addr ret_cmp) {
    const rx_addr sp = rx_sp();
    SU32(sp - 4, 0, s);
    rx_set_sp(sp - 4);
    const uint32_t name = ik_call(U32(U32(part, 0), 8), RX_THISCALL, part, 0, 0, ret_name);
    rx_set_sp(sp);
    return ent_stricmp(name, s, ret_cmp) == 0;
}
/* The board's parts whose names match: their material's texture = digit texture [value] (when both exist), the part locked
 * meanwhile. names[k] / values[k] / return addresses per name (vtable call, _stricmp) as each caller has them. */
static void board_set_digits(rx_addr self, int count, const rx_addr* names, const uint32_t* values, const rx_addr (*rets)[2],
                             rx_addr slot_i, rx_addr slot_off, rx_addr slot_j, rx_addr slot_joff, const rx_addr* ret_checks) {
    const rx_addr set = U32(U32(self, 0x1e0), 0x44);
    SU32(slot_i, 0, 0);
    if (U32(set, 0x118) == 0) return;
    SU32(slot_off, 0, 0);
    uint32_t i = 0, off = 0;
    do {
        if (i >= (uint32_t)((int32_t)(U32(set, 0x134) - U32(set, 0x130)) / 0x1c)) ik_invalid_parameter(ret_checks[0]);
        const rx_addr grp = U32(set, 0x130) + off;
        if (slot_j) SU32(slot_j, 0, 0);
        if ((uint32_t)((int32_t)(U32(grp, 0x14) - U32(grp, 0x10)) >> 4) != 0) {
            SU32(slot_joff, 0, 0);
            uint32_t j = 0;
            do {
                if (i >= (uint32_t)((int32_t)(U32(set, 0x134) - U32(set, 0x130)) / 0x1c)) ik_invalid_parameter(ret_checks[1]);
                const rx_addr g = U32(set, 0x130) + off;
                if (j >= (uint32_t)((int32_t)(U32(g, 0x14) - U32(g, 0x10)) >> 4)) ik_invalid_parameter(ret_checks[2]);
                const rx_addr part = U32(U32(g, 0x10) + 16 * j, 0xc);
                for (int k = 0; k < count; k++) {
                    if (part_name_is(part, names[k], rets[k][0], rets[k][1])) {
                        const rx_addr mat = U32(part, 0x44);
                        SU32(part, 0x4c, 1);
                        if (mat) { const rx_addr tex = U32(self, 0x1b4 + 4 * values[k]); if (tex) SU32(mat, 0x1c, tex); }
                        SU32(part, 0x4c, 0);
                        break;
                    }
                }
                j++;
                SU32(slot_joff, 0, 16 * j);
                if (slot_j) SU32(slot_j, 0, j);
            } while (j < (uint32_t)((int32_t)(U32(grp, 0x14) - U32(grp, 0x10)) >> 4));
        }
        off += 0x1c; i++;
        SU32(slot_off, 0, off); SU32(slot_i, 0, i);
    } while (i < U32(set, 0x118));
}

/* 006bf990 (stdcall (board, position index, rider name); RET 0xc): shows position index + 1's rider number (the name's first
 * up to three characters as digits, atoi each; missing leading digits blank = 10) on the parts "LPos<n>_Num1..3". */
void rx_leader_board_set_position(rx_addr self, uint32_t index, rx_addr name) {
    const rx_addr S = rx_sp();                     /* sub esp, 0x60 (no alignment); push ebx; push ebp; push esi; push edi */
    ik_frame_cookie(S - 4, S - 0x60);
    const rx_addr d0 = S - 0x60, d1 = S - 0x5c, d2 = S - 0x58;
    const uint8_t c0 = U8(name, 0), c1 = U8(name, 1), c2 = U8(name, 2);
    SU32(S - 0x44, 0, self);
    SU8(d0, 1, 0); SU8(d0, 0, c0); SU8(d1, 1, 0); SU8(d1, 0, c1); SU8(d2, 1, 0); SU8(d2, 0, c2);
    rx_set_sp(S - 0x70);
    uint32_t v;
    if (c2 == 0) {
        if (c1 == 0) {
            SU32(S - 0x48, 0, 10); SU32(S - 0x4c, 0, 10);
            v = 10;
            if (c0 != 0) v = ent_atoi(d0, 0x006bf9f4);
        } else {
            SU32(S - 0x4c, 0, 10);
            SU32(S - 0x48, 0, ent_atoi(d0, 0x006bfa0e));
            rx_set_sp(S - 0x74);
            v = ent_atoi(d1, 0x006bfa19);
        }
    } else {
        SU32(S - 0x4c, 0, ent_atoi(d0, 0x006bfa2b));
        rx_set_sp(S - 0x74);
        SU32(S - 0x48, 0, ent_atoi(d1, 0x006bfa36));
        rx_set_sp(S - 0x78);
        v = ent_atoi(d2, 0x006bfa41);
    }
    SU32(S - 0x50, 0, v);
    /* the three part names "LPos%d_Num1..3" (sprintf into zeroed 16-byte buffers) */
    const rx_addr b1 = S - 0x34, b2 = S - 0x24, b3 = S - 0x14;
    for (int k = 0; k < 16; k++) { SU8(b1, k, 0); SU8(b2, k, 0); SU8(b3, k, 0); }
    const uint32_t n = index + 1;
    rx_set_sp(S - 0x70); ent_sprintf1(b1, 0x0095f314, n, 0x006bfab1);   /* "LPos%d_Num1" */
    rx_set_sp(S - 0x7c); ent_sprintf1(b2, 0x0095f320, n, 0x006bfabe);
    rx_set_sp(S - 0x88); ent_sprintf1(b3, 0x0095f32c, n, 0x006bfacb);
    rx_set_sp(S - 0x70);
    const rx_addr model = U32(self, 0x1e0);
    SU32(model, 0x4c, 1);
    const rx_addr names[3] = {b1, b2, b3};
    const uint32_t values[3] = {U32(S - 0x4c, 0), U32(S - 0x48, 0), U32(S - 0x50, 0)};
    static const rx_addr rets[3][2] = {{0x006bfba7, 0x006bfbae}, {0x006bfbd7, 0x006bfbde}, {0x006bfc07, 0x006bfc0e}};
    static const rx_addr checks[3] = {0x006bfb1d, 0x006bfb69, 0x006bfb8e};
    board_set_digits(self, 3, names, values, rets, S - 0x38, S - 0x54, S - 0x40, S - 0x3c, checks);
    SU32(U32(self, 0x1e0), 0x4c, 0);
    SU32(S - 0x64, 0, U32(U32(model, 0x44), 0x118) ? 0x006bfc91 : 0x006bfcaf);   /* __security_check_cookie's return */
    rx_set_sp(S);
}
RX_FUNCTION(0x006bf990, rx_leader_board_set_position)

/* 006bfcc0 (ECX = lap (0..99); stack: board; RET 4): shows the lap on "LapNum_01" (tens; blank (10) below 10, 0 for lap 0) and
 * "LapNum_02" (units). */
void rx_leader_board_set_lap(rx_addr self, int32_t lap) {
    const rx_addr S = rx_sp();                     /* sub esp, 0x14; push ebx; push ebp; push esi; push edi */
    const rx_addr model = U32(self, 0x1e0);
    SU32(model, 0x4c, 1);
    int32_t units = 0, tens = 0;
    if (lap != 0) {
        tens = lap / 10; units = lap - tens * 10;
        if (lap < 10) tens = 10;
    }
    SU32(S - 4, 0, (uint32_t)units); SU32(S - 0x10, 0, (uint32_t)tens);
    rx_set_sp(S - 0x24);
    const rx_addr names[2] = {0x0095f344 /* "LapNum_02" */, 0x0095f338 /* "LapNum_01" */};
    const uint32_t values[2] = {(uint32_t)units, (uint32_t)tens};
    static const rx_addr rets[2][2] = {{0x006bfde5, 0x006bfdec}, {0x006bfe15, 0x006bfe1c}};
    static const rx_addr checks[3] = {0x006bfd59, 0x006bfda9, 0x006bfdcc};
    board_set_digits(self, 2, names, values, rets, S - 8, S - 0x14, 0, S - 0xc, checks);
    SU32(U32(self, 0x1e0), 0x4c, 0);
    rx_set_sp(S);
}
RX_FUNCTION(0x006bfcc0, rx_leader_board_set_lap)

/* 005ee840 (thiscall: [ECX] + 4 * index; RET 4): an array element's address (not in the cluster). */
static rx_addr array_at(rx_addr arr, uint32_t index, rx_addr ret) { return ik_call(0x005ee840, RX_THISCALL, arr, 1, &index, ret); }

/* 006bf760 LeaderBoard::vf19 (thiscall (dt); RET 4): every +0x1e4 + 1 frames, while the owner's race is on (player
 * +0x21da8), the up to five leaders of its standings (player +0x21da4 [0]: +0x26c count, +0x2d0 riders; a rider's name at
 * [+0x74] + 0x210) are shown where changed (006bf990, name copied into the slot), the positions beyond them cleared (with ""),
 * and the leader's lap + 1 (clamped 0..99; 0 without a leader) shown (006bfcc0). */
void rx_leader_board_update(rx_addr self, float dt) {
    (void)dt;
    const rx_addr S = rx_sp();                     /* push ecx; push ebx */
    SU32(self, 0x1e8, U32(self, 0x1e8) + 1);
    SU32(S - 4, 0, self);
    if (I32(self, 0x1e8) <= I32(self, 0x1e4)) return;
    const rx_addr g = U32(0, 0x00a95844), game = U32(0, 0x00a946b8), owner = U32(self, 0x10);
    SU32(self, 0x1e8, 0);
    rx_addr player = 0;
    if (owner == U32(g, 0x20)) player = U32(game, 0x5a54);
    else if (owner == U32(g, 0x1c)) player = U32(game, 0x20c8);
    if (U32(player, 0x21da8) == 0) return;
    rx_set_sp(S - 0x14);                           /* push esi; push edi; push ebp */
    const rx_addr standings = U32(array_at(player + 0x21da4, 0, 0x006bf7ef), 0);
    int32_t shown = I32(standings, 0x26c);
    if (shown > 5) shown = 5;
    if (owner == U32(g, 0x20)) player = U32(game, 0x5a54);
    else if (owner == U32(g, 0x1c)) player = U32(game, 0x20c8);
    else player = 0;
    const rx_addr riders = U32(array_at(player + 0x21da4, 0, 0x006bf843), 0) + 0x2d0;
    int32_t i = 0;
    for (; i < shown; i++) {
        const rx_addr slot = self + 0x1a0 + 4 * (uint32_t)i;
        const rx_addr name = U32(U32(riders, 4 * (uint32_t)i), 0x74) + 0x210;
        int differ = 0;
        for (uint32_t k = 0;; k++) { const uint8_t a = U8(slot, k); if (a != U8(name, k)) { differ = 1; break; } if (!a) break; }
        if (differ) {
            SU32(S - 0x18, 0, name); SU32(S - 0x1c, 0, (uint32_t)i); SU32(S - 0x20, 0, self); SU32(S - 0x24, 0, 0x006bf8b1);
            rx_set_sp(S - 0x24); rx_leader_board_set_position(self, (uint32_t)i, name); rx_set_sp(S - 0x14);
            const rx_addr src = U32(U32(riders, 4 * (uint32_t)i), 0x74) + 0x210;
            for (uint32_t k = 0;; k++) { const uint8_t ch = U8(src, k); SU8(slot, k, ch); if (!ch) break; }
        }
    }
    for (; i < 5; i++) {
        const rx_addr slot = self + 0x1a0 + 4 * (uint32_t)i;
        if (U8(slot, 0) != 0) {                    /* differs from "" (0x95f30c) */
            SU32(S - 0x18, 0, 0x0095f30c); SU32(S - 0x1c, 0, (uint32_t)i); SU32(S - 0x20, 0, self); SU32(S - 0x24, 0, 0x006bf925);
            rx_set_sp(S - 0x24); rx_leader_board_set_position(self, (uint32_t)i, 0x0095f30c); rx_set_sp(S - 0x14);
            SU8(slot, 0, 0);
        }
    }
    int32_t lap = 0;
    rx_addr ret = 0x006bf97c;
    const rx_addr lead = U32(riders, 0);
    if (lead) {
        lap = I32(lead, 0xf1c) + 1;
        if (lap < 0) { lap = 0; ret = 0x006bf952; }
        else { if (lap > 0x63) lap = 0x63; ret = 0x006bf96c; }
    }
    SU32(S - 0x18, 0, self); SU32(S - 0x1c, 0, ret);
    rx_set_sp(S - 0x1c); rx_leader_board_set_lap(self, lap); rx_set_sp(S);
}
RX_FUNCTION(0x006bf760, rx_leader_board_update)
