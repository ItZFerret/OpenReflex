/* anim_rider_play.c -- the rider's trick / crash animation playback (2026-10-21, agent "trickplayback").
 *
 * 0061d8b0 (rx_world_rider_update_blends) calls the crash/trick pair every frame while the rider's +0xaf10 is set: the crash
 * state step 00621f40 and the clip-time / chain-event update 00620a60. The update drives the rider's trick:
 *   - the animation object (the rider's +0xaf14, a PerformanceAnimator) has its time +0x98 advanced by dt into +0x46c (the
 *     listeners +0x46c, clamped to [+0x464, +0x468]); the clip blocks' ranges are tested by 0083e170 (a looping block contains
 *     the time), 0083e1d0 (a plain block starts there) and 0083e230 (a plain block ends there: start + cropFront .. end - cropBack);
 *   - 0083e0e0 picks the block at the animator's time into +0x84 / +0x80 (its normalized position);
 *   - in rider state 3 the block's end goes to the +0x48c listeners (the tracker's completion progress; 00622bc0 set their range
 *     to [0, the perf end]), and past the clip's last block end the next clip (+0xaf18) is switched in, its block end / events
 *     played;
 *   - the clip events (00620fc0) between the old and the new time play for the current clip (+0xaf14), and - with a scripted
 *     pose clip +0xc928 - the pose override's blend / pose go to the bike simulation.
 * 00621f40 steps the crash wind-up (+0x5b2d4): blends at 1 -> 2, then 00622160 (to state 2, the crash layer) or 00622460 (to
 * state 1, the air layer) named by the trick record's +0x20 animation, with its perf asset end / clip length and the +0xaf34
 * wind-up flag. 006cd240 is the first looping block's effective end (end - cropBack) or 0.
 *
 * PerformanceAnimator (the clip object the rider's +0xaf14 / +0xaf18 point at): +0x70 the performance asset (+0x18 the 0x70-byte
 * AnimBlock records), +0x74 the block pointers, +0x7c the blend, +0x80 the block position, +0x84 the block pointer, +0x8c the
 * block count, +0x98 the time. AnimBlock: +0x40 start, +0x44 duration, +0x4c cropFront, +0x50 cropBack, +0x54 end,
 * +0x64 additive, +0x68 override, +0x6c looping (build_perf.h).
 *
 * x87: the composite scalars (the clip time, the block ends) pass through fld / fstp in the original, so they use
 * world_x87_pass; the arithmetic itself is SSE. notes/fx.md. */
#include "rx_game.h"
#include "anim_rider_play.h"
#include "anim_rider_stunt.h"
#include "fx_stunt_table.h"
#include "world_rider.h"
#include "world_wreck.h"
#include "world_misc.h"
#include "world_util.h"
#include "phys_quat.h"
#include "bike_sim.h"
#include "build_rider_misc.h"
#include "crt.h"
#include "fx.h"
#include "fx_util.h"

/* the rider animation's ranged clamp as its comiss sequences write it: t = [ra + hi] limited to <= 0, then raised to
 * [ra + lo] (identical to anim_rider_stunt.c's helper). */
static float range_clamp(rx_addr ra, uint32_t hi_off, uint32_t lo_off) {
    float t = F32(ra, hi_off);
    if (!(0.0f >= t)) t = 0.0f;
    const float lo = F32(ra, lo_off);
    if (lo >= t) t = lo;
    return t;
}

/* ---- 0083e170 (EAX = the animator, xmm0 = the time): 1 when a looping block contains the time (strictly inside its
 * start + cropFront .. end - cropBack, not additive / override). Frame: none. ---- */
uint32_t rx_rider_clip_looping_at(rx_addr anim, float t) {
    const int32_t n = I32(anim, 0x8c);
    if (n <= 0) return 0;
    rx_addr b = U32(U32(anim, 0x70), 0x18);
    for (int32_t i = 0; i < n; i++, b += 0x70) {
        const float lo = rx_adds(F32(b, 0x4c), F32(b, 0x40));
        const float hi = rx_subs(F32(b, 0x54), F32(b, 0x50));
        if (!(t > lo)) continue;
        if (!(hi > t)) continue;
        if (U32(b, 0x64) != 0) continue;
        if (U32(b, 0x68) != 0) continue;
        if (U32(b, 0x6c) == 0) continue;
        return 1;
    }
    return 0;
}
RX_FUNCTION(0x0083e170, rx_rider_clip_looping_at)

