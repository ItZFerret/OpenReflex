/* phys_anim_sample.c -- the pose samplers of the physics-driven animation (ragdoll / wreck poses): 00840440 samples one motion
 * slot at a time into a pose record array, 00840b70 blends two slots.
 * Motion (slot +0 -> handle, handle +0x44 = motion; handle +0x4c = busy flag set while sampling): +0x4c time scale, rotation
 * tracks +0x20 count / +0x24 records (0x18 bytes: keys*, cached key index +4, bone +8, enabled +0xc, key count +0x10; keys of 12
 * bytes: time, packed quaternion (phys_quat.c)), translation tracks +0x18 / +0x1c (0x14 bytes: keys*, bone +4, linear +8, count
 * +0xc; keys of 16 bytes: time, x, y, z), compressed keys +0x64 (when set: AnimKeys_Locate / AnimKeys_SampleRotation and its
 * translation table). Slot: +4.. bone map (one int per track, compressed motions), +0xcc pose bone count, +0xd0 mirror flag
 * (negates rotation y / z and translation x). Pose record (0x30 bytes per bone): +0 translation set, +4 rotation set, +0x10
 * translation, +0x20 rotation. */
#include "phys_anim_sample.h"
#include "phys_quat.h"
#include "loop_anim.h"
#include "world_rider.h"

#define CALL_AT_SP(cur, nstack, stmt) do { rx_set_sp((cur) - 4u * (nstack) - 4u); stmt; rx_set_sp(cur); } while (0)

static float minus_one(void) { return F32(0, 0x009cd9e8); }   /* writable -1.0f */

/* 0082b930 AnimKeys_Locate (ECX = compressed keys, xmm0 = time; stack locate out*; RET 4) */
static void keys_locate(rx_addr keys, float t, rx_addr locate) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 8u); SU32(rx_sp(), 4, locate);
    rx_animkeys_locate(keys, locate, t);
    rx_set_sp(sp0);
}
/* 0082b9e0 AnimKeys_SampleRotation (ECX = locate*, EDX = track index, ESI = quaternion out*) */
static void keys_sample_rotation(rx_addr locate, uint32_t track, rx_addr q) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4u);
    rx_animkeys_sample_rotation(locate, track, q);
    rx_set_sp(sp0);
}
/* 004dbce0 KeyTrack_FindSegment (EAX = keys, EDX = count, xmm0 = time; cdecl prev*, next*, fraction*): 1 inside the keys */
static uint32_t keytrack_find_segment(rx_addr keys, uint32_t count, float t, rx_addr prev, rx_addr next, rx_addr frac) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 0x10u); SU32(rx_sp(), 4, prev); SU32(rx_sp(), 8, next); SU32(rx_sp(), 0xc, frac);
    const uint32_t r = rx_world_keytrack_find_segment(keys, count, t, prev, next, frac);
    rx_set_sp(sp0);
    return r;
}

/* One keyframed rotation track at time t into q (a stack quaternion; left as it is when the track is disabled or empty).
 * Up to two keys: slerp of key 0 and key 1 (one key: with itself, t = 0). More: the key pair around t from the track's cached
 * index (+4, reset to 1 / to the last key by quarter / three-quarter probes, then stepped up and down), slerped
 * (QuatSlerp_Packed); a time past the last key or before the first gives that key unpacked as it is. */
