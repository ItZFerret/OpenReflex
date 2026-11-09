// loop_input.cpp -- oracle adapters for core/game/loop_input.c (group "rx_loop").
#include "oracle.h"
#include <cstdlib>
extern "C" {
#include "../game/loop_input.h"
}
using namespace reflex_port;
using rx_oracle::f32;

// thiscall (dt); ret 4. void: no result register (callers of slot 5 read none).
RX_PORT(0x00811490, KeyMap_vf5, "rx_loop", OUT_FPU) { rx_keymap_update(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x008114e0, KeyMap_vf6, "rx_loop", OUT_FPU) { rx_keymap_sample(m.r[1], m.arg(0), f32(m.arg(1))); return m.ret(8); }
// vf7 leaves EAX = 1 or 0 (the state it stored)
RX_PORT(0x00811520, KeyMap_vf7, "rx_loop", OUT_FPU) { rx_keymap_sample_repeat(m.r[1], m.arg(0), f32(m.arg(1))); return m.ret(8); }
// EAX = pad record, ECX = id - 1; plain ret; result EAX
RX_PORT(0x00816530, Pad_Button, "rx_loop", OUT_EAX) { m.r[0] = rx_pad_button(m.r[0], m.r[1]); return m.ret(0); }
// EAX = id, EDX = state, stack keys; ret 4; result EAX (ECX = keys, as the original leaves it)
RX_PORT(0x008172b0, Keyboard_KeyState, "rx_loop", OUT_EAX | OUT_ECX) {
    const uint32_t keys = m.arg(0);
    m.r[0] = rx_keyboard_key_state(keys, m.r[0], m.r[2]); m.r[1] = keys; return m.ret(4);
}
// EAX = mouse record, ECX = index; plain ret; AL = result, the rest of EAX as the original leaves it
RX_PORT(0x00817af0, Mouse_Button, "rx_loop", OUT_EAX) {
    const uint32_t mouse = m.r[0], i = m.r[1], r = rx_mouse_button(mouse, i);
    uint32_t eax;
    if (i < 2) eax = (mouse & 0xffffff00u) | r;                                   // mov al, byte
    else if (i == 2) eax = r;                                                     // and eax, 1
    else eax = ((m.mem.read(mouse + 0x120, 4) >> (i - 2)) & 0xffffff00u) | r;     // shr eax, n; and al, 1
    m.r[0] = eax; return m.ret(0);
}
RX_PORT(0x00814a70, XenonInputManager_vf27, "rx_loop", OUT_EAX) {
    m.r[0] = rx_input_held(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4)); return m.ret(0x14);
}
RX_PORT(0x00811850, AnalogKeyMap_vf7, "rx_loop", OUT_FPU) { rx_analog_keymap_sample(m.r[1], m.arg(0), f32(m.arg(1))); return m.ret(8); }
RX_PORT(0x00811a60, CompoundKeyMap_vf1, "rx_loop", OUT_EAX) { m.r[0] = rx_compound_keymap_pressed(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00811c50, CompoundKeyMap_vf9, "rx_loop", OUT_FPU) { rx_compound_keymap_set(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00811dc0, CumulativeKeyMap_vf5, "rx_loop", OUT_FPU) { rx_cumulative_keymap_update(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00816b60, Pad_Active, "rx_loop", OUT_EAX) { m.r[0] = rx_pad_active(m.r[1]); return m.ret(0); }
// AL = result; the original's "mov al, 1" keeps the mouse address in the upper bytes, its setne path too
RX_PORT(0x00817c60, Mouse_Active, "rx_loop", OUT_EAX) { m.r[0] = (m.r[0] & 0xffffff00u) | rx_mouse_active(m.r[0]); return m.ret(0); }
RX_PORT(0x00814750, XenonInputManager_vf2, "rx_loop", OUT_EAX) { m.r[0] = rx_input_device_count(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x008147f0, XenonInputManager_vf3, "rx_loop", OUT_EAX) { m.r[0] = rx_input_connected(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00814830, XenonInputManager_vf23, "rx_loop", OUT_EAX) { m.r[0] = rx_input_any_active(m.r[1]); return m.ret(0); }
RX_PORT(0x00814890, XenonInputManager_vf21, "rx_loop", OUT_EAX) { m.r[0] = rx_input_active(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00814920, XenonInputManager_vf26, "rx_loop", OUT_EAX) { m.r[0] = rx_input_any_button(m.r[1]); return m.ret(0); }
RX_PORT(0x008149d0, XenonInputManager_vf24, "rx_loop", OUT_EAX) { m.r[0] = rx_input_button_down(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00815090, XenonInputManager_vf52, "rx_loop", OUT_EAX) { m.r[0] = rx_input_mouse_active(m.r[1]); return m.ret(0); }
RX_PORT(0x00815290, XenonInputManager_vf48, "rx_loop", 0) { rx_input_stop_vibration(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
static float xmm0f(reflex_vm::Machine& m, int i) { return f32(uint32_t(m.xmm[i].lo)); }
// LTCG device updates: EAX / ESI = the device record, dt on the stack or in an xmm register; void
RX_PORT(0x008157a0, Pad_Update, "rx_loop", OUT_FPU) { rx_pad_update(m.r[0], f32(m.arg(0))); return m.ret(4); }
// EAX = analog record, CX = axis value; stack dt (cdecl, plain ret); result xmm0 lane 0 (movss: the upper lanes as the caller had
// them for the computed results, cleared by xorps for the centred case)
RX_PORT(0x008156e0, Pad_Axis, "rx_loop", OUT_XMM0) {
    const int16_t v = int16_t(m.r[1] & 0xffff);
    const float r = rx_pad_axis(m.r[0], m.r[1], f32(m.arg(0)));
    if (v > 0x1eb8 || v < -0x1eb8) m.xmm[0].lo = (m.xmm[0].lo & 0xffffffff00000000ull) | rx_oracle::bits(r);
    else { m.xmm[0].lo = 0; m.xmm[0].hi = 0; }
    return m.ret(0);
}
// EAX = gamepad, ESI = pad; stack old, dt; ret 8; ECX = the dead-zoned new RX
RX_PORT(0x00815900, Pad_Sticks, "rx_loop", OUT_ECX) { m.r[1] = rx_pad_sticks(m.r[0], m.r[6], m.arg(0), f32(m.arg(1))); return m.ret(8); }
// ESI = gamepad, EDI = pad; stack old; ret 4
RX_PORT(0x00816400, Pad_Triggers, "rx_loop", 0) { rx_pad_triggers(m.r[6], m.r[7], m.arg(0)); return m.ret(4); }
// EAX = pad, xmm2 = dt (EAX unchanged; xmm4 untouched)
RX_PORT(0x00815b60, Pad_ButtonsHeld, "rx_loop", OUT_EAX) {
    rx_pad_buttons_held(m.r[0], rx_oracle::f32(uint32_t(m.xmm[2].lo))); m.r[1] = 2; m.r[2] = 1; return m.ret(0);
}
// EAX = pad, xmm1 = dt; ECX = 1
RX_PORT(0x00815fb0, Pad_ButtonsReleased, "rx_loop", OUT_ECX | OUT_FPU) {
    rx_pad_buttons_released(m.r[0], rx_oracle::f32(uint32_t(m.xmm[1].lo))); m.r[1] = 1; m.r[2] = 0; return m.ret(0);
}
// ESI = pad; stack dt; ret 4
RX_PORT(0x008169f0, Pad_UpdateVibration, "rx_loop", 0) { rx_pad_update_vibration(m.r[6], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00816960, Pad_AnyInput, "rx_loop", OUT_EAX) { m.r[0] = rx_pad_any_input(m.r[0]); return m.ret(0); }
RX_PORT(0x00816ef0, Keyboard_Poll, "rx_loop", OUT_FPU) { rx_keyboard_poll(m.r[6]); return m.ret(0); }
RX_PORT(0x00817400, Mouse_UpdateCursor, "rx_loop", OUT_FPU) { rx_mouse_update_cursor(m.r[0]); return m.ret(0); }
RX_PORT(0x00817590, Mouse_UpdateAxes, "rx_loop", OUT_FPU) { rx_mouse_update_axes(m.r[0], xmm0f(m, 3)); return m.ret(0); }
RX_PORT(0x00817700, Mouse_UpdateWheel, "rx_loop", OUT_FPU) { rx_mouse_update_wheel(m.r[0], xmm0f(m, 2)); return m.ret(0); }
RX_PORT(0x00817850, Mouse_UpdateButtons, "rx_loop", OUT_FPU) { rx_mouse_update_buttons(m.r[0], xmm0f(m, 2)); return m.ret(0); }
RX_PORT(0x00817340, Mouse_Update, "rx_loop", OUT_FPU) { rx_mouse_update(m.r[0], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x008150b0, XenonInputManager_vf43, "rx_loop", OUT_FPU) { rx_input_update(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00813de0, KeyMaps_Update, "rx_loop", OUT_FPU) { rx_keymaps_update(m.r[7], f32(m.arg(0))); return m.ret(4); }

// ---- test hook (development only): a synthetic XInput pad 0 for the pad code's verification ----
// The reference worlds run without pads (the world test passes none), so Pad_Update never reaches its state path. With
// REFLEX_PADSIM=<seed> and this hook in the filter (REFLEX_PORT=...,rx_PadSimHook, for the reference run alone too), the first
// XInputGetCapabilities call replaces the emulator's XInput imports (both the original and the native phase see them): pad 0 is
// connected and vibration-capable; its state is a function of the game clock (frame timing +0x3c, so the verified phases of one
// call see the same state): buttons but Start / Back, triggers around the 30 threshold, sticks across the dead zone and centred;
// XInputSetState succeeds. Group "thunk": never verified, returns to the import thunk (RUN_ORIGINAL).
namespace {
uint32_t g_padsim_seed = 1;
uint32_t padsim_hash(uint32_t x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x; }
}
RX_PORT(0x008ea364, PadSimHook, "thunk", 0) {
    static bool done = false;
    if (!done && std::getenv("REFLEX_PADSIM")) {
        done = true;
        g_padsim_seed = uint32_t(std::strtoul(std::getenv("REFLEX_PADSIM"), nullptr, 10)) * 2654435761u + 1;
        m.imports["xinput1_3.dll!#4"] = [](reflex_vm::Machine& mm) {          // XInputGetCapabilities (user, flags, caps)
            const uint32_t i = mm.arg(0), out = mm.arg(2);
            if (i != 0) return mm.ret(12, 1167);
            const uint8_t caps[20] = {1, 1, 0, 0, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xc0, 0xff, 0xc0, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff};
            mm.mem.write_bytes(out, caps, sizeof caps);
            return mm.ret(12, 0);
        };
        m.imports["xinput1_3.dll!#2"] = [](reflex_vm::Machine& mm) {          // XInputGetState (user, state)
            const uint32_t i = mm.arg(0), out = mm.arg(1);
            if (i != 0) return mm.ret(8, 1167);
            const uint32_t timing = uint32_t(mm.mem.read(uint32_t(mm.mem.read(0x00a95844, 4)) + 0x10, 4));
            const uint32_t clock = uint32_t(mm.mem.read(timing + 0x3c, 4));
            const uint32_t k = padsim_hash(clock ^ g_padsim_seed), slow = padsim_hash((clock >> 12) ^ g_padsim_seed);
            const uint16_t buttons = uint16_t(slow & 0xffcf);                       // no Start (0x10) / Back (0x20)
            const uint8_t lt = uint8_t((slow >> 8) % 3 == 0 ? 0 : k), rt = uint8_t(28 + (k >> 8) % 5);
            mm.mem.write(out, 4, clock); mm.mem.write(out + 4, 2, buttons); mm.mem.write(out + 6, 1, lt); mm.mem.write(out + 7, 1, rt);
            for (uint32_t a = 0; a < 4; a++) {
                const uint32_t r = padsim_hash(slow + a);
                mm.mem.write(out + 8 + 2 * a, 2, uint16_t(r % 4 == 0 ? 0 : (r >> 16)));
            }
            return mm.ret(8, 0);
        };
        m.imports["xinput1_3.dll!#3"] = [](reflex_vm::Machine& mm) { return mm.ret(8, mm.arg(0) == 0 ? 0 : 1167); };   // XInputSetState
    }
    return reflex_vm::RUN_ORIGINAL;
}
