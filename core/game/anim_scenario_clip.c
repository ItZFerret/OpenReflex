/* anim_scenario_clip.c -- the highlight clip recorder (MXClipRecorder, 0x006aa7b0..0x006ac0df; _re/notes/replay.md).
 * Globals: 0xa942a4 recording start, 0xa942a8 / 0xa942ac snapshot timers, 0xa942b0 ring cursor (150 samples), 0xa942b4 state
 * (1 recording), 0xa942b8 the race activity A (+0x268 riders, +0x26c count, +0x27c second rider table, +0x294 race clock),
 * 0xa942bc the current sample's time slot, 0xa942c0 time since the last clip, 0xa942c4 the last snapshot's section, 0xa942c8
 * event log cursor (100 x 0x80 at 0xd2f160: time, kind, rider, record ...), 0xa942d8 records per clip section, 0xa942dc the ring
 * length, 0xa942e0 the ring write section, 0xa7f124 the sample buffer (0x50-byte records), 0xa7f128 sample times (150, -1 empty),
 * 0xa7f3d8 sample sections (150, -1 none), 0xa7f3a8 the riders' places (w-rc-core: 12..31 in rider_cap.h's side storage), 0xa7f630 the clip playing order (12), 0xa959a0 the clip
 * candidate (start, end, type, rider), 0xa959a8 its type (0 none), 0xa959ac its rider; 12 clip slots at 0xd323c0 + i * 0x35d0
 * (+0 rider, +4 activity, +8 type, +0xc copy, +0x39c section, +0x3a0 sample count, +0x3b0 events, +0x35b0 start, +0x35b4 end). */
#include "anim_scenario.h"
#include "rider_cap.h"
#include "anim_extern.h"
#include "crt_util.h"
#include "build_activity_util.h"
#include "race_activity.h"
#include "world_wreck.h"
#include "world_misc.h"

#define CLIP_SLOT(i) (0x00d323c0u + 0x35d0u * (uint32_t)(i))
static rx_addr activity(void) { return U32(0, 0x00a942b8); }

/* A direct call at the original call site: the callee enters with its `n` stack arguments (and the return-address slot the
 * replaced call left) below the caller's stack pointer. Returns the caller's SP to restore after the call. */
static rx_addr clip_callsite(uint32_t n, const uint32_t* a) {
    const rx_addr sp0 = rx_sp(), sp = sp0 - 4u * n - 4u;
    SU32(sp, 0, 0);
    for (uint32_t i = 0; i < n; i++) SU32(sp + 4 + 4u * i, 0, a[i]);
    rx_set_sp(sp);
    return sp0;
}

/* 006aa750 ClipRecorder_ClearClips: no playing order, every slot empty (type 0). */
void rx_clip_recorder_clear_clips(void) {
    for (uint32_t i = 0; i < 12; i++) SU32(0, 0x00a7f630 + 4 * i, 0xffffffffu);
    for (rx_addr t = 0x00d323c8; t < 0x00d5a988; t += 0x35d0) SU32(t, 0, 0);
}
RX_FUNCTION(0x006aa750, rx_clip_recorder_clear_clips)

/* 006ab530 ClipRecorder_RecordPlaces: every rider's place (+0x78) noted. */
void rx_clip_recorder_record_places(void) {
    rx_addr a = activity();
    for (int32_t i = 0; i < I32(a, 0x26c); i++) SU32(rx_rc_tab(0x00a7f3a8, 4, (uint32_t)i), 0, U32(U32(U32(a, 0x268), 4u * (uint32_t)i), 0x78));
}
RX_FUNCTION(0x006ab530, rx_clip_recorder_record_places)

/* 006abcb0 ClipRecorder_Snapshot: every rider's 0x50-byte replay record (vehicle +0x12c0: two vec4, eight x87-copied floats, the
 * flags +0x1300; a teleport flag 0x40000 clears the simulation's +0x2e98) into the write section of the ring; advances it.
 * Returns the section written. */