/* ---- 0083e230 (EAX = the animator, xmm1 = the time) -> xmm0: the effective end (end - cropBack) of the plain block (no
 * additive / override flags) containing the time, else -1 [0x9cd9e8]. Frame: none. ---- */
float rx_rider_clip_end_at(rx_addr anim, float t) {
    const int32_t n = I32(anim, 0x8c);
    const float none = F32(0, 0x009cd9e8);
    if (n <= 0) return none;
    rx_addr b = U32(U32(anim, 0x70), 0x18);
    for (int32_t i = 0; i < n; i++, b += 0x70) {
        const float lo = rx_adds(F32(b, 0x4c), F32(b, 0x40));
        const float hi = rx_subs(F32(b, 0x54), F32(b, 0x50));
        if (!(t > lo)) continue;
        if (!(hi > t)) continue;
        if (U32(b, 0x64) != 0) continue;
        if (U32(b, 0x68) != 0) continue;
        return hi;
    }
    return none;
}
RX_FUNCTION(0x0083e230, rx_rider_clip_end_at)

/* ---- 0083e1d0 (EAX = the animator, xmm1 = the time) -> xmm0: the start + cropFront of the plain block containing the time,
 * else -1. Frame: none. ---- */
float rx_rider_clip_start_at(rx_addr anim, float t) {
    const int32_t n = I32(anim, 0x8c);
    const float none = F32(0, 0x009cd9e8);
    if (n <= 0) return none;
    rx_addr b = U32(U32(anim, 0x70), 0x18);
    for (int32_t i = 0; i < n; i++, b += 0x70) {
        const float lo = rx_adds(F32(b, 0x4c), F32(b, 0x40));
        const float hi = rx_subs(F32(b, 0x54), F32(b, 0x50));
        if (!(t > lo)) continue;
        if (!(hi > t)) continue;
        if (U32(b, 0x64) != 0) continue;
        if (U32(b, 0x68) != 0) continue;
        return lo;
    }
    return none;
}
RX_FUNCTION(0x0083e1d0, rx_rider_clip_start_at)

/* ---- 0083e0e0 (ECX = the animator): the blend +0x7c to 0 and the block at the animator's time +0x98: its pointer (+0x74 +
 * 4 i) into +0x84 and its position (t - start) / (end - start) into +0x80. Frame: none. ---- */
void rx_rider_clip_block_at(rx_addr anim) {
    SU32(anim, 0x7c, 0);
    const int32_t n = I32(anim, 0x8c);
    if (n <= 0) return;
    const float t = F32(anim, 0x98);
    rx_addr b = U32(U32(anim, 0x70), 0x18);
    for (int32_t i = 0; i < n; i++, b += 0x70) {
        const float lo = rx_adds(F32(b, 0x4c), F32(b, 0x40));
        const float hi = rx_subs(F32(b, 0x54), F32(b, 0x50));
        if (!(t >= lo)) continue;                       /* comiss t, start+cropFront: jb (also unordered) skips */
        if (!(hi >= t)) continue;                       /* comiss end-cropBack, t */
        if (U32(b, 0x64) != 0) continue;
        if (U32(b, 0x68) != 0) continue;
        SU32(anim, 0x84, U32(anim, 0x74) + 4u * (uint32_t)i);
        const float start = F32(b, 0x40);
        SF32(anim, 0x80, rx_divs(rx_subs(t, start), rx_subs(F32(b, 0x54), start)));
        return;
    }
}
RX_FUNCTION(0x0083e0e0, rx_rider_clip_block_at)

/* ---- 0063b5f0 (ESI = the clamped float {min +0, max +4, listeners +8}; stdcall value*; RET 4): the listeners' current value
 * minus *value, clamped to [min, max], broadcast (the same clamp forms as 0063b4b0: max first, then min; a NaN keeps the
 * running value). Frame: none. ---- */
