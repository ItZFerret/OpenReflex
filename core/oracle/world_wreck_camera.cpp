// world_wreck_camera.cpp -- oracle adapters for core/game/world_wreck_camera.c (group rx_world).
#include "oracle.h"
extern "C" {
#include "../game/world_wreck.h"
}
using namespace reflex_port;
using rx_oracle::f32;

RX_PORT(0x00649bc0, MXThirdPersonCamera_vf22, "rx_world", OUT_EAX) { return m.ret(0, rx_world_third_person_camera_vf22(m.r[1])); }
RX_PORT(0x006467f0, Camera_ModeIndex, "rx_world", OUT_EAX) { return m.ret(0, rx_world_camera_mode_index(m.r[0])); }
// 006510a0 / 00651320 (EDI = camera; stdcall v; RET 4): AL = started; the upper bytes of EAX are the camera's (left by the start
// call) or the vector's (the early exit); ECX = the security cookie of its epilogue check.
RX_PORT(0x006510a0, Camera_ImpactShake, "rx_world", OUT_EAX | OUT_ECX) {
    const uint32_t cam = m.r[7], v = m.arg(0);
    const int r = rx_world_camera_impact_shake(cam, v);
    m.r[1] = U32(0, 0x009b45e4);
    return m.ret(4, r ? ((cam & 0xffffff00u) | 1u) : (v & 0xffffff00u));
}
RX_PORT(0x00651320, Camera_ImpactKick, "rx_world", OUT_EAX | OUT_ECX) {
    const uint32_t cam = m.r[7], v = m.arg(0);
    const int r = rx_world_camera_impact_kick(cam, v);
    m.r[1] = U32(0, 0x009b45e4);
    return m.ret(4, r ? ((cam & 0xffffff00u) | 1u) : (v & 0xffffff00u));
}
// 006514e0 (EDI = camera; stdcall impact vector; RET 4; AL = started): EAX = the camera (started) or the vector (not) with AL set;
// EDX = the second kick vector's address when started (Camera_StartKick leaves it), else unchanged.
RX_PORT(0x006514e0, Camera_WreckKick, "rx_world", OUT_EAX | OUT_EDX) {
    const uint32_t cam = m.r[7], v = m.arg(0), entry = m.r[4];
    const int r = rx_world_camera_wreck_kick(cam, v);
    if (r) m.r[2] = (((entry - 4) & ~15u) - 0x40) + 0x20;
    return m.ret(4, r ? ((cam & 0xffffff00u) | 1u) : (v & 0xffffff00u));
}
// 00651940 (stdcall camera, kick, strength, duration; EDX = kick 2; RET 0x10): EAX = camera, ECX = kick.
RX_PORT(0x00651940, Camera_StartKick, "rx_world", OUT_EAX | OUT_ECX) {
    rx_world_camera_start_kick(m.arg(0), m.arg(1), f32(m.arg(2)), f32(m.arg(3)), m.r[2]);
    m.r[1] = m.arg(1);
    return m.ret(0x10, m.arg(0));
}
// 006516a0 (stdcall camera, scale, duration; ECX / EDX = shake vectors; RET 0xc): EAX = camera, ECX = scale.
RX_PORT(0x006516a0, Camera_StartShake, "rx_world", OUT_EAX | OUT_ECX) {
    rx_world_camera_start_shake(m.arg(0), m.arg(1), f32(m.arg(2)), m.r[1], m.r[2]);
    m.r[1] = m.arg(1);
    return m.ret(0xc, m.arg(0));
}
RX_PORT(0x00653300, MXThirdPersonCamera_vf34, "rx_world", OUT_ECX) {
    rx_world_third_person_camera_vf34(m.r[1], m.arg(0), f32(m.arg(1)));
    m.r[1] = U32(0, 0x009b45e4);
    return m.ret(8);
}
RX_PORT(0x00651000, MXThirdPersonCamera_vf33, "rx_world", 0) {
    rx_world_third_person_camera_vf33(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3));
    return m.ret(0x10);
}
