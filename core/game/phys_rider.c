/* phys_rider.c -- the rider animation's crash physics: the stages 00425640 runs around the ragdoll step (see phys_rider.h). */
#include "phys_rider.h"
#include "phys_crash_solver.h"
#include "world.h"
#include "world_node.h"
#include "world_wreck.h"
#include "phys_ragdoll.h"
#include "phys_rigbody.h"
#include "phys_coll_terrain.h"
#include "phys_rig.h"
#include "crt.h"
#include "build_alloc.h"
#include "phys_body.h"
#include "world_veh.h"
#include "vehicle_water_wake.h"
#include "vehicle_atv_wheels.h"
#include "world_veh_water.h"
#include "phys_coll_misc.h"
#ifndef RX_ORACLE
rx_addr rx_vector_int_construct16(rx_addr vec,rx_addr value); /*0042b410*/
uint32_t rx_crt_atexit(uint32_t handler); /*008f9fbf product exit policy*/
#endif

enum { R_EAX, R_ECX, R_EDX, R_EBX, R_ESP, R_EBP, R_ESI, R_EDI };
#define CALL_AT_SP(cur, nstack, stmt) do { rx_set_sp((cur) - 4u * (nstack) - 4u); stmt; rx_set_sp(cur); } while (0)

/* msvcr90 _invalid_parameter_noinfo through the IAT (0x916224): a checked iterator failed (never in practice) */
static void invalid_parameter(void) { crt_invalid_parameter_noinfo(); }
/* an original call with the registers tracked: r in, r = the callee's registers out (ESP kept) */
static void tracked_call(rx_regs* r, rx_addr fn, int nargs, const uint32_t* args) {
    rx_regs out = phys_regs();
    const uint32_t esp = r->r[R_ESP];
    r->fpu_in = 0;
    rx_call_original_ex(fn, r, &out, nargs, args);
    *r = out; r->r[R_ESP] = esp; r->fpu_in = 0;
}

/* AUDIO BOUNDARY (see world_util.h): the rider's crash sound is a sound instance of the audio layer (handle A+0x2f0, state A+0x2f4). The
 * handle is created only by the audio layer's instance allocator (no audio system exists in the product: world_util.h) and every
 * path below tests it first (A+0x2f0 != 0), so 00804e40 (stop), 00807890 (stop below a gain), 00807970 (fade), 008077c0 (start) and
 * 008078d0 / 00807c30 (levels) never run; the oracle runs the originals. Fatal in the product if reached. */
#define AUDIO_BOUNDARY_TRACKED_CALL(r, fn, nargs, args) tracked_call((r), (fn), (nargs), (args))

/* 0042bd90 (EAX = handle -> EAX = the 0xe0-byte record) and 00402dc0 (EDX = registry -> EAX = the current world) as ports: the
 * result goes to EAX of the tracked registers */
static void tracked_body_record(rx_regs* r) {
    rx_addr rec;
    PHYS_CALL_SP(0, rec = phys_body_record(r->r[R_EAX]));
    r->r[R_EAX] = rec;
}
static void tracked_find_world(rx_regs* r) {
    rx_addr w;
    PHYS_CALL_SP(0, w = rx_world_find_by_controller(r->r[R_EDX]));
    r->r[R_EAX] = w;
}

/* 0042a240 (stdcall bones; RET 4): for the 16 bone handles at +0xc (8 bytes apart; handle +4 the owner, +8 the index into the
 * owner's 0xe0-byte records at +0x1960, range-checked against +0x1964): the record's node (+0x2c) flags +0x14 = 0, then |= 8. */
void phys_rider_clear_bone_flags(rx_addr bones) {
    for (uint32_t k = 0; k < 16; k++) {
        const rx_addr slot = bones + 0xc + 8u * k;
        for (int pass = 0; pass < 2; pass++) {
            const rx_addr h = U32(slot, 0);
            if (h == 0) break;
            const rx_addr owner = U32(h, 4);
            const uint32_t idx = U32(h, 8);
            const uint32_t n = (uint32_t)((I32(owner, 0x1964) - I32(owner, 0x1960)) / 0xe0);
            if (idx >= n) invalid_parameter();
            const rx_addr flags = U32(U32(owner, 0x1960) + idx * 0xe0, 0x2c) + 0x14;
            SU32(flags, 0, pass == 0 ? 0 : (U32(flags, 0) | 8));
        }
    }
}
RX_FUNCTION(0x0042a240, phys_rider_clear_bone_flags)

/* 0042aca0 (stdcall A; RET 4): 0 unless the ragdoll is active (A+0x370); else whether the root's rotation (A+0xe80) applied to
 * the writable axis 0xd61680 (w masked off, 0092db80) points along A+0xa840: q (v, 0) q* dotted ((x + y) + z) and compared
 * against the zero vector 0xd61670 (cmpleps; the three lane masks summed as -3). *ecx = the mask sum (unchanged when inactive). */
uint32_t phys_rider_up_facing(rx_addr A, uint32_t* ecx) {
    if (U32(A, 0x370) == 0) return 0;
    const rx_v4 q = v4_ld(A + 0xe80), axis = v4_and(v4_ld(0x0092db80), v4_ld(0x00d61680));
    const rx_v4 v = phys_quat_mul(q, phys_quat_mul(axis, phys_quat_conj(q)));
    const float d = phys_hsum3(v4_mul(v, v4_ld(A + 0xa840)));
    const rx_v4 m = v4_cmple(v4_ld(0x00d61670), v4_splat(d));
    *ecx = v4_lb(m, 2) + v4_lb(m, 1) + v4_lb(m, 0);
    return *ecx == 0xfffffffdu;
}
RX_FUNCTION(0x0042aca0, phys_rider_up_facing)

/* 00425800 (stdcall A, dt; RET 8): the at-rest timer: while the root's linear and angular speeds squared (A+0x410, A+0xe90:
 * (x^2 + y^2) + z^2) are below the thresholds A+0x330 / A+0x340 on all three lanes and the ragdoll runs (A+0x374), the timer
 * A+0x310 grows by dt and passing A+0x320 on all lanes sets A+0x370; otherwise the timer is reset to 0xd61670. A+0x378 also sets
 * A+0x370. Returns the EDX the original leaves (a mask lane or sum). */
static uint32_t lane_sum(rx_v4 m) { return v4_lb(m, 2) + v4_lb(m, 1) + v4_lb(m, 0); }
uint32_t phys_rider_rest_timer(rx_addr A, float dt) {
    const float lin = phys_hsum3(v4_mul(v4_ld(A + 0x410), v4_ld(A + 0x410)));
    const float ang = phys_hsum3(v4_mul(v4_ld(A + 0xe90), v4_ld(A + 0xe90)));
    const rx_v4 m1 = v4_cmplt(v4_splat(lin), v4_ld(A + 0x330));
    uint32_t edx = lane_sum(m1);
    int resting = 0;
    if (edx == 0xfffffffdu) {
        const rx_v4 m2 = v4_cmplt(v4_splat(ang), v4_ld(A + 0x340));
        edx = v4_lb(m2, 1);
        resting = lane_sum(m2) == 0xfffffffdu && U32(A, 0x374) != 0;
    }
    if (resting) {
        v4_st(A + 0x310, v4_add(v4_splat(dt), v4_ld(A + 0x310)));
        const rx_v4 m3 = v4_cmplt(v4_ld(A + 0x320), v4_ld(A + 0x310));
        edx = v4_lb(m3, 1);
        if (lane_sum(m3) == 0xfffffffdu) SU32(A, 0x370, 1);
    } else {
        v4_st(A + 0x310, v4_ld(0x00d61670));
    }
    if (U32(A, 0x378) != 0) SU32(A, 0x370, 1);
    return edx;
}
RX_FUNCTION(0x00425800, phys_rider_rest_timer)

/* 00426930 (ECX = A): the ragdoll's pose written to the scene nodes: the root's node (A+0xa540; records of 16 bytes at
 * ((A+0x37c)+0x240)+8) gets the root rotation (A+0xe80) and position (A+0x400), each further body i (< P+0x7c) the rotation
 * A+0xef0 + 0x70 (i-1) and the world point of bone i-1 (0041feb0) at node A+0xa544 + 4 (i-1) (Node_SetWorldRotation 007dcfb0,
 * Node_SetWorldPosition 007dd1b0). Frame: push ebp; and esp, -16; sub esp, 0x54 (cookie +0x50); push ebx / esi / edi: +0x2c A,
 * +0x30 the rotation, +0x40 the position. out = the registers left (EAX the body count, EDX the last call's, ECX the cookie). */
void phys_rider_pose_nodes(rx_addr A, const rx_regs* caller, rx_regs* out) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x54 - 12;
    rx_set_sp(L);
    SU32(L, 0x5c, U32(0, 0x009b45e4) ^ (L + 0xc));
    rx_regs r = caller ? *caller : phys_regs();
    r.fpu_in = 0; r.r[R_ESP] = L; r.r[R_EBP] = sp0 - 4; r.r[R_EDI] = A;
    const rx_addr rot = L + 0x30, pos = L + 0x40;
    rx_addr nodes = U32(U32(A, 0x37c), 0x240);
    rx_addr node = U32(A, 0xa540) * 16u + U32(nodes, 8);
    v4_st(pos, v4_ld(A + 0x400));
    SU32(L, 0x2c, A);
    v4_st(rot, v4_ld(A + 0xe80));
    {   /* 007dcfb0 / 007dd1b0: the core's node ports (world_node.h) at the original's ESP; the oracle runs the originals so
         * the register leftovers this function returns (EAX / EDX) are the original's */
        const uint32_t a1[1] = {rot}, a2[2] = {node, pos};
        const rx_addr sp = r.r[R_ESP];
        r.r[R_EAX] = U32(A, 0x37c); r.r[R_ESI] = node; r.r[R_ECX] = node; r.r[R_EDX] = rot;
#ifdef RX_ORACLE
        tracked_call(&r, 0x007dcfb0, 1, a1);
        r.r[R_EAX] = pos;
        tracked_call(&r, 0x007dd1b0, 2, a2);
#else
        rx_set_sp(sp - 8);
        rx_world_node_set_world_rotation(node, rot, r.r[R_EBX]);
        rx_set_sp(sp);
        r.r[R_EAX] = pos;
        rx_set_sp(sp - 12);
        rx_world_node_set_world_position(node, pos, r.r[R_ESI], r.r[R_EDI]);
        rx_set_sp(sp);
        r.r[R_ECX] = U32(0, 0x009b45e4);                             /* the epilogue's cookie */
#endif
    }
    uint32_t i = 1;
    r.r[R_EAX] = 1;
    SU32(L, 0x28, 1);
    if (I32(A, 0x3fc) > 1) {
        SU32(L, 0x10, A + 0xef0); SU32(L, 0x24, A + 0xa544);
        r.r[R_ECX] = A + 0xef0; r.r[R_EDX] = A + 0xa544; r.r[R_ESI] = A + 0x380;
        for (; (int32_t)i < I32(A, 0x3fc); i++) {
            r.r[R_EDI] = U32(L, 0x2c);
            nodes = U32(U32(r.r[R_EDI], 0x37c), 0x240);
            node = U32(U32(L, 0x24), 0) * 16u + U32(nodes, 8);
            r.r[R_EBX] = node;
            v4_st(rot, v4_ld(U32(L, 0x10)));
            const uint32_t a1[1] = {rot}, a2[2] = {node, pos};
            const rx_addr sp = r.r[R_ESP];
            r.r[R_EAX] = U32(L, 0x10); r.r[R_EDX] = U32(L, 0x24); r.r[R_ECX] = node;
#ifdef RX_ORACLE
            tracked_call(&r, 0x007dcfb0, 1, a1);
            r.r[R_EDX] = i - 1; r.r[R_EDI] = pos;
            tracked_call(&r, 0x0041feb0, 0, NULL);                      /* the bone's world point (ESI rig, EDX bone, EDI out) */
            r.r[R_EDX] = pos;
            tracked_call(&r, 0x007dd1b0, 2, a2);
#else
            rx_set_sp(sp - 8);
            rx_world_node_set_world_rotation(node, rot, r.r[R_EBX]);
            rx_set_sp(sp);
            r.r[R_EDX] = i - 1; r.r[R_EDI] = pos;
            rx_set_sp(sp - 4);
            rx_world_bone_point_world(pos, r.r[R_ESI], (uint32_t)(i - 1));
            rx_set_sp(sp);
            r.r[R_EDX] = pos;
            rx_set_sp(sp - 12);
            rx_world_node_set_world_position(node, pos, r.r[R_ESI], r.r[R_EDI]);
            rx_set_sp(sp);
            r.r[R_ECX] = U32(0, 0x009b45e4);                             /* the epilogue's cookie */
#endif
            SU32(L, 0x24, U32(L, 0x24) + 4); SU32(L, 0x10, U32(L, 0x10) + 0x70);
            r.r[R_EAX] = i + 1; r.r[R_ECX] = U32(L, 0x2c);
            SU32(L, 0x28, i + 1);
        }
    }
    r.r[R_ECX] = U32(0, 0x009b45e4);
    if (out) *out = r;
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00426930, phys_rider_pose_nodes)

