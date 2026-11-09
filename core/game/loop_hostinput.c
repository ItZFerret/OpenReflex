/* loop_hostinput.c -- the product's platform input and clock, and the mouse's WM_INPUT path (loop_hostinput.h).
 *
 * Device boundaries, where the original's operating system sits (disassembly checked):
 *   keyboard  Keyboard_Poll 00816ef0 -> IDirectInputDevice8::GetDeviceState(256)  -> rx_loop_host.keyboard_state
 *   pads      Pad_Update 008157a0    -> XInputGetCapabilities / XInputGetState   -> rx_loop_host.xinput_get_capabilities / _get_state
 *             Pad_UpdateVibration 008169f0 -> XInputSetState                      -> rx_loop_host.xinput_set_state
 *   mouse     Mouse_UpdateCursor 00817400 -> GetCursorPos                        -> rx_loop_host.cursor
 *             MXWndProc 00880dc0 WM_INPUT -> GetRawInputData -> Mouse_AccumulateRawInput 00817cb0 (ported here)
 *   clock     Engine::UpdateTiming 0084ea80 -> QueryPerformanceCounter / Frequency -> rx_loop_host.qpc / qpf
 *             GetTickCount (package cache stamps, the random seed) and timeGetTime -> the same clock in ms
 * The host layer only serves those calls from the state the host last gave it; every record is written by the game's own code. */
#include "loop_hostinput.h"
#include "loop_platform.h"
#include "loop_util.h"
#include "loop_engine.h"
#include "loop_input.h"
#include "build_alloc.h"
#include "../src/rx_os.h"

#include <string.h>
#ifdef RX_ORACLE
#include <stdlib.h>
#endif

/* ---- device state ---- */

static struct {
    uint8_t keys[256];
    rx_host_pad pads[4], last[4];
    uint8_t have_last[4];
    uint32_t pad_mask, packet[4];
    int32_t cx, cy;
    uint16_t motor_l[4], motor_r[4];
    uint32_t motor_calls[4];
} g_in;

/* XINPUT_CAPABILITIES of the connected pad: Type 1 (gamepad), SubType 1, Flags 0, the gamepad mask (buttons, triggers, sticks) and
 * the vibration motors' range. The same bytes the oracle's emulator reports (native/drafts/world_runtime/runtime.cpp): the game only
 * reads the two vibration words (Pad_UpdateVibration) and stores the rest in the pad record (+0x27c). */
static const uint8_t k_caps[0x14] = {1, 1, 0, 0, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xc0, 0xff, 0xc0, 0xff, 0xc0, 0xff,
                                     0xff, 0xff, 0xff, 0xff};

static uint32_t h_get_capabilities(uint32_t user, uint32_t flags, uint8_t caps[0x14]) {
    (void)flags;
    if (user >= 4 || !(g_in.pad_mask & (1u << user))) return 0x48f;   /* ERROR_DEVICE_NOT_CONNECTED */
    memcpy(caps, k_caps, sizeof k_caps);
    return 0;
}

static uint32_t h_get_state(uint32_t user, uint8_t state[0x10]) {
    if (user >= 4 || !(g_in.pad_mask & (1u << user))) return 0x48f;
    const rx_host_pad* p = &g_in.pads[user];
    if (!g_in.have_last[user] || memcmp(&g_in.last[user], p, sizeof *p) != 0) { g_in.packet[user]++; g_in.last[user] = *p; g_in.have_last[user] = 1; }
    const uint32_t pk = g_in.packet[user];
    memcpy(state + 0, &pk, 4);
    memcpy(state + 4, &p->buttons, 2);
    state[6] = p->left_trigger;
    state[7] = p->right_trigger;
    memcpy(state + 8, &p->lx, 2);
    memcpy(state + 10, &p->ly, 2);
    memcpy(state + 12, &p->rx, 2);
    memcpy(state + 14, &p->ry, 2);
    return 0;
}

