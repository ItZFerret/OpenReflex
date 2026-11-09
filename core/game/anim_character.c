/* anim_character.c -- keyframe sampling of a character's motions (0x0083xxxx: Character_SampleKeyframes, the transition samplers,
 * float event tracks).
 *
 * Character animation state C (the object Character_SampleKeyframes works on; CharacterEntity holds one):
 *   +0x18 node hierarchy (records +0x08), +0x1c bone array (0xe0 each: translation +0x50, rotation +0x60),
 *   +0x24 finished, +0x28 in transition, +0x2c current motion slot, +0x30 previous slot, +0x34 transition source slot (0: 00838ba0 blends instead),
 *   +0x3c motion time, +0x40 previous time, +0x44 transition clock, +0x48 transition start, +0x4c transition end,
 *   +0x50 transition length, +0x64 write translations to the nodes, +0x68 write rotations to the nodes,
 *   +0x70 event value outputs (float per event track), +0x74 their count, +0x84 wrapped this step, +0x88 ended this step.
 * Motion slot S: +0x00 motion handle H (+0x44 motion data M, +0x4c in use), +0x08 bone map (u8 per motion track / bone: character
 * bone, 0xff none).
 * Motion data M: +0x18 translation tracks count, +0x1c translation tracks (0x14 each), +0x20 rotation tracks count, +0x24 rotation
 * tracks (0x18 each), +0x3c event tracks count, +0x40 event tracks (0x14 each), +0x4c duration, +0x54 loops, +0x64 compressed
 * track set (0 = keyframe tracks). (core/notes/anim.md) */
#include "anim.h"
#include "anim_extern.h"
#include "anim_math.h"
#include "build_asset.h"
#include "fx.h"
#include "rx_vcall.h"
#include "loop_jobs.h"

/* 00838ba0 (stdcall: C, w; RET 8; not in plan/rx_anim.tsv): the transition blend of kind 0 (C+0x34 = 0): the current motion
 * slot C+0x2c is sampled at C+0x3c (one AnimKeys_Locate when its motion is compressed) and every mapped bone's rotation and
 * translation is blended against the bone's stored rotation +0x80 / translation +0x70 with the weight w, then written to the
 * bone (+0x60 / +0x50) and, when C+0x68 / C+0x64, to the bone's node (Node_SetLocalRotation / Node_SetLocalTranslation).
 * The rotation uses the inlined slerp weights (anim_slerp_weights: d = bone * sample, theta = acos|d|): out = s0 * bone +
 * s1 * sample. The translation is sample + (bone - sample) * (1 - w). The current handle's in-use flag (+0x4c) is set and
 * cleared. A rotation track that is not animated (or empty) leaves the sample slot as it is: on the first such track the
 * frame slot holds the caller's stale stack (the original never initializes it) - the frame below keeps the original's
 * addresses so that read sees the same bytes. Frame: push ebp; and esp,-16; sub esp,0x74; push ebx / esi / edi (the locals
 * at [esp+0x50] = sample, [esp+0x60] = blend / translation out, [esp+0x70] = locate record, [esp+0x2c]..[esp+0x40] the
 * FindSegment outputs / weights). */
static void node_set_rotation_at(rx_addr rec, rx_addr q, rx_addr ret, uint32_t ebx, uint32_t edi);
void rx_character_transition_blend(rx_addr c, float w) {
    ANIM_FRAME_ENTER(A, 0x80);
    const rx_addr S = A - 0x74;           /* the original's ESP after `sub esp,0x74` */
    const rx_addr sample = S + 0x44;      /* [esp+0x50]: the sampled rotation / the current translation */
    const rx_addr out = S + 0x54;         /* [esp+0x60]: the blend result / the sampled translation */
    const rx_addr locate = S + 0x64;      /* [esp+0x70]: the AnimKeys_Locate record (16 bytes) */
    const rx_addr tprev = S + 0x30;       /* KeyTrack_FindSegment's prev out */
    const rx_addr tnext = S + 0x34;       /* next out */
    const rx_addr tfrac = S + 0x2c;       /* fraction out */
    const rx_addr slot = U32(c, 0x2c), h = U32(slot, 0), m = U32(h, 0x44);
    SU32(h, 0x4c, 1);
    if (U32(m, 0x64) != 0) anim_keys_locate(U32(m, 0x64), F32(c, 0x3c), locate);

    for (int32_t i = 0; i < I32(m, 0x20); i++) {
        const rx_addr track = U32(m, 0x24) + 0x18u * (uint32_t)i;
        const uint32_t packed = U32(m, 0x64);
        const uint32_t bone = packed ? U8(U32(slot, 8), (uint32_t)i) : U8(U32(track, 8), U32(slot, 8));
        if (bone == 0xff) continue;
        if (packed) anim_keys_sample_rotation(locate, (uint32_t)i, sample);
        else anim_sample_rotation_track(track, F32(c, 0x3c), sample);
        const rx_addr bb = U32(c, 0x1c) + 0xe0u * bone;
        /* d = ((bx*sx + by*sy) + sz*bz) + bw*sw (operands as in the disassembly's mulss / addss order) */
        const float d = phys_add(phys_add(phys_add(phys_mul(F32(sample, 0), F32(bb, 0x80)), phys_mul(F32(sample, 4), F32(bb, 0x84))),
                                                 phys_mul(F32(sample, 8), F32(bb, 0x88))),
                                         phys_mul(F32(sample, 0xc), F32(bb, 0x8c)));
        float s0, s1;
        anim_slerp_weights(d, w, &s0, &s1);
        /* the result is written back over the sample slot (the original reuses it for the node call and as the next track's
         * stale value), then to the bone's rotation */
        const float x = phys_add(phys_mul(F32(bb, 0x80), s0), phys_mul(F32(sample, 0), s1));
        const float y = phys_add(phys_mul(F32(sample, 4), s1), phys_mul(F32(bb, 0x84), s0));
        const float z = phys_add(phys_mul(F32(sample, 8), s1), phys_mul(F32(bb, 0x88), s0));
        const float ww = phys_add(phys_mul(F32(bb, 0x8c), s0), phys_mul(F32(sample, 0xc), s1));
        SF32(sample, 0, x); SF32(sample, 4, y); SF32(sample, 8, z); SF32(sample, 0xc, ww);
        SF32(bb, 0x60, x); SF32(bb, 0x64, y); SF32(bb, 0x68, z); SF32(bb, 0x6c, ww);
        if (U32(c, 0x68) != 0) {
            const rx_addr rec = U32(U32(c, 0x18), 8) + 16u * bone;
            node_set_rotation_at(rec, sample, 0x0083943f, bone, c);
        }
    }

    for (int32_t i = 0; i < I32(m, 0x18); i++) {
        const rx_addr track = U32(m, 0x1c) + 0x14u * (uint32_t)i;
        const uint32_t packed = U32(m, 0x64);
        const uint32_t bone = packed ? U8(U32(slot, 8), (uint32_t)i) : U8(U32(track, 4), U32(slot, 8));
        if (bone == 0xff) continue;
        const rx_addr bb = U32(c, 0x1c) + 0xe0u * bone;
        const float cx = F32(bb, 0x70), cy = F32(bb, 0x74), cz = F32(bb, 0x78);
        float x, y, z;
        if (packed) {
            anim_keys_sample_translation(U32(m, 0x64), (uint32_t)i, locate, out);
            x = F32(out, 0); y = F32(out, 4); z = F32(out, 8);
        } else if (U32(track, 8) != 0 && I32(track, 0xc) > 1) {
            const uint32_t inside = anim_keytrack_find_segment(U32(track, 0), U32(track, 0xc), F32(c, 0x3c), tprev, tnext, tfrac);
            const rx_addr p = U32(tprev, 0), n = U32(tnext, 0);
            const float f = F32(tfrac, 0);
            if (inside) {
                /* prev + (next - prev) * f, each lane's operands as in the original */
                x = phys_add(phys_mul(phys_sub(F32(n, 4), F32(p, 4)), f), F32(p, 4));
                y = phys_add(phys_mul(phys_sub(F32(n, 8), F32(p, 8)), f), F32(p, 8));
                z = phys_add(phys_mul(phys_sub(F32(n, 0xc), F32(p, 0xc)), f), F32(p, 0xc));
            } else {
                /* next + (next - prev) * f (the original extrapolates from the next key here) */
                x = phys_add(phys_mul(phys_sub(F32(n, 4), F32(p, 4)), f), F32(n, 4));
                y = phys_add(phys_mul(phys_sub(F32(n, 8), F32(p, 8)), f), F32(n, 8));
                z = phys_add(phys_mul(phys_sub(F32(n, 0xc), F32(p, 0xc)), f), F32(n, 0xc));
            }
        } else {
            const rx_addr keys = U32(track, 0);
            const uint32_t k = anim_keytrack_upper(keys, U32(track, 0xc), F32(c, 0x3c));
            const rx_addr key = keys + 16u * k - 0x10;
            x = F32(key, 4); y = F32(key, 8); z = F32(key, 0xc);
        }
        const float k = 1.0f - w;
        x = phys_add(phys_mul(phys_sub(cx, x), k), x);
        y = phys_add(phys_mul(phys_sub(cy, y), k), y);
        z = phys_add(phys_mul(phys_sub(cz, z), k), z);
        SF32(bb, 0x50, x); SF32(bb, 0x54, y); SF32(bb, 0x58, z);
        SF32(out, 0, x); SF32(out, 4, y); SF32(out, 8, z);
        if (U32(c, 0x64) != 0) rx_node_set_local_translation(U32(U32(c, 0x18), 8) + 16u * bone, out);
    }
    SU32(U32(slot, 0), 0x4c, 0);
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x00838ba0, rx_character_transition_blend)


