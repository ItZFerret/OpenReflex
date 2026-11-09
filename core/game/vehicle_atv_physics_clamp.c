/*0048d430: body angular-speed caps, with original four-lane scaling. */
#include "vehicle_atv_physics.h"
#include "build_vehicle_util.h"
#include "bike_math.h"
void rx_atv_clamp_body_spin(rx_addr s){
 float childCap=F32(0,0x009c4750);
 for(int32_t i=0;i<I32(s,0xec)||i==0;i++){
  rx_addr p=s+0x740+0xa0*(uint32_t)i;rx_v4 velocity=rx_ld4(p);
  rx_v4 square=v4_mul(velocity,velocity);rx_v4 length=v4_sqrt(v4_splat(bike_hsum_021(square)));
  rx_v4 cap=v4_splat(i?childCap:20);
  if(bike_all3(v4_cmplt(cap,length))){
   length=v4_sqrt(v4_splat(bike_hsum_021(square)));
   rx_st4(p,v4_div(v4_mul(velocity,cap),length));SU32(s,0x90e0,1);
  }
 }
}
RX_FUNCTION(0x0048d430,rx_atv_clamp_body_spin)
