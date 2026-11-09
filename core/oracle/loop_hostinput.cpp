// loop_hostinput.cpp -- oracle adapters and the differential check of the product's host input / clock layer
// (core/game/loop_hostinput.c, notes/input-clock.md). Group "rx_inputclock".
//
// What is compared (native/game/tools/verify.sh style, Verify mode: the native runs first on the entry state, then the original on the same state,
// memory writes and registers are compared):
//  * XenonInputManager::vf43 008150b0: the native reads the emulator's keyboard and pad as the host would hand them to rx_session_set_input,
//    serves the game's platform calls (DirectInput GetDeviceState, XInputGetCapabilities / GetState / SetState, GetCursorPos) from the host layer,
//    and runs the game's poll (rx_input_update). The original runs the same poll on the emulator's imports. Every record the poll writes must match.
//  * Engine::UpdateTiming 0084ea80: the same with the performance counter / frequency served by the host clock.
//  * MXWndProc 00880dc0 (WM_INPUT): the host's raw mouse path (the ported window procedure and Mouse_AccumulateRawInput) against the original window
//    procedure, both reading the same RAWINPUT through the emulator's GetRawInputData.
//  * rx_FrameStartHook (group thunk, at Engine::Update 0084ed20, runs once per frame outside every verified call): a synthetic mouse (motion, wheel,
//    buttons) posted through MXWndProc (that call IS the verified one) and a jittered frame time (the emulator's counter read n times: 100 ticks each).
// RX_INPUTCLOCK=1 enables the check (tools: builds/inputclock/verify_inputclock.sh). With REFLEX_PADSIM=<seed> and rx_PadSimHook (loop_input.cpp) the pad 0 changes every frame; REFLEX_DRIVE moves the keyboard.
#include "oracle.h"
#include <cstring>
#include <cstdlib>
extern "C" {
#include "../game/loop_hostinput.h"
#include "../game/loop_input.h"
#include "../game/loop_engine.h"
}
using namespace reflex_port;
using rx_oracle::f32;

namespace {
// The differential check (host-layer poll / timing, the frame-start hook) runs only with RX_INPUTCLOCK=1: with another filter ("all") the
// adapters at the rx_loop addresses behave like rx_loop's and the hook does nothing.
bool active() { static const bool a = std::getenv("RX_INPUTCLOCK") != nullptr; return a; }
uint8_t g_raw[40];
bool g_imports = false;
uint32_t g_frame = 0, g_buttons = 0;
uint32_t hash(uint32_t x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x; }

void install_raw_imports(reflex_vm::Machine& m) {
    // the window procedure's two OS calls (as native/drafts/world_runtime/input.cpp): GetRawInputData asks the size, then copies the record
    m.imports["user32.dll!GetRawInputData"] = [](reflex_vm::Machine& mm) {
        const uint32_t cmd = mm.arg(1), data = mm.arg(2), psize = mm.arg(3), header = mm.arg(4);
        if (cmd != 0x10000003u || header != 16 || !psize) return mm.ret(20, 0xffffffffu);
        if (!data) { mm.mem.write(psize, 4, sizeof g_raw); return mm.ret(20, 0); }
        if (mm.mem.read(psize, 4) < sizeof g_raw) return mm.ret(20, 0xffffffffu);
        mm.mem.write_bytes(data, g_raw, sizeof g_raw);
        return mm.ret(20, uint32_t(sizeof g_raw));
    };
    m.imports["user32.dll!DefWindowProcA"] = [](reflex_vm::Machine& mm) { return mm.ret(16, 0); };
    // the window procedure's other messages (RX_INPUTCLOCK_WM): the cursor clip and the window rectangles of a 1280 x 720 client area
    m.imports["user32.dll!ClipCursor"] = [](reflex_vm::Machine& mm) { return mm.ret(4, 1); };
    m.imports["user32.dll!ClientToScreen"] = [](reflex_vm::Machine& mm) { mm.mem.write(mm.arg(1), 4, 8); mm.mem.write(mm.arg(1) + 4, 4, 31); return mm.ret(8, 1); };
    auto rect = [](reflex_vm::Machine& mm) { const uint32_t v[4] = {0, 0, 1280, 720}; mm.mem.write_bytes(mm.arg(1), v, 16); return mm.ret(8, 1); };
    m.imports["user32.dll!GetClientRect"] = rect;
    m.imports["user32.dll!GetWindowRect"] = rect;
    m.imports["user32.dll!GetWindowLongA"] = [](reflex_vm::Machine& mm) { return mm.ret(8, 0x16cf0000u); };
}
}  // namespace

