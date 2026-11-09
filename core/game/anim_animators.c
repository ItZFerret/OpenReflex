/* anim_animators.c -- the animation tree's animators (0x0083xxxx: Blend / Additive / Motion / Switch / Performance).
 * Every animator's vtable slot 4 evaluates it: thiscall (character, root delta out or 0, pose), RET 0xc. A pose is the character's
 * bone count (+0x234) of 0x30-byte bone records: rotation quaternion +0x00 (x y z w), translation +0x10, "rotation valid" +0x20 and
 * "translation valid" +0x21 bytes. Temporary poses come from the character's pose pool (anim_pose_acquire) and are handed back by
 * clearing their used flag. (core/notes/anim.md) */
#include "anim.h"
#include "crt.h"
#include "anim_extern.h"
#include "anim_math.h"
#include "loop_jobs.h"

/* Hands a pool pose back: used flag [+0x22c][slot] = 0 under the pool's critical section (slot = (pose - base) / 0x30 / bones). */
static void pose_release(rx_addr ch, rx_addr pose) {
    anim_enter_cs(ch + 0x208);
    int32_t records = (int32_t)(pose - U32(ch, 0x228)) / 0x30;
    uint32_t slot = (uint32_t)records / U32(ch, 0x234);
    SU32(U32(ch, 0x22c), 4u * slot, 0);
    anim_leave_cs(ch + 0x208);
}

/* 00834390 (EAX = blend animator, EDI = pose; stack: character, root delta out, which; RET 0xc): a blend weight at one end
 * evaluates only that side (which != 0: the first child +0x58, else the second +0x5c) into the reset pose, marks every bone's
 * rotation and translation valid and passes the child's root delta on. The translations go through the x87 (fld / fstp): the
 * unrolled part of the original loop (groups of four bones) does that for x only, the rest for x y z. */
void rx_blend_animator_eval_one(rx_addr self, rx_addr pose, rx_addr ch, rx_addr delta_out, uint32_t which) {
    ANIM_FRAME_ENTER(A, 0x20);                                /* push ebp; and esp, -16; sub esp, 0x18; push ebx, esi */
    anim_pose_reset(ch, pose);
    rx_addr delta = A - 0x10;
    SF32(delta, 0, F32(0, 0xa95d50)); SF32(delta, 4, F32(0, 0xa95d54)); SF32(delta, 8, F32(0, 0xa95d58));
    anim_animator_eval(U32(self, which ? 0x58 : 0x5c), ch, delta, pose, which ? 0x008343ea : 0x008343fc);
    uint32_t n = U32(ch, 0x234), unrolled = (int32_t)n >= 4 ? (n & ~3u) : 0;
    for (uint32_t i = 0; i < n; i++) {
        rx_addr b = pose + 0x30u * i;
        SU8(b, 0x20, 1); SU8(b, 0x21, 1);
        anim_x87_restore_f32(b + 0x10);
        if (i >= unrolled) { anim_x87_restore_f32(b + 0x14); anim_x87_restore_f32(b + 0x18); }
    }
    SF32(delta_out, 0, F32(delta, 0)); SF32(delta_out, 4, F32(delta, 4)); SF32(delta_out, 8, F32(delta, 8));
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x00834390, rx_blend_animator_eval_one)

/* 008349c0 AdditiveAnimator::vf4: the base child (+0x2c) into the pose, the additive child (+0x30) into a pool pose; then per bone
 * rotation = base * additive (quaternion product in double precision) and translation = base + additive, both valid. The root
 * delta is the 0xa95d50 vector. */
void rx_additive_animator_eval(rx_addr self, rx_addr ch, rx_addr delta_out, rx_addr pose) {
    ANIM_FRAME_ENTER(A, 0x20);                                /* push ebp; and esp, -16; sub esp, 0x14; push ebx, esi, edi */
    anim_animator_eval(U32(self, 0x2c), ch, delta_out, pose, 0x008349e4);
    rx_addr add = anim_pose_acquire(ch);
    anim_animator_eval(U32(self, 0x30), ch, 0, add, 0x008349f9);
    for (uint32_t i = 0, n = U32(ch, 0x234); i < n; i++) {
        rx_addr a = pose + 0x30u * i, b = add + 0x30u * i;
        double ax = F32(a, 0), ay = F32(a, 4), az = F32(a, 8), aw = F32(a, 0xc);
        double bx = F32(b, 0), by = F32(b, 4), bz = F32(b, 8), bw = F32(b, 0xc);
        float w = (float)(((bw * aw - bx * ax) - by * ay) - az * bz);
        float x = (float)(((bw * ax + bx * aw) + ay * bz) - by * az);
        float y = (float)(((bw * ay + by * aw) + bx * az) - ax * bz);
        float z = (float)(((bw * az + aw * bz) + by * ax) - bx * ay);
        SU8(a, 0x20, 1);
        SF32(a, 0, x); SF32(a, 4, y); SF32(a, 8, z); SF32(a, 0xc, w);
        SF32(a, 0x10, F32(b, 0x10) + F32(a, 0x10));
        SF32(a, 0x14, F32(b, 0x14) + F32(a, 0x14));
        SF32(a, 0x18, F32(b, 0x18) + F32(a, 0x18));
        SU8(a, 0x21, 1);
    }
    pose_release(ch, add);
    if (delta_out) { SF32(delta_out, 0, F32(0, 0xa95d50)); SF32(delta_out, 4, F32(0, 0xa95d54)); SF32(delta_out, 8, F32(0, 0xa95d58)); }
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x008349c0, rx_additive_animator_eval)

/* 00833080 BlendAnimator::vf4: weight +0x68 at an end (< 1e-7 / > 0.9999999) evaluates one child (00834390); otherwise both children
 * into pool poses (+0x50 from +0x58 with root delta +0x30, +0x54 from +0x5c with +0x40) and per bone: rotation slerp by the weight
 * (only one side valid: that side; none: invalid), translation lerp (same rule; copies pass the x87). Root delta = lerp of the two
 * child deltas (written whatever the pointer). Both pool poses are handed back. */
