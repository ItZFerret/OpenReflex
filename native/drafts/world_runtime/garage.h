// The garage (WP-G; garage.cpp, _re/notes/garage.md, _re/names/95_garage.tsv): vehicle tuning on the running physics and the test drive,
// through the unchanged game's own code.
//
// Tuning. PlayerVehicleTuning (Engine1/2, Suspension1/2, Tire1/2: the bonus 0..100 over VehicleModel DefaultEngine1/2 ..., Braking /
// Steering 0 / 50 / 100) reaches the physics through VehicleSpec_ApplyTuning 00543130: the vehicle spec's model defaults (spec+8 ->
// +0x1c..+0x38) plus the tuning row of its PlayerVehicleId (spec+0x9c) when its Player List row has a controller (00543020), else the
// bonus of its parts with a TuningAttribute; then, with a vehicle made (spec+0xc4), RavenVehicle_SetTuning 0045f270 (EDI vehicle, ESI the
// spec's eight values, stack the model's): V+0x220..0x230 and S+0x2ef0..0x2efc of the simulation S = [V+0x234], then S's vf35. The game
// runs it when it makes the vehicle (VehicleFactory_CreateVehicle 00542b90, last step) and from App.ApplyVehicleTuning( device )
// (00730920 -> App_ApplyVehicleTuning 00743550: the device's MXPlayer (00552c10) vehicle's spec in G+0x154d8, then the tuning highlight
// 0x27). GarageVehicleTuning:End writes the row and calls it.
//
// Test drive. GarageHUB:OnStartPressed: MXGarage:Unload, App.EnableTestDrive( TRUE ) (00729d70 -> MXUIManager_EnableTestDrive 00599550:
// ui+0x290, UI TestDriveEnabled, FUN_005992f0( ui, 0 ): ui+0x284 = 0, the local players active), App.SyncDatabaseWithManagers
// (00736180 = Game_SetupActivityArea 0075c430), "GotoLoadingPage" -> TransitionPageGraph "Test Drive Loading Page", whose
// TestDriveTransitionPage:Present calls mxGarage:TestDriveStart( ) (binding 0073c7b0 -> MXGarage_TestDriveStart 00762a20, EAX = the
// garage G+0x15570: the garage vehicle hidden, the Player List's local players active with activity 8 (FreeRide, 005a3590), then
// Game_SetupActivityArea) and the page sets the activity area up until the local slot has its vehicle and rider; GotoInGameHUB. The
// front end's own world is the level: UI_Compound (MXRegistry UISceneName, loaded at start-up), MXActivityFreeRide 0x94329c, game state 2.
// The pause request in a test drive (MXUIManager_OnPauseRequest 00598e40, ui+0x290 set) ends it instead of pausing: ui+0x294 = 1,
// EnableTestDrive( FALSE ) (hub route "GotoGarageVehiclePage" -> "Test Drive Exit Page", TestDriveEnd 00762cc0 -> the Garage Page).
#pragma once
#include "boot.h"
#include <cstdint>
#include <string>

namespace reflex_vm {
class Machine;
class Memory;
namespace garage {
constexpr uint32_t GARAGE = 0x15570;   // MXGarage = G + 0x15570 (App.GetMXGarage 007373f0)
// The test drive's world, booted with the profile signed in with storage (BootConfig::remote_storage, as the career): the front end's
// state (PlayerMode SinglePlayer, InCareerMode, the exclusive device 0, the Player List row of controller 0; e.vehicle_owner / vehicle_id
// equip a vehicle as MXTableHelper:EquipVehicle, else the profile's LastEquippedVehicle), then GarageHUB:OnStartPressed and the Test Drive
// Loading Page as above, then the UI where GotoInGameHUB leaves it (modes.h ui_activate). Returns false when the rider did not spawn.
bool start_test_drive(Boot& b, const Boot::EventSpec& e, uint32_t max_frames, std::string* log);
// The pause request's end of a test drive (w-testride): ui+0x294 = 1 and EnableTestDrive( FALSE ) as MXUIManager_OnPauseRequest does;
// with REFLEX_TEST_DRIVE_END_CALL also MXGarage_TestDriveEnd (the Test Drive Exit Page's call) at once.
void end_test_drive(Boot& b, std::string* log);
// Between frames of a world (reflex_world_garage):
//   "lua <source>"             the source in the game's UI Lua state (as career's front-end "lua"; GarageVehicleTuning:End's table writes)
//   "apply_tuning <device>"    App.ApplyVehicleTuning( device ) (00743550, EAX device)
//   "tuning <slot>"            JSON: the slot's vehicle spec values (+0xa0..+0xbc), the model's (spec+8 -> +0x1c..+0x38), V+0x220..0x230,
//                              S+0x2ef0..0x2efc
std::string world_command(Machine& m, const std::string& cmd, std::string* error);
// reflex_world_camera's camera in a world whose game never copies its viewport camera: in game state 2 (the test drive in the front end's
// world) [0xa7b1d8] stays 0 (no ForestSystem in UI_Compound) and the static Camera 0xd6f620 keeps its constructed state (position 0, forward
// +z): the local player's current camera entity P+0x9a8 (game-camera.md; its Camera at +0x90: the chase camera the frame is drawn with).
// Any other world: `cam` as given.
uint32_t camera_of(Memory& m, uint32_t cam);
}  // namespace garage
}  // namespace reflex_vm
