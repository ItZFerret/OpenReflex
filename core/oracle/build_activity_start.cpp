// build_activity_start.cpp -- oracle adapters for construction unit 11 (the activity's start, the race's grid and start, the
// records, FreeRide; core/game/build_activity_{start,grid,frame,records}.c, build_freeride*.c; group rx_build11).
// Verified per call inside the boot (REFLEX_PORT=rx_build11 REFLEX_PORT_VERIFY=1 REFLEX_BOOT_SCHED=baton). ABI glue only.
#include "oracle.h"
extern "C" {
#include "../game/build_activity_int.h"
}
using namespace reflex_port;

#define G "rx_build11"
// ---- build_activity_frame.c ----
RX_PORT(0x0084ea00, EngineFrameMark, G, 0) { rx_engine_frame_mark(m.r[7]); return m.ret(0); }
RX_PORT(0x0081f290, RumbleFrame, G, 0) { rx_rumble_frame(); return m.ret(0); }
RX_PORT(0x007d20b0, SceneSetActive, G, 0) { rx_scene_set_active(m.r[0], m.arg(0)); return m.ret(4); }
// ---- build_activity_start.c ----
RX_PORT(0x005585e0, ActivityNameState, G, 0) { rx_activity_name_state(m.arg(0), m.arg(1), m.r[4] + 0xc); return m.ret(0x24); }
RX_PORT(0x00556ec0, ActivityVf2, G, 0) { rx_activity_vf2(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00566170, RaceStubVf2, G, 0) { rx_race_stub_vf2(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00564b20, RaceVf2, G, 0) { rx_race_vf2(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00566310, RaceStubVf4, G, 0) { rx_race_stub_vf4(m.r[1]); return m.ret(0); }
RX_PORT(0x00767ba0, WorldLinkLights, G, 0) { rx_world_link_lights(m.r[6]); return m.ret(0); }
RX_PORT(0x007d29e0, SceneCollectPrefixed, G, OUT_EAX) { m.r[0] = rx_scene_collect_prefixed(m.r[0], m.arg(0), m.arg(1), (int32_t)m.arg(2), m.arg(3)); return m.ret(0x10); }
// ---- build_activity_grid.c ----
RX_PORT(0x00538bb0, TrackSegmentContains, G, OUT_EAX) { m.r[0] = rx_track_quad_contains(m.r[1], m.r[0], m.r[7]); return m.ret(0); }
RX_PORT(0x005385f0, TrackNearestNode, G, 0) {
    rx_track_nearest_in_radius(m.r[0], m.arg(0), m.arg(1), rx_oracle::f32(m.arg(2)), m.arg(3), m.arg(4)); return m.ret(0x14);
}
RX_PORT(0x005666d0, RaceGridFromGate, G, 0) { rx_race_grid_from_gate(m.arg(0)); return m.ret(4); }
// ---- build_activity_audio.c ----
RX_PORT(0x00580e70, AudioLogicVf29, G, 0) { rx_audio_logic_vf29(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00582ed0, RaceAudioLogicVf29, G, 0) { rx_race_audio_logic_vf29(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00582610, RaceAudioLogicVf3, G, 0) { rx_race_audio_logic_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x004df6a0, StartupTableValue, G, OUT_EAX) { m.r[0] = rx_startup_table_value(m.r[0]); return m.ret(0); }
RX_PORT(0x0075d760, GameUpdateLoadWait, G, OUT_EAX) { m.r[0] = rx_game_update_load_wait(m.arg(0), rx_oracle::f32(m.arg(1))); return m.ret(8); }
RX_PORT(0x0075f500, GameCanBeginWorldLoad, G, OUT_EAX) { m.r[0] = rx_game_can_begin_world_load(m.arg(0)); return m.ret(4); }
RX_PORT(0x0075f2b0, GamePlayersReady, G, OUT_EAX) { m.r[0] = rx_game_players_ready(m.arg(0)); return m.ret(4); }
RX_PORT(0x0075f890, GameGridPass, G, 0) { rx_game_grid_pass(m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0x10); }
RX_PORT(0x0075e020, GameRaceStart, G, 0) { rx_game_race_start(); return m.ret(0); }
// ---- build_activity_records.c ----
RX_PORT(0x00557fe0, ActivityEntitiesStart, G, 0) { rx_activity_entities_start(m.arg(0)); return m.ret(4); }
RX_PORT(0x0055a4c0, ActivityVf50, G, 0) { rx_activity_vf50(m.r[1]); return m.ret(0); }
RX_PORT(0x00764170, RecordsJoin, G, 0) { rx_records_join(m.r[0], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4)); return m.ret(0x14); }
// ---- build_activity_track.c (the .tdf07 racing-line loader FUN_00536ef0 and its parser) ----
RX_PORT(0x00536ef0, TrackLoad, G, OUT_EAX) { m.r[0] = rx_track_load(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x00536d00, TrackFilePath, G, 0) { rx_track_file_path(m.r[6], m.r[1], m.r[7], m.arg(0)); return m.ret(0); }
RX_PORT(0x00536dc0, TrackAssetName, G, 0) { rx_track_asset_name(m.r[6], m.r[1], m.r[7], m.arg(0)); return m.ret(0); }
RX_PORT(0x00536e80, TrackHasAsset, G, OUT_EAX) { m.r[0] = rx_track_has_asset(m.r[1], m.r[2], m.arg(0)); return m.ret(0); }
RX_PORT(0x00538090, TrackBlockReset, G, 0) { rx_track_block_reset(m.r[0]); return m.ret(0); }
RX_PORT(0x00539550, TrackReadTag, G, OUT_EAX) { m.r[0] = rx_track_read_tag(m.r[0], m.arg(0), m.r[7]); return m.ret(4); }
RX_PORT(0x005395d0, TrackReadVec3, G, 0) { rx_track_read_vec3(m.r[7], m.r[6]); return m.ret(4); }
RX_PORT(0x00539680, TrackReadU32Array, G, 0) { rx_track_read_u32_array(m.r[7], m.arg(0)); return m.ret(4); }
RX_PORT(0x00539720, TrackReadGrid, G, 0) { rx_track_read_grid(m.r[7], m.arg(0)); return m.ret(4); }
RX_PORT(0x00539820, TrackReadRecord30, G, 0) { rx_track_read_record30(m.r[1], m.r[6], m.arg(0)); return m.ret(4); }
// ---- build_activity_freeride.c ----
RX_PORT(0x0055fdb0, FreeRideConstruct, G, OUT_EAX) { m.r[0] = rx_free_ride_construct(m.arg(0), m.r[1], m.r[0]); return m.ret(4); }
RX_PORT(0x005857f0, JumpTrackerInit, G, 0) { rx_free_ride_jump_tracker_init(m.r[0]); return m.ret(0); }
RX_PORT(0x00581030, AudioLogicVf32, G, 0) { rx_audio_logic_vf32(m.r[1]); return m.ret(0); }
// ---- build_activity_controller.c (the player's controller key maps) ----
RX_PORT(0x00547f30, ControllerBuildKeyMaps, G, OUT_EAX | OUT_ECX | OUT_EDX) {
    const uint32_t controller = m.arg(0), preset = m.arg(1), handedness = m.arg(2), motion = m.arg(3);
    m.r[0] = rx_controller_build_key_maps(controller, preset, handedness, motion);
    m.r[1] = motion;       /* ECX at the original's RET */
    m.r[2] = handedness;   /* EDX at the original's RET */
    return m.ret(0);
}

// ---- build_activity_spawn.c (the player slot's vehicle / rider spawn) ----
RX_PORT(0x00767200, PlayerSlotSpawnStep, G, OUT_EAX) { m.r[0] = rx_player_slot_spawn_step(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0054a500, PlayerVehicleClear, G, 0) { rx_player_vehicle_clear(m.arg(0)); return m.ret(4); }
RX_PORT(0x00549f30, PlayerSpawnSpec, G, 0) { rx_player_spawn_spec(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0054a050, PlayerVehicleSpawn, G, OUT_EAX) { m.r[0] = rx_player_vehicle_spawn(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0054a320, PlayerRiderSpawn, G, OUT_EAX) { m.r[0] = rx_player_rider_spawn(m.r[0], m.r[1]); return m.ret(0); }
RX_PORT(0x0054a3f0, PlayerRiderAttach, G, 0) { rx_player_rider_attach(m.r[7], m.arg(0)); return m.ret(4); }
RX_PORT(0x0053fa30, PartPackagesReady, G, OUT_EAX) { m.r[0] = rx_part_packages_ready(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00542ac0, VehicleSpecReady, G, OUT_EAX) { m.r[0] = rx_vehicle_spec_ready(m.r[1], m.r[0]); return m.ret(0); }
RX_PORT(0x0053cea0, RiderEntryPartsReady, G, OUT_EAX) { m.r[0] = rx_rider_entry_parts_ready(m.arg(0)); return m.ret(4); }
RX_PORT(0x0053e880, RiderEntryReady, G, OUT_EAX) { m.r[0] = rx_rider_entry_ready(m.r[1], m.r[7]); return m.ret(0); }
