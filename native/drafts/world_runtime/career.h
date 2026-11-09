// MOTOCAREER (career mode) and its persistence, through the unchanged game's own code (career.cpp; _re/notes/career.md,
// _re/names/80_career.tsv).
//
// The PC build saves to Steam Cloud: PCStorageDevice / SteamCloudInputStream / SteamCloudOutputStream (006d4680..006d4fa0) call
// SteamRemoteStorage() for one file, "savegame.bxml" (the profile as BXML, Version 0x12, PlayerDatabaseVersion, PlayerTableVersion
// and every Player* table, followed by the 4-byte trailer 0x27720006). The stand-in below is that interface over a host folder
// (Steam's own layout: <userdata>/<appid>/remote/savegame.bxml), so the file is the game's own save.
//
// Sign-in (the PC path): PCSignInManager::vf2 (006e0f30) signs controller 0 in from SteamUser() / SteamFriends() (offline:
// BLoggedOn false -> NetLoginStatus 1 -> MXProfileManager 00616ed0 -> MXProfile_SignInWithoutStorage 00616740); Title:OnStartPressed
// then requests a storage device (MXProfile_RequestNewStorageDevice 00616560); PCStorage::vf7 makes the PCStorageDevice and sends
// Storage/DeviceAcquired; the profile manager's handler 00617110 queries the directories (PCStorageDevice::vf5: FileExists
// "savegame.bxml"), creates "SaveGame" when there is none and signs in with it (MXProfile_SignIn 006166a0), which queues the load
// (SavedGameManager_RequestLoad 005a6880) for the saved-game worker thread (005a7f10 -> SavedGameManager_Process 005a76c0).
#pragma once
#include "boot.h"
#include <cstdint>
#include <string>
#include <vector>

namespace reflex_vm {
class Machine;
class World;

// ISteamRemoteStorage (STEAMREMOTESTORAGE_INTERFACE_VERSION010, the slots the game calls: 0 FileWrite, 1 FileRead, 10 FileExists,
// 12 GetFileSize, 17 GetQuota; the others answer as an empty cloud) over `folder`, returned by steam_api.dll!SteamRemoteStorage.
void install_remote_storage(Machine& m, const std::string& folder);
// SteamUser() / SteamFriends() of a Steam user who is not logged on to the Steam servers (offline mode): BLoggedOn false,
// GetSteamID, GetPersonaName = `persona`. The sign-in manager then signs controller 0 in as the PC game does.
void install_steam_user(Machine& m, const std::string& persona);

namespace career {
constexpr uint32_t PROFILE_MANAGER = 0xd92ee0;     // MXProfileManager*; profile c = [mgr + 4 + 4c] (0x30 bytes, profile-bootstrap.md)
constexpr uint32_t SERIES_MANAGER = 0x9e2188;      // MXSeriesManager (static; MXSeriesProgress is the same object)
constexpr uint32_t SAVED_GAMES = 0xdce458;         // MXUISavedGameManager (static; 4 controller slots of 0x70 at +0x10)

// The profile of controller 0 signed in with storage and its save loaded (or found missing), as above. The game is at the front
// end (game state 2). Returns false when it did not get there.
bool sign_in(Boot& b, std::string* log);
// The offline Steam user's persona name: <save_dir>/steam_persona.txt (the motocard name the player entered), else "Player".
std::string persona(const std::string& save_dir);
// Starts a career series event as the shipping front end does: MainMenu:CareerMode (PlayerMode SinglePlayer, EnableTourMode,
// InCareerMode), RaceAndRideEvents:OnAcceptSeries (progress:Reset, LoadSeries(device, seriesId, Tour), SetVehicleTypeId),
// StartRacePopup:End (CommitAndInitializeSeries, StartEvent) and "GotoLoadingPage", then the loading as Boot::start_event.
bool start_series_event(Boot& b, const Boot::EventSpec& e, uint32_t max_frames, std::string* log);
// App.RequestSaveGame(0) (SavedGameManager_RequestSave 005a6910) and frames until the saved-game worker wrote the file.
bool save(Boot& b, std::string* log, uint32_t max_frames = 600);
// The signed-in profile as JSON: sign-in state, the Player* tables of controller 0, and the game's own answers the career pages
// ask (MXTableHelper IsLocked / IsHidden / IsStoreProductNew per Series store product, GetPlayerEventStatus, career completion).
std::string profile_json(Machine& m, bool with_logic, Boot* b = nullptr);
// A front-end operation on the booted game (see career.cpp for the list); returns false and an error in *log on failure.
bool command(Boot& b, const std::string& cmd, std::string* out);

// After a career race in the world runtime, between frames: what the shipping scripts run once the race's post state is done,
// PostEventOverlay:OnPostStateComplete (MXSeriesManager CompleteSeriesEvent 005b41b0) then CareerFrontEndLoadingPage:DoneLoading
// (CompleteSeries 005b4310, which commits PlayerSeriesStatus and requests the save; EnableTourMode), and the saved-game worker's
// write of that request (SavedGameManager_WriteSaveGame 005a6690 -> SteamCloudOutputStream -> FileWrite "savegame.bxml").
// `remote` is the Steam Cloud folder. Returns JSON (series status, results, save result).
std::string finish_event(Machine& m, const std::string& remote, std::string* error);
// The post-event steps a multi-race series (Heat / Moto) takes between its races, in the world: the results of the race just run
// (returns SeriesEventStatus: "HeatOrLCQ" = another race follows) and the next sub event (HandleAdvanceNextSubEvent).
std::string post_results(Machine& m);
void next_sub_event(Machine& m);
// The outcome of the race a world ran (drivers' order and results; flags bit 0: the rules popup's "don't show again"), for a boot
// that replays the series up to the next race (Boot::EventSpec::series_outcomes).
std::vector<uint32_t> race_outcome(Machine& m, uint32_t flags);
// `count` outcomes concatenated in a file (each {magic, 1, n, flags, order[n], fields[n * 10]}).
bool read_outcomes(const std::string& path, int count, std::vector<std::vector<uint32_t>>& out);
// The player's gate choices (WP-5): <save_dir>/career_gates.bin, int32 per gate selection of the series in order (-1 the default). False
// when there is no such file (no choice made: every gate selection takes the page's default).
bool read_gates(const std::string& path, std::vector<int>& out);
std::string world_ui_string(Machine& m, const char* key);
// The in-race career pages' calls, between frames (reflex_world_career): "status" (JSON: series, format, race state, the activity's
// rules hold and RulesType, SeriesEventStatus, SeriesAllowRestart), "rules_done <dontShowAgain>" (RulesPopup), "results" /
// "moto_results" (PostEventResults / PostEventMotoResults: the game's "Activity Results" table), "outcome <flags>" (race_outcome as hex).
std::string world_command(Machine& m, const std::string& cmd, std::string* error);
// A pending gate selection (Heat / Moto / Single with gates) answered as the GateSelection page does: the AI picks, the local player
// takes gate `choice` when available (else the first available). Returns the gate, -1 when none is pending.
int gate_selection(Machine& m, int choice = -1);   // a UI property of the world (UIProperty_GetString)
}  // namespace career
}  // namespace reflex_vm