uint32_t rx_clip_recorder_snapshot(void) {
    uint32_t sec = U32(0, 0x00a942e0);
    rx_addr a = activity();
    rx_addr dst = U32(0, 0x00a7f124) + 0x50u * sec;
    for (int32_t i = 0; i < I32(a, 0x26c); i++) {
        if (i >= (int32_t)RX_RC_ORIG) dst = rx_clip_record(rx_clip_index(0, (uint32_t)i, sec));   /* (w-rc-core) riders 12..31: the side buffer */
        rx_addr v = U32(U32(U32(U32(a, 0x268), 4u * (uint32_t)i), 0x74), 0x1ac);
        rx_st4(dst, rx_ld4(v + 0x12c0));
        rx_st4(dst + 0x10, rx_ld4(v + 0x12d0));
        for (uint32_t k = 0; k < 8; k++) anim_x87_copy_f32(dst + 0x20 + 4 * k, v + 0x12e0 + 4 * k);
        uint32_t flags = U32(v, 0x1300);
        SU32(dst, 0x40, flags);
        if (flags & 0x40000) SU32(U32(v, 0x234), 0x2e98, 0);
        dst += 0x50u * U32(0, 0x00a942dc);
    }
    uint32_t cur = U32(0, 0x00a942e0);
    SU32(0, 0x00a942e0, (int32_t)cur >= (int32_t)(U32(0, 0x00a942dc) - 1) ? 0 : cur + 1);
    return sec;
}
RX_FUNCTION(0x006abcb0, rx_clip_recorder_snapshot)

/* 006aa7b0 ClipRecorder_StartRecording (stdcall: activity; RET 4): state 1, all samples / sections / events empty, places noted,
 * the first sample taken at the race clock. */
void rx_clip_recorder_start_recording(rx_addr a) {
    float t = F32(a, 0x294);
    for (uint32_t i = 0; i < 0x96; i++) SU32(0, 0x00a7f128 + 4 * i, 0xbf800000u);
    SU32(0, 0x00a942b8, a);
    SF32(0, 0x00a942a4, t);
    SU32(0, 0x00a942b4, 1);
    SU32(0, 0x009c4070, 0xffffffffu); SU32(0, 0x009c4074, 0xffffffffu);
    SF32(0, 0x00a942c0, 0.0f);
    for (uint32_t i = 0; i < 0x96; i++) SU32(0, 0x00a7f3d8 + 4 * i, 0xffffffffu);
    rx_clip_recorder_clear_clips();
    SU32(0, 0x00a959a8, 0);
    for (rx_addr e = 0x00d2f160; e < 0x00d32360; e += 0x80) SF32(e, 0, -1.0f);
    rx_clip_recorder_record_places();
    SF32(0, 0x00a7f128, F32(a, 0x294));
    SU32(0, 0x00a942bc, 0x00a7f128);
    uint32_t sec = rx_clip_recorder_snapshot();
    const uint32_t zeros[] = {0x00a7f384, 0x00a7f388, 0x00a7f38c, 0x00a7f390, 0x00a7f398, 0x00a7f39c, 0x00a7f3a0, 0x00a7f3a4};
    for (uint32_t i = 0; i < 8; i++) SU32(0, zeros[i], 0);
    SU32(0, 0x00a7f3d8, sec);
    SU32(0, 0x00a942c4, sec);
    SU32(0, 0x00a942b0, 1);
    SF32(0, 0x00a942a8, 0.0f); SF32(0, 0x00a942ac, 0.0f); SF32(0, 0x00a942cc, 0.0f); SF32(0, 0x00a942d0, 0.0f);
}
RX_FUNCTION(0x006aa7b0, rx_clip_recorder_start_recording)

/* 006aacf0 / 006aaca0 (not in the list): a clip candidate (ECX = type, EDX = rider; the first with a time window on the stack). */
static void clip_candidate(uint32_t type, rx_addr rider, float t0, float t1) {
    const uint32_t a[2] = {rx_fb(t0), rx_fb(t1)};
    const rx_addr sp0 = clip_callsite(2, a);
    rx_world_clip_recorder_candidate(type, rider, t0, t1);
    rx_set_sp(sp0);
}
static void clip_candidate_of_type(uint32_t type, rx_addr rider) {
    const rx_addr sp0 = clip_callsite(0, 0);
    rx_world_clip_recorder_candidate_of_type(type, rider);
    rx_set_sp(sp0);
}

