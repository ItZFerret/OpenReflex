// anim_rider_a.cpp -- oracle adapters for core/game/anim_rider_a*.c (the rider's procedural pose).
#include "anim_oracle.h"
extern "C" {
#include "../game/anim_rider_a.h"
}
using namespace reflex_port;
using rx_oracle::f32;

// 00623bd0 (ECX = RiderAnimation, ESI = position out; stack: rotation out; RET 4). Void.
ANIM_PORT(0x00623bd0, RiderAnim_HeadTransform, "rx_anim", 0) {
    rx_rider_anim_head_transform(m.r[1], m.r[6], m.arg(0));
    return m.ret(4);
}
// 00623c80 (fastcall ECX = RiderAnimation, EDX = axis 0; stack: rate, dt, amount, axis 1, axis 2; RET 0x14). Void.
ANIM_PORT(0x00623c80, RiderAnim_NoiseWobble, "rx_anim", 0) {
    rx_rider_anim_noise_wobble(m.r[1], m.r[2], f32(m.arg(0)), f32(m.arg(1)), f32(m.arg(2)), m.arg(3), m.arg(4));
    return m.ret(0x14);
}
// 00623f30 (stdcall RiderAnimation; RET 4). Void.
ANIM_PORT(0x00623f30, RiderAnim_Nod, "rx_anim", 0) {
    rx_rider_anim_nod(m.arg(0));
    return m.ret(4);
}
// 00629910 (EAX = lean controller; stack: pose; RET 4). Void.
ANIM_PORT(0x00629910, RiderLean_Eval, "rx_anim", 0) {
    rx_rider_lean_eval(m.r[0], m.arg(0));
    return m.ret(4);
}
// 00629a60 (ECX = bone quaternion; stack: lean controller; RET 4). Void.
ANIM_PORT(0x00629a60, RiderLean_RotateBone, "rx_anim", 0) {
    rx_rider_lean_rotate_bone(m.r[1], m.arg(0));
    return m.ret(4);
}

// 006259c0 (ECX = character, ESI = limb controller; stack: pose; RET 4). Void.
ANIM_PORT(0x006259c0, RiderLimbs_Eval, "rx_anim", 0) {
    rx_rider_limbs_eval(m.r[6], m.r[1], m.arg(0));
    return m.ret(4);
}
// 00626740 (fastcall ECX = target, EDX = limb controller; stack: parent record, bone record, weight, smoothers 0..2; RET 0x18).
// No result: its only caller (006259c0) reads none of EAX / EDX afterwards.
ANIM_PORT(0x00626740, RiderLimb_Solve, "rx_anim", 0) {
    rx_rider_limb_solve(m.r[1], m.r[2], m.arg(0), m.arg(1), f32(m.arg(2)), m.arg(3), m.arg(4), m.arg(5));
    return m.ret(0x18);
}
