/* Native source proposal: reuse accepted contact ForceAtPoint observer once.
 * No target registration or fallback. Future composition must prove this TU. */
#include "fourwheel_suspension.h"
#if !defined(RX_ORACLE) || defined(RX_ORACLE_FOURWHEEL)
#include "fx_util.h"
#include "contact_dependencies.h"
#include <stdlib.h>
void rx_family_suspension_dependency(unsigned family,rx_addr expected){
 static const uint32_t sites[3][9]={
  {0x4d05f4,0x4d0605,0x4d0866,0x4d0877,0x4d0ad8,0x4d0ae9,0x4d0d5c,0x4d0d6d,0x4d0d7d},
  {0x4b5e44,0x4b5e55,0x4b60b9,0x4b60ca,0x4b632b,0x4b633c,0x4b65af,0x4b65c0,0x4b65d0},
  {0x453433,0x453447,0x4536b4,0x4536c8,0x45392c,0x453940,0x453bb6,0x453bca,0x453bda}};
 if(family>2)abort();rx_addr entry=rx_sp();uint32_t ra=U32(entry,0);unsigned role=9;
 for(unsigned i=0;i<9;i++)if(ra==sites[family][i])role=i;
 if(role==9||expected!=entry+(role==8?4:8))abort();
 if(role==8)phys_security_check_cookie(fx_reg(FX_ECX));
 else (void)rx_contact_force_at_point_frame(fx_reg(FX_ECX),fx_reg(FX_EDX),fx_reg(FX_ESI),U32(entry,4));
 if(rx_sp()!=entry)abort();rx_set_sp(expected);
}
#endif
