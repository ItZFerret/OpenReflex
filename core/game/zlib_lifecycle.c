/* Exact original zlib lifecycle, guest56-byte z_stream / 0x1ba8 state.
 * No host zlib, decompression substitute, product registration or RTC bypass. */
#include "zlib_lifecycle.h"
#include "fx_util.h"
#include "build_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef RX_ORACLE
extern void rx_zlib_lifecycle_compare(uint32_t left,uint32_t right);
extern uint32_t rx_zlib_lifecycle_callback(rx_addr target,unsigned n,const uint32_t* args);
#endif
static void reg(int i,uint32_t v){fx_set_reg(i,v);}
static void call_frame(rx_addr b,rx_addr ra,unsigned n,const uint32_t* a){
 rx_addr e=b-4*(n+1);SU32(e,0,ra);for(unsigned i=0;i<n;i++)SU32(e,4+4*i,a[i]);rx_set_sp(e);
}
/* __RTC_CheckEsp has a real RET-only equal branch; unequal invokes _RTC_Failure.
 * Oracle executes that original conditional boundary with actual CMP flags.
 * Native mismatch is an explicit unsupported failure boundary, never success. */
void rx_guest_zlib_rtc(rx_addr b,rx_addr ra,uint32_t left,uint32_t right){
#ifdef RX_ORACLE
 rx_zlib_lifecycle_compare(left,right);rx_set_sp(b);
 (void)rx_call_original(0x8fa3bd,RX_CDECL,0,0,0);SU32(b-4,0,ra);
#else
 call_frame(b,ra,0,0);
 if(left!=right){fprintf(stderr,"rxcore: zlib RTC stack imbalance %08x/%08x; original _RTC_Failure008faa57 unavailable\n",left,right);abort();}
#endif
 rx_set_sp(b);
}
static fx_frame enter(uint32_t size){
 fx_frame f;fx_frame_enter(&f,size,0,FX_EBX,FX_ESI,FX_EDI);
 reg(FX_EDI,f.ebp-size);reg(FX_ECX,size/4);reg(FX_EAX,0xcccccccc);
 for(uint32_t i=0;i<size;i+=4)SU32(f.ebp-size,i,0xcccccccc);
 reg(FX_EDI,f.ebp);reg(FX_ECX,0);return f;
}
static uint32_t leave(fx_frame f,uint32_t result,rx_addr rtc_ra){
 reg(FX_EAX,result);
 reg(FX_EDI,f.in.edi);reg(FX_ESI,f.in.esi);reg(FX_EBX,f.in.ebx);reg(FX_EBP,f.ebp);rx_set_sp(f.ebp);
 if(rtc_ra)rx_guest_zlib_rtc(f.ebp,rtc_ra,f.ebp,rx_sp());
 reg(FX_EBP,f.in.ebp);rx_set_sp(f.sp0);return result;
}
/* Callback args are cdecl opaque/items/size or opaque/address. Product uses
 * genuine typed original-address callbacks, no cast to host pointer/allocator. */
