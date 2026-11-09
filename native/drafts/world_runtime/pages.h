// The in-race pages (WP-5, pages.cpp; _re/notes/in-race-pages.md, _re/names/90_pages.tsv): what the port's pause menu, gate selection
// and the game's localized voice-over need from a running world, read from the game itself between frames.
//  * Localized UI voice: the page Lua's App.PlayUILocalizedSoundEvent / StopUILocalizedSoundEvent (RulesPopup:Start, AAANotification,
//    the challenge popups ...) reach the localized sound manager [0xd94ad4] (UILocSound_Play 0068dd40, UILocSound_Stop 0068de20), whose
//    requests the world never services (no database loader); the hooks record them for the client, which plays the event itself.
//  * World commands: the game's UI Lua run in the world (the pause page's row conditions are PauseMenu:Start's own), UI variables, the
//    series' gate selection.
#pragma once
#include <cstdint>
#include <string>

namespace reflex_vm {
class Machine;
namespace pages {
// The localized UI sound requests since the last call, one per line: "play <database>|<package>|<event path>|<owner>" and
// "stop <owner>" (owner "" = all). The first call installs the hooks (a world that never asks is unchanged). Between frames.
std::string voice(Machine& m);
// "lua <source>"            the source in the game's UI Lua state; the answer is what it passes to App.DebugPrint( "REFLEXANSWER:" .. s )
//                           (hooked while the command runs; any length)
// "state"                   {race, rulesHold, allowPause, post (MXActivityRace's post sub-state, -1 other classes), skip, vtable}
// "skip_scenario"           App.SkipToNextScenario ([0xa7f748] = 1)
// "hold <v>"                the activity's rules hold A+0xff068 = v (the answer: the value before)
// "gate_camera <gate>"      GateSelection_SetCameraToGate( gate, 25, 7, 0, 0 ): the UI_StartGate_Camera matrix (16 floats, row vectors)
// "ui_images"               hooks SetMaterial / SetIconId of the page Lua; "<object>|<material>" / "<object>|#<icon>" lines since the last call
// "com_stubs"               AddRef / Release stubs for the boot's COM resources (UI material clones); the count installed
// "lua_ui <key> <source>"   the source in the game's UI Lua state (wrapper 00d94938: FUN_00744920 loads the chunk, FUN_00744e70 calls
//                           it), then the UI variable <key> (UIProperty_GetString 005dc060; a UI string keeps 31 characters) as the answer; the variable and the
//                           chunks' globals are removed after.
// "ui <key>" / "ui_int <key>"  a UI variable (UIProperty_GetString 005dc060 / UIProperty_GetInt 005dc0c0).
// "gates"                   the series' gate selection as GateSelection:Start / OnGateSelectionDeviceIndexChanged run it, without the
//                           pick: {"status","device","available":[16 x 0/1],"first"} (ProcessGateSelection 005a9a30 when the series
//                           event waits in "GateSelection"; IsSeriesGateAvailable 0073ecc0).
std::string world_command(Machine& m, const std::string& cmd, std::string* error);
void forget(const Machine* m);
// WP-T (tutorial.cpp): a line of the voice queue from another hook ("stop <owner>": StopTutorialAudio's 0080c880), in order with the others.
void voice_note(Machine& m, const std::string& line);
}  // namespace pages
}  // namespace reflex_vm
