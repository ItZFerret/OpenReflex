/* anim_extern.h -- the animation code's calls into functions it does not own (math / mechanism = rx_phys, node hierarchy = rx_world,
 * skeleton core 0x0082xxxx = loop_anim.c, imports): each wrapper calls the owner's core function (all of them are ported since
 * FloatKeys_FindSegment 008435f0; virtual calls dispatch through the registry). One wrapper per callee, named after what it does, so the call sites
 * read like the game code and this file is the list of what the animation code still borrows (shared by all anim_*.c; wrappers
 * used by one file only live in that file). Register conventions are the original's (LTCG); see core/notes/anim.md. */
#pragma once
#include "rx_game.h"
#include "crt.h"
#include "world_node.h"
#include "world_rider.h"
#include "world_veh.h"
#include "loop_anim.h"
#include "loop_cam_player.h"
#include "loop_platform.h"
#include "phys_quat.h"
#include "build_alloc.h"
#include <stdio.h>
#include <stdlib.h>

enum { ANIM_EAX, ANIM_ECX, ANIM_EDX, ANIM_EBX, ANIM_ESP, ANIM_EBP, ANIM_ESI, ANIM_EDI };
/* The machine's registers as the original code would have them now (oracle: the emulated CPU of the running port, bound by the
 * adapters' ANIM_PORT; product: a register file of its own): the frame macros below push and set the frame pointer EBP like the
 * originals' `push ebp; mov ebp, esp`, so the originals called from a core function see (and save on the stack) the same EBX /
 * EBP / ESI / EDI values as under the original caller. (A stale EBP saved by a callee is read back later as a dead vector lane,
 * 006259c0's +0x7c: core/notes/anim.md.) */
uint32_t anim_reg_get(int i);
void anim_reg_set(int i, uint32_t v);
/* A register set for rx_call_original_ex: the current callee-saved registers (EBX EBP ESI EDI), EAX / ECX / EDX zero, then the
 * callee's live-in registers. */
static inline rx_regs anim_regs(void) {
    rx_regs r; memset(&r, 0, sizeof r);
    r.r[ANIM_EBX] = anim_reg_get(ANIM_EBX); r.r[ANIM_EBP] = anim_reg_get(ANIM_EBP);
    r.r[ANIM_ESI] = anim_reg_get(ANIM_ESI); r.r[ANIM_EDI] = anim_reg_get(ANIM_EDI);
    return r;
}
/* `push ebp; mov ebp, esp` at entry (stack pointer sp0): the caller's EBP saved at sp0 - 4, EBP = sp0 - 4; returns the saved one. */
static inline uint32_t anim_frame_push_ebp(rx_addr sp0) {
    uint32_t ebp = anim_reg_get(ANIM_EBP);
    SU32(sp0 - 4, 0, ebp);
    anim_reg_set(ANIM_EBP, sp0 - 4);
    return ebp;
}

/* ---- imports (KERNEL32 through the IAT) ---- */
static inline void anim_enter_cs(rx_addr cs) { loop_enter_cs(cs); }
static inline void anim_leave_cs(rx_addr cs) { loop_leave_cs(cs); }

/* ---- node hierarchy (world) ---- */
/* 007db7e0 Node_Invalidate: ECX = node record; clears the "world valid" bit of the record and its subtree, marks the hierarchy
 * dirty (+0x34 = 1). */
static inline void anim_node_invalidate(rx_addr rec) {
    rx_world_node_invalidate(rec, anim_reg_get(ANIM_EBX), anim_reg_get(ANIM_ESI), anim_reg_get(ANIM_EDI));
}
/* The return address the original's call leaves in the dead stack (the slot below the arguments) after the callee returned. */
static inline void anim_ret_slot(uint32_t arg_bytes, rx_addr ret) { SU32(rx_sp() - arg_bytes - 4u, 0, ret); }
/* Node_Invalidate as called from an original call site: its callee-saved registers as the original has them there (it pushes
 * EBX, and ESI / EDI when it walks the subtree) and the call's return address left in the dead stack. */
static inline void anim_node_invalidate_from(rx_addr rec, rx_regs* r, rx_addr ret) {
    r->r[ANIM_ECX] = rec;
    rx_set_sp(rx_sp() - 4u); SU32(rx_sp(), 0, ret);
    rx_world_node_invalidate(rec, r->r[ANIM_EBX], r->r[ANIM_ESI], r->r[ANIM_EDI]);
    rx_set_sp(rx_sp() + 4u);
}

