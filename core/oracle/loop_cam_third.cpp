// loop_cam_third.cpp -- oracle adapters for core/game/loop_cam_third.c (group "rx_loop").
#include "oracle.h"
extern "C" {
#include "../game/loop_cam.h"
}
using namespace reflex_port;
using rx_oracle::f32;
using rx_oracle::bits;

// EDI = position vec3; no result read
RX_PORT(0x0063e400, Cam_GroundClearance, "rx_loop", 0) { rx_cam_ground_clearance(m.r[7]); return m.ret(0); }
// ECX = camera, EAX = position; stack out, margin; ret 8
RX_PORT(0x00650c60, Cam_LiftAboveWater, "rx_loop", 0) { static int n = 0; if (getenv("LOOP_DBG") && n++ < 3) fprintf(stderr, "lift esp=%08x eax=%08x ecx=%08x out=%08x\n", m.r[4], m.r[0], m.r[1], m.arg(0)); rx_cam_lift_above_water(m.r[1], m.r[0], m.arg(0), f32(m.arg(1))); return m.ret(8); }
// ESI = camera, EAX = direction; stack side, target, out position, out look-at; ret 0x10
RX_PORT(0x006502e0, Cam_OrbitPose, "rx_loop", 0) { rx_cam_orbit_pose(m.r[6], m.r[0], m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0x10); }
// EAX = camera, EDI = direction; stack target; ret 4
RX_PORT(0x00650250, Cam_OrbitProbe, "rx_loop", 0) { rx_cam_orbit_probe(m.r[0], m.r[7], m.arg(0)); return m.ret(4); }
// EAX = camera; stack target, direction; ret 8
RX_PORT(0x0064f240, Cam_WaterAhead, "rx_loop", 0) { rx_cam_water_ahead(m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