static void sample_rotation_track(rx_addr track, float t, rx_addr q) {
    if (U32(track, 0xc) == 0 || I32(track, 0x10) < 1) return;
    const int32_t n = I32(track, 0x10);
    const rx_addr keys = U32(track, 0);
    if (n <= 2) {
        if (n == 2) {
            const float t0 = F32(keys, 0);
            phys_quat_slerp_packed(keys + 4, keys + 0xc + 4, q, phys_div(phys_sub(t, t0), phys_sub(F32(keys, 0xc), t0)));
        } else {
            phys_quat_slerp_packed(keys + 4, keys + 4, q, 0.0f);
        }
        return;
    }
    const int32_t last = n - 1, quarter = last >> 2, mid = (last >> 1) + quarter;
    if (I32(track, 4) > mid && F32(keys, 12u * (uint32_t)quarter) > t) SU32(track, 4, 1);
    else if (I32(track, 4) < quarter && t > F32(keys, 12u * (uint32_t)mid)) SU32(track, 4, (uint32_t)last);
    int32_t k = I32(track, 4) < 1 ? 1 : I32(track, 4);
    int inside = 1;
    while (t > F32(keys, 12u * (uint32_t)k)) {
        if (++k >= n) { k = last; inside = 0; break; }
    }
    while (F32(keys, 12u * (uint32_t)(k - 1)) > t) {
        if (--k < 1) { k = 1; inside = 0; break; }
    }
    SU32(track, 4, (uint32_t)k);
    const rx_addr a = keys + 12u * (uint32_t)(k - 1), b = keys + 12u * (uint32_t)k;
    const float frac = phys_div(phys_sub(t, F32(a, 0)), phys_sub(F32(b, 0), F32(a, 0)));
    if (inside) phys_quat_slerp_packed(a + 4, b + 4, q, frac);
    else v4_st(q, phys_quat_unpack(b + 4));
}

/* One keyframed translation track at time t: linear tracks with two or more keys through KeyTrack_FindSegment (lerp from the
 * previous key inside, from the next key outside); others the key at or before t (binary search over the 16-byte keys; time
 * past the last key: the last; before the first: the key before the array, as the original reads it). prev / next / frac are
 * the caller's stack slots for FindSegment. */
static rx_v4 sample_translation_track(rx_addr track, float t, rx_addr prev, rx_addr next, rx_addr frac, rx_addr sp) {
    const rx_addr keys = U32(track, 0);
    const int32_t n = I32(track, 0xc);
    if (U32(track, 8) != 0 && n > 1) {
        rx_set_sp(sp);
        const uint32_t inside = keytrack_find_segment(keys, (uint32_t)n, t, prev, next, frac);
        const rx_addr p = U32(prev, 0), q = U32(next, 0), from = inside ? p : q;
        const float f = F32(frac, 0);
        rx_v4 r;
        for (int i = 0; i < 3; i++)
            r.v[i] = phys_add(phys_mul(phys_sub(F32(q, 4 + 4 * i), F32(p, 4 + 4 * i)), f), F32(from, 4 + 4 * i));
        r.v[3] = 0.0f;
        return r;
    }
    int32_t k;
    if (n == 0) k = 0;
    else if (t >= F32(keys, 16u * (uint32_t)n - 16)) k = n;
    else if (F32(keys, 0) > t) k = 0;
    else {
        int32_t lo = 0, hi = n;
        for (;;) {
            const int32_t m = (lo + hi) >> 1;
            const rx_addr key = keys + 16u * (uint32_t)m;
            if (!(t >= F32(key, 0))) {
                if (t >= F32(key, -0x10)) { k = m; break; }
                hi = m;
            } else {
                if (F32(key, 0x10) > t) { k = m + 1; break; }
                lo = m;
            }
        }
    }
    const rx_addr key = keys + 16u * (uint32_t)k - 16;
    return v4(F32(key, 4), F32(key, 8), F32(key, 0xc), 0.0f);
}

/* the compressed motions' translation table (packed +0x3c, 16-byte entries; +0x1c frames, +0xc frame count, +4 the animated
 * track): the animated track lerps two frames at the located frame / fraction (locate +0 / +4), the others are constant */
