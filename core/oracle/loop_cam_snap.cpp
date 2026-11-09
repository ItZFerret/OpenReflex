// loop_cam_snap.cpp -- oracle adapter for core/game/loop_cam_snap.c (group "rx_loop").
#include "oracle.h"
extern "C" {
#include "../game/loop_cam_snap.h"
}
using namespace reflex_port;
using rx_oracle::f32;

// ESI = the new camera entity, EDI = the old camera entity; stack duration; ret 4
RX_PORT(0x00649df0, Cam_Snap, "rx_loop", OUT_FPU) {
    rx_cam_snap(m.r[6], m.r[7], f32(m.arg(0))); return m.ret(4);
}
