// w-parity5: oracle adapters of the four-wheel native compositions that ran only in the product: the vehicles' frame update Vvf29
// (fourwheel_vehicle_update.c + its finite dependency boundary fourwheel_update_native.c: UTV 004c1620, Buggy 004a38e0, TrophyTruck
// 0046b6d0; thiscall ECX = the vehicle, stack dt, RET 4) and the UTV's contact response vf15 004cdd20 (utv_contact_response.c,
// utv_response_roles.c: thiscall ECX = the simulation, stack dt, RET 4). Select by group (rx_FourWheelUpdate, rx_UtvContactResponse)
// or by address; Verify them with REFLEX_PORT_ALL_OUTPUTS=1 (the default live-output mask skips memory the callers do not read back).
#include "oracle.h"
#include "fx_oracle.h"   /* FxBind: the compositions keep EBX / EBP / ESI / EDI in the fx register model (fx_reg) */
extern "C" {
#include "../game/fourwheel_vehicle_update.h"
#include "../game/utv_contact_response.h"
void rx_utv_contact_response_native_entry(rx_addr self, float dt);
}
using namespace reflex_port;
RX_PORT(0x004c1620, FourWheel_UpdateUtv, "rx_FourWheelUpdate", 0) { FxBind fb_(m); rx_fourwheel_vehicle_update_utv(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x004a38e0, FourWheel_UpdateBuggy, "rx_FourWheelUpdate", 0) { FxBind fb_(m); rx_fourwheel_vehicle_update_buggy(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0046b6d0, FourWheel_UpdateTruck, "rx_FourWheelUpdate", 0) { FxBind fb_(m); rx_fourwheel_vehicle_update_truck(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x004cdd20, Utv_ContactResponse, "rx_UtvContactResponse", 0) { FxBind fb_(m); rx_utv_contact_response_native_entry(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
