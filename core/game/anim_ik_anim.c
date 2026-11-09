/* anim_ik_anim.c -- the 0x006a / 0x006b animators around the rider's procedural motion: ProceduralAnimator (RTTI vtable
 * 0095f00c), ExpressionBoneAnimator (0095ef44) and FeatherstoneAnimator (0095efa0).
 *
 * Procedural and Expression animators evaluate their child (+0x28) into a pool pose, copy it into the output pose (translations
 * through the x87) and then run their own pose stages on the output:
 *  - ProceduralAnimator, when enabled (+0x6c byte): the rider stage objects +0x54 (006259c0), +0x58 (00629910), +0x5c / +0x60
 *    (0062aed0, two instances) and +0x64 (00636990, with its own float +0x18); vf3 sets the rate +0x30 and forwards it to +0x54
 *    (its +0x0c) and +0x58 (its +0x04).
 *  - ExpressionBoneAnimator, when enabled (+0x5c byte): its controllers, a std::vector<ExpressionBoneController*> (checked
 *    iterators: container proxy +0x44, first +0x50, last +0x54), each evaluated through its vtable slot 0, thiscall (pose,
 *    character), RET 8: ArmTwistController 006ad830 (vtable 0095ef78), OrientationConstController 006ae1d0 (0095ef80),
 *    ForearmLookatController 006ae9a0 (0095ef88), HipController 006af0a0 (0095ef90), DeltoidTransController 006af400 (0095ef98).
 * Both pass the child's root delta out. FeatherstoneAnimator::vf4 is a tail call to its child +0x30. */
#include "anim_ik.h"
#include "loop_rider.h"
#include "anim_rider_a.h"
#include "anim_rider_b.h"

/* ---- the stages the animators call (ported: rider_a / rider_b / loop_rider), entered at the original call site's stack ---- */

/* 006259c0 (rx_anim: rider_a; thiscall ECX = character, ESI = stage object; stack: pose; RET 4). */
static void rider_stage_006259c0(rx_addr stage, rx_addr ch, rx_addr pose, rx_addr ret, uint32_t ebx, uint32_t ebp, uint32_t edi) {
    (void)ebx; (void)ebp; (void)edi;
    const rx_addr sp0 = ik_enter(1, &pose, ret);
    rx_rider_limbs_eval(stage, ch, pose);
    rx_set_sp(sp0);
}
/* 00629910 (rx_anim: rider_a; EAX = stage object; stack: pose; RET 4). */
static void rider_stage_00629910(rx_addr stage, rx_addr pose, rx_addr ret, uint32_t ebx, uint32_t ebp, uint32_t esi, uint32_t edi) {
    (void)ebx; (void)ebp; (void)esi; (void)edi;
    const rx_addr sp0 = ik_enter(1, &pose, ret);
    rx_rider_lean_eval(stage, pose);
    rx_set_sp(sp0);
}
/* 0062aed0 (rx_anim: rider_b; ESI = stage object; stack: pose; RET 4). */
static void rider_stage_0062aed0(rx_addr stage, rx_addr pose, rx_addr ret, uint32_t ebx, uint32_t ebp, uint32_t edi) {
    (void)ebx; (void)ebp; (void)edi;
    const rx_addr sp0 = ik_enter(1, &pose, ret);
    rx_rider_limb_state_eval(stage, pose);
    rx_set_sp(sp0);
}
/* 00636990 (EDI = pose; stack: stage object, float (its +0x18, through the x87); RET 8). Still the original: with rx_rider_stage_twist
 * (loop_rider.c) here, Verify of RiderAnimation_KickJobs shows the documented stale w lane (core/notes/loop.md "Animation worker dead
 * stack": Concord call 1697 [152414cc], SX 1375, Terracross 1201): the core stage leaves a different dead stack than the original. */
