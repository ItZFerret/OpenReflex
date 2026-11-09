// loop_cam_parts_view.cpp -- oracle adapters for core/game/loop_cam_parts_view.c (group rx_loop): the Garage parts-view camera's
// update 00656070 and its helpers. The reference worlds never update that camera (the Garage UI); the probe
// core/oracle/loop_cam_parts_view_probe.cpp drives the original on the MXGarage's camera (MXGame + 0x15580).
#include "oracle.h"
extern "C" {
#include "../game/loop_cam_parts_view.h"
}
using namespace reflex_port;
using rx_oracle::f32;

#define G "rx_loop"
// 00656070 MXUIPartsViewCamera::vf19 (ECX = camera, stack dt; RET 4)
RX_PORT(0x00656070, PartsViewCamera_vf19, G, 0) {
    rx_parts_view_camera_update(m.r[1], f32(m.arg(0)));
    return m.ret(4);
}
// 00658550 / 00658d80 (EAX = camera, ESI = out; RET 0) -> EAX = out
RX_PORT(0x00658550, PartsView_BlendOffset, G, OUT_EAX) {
    m.r[0] = rx_parts_view_blend_offset(m.r[0], m.r[6]);
    return m.ret(0);
}
RX_PORT(0x00658d80, PartsView_BlendTarget, G, OUT_EAX) {
    m.r[0] = rx_parts_view_blend_target(m.r[0], m.r[6]);
    return m.ret(0);
}
// 006596a0 (xmm1 = t, stack duration; RET 4) -> xmm0 lane 0 (lanes 1..3: xmm1's, through the movaps chain)
RX_PORT(0x006596a0, EaseOutQuintic, G, OUT_XMM0) {
    const float r = rx_ease_out_quintic(f32(uint32_t(m.xmm[1].lo)), f32(m.arg(0)));
    m.xmm[0].lo = (m.xmm[1].lo & 0xffffffff00000000ull) | rx_oracle::bits(r);
    m.xmm[0].hi = m.xmm[1].hi;
    return m.ret(4);
}
// 0063e160 (ESI = out, EAX = target, ECX = eye; RET 0) -> EAX = out
RX_PORT(0x0063e160, LookAtMatrix, G, OUT_EAX) {
    m.r[0] = rx_look_at_matrix(m.r[6], m.r[0], m.r[1]);
    return m.ret(0);
}
// 004073e0 (EAX = row, ECX = out, stack matrix; cdecl) -> EAX = the row's address, EDX = the matrix
RX_PORT(0x004073e0, MatrixRow3, G, OUT_EAX | OUT_EDX) {
    const uint32_t matrix = m.arg(0);
    m.r[0] = rx_matrix_row3(m.r[1], matrix, m.r[0]);
    m.r[2] = matrix;
    return m.ret(0);
}
// 0063c890 (EAX = p, xmm0 = v; RET 0)
RX_PORT(0x0063c890, StoreFloat, G, 0) {
    rx_store_float(m.r[0], f32(uint32_t(m.xmm[0].lo)));
    return m.ret(0);
}
