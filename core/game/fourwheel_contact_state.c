/* Complete simulation vf29/30/31 pairs and shared vf33. Each family differs
 * only by the four wheel-record offsets and its vf29/30 entry addresses.
 * Exact source order, four normal lanes, and recovery snapshots are retained. */
#include "fourwheel_contact_state.h"
#include "phys_vec.h"
#include "fx_util.h"
#include <stdlib.h>
static uint32_t all_contact(rx_addr s,unsigned delta){
 const uint32_t result=U32(s,0x8420-delta)!=0&&U32(s,0x8180-delta)!=0&&U32(s,0x8570-delta)!=0&&U32(s,0x82d0-delta)!=0;
 fx_set_reg(FX_EAX,result);return result;
}
uint32_t rx_fourwheel_all_contact_truck(rx_addr s){return all_contact(s,0);}
uint32_t rx_fourwheel_all_contact_buggy(rx_addr s){return all_contact(s,0x10);}
static uint32_t grounded(rx_addr s,unsigned delta){
 const rx_addr target=delta?0x4b40e0:0x4ce8e0,ra=delta?0x4b411a:0x4ce91a;
 fx_frame frame;fx_frame_flat(&frame,0,FX_ESI);fx_set_reg(FX_ESI,s);fx_set_reg(FX_ECX,s);
 const rx_addr vt=U32(s,0);fx_set_reg(FX_EAX,vt);fx_set_reg(FX_EDX,U32(vt,0x74));
 /* Finite supported class binding. The genuine virtual target is checked;
  * no original fallback or replacement contact state is introduced. */
 if(fx_reg(FX_EDX)!=target)abort();
 const rx_addr before=fx_push(0,0,ra);uint32_t result=all_contact(s,delta);fx_pop(before);
 const uint16_t fields[]={0x83e0,0x8140,0x8530,0x8290,0x83e4,0x8144,0x8534,0x8294};
 for(unsigned i=0;result&&i<8;i++)if(!U32(s,fields[i]-delta))result=0;
 fx_set_reg(FX_EAX,result);fx_frame_leave(&frame);return result;
}
uint32_t rx_fourwheel_grounded_truck(rx_addr s){return grounded(s,0);}
uint32_t rx_fourwheel_grounded_buggy(rx_addr s){return grounded(s,0x10);}
static rx_addr contact_normal(rx_addr s,rx_addr out,unsigned delta){
 fx_frame frame;fx_frame_enter0(&frame,0,16);fx_set_reg(FX_EAX,out);
 rx_v4 sum=v4_add(rx_ld4(s+0x8450-delta),rx_ld4(s+0x81b0-delta));
 sum=v4_add(sum,rx_ld4(s+0x85a0-delta));sum=v4_add(sum,rx_ld4(s+0x8300-delta));
 const rx_v4 square=v4_mul(sum,sum);const float xy=phys_add(square.v[0],square.v[1]);
 rx_st4(out,sum);const rx_v4 infinity=rx_ld4(0x92dc20);
 const float length2=phys_add(xy,square.v[2]),length=phys_sqrt(length2);
 rx_v4 normal=v4_div(sum,v4_splat(length));const uint32_t nonzero=length!=0.0f?~0u:0u;
 const rx_v4 fallback=rx_ld4(0x92dc30);
 for(unsigned i=0;i<4;i++){
  const uint32_t finite=length2!=infinity.v[i]?~0u:0u;
  normal.v[i]=rx_bf((rx_fb(normal.v[i])&nonzero&finite)|(~finite&rx_fb(fallback.v[i])));
 }
 rx_st4(out,normal);fx_frame_leave(&frame);return out;
}
rx_addr rx_fourwheel_contact_normal_truck(rx_addr s,rx_addr out){return contact_normal(s,out,0);}
rx_addr rx_fourwheel_contact_normal_buggy(rx_addr s,rx_addr out){return contact_normal(s,out,0x10);}
void rx_fourwheel_save_recovery_pose(rx_addr s){
 fx_frame frame;fx_frame_enter0(&frame,0,16);fx_set_reg(FX_EAX,s);
 SU32(s,0x2de0,1);SU32(s,0x2e70,0);rx_st4(s+0x2e50,rx_ld4(s+0x730));rx_st4(s+0x2e60,rx_ld4(s+0xf0));fx_frame_leave(&frame);
}
RX_FUNCTION(0x004ce8e0,rx_fourwheel_all_contact_truck)
RX_FUNCTION(0x004b40e0,rx_fourwheel_all_contact_buggy)
RX_FUNCTION(0x004ce910,rx_fourwheel_grounded_truck)
RX_FUNCTION(0x004b4110,rx_fourwheel_grounded_buggy)
RX_FUNCTION(0x004ce980,rx_fourwheel_contact_normal_truck)
RX_FUNCTION(0x004b4180,rx_fourwheel_contact_normal_buggy)
RX_FUNCTION(0x0045a940,rx_fourwheel_save_recovery_pose)
