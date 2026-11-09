// world_cam_ramp.cpp -- oracle adapters for core/game/world_cam_ramp.c (group rx_world).
#include "oracle.h"
extern "C" {
#include "../game/world_cam_ramp.h"
}
using namespace reflex_port;
using rx_oracle::f32;

// 0067f3c0 (XMM2 = the limit, stack the base value; RET 4). Void.
RX_PORT(0x0067f3c0, CamRampInit, "rx_world", 0) { rx_cam_ramp_init(f32(uint32_t(m.xmm[2].lo)), m.arg(0)); return m.ret(4); }
// 00674d90 (void)
RX_PORT(0x00674d90, CamRampSetup, "rx_world", 0) { rx_cam_ramp_setup(); return m.ret(0); }
