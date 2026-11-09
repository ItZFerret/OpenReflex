// loop_cam_shake.cpp -- oracle adapters for core/game/loop_cam_shake.c (group "rx_loop").
#include "oracle.h"
extern "C" {
#include "../game/loop_cam_shake.h"
}
using namespace reflex_port;
using rx_oracle::f32;

RX_PORT(0x006519b0, Cam_Shake, "rx_loop", 0) { rx_cam_shake(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00652800, Cam_NoiseShake, "rx_loop", 0) { rx_cam_noise_shake(m.r[6], f32(m.arg(0))); return m.ret(4); }