void rx_blend_animator_eval(rx_addr self, rx_addr ch, rx_addr delta_out, rx_addr pose) {
    ANIM_FRAME_ENTER(A, 0x50);                                /* push ebp; and esp, -16; sub esp, 0x44; push ebx, esi, edi */
    float w = F32(self, 0x68);
    if (1e-07f /* 0x977324 */ > w || w > 0.9999999f /* 0x977320 */) {
        const uint32_t which = 1e-07f > w;                    /* EAX = self, EDI = pose; stack: ch, delta out, which */
        ANIM_PUSH(which); ANIM_PUSH(delta_out); ANIM_PUSH(ch); ANIM_PUSH(which ? 0x008330b4u : 0x008330dau);
        rx_blend_animator_eval_one(self, pose, ch, delta_out, which);
        rx_set_sp(rx_sp() + 0x10);
        ANIM_FRAME_LEAVE(A);
        return;
    }
    anim_pose_reset(ch, pose);
    SU32(self, 0x50, anim_pose_acquire(ch));
    anim_pose_reset(ch, U32(self, 0x50));
    anim_animator_eval(U32(self, 0x58), ch, self + 0x30, U32(self, 0x50), 0x00833116);
    rx_addr p1 = U32(self, 0x50);
    SU32(self, 0x54, anim_pose_acquire(ch));
    anim_pose_reset(ch, U32(self, 0x54));
    anim_animator_eval(U32(self, 0x5c), ch, self + 0x40, U32(self, 0x54), 0x00833141);
    rx_addr p2 = U32(self, 0x54);
    for (uint32_t i = 0, n = U32(ch, 0x234); i < n; i++) {
        rx_addr a = p1 + 0x30u * i, b = p2 + 0x30u * i, o = pose + 0x30u * i;
        SU8(o, 0x20, 1);
        if (U8(a, 0x20) && U8(b, 0x20)) {
            float d = ((F32(b, 0) * F32(a, 0) + F32(a, 4) * F32(b, 4)) + F32(b, 8) * F32(a, 8)) + F32(b, 0xc) * F32(a, 0xc);
            float s0, s1;
            anim_slerp_weights(d, F32(self, 0x68), &s0, &s1);
            SF32(o, 0xc, F32(b, 0xc) * s1 + F32(a, 0xc) * s0);
            SF32(o, 0, F32(b, 0) * s1 + F32(a, 0) * s0);
            SF32(o, 4, F32(a, 4) * s0 + F32(b, 4) * s1);
            SF32(o, 8, F32(b, 8) * s1 + F32(a, 8) * s0);
        } else if (U8(a, 0x20)) {
            for (int j = 0; j < 4; j++) SU32(o, 4 * j, U32(a, 4 * j));
        } else if (U8(b, 0x20)) {
            for (int j = 0; j < 4; j++) SU32(o, 4 * j, U32(b, 4 * j));
        } else SU8(o, 0x20, 0);
        SU8(o, 0x21, 1);
        if (U8(a, 0x21) && U8(b, 0x21)) {
            float k = F32(self, 0x68);
            for (int j = 0; j < 3; j++) {
                float x = F32(a, 0x10 + 4 * j);
                SF32(o, 0x10 + 4 * j, x - (x - F32(b, 0x10 + 4 * j)) * k);
            }
        } else if (U8(a, 0x21)) {
            for (int j = 0; j < 3; j++) anim_x87_copy_f32(o + 0x10 + 4 * j, a + 0x10 + 4 * j);
        } else if (U8(b, 0x21)) {
            for (int j = 0; j < 3; j++) anim_x87_copy_f32(o + 0x10 + 4 * j, b + 0x10 + 4 * j);
        } else SU8(o, 0x21, 0);
    }
    float k = F32(self, 0x68);
    float dx = F32(self, 0x30) - F32(self, 0x40), dy = F32(self, 0x34) - F32(self, 0x44), dz = F32(self, 0x38) - F32(self, 0x48);
    dy = dy * k; dx = dx * k; dz = dz * k;
    SF32(delta_out, 0, F32(self, 0x30) - dx); SF32(delta_out, 4, F32(self, 0x34) - dy); SF32(delta_out, 8, F32(self, 0x38) - dz);
    pose_release(ch, U32(self, 0x54));
    pose_release(ch, U32(self, 0x50));
    ANIM_FRAME_LEAVE(A);
    SU32(self, 0x50, 0); SU32(self, 0x54, 0);
}
RX_FUNCTION(0x00833080, rx_blend_animator_eval)

/* BlendAnimator::vf4 called directly from an original call site (its three pushes and the return address `ret`). */
static void blend_eval_from(rx_addr self, rx_addr ch, rx_addr delta_out, rx_addr pose, rx_addr ret) {
    ANIM_PUSH(pose); ANIM_PUSH(delta_out); ANIM_PUSH(ch); ANIM_PUSH(ret);
    rx_blend_animator_eval(self, ch, delta_out, pose);
    rx_set_sp(rx_sp() + 0x10);
}

/* 00840040 (engine): a compressed motion sampled into a pose: ECX = pose, ESI = compressed track set, xmm0 = time, stack = bone map;
 * RET 4. */
static void motion_sample_compressed(rx_addr pose, rx_addr tracks, float time, rx_addr map) {
    const rx_addr sp0 = rx_sp();
    ANIM_PUSH(map); ANIM_PUSH(0);
    rx_motion_sample(map, pose, tracks, time);
    rx_set_sp(sp0);
}

/* 0083c530 Pose_CopyBone (ECX = source bone record, EAX = destination): rotation and translation through the x87, valid bytes. */
void rx_pose_copy_bone(rx_addr src, rx_addr dst) {
    for (int j = 0; j < 7; j++) anim_x87_copy_f32(dst + 4u * (uint32_t)j, src + 4u * (uint32_t)j);
    SU8(dst, 0x20, U8(src, 0x20));
    SU8(dst, 0x21, U8(src, 0x21));
}
RX_FUNCTION(0x0083c530, rx_pose_copy_bone)

