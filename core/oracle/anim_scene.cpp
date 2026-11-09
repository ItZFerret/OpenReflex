// anim_scene.cpp -- oracle adapters for core/game/anim_scene.c.
#include "anim_oracle.h"
#include "fx_oracle.h"   /* FxBind: 00545120 inside 007d4030 (loop_player.c) */
extern "C" {
#include "../game/anim.h"
#include "../game/build_asset.h"
}
using namespace reflex_port;
static float xmm_f(reflex_vm::Machine& m, int i) { return rx_oracle::f32(uint32_t(m.xmm[i].lo)); }

ANIM_PORT(0x007d1cd0, Scene_ProfileScope, "rx_anim", 0) { rx_scene_profile_scope(m.arg(0)); return m.ret(4); }
ANIM_PORT(0x007d23a0, SceneManager_Tick, "rx_anim", 0) { rx_scene_manager_tick(m.arg(0)); return m.ret(4); }
ANIM_PORT(0x007d21d0, SceneManager_TickLate, "rx_anim", 0) { rx_scene_manager_tick_late(m.r[7]); return m.ret(0); }
ANIM_PORT(0x007d2190, SceneManager_UpdateRenderSync, "rx_anim", 0) { rx_scene_manager_update_render_sync(m.arg(0)); return m.ret(4); }
ANIM_PORT(0x007d2490, SceneManager_PostUpdate, "rx_anim", 0) { rx_scene_manager_post_update(m.arg(0)); return m.ret(4); }
ANIM_PORT(0x007d2e70, SceneManager_FindTyped, "rx_anim", OUT_EAX) {
    m.r[0] = rx_scene_manager_find_typed(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc);
}
ANIM_PORT(0x007d2920, SceneManager_CollectTyped, "rx_anim", OUT_EAX) {
    m.r[0] = rx_scene_manager_collect_typed(m.r[0], m.arg(0), m.arg(1), int32_t(m.arg(2)), m.arg(3)); return m.ret(0x10);
}
ANIM_PORT(0x007d2c80, SceneManager_FindInSubscene, "rx_anim", OUT_EAX) { m.r[0] = rx_scene_manager_find_in_subscene(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
ANIM_PORT(0x007d2da0, SceneManager_FindNamed, "rx_anim", OUT_EAX) { m.r[0] = rx_scene_manager_find_named(m.arg(0), m.arg(1)); return m.ret(8); }
ANIM_PORT(0x007d3a40, SceneManager_FindSorted0, "rx_anim", OUT_EAX) { m.r[0] = rx_scene_manager_find_sorted0(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
ANIM_PORT(0x007d3b50, SceneManager_FindSorted1, "rx_anim", OUT_EAX) { m.r[0] = rx_scene_manager_find_sorted1(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
ANIM_PORT(0x007d3c40, SceneManager_FindFlagged28, "rx_anim", OUT_EAX) { m.r[0] = rx_scene_manager_find_flagged28(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
ANIM_PORT(0x007d3d30, SceneManager_FindFlagged2c, "rx_anim", OUT_EAX) { m.r[0] = rx_scene_manager_find_flagged2c(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
ANIM_PORT(0x007d3850, SceneManager_Unlink, "rx_anim", 0) { rx_scene_manager_unlink(m.arg(0), m.arg(1)); return m.ret(8); }
ANIM_PORT(0x007d3580, SceneManager_Link, "rx_anim", 0) { rx_scene_manager_link(m.arg(0), m.arg(1)); return m.ret(8); }
// 007d2f70: EDI = entity, stack scene; RET 4 (the callee's RET 8 also pops the saved EDI: EDI comes back as the entity anyway).
ANIM_PORT(0x007d2f70, SceneManager_AddEntity, "rx_anim", 0) { rx_scene_manager_add_entity(m.r[7], m.arg(0)); return m.ret(4); }
// 0084dd30 (EDI = the name table; stdcall the address of the key slot; RET 4): EAX = 1 when the entry was removed
ANIM_PORT(0x0084dd30, StrTab_Remove, "rx_anim", OUT_EAX) { m.r[0] = (uint32_t)rx_strtab_remove(m.r[7], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x007d3040, SceneManager_RemoveEntity, "rx_anim", 0) { rx_scene_manager_remove_entity(m.r[6], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x007d7230, EventGrid_CellsInRadius, "rx_anim", OUT_EAX) {
    m.r[0] = rx_event_grid_cells_in_radius(m.r[1], m.r[0], xmm_f(m, 0), m.arg(0)); return m.ret(4);
}
ANIM_PORT(0x007da810, EventCell_RemoveB, "rx_anim", OUT_EAX) { m.r[0] = rx_event_cell_remove_b(m.arg(0), m.arg(1)); return m.ret(8); }
ANIM_PORT(0x007dad20, EventCell_RemoveA, "rx_anim", OUT_EAX) { m.r[0] = rx_event_cell_remove_a(m.arg(0), m.arg(1)); return m.ret(8); }
ANIM_PORT(0x007d61a0, EventCell_Push38, "rx_anim", 0) { rx_event_cell_push_l38(m.r[6], m.r[7]); return m.ret(0); }
ANIM_PORT(0x007d6220, EventCell_Push44, "rx_anim", 0) { rx_event_cell_push_l44(m.r[6], m.r[7]); return m.ret(0); }
ANIM_PORT(0x007d62a0, EventCell_Push20, "rx_anim", 0) { rx_event_cell_push_l20(m.r[6], m.r[7]); return m.ret(0); }
ANIM_PORT(0x007da4d0, EventGrid_Add38, "rx_anim", 0) {
    rx_event_grid_add_l38(m.r[1], m.r[2], m.arg(0), rx_oracle::f32(m.arg(1)), rx_oracle::f32(m.arg(2))); return m.ret(0xc);
}
ANIM_PORT(0x007da5a0, EventGrid_Add44, "rx_anim", 0) {
    rx_event_grid_add_l44(m.r[1], m.r[2], m.arg(0), rx_oracle::f32(m.arg(1)), rx_oracle::f32(m.arg(2))); return m.ret(0xc);
}
ANIM_PORT(0x007da670, EventGrid_Add20, "rx_anim", 0) {
    rx_event_grid_add_l20(m.r[1], m.r[2], m.arg(0), rx_oracle::f32(m.arg(1)), rx_oracle::f32(m.arg(2))); return m.ret(0xc);
}
ANIM_PORT(0x007d6320, EventCell_PushA, "rx_anim", 0) { rx_event_cell_push_a(m.r[6], m.r[7]); return m.ret(0); }
ANIM_PORT(0x007da1d0, EventGrid_RemoveB, "rx_anim", OUT_EAX) {
    m.r[0] = rx_event_grid_remove_b(m.r[7], m.arg(0), rx_oracle::f32(m.arg(1)), m.arg(2)); return m.ret(0xc);
}
ANIM_PORT(0x007da410, EventGrid_RemoveA, "rx_anim", OUT_EAX) {
    m.r[0] = rx_event_grid_remove_a(m.r[7], m.arg(0), rx_oracle::f32(m.arg(1)), m.arg(2)); return m.ret(0xc);
}
ANIM_PORT(0x007da740, EventGrid_AddA, "rx_anim", 0) {
    rx_event_grid_add_a(m.r[1], m.r[2], m.arg(0), rx_oracle::f32(m.arg(1)), rx_oracle::f32(m.arg(2))); return m.ret(0xc);
}
ANIM_PORT(0x007dfdd0, Lights_Add, "rx_anim", 0) { rx_lights_add(m.arg(0), m.arg(1)); return m.ret(8); }
ANIM_PORT(0x007d4030, ModelInstance_Destroy, "rx_anim", 0) { FxBind fb_(m); rx_model_instance_destroy(m.r[6], m.r[1]); return m.ret(0); }

// 007d2500 Scene_RegisterEntityName (EAX = the scene, stack entity; RET 4). Void.
ANIM_PORT(0x007d2500, Scene_RegisterEntityName, "rx_anim", 0) { rx_scene_register_entity_name(m.r[0], m.arg(0)); return m.ret(4); }
