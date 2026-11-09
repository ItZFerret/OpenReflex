// anim_animators.cpp -- oracle adapters for core/game/anim_animators.c.
#include "anim_oracle.h"
extern "C" {
#include "../game/anim.h"
}
using namespace reflex_port;

// 00834390 (EAX = blend animator, EDI = pose; stack character, delta out, which; RET 0xc). Void.
ANIM_PORT(0x00834390, BlendAnimator_EvalOneSide, "rx_anim", 0) {
    rx_blend_animator_eval_one(m.r[0], m.r[7], m.arg(0), m.arg(1), m.arg(2));
    return m.ret(0xc);
}
// 008349c0 AdditiveAnimator::vf4 (thiscall; RET 0xc). Void.
ANIM_PORT(0x008349c0, AdditiveAnimator_vf4, "rx_anim", 0) {
    rx_additive_animator_eval(m.r[1], m.arg(0), m.arg(1), m.arg(2));
    return m.ret(0xc);
}
// 00833080 BlendAnimator::vf4 (thiscall; RET 0xc). Void.
ANIM_PORT(0x00833080, BlendAnimator_vf4, "rx_anim", 0) {
    rx_blend_animator_eval(m.r[1], m.arg(0), m.arg(1), m.arg(2));
    return m.ret(0xc);
}
// 0083bc70 MotionAnimator::vf4 (thiscall; RET 0xc). Void.
ANIM_PORT(0x0083bc70, MotionAnimator_vf4, "rx_anim", 0) {
    rx_motion_animator_eval(m.r[1], m.arg(0), m.arg(1), m.arg(2));
    return m.ret(0xc);
}
// 0083c2f0 Pose_Mirror (cdecl pose). Void.
ANIM_PORT(0x0083c2f0, Pose_Mirror, "rx_anim", 0) {
    rx_pose_mirror(m.arg(0));
    return m.ret(0);
}
// 0083c530 Pose_CopyBone (ECX = source, EAX = destination). Leaves EAX = destination.
ANIM_PORT(0x0083c530, Pose_CopyBone, "rx_anim", OUT_EAX) {
    rx_pose_copy_bone(m.r[1], m.r[0]);
    return m.ret(0);
}
// 00842630 (ESI = map, EAX = key*; EAX = the bucket index). ECX / EDX are clobbered, not read by the callers.
ANIM_PORT(0x00842630, HashMap_Bucket, "rx_anim", OUT_EAX) { m.r[0] = rx_hashmap_bucket(m.r[6], m.r[0]); return m.ret(0); }
// 008424e0 (EAX = map; stdcall iterator out, key*; RET 8; EAX = out).
ANIM_PORT(0x008424e0, HashMap_Find, "rx_anim", OUT_EAX) { m.r[0] = rx_hashmap_find(m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }

// 0083c9c0 SwitchAnimator_StartTransition (ESI = switch animator). Void.
ANIM_PORT(0x0083c9c0, SwitchAnimator_StartTransition, "rx_anim", 0) {
    rx_switch_animator_start_transition(m.r[6]);
    return m.ret(0);
}
// 0083cad0 SwitchAnimator::vf4 (thiscall; RET 0xc). Void.
ANIM_PORT(0x0083cad0, SwitchAnimator_vf4, "rx_anim", 0) {
    rx_switch_animator_eval(m.r[1], m.arg(0), m.arg(1), m.arg(2));
    return m.ret(0xc);
}
// 0083b3e0 MotionAnimator::vf11 (thiscall name, out; RET 8). EAX = found (al) with the strcmp's upper bytes.
ANIM_PORT(0x0083b3e0, MotionAnimator_vf11, "rx_anim", OUT_EAX) {
    m.r[0] = rx_motion_animator_get_param(m.r[1], m.arg(0), m.arg(1));
    return m.ret(8);
}
// 00834570 BlendAnimator::vf11 (thiscall name, out; RET 8). EAX as above.
ANIM_PORT(0x00834570, BlendAnimator_vf11, "rx_anim", OUT_EAX) {
    m.r[0] = rx_blend_animator_get_param(m.r[1], m.arg(0), m.arg(1));
    return m.ret(8);
}
// 0083ce40 SwitchAnimator::vf11 (thiscall name, out; RET 8). EAX as above.
ANIM_PORT(0x0083ce40, SwitchAnimator_vf11, "rx_anim", OUT_EAX) {
    m.r[0] = rx_switch_animator_get_param(m.r[1], m.arg(0), m.arg(1));
    return m.ret(8);
}
// 0083d430 PerformanceAnimator::vf11 (thiscall name, out; RET 8). EAX as above.
ANIM_PORT(0x0083d430, PerformanceAnimator_vf11, "rx_anim", OUT_EAX) {
    m.r[0] = rx_performance_animator_get_param(m.r[1], m.arg(0), m.arg(1));
    return m.ret(8);
}
// 008d8db0 Animator::vf11 (base, thiscall name, out; RET 8): al = 0, the rest of EAX stays the caller's.
ANIM_PORT(0x008d8db0, Animator_vf11, "rx_anim", OUT_EAX) {
    m.r[0] = (m.r[0] & 0xffffff00u) | (rx_animator_get_param(m.r[1], m.arg(0), m.arg(1)) & 0xffu);
    return m.ret(8);
}
// 0083e290 PerformanceAnimator::vf4 (thiscall; RET 0xc). Void.
ANIM_PORT(0x0083e290, PerformanceAnimator_vf4, "rx_anim", 0) {
    rx_performance_animator_eval(m.r[1], m.arg(0), m.arg(1), m.arg(2));
    return m.ret(0xc);
}
// 0083e930 PerformanceAnimator additive clip (EAX = the clip animator; stack character, pose, weight; RET 0xc). Void.
ANIM_PORT(0x0083e930, PerformanceAnimator_AdditiveClip, "rx_anim", 0) {
    rx_performance_additive_clip(m.r[0], m.arg(0), m.arg(1), rx_oracle::f32(m.arg(2)));
    return m.ret(0xc);
}
