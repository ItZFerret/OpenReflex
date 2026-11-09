// loop_cam_entity.cpp -- oracle adapters for core/game/loop_cam_entity.c (group "rx_loop").
#include "oracle.h"
extern "C" {
#include "../game/loop_cam_entity.h"
}
using namespace reflex_port;
using rx_oracle::f32;
static float x0(reflex_vm::Machine& m) { return f32(uint32_t(m.xmm[0].lo)); }

// ESI = camera, xmm0 = fwd.x; stack fwd.y, fwd.z, up.x, up.y, up.z; ret 0x14 (void: EAX = 0 after 00790ce0's flag clear)
RX_PORT(0x00790ce0, Cam_SetBasis, "rx_loop", 0) { rx_cam_set_basis(m.r[6], x0(m), f32(m.arg(0)), f32(m.arg(1)), f32(m.arg(2)), f32(m.arg(3)), f32(m.arg(4))); return m.ret(0x14); }
RX_PORT(0x00790f40, Cam_SetRenderBasis, "rx_loop", 0) { rx_cam_set_render_basis(m.r[6], x0(m), f32(m.arg(0)), f32(m.arg(1)), f32(m.arg(2)), f32(m.arg(3)), f32(m.arg(4))); return m.ret(0x14); }
// ESI = camera, EAX = up, ECX = forward
RX_PORT(0x007911b0, Cam_SetRenderBasisFrom, "rx_loop", 0) { rx_cam_set_render_basis_from(m.r[6], m.r[0], m.r[1]); return m.ret(0); }
// EAX = camera, xmm0 = fov
RX_PORT(0x007911e0, Cam_SetFov, "rx_loop", 0) { rx_cam_set_fov(m.r[0], x0(m)); return m.ret(0); }
// ECX = camera, EAX = out
RX_PORT(0x00792330, Cam_GetPosition, "rx_loop", 0) { rx_cam_get_position(m.r[1], m.r[0]); return m.ret(0); }
RX_PORT(0x007923a0, Cam_GetRight, "rx_loop", 0) { rx_cam_get_right(m.r[1], m.r[0]); return m.ret(0); }
RX_PORT(0x00792380, Cam_GetForward, "rx_loop", 0) { rx_cam_get_forward(m.r[1], m.r[0]); return m.ret(0); }
// thiscall; stack out; ret 4
RX_PORT(0x00792350, Camera_vf0, "rx_loop", 0) { rx_camera_vf0(m.r[1], m.arg(0)); return m.ret(4); }
// ECX = camera, EAX = vec3
RX_PORT(0x00790cb0, Cam_SetRenderPosition, "rx_loop", 0) { rx_cam_set_render_position(m.r[1], m.r[0]); return m.ret(0); }
// ECX = source, EAX = destination
RX_PORT(0x0063ddd0, Camera_CopyBlock, "rx_loop", 0) { rx_camera_copy_block(m.r[1], m.r[0]); return m.ret(0); }
// stdcall (out, source); ret 8; EAX = out
RX_PORT(0x0063dc30, Camera_Copy, "rx_loop", OUT_EAX) { m.r[0] = rx_camera_copy(m.arg(0), m.arg(1)); return m.ret(8); }
// EDI = source, ESI = destination
RX_PORT(0x00790b30, Camera_Assign, "rx_loop", 0) { rx_camera_assign(m.r[7], m.r[6]); return m.ret(0); }
// stdcall (entity, view); ret 8
RX_PORT(0x0063dba0, CamEntity_ToView, "rx_loop", 0) { rx_cam_entity_to_view(m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0063de70, MXCameraEntity_vf29, "rx_loop", 0) { rx_cam_entity_invalidate(m.r[1]); return m.ret(0); }
RX_PORT(0x0063de80, MXCameraEntity_vf31, "rx_loop", 0) { rx_cam_entity_record(m.r[1]); return m.ret(0); }
// stdcall (entity, pose); ret 8
RX_PORT(0x0063dee0, CamEntity_SetPose, "rx_loop", 0) { rx_cam_entity_set_pose(m.arg(0), m.arg(1)); return m.ret(8); }
// ECX = from, EAX = to; stack entity; ret 4
RX_PORT(0x0063df90, CamEntity_LookAt, "rx_loop", 0) { rx_cam_entity_look_at(m.r[1], m.r[0], m.arg(0)); return m.ret(4); }
// thiscall dt; ret 4
RX_PORT(0x0063e310, MXCameraEntity_vf19, "rx_loop", 0) { rx_cam_entity_vf19(m.r[1], f32(m.arg(0))); return m.ret(4); }
