// Game modes beyond the races (modes.cpp; _re/notes/modes.md, _re/names/83_modes.tsv): Freestyle, Section Race and the Free Ride
// challenges, read from the running game.
//  * The live in-game UI. The game builds every page of its page graphs (MXUI uicmpnt assets: InGamePageGraph, PopupPageStack, ...)
//    as UIComponent trees, and its MXUI*Component classes and page Lua update them each frame from the activity: texts, visibility,
//    colours, composite animations, popups (RulesPopup, Pre / PostChallengePopup), the post-event pages. A world booted by Boot
//    keeps the start-up FrontEndIntro graph in front (its Publisher Video page: the boot starts the event without the front end's
//    pages), so the InGame graph never runs; ui_activate puts the UI where the shipping flow has it once the level runs. ui_frame then
//    mirrors what is shown: the current page of every visible graph, its layers and their component trees, for a client that builds
//    the same layers from the install.
//  * The activity (mode_state): its class, state, clock.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace reflex_vm {
class Memory;
class Machine;

// UIComponent layout (UIComponent::vf2 005c6a30 / UIContainer::vf2 005ced00 / UIComponent::vf1 005c6520 / FUN_005c9950; _re/notes/ui.md 3):
//   +0x10 name char[48]; +0x40 Position (pivot point in the parent, set by code too); +0x50 the pivot point on screen; +0x60 scale x/y;
//   +0x80 pivot; +0x88 Size; +0x90 parent; +0x9c / +0xa0 the CompositeAnimation records (count, array of 0x4c: name, +0x34 tracks,
//   +0x38 state 0 playing / 3 stopped, +0x3c time, +0x40 length; FUN_005c5ea0 = SetState, 005c5e20 = advance); +0xb4 state bits
//   (0 Focused, 1 Highlighted, 2 Focusable, 3 Enabled, 4 Visible); +0xc4 UIColorState, current colour +0xd8 (A8R8G8B8);
//   UIContainer children +0xcc / +0xd0; UITextComponent text +0x368 (wchar_t*), capacity +0x36c.
// reflex_world.h ReflexUiNode / ReflexUiLayer / ReflexUiAnim, same layouts.
struct UiNode {
    int32_t parent, layer;     // parent node index (-1: the layer's root), layer index
    uint32_t object, vtable, state;
    float position[2], scale[2], size[2];
    uint32_t color;
    int32_t text;              // byte offset of the UTF-8 text in the text buffer, -1: not a text component
    char name[48];
};
struct UiLayer {
    int32_t first_node, nodes;
    uint32_t object, state;
    char graph[32], page[48], name[48];
};
struct UiAnim {               // a composite animation started (state 0) or stopped (3) since the last ui_frame, in call order
    int32_t node, state;      // node: index in ui_frame's nodes (-1: a component not mirrored)
    float time, length;
    char name[48];
};
// The live UI: every visible page graph's current page (container order = draw order), its layers (page order) and their trees.
// The animation events are those since the previous call. Between frames.
void ui_frame(Machine& m, std::vector<UiLayer>& layers, std::vector<UiNode>& nodes, std::vector<UiAnim>& anims, std::string& text);
// One named subtree (diagnostics): the visible instance, depth first.
int ui_collect(Machine& m, const std::string& root, std::vector<UiNode>& out, std::string& text);
// Puts the game's UI where the shipping flow has it once a level runs: the InGame Loading Page's RoutedGotoInGameHUB enters
// InGamePageGraph on its "Fullscreen HUB" with the focus (the start-up graphs lose focus and visibility), then the hub's route for the
// running activity ("Goto<Activity>SinglePlayer", built like the hub's PlayerJoined handler 005e2d20; or `event`). Installs the
// animation hook ui_frame reports from. Between frames. Returns the event posted ("" failed).
std::string ui_activate(Machine& m, const std::string& event = "");
bool ui_active(const Machine& m);
// The UI events held back since the last call, one per line: those with which the pages leave the level (GotoFrontEndMainMenuPage,
// GotoQuittingPage, GotoCareerRaceAndRidePage, ...; their pages would unload the world through the database loader).
std::string ui_requests(Machine& m);
// A UI event as App.SendUIEvent posts it (00740ed0, EAX name). Between frames.
void ui_event(Machine& m, const std::string& name);
void ui_forget(const Machine* m);   // a world is gone

// The activity of the local player (reflex_world.h ReflexModeState, same layout).
struct ModeState {
    int32_t valid, kind, state, sub_state;   // kind: index in mode_names (the activity class); state A+0x21c; A+0x238
    uint32_t activity, vtable;
    float clock, countdown;                  // A+0x294 activity clock; A+0x29c the countdown
    int32_t rules_hold, area_active;         // A+0xff068: the rules page is pending; P+0xa0: the player is in an activation area
    int32_t area_track, reserved;            // P+0x1a8: its TrackId
    char area[32];                           // P+0xa4: its activity ("HillClimb", "Trailblazer", "TargetPractice"; "FreeRide" outside)
    char name[32];                           // the class name without "MXActivity"
};
bool mode_state(Memory& m, ModeState& out);

// The challenges' collectables in the world (reflex_world.h ReflexModeObject, same layout): MXChallengeCollectable (0x938e04),
// MXTargetPracticeCollectable (0x938ebc: the targets of "<level>ChallengeTargetPractice", scene CollectibleTargetPracticeRSData) and
// MXHitDiscoveryCollectable (0x938f7c: "<level>FreeRide"). ModelEntity layout (ambient.cpp): +0x14 scene entity id, +0x30 world
// matrix, +0xa0 model instance (+0xa2 drawn); targets: +0x140 hit (their vf19 stops), +0x154 "Axis To Align", +0xab0 gTextureOffset.
struct ModeObject {
    uint32_t object, vtable;
    int32_t kind, entity_id, drawn, hit, axis, reserved;
    float uv_offset[2];
    float matrix[16];
};
int mode_objects(Machine& m, std::vector<ModeObject>& out);
// WP-T (tutorial.h): a world whose UI is already in game (the MotoSkills world is written on its Freeride page): ui_activate's hooks
// (the held-back UI events, the animation log) and the mirror marked active, nothing navigated. Between frames.
void ui_attach(Machine& m);
}  // namespace reflex_vm