/* 00425920 (stdcall A; RET 4): the speed clamp: when some body's linear speed squared (A+0x410 + 0x80 i: (x^2 + y^2) + z^2) passes
 * A+0x350 or its angular speed squared (A+0xe90 + 0x70 i) passes A+0x360 (on all three lanes), every body gets the mean
 * velocity's direction (the sum normalized as phys_normalize3) times the square root of the mean speed squared (at most
 * A+0x350), the angular velocities are zeroed and the fixed joints re-enforced (00423b80, 00423df0 on P = A+0x380).
 * *ecx / *edx = the registers left. */
void phys_rider_clamp_speed(rx_addr A, uint32_t* ecx, uint32_t* edx) {
    const int32_t n = I32(A, 0x3fc);
    *ecx = 0;
    if (n <= 0) return;
    const rx_v4 lin_max = v4_ld(A + 0x350), ang_max = v4_ld(A + 0x360);
    rx_v4 sum = v4_ld(0x00d61670), speed2 = sum;
    int fast_lin = 0, fast_ang = 0;
    for (int32_t i = 0; i < n; i++) {
        const rx_v4 v = v4_ld(A + 0x410 + 0x80u * (uint32_t)i), w = v4_ld(A + 0xe90 + 0x70u * (uint32_t)i);
        const rx_v4 s = v4_splat(phys_dot3(v, v));
        speed2 = v4_add(s, speed2);
        const rx_v4 m1 = v4_cmplt(lin_max, s);
        if (v4_lb(m1, 2) + v4_lb(m1, 1) + v4_lb(m1, 0) == 0xfffffffdu) fast_lin = 1;
        sum = v4_add(v, sum);
        const rx_v4 m2 = v4_cmplt(ang_max, v4_splat(phys_dot3(w, w)));
        *edx = v4_lb(m2, 1);
        if (v4_lb(m2, 2) + v4_lb(m2, 1) + v4_lb(m2, 0) == 0xfffffffdu) fast_ang = 1;
    }
    *ecx = A + 0x410 + 0x80u * (uint32_t)n;
    if (!fast_lin && !fast_ang) return;
    *edx = U32(A, 0x3fc);
    rx_v4 mean = v4_div(speed2, v4_splat((float)I32(A, 0x3fc)));
    const rx_v4 m = v4_cmplt(lin_max, mean);
    if (v4_lb(m, 2) + v4_lb(m, 1) + v4_lb(m, 0) == 0xfffffffdu) mean = lin_max;
    const rx_v4 vel = v4_mul(phys_normalize3(sum), v4_sqrt(mean));
    for (int32_t i = 0; i < I32(A, 0x3fc); i++) v4_st(A + 0x410 + 0x80u * (uint32_t)i, vel);
    int32_t i = 0;
    for (; i < I32(A, 0x3fc); i++) v4_st(A + 0xe90 + 0x70u * (uint32_t)i, v4_ld(0x00d61670));
    if (i > 0) *edx = A + 0xe90 + 0x70u * (uint32_t)i;
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x34 - 12;
    SU32(L - 4, 0, A + 0x380);
    CALL_AT_SP(L, 1, phys_ragdoll_fix_positions(A + 0x380, ecx, edx));
    SU32(L - 4, 0, A + 0x380);
    CALL_AT_SP(L, 1, phys_ragdoll_fix_velocities(A + 0x380, ecx, edx));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00425920, phys_rider_clamp_speed)

/* 008070a0 (EAX = a sound parameter block; xmm0 out): (p+4 * p+0) * p+0x18 -- a tiny pure helper, ported here */
static float sound_gain(rx_addr p) { return phys_mul(phys_mul(F32(p, 4), F32(p, 0)), F32(p, 0x18)); }
/* the speed factor: |A+0x410| ((x^2 + z^2) + y^2, the sqrtps of the splat) over the state's reference speed (00679540: ESI the
 * state byte, xmm0 out), in double, at most 1 (the reference is asked again when it is not above), times 3, as a float */
static float speed_factor(rx_addr A, rx_regs* r) {
    const rx_v4 v = v4_ld(A + 0x410), p = v4_mul(v, v);
    const float speed = phys_sqrt(phys_add(phys_add(p.v[0], p.v[2]), p.v[1]));
    r->r[R_ESI] = U8(A, 0x2f4);
#ifdef RX_ORACLE   /* the original leaves the registers the later sound calls track (00679540's tail call) */
    tracked_call(r, 0x00679540, 0, NULL);
#else
    r->xmm[0].v[0] = rx_world_crowd_level(U8(A, 0x2f4));             /* 00679540 (world_wreck.h): the slot's level, xmm0 */
#endif
    double ratio = (double)speed / (double)r->xmm[0].v[0];
    if (!(ratio > 1.0 /* 0x977638 */)) {
        r->r[R_ESI] = U8(A, 0x2f4);
#ifdef RX_ORACLE
        tracked_call(r, 0x00679540, 0, NULL);
#else
        r->xmm[0].v[0] = rx_world_crowd_level(U8(A, 0x2f4));
#endif
        ratio = (double)speed / (double)r->xmm[0].v[0];
    } else {
        ratio = 1.0;
    }
    return (float)((double)(float)ratio * 3.0 /* 0x977500 */);
}

/* 004252a0 (stdcall A; RET 4): the crash sound (handle A+0x2f0, its state A+0x2f4, level A+0x2f8): the wanted state is 6 while
 * A+0xa8a0, else the surface byte A+0x2fc (2 -> 0) while the ragdoll is down (A+0xa834); with neither a playing sound is stopped
 * (00804e40) and the state reset to -1. On a state change the old sound is faded (00807970 with 0.5 for the states 5..7) or
 * stopped (00807890, 00804e40) and the level set from the speed factor; with a sound playing its level follows the speed factor
 * times the sound's gains (00807890 stops it below 0.001; 008078d0 sets the level, 00807c30 / 008077c0 (2, -1) restart it at
 * the bank A+0xa5a0). r = the caller's registers in, the registers left out. */
void phys_rider_crash_sound(rx_addr A, rx_regs* r) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x14 - 12;
    rx_set_sp(L);
    r->r[R_ESP] = L; r->r[R_EBP] = sp0 - 4; r->r[R_EBX] = A; r->fpu_in = 0;
    uint32_t state;
    if (U32(A, 0xa8a0) != 0) {
        state = 6;
    } else if (U32(A, 0xa834) == 0) {
        r->r[R_ESI] = U32(A, 0x2f0);
        if (r->r[R_ESI] != 0) {
            AUDIO_BOUNDARY_TRACKED_CALL(r, 0x00804e40, 0, NULL);
            SU32(A, 0x2f4, 0xffffffffu); SU32(A, 0x2f0, 0);
        }
        rx_set_sp(sp0);
        return;
    } else {
        r->r[R_EAX] = (r->r[R_EAX] & ~0xffu) | U8(A, 0x2fc);
        state = U8(A, 0x2fc) == 2 ? 0 : U8(A, 0x2fc);
    }
    r->r[R_EDI] = state;
    const uint32_t cur = U32(A, 0x2f4);
    r->r[R_ECX] = cur;
    if (state != cur) {
        const rx_addr h = U32(A, 0x2f0);
        r->r[R_EAX] = h;
        if (h != 0) {
            if (cur == 5 || cur == 6 || cur == 7) {
                const uint32_t a[1] = {rx_fb(0.5f) /* 0x9322cc */};
                AUDIO_BOUNDARY_TRACKED_CALL(r, 0x00807970, 1, a);
            } else {
                r->r[R_ESI] = h;
                AUDIO_BOUNDARY_TRACKED_CALL(r, 0x00807890, 0, NULL);
                r->r[R_ESI] = U32(A, 0x2f0);
                AUDIO_BOUNDARY_TRACKED_CALL(r, 0x00804e40, 0, NULL);
            }
            SU32(A, 0x2f0, 0);
        }
        SU32(A, 0x2f4, state);
        SU32(A, 0x2f0, 0);
        const float f = speed_factor(A, r);
        r->xmm[0].v[0] = f;
        SF32(A, 0x2f8, f);
        r->r[R_ESI] = U32(A, 0x2f0);
        if (r->r[R_ESI] != 0) {                                         /* (never: the handle was just cleared) */
            const uint32_t a[2] = {2, rx_fb(F32(0, 0x009cd9e8))};
            r->r[R_EAX] = A + 0xa5a0;
            AUDIO_BOUNDARY_TRACKED_CALL(r, 0x008077c0, 2, a);
        }
    }
    if (U32(A, 0x2f0) == 0) { rx_set_sp(sp0); return; }
    const float level = speed_factor(A, r);
    SF32(A, 0x2f8, level);
    r->r[R_EAX] = 0x00d67b98;
    const float g = sound_gain(0x00d67b98);
    r->xmm[0].v[0] = g;
    const rx_addr h = U32(A, 0x2f0);
    r->r[R_ESI] = h;
    float x = (float)((double)g * (double)level);
    SF32(L, 0x14, x);
    if (U32(h, 0) == 1) {
        if (0.001f /* 0x97743c */ > x && F32(h, 0x1c) == 0.0f) {
            AUDIO_BOUNDARY_TRACKED_CALL(r, 0x00807890, 0, NULL);
            rx_set_sp(sp0);
            return;
        }
        if (!(F32(h, 0x1c) == 0.0f)) {
            r->r[R_EAX] = h + 8;
            const float g2 = sound_gain(h + 8);
            x = (float)((double)g2 * (double)x);
        }
        r->r[R_EDI] = h; r->xmm[0].v[0] = x; r->xmm[1].v[0] = x;
        AUDIO_BOUNDARY_TRACKED_CALL(r, 0x008078d0, 0, NULL);
        r->r[R_EAX] = U32(A, 0x2f0); r->r[R_ECX] = A + 0xa5a0;
        AUDIO_BOUNDARY_TRACKED_CALL(r, 0x00807c30, 0, NULL);
        rx_set_sp(sp0);
        return;
    }
    if (x > 0.001f) {
        if (!(F32(h, 0x1c) == 0.0f)) {
            r->r[R_EAX] = h + 8;
            x = (float)((double)sound_gain(h + 8) * (double)x);
            SF32(L, 0x14, x);
        }
        const uint32_t a[2] = {2, rx_fb(F32(0, 0x009cd9e8))};
        r->r[R_EAX] = A + 0xa5a0;
        AUDIO_BOUNDARY_TRACKED_CALL(r, 0x008077c0, 2, a);
        r->r[R_EDI] = U32(A, 0x2f0); r->xmm[0].v[0] = F32(L, 0x14);
        AUDIO_BOUNDARY_TRACKED_CALL(r, 0x008078d0, 0, NULL);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x004252a0, phys_rider_crash_sound)