static uint32_t h_set_state(uint32_t user, uint16_t left, uint16_t right) {
    if (user >= 4 || !(g_in.pad_mask & (1u << user))) return 0x48f;
    g_in.motor_l[user] = left;
    g_in.motor_r[user] = right;
    g_in.motor_calls[user]++;
    return 0;
}

static int32_t h_keyboard(uint8_t keys[256]) { memcpy(keys, g_in.keys, 256); return 0; }
static void h_cursor(int32_t* x, int32_t* y) { *x = g_in.cx; *y = g_in.cy; }

/* ---- clock ---- */

static struct { uint64_t now; uint32_t reads; } g_clock;

static uint64_t h_qpc(void) {
    if (g_clock.reads++ > 0) g_clock.now += 100;   /* a busy wait's reads take time */
    return g_clock.now;
}
static uint64_t h_qpf(void) { return RX_HOST_QPF; }
static uint32_t h_tick_ms(void) { return rx_host_clock_ms(); }

void rx_host_clock_advance(uint64_t ticks) { g_clock.now += ticks; g_clock.reads = 0; }
void rx_host_clock_set(uint64_t ticks) { g_clock.now = ticks; g_clock.reads = 0; }
uint64_t rx_host_clock_ticks(void) { return g_clock.now; }
uint32_t rx_host_clock_ms(void) { return (uint32_t)(g_clock.now / (RX_HOST_QPF / 1000u)); }

/* ---- public ---- */

void rx_host_input_install(void) {
    rx_loop_host.xinput_get_capabilities = h_get_capabilities;
    rx_loop_host.xinput_get_state = h_get_state;
    rx_loop_host.xinput_set_state = h_set_state;
    rx_loop_host.keyboard_state = h_keyboard;
    rx_loop_host.cursor = h_cursor;
    rx_loop_host.qpc = h_qpc;
    rx_loop_host.qpf = h_qpf;
    rx_loop_host.tick_ms = h_tick_ms;
}

void rx_host_input_set(const uint8_t* keys, const rx_host_pad* pads, uint32_t pad_mask) {
    for (int i = 0; i < 256; i++) g_in.keys[i] = keys ? (uint8_t)(keys[i] & 0x80) : 0;   /* DirectInput: the high bit is the state */
    for (int i = 0; i < 4; i++) {
        if (pads) g_in.pads[i] = pads[i]; else memset(&g_in.pads[i], 0, sizeof g_in.pads[i]);
    }
    g_in.pad_mask = pad_mask & 0xfu;
}

void rx_host_input_cursor(int32_t x, int32_t y) { g_in.cx = x; g_in.cy = y; }

uint32_t rx_host_input_rumble(uint32_t pad, uint16_t* left, uint16_t* right) {
    if (pad >= 4) return 0;
    if (left) *left = g_in.motor_l[pad];
    if (right) *right = g_in.motor_r[pad];
    return g_in.motor_calls[pad];
}

/* 00843a00 XenonCore::vf4 (= vf22; ECX = the engine context; plain ret): the display settings block (+0x3c), which Mouse_UpdateCursor reads the
 * viewport rectangle from. */
uint32_t rx_xenon_core_display(rx_addr self) { return self + 0x3c; }
RX_FUNCTION(0x00843a00, rx_xenon_core_display)

/* ---- the mouse: WM_INPUT -> MXWndProc -> Mouse_AccumulateRawInput ---- */

/* 00817cb0 Mouse_AccumulateRawInput (EAX = mouse = manager + 0x3234, ECX = the raw record {+0 button flags (RAWMOUSE usButtonFlags),
 * +4 dx, +8 dy, +0xc wheel word}; plain ret): the button flag pairs (down bit 2i, up bit 2i + 1 for the five buttons) go into the held
 * bits +0xf8 (set by a down, cleared by an up, in that order: an up wins) and the pressed / released sums +0x118 / +0x11c; the motion
 * sums +0xfc / +0x100 and the wheel sum (word +0x104) grow. Mouse_Update consumes and clears the sums once per frame. */
