// world_veh_track.cpp -- oracle adapters for core/game/world_veh_track.c (group rx_world).
#include "oracle.h"
extern "C" {
#include "../game/world_veh.h"
}
using namespace reflex_port;

// 004607e0 (thiscall block, stdcall vehicle, surface, dt; RET 0xc).
RX_PORT(0x004607e0, Vehicle_WheelDeformAndTrackMarks, "rx_world", 0) {
    rx_world_wheel_deform_and_track_marks(m.r[1], m.arg(0), m.arg(1), rx_oracle::f32(m.arg(2)));
    return m.ret(0xc);
}
// 00468120 (EDI = block, stdcall vehicle; RET 4).
RX_PORT(0x00468120, Vehicle_SlipTrackMark, "rx_world", 0) { rx_world_slip_track_mark(m.r[7], m.arg(0)); return m.ret(4); }
