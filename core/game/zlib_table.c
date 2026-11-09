/* Hand-written original1.2.1 canonical Huffman table builder. Guest local arrays
 * stay on the actual original debug frame, including their guarded boundaries.
 * No host zlib tables, malformed-input normalization, or registration. */
#include "zlib_table.h"
#include "fx_util.h"
#include "phys_crt.h"
#include "zlib_stackvars.h"
#define L(off) U32(bp,-(off))
#define S(off,v) SU32(bp,-(off),(v))
#define D(v) fx_set_reg(FX_EDX,(v))
static uint32_t shift_one(uint32_t bits){return 1u<<(bits&31);}
static void stackvars_at(rx_addr caller,rx_addr bp){
 fx_set_reg(FX_ECX,bp);fx_set_reg(FX_EDX,0x8f0478);
 SU32(caller-4,0,0x8f0464);rx_set_sp(caller-4);
 rx_guest_inflate_check_stackvars(bp,0x8f0478);rx_set_sp(caller);
}
int32_t rx_guest_inflate_table(uint32_t kind,rx_addr lens,uint32_t codes,rx_addr tablep,rx_addr bitsp,rx_addr work){
 const rx_addr e=rx_sp(),bp=e-4,base=bp-0x1fc,b=base-12;
 const fx_saved saved=fx_save();SU32(bp,0,saved.ebp);SU32(base-4,0,saved.ebx);SU32(base-8,0,saved.esi);SU32(base-12,0,saved.edi);fx_set_reg(FX_EBP,bp);
 for(unsigned i=0;i<0x1fc;i+=4)SU32(base,i,0xcccccccc);fx_set_reg(FX_EDI,bp);fx_set_reg(FX_ECX,0);rx_set_sp(b);
 S(4,U32(0,0x9b45e4)^bp);int32_t result=-1;
 /* Count lengths and determine requested/minimum/maximum root width. */
 for(S(0xc,0);L(0xc)<=15;S(0xc,L(0xc)+1))SU16(bp-0x10c,2*L(0xc),0);
 for(S(0x18,0);L(0x18)<codes;S(0x18,L(0x18)+1)){
  uint32_t n=U16(lens,2*L(0x18));SU16(bp-0x10c,2*n,(uint16_t)(U16(bp-0x10c,2*n)+1));D(lens);
 }
 S(0x3c,U32(bitsp,0));S(0x30,15);
 while(L(0x30)>=1&&!U16(bp-0x10c,2*L(0x30)))S(0x30,L(0x30)-1);
 if(L(0x3c)>L(0x30))S(0x3c,L(0x30));if(!L(0x30))goto done;
 S(0x24,1);while(L(0x24)<=15&&!U16(bp-0x10c,2*L(0x24)))S(0x24,L(0x24)+1);
 if(L(0x3c)<L(0x24))S(0x3c,L(0x24));
 S(0x60,1);for(S(0xc,1);L(0xc)<=15;S(0xc,L(0xc)+1)){
  S(0x60,L(0x60)<<1);uint32_t left=L(0x60)-U16(bp-0x10c,2*L(0xc));D(left);S(0x60,left);if((int32_t)left<0)goto done;
 }
 if((int32_t)L(0x60)>0&&(kind==0||codes-U16(bp-0x10c,0)!=1))goto done;
 /* Stable sorting of actual symbol numbers by length. */
 SU16(bp-0x134,2,0);for(S(0xc,1);L(0xc)<15;S(0xc,L(0xc)+1)){
  uint16_t next=(uint16_t)(U16(bp-0x134,2*L(0xc))+U16(bp-0x10c,2*L(0xc)));D(L(0xc));SU16(bp-0x134,2*(L(0xc)+1),next);
 }
 for(S(0x18,0);L(0x18)<codes;S(0x18,L(0x18)+1)){
  uint32_t n=U16(lens,2*L(0x18));D(n);if(n){uint16_t at=U16(bp-0x134,2*n);SU16(work,2*at,(uint16_t)L(0x18));SU16(bp-0x134,2*n,(uint16_t)(at+1));D(lens);}
 }
 S(0x1fc,kind);
 if(kind==0){S(0xd8,work);S(0xcc,work);S(0xe4,19);}
 else if(kind==1){S(0xcc,0x927a00-0x202);S(0xd8,0x927a40-0x202);S(0xe4,256);}
 else{S(0xcc,0x927a80);S(0xd8,0x927ac0);S(0xe4,0xffffffff);}
 S(0x78,0);S(0x18,0);S(0xc,L(0x24));S(0xc0,U32(tablep,0));S(0x48,L(0x3c));S(0x54,0);S(0x9c,0xffffffff);S(0x6c,shift_one(L(0x3c)));S(0xa8,L(0x6c)-1);
 if(kind==1&&L(0x6c)>=1286){result=1;goto done;}
 for(;;){
  /* Construct op/bits/value before replicating this reversed-bit code. */
  SU8(bp-0xb4,1,(uint8_t)(L(0xc)-L(0x54)));uint32_t symbol=U16(work,2*L(0x18));D(symbol);
  if((int32_t)symbol<(int32_t)L(0xe4)){SU8(bp-0xb4,0,0);SU16(bp-0xb4,2,(uint16_t)symbol);}
  else if((int32_t)symbol>(int32_t)L(0xe4)){SU8(bp-0xb4,0,U8(L(0xd8),2*symbol));SU16(bp-0xb4,2,U16(L(0xcc),2*symbol));}
  else{SU8(bp-0xb4,0,0x60);SU16(bp-0xb4,2,0);}
  S(0x84,shift_one(L(0xc)-L(0x54)));S(0x90,shift_one(L(0x48)));
  do{S(0x90,L(0x90)-L(0x84));uint32_t code=L(0xb4);D(code);SU32(L(0xc0),4*((L(0x78)>>(L(0x54)&31))+L(0x90)),code);}while(L(0x90));
  S(0x84,shift_one(L(0xc)-1));while(L(0x78)&L(0x84))S(0x84,L(0x84)>>1);
  S(0x78,L(0x84)?(L(0x78)&(L(0x84)-1))+L(0x84):0);S(0x18,L(0x18)+1);
  SU16(bp-0x10c,2*L(0xc),(uint16_t)(U16(bp-0x10c,2*L(0xc))-1));D(L(0xc));
  if(!U16(bp-0x10c,2*L(0xc))){
   if(L(0xc)==L(0x30))break;
   uint32_t next=U16(work,2*L(0x18));D(next);S(0xc,U16(lens,2*next));
  }
  if(L(0xc)<=L(0x3c)||(L(0x78)&L(0xa8))==L(0x9c))continue;
  if(!L(0x54))S(0x54,L(0x3c));S(0xc0,L(0xc0)+4*shift_one(L(0x48)));D(L(0xc0));S(0x48,L(0xc)-L(0x54));S(0x60,shift_one(L(0x48)));
  while(L(0x48)+L(0x54)<L(0x30)){
   uint32_t left=L(0x60)-U16(bp-0x10c,2*(L(0x48)+L(0x54)));D(left);S(0x60,left);if((int32_t)left<=0)break;S(0x48,L(0x48)+1);S(0x60,L(0x60)<<1);
  }
  S(0x6c,L(0x6c)+shift_one(L(0x48)));if(kind==1&&L(0x6c)>=1286){result=1;goto done;}
  S(0x9c,L(0x78)&L(0xa8));D(L(0x9c));SU8(U32(tablep,0),4*L(0x9c),(uint8_t)L(0x48));SU8(U32(tablep,0),4*L(0x9c)+1,(uint8_t)L(0x3c));SU16(U32(tablep,0),4*L(0x9c)+2,(uint16_t)((int32_t)(L(0xc0)-U32(tablep,0))>>2));
 }
 /* Fill unused reversed codes with invalid entries, including root fallback. */
 SU8(bp-0xb4,0,0x40);SU8(bp-0xb4,1,(uint8_t)(L(0xc)-L(0x54)));SU16(bp-0xb4,2,0);
 while(L(0x78)){
  if(L(0x54)&&(L(0x78)&L(0xa8))!=L(0x9c)){S(0x54,0);S(0xc,L(0x3c));S(0xc0,U32(tablep,0));S(0x48,L(0x3c));SU8(bp-0xb4,1,(uint8_t)L(0xc));}
  D(L(0xb4));SU32(L(0xc0),4*(L(0x78)>>(L(0x54)&31)),L(0xb4));S(0x84,shift_one(L(0xc)-1));while(L(0x78)&L(0x84))S(0x84,L(0x84)>>1);S(0x78,L(0x84)?(L(0x78)&(L(0x84)-1))+L(0x84):0);
 }
 D(L(0x6c));SU32(tablep,0,U32(tablep,0)+4*L(0x6c));SU32(bitsp,0,L(0x3c));result=0;
done:
 /* Original preserves EAX/EDX around CheckStackVars, then restores saved regs
  * before security-cookie validation. Neither failure path is bypassed. */
 fx_set_reg(FX_EAX,(uint32_t)result);SU32(b-4,0,fx_reg(FX_EDX));SU32(b-8,0,(uint32_t)result);stackvars_at(b-8,bp);fx_set_reg(FX_EAX,U32(b-8,0));D(U32(b-4,0));fx_restore(saved);fx_set_reg(FX_EBP,bp);
 uint32_t cookie=U32(bp,-4)^bp;fx_set_reg(FX_ECX,cookie);SU32(base-4,0,0x8f0473);rx_set_sp(base-4);phys_security_check_cookie(cookie);fx_set_reg(FX_EBP,saved.ebp);rx_set_sp(e);return result;
}
