// The MotoSkills tutorials (WP-T; _re/notes/tutorials.md, _re/names/98_tutorials.tsv): the game's own tutorial flow, booted and run.
//  * The tutorials are ridden in the front end's own 3D world, the UI world (G+0x20c8: MXUIWorld over UI_Compound + its
//    "UI_CompoundFreeRide" section), not in a level of their own: MainMenu:OnTutorialKeypress (MainMenu.lua) equips an MX Lite
//    (App.ChangeVehicle( row, 2 )), App.EnableTutorialMode( TRUE ) (MXUIManager_EnableTutorialMode 00599820: TutorialManager on,
//    the local players active) and sends GotoLoadingPage -> TransitionPageGraph "Tutorial Load Page" (MXUIPlayersLoadingPage:
//    Game_SetupActivityArea makes the Free Ride activity and the player's vehicle in the UI world, then RoutedGotoInGameHUB) ->
//    InGamePageGraph "Freeride Single Player Page" + the hub's PendingHUBPopup "ShowTutorialIntroPopup". From there the game's
//    TutorialManager ([[G+0x20c8]+0x22180], update 007500c0 from MXUIWorld) and the page Lua run the six lessons by themselves:
//    TutorialIntroductionPopup -> TutorialPauseMenu (Tutorial Pause Menu Page) -> PreTutorialPopup (Tutorial<n>.bik) -> the lesson
//    (TutorialActivation HUD hints, the lesson's Tut<n> scene objects) -> PostTutorialPopup / FailedTutorialPopup -> the pause menu
//    again, TutorialConclusionPopup after the sixth.
//  * start(): the boot path (Boot::start_event with EventSpec type "Tutorial"): the front end's Main Menu Page, then
//    MainMenu:OnTutorialKeypress in the game's own Lua, until the Freeride page is up (the caller writes the world image there).
//  * world_command(): what the client needs from a tutorial world between frames.
#pragma once
#include "boot.h"
#include <cstdint>
#include <string>

namespace reflex_vm {
class Machine;
namespace tutorial {
// The boot path. e.activity: EnsureLocalPlayers' activity (8, Free Ride: the tutorial's). False with the reason in *log.
bool start(Boot& b, const Boot::EventSpec& e, uint32_t max_frames, std::string* log);
// "attach"     once per world, before the client shows anything (between frames): the live UI mirror's hooks (modes.h ui_attach: the UI
//              is in game already, nothing is navigated); the localized voice (pages.h voice) with UILocSound_Play (0068dd40) recorded and
//              not run (the request's package load would need the database loader: its SetEvent from the manager's update 0068dea0 stops
//              the world) and StopTutorialAudio's stop (0080c880 cdecl(owner)) as a "stop <owner>" voice line; the boot's COM resources'
//              AddRef / Release (pages "com_stubs": PreTutorialPopup's SetMaterial); Bink as the boot has it (BinkOpen 0: the client plays
//              the "Tutorial Video" component's file); the vehicle part of MainMenu:OnTutorialKeypress again on the player's tables (the
//              client's persistence attach loaded the save after the boot ran it on the default profile): UI TutorialOwnerVehicleId /
//              TutorialVehicleOwner = PlayerRider.LastEquippedVehicle / Player1, ChangeVehicle( row, 2 ). When the
//              TutorialIntroductionPopup is up (the image is written with it: its Start ran in the boot) its Start's voice request is
//              queued for the client. Answer: "attached[ intro] vehicle <before> <after>".
// "state"      {"mode","manager","off","tutorial","state","step","success","current","video":{"object","path","state"},"music"}: UI TutorialMode
//              (ui+0x298), the TutorialManager's +0 (1: not running), +4 lesson 0..5, +8 (0 idle, 1 waiting for the start, 2 running),
//              +0x7c the lesson's step, +0x64 passed; UI CurrentTutorial; the shown PreTutorialPopup's "Tutorial Video" UIVideoComponent
//              (path +0x270, state +0x2ac: 3 playing); the MusicPlayer's mode (G+0x15c6c +8: 1 stopped, 2 jukebox, 3 UI, 4 scenario).
// "progress"   "<TutorialsCompleted> <TutorialsEntered>" of PlayerRider (the exclusive device's), read by the game's own Lua.
// "frame"      once per world frame, between frames: the saved-game worker's pass for a save the page Lua requested (after 60 frames,
//              the worker's 1000 ms period: "Game Save", the write through the Steam Cloud stand-in, "Game Saved"; answer "Saved").
// "objects"    the lessons' scene objects (TUT0<n>_*: road flares, chevron walls, trajectory arrows) as TutorialManager_ShowLessonObjects (00751c40) shows
//              them for the running lesson: "<entity id> <shown +0xa2> <name>" lines (the client draws the level's instances by id).
// "test_pass"  tests only: the running lesson's own outcome as its check reports a pass (+0x64 = 1, 007520a0).
// Leaving: TutorialPauseMenu QUIT (MXUIManager::SwitchToMainMenu 0059b370) and TutorialConclusionPopup (SendHubRoutingEvent
// GotoFrontEndMainMenuPage) take the game back to its Main Menu Page in the same world (the UI world is the front end's: nothing is
// unloaded); the client sees that page and goes back to its own front end.
std::string world_command(Machine& m, const std::string& cmd, std::string* error);
// w-tutorials (the open core's oracle): a scripted lesson driver for the boot's frames after start(). REFLEX_TUT_SCRIPT=<file>, one
// command per line "<frame> <verb> <rest>" (frames counted from the first frame after the boot; '#' comments):
//   lua <chunk>          a chunk in the UI scripts' Lua state (App.EnableTutorial( 0 ), App.SwitchToHud( ) ...)
//   keys <dik,dik,...>   these DirectInput keys held from this frame on (an empty list releases them)
//   tap <dik> [frames]   the key held for <frames> (default 3) frames from this frame
//   state                the tutorial's state line (world_command "state") and the top popup
//   objects              the lessons' scene objects as shown
// script_frame(b, frame) runs the lines of that frame (before the frame runs) and sets the keys.
void script_load(const std::string& path);
void script_frame(Boot& b, uint32_t frame);
}  // namespace tutorial
}  // namespace reflex_vm
