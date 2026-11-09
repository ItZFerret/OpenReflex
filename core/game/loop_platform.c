/* loop_platform.c -- the frame code's operating-system services (loop_platform.h). */
#include "loop_platform.h"
#include "loop_hostinput.h"
#include "build_alloc.h"
#include "loop_jobs.h"

rx_loop_platform rx_loop_host;

/* The host-served versions of the input / clock calls (the product's platform layer: rx_loop_host, loop_hostinput.c). The oracle
 * build uses them only in rx_loop_host_mode (the differential check of the host layer against the emulator's imports). */
static void put_bytes(rx_addr a, const uint8_t* p, uint32_t n) { for (uint32_t i = 0; i < n; i++) SU8(a, i, p[i]); }
static uint32_t host_xinput_get_capabilities(uint32_t user, uint32_t flags, rx_addr caps) {
    uint8_t b[0x14] = {0};
    if (!rx_loop_host.xinput_get_capabilities) return 0x48f;   /* ERROR_DEVICE_NOT_CONNECTED */
    uint32_t r = rx_loop_host.xinput_get_capabilities(user, flags, b);
    if (r == 0) put_bytes(caps, b, sizeof b);
    return r;
}
static uint32_t host_xinput_get_state(uint32_t user, rx_addr state) {
    uint8_t b[0x10] = {0};
    if (!rx_loop_host.xinput_get_state) return 0x48f;
    uint32_t r = rx_loop_host.xinput_get_state(user, b);
    if (r == 0) put_bytes(state, b, sizeof b);
    return r;
}
static uint32_t host_xinput_set_state(uint32_t user, rx_addr vibration) {
    if (!rx_loop_host.xinput_set_state) return 0x48f;
    return rx_loop_host.xinput_set_state(user, U16(vibration, 0), U16(vibration, 2));
}
static int32_t host_dinput_get_device_state(rx_addr device, uint32_t size, rx_addr out) {
    (void)device;
    uint8_t keys[256] = {0};
    if (!rx_loop_host.keyboard_state || size != 256) return (int32_t)0x8007001e;   /* DIERR_NOTACQUIRED-like failure */
    int32_t r = rx_loop_host.keyboard_state(keys);
    if (r >= 0) put_bytes(out, keys, 256);
    return r;
}
static uint32_t host_get_cursor_pos(rx_addr point) {
    int32_t x = 0, y = 0;
    if (rx_loop_host.cursor) rx_loop_host.cursor(&x, &y);
    SU32(point, 0, (uint32_t)x); SU32(point, 4, (uint32_t)y);
    return 1;
}
static uint64_t g_clock;   /* no host clock: a 10 MHz counter that advances 10 us per read (the frame cap's busy wait ends) */
static uint32_t host_qpc(rx_addr out) {
    uint64_t t = rx_loop_host.qpc ? rx_loop_host.qpc() : (g_clock += 100);
    rx_wr64(out, t);
    return 1;
}
static uint32_t host_qpf(rx_addr out) { rx_wr64(out, rx_loop_host.qpf ? rx_loop_host.qpf() : 10000000ull); return 1; }
static uint32_t host_get_tick_count(void) {
    if (rx_loop_host.tick_ms) return rx_loop_host.tick_ms();   /* read without advancing the counter */
    const uint64_t f = rx_loop_host.qpf ? rx_loop_host.qpf() : 10000000ull, t = rx_loop_host.qpc ? rx_loop_host.qpc() : g_clock;
    return (uint32_t)(t / (f / 1000u));   /* ms of the QPC clock */
}

