// Input and settings stand-ins of a running world (input.h, _re/notes/input-settings.md, names _re/names/85_input.tsv).
#include "input.h"
#include "vm.h"
#include <cstring>
#include <strings.h>
#include <string>

namespace reflex_vm {
namespace input {
namespace {
constexpr uint32_t WNDPROC = 0x880dc0;           // MXWndProc (WM_INPUT 0xff, WM_SYSKEYUP Alt, WM_SYSCOMMAND, WM_ACTIVATEAPP, ...)
constexpr uint32_t HWND_GLOBAL = 0xa95984;       // the game window
constexpr uint32_t RAW_HANDLE = 0x52415749;      // HRAWINPUT of the message ('IWAR'; the game only passes it back)
constexpr uint32_t RID_INPUT = 0x10000003;
constexpr uint32_t UI_SETTINGS = 0xd92efc;       // MXUISettings*: per device +4 + device * 0x48 (16 ints), +0x124 the default block, +0x16c active device
constexpr uint32_t UI_SETTING_NAMES = 0x9dd028;  // 16 char* (UISetting_Index 00740c30)
constexpr uint32_t UI_SETTINGS_GET = 0x5b9f50;   // ECX settings, EAX index, ESI device
constexpr uint32_t UI_SETTINGS_SET = 0x5b9fa0;   // EAX settings, EDX value, ESI index, EDI device
uint32_t rd(Machine& m, uint32_t a) { return uint32_t(m.mem.read(a, 4)); }
}  // namespace

void install(Machine& m, State& s) {
    // GetRawInputData(hRawInput, uiCommand, pData, pcbSize, cbSizeHeader): the window procedure asks the size first (pData NULL),
    // (re)allocates its buffer [00da68c8] when it changed, then reads the RAWINPUT.
    m.imports["user32.dll!GetRawInputData"] = [&s](Machine& m) {
        uint32_t cmd = m.arg(1), data = m.arg(2), psize = m.arg(3), header = m.arg(4);
        if (cmd != RID_INPUT || header != 16 || !psize) return m.ret(20, 0xffffffffu);
        if (!data) { m.mem.write(psize, 4, uint32_t(sizeof s.raw)); return m.ret(20, 0); }
        if (rd(m, psize) < sizeof s.raw) return m.ret(20, 0xffffffffu);
        m.mem.write_bytes(data, s.raw, sizeof s.raw);
        return m.ret(20, uint32_t(sizeof s.raw));
    };
    m.imports["user32.dll!DefWindowProcA"] = [](Machine& m) { return m.ret(16, 0); };
    // XInputSetState(dwUserIndex, XINPUT_VIBRATION*): Pad_UpdateVibration 008169f0 writes wLeftMotorSpeed = pad+0x2cc * 65535,
    // wRightMotorSpeed = pad+0x2d4 * 65535 for a connected pad whose vibration is on (pad+0x2b8). Recorded, then the runtime's
    // own stand-in answers (ERROR_DEVICE_NOT_CONNECTED for a pad not in pad_mask, which the game then retries).
    auto it = m.imports.find("xinput1_3.dll!#3");
    Native prev = it != m.imports.end() ? it->second : Native([](Machine& m) { return m.ret(8, 1167); });
    m.imports["xinput1_3.dll!#3"] = [&s, prev](Machine& m) {
        uint32_t index = m.arg(0), vib = m.arg(1);
        if (index < 4 && vib) {
            uint32_t v = rd(m, vib);   // wLeftMotorSpeed (low word), wRightMotorSpeed (high word)
            s.motors[index].store(((v & 0xffff) << 16) | (v >> 16)); s.calls[index].fetch_add(1);
        }
        return prev(m);
    };
}

void post_mouse(Machine& m, State& s, int32_t dx, int32_t dy, int32_t wheel, uint32_t button_flags) {
    uint32_t hwnd = rd(m, HWND_GLOBAL);
    uint16_t flags = uint16_t(button_flags & 0x3ff);
    if (wheel) flags |= 0x400;                                           // RI_MOUSE_WHEEL: usButtonData is the wheel delta
    std::memset(s.raw, 0, sizeof s.raw);
    uint32_t header[4] = {0 /*RIM_TYPEMOUSE*/, uint32_t(sizeof s.raw), 0 /*hDevice*/, 0 /*wParam RIM_INPUT*/};
    std::memcpy(s.raw, header, 16);
    uint16_t usFlags = 0 /*MOUSE_MOVE_RELATIVE*/, data = uint16_t(int16_t(wheel));
    std::memcpy(s.raw + 16, &usFlags, 2);                                // RAWMOUSE +0 usFlags
    std::memcpy(s.raw + 20, &flags, 2);                                  // +4 usButtonFlags
    std::memcpy(s.raw + 22, &data, 2);                                   // +6 usButtonData
    std::memcpy(s.raw + 28, &dx, 4);                                     // +12 lLastX
    std::memcpy(s.raw + 32, &dy, 4);                                     // +16 lLastY
    uint32_t args[4] = {hwnd, 0xff /*WM_INPUT*/, 0 /*RIM_INPUT*/, RAW_HANDLE};
    m.call_function(WNDPROC, args, 4);
    s.messages++;
}

int32_t ui_setting(Machine& m, const char* name, int32_t value, int32_t device, int mode) {
    bool set = mode != 0;
    uint32_t st = rd(m, UI_SETTINGS);
    if (!st || !m.mem.mapped(st + 0x16c)) throw VmError("no MXUISettings object ([00d92efc])");
    int32_t index = -1;
    for (int32_t i = 0; i < 16 && index < 0; i++) {
        uint32_t p = rd(m, UI_SETTING_NAMES + 4 * i); char n[40] = {};
        for (uint32_t k = 0; k + 1 < sizeof n; k++) { n[k] = char(m.mem.read(p + k, 1)); if (!n[k]) break; }
        if (name && !strcasecmp(n, name)) index = i;
    }
    if (index < 0) throw VmError(std::string("unknown UI setting ") + (name ? name : "(null)"));
    if (device < 0 || device > 3) { device = int32_t(rd(m, st + 0x16c)); if (device < 0 || device > 3) device = 0; }
    m.r[6] = uint32_t(device);
    int32_t before = int32_t(m.call_function(UI_SETTINGS_GET, nullptr, 0, st, index));
    if (!set) return before;
    m.r[2] = uint32_t(value); m.r[6] = uint32_t(index); m.r[7] = uint32_t(device);
    m.call_function(UI_SETTINGS_SET, nullptr, 0, -1, st);
    if (mode == 2 && index == 0) {
        // As the settings the world was made with: the local player was created (PlayerSlot_FromPlayerList 0054a7b0) before they
        // reached the game, with P+0x988 = [009c5410 + 4 * controller] (the camera the settings select); a race still on the gate
        // (activity state 0, where 0054c9b0 does not switch) gets that assignment now, as if created after them.
        uint32_t G = rd(m, 0xa946b8);
        if (G) {
            uint32_t P = G + 0x7460 + rd(m, G + 0x153ec) * 0x12a0, A = rd(m, P + 0x94), ctrl = rd(m, P + 0x1f0);
            if (rd(m, P + 0x10) == 0 && ctrl < 4 && A && rd(m, A + 0x21c) == 0) m.mem.write(P + 0x988, 4, rd(m, 0x9c5410 + 4 * ctrl));
        }
    }
    return before;
}

}  // namespace input
}  // namespace reflex_vm