void rx_mouse_accumulate_raw_input(rx_addr mouse, rx_addr rec) {
    const uint32_t f = U32(rec, 0);
    uint32_t down = 0, up = 0;
    for (uint32_t i = 0; i < 5; i++) {
        down |= ((f >> (2 * i)) & 1u) << i;
        up |= ((f >> (2 * i + 1)) & 1u) << i;
    }
    SU32(mouse, 0xf8, (U32(mouse, 0xf8) | down) & ~up);
    SU32(mouse, 0x118, U32(mouse, 0x118) | down);
    SU32(mouse, 0x11c, U32(mouse, 0x11c) | up);
    SU32(mouse, 0xfc, U32(mouse, 0xfc) + U32(rec, 4));
    SU32(mouse, 0x100, U32(mouse, 0x100) + U32(rec, 8));
    SU16(mouse, 0x104, U16(mouse, 0x104) + U16(rec, 0xc));
}
RX_FUNCTION(0x00817cb0, rx_mouse_accumulate_raw_input)

/* ---- the window procedure ---------------------------------------------------------------------------------------------------- */

/* The OS calls of the window procedure and of its two helpers. The oracle runs the emulator's imports through their IAT slots
 * (ClipCursor 0x00916430, DefWindowProcA 0x00916438, GetWindowLongA 0x00916440, GetRawInputData 0x00916448, ClientToScreen 0x00916470,
 * GetWindowRect 0x009164a0, GetClientRect 0x009164b0). The product has no window ([0x00a95984] stays 0) and no cursor to clip: the
 * window calls fail without writing (GetClientRect / GetWindowRect / ClientToScreen leave their rectangle alone, GetWindowLongA of no
 * window is 0, ClipCursor has no effect) and the default procedure does nothing; the one thing it serves is the raw mouse record the
 * host posted (rx_host_mouse_post). */
#define RAW_SIZE 40u   /* sizeof(RAWINPUT) of a mouse record: RAWINPUTHEADER 16 + RAWMOUSE 24 */
#ifndef RX_ORACLE
static uint8_t g_raw[RAW_SIZE];

static uint32_t os_get_raw_input_data(uint32_t handle, uint32_t cmd, rx_addr data, rx_addr psize, uint32_t header) {
    (void)handle;
    if (cmd != 0x10000003u || header != 16 || !psize) return 0xffffffffu;
    if (!data) { SU32(psize, 0, RAW_SIZE); return 0; }
    if (U32(psize, 0) < RAW_SIZE) return 0xffffffffu;
    for (uint32_t i = 0; i < RAW_SIZE; i++) SU8(data, i, g_raw[i]);
    return RAW_SIZE;
}
static uint32_t os_def_window_proc(uint32_t hwnd, uint32_t msg, uint32_t wparam, uint32_t lparam) {
    (void)hwnd; (void)msg; (void)wparam; (void)lparam;
    return 0;   /* no window: the default procedure has no effect */
}
static void os_clip_cursor(rx_addr rect) { (void)rect; }
static uint32_t os_get_window_long(uint32_t hwnd, int32_t index) { (void)hwnd; (void)index; return 0; }
static uint32_t os_client_to_screen(uint32_t hwnd, rx_addr point) { (void)hwnd; (void)point; return 0; }
static uint32_t os_get_client_rect(uint32_t hwnd, rx_addr rect) { (void)hwnd; (void)rect; return 0; }
static uint32_t os_get_window_rect(uint32_t hwnd, rx_addr rect) { (void)hwnd; (void)rect; return 0; }
#else
static uint32_t os_import(uint32_t slot, int n, const uint32_t* a) { return rx_call_original(U32(0, slot), RX_STDCALL, 0, n, a); }
static uint32_t os_get_raw_input_data(uint32_t handle, uint32_t cmd, rx_addr data, rx_addr psize, uint32_t header) {
    const uint32_t a[5] = {handle, cmd, data, psize, header};
    return os_import(0x00916448, 5, a);
}
static uint32_t os_def_window_proc(uint32_t hwnd, uint32_t msg, uint32_t wparam, uint32_t lparam) {
    const uint32_t a[4] = {hwnd, msg, wparam, lparam};
    return os_import(0x00916438, 4, a);
}
static void os_clip_cursor(rx_addr rect) { const uint32_t a[1] = {rect}; (void)os_import(0x00916430, 1, a); }
static uint32_t os_get_window_long(uint32_t hwnd, int32_t index) { const uint32_t a[2] = {hwnd, (uint32_t)index}; return os_import(0x00916440, 2, a); }
static uint32_t os_client_to_screen(uint32_t hwnd, rx_addr point) { const uint32_t a[2] = {hwnd, point}; return os_import(0x00916470, 2, a); }
static uint32_t os_get_client_rect(uint32_t hwnd, rx_addr rect) { const uint32_t a[2] = {hwnd, rect}; return os_import(0x009164b0, 2, a); }
static uint32_t os_get_window_rect(uint32_t hwnd, rx_addr rect) { const uint32_t a[2] = {hwnd, rect}; return os_import(0x009164a0, 2, a); }
#endif

