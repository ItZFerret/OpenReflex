#pragma once
#include "compressed_input.h"
#include "fx_util.h"
static inline void wc(rx_addr B,rx_addr ra,rx_addr fn,unsigned n,const uint32_t*args,unsigned pop){
 rx_addr E=B-4u*(n+1);SU32(E,0,ra);for(unsigned i=0;i<n;i++)SU32(E,4u*(i+1),args[i]);rx_set_sp(E);rx_wrapper_dependency(fn,B-4u*(n-pop));rx_set_sp(B);
}
static inline void w0(rx_addr B,rx_addr ra,rx_addr fn){wc(B,ra,fn,0,0,0);}
static inline void w1(rx_addr B,rx_addr ra,rx_addr fn,uint32_t a,unsigned pop){wc(B,ra,fn,1,&a,pop);}
static inline void w2(rx_addr B,rx_addr ra,rx_addr fn,uint32_t a,uint32_t b,unsigned pop){uint32_t v[]={a,b};wc(B,ra,fn,2,v,pop);}
static inline void wlock(rx_addr B,rx_addr ra,rx_addr slot){w1(B,ra,U32(0,slot),0xd61704,1);}
static inline void wring(rx_addr s){
 int64_t n=(int32_t)fx_reg(FX_EAX),d=I32(s,0x14);
 if(!d||(n==INT32_MIN&&d==-1)){rx_wrapper_divide_error();return;}
 fx_set_reg(FX_EAX,(uint32_t)(int32_t)(n/d));fx_set_reg(FX_EDX,(uint32_t)(int32_t)(n%d));
}