/* Rotation keyframe track (0x18 bytes): +0x00 keys (0x0c each: time, packed rotation), +0x04 cursor (last segment found),
 * +0x08 motion bone, +0x0c animated, +0x10 key count. Samples the track at `time` into the quaternion at q (game memory): a
 * slerp of the segment's two keys, or one key decoded when the time is outside the keys. The cursor search starts from the last
 * segment (reset to 1 / to the end when the time jumped across the first / last quarter). Untouched when the track is not
 * animated (the caller then stores what q held). */
void anim_sample_rotation_track(rx_addr track, float time, rx_addr q) {
    if (U32(track, 0xc) == 0) return;
    int32_t n = I32(track, 0x10);
    if (n < 1) return;
    rx_addr keys = U32(track, 0);
    rx_addr k0, k1;
    float f;
    if (n <= 2) {
        if (n == 2) { k0 = keys; k1 = keys + 0xc; f = (time - F32(k0, 0)) / (F32(k1, 0) - F32(k0, 0)); }
        else { k0 = keys; k1 = keys; f = 0.0f; }
    } else {
        int32_t last = n - 1, quarter = last >> 2, three_quarters = (last >> 1) + quarter;
        /* the cached segment (+4) is the track's, shared by every entity of the clip: read once, stored back as the last job of a
         * job batch leaves it (rx_job_wr32, w-jobthreads2) */
        int32_t i = I32(track, 4);
        if (!(i <= three_quarters) && F32(keys, 0xc * quarter) > time) i = 1;
        else if (i < quarter && time > F32(keys, 0xc * three_quarters)) i = last;
        if (i < 1) i = 1;
        int inside = 1;
        while (time > F32(keys, 0xc * i)) {
            if (++i >= n) { i = last; inside = 0; break; }
        }
        while (F32(keys, 0xc * (i - 1)) > time) {
            if (--i < 1) { i = 1; inside = 0; break; }
        }
        rx_job_wr32(track + 4, (uint32_t)i);
        k0 = keys + 0xc * (i - 1); k1 = keys + 0xc * i;
        f = (time - F32(k0, 0)) / (F32(k1, 0) - F32(k0, 0));
        if (!inside) {
            float r[4];
            anim_packed_quat(k1 + 4, r);
            for (int j = 0; j < 4; j++) SF32(q, 4 * j, r[j]);
            return;
        }
    }
    anim_quat_slerp_packed(k0 + 4, k1 + 4, q, f);
}

#define ANIM_SITE_REC 0xffffffffu   /* "the node record" in store_translation's register arguments */

/* Node_SetLocalRotation (007dcdf0, core) as called from an original call site: the quaternion's address pushed, the call's return
 * address in the dead stack, and the callee-saved registers its Node_Invalidate saves (EBX, ESI = the record, EDI) as the original
 * has them there. */
static void node_set_rotation_at(rx_addr rec, rx_addr q, rx_addr ret, uint32_t ebx, uint32_t edi) {
    const uint32_t b = anim_reg_get(ANIM_EBX), s = anim_reg_get(ANIM_ESI), d = anim_reg_get(ANIM_EDI);
    anim_reg_set(ANIM_EBX, ebx); anim_reg_set(ANIM_ESI, rec); anim_reg_set(ANIM_EDI, edi);
    rx_set_sp(rx_sp() - 4); SU32(rx_sp(), 0, q);
    rx_set_sp(rx_sp() - 4); SU32(rx_sp(), 0, ret);
    rx_node_set_local_rotation(rec, q);
    rx_set_sp(rx_sp() + 8);
    anim_reg_set(ANIM_EBX, b); anim_reg_set(ANIM_ESI, s); anim_reg_set(ANIM_EDI, d);
}

/* The rotation tracks of motion m (slot = its motion slot: bone map +0x08) at `time` into the character's bones (+0x60) and, when
 * C+0x68, their nodes. q = the 16-byte sample local of the caller's frame (a track that is not animated stores what it holds),
 * locate = its AnimKeys_Locate record (compressed motions). */
/* motion_at: the caller is Character_SampleMotionAt (else Character_SampleKeyframes): its registers at the node call. */
static void sample_rotations(rx_addr c, rx_addr m, rx_addr slot, float time, rx_addr q, rx_addr locate, int motion_at) {
    for (int32_t i = 0; i < I32(m, 0x20); i++) {
        rx_addr track = U32(m, 0x24) + 0x18u * (uint32_t)i;
        rx_addr packed = U32(m, 0x64);
        uint32_t bone = packed ? U8(U32(slot, 8), i) : U8(U32(track, 8), U32(slot, 8));
        if (bone == 0xff) continue;
        if (packed) anim_keys_sample_rotation(locate, (uint32_t)i, q);
        else anim_sample_rotation_track(track, time, q);
        rx_addr b = U32(c, 0x1c) + 0xe0u * bone + 0x60;
        for (int j = 0; j < 4; j++) SU32(b, 4 * j, U32(q, 4 * j));
        if (U32(c, 0x68) == 0) continue;
        rx_addr rec = U32(U32(c, 0x18), 8) + 16u * bone;
        if (motion_at) node_set_rotation_at(rec, q, 0x00836000, bone, m);
        else node_set_rotation_at(rec, q, 0x0083588d, c, bone);
    }
}

/* A bone's sampled translation: into the bone (+0x50), the sample local q and, when C+0x64, the node's local translation (its
 * Node_Invalidate with the caller's return address and EBX / ESI / EDI). */
static void store_translation(rx_addr c, uint32_t bone, rx_addr q, float x, float y, float z, rx_addr ret, uint32_t ebx,
                              uint32_t esi, uint32_t edi) {
    rx_addr b = U32(c, 0x1c) + 0xe0u * bone + 0x50;
    SF32(b, 0, x); SF32(b, 4, y); SF32(b, 8, z);
    SF32(q, 0, x); SF32(q, 4, y); SF32(q, 8, z);
    if (U32(c, 0x64) != 0) {
        rx_addr rec = U32(U32(c, 0x18), 8) + 16u * bone;
        rx_addr lm = U32(U32(rec, 0), 0x18) + 0x40u * U8(rec, 0xb);
        SF32(lm, 0x30, x); SF32(lm, 0x34, y); SF32(lm, 0x38, z);
        rx_regs r = anim_regs();
        r.r[ANIM_EBX] = ebx == ANIM_SITE_REC ? rec : ebx; r.r[ANIM_ESI] = esi == ANIM_SITE_REC ? rec : esi; r.r[ANIM_EDI] = edi;
        anim_node_invalidate_from(rec, &r, ret);
    }
}

