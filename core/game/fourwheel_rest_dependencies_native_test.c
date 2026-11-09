/* Typed real vf29 bindings. No registration or fake result for a changed
 * unknown target: the existing product dispatcher records that boundary. */
#include "fourwheel_rest_callbacks.h"
#if !defined(RX_ORACLE) || defined(RX_ORACLE_FOURWHEEL) || defined(RX_FOURWHEEL_REST_FRAME_NATIVE_TEST)
#include "fx_util.h"
#include <stdlib.h>
void rx_fourwheel_rest_contact_dependency(unsigned family,rx_addr expected){
 rx_addr entry=rx_sp();uint32_t ra=U32(entry,0),target=fx_reg(FX_EDX),self=fx_reg(FX_ECX);
 if(family>1||ra!=(family?0x4b411a:0x4ce91a)||expected!=entry+4||U32(U32(self,0),0x74)!=target)abort();
 if(target==0x4ce8e0)(void)rx_utv_truck_rest_all_contacts(self);
 else if(target==0x4b40e0)(void)rx_buggy_rest_all_contacts(self);
 else{
  /* No arbitrary LTCG cast. Product cannot execute original bytes. Restore
   * caller depth before its established missing-function diagnostic dispatch
   * so it does not reserve this already-entered frame twice. Never continue
   * with an invented callback value if a diagnostic handler returns. */
  rx_regs in=fx_regs(),out;for(unsigned i=0;i<8;i++)in.r[i]=fx_reg((int)i);
  rx_set_sp(expected);rx_call_original_ex(target,&in,&out,0,NULL);abort();
 }
 if(rx_sp()!=entry)abort();rx_set_sp(expected);
}
#endif
