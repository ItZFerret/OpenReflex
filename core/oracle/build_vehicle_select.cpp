// build_vehicle_select.cpp -- oracle adapters (group rx_vselect) for core/game/build_vehicle_select.c: App.ChangeVehicle's native 005a0670,
// MXTableHelper_EquipVehicle 004e7930, MXTableHelper_GetEquippedVehicle 004e7c60. The boots that reach them: the tutorial boot
// (MainMenu:OnTutorialKeypress: GetEquippedVehicle, ChangeVehicle( row, 2 ); the lesson driver's "lua" lines for the other branches), the
// garage's equip (Boot::start_event with REFLEX_GARAGE).
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
extern "C" {
#include "../game/build_vehicle_select.h"
#include "../game/build_vehicle.h"
}
using namespace reflex_port;
#define G "rx_vselect"

// 005a0670 (stdcall row, vehicle type; RET 8)
// REFLEX_VSELECT_LOG=1: each call's row, type and the vehicle it equipped (the row's requested Vehicle List row: VehicleOwner / OwnerVehicleId)
RX_PORT(0x005a0670, AppChangeVehicle, G, 0) {
    const uint32_t row = m.arg(0), type = m.arg(1);
    rx_change_vehicle(row, type);
    if (std::getenv("REFLEX_VSELECT_LOG")) {   // (read below ESP: dead stack)
        const rx_addr sp = rx_sp(), o = sp - 0x40;
        SU32(o, 0, 0xffffffffu); SU32(o, 4, 0xffffffffu);
        rx_set_sp(o - 12); rx_table_get_equipped_vehicle(row, o, o + 4); rx_set_sp(sp);
        std::printf("vselect: ChangeVehicle( %u, %u ) -> owner %d id %d\n", row, type, int(U32(o, 4)), int(U32(o, 0)));
    }
    return m.ret(8);
}
// 004e7930 (EAX = owner; stdcall row, owner vehicle id; RET 8; ends in a tail call of 004ece50)
RX_PORT(0x004e7930, TableHelperEquipVehicle, G, 0) {
    if (std::getenv("REFLEX_VSELECT_LOG")) {   // the row's RequestedVehicleId / VehicleId before (the erase of the old requested row)
        const rx_addr pl = 0x00ae0e08u;
        const uint32_t req = U32(rx_tbl_cell_word(pl, rx_tbl_column_index(pl, 0x0093df5c), m.arg(0)), 0);
        const uint32_t cur = U32(rx_tbl_cell_word(pl, rx_tbl_column_index(pl, 0x00936cb4), m.arg(0)), 0);
        std::printf("vselect: EquipVehicle( owner %d, row %u, id %d ): requested %d vehicle %d\n", int(m.r[0]), m.arg(0), int(m.arg(1)), int(req), int(cur));
    }
    rx_table_equip_vehicle(m.r[0], m.arg(0), m.arg(1));
    return m.ret(8);
}
// 004e7c60 (EAX = row; stdcall &owner vehicle id, &owner; RET 8) -> EAX 1
RX_PORT(0x004e7c60, TableHelperGetEquippedVehicle, G, OUT_EAX) { const uint32_t r = rx_table_get_equipped_vehicle(m.r[0], m.arg(0), m.arg(1)); return m.ret(8, r); }
