// race_activation.cpp -- oracle adapters for core/game/race_activation.c (the Free Ride activation areas: the UI manager's trigger
// handlers and their callees, the career Free Ride's discovery collectables). Group rx_race. ABI glue only.
#include "race_oracle.h"
extern "C" {
#include "../game/race_activation.h"
}
using namespace reflex_port;

#define GR "rx_race"

// 00596540 (cdecl name) -> EAX
RX_PORT(0x00596540, ChallengeActivityIndex, GR, OUT_EAX) { m.r[0] = rx_challenge_activity_index(m.arg(0)); return m.ret(0); }
// 004109d0 (EAX = type, ECX = track; stdcall spec; RET 4) -> EAX
RX_PORT(0x004109d0, ChallengeRequiredClass, GR, OUT_EAX) {
    m.r[0] = uint32_t(rx_challenge_required_class(m.r[0], m.r[1], m.arg(0)));
    return m.ret(4);
}
// 00410830 (EAX = type, ECX = track; stdcall spec, controller; RET 8) -> EAX
RX_PORT(0x00410830, ChallengeVehicleAllowed, GR, OUT_EAX) {
    m.r[0] = rx_challenge_vehicle_allowed(m.r[0], m.r[1], m.arg(0), int32_t(m.arg(1)));
    return m.ret(8);
}
// 004e78b0 MXTableHelper_GetPlayerTableRow (stdcall controller; RET 4) -> EAX
RX_PORT(0x004e78b0, PlayerTableRow, GR, OUT_EAX) { m.r[0] = rx_player_table_row(int32_t(m.arg(0))); return m.ret(4); }
// 00542ff0 (EAX = list, EDI = vehicle id) -> EAX
RX_PORT(0x00542ff0, VehicleRecordFind, GR, OUT_EAX) { m.r[0] = rx_vehicle_record_find(m.r[0], int32_t(m.r[7])); return m.ret(0); }
// 004199d0 (ESI = table, EAX = column name; stdcall &key; RET 4) -> EAX
RX_PORT(0x004199d0, TableRowsByName, GR, OUT_EAX) { m.r[0] = rx_tbl_rows_by_name(m.r[6], m.r[0], m.arg(0)); return m.ret(4); }
// 005a3b60 MXUIManager_OnTriggerEnter / 005a3d00 OnTriggerExit (thiscall ECX = the UI manager; stdcall vehicle, trigger; RET 8)
RX_PORT(0x005a3b60, UiTriggerEnter, GR, 0) { rx_ui_trigger_enter(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x005a3d00, UiTriggerExit, GR, 0) { rx_ui_trigger_exit(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
// 0054d640 (stdcall player, index; RET 8) -> EAX
RX_PORT(0x0054d640, PlayerDiscoveryFound, GR, OUT_EAX) { m.r[0] = rx_player_discovery_found(m.arg(0), m.arg(1)); return m.ret(8); }
// 00585520 (stdcall tracker, activity; RET 8)
RX_PORT(0x00585520, FreeRideDiscoveriesSetup, GR, 0) { rx_free_ride_discoveries_setup(m.arg(0), m.arg(1)); return m.ret(8); }
