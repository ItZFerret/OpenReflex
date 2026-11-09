// anim_rider_play.cpp -- oracle adapters for core/game/anim_rider_play.c (group "rx_fx", agent "trickplayback").
#include "fx_oracle.h"
extern "C" {
#include "../game/anim_rider_play.h"
}
using namespace reflex_port;
using rx_oracle::f32;
using rx_oracle::bits;

/* 0083e170 (EAX = the animator, xmm0 = the time) -> EAX */
FX_PORT(0x0083e170, RiderClipLooping_0083e170, OUT_EAX) { m.r[0] = rx_rider_clip_looping_at(m.r[0], f32(m.xmm[0].lo)); return m.ret(0); }
/* 0083e230 (EAX = the animator, xmm1 = the time) -> xmm0 */
FX_PORT(0x0083e230, RiderClipEnd_0083e230, OUT_XMM0) {
    const float v = rx_rider_clip_end_at(m.r[0], f32(m.xmm[1].lo));
    m.xmm[0].lo = bits(v); m.xmm[0].hi = 0; return m.ret(0);
}
/* 0083e1d0 (EAX = the animator, xmm1 = the time) -> xmm0 */
FX_PORT(0x0083e1d0, RiderClipStart_0083e1d0, OUT_XMM0) {
    const float v = rx_rider_clip_start_at(m.r[0], f32(m.xmm[1].lo));
    m.xmm[0].lo = bits(v); m.xmm[0].hi = 0; return m.ret(0);
}
/* 0083e0e0 (ECX = the animator) */
FX_PORT(0x0083e0e0, RiderClipBlock_0083e0e0, 0) { rx_rider_clip_block_at(m.r[1]); return m.ret(0); }
/* 0063b5f0 (ESI = the clamped float; stdcall value*; RET 4) */
FX_PORT(0x0063b5f0, RiderClampedFloatSub_0063b5f0, 0) { rx_rider_clamped_float_sub(m.r[6], m.arg(0)); return m.ret(4); }
/* 006cd240 (ECX = the trick, EAX = the class) -> xmm0 */
FX_PORT(0x006cd240, RiderPerfLoopEnd_006cd240, OUT_XMM0) {
    const float v = rx_rider_perf_loop_end(m.r[1], m.r[0]);
    m.xmm[0].lo = bits(v); m.xmm[0].hi = 0; return m.ret(0);
}
/* 00621f40 (ESI = the rider) */
FX_PORT(0x00621f40, RiderCrashStep_00621f40, 0) { rx_rider_crash_step(m.r[6]); return m.ret(0); }
/* 00620a60 (stdcall ra, dt; RET 8) */
FX_PORT(0x00620a60, RiderCrashUpdate_00620a60, 0) { rx_rider_crash_update(m.arg(0), f32(m.arg(1))); return m.ret(8); }
/* 00622160 (ECX = the player; stdcall ra, trick; RET 8) */
FX_PORT(0x00622160, RiderCrashBegin_00622160, 0) { rx_rider_crash_begin(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
/* 00622460 (ECX = the player; stdcall ra, trick; RET 8) */
FX_PORT(0x00622460, RiderCrashResume_00622460, 0) { rx_rider_crash_resume(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
/* 00621fe0 / 006223e0 (ESI = the rider, EDI = the player, ECX = the trick record; plain RET) */
/* 00623060 (EAX = the rider controller; plain RET): the controller outputs of PhysicsStuntEnd kinds 3 / 4 */
FX_PORT(0x00623060, RiderStuntEndOutputs_00623060, 0) { rx_rider_stunt_end_outputs(m.r[0]); return m.ret(0); }
FX_PORT(0x00621fe0, RiderCrashRequestBegin_00621fe0, 0) { rx_rider_crash_request_begin(m.r[6], m.r[7], m.r[1]); return m.ret(0); }
FX_PORT(0x006223e0, RiderCrashRequestResume_006223e0, 0) { rx_rider_crash_request_resume(m.r[6], m.r[7], m.r[1]); return m.ret(0); }