/* ---- skeleton pose core (engine, 0x0082xxxx) ---- */
/* 0082e250 SkeletonPose_Reset: EAX = character (+0x234 bone count), EDX = pose: every 0x30-byte bone record to identity rotation
 * (0x9cb550), translation = the 0xa95d50 vector, valid bytes +0x20 / +0x21 = 0. */
static inline void anim_pose_reset(rx_addr ch, rx_addr pose) {
    rx_skeleton_pose_reset(ch, pose);
}
/* 0082e2d0 SkeletonPosePool_Acquire: EDI = character; under the pool's critical section (+0x208) the first free slot of the used
 * flags (+0x22c, count +0x230) is taken and its pose (+0x228 + slot * bones * 0x30) reset; returns it (or the pool base when full). */
static inline rx_addr anim_pose_acquire(rx_addr ch) {
    return rx_skeleton_pose_acquire(ch);
}

/* ---- animators (vtable slot 4 = evaluate: thiscall (character, root delta out or 0, pose); RET 0xc) ---- */
typedef void (*anim_eval_fn)(rx_addr self, rx_addr ch, rx_addr delta, rx_addr pose);
/* A push onto the game stack (an argument or a return address the original leaves there). */
#define ANIM_PUSH(v) (rx_set_sp(rx_sp() - 4u), SU32(rx_sp(), 0, (v)))
/* The child's evaluate called from the call site whose return address is `ret`: the original's three pushes and the return
 * address on the game stack either way (the core function when registered, else the original), so the child's frame lands
 * where the original's does and the dead stack holds what the original's call leaves there. */
static inline void anim_animator_eval(rx_addr animator, rx_addr ch, rx_addr delta, rx_addr pose, rx_addr ret) {
    rx_addr target = U32(U32(animator, 0), 0x10);
    anim_eval_fn f = RX_FN(target, anim_eval_fn);
    if (f) {
        ANIM_PUSH(pose); ANIM_PUSH(delta); ANIM_PUSH(ch); ANIM_PUSH(ret);
        f(animator, ch, delta, pose);
        rx_set_sp(rx_sp() + 0x10u);
        return;
    }
    uint32_t a[3] = {ch, delta, pose};
    rx_call_original(target, RX_THISCALL, animator, 3, a);
    anim_ret_slot(0xc, ret);
}

/* A float passed through the x87 (fld m32 / fstp m32): the same bits, except that a signaling NaN comes back quiet. */
static inline void anim_x87_restore_f32(rx_addr a) {
    uint32_t b = U32(a, 0);
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu) && !(b & 0x00400000u)) SU32(a, 0, b | 0x00400000u);
}

/* A frame on the game stack laid out like the original's (`push ebp; mov ebp, esp; and esp, -16; sub esp, ...`): A = the aligned
 * frame base (the original's ESP right after `and esp, -16`), the stack pointer set to A - below (the original's ESP at its calls),
 * so locals passed by address sit at the original's addresses and callees run on the original's stack. ANIM_FRAME_LEAVE restores. */
