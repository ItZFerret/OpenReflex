// race_math.cpp -- oracle adapters for core/game/race_math.c.
#include "oracle.h"
extern "C" {
#include "../game/race.h"
}
using namespace reflex_port;
using rx_oracle::f32;

// 005084f0 (EAX = vehicle, ECX = vector; ret).
RX_PORT(0x005084f0, Race_ToVehicleFrame, "rx_race", 0) { rx_race_to_vehicle_frame(m.r[0], m.r[1]); return m.ret(0); }
// 005089d0 (ECX = record) -> EAX = record.
RX_PORT(0x005089d0, Race_PoseReset, "rx_race", OUT_EAX) { m.r[0] = rx_race_pose_reset(m.r[1]); m.r[1] &= 0xffffff00u; return m.ret(0); }
// 0050b850 (ECX = point, EDX = b, arg 0 = a, arg 1 = radius, ESI = out; cdecl) -> EAX.
RX_PORT(0x0050b850, Race_PointNearSegment, "rx_race", OUT_EAX) {
    m.r[0] = uint32_t(rx_race_point_near_segment(m.r[1], m.arg(0), m.r[2], f32(m.arg(1)), m.r[6]));
    return m.ret(0);
}
// 0050b990 (EDX = p0, EAX = p1, arg 0 = q0, ESI = q1, EDI = out; cdecl) -> EAX; ECX = q0.
RX_PORT(0x0050b990, Race_SegmentsCrossXZ, "rx_race", OUT_EAX | OUT_ECX) {
    const uint32_t q0 = m.arg(0);
    m.r[0] = uint32_t(rx_race_segments_cross_xz(m.r[2], m.r[0], q0, m.r[6], m.r[7]));
    m.r[1] = q0;
    return m.ret(0);
}
// 0050a650 (EAX = node, EDI = out position, arg 0 = out direction, arg 1 = distance; RET 8).
RX_PORT(0x0050a650, Track_LookAhead, "rx_race", 0) {
    rx_track_look_ahead(m.r[0], m.r[7], m.arg(0), f32(m.arg(1)));
    return m.ret(8);
}
// 0050a8b0 (ESI = driver, arg 0 = side, arg 1 = distance, arg 2 = width; RET 0xc) -> EAX.
RX_PORT(0x0050a8b0, Race_RiderNear, "rx_race", OUT_EAX) {
    m.r[0] = uint32_t(rx_race_rider_near(m.r[6], int(m.arg(0)), f32(m.arg(1)), f32(m.arg(2))));
    return m.ret(0xc);
}
// 00509fc0 (arg 0 = driver, arg 1 = centre ray, EDI = left, ESI = right; RET 8).
RX_PORT(0x00509fc0, Race_ProbeRays, "rx_race", 0) {
    rx_race_probe_rays(m.arg(0), m.arg(1), m.r[7], m.r[6]);
    return m.ret(8);
}
// 0053ffb0 (arg 0 = parts; RET 4) -> EAX.
RX_PORT(0x0053ffb0, Race_PartsLoading, "rx_race", OUT_EAX) { m.r[0] = uint32_t(rx_race_parts_loading(m.arg(0))); return m.ret(4); }
// 0050a5f0 (arg 0 = node, arg 1 = out position, arg 2 = distance, ESI = out direction; RET 0xc).
RX_PORT(0x0050a5f0, Track_LookAheadXZ, "rx_race", 0) {
    rx_track_look_ahead_xz(m.arg(0), m.arg(1), f32(m.arg(2)), m.r[6]);
    return m.ret(0xc);
}
// 0050a360 (args driver, list, count, prefer; RET 0x10) -> EAX; the original keeps its result in argument slot 1.
RX_PORT(0x0050a360, Race_ChooseSection, "rx_race", OUT_EAX) {
    rx_addr slot = 0;
    m.r[0] = rx_race_choose_section(m.arg(0), m.arg(1), m.arg(2), m.arg(3), &slot);
    m.mem.write(m.r[4] + 8, 4, slot);
    return m.ret(0x10);
}
// 0050aa70 (EAX = driver, ECX = node) -> EAX.
RX_PORT(0x0050aa70, Race_OnTrackAhead, "rx_race", OUT_EAX) { m.r[0] = uint32_t(rx_race_on_track_ahead(m.r[0], m.r[1])); return m.ret(0); }
// 0050b150 (ECX = a, EAX = b, ESI = direction) -> xmm0 (upper lanes 0).
RX_PORT(0x0050b150, Race_HeadingFactor, "rx_race", OUT_XMM0) {
    const float r = rx_race_heading_factor(m.r[1], m.r[0], m.r[6]);
    m.xmm[0].lo = rx_oracle::bits(r); m.xmm[0].hi = 0;
    return m.ret(0);
}
// 0050b220 (ESI = driver, EDI = controls).
RX_PORT(0x0050b220, Race_LimitThrottleInTurns, "rx_race", 0) { rx_race_limit_throttle_in_turns(m.r[6], m.r[7]); return m.ret(0); }
// 0050b0f0 (xmm1 = x, args a, b, c, base; RET 0x10) -> xmm0 (upper lanes 0).
RX_PORT(0x0050b0f0, Race_PowDifference, "rx_race", OUT_XMM0) {
    const float x = rx_oracle::f32(uint32_t(m.xmm[1].lo));
    const float r = rx_race_pow_difference(x, f32(m.arg(0)), f32(m.arg(1)), f32(m.arg(2)), f32(m.arg(3)));
    m.xmm[0].lo = rx_oracle::bits(r); m.xmm[0].hi = 0;
    return m.ret(0x10);
}
// 00509e30 (ECX = driver, EDX = position) -> xmm0 (upper 0); EAX = next node, ECX = previous node.
RX_PORT(0x00509e30, Race_TrackLateral, "rx_race", OUT_EAX | OUT_ECX | OUT_XMM0) {
    const uint32_t d = m.r[1];
    const float r = rx_race_track_lateral(d, m.r[2]);
    m.r[0] = U32(d, 0x1c); m.r[1] = U32(d, 0x18);
    m.xmm[0].lo = rx_oracle::bits(r); m.xmm[0].hi = 0;
    return m.ret(0);
}
// 0050a570 (EAX = vector, ESI = out).
RX_PORT(0x0050a570, Race_FlatDirection, "rx_race", 0) { rx_race_flat_direction(m.r[0], m.r[6]); return m.ret(0); }
// 0050ae20 (ESI = driver, arg 0 = dt; RET 4) -> EAX. orig_frame = the original's aligned frame ((entry ESP - 4) & -16) - 0x60.
RX_PORT(0x0050ae20, Race_LeanRecover, "rx_race", OUT_EAX) {
    const uint32_t frame = ((m.r[4] - 4) & ~15u) - 0x60;
    m.r[0] = uint32_t(rx_race_lean_recover(m.r[6], rx_oracle::f32(m.arg(0)), frame));
    return m.ret(4);
}

// 00407430 (ECX = out, ESI = point, EAX = segment start, arg 0 = segment end; cdecl) -> xmm0 (upper lanes 0).
RX_PORT(0x00407430, Math_ClosestPointOnSegment, "rx_race", OUT_XMM0) {
    m.xmm[0].lo = rx_oracle::bits(rx_closest_point_on_segment(m.r[1], m.r[6], m.r[0], m.arg(0))); m.xmm[0].hi = 0;
    return m.ret(0);
}
