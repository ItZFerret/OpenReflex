// world_node_input.cpp -- oracle adapters for core/game/world_node_input.c (group rx_world).
#include "oracle.h"
#include "f80/f80.h"
extern "C" {
#include "../game/world_node.h"
}
using namespace reflex_port;
using rx_oracle::f32;

static void push_st0(reflex_vm::Machine& m, float v) { m.fpu.push_back(reflex_f80::from_f32(rx_oracle::bits(v))); }

// KeyMap (thiscall (mask, key), RET 8)
RX_PORT(0x00811420, KeyMap_vf1, "rx_world", OUT_EAX) { return m.ret(8, rx_world_keymap_latched(m.r[1], m.arg(0), m.arg(1))); }
RX_PORT(0x008115f0, KeyMap_vf2, "rx_world", OUT_EAX) { return m.ret(8, rx_world_keymap_down(m.r[1], m.arg(0), m.arg(1))); }
RX_PORT(0x00811710, KeyMap_vf4, "rx_world", OUT_FPU) {
    push_st0(m, rx_world_keymap_value_at(m.r[1], m.arg(0), m.arg(1), m.r[4] + 8));
    return m.ret(8);
}
RX_PORT(0x008115e0, KeyMap_vf8, "rx_world", OUT_EAX) { return m.ret(8, rx_world_keymap_code(m.r[1], m.arg(0), m.arg(1))); }
RX_PORT(0x00811ab0, CompoundKeyMap_vf2, "rx_world", OUT_EAX) {
    return m.ret(8, rx_world_compound_keymap_down(m.r[1], m.arg(0), m.arg(1)));
}
RX_PORT(0x00811b50, CompoundKeyMap_vf4, "rx_world", OUT_FPU) {
    push_st0(m, rx_world_compound_keymap_value(m.r[1], m.arg(0), m.arg(1)));
    return m.ret(8);
}
RX_PORT(0x00811db0, CumulativeKeyMap_vf4, "rx_world", OUT_FPU) {
    push_st0(m, rx_world_cumulative_keymap_value(m.r[1], m.arg(0), m.arg(1)));
    return m.ret(8);
}
// XenonInputManager
RX_PORT(0x00814c00, XenonInputManager_vf31, "rx_world", OUT_EAX) {
    return m.ret(8, rx_world_xenon_key_pressed(m.r[1], m.arg(0), m.arg(1)));
}
RX_PORT(0x00814e60, XenonInputManager_vf37, "rx_world", OUT_EAX) {
    return m.ret(8, rx_world_xenon_key_held(m.r[1], m.arg(0), m.arg(1)));
}
RX_PORT(0x00814cd0, XenonInputManager_vf33, "rx_world", OUT_EAX) {
    return m.ret(0x14, rx_world_xenon_button(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4)));
}
RX_PORT(0x00814f30, XenonInputManager_vf41, "rx_world", OUT_EAX) {
    return m.ret(0xc, rx_world_xenon_control(m.r[1], m.arg(0), m.arg(1), m.arg(2)));
}
RX_PORT(0x008151e0, XenonInputManager_vf46, "rx_world", 0) {
    rx_world_xenon_set_rumble(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4));
    return m.ret(0x14);
}
// 0083d3d0 (ECX = out, xmm2 = from, xmm1 = to; stdcall clip; RET 4): EDX = count, ECX = past the last copied event.
RX_PORT(0x0083d3d0, FUN_0083d3d0, "rx_world", OUT_EDX | OUT_ECX) {
    const uint32_t out = m.r[1];
    const uint32_t n = rx_world_clip_events_between(out, m.arg(0), f32(uint32_t(m.xmm[2].lo)), f32(uint32_t(m.xmm[1].lo)));
    m.r[2] = n;
    m.r[1] = out + 12 * n;
    return m.ret(4);
}