/* 006ab5b0 ClipRecorder_PlaceClips: after the first 7 seconds, a rider (of a live, unfinished state, not the candidate's) who lost
 * a place gets a type-3 candidate (4 s before to 3.8 s after, when ClipRecorderEnabled [0xd5a9a4]), one who gained a place a
 * type-2 one; the places are noted. Before 7 s only the places. Nothing while the race state flags 0xa7f388 / 0xa7f38c are 0. */
void rx_clip_recorder_place_clips(void) {
    if (U32(0, 0x00a7f388) == 0 && U32(0, 0x00a7f38c) == 0) return;
    rx_addr a = activity();
    int32_t n = I32(a, 0x26c);
    if (!(F32(a, 0x294) > 7.0f /* 0x930004 */)) {
        for (int32_t i = 0; i < n; i++) SU32(rx_rc_tab(0x00a7f3a8, 4, (uint32_t)i), 0, U32(U32(U32(a, 0x268), 4u * (uint32_t)i), 0x78));
        return;
    }
    for (int32_t i = 0; i < n; i++) {
        rx_addr r = U32(U32(a, 0x268), 4u * (uint32_t)i), st = U32(r, 0x74);
        if (U32(st, 4) == 0 || U32(st, 0x10) != 0 || U32(0, 0x00a959ac) == r) continue;
        uint32_t prev = U32(rx_rc_tab(0x00a7f3a8, 4, (uint32_t)i), 0), cur = U32(r, 0x78);
        if (cur < prev) {
            if (U32(0, 0x00d5a9a4) != 0) {
                float t = F32(a, 0x294);
                clip_candidate(3, r, t - 4.0f /* 0x9c40c0 */, t + 3.8f /* 0x9c40c4 */);
                a = activity();
            }
            SU32(rx_rc_tab(0x00a7f3a8, 4, (uint32_t)i), 0, U32(r, 0x78));
        } else if (cur > prev) {
            clip_candidate_of_type(2, r);
            SU32(rx_rc_tab(0x00a7f3a8, 4, (uint32_t)i), 0, U32(r, 0x78));
            a = activity();
        }
    }
}
RX_FUNCTION(0x006ab5b0, rx_clip_recorder_place_clips)

/* 006abae0 ClipRecorder_ClipAt (xmm0 = time): the used slot whose [start, end] holds the time, or 0. */
rx_addr rx_clip_recorder_clip_at(float t) {
    for (uint32_t i = 0; i < 12; i++) {
        rx_addr s = CLIP_SLOT(i);
        if (U32(s, 8) != 0 && t >= F32(s, 0x35b0) && F32(s, 0x35b4) >= t) return s;
    }
    return 0;
}
RX_FUNCTION(0x006abae0, rx_clip_recorder_clip_at)

/* 006abb30 ClipRecorder_OrderClip (EDX = slot index): the slot moves to the end of the playing order (an unused order entry at its
 * own index just takes it). */
void rx_clip_recorder_order_clip(uint32_t idx) {
    rx_addr order = 0x00a7f630;
    if (U32(order, 4 * idx) == 0xffffffffu) { SU32(order, 4 * idx, idx); return; }
    uint32_t j = 0;
    while (j < 12 && U32(order, 4 * j) != idx) j++;
    if ((int32_t)j < 11) for (uint32_t k = j; k < 11; k++) SU32(order, 4 * k, U32(order, 4 * k + 4));
    SU32(order, 44, idx);
}
RX_FUNCTION(0x006abb30, rx_clip_recorder_order_clip)

/* The game's random generator (seed 0x9cb860, calls 0x9cb864): 15 bits. */
static uint32_t game_rand15(void) {
    uint32_t seed = U32(0, 0x009cb860) * 0x41c64e6du + 0x3039u;
    SU32(0, 0x009cb860, seed);
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
    return (seed >> 16) & 0x7fffu;
}

/* 006aadc0 ClipRecorder_FindSlot (cdecl: type): a free slot, else one of a lower type (not a start clip, 8), else a random one of
 * the same type (the game's rand); it moves to the end of the playing order. 0 when none. */
