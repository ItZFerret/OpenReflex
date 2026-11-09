// career_heat.cpp -- oracle adapters for core/game/career_heat.c, career_heat_events.c and career_aaa_loading.c (group rx_career):
// MXSeriesEventHeat's slots, its member proxies' clones and MXUIAAALoadingPage::vf4.
//
// The career event code reaches the series manager, its player table and the UI properties through the composition's dependencies
// (career_single.h). The product installs its own (career_composition.c: the owned property store, the host's hub); the oracle installs
// the original's: the manager / player-table / state / standings ports, and for the UI properties and the hub the original functions
// the events call (UIProperty_SetString 005dbf50, UIProperty_SetInt 005dbfb0, the "GotoHub" raise of 005db500 + 004020c0), so a Verify
// of a slot compares the same memory effects.
#include "oracle.h"
#include <cstring>
extern "C" {
#include "../game/career_heat.h"
#include "../game/career_manager.h"
#include "../game/career_standings_seed.h"
#include "../game/race_activity_ui.h"
#include "../game/fx.h"
#include "../game/crt.h"
}
using namespace reflex_port;

namespace {
uint32_t ui_manager() { return rx_rd32(0x00a93fcc); }
rx_regs zero() { rx_regs r; std::memset(&r, 0, sizeof r); return r; }

// the original's call at its depth: n pushed arguments and the return address below the caller's ESP
struct Depth { rx_addr sp; explicit Depth(int n) : sp(rx_sp()) { rx_set_sp(sp - 4u * uint32_t(n) - 4u); } ~Depth() { rx_set_sp(sp); } };
rx_addr dep_manager(void) { return rx_fx_series_manager(); }
rx_addr dep_player_table(rx_addr m, uint32_t selector) { Depth d(0); return rx_career_player_table(m, selector); }     // 005b6840 (EDX, ECX)
void dep_apply(rx_addr m) { rx_career_apply_player_states(m); }   // 005b39a0 (EDX): the callers make its depth (as the product composition)
void dep_request(rx_addr m, uint32_t from, uint32_t to) { Depth d(2); rx_career_request_player_states(m, from, to); }   // 005b3890 (EDX; from, to)
void dep_refresh(rx_addr m) { Depth d(0); rx_career_update_standings(m); }                                          // 005b43c0 (EDX)
void dep_standings(rx_addr m, int32_t a, int32_t b, int32_t k) { Depth d(4); rx_career_accumulate_standings(m, a, b, k); }   // 005b55d0 (stdcall 4)
void dep_clear(rx_addr m, int32_t selector) { rx_career_clear_standings(m, selector); }
void dep_seed(rx_addr m, int32_t selector) { rx_career_seed_standings(m, selector); }
// UIProperty_GetInt 005dc0c0 into a slot of dead stack, the initial value when the property is missing
uint32_t dep_get_int(rx_addr key, uint32_t initial) {
    const rx_addr slot = (rx_sp() - 0x40) & ~3u;
    rx_wr32(slot, initial);
    rx_ui_session_get_int(key, slot);
    return rx_rd32(slot);
}
// UIProperty_SetString 005dbf50 (ESI = the UI manager, EDI = key; value; RET 4)
void dep_string(rx_addr key, rx_addr value) {
    rx_regs in = zero(), out;
    in.r[6] = ui_manager();
    in.r[7] = key;
    const uint32_t a[1] = { value };
    rx_call_original_ex(0x005dbf50, &in, &out, 1, a);
}
// UIProperty_SetInt 005dbfb0 (ECX = the UI manager; key, value; RET 8)
void dep_int(rx_addr key, uint32_t value) {
    const uint32_t a[2] = { key, value };
    rx_call_original(0x005dbfb0, RX_THISCALL, ui_manager(), 2, a);
}
// the UI event `name` raised as the events raise it: 005db500 (ESI = the UI manager; name; RET 4) finds the registered name, then
// the typed raise 004020c0 (EAX = the UI channel [manager + 0x98], EDI = the name; &payload, 4) with the name as the payload
void raise_ui(rx_addr name) {
    rx_regs in = zero(), out;
    in.r[6] = ui_manager();
    const uint32_t n[1] = { name };
    rx_call_original_ex(0x005db500, &in, &out, 1, n);
    const uint32_t found = out.r[0];
    if (!found) return;
    (void)crt_type_info_name(0x009cdb68, 0x009e1a7c);                        // type_info::_name_internal_method
    const rx_addr slot = (rx_sp() - 0x40) & ~3u;
    rx_wr32(slot, found);
    rx_regs raise = zero(), done;
    raise.r[0] = rx_rd32(ui_manager() + 0x98);
    raise.r[7] = found;
    const uint32_t a[2] = { slot, 4 };
    rx_call_original_ex(0x004020c0, &raise, &done, 2, a);
}
void dep_event(rx_addr name) { raise_ui(name); }
void dep_hub(void) { raise_ui(0x009424ec /* "GotoHub" */); }

void install_dependencies() {
    static bool done = false;
    if (done) return;
    done = true;
    static rx_career_single_dependencies d;
    std::memset(&d, 0, sizeof d);
    d.manager = dep_manager;
    d.player_table = dep_player_table;
    d.apply_player_states = dep_apply;
    d.request_player_states = dep_request;
    d.refresh_standings = dep_refresh;
    d.standings = dep_standings;
    d.clear_standings = dep_clear;
    d.seed_standings = dep_seed;
    d.ui_get_int = dep_get_int;
    d.ui_string = dep_string;
    d.ui_int = dep_int;
    d.ui_event = dep_event;
    d.ui_goto_hub = dep_hub;
    rx_career_single_install_dependencies(&d);
}
}  // namespace