void rx_rider_clamped_float_sub(rx_addr cf, rx_addr value) {
    const rx_addr l = cf + 8;
    const float cur = rx_world_float_listeners_get(l);
    const float x = rx_subs(cur, F32(value, 0));
    const float hi = F32(cf, 4), lo = F32(cf, 0);
    float r = x;
    if (r >= hi) r = hi;
    if (lo >= r) r = lo;
    { const rx_addr s = rx_sp() - 4;
      SF32(s, 0, r);
      WORLD_CALL_SP(8, rx_world_float_listeners_set(l, s)); }
}
RX_FUNCTION(0x0063b5f0, rx_rider_clamped_float_sub)

/* ---- 006cd240 (ECX = the trick, EAX = the vehicle class) -> xmm0: the trick's perf asset's first looping AnimBlock's
 * effective end (end - cropBack), or 0; the handle's in-use flag +0x4c raised around the read. Frame: sub esp, 8; push esi. */
float rx_rider_perf_loop_end(rx_addr trick, uint32_t klass) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0 - 4, 0, sv.esi);
    SU32(sp0 - 8, 0, klass);
    rx_set_sp(sp0 - 0x14);                               /* sub 8; push esi; push the class; the call's return address */
    const rx_addr h = rx_fx_trick_asset_handle(trick, klass);
    rx_set_sp(sp0 - 8);
    const rx_addr asset = U32(h, 0x44);
    SU32(h, 0x4c, 1);
    float r = 0.0f;
    const int32_t n = I32(asset, 0x14);
    if (n > 0) {
        rx_addr b = U32(asset, 0x18);
        for (int32_t i = 0; i < n; i++, b += 0x70) {
            if (U32(b, 0x6c) == 0) continue;
            r = rx_subs(F32(b, 0x54), F32(b, 0x50));
            break;
        }
    }
    SU32(h, 0x4c, 0);
    fx_restore(sv);
    rx_set_sp(sp0);
    return r;
}
RX_FUNCTION(0x006cd240, rx_rider_perf_loop_end)

/* the two crash-clip starts (00622160 / 00622460): the trick's "%sRider_%s" perf handle -> its end +0x24 into the rider's
 * +0xaf20, the state (2 / 1), the clip slot from the crash (ground +0xaf9c) / air (+0xaf7c) map, the layer property ranges,
 * the animation clip (+0xaf14) and its length +0x94 into +0xaf20, then the wind-up flag +0xaf34 from the hold time / first
 * looping end against the driver's progress. Frame: sub esp, 0x13c; ebx / ebp / esi / edi; the formatted name at f.esp + 0x3c
 * (0x104), the std::string at f.esp + 0x20; ECX = the player, stack: the rider, the trick record. */
