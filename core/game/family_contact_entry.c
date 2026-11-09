/* Typed public entry sets the real original ECX and stack-dt inputs. */
#include "fourwheel_contact_dispatch.h"
#include "fx_util.h"
void rx_utv_contact_solve_native_entry(rx_addr s,float dt){fx_set_reg(FX_ECX,s);SU32(rx_sp(),4,rx_fb(dt));rx_utv_contact_solve_dispatch(s,dt);}
void rx_buggy_contact_solve_native_entry(rx_addr s,float dt){fx_set_reg(FX_ECX,s);SU32(rx_sp(),4,rx_fb(dt));rx_buggy_contact_solve_dispatch(s,dt);}
void rx_truck_contact_solve_native_entry(rx_addr s,float dt){fx_set_reg(FX_ECX,s);SU32(rx_sp(),4,rx_fb(dt));rx_truck_contact_solve_dispatch(s,dt);}
RX_FUNCTION(0x004d1d30,rx_utv_contact_solve_native_entry)
RX_FUNCTION(0x004b7580,rx_buggy_contact_solve_native_entry)
RX_FUNCTION(0x00453be0,rx_truck_contact_solve_native_entry)