extern "C" const rx_career_single_dependencies* rx_career_event_dependencies(void);
static const rx_career_single_dependencies* deps() {
    install_dependencies();
    return rx_career_event_dependencies();
}

// 005ad840 vf1 (thiscall; phase; RET 4) -> EAX
RX_PORT(0x005ad840, HeatPlayerState, "rx_career", OUT_EAX) { m.r[0] = rx_career_heat_player_state(m.arg(0)); return m.ret(4); }
// 005aee00 vf2 (thiscall event; enable; RET 4)
RX_PORT(0x005aee00, HeatHoleshot, "rx_career", 0) { rx_career_heat_holeshot(m.r[1], m.arg(0), deps()); return m.ret(4); }
// 005ad880 vf7 / 005ad8f0 vf8 (thiscall event; channel; RET 4)
RX_PORT(0x005ad880, HeatBind, "rx_career", 0) { rx_career_heat_bind(m.r[1], m.arg(0), 1, deps()); return m.ret(4); }
RX_PORT(0x005ad8f0, HeatUnbind, "rx_career", 0) { rx_career_heat_bind(m.r[1], m.arg(0), 0, deps()); return m.ret(4); }
// 005ad960 vf10 (thiscall event; practice; RET 4)
RX_PORT(0x005ad960, HeatGateResult, "rx_career", 0) { rx_career_heat_gate_result(m.r[1], m.arg(0), deps()); return m.ret(4); }
// 005adb00 vf12 / 005adde0 vf13 (thiscall event)
RX_PORT(0x005adb00, HeatAdvance, "rx_career", 0) { rx_career_heat_advance(m.r[1], deps()); return m.ret(0); }
RX_PORT(0x005adde0, HeatPostEvent, "rx_career", 0) { rx_career_heat_post_event(m.r[1], deps()); return m.ret(0); }
// 005aedd0 vf18 (thiscall event) -> EAX
RX_PORT(0x005aedd0, HeatCapacity, "rx_career", OUT_EAX) { m.r[0] = rx_career_heat_capacity(m.r[1], deps()); return m.ret(0); }
// 005bfc40 / 005bfce0 / 005bfda0 the proxies' vf3 (thiscall proxy) -> EAX: the clone
RX_PORT(0x005bfc40, HeatProxyCloneInt, "rx_career", OUT_EAX) {
    using F = rx_addr (*)(rx_addr);
    m.r[0] = RX_FN(0x005bfc40, F)(m.r[1]);
    return m.ret(0);
}
RX_PORT(0x005bfce0, HeatProxyCloneVoid, "rx_career", OUT_EAX) {
    using F = rx_addr (*)(rx_addr);
    m.r[0] = RX_FN(0x005bfce0, F)(m.r[1]);
    return m.ret(0);
}
RX_PORT(0x005bfda0, HeatProxyClonePlayer, "rx_career", OUT_EAX) {
    using F = rx_addr (*)(rx_addr);
    m.r[0] = RX_FN(0x005bfda0, F)(m.r[1]);
    return m.ret(0);
}
// 005a8030 MXUIAAALoadingPage::vf4 (thiscall page)
RX_PORT(0x005a8030, AAALoadingPageEnter, "rx_career", 0) { install_dependencies(); rx_aaa_loading_page_enter(m.r[1]); return m.ret(0); }