/* The event value tracks of motion m at `time` into C+0x70[i]. */
static void sample_events(rx_addr c, rx_addr m, float time) {
    if (U32(m, 0x40) != 0 && I32(c, 0x74) > 0)
        for (int32_t i = 0; i < I32(m, 0x3c); i++)
            rx_float_track_sample(U32(m, 0x40) + 0x14u * (uint32_t)i, U32(c, 0x70) + 4u * (uint32_t)i, time);
}

/* Index of the first of the 16-byte keys (time first) after `time` (binary search; 0 when empty or before the first, count at or
 * after the last): KeyTrack_Upper 004dbe00 as the samplers inline it. */
int32_t anim_key16_upper(rx_addr keys, int32_t n, float time) {
    if (n == 0) return 0;
    if (time >= F32(keys, 16 * (n - 1))) return n;
    if (F32(keys, 0) > time) return 0;
    int32_t lo = 0, hi = n;
    for (;;) {
        int32_t mid = (lo + hi) >> 1;
        if (!(time >= F32(keys, 16 * mid))) {
            if (time >= F32(keys, 16 * (mid - 1))) return mid;
            hi = mid;
        } else {
            if (F32(keys, 16 * (mid + 1)) > time) return mid + 1;
            lo = mid;
        }
    }
}

/* 00835430 Character_SampleKeyframes (stack: C, RET 4; xmm1 = dt): advances the current motion's time by dt (wrapping or ending at
 * its duration), samples every rotation and translation track into the character's bones (and their nodes when C+0x68 / +0x64
 * ask), or runs the transition blend while one is active; then samples the event value tracks. */
void rx_character_sample_keyframes(rx_addr c, float dt) {
    SU32(c, 0x88, 0); SU32(c, 0x84, 0);
    rx_addr slot = U32(c, 0x2c);
    if (U32(c, 0x24) != 0 || slot == 0) return;
    /* the frame of the original: locals at their addresses (the samplers write them, stale values are stored on purpose) */
    ANIM_FRAME_ENTER(A, 0x60);
    const rx_addr q = A - 0x40;          /* sampled rotation / translation (16 bytes) */
    const rx_addr frac = A - 0x48;       /* FindSegment fraction */
    const rx_addr next_key = A - 0x2c, prev_key = A - 0x28;
    const rx_addr locate = A - 0x20;     /* AnimKeys_Locate record */

    SF32(c, 0x40, F32(c, 0x3c));
    SU32(c, 0x30, slot);
    SF32(c, 0x3c, F32(c, 0x3c) + dt);
    rx_addr h = U32(slot, 0), m = U32(h, 0x44);
    SU32J(h, 0x4c, 1);
    if (F32(c, 0x3c) > F32(m, 0x4c)) {
        if (U32(m, 0x54) != 0) { SF32(c, 0x3c, F32(c, 0x3c) - F32(m, 0x4c)); SU32(c, 0x84, 1); }
        else SU32(c, 0x88, 1);
    }
    if (U32(c, 0x28) == 0) {
        rx_addr packed = U32(m, 0x64);
        if (packed) anim_keys_locate(packed, F32(c, 0x3c), locate);
        sample_rotations(c, m, U32(c, 0x2c), F32(c, 0x3c), q, locate, 0);
        for (int32_t i = 0; i < I32(m, 0x18); i++) {
            rx_addr track = U32(m, 0x1c) + 0x14u * (uint32_t)i;
            packed = U32(m, 0x64);
            uint32_t bone = packed ? U8(U32(U32(c, 0x2c), 8), i) : U8(U32(track, 4), U32(U32(c, 0x2c), 8));
            if (bone == 0xff) continue;
            /* translation track (0x14): +0x00 keys (0x10: time, x y z), +0x04 motion bone, +0x08 animated, +0x0c key count */
            float x, y, z;
            if (packed) {
                anim_keys_sample_translation(packed, (uint32_t)i, locate, q);
                x = F32(q, 0); y = F32(q, 4); z = F32(q, 8);
            } else if (U32(track, 8) != 0 && I32(track, 0xc) > 1) {
                uint32_t inside = anim_keytrack_find_segment(U32(track, 0), U32(track, 0xc), F32(c, 0x3c), prev_key, next_key, frac);
                rx_addr a = U32(next_key, 0), p = U32(prev_key, 0);
                float t = F32(frac, 0);
                rx_addr base = inside ? p : a;
                x = (F32(a, 4) - F32(p, 4)) * t + F32(base, 4);
                y = (F32(a, 8) - F32(p, 8)) * t + F32(base, 8);
                z = (F32(a, 0xc) - F32(p, 0xc)) * t + F32(base, 0xc);
            } else {
                rx_addr keys = U32(track, 0);
                uint32_t k = anim_keytrack_upper(keys, U32(track, 0xc), F32(c, 0x3c));
                rx_addr key = keys + 16u * k - 0x10;
                x = F32(key, 4); y = F32(key, 8); z = F32(key, 0xc);
            }
            store_translation(c, bone, q, x, y, z, 0x00835ac8, c, ANIM_SITE_REC, m);
        }
    } else {
        /* transition: smoothstep of the transition clock over its length */
        SF32(c, 0x44, F32(c, 0x44) + dt);
        float x = ((F32(c, 0x44) - F32(c, 0x48)) - dt) / F32(c, 0x50), w;
        if (0.0f > x) w = 0.0f;
        else if (x >= 1.0f) w = 1.0f;
        else w = (3.0f /* 0x92fe48 */ - x * 2.0f) * x * x;
        if (U32(c, 0x34) != 0) rx_character_transition_sample(c, w);
        else { ANIM_CALL_FRAME(8); rx_character_transition_blend(c, w); ANIM_CALL_FRAME_END(8); }
    }
    sample_events(c, m, F32(c, 0x3c));
    if (F32(c, 0x44) >= F32(c, 0x4c)) { SU32(c, 0x28, 0); SF32(c, 0x44, 0.0f); }
    if (U32(c, 0x88) != 0) { SU32(c, 0x24, 1); SF32(c, 0x44, 0.0f); }
    SU32J(U32(U32(c, 0x2c), 0), 0x4c, 0);
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x00835430, rx_character_sample_keyframes)

/* 008435f0 FloatKeys_FindSegment (EAX = keys, EDX = count, xmm0 = time; cdecl: prev key out, next key out, fraction out; RET 0xc)
 * -> EAX: the 8-byte-key (time, value) segment of the time, as KeyTrack_FindSegment for the 16-byte tracks: the binary search
 * narrows to a key i (first key at or before the time) and the outs take key[i-1] / key[i] and (t - t[i-1]) / (t[i] - t[i-1]);
 * i == count clamps to the last pair (return 0); a time before the first key, i == 0 or an empty track takes the outs key[1] /
 * key[0] and (t - last) / (first - second) (return 0). The comparisons are the original's comiss branches (jb / ja / jae: a NaN
 * takes the jb path). */
