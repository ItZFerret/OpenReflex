// world_crowd_audio.cpp -- oracle adapters for core/game/world_crowd_audio.c (group rx_sfx, w-sfx): the crowd momentum block's state
// machine and its wander smoother / draw, the crowd type. The original's crowd frame update (00674e90) calls them every world frame.
#include "oracle.h"
extern "C" {
#include "../game/world_crowd_audio.h"
}
using namespace reflex_port;
using rx_oracle::f32;

#define G "rx_sfx"
// 0067f1c0 (ECX = smoother)
RX_PORT(0x0067f1c0, CrowdSmootherStart, G, 0) { rx_crowd_smoother_start(m.r[1]); return m.ret(0); }
// 0067f210 (EAX = smoother, float dt; RET 4)
RX_PORT(0x0067f210, CrowdSmootherStep, G, 0) { rx_crowd_smoother_step(m.r[0], f32(m.arg(0))); return m.ret(4); }
// 0067f6d0 (void)
RX_PORT(0x0067f6d0, CrowdWander, G, 0) { rx_crowd_wander(); return m.ret(0); }
// 0067f450 (float dt; RET 4)
RX_PORT(0x0067f450, CrowdMomentumTick, G, 0) { rx_crowd_momentum_tick(f32(m.arg(0))); return m.ret(4); }
// 00674680 -> EAX
RX_PORT(0x00674680, CrowdType, G, OUT_EAX) { m.r[0] = (uint32_t)rx_crowd_type(); return m.ret(0); }
