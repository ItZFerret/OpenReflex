// world_misc.cpp -- oracle adapters for core/game/world_misc.c (group rx_world).
#include "oracle.h"
extern "C" {
#include "../game/world_misc.h"
#include "../game/world_rider.h"
}
using namespace reflex_port;
using rx_oracle::f32;
using rx_oracle::bits;

static void xmm0_ss(reflex_vm::Machine& m, float v) { m.xmm[0].lo = bits(v); m.xmm[0].hi = 0; }

RX_PORT(0x0051bd60, Entity_vf13, "rx_world", OUT_EAX) { return m.ret(0, rx_world_entity_vf13(m.r[1])); }
RX_PORT(0x00617e20, BehaviorLapPacing_vf5, "rx_world", OUT_EAX) { return m.ret(0, rx_world_lap_pacing_vf5(m.r[1])); }
RX_PORT(0x00617f30, RiderAnimation_vf12, "rx_world", OUT_EAX) { return m.ret(0, rx_world_rider_animation_vf12(m.r[1])); }
RX_PORT(0x005a4150, rtti_shared_vf_005a4150, "rx_world", 0) { rx_world_shared_vf_ret8(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x005de600, rtti_shared_vf_005de600, "rx_world", 0) {
    rx_world_shared_vf_ret16(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3));
    return m.ret(0x10);
}
// 00592b20: a tail jump to vf13 (RET 8 there); EAX its result.
RX_PORT(0x00592b20, MXSeriesEvent_OnAdvancePostEvent, "rx_world", OUT_EAX) {
    return m.ret(8, rx_world_series_event_on_advance_post(m.r[1], m.arg(0), m.arg(1)));
}
RX_PORT(0x0055eb10, BaseScenario_vf8, "rx_world", OUT_EAX) { return m.ret(0, rx_world_base_scenario_vf8(m.r[1])); }
RX_PORT(0x0055ebd0, MXActivityFreeRide_vf8, "rx_world", OUT_EAX) { return m.ret(0, rx_world_free_ride_vf8(m.r[1])); }
RX_PORT(0x0055e960, Activity_vf21, "rx_world", OUT_EAX) { return m.ret(0, rx_world_activity_vf21(m.r[1])); }
RX_PORT(0x0056a020, MXActivityRaceStub_vf8, "rx_world", OUT_EAX) { return m.ret(0, rx_world_race_stub_vf8(m.r[1])); }
RX_PORT(0x00617d20, AnimationEntity_vf14, "rx_world", OUT_EAX) { return m.ret(0, rx_world_anim_entity_matrix(m.r[1])); }
RX_PORT(0x00617dc0, AnimationEntity_vf16, "rx_world", 0) {
    rx_world_anim_entity_set_matrix_regs(m.r[1], m.arg(0), m.r[3], m.r[6], m.r[7]);
    return m.ret(4);
}
RX_PORT(0x00617f00, FUN_00617f00, "rx_world", OUT_EAX | OUT_EDX) {
    const uint32_t r = rx_world_rider_pose_is_current(m.r[1]);
    m.r[2] = r;
    return m.ret(0, r);
}
RX_PORT(0x00545340, FUN_00545340, "rx_world", OUT_EAX | OUT_EDX) {
    const uint32_t r = rx_world_rider_pose_is_current_5a0(m.r[1]);
    m.r[2] = r;
    return m.ret(0, r);
}
RX_PORT(0x006298f0, FUN_006298f0, "rx_world", 0) { rx_world_blend_set_18(m.r[0], f32(uint32_t(m.xmm[0].lo))); return m.ret(0); }
RX_PORT(0x00629900, FUN_00629900, "rx_world", 0) { rx_world_blend_set_1c(m.r[0], f32(uint32_t(m.xmm[0].lo))); return m.ret(0); }
RX_PORT(0x004db570, FUN_004db570, "rx_world", OUT_EAX) { return m.ret(0, rx_world_record_vector_size(m.r[1])); }
RX_PORT(0x004dbfa0, FUN_004dbfa0, "rx_world", 0) { rx_world_record_construct(m.r[0], m.r[1]); return m.ret(0); }
RX_PORT(0x004dbed0, FUN_004dbed0, "rx_world", OUT_EAX) { return m.ret(0, rx_world_record_copy_range(m.r[1], m.r[2], m.arg(0))); }
// 004dbe80 (EDI = destination, EAX = last; cdecl first): zeroes the low byte of its argument slot (a tag); EAX.
RX_PORT(0x004dbe80, FUN_004dbe80, "rx_world", OUT_EAX) {
    const uint32_t first = m.arg(0);
    SU8(m.r[4] + 4, 0, 0);
    return m.ret(0, rx_world_record_uninitialized_copy(first, m.r[0], m.r[7]));
}
// 00646e80 (EAX = mode, ESI = camera; plain ret). ECX / EDX are what vf19 leaves (undeclared).
RX_PORT(0x00646e80, QuickCamera_SetMode, "rx_world", 0) { rx_world_quick_camera_set_mode(m.r[6], m.r[0]); return m.ret(0); }
RX_PORT(0x00544cc0, FUN_00544cc0, "rx_world", OUT_EAX) { return m.ret(4, rx_world_ptr_vector_push_unique(m.r[6], m.arg(0))); }
RX_PORT(0x00548660, FUN_00548660, "rx_world", OUT_XMM0) { xmm0_ss(m, rx_world_pad_look_back(m.r[6])); return m.ret(0); }
RX_PORT(0x00548b00, FUN_00548b00, "rx_world", 0) { rx_world_pad_rumble(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0); }
RX_PORT(0x00548b70, FUN_00548b70, "rx_world", OUT_EAX) { return m.ret(0, rx_world_pad_reset_combo(m.r[6])); }
RX_PORT(0x00548f20, FUN_00548f20, "rx_world", OUT_EAX) { return m.ret(0, rx_world_pad_steer_map(m.r[0])); }
RX_PORT(0x00548f70, FUN_00548f70, "rx_world", OUT_EAX) { return m.ret(0, rx_world_pad_lean_map(m.r[0])); }
RX_PORT(0x00548fc0, FUN_00548fc0, "rx_world", OUT_EAX) { return m.ret(0, rx_world_pad_wreck_steer_map(m.r[6])); }
RX_PORT(0x00549050, FUN_00549050, "rx_world", OUT_EAX) { return m.ret(0, rx_world_pad_wreck_lean_map(m.r[6])); }
RX_PORT(0x0054ee80, FUN_0054ee80, "rx_world", 0) { rx_world_rider_wreck_input(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00554da0, FUN_00554da0, "rx_world", 0) { rx_world_raise_event(m.r[1], m.r[2], m.r[6], m.arg(0)); return m.ret(4); }
RX_PORT(0x0054e330, FUN_0054e330, "rx_world", 0) { rx_world_wreck_checker_add_severity(m.r[2]); return m.ret(0); }
RX_PORT(0x0054e890, FUN_0054e890, "rx_world", 0) { rx_world_rider_clear_wreck(m.r[0]); return m.ret(0); }
RX_PORT(0x0054e8f0, FUN_0054e8f0, "rx_world", 0) { rx_world_rider_start_wreck(m.r[7], m.r[0], m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0054ed10, FUN_0054ed10, "rx_world", 0) { rx_world_rider_fall(m.r[7], m.r[6]); return m.ret(0); }
RX_PORT(0x0055a050, FUN_0055a050, "rx_world", OUT_EAX) { return m.ret(0, uint32_t(rx_world_group_rider_index(m.r[1], m.r[6]))); }
RX_PORT(0x00581c30, ActivityAudioLogic_vf13, "rx_world", 0) { rx_world_activity_audio_vf13(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00581cb0, ActivityAudioLogic_vf14, "rx_world", 0) { rx_world_activity_audio_vf14(m.r[1], m.arg(0), m.arg(1), m.r[5]); return m.ret(8); }
RX_PORT(0x00586660, FUN_00586660, "rx_world", OUT_EAX) { return m.ret(4, rx_world_tree_rotate_left(m.r[1], m.arg(0))); }
RX_PORT(0x005866b0, FUN_005866b0, "rx_world", OUT_EAX) { return m.ret(4, rx_world_tree_rotate_right(m.r[1], m.arg(0))); }
RX_PORT(0x00586c10, FUN_00586c10, "rx_world", 0) { rx_world_tree_iterator_decrement(m.r[6]); return m.ret(0); }
RX_PORT(0x00593220, FUN_00593220, "rx_world", OUT_EAX) { return m.ret(8, rx_world_registry_find(m.arg(0), m.arg(1))); }
RX_PORT(0x005c3380, FUN_005c3380, "rx_world", OUT_EAX) { return m.ret(0, rx_world_elf_hash(m.r[2])); }
RX_PORT(0x00613420, FUN_00613420, "rx_world", 0) { rx_world_swap_dwords(m.arg(0), m.arg(1)); return m.ret(0); }
RX_PORT(0x005cf8f0, UIContainer_vf14, "rx_world", 0) { rx_world_ui_container_vf14(m.r[1]); return m.ret(0); }
RX_PORT(0x005d57e0, FUN_005d57e0, "rx_world", 0) { rx_world_ui_set_color(m.r[1], m.r[0]); return m.ret(0); }
RX_PORT(0x00592d80, FUN_00592d80, "rx_world", 0) { rx_world_handler_list_call(m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
// rider (world_rider_62.c)
RX_PORT(0x00623960, FUN_00623960, "rx_world", OUT_EAX) { return m.ret(0, rx_world_rider_chain_group(m.r[0], m.r[1])); }
RX_PORT(0x00623310, FUN_00623310, "rx_world", 0) { rx_world_rider_end_wreck(m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0062a370, FUN_0062a370, "rx_world", 0) { rx_world_gesture_set_level(m.r[0], int32_t(m.r[7])); return m.ret(0); }
RX_PORT(0x0062a420, FUN_0062a420, "rx_world", 0) { rx_world_gesture_set_hold(m.r[2], m.r[7]); return m.ret(0); }
RX_PORT(0x0062a9a0, FUN_0062a9a0, "rx_world", OUT_EAX) { return m.ret(0, rx_world_gesture_allowed(m.r[0])); }
RX_PORT(0x0062aa30, FUN_0062aa30, "rx_world", 0) { rx_world_gesture_wobble(m.r[1]); return m.ret(0); }
RX_PORT(0x0062a490, FUN_0062a490, "rx_world", 0) { rx_world_gesture_update(m.r[6], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x004db5b0, Vector16_EraseRange, "rx_world", OUT_EAX) {
    return m.ret(0x14, rx_world_record_vector_erase(m.r[6], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4)));
}
RX_PORT(0x007fa030, FUN_007fa030, "rx_world", OUT_EAX) { return m.ret(4, rx_world_string_table_find(m.r[6], m.r[0], m.arg(0))); }
RX_PORT(0x005860e0, FUN_005860e0, "rx_world", OUT_EAX) {
    return m.ret(0xc, rx_world_tree_insert_at(m.r[0], m.r[7], m.arg(0), m.arg(1), m.arg(2)));
}
RX_PORT(0x0054c020, Player_ActiveCameraEntity, "rx_world", OUT_EAX) { return m.ret(0, rx_world_player_active_camera_entity(m.r[7])); }
RX_PORT(0x006243d0, FUN_006243d0, "rx_world", 0) { rx_world_rider_root_matrix(m.r[1], m.r[0]); return m.ret(0); }
RX_PORT(0x0061e950, FUN_0061e950, "rx_world", 0) { rx_world_rider_arm_side(m.r[7]); return m.ret(0); }
RX_PORT(0x0061e3d0, FUN_0061e3d0, "rx_world", 0) { rx_world_rider_update_locomotion(m.r[0], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0061c210, RiderAnimation_vf19, "rx_world", 0) { rx_world_rider_animation_vf19(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0061f160, FUN_0061f160, "rx_world", 0) { rx_world_rider_update_state(m.arg(0), f32(m.arg(1))); return m.ret(8); }
RX_PORT(0x00621100, FUN_00621100, "rx_world", 0) { rx_world_rider_slow_pose(m.r[6]); return m.ret(0); }
RX_PORT(0x006211d0, FUN_006211d0, "rx_world", 0) { rx_world_rider_crouch_pose(m.r[0]); return m.ret(0); }
