// anim_rider_b.cpp -- oracle adapters for core/game/anim_rider_b*.c.
#include "anim_oracle.h"
extern "C" {
#include "../game/anim_rider_b.h"
}
using namespace reflex_port;

// 0062aed0 (ESI = limb state, stack pose; RET 4). Void.
ANIM_PORT(0x0062aed0, RiderLimb_Evaluate, "rx_anim", 0) { rx_rider_limb_state_eval(m.r[6], m.arg(0)); return m.ret(4); }
// 0062ae60 (EAX = state, ECX = out). Void.
ANIM_PORT(0x0062ae60, RiderLimb_RestTarget, "rx_anim", 0) { rx_rider_limb_rest_target(m.r[0], m.r[1]); return m.ret(0); }
// 0062ab30 (EDI = state, stack out; RET 4). Void.
ANIM_PORT(0x0062ab30, RiderLimb_SwingTarget, "rx_anim", 0) { rx_rider_limb_swing_target(m.r[7], m.arg(0)); return m.ret(4); }
// 0062b0e0 / 0062b3f0 (EDI = state, stack pose; RET 4). Void.
ANIM_PORT(0x0062b0e0, RiderLimb_YawFixed, "rx_anim", 0) { rx_rider_limb_yaw_fixed(m.r[7], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x0062b3f0, RiderLimb_YawSigned, "rx_anim", 0) { rx_rider_limb_yaw_signed(m.r[7], m.arg(0)); return m.ret(4); }
// 0062b710 / 0062c840 (stdcall state, pose; RET 8). Void.
ANIM_PORT(0x0062b710, RiderLimb_SwingBlendA, "rx_anim", 0) { rx_rider_limb_swing_blend_a(m.arg(0), m.arg(1)); return m.ret(8); }
ANIM_PORT(0x0062c840, RiderLimb_SwingBlendB, "rx_anim", 0) { rx_rider_limb_swing_blend_b(m.arg(0), m.arg(1)); return m.ret(8); }
ANIM_PORT(0x0062d970, RiderLimb_Reach, "rx_anim", 0) { rx_rider_limb_reach(m.r[0], m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0x10); }
ANIM_PORT(0x0062fa90, RiderLimb_Rest, "rx_anim", 0) { rx_rider_limb_rest(m.r[0], m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0x10); }
