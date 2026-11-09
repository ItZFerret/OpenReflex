// build_scene_entity.cpp -- oracle adapters for core/game/build_scene_entity.c (group rx_build_rider: the rider's entity attach in the baton boot).
#include "oracle.h"
#include "f80/f80.h"
extern "C" {
#include "../game/build_scene_entity.h"
}
using namespace reflex_port;

// 0080ddf0 Entity base description (stdcall entity, desc; RET 8)
RX_PORT(0x0080ddf0, EntityDescribe, "rx_build_rider", 0) { rx_entity_describe(m.arg(0), m.arg(1)); return m.ret(8); }
// 007f1810 ModelInstance::SetModel (EAX = model handle, ECX = flags; s0 instance, s1 overrides; RET 8)
RX_PORT(0x007f1810, ModelInstanceSetModel, "rx_build_rider", 0) { rx_model_instance_set_model(m.arg(0), m.r[0], m.r[1], m.arg(1)); return m.ret(8); }
// 0063dae0 (EAX = the camera entity, ESI = the scene)
RX_PORT(0x0063dae0, CameraEntityAddToScene, "rx_build_rider", 0) { rx_camera_entity_add_to_scene(m.r[0], m.r[6]); return m.ret(0); }
// 004045c0 (fastcall ECX; st(0) = the float at [ECX + 0x9d4])
RX_PORT(0x004045c0, InstancedAnimationEntity_vf7_o90, "rx_build_rider", OUT_FPU) {
    m.fpu.push_back(reflex_f80::from_f32(rx_oracle::bits(rx_instanced_anim_light_range(m.r[1]))));
    return m.ret(0);
}
// 004045d0 (thiscall ECX, stdcall out a, b, c; RET 0xc). Void.
RX_PORT(0x004045d0, InstancedAnimationEntity_vf6_o90, "rx_build_rider", 0) { rx_instanced_anim_light_shape(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
// 00404620 / 00404590 (tail jumps: EAX is the callee's)
RX_PORT(0x00404620, InstancedAnimationEntity_vf12_o90, "rx_build_rider", OUT_EAX) { m.r[0] = rx_instanced_anim_vf12_o90(m.r[1]); return m.ret(0); }
RX_PORT(0x00404590, InstancedAnimationEntity_vf11_o90, "rx_build_rider", OUT_EAX) { m.r[0] = rx_instanced_anim_vf11_o90(m.r[1]); return m.ret(0); }
// 004045b0 -> EAX = 0x009cb510
RX_PORT(0x004045b0, InstancedAnimationEntity_vf14, "rx_build_rider", OUT_EAX) { m.r[0] = rx_instanced_anim_vf14(); return m.ret(0); }
// 00617f60 (thiscall ECX, stdcall centre, extents, radius; RET 0xc). Void.
RX_PORT(0x00617f60, CharacterEntity_vf6_o90, "rx_build_rider", 0) { rx_character_entity_vf6_o90(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
// 007f1aa0 (ESI = the model instance): the instance's mesh block freed, +0x64 .. +0x7c cleared.
RX_PORT(0x007f1aa0, ModelInstanceRelease, "rx_build_rider", 0) { rx_model_instance_release(m.r[6]); return m.ret(0); }