// ---- the base MXSeriesEvent / series manager functions the Heat event runs (career_single*.c, career_manager.c, career_standings.c):
// verified here in the same group, so a career boot localizes a difference to the deepest of them ----
// 005a9290 MXSeriesEvent_SetupActivity (stdcall event, requested state, mode, length, limit kind; RET 0x14)
RX_PORT(0x005a9290, SeriesSetupActivity, "rx_career", 0) {
    rx_career_event_setup_activity(m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4), deps());
    return m.ret(0x14);
}
// 005a8af0 MXSeriesEvent_SetupNextActivityPlayers (stdcall event, reset; RET 8) -> EAX: the next riders not yet active
RX_PORT(0x005a8af0, SeriesSetupNextPlayers, "rx_career", OUT_EAX) { m.r[0] = rx_career_event_setup_next_players(m.arg(0), m.arg(1), deps()); return m.ret(8); }
// 005aad40 MXSeriesEvent_OnAdvanceNextSubEvent (thiscall event)
RX_PORT(0x005aad40, SeriesNextSubEvent, "rx_career", 0) { rx_career_event_next_sub_event(m.r[1], deps()); return m.ret(0); }
// 005a8a10 vf9 the gate selection / 005a9a30 ProcessGateSelection / 005a9ce0 AdvanceGateSelection / 005a9f40 vf11 (thiscall event)
RX_PORT(0x005a8a10, SeriesGate, "rx_career", 0) { rx_career_event_gate(m.r[1], deps()); return m.ret(0); }
RX_PORT(0x005a9a30, SeriesProcessGate, "rx_career", 0) { rx_career_event_process_gate(m.r[1], deps()); return m.ret(0); }
RX_PORT(0x005a9ce0, SeriesAdvanceGate, "rx_career", 0) { rx_career_event_advance_gate(m.r[1], deps()); return m.ret(0); }
RX_PORT(0x005a9f40, SeriesPostResult, "rx_career", 0) { rx_career_event_post_result(m.r[1], deps()); return m.ret(0); }
// 005a9e00 AssignPlayerGate (thiscall event; device, gate; RET 8)
RX_PORT(0x005a9e00, SeriesAssignGate, "rx_career", 0) { install_dependencies(); rx_career_event_assign_gate(m.r[1], m.arg(0), int32_t(m.arg(1))); return m.ret(8); }
// 005b6840 the player table (EDX = manager, ECX = selector) -> EAX
RX_PORT(0x005b6840, SeriesPlayerTable, "rx_career", OUT_EAX) { m.r[0] = rx_career_player_table(m.r[2], m.r[1]); return m.ret(0); }
// 005b39a0 ApplyPlayerStates (EDX = manager) / 005b3890 RequestPlayerStates (EDX = manager; from, to; RET 8)
RX_PORT(0x005b39a0, SeriesApplyStates, "rx_career", 0) { rx_career_apply_player_states(m.r[2]); return m.ret(0); }
RX_PORT(0x005b3890, SeriesRequestStates, "rx_career", 0) { rx_career_request_player_states(m.r[2], m.arg(0), m.arg(1)); return m.ret(8); }
// 005b43c0 the standings of the race (EDX = manager) / 005b55d0 the standings accumulated (stdcall manager, dest, source, mult; RET 0x10)
RX_PORT(0x005b43c0, SeriesUpdateStandings, "rx_career", 0) { rx_career_update_standings(m.r[2]); return m.ret(0); }
RX_PORT(0x005b55d0, SeriesAccumulateStandings, "rx_career", 0) {
    rx_career_accumulate_standings(m.arg(0), int32_t(m.arg(1)), int32_t(m.arg(2)), int32_t(m.arg(3)));
    return m.ret(0x10);
}
// 005a1330 the event type's rider limit (stdcall type; RET 4) -> EAX
RX_PORT(0x005a1330, SeriesEventRiderLimit, "rx_career", OUT_EAX) { m.r[0] = rx_career_event_rider_limit(m.arg(0), deps()); return m.ret(4); }