/* 0083c2f0 Pose_Mirror (cdecl: pose): mirrors a pose left / right with the 28-entry table at 0xd6b5e8 (16 bytes: bone, mirror bone
 * or -1, "copy" flag): a bone paired with another swaps rotations with it, negating two components (x y; x z for entries
 * 0x14..0x16; entry 5 (the root) maps (x y z w) -> (-w z y -x)); a bone of its own negates x and z, or (flag) takes (-y -x w z)
 * and negates its translation x. */
void rx_pose_mirror(rx_addr pose) {
    const float neg = -1.0f /* 0x9cd9e8 */;
    for (int32_t i = 0; i < 0x1c; i++) {
        rx_addr e = 0x00d6b5e8u + 16u * (uint32_t)i;
        int32_t a = I32(e, 0), b = I32(e, 4);
        if (a == -1) continue;
        if (b == -1) {
            rx_addr r = pose + 0x30u * (uint32_t)a;
            if (U32(e, 8) != 0) {
                RX_FRAME_ENTER(F, 0x30);          /* the copy is a local of the original's frame */
                rx_pose_copy_bone(r, F);
                float x = F32(F, 0), y = F32(F, 4), z = F32(F, 8), w = F32(F, 0xc);
                SF32(r, 4, rx_subs(-0.0f, x));
                SF32(r, 8, w);
                SF32(r, 0, rx_subs(-0.0f, y));
                SF32(r, 0xc, z);
                SF32(r, 0x10, rx_muls(F32(r, 0x10), neg));
                RX_FRAME_LEAVE(F, 0x30);
            } else {
                SF32(r, 0, rx_muls(F32(r, 0), neg));
                SF32(r, 8, rx_muls(F32(r, 8), neg));
            }
            continue;
        }
        rx_addr ra = pose + 0x30u * (uint32_t)a, rb = pose + 0x30u * (uint32_t)b;
        if (i == 5) {
            uint32_t q[4];
            for (int j = 0; j < 4; j++) q[j] = U32(ra, 4 * j);
            anim_x87_copy_f32(ra, rb + 0xc);
            anim_x87_copy_f32(ra + 4, rb + 8);
            float x = F32(ra, 0);
            anim_x87_copy_f32(ra + 8, rb + 4);
            SF32(ra, 0, rx_muls(x, neg));
            anim_x87_copy_f32(ra + 0xc, rb);
            SF32(ra, 0xc, rx_muls(F32(ra, 0xc), neg));
            SU32(rb, 4, q[2]);
            SU32(rb, 8, q[1]);
            SF32(rb, 0, rx_muls(rx_bf(q[3]), neg));
            SF32(rb, 0xc, rx_muls(rx_bf(q[0]), neg));
            continue;
        }
        for (int j = 0; j < 4; j++) { uint32_t t = U32(ra, 4 * j); SU32(ra, 4 * j, U32(rb, 4 * j)); SU32(rb, 4 * j, t); }
        int k = (i > 0x13 && i <= 0x16) ? 8 : 4;           /* the second negated component: z or y */
        SF32(ra, 0, rx_muls(F32(ra, 0), neg)); SF32(ra, k, rx_muls(F32(ra, k), neg));
        SF32(rb, 0, rx_muls(F32(rb, 0), neg)); SF32(rb, k, rx_muls(F32(rb, k), neg));
    }
}
RX_FUNCTION(0x0083c2f0, rx_pose_mirror)

/* 0083bc70 MotionAnimator::vf4: one motion (+0x28 handle: +0x44 motion data, +0x4c in use) sampled at phase +0x38 * duration
 * into the reset pose through the bone map +0x30 (u8 per motion bone): compressed motions by 00840040, keyframe motions here
 * (rotation tracks valid even when not animated; translations valid, the root bone +0x34 zeroed when a root delta is wanted, which
 * is the 0xa95d50 vector); mirrored when +0x40 (Pose_Mirror). Keyframe motions are an error case: the original formats a
 * message about the missing compression metadata (to its stack, never shown). */
void rx_motion_animator_eval(rx_addr self, rx_addr ch, rx_addr delta_out, rx_addr pose) {
    ANIM_FRAME_ENTER(A, 0x150);
    if (delta_out) { SF32(delta_out, 0, F32(0, 0xa95d50)); SF32(delta_out, 4, F32(0, 0xa95d54)); SF32(delta_out, 8, F32(0, 0xa95d58)); }
    rx_addr h = U32(self, 0x28), m = U32(h, 0x44);
    SU32J(h, 0x4c, 1);   /* the motion handle is shared by the riders' parallel passes (SU32J: loop_jobs.h) */
    float time = F32(self, 0x38) * F32(m, 0x4c);
    anim_pose_reset(ch, pose);
    if (U32(m, 0x64)) {
        motion_sample_compressed(pose, U32(m, 0x64), time, U32(self, 0x30));
        SU32J(U32(self, 0x28), 0x4c, 0);
    } else {
        rx_addr name = rx_call_original(U32(U32(U32(self, 0x28), 0), 8), RX_THISCALL, U32(self, 0x28), 0, 0);
        /* sprintf(buffer, "Error, animation %s does not have ...") (IAT 0x916204): the arguments where the original pushes them */
        { const rx_addr sp = rx_sp(); SU32(sp - 12, 0, A - 0x108); SU32(sp - 8, 0, 0x00975c80u); SU32(sp - 4, 0, name);
          rx_set_sp(sp - 12); crt_sprintf(A - 0x108, 0x00975c80u, sp - 4); rx_set_sp(sp); }   /* arguments live during the call */
        const rx_addr frac = A - 0x128, next = A - 0x120, prev = A - 0x11c;
        for (int32_t i = 0; i < I32(m, 0x20); i++) {
            rx_addr track = U32(m, 0x24) + 0x18u * (uint32_t)i;
            uint32_t bone = U8(U32(track, 8), U32(self, 0x30));
            if (bone == 0xff) continue;
            rx_addr rec = pose + 0x30u * bone;
            anim_sample_rotation_track(track, time, rec);
            SU8(rec, 0x20, 1);
        }
        for (int32_t i = 0; i < I32(m, 0x18); i++) {
            rx_addr track = U32(m, 0x1c) + 0x14u * (uint32_t)i;
            uint32_t bone = U8(U32(track, 4), U32(self, 0x30));
            if (bone == 0xff) continue;
            rx_addr rec = pose + 0x30u * bone;
            if (U32(track, 8) != 0 && I32(track, 0xc) > 1) {
                uint32_t inside = anim_keytrack_find_segment(U32(track, 0), U32(track, 0xc), time, prev, next, frac);
                rx_addr p = U32(prev, 0), n = U32(next, 0), base = inside ? p : n;
                float t = F32(frac, 0);
                SF32(rec, 0x10, (F32(n, 4) - F32(p, 4)) * t + F32(base, 4));
                SF32(rec, 0x14, (F32(n, 8) - F32(p, 8)) * t + F32(base, 8));
                SF32(rec, 0x18, (F32(n, 0xc) - F32(p, 0xc)) * t + F32(base, 0xc));
            } else {
                rx_addr key = U32(track, 0) + 16u * (uint32_t)anim_key16_upper(U32(track, 0), I32(track, 0xc), time);
                anim_x87_copy_f32(rec + 0x10, key - 0xc);
                anim_x87_copy_f32(rec + 0x14, key - 8);
                anim_x87_copy_f32(rec + 0x18, key - 4);
            }
            SU8(rec, 0x21, 1);
            if (delta_out != 0 && U32(self, 0x34) == bone) {
                SF32(rec, 0x10, F32(0, 0xa95d50)); SF32(rec, 0x14, F32(0, 0xa95d54)); SF32(rec, 0x18, F32(0, 0xa95d58));
            }
        }
        SU32J(U32(self, 0x28), 0x4c, 0);
    }
    if (U32(self, 0x40) != 0) rx_pose_mirror(pose);
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x0083bc70, rx_motion_animator_eval)

