#pragma once
#include "fx_util.h"
#include "phys_math.h"
#include <stdlib.h>
static inline int rx_ts_overlap(rx_addr a,uint64_t n,rx_addr b,uint64_t m){return n&&m&&(uint64_t)a+n>b&&(uint64_t)b+m>a;}
static inline void rx_ts_domain(rx_addr entry,rx_addr B,rx_addr PS,rx_addr QT,rx_addr M,rx_addr T,rx_addr PT,rx_addr S,rx_addr QS){
 /* Actual nested85ee70 entry is B-1c; EBP=B-20 (already16 aligned),
  * SUB5b4/three saves yields B-5e0. No source or output aliases scratch. */
 if(B<0x5e0||entry<=B)abort();
 const rx_addr low=B-0x5e0;const uint64_t scratch=(uint64_t)entry-low;
 rx_addr a[]={PS,QT,T,PT,S,QS,U32(T,0x24),U32(S,0x24)};
 uint64_t n[]={16,16,0x28,16,0x28,16,I32(T,0x20)>0?48ull*(uint32_t)I32(T,0x20):0,I32(S,0x20)>0?32ull*(uint32_t)I32(S,0x20):0};
 if(rx_ts_overlap(M,0x6a4,low,scratch)||(uint64_t)M+0x6a4>0x100000000ull)abort();
 for(unsigned i=0;i<8;i++)if((uint64_t)a[i]+n[i]>0x100000000ull||rx_ts_overlap(a[i],n[i],low,scratch)||rx_ts_overlap(a[i],n[i],M,0x6a4))abort();
}
/* Complete original outer frame lifetime; not a numerical-body adapter. */
typedef struct rx_tri_segment_frame {fx_frame saved;rx_addr base;int triangle_frame;} rx_tri_segment_frame;
static inline void rx_tri_segment_frame_enter(rx_tri_segment_frame*f,rx_addr PS,rx_addr QT,rx_addr M,rx_addr T,rx_addr PT,rx_addr S,rx_addr QS,uint32_t flag){
 rx_addr entry=rx_sp();
 if(fx_reg(FX_ECX)!=PS||fx_reg(FX_EDX)!=QT||fx_reg(FX_ESI)!=M||U32(entry,4)!=T||U32(entry,8)!=PT||U32(entry,12)!=S||U32(entry,16)!=QS||U32(entry,20)!=flag)abort();
 rx_ts_domain(entry,((entry-4)&~15u)-0x490,PS,QT,M,T,PT,S,QS);
 fx_frame_enter(&f->saved,0x488,16,FX_EBX,FX_EDI);f->base=f->saved.E;rx_addr B=f->base;
 SU32(B,0x48c,U32(0,0x9b45e4)^(B+8));
 /* Source86fadc..86fbb2: real retained source pointers/counts. Quaternion
  * stages subsequently overwrite several words; do not zero the frame. */
 SU32(B,0x9c,T);SU32(B,0x80,S);SU32(B,0x38,U32(S,0x20));SU32(B,0x68,U32(T,0x20));
 SU32(B,0x70,QT);SU32(B,0x40,PT);SU32(B,0xa0,QS);SU32(B,0xa4,PS);
 f->triangle_frame=I32(T,0x20)>=I32(S,0x20);
}
static inline void rx_tri_segment_frame_leave(rx_tri_segment_frame*f,int transformed_new_contacts){
 const rx_addr B=f->base;
 /* First branch POPs both registers before loading cookie; second branch
  * loads cookie first. The triangle-frame no-new-contact path jumps to the
  * second epilogue too (8707b8 ->871696). No observer/callback interleaves those instructions. */
 if(rx_sp()!=B)abort();
 fx_set_reg(FX_EDI,f->saved.in.edi);fx_set_reg(FX_EBX,f->saved.in.ebx);
 fx_set_reg(FX_ECX,U32(B,0x48c)^(B+8));
 if(fx_reg(FX_ECX)!=U32(0,0x9b45e4))abort();
 rx_set_sp(B+8);SU32(B+4,0,(f->triangle_frame&&transformed_new_contacts)?0x8708da:0x8716a6);rx_set_sp(B+4);
 phys_security_check_cookie(fx_reg(FX_ECX));rx_set_sp(B+8);fx_frame_leave(&f->saved);
}