static rx_v4 sample_compressed_translation(rx_addr packed, int32_t j, rx_addr locate) {
    const rx_addr table = U32(packed, 0x3c);
    if (j != I32(packed, 4)) {
        const rx_addr e = table + 16u * (uint32_t)(I32(packed, 0x1c) + j);
        return v4(F32(e, 0), F32(e, 4), F32(e, 8), 0.0f);
    }
    const int32_t f = rx_cvtt(phys_mul((float)I32(packed, 0xc), F32(locate, 0)));
    const int32_t g = f + 1 > I32(packed, 0x1c) ? f : f + 1;
    const rx_addr a = table + 16u * (uint32_t)f, b = table + 16u * (uint32_t)g;
    const float w = F32(locate, 4), om = phys_sub(1.0f, w);
    return v4(phys_add(phys_mul(F32(b, 0), w), phys_mul(om, F32(a, 0))),
              phys_add(phys_mul(om, F32(a, 4)), phys_mul(w, F32(b, 4))),
              phys_add(phys_mul(om, F32(a, 8)), phys_mul(F32(b, 8), w)), 0.0f);
}

/* ---- 00840440 PoseSampler_Sample (stdcall: slot*, time, pose out*; RET 0xc): the slot's motion at time * its time scale into
 * the pose records: every record's flags cleared first, then each rotation track with a bone (!= 0xff) sampled into the stack
 * quaternion (+0x40; a disabled track leaves the previous one there) and stored with its flag, then each translation track.
 * Frame: push ebp; and esp, -16; sub esp, 0x54; push ebx / esi / edi (FindSegment slots +0x34 / +0x38 / +0x3c, quaternion
 * +0x40, locate +0x50). Returns the ECX the original leaves (the slot's handle, 0 without translation tracks). */
uint32_t phys_pose_sample(rx_addr slot, float time, rx_addr pose) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x54 - 12;
    rx_set_sp(L);
    const rx_addr frac = L + 0x34, prev = L + 0x38, next = L + 0x3c, q = L + 0x40, locate = L + 0x50;
    const rx_addr handle = U32(slot, 0), motion = U32(handle, 0x44);
    SU32(handle, 0x4c, 1);
    const float t = phys_mul(F32(motion, 0x4c), time);
    if (U32(motion, 0x64) != 0) keys_locate(U32(motion, 0x64), t, locate);
    for (int32_t i = 0; i < I32(slot, 0xcc); i++) { SU32(pose + 0x30u * (uint32_t)i, 4, 0); SU32(pose + 0x30u * (uint32_t)i, 0, 0); }
    const int mirror_flag = U32(slot, 0xd0) != 0;
    for (int32_t i = 0; i < I32(motion, 0x20); i++) {
        const rx_addr packed = U32(motion, 0x64), track = U32(motion, 0x24) + 0x18u * (uint32_t)i;
        const uint32_t bone = packed ? U32(slot, 4 + 4u * (uint32_t)i) : U32(track, 8);
        if (bone == 0xff) continue;
        if (packed) keys_sample_rotation(locate, (uint32_t)i, q);
        else sample_rotation_track(track, t, q);
        if (mirror_flag) {
            SF32(q, 4, phys_mul(F32(q, 4), minus_one()));
            SF32(q, 8, phys_mul(F32(q, 8), minus_one()));
        }
        const rx_addr rec = pose + 0x30u * bone;
        v4_st(rec + 0x20, v4_ld(q));
        SU32(rec, 4, 1);
    }
    for (int32_t j = 0; j < I32(motion, 0x18); j++) {
        const rx_addr packed = U32(motion, 0x64), track = U32(motion, 0x1c) + 0x14u * (uint32_t)j;
        const uint32_t bone = packed ? U32(slot, 4 + 4u * (uint32_t)j) : U32(track, 4);
        if (bone == 0xff) continue;
        rx_v4 p = packed ? sample_compressed_translation(packed, j, locate)
                         : sample_translation_track(track, t, prev, next, frac, L);
        if (U32(slot, 0xd0) != 0) p.v[0] = phys_mul(p.v[0], minus_one());
        const rx_addr rec = pose + 0x30u * bone;
        SF32(rec, 0x10, p.v[0]); SF32(rec, 0x14, p.v[1]); SF32(rec, 0x18, p.v[2]);
        SU32(rec, 0, 1);
    }
    SU32(U32(slot, 0), 0x4c, 0);
    rx_set_sp(sp0);
    return I32(motion, 0x18) > 0 ? U32(slot, 0) : 0;
}
RX_FUNCTION(0x00840440, phys_pose_sample)