/* ---- the switch transitions' hash map (stdext::hash_multimap<uint, ...>, 0x84xxxx): +8 the container proxy, +0x18 / +0x1c the
 * node list (head node +0x1c), +0x30 / +0x34 the buckets (8 bytes: first / last node), +0x3c the bucket mask, +0x40 the bucket
 * count; a node: +0 next, +4 previous, +8 the key, +0xc the value. Checked iterators report through _invalid_parameter_noinfo
 * (which does not return in practice; the code continues as the original does). ---- */

/* 00842630 (ESI = map, EAX = key*; the bucket index in EAX; clobbers ECX / EDX): the key's bucket: h = (key ^ 0xdeadbeef) &
 * 0x7fffffff run through the minimal-standard generator step 16807 * (h % 127773) - 2836 * (h / 127773) (ldiv, msvcr90), + 0x7fffffff
 * when negative; then h & mask, or past the bucket count: + (-1 - mask / 2). Frame: push ecx; push 0x1f31d; push the dividend;
 * call [ldiv] (return address, its push ebp). rx_sp() = the entry ESP. */
uint32_t rx_hashmap_bucket(rx_addr map, rx_addr key) {
    const rx_addr sp = rx_sp();
    const uint32_t k = (U32(key, 0) ^ 0xdeadbeefu) & 0x7fffffffu;
    SU32(sp, -4u, anim_reg_get(ANIM_ECX)); SU32(sp, -8u, 0x0001f31du); SU32(sp, -0xcu, k);
    SU32(sp, -0x10u, 0x0084264bu); SU32(sp, -0x14u, anim_reg_get(ANIM_EBP));
    const crt_ldiv_t q = crt_ldiv((int32_t)k, 0x1f31d);
    uint32_t h = (uint32_t)q.rem * 0x41a7u - (uint32_t)q.quot * 0xb14u;
    if ((int32_t)h < 0) h += 0x7fffffffu;
    const uint32_t mask = U32(map, 0x3c);
    uint32_t b = mask & h;
    if (!(U32(map, 0x40) > b)) b += 0xffffffffu - (mask >> 1);
    return b;
}
RX_FUNCTION(0x00842630, rx_hashmap_bucket)

/* 008424e0 (EAX = map; stdcall iterator out (proxy, node), key*; RET 8; EAX = out): find the first node with key <= the wanted
 * one in the key's bucket (walking its nodes while their key is below it): the iterator of it when its key is not above the
 * wanted one, else end (proxy [map + 8], head node [map + 0x1c]); end too when the bucket is empty. Frame: sub esp, 0x1c; push
 * ebx, ebp, esi, edi: E = ESP after them: +0x14 &proxy slot, +0x18 the next bucket, +0x1c the end node, +0x20 / +0x24 the proxy
 * and the node. */
