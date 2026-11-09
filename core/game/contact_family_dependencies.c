/* Reuse accepted IndexedCurve/CRT/ForceAtPoint frame owners exactly once.
 * Do not link accepted contact_native_dispatch.c alongside this replacement. */
#include "fourwheel_contact_forces.h"
#include "family_force_sites.h"
#if !defined(RX_ORACLE) || defined(RX_ORACLE_FOURWHEEL)
#include "contact_dependencies.h"
#include "phys_crt.h"
#include "fx_util.h"
#include <stdlib.h>
float rx_fourwheel_contact_dependency(rx_addr target,rx_addr expected,float input){
 const rx_addr entry=rx_sp();unsigned cleanup=target==0x85c770?8:4;
 if(!rx_family_contact_site(U32(entry,0),target)||expected!=entry+cleanup)abort();
 float value=input;
 if(target==0x4390d0)value=rx_contact_indexed_frame(fx_reg(FX_ESI),input);
 else if(target==0x85c770)rx_contact_force_at_point_frame(fx_reg(FX_ECX),fx_reg(FX_EDX),fx_reg(FX_ESI),U32(entry,4));
 else if(target==0x8f9efe)phys_security_check_cookie(fx_reg(FX_ECX));
 else abort();
 if(rx_sp()!=entry)abort();rx_set_sp(expected);return value;
}
#endif
