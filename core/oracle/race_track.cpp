// race_track.cpp -- oracle adapters for core/game/race_track.c.
#include "race_oracle.h"
extern "C" {
#include "../game/race.h"
}
using namespace reflex_port;

// 00538e00 (ECX = second edge pair, EDX = node a, ESI = node b; args position, length (float), from_end; cdecl) -> EAX.
RX_PORT(0x00538e00, Track_SegmentContains, "rx_race", OUT_EAX) {
    race_check_pc53(m, "00538e00");
    m.r[0] = uint32_t(rx_track_segment_contains(int(m.r[1]), m.r[2], m.r[6], m.arg(0), rx_oracle::f32(m.arg(1)), int(m.arg(2))));
    return m.ret(0);
}
// 005387c0 (EDI = query, arg 0 = second edge pair; cdecl) -> EAX.
RX_PORT(0x005387c0, Track_FindSegment, "rx_race", OUT_EAX) {
    race_check_pc53(m, "005387c0");
    m.r[0] = uint32_t(rx_track_find_segment(m.r[7], int(m.arg(0))));
    return m.ret(0);
}
// 005381f0 (args start, position, out, budget; cdecl).
RX_PORT(0x005381f0, Track_NearestNode, "rx_race", 0) {
    float budget = rx_oracle::f32(m.arg(3));
    rx_track_nearest_node(m.arg(0), m.arg(1), m.arg(2), &budget);
    m.mem.write(m.r[4] + 0x10, 4, rx_oracle::bits(budget));   // the original keeps the remaining budget in its argument slot
    return m.ret(0);
}
// 00538440 (EDX = out, ESI = position, args track, direction, second edge pair; cdecl).
RX_PORT(0x00538440, Track_NearestNodeAll, "rx_race", 0) {
    rx_track_nearest_node_all(m.arg(0), m.arg(1), int(m.arg(2)), m.r[6], m.r[2]);
    return m.ret(0);
}
// 005389c0 (ECX = a, EAX = b, arg 0 = point, ESI = out; cdecl); EDX = point.
RX_PORT(0x005389c0, Track_ProjectClampedXZ, "rx_race", OUT_EDX) {
    rx_track_project_clamped_xz(m.r[1], m.r[0], m.arg(0), m.r[6]);
    m.r[2] = m.arg(0);
    return m.ret(0);
}
// 00538b30 (EDX = a, ECX = b, ESI = point, arg 0 = out; cdecl) -> EAX = out, xmm0 = (t, 0, 0, 0).
RX_PORT(0x00538b30, Track_ProjectXZ, "rx_race", OUT_EAX | OUT_XMM0) {
    const uint32_t out = m.arg(0);
    const float t = rx_track_project_xz(m.r[2], m.r[1], m.r[6], out);
    m.r[0] = out;
    m.xmm[0].lo = rx_oracle::bits(t); m.xmm[0].hi = 0;
    return m.ret(0);
}