/* the window state the procedure keeps (globals of the original) */
#define WND_QUIT_REQUESTED  0x00a95964u   /* set by WM_CLOSE / WM_DESTROY / WM_QUIT */
#define WND_STYLE           0x00a95968u   /* GWL_STYLE (-16) of the window, kept at WM_SIZE while windowed */
#define WND_FOCUSED         0x00a9596cu   /* WM_SETFOCUS 1 / WM_KILLFOCUS 0 */
#define WND_WINDOWED        0x00a95970u   /* windowed mode */
#define WND_MINIMIZED       0x00a95974u   /* WM_SIZE SIZE_MINIMIZED (wparam 1) */
#define WND_MAXIMIZED       0x00a95978u   /* WM_SIZE SIZE_MAXIMIZED (wparam 2) */
#define WND_RECLIP_PENDING  0x00a9597cu   /* the cursor was released with Alt: re-clip at the next mouse button */
#define WND_HANDLE          0x00a95984u   /* g_hWnd */
#define WND_CLIENT_RECT     0x00a89ba0u
#define WND_WINDOW_RECT     0x00a89bb0u
#define WND_FLAG_7E         0x00a956f9u   /* WM_DISPLAYCHANGE (0x7e) seen (byte) */

/* 00880d30 Window_StoreRects (cdecl, no arguments): GetWindowRect / GetClientRect of the window into the two stored rectangles. EAX = 0. */
uint32_t rx_window_store_rects(void) {
    (void)os_get_window_rect(U32(0, WND_HANDLE), WND_WINDOW_RECT);
    (void)os_get_client_rect(U32(0, WND_HANDLE), WND_CLIENT_RECT);
    return 0;
}
RX_FUNCTION(0x00880d30, rx_window_store_rects)

/* 00880d60 Window_ClipCursor (cdecl, no arguments): ClipCursor to the client rectangle of the window in screen coordinates. Frame: sub esp, 0x1c;
 * the point at +0, the rectangle at +8 {left, top, right, bottom}. */
void rx_window_clip_cursor(void) {
    const rx_addr sp0 = rx_sp();
    const rx_addr F = sp0 - 0x1c, point = F, rect = F + 8;
    rx_set_sp(F);
    SU32(point, 0, 0);
    SU32(point, 4, 0);
    (void)os_client_to_screen(U32(0, WND_HANDLE), point);
    (void)os_get_client_rect(U32(0, WND_HANDLE), rect);
    const uint32_t x = U32(point, 0), y = U32(point, 4);
    SU32(rect, 0, U32(rect, 0) + x);                        /* left   */
    SU32(rect, 8, U32(rect, 8) + x);                        /* right  */
    SU32(rect, 4, U32(rect, 4) + y);                        /* top    */
    SU32(rect, 0xc, U32(rect, 0xc) + y);                    /* bottom */
    os_clip_cursor(rect);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00880d60, rx_window_clip_cursor)

