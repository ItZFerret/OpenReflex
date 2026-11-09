// anim_ik_anim.cpp -- oracle adapters for core/game/anim_ik_anim.c (Procedural / ExpressionBone / Featherstone animators).
#include "anim_oracle.h"
extern "C" {
#include "../game/anim_ik.h"
}
using namespace reflex_port;

// The animators' vf4: thiscall (character, root delta out, pose), RET 0xc; void. vf3: thiscall (rate), RET 4; void.
ANIM_PORT(0x006b5870, ProceduralAnimator_vf4, "rx_anim", 0) {
    rx_procedural_animator_eval(m.r[1], m.arg(0), m.arg(1), m.arg(2));
    return m.ret(0xc);
}
ANIM_PORT(0x006b5cd0, ProceduralAnimator_vf3, "rx_anim", 0) {
    rx_procedural_animator_set_rate(m.r[1], rx_oracle::f32(m.arg(0)));
    return m.ret(4);
}
ANIM_PORT(0x006ad090, ExpressionBoneAnimator_vf4, "rx_anim", 0) {
    rx_expression_bone_animator_eval(m.r[1], m.arg(0), m.arg(1), m.arg(2));
    return m.ret(0xc);
}
ANIM_PORT(0x006ac630, ExpressionBoneAnimator_vf3, "rx_anim", 0) {
    rx_expression_bone_animator_set_rate(m.r[1], rx_oracle::f32(m.arg(0)));
    return m.ret(4);
}
// 006afa50 FeatherstoneAnimator::vf4: a tail jump; the child returns to the caller with RET 0xc.
ANIM_PORT(0x006afa50, FeatherstoneAnimator_vf4, "rx_anim", 0) {
    rx_featherstone_animator_eval(m.r[1], m.arg(0), m.arg(1), m.arg(2));
    return m.ret(0xc);
}
