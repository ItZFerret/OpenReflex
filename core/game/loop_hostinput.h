/* loop_hostinput.h -- the PRODUCT's platform input and clock (see core/notes/input-clock.md).
 *
 * The original reads its devices through the OS at fixed points of Engine::Update: DirectInput GetDeviceState (the keyboard,
 * Keyboard_Poll 00816ef0), XInputGetCapabilities / XInputGetState (the pads, Pad_Update 008157a0), GetCursorPos (Mouse_UpdateCursor), and the
 * raw mouse input as WM_INPUT messages through MXWndProc 00880dc0 (Mouse_AccumulateRawInput 00817cb0), and its clock through
 * QueryPerformanceCounter / Frequency (Engine::UpdateTiming 0084ea80) and timeGetTime. In the product the host (Unity, through
 * rx_session_set_input / rx_session_advance_dt) is the operating system: this file keeps the host's device state and clock and serves
 * those calls through rx_loop_host (loop_platform.h). The game's own code does all the rest: the dead zones, the edges, the records.
 * Nothing here writes a game record.
 *
 * Compiled in both builds. The oracle build uses it only when rx_loop_host_mode is set (the differential input / clock check,
 * oracle/loop_hostinput.cpp); otherwise the oracle's emulator stand-ins serve those calls as before. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

/* One XInput gamepad as XINPUT_GAMEPAD (12 bytes, the same layout as rx_api.h's rx_pad). */
typedef struct rx_host_pad { uint16_t buttons; uint8_t left_trigger, right_trigger; int16_t lx, ly, rx, ry; } rx_host_pad;

/* Installs the callbacks into rx_loop_host (keyboard, pads, cursor, QPC / QPF, tick). Call after every session create. */
void rx_host_input_install(void);

/* The device state the next poll sees: keys = 256 DirectInput keyboard bytes (0x80 = down; NULL = none), pads = 4 XInput pads (NULL =
 * none), pad_mask = bit per connected pad. A pad's dwPacketNumber counts its state changes (as XInput does). */
void rx_host_input_set(const uint8_t* keys, const rx_host_pad* pads, uint32_t pad_mask);
void rx_host_input_cursor(int32_t x, int32_t y);
/* the last motor speeds the game wrote through XInputSetState (Pad_UpdateVibration) and the number of calls, per pad */
uint32_t rx_host_input_rumble(uint32_t pad, uint16_t* left, uint16_t* right);

/* A raw mouse event (WM_INPUT, RIM_TYPEMOUSE) as the Windows message pump would deliver it between two frames: relative motion,
 * wheel delta (WHEEL_DELTA = 120 per notch) and RAWMOUSE usButtonFlags (bit 0 left down, 1 left up, 2 right down, 3 right up,
 * 4 middle down, 5 middle up, 6 / 7 button 4, 8 / 9 button 5). It runs the game's window procedure (rx_wndproc, MXWndProc 00880dc0), which
 * sums it into the mouse record. Call between frames (the frame consumes the sums). Returns the window procedure's result. */
uint32_t rx_host_mouse_post(int32_t dx, int32_t dy, int32_t wheel, uint32_t button_flags);

/* The performance counter: 10 MHz ticks. A frame start moves it on by the real time that passed (rx_host_clock_advance); the first read
 * of a frame returns that value and every further read 100 ticks (10 us) more: the time a busy wait (UpdateTiming's frame cap) takes
 * to read the counter. rx_host_clock_ms is the same clock in milliseconds, read without advancing it. */
void     rx_host_clock_advance(uint64_t ticks);
void     rx_host_clock_set(uint64_t ticks);   /* absolute value; the next read returns it (tests) */
uint64_t rx_host_clock_ticks(void);
uint32_t rx_host_clock_ms(void);
#define RX_HOST_QPF 10000000ull

/* 00880dc0 MXWndProc (stdcall hwnd, msg, wparam, lparam; ret 0x10): the game window's procedure, every message branch (the product has no window: it
 * only receives the WM_INPUT mouse records above). 00880d30 Window_StoreRects, 00880d60 Window_ClipCursor, 0084f230 (the input manager hears of
 * a focus change) are its helpers. */
uint32_t rx_wndproc(uint32_t hwnd, uint32_t msg, uint32_t wparam, uint32_t lparam);
uint32_t rx_window_store_rects(void);
void rx_window_clip_cursor(void);
void rx_input_manager_focus_notify(void);
/* 0084e070 Engine::vf0: the frame cap = 1 / fps */
void rx_engine_set_frame_rate(rx_addr engine, float fps);
/* the head of Engine::Update 0084ed20: UpdateTiming, the input device poll (vf43), the key maps (see loop_hostinput.c) */
void rx_engine_frame_head(rx_addr engine);
/* 00843a00 XenonCore::vf4 / vf22 (ECX = the engine context): the display settings block, context + 0x3c */
uint32_t rx_xenon_core_display(rx_addr self);
/* 00817cb0 Mouse_AccumulateRawInput (EAX = mouse = manager + 0x3234, ECX = record {flags, dx, dy, wheel word}) */
void rx_mouse_accumulate_raw_input(rx_addr mouse, rx_addr rec);

#ifdef RX_ORACLE
/* development only: nonzero = the oracle's loop_platform calls go through rx_loop_host (this file) instead of the emulator's imports */
extern int rx_loop_host_mode;
/* The differential check of oracle/loop_hostinput.cpp (development only; the oracle runs the emulator's devices and clock):
 * rx_host_check_poll polls the input manager through the host layer: it reads the emulator's keyboard and pads (the platform calls in
 * the original mode), gives them to the host layer as rx_session_set_input would, and runs XenonInputManager::vf43 (rx_input_update) with the
 * platform calls served by the host layer. rx_host_check_timing does the same for Engine::UpdateTiming: the host clock continues at the
 * emulator's counter. */
void rx_host_check_poll(rx_addr im, float dt);
void rx_host_check_timing(rx_addr engine);
#endif

#ifdef __cplusplus
}
#endif