/* 0042a310 (ECX = A): the ragdoll's box (the 15 bodies listed at A+0xa768, positions A+0x400 + 0x80 i): per axis (the writable
 * axes 0xd616a0, 0xd615c0, 0xd61680 in that order) a body whose projection ((a.x p.x + a.y p.y) + a.z p.z) passes the high
 * corner's (or falls below the low corner's) gives that corner its coordinate (the z axis copies z into w as well); widened by
 * A+0xa7b8 and tested against the world's dynamic objects (World_BoxFreeOfDynamics 00773110: EAX high, EDX low, EDI the
 * scene's collision world). When free: A+0x94 = 0 and the bone nodes' flag 8 cleared (as 0042a240). Frame: push ebp; and
 * esp, -16; sub esp, 0x34 (cookie +0x30); push ebx / esi / edi: +0x10 high, +0x20 low. Returns the EDX left. */
uint32_t phys_rider_wake_if_clear(rx_addr A, const rx_regs* caller) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x34 - 12;
    rx_set_sp(L);
    SU32(L, 0x3c, U32(0, 0x009b45e4) ^ (L + 0xc));
    const rx_v4 ay = v4_ld(0x00d616a0);
    rx_v4 hi = v4_ld(A + 0x400), lo = hi;
    for (uint32_t k = 0; k < 15; k++) {
        const rx_v4 p = v4_ld(A + 0x80u * (U32(A, 0xa768 + 4 * k) + 8));
        const float py = phys_dot3(p, ay);
        if (phys_dot3(ay, hi) < py) hi.v[1] = p.v[1];
        if (py < phys_dot3(ay, lo)) lo.v[1] = p.v[1];
        const rx_v4 ax = v4_ld(0x00d615c0);
        const float px = phys_dot3(p, ax);
        if (phys_dot3(ax, hi) < px) hi.v[0] = p.v[0];
        if (px < phys_dot3(v4_ld(0x00d615c0), lo)) lo.v[0] = p.v[0];
        const rx_v4 az = v4_ld(0x00d61680);
        const float pz = phys_dot3(p, az);
        if (phys_dot3(az, hi) < pz) { hi.v[2] = p.v[2]; hi.v[3] = p.v[2]; }
        if (pz < phys_dot3(v4_ld(0x00d61680), lo)) { lo.v[2] = p.v[2]; lo.v[3] = p.v[2]; }
    }
    const rx_v4 r = v4_splat(F32(A, 0xa7b8));
    v4_st(L + 0x10, v4_add(r, hi));
    v4_st(L + 0x20, v4_sub(lo, r));
    rx_regs regs = caller ? *caller : phys_regs();
    regs.fpu_in = 0; regs.r[R_ESP] = L; regs.r[R_EBP] = sp0 - 4; regs.r[R_ESI] = A; regs.r[R_ECX] = 1;
    const rx_addr scene = U32(0, 0x00a95844);
    rx_addr world = 0;
    if (U32(U32(scene, 0x20), 0x34) == 1) world = U32(U32(0, 0x00a946b8), 0x5a54);
    else if (U32(U32(scene, 0x1c), 0x34) == 1) world = U32(U32(0, 0x00a946b8), 0x20c8);
    regs.r[R_EDI] = U32(world, 0x20134);
    regs.r[R_EDX] = L + 0x20; regs.r[R_EAX] = L + 0x10;
    /* 00773110 (world_body.c): EAX = high corner, EDX = low corner, EDI = the collision world; AL = 1 when free. Called at the
     * original's ESP. The original's EDX residue at return is not modelled (its only caller ignores the result). */
    int free_box = 0;
    CALL_AT_SP(L, 0, free_box = rx_world_box_free_of_dynamics(U32(world, 0x20134), L + 0x20, L + 0x10));
    uint32_t edx = 0;
    if (free_box != 0) {
        SU8(A, 0x94, 0);
        for (uint32_t k = 0; k < 16; k++) {
            const rx_addr h = U32(A, 0xc + 8u * k);
            if (h == 0) continue;
            const rx_addr owner = U32(h, 4);
            const uint32_t idx = U32(h, 8);
            const int32_t span = I32(owner, 0x1964) - I32(owner, 0x1960);
            edx = (uint32_t)(((int64_t)(int32_t)0x92492493 * span) >> 32);
            if (idx >= (uint32_t)(span / 0xe0)) invalid_parameter();
            edx = U32(owner, 0x1960);
            const rx_addr flags = U32(edx + idx * 0xe0, 0x2c) + 0x14;
            SU32(flags, 0, U32(flags, 0) & ~8u);
        }
    }
    rx_set_sp(sp0);
    return edx;
}
RX_FUNCTION(0x0042a310, phys_rider_wake_if_clear)

/* ---- the packed acos / cos of 00420160 (every operation lane by lane; only lane 0, and lane 3 = lane 0's acos, reach the
 * results, so they are done on scalars here). maxps / minps keep the second operand unless the first is greater / smaller. */
static float trunc_ss(float x) {                                            /* cvttss2si, then cvtsi2ss */
    if (!(x > -2147483904.0f && x < 2147483648.0f)) return -2147483648.0f;
    return (float)(int32_t)x;
}
/* the signed asin-like core: |x| or sqrt(1 - x^2) (above 0.70710599, 0x977780) through the rational fit 0x935b70..0x935bc0,
 * folded with pi / 2 (0x935b00) and the sign of x (zero <= x; zero / one = the writable splats 0xd61660 / 0xd61690) */
static float packed_asin(float x) {
    const float one = F32(0, 0x00d61690), zero = F32(0, 0x00d61660), h = 1.57079637f;
    const float ax = phys_max(phys_sub(0.0f, x), x);
    const float om = phys_sub(one, phys_mul(x, x)), omp = phys_max(phys_sub(0.0f, om), om);
    const int big = 0.707105994f <= ax;
    const float t = big ? phys_sqrt(omp) : ax;
    const float t2 = phys_mul(t, t), t4 = phys_mul(t2, t2);
    const float den = phys_add(phys_mul(phys_add(phys_mul(t2, -0.0499285087f), 0.564809322f), t4), phys_add(phys_mul(t2, -1.44303548f), one));
    const float inv = phys_div(1.0f, den);
    const float num = phys_mul(phys_add(phys_mul(phys_add(phys_mul(t2, -0.0193784237f), 0.399303406f), t4),
                                        phys_add(phys_mul(t2, -1.27636874f), one)), t);
    const float r = phys_mul(inv, num);
    const float as = big ? phys_sub(h, r) : r;
    return zero <= x ? as : phys_sub(0.0f, as);
}
/* minus the cosine: the polynomial 0x935b10..0x935b60 of y^2, y = pi - (|x| mod 2 pi) (the remainder through cvttss2si) */
static float packed_neg_cos(float x) {
    const float ax = phys_max(phys_sub(0.0f, x), x);
    const float n = trunc_ss(phys_mul(ax, 0.159154937f));
    const float y = phys_sub(3.14159274f, phys_sub(ax, phys_mul(n, 6.28318548f)));
    const float y2 = phys_mul(y, y), y4 = phys_mul(y2, y2);
    const float a = phys_mul(phys_add(phys_mul(y2, -2.19729600e-07f), 2.42029400e-05f), y4);
    const float b = phys_add(phys_mul(y2, -0.00138587900f), 0.0416597798f);
    return phys_add(phys_mul(phys_add(a, b), y4), phys_add(phys_mul(y2, -0.499994189f), 0.999999225f));
}

/* 00420160 (stdcall q*; RET 4; ECX / EDX / ESI / EDI = the four float outputs): a rotation q split into its swing and twist about
 * the writable axis 0xd615c0: u = q* (a q) (a = the axis with w masked off), twist = acos u.x (*ESI), swing = 2 asin(clamp(q.x /
 * cos(twist / 2), -1, 1)) (*EDI), and the swing direction ((-(u.y cos s - u.z sin s), -(u.y sin s + u.z cos s)) / sin twist; (1, 0)
 * when sin twist < 1e-4) (*ECX, *EDX). Frame: push ebp; and esp, -16; sub esp, 0x5c; push ebx. */
void phys_rider_swing_twist(rx_addr q, rx_addr out_x, rx_addr out_y, rx_addr out_twist, rx_addr out_swing) {
    const rx_v4 Q = v4_ld(q), axis = v4_and(v4_ld(0x0092db80), v4_ld(0x00d615c0));
    const rx_v4 u = phys_quat_mul(phys_quat_conj(Q), phys_quat_mul(axis, Q));
    const float h = 1.57079637f, one = F32(0, 0x00d61690);
    const float twist = phys_sub(h, packed_asin(u.v[0]));
    const float half = phys_mul(0.5f, twist);
    float c = phys_div(Q.v[0], phys_sub(0.0f, packed_neg_cos(half)));
    c = phys_min(c, one);
    c = phys_max(c, phys_sub(0.0f, one));
    const float swing = phys_mul(packed_asin(c), 2.0f);
    const float cos_s = phys_sub(0.0f, packed_neg_cos(swing)), sin_s = packed_neg_cos(phys_add(swing, h));
    const float sin_t = packed_neg_cos(phys_add(twist, h));
    float x, y;
    if (sin_t < 9.99999975e-05f /* 0x9775a4 */) {
        x = 1.0f; y = 0.0f;
    } else {
        x = phys_div(phys_sub(0.0f, phys_sub(phys_mul(u.v[1], cos_s), phys_mul(u.v[2], sin_s))), sin_t);
        y = phys_div(phys_sub(0.0f, phys_add(phys_mul(u.v[1], sin_s), phys_mul(u.v[2], cos_s))), sin_t);
    }
    SF32(out_x, 0, x); SF32(out_y, 0, y); SF32(out_twist, 0, twist); SF32(out_swing, 0, swing);
}
RX_FUNCTION(0x00420160, phys_rider_swing_twist)

