/* build_input.c -- unit 12b: the engine's input manager (XenonInputManager), created by Engine::Startup through XenonCore vf6
 * (00843a10, right before InputNames_Init / the KeyBind loads) and the records it builds. Layouts and the platform boundaries in
 * build_input.h. The original in one block (decompile 008145b0, disassembly checked for the field offsets and the call order):
 *
 *   vtable 0x00974e0c; 4 pad records at +4 (0x2dc, ctor 00814710); 4 keyboard records at +0xb74 (0x9b0, ctor 00814740 =
 *   Keyboard_InitScanTable); Mouse_Init (+0x3234); std::string ctor (+0x337c); DirectInput8Create into +0x33a0;
 *   string = ""; +0x3398 = +0x3399 = 1, +0x339a = 0, +0x339c = 10; 4 x {FUN_00816870, +0 = index, +0x2d8 = 1, +0x2c0 = 0,
 *   +0x2b8 = 1}; Keyboard_Create (the first keyboard, IDirectInput8 = +0x33a0); Mouse_Clear; mouse+0xf4 = 1; +0x3378 = 0.0f.
 *
 * The input queries the game makes of the manager (loop_input.c, world_node_input.c) are already ported; this file only builds it. */
#include "build_input.h"
#include "build_alloc.h"
#include "crt.h"

/* ---- DirectInput 8: the platform boundary ---------------------------------------------------------------
 * Oracle build: the original calls, so the emulator's DInput stand-ins run (native/drafts/world_runtime/boot_devices.inc: a
 * DirectInput8 object, CreateDevice -> a keyboard device, EnumObjects a no-op, GetDeviceState the host's keys). The call sites and
 * the IAT slots are the original's: GetModuleHandleA [0x009160e0], DirectInput8Create [0x00916000] (thunk 0x008fafd4), the COM
 * slots of IDirectInput8 (+0x0c CreateDevice) and IDirectInputDevice8 (+0x10 EnumObjects, +0x1c Acquire, +0x2c SetDataFormat,
 * +0x34 SetCooperativeLevel).
 * Product: there is no DirectInput. The DirectInput object stays 0, so rx_keyboard_create takes the original's CreateDevice-failure
 * path (device / acquired fields 0) and the device pointer is never dereferenced. The keyboard state is the host's, through
 * loop_dinput_get_device_state (loop_platform.c: product returns rx_loop_host.keyboard_state and ignores the device argument). */
static void dinput8_create(rx_addr out) {
#ifdef RX_ORACLE
    const uint32_t z[1] = {0};
    const uint32_t hinst = rx_call_original(U32(0, 0x009160e0), RX_STDCALL, 0, 1, z);   /* GetModuleHandleA(NULL) */
    const uint32_t a[5] = {hinst, 0x800, 0x0092af8c, out, 0};                          /* IID_IDirectInput8A */
    rx_call_original(U32(0, 0x00916000), RX_STDCALL, 0, 5, a);                         /* DirectInput8Create (result unused) */
#else
    SU32(out, 0, 0);   /* boundary: no DirectInput8 in the product (see above) */
#endif
}

/* 00817090 Keyboard_Create (EDI = the keyboard record = manager + 0xb74 + 0x9b0 * i; IDirectInput8 on the stack; RET 4): clear the
 * 0x990-byte device block, +0x990 / +0x994 = 0, then CreateDevice(GUID_SysKeyboard 0x0092afac, &record+0x9ac), +0x998 = 0,
 * +0x99c = 1 (enabled); on success SetDataFormat(c_dfDIKeyboard 0x0092af74), SetCooperativeLevel(g_hWnd 0x00a95984, 0x16),
 * Acquire, clear the key name table (0x00a81b28, 0x4000 bytes) and EnumObjects (callback 0x00817140). */
