// anim_ik.cpp -- oracle adapters for core/game/anim_ik.c (InverseKinematicsAnimator and its chain solvers).
#include "anim_oracle.h"
extern "C" {
#include "../game/anim_ik.h"
}
using namespace reflex_port;

// The cluster's x87 code (006b0eb0's sums of squares) assumes the animation threads' control word 0x027f (53-bit precision,
// round to nearest): report any other once.
static void ik_check_cw(reflex_vm::Machine& m, const char* who) {
    static bool said = false;
    if ((m.fpu_cw & 0xf00) != 0x200 && !said) { said = true; std::fprintf(stderr, "rx_anim ik: %s runs with x87 CW %04x (core assumes 027f)\n", who, m.fpu_cw); }
}

// 006b0530 (ECX = skeleton, ESI = chain record, EDX = pose; stack dt; RET 4). Void; the caller reads no register after it.
ANIM_PORT(0x006b0530, IK_ChainCCD, "rx_anim", 0) {
    rx_ik_chain_ccd(m.r[1], m.r[6], m.r[2], rx_oracle::f32(m.arg(0)));
    return m.ret(4);
}
// 006b5300 InverseKinematicsAnimator::vf4 (thiscall (character, root delta out, pose); RET 0xc). Void.
ANIM_PORT(0x006b5300, InverseKinematicsAnimator_vf4, "rx_anim", 0) {
    ik_check_cw(m, "006b5300");
    rx_ik_animator_eval(m.r[1], m.arg(0), m.arg(1), m.arg(2));
    return m.ret(0xc);
}
// 006b57a0 InverseKinematicsAnimator::vf3 (thiscall (rate); RET 4). Void.
ANIM_PORT(0x006b57a0, InverseKinematicsAnimator_vf3, "rx_anim", 0) {
    rx_ik_animator_set_rate(m.r[1], rx_oracle::f32(m.arg(0)));
    return m.ret(4);
}
// 006b0eb0 (EAX = skeleton; stack: limb record, pose, dt; RET 0xc). Void; the caller reads no register after it.
ANIM_PORT(0x006b0eb0, IK_TwoBoneSolve, "rx_anim", 0) {
    ik_check_cw(m, "006b0eb0");
    rx_ik_two_bone_solve(m.r[0], m.arg(0), m.arg(1), rx_oracle::f32(m.arg(2)));
    return m.ret(0xc);
}
// 006b04d0 (ECX = IK animator; plain ret): with the IK on, EDX = the goal count and EAX = the last goal visited (when any);
// otherwise both are left as they were. xmm0..3 are scratch (callers read xmm4, which it keeps).
ANIM_PORT(0x006b04d0, IKAnimator_EngageGoals, "rx_anim", OUT_EAX | OUT_EDX) {
    const uint32_t self = m.r[1], on = U32(self, 0x70);
    const uint32_t n = rx_ik_animator_engage_goals(self);
    if (on) { m.r[2] = n; if (n) m.r[0] = U32(self, 0x64) + 0x80u * (n - 1); }
    return m.ret(0);
}