/* 0084f230 (cdecl, no arguments): tells the input manager (the engine context's slot 2) that the window's focus changed: its vf42 (+0xa8), a
 * tail call, when there is one. */
void rx_input_manager_focus_notify(void) {
    const rx_addr ctx = U32(0, 0x00a95844);
    if (loop_tc0(loop_vslot(ctx, 8), ctx) == 0) return;     /* the input manager (context slot 2) */
    const rx_addr im = loop_tc0(loop_vslot(ctx, 8), ctx);
    loop_tc0r(loop_vslot(im, 0xa8), im, 0x00880e2c);        /* jmp [vtable + 0xa8]: the callee returns to the window procedure */
}
RX_FUNCTION(0x0084f230, rx_input_manager_focus_notify)

/* the window messages handled below (winuser.h) */
enum {
    WM_DESTROY = 0x02, WM_SIZE = 0x05, WM_SETFOCUS = 0x07, WM_KILLFOCUS = 0x08, WM_CLOSE = 0x10, WM_QUIT = 0x12,
    WM_DISPLAYCHANGE = 0x7e, WM_INPUT = 0xff, WM_SYSKEYUP = 0x105, WM_SYSCOMMAND = 0x112, WM_EXITSIZEMOVE = 0x232
};

/* WM_SETFOCUS / WM_KILLFOCUS tail 00880e11: the input manager hears of the focus change when the engine context has a window object (+0x10). */
static void wnd_notify_focus(void) {
    const rx_addr ctx = U32(0, 0x00a95844);
    if (ctx != 0 && U32(ctx, 0x10) != 0) {
        const rx_addr s = loop_push(0, 0, 0x00880e2c);
        rx_input_manager_focus_notify();
        loop_pop(s);
    }
}

/* 00880dc0 MXWndProc (stdcall hwnd, msg, wparam, lparam; ret 0x10) -> LRESULT. Frame: push ebp; and esp, -8; sub esp, 0x14; push ebx / esi / edi:
 * the WM_INPUT locals are the size at A - 0x14 and the record {flags, dx, dy, wheel word} at A - 0x10 (A = the aligned frame top).
 *  - WM_DESTROY / WM_CLOSE / WM_QUIT: quit requested [00a95964] = 1; the window object's (engine context +0x10, its +8) vf2 is told; 0.
 *  - WM_SIZE: windowed with a window: [00a95968] = GWL_STYLE; the minimized [00a95974] / maximized [00a95978] flags follow wparam
 *    (SIZE_MINIMIZED 1, SIZE_MAXIMIZED 2); the stored rectangles are refreshed when maximizing and when a restore leaves either state.
 *  - WM_SETFOCUS / WM_KILLFOCUS: windowed: re-clip / release the cursor; [00a9596c] = 1 / 0; the input manager is told.
 *  - WM_DISPLAYCHANGE: [00a956f9] = 1, result 1.
 *  - WM_INPUT: GetRawInputData asks the size (the buffer [00da68c8] with its size [00da68cc] is reallocated when it changed), reads the
 *    record; a mouse record (header type 0) becomes {flags, lLastX, lLastY, wheel (usButtonData only with RI_MOUSE_WHEEL 0x400)} and goes
 *    to the input manager's mouse (the manager = vslot 2 of [00a95960], mouse = +0x3234) through Mouse_AccumulateRawInput; a record with
 *    button flags clears the pending cursor clip ([00a9597c], Window_ClipCursor). Then the default window procedure answers.
 *  - WM_SYSKEYUP: Alt (wparam 0x12) while windowed releases the cursor clip and arms the re-clip [00a9597c].
 *  - WM_SYSCOMMAND: SC_SIZE 0xf000, SC_MOVE 0xf010, SC_MAXIMIZE 0xf030, SC_KEYMENU 0xf100 and SC_MONITORPOWER 0xf170 are swallowed (result 1).
 *  - WM_EXITSIZEMOVE: the rectangles are stored again; windowed: re-clip.
 * Everything else goes to the default window procedure. */
