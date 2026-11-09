// loop_cam_player.cpp -- oracle adapters for core/game/loop_cam_player.c (group "rx_loop").
#include "oracle.h"
extern "C" {
#include "../game/loop_cam_player.h"
}
using namespace reflex_port;
using rx_oracle::f32;

// EAX = camera -> EAX = the camera class; ECX = the followed vehicle
RX_PORT(0x006467c0, Cam_VehicleClass, "rx_loop", OUT_EAX) {
    const uint32_t cam = m.r[0];
    m.r[0] = rx_cam_vehicle_class(cam);
    m.r[1] = rx_rd32(rx_rd32(cam + 0x344) + 0x1ac);
    return m.ret(0);
}
// EDI = camera
RX_PORT(0x00646a50, Cam_ReloadTuning, "rx_loop", 0) { rx_cam_reload_tuning(m.r[7]); return m.ret(0); }
// EAX = camera, ESI = out; EAX = out
RX_PORT(0x006468a0, Cam_FollowPoint, "rx_loop", OUT_EAX) { m.r[0] = rx_cam_follow_point(m.r[0], m.r[6]); return m.ret(0); }
// thiscall dt; ret 4
RX_PORT(0x00646940, MXPlayerCamera_vf19, "rx_loop", 0) { rx_player_camera_vf19(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00646bf0, MXPlayerCamera_vf31, "rx_loop", 0) { rx_player_camera_vf31(m.r[1]); return m.ret(0); }
RX_PORT(0x0063d7e0, MXAnimatedCamera_vf19, "rx_loop", 0) { rx_animated_camera_vf19(m.r[1], f32(m.arg(0))); return m.ret(4); }
// ESI = camera; stack t; ret 4
RX_PORT(0x0063d600, AnimatedCamera_At, "rx_loop", 0) { rx_animated_camera_at(m.r[6], f32(m.arg(0))); return m.ret(4); }
// EAX = count, EDI = keys, xmm0 = t; EAX
RX_PORT(0x004dbe00, KeyTrack_CountBefore, "rx_loop", OUT_EAX) { m.r[0] = rx_keytrack_count_before(m.r[0], m.r[7], f32(uint32_t(m.xmm[0].lo))); return m.ret(0); }
// stdcall (path, t, matrix, fov, a, b); ret 0x18
RX_PORT(0x0063e850, CamPath_Sample, "rx_loop", 0) { rx_cam_path_sample(m.arg(0), f32(m.arg(1)), m.arg(2), m.arg(3), m.arg(4), m.arg(5)); return m.ret(0x18); }
