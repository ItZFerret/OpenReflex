/* Product typed ownership closure; oracle comparisons use true original calls.
 * No host stream, synthetic vtable or broad untyped callback dispatch. */
#include "pending_tuning_producer.h"
#if !defined(RX_ORACLE) || defined(RX_ORACLE_FOURWHEEL)
#include "fx_util.h"
#include "build_alloc.h"
#include "tdf_owner.h"
#include "crt.h"
#include "../src/rx_os.h"
#include <stdlib.h>
#include <stdio.h>
static void contract(int ok,rx_addr fn){if(!ok){fprintf(stderr,"pending producer native dependency contract %08x\n",fn);abort();}}
void rx_pending_allocation_fault(void){abort();} /* original NULL dereference */
void rx_pending_dependency(enum rx_pending_role role,rx_addr fn,rx_addr expected,unsigned n,const uint32_t*a){
 const rx_addr entry=rx_sp();uint32_t result=0;
 const uint32_t rets[]={0x4391ac,0x4391bd,0x43920c,0x439225,0x439236};
 contract((unsigned)role<5,fn);unsigned want=role==RX_PENDING_COMPARE?3:1;
 contract(n==want&&a&&U32(entry,0)==rets[role]&&expected==entry+((role==RX_PENDING_ENTER||role==RX_PENDING_LEAVE||role==RX_PENDING_DELETE)?8u:4u),fn);
 for(unsigned i=0;i<n;i++)contract(U32(entry,4u*(i+1))==a[i],fn);
 switch(role){
 case RX_PENDING_ENTER:case RX_PENDING_LEAVE:{
  const rx_addr slot=role==RX_PENDING_ENTER?0x91600c:0x9160f8;
  contract(fn==U32(0,slot)&&a[0]==0xad7538,fn);
  rx_set_sp(entry+8);result=rx_os_import(slot,1,a);contract(rx_sp()==entry+8,fn);rx_set_sp(entry);break;}
 case RX_PENDING_ALLOC:
  contract(fn==0x78fbe0&&a[0]==0x1c,fn);result=rx_reflex_aligned_alloc(a[0]);contract(rx_sp()==entry,fn);break;
 case RX_PENDING_COMPARE:
  contract(fn==U32(0,0x916234)&&a[0]==0x93a784&&a[2]==7,fn);result=(uint32_t)crt_strncmp(a[0],a[1],a[2]);contract(rx_sp()==entry,fn);break;
 case RX_PENDING_DELETE:
  contract(a[0]==1&&fn==U32(U32(fx_reg(FX_ECX),0),0),fn);
  if(fn==0x810890)result=rx_owned_memory_stream_delete(fx_reg(FX_ECX),a[0]);
  else result=rx_call_original(fn,RX_THISCALL,fx_reg(FX_ECX),1,a); /* named unknown-owner boundary */
  contract(rx_sp()==entry,fn);break;
 default:abort();
 }
 fx_set_reg(FX_EAX,result);contract(rx_sp()==entry,fn);
}
#endif
