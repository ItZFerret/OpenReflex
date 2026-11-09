/* Isolated actual native registration: typed C ABI supplies self explicitly.
 * Original thiscall supplies ECX=self before entering004cdd20. Map that one
 * volatile argument register; do not invent any other incoming register/frame. */
#include "utv_contact_response.h"
#include "fx_util.h"
void rx_utv_contact_response_native_entry(rx_addr self,float dt){
 fx_set_reg(FX_ECX,self);
 rx_utv_contact_response_exact(self,dt);
}
RX_FUNCTION(0x004cdd20,rx_utv_contact_response_native_entry)
