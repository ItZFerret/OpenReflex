// loop_cam_replay.cpp -- oracle adapters for core/game/loop_cam_replay.c (group "rx_loop"): the replay cameras (MXReplayCamera and the tower /
// zip line / spline / animated classes). ABI glue only.
//
// NOT ADAPTED: the RSData readers vf5 (006486f0, 00648e40, 006536c0, 00654270, 006553d0) and the property functions they call (00648f50,
// 006537d0, 00654380, 006554e0): the core reads the entity's RSData through the host BXML reader of the scene unit (rx_scene_rs(), as every
// scene class), the original through the XMLReader in guest memory. They are verified by the construction compare of the scene (core/tools/
// build_scene_test + rx_canon_cmp against the original's canonical dump), see QUEUE.md "w-tvcam". The constructors, destructors, vf28, vf37,
// vf19 and vf39 run on guest memory only and are checked per call (baton boots of Supercross SX_Kingston for the zip line and the spline,
// the five reference worlds for the tower, the probe hook for the replay views).
#include "oracle.h"
#include "f80/f80.h"
extern "C" {
#include "../game/loop_cam_replay.h"
}
using namespace reflex_port;
using rx_oracle::f32;
using rx_oracle::bits;

static void push_float(reflex_vm::Machine& m, float v) { m.fpu.push_back(reflex_f80::from_f32(bits(v))); }

