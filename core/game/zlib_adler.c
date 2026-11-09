/* Hand-written008f04c0. Exact 1.2.1 executable arithmetic/chunk order plus
 * debug frame and consumed argument writes, no host checksum substitution. */
#include "zlib_inflate_helpers.h"
#include "fx_util.h"
#define REG(i,v) fx_set_reg(i,v)
#define GET(i) fx_reg(i)
uint32_t rx_guest_adler32(uint32_t checksum,rx_addr bytes,uint32_t length){
 const rx_addr entry=rx_sp(),frame=entry-4,base=frame-0xe8;
 const uint32_t bp=GET(5),bx=GET(3),si=GET(6),di=GET(7);
 SU32(frame,0,bp);SU32(base-4,0,bx);SU32(base-8,0,si);SU32(base-12,0,di);REG(5,frame);
 /* Original rep stosd fills only locals, not the saved-register words. */
 for(unsigned i=0;i<0xe8;i+=4)SU32(base,i,0xcccccccc);
 REG(7,frame);REG(1,0);rx_set_sp(base-12);
 uint32_t low=checksum&0xffffu,high=(checksum>>16)&0xffffu;
 SU32(frame-8,0,low);SU32(frame-0x14,0,high);
 uint32_t result;
 if(!bytes){result=1;goto done;}
 while(length){
  const uint32_t block=length<5552?length:5552;uint32_t remaining=block;
  SU32(base,0,block);SU32(frame-0x20,0,block);
  length-=block;SU32(entry,0xc,length); /* original consumes caller arg length */
  while(remaining>=16){
   /* The source is unrolled16 times: preserve each uint32 addition/store order
    * before advancing consumed pointer once by16. No modulus reassociation. */
   for(unsigned i=0;i<16;i++){
    low+=U8(bytes,i);SU32(frame-8,0,low);high+=low;SU32(frame-0x14,0,high);
   }
   bytes+=16;SU32(entry,8,bytes);remaining-=16;SU32(frame-0x20,0,remaining);
  }
  while(remaining){
   low+=U8(bytes,0);SU32(frame-8,0,low);bytes++;SU32(entry,8,bytes);
   high+=low;SU32(frame-0x14,0,high);--remaining;SU32(frame-0x20,0,remaining);
  }
  low%=65521u;SU32(frame-8,0,low);high%=65521u;SU32(frame-0x14,0,high);
  REG(2,high);REG(1,65521);
 }
 result=(high<<16)|low;
done:
 REG(0,result);REG(7,di);REG(6,si);REG(3,bx);REG(5,bp);rx_set_sp(entry);return result;
}
