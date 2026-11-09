// fx_entity.cpp -- oracle adapters for core/game/fx_entity.c (group "rx_fx").
#include "fx_oracle.h"
#include "f80/f80.h"
extern "C" {
#include "../game/fx.h"
}
using namespace reflex_port;
using rx_oracle::f32;
using rx_oracle::bits;

static rx_v4 xmm(reflex_vm::Machine& m, int i) {
    rx_v4 r; uint32_t b[4] = {uint32_t(m.xmm[i].lo), uint32_t(m.xmm[i].lo >> 32), uint32_t(m.xmm[i].hi), uint32_t(m.xmm[i].hi >> 32)};
    std::memcpy(r.v, b, 16); return r;
}
static void set_xmm(reflex_vm::Machine& m, int i, rx_v4 v) {
    uint32_t b[4]; std::memcpy(b, v.v, 16);
    m.xmm[i].lo = b[0] | (uint64_t(b[1]) << 32); m.xmm[i].hi = b[2] | (uint64_t(b[3]) << 32);
}

FX_PORT(0x00403cf0, TriggerInside_00403cf0, OUT_EAX) { m.r[0] = rx_fx_trigger_inside(m.r[0], m.r[6]); return m.ret(0); }
FX_PORT(0x00403c10, TriggerContains_00403c10, OUT_EAX) { m.r[0] = rx_fx_trigger_contains(m.r[0], m.r[1]); return m.ret(0); }
FX_PORT(0x0040a7f0, TriggerFind_0040a7f0, OUT_EAX) { m.r[0] = rx_fx_trigger_find(m.arg(0), m.r[7]); return m.ret(4); }
FX_PORT(0x0040a830, TriggerAdd_0040a830, 0) { rx_fx_trigger_add(m.arg(0), m.r[0]); return m.ret(4); }
FX_PORT(0x0040a870, TriggerUnsee_0040a870, 0) { rx_fx_trigger_unsee(m.r[1]); return m.ret(0); }
FX_PORT(0x00419820, TriggerPost_00419820, 0) { rx_fx_trigger_post(m.arg(0), m.r[1], m.r[2]); return m.ret(4); }
FX_PORT(0x00419880, TriggerPostPhysics_00419880, 0) { rx_fx_trigger_post_physics(m.arg(0), m.r[1], m.r[2]); return m.ret(4); }
FX_PORT(0x00419a10, SectionPost_00419a10, 0) { rx_fx_section_post(m.arg(0), m.r[1]); return m.ret(4); }
FX_PORT(0x0040a600, TriggerStay_0040a600, 0) { rx_fx_trigger_stay(m.r[6]); return m.ret(0); }
FX_PORT(0x0040a900, TriggerWatchObject_0040a900, 0) { rx_fx_trigger_watch_object(m.arg(0), m.r[7]); return m.ret(4); }
FX_PORT(0x0040a8a0, TriggerPlayerInside_0040a8a0, 0) { rx_fx_trigger_player_inside(m.r[0], m.r[1]); return m.ret(0); }
FX_PORT(0x0040a980, TriggerPrune_0040a980, 0) { rx_fx_trigger_prune(m.r[6]); return m.ret(0); }
FX_PORT(0x0040a650, MXTrigger_vf19, 0) { rx_fx_trigger_tick(m.r[1], f32(m.arg(0))); return m.ret(4); }
FX_PORT(0x00413b40, SectionPlayerInside_00413b40, 0) { rx_fx_section_player_inside(m.r[0], m.r[1]); return m.ret(0); }
FX_PORT(0x00413a80, TimedRaceSection_vf19, 0) { rx_fx_timed_section_tick(m.r[1], f32(m.arg(0))); return m.ret(4); }
FX_PORT(0x0040b770, OverlayColorEntity_vf19, 0) { rx_fx_overlay_colour_tick(m.r[1], f32(m.arg(0))); return m.ret(4); }
FX_PORT(0x00410dd0, ScrollingTextureEntity_vf19, 0) { rx_fx_scrolling_texture_tick(m.r[1], f32(m.arg(0))); return m.ret(4); }
FX_PORT(0x00404020, UIGateSelectionCameraEntity_vf19, 0) { rx_fx_gate_camera_tick(m.r[1], f32(m.arg(0))); return m.ret(4); }
FX_PORT(0x00401060, CopyDword_00401060, 0) { m.r[0] = m.arg(0); rx_fx_copy_dword(m.r[1], m.arg(0)); m.r[2] = uint32_t(m.mem.read(m.arg(0), 4)); return m.ret(4); }
FX_PORT(0x00401070, Deref2_00401070, OUT_EAX) { m.r[0] = rx_fx_deref2(m.r[1]); return m.ret(0); }
FX_PORT(0x004010e0, Second_004010e0, OUT_EAX) { m.r[0] = rx_fx_second(m.arg(0), m.arg(1)); return m.ret(0); }
FX_PORT(0x00401c80, HolderLock_00401c80, OUT_EAX | OUT_ECX) { m.r[0] = m.r[1] = rx_fx_holder_lock(m.r[0], m.r[2]); return m.ret(0); }
FX_PORT(0x00401ca0, HolderUnlock_00401ca0, 0) { rx_fx_holder_unlock(m.r[0]); return m.ret(0); }
FX_PORT(0x00401d40, IRenderable_vf7, OUT_FPU) { m.fpu.push_back(reflex_f80::from_f32(bits(rx_fx_renderable_vf7(m.r[1])))); return m.ret(0); }
FX_PORT(0x00401f00, ModelEntity_vf1_o90, OUT_EAX) { m.r[0] = rx_fx_model_entity_vf1(m.r[1]); return m.ret(0); }
FX_PORT(0x004023c0, Field10_004023c0, OUT_EAX) { m.r[0] = rx_fx_field_10(m.r[0]); return m.ret(0); }
FX_PORT(0x00402af0, Field68_00402af0, OUT_EAX) { m.r[0] = rx_fx_field_68(m.r[0]); return m.ret(0); }
FX_PORT(0x00402ad0, Set118_00402ad0, 0) { rx_fx_set_118(m.r[1], f32(uint32_t(m.xmm[0].lo))); return m.ret(0); }
FX_PORT(0x00402c20, UIManager_00402c20, OUT_EAX) { m.r[0] = rx_fx_ui_manager(); return m.ret(0); }
FX_PORT(0x0041ad70, PushDword_0041ad70, 0) { rx_fx_push_dword(m.r[0], m.r[2]); return m.ret(0); }
FX_PORT(0x0041c4b0, Atan2f_0041c4b0, OUT_FPU) { const float r = rx_fx_atan2f(f32(m.arg(0)), f32(m.arg(1))); m.fpu.push_back(reflex_f80::from_f32(bits(r))); return m.ret(0); }
FX_PORT(0x0041c580, Vec4_Int4ToFloatDivScalar, OUT_XMM0 | (OUT_XMM0 << 1)) {
    const rx_v4 x = xmm(m, 0); int32_t in[4]; std::memcpy(in, x.v, 16);
    rx_v4 sp; const rx_v4 r = rx_fx_int4_div(in, f32(m.arg(0)), &sp); set_xmm(m, 0, r); set_xmm(m, 1, sp); return m.ret(0);
}
FX_PORT(0x0041c5c0, Vec3_DotDoubleSplat, OUT_XMM0) { set_xmm(m, 0, rx_fx_dot3_splat(xmm(m, 0), xmm(m, 1))); return m.ret(0); }