#ifdef RX_ORACLE
static uint32_t stdcall_import(rx_addr iat_slot, int n, const uint32_t* args) {
    return rx_call_original(U32(0, iat_slot), RX_STDCALL, 0, n, args);
}
int rx_loop_host_mode;   /* development only (loop_hostinput.h): serve the input / clock calls from rx_loop_host */
uint32_t loop_xinput_get_capabilities(uint32_t user, uint32_t flags, rx_addr caps) {
    if (rx_loop_host_mode) return host_xinput_get_capabilities(user, flags, caps);
    const uint32_t a[3] = {user, flags, caps};
    return rx_call_original(0x008ea364, RX_STDCALL, 0, 3, a);
}
uint32_t loop_xinput_get_state(uint32_t user, rx_addr state) {
    if (rx_loop_host_mode) return host_xinput_get_state(user, state);
    const uint32_t a[2] = {user, state};
    return rx_call_original(0x008ea36a, RX_STDCALL, 0, 2, a);
}
uint32_t loop_xinput_set_state(uint32_t user, rx_addr vibration) {
    if (rx_loop_host_mode) return host_xinput_set_state(user, vibration);
    const uint32_t a[2] = {user, vibration};
    return rx_call_original(0x008ea370, RX_STDCALL, 0, 2, a);
}
int32_t loop_dinput_get_device_state(rx_addr device, uint32_t size, rx_addr out) {
    if (rx_loop_host_mode) return host_dinput_get_device_state(device, size, out);
    const uint32_t a[3] = {device, size, out};
    return (int32_t)rx_call_original(U32(U32(device, 0), 0x24), RX_STDCALL, 0, 3, a);
}
int32_t loop_dinput_acquire(rx_addr device) {
    if (rx_loop_host_mode) return 0;
    const uint32_t a[1] = {device};
    return (int32_t)rx_call_original(U32(U32(device, 0), 0x1c), RX_STDCALL, 0, 1, a);
}
uint32_t loop_get_cursor_pos(rx_addr point) {
    if (rx_loop_host_mode) return host_get_cursor_pos(point);
    const uint32_t a[1] = {point}; return stdcall_import(0x009164ac, 1, a);
}
uint32_t loop_get_client_rect(uint32_t hwnd, rx_addr rect) { const uint32_t a[2] = {hwnd, rect}; return stdcall_import(0x009164b0, 2, a); }
uint32_t loop_client_to_screen(uint32_t hwnd, rx_addr point) { const uint32_t a[2] = {hwnd, point}; return stdcall_import(0x00916470, 2, a); }
uint32_t loop_qpc(rx_addr out) { if (rx_loop_host_mode) return host_qpc(out); const uint32_t a[1] = {out}; return stdcall_import(0x00916098, 1, a); }
uint32_t loop_qpf(rx_addr out) { if (rx_loop_host_mode) return host_qpf(out); const uint32_t a[1] = {out}; return stdcall_import(0x009160a0, 1, a); }
uint32_t loop_get_tick_count(void) { if (rx_loop_host_mode) return host_get_tick_count(); return stdcall_import(0x0091601c, 0, 0); }
void loop_wake_worker(uint32_t go, void (*body)(rx_addr), rx_addr arg) {
    (void)body; (void)arg;
    if (!loop_set_event(go)) loop_get_last_error();
}
uint32_t loop_wait_event(uint32_t h) { const uint32_t a[2] = {h, 0xffffffffu}; return stdcall_import(0x00916008, 2, a); }
uint32_t loop_wait_event_timeout(uint32_t h, uint32_t ms) { const uint32_t a[2] = {h, ms}; return stdcall_import(0x00916008, 2, a); }
uint32_t loop_set_event(uint32_t h) { const uint32_t a[1] = {h}; return stdcall_import(0x00916034, 1, a); }
uint32_t loop_reset_event(uint32_t h) { const uint32_t a[1] = {h}; return stdcall_import(0x00916090, 1, a); }
uint32_t loop_get_last_error(void) { return stdcall_import(0x00916078, 0, 0); }
void loop_enter_cs(rx_addr cs) { const uint32_t a[1] = {cs}; stdcall_import(0x0091600c, 1, a); }
void loop_leave_cs(rx_addr cs) { const uint32_t a[1] = {cs}; stdcall_import(0x009160f8, 1, a); }
void loop_sleep(uint32_t ms) { const uint32_t a[1] = {ms}; stdcall_import(0x00916020, 1, a); }
uint32_t loop_get_current_thread_id(void) { return stdcall_import(0x0091607c, 0, 0); }
#else
uint32_t loop_xinput_get_capabilities(uint32_t user, uint32_t flags, rx_addr caps) { return host_xinput_get_capabilities(user, flags, caps); }
uint32_t loop_xinput_get_state(uint32_t user, rx_addr state) { return host_xinput_get_state(user, state); }
uint32_t loop_xinput_set_state(uint32_t user, rx_addr vibration) { return host_xinput_set_state(user, vibration); }
int32_t loop_dinput_get_device_state(rx_addr device, uint32_t size, rx_addr out) { return host_dinput_get_device_state(device, size, out); }
int32_t loop_dinput_acquire(rx_addr device) { (void)device; return 0; }
uint32_t loop_get_cursor_pos(rx_addr point) { return host_get_cursor_pos(point); }
uint32_t loop_get_client_rect(uint32_t hwnd, rx_addr rect) { (void)hwnd; (void)rect; return 1; }
uint32_t loop_client_to_screen(uint32_t hwnd, rx_addr point) { (void)hwnd; (void)point; return 1; }   /* client = screen origin */
uint32_t loop_qpc(rx_addr out) { return host_qpc(out); }
uint32_t loop_qpf(rx_addr out) { return host_qpf(out); }
uint32_t loop_get_tick_count(void) { return host_get_tick_count(); }
/* ---- the thread's static TLS block: the part of a thread the Windows loader makes ----
 * fs:[0x2c] (the TEB at 0x7ffd0000, fx_util.h FX_TEB) -> the thread's TLS pointer array -> array[_tls_index] = the thread's copy
 * of MXReflex.exe's .tls raw data (the TLS directory: start 0x9d0000, end 0x9d0810, index address 0x00dcf74c, raw bytes all
 * zero). The original's worker threads each get their own array and block from the loader; the core runs a woken worker in place
 * (loop_platform.h), so loop_wake_worker points fs:[0x2c] at that worker's array for the body and restores the caller's after
 * (the emulator's runtime switches TEB + 0x2c at every thread switch the same way). Reached by the FX particle job's own
 * thread-local scratch (006925f0: [fs:[0x2c]] + 8 = the job, + 4 = the particle index, fx_particle.c). */
