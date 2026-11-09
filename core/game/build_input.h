/* build_input.h -- construction unit 12b: the engine's input manager (XenonInputManager), built by Engine::Startup.
 *
 * Engine::Startup 0084e320 calls XenonCore vf6 (00843a10) right before InputNames_Init and the KeyBind loads: it allocates the
 * manager (0x33a4 bytes, ReflexAlignedAlloc 0078fbe0) and constructs it (008145b0), storing it at the engine context's +0x34
 * (XenonCore vf2 008439f0 returns it). The manager (vtable 0x00974e0c, loop_input.c's queries):
 *   +0x000 vtable  +0x004 4 pad records (0x2dc each; +0x29c connected, +0x2a0 connected / +0x2a4 lost this frame, +0x2b8
 *          vibration enabled, +0x2c0 rumble capable, +0x2d8 on, +0x2dc.. the next pad)
 *   +0xb74 4 keyboard records (0x9b0 each; +0x990 device acquired, +0x994 a key down, +0x998 / +0x99c enabled, +0x9a8
 *          modifiers, +0x9ac IDirectInputDevice8)
 *   +0x3234 mouse (0x148: +0xf4 enabled; branch records and sums used by loop_input.c)
 *   +0x337c std::string (empty after the constructor), +0x3398 / +0x3399 bytes 1, +0x339a byte 0, +0x339c 10
 *   +0x33a0 IDirectInput8, +0x3378 idle time (float), +0x3328 = mouse +0xf4 = 1
 *
 * Platform boundaries (marked in build_input.c): DirectInput8Create and the device COM calls exist only in the oracle build (the
 * emulator's stand-ins, native/game/vm boot_devices.inc). The product has no DirectInput: the object stays 0 and keyboard 0 takes
 * the original's "CreateDevice failed" path (+0x9ac / +0x990 0); the keyboard state itself comes from rx_loop_host.keyboard_state
 * through loop_dinput_get_device_state (loop_platform.c ignores the device in the product), i.e. the host (Unity) feeds it. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

rx_addr rx_xenon_core_vf6(rx_addr self);                 /* 00843a10 XenonCore::vf6: alloc + construct, self + 0x34 = it (EAX) */
rx_addr rx_xenon_input_manager_construct(rx_addr self);  /* 008145b0 XenonInputManager_Construct (stdcall self; RET 4) */

/* the record constructors and leaves (custom register conventions documented at each definition) */
rx_addr rx_pad_record_construct(rx_addr rec);            /* 00814710 (ECX = pad record) */
rx_addr rx_keyboard_record_construct(rx_addr rec);       /* 00814740 (ECX = keyboard record): Keyboard_InitScanTable */
rx_addr rx_pad_state_clear(rx_addr rec);                 /* 00816870 (EAX = pad record; EAX out = record + 4) */
void    rx_keyboard_create(rx_addr kb, rx_addr di);      /* 00817090 (EDI = keyboard record; IDirectInput8 on the stack) */
rx_addr rx_mouse_init(rx_addr mouse);                    /* 008172f0 (ESI = mouse; EAX = mouse) */
rx_addr rx_mouse_clear(rx_addr mouse);                   /* 00817be0 (EAX = mouse; EAX out = mouse + 4) */
uint32_t rx_keyboard_init_scan_table(rx_addr dev);       /* 00816ba0 Keyboard_InitScanTable (EDI; build_globals_engine.c) */

#ifdef __cplusplus
}
#endif