rx_addr rx_clip_recorder_find_slot(int32_t type) {
    const rx_addr sp0 = rx_sp(), list = sp0 - 0x30;            /* local array of candidate indices */
    rx_set_sp(sp0 - 0x38);
    rx_addr slot = 0;
    int32_t idx = -1;
    for (uint32_t i = 0; i < 12; i++) if (U32(CLIP_SLOT(i), 8) == 0) { slot = CLIP_SLOT(i); idx = (int32_t)i; break; }
    if (!slot)
        for (uint32_t i = 0; i < 12; i++) {
            int32_t t = I32(CLIP_SLOT(i), 8);
            if (type > t && t != 8) { slot = CLIP_SLOT(i); idx = (int32_t)i; break; }
        }
    if (idx >= 0) rx_clip_recorder_order_clip((uint32_t)idx);
    if (!slot) {
        uint32_t k = 0;
        for (uint32_t i = 0; i < 12; i++) if (type == I32(CLIP_SLOT(i), 8)) SU32(list, 4 * k++, i);
        if (k == 0) { rx_set_sp(sp0); return 0; }
        uint32_t r = game_rand15();
        float f = (float)((double)(float)r * -3.0517578125e-05 /* 0x977708 */) * (float)k;
        int32_t pick = rx_cvtt(f);
        uint32_t i = U32(list - 4u * (uint32_t)pick, 0);
        slot = CLIP_SLOT(i);
        rx_clip_recorder_order_clip(i);
    }
    rx_set_sp(sp0);
    return slot;
}
RX_FUNCTION(0x006aadc0, rx_clip_recorder_find_slot)

/* 006ab160 ClipRecorder_CopyRecord (ECX = source, EAX = destination): a 0x4c-byte rider record (the vectors through the x87). */
void rx_clip_recorder_copy_record(rx_addr src, rx_addr dst) {
    SU32(dst, 0, U32(src, 0)); SU32(dst, 4, U32(src, 4));
    const uint32_t f[] = {0x10, 0x14, 0x18, 0x20, 0x24, 0x28, 0x30, 0x34, 0x38};
    for (uint32_t i = 0; i < 9; i++) anim_x87_copy_f32(dst + f[i], src + f[i]);
    SU32(dst, 0x40, U32(src, 0x40)); SU32(dst, 0x44, U32(src, 0x44)); SU32(dst, 0x48, U32(src, 0x48));
}
RX_FUNCTION(0x006ab160, rx_clip_recorder_copy_record)

/* 006ab110 ClipRecorder_CopyEvent (EAX = source, EDI = destination): an event-log entry (time, kind, rider, the rider record,
 * 0x15 bytes of name at +0x60). Returns the destination. */
rx_addr rx_clip_recorder_copy_event(rx_addr src, rx_addr dst) {
    anim_x87_copy_f32(dst, src);
    SU32(dst, 4, U32(src, 4)); SU32(dst, 8, U32(src, 8));
    rx_clip_recorder_copy_record(src + 0x10, dst + 0x10);
    for (int k = 0; k < 3; k++) SU32(dst, 0x60, U32(src, 0x60));
    for (uint32_t i = 0; i < 0x15; i++) SU8(dst, 0x60 + i, U8(src, 0x60 + i));
    return dst;
}
RX_FUNCTION(0x006ab110, rx_clip_recorder_copy_event)

/* 006ac050 ClipRecorder_CopySamples (EDX = riders; cdecl: first ring section, clip section): each rider's ring (from `first`,
 * wrapped) copied into the clip's sections. */