/* ---- quaternion helpers of 00420770 (the packed product phys_quat_mul; the guarded normalizations: v / |v| where |v| != 0, 0
 * where |v| == 0, the NaN vec4 0092dc30 where |v|^2 is infinite (0092dc20); 4 lanes ((q1^2 + q3^2) + (q0^2 + q2^2)) or 3 lanes
 * ((x^2 + y^2) + z^2, the w lane divided too)) ---- */
static rx_v4 guarded_div(rx_v4 v, float n2) {
    const float n = phys_sqrt(n2);
    const rx_v4 inf = v4_ld(0x0092dc20), nan = v4_ld(0x0092dc30);
    rx_v4 r;
    for (int i = 0; i < 4; i++)
        r.v[i] = inf.v[i] != n2 ? (0.0f != n ? phys_div(v.v[i], n) : rx_bf(0)) : nan.v[i];
    return r;
}
static rx_v4 qnorm4(rx_v4 q) { return guarded_div(q, phys_hsum4(v4_mul(q, q))); }
static rx_v4 qnorm3(rx_v4 v) { return guarded_div(v, phys_dot3(v, v)); }
static rx_v4 xyz(rx_v4 v) { return v4_and(v, v4_ld(0x0092db80)); }
/* v rotated by q: q (v q*) */
static rx_v4 qrot(rx_v4 q, rx_v4 v) { return phys_quat_mul(q, phys_quat_mul(v, phys_quat_conj(q))); }
/* v rotated by q*: q* (v q) */
static rx_v4 qrot_inv(rx_v4 q, rx_v4 v) { return phys_quat_mul(phys_quat_conj(q), phys_quat_mul(v, q)); }

/* the scalar minus-cosine of the rig code (Float_Abs 00401000 = and with 0x977570; 1 / 2 pi 0x9774a0, 2 pi 0x97749c, pi
 * 0x977498; the polynomial 0x935d3c..0x935d50 of t^2, t = pi - (|x| mod 2 pi)) */
static float scalar_neg_cos(float x) {
    const float ax = rx_bf(rx_fb(x) & U32(0, 0x00977570));
    const float n = trunc_ss(phys_mul(ax, 0.159154937f));
    const float y = phys_sub(3.14159274f, phys_sub(ax, phys_mul(n, 6.28318548f)));
    const float y2 = phys_mul(y, y), y4 = phys_mul(y2, y2);
    const float a = phys_mul(phys_add(phys_mul(y2, -2.19729600e-07f), 2.42029400e-05f), y4);
    const float b = phys_add(phys_mul(y2, -0.00138587900f), 0.0416597798f);
    return phys_add(phys_mul(phys_add(a, b), y4), phys_add(phys_mul(y2, -0.499994189f), 0.999999225f));
}
static float scalar_cos(float x) { return phys_sub(rx_bf(0x80000000u), scalar_neg_cos(x)); }
static float scalar_sin(float x) { return scalar_neg_cos(phys_add(x, 1.57079637f /* 0x977508 */)); }

/* 00420770 (stdcall P; RET 4): the ragdoll's joint limits (joints listed at P+0x37f0, P+0x1b20 of them; joint j's record P +
 * 0x170 j: +0x1b30 body a, +0x1b50 body b, +0x1be0 the rest rotation, +0x1c10 the swing limit; the twist limit from the spline
 * 0041e080 on the joint):
 *   1. per joint: the relative rotation r = rest* (qa* qb) (normalized) split by 00420160 into the swing direction (stored at
 *      P+0x3930 / P+0x3980 + 4 j), twist and swing; the swing clamped to +/- (limit - 0xd72ac8) (flags P+0x3840 / P+0x3890), the
 *      twist to the spline's limit - 0xd72acc (flag P+0x38e0; the direction then becomes (cos, sin) of its angle); a limited joint
 *      stores the rotation rebuilt from the half angles, the others r, at P+0x19d0 + 16 b;
 *   2. the limited joints backwards: qb = qa (rest r') (normalized);
 *   3. the limited joints forwards: the relative angular velocity along the limited axis removed (twist: half from each body
 *      about the rest-rotated axis (0, -sin, cos); swing: all from b about the rest-rotated (r' x r'*) twist axis 0xd615c0).
 * Frame: push ebp; and esp, -16; sub esp, 0x194 (cookie +0x190); push ebx / esi / edi: +0x20 the twist direction angle, +0x24
 * twist, +0x28 swing, +0x4c the twist limit, +0x90 (0.5 x4), +0xa0 the limited flags per joint, +0x140 r. */
void phys_rider_joint_limits(rx_addr P, const rx_regs* caller) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x194 - 12;
    rx_set_sp(L);
    SU32(L, 0x19c, U32(0, 0x009b45e4) ^ (L + 0xc));
    v4_st(L + 0x90, v4_splat(0.5f /* 0x9322cc */));
    const rx_v4 zero4 = v4_zero();
#define JREC(j) (P + 0x170u * (uint32_t)(j))
#define BODYQ(b) (P + 0xb00u + 0x70u * (uint32_t)(b))
#define BODYW(b) (P + 0xb10u + 0x70u * (uint32_t)(b))
#define STORED(b) (P + 0x19d0u + 0x10u * (uint32_t)(b))
    for (int32_t k = 0; k < I32(P, 0x1b20); k++) {
        const uint32_t j = U32(P, 0x37f0 + 4u * (uint32_t)k);
        const rx_addr J = JREC(j), flag = L + 0xa0 + 4u * j;
        const uint32_t a = U32(J, 0x1b30), b = U32(J, 0x1b50);
        SU32(flag, 0, 0);
        const rx_v4 rel = phys_quat_mul(phys_quat_conj(v4_ld(J + 0x1be0)), phys_quat_mul(phys_quat_conj(v4_ld(BODYQ(a))), v4_ld(BODYQ(b))));
        const rx_v4 r = qnorm4(rel);
        v4_st(L + 0x140, r);
        const rx_addr dx = P + 0x3930 + 4u * j, dy = P + 0x3980 + 4u * j;
        SU32(L - 4, 0, L + 0x140);
        CALL_AT_SP(L, 1, phys_rider_swing_twist(L + 0x140, dx, dy, L + 0x24, L + 0x28));
        SF32(L, 0x4c, F32(L, 0x24));
        SF32(L, 0x20, rx_f80_to_f32(crt_ci_atan2(rx_f80_from_f32(F32(dy, 0)), rx_f80_from_f32(F32(dx, 0)), CRT_CW_MAIN), 0));
        {   /* 0041e080 (ESI = the joint; stdcall the angle*, the twist*): the twist limit at the direction's angle */
            rx_regs regs = caller ? *caller : phys_regs();
            const uint32_t args[2] = {L + 0x20, L + 0x4c};
            regs.r[R_ESI] = J + 0x1b30; regs.r[R_EBX] = P; regs.r[R_EDI] = dy; regs.r[R_EBP] = sp0 - 4;
            regs.r[R_EAX] = L + 0x20; regs.r[R_EDX] = L + 0x4c;
#ifndef RX_ORACLE   /* the product: the core's spline-nearest-angles (world_veh_math.c; ESI = the joint's track spline, stdcall the angle*, the twist*) */
            (void)args;
            { const rx_addr spx = rx_sp(); rx_set_sp(spx - 12u); rx_world_spline_nearest_angles(J + 0x1b30, L + 0x20, L + 0x4c); rx_set_sp(spx); }
#else
            tracked_call(&regs, 0x0041e080, 2, args);
#endif
        }
        const float lim = F32(J, 0x1c10);
        float swing = F32(L, 0x28);
        SU32(P, 0x3840 + 4u * j, 0); SU32(P, 0x3890 + 4u * j, 0); SU32(P, 0x38e0 + 4u * j, 0);
        if ((double)swing > (double)lim) {
            swing = (float)((double)lim - (double)F32(0, 0x00d72ac8));
            SU32(P, 0x3840 + 4u * j, 1);
            SF32(L, 0x28, swing);
        } else if ((double)phys_sub(rx_bf(0x80000000u), lim) > (double)swing) {
            swing = (float)((double)F32(0, 0x00d72ac8) - (double)lim);
            SU32(P, 0x3890 + 4u * j, 1);
            SF32(L, 0x28, swing);
        }
        if ((double)F32(L, 0x24) > (double)F32(L, 0x4c)) {
            SF32(L, 0x24, (float)((double)F32(L, 0x4c) - (double)F32(0, 0x00d72acc)));
            const float ang = F32(L, 0x20);
            SF32(dx, 0, scalar_cos(ang));
            SF32(dy, 0, scalar_sin(ang));
            SU32(P, 0x38e0 + 4u * j, 1);
        }
        rx_v4 out = v4_ld(L + 0x140);
        if (U32(P, 0x38e0 + 4u * j) != 0 || U32(P, 0x3840 + 4u * j) != 0 || U32(P, 0x3890 + 4u * j) != 0) {
            SU32(flag, 0, 1);
            const float ht = (float)((double)F32(L, 0x24) * 0.5), hs = (float)((double)F32(L, 0x28) * 0.5);
            SF32(L, 0x68, ht); SF32(L, 0x60, hs);
            const float s_ht = scalar_sin(ht), c_ht = scalar_cos(ht), s_hs = scalar_sin(hs), c_hs = scalar_cos(hs);
            SF32(L, 0x5c, s_ht); SF32(L, 0x58, c_ht); SF32(L, 0x2c, s_hs);
            const double ddx = F32(dx, 0), ddy = F32(dy, 0);
            const float w = (float)((double)c_hs * (double)c_ht);
            const float A = (float)(((double)c_hs * (double)s_ht) * ddx + (ddy * (double)s_hs) * (double)s_ht);
            const float B = (float)(((double)s_hs * (double)s_ht) * ddx - (ddy * (double)c_hs) * (double)s_ht);
            const float C = (float)((double)s_hs * (double)c_ht);
            out = qnorm4(v4(C, B, A, w));
        }
        v4_st(STORED(b), out);
    }
    for (int32_t k = I32(P, 0x1b20) - 1; k >= 0; k--) {
        const uint32_t j = U32(P, 0x37f0 + 4u * (uint32_t)k);
        if (U32(L, 0xa0 + 4u * j) == 0) continue;
        const rx_addr J = JREC(j);
        const uint32_t a = U32(J, 0x1b30), b = U32(J, 0x1b50);
        const rx_v4 t = phys_quat_mul(v4_ld(J + 0x1be0), v4_ld(STORED(b)));
        v4_st(BODYQ(b), qnorm4(phys_quat_mul(v4_ld(BODYQ(a)), t)));
    }
    for (int32_t k = 0; k < I32(P, 0x1b20); k++) {
        const uint32_t j = U32(P, 0x37f0 + 4u * (uint32_t)k);
        if (U32(L, 0xa0 + 4u * j) == 0) continue;
        const rx_addr J = JREC(j);
        const uint32_t a = U32(J, 0x1b30), b = U32(J, 0x1b50);
        const rx_v4 qa = v4_ld(BODYQ(a)), rest = v4_ld(J + 0x1be0), qb = v4_ld(BODYQ(b));
        const rx_v4 stored = v4_ld(STORED(b));
        v4_st(L + 0x140, stored);
        if (U32(P, 0x38e0 + 4u * j) != 0) {
            /* the twist limit: the axis (0, -sin, cos) of the direction, through the rest rotation, then a's rotation */
            const rx_v4 v = v4(F32(0, 0x0092e12c), phys_sub(rx_bf(0x80000000u), F32(P, 0x3980 + 4u * j)), F32(P, 0x3930 + 4u * j),
                               F32(P, 0x3930 + 4u * j));
            v4_st(L + 0x110, zero4);
            const rx_v4 a1 = xyz(qnorm3(v));
            const rx_v4 a2 = xyz(qnorm3(qrot(rest, a1)));
            const rx_v4 axis = qnorm3(qrot(qa, a2));
            v4_st(L + 0x180, axis);
            const rx_v4 rot_wa = qrot(qa, xyz(v4_ld(BODYW(a))));
            const rx_v4 wb = v4_ld(BODYW(b));
            const rx_v4 rot_wb = qrot(qb, xyz(wb));
            const float s = phys_hsum3(v4_mul(v4_sub(rot_wb, rot_wa), axis));
            const rx_v4 corr = v4_mul(v4_mul(v4_splat(s), v4_ld(L + 0x90)), axis);
            v4_st(L + 0x170, corr);
            v4_st(BODYW(b), v4_sub(wb, qrot_inv(qb, xyz(corr))));
            v4_st(BODYW(a), v4_add(qrot_inv(qa, xyz(v4_ld(L + 0x170))), v4_ld(BODYW(a))));
        }
        if (U32(P, 0x3840 + 4u * j) == 0 && U32(P, 0x3890 + 4u * j) == 0) continue;
        /* the swing limit: the twist axis through r (r a r*), the rest rotation and a's rotation */
        const rx_v4 a3 = xyz(qnorm3(qrot(stored, xyz(v4_ld(0x00d615c0)))));
        v4_st(L + 0x110, zero4);
        const rx_v4 a4 = xyz(qnorm3(qrot(rest, a3)));
        const rx_v4 axis = qnorm3(qrot(qa, a4));
        v4_st(L + 0x160, axis);
        const rx_v4 rot_wa = qrot(qa, xyz(v4_ld(BODYW(a))));
        const rx_v4 wb = v4_ld(BODYW(b));
        const rx_v4 rot_wb = qrot(qb, xyz(wb));
        const float s = phys_hsum3(v4_mul(v4_sub(rot_wb, rot_wa), axis));
        const rx_v4 corr = v4_mul(v4_splat(s), axis);
        v4_st(BODYW(b), v4_sub(wb, qrot_inv(qb, xyz(corr))));
    }
