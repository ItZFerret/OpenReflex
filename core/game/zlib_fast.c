/* Hand source draft of008ef330: original guest bit reservoir, table pointers,
 * circular history and bytewise overlap-safe copies. No host inflate/memcpy. */
#include "zlib_fast.h"
#include "zlib_stackvars.h"
#include "fx_util.h"
#define V(off) U32(bp,-(off))
#define SET(off,v) SU32(bp,-(off),(v))
static uint32_t one(uint32_t n){return 1u<<(n&31);}
static void pull_byte(rx_addr bp){
 SET(0x14,V(0x14)+1);SET(0x80,V(0x80)+((uint32_t)U8(V(0x14),0)<<(V(0x8c)&31)));SET(0x8c,V(0x8c)+8);
}
static void consume(rx_addr bp,uint32_t bits){SET(0x80,V(0x80)>>(bits&31));SET(0x8c,V(0x8c)-bits);}
static void copy_byte(rx_addr bp){
 SET(0xf8,V(0xf8)+1);SET(0x2c,V(0x2c)+1);SU8(V(0x2c),0,U8(V(0xf8),0));
}
/* Consumes a history span, leaving remaining match length and op exactly as
 * original loop. Source/target pointers are both last-used-byte addresses. */
static void copy_history_span(rx_addr bp,uint32_t span){
 SET(0xe0,V(0xe0)-span);
 do{copy_byte(bp);SET(0xd4,V(0xd4)-1);}while(V(0xd4));
}
void rx_guest_inflate_fast(rx_addr stream,uint32_t out_start){
 rx_addr e=rx_sp(),bp=e-4,base=bp-0x1c0,b=base-12;fx_saved saved=fx_save();
 SU32(bp,0,saved.ebp);SU32(base-4,0,saved.ebx);SU32(base-8,0,saved.esi);SU32(base-12,0,saved.edi);fx_set_reg(FX_EBP,bp);
 for(unsigned i=0;i<0x1c0;i+=4)SU32(base,i,0xcccccccc);fx_set_reg(FX_EDI,bp);fx_set_reg(FX_ECX,0);rx_set_sp(b);
 rx_addr state=U32(stream,0x1c);SET(8,state);SET(0x14,U32(stream,0)-1);SET(0x20,V(0x14)+U32(stream,4)-5);
 SET(0x2c,U32(stream,0xc)-1);SET(0x38,V(0x2c)-(out_start-U32(stream,0x10)));SET(0x44,V(0x2c)+U32(stream,0x10)-257);
 SET(0x50,U32(state,0x20));SET(0x5c,U32(state,0x24));SET(0x68,U32(state,0x28));SET(0x74,U32(state,0x2c));SET(0x80,U32(state,0x30));SET(0x8c,U32(state,0x34));SET(0x98,U32(state,0x44));SET(0xa4,U32(state,0x48));SET(0xb0,one(U32(state,0x4c))-1);SET(0xbc,one(U32(state,0x50))-1);
 for(;;){
  if(V(0x8c)<15){pull_byte(bp);pull_byte(bp);}
  SET(0xc8,U32(V(0x98),4*(V(0x80)&V(0xb0))));
  /* Resolve literal/length root and subtable entries. The entry scratch itself
   * is guarded by RTC and remains a full original4-byte op/bits/value word. */
length_entry:
  SET(0xd4,U8(bp-0xc8,1));consume(bp,V(0xd4));SET(0xd4,U8(bp-0xc8,0));
  if(!V(0xd4)){
   SET(0x2c,V(0x2c)+1);SU8(V(0x2c),0,U8(bp-0xc8,2));goto next_symbol;
  }
  if(!(V(0xd4)&16)){
   if(!(V(0xd4)&64)){SET(0xc8,U32(V(0x98),4*(U16(bp-0xc8,2)+(V(0x80)&(one(V(0xd4))-1)))));goto length_entry;}
   if(V(0xd4)&32)SU32(state,0,11);
   else{SU32(stream,0x18,0x927718);SU32(state,0,27);}goto finish;
  }
  SET(0xe0,U16(bp-0xc8,2));SET(0xd4,V(0xd4)&15);
  if(V(0xd4)){
   if(V(0x8c)<V(0xd4))pull_byte(bp);
   SET(0xe0,V(0xe0)+(V(0x80)&(one(V(0xd4))-1)));consume(bp,V(0xd4));
  }
  if(V(0x8c)<15){pull_byte(bp);pull_byte(bp);}
  SET(0xc8,U32(V(0xa4),4*(V(0x80)&V(0xbc))));
distance_entry:
  SET(0xd4,U8(bp-0xc8,1));consume(bp,V(0xd4));SET(0xd4,U8(bp-0xc8,0));
  if(!(V(0xd4)&16)){
   if(!(V(0xd4)&64)){SET(0xc8,U32(V(0xa4),4*(U16(bp-0xc8,2)+(V(0x80)&(one(V(0xd4))-1)))));goto distance_entry;}
   SU32(stream,0x18,0x927700);SU32(state,0,27);goto finish;
  }
  SET(0xd4,V(0xd4)&15);
  if(V(0x8c)<V(0xd4)){pull_byte(bp);if(V(0x8c)<V(0xd4))pull_byte(bp);}
  SET(0xec,U16(bp-0xc8,2)+(V(0x80)&(one(V(0xd4))-1)));consume(bp,V(0xd4));SET(0xd4,V(0x2c)-V(0x38));
  if(V(0xec)>V(0xd4)){
   SET(0xd4,V(0xec)-V(0xd4));
   if(V(0xd4)>V(0x5c)){SU32(stream,0x18,0x9276e0);SU32(state,0,27);goto finish;}
   SET(0xf8,V(0x74)-1);
   if(!V(0x68)){
    SET(0xf8,V(0xf8)+V(0x50)-V(0xd4));
    if(V(0xd4)<V(0xe0)){copy_history_span(bp,V(0xd4));SET(0xf8,V(0x2c)-V(0xec));}
   }else if(V(0x68)<V(0xd4)){
    SET(0xf8,V(0xf8)+V(0x50)+V(0x68)-V(0xd4));SET(0xd4,V(0xd4)-V(0x68));
    if(V(0xd4)<V(0xe0)){
     copy_history_span(bp,V(0xd4));SET(0xf8,V(0x74)-1);
     if(V(0x68)<V(0xe0)){SET(0xd4,V(0x68));copy_history_span(bp,V(0x68));SET(0xf8,V(0x2c)-V(0xec));}
    }
   }else{
    SET(0xf8,V(0xf8)+V(0x68)-V(0xd4));
    if(V(0xd4)<V(0xe0)){copy_history_span(bp,V(0xd4));SET(0xf8,V(0x2c)-V(0xec));}
   }
   while(V(0xe0)>2){copy_byte(bp);copy_byte(bp);copy_byte(bp);SET(0xe0,V(0xe0)-3);}
   if(V(0xe0)){copy_byte(bp);if(V(0xe0)>1)copy_byte(bp);}
  }else{
   SET(0xf8,V(0x2c)-V(0xec));
   do{copy_byte(bp);copy_byte(bp);copy_byte(bp);SET(0xe0,V(0xe0)-3);}while(V(0xe0)>2);
   if(V(0xe0)){copy_byte(bp);if(V(0xe0)>1)copy_byte(bp);}
  }
next_symbol:
  if(V(0x14)>=V(0x20)||V(0x2c)>=V(0x44))break;
 }
finish:
 /* Return every unused complete input byte to next_in before exporting bit
  * reservoir. This helper changes avail counts, not caller-owned totals. */
 SET(0xe0,V(0x8c)>>3);SET(0x14,V(0x14)-V(0xe0));SET(0x8c,V(0x8c)-(V(0xe0)<<3));SET(0x80,V(0x80)&(one(V(0x8c))-1));
 SU32(stream,0,V(0x14)+1);SU32(stream,0xc,V(0x2c)+1);
 SET(0x1c0,V(0x14)<V(0x20)?V(0x20)-V(0x14)+5:5-(V(0x14)-V(0x20)));SU32(stream,4,V(0x1c0));
 SET(0x1c0,V(0x2c)<V(0x44)?V(0x44)-V(0x2c)+257:257-(V(0x2c)-V(0x44)));SU32(stream,0x10,V(0x1c0));SU32(state,0x30,V(0x80));SU32(state,0x34,V(0x8c));
 fx_set_reg(FX_EAX,state);fx_set_reg(FX_ECX,V(0x8c));fx_set_reg(FX_EDX,V(0x1c0));SU32(b-4,0,fx_reg(FX_EDX));SU32(b-8,0,state);
 fx_set_reg(FX_ECX,bp);fx_set_reg(FX_EDX,0x8efd50);SU32(b-12,0,0x8efd46);rx_set_sp(b-12);rx_guest_inflate_check_stackvars(bp,0x8efd50);fx_set_reg(FX_EAX,U32(b-8,0));fx_set_reg(FX_EDX,U32(b-4,0));fx_restore(saved);rx_set_sp(e);
}