// ---- the base ----
// 006485e0 (ESI = the camera) -> EAX = the camera; the camera entity base (0063d930) takes the caller's pushed ESI word
RX_PORT(0x006485e0, MXReplayCamera_Construct, "rx_loop", OUT_EAX) { m.r[0] = rx_replay_camera_construct(m.r[6]); return m.ret(0); }
// 006486c0 MXReplayCamera::sdtor (ECX = this; stack flags; ret 4) -> EAX = this
RX_PORT(0x006486c0, MXReplayCamera_sdtor, "rx_loop", OUT_EAX) { m.r[0] = rx_replay_camera_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
// 00648a40 MXReplayCamera::vf37 -> st(0)
RX_PORT(0x00648a40, MXReplayCamera_vf37, "rx_loop", OUT_FPU) { push_float(m, rx_replay_camera_vf37(m.r[1])); return m.ret(0); }
// 00648a50 MXReplayCamera::vf28 (ECX = this; ret 4)
RX_PORT(0x00648a50, MXReplayCamera_vf28, "rx_loop", 0) { rx_replay_camera_vf28(m.r[1], m.arg(0)); return m.ret(4); }
// 00655880 MXReplayCamera::vf38 (ECX = this)
RX_PORT(0x00655880, MXReplayCamera_vf38, "rx_loop", 0) { rx_replay_camera_vf38(m.r[1]); return m.ret(0); }

// ---- the activation-area test (stack a; EAX = b, EDX = c, ESI = n, EDI = p; plain ret) -> EAX ----
RX_PORT(0x0063d060, TrianglePrismTest, "rx_loop", OUT_EAX) {
    m.r[0] = rx_point_in_triangle_prism(m.arg(0), m.r[0], m.r[2], m.r[6], m.r[7]); return m.ret(0);
}

// ---- tower ----
// 00648cc0 (EAX = the camera) -> EAX
RX_PORT(0x00648cc0, MXReplayTowerCamera_Construct, "rx_loop", OUT_EAX) { m.r[0] = rx_replay_tower_camera_construct(m.r[0]); return m.ret(0); }
RX_PORT(0x00648e60, MXReplayTowerCamera_vf28, "rx_loop", 0) { rx_replay_tower_camera_vf28(m.r[1], m.arg(0)); return m.ret(4); }
// 00654720 (MXReplayTowerCamera / MXSplineCamera vf37) -> st(0)
RX_PORT(0x00654720, MXReplayTowerCamera_vf37, "rx_loop", OUT_FPU) { push_float(m, rx_replay_tower_camera_vf37(m.r[1])); return m.ret(0); }
// 00649470 (MXReplayTowerCamera / MXZipLineCamera vf39)
RX_PORT(0x00649470, MXReplayTowerCamera_vf39, "rx_loop", 0) { rx_replay_activation_vf39(m.r[1]); return m.ret(0); }

// ---- zip line ----
RX_PORT(0x006534f0, MXZipLineCamera_Construct, "rx_loop", OUT_EAX) { m.r[0] = rx_zipline_camera_construct(m.r[0]); return m.ret(0); }
// 00653670 (EAX = the camera)
RX_PORT(0x00653670, MXZipLineCamera_destroy, "rx_loop", 0) { rx_zipline_camera_destroy(m.r[0]); return m.ret(0); }
RX_PORT(0x00653650, MXZipLineCamera_sdtor, "rx_loop", OUT_EAX) { m.r[0] = rx_zipline_camera_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x006536e0, MXZipLineCamera_vf28, "rx_loop", 0) { rx_zipline_camera_vf28(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00653aa0, MXZipLineCamera_vf37, "rx_loop", OUT_FPU) { push_float(m, rx_zipline_camera_vf37(m.r[1])); return m.ret(0); }
RX_PORT(0x00653c30, MXZipLineCamera_vf19, "rx_loop", 0) { rx_zipline_camera_vf19(m.r[1], f32(m.arg(0))); return m.ret(4); }

// ---- spline ----
RX_PORT(0x00653f60, MXSplineCamera_Construct, "rx_loop", OUT_EAX) { m.r[0] = rx_spline_camera_construct(m.r[0]); return m.ret(0); }
RX_PORT(0x006541e0, MXSplineCamera_destroy, "rx_loop", 0) { rx_spline_camera_destroy(m.r[0]); return m.ret(0); }
RX_PORT(0x006541c0, MXSplineCamera_sdtor, "rx_loop", OUT_EAX) { m.r[0] = rx_spline_camera_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00654290, MXSplineCamera_vf28, "rx_loop", 0) { rx_spline_camera_vf28(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00654870, MXSplineCamera_vf19, "rx_loop", 0) { rx_spline_camera_vf19(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00654c90, MXSplineCamera_vf39, "rx_loop", 0) { rx_spline_camera_vf39(m.r[1]); return m.ret(0); }
// 00655050 (ECX = the camera, EAX = out vec3, xmm0 = u as a double; plain ret)
RX_PORT(0x00655050, MXSplineCamera_CurveAt, "rx_loop", 0) {
    double u; const uint64_t b = m.xmm[0].lo; std::memcpy(&u, &b, 8);
    rx_spline_curve_at(m.r[1], m.r[0], u); return m.ret(0);
}

// ---- animated ----
RX_PORT(0x00655200, MXAnimatedReplayCamera_Construct, "rx_loop", OUT_EAX) { m.r[0] = rx_animated_replay_camera_construct(m.r[0]); return m.ret(0); }
RX_PORT(0x00655360, MXAnimatedReplayCamera_destroy, "rx_loop", 0) { rx_animated_replay_camera_destroy(m.r[0]); return m.ret(0); }
RX_PORT(0x00655340, MXAnimatedReplayCamera_sdtor, "rx_loop", OUT_EAX) { m.r[0] = rx_animated_replay_camera_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x006553f0, MXAnimatedReplayCamera_vf28, "rx_loop", 0) { rx_animated_replay_camera_vf28(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00655730, MXAnimatedReplayCamera_vf37, "rx_loop", OUT_FPU) { push_float(m, rx_animated_replay_camera_vf37(m.r[1])); return m.ret(0); }
RX_PORT(0x006558b0, MXAnimatedReplayCamera_vf19, "rx_loop", 0) { rx_animated_replay_camera_vf19(m.r[1], f32(m.arg(0))); return m.ret(4); }
// 00655b40 (ESI = the camera; stack t; ret 4)
RX_PORT(0x00655b40, MXAnimatedReplayCamera_SetTime, "rx_loop", 0) { rx_animated_replay_camera_set_time(m.r[6], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00655c30, MXAnimatedReplayCamera_vf39, "rx_loop", 0) { rx_animated_replay_camera_vf39(m.r[1]); return m.ret(0); }