rx_addr rx_hashmap_find(rx_addr map, rx_addr out, rx_addr key) {
    const rx_addr sp0 = rx_sp();
    const uint32_t s_ebx = anim_reg_get(ANIM_EBX), s_ebp = anim_reg_get(ANIM_EBP), s_esi = anim_reg_get(ANIM_ESI), s_edi = anim_reg_get(ANIM_EDI);
    const rx_addr s1 = sp0 - 0x1c;
    SU32(s1, -4u, s_ebx); SU32(s1, -8u, s_ebp); SU32(s1, -0xcu, s_esi); SU32(s1, -0x10u, s_edi);
    const rx_addr E = s1 - 0x10;
    anim_reg_set(ANIM_ESI, map);
    rx_set_sp(E - 4u); SU32(E, -4u, 0x008424f2u);
    uint32_t edi = rx_hashmap_bucket(map, key);
    rx_set_sp(E);
#define BUCKETS ((uint32_t)((int32_t)(U32(map, 0x34) - U32(map, 0x30)) >> 3))
#define NODE_CHECK() do { if (ebp == 0) { anim_invalid_parameter(); eax = 0; } else eax = U32(ebp, 0); \
                          if (ebx == U32(eax, 0x14)) anim_invalid_parameter(); } while (0)
    uint32_t eax;
    if (!(edi < BUCKETS)) anim_invalid_parameter();
    uint32_t ebx = U32(U32(map, 0x30) + edi * 8u, 4);
    const rx_addr px = map + 8;
    SU32(E, 0x14, px);
    if (px == 0) anim_invalid_parameter();
    const uint32_t ebp = U32(px, 0);
    edi++;
    SU32(E, 0x20, ebp); SU32(E, 0x24, ebx); SU32(E, 0x18, edi);
    for (;;) {
        if (!(edi < BUCKETS)) anim_invalid_parameter();
        if (U32(U32(map, 0x30) + edi * 8u, 4) == ebx) {      /* an empty bucket: end */
            const rx_addr p = U32(E, 0x14);
            const uint32_t end = U32(p, 0x14), proxy = U32(p, 0);
            SU32(E, 0x24, ebx);
            SU32(out, 4, end); SU32(out, 0, proxy);
            break;
        }
        NODE_CHECK();
        edi = key;
        if (!(U32(ebx, 8) < U32(edi, 0))) {
            SU32(E, 0x24, ebx);
            NODE_CHECK();
            if (U32(edi, 0) >= U32(ebx, 8)) { SU32(out, 4, U32(E, 0x24)); SU32(out, 0, U32(E, 0x20)); }
            else {
                const rx_addr p = U32(E, 0x14);
                SU32(E, 0x1c, U32(p, 0x14)); SU32(E, 0x18, U32(p, 0));
                SU32(out, 4, U32(E, 0x1c)); SU32(out, 0, U32(E, 0x18));
            }
            break;
        }
        NODE_CHECK();
        ebx = U32(ebx, 0);
        edi = U32(E, 0x18);
    }
#undef NODE_CHECK
#undef BUCKETS
    anim_reg_set(ANIM_EBX, s_ebx); anim_reg_set(ANIM_EBP, s_ebp); anim_reg_set(ANIM_ESI, s_esi); anim_reg_set(ANIM_EDI, s_edi);
    rx_set_sp(sp0);
    return out;
}
RX_FUNCTION(0x008424e0, rx_hashmap_find)

/* the find at its call site 0x0083ca47: the arguments and the return address as the original's pushes leave them; ECX and EDI hold
 * the target child index there (movzx from +0xd2) */
static void switch_transitions_find(rx_addr map, rx_addr iter_out, rx_addr key, uint32_t target) {
    const rx_addr sp0 = rx_sp();
    const uint32_t edi = anim_reg_get(ANIM_EDI);
    anim_reg_set(ANIM_EDI, target); anim_reg_set(ANIM_ECX, target);
    SU32(sp0, -4u, key); SU32(sp0, -8u, iter_out); SU32(sp0, -0xcu, 0x0083ca4cu);
    rx_set_sp(sp0 - 0xcu);
    rx_hashmap_find(map, iter_out, key);
    rx_set_sp(sp0);
    anim_reg_set(ANIM_EDI, edi);
}

/* The switch animator's children: a checked std::vector<Animator*> at +0xc4 (begin) / +0xc8 (end). */
static rx_addr switch_child(rx_addr self, uint32_t i) {
    if (!(i < (uint32_t)((int32_t)(U32(self, 0xc8) - U32(self, 0xc4)) >> 2))) anim_invalid_parameter();
    return U32(U32(self, 0xc4), 4 * i);
}

/* 0083c9c0 SwitchAnimator_StartTransition (ESI = switch animator): the blend's two children (+0x58 / +0x5c) = the current
 * (+0xd0) and target (+0xd2) children, the blend duration +0xd8 = the transition table's (+0x70, key current << 16 | target)
 * entry +0x0c, or 1 when there is none; clears the pending flag +0xd4. */
void rx_switch_animator_start_transition(rx_addr self) {
    ANIM_FRAME8_ENTER(A, 0x18);
    const rx_addr key = A - 0xc, iter = A - 8;          /* iterator: (container, node) */
    SU32(self, 0x58, switch_child(self, U16(self, 0xd0)));
    SU32(self, 0x5c, switch_child(self, U16(self, 0xd2)));
    SU32(key, 0, ((uint32_t)U16(self, 0xd0) << 16) | U16(self, 0xd2));
    switch_transitions_find(self + 0x70, iter, key, U16(self, 0xd2));
    rx_addr cont = U32(iter, 0), end = U32(self, 0x8c);
    if (cont == 0 || cont != U32(self, 0x78)) anim_invalid_parameter();
    if (U32(iter, 4) == end) {
        SF32(self, 0xd8, 1.0f);
        SU8(self, 0xd4, 0);
    } else {
        rx_addr c = cont;
        if (c == 0) { anim_invalid_parameter(); c = 0; } else c = U32(c, 0);
        if (U32(iter, 4) == U32(c, 0x14)) anim_invalid_parameter();
        SU8(self, 0xd4, 0);
        anim_x87_copy_f32(self + 0xd8, U32(iter, 4) + 0xc);
    }
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x0083c9c0, rx_switch_animator_start_transition)

/* 0083cad0 SwitchAnimator::vf4: settled (current +0xd0 == target +0xd2): the current child alone. Switching: (re)start the
 * transition when pending, finish it when the blend weight reached 1 (current = target), and evaluate as the BlendAnimator. */
void rx_switch_animator_eval(rx_addr self, rx_addr ch, rx_addr delta_out, rx_addr pose) {
    ANIM_FRAME8_ENTER(A, 8);                                  /* push ebp; and esp, -8; push esi, edi */
    if (U16(self, 0xd0) == U16(self, 0xd2)) {
        anim_animator_eval(switch_child(self, U16(self, 0xd0)), ch, delta_out, pose, 0x0083cb22);
        ANIM_FRAME_LEAVE(A);
        return;
    }
    if (U8(self, 0xd4) != 0) { ANIM_CALL_FRAME(0); rx_switch_animator_start_transition(self); ANIM_CALL_FRAME_END(0); }
    if (F32(self, 0x68) >= 1.0f) SU16(self, 0xd0, U16(self, 0xd2));
    blend_eval_from(self, ch, delta_out, pose, 0x0083cb67);
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x0083cad0, rx_switch_animator_eval)