uint32_t rx_floatkeys_find_segment(rx_addr keys, uint32_t count, float time, rx_addr prev, rx_addr next, rx_addr frac) {
    const float t = time;
    if (count != 0) {
        const float last = F32(keys, 8u * count - 8u);
        uint32_t i = count;
        if (!(t >= last)) {                                   /* comiss t, last; jb */
            const float first = F32(keys, 0);
            if (first > t) goto clamp;                        /* comiss first, t; ja */
            {
                uint32_t lo = 0, hi = count;
                for (;;) {
                    i = (hi + lo) >> 1;
                    if (!(t >= F32(keys, 8u * i))) {          /* comiss t, [key i]; jb */
                        if (t >= F32(keys, 8u * i - 8u)) break;   /* comiss t, [key i - 1]; jae */
                        hi = i;
                    } else {
                        if (F32(keys, 8u * i + 8u) > t) { i++; break; }   /* comiss [key i + 1], t; ja */
                        lo = i;
                    }
                }
            }
        }
        if (i != 0) {
            if (i == count) {                                 /* 008436bc */
                const rx_addr kb = keys + 8u * count;
                SU32(prev, 0, kb - 0x10u);
                SU32(next, 0, kb - 8u);
                SF32(frac, 0, rx_divs(rx_subs(t, last), rx_subs(F32(kb, -8), F32(kb, -0x10))));
                return 0;
            }
            const rx_addr kb = keys + 8u * i;                 /* 00843657 */
            SU32(prev, 0, kb - 8u);
            SU32(next, 0, kb);
            SF32(frac, 0, rx_divs(rx_subs(t, F32(kb, -8)), rx_subs(F32(kb, 0), F32(kb, -8))));
            return 1;
        }
    }
clamp:                                                        /* 00843692 */
    SU32(prev, 0, keys + 8u);
    SU32(next, 0, keys);
    SF32(frac, 0, rx_divs(rx_subs(t, F32(keys + 8u * count, -8)), rx_subs(F32(keys, 0), F32(keys, 8u))));
    return 0;
}
RX_FUNCTION(0x008435f0, rx_floatkeys_find_segment)

/* 00839f40 FloatTrack_Sample (ECX = track, EDI = float out, stack time; RET 4): an event value track (+0x04 interpolate,
 * +0x08 key count, +0x10 keys of 8 bytes: time, value): the lerp of the time's segment, or the value of the last key at or before
 * the time (binary search; the key before the first for an empty track or a time before it). */
void rx_float_track_sample(rx_addr track, rx_addr out, float time) {
    int32_t n = I32(track, 8);
    if (U32(track, 4) != 0 && n > 1) {
        const rx_addr sp = rx_sp();       /* locals: prev at E-8, fraction at E-4, next in the argument slot E+4 */
        rx_set_sp(sp - 8);
        rx_addr prev = sp - 8, frac = sp - 4, next = sp + 4;
        uint32_t inside = anim_floatkeys_find_segment(U32(track, 0x10), (uint32_t)n, time, prev, next, frac);
        rx_addr p = U32(prev, 0), a = U32(next, 0);
        float base = inside ? F32(p, 4) : F32(a, 4);
        SF32(out, 0, (F32(a, 4) - F32(p, 4)) * F32(frac, 0) + base);
        rx_set_sp(sp);
        return;
    }
    rx_addr keys = U32(track, 0x10);
    int32_t k;
    if (n == 0 || F32(keys, 0) > time) k = 0;
    else if (time >= F32(keys, 8 * (n - 1))) k = n;
    else {
        int32_t lo = 0, hi = n;
        for (;;) {
            int32_t mid = (hi + lo) >> 1;
            if (!(time >= F32(keys, 8 * mid))) {
                if (time >= F32(keys, 8 * (mid - 1))) { k = mid; break; }
                hi = mid;
            } else {
                if (F32(keys, 8 * (mid + 1)) > time) { k = mid + 1; break; }
                lo = mid;
            }
        }
    }
    anim_x87_copy_f32(out, keys + 8u * (uint32_t)k - 4);
}
RX_FUNCTION(0x00839f40, rx_float_track_sample)

/* 00835170 (ESI = character C): the current motion slot's (+0x2c -> handle H, H+0x44 the data M) random start time: rand() /
 * 32768 (0x92e054) * M+0x4c (duration) into the motion time +0x3c and the previous time +0x40; H+0x4c is held while read. The
 * original keeps its first, overwritten store of the fraction in +0x3c (the port writes the final time directly, as the value
 * at the end is what the animation sampler and every reader see). */
void rx_character_random_motion_time(rx_addr c) {
    const rx_addr slot = U32(c, 0x2c);
    const rx_addr h = U32(slot, 0);
    const rx_addr data = U32(h, 0x44);
    SU32(h, 0x4c, 1);
    const float f = (float)crt_rand() * F32(0, 0x0092e054);
    SF32(c, 0x3c, f);                                     /* the original's first (overwritten) store */
    const float t = rx_muls(F32(data, 0x4c), f);
    SF32(c, 0x3c, t);
    SU32(U32(slot, 0), 0x4c, 0);
    anim_x87_copy_f32(c + 0x40, c + 0x3c);                /* fld / fstp */
}
RX_FUNCTION(0x00835170, rx_character_random_motion_time)

/* 00835bf0 Character_SampleMotionAt (ECX = motion slot, stack: C, phase; RET 8): samples the slot's motion at phase * duration
 * into the character's bones (rotations, translations, event values) without advancing the character's clock: clears the current
 * slot (C+0x2c) and C+0x84, leaves C+0x30 = the slot and C+0x40 = the sampled time. The compressed translation sampling and the
 * static key search are inlined here (as AnimKeys_SampleTranslation 0082bac0 / KeyTrack_Upper 004dbe00). */
void rx_character_sample_motion_at(rx_addr slot, rx_addr c, float phase) {
    SU32(c, 0x84, 0); SU32(c, 0x2c, 0);
    rx_addr h = U32(slot, 0), m = U32(h, 0x44);
    SU32J(h, 0x4c, 1);
    float time = F32(m, 0x4c) * phase;
    ANIM_FRAME_ENTER(A, 0x90);
    const rx_addr q = A - 0x60, locate = A - 0x30;
    const rx_addr frac = A - 0x64, prev_key = A - 0x40, next_key = A - 0x3c;
    if (U32(m, 0x64)) anim_keys_locate(U32(m, 0x64), time, locate);
    sample_rotations(c, m, slot, time, q, locate, 1);
    for (int32_t i = 0; i < I32(m, 0x18); i++) {
        rx_addr track = U32(m, 0x1c) + 0x14u * (uint32_t)i;
        rx_addr packed = U32(m, 0x64);
        uint32_t bone = packed ? U8(U32(slot, 8), i) : U8(U32(track, 4), U32(slot, 8));
        if (bone == 0xff) continue;
        float x, y, z;
        if (packed) {
            /* compressed set: +0x04 the interpolated track, +0x0c frame rate, +0x1c frames, +0x3c keys (16 bytes) */
            if ((uint32_t)i != U32(packed, 4)) {
                rx_addr k = U32(packed, 0x3c) + 16u * (U32(packed, 0x1c) + (uint32_t)i);
                x = F32(k, 0); y = F32(k, 4); z = F32(k, 8);
            } else {
                int32_t f0 = rx_cvtt((float)I32(packed, 0xc) * F32(locate, 0)), f1 = f0 + 1;
                if (f1 > I32(packed, 0x1c)) f1 = f0;
                rx_addr k0 = U32(packed, 0x3c) + 16u * (uint32_t)f0, k1 = U32(packed, 0x3c) + 16u * (uint32_t)f1;
                float f = F32(locate, 4), g = 1.0f - f;
                x = g * F32(k0, 0) + f * F32(k1, 0);
                y = F32(k0, 4) * g + F32(k1, 4) * f;
                z = F32(k0, 8) * g + F32(k1, 8) * f;
            }
        } else if (U32(track, 8) != 0 && I32(track, 0xc) > 1) {
            uint32_t inside = anim_keytrack_find_segment(U32(track, 0), U32(track, 0xc), time, prev_key, next_key, frac);
            rx_addr a = U32(next_key, 0), p = U32(prev_key, 0), base = inside ? p : a;
            float t = F32(frac, 0);
            x = (F32(a, 4) - F32(p, 4)) * t + F32(base, 4);
            y = (F32(a, 8) - F32(p, 8)) * t + F32(base, 8);
            z = (F32(a, 0xc) - F32(p, 0xc)) * t + F32(base, 0xc);
        } else {
            rx_addr keys = U32(track, 0);
            rx_addr key = keys + 16u * (uint32_t)anim_key16_upper(keys, I32(track, 0xc), time) - 0x10;
            x = F32(key, 4); y = F32(key, 8); z = F32(key, 0xc);
        }
        store_translation(c, bone, q, x, y, z, 0x00836329, ANIM_SITE_REC, m, (uint32_t)i);
    }
    sample_events(c, m, time);
    SU32(c, 0x30, slot);
    SF32(c, 0x40, time);
    SU32J(U32(slot, 0), 0x4c, 0);
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x00835bf0, rx_character_sample_motion_at)