void rx_keyboard_create(rx_addr kb, rx_addr di) {
    SU32(kb, 0x990, 0);
    SU32(kb, 0x994, 0);
    crt_memset(kb, 0, 0x990);
    SU32(kb, 0x998, 0);
    SU32(kb, 0x99c, 1);
#ifdef RX_ORACLE
    if (di) {
        const uint32_t a[4] = {di, 0x0092afac, kb + 0x9ac, 0};
        if ((int32_t)rx_call_original(U32(U32(di, 0), 0x0c), RX_STDCALL, 0, 4, a) >= 0) {
            const rx_addr dev = U32(kb, 0x9ac);
            const uint32_t b[2] = {dev, 0x0092af74};
            rx_call_original(U32(U32(dev, 0), 0x2c), RX_STDCALL, 0, 2, b);
            const uint32_t c[3] = {dev, U32(0, 0x00a95984), 0x16};
            rx_call_original(U32(U32(dev, 0), 0x34), RX_STDCALL, 0, 3, c);
            const uint32_t d[1] = {dev};
            rx_call_original(U32(U32(dev, 0), 0x1c), RX_STDCALL, 0, 1, d);
            crt_memset(0x00a81b28, 0, 0x4000);
            const uint32_t e[4] = {dev, 0x00817140, kb, 0x0c};
            rx_call_original(U32(U32(dev, 0), 0x10), RX_STDCALL, 0, 4, e);
        }
    }
#else
    (void)di;   /* boundary: no device; +0x9ac / +0x990 stay 0 (the "CreateDevice failed" path) */
#endif
}
RX_FUNCTION(0x00817090, rx_keyboard_create)

/* 00817be0 Mouse_Clear (EAX = the mouse block = manager + 0x3234; EAX out = mouse + 4, memset's result): sums / edge bytes / last
 * frame's motion 0, the 0xf0-byte record array (+4) cleared. */
rx_addr rx_mouse_clear(rx_addr mouse) {
    SU32(mouse, 0x120, 0);
    SU8(mouse, 0x128, 0);
    SU8(mouse, 0x129, 0);
    SU32(mouse, 0x12c, 0);
    SU8(mouse, 0x130, 0);
    SU8(mouse, 0x131, 0);
    SU8(mouse, 0x132, 0);
    SU32(mouse, 0x118, 0);
    SU32(mouse, 0x11c, 0);
    SU32(mouse, 0x108, 0); SU32(mouse, 0x10c, 0);
    SU32(mouse, 0x110, 0); SU32(mouse, 0x114, 0);
    SU32(mouse, 0xf8, 0);  SU32(mouse, 0xfc, 0);
    SU32(mouse, 0x100, 0); SU32(mouse, 0x104, 0);
    return crt_memset(mouse + 4, 0, 0xf0);
}
RX_FUNCTION(0x00817be0, rx_mouse_clear)

/* 008172f0 Mouse_Init (ESI = the mouse block; EAX out = mouse): enabled +0xf4 = 0, the frame sums / axes 0, then Mouse_Clear. */
rx_addr rx_mouse_init(rx_addr mouse) {
    SU8(mouse, 0xf4, 0);
    SU32(mouse, 0xf8, 0);
    SU32(mouse, 0xfc, 0);
    SU32(mouse, 0x100, 0);
    SU16(mouse, 0x104, 0);
    SU32(mouse, 0x108, 0);
    SU32(mouse, 0x10c, 0);
    SU32(mouse, 0x110, 0);
    SU16(mouse, 0x114, 0);
    SU32(mouse, 0x118, 0);
    rx_mouse_clear(mouse);
    return mouse;
}
RX_FUNCTION(0x008172f0, rx_mouse_init)

/* 00816870 (EAX = a pad record; EAX out = record + 4, memset's result): the per-pad state block: the pressed / released words, the
 * stick and trigger edge flags, the gamepad copy +0x290 (8 bytes), +0x298, the vibration state (+0x2c8..+0x2d4, +0x2c4 pending =
 * 1) 0, +0x240 bytes +4 cleared. */
rx_addr rx_pad_state_clear(rx_addr rec) {
    SU16(rec, 0x246, 0);
    SU32(rec, 0x248, 0);
    SU32(rec, 0x24c, 0);
    SU32(rec, 0x250, 0);
    SU32(rec, 0x254, 0);
    SU32(rec, 0x258, 0);
    SU32(rec, 0x25c, 0);
    SU32(rec, 0x264, 0);
    SU32(rec, 0x268, 0);
    SU32(rec, 0x26c, 0);
    SU32(rec, 0x270, 0);
    SU32(rec, 0x274, 0);
    SU32(rec, 0x278, 0);
    SU16(rec, 0x260, 0);
    SU32(rec, 0x290, 0); SU32(rec, 0x294, 0);
    SU32(rec, 0x298, 0);
    SU32(rec, 0x2c8, 0);
    SU32(rec, 0x2cc, 0);
    SU32(rec, 0x2d0, 0);
    SU32(rec, 0x2d4, 0);
    SU32(rec, 0x2c4, 1);
    return crt_memset(rec + 4, 0, 0x240);
}
RX_FUNCTION(0x00816870, rx_pad_state_clear)