/* 0083b3e0 MotionAnimator::vf11 GetFloatParam (thiscall: name, float** out; RET 8): "AnimationPercent" (0x9583d0) -> the phase
 * +0x38, "TimeScale" (0x958550) -> +0x3c; returns EAX with al = found (upper bytes: the out pointer's for the first name, else the inlined strcmp's). */
uint32_t rx_motion_animator_get_param(rx_addr self, rx_addr name, rx_addr out) {
    if (anim_strcmp(name, 0x009583d0) == 0) { SU32(out, 0, self + 0x38); return (out & 0xffffff00u) | 1u; }
    int32_t r = anim_strcmp(name, 0x00958550);
    if (r == 0) { SU32(out, 0, self + 0x3c); return 1; }
    return (uint32_t)r & 0xffffff00u;
}
RX_FUNCTION(0x0083b3e0, rx_motion_animator_get_param)

/* 00834570 BlendAnimator::vf11 GetFloatParam (thiscall: name, float** out; RET 8): "BlendPercent" (0x9583c0) -> the weight +0x68.
 * EAX: the out pointer's (al = 1) when found, else the inlined strcmp's -1 / 1 with al cleared. */
uint32_t rx_blend_animator_get_param(rx_addr self, rx_addr name, rx_addr out) {
    int32_t r = anim_strcmp(name, 0x009583c0);
    if (r == 0) { SU32(out, 0, self + 0x68); return (out & 0xffffff00u) | 1u; }
    return (uint32_t)r & 0xffffff00u;
}
RX_FUNCTION(0x00834570, rx_blend_animator_get_param)

/* 0083ce40 SwitchAnimator::vf11 GetParam (thiscall: name, out; RET 8): "CurrentAnimator" (0x9583a4) -> +0xd0, "NextAnimator"
 * (0x958394) -> +0xd2, "Transition" (0x9583b4) -> +0xd4, "TransitionParam" (0x975cdc) -> +0xd8 (the address stored in *out, al = 1);
 * otherwise BlendAnimator::vf11 (its EAX returned). */
uint32_t rx_switch_animator_get_param(rx_addr self, rx_addr name, rx_addr out) {
    /* EAX at the return: "CurrentAnimator" / "Transition" leave EAX = the out pointer (mov eax, [esp + 0x10]) with al = 1; "NextAnimator" /
     * "TransitionParam" leave the strcmp's 0 there (edx carries the pointer) so EAX = 1 */
    static const struct { uint32_t name, off; int eax_out; } p[4] = {{0x009583a4, 0xd0, 1}, {0x00958394, 0xd2, 0}, {0x009583b4, 0xd4, 1}, {0x00975cdc, 0xd8, 0}};
    for (int i = 0; i < 4; i++)
        if (anim_strcmp(name, p[i].name) == 0) { SU32(out, 0, self + p[i].off); return p[i].eax_out ? ((out & 0xffffff00u) | 1u) : 1u; }
    return rx_blend_animator_get_param(self, name, out);
}
RX_FUNCTION(0x0083ce40, rx_switch_animator_get_param)

/* 0083d430 PerformanceAnimator::vf11 GetParam (thiscall: name, out; RET 8): "PerformanceTime" (0x958404) -> the timeline time +0x98
 * (address in *out, al = 1); else al = 0 with the inlined strcmp's -1 / 1 above it. */
uint32_t rx_performance_animator_get_param(rx_addr self, rx_addr name, rx_addr out) {
    int32_t r = anim_strcmp(name, 0x00958404);
    if (r == 0) { SU32(out, 0, self + 0x98); return (out & 0xffffff00u) | 1u; }
    return (uint32_t)r & 0xffffff00u;
}
RX_FUNCTION(0x0083d430, rx_performance_animator_get_param)

/* 008d8db0 Animator::vf11 (the base: no parameters; thiscall, RET 8): al = 0 (the other bytes of EAX are the caller's). */
uint32_t rx_animator_get_param(rx_addr self, rx_addr name, rx_addr out) { (void)self; (void)name; (void)out; return 0; }
RX_FUNCTION(0x008d8db0, rx_animator_get_param)

/* 0083e930 PerformanceAnimator additive clip (EAX = the clip's animator; stack character, pose, weight; RET 0xc): the clip is
 * evaluated into a pool pose t (vf4 with a null root delta), and per bone the pose's rotation p is blended toward p * t by the
 * weight: r = the quaternion product (double precision, rounded to float per component), d = dot(p, r), p' = s0 p + s1 r with the
 * unsigned slerp weights of (d, weight) (anim_math.h); the rotation is marked valid; the translation gets t * weight added
 * (float, t * w + p) and is marked valid. The pool pose is handed back. (The layered clips 0083eff0 are never reached: no
 * performance asset has the layered flag.) Frame: push ebp; and esp, -16; sub esp, 0x54; push ebx, esi, edi. */