/* One translation track of a motion (track index i, sampled at `time`) into out (game memory): compressed tracks through
 * AnimKeys_SampleTranslation, keyframe tracks through KeyTrack_FindSegment (lerp; clamped: from the next key) or KeyTrack_Upper.
 * prev / next / frac = the caller's FindSegment locals. As the blenders write it (one call per motion). */
static void sample_translation_track(rx_addr m, int32_t i, float time, rx_addr locate, rx_addr out, rx_addr prev, rx_addr next,
                                     rx_addr frac) {
    rx_addr packed = U32(m, 0x64);
    if (packed) { anim_keys_sample_translation(packed, (uint32_t)i, locate, out); return; }
    rx_addr track = U32(m, 0x1c) + 0x14u * (uint32_t)i;
    if (U32(track, 8) != 0 && I32(track, 0xc) > 1) {
        uint32_t inside = anim_keytrack_find_segment(U32(track, 0), U32(track, 0xc), time, prev, next, frac);
        rx_addr p = U32(prev, 0), n = U32(next, 0), base = inside ? p : n;
        float t = F32(frac, 0);
        SF32(out, 0, (F32(n, 4) - F32(p, 4)) * t + F32(base, 4));
        SF32(out, 4, (F32(n, 8) - F32(p, 8)) * t + F32(base, 8));
        SF32(out, 8, (F32(n, 0xc) - F32(p, 0xc)) * t + F32(base, 0xc));
    } else {
        rx_addr keys = U32(track, 0);
        rx_addr key = keys + 16u * anim_keytrack_upper(keys, U32(track, 0xc), time) - 0x10;
        SF32(out, 0, F32(key, 4)); SF32(out, 4, F32(key, 8)); SF32(out, 8, F32(key, 0xc));
    }
}

/* 008363b0 Character_SampleBlend3 (ECX = slot C, stack: C, slot A, slot B, phase A, phase B, phase C, w; RET 0x1c): a three-way
 * blend along w in [0, 1]: near 0 / 0.5 / 1 (0.05, 0.45..0.55, 0.95) one motion alone (Character_SampleMotionAt), else the two
 * motions around w (A and B for w < 0.5, B and C above) sampled at their phases and blended with w' = 2w (or 2w - 1): rotations
 * slerped, translations lerped. The bone maps and C+0x30 are slot A's whatever the pair. */
void rx_character_sample_blend3(rx_addr slot_c, rx_addr c, rx_addr slot_a, rx_addr slot_b, float phase_a, float phase_b,
                                float phase_c, float w) {
    ANIM_FRAME_ENTER(A, 0xa0);                                /* push ebp; and esp, -16; sub esp, 0x94; push ebx, esi, edi */
    {   /* near a corner of the blend: that motion alone (pushes: the phase through the x87, C; ESI = slot A as the original) */
        rx_addr slot = 0, ret = 0; float phase = 0.0f;
        if (0.05f /* 0x9d3bfc */ > w) { slot = slot_a; phase = phase_a; ret = 0x008363e9; }
        else if (w > 0.45f /* 0x9cd960 */ && 0.55f /* 0x9776f0 */ > w) { slot = slot_b; phase = phase_b; ret = 0x00836417; }
        else if (w > 0.95f /* 0x9cd9ac */) { slot = slot_c; phase = phase_c; ret = 0x00836436; }
        if (slot) {
            const uint32_t esi = anim_reg_get(ANIM_ESI);
            anim_reg_set(ANIM_ESI, slot_a);
            rx_set_sp(rx_sp() - 4); SF32(rx_sp(), 0, phase); anim_x87_restore_f32(rx_sp());
            rx_set_sp(rx_sp() - 4); SU32(rx_sp(), 0, c);
            rx_set_sp(rx_sp() - 4); SU32(rx_sp(), 0, ret);
            rx_character_sample_motion_at(slot, c, F32(rx_sp() + 8, 0));
            rx_set_sp(rx_sp() + 0xc);
            anim_reg_set(ANIM_ESI, esi);
            ANIM_FRAME_LEAVE(A);
            return;
        }
    }
    const rx_addr m1_l = A - 0x90, t1_l = A - 0x84, t2_l = A - 0x8c, m2_l = A - 0x38;   /* locals (stale when w is NaN) */
    const rx_addr qa = A - 0x70, qb = A - 0x80, locate1 = A - 0x20, locate2 = A - 0x10;
    const rx_addr next1 = A - 0x50, frac1 = A - 0x4c, prev1 = A - 0x48;
    const rx_addr prev2 = A - 0x54, next2 = A - 0x44, frac2 = A - 0x40;
    SU32(c, 0x84, 0); SU32(c, 0x2c, 0);
    if (0.5f /* 0x9322cc */ > w) {
        rx_addr ha = U32(slot_a, 0), hb = U32(slot_b, 0);
        SU32(ha, 0x4c, 1); SU32(hb, 0x4c, 1);
        SU32(m1_l, 0, U32(ha, 0x44)); SU32(m2_l, 0, U32(hb, 0x44));
        SF32(t1_l, 0, F32(U32(ha, 0x44), 0x4c) * phase_a); SF32(t2_l, 0, F32(U32(hb, 0x44), 0x4c) * phase_b);
        w = w * 2.0f;          /* (the original keeps w' in its argument slot) */
    } else if (w >= 0.5f) {
        rx_addr hb = U32(slot_b, 0), hc = U32(slot_c, 0);
        SU32(hb, 0x4c, 1); SU32(hc, 0x4c, 1);
        SU32(m1_l, 0, U32(hb, 0x44)); SU32(m2_l, 0, U32(hc, 0x44));
        SF32(t1_l, 0, F32(U32(hb, 0x44), 0x4c) * phase_b); SF32(t2_l, 0, F32(U32(hc, 0x44), 0x4c) * phase_c);
        w = (w - 0.5f) * 2.0f;
    }
    rx_addr m1 = U32(m1_l, 0), m2 = U32(m2_l, 0);
    float t1 = F32(t1_l, 0), t2 = F32(t2_l, 0);
    if (U32(m1, 0x64)) anim_keys_locate(U32(m1, 0x64), t1, locate1);
    if (U32(m2, 0x64)) anim_keys_locate(U32(m2, 0x64), t2, locate2);
    for (int32_t i = 0; i < I32(m1, 0x20); i++) {
        rx_addr packed = U32(m1, 0x64);
        uint32_t bone = packed ? U8(U32(slot_a, 8), i) : U8(U32(U32(m1, 0x24) + 0x18u * (uint32_t)i, 8), U32(slot_a, 8));
        if (bone == 0xff) continue;
        if (packed) anim_keys_sample_rotation(locate1, (uint32_t)i, qa);
        else anim_sample_rotation_track(U32(m1, 0x24) + 0x18u * (uint32_t)i, t1, qa);
        if (U32(m2, 0x64)) anim_keys_sample_rotation(locate2, (uint32_t)i, qb);
        else anim_sample_rotation_track(U32(m2, 0x24) + 0x18u * (uint32_t)i, t2, qb);
        float d = ((F32(qa, 0) * F32(qb, 0) + F32(qa, 4) * F32(qb, 4)) + F32(qb, 8) * F32(qa, 8)) + F32(qa, 0xc) * F32(qb, 0xc);
        float s0, s1;
        anim_slerp_weights(d, w, &s0, &s1);
        SF32(qb, 0xc, s1 * F32(qb, 0xc) + F32(qa, 0xc) * s0);
        SF32(qb, 0, s1 * F32(qb, 0) + s0 * F32(qa, 0));
        SF32(qb, 4, F32(qb, 4) * s1 + F32(qa, 4) * s0);
        SF32(qb, 8, F32(qb, 8) * s1 + F32(qa, 8) * s0);
        rx_addr b = U32(c, 0x1c) + 0xe0u * bone + 0x60;
        for (int j = 0; j < 4; j++) SU32(b, 4 * j, U32(qb, 4 * j));
        if (U32(c, 0x68) != 0) node_set_rotation_at(U32(U32(c, 0x18), 8) + 16u * bone, qb, 0x008370b4, anim_reg_get(ANIM_EBX), A - 0x7c);
    }
    for (int32_t i = 0; i < I32(m1, 0x18); i++) {
        rx_addr packed = U32(m1, 0x64);
        uint32_t bone = packed ? U8(U32(slot_a, 8), i) : U8(U32(U32(m1, 0x1c) + 0x14u * (uint32_t)i, 4), U32(slot_a, 8));
        if (bone == 0xff) continue;
        sample_translation_track(m1, i, t1, locate1, qa, prev1, next1, frac1);
        sample_translation_track(m2, i, t2, locate2, qb, prev2, next2, frac2);
        float wb = w;
        float x = F32(qa, 0) - (F32(qa, 0) - F32(qb, 0)) * wb;
        float y = F32(qa, 4) - (F32(qa, 4) - F32(qb, 4)) * wb;
        float z = F32(qa, 8) - (F32(qa, 8) - F32(qb, 8)) * wb;
        rx_addr b = U32(c, 0x1c) + 0xe0u * bone + 0x50;
        SF32(b, 0, x); SF32(b, 4, y); SF32(b, 8, z);
        SF32(qa, 0, x); SF32(qa, 4, y); SF32(qa, 8, z);
        if (U32(c, 0x64) != 0) rx_node_set_local_translation(U32(U32(c, 0x18), 8) + 16u * bone, qa);
    }
    sample_events(c, m1, t1);
    SU32(c, 0x30, slot_a);
    SF32(c, 0x40, t1);
    SU32(U32(slot_a, 0), 0x4c, 0);
    SU32(U32(slot_b, 0), 0x4c, 0);
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x008363b0, rx_character_sample_blend3)

