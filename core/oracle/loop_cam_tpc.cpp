// loop_cam_tpc.cpp -- oracle adapters for core/game/loop_cam_tpc.c (group "rx_loop").
#include "oracle.h"
extern "C" {
#include "../game/loop_cam_tpc.h"
}
using namespace reflex_port;
using rx_oracle::f32;

// thiscall dt; ret 4
RX_PORT(0x0064c110, MXThirdPersonCamera_vf19, "rx_loop", 0) { rx_third_person_camera_vf19(m.r[1], f32(m.arg(0))); return m.ret(4); }
// ECX = camera, stack controller index, dt; ret 8
RX_PORT(0x00649ed0, MXThirdPersonCamera_vf30, "rx_loop", 0) { rx_third_person_camera_vf30(m.r[1], m.arg(0), f32(m.arg(1))); return m.ret(8); }
// EAX = camera
RX_PORT(0x00650210, Cam_WaterReset, "rx_loop", 0) { rx_cam_water_reset(m.r[0]); return m.ret(0); }
