#pragma once
// The controller remapping of the PC game (WP-C; remap.cpp, _re/notes/input-settings.md "Controller remapping", names
// _re/names/9a_remap.tsv): the database table "ControllerRemapping" (two rows: 0 the keyboard, 1 the joystick = the XInput pad;
// 19 columns Throttle .. CameraTweakDown; cells are the game's input ids: keyboard 0x23..0x87, pad 1..0x18 = VIJoystick_*), which the
// Controller Remap Page edits (ControllerRemapping.lua: App.GetRemapTable*, SetRemapTableByIndex, InitializeRemapTables), the save
// keeps (savegame.bxml: the table is registered in controller 0's database 00ae10f0 and saved with the Player* tables) and the player
// creation reads (PlayerSlot_FromPlayerList 0054a7b0 -> Controller_BuildKeyMaps 00547f30: the controller's KeyMaps, device 2 the
// keyboard key, device 1 the pad input).
#include <cstdint>
#include <string>

namespace reflex_vm {
class Machine;
class Boot;
namespace remap {

constexpr uint32_t STATIC_DB = 0xae0910;      // the static database (MXTables_Default; the table's defaults are built here, 005b9340)
constexpr uint32_t CONTROLLER_DB = 0xae10f0;  // controller 0's database (the save's tables; the remap page's App bindings use it)

// The table as JSON: {"table":<addr>,"static":<addr>,"columns":[...],"kb":[...],"js":[...]} (+ "kbText"/"jsText", the names the
// page shows (App.GetRemapTableCharByIndex: DirectInput's key names / the pad names "rt", "ls_x", ...), with names).
std::string table_json(Machine& m, bool names);

// Profile session (career::command delegates "remap ..." here):
//   "remap"                   table_json with names, plus "scan" (DIK -> game key id, g_ScanCodeToGameKey 00a95848), "keyNames"
//                             (every keyboard id), "joyNames" (pad ids 1..24); the key names are DirectInput's keyboard object names,
//                             which Keyboard_Create 00817090 has the game copy at start-up (EnumObjects -> 00817140); the profile
//                             boot's DirectInput stand-in enumerates none, so they are given to the same callback here first.
//   "remap mapped <id>"       Remap_IsMapped 005b99e0 (any cell of the table holds <id>): "0" / "1" (the UIKeyboardInput /
//                             UIJoystickInput events' alreadyMapped).
//   "remap glyph <MapKey>"    what a UIImageComponent with KeyMap=<MapKey> shows (UIImageComponent::vf1 005cb3f0): the KeyMap
//                             (00813690 over 00d92f50) -> vf8(7, 0) the bound input -> 009d3d20 -> the image name, else UIEmpty.
bool profile_command(Boot& b, const std::string& cmd, std::string* out);

// World runtime (reflex_world_remap), between frames:
//   "table"   table_json (no names).
//   "apply"   the local human players' KeyMaps built again from the table, as their creation did (PlayerSlot_FromPlayerList:
//             00547f30(controller, ControllerPreset, ControllerHandedness, ControllerMotion of that controller's UI settings)).
//             For a world whose save was attached after the boot (persist "attach"): the creation ran on the boot's defaults.
std::string world_command(Machine& m, const std::string& cmd, std::string* error);

}  // namespace remap
}  // namespace reflex_vm
