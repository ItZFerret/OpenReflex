// loop_player.cpp -- oracle adapters for core/game/loop_player.c (group "rx_loop").
#include "oracle.h"
#include "fx_oracle.h"   /* FxBind: 00545120's frame registers (loop_player.c) */
extern "C" {
#include "../game/loop_player.h"
}
using namespace reflex_port;
using rx_oracle::f32;

// stdcall (p, dt); ret 8; void
RX_PORT(0x0054af90, MXPlayer_Update, "rx_loop", OUT_FPU) { rx_player_update(m.arg(0), f32(m.arg(1))); return m.ret(8); }
RX_PORT(0x0054c150, Player_UpdateCamera, "rx_loop", OUT_FPU) { rx_player_update_camera(m.arg(0), f32(m.arg(1))); return m.ret(8); }
// EAX = p, ECX = camera (the Camera key's Player_SetCamera); result EAX
RX_PORT(0x0054c9b0, Player_SetCamera, "rx_loop", OUT_EAX) { m.r[0] = rx_player_set_camera(m.r[0], m.r[1]); return m.ret(0); }
// ECX = arg, ESI = the event dispatcher, stack player; ret 4; void
RX_PORT(0x00554ce0, Player_CameraChanged, "rx_loop", OUT_FPU) { rx_player_camera_changed(m.r[1], m.r[6], m.arg(0)); return m.ret(4); }
// EAX = p; stack dt; ret 4
RX_PORT(0x0054cc10, Player_UpdateCameras, "rx_loop", OUT_FPU) { rx_player_update_cameras(m.r[0], f32(m.arg(0))); return m.ret(4); }
// EDI = p; stack dt; ret 4
RX_PORT(0x0054cec0, Player_UpdateIdleRider, "rx_loop", OUT_FPU) { rx_player_update_idle_rider(m.r[7], f32(m.arg(0))); return m.ret(4); }
// ECX = p; stack dt; ret 4
RX_PORT(0x0054eee0, Player_UpdateResetPoint, "rx_loop", OUT_FPU) { rx_player_update_reset_point(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0054f3e0, Player_CheckResetTerrain, "rx_loop", OUT_FPU) { rx_player_check_reset_terrain(m.r[0]); return m.ret(0); }
RX_PORT(0x0054f330, Player_CheckOutOfBounds, "rx_loop", OUT_FPU) { rx_player_check_out_of_bounds(m.r[0]); return m.ret(0); }
RX_PORT(0x0054e4f0, Player_HandleResetInput, "rx_loop", OUT_FPU) { rx_player_handle_reset_input(m.r[1]); return m.ret(0); }
RX_PORT(0x0054ea30, Player_UpdateWreckRecovery, "rx_loop", OUT_FPU) { rx_player_update_wreck_recovery(m.r[0], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0054ee00, Player_UpdateCrashResetTimer, "rx_loop", OUT_FPU) { rx_player_update_crash_reset_timer(m.r[0], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0054b850, Player_UpdateRespawnFade, "rx_loop", OUT_FPU) { rx_player_update_respawn_fade(m.r[0], f32(m.arg(0))); return m.ret(4); }
// EAX = p; result EAX (0 / 1)
RX_PORT(0x0054d7c0, Player_CanReset, "rx_loop", OUT_EAX) { m.r[0] = rx_player_can_reset(m.r[0]); return m.ret(0); }
RX_PORT(0x0054d480, Player_RestoreInputGroups, "rx_loop", OUT_FPU) { rx_player_restore_input_groups(m.r[0]); return m.ret(0); }
RX_PORT(0x0054ec40, Player_CancelWreck, "rx_loop", OUT_FPU) { rx_player_cancel_wreck(m.r[0]); return m.ret(0); }
// EAX = p; stack p; ret 4
RX_PORT(0x0054ec80, Player_WreckRecoverySuccess, "rx_loop", OUT_FPU) { rx_player_wreck_recovery_success(m.arg(0)); return m.ret(4); }
// ESI = p
RX_PORT(0x0054b720, Player_SetFadeHold, "rx_loop", OUT_FPU) { rx_player_set_fade_hold(m.r[6]); return m.ret(0); }
RX_PORT(0x0054b780, Player_SetFadeOut, "rx_loop", OUT_FPU) { rx_player_set_fade_out(m.r[6]); return m.ret(0); }
RX_PORT(0x0054b7f0, Player_SetFadeClear, "rx_loop", OUT_FPU) { rx_player_set_fade_clear(m.r[6]); return m.ret(0); }
// EAX = p; stack flag; ret 4
RX_PORT(0x0054fff0, Player_Reset, "rx_loop", OUT_FPU) { rx_player_reset(m.r[0], m.arg(0)); return m.ret(4); }
// ECX = p; stack who, arg; ret 8
RX_PORT(0x0054d2a0, Player_EventA, "rx_loop", OUT_FPU) { rx_player_event_a(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0054d1a0, Player_StuntHoldBegin, "rx_loop", OUT_FPU) { rx_player_event_hold_begin(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0054d1e0, Player_StuntHoldEnd, "rx_loop", OUT_FPU) { rx_player_event_hold_end(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0054d310, Player_StuntLanded, "rx_loop", OUT_FPU) { rx_player_event_landed(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0054d360, Player_StuntTakeoff, "rx_loop", OUT_FPU) { rx_player_event_takeoff(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0054d530, Player_EventB, "rx_loop", OUT_FPU) { rx_player_event_b(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x0054f0b0, Player_EventName, "rx_loop", OUT_FPU) { rx_player_event_name(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
// EAX = a, ESI = b; stack p, c; ret 8
RX_PORT(0x0054feb0, Player_PostVectors, "rx_loop", OUT_FPU) { rx_player_post_vectors(m.r[0], m.r[6], m.arg(0), m.arg(1)); return m.ret(8); }
// EAX = p, EDI = on
RX_PORT(0x0054fcd0, Player_SetGroup0, "rx_loop", OUT_FPU) { rx_player_set_group0(m.r[0], m.r[7]); return m.ret(0); }
RX_PORT(0x0054fd20, Player_SetGroup3, "rx_loop", OUT_FPU) { rx_player_set_group3(m.r[0], m.r[7]); return m.ret(0); }
// ECX = p, EDX = on
RX_PORT(0x0054fd70, Player_SetGroup147c, "rx_loop", OUT_FPU) { rx_player_set_group_147c(m.r[1], m.r[2]); return m.ret(0); }
RX_PORT(0x0054fdc0, Player_SetGroup167c, "rx_loop", OUT_FPU) { rx_player_set_group_167c(m.r[1], m.r[2]); return m.ret(0); }
RX_PORT(0x0054fe10, Player_SetGroup748, "rx_loop", OUT_FPU) { rx_player_set_group_748(m.r[1], m.r[2]); return m.ret(0); }
RX_PORT(0x0054fe60, Player_SetGroupF34, "rx_loop", OUT_FPU) { rx_player_set_group_f34(m.r[1], m.r[2]); return m.ret(0); }
// ESI = slot, EDI = on
RX_PORT(0x00548bf0, Player_SlotGroup2, "rx_loop", OUT_FPU) { rx_player_slot_group2(m.r[6], m.r[7]); return m.ret(0); }
RX_PORT(0x00548c40, Player_SlotGroup5, "rx_loop", OUT_FPU) { rx_player_slot_group5(m.r[6], m.r[7]); return m.ret(0); }
// EAX = slot; result st(0)
RX_PORT(0x005485c0, Player_Analog348, "rx_loop", OUT_FPU) {
    m.fpu.push_back(reflex_f80::from_f32(rx_oracle::bits(rx_player_analog_348(m.r[0]))));
    return m.ret(0);
}
RX_PORT(0x00548880, Player_Pressed2fa0, "rx_loop", OUT_EAX) { m.r[0] = rx_player_pressed_2fa0(m.r[0]); return m.ret(0); }
RX_PORT(0x00548850, Player_Pressed2da0, "rx_loop", OUT_EAX) { m.r[0] = rx_player_pressed_2da0(m.r[0]); return m.ret(0); }
RX_PORT(0x005488b0, Player_Pressed31a0, "rx_loop", OUT_EAX) { m.r[0] = rx_player_pressed_31a0(m.r[0]); return m.ret(0); }
// 00548960 (ESI = player slot; result EAX): the wreck type of the scripted control state ([0x00af6c00] is never set by the exe)
RX_PORT(0x00548960, Player_ScriptedWreckType, "rx_loop", OUT_EAX) { m.r[0] = rx_player_scripted_wreck_type(m.r[6]); return m.ret(0); }
// EAX = pos, ESI = tracker
RX_PORT(0x004db070, Tracker_Land, "rx_loop", OUT_FPU) { rx_player_tracker_land(m.r[6], m.r[0]); return m.ret(0); }
// EAX = tracker, EDI = vehicle; stack dt; ret 4
RX_PORT(0x004db160, Tracker_Update, "rx_loop", OUT_FPU) { rx_player_tracker_update(m.r[0], m.r[7], f32(m.arg(0))); return m.ret(4); }
// ECX = lock object
RX_PORT(0x00883ac0, Lock_EnterRaw, "rx_loop", OUT_FPU) { rx_lock_enter_raw(m.r[1]); return m.ret(0); }
RX_PORT(0x00883ad0, Lock_LeaveRaw, "rx_loop", OUT_FPU) { rx_lock_leave_raw(m.r[1]); return m.ret(0); }
RX_PORT(0x00883d80, Lock_Enter, "rx_loop", OUT_FPU) { rx_lock_enter(m.r[1]); return m.ret(0); }
RX_PORT(0x00883d90, Lock_Leave, "rx_loop", OUT_FPU) { rx_lock_leave(m.r[1]); return m.ret(0); }

// ECX = delegate; stack arg; ret 4; the callee's EAX (the only caller, the event dispatch 00592e64, reads none)
RX_PORT(0x00554fc0, Delegate_Invoke, "rx_loop", OUT_FPU) { m.r[0] = rx_delegate_invoke(m.r[1], m.arg(0)); return m.ret(4); }
// ECX = entity; stack a, b; ret 8
RX_PORT(0x0055dc20, Entity_PlaceBody, "rx_loop", OUT_FPU) { rx_entity_place_body(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0055e820, Activity_vf12, "rx_loop", OUT_EAX) { m.r[0] = rx_activity_can_restart(m.r[1]); return m.ret(0); }
RX_PORT(0x00565640, MXActivityRace_vf52, "rx_loop", OUT_FPU) { rx_race_audio_state(m.r[1]); return m.ret(0); }
// ECX = race; stack 4 dwords; ret 0x10
RX_PORT(0x00567660, MXRace_PopRider, "rx_loop", OUT_FPU) { rx_race_pop_rider(m.r[1], m.arg(0), int32_t(m.arg(1)), m.arg(2), m.arg(3)); return m.ret(0x10); }
// EAX = object
RX_PORT(0x00525cd0, Activity_ApplyTuning, "rx_loop", OUT_FPU) { rx_activity_apply_tuning(m.r[0]); return m.ret(0); }
// cdecl (rec, delta, name)
RX_PORT(0x00764fc0, MXRecords_AddInt, "rx_loop", OUT_FPU) { rx_records_add_int(m.arg(0), int32_t(m.arg(1)), m.arg(2)); return m.ret(0); }
// EAX = index, ECX = base
RX_PORT(0x00764ef0, MXRecords_HoleshotWon, "rx_loop", OUT_FPU) { rx_records_holeshot_won(m.r[0], m.r[1]); return m.ret(0); }
RX_PORT(0x00764f40, MXRecords_AttemptedJump, "rx_loop", OUT_FPU) { rx_records_attempted_jump(m.r[0], m.r[1]); return m.ret(0); }
RX_PORT(0x00764f80, MXRecords_FailedJump, "rx_loop", OUT_FPU) { rx_records_failed_jump(m.r[0], m.r[1]); return m.ret(0); }
// EAX = index, ECX = base; stack value; ret 4; result EAX
RX_PORT(0x00764b90, MXRecords_LongestWheelie, "rx_loop", OUT_FPU) { m.r[0] = rx_records_longest_wheelie(m.r[0], m.r[1], int32_t(m.arg(0))); return m.ret(4); }
// ESI = header; EAX = header, ECX = the entry ECX (push ecx ... pop ecx)
RX_PORT(0x005442c0, List_Init, "rx_loop", OUT_EAX | OUT_ECX) {
    const uint32_t ecx = m.r[1];
    m.r[0] = rx_list_init(m.r[6]);
    m.r[1] = ecx;
    return m.ret(0);
}
// EAX = first; stack last; cdecl
RX_PORT(0x00545120, Records_FreeRange, "rx_loop", OUT_FPU) { FxBind fb_(m); rx_records_free_range(m.r[0], m.arg(0)); return m.ret(0); }
// ECX = table; stack index; ret 4; result EAX
RX_PORT(0x008841d0, Table11_Get, "rx_loop", OUT_EAX) { m.r[0] = rx_table11_get(m.r[1], m.arg(0)); return m.ret(4); }
// cdecl (a0..a3)
RX_PORT(0x00884390, Thread_Note, "rx_loop", OUT_FPU) { rx_thread_note(m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0); }
// EAX = obj, EDI = manager; stack dt; ret 4
RX_PORT(0x00540130, PackageManager_Update, "rx_loop", OUT_FPU) { rx_package_manager_update(m.r[0], m.r[7], f32(m.arg(0))); return m.ret(4); }

RX_PORT(0x0054aef0, Player_ApplyDrivingCamera, "rx_PlayerDrivingCamera", OUT_ECX) {
    m.r[1] = rx_player_apply_driving_camera(m.r[1]);
    return m.ret(0);
}
