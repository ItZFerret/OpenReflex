// race_activity.cpp -- oracle adapters for core/game/race_activity*.c.
#include "race_oracle.h"
extern "C" {
#include "../game/race.h"
#include "../game/race_activity.h"
}
using namespace reflex_port;

// 00556280 (EAX = activity) -> EAX.
RX_PORT(0x00556280, Activity_UiKind, "rx_race", OUT_EAX) { m.r[0] = rx_activity_ui_kind(m.r[0]); return m.ret(0); }
RX_PORT(0x00567520, RaceStub_Compare, "rx_race", OUT_EAX) { m.r[0] = uint32_t(rx_race_stub_compare(m.r[1], m.arg(0), m.arg(1), m.arg(2))); return m.ret(0xc); }
RX_PORT(0x00559eb0, Activity_SortStandings, "rx_race", 0) { rx_activity_sort_standings(m.r[6]); return m.ret(0); }
RX_PORT(0x00567f30, Race_LappingWarnings, "rx_race", 0) { rx_race_lapping_warnings(m.arg(0)); return m.ret(4); }
RX_PORT(0x00567c70, Race_TimeGap, "rx_race", 0) { rx_race_time_gap(m.r[7], m.r[6], m.arg(0)); return m.ret(4); }
RX_PORT(0x00552ce0, Players_Count, "rx_race", OUT_EAX) { m.r[0] = rx_players_count(m.r[6], m.r[7]); return m.ret(0); }
RX_PORT(0x00552a10, Players_UpdateCameras, "rx_race", 0) { rx_players_update_cameras(m.r[0], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00550f00, Race_DryGround, "rx_race", OUT_EAX) { m.r[0] = uint32_t(rx_race_dry_ground(m.r[0])); return m.ret(0); }
RX_PORT(0x00567b10, Race_GapsToLeader, "rx_race", 0) { rx_race_gaps_to_leader(m.arg(0)); return m.ret(4); }
RX_PORT(0x00567e30, Race_LapFlags, "rx_race", 0) { rx_race_lap_flags(m.r[7]); return m.ret(0); }
RX_PORT(0x00569820, Race_LapHighlight, "rx_race", 0) { rx_race_lap_highlight(m.arg(0)); return m.ret(4); }
RX_PORT(0x00566e70, RaceStub_DriverDone, "rx_race", OUT_EAX) { m.r[0] = rx_race_driver_done(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00565270, Race_InProgress, "rx_race", 0) { rx_race_activity_in_progress(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x005680c0, RaceStub_Update, "rx_race", 0) { rx_race_stub_update(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
struct ActFpuKeep { reflex_vm::Machine& m; std::vector<reflex_f80::F80> fpu; explicit ActFpuKeep(reflex_vm::Machine& mm) : m(mm), fpu(mm.fpu) {} ~ActFpuKeep() { m.fpu = fpu; } };
RX_PORT(0x00559d30, Activity_UpdateDrivers, "rx_race", 0) { ActFpuKeep k(m); rx_activity_update_drivers(m.r[6], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0055e260, Activity_UpdateSequences, "rx_race", 0) { rx_activity_update_sequences(m.arg(0), rx_oracle::f32(m.arg(1))); return m.ret(8); }
RX_PORT(0x00558930, Activity_StateUpdate, "rx_race", 0) { ActFpuKeep k(m); rx_activity_state_update(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00559c10, Activity_Update, "rx_race", 0) { ActFpuKeep k(m); rx_activity_update(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x005594a0, Activity_InProgress, "rx_race", 0) { ActFpuKeep k(m); rx_activity_in_progress(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00568a20, RaceStub_InProgress, "rx_race", 0) { ActFpuKeep k(m); rx_race_stub_in_progress(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00585860, Activity_StateName, "rx_race", OUT_EAX) { m.r[0] = rx_activity_state_name(m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x005586c0, Activity_SetState, "rx_race", 0) { ActFpuKeep k(m); rx_activity_set_state(m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00558880, Activity_EnterState, "rx_race", 0) { ActFpuKeep k(m); rx_activity_enter_state(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00558ce0, Activity_Pre, "rx_race", 0) { ActFpuKeep k(m); rx_activity_pre(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
// ActivityAudioLogic
RX_PORT(0x00585ad0, Audio_CueCount, "rx_race", OUT_EAX) { m.r[0] = rx_audio_cue_count(m.r[2], m.r[0]); return m.ret(0); }
RX_PORT(0x005812f0, Audio_PlayCue, "rx_race", 0) { rx_audio_play_cue(m.r[1], m.r[7]); return m.ret(0); }
RX_PORT(0x005811a0, Audio_Cue, "rx_race", 0) { rx_audio_cue(m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0x10); }
RX_PORT(0x00580870, Audio_DefaultModifier, "rx_race", OUT_EAX) { m.r[0] = rx_audio_default_modifier(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4)); return m.ret(0x1c); }
RX_PORT(0x005809d0, Audio_CountdownModifier, "rx_race", OUT_EAX) { m.r[0] = rx_audio_countdown_modifier(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4)); return m.ret(0x1c); }
RX_PORT(0x00580a80, Audio_HumanModifier, "rx_race", OUT_EAX) { m.r[0] = rx_audio_human_modifier(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4)); return m.ret(0x1c); }
RX_PORT(0x00555de0, Audio_ModifierNop, "rx_race", 0) { return m.ret(0x1c); }
RX_PORT(0x00581570, Audio_PlaceCues, "rx_race", 0) { ActFpuKeep k(m); rx_audio_place_cues(m.arg(0)); return m.ret(4); }
RX_PORT(0x00581010, Audio_Update, "rx_race", 0) { ActFpuKeep k(m); rx_audio_update(m.r[1]); return m.ret(4); }
RX_PORT(0x005818c0, Audio_DriverCheer, "rx_race", 0) { ActFpuKeep k(m); rx_audio_driver_cheer(m.r[1], m.arg(0)); return m.ret(0xc); }
RX_PORT(0x005819f0, Audio_StuntEnd, "rx_race", 0) { ActFpuKeep k(m); rx_audio_stunt_end(m.r[1], m.arg(0)); return m.ret(0xc); }
RX_PORT(0x005816e0, Audio_CountdownGo, "rx_race", 0) { ActFpuKeep k(m); rx_audio_countdown_go(); return m.ret(0); }
RX_PORT(0x00582150, Audio_CountdownStep, "rx_race", 0) { ActFpuKeep k(m); rx_audio_countdown_step(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00582270, Audio_InProgress, "rx_race", 0) { ActFpuKeep k(m); rx_audio_in_progress(m.r[1], m.arg(0)); return m.ret(8); }
RX_PORT(0x005822e0, Audio_FreeRideInProgress, "rx_race", 0) { ActFpuKeep k(m); rx_audio_free_ride_in_progress(m.r[1], m.arg(0)); return m.ret(8); }
RX_PORT(0x00581f90, Audio_PreExited, "rx_race", 0) { ActFpuKeep k(m); rx_audio_pre_exited(m.r[1]); return m.ret(8); }
RX_PORT(0x00581fd0, Audio_CountdownEntered, "rx_race", 0) { ActFpuKeep k(m); rx_audio_countdown_entered(m.r[1]); return m.ret(8); }
RX_PORT(0x00581ff0, Audio_CountdownExited, "rx_race", 0) { ActFpuKeep k(m); rx_audio_countdown_exited(m.r[1]); return m.ret(8); }
RX_PORT(0x00583130, Audio_InProgressExited, "rx_race", 0) { ActFpuKeep k(m); rx_audio_in_progress_exited(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00583170, Audio_WindDownEntered, "rx_race", 0) { ActFpuKeep k(m); rx_audio_wind_down_entered(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00582070, Audio_WindDownExited, "rx_race", 0) { ActFpuKeep k(m); rx_audio_wind_down_exited(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x005848a0, Audio_PostEntered, "rx_race", 0) { ActFpuKeep k(m); rx_audio_post_entered(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x005820d0, Audio_PostExited, "rx_race", 0) { ActFpuKeep k(m); rx_audio_post_exited(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00581bc0, Audio_Crash, "rx_race", 0) { rx_audio_crash(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00555df0, Audio_Name, "rx_race", OUT_EAX) { m.r[0] = rx_audio_name(); return m.ret(0); }
RX_PORT(0x00566ee0, RaceStub_CheckFinish, "rx_race", 0) { ActFpuKeep k(m); rx_race_stub_check_finish(m.r[1]); return m.ret(0); }
RX_PORT(0x00559e60, Activity_HumanDriver, "rx_race", OUT_EAX) { m.r[0] = rx_activity_human_driver(m.r[0], int32_t(m.arg(0))); return m.ret(4); }
RX_PORT(0x0055a000, Activity_DriverOfKind, "rx_race", OUT_EAX) { m.r[0] = rx_activity_driver_of_kind(m.r[0], m.arg(0), int32_t(m.arg(1))); return m.ret(8); }
RX_PORT(0x00559fd0, Activity_PlayerDriver, "rx_race", OUT_EAX) { m.r[0] = rx_activity_player_driver(m.r[0], m.r[7]); return m.ret(0); }
RX_PORT(0x0057b2f0, Activity_Nop, "rx_race", 0) { return m.ret(0); }
RX_PORT(0x00565570, Race_Compare, "rx_race", OUT_EAX) { m.r[0] = uint32_t(rx_race_compare(m.r[1], m.arg(0), m.arg(1), m.arg(2))); return m.ret(0xc); }
RX_PORT(0x0055f700, ActivityManager_Update, "rx_race", 0) { ActFpuKeep k(m); rx_activity_manager_update(m.r[6], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00552930, Players_Deferred, "rx_race", 0) { ActFpuKeep k(m); rx_players_deferred(m.r[7]); return m.ret(0); }
RX_PORT(0x0055cf10, Activity_Presence, "rx_race", 0) { ActFpuKeep k(m); rx_activity_presence(m.r[7]); return m.ret(0); }
RX_PORT(0x00585c60, Map_IteratorValue, "rx_race", OUT_EAX) { m.r[0] = rx_map_iterator_value(m.r[6]); return m.ret(0); }
RX_PORT(0x0058f270, EventProxy_Call3, "rx_race", 0) { ActFpuKeep k(m); rx_event_proxy_call3(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00555680, Net_PendingPlayers, "rx_race", OUT_EAX) { m.r[0] = rx_net_pending_players(); return m.ret(0); }
RX_PORT(0x0055eb00, Scenario_Done, "rx_race", OUT_EAX) { m.r[0] = rx_scenario_done(m.r[1]); return m.ret(0); }
RX_PORT(0x0055e710, Activity_CanStart, "rx_race", OUT_EAX) { m.r[0] = rx_activity_can_start(m.r[1]); return m.ret(0); }
RX_PORT(0x005651b0, Race_Countdown, "rx_race", 0) { ActFpuKeep k(m); rx_race_countdown(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x005851f0, FreeRide_Jumps, "rx_race", 0) { ActFpuKeep k(m); rx_free_ride_jumps(m.r[1], m.r[0], m.r[6], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x005609d0, FreeRide_InProgress, "rx_race", 0) { ActFpuKeep k(m); rx_free_ride_in_progress(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x005604f0, FreeRide_Pre, "rx_race", 0) { ActFpuKeep k(m); rx_free_ride_pre(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00560b70, FreeRide_CheckFinish, "rx_race", 0) { ActFpuKeep k(m); rx_free_ride_check_finish(m.r[1]); return m.ret(0); }
RX_PORT(0x0055a1e0, Activity_EnableControls, "rx_race", 0) { ActFpuKeep k(m); rx_activity_enable_controls(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00502870, NameMap_LowerBound, "rx_race", OUT_EAX) { m.r[0] = rx_name_map_lower_bound(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00569010, RaceStub_RespawnPoint, "rx_race", OUT_EAX) { ActFpuKeep k(m); m.r[0] = rx_race_stub_respawn_point(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0x10); }
RX_PORT(0x00561650, FreeRide_RespawnPoint, "rx_race", OUT_EAX) { ActFpuKeep k(m); m.r[0] = rx_free_ride_respawn_point(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0x10); }
RX_PORT(0x0055d9d0, Activity_PlaceObject, "rx_race", 0) { ActFpuKeep k(m); rx_activity_place_object(m.r[1], m.arg(0)); return m.ret(8); }
RX_PORT(0x0055d870, Activity_PlaceObject2, "rx_race", 0) { ActFpuKeep k(m); rx_activity_place_object2(m.r[1], m.arg(0)); return m.ret(8); }
RX_PORT(0x0055e100, Activity_StartSequence, "rx_race", OUT_EAX) { ActFpuKeep k(m); m.r[0] = rx_activity_start_sequence(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x00569730, Activity_VehicleFxReset, "rx_race", 0) { ActFpuKeep k(m); rx_activity_vehicle_fx_reset(m.r[7]); return m.ret(0); }
RX_PORT(0x0058fde0, EventProxy_Call4, "rx_race", 0) { ActFpuKeep k(m); rx_event_proxy_call4(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00569d10, Race_ScriptsEnabled, "rx_race", OUT_EAX) { m.r[0] = rx_race_scripts_enabled(m.arg(0)); return m.ret(4); }
RX_PORT(0x00569e10, Race_StartScript, "rx_race", 0) { ActFpuKeep k(m); rx_race_start_script(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00569eb0, Race_Holeshot, "rx_race", 0) { ActFpuKeep k(m); rx_race_holeshot(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00569f40, Race_Trigger, "rx_race", 0) { ActFpuKeep k(m); rx_race_trigger(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0055e8b0, Activity_VehicleEntered, "rx_race", 0) { ActFpuKeep k(m); rx_activity_vehicle_entered(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00559190, Activity_Countdown, "rx_race", 0) { ActFpuKeep k(m); rx_activity_countdown(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00558800, Activity_ExitState, "rx_race", 0) { ActFpuKeep k(m); rx_activity_exit_state(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0055a410, Activity_PlayersRacing, "rx_race", 0) { rx_activity_players_racing(m.r[1]); return m.ret(0); }
RX_PORT(0x00558f70, Activity_CountdownEnter, "rx_race", 0) { ActFpuKeep k(m); rx_activity_countdown_enter(m.r[1]); return m.ret(4); }
RX_PORT(0x005593e0, Activity_InProgressEnter, "rx_race", 0) { ActFpuKeep k(m); rx_activity_in_progress_enter(m.r[1]); return m.ret(4); }
RX_PORT(0x005595b0, Activity_InProgressExit, "rx_race", 0) { ActFpuKeep k(m); rx_activity_in_progress_exit(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00559020, Activity_CountdownExit, "rx_race", 0) { ActFpuKeep k(m); rx_activity_countdown_exit(m.r[1]); return m.ret(4); }
RX_PORT(0x0055e2b0, Activity_FinishSequences, "rx_race", 0) { ActFpuKeep k(m); rx_activity_finish_sequences(m.arg(0)); return m.ret(4); }
RX_PORT(0x00550350, Player_CameraFlash, "rx_race", 0) { rx_player_camera_flash(m.r[1]); return m.ret(0); }
RX_PORT(0x0055ea60, Cameras_Fade, "rx_race", 0) { rx_cameras_fade(m.r[1]); return m.ret(0); }
RX_PORT(0x0055d240, LocalVehicles_Vf49, "rx_race", 0) { ActFpuKeep k(m); rx_local_vehicles_vf49(); return m.ret(0); }
RX_PORT(0x00559e20, Activity_CountHumansOf, "rx_race", OUT_EAX) { m.r[0] = rx_activity_count_humans_of(m.r[2], m.r[7]); return m.ret(0); }
RX_PORT(0x00559df0, Activity_CountKind, "rx_race", OUT_EAX) { m.r[0] = rx_activity_count_kind(m.r[2], m.r[6]); return m.ret(0); }
RX_PORT(0x005544b0, UiSettings, "rx_race", OUT_EAX) { m.r[0] = rx_ui_settings(); return m.ret(0); }
RX_PORT(0x00554d30, UiSettings_Get, "rx_race", OUT_EAX) { m.r[0] = rx_ui_settings_get(m.r[1], m.r[0], m.r[6]); return m.ret(0); }
RX_PORT(0x00551070, Player_HasGhost, "rx_race", OUT_EAX) { m.r[0] = rx_player_has_ghost(m.r[1]); return m.ret(0); }
RX_PORT(0x005060e0, Alloc_3c, "rx_race", OUT_EAX) { m.r[0] = rx_alloc_0x3c(); return m.ret(0); }
RX_PORT(0x00506130, Alloc_20, "rx_race", OUT_EAX) { m.r[0] = rx_alloc_0x20(); return m.ret(0); }
RX_PORT(0x00503440, TreeNext_39, "rx_race", 0) { rx_tree_next_39(m.r[6]); return m.ret(0); }
RX_PORT(0x00503750, TreeNext_1d, "rx_race", 0) { rx_tree_next_1d(m.r[6]); return m.ret(0); }
RX_PORT(0x00560290, FreeRide_PreExit, "rx_race", 0) { ActFpuKeep k(m); rx_free_ride_pre_exit(m.r[1]); return m.ret(4); }
RX_PORT(0x005609b0, FreeRide_Countdown, "rx_race", 0) { ActFpuKeep k(m); rx_free_ride_countdown(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0057a4a0, Activity_CountdownExitThunk, "rx_race", 0) { ActFpuKeep k(m); rx_activity_countdown_exit(m.r[1]); return m.ret(4); }
RX_PORT(0x00550650, Player_Respawn, "rx_race", 0) { ActFpuKeep k(m); rx_player_respawn(m.r[6]); return m.ret(0); }
RX_PORT(0x00504ee0, MapNode_IntString, "rx_race", OUT_EAX) { m.r[0] = rx_map_node_int_string(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x005053b0, MapNode_16, "rx_race", OUT_EAX) { m.r[0] = rx_map_node_16(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
// std::_Tree rotations (ECX = node x; stack tree; RET 4)
RX_PORT(0x00500480, Tree_RotateLeftIntString, "rx_race", 0) { rx_tree_rotate_left_int_string(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x004ffe80, Tree_RotateRightIntString, "rx_race", 0) { rx_tree_rotate_right_int_string(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00765c90, Tree_RotateLeft16, "rx_race", 0) { rx_tree_rotate_left_16(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00765ce0, Tree_RotateRight16, "rx_race", 0) { rx_tree_rotate_right_16(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00502e30, Tree_InsertIntString, "rx_race", OUT_EAX) { rx_tree_insert_int_string(m.r[0], m.r[7], m.arg(0), m.arg(1), m.arg(2)); m.r[0] = m.arg(0); return m.ret(0xc); }
RX_PORT(0x005035c0, Tree_Insert16, "rx_race", OUT_EAX) { rx_tree_insert_16(m.r[0], m.r[7], m.arg(0), m.arg(1), m.arg(2)); m.r[0] = m.arg(0); return m.ret(0xc); }
RX_PORT(0x00501a30, Vector_InsertOne, "rx_race", 0) { rx_vector_insert_one(m.arg(0), m.arg(1), m.arg(3)); return m.ret(0x10); }
RX_PORT(0x0055e360, Activity_SetupObject, "rx_race", 0) { ActFpuKeep k(m); rx_activity_setup_object(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0055dac0, Activity_MakeLocator, "rx_race", 0) { ActFpuKeep k(m); rx_activity_make_locator(m.r[1], m.arg(0)); return m.ret(8); }
RX_PORT(0x0055dff0, Activity_ShowObject, "rx_race", 0) { ActFpuKeep k(m); rx_activity_show_object(m.r[1], m.arg(0)); return m.ret(8); }
RX_PORT(0x0055de70, Activity_PlaceAtLocator, "rx_race", 0) { ActFpuKeep k(m); rx_activity_place_at_locator(m.r[1], m.arg(0)); return m.ret(8); }
RX_PORT(0x0055d4e0, Activity_PlaceEntity, "rx_race", 0) { ActFpuKeep k(m); rx_activity_place_entity(m.r[1], m.arg(0)); return m.ret(8); }
RX_PORT(0x00568620, RaceStub_InProgressEnter, "rx_race", 0) { ActFpuKeep k(m); rx_race_stub_in_progress_enter(m.r[1], m.arg(0)); return m.ret(4); }
// 00550cc0 (ESI = player): the respawn of 00550650 with +0x1170 = 9 (a "ResetPlayer" trigger)
RX_PORT(0x00550cc0, Player_RespawnTrigger, "rx_race", 0) { ActFpuKeep k(m); rx_player_respawn_trigger(m.r[6]); return m.ret(0); }