#undef JREC
#undef BODYQ
#undef BODYW
#undef STORED
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00420770, phys_rider_joint_limits)

/* 004263a0 (ECX = body i, EDX = the contact point*; stack: A, the contact normal n*, the penetration*; RET 0xc): a ground contact
 * impulse on body i (orientation q = A+0xe80 + 0x70 i, body-frame angular velocity w = A+0xe90 + 0x70 i, position A+0x400 +
 * 0x80 i, linear velocity A+0x410 + 0x80 i): with d = p - pos, the contact velocity v + (q w q*) x d along n, the effective
 * mass n . ((q (I^-1 (q* (d x n) q)) q*) x d) + A+0x430 (I^-1 the diagonal A+0xeb0 + 0x70 i), the impulse -(e + 0xd61650) vn /
 * that times n (e = A+0xa868) applied at q* d q (00422180); then the tangential velocity's friction 0x9cc378 into A+0x450 and,
 * with A+0xa878, the penetration push 0x9c4750 * depth * n and 0x9cc374 times the normal velocity. Sets A+0xa870 = 0x9c4750.
 * Frame: push ebp; and esp, -16; sub esp, 0x94 (cookie +0x90); push ebx / esi / edi: +0x60 the impulse, +0x80 d (later the
 * lever). */
void phys_rider_contact_impulse(uint32_t i, rx_addr p, rx_addr A, rx_addr n_ptr, rx_addr depth_ptr) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x94 - 12;
    rx_set_sp(L);
    SU32(L, 0x9c, U32(0, 0x009b45e4) ^ (L + 0xc));
    SF32(A, 0xa870, F32(0, 0x009c4750));
    const rx_addr body = A + 0x80u * i;
    const rx_v4 q = v4_ld(A + 0xe80 + 0x70u * i), w = xyz(v4_ld(A + 0xe90 + 0x70u * i));
    const rx_v4 d = v4_sub(v4_ld(p), v4_ld(A + 0x80u * (i + 8)));
    v4_st(L + 0x80, d);
    const rx_v4 n = v4_ld(n_ptr);
    const rx_v4 wr = qrot(q, w);
    const rx_v4 vc = v4_add(phys_cross_ba(d, wr), v4_ld(body + 0x410));
    const float vn = phys_hsum3(v4_mul(vc, n));
    const rx_v4 u = xyz(v4_mul(qrot_inv(q, phys_cross_ba(n, d)), v4_ld(A + 0xeb0 + 0x70u * i)));
    const rx_v4 r = qrot(q, u);
    const float k = phys_hsum3(v4_mul(phys_cross_ba(d, r), n));
    const rx_v4 num = v4_mul(v4_sub(v4_zero(), v4_add(v4_splat(F32(A, 0xa868)), v4_ld(0x00d61650))), v4_splat(vn));
    const rx_v4 imp = v4_mul(v4_div(num, v4_add(v4_splat(k), v4_ld(body + 0x430))), n);
    v4_st(L + 0x60, imp);
    v4_st(L + 0x80, qrot_inv(q, xyz(d)));
    SU32(L - 4, 0, L + 0x80);
    CALL_AT_SP(L, 1, phys_rigbody_apply_impulse(A + 0x380, (int32_t)i, L + 0x60, L + 0x80));
    {
        const rx_v4 nn = v4_ld(n_ptr), v = v4_ld(body + 0x410);
        const float dn = phys_hsum3(v4_mul(nn, v));
        const rx_v4 t = v4_mul(v4_sub(v, v4_mul(v4_splat(dn), nn)), v4_splat(F32(0, 0x009cc378)));
        v4_st(body + 0x450, v4_add(t, v4_ld(body + 0x450)));
    }
    if (U32(A, 0xa878) != 0) {
        const rx_v4 push = v4_mul(v4_mul(v4_splat(F32(0, 0x009c4750)), v4_ld(depth_ptr)), v4_ld(n_ptr));
        v4_st(body + 0x450, v4_add(push, v4_ld(body + 0x450)));
        const rx_v4 nn = v4_ld(n_ptr), v = v4_ld(body + 0x410);
        const float dn = phys_hsum3(v4_mul(nn, v));
        const rx_v4 t = v4_mul(v4_mul(v4_splat(dn), nn), v4_splat(F32(0, 0x009cc374)));
        v4_st(body + 0x450, v4_add(t, v4_ld(body + 0x450)));
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x004263a0, phys_rider_contact_impulse)

/* the scene's collision world (0xa95844 views: +0x20 / +0x1c active -> 0xa946b8 +0x5a54 / +0x20c8), as 0042a310 / 00425b20 pick it */
static rx_addr scene_world(void) {
    const rx_addr scene = U32(0, 0x00a95844);
    if (U32(U32(scene, 0x20), 0x34) == 1) return U32(U32(0, 0x00a946b8), 0x5a54);
    if (U32(U32(scene, 0x1c), 0x34) == 1) return U32(U32(0, 0x00a946b8), 0x20c8);
    return 0;
}
/* the registered terrain of the active view (the static list 0x9e30d4, 0x9e30dc entries, its guard bit 0x9e30e0) */
static rx_addr active_terrain(void) {
    if ((U32(0, 0x009e30e0) & 1) == 0) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
    const rx_addr scene = U32(0, 0x00a95844);
    for (uint32_t k = 0; k < U32(0, 0x009e30dc); k++) {
        const rx_addr t = U32(0, 0x009e30d4 + 4 * k);
        if (t != 0 && U32(scene, 0x1c + 4 * U32(scene, 4)) == U32(t, 0x44a8)) return t;
    }
    return 0;
}

/* 00425b20 (stdcall A, dt; RET 8): the ragdoll's ground contacts. The 16 contact spheres (four groups of four at A+0xa768: the
 * body index at -4 / 0 / 4 / 8, the radius at +0x50..0x5c) are tested at their low point (position - up * r, up = A+0xa840)
 * against the terrain (TerrainHeightNormal 007c5e80); a sphere below the ground gets the contact impulse (004263a0 with the
 * depth (ground - low) . n), its friction factor at A+0x398 + 4 b (0.02, or between 0.18 |up.y| + 0.02 and 0.02 over the speed
 * 0..20) and the contact flag A+0xa4 + 4 k; the ground points / offsets are kept at A+0xf0 / A+0x1f0 + 0x10 k. With contacts:
 * A+0x374 = 1, the optional offset A+0xa890 added to the positions, up = the normalized normal sum (A+0xa850), the tumble spin
 * A+0xa810 (the speed after times 4.5 times the axis up x v over the rig's inertia about it, times dt) and its velocity at the root
 * A+0xa820, an impact record in the world's list (+0x1900 count, 64-byte records, at most 100), and A+0x300 / A+0x304 = 1 for a
 * fast spin (above 1) or impact (above 60); without: A+0xa830 = 0 and up = 0xd616a0. Then 00423df0. Frame: push ebp; and esp,
 * -16; sub esp, 0x1d4 (cookie +0x1d0); push ebx / esi / edi: +0x60 the low point, +0x80 the group's bodies, +0x90 dt, +0xb0 /
 * +0xc0 the mean velocities after / before, +0xd0.. the high / low points, +0x150 the normals, +0x190 the ground points, +0x1d4
 * the surface words. */
