/* Native public typed entry adaptation only. The original ECX=self input is
 * explicit in the accepted callers; plain product PHYS_CALL_SP supplies the
 * typed argument but does not initialize the fx observer's ECX shadow. */
#include "family_response_entry.h"
#include "family_contact_response.h"
#include "fx_util.h"
void rx_buggy_contact_response_native_entry(rx_addr simulation,float dt){
 fx_set_reg(FX_ECX,simulation);
 rx_buggy_contact_response_exact(simulation,dt);
}
void rx_truck_contact_response_native_entry(rx_addr simulation,float dt){
 fx_set_reg(FX_ECX,simulation);
 rx_truck_contact_response_exact(simulation,dt);
}
