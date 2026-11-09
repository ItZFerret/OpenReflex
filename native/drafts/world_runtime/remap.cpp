// The controller remapping of the PC game (remap.h, _re/notes/input-settings.md "Controller remapping", _re/names/9a_remap.tsv).
#include "remap.h"
#include "boot.h"
#include "vm.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <iterator>

namespace reflex_vm {
namespace remap {
namespace {
constexpr uint32_t FIND_TABLE = 0x402220;        // Database_FindTable (EAX database, stack(name), ret 4)
constexpr uint32_t COLUMN_INDEX = 0x402160;      // Table_ColumnIndex (EAX table, stack(name), ret 4)
constexpr uint32_t IS_MAPPED = 0x5b99e0;         // Remap_IsMapped (cdecl(id)): any cell of controller 0's table == id
constexpr uint32_t BUILD_KEYMAPS = 0x547f30;     // Controller_BuildKeyMaps (cdecl(controller, preset, handedness, motion))
constexpr uint32_t KEY_NAME_CALLBACK = 0x817140; // Keyboard_EnumKeyNameCallback (stdcall(DIDEVICEOBJECTINSTANCEW*, ref)): wcscpy_s into 00a81b28
constexpr uint32_t KEY_NAMES = 0xa81b28;         // 256 x 0x40: wide key names by scan code
constexpr uint32_t SCAN_TO_KEY = 0xa95848;       // byte[256]: DIK scan code -> game key id
constexpr uint32_t KEY_TO_SCAN = 0xa85a9c;       // int[]: game key id -> scan code (valid for 0x23..0x88)
constexpr uint32_t JOY_NAMES = 0xd94044;         // pad input id * 0x40: wide names (00812e40)
constexpr uint32_t INPUT_NAMES_INIT = 0x812170;  // InputNames_Init: the key / pad name tables and the KeyMap registry 00d92f50
constexpr uint32_t INPUT_NAMES_GUARD = 0xd92f48; // its static-init guard (bit 0)
constexpr uint32_t KEYMAP_REGISTRY = 0xd92f50;
constexpr uint32_t KEYMAP_FIND = 0x813690;       // KeyMapRegistry_Find (ESI registry, stack(name), ret 4)
constexpr uint32_t GLYPH_TABLE = 0x9d3d20;       // 10 x {pad input id, "xenon_Controller_Button_N"}
constexpr uint32_t UI_SETTINGS = 0xd92efc;

// The table's columns in the order the defaults create them (005b9340); the remap page shows the first 15.
const char* const COLUMNS[] = {"Throttle", "Brake", "SteerLeft", "SteerRight", "TrickModifier", "Clutch", "ReflexLeft", "ReflexRight",
                               "ReflexUp", "ReflexDown", "Camera", "SetResetPoint", "Reset", "UseResetPoint", "Reverse",
                               "CameraTweakLeft", "CameraTweakRight", "CameraTweakUp", "CameraTweakDown"};

// DirectInput's keyboard object names (DIDEVICEOBJECTINSTANCE.tszName) for the DIK offsets of a US keyboard on an English Windows:
// DirectInput takes them from the keyboard layout (GetKeyNameText of the scan code; extended keys with the extended bit). Not game
// data: what the Windows the game ran on hands Keyboard_Create's EnumObjects.
struct KeyName { uint8_t dik; const char* name; };
const KeyName DI_KEY_NAMES[] = {
    {0x01, "Esc"}, {0x02, "1"}, {0x03, "2"}, {0x04, "3"}, {0x05, "4"}, {0x06, "5"}, {0x07, "6"}, {0x08, "7"}, {0x09, "8"}, {0x0a, "9"},
    {0x0b, "0"}, {0x0c, "-"}, {0x0d, "="}, {0x0e, "Backspace"}, {0x0f, "Tab"}, {0x10, "Q"}, {0x11, "W"}, {0x12, "E"}, {0x13, "R"},
    {0x14, "T"}, {0x15, "Y"}, {0x16, "U"}, {0x17, "I"}, {0x18, "O"}, {0x19, "P"}, {0x1a, "["}, {0x1b, "]"}, {0x1c, "Enter"},
    {0x1d, "Ctrl"}, {0x1e, "A"}, {0x1f, "S"}, {0x20, "D"}, {0x21, "F"}, {0x22, "G"}, {0x23, "H"}, {0x24, "J"}, {0x25, "K"}, {0x26, "L"},
    {0x27, ";"}, {0x28, "'"}, {0x29, "`"}, {0x2a, "Shift"}, {0x2b, "\\"}, {0x2c, "Z"}, {0x2d, "X"}, {0x2e, "C"}, {0x2f, "V"}, {0x30, "B"},
    {0x31, "N"}, {0x32, "M"}, {0x33, ","}, {0x34, "."}, {0x35, "/"}, {0x36, "Right Shift"}, {0x37, "Num *"}, {0x38, "Alt"}, {0x39, "Space"},
    {0x3a, "Caps Lock"}, {0x3b, "F1"}, {0x3c, "F2"}, {0x3d, "F3"}, {0x3e, "F4"}, {0x3f, "F5"}, {0x40, "F6"}, {0x41, "F7"}, {0x42, "F8"},
    {0x43, "F9"}, {0x44, "F10"}, {0x45, "Num Lock"}, {0x46, "Scroll Lock"}, {0x47, "Num 7"}, {0x48, "Num 8"}, {0x49, "Num 9"},
    {0x4a, "Num -"}, {0x4b, "Num 4"}, {0x4c, "Num 5"}, {0x4d, "Num 6"}, {0x4e, "Num +"}, {0x4f, "Num 1"}, {0x50, "Num 2"}, {0x51, "Num 3"},
    {0x52, "Num 0"}, {0x53, "Num Del"}, {0x56, "\\"}, {0x57, "F11"}, {0x58, "F12"}, {0x9c, "Num Enter"}, {0x9d, "Right Ctrl"},
    {0xb5, "Num /"}, {0xb7, "Prnt Scrn"}, {0xb8, "Right Alt"}, {0xc5, "Pause"}, {0xc7, "Home"}, {0xc8, "Up"}, {0xc9, "Page Up"},
    {0xcb, "Left"}, {0xcd, "Right"}, {0xcf, "End"}, {0xd0, "Down"}, {0xd1, "Page Down"}, {0xd2, "Insert"}, {0xd3, "Delete"},
    {0xdb, "Left Windows"}, {0xdc, "Right Windows"}, {0xdd, "Application"},
};

uint32_t rd(Machine& m, uint32_t a) { return uint32_t(m.mem.read(a, 4)); }
std::string json_escape(const std::string& s) {
    std::string o;
    for (char c : s) { if (c == '"' || c == '\\') { o += '\\'; o += c; } else if (uint8_t(c) < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); o += b; } else o += c; }
    return o;
}
// A wide (UTF-16) string of the game (<= 31 characters) as UTF-8 (the names are ASCII).
std::string wide(Machine& m, uint32_t a) {
    std::string o;
    for (uint32_t i = 0; i < 32; i++) { uint16_t c = uint16_t(m.mem.read(a + 2 * i, 2)); if (!c) break; o += c < 0x80 ? char(c) : '?'; }
    return o;
}
std::string key_name(Machine& m, int32_t id) {   // RemapTable_InputName 00741130's lookup of an input id
    if (id >= 0x23 && id <= 0x88) return wide(m, KEY_NAMES + rd(m, KEY_TO_SCAN + 4 * uint32_t(id)) * 0x40);
    if (id >= 1 && id <= 0x18) return wide(m, JOY_NAMES + uint32_t(id) * 0x40);
    return "";
}

struct Table { uint32_t addr = 0; std::vector<int32_t> col; };   // col[i]: the table's column index of COLUMNS[i] (-1 none)
// The table's cell (row, column): column-major storage (table +0x10 the columns; column +0x40 its cells, +0xc / +0x10), as 005be930.
int32_t cell(Machine& m, uint32_t table, int32_t row, int32_t column) {
    uint32_t b = rd(m, table + 0x10), e = rd(m, table + 0x14);
    if (column < 0 || uint32_t(column) >= (e - b) / 4) return -1;
    uint32_t c = rd(m, b + 4 * uint32_t(column)), d = rd(m, c + 0x40), db = rd(m, d + 0xc), de = rd(m, d + 0x10);
    if (row < 0 || uint32_t(row) >= (de - db) / 4) return -1;
    return int32_t(rd(m, db + 4 * uint32_t(row)));
}
}  // namespace

// Machine-level calls: the world (call_function between frames, as input.cpp) or the profile boot (Boot::call).
struct Caller {
    Machine& m; Boot* b;
    uint32_t operator()(uint32_t fn, std::vector<uint32_t> args, uint32_t eax = 0, uint32_t ecx = 0, uint32_t esi = 0) {
        if (b) { Boot::GuestCall c; c.fn = fn; c.args = std::move(args); c.eax = eax; c.ecx = ecx; c.esi = esi; return b->call(c); }
        uint32_t sp = m.r[4]; m.r[6] = esi; uint32_t r = m.call_function(fn, args.data(), int(args.size()), ecx, eax); m.r[4] = sp; return r;
    }
    uint32_t str(const std::string& s) {
        if (b) return b->guest_string(s);
        // world: a scratch string below the stack (the frame's own stack is not in use between frames)
        uint32_t a = (m.r[4] - 0x400 - uint32_t(s.size()) - 1) & ~15u; m.mem.write_block(a, s.c_str(), s.size() + 1); m.r[4] = a - 0x40; return a;
    }
};

static Table find(Caller& call, uint32_t db) {
    Table t;
    uint32_t sp = call.m.r[4];
    t.addr = call(FIND_TABLE, {call.str("ControllerRemapping")}, db);
    if (t.addr) for (const char* c : COLUMNS) t.col.push_back(int32_t(call(COLUMN_INDEX, {call.str(c)}, t.addr)));
    if (!call.b) call.m.r[4] = sp;
    return t;
}

static std::string json(Caller& call, bool names) {
    Machine& m = call.m;
    Table t = find(call, CONTROLLER_DB), s = find(call, STATIC_DB);
    std::string o = "{\"table\":" + std::to_string(t.addr) + ",\"static\":" + std::to_string(s.addr) + ",\"columns\":[";
    for (size_t i = 0; i < std::size(COLUMNS); i++) o += std::string(i ? "," : "") + "\"" + COLUMNS[i] + "\"";
    for (int row = 0; row < 2; row++) {
        o += row ? "],\"js\":[" : "],\"kb\":[";
        for (size_t i = 0; i < std::size(COLUMNS); i++) o += (i ? "," : "") + std::to_string(t.addr && i < t.col.size() ? cell(m, t.addr, row, t.col[i]) : -1);
    }
    o += "]";
    if (names && t.addr) {
        for (int row = 0; row < 2; row++) {
            o += row ? ",\"jsText\":[" : ",\"kbText\":[";
            for (size_t i = 0; i < std::size(COLUMNS); i++) o += std::string(i ? "," : "") + "\"" + json_escape(key_name(m, cell(m, t.addr, row, t.col[i]))) + "\"";
            o += "]";
        }
    }
    return o + "}";
}

std::string table_json(Machine& m, bool names) { Caller c{m, nullptr}; return json(c, names); }

// InputNames_Init has run (its guard), and the key names DirectInput gives Keyboard_Create's EnumObjects are in 00a81b28, through
// the game's own callback 00817140 (an all-empty table: the boot's DirectInput stand-in enumerated no objects).
static void ensure_names(Boot& b) {
    Machine& m = b.machine();
    if (!(rd(m, INPUT_NAMES_GUARD) & 1)) { m.mem.write(INPUT_NAMES_GUARD, 4, rd(m, INPUT_NAMES_GUARD) | 1); Boot::GuestCall c; c.fn = INPUT_NAMES_INIT; b.call(c); }
    bool empty = true;
    for (uint32_t i = 0; i < 256 && empty; i++) if (m.mem.read(KEY_NAMES + i * 0x40, 2)) empty = false;
    if (!empty) return;
    uint32_t inst = b.guest_string(std::string(0x240, ' '));
    for (const KeyName& k : DI_KEY_NAMES) {
        m.mem.write_block(inst, nullptr, 0x240);
        m.mem.write(inst, 4, 0x240);                    // dwSize (DIDEVICEOBJECTINSTANCEW)
        m.mem.write(inst + 0x14, 4, k.dik);             // dwOfs: the DIK code
        m.mem.write(inst + 0x18, 4, 0x0c | (uint32_t(k.dik) << 8));   // dwType: DIDFT_PSHBUTTON | DIDFT_MAKEINSTANCE(dik)
        for (uint32_t i = 0; k.name[i]; i++) m.mem.write(inst + 0x20 + 2 * i, 2, uint8_t(k.name[i]));   // tszName
        Boot::GuestCall c; c.fn = KEY_NAME_CALLBACK; c.args = {inst, 0}; b.call(c);
    }
}

bool profile_command(Boot& b, const std::string& cmd, std::string* out) {
    Machine& m = b.machine(); Caller call{m, &b};
    ensure_names(b);
    char a[128] = {}; int v = 0;
    if (cmd == "remap") {
        std::string o = json(call, true); o.pop_back();
        o += ",\"scan\":[";
        for (uint32_t i = 0; i < 256; i++) o += (i ? "," : "") + std::to_string(m.mem.read(SCAN_TO_KEY + i, 1));
        o += "],\"keyNames\":{";
        for (int32_t id = 0x23; id <= 0x87; id++) o += std::string(id > 0x23 ? "," : "") + "\"" + std::to_string(id) + "\":\"" + json_escape(key_name(m, id)) + "\"";
        o += "},\"joyNames\":{";
        for (int32_t id = 1; id <= 0x18; id++) o += std::string(id > 1 ? "," : "") + "\"" + std::to_string(id) + "\":\"" + json_escape(key_name(m, id)) + "\"";
        if (out) *out = o + "}}";
        return true;
    }
    if (std::sscanf(cmd.c_str(), "remap mapped %d", &v) == 1) { if (out) *out = std::to_string(call(IS_MAPPED, {uint32_t(v)}) & 0xff); return true; }
    if (std::sscanf(cmd.c_str(), "remap glyph %127s", a) == 1) {
        uint32_t km = call(KEYMAP_FIND, {b.guest_string(a)}, 0, 0, KEYMAP_REGISTRY);
        if (!km) { if (out) *out = "{\"keymap\":0}"; return true; }
        uint32_t input = call(rd(m, rd(m, km) + 0x20), {7, 0}, 0, km);   // KeyMap vf8(7, 0): the bound input UIImageComponent::vf1 asks for
        std::string image = "UIEmpty";
        for (uint32_t i = 0; i < 10; i++) if (rd(m, GLYPH_TABLE + 8 * i) == input) {
            uint32_t p = rd(m, GLYPH_TABLE + 8 * i + 4); image.clear();
            for (uint32_t k = 0; k < 64; k++) { char c = char(m.mem.read(p + k, 1)); if (!c) break; image += c; }
        }
        std::string kids;   // a CompoundKeyMap (vtable 00974c10): its children (+0xa4, 4) and their slot-0 device / input
        if (rd(m, km) == 0x974c10) for (uint32_t i = 0; i < 4; i++) if (uint32_t k = rd(m, km + 0xa4 + 4 * i))
            kids += std::string(kids.empty() ? "" : ",") + "{\"device\":" + std::to_string(rd(m, k + 0x14)) + ",\"input\":" + std::to_string(rd(m, k + 0x18)) + ",\"name\":\"" + json_escape(key_name(m, int32_t(rd(m, k + 0x18)))) + "\"}";
        if (out) *out = "{\"keymap\":" + std::to_string(km) + ",\"vtable\":" + std::to_string(rd(m, km)) + ",\"children\":[" + kids + "],\"input\":" + std::to_string(int32_t(input)) + ",\"inputName\":\"" + json_escape(key_name(m, int32_t(input))) + "\",\"image\":\"" + image + "\"}";
        return true;
    }
    if (out) *out = "remap | remap mapped <id> | remap glyph <MapKey>";
    return false;
}

std::string world_command(Machine& m, const std::string& cmd, std::string* error) {
    Caller call{m, nullptr};
    if (cmd == "table") return json(call, false);
    if (cmd == "apply") {
        uint32_t G = rd(m, 0xa946b8), st = rd(m, UI_SETTINGS);
        if (!G || !st) { if (error) *error = "no game / UI settings"; return ""; }
        std::string o = "{\"applied\":[";
        int n = 0;
        for (uint32_t slot = 0; slot < 8; slot++) {
            uint32_t P = G + 0x7460 + slot * 0x12a0;
            if (!m.mem.mapped(P + 0x1f0)) break;
            int32_t ctrl = int32_t(rd(m, P + 0x1f0));
            if (rd(m, P + 0x10) != 0 || !rd(m, P + 4) || ctrl < 0 || ctrl > 3) continue;   // a local human (PlayerLocal, PlayerType 0) with a controller
            uint32_t preset = rd(m, st + 0x18 + uint32_t(ctrl) * 0x48), handed = rd(m, st + 0x30 + uint32_t(ctrl) * 0x48), motion = rd(m, st + 0x2c + uint32_t(ctrl) * 0x48);
            uint32_t sp = m.r[4]; uint32_t args[4] = {uint32_t(ctrl), preset, handed, motion};
            m.call_function(BUILD_KEYMAPS, args, 4); m.r[4] = sp;
            o += std::string(n++ ? "," : "") + "{\"slot\":" + std::to_string(slot) + ",\"controller\":" + std::to_string(ctrl) + ",\"preset\":" + std::to_string(preset) +
                 ",\"handedness\":" + std::to_string(handed) + ",\"motion\":" + std::to_string(motion) + "}";
        }
        return o + "],\"table\":" + json(call, false) + "}";
    }
    if (error) *error = "unknown remap command " + cmd;
    return "";
}

}  // namespace remap
}  // namespace reflex_vm