void rx_clip_recorder_copy_samples(int32_t riders, uint32_t first, uint32_t section) {
    if (riders <= 0) return;
    /* the original's frame: sub esp, 8; push ebx, ebp, esi, edi (the caller's); locals: source cursor, riders left. Its memcpy
     * calls leave their arguments, return addresses and saved registers in the dead stack, which a particle template built
     * later at the same depth picks up (crt_memcpy writes memcpy's own frame from crt_caller_regs). */
    const rx_addr sp0 = rx_sp(), base = sp0 - 0x18;
    SU32(sp0 - 0xc, 0, anim_reg_get(ANIM_EBX)); SU32(sp0 - 0x10, 0, anim_reg_get(ANIM_EBP));
    SU32(sp0 - 0x14, 0, anim_reg_get(ANIM_ESI)); SU32(sp0 - 0x18, 0, anim_reg_get(ANIM_EDI));
    rx_set_sp(base);
    uint32_t ring = U32(0, 0x00a942dc);
    uint32_t tail = (ring - first) * 0x50u, head = first * 0x50u, stride = ring * 0x50u;
    rx_addr src = U32(0, 0x00a7f124), dst = src + U32(0, 0x00a942d8) * section * 0x50u;
    const uint32_t saved[4] = {crt_caller_regs[3], crt_caller_regs[5], crt_caller_regs[6], crt_caller_regs[7]};
    crt_caller_regs[3] = tail; crt_caller_regs[5] = stride; crt_caller_regs[7] = head;   /* EBX EBP EDI at the calls */
    for (int32_t i = 0; i < riders; i++) {
        if (i >= (int32_t)RX_RC_ORIG) {   /* (w-rc-core) riders 12..31: their ring and clip sections in the side buffer */
            src = rx_clip_record(rx_clip_index(0, (uint32_t)i, 0));
            dst = rx_clip_record(rx_clip_index(section, (uint32_t)i, 0));
        }
        SU32(base, 0x10, src); SU32(base, 0x14, (uint32_t)(riders - i));
        crt_caller_regs[6] = dst;                                                          /* ESI */
        SU32(base, -4, tail); SU32(base, -8, src + head); SU32(base, -0xc, dst); SU32(base, -0x10, 0x006ac0ae);
        crt_memcpy(dst, src + head, tail);
        if ((int32_t)first > 0) {
            SU32(base, -4, head); SU32(base, -8, src); SU32(base, -0xc, dst + tail); SU32(base, -0x10, 0x006ac0c7);
            crt_memcpy(dst + tail, src, head);
        }
        src += stride; dst += stride;
    }
    crt_caller_regs[3] = saved[0]; crt_caller_regs[5] = saved[1]; crt_caller_regs[6] = saved[2]; crt_caller_regs[7] = saved[3];
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006ac050, rx_clip_recorder_copy_samples)

static uint32_t ring_prev(uint32_t i) { return i == 0 ? 0x95 : i - 1; }
static uint32_t ring_next(uint32_t i) { return (int32_t)i >= 0x95 ? 0 : i + 1; }

/* 006aaf20 ClipRecorder_SaveClip (cdecl: candidate, slot): the candidate's window found in the sample ring (back from the cursor
 * to the last sample at or before its start, then forward to the first one with a section), the riders' samples copied from
 * there, the logged events inside the window copied, and the slot filled (type, start, end, activity, rider, sample count). */