#define ANIM_FRAME_ENTER(A, below) const rx_addr A##_sp0 = rx_sp(); const uint32_t A##_ebp = anim_frame_push_ebp(A##_sp0); \
    const rx_addr A = (A##_sp0 - 4u) & ~15u; rx_set_sp(A - (below))
#define ANIM_FRAME_LEAVE(A) (anim_reg_set(ANIM_EBP, A##_ebp), rx_set_sp(A##_sp0))

/* ---- key tracks (engine 0x0082 compressed tracks, math 0x004d / 0x0042 key helpers) ---- */
static inline void anim_xmm0(rx_regs* r, float x) { r->xmm[0].v[0] = x; }
/* 0082b930 AnimKeys_Locate: ECX = compressed track set, xmm0 = time (wrapped by its +0x10 length), stack = locate out (16 bytes:
 * time, fraction, key k, key k+1) or 0; RET 4. */
static inline void anim_keys_locate(rx_addr tracks, float time, rx_addr out) {
    rx_set_sp(rx_sp() - 8u); SU32(rx_sp(), 4, out);   /* the original's push and return address */
    rx_animkeys_locate(tracks, out, time);
    rx_set_sp(rx_sp() + 8u);
}
/* 0082b9e0 AnimKeys_SampleRotation: ECX = locate record, EDX = track index, ESI = quaternion out (slerp of the two int16x4 keys). */
static inline void anim_keys_sample_rotation(rx_addr locate, uint32_t index, rx_addr out) {
    rx_set_sp(rx_sp() - 4u);
    rx_animkeys_sample_rotation(locate, index, out);
    rx_set_sp(rx_sp() + 4u);
}
/* 0082bac0 AnimKeys_SampleTranslation: ECX = track index, EDX = vec3 out, EDI = locate record, stack = track set; RET 4. */
static inline void anim_keys_sample_translation(rx_addr tracks, uint32_t index, rx_addr locate, rx_addr out) {
    rx_set_sp(rx_sp() - 8u); SU32(rx_sp(), 4, tracks);
    rx_animkeys_sample_translation(tracks, index, out, locate);
    rx_set_sp(rx_sp() + 8u);
}
/* 0042ce40 QuatSlerp_Packed: EAX = packed key a, ESI = packed key b, EDI = quaternion out, stack float t (cdecl). */
static inline void anim_quat_slerp_packed(rx_addr a, rx_addr b, rx_addr out, float t) {
    rx_set_sp(rx_sp() - 8u); SU32(rx_sp(), 4, rx_fb(t));
    phys_quat_slerp_packed(a, b, out, t);
    rx_set_sp(rx_sp() + 8u);
}
/* 004dbce0 KeyTrack_FindSegment: EAX = keys (16 bytes: time, x y z), EDX = count, xmm0 = time; cdecl (prev key out, next key out,
 * fraction out); returns 1 when the time lies inside the track, 0 when clamped to its ends. */
static inline uint32_t anim_keytrack_find_segment(rx_addr keys, uint32_t count, float time, rx_addr prev, rx_addr next, rx_addr frac) {
    rx_set_sp(rx_sp() - 0x10u); SU32(rx_sp(), 4, prev); SU32(rx_sp(), 8, next); SU32(rx_sp(), 0xc, frac);
    const uint32_t r = rx_world_keytrack_find_segment(keys, count, time, prev, next, frac);
    rx_set_sp(rx_sp() + 0x10u);
    return r;
}
/* 004dbe00 KeyTrack_Upper: EAX = count, EDI = keys (16 bytes, time first), xmm0 = time; the index of the first key after the time
 * (binary search; 0 for an empty track). */
static inline uint32_t anim_keytrack_upper(rx_addr keys, uint32_t count, float time) {
    rx_set_sp(rx_sp() - 4u);
    const uint32_t r = rx_keytrack_count_before(count, keys, time);
    rx_set_sp(rx_sp() + 4u);
    return r;
}
/* 008435f0 FloatKeys_FindSegment: as KeyTrack_FindSegment for 8-byte keys (time, value): EAX = keys, EDX = count, xmm0 = time;
 * cdecl (prev key out, next key out, fraction out); returns 1 inside the track, 0 clamped. */
uint32_t rx_floatkeys_find_segment(rx_addr keys, uint32_t count, float time, rx_addr prev, rx_addr next, rx_addr frac);
static inline uint32_t anim_floatkeys_find_segment(rx_addr keys, uint32_t count, float time, rx_addr prev, rx_addr next, rx_addr frac) {
    return rx_floatkeys_find_segment(keys, count, time, prev, next, frac);
}
/* fld m32; fstp m32 from src to dst (a signaling NaN arrives quiet). */
static inline void anim_x87_copy_f32(rx_addr dst, rx_addr src) {
    uint32_t b = U32(src, 0);
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu)) b |= 0x00400000u;
    SU32(dst, 0, b);
}
/* As ANIM_FRAME_ENTER for `and esp, -8` frames. */
#define ANIM_FRAME8_ENTER(A, below) const rx_addr A##_sp0 = rx_sp(); const uint32_t A##_ebp = anim_frame_push_ebp(A##_sp0); \
    const rx_addr A = (A##_sp0 - 4u) & ~7u; rx_set_sp(A - (below))

/* MSVCR90 _invalid_parameter_noinfo (IAT 0x916224): the checked STL iterators' failure report (does not return normally). */
static inline void anim_invalid_parameter(void) { crt_invalid_parameter_noinfo(); }

/* ---- animator float parameters (vtable slot 11: thiscall (name, float** out) -> al found; RET 8) ---- */
typedef uint32_t (*anim_param_fn)(rx_addr self, rx_addr name, rx_addr out);
static inline uint32_t anim_animator_get_param(rx_addr animator, rx_addr name, rx_addr out, rx_addr ret) {
    rx_addr target = U32(U32(animator, 0), 0x2c);
    anim_param_fn f = RX_FN(target, anim_param_fn);
    if (f) {
        ANIM_PUSH(out); ANIM_PUSH(name); ANIM_PUSH(ret);
        uint32_t r = f(animator, name, out);
        rx_set_sp(rx_sp() + 0xcu);
        return r;
    }
    uint32_t a[2] = {name, out};
    uint32_t r = rx_call_original(target, RX_THISCALL, animator, 2, a);
    anim_ret_slot(8, ret);
    return r;
}

/* ---- memory (engine) ---- */
static inline rx_addr anim_aligned_alloc(uint32_t size) { return rx_reflex_aligned_alloc(size); }   /* 0078fbe0 */
static inline void anim_aligned_free(rx_addr p) { rx_reflex_aligned_free(p); }                       /* 0078fc50 */
static inline void anim_memcpy(rx_addr dst, rx_addr src, uint32_t n) { crt_memcpy(dst, src, n); }   /* 008fa070: memcpy thunk */

/* ---- more node hierarchy (world) / math ---- */
/* 007db890 Node_EvaluateWorld (rx_world: rx_world_node_evaluate_world; ECX = record, stack flag; RET 4): brings the world matrices
 * up to date. Called as the original does: the flag pushed and the call site's return address `ret` on the game stack. */
static inline void anim_node_evaluate_world(rx_addr rec, uint32_t flag, rx_addr ret) {
    ANIM_PUSH(flag); ANIM_PUSH(ret);
    rx_world_node_evaluate_world(rec, flag);
    rx_set_sp(rx_sp() + 8);
}
/* 00401740 Matrix3_ToQuaternionToEsi: ESI = quaternion out (16 bytes), stack = matrix (rotation rows); cdecl. */
static inline void anim_matrix_to_quat(rx_addr matrix, rx_addr out) {
    rx_set_sp(rx_sp() - 8u); SU32(rx_sp(), 4, matrix);
    rx_world_matrix3_to_quaternion_to_fr(out, matrix, anim_reg_get(ANIM_EBP));
    rx_set_sp(rx_sp() + 8u);
}
/* MSVCR90 _stricmp (IAT 0x9162bc). */
static inline int32_t anim_stricmp(rx_addr a, rx_addr b) { return crt_stricmp(a, b); }

/* The strcmp MSVC inlines (two bytes per step, `sbb` result): 0 equal, -1 / 1 by the first differing byte (unsigned). */
static inline int32_t anim_strcmp(rx_addr a, rx_addr b) {
    for (uint32_t i = 0;; i++) {
        uint8_t x = U8(a, i), y = U8(b, i);
        if (x != y) return x < y ? -1 : 1;
        if (x == 0) return 0;
    }
}

/* Virtual calls (thiscall) through slot `slot` (byte offset / 4): no arguments -> EAX; returning a float on the x87 (st(0) popped,
 * as the caller's fstp m32 rounds it). */
static inline uint32_t anim_vcall0(rx_addr obj, uint32_t off) {
    const rx_addr target = U32(U32(obj, 0), off);
    uint32_t (*f)(rx_addr) = RX_FN(target, uint32_t (*)(rx_addr));
    if (f) { rx_set_sp(rx_sp() - 4u); const uint32_t r = f(obj); rx_set_sp(rx_sp() + 4u); return r; }
    return rx_call_original(target, RX_THISCALL, obj, 0, 0);   /* product: fatal, naming the slot's function */
}
static inline float anim_vcall0_f(rx_addr obj, uint32_t off) {
    const rx_addr target = U32(U32(obj, 0), off);
    float (*f)(rx_addr) = RX_FN(target, float (*)(rx_addr));
    if (f) { rx_set_sp(rx_sp() - 4u); const float r = f(obj); rx_set_sp(rx_sp() + 4u); return r; }
    rx_regs r = anim_regs(), o; r.r[ANIM_ECX] = obj; rx_call_original_ex(target, &r, &o, 0, 0);
    return o.fpu_depth > 0 ? rx_f80_to_f32(o.st[0], 0) : 0.0f;
}
/* A direct call into another core function where the original pushes `bytes` of arguments and a return address: the stack pointer
 * moves as in the original so the callee's frame lands at the original's addresses. Use around the call. */
#define ANIM_CALL_FRAME(bytes) rx_set_sp(rx_sp() - (bytes) - 4)
#define ANIM_CALL_FRAME_END(bytes) rx_set_sp(rx_sp() + (bytes) + 4)
