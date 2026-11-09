// loop_cam_build.cpp -- oracle adapters for core/game/loop_cam_build.c (group "rx_loop"): the player camera entities'
// constructors, the camera tuning apply (vf32) and the two class methods the camera path calls. ABI glue only; each port is
// verified per call in a baton boot, where the original 0054bc00 at the spawn runs these functions. 0054bc00 / 0063f160 are
// not adapted: the product leaves the first-person camera slot 0 (its vf19 00642680 is not ported), a documented difference.
#include "oracle.h"
extern "C" {
#include "../game/loop_cam_build.h"
}
using namespace reflex_port;

// ECX = camera, stack controller; RET 4 -> EAX = camera
RX_PORT(0x00646730, MXPlayerCamera_Construct, "rx_loop", OUT_EAX) { return m.ret(4, rx_player_camera_construct(m.r[1], m.arg(0))); }
// ECX = camera, stack controller, config; RET 8 -> EAX = camera
RX_PORT(0x00649610, MXThirdPersonCamera_Construct, "rx_loop", OUT_EAX) {
    return m.ret(8, rx_third_person_camera_construct(m.r[1], m.arg(0), m.arg(1)));
}
// ECX = camera, EAX = controller; RET 0 -> EAX = camera
RX_PORT(0x00647cf0, MXStartGateCamera_Construct, "rx_loop", OUT_EAX) { return m.ret(0, rx_start_gate_camera_construct(m.r[1], m.r[0])); }
// ECX = camera, EAX = controller; RET 0 -> EAX = camera
RX_PORT(0x00646d70, MXQuickCamera_Construct, "rx_loop", OUT_EAX) { return m.ret(0, rx_quick_camera_construct(m.r[1], m.r[0])); }
// ECX = camera; RET 0
RX_PORT(0x00646b70, MXPlayerCamera_vf32, "rx_loop", 0) { rx_player_camera_vf32(m.r[1]); return m.ret(0); }
RX_PORT(0x0064a330, MXThirdPersonCamera_vf32, "rx_loop", 0) { rx_third_person_camera_vf32(m.r[1]); return m.ret(0); }
RX_PORT(0x00647700, MXQuickCamera_vf32, "rx_loop", 0) { rx_quick_camera_vf32(m.r[1]); return m.ret(0); }
// ECX = camera; RET 4 (the vf28 transition's argument)
RX_PORT(0x00649bd0, MXThirdPersonCamera_vf28, "rx_loop", 0) { rx_third_person_camera_vf28(m.r[1]); return m.ret(4); }
// ECX = camera; RET 4
RX_PORT(0x00647e00, MXStartGateCamera_vf28, "rx_loop", 0) { rx_start_gate_camera_vf28(m.r[1]); return m.ret(4); }
// ESI = camera; RET 0
RX_PORT(0x00647e40, MXStartGateCamera_Place, "rx_loop", 0) { rx_start_gate_camera_place(m.r[6]); return m.ret(0); }
// ECX = camera; RET 0
RX_PORT(0x0063de60, MXCameraEntity_vf28, "rx_loop", 0) { rx_camera_entity_vf28(m.r[1]); return m.ret(0); }
RX_PORT(0x006534e0, MXThirdPersonCamera_vf35, "rx_loop", 0) { rx_third_person_camera_vf35(m.r[1]); return m.ret(0); }
RX_PORT(0x0063f110, MXPlayerCamera_vf36, "rx_loop", 0) { rx_player_camera_vf36(m.r[1]); return m.ret(0); }

// 00641280 (ECX = camera, EAX = controller; stack config; RET 4) -> EAX = camera. NOT CREATED BY THE PRODUCT (the factory's
// type 0 path): the first-person camera's per-frame update 00642680 is not ported. The original's camera creation calls it in
// the boot, which is this port's per-call Verify.
RX_PORT(0x00641280, MXFirstPersonCamera_Construct, "rx_loop", OUT_EAX) {
    return m.ret(4, rx_first_person_camera_construct(m.r[1], m.r[0], m.arg(0)));
}
// 00407410 (EAX = row index, ECX = source vec3; stack the 4x4; RET 0)
RX_PORT(0x00407410, Camera_PoseRow, "rx_loop", 0) { rx_cam_pose_row(m.r[0], m.r[1], m.arg(0)); return m.ret(0); }