static void rider_stage_00636990(rx_addr stage, rx_addr pose, uint32_t value_bits, rx_addr ret, uint32_t ebx, uint32_t ebp, uint32_t esi) {
#ifndef RX_ORACLE   /* the product: the core's twist stage (loop_rider.c) */
    (void)ret;
    { const rx_addr sp = rx_sp(); rx_set_sp(sp - 12u); rx_rider_stage_twist_fr(stage, rx_bf(value_bits), pose, ebx, ebp, esi); rx_set_sp(sp); }
    return;
#endif
    rx_regs r = anim_regs();
    r.r[ANIM_EDI] = pose; r.r[ANIM_EAX] = stage;
    r.r[ANIM_EBX] = ebx; r.r[ANIM_EBP] = ebp; r.r[ANIM_ESI] = esi;
    uint32_t a[2] = {stage, value_bits};
    ik_call_ex(0x00636990, &r, 0, 2, a, ret);
}

/* ---- ProceduralAnimator ---- */

/* 006b5870 ProceduralAnimator::vf4 (thiscall (character, root delta out, pose); RET 0xc). See the file comment. */
void rx_procedural_animator_eval(rx_addr self, rx_addr ch, rx_addr delta_out, rx_addr pose) {
    ANIM_FRAME_ENTER(A, 0x30);                     /* sub esp, 0x24; push ebx; push esi; push edi */
    const rx_addr ebp = A_sp0 - 4, delta = A - 0x10;
    SU32(A - 0x1c, 0, self);
    ik_pose_reset(ch, pose, 0x006b588d);
    const rx_addr pool = ik_pose_acquire(ch, 0x006b5894);
    ik_pose_reset(ch, pool, 0x006b589f);
    ik_eval_child(U32(self, 0x28), ch, delta, pool, 0x006b58b4);
    const uint32_t bones = U32(ch, 0x234);
    SU32(A - 0x18, 0, bones);
    if ((int32_t)bones >= 4) SU32(A - 0x14, 0, (((bones - 4) >> 2) + 1) * 4);   /* the unrolled copy's count */
    ik_pose_copy(pose, pool, bones);
    if (U8(self, 0x6c)) {
        rx_addr stage;
        if ((stage = U32(self, 0x54)) != 0) rider_stage_006259c0(stage, ch, pose, 0x006b5ae0, pool, ebp, self);
        if ((stage = U32(self, 0x58)) != 0) rider_stage_00629910(stage, pose, 0x006b5af3, pool, ebp, ch, self);
        if ((stage = U32(self, 0x5c)) != 0) rider_stage_0062aed0(stage, pose, 0x006b5b05, pool, ebp, self);
        if ((stage = U32(self, 0x60)) != 0) rider_stage_0062aed0(stage, pose, 0x006b5b1e, pool, ebp, self);
        if ((stage = U32(self, 0x64)) != 0) rider_stage_00636990(stage, pose, ik_x87_bits(U32(stage, 0x18)), 0x006b5b3c, pool, ebp, ch);
    }
    SU32(delta_out, 0, U32(delta, 0)); SU32(delta_out, 4, U32(delta, 4)); SU32(delta_out, 8, U32(delta, 8));
    ik_pose_release(ch, pool, 0x006b5b6c, 0x006b5b9f);
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x006b5870, rx_procedural_animator_eval)

/* 006b5cd0 ProceduralAnimator::vf3 (thiscall (rate); RET 4): rate -> +0x30, and into the stages +0x54 (+0x0c) and +0x58 (+0x04). */
void rx_procedural_animator_set_rate(rx_addr self, float rate) {
    const uint32_t bits = rx_fb(rate);
    const rx_addr a = U32(self, 0x54);
    SU32(self, 0x30, bits);
    if (a) SU32(a, 0x0c, bits);
    const rx_addr b = U32(self, 0x58);
    if (b) SU32(b, 0x04, bits);
}
RX_FUNCTION(0x006b5cd0, rx_procedural_animator_set_rate)

/* ---- ExpressionBoneAnimator ---- */

