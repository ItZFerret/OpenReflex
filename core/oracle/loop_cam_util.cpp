// loop_cam_util.cpp -- oracle adapters for core/game/loop_cam_util.c (group "rx_loop").
#include "oracle.h"
extern "C" {
#include "../game/loop_cam.h"
}
using namespace reflex_port;
using rx_oracle::f32;
using rx_oracle::bits;

// xmm0 = (v, 0, 0, 0): a movss-loaded or stored-and-reloaded float
static void set_xmm0_ss(reflex_vm::Machine& m, float v) { m.xmm[0].lo = bits(v); m.xmm[0].hi = 0; }

// stack angle (ret plain), result xmm0
RX_PORT(0x0063c6a0, Cam_WrapAngle, "rx_loop", OUT_XMM0 | OUT_FPU) {
    set_xmm0_ss(m, rx_cam_wrap_angle(f32(m.arg(0)))); return m.ret(0);
}
// ESI = spring; stack dt, accel, max speed, k; ret 0x10
RX_PORT(0x0063c7c0, Cam_AngleSpring, "rx_loop", OUT_FPU) { rx_cam_angle_spring(m.r[6], f32(m.arg(0)), f32(m.arg(1)), f32(m.arg(2)), f32(m.arg(3))); return m.ret(0x10); }
RX_PORT(0x0063c8c0, Cam_Spring, "rx_loop", OUT_FPU) { rx_cam_spring(m.r[6], f32(m.arg(0)), f32(m.arg(1)), f32(m.arg(2)), f32(m.arg(3))); return m.ret(0x10); }
RX_PORT(0x0063c9b0, Cam_Vec3Spring, "rx_loop", OUT_FPU) { rx_cam_vec3_spring(m.r[6], f32(m.arg(0)), f32(m.arg(1)), f32(m.arg(2)), f32(m.arg(3))); return m.ret(0x10); }
// stack matrix, angle; plain ret (the caller pops)
RX_PORT(0x0063cf00, Cam_RotateMatrix, "rx_loop", OUT_FPU) { const float scratch = rx_cam_rotate_matrix(m.arg(0), f32(m.arg(1)));
    m.mem.write(m.r[4] + 8, 4, bits(scratch));   // the angle's stack slot (ESP + 8 at entry)
    return m.ret(0); }
// EAX = spring; result xmm0
RX_PORT(0x0063c7b0, Cam_SpringTarget, "rx_loop", OUT_XMM0) { set_xmm0_ss(m, rx_cam_spring_target(m.r[0])); return m.ret(0); }
RX_PORT(0x0063c8b0, Cam_SpringSpeed, "rx_loop", OUT_XMM0) { set_xmm0_ss(m, rx_cam_spring_speed(m.r[0])); return m.ret(0); }
RX_PORT(0x0063c8a0, Cam_SpringSnap, "rx_loop", OUT_XMM0) { rx_cam_spring_snap(m.r[0]); m.xmm[0].lo = 0; m.xmm[0].hi = 0; return m.ret(0); }
// EAX = object, xmm0 = value
RX_PORT(0x0063d920, Cam_SetFloat254, "rx_loop", 0) { rx_cam_set_float_254(m.r[0], f32(uint32_t(m.xmm[0].lo))); return m.ret(0); }
// EAX = source, ECX = destination
RX_PORT(0x0063c990, Cam_CopyVec3_10, "rx_loop", OUT_FPU) { rx_cam_copy_vec3_10(m.r[1], m.r[0]); return m.ret(0); }
