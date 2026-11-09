// bike_remote.cpp -- oracle adapters for core/game/bike_remote.c (group rx_bike): the remote simulation's record application 00432be0,
// the jump 004331d0 and the track filters. No reference world has a remote (type 1) simulation; the probe
// core/oracle/bike_remote_probe.cpp drives 00432be0 on the live rider's simulation.
// Scratch registers: every adapter restores them to their entry values (bike_oracle.h); the callers (00432be0 / 0042eb20 / 0042ec50
// and 0045d510 / 0045ff71) read none of the scratch registers these functions leave (EAX = the track where the original loads it).
#include "bike_oracle.h"
extern "C" {
#include "../game/bike_remote.h"
}
using namespace bike_oracle;

#define G "rx_bike"
// 00432be0 (ECX = the record, stack S; RET 4)
RX_PORT(0x00432be0, Remote_ApplyRecord, G, 0) {
    Scratch s(m);
    bike_remote_apply_record(m.r[1], m.arg(0));
    s.restore(m);
    return m.ret(4);
}
// 004331d0 (EDI = the record, stack S; RET 4)
RX_PORT(0x004331d0, Remote_JumpToRecord, G, 0) {
    Scratch s(m);
    bike_remote_jump_to_record(m.r[7], m.arg(0));
    s.restore(m);
    return m.ret(4);
}
// 0042e660 (stdcall track; RET 4) -> EAX = track
RX_PORT(0x0042e660, Track_Reset, G, OUT_EAX) {
    Scratch s(m);
    const uint32_t r = bike_track_reset(m.arg(0));
    s.restore(m);
    m.r[0] = r;
    return m.ret(4);
}
// 0042e6d0 (ECX = the value, stack track; RET 4) -> EAX = track
RX_PORT(0x0042e6d0, Track_ResetTo, G, OUT_EAX) {
    Scratch s(m);
    const uint32_t r = bike_track_reset_to(m.r[1], m.arg(0));
    s.restore(m);
    m.r[0] = r;
    return m.ret(4);
}
// 0042e720 (ECX = the key, xmm2 = the time, stack track; RET 4) -> EAX = track
RX_PORT(0x0042e720, Track_AddKey, G, OUT_EAX) {
    Scratch s(m);
    const uint32_t track = m.arg(0);
    bike_track_add_key(m.r[1], xmm_f(m, 2), track);
    s.restore(m);
    m.r[0] = track;
    return m.ret(4);
}
// 0042e840 (ECX = the output, xmm1 = the time, stack track; RET 4) -> EAX = track
RX_PORT(0x0042e840, Track_Evaluate, G, OUT_EAX) {
    Scratch s(m);
    const uint32_t track = m.arg(0);
    bike_track_evaluate(m.r[1], xmm_f(m, 1), track);
    s.restore(m);
    m.r[0] = track;
    return m.ret(4);
}
// 0042ee50 (EAX = track, xmm0 = the value; RET 0)
RX_PORT(0x0042ee50, ScalarTrack_ResetTo, G, 0) {
    Scratch s(m);
    bike_scalar_track_reset_to(m.r[0], xmm_f(m, 0));
    s.restore(m);
    return m.ret(0);
}
// 0042ee90 (EAX = track, xmm0 = the time, xmm3 = the value; RET 0)
RX_PORT(0x0042ee90, ScalarTrack_AddKey, G, 0) {
    Scratch s(m);
    bike_scalar_track_add_key(m.r[0], xmm_f(m, 0), xmm_f(m, 3));
    s.restore(m);
    return m.ret(0);
}
// 0042e8e0 (ECX = track; RET 0) -> EAX = track
RX_PORT(0x0042e8e0, OrientTrack_Init, G, OUT_EAX) {
    Scratch s(m);
    const uint32_t r = bike_orient_track_init(m.r[1]);
    s.restore(m);
    m.r[0] = r;
    return m.ret(0);
}
// 0042e9f0 (EDX = track; RET 0)
RX_PORT(0x0042e9f0, OrientTrack_Reset, G, 0) {
    Scratch s(m);
    bike_orient_track_reset(m.r[2]);
    s.restore(m);
    return m.ret(0);
}
// 0042eb20 (EDX = track; stack key, time; RET 8)
RX_PORT(0x0042eb20, OrientTrack_AddKey, G, 0) {
    Scratch s(m);
    bike_orient_track_add_key(m.r[2], m.arg(0), argf(m, 1));
    s.restore(m);
    return m.ret(8);
}
// 0042ec50 (EDX = track, xmm5 = the time, ESI = the output; RET 0)
RX_PORT(0x0042ec50, OrientTrack_Evaluate, G, 0) {
    Scratch s(m);
    bike_orient_track_evaluate(m.r[2], xmm_f(m, 5), m.r[6]);
    s.restore(m);
    return m.ret(0);
}