void phys_rider_ground_contacts(rx_addr A, float dt, const rx_regs* caller) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x1d4 - 12;
    rx_set_sp(L);
    SU32(L, 0x1dc, U32(0, 0x009b45e4) ^ (L + 0xc));
    const rx_addr P = A + 0x380;
    SU32(L, 0x30, P);
    CALL_AT_SP(L, 0, phys_rigbody_center_of_mass(P, A + 0xa5a0));
    SU32(A, 0x374, 0); SU32(A, 0x378, 0); SF32(A, 0x304, 0.0f); SF32(A, 0x300, 0.0f);
    SU32(A, 0xa834, 0); SF32(A, 0xa880, 0.0f);
    v4_st(A + 0xa850, v4_ld(0x00d61670));
    CALL_AT_SP(L, 0, phys_rigbody_mean_velocity(P, L + 0xc0));
    v4_st(L + 0x90, v4_splat(dt));
    for (uint32_t o = 0; o < 8; o += 2) SU16(L, 0x1d4 + o, 0);
    const float up_y = rx_bf(rx_fb(F32(A, 0xa844)) & U32(0, 0x00977570));
    SF32(L, 0x7c, up_y);
    for (uint32_t g = 0; g < 4; g++) {
        const rx_addr list = A + 0xa768 + 0x10u * g;
        const rx_v4 up = v4_ld(A + 0xa840);
        for (uint32_t k = 0; k < 4; k++) {
            const uint32_t b = U32(list, 4u * k - 4);
            SU32(L, 0x80 + 4u * k, b);
            const rx_v4 pos = v4_ld(A + 0x80u * (b + 8)), ur = v4_mul(up, v4_splat(F32(list, 0x50 + 4u * k)));
            v4_st(L + 0xd0 + 0x20u * k, v4_add(pos, ur));
            v4_st(L + 0xe0 + 0x20u * k, v4_sub(pos, ur));
        }
        for (uint32_t k = 0; k < 4; k++) {
            const uint32_t o = 0x10u * k, b = U32(L, 0x80 + 4u * k);
            const rx_v4 lo = v4_ld(L + 0xe0 + 0x20u * k);
            const rx_addr pt = L + 0x190 + o, nrm = L + 0x150 + o;
            v4_st(L + 0x60, lo);
            v4_st(pt, lo);
            const rx_addr terrain = active_terrain();
            uint32_t hit = 0;
            CALL_AT_SP(L, 2, hit = phys_terrain_height_normal(nrm, terrain, pt));
            if (hit != 0 && lo.v[1] < F32(pt, 4)) {
                const rx_v4 v = v4_ld(A + 0x410 + 0x80u * b), vv = v4_mul(v, v);
                const float speed = phys_sqrt(phys_add(phys_add(vv.v[0], vv.v[2]), vv.v[1]));
                SF32(L, 0x78, speed);
                const rx_v4 n = v4_ld(nrm);
                v4_st(L + 0xa0, n);
                v4_st(L + 0xb0, v4_splat(phys_hsum3(v4_mul(v4_sub(v4_ld(pt), v4_ld(L + 0x60)), n))));
                SU32(L - 4, 0, L + 0xb0); SU32(L - 8, 0, nrm); SU32(L - 12, 0, A);
                CALL_AT_SP(L, 3, phys_rider_contact_impulse(b, pt, A, nrm, L + 0xb0));
                v4_st(A + 0xa850, v4_add(v4_ld(L + 0xa0), v4_ld(A + 0xa850)));
                SU32(A, 0xa834, 1);
                float f;
                if (20.0f /* 0x935aac */ > speed) {
                    const float k0 = (float)((double)up_y * rx_bd(rx_rd64(0x00977d48)) /* 0.18000000342726707 */ + (double)0.02f /* 0x977d40 */);
                    f = (float)((((double)0.02f - (double)k0) * (double)speed) / 20.0 /* 0x9779a0 */ + (double)k0);
                } else {
                    f = 0.0199999996f /* 0x977408 */;
                }
                SF32(A, 0x398 + 4u * b, f);
                SU8(A, 0x2fc, U8(L, 0x1d4 + 2u * k + 1) & 0xf);
                SU32(A, 0xa4 + 0x10u * g + 4u * k, 1);
            }
            const rx_addr keep = A + 0x1f0 + 0x40u * g + o;
            v4_st(keep - 0x100, v4_ld(pt));
            v4_st(keep, v4_sub(v4_ld(pt), v4_ld(L + 0x60)));
        }
    }
    CALL_AT_SP(L, 0, phys_rigbody_mean_velocity(P, L + 0xb0));
    if (U32(A, 0xa834) == 0) {
        SU32(A, 0xa830, 0);
        v4_st(A + 0xa840, v4_ld(0x00d616a0));
    } else {
        const rx_v4 vb = v4_ld(L + 0xc0), vb2 = v4_mul(vb, vb);
        const float speed0 = phys_sqrt(phys_add(phys_add(vb2.v[0], vb2.v[2]), vb2.v[1]));
        v4_st(L + 0x60, v4_splat(1.0f));
        SU32(A, 0x374, 1);
        SF32(L, 0x18, speed0);
        if (U32(A, 0xa87c) != 0)
            for (int32_t i = 0; i < I32(P, 0x7c); i++) {
                const rx_addr pp = P + 0x80 + 0x80u * (uint32_t)i;
                v4_st(pp, v4_add(v4_ld(A + 0xa890), v4_ld(pp)));
            }
        v4_st(A + 0xa840, qnorm3(v4_ld(A + 0xa850)));
        const rx_v4 va = v4_ld(L + 0xb0);
        const rx_v4 axis = qnorm3(phys_cross_ba(va, v4_ld(A + 0xa840)));
        const rx_v4 va2 = v4_mul(va, va);
        const float s1 = phys_add(phys_add(va2.v[0], va2.v[2]), va2.v[1]);
        v4_st(L + 0xc0, axis);
        SU32(L - 4, 0, A + 0xa5a0); SU32(L - 8, 0, L + 0xb0);
        CALL_AT_SP(L, 2, phys_rigbody_axis_inertia(P, L + 0xc0, L + 0xb0, A + 0xa5a0));
        const rx_v4 spin = v4_div(v4_mul(v4_mul(v4_mul(v4_splat(phys_sqrt(s1)), v4_splat(F32(0, 0x009cd9e4))), axis), v4_ld(L + 0x90)),
                                  v4_ld(L + 0xb0));
        v4_st(A + 0xa810, spin);
        const rx_v4 root = v4_ld(A + 0x400);
        v4_st(L + 0xa0, root);
        v4_st(A + 0xa820, phys_cross_ba(v4_sub(root, v4_ld(A + 0xa5a0)), v4_ld(A + 0xa810)));
        SU32(A, 0xa830, 1);
        const rx_addr list = U32(scene_world(), 0x20134);
        const uint32_t count = U32(list, 0x1900);
        if (count < 100) {
            SU32(list, 0x1900, count + 1);
            const rx_addr rec = list + 64u * count;
            rx_regs r = caller ? *caller : phys_regs();
            r.r[R_ESP] = L; r.r[R_EBP] = sp0 - 4; r.r[R_EBX] = A; r.r[R_EDI] = 1; r.r[R_ESI] = rec;
            r.r[R_EAX] = U32(A, 0xc);
            tracked_body_record(&r);
            SU32(rec, 0, U32(r.r[R_EAX], 0x34));
            if ((U32(0, 0x009e30e0) & 1) == 0) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
            r.r[R_EDX] = 0x009e30d4;
            tracked_find_world(&r);
            SU32(rec, 4, U32(r.r[R_EAX], 0xe4));
            SU32(rec, 0x2c, 0); SU8(rec, 0x20, 0); SU32(rec, 0x30, 0xffffffffu); SU32(rec, 0x34, 0xffffffffu);
            SF32(rec, 0x10, F32(L, 0xa0)); SF32(rec, 0x14, F32(L, 0xa4)); SF32(rec, 0x18, F32(L, 0xa8));
            SF32(rec, 0x24, F32(L, 0x18)); SF32(rec, 0x28, 0.0f);
        }
        const rx_v4 sp = v4_ld(A + 0xa810), sp2 = v4_mul(sp, sp);
        const float spin_len = phys_sqrt(phys_add(phys_add(sp2.v[0], sp2.v[2]), sp2.v[1]));
        if (F32(L, 0x60) < spin_len || (double)F32(L, 0x18) > 60.0 /* 0x977d38 */) {
            SF32(A, 0x300, 1.0f); SF32(A, 0x304, 1.0f);
        }
    }
    SU32(L - 4, 0, P);
    uint32_t ecx = 0, edx = 0;
    CALL_AT_SP(L, 1, phys_ragdoll_fix_velocities(P, &ecx, &edx));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00425b20, phys_rider_ground_contacts)

/* the scene bone handle's record (owner +0x1960 records of 0xe0 bytes, range-checked against +0x1964) */
static rx_addr bone_record(rx_addr h) {
    const rx_addr owner = U32(h, 4);
    const uint32_t idx = U32(h, 8);
    if (idx >= (uint32_t)((I32(owner, 0x1964) - I32(owner, 0x1960)) / 0xe0)) invalid_parameter();
    return U32(owner, 0x1960) + idx * 0xe0;
}

/* 00429a40 (stdcall A, dt; RET 8): the ragdoll's contacts with the scene's collision bones (16 handles at A+0xc; a bone record's
 * +0xc0 contact normal n, +0xb0 contact point; body A+0xa764 + 4 k, radius A+0xa7b8 + 4 k). A bone with |n| > 0 (on all lanes
 * against 0xd61670) is touching (A+0x88 = 1). Per touching bone: a body moving into it (its velocity direction . n < 0) gets the
 * impulse -(v . n) n / m at the contact point (00422180); its tangential velocity is damped by dt * -3 (0x977a38); the
 * penetration (r - |p - pos|, plus r when the body's centre is in front) pushes it out with 300 / m (0x935d28) and the matching
 * torque (00422700); a body not moving in gets, when the push opposes its normal velocity, the extra -40 / m (0x9cc37c) and its
 * torque. The normal / point are read from the frame slots +0x50 / +0x60, which keep the last bone's when a handle is null (the
 * first normal is the frame's stale +0x50). Frame: push ebp; and esp, -16; sub esp, 0xe4 (cookie +0xe0); push ebx / esi / edi:
 * +0x20 v * n, +0x70 the impulse, +0x80 / +0xb0 the torques, +0x90 the push, +0xa0 v, +0xc0 the lever, +0xdc the touching
 * flags. */
