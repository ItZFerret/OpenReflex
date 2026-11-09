// build_activity_freeride_run.cpp -- oracle adapters for core/game/build_activity_freeride_run.c (group rx_build11; the free ride activity's methods run in
// the baton boot and in the FreeRide worlds).
#include "oracle.h"
extern "C" {
#include "../game/build_activity_freeride_run.h"
}
using namespace reflex_port;

// 007d2820 (stdcall scene, type, out, capacity, name, overflow; RET 0x18) -> EAX
RX_PORT(0x007d2820, SceneGroupQuery, "rx_build11", OUT_EAX) { m.r[0] = rx_scene_group_query(m.arg(0), m.arg(1), m.arg(2), (int32_t)m.arg(3), m.arg(4), m.arg(5)); return m.ret(0x18); }
// 005555a0 (cdecl name) -> EAX
RX_PORT(0x005555a0, EventTypeIndex, "rx_build11", OUT_EAX) { m.r[0] = rx_event_type_index(m.arg(0)); return m.ret(0); }
// 005605e0 (stdcall activity; RET 4)
RX_PORT(0x005605e0, FreeRideTriggersRefresh, "rx_build11", 0) { rx_free_ride_triggers_refresh(m.arg(0)); return m.ret(4); }
// 00560110 MXActivityFreeRide::vf5 (thiscall)
RX_PORT(0x00560110, FreeRide_vf5, "rx_build11", 0) { rx_free_ride_vf5(m.r[1]); return m.ret(0); }
// 00560070 MXActivityFreeRide::vf4 (thiscall): a tail jump to 00591f20 with EAX = the channel
RX_PORT(0x00560070, FreeRide_vf4, "rx_build11", 0) { rx_free_ride_vf4(m.r[1]); return m.ret(0); }
// 0055ff70 MXActivityFreeRide::vf2 (thiscall arg; RET 4)
RX_PORT(0x0055ff70, FreeRide_vf2, "rx_build11", 0) { rx_free_ride_vf2(m.r[1], m.arg(0)); return m.ret(4); }
// 0055ffd0 (ESI = the activity)
RX_PORT(0x0055ffd0, FreeRideFlagMarker, "rx_build11", 0) { rx_free_ride_flag_marker(m.r[6]); return m.ret(0); }
// 00584450 (thiscall activity; RET 4): jmp ActivityAudioLogic::vf29
RX_PORT(0x00584450, AudioLogicVf29Shared, "rx_build11", 0) { rx_audio_logic_vf29_shared(m.r[1], m.arg(0)); return m.ret(4); }
// 00582330 FreeRideActivityAudioLogic::vf3 (thiscall)
RX_PORT(0x00582330, FreeRideAudioLogic_vf3, "rx_build11", 0) { rx_free_ride_audio_logic_vf3(m.r[1]); return m.ret(0); }
// 00560cd0 (stdcall activity; RET 4)
RX_PORT(0x00560cd0, FreeRideStartSlots, "rx_build11", 0) { rx_free_ride_start_slots(m.arg(0)); return m.ret(4); }
// 00560220 MXActivityFreeRide::vf44 (thiscall player; RET 4) -> EAX = the driver
RX_PORT(0x00560220, FreeRide_vf44, "rx_build11", OUT_EAX) { m.r[0] = rx_free_ride_vf44(m.r[1], m.arg(0)); return m.ret(4); }
// 00560c20 MXActivityFreeRide::vf47 (thiscall driver; RET 4)
RX_PORT(0x00560c20, FreeRide_vf47, "rx_build11", 0) { rx_free_ride_vf47(m.r[1], m.arg(0)); return m.ret(4); }
// 0050bce0 FreeRideDriver::vf1 (thiscall): two handlers on the activity channel, then NetworkDriver::vf1
RX_PORT(0x0050bce0, FreeRideDriver_vf1, "rx_build11", 0) { rx_free_ride_driver_vf1(m.r[1]); return m.ret(0); }
// 00561620 MXActivityFreeRide::vf52 (thiscall). Void.
RX_PORT(0x00561620, FreeRide_vf52, "rx_build11", 0) { rx_free_ride_vf52(m.r[1]); return m.ret(0); }
// 00557cd0 Activity::vf17 (thiscall). Void.
RX_PORT(0x00557cd0, Activity_vf17, "rx_build11", 0) { rx_activity_vf17(m.r[1]); return m.ret(0); }
// 00557a70 Activity::vf15 (thiscall). Void.
RX_PORT(0x00557a70, Activity_vf15, "rx_build11", 0) { rx_activity_vf15(m.r[1]); return m.ret(0); }
// 0050abe0 (ESI = driver). Void.
RX_PORT(0x0050abe0, DriverResetVehicle, "rx_build11", 0) { rx_driver_reset_vehicle(m.r[6]); return m.ret(0); }
using rx_oracle::f32;
// 00767df0 (EAX = world; stdcall float; RET 4). Void.
RX_PORT(0x00767df0, WorldSetTerrainWetness, "rx_build11", 0) { rx_world_set_terrain_wetness(m.r[0], f32(m.arg(0))); return m.ret(4); }
// 00409380 / 00409470 (void)
RX_PORT(0x00409380, MudRecordsClearAmounts, "rx_build11", 0) { rx_mud_records_clear_amounts(); return m.ret(0); }
RX_PORT(0x00409470, MudRecordsClearWetness, "rx_build11", 0) { rx_mud_records_clear_wetness(); return m.ret(0); }
// 00557de0 Activity::vf18 (thiscall) / 00561600 MXActivityFreeRide::vf18 (thiscall). Void.
RX_PORT(0x00557de0, Activity_vf18, "rx_build11", 0) { rx_activity_vf18(m.r[1]); return m.ret(0); }
RX_PORT(0x00561600, FreeRide_vf18, "rx_build11", 0) { rx_free_ride_vf18(m.r[1]); return m.ret(0); }
// 00764710 MXRecords_Reset (ECX = records, EAX = slot). Void.
RX_PORT(0x00764710, RecordsReset, "rx_build11", 0) { rx_records_reset(m.r[1], m.r[0]); return m.ret(0); }
// 00582100 ActivityAudioLogic::vf25 (thiscall; two unused arguments; RET 8). Void.
RX_PORT(0x00582100, AudioLogic_vf25, "rx_build11", 0) { rx_audio_logic_vf25(m.r[1]); return m.ret(8); }
// 00582120 ActivityAudioLogic::vf26 (thiscall; two unused arguments; RET 8). Void.
RX_PORT(0x00582120, AudioLogic_vf26, "rx_build11", 0) { rx_audio_logic_vf26(m.r[1]); return m.ret(8); }
// 0075c530 (-> EAX)
RX_PORT(0x0075c530, GameCurrentWorld, "rx_build11", OUT_EAX) { m.r[0] = rx_game_current_world(); return m.ret(0); }
// 0055e560 (stdcall activity; RET 4) -> EAX
RX_PORT(0x0055e560, ActivityMusicEntity, "rx_build11", OUT_EAX) { m.r[0] = rx_activity_music_entity(m.arg(0)); return m.ret(4); }
// 005821a0 ActivityAudioLogic::vf35 (thiscall). Void.
RX_PORT(0x005821a0, AudioLogic_vf35, "rx_build11", 0) { rx_audio_logic_vf35(m.r[1]); return m.ret(0); }
// 0055e620 (stdcall activity; RET 4) -> EAX
RX_PORT(0x0055e620, ActivityMusicEntity2, "rx_build11", OUT_EAX) { m.r[0] = rx_activity_music_entity2(m.arg(0)); return m.ret(4); }
// 00581f50 / 005821e0 ActivityAudioLogic::vf15 / vf36 (thiscall). Void.
RX_PORT(0x00581f50, AudioLogic_vf15, "rx_build11", 0) { rx_audio_logic_vf15(m.r[1]); return m.ret(8); }
RX_PORT(0x005821e0, AudioLogic_vf36, "rx_build11", 0) { rx_audio_logic_vf36(m.r[1]); return m.ret(0); }
// 00582220 ActivityAudioLogic::vf37 (thiscall). Void.
RX_PORT(0x00582220, AudioLogic_vf37, "rx_build11", 0) { rx_audio_logic_vf37(m.r[1]); return m.ret(0); }
// 004dd3d0 (stdcall slot; RET 4) -> EAX
RX_PORT(0x004dd3d0, PlayerControllerPresent, "rx_build11", OUT_EAX) { m.r[0] = rx_player_controller_present(m.arg(0)); return m.ret(4); }
// 00756040 (EAX = the name, ESI = the config). Void.
RX_PORT(0x00756040, LevelConfigSetName, "rx_build11", 0) { rx_level_config_set_name(m.r[6], m.r[0]); return m.ret(0); }
// 00753200 / 00753250 (EAX = the save-game object; EAX stays). Void.
RX_PORT(0x00753200, RecordsSlotsStart, "rx_build11", 0) { rx_records_slots_start(m.r[0]); return m.ret(0); }
RX_PORT(0x00753250, RecordsSlotsAttempt, "rx_build11", 0) { rx_records_slots_attempt(m.r[0]); return m.ret(0); }
// 00753760 (thiscall activity; RET 4). EAX = the world when only the first test passed, EDX = the criteria vector's end when it was destroyed (live: eax, edx, x87)
RX_PORT(0x00753760, RecordsSetActivity, "rx_chain", OUT_EAX | OUT_EDX | OUT_FPU) {
    rx_records_set_activity(m.r[1], m.arg(0));
    uint32_t v;
    uint32_t eax = rx_records_set_activity_regs(0, &v); if (v) m.r[0] = eax;
    uint32_t edx = rx_records_set_activity_regs(1, &v); if (v) m.r[2] = edx;
    return m.ret(4);
}
// 004e7780 (stdcall type; RET 4) -> EAX
RX_PORT(0x004e7780, EventTypeIdOf, "rx_build11", OUT_EAX) { m.r[0] = rx_event_type_id_of(m.arg(0)); return m.ret(4); }
// 00402540 (EAX = the session) -> EAX
RX_PORT(0x00402540, SessionIsNetworked, "rx_build11", OUT_EAX) { m.r[0] = rx_session_is_networked(m.r[0]); return m.ret(0); }
// 00766930 (stdcall world; RET 4). Void.
RX_PORT(0x00766930, WorldPlayersLeave, "rx_build11", 0) { rx_world_players_leave(m.arg(0)); return m.ret(4); }
// 005d86e0 (thiscall). Void.
RX_PORT(0x005d86e0, SharedClear5c, "rx_build11", 0) { rx_shared_clear_5c(m.r[1]); return m.ret(0); }
// 0055f630 (EAX = the manager, EDI = the kind; stack the area; RET 4) -> EAX
RX_PORT(0x0055f630, ActivityFindByKindArea, "rx_build11", OUT_EAX) { m.r[0] = rx_activity_find_by_kind_area(m.r[0], m.r[7], m.arg(0)); return m.ret(4); }
