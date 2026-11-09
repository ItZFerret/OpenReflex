// build_input.cpp -- oracle adapters for unit 12b's input manager (core/game/build_input.c; group rx_build10): XenonCore vf6 and
// XenonInputManager_Construct with the records they build, verified per call in a baton boot of the reference event
// (REFLEX_PORT=rx_build10 REFLEX_PORT_VERIFY=1). ABI glue only; the register conventions are the original's (documented in
// build_input.h): the record constructors take their record in ECX, FUN_00816870 / Mouse_Clear in EAX, Mouse_Init in ESI,
// Keyboard_Create in EDI with the IDirectInput8 on the stack.
#include "oracle.h"
extern "C" {
#include "../game/build_input.h"
}
using namespace reflex_port;

#define G "rx_build10"

// 00843a10 XenonCore::vf6 (ECX = the engine context; EAX = the manager)
RX_PORT(0x00843a10, XenonCoreVf6, G, OUT_EAX) { m.r[0] = rx_xenon_core_vf6(m.r[1]); return m.ret(0); }
// 008145b0 XenonInputManager_Construct (stdcall self; EAX = self)
RX_PORT(0x008145b0, XenonInputManagerConstruct, G, OUT_EAX) { m.r[0] = rx_xenon_input_manager_construct(m.arg(0)); return m.ret(4); }
// 00814710 Fn_00814710 (ECX = a pad record; EAX = it)
RX_PORT(0x00814710, PadRecordConstruct, G, OUT_EAX) { m.r[0] = rx_pad_record_construct(m.r[1]); return m.ret(0); }
// 00814740 (ECX = a keyboard record; EAX = it): Keyboard_InitScanTable
RX_PORT(0x00814740, KeyboardRecordConstruct, G, OUT_EAX) { m.r[0] = rx_keyboard_record_construct(m.r[1]); return m.ret(0); }
// 00816ba0 Keyboard_InitScanTable (EDI = the keyboard record; EAX = it)
RX_PORT(0x00816ba0, KeyboardInitScanTable, G, OUT_EAX) { m.r[0] = rx_keyboard_init_scan_table(m.r[7]); return m.ret(0); }
// 00816870 (EAX = a pad record; EAX = record + 4, memset's result)
RX_PORT(0x00816870, PadStateClear, G, OUT_EAX) { m.r[0] = rx_pad_state_clear(m.r[0]); return m.ret(0); }
// 00817090 Keyboard_Create (EDI = the keyboard record, IDirectInput8 on the stack; RET 4)
RX_PORT(0x00817090, KeyboardCreate, G, 0) { rx_keyboard_create(m.r[7], m.arg(0)); return m.ret(4); }
// 008172f0 Mouse_Init (ESI = the mouse; EAX = the mouse)
RX_PORT(0x008172f0, MouseInit, G, OUT_EAX) { m.r[0] = rx_mouse_init(m.r[6]); return m.ret(0); }
// 00817be0 Mouse_Clear (EAX = the mouse; EAX = mouse + 4)
RX_PORT(0x00817be0, MouseClear, G, OUT_EAX) { m.r[0] = rx_mouse_clear(m.r[0]); return m.ret(0); }