void phys_rider_bone_contacts(rx_addr A, float dt) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0xe4 - 12;
    rx_set_sp(L);
    SU32(L, 0xec, U32(0, 0x009b45e4) ^ (L + 0xc));
    const rx_addr P = A + 0x380;
    const rx_v4 Z = v4_ld(0x00d61670);
    SU32(L, 0x14, A);
    SU8(A, 0x88, 0);
    for (uint32_t k = 0; k < 16; k++) {
        const rx_addr h = U32(A, 0xc + 8 * k);
        SU8(L, 0xdc + k, 0);
        if (h != 0) v4_st(L + 0x50, v4_ld(bone_record(h) + 0xc0));
        const rx_v4 n = v4_ld(L + 0x50), nn = v4_mul(n, n);
        const float len = phys_sqrt(phys_add(phys_add(nn.v[0], nn.v[2]), nn.v[1]));
        const rx_v4 m = v4_cmplt(Z, v4_splat(len));
        if (v4_lb(m, 2) + v4_lb(m, 1) + v4_lb(m, 0) == 0xfffffffdu) { SU8(A, 0x88, 1); SU8(L, 0xdc + k, 1); }
    }
    for (uint32_t k = 0; k < 16; k++) {
        if (U8(L, 0xdc + k) == 0) continue;
        const rx_addr h = U32(A, 0xc + 8 * k);
        const uint32_t b = U32(A, 0xa764 + 4 * k);
        if (h != 0) {
            v4_st(L + 0x50, v4_ld(bone_record(h) + 0xc0));
            v4_st(L + 0x60, v4_ld(bone_record(h) + 0xb0));
        }
        const rx_v4 n = v4_ld(L + 0x50);
        const rx_addr body = A + 0x80u * b, pos = A + 0x80u * (b + 8);
        const rx_v4 v = v4_ld(body + 0x410), vn4 = v4_mul(v, n);
        const float vn = phys_hsum3(vn4);
        const rx_v4 dir = guarded_div(v, phys_dot3(v, v));
        v4_st(L + 0xa0, v); v4_st(L + 0x20, vn4); v4_st(L + 0x30, v4_zero());
        int moving_in = 0;
        {
            const rx_v4 mm = v4_cmplt(v4_splat(phys_hsum3(v4_mul(dir, n))), Z);
            moving_in = v4_lb(mm, 2) + v4_lb(mm, 1) + v4_lb(mm, 0) == 0xfffffffdu;
        }
        if (moving_in) {
            const rx_v4 imp = v4_div(v4_mul(v4_sub(v4_zero(), v4_splat(vn)), n), v4_ld(body + 0x430));
            v4_st(L + 0x70, imp);
            const rx_v4 q = v4_ld(A + 0xe80 + 0x70u * b);
            v4_st(L + 0xc0, qrot_inv(q, xyz(v4_sub(v4_ld(L + 0x60), v4_ld(pos)))));
            SU8(L, 0xf, 0);
            SU32(L - 4, 0, L + 0xc0);
            CALL_AT_SP(L, 1, phys_rigbody_apply_impulse(P, (int32_t)b, L + 0x70, L + 0xc0));
        } else {
            SU8(L, 0xf, 1);
        }
        {
            const rx_v4 vv = v4_ld(L + 0xa0), nn = v4_ld(L + 0x50);
            const float f = (float)((double)dt * rx_bd(rx_rd64(0x00977a38)) /* -3.0 */);
            const rx_v4 tang = v4_mul(v4_sub(vv, v4_mul(v4_splat(phys_hsum3(v4_ld(L + 0x20))), nn)), v4_splat(f));
            v4_st(body + 0x410, v4_add(tang, v4_ld(body + 0x410)));
        }
        const rx_v4 pt = v4_ld(L + 0x60), d = v4_sub(pt, v4_ld(pos)), dd = v4_mul(d, d);
        const rx_v4 r = v4_splat(F32(A, 0xa7b8 + 4 * k));
        rx_v4 pen = v4_sub(r, v4_splat(phys_sqrt(phys_add(phys_add(dd.v[0], dd.v[2]), dd.v[1]))));
        {
            const rx_v4 mm = v4_cmplt(Z, v4_splat(phys_hsum3(v4_mul(d, n))));
            if (v4_lb(mm, 2) + v4_lb(mm, 1) + v4_lb(mm, 0) == 0xfffffffdu) pen = v4_add(pen, r);
        }
        const rx_v4 push = v4_mul(v4_div(v4_splat(F32(0, 0x00935d28)), v4_ld(body + 0x430)), v4_mul(pen, n));
        v4_st(body + 0x450, v4_add(v4_ld(body + 0x450), push));
        v4_st(L + 0x90, push);
        v4_st(L + 0x80, phys_cross_ba(push, v4_sub(pt, v4_ld(pos))));
        SU32(L - 4, 0, L + 0x80); SU32(L - 8, 0, b);
        CALL_AT_SP(L, 2, phys_rigbody_add_torque(P, (int32_t)b, L + 0x80));
        const rx_v4 vnn = v4_mul(v4_splat(phys_hsum3(v4_ld(L + 0x20))), v4_ld(L + 0x50));
        if (U8(L, 0xf) == 0) continue;
        const rx_v4 mm = v4_cmplt(Z, v4_splat(phys_hsum3(v4_mul(vnn, v4_ld(L + 0x90)))));
        if (v4_lb(mm, 2) + v4_lb(mm, 1) + v4_lb(mm, 0) != 0xfffffffdu) continue;
        const rx_v4 imp2 = v4_mul(v4_sub(v4_ld(L + 0x30), v4_div(v4_splat(F32(0, 0x009cc37c)), v4_ld(body + 0x430))), vnn);
        v4_st(body + 0x450, v4_add(v4_ld(body + 0x450), imp2));
        v4_st(L + 0xb0, phys_cross_ba(imp2, v4_sub(v4_ld(L + 0x60), v4_ld(pos))));
        SU32(L - 4, 0, L + 0xb0); SU32(L - 8, 0, b);
        CALL_AT_SP(L, 2, phys_rigbody_add_torque(P, (int32_t)b, L + 0xb0));
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00429a40, phys_rider_bone_contacts)

/* the impact record of 00425b20 / 0042a6c0 in the world's list (+0x1900 count, 64-byte records, at most 100): +0 the rider's
 * id (0042bd90 on A+0xc, +0x34), +4 the kind, +0x10 the position, +0x24 the speed; returns 0 when the list is full */
static rx_addr impact_record(rx_addr A, rx_regs* r) {
    const rx_addr list = U32(scene_world(), 0x20134);
    const uint32_t count = U32(list, 0x1900);
    if (count >= 100) return 0;
    SU32(list, 0x1900, count + 1);
    const rx_addr rec = list + 64u * count;
    r->r[R_ESI] = rec; r->r[R_EAX] = U32(A, 0xc);
    tracked_body_record(r);
    SU32(rec, 0, U32(r->r[R_EAX], 0x34));
    return rec;
}

/* 0042a6c0 (stdcall A; RET 4): the ragdoll in water. Per contact sphere k (body A+0xa764 + 4 k, radius r = A+0xa7b8 + 4 k,
 * centre the bone's world position +0x40 from the handle at A+0xc + 8 k, kept in the frame's +0xd0 when the handle is null) the
 * water query 007efef0 (on the static water list 0xd72aac, created once with 0042b410 / 007cf450) gives the surface height w:
 * a sphere wholly below adds the buoyancy (pi g 0.4) 4/3 r^3 up (pi 0x977498, g 32.2 0x977878, 0.4 0x935860, up 0xd616a0)
 * to its force A+0x450 + 0x80 b; a partly wet one the cap's ((r^2 - d^2 / 3) d K) up plus (when the centre is under) 2/3 K r^3
 * up, d the centre's distance to the surface; the first wet frame (A+0xa8a0) starts the splash (007efed0) and an impact record
 * (00424340's kind). Wet bodies are damped (linear * 0.9 0x9cd970, angular * 0.85 0x9cd958). Frame: push ebp; and esp, -16;
 * sub esp, 0x104 (cookie +0x100); push ebx / esi / edi: +0x20 the query point, +0x40 w, +0x50.. the constants, +0xc0 r,
 * +0xd0 the centre, +0xe0 K, +0xf0 the mean velocity. */
void phys_rider_water(rx_addr A, const rx_regs* caller) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x104 - 12;
    rx_set_sp(L);
    SU32(L, 0x10c, U32(0, 0x009b45e4) ^ (L + 0xc));
    rx_regs r = caller ? *caller : phys_regs();
    r.fpu_in = 0; r.r[R_ESP] = L; r.r[R_EBP] = sp0 - 4; r.r[R_EBX] = A;
    v4_st(L + 0x80, v4_splat(F32(0, 0x009776dc)));       /* 4/3 */
    v4_st(L + 0xa0, v4_splat(F32(0, 0x00977714)));       /* 2/3 */
    v4_st(L + 0x90, v4_splat(F32(0, 0x0092e214)));       /* 1/3 */
    v4_st(L + 0x50, v4_splat(F32(0, 0x009cd958)));       /* angular damping */
    v4_st(L + 0x60, v4_splat(F32(0, 0x009cd970)));       /* linear damping */
    const rx_v4 K = v4_mul(v4_mul(v4_splat(F32(0, 0x00977498)), v4_splat(F32(0, 0x00977878))), v4_splat(F32(0, 0x00935860)));
    v4_st(L + 0xe0, K);
    SU32(L, 0x3c, A);
    for (uint32_t k = 0; k < 16; k++) {
        const uint32_t b = U32(A, 0xa764 + 4 * k);
        SU32(L, 0x18, b);
        const rx_addr h = U32(A, 0xc + 8 * k);
        if (h != 0) v4_st(L + 0xd0, v4_ld(bone_record(h) + 0x40));
        const rx_v4 rr = v4_splat(F32(A, 0xa7b8 + 4 * k));
        v4_st(L + 0xc0, rr);
        SF32(L, 0x20, F32(L, 0xd0)); SF32(L, 0x24, F32(L, 0xd4)); SF32(L, 0x28, F32(L, 0xd8));
        if ((U32(0, 0x00d72aa8) & 1) == 0) {                            /* the static water list, created once */
            SU32(0, 0x00d72aa8, U32(0, 0x00d72aa8) | 1);
            const uint32_t a4[1] = {4};
            rx_addr obj;
            SU32(rx_sp() - 4, 0, a4[0]);
            PHYS_CALL_SP(4, obj = rx_reflex_aligned_alloc(a4[0]));   /* ReflexAlignedAlloc */
            if (obj != 0) SU32(obj, 0, 0x00d72ab0);
            SU32(0, 0x00d72ab0, obj);
            SU32(L, 0x38, 0);
            #ifdef RX_ORACLE
            { const uint32_t a[2] = {0x00d72ab0, L + 0x38}; tracked_call(&r, 0x0042b410, 2, a); }
#else
            CALL_AT_SP(L,2,(void)rx_vector_int_construct16(0x00d72ab0,L+0x38));
#endif
            if (U32(0, 0x00d72abc) > U32(0, 0x00d72ac0)) invalid_parameter();
            if (U32(0, 0x00d72abc) > U32(0, 0x00d72ac0)) invalid_parameter();
            {
                const uint32_t a[5] = {L + 0x78, U32(0, 0x00d72ab0), U32(0, 0x00d72abc), U32(0, 0x00d72ab0), U32(0, 0x00d72ac0)};
                r.r[R_ESI] = 0x00d72ab0; r.r[R_ECX] = U32(0, 0x00d72ab0);
#ifdef RX_ORACLE
                tracked_call(&r, 0x007cf450, 5, a);
#else
                CALL_AT_SP(L,5,r.r[R_EAX]=phys_vector_erase(0x00d72ab0,a[0],a[2],a[4]));
#endif
            }
#ifdef RX_ORACLE
            { const uint32_t a[1] = {0x00911e90}; rx_call_original(0x008f9fbf, RX_CDECL, 0, 1, a); }   /* _atexit */
#else
            CALL_AT_SP(L,1,(void)rx_crt_atexit(0x00911e90));
#endif
            r.r[R_EBX] = A; r.r[R_EDI] = b;
        }
        {   /* 007efef0 (the water set's surface height; the core's port, world_node.h): the set in ESI, the point on the stack */
            r.r[R_ESI] = 0x00d72aac; r.r[R_EAX] = L + 0x20;
#ifdef RX_ORACLE
            const uint32_t a[1] = {L + 0x20};
            tracked_call(&r, 0x007efef0, 1, a);
#else
            uint32_t found;
            CALL_AT_SP(L, 1, found = rx_world_water_set_height(0x00d72aac, L + 0x20));
            r.r[R_EAX] = found;
#endif
        }
        if (r.r[R_EAX] == 0) continue;
        const rx_v4 p = v4_ld(L + 0xd0), up = v4_ld(0x00d616a0);
        const rx_v4 w = v4_splat(F32(L, 0x24));
        v4_st(L + 0x40, w);
        rx_v4 low = p; low.v[1] = phys_sub(p.v[1], rr.v[1]);
        const rx_v4 m1 = v4_cmple(w, v4_splat(phys_hsum3(v4_mul(low, up))));
        if (v4_lb(m1, 2) + v4_lb(m1, 1) + v4_lb(m1, 0) == 0xfffffffdu) continue;    /* wholly above */
        const rx_addr body = A + 0x80u * b;
        rx_v4 top = p; top.v[1] = phys_add(rr.v[1], p.v[1]);
        const rx_v4 m2 = v4_cmple(v4_splat(phys_hsum3(v4_mul(top, up))), w);
        if (v4_lb(m2, 2) + v4_lb(m2, 1) + v4_lb(m2, 0) == 0xfffffffdu) {
            const rx_v4 f = v4_mul(v4_mul(v4_mul(v4_mul(v4_mul(v4_ld(L + 0xe0), v4_ld(L + 0x80)), rr), rr), rr), up);
            v4_st(body + 0x450, v4_add(f, v4_ld(body + 0x450)));
        } else {
            if (U32(A, 0xa8a0) == 0) {
                if ((U32(0, 0x00d72aa8) & 1) == 0) {
                    SU32(0, 0x00d72aa8, U32(0, 0x00d72aa8) | 1);
#ifdef RX_ORACLE
                    tracked_call(&r, 0x004242f0, 0, NULL);
                    const uint32_t a[1] = {0x00911e90};
                    rx_call_original(0x008f9fbf, RX_CDECL, 0, 1, a);
#else
                    CALL_AT_SP(L,0,(void)rx_water_heights_init());
                    CALL_AT_SP(L,1,(void)rx_crt_atexit(0x00911e90));
#endif
                }
                {
                    const uint32_t a[1] = {rx_fb(1.0f)};
                    r.r[R_EAX] = 0x00d72aac; r.r[R_ECX] = L + 0x20;
                    /*0042aa38..0042aa4f: MOVSS loads this sphere's radius
                     * immediately before the splash; it is not stale XMM0. */
                    r.xmm[0]=v4(rr.v[0],0,0,0);
#ifdef RX_ORACLE
                    tracked_call(&r, 0x007efed0, 1, a);
#else
                    CALL_AT_SP(L,1,rx_wake_add_guarded_splat(0x00d72aac,L+0x20,a[0],rr.v[0]));
#endif
                }
                CALL_AT_SP(L, 0, phys_rigbody_mean_velocity(A + 0x380, L + 0xf0));
                const rx_addr rec = impact_record(A, &r);
                if (rec != 0) {
#ifdef RX_ORACLE
                    tracked_call(&r, 0x00424340, 0, NULL);
#else
                    CALL_AT_SP(L,0,r.r[R_EAX]=rx_atv_water_singleton());
#endif
                    const rx_addr obj = r.r[R_EAX];
                    if (((uint32_t)(I32(obj, 0x14) - I32(obj, 0x10)) >> 2) == 0) invalid_parameter();
                    SU32(rec, 4, U32(U32(U32(obj, 0x10), 0), 0x50));
                    SU32(rec, 0x2c, 0); SU8(rec, 0x20, 0); SU32(rec, 0x30, 0xffffffffu); SU32(rec, 0x34, 0xffffffffu);
                    SF32(rec, 0x10, F32(L, 0x20)); SF32(rec, 0x14, F32(L, 0x24)); SF32(rec, 0x18, F32(L, 0x28));
                    const rx_v4 mv = v4_ld(L + 0xf0), mv2 = v4_mul(mv, mv);
                    SF32(rec, 0x24, phys_sqrt(phys_add(phys_add(mv2.v[0], mv2.v[2]), mv2.v[1])));
                    SF32(rec, 0x28, 0.0f);
                }
            }
            SU32(A, 0xa8a0, 1);
            const rx_v4 Kc = v4_ld(L + 0xe0), rc = v4_ld(L + 0xc0), wc = v4_ld(L + 0x40);
            const rx_v4 hp = v4_splat(phys_hsum3(v4_mul(v4_ld(L + 0xd0), up)));
            const rx_v4 m3 = v4_cmple(hp, wc);
            rx_v4 base = v4_mul(v4_mul(v4_mul(v4_mul(v4_mul(Kc, v4_ld(L + 0xa0)), rc), rc), rc), up), d;
            if (v4_lb(m3, 2) + v4_lb(m3, 1) + v4_lb(m3, 0) == 0xfffffffdu) {
                d = v4_sub(wc, hp);
            } else {
                base = v4_ld(0x00d61670);
                d = v4_sub(hp, wc);
            }
            const rx_v4 cap = v4_mul(v4_mul(v4_sub(v4_mul(rc, rc), v4_mul(v4_mul(d, d), v4_ld(L + 0x90))), v4_mul(d, Kc)), up);
            v4_st(body + 0x450, v4_add(v4_add(cap, base), v4_ld(body + 0x450)));
        }
        v4_st(body + 0x410, v4_mul(v4_ld(L + 0x60), v4_ld(body + 0x410)));
        const rx_addr wa = A + 0xe90 + 0x70u * b;
        v4_st(wa, v4_mul(v4_ld(wa), v4_ld(L + 0x50)));
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0042a6c0, phys_rider_water)