static void crash_clip_start(rx_addr player, rx_addr ra, rx_addr trick, uint32_t state, rx_addr map_off) {
    world_frame f = world_frame_flat(4, 0x13c);
    const rx_addr str = f.esp + 0x20, buf = f.esp + 0x3c;
    SU32(f.esp, 0x148, U32(0, 0x009b45e4) ^ (f.esp + 0x10));
    if (U32(ra, 0xaf10) == state) goto end;              /* already in that layer */
    {
        const uint32_t klass = U32(U32(player, 0x1ac), 0x194);
        rx_addr prefix = 0;
        if (klass == 0) prefix = 0x0092fc24;             /* "MX" */
        else if (klass == 1) prefix = 0x0092fc28;        /* "ATV" */
        { const rx_addr sp = rx_sp();
          SU32(sp, -8, prefix); SU32(sp, -4, trick + 0x20);
          rx_set_sp(sp - 0x10);                           /* 3 arguments + the call's return address */
          rx_format_buffer(buf, 0x104, 0x00960ac0 /* "%sRider_%s" */, sp - 8);
          rx_set_sp(sp); }
        crt_str_ctor_cstr(str, buf);
        const rx_addr h = rx_fx_trick_asset_handle(trick, klass);
        const rx_addr asset = U32(h, 0x44);
        SU32(h, 0x4c, 1);
        const float end = F32(asset, 0x24);
        SU32(h, 0x4c, 0);
        SF32(ra, 0xaf20, end);
        SU32(ra, 0xaf10, state);
        const rx_addr v = rx_rider_stunt_map_u16(ra + map_off, str);
        const uint32_t slot = (uint32_t)U16(v, 0);
        if (state == 2) {
            if (U32(ra, 0x57c) != 0) SU8(U32(ra, 0x57c), 0, 1);
            if (U32(ra, 0x580) != 0) SU16(U32(ra, 0x580), 0, (uint16_t)slot);
            { const float w = range_clamp(ra, 0x594, 0x590);
              if (U32(ra, 0x598) != 0) SF32(U32(ra, 0x598), 0, w); }
            if (U32(ra, 0x564) != 0) SU8(U32(ra, 0x564), 0, 1);
            if (U32(ra, 0x568) != 0) SU16(U32(ra, 0x568), 0, U16(ra, 0x5ac));
            { const float w = range_clamp(ra, 0x588, 0x584);
              if (U32(ra, 0x58c) != 0) SF32(U32(ra, 0x58c), 0, w); }
        } else {
            if (U32(ra, 0x564) != 0) SU8(U32(ra, 0x564), 0, 1);
            if (U32(ra, 0x568) != 0) SU16(U32(ra, 0x568), 0, (uint16_t)slot);
            { const float w = range_clamp(ra, 0x588, 0x584);
              if (U32(ra, 0x58c) != 0) SF32(U32(ra, 0x58c), 0, w); }
            { const float w = range_clamp(ra, 0x594, 0x590);
              if (U32(ra, 0x598) != 0) SF32(U32(ra, 0x598), 0, w); }
        }
        /* the clip record from the rider's animation map (007fa030 of the name into the local at +0x14) */
        {
            const rx_addr out = f.esp + 0x14, keyp = f.esp + 0x1c;
            SU32(out, 0, 0);
            SU32(keyp, 0, buf);
            { const rx_addr sp = rx_sp();
              rx_set_sp(sp - 8);                          /* 1 argument + the call's return address */
              rx_world_string_table_find(U32(ra, 0x144) + 0x248, keyp, out);
              rx_set_sp(sp); }
            const rx_addr clip = U32(out, 0);
            SU32(ra, 0xaf14, clip);
            const float clen = rx_bf(world_x87_pass(U32(clip, 0x94)));
            SF32(ra, 0xaf20, clen);
            if (0.0f < clen) { SF32(ra, 0x464, 0.0f); SF32(ra, 0x468, clen); }
        }
        if (U32(ra, 0xaf30) == 1) {
            const rx_addr lst = ra + (U32(ra, 0xaf10) == 3 ? 0x48c : 0x46c);
            const float got = rx_world_float_listeners_get(lst);
            const float hold = rx_fx_trick_hold_time(trick, klass);
            const float end2 = rx_rider_perf_loop_end(trick, klass);
            if (hold > got) SU32(ra, 0xaf34, 1);
            else if (got > end2) SU32(ra, 0xaf34, 0);
        }
        crt_str_dtor(str);
    }
end:
    phys_security_check_cookie(U32(f.esp, 0x148) ^ (f.esp + 0x10));
    world_frame_leave(&f);
}

/* ---- 00621f40 (ESI = the rider): the crash / trick wind-up step +0x5b2d4: 0 nothing; 1: both layer blends (+0x598, +0x58c)
 * at 1 -> 2; 2: the wind-up clip (00622160 when +0x5b2d0 is set, else 00622460) with the stored player +0x5b204 and the
 * trick snapshot at +0x5b210, then 0. Frame: push ecx. ---- */