void rx_performance_additive_clip(rx_addr clip, rx_addr ch, rx_addr pose, float weight) {
    ANIM_FRAME_ENTER(A, 0x60);
    const rx_addr t_pose = anim_pose_acquire(ch);
    anim_animator_eval(clip, ch, 0, t_pose, 0x0083e959);
    for (uint32_t i = 0, n = U32(ch, 0x234); i < n; i++) {
        const rx_addr p = pose + 0x30u * i, t = t_pose + 0x30u * i;
        const float px = F32(p, 0), py = F32(p, 4), pz = F32(p, 8), pw = F32(p, 0xc);
        const float tx = F32(t, 0), ty = F32(t, 4), tz = F32(t, 8), tw = F32(t, 0xc);
        /* r = p * t, each component in double precision then rounded */
        const float rw = (float)((((double)tw * (double)pw - (double)px * (double)tx) - (double)ty * (double)py) - (double)tz * (double)pz);
        const float rx = (float)((((double)px * (double)tw + (double)pw * (double)tx) + (double)tz * (double)py) - (double)ty * (double)pz);
        const float ry = (float)((((double)py * (double)tw + (double)ty * (double)pw) + (double)pz * (double)tx) - (double)tz * (double)px);
        const float rz = (float)((((double)pz * (double)tw + (double)tz * (double)pw) + (double)ty * (double)px) - (double)py * (double)tx);
        const float d = (((px * rx + py * ry) + pz * rz) + pw * rw);
        float s0, s1;
        anim_slerp_weights_unsigned(d, weight, &s0, &s1);
        SF32(p, 0xc, pw * s0 + s1 * rw);
        SF32(p, 0x0, px * s0 + s1 * rx);
        SF32(p, 0x4, py * s0 + s1 * ry);
        SF32(p, 0x8, pz * s0 + s1 * rz);
        SU8(p, 0x20, 1);
        const float tpx = F32(t, 0x10) * weight, tpy = F32(t, 0x14) * weight, tpz = F32(t, 0x18) * weight;
        SF32(p, 0x10, F32(p, 0x10) + tpx);
        SF32(p, 0x14, F32(p, 0x14) + tpy);
        SF32(p, 0x18, tpz + F32(p, 0x18));
        SU8(p, 0x21, 1);
    }
    pose_release(ch, t_pose);
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x0083e930, rx_performance_additive_clip)

/* The clip functions called from PerformanceAnimator::vf4's call sites: the weight through the x87 (fld / fstp m32: a signalling NaN
 * comes out quiet), then (character, pose, weight) pushed and the return address `ret` (RET 0xc pops the three). 0083e930 is
 * ported; the layered 0083eff0 stays the original (never reached). */
static void performance_apply_clip(rx_addr fn, rx_addr anim, rx_addr ch, rx_addr pose, float w) {
    if (fn == 0x0083e930) {
        ANIM_PUSH(((rx_fb(w) & 0x7fffffffu) > 0x7f800000u ? (rx_fb(w) | 0x400000u) : rx_fb(w))); ANIM_PUSH(pose); ANIM_PUSH(ch); ANIM_PUSH(0x0083e852);
        rx_performance_additive_clip(anim, ch, pose, rx_bf(U32(rx_sp(), 12)));
        rx_set_sp(rx_sp() + 0x10);
        return;
    }
    rx_regs r = anim_regs(); r.r[ANIM_EAX] = anim; uint32_t a[3] = {ch, pose, rx_fb(w)};
    rx_call_original_ex(fn, &r, 0, 3, a);
}

/* The clip's weight at fraction f: weight +0x58, ramped in over the fade-in fraction +0x5c and out after the fade-out +0x60. */
static float performance_clip_weight(rx_addr clip, float f) {
    float w = F32(clip, 0x58), fin = F32(clip, 0x5c), fout = F32(clip, 0x60);
    if (fin > f) w = f / fin * w;
    if (f > fout) w = (1.0f - (f - fout) / (1.0f - fout)) * w;
    return w;
}

/* 0083e290 PerformanceAnimator::vf4: a timeline of clips (+0x70 performance: +0x18 clips of 0x70 bytes: +0x40 start, +0x4c blend
 * in, +0x50 blend out, +0x54 end, +0x58 weight, +0x5c / +0x60 fade in / out fractions, +0x64 additive, +0x68 layered), one child
 * animator per clip (+0x74), clip count +0x8c, timeline time +0x98, cross-fade from the previous performance +0x7c (ramps by 0.25
 * per call to 1) from the animator at *(+0x84) with its phase +0x80. The base clip active at the time is evaluated (its
 * "AnimationPercent" set to the clip fraction), or, between clips, the last one ended and / or the next one blended by the
 * BlendAnimator part (+0x58 / +0x5c / weight +0x68); then up to 4 additive and 4 layered active clips are applied with their
 * weights (0083e930 / 0083eff0). */