/* 00837590 Character_TransitionSample (stdcall: C, w; RET 8): a transition from the source slot C+0x34 (its motion sampled at
 * C+0x44) to the current slot C+0x2c (at C+0x3c): rotations slerped by w (current = the slerp's second quaternion), translations
 * lerped (source + (current - source)... as cur + (src - cur) * (1 - w)). Tracks the source motion lacks (index >= its count) are
 * first written from the current motion alone; for translations then (quirk of the original) the bone keeps its translation
 * (nodes on) or takes the stale rotation sample's x y z (nodes off). No event tracks, no clock update. */
void rx_character_transition_sample(rx_addr c, float w) {
    rx_addr cur = U32(U32(c, 0x2c), 0), src = U32(U32(c, 0x34), 0);
    SU32(cur, 0x4c, 1);
    rx_addr m1 = U32(cur, 0x44), m2 = U32(src, 0x44);
    SU32(src, 0x4c, 1);
    ANIM_FRAME_ENTER(A, 0xb0);
    const rx_addr qa = A - 0xa0, qb = A - 0x70, locate1 = A - 0x20, locate2 = A - 0x10;
    if (U32(m1, 0x64)) anim_keys_locate(U32(m1, 0x64), F32(c, 0x3c), locate1);
    if (U32(m2, 0x64)) anim_keys_locate(U32(m2, 0x64), F32(c, 0x44), locate2);
    for (int32_t i = 0; i < I32(m1, 0x20); i++) {
        rx_addr slot = U32(c, 0x2c);
        uint32_t bone = U32(m1, 0x64) ? U8(U32(slot, 8), i) : U8(U32(U32(m1, 0x24) + 0x18u * (uint32_t)i, 8), U32(slot, 8));
        if (bone == 0xff) continue;
        rx_addr rot = U32(c, 0x1c) + 0xe0u * bone + 0x60;
        if (i >= I32(m2, 0x20)) {
            if (U32(m1, 0x64)) anim_keys_sample_rotation(locate1, (uint32_t)i, qa);
            else anim_sample_rotation_track(U32(m1, 0x24) + 0x18u * (uint32_t)i, F32(c, 0x3c), qa);
            for (int j = 0; j < 4; j++) SU32(rot, 4 * j, U32(qa, 4 * j));
            if (U32(c, 0x68) != 0) node_set_rotation_at(U32(U32(c, 0x18), 8) + 16u * bone, qa, 0x008379d8, (uint32_t)i, c);
        }
        if (U32(m1, 0x64)) anim_keys_sample_rotation(locate1, (uint32_t)i, qa);
        else anim_sample_rotation_track(U32(m1, 0x24) + 0x18u * (uint32_t)i, F32(c, 0x3c), qa);
        if (U32(m2, 0x64)) anim_keys_sample_rotation(locate2, (uint32_t)i, qb);
        else anim_sample_rotation_track(U32(m2, 0x24) + 0x18u * (uint32_t)i, F32(c, 0x44), qb);
        float d = ((F32(qb, 0) * F32(qa, 0) + F32(qa, 4) * F32(qb, 4)) + F32(qb, 8) * F32(qa, 8)) + F32(qa, 0xc) * F32(qb, 0xc);
        float s0, s1;
        anim_slerp_weights(d, w, &s0, &s1);
        SF32(qa, 0xc, F32(qa, 0xc) * s1 + F32(qb, 0xc) * s0);
        SF32(qa, 0, s0 * F32(qb, 0) + s1 * F32(qa, 0));
        SF32(qa, 4, F32(qa, 4) * s1 + F32(qb, 4) * s0);
        SF32(qa, 8, F32(qa, 8) * s1 + F32(qb, 8) * s0);
        for (int j = 0; j < 4; j++) SU32(rot, 4 * j, U32(qa, 4 * j));
        if (U32(c, 0x68) != 0) node_set_rotation_at(U32(U32(c, 0x18), 8) + 16u * bone, qa, 0x008379d8, (uint32_t)i, c);
    }
    for (int32_t i = 0; i < I32(m1, 0x18); i++) {
        rx_addr slot = U32(c, 0x2c);
        uint32_t bone = U32(m1, 0x64) ? U8(U32(slot, 8), i) : U8(U32(U32(m1, 0x1c) + 0x14u * (uint32_t)i, 4), U32(slot, 8));
        if (bone == 0xff) continue;
        rx_addr tr = U32(c, 0x1c) + 0xe0u * bone + 0x50;
        if (i >= I32(m2, 0x18)) {
            if (U32(c, 0x64) != 0) {
                sample_translation_track(m1, i, F32(c, 0x3c), locate1, qb, A - 0x48, A - 0x3c, A - 0x4c);
                SU32(qa, 0, U32(tr, 0)); SU32(qa, 4, U32(tr, 4)); SU32(qa, 8, U32(tr, 8));
            }
            SU32(tr, 0, U32(qa, 0)); SU32(tr, 4, U32(qa, 4)); SU32(tr, 8, U32(qa, 8));
            if (U32(c, 0x64) != 0) rx_node_set_local_translation(U32(U32(c, 0x18), 8) + 16u * bone, qa);
            continue;
        }
        sample_translation_track(m1, i, F32(c, 0x3c), locate1, qa, A - 0x50, A - 0x44, A - 0x40);
        sample_translation_track(m2, i, F32(c, 0x44), locate2, qb, A - 0x7c, A - 0x84, A - 0x54);
        float k = 1.0f - w;
        float x = (F32(qb, 0) - F32(qa, 0)) * k + F32(qa, 0);
        float y = (F32(qb, 4) - F32(qa, 4)) * k + F32(qa, 4);
        float z = (F32(qb, 8) - F32(qa, 8)) * k + F32(qa, 8);
        SF32(tr, 0, x); SF32(tr, 4, y); SF32(tr, 8, z);
        SF32(qa, 0, x); SF32(qa, 4, y); SF32(qa, 8, z);
        if (U32(c, 0x64) != 0) rx_node_set_local_translation(U32(U32(c, 0x18), 8) + 16u * bone, qa);
    }
    SU32(U32(U32(c, 0x2c), 0), 0x4c, 0);
    SU32(U32(U32(c, 0x34), 0), 0x4c, 0);
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x00837590, rx_character_transition_sample)