/* 00814710 Fn_00814710 (ECX = a pad record): +0, +0x2d8, +0x29c, +0x2a0, +0x2a4, +0x2bc = 0, then FUN_00816870. */
rx_addr rx_pad_record_construct(rx_addr rec) {
    SU32(rec, 0, 0);
    SU32(rec, 0x2d8, 0);
    SU32(rec, 0x29c, 0);
    SU32(rec, 0x2a0, 0);
    SU32(rec, 0x2a4, 0);
    SU32(rec, 0x2bc, 0);
    rx_pad_state_clear(rec);
    return rec;
}
RX_FUNCTION(0x00814710, rx_pad_record_construct)

/* 00814740 (ECX = a keyboard record): Keyboard_InitScanTable 00816ba0 (the scan code table 0x00a95848 / 0x00a85b28 on the first
 * record, the record's +0x9a8 / +0x9ac / +0x998 .. +0x9a4 cleared). build_globals_engine.c owns the port. */
rx_addr rx_keyboard_record_construct(rx_addr rec) {
    rx_keyboard_init_scan_table(rec);
    return rec;
}
RX_FUNCTION(0x00814740, rx_keyboard_record_construct)

/* 008145b0 XenonInputManager_Construct (stdcall self; RET 4; EAX = self) */
rx_addr rx_xenon_input_manager_construct(rx_addr self) {
    SU32(self, 0, 0x00974e0c);
    for (uint32_t i = 0; i < 4; i++) rx_pad_record_construct(self + 4 + 0x2dcu * i);
    for (uint32_t i = 0; i < 4; i++) rx_keyboard_record_construct(self + 0xb74 + 0x9b0u * i);
    rx_mouse_init(self + 0x3234);
    crt_str_ctor(self + 0x337c);                 /* std::basic_string() 0x78485e6a */
    dinput8_create(self + 0x33a0);               /* boundary (see above) */
    crt_str_assign(self + 0x337c, 0x00935d6c);   /* operator=("") 0x78486428; 0x935d6c is the empty literal in the exe's .rdata */
    SU8(self, 0x3398, 1);
    SU8(self, 0x3399, 1);
    SU8(self, 0x339a, 0);
    SU32(self, 0x339c, 10);
    for (uint32_t i = 0; i < 4; i++) {
        rx_addr rec = self + 4 + 0x2dcu * i;
        rx_pad_state_clear(rec);
        SU32(rec, 0, i);            /* the XInput user index */
        SU32(rec, 0x2d8, 1);        /* pad on */
        SU32(rec, 0x2c0, 0);        /* rumble capable (set by Pad_Update's capabilities) */
        SU32(rec, 0x2b8, 1);        /* vibration enabled */
    }
    rx_keyboard_create(self + 0xb74, U32(self, 0x33a0));
    rx_mouse_clear(self + 0x3234);
    SU8(self, 0x3328, 1);                       /* mouse + 0xf4: enabled */
    SF32(self, 0x3378, 0.0f);                   /* idle time */
    return self;
}
RX_FUNCTION(0x008145b0, rx_xenon_input_manager_construct)

/* 00843a10 XenonCore::vf6 (ECX = the engine context): ReflexAlignedAlloc(0x33a4); construct it and store it at +0x34 (0 when the
 * allocation fails: EAX 0). Returns the manager (EAX). */
rx_addr rx_xenon_core_vf6(rx_addr self) {
    rx_addr im = rx_reflex_aligned_alloc(0x33a4);
    if (im) im = rx_xenon_input_manager_construct(im);
    SU32(self, 0x34, im);
    return im;
}
RX_FUNCTION(0x00843a10, rx_xenon_core_vf6)