/* 00425640 (stdcall A, dt; RET 8): one crash-physics step of the rider: the tumble spin / velocity from the last impact
 * (A+0xa830: 00422450 with A+0xa810, A+0x410 += A+0xa820); before the ragdoll settles (A+0x374, A+0x88 clear) the random spins
 * of the limbs 7 8 10 11 18 19 14 15; the wake test (0042a310 while A+0x94); the joint limits and the fixed joints; the
 * velocities saved, forces cleared and the friction factors P+0x18 zeroed; the ground contacts (00425b20) with the saved
 * velocities restored after; the bike contacts (00426d00, phys_crash_bike_solver), the bone contacts (00429a40) and the water
 * (0042a6c0); the ragdoll step (004238a0), damping (00423980) and the speed clamp (00425920); A+0x8c = the ragdoll is
 * settled-or-touching, and its rising edge starts the camera's speed (0042ade0). Frame: push ebp; and esp, -16; push ecx;
 * push ebx / esi / edi. Returns the ECX left. */
uint32_t phys_rider_crash_step(rx_addr A, float dt, const rx_regs* caller) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 16;
    rx_set_sp(L);
    const rx_addr P = A + 0x380;
    rx_regs r = caller ? *caller : phys_regs();
    r.fpu_in = 0; r.r[R_ESP] = L; r.r[R_EBP] = sp0 - 4; r.r[R_EBX] = A;
    if (U32(A, 0xa830) != 0) {
        CALL_AT_SP(L, 0, phys_rigbody_add_world_angular_velocity(P, A + 0xa810));
        v4_st(A + 0x410, v4_add(v4_ld(A + 0xa820), v4_ld(A + 0x410)));
    }
    if (U32(A, 0x374) == 0 && U8(A, 0x88) == 0) {
        static const int32_t limbs[8] = {7, 8, 10, 11, 18, 19, 14, 15};
        for (int i = 0; i < 8; i++) {
            SF32(L - 4, 0, 1.0f);
            CALL_AT_SP(L, 1, phys_rigbody_random_spin(P, limbs[i], 1.0f));
        }
    }
    if (U8(A, 0x94) != 0) CALL_AT_SP(L, 0, phys_rider_wake_if_clear(A, &r));
    uint32_t ecx = 0, edx = 0;
    SU32(L - 4, 0, P); CALL_AT_SP(L, 1, phys_rider_joint_limits(P, &r));
    SU32(L - 4, 0, P); CALL_AT_SP(L, 1, phys_ragdoll_fix_positions(P, &ecx, &edx));
    SU32(L - 4, 0, P); CALL_AT_SP(L, 1, phys_ragdoll_fix_velocities(P, &ecx, &edx));
    SU32(A, 0x90, U32(A, 0x8c));
    CALL_AT_SP(L, 0, phys_rigbody_save_velocities(P));
    CALL_AT_SP(L, 0, phys_rigbody_clear_forces(P));
    for (int32_t i = 0; i < I32(P, 0x7c); i++) SF32(P, 0x18 + 4u * (uint32_t)i, 0.0f);
    SF32(L - 4, 0, dt); SU32(L - 8, 0, A);
    CALL_AT_SP(L, 2, phys_rider_ground_contacts(A, dt, &r));
    CALL_AT_SP(L, 0, phys_rigbody_restore_velocities(P));
    {   /* 00426d00: the ragdoll against the bike */
        r.fpu_in = 0; r.r[R_ESP] = L; r.r[R_EBP] = sp0 - 4;
        r.r[R_EBX] = A; r.r[R_EDI] = P; r.r[R_ESI] = P;
        CALL_AT_SP(L, 2, phys_crash_bike_solver(A, dt));
    }
    SF32(L - 4, 0, dt); SU32(L - 8, 0, A);
    CALL_AT_SP(L, 2, phys_rider_bone_contacts(A, dt));
    SU32(L - 4, 0, A);
    CALL_AT_SP(L, 1, phys_rider_water(A, &r));
    CALL_AT_SP(L, 0, phys_ragdoll_step(P, dt));
    SU32(L - 4, 0, P);
    CALL_AT_SP(L, 1, phys_rigbody_damp(P, dt));
    SU32(L - 4, 0, A);
    CALL_AT_SP(L, 1, phys_rider_clamp_speed(A, &ecx, &edx));
    const uint32_t active = U32(A, 0x374) != 0 || U8(A, 0x88) != 0;
    const uint32_t was = U32(A, 0x90);
    SU32(A, 0x8c, active);
    if (was == 0 && active != 0) {
#ifdef RX_ORACLE
        const uint32_t a[1] = {A};
        r.r[R_ECX] = ecx; r.r[R_EDX] = edx; r.r[R_EAX] = active; r.r[R_EDI] = P;
        tracked_call(&r, 0x0042ade0, 1, a);
        ecx = r.r[R_ECX];
#else
        /* PRODUCT BOUNDARY (camera, oracle original): 0042ade0 hands the rider's speed (the axis dot, splatted) to the
         * active camera entity's vf34. The product's active-camera slot is null at this point (the camera unit owns the
         * view entities: Player_ActiveCameraEntity 0054c020 returns 0, so the vcall would be through address 0). Nothing
         * gameplay reads the vector; the oracle's tracked call above keeps the original (and its vcall) in the reference. */
        ecx = U32(0, 0x009b45e4);                                   /* the epilogue's cookie */
#endif
    }
    rx_set_sp(sp0);
    return ecx;
}
RX_FUNCTION(0x00425640, phys_rider_crash_step)

/* 00425600 (ESI = A; stdcall dt; RET 4): the crash step with dt clamped (above 0.03 (0x977dd8) it becomes 0x9cf5a8), then the
 * crash sound (004252a0, tail-called in the dt slot). Returns the ECX left. */
uint32_t phys_rider_crash_update(rx_addr A, float dt, const rx_regs* caller) {
    const rx_addr sp0 = rx_sp();
    if ((double)dt > (double)0.03f) { dt = F32(0, 0x009cf5a8); SF32(sp0, 4, dt); }
    SF32(sp0 - 4, 0, dt); SU32(sp0 - 8, 0, A);
    uint32_t ecx = 0;
    CALL_AT_SP(sp0, 2, ecx = phys_rider_crash_step(A, dt, caller));
    SU32(sp0, 4, A);
    rx_regs r = caller ? *caller : phys_regs();
    r.r[R_ECX] = ecx; r.r[R_ESI] = A;
    rx_set_sp(sp0 + 4);
    CALL_AT_SP(sp0 + 4, 0, phys_rider_crash_sound(A, &r));
    rx_set_sp(sp0);
    return r.r[R_ECX];
}
RX_FUNCTION(0x00425600, phys_rider_crash_update)