#define LOOP_TLS_INDEX  0x00dcf74cu
#define LOOP_TLS_SIZE   0x810u
static struct { uint32_t key; rx_addr array; } g_tls[64];   /* (w-rc-core) 32 before: up to 32 rider workers + the others */
static int g_tls_n;
static rx_addr tls_array(uint32_t key) {
    for (int i = 0; i < g_tls_n; i++) if (g_tls[i].key == key) return g_tls[i].array;
    if (g_tls_n >= 64) return 0;
    /* the loader's array: 64 pointers; the exe's block: _tls_index slot, raw copy (zero), the zero-fill tail */
    const rx_addr array = rx_crt_aligned_malloc(64 * 4, 16);
    const rx_addr block = rx_crt_aligned_malloc(LOOP_TLS_SIZE + 16, 16);
    if (!array || !block) return 0;
    SU32(array + 4 * U32(0, LOOP_TLS_INDEX), 0, block);
    g_tls[g_tls_n].key = key; g_tls[g_tls_n].array = array;
    return g_tls[g_tls_n++].array;
}
void loop_wake_worker(uint32_t go, void (*body)(rx_addr), rx_addr arg) {
    const rx_addr teb = rx_teb();              /* the waking thread's (the game thread's 0x7ffd0000) */
    const rx_addr main_array = tls_array(0);   /* the game (main) thread's, made first */
    const rx_addr prev = U32(teb, 0x2c);
    const rx_addr mine = tls_array(go);        /* the worker's (created on its first wake, kept for the next) */
    if (mine) SU32(teb, 0x2c, mine);
    body(arg);
    SU32(teb, 0x2c, prev ? prev : main_array);
}
/* the same arrays and the same fs:[0x2c] afterwards, for a job that runs as its own thread (loop_jobs.h) */
rx_addr loop_worker_tls(uint32_t go) {
    const rx_addr teb = rx_teb();
    const rx_addr main_array = tls_array(0);
    const rx_addr prev = U32(teb, 0x2c);
    const rx_addr mine = tls_array(go);
    SU32(teb, 0x2c, prev ? prev : main_array);
    return mine ? mine : (prev ? prev : main_array);
}
uint32_t loop_wait_event(uint32_t h) { (void)h; return 0; }
uint32_t loop_wait_event_timeout(uint32_t h, uint32_t ms) { (void)h; (void)ms; return 0x102; }
uint32_t loop_set_event(uint32_t h) { (void)h; return 1; }
uint32_t loop_reset_event(uint32_t h) { (void)h; return 1; }
uint32_t loop_get_last_error(void) { return 0; }
void loop_enter_cs(rx_addr cs) { rx_jobs_cs_enter_key(cs); }   /* a parallel job's (loop_jobs.h); else nothing */
void loop_leave_cs(rx_addr cs) { rx_jobs_cs_leave_key(cs); }
void loop_sleep(uint32_t ms) { (void)ms; }
uint32_t loop_get_current_thread_id(void) { return 1; }
#endif