/* 006ad090 ExpressionBoneAnimator::vf4 (thiscall (character, root delta out, pose); RET 0xc). See the file comment. */
void rx_expression_bone_animator_eval(rx_addr self, rx_addr ch, rx_addr delta_out, rx_addr pose) {
    ANIM_FRAME_ENTER(A, 0x30);                     /* sub esp, 0x24; push ebx; push esi; push edi */
    const rx_addr delta = A - 0x10;
    SU32(A - 0x20, 0, self);
    ik_pose_reset(ch, pose, 0x006ad0ad);
    const rx_addr pool = ik_pose_acquire(ch, 0x006ad0b4);
    SU32(A - 0x1c, 0, pool);
    ik_pose_reset(ch, pool, 0x006ad0c3);
    ik_eval_child(U32(self, 0x28), ch, delta, pool, 0x006ad0d8);
    const uint32_t bones = U32(ch, 0x234);
    if ((int32_t)bones >= 4) SU32(A - 0x14, 0, (((bones - 4) >> 2) + 1) * 4);
    ik_pose_copy(pose, pool, bones);
    if (U8(self, 0x5c)) {
        /* for (it = first; it != last; ++it) (*it)->evaluate(pose, character), with the checked iterator's tests */
        rx_addr it = U32(self, 0x50);
        SU32(A - 0x14, 0, it);
        if (it > U32(self, 0x54)) ik_invalid_parameter(0x006ad303);
        const rx_addr cont = U32(self, 0x44);
        SU32(A - 0x18, 0, it);
        for (;;) {
            const rx_addr last = U32(self, 0x54);
            SU32(A - 0x14, 0, last);
            if (U32(self, 0x50) > last) ik_invalid_parameter(0x006ad326);
            if (cont == 0 || cont != U32(self, 0x44)) ik_invalid_parameter(0x006ad33b);
            if (U32(A - 0x18, 0) == last) break;
            rx_addr base;
            if (cont == 0) { ik_invalid_parameter(0x006ad34f); base = 0; } else base = U32(cont, 0);
            if (U32(A - 0x18, 0) >= U32(base, 0x10)) ik_invalid_parameter(0x006ad360);
            const rx_addr ctrl = U32(U32(A - 0x18, 0), 0);
            uint32_t a[2] = {pose, ch};
            ik_call(U32(U32(ctrl, 0), 0), RX_THISCALL, ctrl, 2, a, 0x006ad371);
            if (cont == 0) { ik_invalid_parameter(0x006ad37b); base = 0; } else base = U32(cont, 0);
            if (U32(A - 0x18, 0) >= U32(base, 0x10)) ik_invalid_parameter(0x006ad38c);
            SU32(A - 0x18, 0, U32(A - 0x18, 0) + 4);
        }
    }
    SU32(delta_out, 0, U32(delta, 0)); SU32(delta_out, 4, U32(delta, 4)); SU32(delta_out, 8, U32(delta, 8));
    ik_pose_release(ch, pool, 0x006ad3ce, 0x006ad401);
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x006ad090, rx_expression_bone_animator_eval)

/* 006ac630 ExpressionBoneAnimator::vf3 (thiscall (rate); RET 4): forwards the rate (through the x87) to the child +0x28. */
void rx_expression_bone_animator_set_rate(rx_addr self, float rate) {
    const rx_addr child = U32(self, 0x28);
    if (child) anim_set_rate_child(child, ik_x87_bits(rx_fb(rate)), 0x006ac648);
}
RX_FUNCTION(0x006ac630, rx_expression_bone_animator_set_rate)

/* ---- FeatherstoneAnimator ---- */

/* 006afa50 FeatherstoneAnimator::vf4 (thiscall (character, root delta out, pose); RET 0xc): `jmp` to the child +0x30's vf4 with
 * the same arguments and return address (the child's frame is where it would be had the caller called it). */
void rx_featherstone_animator_eval(rx_addr self, rx_addr ch, rx_addr delta_out, rx_addr pose) {
    const rx_addr child = U32(self, 0x30), target = U32(U32(child, 0), 0x10);
    anim_eval_fn f = RX_FN(target, anim_eval_fn);
    if (f) { f(child, ch, delta_out, pose); return; }
    /* the original on the same stack: drop the return address and arguments, let the call push them again, restore the slot */
    const rx_addr sp = rx_sp(), ret = U32(sp, 0);
    uint32_t a[3] = {ch, delta_out, pose};
    rx_set_sp(sp + 16);
    rx_call_original(target, RX_THISCALL, child, 3, a);
    SU32(sp, 0, ret);
    rx_set_sp(sp);
}
RX_FUNCTION(0x006afa50, rx_featherstone_animator_eval)