// stdcall (hwnd, msg, wparam, lparam); ret 0x10; result EAX. Every message branch is ported (WM_INPUT, the focus / size / syscommand / close messages).
RX_PORT(0x00880dc0, MXWndProc, "rx_inputclock", OUT_EAX) {
    m.r[0] = rx_wndproc(m.arg(0), m.arg(1), m.arg(2), m.arg(3));
    if (const char* br = std::getenv("RX_INPUTCLOCK_BREAK"))   // negative control of the check: an extra pixel of motion
        if (std::strcmp(br, "mouse") == 0) { const uint32_t im = rx_rd32(rx_rd32(0x00a95844) + 0x34); rx_wr32(im + 0x3234 + 0xfc, rx_rd32(im + 0x3234 + 0xfc) + 1); }
    return m.ret(0x10);
}
RX_PORT(0x00880d30, Window_StoreRects, "rx_inputclock", OUT_EAX) { m.r[0] = rx_window_store_rects(); return m.ret(0); }
RX_PORT(0x00880d60, Window_ClipCursor, "rx_inputclock", 0) { rx_window_clip_cursor(); return m.ret(0); }
RX_PORT(0x0084f230, InputManager_FocusNotify, "rx_inputclock", 0) { rx_input_manager_focus_notify(); return m.ret(0); }
RX_PORT(0x008150b0, XenonInputManager_vf43_host, "rx_inputclock", OUT_FPU) {
    if (active()) rx_host_check_poll(m.r[1], f32(m.arg(0))); else rx_input_update(m.r[1], f32(m.arg(0)));
    return m.ret(4);
}
RX_PORT(0x0084ea80, Engine_UpdateTiming_host, "rx_inputclock", OUT_FPU) {
    if (active()) rx_host_check_timing(m.r[6]); else rx_engine_update_timing(m.r[6]);
    return m.ret(0);
}
RX_PORT(0x0084e070, Engine_vf0, "rx_inputclock", OUT_FPU) { rx_engine_set_frame_rate(m.r[1], f32(m.arg(0))); return m.ret(4); }   // (Engine::Startup only: not in the race worlds)
RX_PORT(0x00843a00, XenonCore_vf4, "rx_inputclock", OUT_EAX) { m.r[0] = rx_xenon_core_display(m.r[1]); return m.ret(0); }

RX_PORT(0x0084ed20, FrameStartHook, "thunk", 0) {
    if (!active()) return reflex_vm::RUN_ORIGINAL;
    if (!g_imports) { install_raw_imports(m); g_imports = true; }
    const uint32_t f = ++g_frame, h = hash(f * 2654435761u + 7);
    // a jittered frame time: the emulator's counter adds 100 ticks per read after the first of a frame (some frames run long: the 0.1 s clamp)
    const uint32_t extra = ((h >> 20) % 16 == 0) ? (h % 24000) : (h % 9);
    const uint32_t scratch = m.r[4] - 0x1000;
    for (uint32_t i = 0; i < extra; i++) { const uint32_t a[1] = {scratch}; rx_call_original(rx_rd32(0x00916098), RX_STDCALL, 0, 1, a); }
    // a synthetic mouse event: motion every frame, a wheel notch now and then, a button edge now and then
    const uint32_t h2 = hash(h + 1), h3 = hash(h + 2);
    const int32_t dx = int32_t(h2 % 161) - 80, dy = int32_t((h2 >> 12) % 161) - 80;
    const int32_t wheel = (h3 % 9 == 0) ? ((h3 >> 8) & 1 ? 120 : -120) : 0;
    uint32_t flags = 0;
    if ((h3 >> 16) % 5 == 0) {
        const uint32_t b = (h3 >> 20) % 5;
        const bool down = !((g_buttons >> b) & 1);
        g_buttons ^= 1u << b;
        flags = down ? (1u << (2 * b)) : (1u << (2 * b + 1));
    }
    uint32_t bf = flags | (wheel ? 0x400u : 0u);
    std::memset(g_raw, 0, sizeof g_raw);
    const uint32_t size = sizeof g_raw;
    std::memcpy(g_raw + 4, &size, 4);
    const uint16_t flags16 = uint16_t(bf), data = uint16_t(int16_t(wheel));
    std::memcpy(g_raw + 20, &flags16, 2); std::memcpy(g_raw + 22, &data, 2);
    std::memcpy(g_raw + 28, &dx, 4); std::memcpy(g_raw + 32, &dy, 4);
    const uint32_t a[4] = {rx_rd32(0x00a95984), 0xff, 0, 0x52415749u};
    rx_call_original(0x00880dc0, RX_STDCALL, 0, 4, a);
    // RX_INPUTCLOCK_WM=1: the other window messages through the verified procedure, a few per frame, with the windowed flag [00a95970] set around the
    // call on alternate frames (the branches that clip the cursor). WM_CLOSE / WM_DESTROY / WM_QUIT quit the game: only with RX_INPUTCLOCK_WM=close.
    if (const char* wm = std::getenv("RX_INPUTCLOCK_WM")) {
        static const uint32_t msgs[] = {0x07, 0x08, 0x05, 0x05, 0x05, 0x105, 0x105, 0x112, 0x112, 0x232, 0x7e, 0x0f, 0x24, 0x105, 0x112, 0x112, 0x112};
        static const uint32_t wps[] = {0, 0, 0, 1, 2, 0x12, 0x10, 0xf030, 0xf170, 0, 0, 0, 0, 0x12, 0xf100, 0xf000, 0xf010};
        const uint32_t k = h3 % (sizeof msgs / sizeof msgs[0]);
        const uint32_t old = rx_rd32(0x00a95970);
        if (f & 1) rx_wr32(0x00a95970, 1);
        const uint32_t b[4] = {rx_rd32(0x00a95984), msgs[k], wps[k], 0};
        rx_call_original(0x00880dc0, RX_STDCALL, 0, 4, b);
        if (std::strcmp(wm, "close") == 0 && f == 40) { const uint32_t c[4] = {rx_rd32(0x00a95984), 0x10, 0, 0}; rx_call_original(0x00880dc0, RX_STDCALL, 0, 4, c); }
        rx_wr32(0x00a95970, old);
    }
    return reflex_vm::RUN_ORIGINAL;
}
