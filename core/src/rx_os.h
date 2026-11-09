/* rx_os.h -- the few Windows (kernel32) services the game's runtime code calls, for the PRODUCT build (no OS emulation: the core
 * runs the game single-threaded, so these are deterministic single-thread stand-ins). Callers keep the original's call through the
 * DLL's import slot in the ORACLE build (the emulator's stand-ins run there); in the product they call rx_os_call with the same
 * slot address, which maps it to the host implementation below. Slots are msvcr90's import slots (0x785210xx) and MXReflex.exe's kernel32 slots (0x009160xx); unknown slots abort
 * with the slot address (the list grows as callers need it). */
#pragma once
#include "rx_mem.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t rx_os_call(uint32_t iat_slot, int nargs, const uint32_t* args);
/* timeGetTime (winmm, 0x009164b8) is the only wall clock the product reads (world_node_audio.c 008b98c0 stores it): by default the host's
 * monotonic milliseconds, so two runs differ in those words. A host that needs reproducible runs (tests, replays) installs its own
 * millisecond source; NULL restores the default. Cleared by the statics restore at session destroy (rx_reset.h). */
void rx_os_set_time_source(uint32_t (*ms)(void));
/* A call of a system import (kernel32, winmm) through its import slot, from game code: the oracle runs the original import (the
 * emulator's), the product rx_os_call. RX_OS(slot, args...) is the variadic form (stdcall, as every kernel32 import). */
static inline uint32_t rx_os_import(uint32_t iat_slot, int nargs, const uint32_t* args) {
#ifdef RX_ORACLE
    return rx_call_original(rx_rd32(iat_slot), RX_STDCALL, 0, nargs, args);
#else
    return rx_os_call(iat_slot, nargs, args);
#endif
}
#define RX_OS_NARGS_(...) (int)(sizeof((uint32_t[]){__VA_ARGS__}) / sizeof(uint32_t))
#define RX_OS(slot, ...) rx_os_import((slot), RX_OS_NARGS_(__VA_ARGS__), (const uint32_t[]){__VA_ARGS__})
#define RX_OS0(slot) rx_os_import((slot), 0, (const uint32_t*)0)
#ifdef __cplusplus
}
#endif
