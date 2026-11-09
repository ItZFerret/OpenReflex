// loop_rider.cpp -- oracle adapters for core/game/loop_rider.c (group "rx_loop").
#include "oracle.h"
extern "C" {
#include "../game/loop_rider.h"
}
using namespace reflex_port;

// ESI = out; stdcall (state, pose, bone); ret 0xc
RX_PORT(0x00632b00, Rider_BoneTargetDir, "rx_loop", 0) { rx_rider_bone_target_dir(m.r[6], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
// EAX = state; stdcall (pose, hit, normal); ret 0xc; EAX
RX_PORT(0x00632dc0, Rider_ReachProbe, "rx_loop", OUT_EAX) { m.r[0] = rx_rider_reach_probe(m.r[0], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
// EDI = pose; stdcall (stage, value); ret 8 (the callers read no register)
RX_PORT(0x00636990, Rider_StageTwist, "rx_loop", 0) { rx_rider_stage_twist_fr(m.arg(0), rx_oracle::f32(m.arg(1)), m.r[7], m.r[3], m.r[5], m.r[6]); return m.ret(8); }
// stdcall (limb state, pose); ret 8 (the caller 0062aed0 reads no register)
RX_PORT(0x00635e10, Rider_LimbRelease, "rx_loop", 0) { rx_rider_limb_release(m.arg(0), m.arg(1)); return m.ret(8); }
// stdcall (limb state, pose, contact point, contact normal); ret 0x10 (the caller 0062aed0 reads no register)
RX_PORT(0x00633010, Rider_LimbReachContact, "rx_loop", 0) { rx_rider_limb_reach_contact(m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0x10); }
