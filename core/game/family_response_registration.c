/* Exact public typed-entry mapping; shared Truck target registered only once. */
#include "family_response_entry.h"
#include "rx_fn.h"
#include "fourwheel_solve_wrapper.h"
#include "fx_util.h"
/* PHYS_CALL_SP reserves the true callee depth but passes float dt as a native
 * argument. These vf12 originals reload entry+4 via FLD on the active path. */
void rx_utv_truck_solve_native_entry(rx_addr s,float dt){fx_set_reg(FX_ECX,s);SU32(rx_sp(),4,rx_fb(dt));rx_utv_truck_solve_vf12_draft(s,dt);}
void rx_buggy_solve_native_entry(rx_addr s,float dt){fx_set_reg(FX_ECX,s);SU32(rx_sp(),4,rx_fb(dt));rx_buggy_solve_vf12_draft(s,dt);}
RX_FUNCTION(0x004d1ce0,rx_utv_truck_solve_native_entry)
RX_FUNCTION(0x004b7530,rx_buggy_solve_native_entry)
RX_FUNCTION(0x004b3520,rx_buggy_contact_response_native_entry)
RX_FUNCTION(0x00451030,rx_truck_contact_response_native_entry)
