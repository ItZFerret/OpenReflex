/* Complete hand008ec530 updatewindow; no registration while RTC/lifecycle
 * dependency and genuine direct fixtures remain unverified. */
#include "zlib_inflate_helpers.h"
#include "zlib_lifecycle.h"
#include "fx_util.h"
#include "crt.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef RX_ORACLE
extern uint32_t rx_zlib_lifecycle_callback(rx_addr,unsigned,const uint32_t*);
#endif
#define REG(i,v) fx_set_reg(i,v)
#define GET(i) fx_reg(i)
static void frame_call(rx_addr b,rx_addr ra,unsigned n,const uint32_t*a){
 rx_addr e=b-4*(n+1);SU32(e,0,ra);for(unsigned i=0;i<n;i++)SU32(e,4+4*i,a[i]);rx_set_sp(e);
}
static void copy_at(rx_addr b,rx_addr ra,rx_addr dst,rx_addr src,uint32_t size){
 uint32_t args[3]={dst,src,size};frame_call(b,ra,3,args);
#ifdef RX_ORACLE
 rx_set_sp(b);rx_call_original(0x8fa070,RX_CDECL,0,3,args);SU32(b-16,0,ra);
#else
 crt_memcpy(dst,src,size);
#endif
 rx_set_sp(b);
}
static rx_addr window_alloc(rx_addr b,rx_addr stream,rx_addr state){
 REG(6,b);REG(0,state);REG(2,1);REG(1,U32(state,0x1c));
 uint32_t size=1u<<(U32(state,0x1c)&31);REG(2,size);REG(0,stream);REG(1,U32(stream,0x28));REG(2,stream);REG(0,U32(stream,0x20));
 uint32_t args[3]={U32(stream,0x28),size,1};frame_call(b,0x8ec581,3,args);rx_addr p;
#ifdef RX_ORACLE
 rx_set_sp(b);p=rx_zlib_lifecycle_callback(U32(stream,0x20),3,args);SU32(b-16,0,0x8ec581);
#else
 rx_addr callback=U32(stream,0x20);
 if(callback==0x8ef290)p=rx_guest_zalloc(args[0],args[1],args[2]);
 else{void*f=rx_fn(callback);if(f)p=((rx_addr(*)(rx_addr,uint32_t,uint32_t))f)(args[0],args[1],args[2]);else{rx_set_sp(b);p=rx_call_original(callback,RX_CDECL,0,3,args);}}
 if(rx_sp()!=b-16){fprintf(stderr,"rxcore: updatewindow callback callee ESP changed\n");abort();}rx_set_sp(b-12);
#endif
 rx_addr after=rx_sp()+12;rx_set_sp(after);rx_guest_zlib_rtc(after,0x8ec58b,GET(6),after);return p;
}
int32_t rx_guest_inflate_updatewindow(rx_addr stream,uint32_t out_start){
 rx_addr e=rx_sp(),bp=e-4,locals=bp-0xe4,b=locals-12;uint32_t oldbp=GET(5),bx=GET(3),si=GET(6),di=GET(7);
 SU32(bp,0,oldbp);SU32(locals-4,0,bx);SU32(locals-8,0,si);SU32(locals-12,0,di);REG(5,bp);
 for(unsigned i=0;i<0xe4;i+=4)SU32(locals,i,0xcccccccc);REG(7,bp);REG(1,0);rx_set_sp(b);
 REG(0,stream);rx_addr state=U32(stream,0x1c);REG(1,state);SU32(bp,-8,state);uint32_t result=0;
 REG(0,state);if(!U32(state,0x2c)){
  rx_addr p=window_alloc(b,stream,state);REG(1,state);SU32(state,0x2c,p);REG(0,state);
  if(!p){result=1;goto done;}
 }
 REG(0,state);if(!U32(state,0x20)){
  REG(0,state);REG(2,1);REG(1,U32(state,0x1c));uint32_t size=1u<<(U32(state,0x1c)&31);REG(2,size);REG(0,state);SU32(state,0x20,size);REG(0,state);SU32(state,0x28,0);REG(0,state);SU32(state,0x24,0);
 }
 REG(0,stream);uint32_t count=out_start-U32(stream,0x10);REG(1,count);SU32(bp,-0x14,count);REG(0,state);REG(1,count);
 if(count>=U32(state,0x20)){
  REG(0,state);uint32_t size=U32(state,0x20);REG(1,size);REG(2,stream);REG(0,state);rx_addr src=U32(stream,0xc)-size;REG(1,src);REG(2,state);REG(0,U32(state,0x2c));
  copy_at(b,0x8ec60b,U32(state,0x2c),src,size);REG(0,state);SU32(state,0x28,0);REG(0,state);REG(1,state);REG(2,U32(state,0x20));SU32(state,0x24,U32(state,0x20));
 }else{
  uint32_t first=U32(state,0x20)-U32(state,0x28);REG(0,state);REG(1,state);REG(2,first);SU32(bp,-0x20,first);REG(0,first);
  if(first>count){first=count;REG(0,count);SU32(bp,-0x20,first);}
  REG(0,first);REG(1,stream);rx_addr src=U32(stream,0xc)-count;REG(2,src);REG(0,state);REG(1,U32(state,0x2c));REG(2,state);REG(1,U32(state,0x2c)+U32(state,0x28));
  copy_at(b,0x8ec666,U32(state,0x2c)+U32(state,0x28),src,first);count-=first;REG(0,count);SU32(bp,-0x14,count);
  if(count){
   REG(0,count);REG(1,stream);src=U32(stream,0xc)-count;REG(2,src);REG(0,state);REG(1,U32(state,0x2c));copy_at(b,0x8ec68e,U32(state,0x2c),src,count);
   REG(0,state);REG(1,count);SU32(state,0x28,count);REG(0,state);REG(1,state);REG(2,U32(state,0x20));SU32(state,0x24,U32(state,0x20));
  }else{
   REG(0,state);REG(1,U32(state,0x28)+first);REG(2,state);SU32(state,0x28,U32(state,0x28)+first);REG(0,state);REG(1,state);REG(2,U32(state,0x28));
   if(U32(state,0x28)==U32(state,0x20)){REG(0,state);SU32(state,0x28,0);}
   REG(0,state);REG(1,state);REG(2,U32(state,0x24));if(U32(state,0x24)<U32(state,0x20)){REG(0,state);REG(1,U32(state,0x24)+first);REG(2,state);SU32(state,0x24,U32(state,0x24)+first);}
  }
 }
done:
 REG(0,result);REG(7,di);REG(6,si);REG(3,bx);rx_set_sp(bp);rx_guest_zlib_rtc(bp,0x8ec6fe,bp,rx_sp());REG(5,oldbp);rx_set_sp(e);return (int32_t)result;
}