void rx_clip_recorder_save_clip(rx_addr cand, rx_addr slot) {
    rx_addr a = activity();
    int32_t riders = I32(a, 0x26c);
    for (uint32_t i = 0; i < 0x390; i += 4) SU32(slot + 0xc, i, U32(cand + 0x10, i));
    for (uint32_t k = RX_RC_ORIG; k < RX_RC_MAX && rx_rc_act(cand + 0x10, 0); k++) {   /* (w-rc-core) the snapshot's entries 12..31 */
        const rx_addr s = rx_clip_entry(cand + 0x10, k, 0), d = rx_clip_entry(slot + 0xc, k, 1);
        for (uint32_t i = 0; i < 0x4c; i += 4) SU32(d, i, U32(s, i));
    }
    uint32_t cur = U32(0, 0x00a942b0);
    uint32_t i = cur == 0 ? 0x95 : cur - 1;
    if (i == cur) return;
    float t0 = F32(cand, 0);
    while (F32(0, 0x00a7f128 + 4 * i) > t0) {
        i = ring_prev(i);
        if (i == cur) return;
    }
    if (i == cur) return;
    i = ring_next(i);
    while (U32(0, 0x00a7f3d8 + 4 * i) == 0xffffffffu) {
        if (i == cur) return;
        i = ring_next(i);
    }
    if (i == cur) return;
    if (F32(0, 0x00a7f128 + 4 * i) >= t0)                        /* the window's end (no effect: the original walks it) */
        for (uint32_t j = i; F32(cand, 4) >= F32(0, 0x00a7f128 + 4 * j);) {
            j = ring_next(j);
            if (!(F32(0, 0x00a7f128 + 4 * j) >= t0)) break;
        }
    uint32_t first = U32(0, 0x00a7f3d8 + 4 * i), last = U32(0, 0x00a942c4);
    uint32_t count = (int32_t)first < (int32_t)last ? last - first : U32(0, 0x00a942dc) - first + last;
    {   /* as the original calls it: EBX = slot, EBP = the sample count, ESI = 4 i, EDI = slot + 0x39c (after its rep movsd);
         * the two arguments and the return address on the stack below its four saved registers */
        const rx_addr sp0 = rx_sp();
        SU32(sp0 - 4, 0, anim_reg_get(ANIM_EBX)); SU32(sp0 - 8, 0, anim_reg_get(ANIM_EBP));
        SU32(sp0 - 0xc, 0, anim_reg_get(ANIM_ESI)); SU32(sp0 - 0x10, 0, anim_reg_get(ANIM_EDI));
        const uint32_t r3 = anim_reg_get(ANIM_EBX), r5 = anim_reg_get(ANIM_EBP), r6 = anim_reg_get(ANIM_ESI), r7 = anim_reg_get(ANIM_EDI);
        anim_reg_set(ANIM_EBX, slot); anim_reg_set(ANIM_EBP, count); anim_reg_set(ANIM_ESI, 4 * i); anim_reg_set(ANIM_EDI, slot + 0x39c);
        rx_set_sp(sp0 - 0x10);
        ANIM_PUSH(U32(slot, 0x39c)); ANIM_PUSH(first); ANIM_PUSH(0x006ab049);
        rx_clip_recorder_copy_samples(riders, first, U32(slot, 0x39c));
        rx_set_sp(sp0);
        anim_reg_set(ANIM_EBX, r3); anim_reg_set(ANIM_EBP, r5); anim_reg_set(ANIM_ESI, r6); anim_reg_set(ANIM_EDI, r7);
    }
    for (uint32_t k = 0; k < 100; k++) SF32(slot + 0x3b0 + 0x80 * k, 0, -1.0f);
    uint32_t e = U32(0, 0x00a942c8);
    rx_addr dst = slot + 0x3b0;
    for (uint32_t k = 0; k < 100; k++) {
        rx_addr ev = 0x00d2f160 + (e << 7);
        float t = F32(ev, 0);
        if (t >= F32(cand, 0) && F32(cand, 4) >= t) { rx_clip_recorder_copy_event(ev, dst); dst += 0x80; }
        e = (uint32_t)((int32_t)(e + 1) % 100);
    }
    SU32(slot, 8, U32(cand, 8));
    anim_x87_copy_f32(slot + 0x35b0, cand);
    SF32(slot, 0x35b4, F32(cand, 4));
    SU32(slot, 4, U32(0, 0x00a942b8));
    SU32(slot, 0, U32(cand, 0xc));
    SU32(slot, 0x3a0, count);
    SF32(0, 0x00a942c0, 0.0f);
}
RX_FUNCTION(0x006aaf20, rx_clip_recorder_save_clip)

/* 006ab220 ClipRecorder_CommitCandidate: when the clock passed the candidate's end (and the last snapshot has a section), the
 * candidate is saved in a slot (unless a clip already covers its start); the candidate is cleared. */