uint32_t rx_wndproc(uint32_t hwnd, uint32_t msg, uint32_t wparam, uint32_t lparam) {
    const rx_addr sp0 = rx_sp();
    const rx_addr A = (sp0 - 4u) & ~7u;                     /* push ebp; and esp, -8 */
    const rx_addr F = A - 0x14u - 12u;                      /* sub esp, 0x14; push ebx, esi, edi */
    rx_set_sp(F);
    const rx_addr size = A - 0x14u, rec = A - 0x10u;
    const uint32_t windowed = U32(0, WND_WINDOWED);
    uint32_t result = 0;

    if (msg == WM_QUIT || msg == WM_DESTROY || msg == WM_CLOSE) {   /* 00880edd */
        SU32(0, WND_QUIT_REQUESTED, 1);
        const rx_addr ctx = U32(0, 0x00a95844);
        if (ctx != 0 && U32(ctx, 0x10) != 0) {
            const rx_addr object = U32(U32(ctx, 0x10), 8);
            const rx_addr s = loop_push(0, 0, 0x00880f05);
            (void)loop_tc0(loop_vslot(object, 8), object);
            loop_pop(s);
        }
        rx_set_sp(sp0);
        return 0;
    }
    if (msg == WM_SIZE) {                                   /* 00880e4a */
        if (windowed != 0 && U32(0, WND_HANDLE) != 0) {
            SU32(0, WND_STYLE, os_get_window_long(U32(0, WND_HANDLE), -0x10));
        }
        if (wparam == 1) {                                  /* SIZE_MINIMIZED */
            SU32(0, WND_MINIMIZED, 1);
            SU32(0, WND_MAXIMIZED, 0);
        } else if (wparam == 2) {                           /* SIZE_MAXIMIZED */
            SU32(0, WND_MINIMIZED, 0);
            SU32(0, WND_MAXIMIZED, 1);
            const rx_addr s = loop_push(0, 0, 0x00880e9c);
            (void)rx_window_store_rects();
            loop_pop(s);
        } else if (wparam == 0) {                           /* SIZE_RESTORED */
            if (U32(0, WND_MAXIMIZED) != 0) {
                SU32(0, WND_MAXIMIZED, 0);
                const rx_addr s = loop_push(0, 0, 0x00880ebc);
                (void)rx_window_store_rects();
                loop_pop(s);
            } else if (U32(0, WND_MINIMIZED) != 0) {
                SU32(0, WND_MINIMIZED, 0);
                const rx_addr s = loop_push(0, 0, 0x00880ed8);
                (void)rx_window_store_rects();
                loop_pop(s);
            }
        }
    } else if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS) {  /* 00880df8 / 00880e31 */
        if (msg == WM_SETFOCUS) {
            if (windowed != 0) {
                const rx_addr s = loop_push(0, 0, 0x00880e07);
                rx_window_clip_cursor();
                loop_pop(s);
            }
            SU32(0, WND_FOCUSED, 1);
        } else {
            if (windowed != 0) os_clip_cursor(0);
            SU32(0, WND_FOCUSED, 0);
        }
        wnd_notify_focus();
    } else if (msg == WM_DISPLAYCHANGE) {                   /* 0088104e */
        SU8(0, WND_FLAG_7E, 1);
        rx_set_sp(sp0);
        return 1;
    } else if (msg == WM_SYSKEYUP) {                        /* 00881061 */
        if (wparam == 0x12 && windowed != 0) {
            os_clip_cursor(0);
            SU32(0, WND_RECLIP_PENDING, 1);
        }
    } else if (msg == WM_SYSCOMMAND) {                      /* 008810a9 */
        if (wparam == 0xf030 || wparam == 0xf000 || wparam == 0xf010 || wparam == 0xf100 || wparam == 0xf170) {
            rx_set_sp(sp0);
            return 1;
        }
    } else if (msg == WM_EXITSIZEMOVE) {                    /* 00881094 */
        const rx_addr s = loop_push(0, 0, 0x00881099);
        (void)rx_window_store_rects();
        loop_pop(s);
        if (windowed != 0) {
            const rx_addr s2 = loop_push(0, 0, 0x008810a7);
            rx_window_clip_cursor();
            loop_pop(s2);
        }
    } else if (msg == WM_INPUT) {                           /* 00880f37 */
        os_get_raw_input_data(lparam, 0x10000003u, 0, size, 0x10);
        const uint32_t need = U32(size, 0);
        rx_addr buf;
        if (U32(0, 0x00da68cc) != need) {
            if (U32(0, 0x00da68c8) != 0) { rx_reflex_aligned_free(U32(0, 0x00da68c8)); SU32(0, 0x00da68c8, 0); }
            SU32(0, 0x00da68cc, need);
            buf = rx_reflex_aligned_alloc(need);
            SU32(0, 0x00da68c8, buf);
        } else {
            buf = U32(0, 0x00da68c8);
        }
        if (buf == 0) { rx_set_sp(sp0); return 0; }
        os_get_raw_input_data(lparam, 0x10000003u, buf, size, 0x10);
        const rx_addr raw = U32(0, 0x00da68c8);
        if (U32(raw, 0) == 0) {   /* RIM_TYPEMOUSE */
            const rx_addr disp = U32(0, 0x00a95960);
            const rx_addr im = loop_tc0(loop_vslot(disp, 8), disp);
            SU32(rec, 0, 0); SU32(rec, 4, 0); SU32(rec, 8, 0); SU16(rec, 0xc, 0);
            if (im != 0) {
                const uint32_t flags = U16(raw, 0x14);
                SU32(rec, 0, flags);
                SU32(rec, 4, U32(raw, 0x1c));
                SU32(rec, 8, U32(raw, 0x20));
                if (flags & 0x400) SU16(rec, 0xc, U16(raw, 0x16));
                if (flags != 0 && U32(0, WND_RECLIP_PENDING) != 0) {
                    const rx_addr s = loop_push(0, 0, 0x0088101a);
                    rx_window_clip_cursor();
                    loop_pop(s);
                    SU32(0, WND_RECLIP_PENDING, 0);
                }
                rx_mouse_accumulate_raw_input(im + 0x3234, rec);
            }
        }
    }
    /* 00881032: the default window procedure answers */
    {
        const uint32_t args[4] = {hwnd, msg, wparam, lparam};
        const rx_addr s = loop_push(4, args, 0x00881045);
        result = os_def_window_proc(hwnd, msg, wparam, lparam);
        loop_pop(s);
    }
    rx_set_sp(sp0);
    return result;
}
RX_FUNCTION(0x00880dc0, rx_wndproc)