void rx_rider_crash_step(rx_addr ra) {
    const uint32_t st = U32(ra, 0x5b2d4);
    if (st == 1) {
        const rx_addr a = U32(ra, 0x598), b = U32(ra, 0x58c);
        if (a != 0 && F32(a, 0) == 1.0f && b != 0 && F32(b, 0) == 1.0f) SU32(ra, 0x5b2d4, 2);
    } else if (st == 2) {
        const rx_addr player = U32(ra, 0x5b204);
        if (U32(ra, 0x5b2d0) != 0) rx_rider_crash_begin(player, ra, ra + 0x5b210);
        else rx_rider_crash_resume(player, ra, ra + 0x5b210);
        SU32(ra, 0x5b2d4, 0);
    }
}
RX_FUNCTION(0x00621f40, rx_rider_crash_step)

void rx_rider_crash_begin(rx_addr player, rx_addr ra, rx_addr trick) { crash_clip_start(player, ra, trick, 2, 0xaf9c); }
RX_FUNCTION(0x00622160, rx_rider_crash_begin)
void rx_rider_crash_resume(rx_addr player, rx_addr ra, rx_addr trick) { crash_clip_start(player, ra, trick, 1, 0xaf7c); }
RX_FUNCTION(0x00622460, rx_rider_crash_resume)

/* The wind-up request shared by 00621fe0 and 006223e0 (ESI = the rider ra, EDI = the player, ECX = the trick record): when both
 * layer blends (+0x598, +0x58c) are at 1 the clip starts now (`start`: 00622160 / 00622460 with ECX = player, stack ra, trick) and
 * the wind-up step is idle (+0x5b2d4 = 0); otherwise the request is queued for rx_rider_crash_step: step 1, +0x5b2d0 = `begin`
 * (1 = 00622160 later, 0 = 00622460 later), the trick record copied to +0x5b210 (00622050: EAX = destination, ECX = source) and
 * the player stored at +0x5b204. Frame: push ecx (the callee's stack arguments reuse it). `start_ret` / `copy_ret` are the call
 * sites' return addresses. */
static void crash_request(rx_addr ra, rx_addr player, rx_addr trick, int begin, rx_addr start_ret, rx_addr copy_ret) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);                                       /* push ecx */
    const rx_addr a = U32(ra, 0x598), b = U32(ra, 0x58c);
    if (a != 0 && F32(a, 0) == 1.0f && b != 0 && F32(b, 0) == 1.0f) {
        const uint32_t args[2] = {ra, trick};
        const rx_addr s = fx_push(2, args, start_ret);
        if (begin) rx_rider_crash_begin(player, ra, trick);
        else rx_rider_crash_resume(player, ra, trick);
        fx_pop(s);
        SU32(ra, 0x5b2d4, 0);
    } else {
        SU32(ra, 0x5b2d4, 1);
        SU32(ra, 0x5b2d0, begin ? 1 : 0);
        const rx_addr s = fx_push(0, 0, copy_ret);
        rx_stunt_record_copy(ra + 0x5b210, trick);
        fx_pop(s);
        SU32(ra, 0x5b204, player);
    }
    rx_set_sp(sp0);
}

/* ---- 00621fe0 (ESI = the rider, EDI = the player, ECX = the trick record): the "StuntLanded" wind-up request: the crash clip
 * 00622160 now when both blends are at 1, else queued (see crash_request). ---- */
void rx_rider_crash_request_begin(rx_addr ra, rx_addr player, rx_addr trick) {
    crash_request(ra, player, trick, 1, 0x00622020, 0x00622048);
}
RX_FUNCTION(0x00621fe0, rx_rider_crash_request_begin)

/* ---- 00623060 (EAX = the rider controller; plain RET): the controller's three output cells written for PhysicsStuntEnd kinds 3 / 4
 * (0054d530): the flag byte the cell +0x564 points at is set to 1, the word cell +0x568 gets the controller's word +0x5ac, and the
 * float cell +0x58c the blend weight: the upper bound +0x588 limited to at most 0 (comiss: a value above 0, or a NaN, becomes 0) and
 * then raised to the lower bound +0x584 when that is not below it (a NaN bound leaves the weight). Null cells are skipped. ---- */
