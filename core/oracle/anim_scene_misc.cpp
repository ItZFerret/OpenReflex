// anim_scene_misc.cpp -- oracle adapters for core/game/anim_scene_misc.c (group rx_anim).
#include "anim_oracle.h"
extern "C" {
#include "../game/anim.h"
#include "../game/anim_scene_misc.h"
}
using namespace reflex_port;

ANIM_PORT(0x007da9c0, EventCell_RemoveC, "rx_anim", OUT_EAX) { m.r[0] = rx_event_cell_remove_c(m.arg(0), m.arg(1)); return m.ret(8); }
ANIM_PORT(0x007dab70, EventCell_RemoveD, "rx_anim", OUT_EAX) { m.r[0] = rx_event_cell_remove_d(m.arg(0), m.arg(1)); return m.ret(8); }
ANIM_PORT(0x007da290, EventGrid_RemoveC, "rx_anim", OUT_EAX) {
    m.r[0] = rx_event_grid_remove_c(m.r[7], m.arg(0), rx_oracle::f32(m.arg(1)), m.arg(2)); return m.ret(0xc);
}
ANIM_PORT(0x007da350, EventGrid_RemoveD, "rx_anim", 0) {
    m.r[0] = rx_event_grid_remove_d(m.r[7], m.arg(0), rx_oracle::f32(m.arg(1)), m.arg(2)); return m.ret(0xc);
}
// 007f9db0 (ECX = array, stack &key; RET 4) -> EAX
ANIM_PORT(0x007f9db0, Array_PtrFind, "rx_anim", OUT_EAX) { m.r[0] = rx_array_ptr_find(m.r[1], m.arg(0)); return m.ret(4); }
// 007f9fd0 (ECX = index, EDX = array)
ANIM_PORT(0x007f9fd0, Array40_Remove, "rx_anim", 0) { rx_array40_remove(m.r[1], m.r[2]); return m.ret(0); }
// 007e0190 (EAX = lights, stack entity; RET 4)
ANIM_PORT(0x007e0190, Lights_RemoveEntity, "rx_anim", 0) { rx_lights_remove_entity(m.r[0], m.arg(0)); return m.ret(4); }