void rx_clip_recorder_commit_candidate(void) {
    const rx_addr sp0 = rx_sp();
    SU32(sp0 - 4, 0, anim_reg_get(ANIM_ESI));                 /* push esi (then ESI = the candidate's type) */
    rx_set_sp(sp0 - 4);
    uint32_t type = U32(0, 0x00a959a8);
    rx_addr p = U32(0, 0x00a942bc);
    if (type != 0 && p != 0 && F32(p, 0) >= F32(0, 0x00a959a4) && U32(0, 0x00a942c4) != 0xffffffffu) {
        ANIM_PUSH(0x006ab257); rx_addr covered = rx_clip_recorder_clip_at(F32(0, 0x00a959a0)); rx_set_sp(rx_sp() + 4);
        if (covered == 0) {
            ANIM_PUSH(type); ANIM_PUSH(0x006ab261);
            rx_addr slot = rx_clip_recorder_find_slot((int32_t)type);
            rx_set_sp(rx_sp() + 8);
            if (slot) {
                const uint32_t esi = anim_reg_get(ANIM_ESI);
                anim_reg_set(ANIM_ESI, type);
                ANIM_PUSH(slot); ANIM_PUSH(0x00a959a0); ANIM_PUSH(0x006ab273);
                rx_clip_recorder_save_clip(0x00a959a0, slot);
                rx_set_sp(rx_sp() + 0xc);
                anim_reg_set(ANIM_ESI, esi);
            }
        }
        SU32(0, 0x00a959ac, 0);
        SU32(0, 0x00a959a8, 0);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006ab220, rx_clip_recorder_commit_candidate)

/* 006ab560 ClipRecorder_FillerClip (cdecl): no candidate clip yet (0xa959a8 == 0) and the first human driver (+0x74 player,
 * active, local, not a mode-2 one) on the bike: a type 1 candidate around the race clock (006aaca0). Frame: push ecx. */
void rx_clip_recorder_filler_clip(void) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);                                       /* push ecx (the original's dead scratch slot) */
    SU32(sp0 - 4, 0, 0);
    if (U32(0, 0x00a959a8) == 0) {
        rx_addr drv;
        ACT_CALLR(drv, 2, rx_activity_driver_of_kind(U32(0, 0x00a942b8), 0, 0));   /* 0055a000 (EAX = the activity, args 0, 0) */
        if (drv != 0) {
            const rx_addr pl = U32(drv, 0x74);
            if (pl != 0 && U32(pl, 4) != 0 && U32(pl, 0x10) == 0 && U32(pl, 0x908) == 0) {
                const rx_addr s = rx_sp();
                rx_set_sp(s - 4);
                SU32(s - 4, 0, 0);
                rx_world_clip_recorder_candidate_of_type(1, drv);   /* 006aaca0 (ECX = 1, EDX = the driver) */
                rx_set_sp(s);
            }
        }
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006ab560, rx_clip_recorder_filler_clip)

/* 006aab70 ClipRecorder_Record (cdecl: dt): every 1/15 s a snapshot into the ring (or, on the second timer, an empty sample),
 * the place clips, a filler clip after 10 s without one (006ab560), and the candidate committed after a sample. */