void rx_rider_stunt_end_outputs(rx_addr e) {
    const rx_addr flag = U32(e, 0x564);
    if (flag != 0) SU8(flag, 0, 1);
    const rx_addr word = U32(e, 0x568);
    if (word != 0) SU16(word, 0, (uint16_t)U32(e, 0x5ac));
    const float upper = F32(e, 0x588), lower = F32(e, 0x584);
    float weight = upper;
    if (!(0.0f >= upper)) weight = 0.0f;           /* comiss 0, upper; jae: taken only when 0 >= upper (ordered) */
    if (lower >= weight) weight = lower;            /* comiss lower, weight; jb skips: lower < weight or unordered */
    const rx_addr cell = U32(e, 0x58c);
    if (cell != 0) SF32(cell, 0, weight);
}
RX_FUNCTION(0x00623060, rx_rider_stunt_end_outputs)

/* ---- 006223e0 (ESI = the rider, EDI = the player, ECX = the trick record): the "StuntTakeoff" wind-up request: the air clip
 * 00622460 now when both blends are at 1, else queued with +0x5b2d0 = 0. ---- */
void rx_rider_crash_request_resume(rx_addr ra, rx_addr player, rx_addr trick) {
    crash_request(ra, player, trick, 0, 0x00622420, 0x0062244b);
}
RX_FUNCTION(0x006223e0, rx_rider_crash_request_resume)

/* ---- 00620a60 (stdcall ra, dt; RET 8): the clip-time / chain-event update of the rider's trick animation. See the file
 * header. Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x54; ebx / esi / edi. ---- */