static uint32_t callback(rx_addr b,rx_addr ra,rx_addr target,unsigned n,const uint32_t* a){
 uint32_t result;call_frame(b,ra,n,a);
#ifdef RX_ORACLE
 rx_set_sp(b);result=rx_zlib_lifecycle_callback(target,n,a);SU32(b-4*(n+1),0,ra);
#else
 if(target==0x8ef290&&n==3)result=rx_guest_zalloc(a[0],a[1],a[2]);
 else if(target==0x8ef2f0&&n==2){rx_guest_zfree(a[0],a[1]);result=fx_reg(FX_EAX);}
 else{void* fn=rx_fn(target);
  if(fn&&n==3)result=((uint32_t(*)(rx_addr,uint32_t,uint32_t))fn)(a[0],a[1],a[2]);
  else if(fn&&n==2)result=((uint32_t(*)(rx_addr,rx_addr))fn)(a[0],a[1]);
  else{rx_set_sp(b);result=rx_call_original(target,RX_CDECL,0,n,a);}
 }
#endif
 /* Oracle preserves the actual callee ESP so the following real CMP/RTC
  * can diagnose a non-cdecl callback; rx_call_original would hide it. */
#ifndef RX_ORACLE
 const rx_addr entry=b-4*(n+1);
 if(rx_sp()!=entry){fprintf(stderr,"rxcore: native zlib callback altered guest callee ESP\n");abort();}
 rx_set_sp(b-4*n);
#endif
 return result;
}
static uint32_t callback_checked(rx_addr b,rx_addr call_ra,rx_addr check_ra,rx_addr target,unsigned n,const uint32_t* a){
 reg(FX_ESI,b);uint32_t result=callback(b,call_ra,target,n,a);
 rx_addr after=rx_sp()+4*n;rx_set_sp(after);rx_guest_zlib_rtc(after,check_ra,fx_reg(FX_ESI),after);return result;
}
rx_addr rx_guest_zalloc(rx_addr opaque,uint32_t items,uint32_t size){
 fx_frame f=enter(0xc0);
 if(opaque){reg(FX_EAX,size);reg(FX_EAX,0);reg(FX_EAX,items);SU32(f.ebp,0xc,items);}
 uint32_t bytes=items*size;reg(FX_EAX,bytes);call_frame(f.E,0x8ef2cd,1,&bytes);
 rx_addr p;
#ifdef RX_ORACLE
 rx_set_sp(f.E);p=rx_call_original(0x8fa05e,RX_CDECL,0,1,&bytes);SU32(f.E-8,0,0x8ef2cd);
#else
 p=rx_crt_malloc(bytes);
#endif
 rx_set_sp(f.E);return leave(f,p,0x8ef2e0);
}
void rx_guest_zfree(rx_addr opaque,rx_addr p){
 (void)opaque;fx_frame f=enter(0xc0);reg(FX_EAX,p);call_frame(f.E,0x8ef317,1,&p);
#ifdef RX_ORACLE
 rx_set_sp(f.E);(void)rx_call_original(0x8fa064,RX_CDECL,0,1,&p);SU32(f.E-8,0,0x8ef317);
#else
 rx_crt_free(p);
#endif
 rx_set_sp(f.E);(void)leave(f,fx_reg(FX_EAX),0x8ef32a);
}
int32_t rx_guest_inflate_reset(rx_addr s){
 fx_frame f=enter(0xcc);
 if(!s)return (int32_t)leave(f,0xfffffffe,0);
 reg(FX_EAX,s);if(!U32(s,0x1c))return (int32_t)leave(f,0xfffffffe,0);
 rx_addr state=U32(s,0x1c);reg(FX_ECX,state);SU32(f.ebp,-8,state);
 reg(FX_EAX,state);SU32(state,0x18,0);
 reg(FX_ECX,s);SU32(s,0x14,0);reg(FX_EDX,s);SU32(s,8,0);reg(FX_EAX,s);SU32(s,0x18,0);
 const uint32_t offsets[]={0,4,0xc,0x20,0x24,0x30,0x34};
 for(unsigned i=0;i<sizeof offsets/sizeof *offsets;i++){reg(FX_EAX,state);SU32(state,offsets[i],0);}
 reg(FX_EAX,state+0x528);reg(FX_ECX,state);SU32(state,0x64,state+0x528);
 reg(FX_EDX,state);reg(FX_EAX,state);reg(FX_ECX,U32(state,0x64));SU32(state,0x48,fx_reg(FX_ECX));
 reg(FX_EDX,state);reg(FX_EAX,state);reg(FX_ECX,U32(state,0x48));SU32(state,0x44,fx_reg(FX_ECX));
 return (int32_t)leave(f,0,0);
}
int32_t rx_guest_inflate_init2(rx_addr s,int32_t bits,rx_addr version,uint32_t size){
 fx_frame f=enter(0xcc);uint32_t result;
 if(!version)return (int32_t)leave(f,0xfffffffa,0x8ea5d0);
 reg(FX_EAX,version);reg(FX_ECX,(uint32_t)(int32_t)(int8_t)U8(version,0));reg(FX_EDX,(uint32_t)(int32_t)(int8_t)U8(0,0x9751dc));
 if(fx_reg(FX_ECX)!=fx_reg(FX_EDX)||size!=0x38)return (int32_t)leave(f,0xfffffffa,0x8ea5d0);
 if(!s)return (int32_t)leave(f,0xfffffffe,0x8ea5d0);
 reg(FX_EAX,s);SU32(s,0x18,0);
 if(!U32(s,0x20)){reg(FX_EAX,s);SU32(s,0x20,0x8ef290);reg(FX_EAX,s);SU32(s,0x28,0);}
 reg(FX_EAX,s);if(!U32(s,0x24)){reg(FX_EAX,s);SU32(s,0x24,0x8ef2f0);}
 reg(FX_EAX,s);reg(FX_ECX,U32(s,0x28));reg(FX_EDX,s);reg(FX_EAX,U32(s,0x20));
 uint32_t a[3]={U32(s,0x28),1,0x1ba8};
 rx_addr state=callback_checked(f.E,0x8ea507,0x8ea511,U32(s,0x20),3,a);SU32(f.ebp,-8,state);
 if(!state)return (int32_t)leave(f,0xfffffffc,0x8ea5d0);
 reg(FX_EAX,s);reg(FX_ECX,state);SU32(s,0x1c,state);
 if(bits<0){reg(FX_EAX,state);SU32(state,8,0);bits=(int32_t)(0u-(uint32_t)bits);reg(FX_EAX,(uint32_t)bits);SU32(f.ebp,0xc,(uint32_t)bits);}
 else{reg(FX_EAX,((uint32_t)(bits>>4))+1);reg(FX_ECX,state);SU32(state,8,fx_reg(FX_EAX));
  if(bits<0x30){bits&=15;reg(FX_EAX,(uint32_t)bits);SU32(f.ebp,0xc,(uint32_t)bits);}}
 if(bits<8||bits>15){
  reg(FX_EAX,state);reg(FX_ECX,s);reg(FX_EDX,U32(s,0x28));reg(FX_EAX,s);reg(FX_ECX,U32(s,0x24));
  uint32_t free_args[2]={U32(s,0x28),state};(void)callback_checked(f.E,0x8ea586,0x8ea590,U32(s,0x24),2,free_args);
  reg(FX_EAX,s);SU32(s,0x1c,0);return (int32_t)leave(f,0xfffffffe,0x8ea5d0);
 }
 reg(FX_EAX,state);reg(FX_ECX,(uint32_t)bits);SU32(state,0x1c,(uint32_t)bits);reg(FX_EAX,state);SU32(state,0x2c,0);reg(FX_EAX,s);
 call_frame(f.E,0x8ea5bd,1,&s);result=(uint32_t)rx_guest_inflate_reset(s);rx_set_sp(f.E);
 return (int32_t)leave(f,result,0x8ea5d0);
}
int32_t rx_guest_inflate_init(rx_addr s,rx_addr version,uint32_t size){
 fx_frame f=enter(0xc0);reg(FX_EAX,size);reg(FX_ECX,version);reg(FX_EDX,s);
 uint32_t args[4]={s,15,version,size};call_frame(f.E,0x8ea611,4,args);
 uint32_t result=(uint32_t)rx_guest_inflate_init2(s,15,version,size);rx_set_sp(f.E);
 return (int32_t)leave(f,result,0x8ea624);
}
int32_t rx_guest_inflate_end(rx_addr s){
 fx_frame f=enter(0xcc);
 if(!s)return (int32_t)leave(f,0xfffffffe,0x8ec7bf);
 reg(FX_EAX,s);if(!U32(s,0x1c))return (int32_t)leave(f,0xfffffffe,0x8ec7bf);
 reg(FX_EAX,s);if(!U32(s,0x24))return (int32_t)leave(f,0xfffffffe,0x8ec7bf);
 reg(FX_EAX,s);rx_addr state=U32(s,0x1c);reg(FX_ECX,state);SU32(f.ebp,-8,state);reg(FX_EAX,state);
 if(U32(state,0x2c)){
  reg(FX_EAX,state);reg(FX_ECX,U32(state,0x2c));reg(FX_EDX,s);reg(FX_EAX,U32(s,0x28));reg(FX_ECX,s);reg(FX_EDX,U32(s,0x24));
  uint32_t a[2]={U32(s,0x28),U32(state,0x2c)};(void)callback_checked(f.E,0x8ec777,0x8ec781,U32(s,0x24),2,a);
 }
 reg(FX_EAX,s);reg(FX_ECX,U32(s,0x1c));reg(FX_EDX,s);reg(FX_EAX,U32(s,0x28));reg(FX_ECX,s);reg(FX_EDX,U32(s,0x24));
 uint32_t a[2]={U32(s,0x28),U32(s,0x1c)};(void)callback_checked(f.E,0x8ec799,0x8ec7a3,U32(s,0x24),2,a);
 reg(FX_EAX,s);SU32(s,0x1c,0);return (int32_t)leave(f,0,0x8ec7bf);
}