/* ---- the character animation state's motion slots (C = CharacterEntity + 0x148: +0x04 slots, +0x08 count, +0x0c capacity,
 *      +0x10 growth, +0x18 node hierarchy, +0x1c bone array, +0x78 / +0x7c owned helper object, +0x8c bone name cache) ---- */

/* 00843350 / 008433b0 (engine): the bone name -> bone index cache (hash map at C+0x8c: +0 bucket count, +4 max load, +8 count,
 * +0xc buckets of {key +0, byte value +4, hash +8, next +0xc}, +0x18 the node allocator): find (EAX = map, EDI = &name cell,
 * stack = byte out; RET 4; 1 found) and insert (EDI = map, stack (&name cell, &byte); the table grows to (int)(n * 1.5) when
 * count / n > the max load; the node from the allocator (vf1, 0x10)). 007de9e0 (scene): the node record of a name (EDI =
 * hierarchy, stack name; 0 none). */
uint32_t rx_bone_cache_find(rx_addr map, rx_addr name_cell, rx_addr out) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 8);                                       /* the argument + the return address (as the original's call) */
    SU32(sp0, -4, 0); SU32(sp0, -8, 0);
    const uint32_t h = rx_string_hash_nocase(U32(name_cell, 0));
    uint32_t r = 0;
    for (rx_addr e = U32(U32(map, 0xc), 4u * (h % U32(map, 0))); e != 0; e = U32(e, 0xc)) {
        if (U32(e, 8) != h) continue;
        if (crt_stricmp(U32(e, 0), U32(name_cell, 0)) == 0) { SU8(out, 0, U8(e, 4)); r = 1; break; }
    }
    rx_set_sp(sp0);
    return r;
}
RX_FUNCTION(0x00843350, rx_bone_cache_find)
void rx_bone_cache_insert(rx_addr map, rx_addr name_cell, rx_addr value) {
    const float ratio = phys_div((float)I32(map, 8), (float)I32(map, 0));
    if (ratio > F32(map, 4)) rx_strtab_rehash(map, (uint32_t)rx_cvtt(phys_mul((float)I32(map, 0), 1.5f /* 0x9322a4 */)));
    const rx_addr name = U32(name_cell, 0);
    const uint32_t h = rx_string_hash_nocase(name);
    const uint32_t b = h % U32(map, 0);
    for (rx_addr e = U32(U32(map, 0xc), 4u * b); e != 0; e = U32(e, 0xc)) {
        if (U32(e, 8) != h) continue;
        if (crt_stricmp(name, U32(e, 0)) == 0) { SU8(e, 4, U8(value, 0)); return; }
    }
    uint32_t sz = 0x10;
    const rx_addr e = rx_vcall(U32(map, 0x18), 4, 1, &sz);    /* the allocator: (0x10) */
    SU32(e, 0xc, 0);                                          /* (the original writes through a null pointer if it fails) */
    SU32(e, 0, name);
    SU8(e, 4, U8(value, 0));
    SU32(e, 8, h);
    SU32(e, 0xc, U32(U32(map, 0xc), 4u * b));
    SU32(U32(map, 0xc), 4u * b, e);
    SU32(map, 8, U32(map, 8) + 1);
}
RX_FUNCTION(0x008433b0, rx_bone_cache_insert)
static rx_addr hierarchy_find_node(rx_addr hierarchy, rx_addr name) {
    ANIM_CALL_FRAME(4); SU32(rx_sp(), 4, name);
    const rx_addr r = rx_hierarchy_find_node(hierarchy, name);
    ANIM_CALL_FRAME_END(4);
    return r;
}

/* 00834eb0 CharacterAnim_SetAnimation (stdcall: C, motion handle; EAX = loops; RET 8): a new motion slot for the motion (+0x54 of
 * its data = loops): 12 bytes (handle, 0, bone map), the bone map one byte per motion bone (data +0x10 count, +0x14 names of 0x20
 * bytes): the character bone of that name (cache C+0x8c, else the hierarchy's node record index, 0xff none); appended to C's slot
 * list (grown by C+0x10). Returns the slot. */
rx_addr rx_character_anim_set_animation(rx_addr c, rx_addr handle, uint32_t loops) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 0x18);
    const rx_addr name_local = sp0 - 4;
    rx_addr m = U32(handle, 0x44);
    SU32(handle, 0x4c, 1);
    SU32(m, 0x54, loops);
    rx_addr slot = anim_aligned_alloc(0xc);
    if (slot) { SU32(slot, 0, 0); SU32(slot, 4, 0); SU32(slot, 8, 0); }
    SU32(slot, 0, handle); SU32(slot, 4, 0);
    SU32(slot, 8, anim_aligned_alloc(U32(m, 0x10)));
    for (int32_t i = 0; i < I32(m, 0x10); i++) {
        rx_addr name = U32(m, 0x14) + 0x20u * (uint32_t)i;
        SU32(name_local, 0, name);
        if (rx_bone_cache_find(c + 0x8c, name_local, U32(slot, 8) + (uint32_t)i)) continue;
        rx_addr hierarchy = U32(c, 0x18);
        rx_addr rec = hierarchy_find_node(hierarchy, U32(m, 0x14) + 0x20u * (uint32_t)i);
        SU8(U32(slot, 8), i, rec ? (uint8_t)((int32_t)(rec - U32(hierarchy, 8)) >> 4) : 0xff);
        SU32(name_local, 0, U32(m, 0x14) + 0x20u * (uint32_t)i);
        rx_bone_cache_insert(c + 0x8c, name_local, U32(slot, 8) + (uint32_t)i);
    }
    SU32(handle, 0x4c, 0);
    if (!(I32(c, 8) < I32(c, 0xc))) {
        uint32_t cap = U32(c, 0x10) + U32(c, 0xc);
        uint64_t bytes = (uint64_t)cap * 4u;
        rx_addr list = anim_aligned_alloc(bytes > 0xffffffffu ? 0xffffffffu : (uint32_t)bytes);
        anim_memcpy(list, U32(c, 4), U32(c, 0xc) * 4u);
        anim_aligned_free(U32(c, 4));
        SU32(c, 4, list);
        SU32(c, 0xc, cap);
    }
    SU32(U32(c, 4), 4u * U32(c, 8), slot);
    SU32(c, 8, U32(c, 8) + 1);
    rx_set_sp(sp0);
    return slot;
}
RX_FUNCTION(0x00834eb0, rx_character_anim_set_animation)

/* 0082af60 ActionContext::vf0 (scalar deleting destructor; ECX = the object, stack the delete flag; RET 4): the vtable
 * (0x00975e8c) written back; with bit 0 the object freed (0078fc50 ReflexAlignedFree). EAX = the object. */
rx_addr rx_action_context_dtor(rx_addr self, uint32_t flag) {
    const rx_addr entry0 = rx_sp(), E = entry0 - 4u;           /* push esi */
    rx_set_sp(E);
    SU32(self, 0, 0x00975e8c);
    if (flag & 1) rx_reflex_aligned_free(self);
    rx_set_sp(entry0);
    return self;
}
RX_FUNCTION(0x0082af60, rx_action_context_dtor)

/* 00834d80 CharacterAnim_Release (ESI = C): frees every slot (its bone map, then itself), the C+0x1c bone array and the owned
 * helper object C+0x78 (deleting destructor) when C+0x7c says it is owned. */
void rx_character_anim_release(rx_addr c) {
    for (int32_t i = 0; i < I32(c, 8); i++) {
        rx_addr slot = U32(U32(c, 4), 4u * (uint32_t)i);
        anim_aligned_free(U32(slot, 8));
        anim_aligned_free(slot);
    }
    SU32(c, 8, 0);
    if (U32(c, 0x1c)) anim_aligned_free(U32(c, 0x1c));
    SU32(c, 0x1c, 0);
    rx_addr o = U32(c, 0x78);
    if (o && U32(c, 0x7c)) { uint32_t a = 1; (void)rx_vcall(o, 0, 1, &a); }   /* vf0 0082af60: the deleting destructor */
    SU32(c, 0x78, 0);
}
RX_FUNCTION(0x00834d80, rx_character_anim_release)