uint32_t rx_host_mouse_post(int32_t dx, int32_t dy, int32_t wheel, uint32_t button_flags) {
#ifndef RX_ORACLE
    uint32_t flags = button_flags & 0x3ffu;
    if (wheel) flags |= 0x400u;
    memset(g_raw, 0, sizeof g_raw);
    const uint32_t type = 0, size = RAW_SIZE;                 /* RAWINPUTHEADER: RIM_TYPEMOUSE, dwSize */
    const uint16_t usFlags = 0, data = (uint16_t)(int16_t)wheel;   /* MOUSE_MOVE_RELATIVE; usButtonData = the wheel delta */
    const uint16_t bf = (uint16_t)flags;
    memcpy(g_raw + 0, &type, 4); memcpy(g_raw + 4, &size, 4);
    memcpy(g_raw + 16, &usFlags, 2); memcpy(g_raw + 20, &bf, 2); memcpy(g_raw + 22, &data, 2);
    memcpy(g_raw + 28, &dx, 4); memcpy(g_raw + 32, &dy, 4);
    return rx_wndproc(U32(0, 0x00a95984), 0xff, 0, 0x52415749u);   /* hwnd = g_hWnd; the HRAWINPUT is only passed back */
#else
    (void)dx; (void)dy; (void)wheel; (void)button_flags;
    return 0;   /* the oracle has no host raw record: its message source is the emulator's GetRawInputData stand-in */
#endif
}