/* 0082bac0 AnimKeys_SampleTranslation (ECX = track index, EDX = out*, EDI = locate*; stack compressed keys; RET 4) */
static void keys_sample_translation(rx_addr packed, uint32_t track, rx_addr out, rx_addr locate) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 8u); SU32(rx_sp(), 4, packed);
    rx_animkeys_sample_translation(packed, track, out, locate);
    rx_set_sp(sp0);
}

/* ---- 00840b70 PoseSampler_Blend (stdcall: slot A*, slot B*, time A, time B, w, pose out*; RET 0x18): w below 0.05 (0x9d3bfc) or
 * above 0.95 (0x9cd9ac): one slot alone (00840440); else the records cleared, both motions sampled at their scaled times (A's
 * tracks and bones; B sampled with A's track index), rotations slerped (A -> B by w: phys_quat_slerp_weights, into B's stack
 * quaternion) and translations lerped (A - (A - B) w, kept in A's stack vector). No mirror. Frame: push ebp; and esp, -16; sub
 * esp, 0x94; push ebx / esi / edi (B quaternion / A translation +0x10, A quaternion / B translation +0x30, FindSegment slots
 * +0x0c / +0x4c / +0x50 / +0x54 / +0x58 / +0x5c, motions +0x60 / +0x70, times +0x28 / +0x24, locates +0x80 / +0x90).
 * Returns the ECX the original leaves (slot B; the single-slot sampler's). */
