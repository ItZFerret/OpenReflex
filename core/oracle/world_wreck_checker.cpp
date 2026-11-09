// world_wreck_checker.cpp -- oracle adapters for core/game/world_wreck_checker.c (group rx_world).
#include "oracle.h"
extern "C" {
#include "../game/world_wreck.h"
}
using namespace reflex_port;
using rx_oracle::bits;
using rx_oracle::f32;
static uint32_t xmm_lane0(reflex_vm::Machine& m, int i) { return uint32_t(m.xmm[i].lo); }

RX_PORT(0x00637ee0, WreckChecker_TypeClass, "rx_world", OUT_EAX) {
    if (m.r[1] > 0x22) return reflex_vm::RUN_ORIGINAL;   // the original's jump table has 35 entries
    return m.ret(0, rx_world_wreck_checker_type_class(m.r[1]));
}
// 00637ff0 (ECX = index, EAX = out): xmm0 = the weight (movss: upper lanes zero).
RX_PORT(0x00637ff0, WreckChecker_ClassAnim, "rx_world", OUT_XMM0) {
    if (m.r[1] > 0x13) return reflex_vm::RUN_ORIGINAL;
    m.xmm[0].lo = bits(rx_world_wreck_checker_class_anim(m.r[1], m.r[0])); m.xmm[0].hi = 0;
    return m.ret(0);
}
// 00638150 (stdcall a, b, c; RET 0xc): xmm0 lane 0 = the result (the abs call leaves the upper lanes of c).
RX_PORT(0x00638150, WreckChecker_Excess, "rx_world", OUT_XMM0) {
    const float r = rx_world_wreck_checker_excess(f32(m.arg(0)), f32(m.arg(1)), f32(m.arg(2)));
    m.xmm[0].lo = bits(r); m.xmm[0].hi = 0;
    return m.ret(0xc);
}
// 006373e0 (stdcall checker, dt; RET 8): ECX = the security cookie its epilogue check leaves.
RX_PORT(0x006373e0, WreckChecker_Update, "rx_world", OUT_ECX) {
    rx_world_wreck_checker_update(m.arg(0), f32(m.arg(1)));
    m.r[1] = U32(0, 0x009b45e4);
    return m.ret(8);
}
RX_PORT(0x00637320, WreckChecker_vf0, "rx_world", 0) {
    rx_world_wreck_checker_vf0(m.r[1], f32(m.arg(0)));
    return m.ret(4);
}
RX_PORT(0x006367b0, FUN_006367b0, "rx_world", 0) {
    rx_world_wreck_state_ramp_to(m.r[1], f32(xmm_lane0(m, 2)));
    return m.ret(0);
}
RX_PORT(0x00636870, FUN_00636870, "rx_world", 0) {
    rx_world_wreck_state_advance(m.r[6], f32(m.arg(0)));
    return m.ret(4);
}
RX_PORT(0x00632aa0, FUN_00632aa0, "rx_world", 0) {
    rx_world_wreck_state_react(m.r[1], m.r[6]);
    return m.ret(0);
}