void rx_performance_animator_eval(rx_addr self, rx_addr ch, rx_addr delta_out, rx_addr pose) {
    int32_t n = I32(self, 0x8c);
    if (n == 0) return;
    ANIM_FRAME8_ENTER(A, 0x68);
    const rx_addr body = A - 0x68;
    const rx_addr out10 = body + 0x10, out14 = body + 0x14, out18 = body + 0x18, out20 = body + 0x20, out24 = body + 0x24;
    const rx_addr name = 0x009583d0;     /* "AnimationPercent" */
    if (!(F32(self, 0x7c) == 1.0f)) {
        float f = F32(self, 0x7c) + 0.25f /* 0x92ff84 */;
        if (!(1.0f > f)) f = 1.0f;
        SF32(self, 0x7c, f);
    }
    SU32(out10, 0, 0);
    int32_t main = -1, before = -1, after = -1, n_add = 0, n_layer = 0;
    int32_t add_idx[4] = {0}, layer_idx[4] = {0};
    float add_frac[4] = {0}, layer_frac[4] = {0}, main_frac = 0.0f;
    float t = F32(self, 0x98);
    for (int32_t i = 0; i < n; i++) {
        rx_addr c = U32(U32(self, 0x70), 0x18) + 0x70u * (uint32_t)i;
        if (!(t >= F32(c, 0x4c) + F32(c, 0x40))) continue;
        if (!(F32(c, 0x54) - F32(c, 0x50) >= t)) continue;
        float frac = (t - F32(c, 0x40)) / (F32(c, 0x54) - F32(c, 0x40));
        if (U32(c, 0x64) != 0 && n_add < 4) { add_idx[n_add] = i; add_frac[n_add] = frac; n_add++; continue; }
        if (U32(c, 0x68) != 0 && n_layer < 4) { layer_idx[n_layer] = i; layer_frac[n_layer] = frac; n_layer++; continue; }
        if (U32(c, 0x64) == 0 && U32(c, 0x68) == 0) { main = i; main_frac = frac; }
    }
    rx_addr anims = U32(self, 0x74);
    if (main != -1) {
        rx_addr a = U32(anims, 4u * (uint32_t)main);
        if (F32(self, 0x7c) == 1.0f) {
            anim_animator_get_param(a, name, out10, 0x0083e767);
            SF32(U32(out10, 0), 0, main_frac);
            anim_animator_eval(U32(anims, 4u * (uint32_t)main), ch, delta_out, pose, 0x0083e793);
        } else {
            SU32(out18, 0, 0);
            anim_animator_get_param(a, name, out18, 0x0083e6d5);
            SF32(U32(out18, 0), 0, main_frac);
            SU32(out14, 0, 0);
            anim_animator_get_param(U32(U32(self, 0x84), 0), name, out14, 0x0083e704);
            anim_x87_copy_f32(U32(out14, 0), self + 0x80);
            SU32(self, 0x58, U32(U32(self, 0x74), 4u * (uint32_t)main));
            SU32(self, 0x5c, U32(U32(self, 0x84), 0));
            SF32(self, 0x68, 1.0f - F32(self, 0x7c));
            blend_eval_from(self, ch, delta_out, pose, 0x0083e749);
        }
    } else {
        /* between base clips: the latest one ended (before) and the next one to start (after) */
        float best_end = -3.4028235e+38f /* 0x977550 */, best_start = 3.4028235e+38f /* 0x977494 */;
        rx_addr clips = U32(U32(self, 0x70), 0x18);
        for (int32_t i = 0; i < I32(self, 0x8c); i++) {
            rx_addr c = clips + 0x70u * (uint32_t)i;
            if (U32(c, 0x64) != 0 || U32(c, 0x68) != 0) continue;
            float end = F32(c, 0x54) - F32(c, 0x50), start = F32(c, 0x40) + F32(c, 0x4c);
            float tt = F32(self, 0x98);
            if (tt >= end && end >= best_end) { best_end = end; before = i; }
            if (start >= tt && best_start >= start) { best_start = start; after = i; }
        }
        if (before == -1) {
            if (after == -1) { ANIM_FRAME_LEAVE(A); return; }
            rx_addr c = clips + 0x70u * (uint32_t)after;
            anim_animator_get_param(U32(U32(self, 0x74), 4u * (uint32_t)after), name, out10, 0x0083e4bd);
            SF32(U32(out10, 0), 0, F32(c, 0x4c) / (F32(c, 0x54) - F32(c, 0x40)));
            anim_animator_eval(U32(U32(self, 0x74), 4u * (uint32_t)after), ch, delta_out, pose, 0x0083e793);
        } else if (after == -1) {
            rx_addr c = clips + 0x70u * (uint32_t)before;
            anim_animator_get_param(U32(U32(self, 0x74), 4u * (uint32_t)before), name, out10, 0x0083e526);
            float len = F32(c, 0x54) - F32(c, 0x40);
            SF32(U32(out10, 0), 0, (len - F32(c, 0x50)) / len);
            if (!(F32(self, 0x7c) == 1.0f)) anim_x87_copy_f32(U32(out10, 0), self + 0x80);
            anim_animator_eval(U32(U32(self, 0x74), 4u * (uint32_t)before), ch, delta_out, pose, 0x0083e793);
        } else {
            rx_addr cb = clips + 0x70u * (uint32_t)before, ca = clips + 0x70u * (uint32_t)after;
            SU32(self, 0x58, U32(U32(self, 0x74), 4u * (uint32_t)before));
            SU32(self, 0x5c, U32(U32(self, 0x74), 4u * (uint32_t)after));
            SU32(out18, 0, 0);
            anim_animator_get_param(U32(self, 0x58), name, out18, 0x0083e5c2);
            SF32(U32(out18, 0), 0, ((F32(cb, 0x54) - F32(cb, 0x50)) - F32(cb, 0x40)) / (F32(cb, 0x54) - F32(cb, 0x40)));
            if (!(F32(self, 0x7c) == 1.0f)) anim_x87_copy_f32(U32(out18, 0), self + 0x80);
            SU32(out24, 0, 0);
            anim_animator_get_param(U32(self, 0x5c), name, out24, 0x0083e622);
            SF32(U32(out24, 0), 0, F32(ca, 0x4c) / (F32(ca, 0x54) - F32(ca, 0x40)));
            float end_b = F32(cb, 0x54) - F32(cb, 0x50);
            SF32(self, 0x68, (F32(self, 0x98) - end_b) / ((F32(ca, 0x4c) + F32(ca, 0x40)) - end_b));
            if (!(F32(self, 0x7c) == 1.0f)) SU32(self, 0x58, U32(U32(self, 0x84), 0));
            blend_eval_from(self, ch, delta_out, pose, 0x0083e69a);
        }
    }
    for (int32_t k = 0; k < n_add; k++) {
        rx_addr slot = U32(self, 0x74) + 4u * (uint32_t)add_idx[k];
        SU32(out18, 0, 0);
        anim_animator_get_param(U32(slot, 0), name, out18, 0x0083e7c8);
        SF32(U32(out18, 0), 0, add_frac[k]);
        rx_addr c = U32(U32(self, 0x70), 0x18) + 0x70u * (uint32_t)add_idx[k];
        performance_apply_clip(0x0083e930, U32(slot, 0), ch, pose, performance_clip_weight(c, add_frac[k]));
    }
    for (int32_t k = 0; k < n_layer; k++) {
        rx_addr slot = U32(self, 0x74) + 4u * (uint32_t)layer_idx[k];
        SU32(out20, 0, 0);
        anim_animator_get_param(U32(slot, 0), name, out20, 0x0083e890);
        SF32(U32(out20, 0), 0, layer_frac[k]);
        rx_addr c = U32(U32(self, 0x70), 0x18) + 0x70u * (uint32_t)layer_idx[k];
        /* BOUNDARY (data): the layered / override clip; no performance asset has the override flag (scan of all 259 perfdat assets, STOP_AUDIT.md T1), and the original's reference world executes none of its 1448 bytes. */
        performance_apply_clip(0x0083eff0, U32(U32(self, 0x74), 4u * (uint32_t)layer_idx[k]), ch, pose,
                               performance_clip_weight(c, layer_frac[k]));
    }
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x0083e290, rx_performance_animator_eval)