/* 0084e070 Engine::vf0 (ECX = engine; stack fps; ret 4): the frame cap +0x14 = 1 / fps seconds (Engine::Startup passes 60.0). */
void rx_engine_set_frame_rate(rx_addr engine, float fps) { SF32(engine, 0x14, 1.0f /* 0x931f24 */ / fps); }
RX_FUNCTION(0x0084e070, rx_engine_set_frame_rate)

/* The head of Engine::Update 0084ed20 (its first calls, before the scenes' pre-tick): Engine::UpdateTiming (the frame's dt), the input
 * manager's device poll (context slot 2 -> XenonInputManager::vf43 with that dt), then the key maps' update (00d92f50). rx_engine_update
 * itself runs these and then the scenes, MXGame::Update and the rest; this is the part of the frame the host's input and clock reach,
 * for a caller that runs MXGame::Update itself while the scene / job tail of the frame is not ported (api/rx_session.c). */
void rx_engine_frame_head(rx_addr engine) {
    rx_engine_update_timing(engine);
    const rx_addr im = loop_input_manager();
    const float dt = F32(engine, 0x18);
    loop_tc_f(loop_vslot(im, 0xac), im, dt);
    rx_keymaps_update(0x00d92f50, dt);
}

#ifdef RX_ORACLE
void rx_host_check_poll(rx_addr im, float dt) {
    RX_FRAME_ENTER(F, 0x140);
    const rx_addr keys = F, caps = F + 0x100, state = F + 0x120;
    uint8_t k[256];
    memset(k, 0, sizeof k);
    if (loop_dinput_get_device_state(U32(im + 0xb74, 0x9ac), 256, keys) >= 0)
        for (uint32_t i = 0; i < 256; i++) k[i] = U8(keys, i);
    rx_host_pad pads[4];
    memset(pads, 0, sizeof pads);
    uint32_t mask = 0;
    for (uint32_t u = 0; u < 4; u++) {
        if (loop_xinput_get_capabilities(u, 1, caps) != 0 || loop_xinput_get_state(u, state) != 0) continue;
        mask |= 1u << u;
        pads[u].buttons = U16(state, 4);
        pads[u].left_trigger = U8(state, 6); pads[u].right_trigger = U8(state, 7);
        pads[u].lx = (int16_t)U16(state, 8); pads[u].ly = (int16_t)U16(state, 10);
        pads[u].rx = (int16_t)U16(state, 12); pads[u].ry = (int16_t)U16(state, 14);
    }
    RX_FRAME_LEAVE(F, 0x140);
    {   /* negative controls of the check itself: RX_INPUTCLOCK_BREAK=keys|pad perturbs what the host layer hands over; the check must report it */
        const char* br = getenv("RX_INPUTCLOCK_BREAK");
        if (br && strcmp(br, "keys") == 0) k[0x11] ^= 0x80;
        if (br && strcmp(br, "pad") == 0 && (mask & 1)) pads[0].lx = (int16_t)(pads[0].lx + 0x2000);
    }
    rx_host_input_install();
    rx_host_input_set(k, pads, mask);
    rx_loop_host_mode = 1;
    rx_input_update(im, dt);
    rx_loop_host_mode = 0;
}

void rx_host_check_timing(rx_addr engine) {
    RX_FRAME_ENTER(F, 0x10);
    loop_qpc(F);                       /* the emulator's counter: this read is the frame's first */
    const uint64_t t = rx_rd64(F);
    RX_FRAME_LEAVE(F, 0x10);
    rx_host_input_install();
    { const char* br = getenv("RX_INPUTCLOCK_BREAK"); rx_host_clock_set(br && strcmp(br, "clock") == 0 ? t + 1000 : t); }
    rx_loop_host_mode = 1;
    rx_engine_update_timing(engine);
    rx_loop_host_mode = 0;
}
#endif
