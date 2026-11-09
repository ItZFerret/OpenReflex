// Persistence of arcade races, highlights and Steam achievements through the unchanged game's own code (persist.cpp;
// _re/notes/persistence.md, _re/names/86_persist.tsv). WP-7 of _re/notes/port-audit.md.
//
// Arcade worlds are booted without the save (default world images stay byte-identical and cached). When the world runtime starts
// one, attach() loads the player's savegame.bxml into the running game the way the game itself loads it (MXProfile_SignIn with a
// PCStorageDirectory "SaveGame", SavedGameManager_ReadSaveGame, the main thread's ApplyLoaded step 005a6620) and joins the race's
// records again (the activity's driver-joined call 00764170, which made the PlayerRecords working row from the boot's empty table).
// The race then updates PlayerRecords / PlayerHighlight as in the original; leave() runs what the game runs when the race ends:
// PostEventOverlay:OnPostStateComplete's App.RequestSaveGame, the activity teardown's records commit (PlayerLeft 0055c1d0 ->
// 00764560) and the front end's next save, each written by the saved-game worker's write (SavedGameManager_WriteSaveGame) into the
// Steam Cloud stand-in (career.h install_remote_storage).
//
// Achievements: MXHighlightTracking (singleton [00d92edc]) counts the 48 Highlight rows in PlayerHighlight (006d7980) and, on an
// unlock, sets the Steam achievement ACHIEVEMENT_* (006d87d0: GetAchievement / SetAchievement) when its PCOnlineAchievements
// (+4) has received the user's stats (+8, UserStatsReceived handler 006d8750); MXGame::Update stores them (StoreStats) when the
// object's dirty flag (+0) is set. install_user_stats is ISteamUserStats011 over a host file (<save>/stats/UserGameStats_55140.txt).
#pragma once
#include <cstdint>
#include <string>

namespace reflex_vm {
class Machine;
class Boot;

// steam_api.dll!SteamUserStats (STEAMUSERSTATS_INTERFACE_VERSION011, slots 0..42; the game calls 0 RequestCurrentStats,
// 4 SetStat(int), 6 GetAchievement, 7 SetAchievement, 10 StoreStats and the online leaderboards 22 / 28 / 30 / 31) over the folder's
// UserGameStats_55140.txt: the 48 achievements of the game's table 009c3964 (achieved, unlock time) and the stats it sets.
void install_user_stats(Machine& m, const std::string& folder);

namespace persist {
constexpr uint32_t HIGHLIGHT_TRACKING = 0xd92edc;   // MXHighlightTracking* (+4 PCOnlineAchievements, +8 the 48 goals)
constexpr uint32_t ACHIEVEMENT_NAMES = 0x9c3964;    // 48 x 12 bytes: +0 "ACHIEVEMENT_*" (index = HighlightId - 1)
constexpr uint32_t STEAM_APP_ID = 55140;           // steam_appid.txt of the PC build (SteamUtils()->GetAppID)

// What a logged-on Steam client gives the PCOnlineAchievements object at start (006d8780 with BLoggedOn true: +4 = the app id,
// RequestCurrentStats, then the UserStatsReceived_t callback -> 006d8750 sets +8). The port's Steam user is offline for the network
// (the PC sign-in path), so this is delivered here, with the game's own handler. Boot: between frames.
void stats_received(Machine& m);
void stats_received(Boot& b);
// MXGame::Update's step (00757fb0): [[00d92edc] + 4] dirty -> SteamUserStats()->StoreStats(), flag cleared; for calls made between
// frames (no MXGame::Update follows them).
void flush_stats(Machine& m);
// The machine is going away (its world / profile session): its stand-in state is dropped (the file stays).
void forget(const Machine& m);
// The 48 achievements as JSON: per HighlightId the Highlight row (Enum, Goal), PlayerHighlight (GoalProgress, UnlockedOrder) of
// controller 0 and the Steam achievement (name, achieved, unlock time), plus the stats (reads only: boot or world).
std::string achievements_json(Machine& m);
// The activity teardown's records commit for the local human players (PlayerLeft 0055c1d0 -> 00764560); returns how many.
int commit_records(Machine& m);

// World runtime commands, between frames (reflex_world_persist):
//   "attach"        the Steam stand-ins (remote storage, user stats, UserStatsReceived), the profile signed in with storage and the
//                   save loaded into the running game, the race's records joined again. For arcade worlds (booted without a save).
//   "attach_steam"  the Steam stand-ins only (career worlds, booted with the save already loaded).
//   "leave"         the race is left (post-race CONTINUE, pause QUIT, the scene going away): RequestSaveGame + write, the records
//                   commit, the next save + write, StoreStats. JSON: what was saved, the PlayerRecords rows, highlights, achievements.
//   "restart"       the activity restarts (Activity::vf18's records reset 00764710, then the commit and a save: the save carries the
//                   profile into the restarted race's world, which attaches again).
//   "achievements"  achievements_json.
std::string world_command(Machine& m, const std::string& save_dir, const std::string& cmd, std::string* error);
}  // namespace persist
}  // namespace reflex_vm
