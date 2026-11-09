/* loop_platform.h -- the operating-system services the frame code calls (the original's imports: XInput, DirectInput, the
 * window's cursor; later the performance counter and thread events). Oracle build: the original import / COM method runs (the
 * emulator's stand-in), so Verify sees the same calls in the same order. Product: the host (Unity, through the API) supplies the
 * device state in rx_loop_host; a missing callback means "no device" (the call fails as on a PC without one). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    /* XInputGetCapabilities / XInputGetState of a user index: 0 = ERROR_SUCCESS (out: XINPUT_CAPABILITIES 0x14 bytes /
     * XINPUT_STATE 0x10 bytes), else not connected. XInputSetState: motor speeds. */
    uint32_t (*xinput_get_capabilities)(uint32_t user, uint32_t flags, uint8_t caps[0x14]);
    uint32_t (*xinput_get_state)(uint32_t user, uint8_t state[0x10]);
    uint32_t (*xinput_set_state)(uint32_t user, uint16_t left, uint16_t right);
    /* the keyboard's 256 DirectInput key bytes (0x80 = down); < 0 = failed (the game retries after an Acquire) */
    int32_t (*keyboard_state)(uint8_t keys[256]);
    /* the cursor in client-area pixels and the client area's size */
    void (*cursor)(int32_t* x, int32_t* y);
    /* the performance counter and its frequency (QueryPerformanceCounter / Frequency); missing: a 10 MHz counter advanced by
     * 1/60 s per frame-start read (the emulator's virtual clock) */
    uint64_t (*qpc)(void);
    uint64_t (*qpf)(void);
    /* GetTickCount / timeGetTime: milliseconds of the same clock, read without advancing it (missing: qpc / qpf, or the 10 MHz counter) */
    uint32_t (*tick_ms)(void);
} rx_loop_platform;
extern rx_loop_platform rx_loop_host;

uint32_t loop_xinput_get_capabilities(uint32_t user, uint32_t flags, rx_addr caps);   /* XINPUT1_3 ordinal 4 (thunk 008ea364) */
uint32_t loop_xinput_get_state(uint32_t user, rx_addr state);                         /* ordinal 2 (thunk 008ea36a) */
uint32_t loop_xinput_set_state(uint32_t user, rx_addr vibration);                     /* ordinal 3 (thunk 008ea370): XINPUT_VIBRATION (2 words) */
int32_t  loop_dinput_get_device_state(rx_addr device, uint32_t size, rx_addr out);    /* IDirectInputDevice8::GetDeviceState (+0x24) */
int32_t  loop_dinput_acquire(rx_addr device);                                         /* IDirectInputDevice8::Acquire (+0x1c) */
uint32_t loop_get_cursor_pos(rx_addr point);                                          /* USER32 GetCursorPos (IAT 009164ac) */
uint32_t loop_get_client_rect(uint32_t hwnd, rx_addr rect);                           /* USER32 GetClientRect (IAT 009164b0) */
uint32_t loop_client_to_screen(uint32_t hwnd, rx_addr point);                         /* USER32 ClientToScreen (IAT 00916470) */

/* Time: QueryPerformanceCounter / QueryPerformanceFrequency into a LARGE_INTEGER at a game address. */
uint32_t loop_qpc(rx_addr out);
uint32_t loop_qpf(rx_addr out);
/* GetTickCount (IAT 0091601c): milliseconds of the same clock as loop_qpc (its 10 MHz count / 10000), read without advancing it
 * (the emulator's GetTickCount returns now / 10000 and, unlike its QPC, does not add 100 per read). */
uint32_t loop_get_tick_count(void);

/* Threads and synchronization. The original runs the scene tickers, the job pool workers, the engine task thread and the rider
 * animation workers on threads woken by SetEvent and waited for with WaitForSingleObject. The core runs them synchronously in the
 * original's order (the emulator serializes them the same way: SetEvent on a worker's "go" event runs the worker until it blocks).
 *  loop_wake_worker(go, body, arg): oracle: SetEvent(go) -- the emulated worker runs (its thread entry's RX_PORT calls body);
 *                                   product: body(arg) now.
 *  loop_wait_event(h):              oracle: WaitForSingleObject(h, INFINITE) (a serialized worker already signalled it); product: nothing.
 *  loop_set_event(h):               oracle: SetEvent(h) (a worker's "done" event); product: 1.
 *  critical sections:               oracle: the imports (host bookkeeping); product: nothing (one thread). */
void     loop_wake_worker(uint32_t go, void (*body)(rx_addr), rx_addr arg);
uint32_t loop_wait_event(uint32_t h);
uint32_t loop_wait_event_timeout(uint32_t h, uint32_t ms);   /* oracle: WaitForSingleObject(h, ms); product: 0x102 (timeout) */
uint32_t loop_set_event(uint32_t h);
uint32_t loop_reset_event(uint32_t h);
uint32_t loop_get_last_error(void);
void     loop_enter_cs(rx_addr cs);
void     loop_leave_cs(rx_addr cs);
void     loop_sleep(uint32_t ms);
/* GetCurrentThreadId (IAT 0091607c): oracle the import; product the one game thread's id (1) */
uint32_t loop_get_current_thread_id(void);
#ifdef __cplusplus
}
#endif
