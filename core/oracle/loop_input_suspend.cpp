// loop_input_suspend.cpp -- oracle adapters (group rx_suspend) for core/game/loop_input_suspend.c: Player_SetInputSuspended and the groups'
// queries, XenonInputManager::vf45, MXUIManager_OnSuspendPlayerInputChanged. The boots that reach them: the tutorial boot (REFLEX_EVENT_SPEC
// "Tutorial,...", REFLEX_TUT_SCRIPT: the MotoSkills popups and the lesson list set SuspendPlayerInput) and any page that holds the controls.
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
extern "C" {
#include "../game/loop_input_suspend.h"
}
using namespace reflex_port;
#define G "rx_suspend"

// 0054f540 (stdcall player, suspended; RET 8)
// REFLEX_SUSPEND_LOG=1: the calls that change the state (player, the value, +0x11cc before / after)
RX_PORT(0x0054f540, PlayerSetInputSuspended, G, 0) {
    const uint32_t P = m.arg(0), on = m.arg(1), was = U32(P, 0x11c8), mask = U32(P, 0x11cc);
    rx_player_set_input_suspended(P, on);
    if (was != on && std::getenv("REFLEX_SUSPEND_LOG"))
        std::printf("suspend: player %08x pad %d %u -> %u mask %03x -> %03x\n", P, int(U32(P, 0x1f0)), was, on, mask, U32(P, 0x11cc));
    return m.ret(8);
}
// 00548d20 / 00548da0 / 00548e00 (ESI = pad), 00548d60 (EAX = pad) -> EAX
RX_PORT(0x00548d20, PlayerGroup0Enabled, G, OUT_EAX) { const uint32_t r = rx_player_group0_enabled(m.r[6]); return m.ret(0, r); }
RX_PORT(0x00548d60, PlayerGroup3Enabled, G, OUT_EAX) { const uint32_t r = rx_player_group3_enabled(m.r[0]); return m.ret(0, r); }
RX_PORT(0x00548da0, PlayerGroup2Enabled, G, OUT_EAX) { const uint32_t r = rx_player_group2_enabled(m.r[6]); return m.ret(0, r); }
RX_PORT(0x00548e00, PlayerGroup5Enabled, G, OUT_EAX) { const uint32_t r = rx_player_group5_enabled(m.r[6]); return m.ret(0, r); }
// 00815170 XenonInputManager::vf45 (thiscall device, controller, value; RET 0xc) -> EAX
RX_PORT(0x00815170, XenonInputManager_vf45, G, OUT_EAX) { const uint32_t r = rx_input_set_vibration_paused(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc, r); }
// 005a3b00 (ECX = ui; stack the event's arguments; RET 4)
RX_PORT(0x005a3b00, UIOnSuspendPlayerInputChanged, G, 0) { rx_ui_on_suspend_input_changed(m.r[1], m.arg(0)); return m.ret(4); }