void rx_clip_recorder_record(float dt) {
    const rx_addr sp0 = rx_sp();                              /* push ecx; push esi (ESI then = "sampled") */
    SU32(sp0 - 4, 0, anim_reg_get(ANIM_ECX)); SU32(sp0 - 8, 0, anim_reg_get(ANIM_ESI));
    rx_set_sp(sp0 - 8);
    const float step = 0.06666667f /* 0x977950 */;
    float t1 = F32(0, 0x00a942a8) + dt, t2 = F32(0, 0x00a942ac) + dt;
    SF32(0, 0x00a942a8, t1); SF32(0, 0x00a942ac, t2);
    int sampled = 0;
    if (t1 >= step) {
        uint32_t i = U32(0, 0x00a942b0);
        rx_addr slot = 0x00a7f128 + 4 * i;
        anim_x87_copy_f32(slot, activity() + 0x294);
        SU32(0, 0x00a942bc, slot);
        ANIM_PUSH(0x006aabd4); uint32_t sec = rx_clip_recorder_snapshot(); rx_set_sp(rx_sp() + 4);
        SU32(0, 0x00a7f3d8 + 4 * i, sec);
        SU32(0, 0x00a942c4, sec);
        SU32(0, 0x00a942b0, (int32_t)i >= 0x95 ? 0 : i + 1);
        SF32(0, 0x00a942a8, 0.0f);
        sampled = 1;
    } else if (t2 >= step) {
        uint32_t i = U32(0, 0x00a942b0);
        rx_addr slot = 0x00a7f128 + 4 * i;
        anim_x87_copy_f32(slot, activity() + 0x294);
        SU32(0, 0x00a942bc, slot);
        SU32(0, 0x00a942c4, 0xffffffffu);
        SU32(0, 0x00a7f3d8 + 4 * i, 0xffffffffu);
        SU32(0, 0x00a942b0, (int32_t)i >= 0x95 ? 0 : i + 1);
        sampled = 1;
    }
    if (sampled) SF32(0, 0x00a942ac, 0.0f);
    ANIM_PUSH(0x006aac67); rx_clip_recorder_place_clips(); rx_set_sp(rx_sp() + 4);
    float since = F32(0, 0x00a942c0) + dt;
    SF32(0, 0x00a942c0, since);
    if (since > 10.0f /* 0x92fd88 */) {
        ANIM_PUSH(0x006aac8b); rx_clip_recorder_filler_clip(); rx_set_sp(rx_sp() + 4);
    }
    if (sampled) {
        const uint32_t esi = anim_reg_get(ANIM_ESI);
        anim_reg_set(ANIM_ESI, 1);
        ANIM_PUSH(0x006aac94); rx_clip_recorder_commit_candidate(); rx_set_sp(rx_sp() + 4);
        anim_reg_set(ANIM_ESI, esi);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006aab70, rx_clip_recorder_record)

/* 0055a050 (not in the list): ECX = activity, ESI = rider state: the rider's index. */
static uint32_t rider_index(rx_addr a, rx_addr state) {
    const rx_addr sp0 = clip_callsite(0, 0);
    const uint32_t r = (uint32_t)rx_world_group_rider_index(a, state);
    rx_set_sp(sp0);
    return r;
}

/* 006ab1c0 ClipRecorder_RiderReset (EAX = rider state S): a rider who is the candidate's (by index in the rider table +0x268, or
 * the second one +0x27c when S+0x908) pulls the candidate's end to now - 0.25. */
void rx_clip_recorder_rider_reset(rx_addr s) {
    rx_addr a = activity();
    uint32_t idx = rider_index(a, s);
    rx_addr r = U32(U32(a, U32(s, 0x908) ? 0x27c : 0x268), 4 * idx);
    if (U32(0, 0x00a959a8) != 0 && r == U32(0, 0x00a959ac)) SF32(0, 0x00a959a4, F32(a, 0x294) - 0.25f /* 0x92ff84 */);
}
RX_FUNCTION(0x006ab1c0, rx_clip_recorder_rider_reset)

/* 006ab800 ClipRecorder_LogReset (EAX = rider state S, stack rider record; RET 4): event kind 5 (reset) with the time, the rider and
 * the record (a live, unfinished rider first pulls the candidate in, 006ab1c0). */
void rx_clip_recorder_log_reset(rx_addr s, rx_addr rec) {
    if (U32(s, 4) != 0 && U32(s, 0x10) == 0) rx_clip_recorder_rider_reset(s);
    uint32_t k = U32(0, 0x00a942c8);
    rx_addr e = 0x00d2f160 + (k << 7);
    SU32(e, 4, 5);
    anim_x87_copy_f32(e, U32(s, 0x94) + 0x294);
    rx_addr a = U32(s, 0x94);
    uint32_t idx = rider_index(a, s);
    SU32(e, 8, U32(U32(a, U32(s, 0x908) ? 0x27c : 0x268), 4 * idx));
    rx_clip_recorder_copy_record(rec, e + 0x10);
    SU32(0, 0x00a942c8, (uint32_t)((int32_t)(k + 1) % 100));
}
RX_FUNCTION(0x006ab800, rx_clip_recorder_log_reset)

/* 006aba80 ClipRecorder_LogEffect (EDX = effect name): while recording, event kind 8 with the time and the name ("%s", 0x15 bytes). */
void rx_clip_recorder_log_effect(rx_addr name) {
    if (U32(0, 0x00a942b4) != 1) return;
    uint32_t k = U32(0, 0x00a942c8);
    rx_addr e = 0x00d2f160 + (k << 7);
    SU32(e, 4, 8);
    anim_x87_copy_f32(e, activity() + 0x294);
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 8);                                       /* push esi, edi */
    const rx_addr sp = sp0 - 8 - 8 - 4;
    SU32(sp, 0, 0); SU32(sp + 4, 0, 0x009419e4); SU32(sp + 8, 0, name);
    rx_set_sp(sp);
    rx_format_buffer(e + 0x60, 0x15, 0x009419e4, sp + 8);
    rx_set_sp(sp0);
    SU32(0, 0x00a942c8, (uint32_t)((int32_t)(U32(0, 0x00a942c8) + 1) % 100));
}
RX_FUNCTION(0x006aba80, rx_clip_recorder_log_effect)