uint32_t phys_pose_sample_blend(rx_addr slot_a, rx_addr slot_b, float time_a, float time_b, float w, rx_addr pose) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x94 - 12;
    uint32_t ecx;
    rx_set_sp(L);
    if (0.05f /* 0x9d3bfc */ > w) { CALL_AT_SP(L, 3, ecx = phys_pose_sample(slot_a, time_a, pose)); goto out; }
    if (w > 0.95f /* 0x9cd9ac */) { CALL_AT_SP(L, 3, ecx = phys_pose_sample(slot_b, time_b, pose)); goto out; }
    const rx_addr qb = L + 0x10, qa = L + 0x30, locate_a = L + 0x80, locate_b = L + 0x90;
    for (int32_t i = 0; i < I32(slot_a, 0xcc); i++) { SU32(pose + 0x30u * (uint32_t)i, 4, 0); SU32(pose + 0x30u * (uint32_t)i, 0, 0); }
    if (w > 0.0f) {
        const rx_addr ha = U32(slot_a, 0), hb = U32(slot_b, 0);
        SU32(L, 0x60, U32(ha, 0x44)); SU32(ha, 0x4c, 1);
        SU32(hb, 0x4c, 1); SU32(L, 0x70, U32(hb, 0x44));
        SF32(L, 0x28, phys_mul(F32(U32(L, 0x60), 0x4c), time_a));
        SF32(L, 0x24, phys_mul(F32(U32(L, 0x70), 0x4c), time_b));
    }   /* (w NaN: the stale stack words, as the original) */
    const rx_addr ma = U32(L, 0x60), mb = U32(L, 0x70);
    const float ta = F32(L, 0x28), tb = F32(L, 0x24);
    if (U32(ma, 0x64) != 0) keys_locate(U32(ma, 0x64), ta, locate_a);
    if (U32(mb, 0x64) != 0) keys_locate(U32(mb, 0x64), tb, locate_b);
    for (int32_t i = 0; i < I32(ma, 0x20); i++) {
        const rx_addr packed = U32(ma, 0x64), track = U32(ma, 0x24) + 0x18u * (uint32_t)i;
        const uint32_t bone = packed ? U32(slot_a, 4 + 4u * (uint32_t)i) : U32(track, 8);
        if (bone == 0xff) continue;
        if (packed) keys_sample_rotation(locate_a, (uint32_t)i, qa);
        else sample_rotation_track(track, ta, qa);
        if (U32(mb, 0x64) != 0) keys_sample_rotation(locate_b, (uint32_t)i, qb);
        else sample_rotation_track(U32(mb, 0x24) + 0x18u * (uint32_t)i, tb, qb);
        const rx_v4 a = v4_ld(qa), b = v4_ld(qb);
        float dot = phys_add(phys_mul(b.v[1], a.v[1]), phys_mul(a.v[0], b.v[0]));
        dot = phys_add(phys_add(dot, phys_mul(a.v[2], b.v[2])), phys_mul(b.v[3], a.v[3]));
        float wa, wb;
        phys_quat_slerp_weights(dot, w, &wa, &wb);
        const rx_v4 r = v4(phys_add(phys_mul(wa, a.v[0]), phys_mul(wb, b.v[0])),
                           phys_add(phys_mul(wb, b.v[1]), phys_mul(wa, a.v[1])),
                           phys_add(phys_mul(wb, b.v[2]), phys_mul(a.v[2], wa)),
                           phys_add(phys_mul(b.v[3], wb), phys_mul(a.v[3], wa)));
        v4_st(qb, r);
        const rx_addr rec = pose + 0x30u * bone;
        v4_st(rec + 0x20, r);
        SU32(rec, 4, 1);
    }
    for (int32_t j = 0; j < I32(ma, 0x18); j++) {
        const rx_addr packed = U32(ma, 0x64), track = U32(ma, 0x1c) + 0x14u * (uint32_t)j;
        const uint32_t bone = packed ? U32(slot_a, 4 + 4u * (uint32_t)j) : U32(track, 4);
        if (bone == 0xff) continue;
        rx_v4 a, b;
        if (packed) { keys_sample_translation(packed, (uint32_t)j, qb, locate_a); a = v4_ld(qb); }
        else {
            a = sample_translation_track(track, ta, L + 0x58, L + 0x50, L + 0x54, L);
            SF32(qb, 0, a.v[0]); SF32(qb, 4, a.v[1]); SF32(qb, 8, a.v[2]);
        }
        if (U32(mb, 0x64) != 0) { keys_sample_translation(U32(mb, 0x64), (uint32_t)j, qa, locate_b); b = v4_ld(qa); }
        else {
            b = sample_translation_track(U32(mb, 0x1c) + 0x14u * (uint32_t)j, tb, L + 0x4c, L + 0x5c, L + 0xc, L);
            SF32(qa, 0, b.v[0]); SF32(qa, 4, b.v[1]); SF32(qa, 8, b.v[2]);
        }
        const float x = phys_sub(a.v[0], phys_mul(phys_sub(a.v[0], b.v[0]), w));
        const float y = phys_sub(a.v[1], phys_mul(phys_sub(a.v[1], b.v[1]), w));
        const float z = phys_sub(a.v[2], phys_mul(phys_sub(a.v[2], b.v[2]), w));
        const rx_addr rec = pose + 0x30u * bone;
        SF32(rec, 0x10, x); SF32(rec, 0x14, y); SF32(rec, 0x18, z);
        SF32(qb, 0, x); SF32(qb, 4, y); SF32(qb, 8, z);
        SU32(rec, 0, 1);
    }
    SU32(U32(slot_a, 0), 0x4c, 0);
    SU32(U32(slot_b, 0), 0x4c, 0);
    ecx = slot_b;
out:
    rx_set_sp(sp0);
    return ecx;
}
RX_FUNCTION(0x00840b70, phys_pose_sample_blend)
