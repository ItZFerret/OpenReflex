/* Complete original callback004391a0 from actual gap assembly, not inferred from
 * consumer behavior. No RX_FUNCTION or product callback registration here. */
#include "pending_tuning_producer.h"
#include "fx_util.h"
static void call(rx_addr B,rx_addr ra,enum rx_pending_role role,rx_addr fn,unsigned n,const uint32_t*a,unsigned pop){
 rx_addr entry=B-4u*(n+1);SU32(entry,0,ra);for(unsigned i=0;i<n;i++)SU32(entry,4u*(i+1),a[i]);rx_set_sp(entry);rx_pending_dependency(role,fn,B-4u*(n-pop),n,a);rx_set_sp(B);
}
void rx_pending_tuning_receive(uint32_t size,rx_addr payload){
 const rx_addr E=rx_sp(),B=E-4;const uint32_t saved_si=fx_reg(FX_ESI);SU32(B,0,saved_si);
 const uint32_t cs=0xad7538;call(B,0x4391ac,RX_PENDING_ENTER,U32(0,0x91600c),1,&cs,1);fx_set_reg(FX_ESI,0);
 if(!U32(0,0x9e3100)){
  const uint32_t n=0x1c;call(B,0x4391bd,RX_PENDING_ALLOC,0x78fbe0,1,&n,0);rx_addr stream=fx_reg(FX_EAX);
  if(stream){SU32(stream,0,0x974d1c);SU32(stream,4,0);SU32(stream,8,0);SU32(stream,0x10,0);SU32(stream,0x14,1);}else fx_set_reg(FX_EAX,0);
#ifdef RX_ORACLE
  /* Original reads arguments after allocation, not before Enter/allocator calls. */
  payload=U32(E,8);size=U32(E,4);
#endif
  fx_set_reg(FX_ECX,payload);fx_set_reg(FX_EDX,size);SU32(0,0x9e3100,fx_reg(FX_EAX));
  if(!stream)rx_pending_allocation_fault(); /* original next write dereferencesNULL */
  SU32(stream,4,fx_reg(FX_ECX));SU32(stream,8,fx_reg(FX_EDX));SU32(stream,0xc,0);SU32(stream,0x14,0);SU32(stream,0x18,0);
  fx_set_reg(FX_EAX,fx_reg(FX_EDX)+fx_reg(FX_ECX)-7u);uint32_t args[]={0x93a784,fx_reg(FX_EAX),7};call(B,0x43920c,RX_PENDING_COMPARE,U32(0,0x916234),3,args,0);
  if(fx_reg(FX_EAX)){
   fx_set_reg(FX_ECX,U32(0,0x9e3100));if(fx_reg(FX_ECX)){
    fx_set_reg(FX_EDX,U32(fx_reg(FX_ECX),0));fx_set_reg(FX_EAX,U32(fx_reg(FX_EDX),0));const uint32_t one=1;call(B,0x439225,RX_PENDING_DELETE,fx_reg(FX_EAX),1,&one,1);
   }
   SU32(0,0x9e3100,fx_reg(FX_ESI));
  }
 }
 call(B,0x439236,RX_PENDING_LEAVE,U32(0,0x9160f8),1,&cs,1);fx_set_reg(FX_ESI,saved_si);rx_set_sp(E);
}