/* 00835000 CharacterAnim_FindSlot (EDI = motion handle, EAX = C): the slot of C playing the handle, or 0 (the handle's in-use flag
 * +0x4c is set during the search). */
rx_addr rx_character_anim_find_slot(rx_addr handle, rx_addr c) {
    if (handle == 0) return 0;
    SU32(handle, 0x4c, 1);
    for (int32_t i = 0; i < I32(c, 8); i++) {
        rx_addr slot = U32(U32(c, 4), 4u * (uint32_t)i);
        if (U32(slot, 0) == handle) { SU32(handle, 0x4c, 0); return slot; }
    }
    SU32(handle, 0x4c, 0);
    return 0;
}
RX_FUNCTION(0x00835000, rx_character_anim_find_slot)

/* 008351c0 CharacterAnim_CapturePose (stdcall C; RET 4): the bones' current pose captured as the transition source: the capture
 * clock +0x6c = 0; for each of the character's bones (count +0x20, the bone refs +0x18 -> +8, 0x10 bytes each) the bone's local
 * rotation (007dd380: EAX = the bone's node record, ESI = the capture record's quaternion at +0x80) and its world translation row (the
 * node's matrix row +0x30 of the byte-indexed 0x40-byte matrix at +0xb of the bone ref) into the capture record's +0x70; the
 * records are 0xe0 bytes at +0x1c. Frame: push ebp; and esp, -16; sub esp, 0x14; push ebx / esi / edi; the loop's locals at A - 0xc /
 * A - 8 / A - 4 (record offset, bone ref offset, index). */
void rx_character_anim_capture_pose(rx_addr c) {
    ANIM_FRAME_ENTER(A, 0x20);
    SU32(A - 0x18, 0, anim_reg_get(ANIM_EBX));
    SU32(A - 0x1c, 0, anim_reg_get(ANIM_ESI));
    SU32(A - 0x20, 0, anim_reg_get(ANIM_EDI));
    SU32(c, 0x6c, 0);
    SU32(A, -4, 0);                                        /* [esp + 0x1c]: the bone index */
    const int32_t count = I32(c, 0x20);
    if (count > 0) {
        uint32_t record = 0, ref = 0;                      /* [esp + 0x14] / [esp + 0x18] */
        SU32(A, -0xc, 0);
        SU32(A, -8, 0);
        for (int32_t i = 0; i < count; i++) {
            const rx_addr bone = U32(U32(c, 0x18), 8) + ref;
            const rx_addr out = U32(c, 0x1c) + record + 0x80;
            anim_reg_set(ANIM_ESI, out);
            anim_reg_set(ANIM_EDI, bone);
            ANIM_CALL_FRAME(0);
            rx_node_local_rotation(bone, out);             /* 007dd380 */
            ANIM_CALL_FRAME_END(0);
            const rx_addr matrices = U32(U32(bone, 0), 0x18);
            const rx_addr row = matrices + ((uint32_t)U8(bone, 0xb) << 6) + 0x30;
            const rx_addr dst = U32(c, 0x1c) + record + 0x70;
            ref += 0x10;
            SU32(A, -8, ref);
            for (uint32_t k = 0; k < 16; k += 4) SU32(dst, k, U32(row, k));   /* movaps / movups */
            record += 0xe0;
            SU32(A, -4, (uint32_t)(i + 1));
            SU32(A, -0xc, record);
        }
    }
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x008351c0, rx_character_anim_capture_pose)

static void character_capture_pose(rx_addr c) {
    SU32(rx_sp(), -4, c);                                  /* push C */
    ANIM_CALL_FRAME(4);
    rx_character_anim_capture_pose(c);
    ANIM_CALL_FRAME_END(4);
}

/* 00837510 CharacterAnim_Play (ESI = C, xmm1 = transition time, stack slot; RET 4): plays the slot from its start: a transition
 * time of 0 cuts; else a transition from the current slot (C+0x34) over that time (clock +0x44 / start +0x48 = the current time,
 * end +0x4c, length +0x50), capturing the pose first when nothing plays or a transition is already running. */
void rx_character_anim_play(rx_addr c, rx_addr slot, float blend) {
    if (!(blend == 0.0f)) {
        float t = F32(c, 0x3c);
        rx_addr cur = U32(c, 0x2c);
        SF32(c, 0x48, t); SF32(c, 0x44, t);
        SF32(c, 0x4c, t + blend);
        SF32(c, 0x50, blend);
        SU32(c, 0x34, cur);
        if (cur == 0 || U32(c, 0x28) != 0) character_capture_pose(c);
        SU32(c, 0x28, 1);
    } else SU32(c, 0x28, 0);
    SF32(c, 0x3c, 0.0f);
    SU32(c, 0x2c, slot);
    SU32(c, 0x24, 0);
}
RX_FUNCTION(0x00837510, rx_character_anim_play)

/* 00839bb0 CharacterEntity_PlaySlot (EAX = entity, ECX = slot): restarts the character on the slot (no transition) when given. */
void rx_character_entity_play_slot(rx_addr e, rx_addr slot) {
    if (slot == 0) return;
    SU32(e, 0x16c, 0); SU32(e, 0x170, 0); SU32(e, 0x174, slot); SF32(e, 0x184, 0.0f);
}
RX_FUNCTION(0x00839bb0, rx_character_entity_play_slot)

/* 00839be0 CharacterEntity::vf19 (thiscall: dt; RET 4): restarts its looping slot +0x1f4 when the motion finished, then advances
 * and samples the character (Character_SampleKeyframes on +0x148). */
void rx_character_entity_animate(rx_addr e, float dt) {
    if (U32(e, 0x16c) != 0 && U32(e, 0x1f4) != 0) rx_character_entity_play_slot(e, U32(e, 0x1f4));
    if (U32(e, 0x1f4) != 0) {
        rx_set_sp(rx_sp() - 8);                 /* the original's push + call */
        rx_character_sample_keyframes(e + 0x148, dt);
        rx_set_sp(rx_sp() + 8);
    }
}
RX_FUNCTION(0x00839be0, rx_character_entity_animate)

/* 00839c30 CharacterEntity_SetLoopAnimation (ESI = entity, EAX = motion handle, stack loops; RET 4): +0x144 = the handle, +0x1f4 =
 * a new slot for it. */
void rx_character_entity_set_loop_animation(rx_addr e, rx_addr handle, uint32_t loops) {
    SU32(e, 0x144, handle);
    rx_set_sp(rx_sp() - 0xc);
    rx_addr slot = rx_character_anim_set_animation(e + 0x148, handle, loops);
    rx_set_sp(rx_sp() + 0xc);
    SU32(e, 0x1f4, slot);
}
RX_FUNCTION(0x00839c30, rx_character_entity_set_loop_animation)

/* 00839b00 CharacterEntity::vf9 (thiscall): releases the character animation, then ModelEntity::vf9 (tail call). */
void rx_character_entity_release(rx_addr e) {
    rx_character_anim_release(e + 0x148);
#ifdef RX_ORACLE
    /* ModelEntity::vf9 keeps its callee-saved registers in the fx unit's register model, which only the fx adapters bind in the oracle */
    rx_call_original(0x00401ea0, RX_THISCALL, e, 0, 0);
#else
    rx_fx_model_entity_vf9(e);
#endif
}
RX_FUNCTION(0x00839b00, rx_character_entity_release)

/* 0083d480: an empty animator slot (thiscall, 3 arguments; RET 0xc). */
void rx_animator_nop3(rx_addr self, rx_addr a, rx_addr b, rx_addr c) { (void)self; (void)a; (void)b; (void)c; }
RX_FUNCTION(0x0083d480, rx_animator_nop3)