void rx_rider_crash_update(rx_addr ra, float dt) {
    world_frame f = world_frame_enter(0x54, 3, 1);
    const rx_addr vA = f.esp + 0x18;                    /* the broadcast value */
    const rx_addr vB = f.esp + 0x1c;                    /* the time before the advance (then 0) */
    SU32(f.esp, 0x5c, U32(0, 0x009b45e4) ^ f.esp);

    if (U32(ra, 0xaf10) == 4) {
        const rx_addr a = U32(ra, 0x58c), b = U32(ra, 0x598);
        if (a != 0 && F32(a, 0) == 1.0f && b != 0 && F32(b, 0) == 1.0f) {
            SU32(ra, 0xaf10, 0); SU32(ra, 0xaf28, 0); SU32(ra, 0xaf2c, 0);
        }
        goto end;
    }
    {
        const rx_addr t = ra + 0x46c;
        const float t0 = rx_world_float_listeners_get(t);
        SF32(vB, 0, t0);
        const float cur = rx_world_float_listeners_get(t);
        const float mn = F32(ra, 0x464), mx = F32(ra, 0x468);
        float next = rx_adds(cur, dt);
        if (next >= mx) next = mx;
        if (mn >= next) next = mn;
        SF32(vA, 0, next);
        rx_world_float_listeners_set(t, vA);
        const float now = rx_world_float_listeners_get(t);
        if (rx_rider_clip_looping_at(U32(ra, 0xaf14), now) == 0) goto after_blocks;
        /* inside a looping block: state 3, or a trick that is not held (+0xaf30 0) and was not held the frame before (+0xaf38 0),
         * leaves it at once (00620c00: the time set to the block's end - cropBack); the frame the hold is let go (+0xaf38 1) only
         * starts the cross-fade (00620be0: 0083e0e0) and the next frame leaves the block. (w-fsfix: the port went on through the
         * looping block when +0xaf38 was 0, so a trick let go before or during its hold played the whole hold: the "dead timer".) */
        if (U32(ra, 0xaf10) != 3 && U32(ra, 0xaf30) == 0 && U32(ra, 0xaf38) != 0) {
            rx_rider_clip_block_at(U32(ra, 0xaf14));
            SU32(ra, 0xaf38, 0);
            goto after_blocks;
        }
        if (U32(ra, 0xaf10) == 3 || U32(ra, 0xaf30) == 0) {
            const float v = rx_world_float_listeners_get(t);
            const float e = rx_rider_clip_end_at(U32(ra, 0xaf14), v);
            SF32(vA, 0, e);
            rx_world_clamped_float_set(ra + 0x464, vA);   /* the animator's time listeners (+0x46c), as ESI is +0x464 here */
            goto after_blocks;
        }
        SU32(ra, 0xaf38, 1);
        {
            const float v = rx_world_float_listeners_get(t);
            const float e = rx_rider_clip_end_at(U32(ra, 0xaf14), v);
            SF32(vA, 0, e);
            const float cur2 = rx_world_float_listeners_get(t);
            const float mn2 = F32(ra, 0x464), mx2 = F32(ra, 0x468);
            float next2 = rx_adds(cur2, dt);
            if (next2 >= mx2) next2 = mx2;
            if (mn2 >= next2) next2 = mn2;
            if (!(next2 > e)) goto after_blocks;
            const float st = rx_rider_clip_start_at(U32(ra, 0xaf14), rx_world_float_listeners_get(t));
            SF32(vA, 0, st);
            rx_world_clamped_float_set(ra + 0x464, vA);   /* ESI is +0x464 here too */
        }
after_blocks:
        ;
    }
    /* a trick in state 3 whose clip reached its last block end: the next clip (+0xaf18) switches in */
    if (U32(ra, 0xaf10) == 3) {
        const float now = rx_world_float_listeners_get(ra + 0x46c);
        const rx_addr clip = U32(ra, 0xaf14);
        if (now >= F32(U32(clip, 0x70), 0x28)) {
            const int16_t slot = (int16_t)U16(ra, 0xaf3c);
            if (slot >= 0) {
                if (U32(ra, 0x564) != 0) SU8(U32(ra, 0x564), 0, 1);
                if (U32(ra, 0x568) != 0) SU16(U32(ra, 0x568), 0, (uint16_t)slot);
                { const float w = range_clamp(ra, 0x588, 0x584);
                  if (U32(ra, 0x58c) != 0) SF32(U32(ra, 0x58c), 0, w); }
                {
                    const rx_addr nclip = U32(ra, 0xaf18);
                    const float t2 = F32(U32(nclip, 0x70), 0x2c);
                    SF32(vA, 0, t2);
                    rx_world_clamped_float_set(ra + 0x484, vA);
                    SU16(ra, 0xaf3c, 0xffff);
                    const float got = rx_world_float_listeners_get(ra + 0x48c);
                    rx_world_rider_play_clip_events(ra, nclip, 0.0f, got);
                }
            }
            {
                const rx_addr p58 = U32(ra, 0x58c);
                if (p58 != 0 && F32(p58, 0) >= 1.0f) {
                    SU32(ra, 0xaf14, U32(ra, 0xaf18));
                    SU32(ra, 0xaf20, U32(ra, 0xaf24));
                    SU32(ra, 0xaf18, 0);
                    {
                        const rx_addr p28 = U32(ra, 0xc928), p2c = U32(ra, 0xc92c);
                        if (p28 != 0) {
                            if (p2c == 0) bike_end_pose_override(U32(U32(ra, 0xaf1c), 0x234));
                        } else if (p2c != 0) {
                            /* 00620d78: mov eax, [ebx+0xaf1c]; mov ecx, [eax+0x234]; mov edx, [ecx]; mov eax, [edx+0x84]; call eax:
                             * simulation vf33 (MX 00446cd0, ATV 0048e8a0, four-wheelers 0045a940: the pose override's start), a
                             * thiscall without stack arguments. (w-pose446: the port passed `.eax = sim` and pushed `sim`; with ECX 0
                             * the product's world_call_ctx took the original-only path and stopped: "unported game function 00446cd0"
                             * when an air trick whose clip drives the bike (+0xe00 keys) chained after one whose clip does not.) */
                            const rx_addr sim = U32(U32(ra, 0xaf1c), 0x234), vt = U32(sim, 0);
                            WORLD_CTX0(U32(vt, 0x84), &WCTX(.eax = U32(vt, 0x84), .ecx = sim, .edx = vt, .ebx = ra), 0);
                        }
                    }
                    {
                        const uint32_t variant = U32(ra, 0xaf2c);
                        const rx_addr p2c = U32(ra, 0xc92c);
                        SU32(ra, 0xc92c, 0);
                        SU32(ra, 0xaf2c, 0);
                        SU32(ra, 0xc928, p2c);
                        SU32(ra, 0xaf28, variant);
                        SU32(ra, 0xaf10, (variant != 1) + 1);
                    }
                    { const float v = rx_world_float_listeners_get(ra + 0x48c);
                      SF32(vA, 0, v);
                      rx_world_clamped_float_set(ra + 0x464, vA); }
                    rx_rider_vec8_mark_all(ra + 0x46c);
                    rx_rider_vec8_clear_all(ra + 0x48c);
                    if (0.0f < F32(ra, 0xaf20)) { SF32(ra, 0x464, 0.0f); SF32(ra, 0x468, F32(ra, 0xaf20)); }
                    SF32(vB, 0, 0.0f);
                }
            }
        }
    }
    /* the pose override clip +0xc928 (a scripted trick): its time / pose go to the bike simulation */
    if (U32(ra, 0xc928) != 0) {
        const float now = rx_world_float_listeners_get(ra + 0x46c);
        {
            rx_addr ent = U32(ra, 0xc928);
            SF32(ent, 0xdb4, now);
            if (U32(ent, 0xda8) != 0) rx_world_keyframe_blend_select(ent);
        }
        const rx_addr pose = f.esp + 0x20;              /* 12 floats: +0x20 .. +0x4c */
        v4_st(pose + 0x00, v4_ld(0x00d61670u));
        v4_st(pose + 0x10, v4_ld(0x00d61640u));
        v4_st(pose + 0x20, v4_ld(0x00d61640u));
        const rx_addr p2c = U32(ra, 0xc92c);
        const rx_addr clip = U32(ra, 0xaf14);
        if (p2c == 0 || !(now >= F32(U32(clip, 0x70), 0x28))) {
            const rx_addr e = U32(ra, 0xc928);
            v4_st(pose + 0x00, v4_ld(e + 0x450));
            v4_st(pose + 0x10, v4_ld(e + 0x460));
            v4_st(pose + 0x20, v4_ld(e + 0x490));
        } else {
            const float v = rx_world_float_listeners_get(ra + 0x48c);
            {
                const rx_addr e2 = U32(ra, 0xc92c);
                SF32(e2, 0xdb4, v);
                if (U32(e2, 0xda8) != 0) rx_world_keyframe_blend_select(e2);
            }
            { const rx_addr p = U32(ra, 0x58c);
              const float w = p ? F32(p, 0) : 0.0f;
              phys_pose_blend(U32(ra, 0xc92c) + 0x440, ra + 0xc940, U32(ra, 0xc928) + 0x440,
                              (int32_t)U32(U32(ra, 0xc928), 0xdac), w); }
            v4_st(pose + 0x00, v4_ld(ra + 0xc950));
            v4_st(pose + 0x10, v4_ld(ra + 0xc960));
            v4_st(pose + 0x20, v4_ld(ra + 0xc990));
        }
        {
            const rx_addr sim = U32(U32(ra, 0xaf1c), 0x234);
            WORLD_CTX(U32(U32(sim, 0), 0x28),
                      &WCTX(.eax = pose, .ecx = sim, .ebx = ra + 0x46c, .esi = U32(ra, 0xaf1c), .edi = ra + 0x46c), 0, pose);
        }
    }
    /* the current clip's events between the old and the new time (unless a chained clip has its own) */
    if (U32(ra, 0xaf18) != 0) {
        const float now = rx_world_float_listeners_get(ra + 0x46c);
        const rx_addr clip = U32(ra, 0xaf14);
        if (!(now < F32(U32(clip, 0x70), 0x28))) goto end;   /* comiss end, now: jbe (equal / unordered) skips */
    }
    {
        const float t1 = F32(vB, 0);
        const float now = rx_world_float_listeners_get(ra + 0x46c);
        rx_world_rider_play_clip_events(ra, U32(ra, 0xaf14), t1, now);
    }
end:
    phys_security_check_cookie(U32(f.esp, 0x5c) ^ f.esp);
    world_frame_leave(&f);
}
RX_FUNCTION(0x00620a60, rx_rider_crash_update)
