#pragma once
// Input and settings stand-ins of a running world (input.cpp, _re/notes/input-settings.md): the mouse as the PC game reads it
// (Raw Input WM_INPUT through the game's own window procedure), the gamepad rumble the game writes (XInputSetState), and the
// game's UI settings (App.SetUISetting / GetUISetting: DrivingCamera, MouseSensitivity, Vibration, ...).
#include <atomic>
#include <cstdint>

namespace reflex_vm {
class Machine;
namespace input {

struct State {
    std::atomic<uint32_t> motors[4] = {};   // the last XInputSetState per pad: (wLeftMotorSpeed << 16) | wRightMotorSpeed
    std::atomic<uint32_t> calls[4] = {};    // XInputSetState calls per pad (a connected pad's)
    uint8_t raw[40] = {};                   // the RAWINPUT (RIM_TYPEMOUSE) GetRawInputData hands the window procedure
    uint32_t messages = 0;                  // WM_INPUT messages delivered
};

// After World::load: GetRawInputData / DefWindowProcA for the window procedure, XInputSetState recorded (wraps the runtime's).
void install(Machine& m, State& s);
// One WM_INPUT (Raw Input mouse: relative lLastX / lLastY in mouse counts, usButtonFlags RI_MOUSE_*, wheel in WHEEL_DELTA units
// with RI_MOUSE_WHEEL 0x400) through the game's window procedure 00880dc0, between two frames (as the message pump before
// Engine::Frame): WM_INPUT -> GetRawInputData -> Mouse_AccumulateRawInput 00817cb0 into the input manager's mouse (+0x3234).
void post_mouse(Machine& m, State& s, int32_t dx, int32_t dy, int32_t wheel, uint32_t button_flags);
// The game's UI setting `name` (009dd028: DrivingCamera, Brightness, ... Vibration) of `device` (-1: the settings' active device,
// [00d92efc]+0x16c, else 0). set: UISettings_Set 005b9fa0 (which runs UISettings_Apply 005b9af0 when the value changes) and
// returns the value before; else UISettings_Get 005b9f50. Throws VmError for an unknown name or no settings object.
// mode 0 get, 1 set (as the options pages do: on the gate a new DrivingCamera applies from the next race, as in the game), 2 set as the
// settings the world was made with (DrivingCamera: the local player's camera also as its creation would have set it, on the gate).
int32_t ui_setting(Machine& m, const char* name, int32_t value, int32_t device, int mode);

}  // namespace input
}  // namespace reflex_vm
