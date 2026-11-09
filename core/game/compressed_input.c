/* Incremental guest stream wrapper, complete original bodies, no registration. */
#include "wrapper_call.h"
uint32_t rx_guest_compressed_init(rx_addr s,rx_addr source,rx_addr compression,uint32_t chunk){
 const rx_addr E=rx_sp();fx_set_reg(FX_ESI,s);fx_set_reg(FX_EAX,source);fx_set_reg(FX_ECX,compression);SU32(s,4,source);SU32(s,8,compression);fx_set_reg(FX_EDX,U32(compression,0));fx_set_reg(FX_EAX,U32(fx_reg(FX_EDX),0xc));w0(E,0x80f92d,fx_reg(FX_EAX));
 if(!fx_reg(FX_EAX)){SU32(s,4,0);SU32(s,8,0);rx_set_sp(E);return 0;}
 fx_saved saved=fx_save();chunk=U32(E,4);fx_set_reg(FX_ECX,chunk);SU32(E-4,0,saved.ebx);SU32(E-8,0,saved.ebp);fx_set_reg(FX_EBP,0);SU32(s,0x14,1);SU32(s,0x18,chunk);SU32(s,0x1c,0);fx_set_reg(FX_EBX,s+0xc);SU32(E-12,0,saved.edi);const rx_addr B=E-12;
 do{
  fx_set_reg(FX_EDI,U32(s,0x18));if(U8(0,0xda67d8))wlock(B,0x80f96a,0x91600c);
  w2(B,0x80f973,U32(0,0x916358),fx_reg(FX_EDI),0x10,0);fx_set_reg(FX_EDI,fx_reg(FX_EAX));
  if(U8(0,0xda67d8))wlock(B,0x80f98c,0x9160f8);
  SU32(fx_reg(FX_EBX),0,fx_reg(FX_EDI));fx_set_reg(FX_EBP,fx_reg(FX_EBP)+1);fx_set_reg(FX_EBX,fx_reg(FX_EBX)+4);
 }while((int32_t)fx_reg(FX_EBP)<I32(s,0x14));
 fx_set_reg(FX_ECX,U32(s,4));fx_set_reg(FX_EBX,1);SU32(s,0x10,1);fx_set_reg(FX_EDX,U32(fx_reg(FX_ECX),0));fx_set_reg(FX_EAX,U32(fx_reg(FX_EDX),8));fx_set_reg(FX_EDI,s+0x24);
 w2(B,0x80f9af,fx_reg(FX_EAX),s+0x24,4,2);
 if(!fx_reg(FX_EAX)||U32(fx_reg(FX_EDI),0)==0xffffffffu){fx_restore(saved);fx_set_reg(FX_EAX,0);SU32(s,4,0);SU32(s,8,0);rx_set_sp(E);return 0;}
 fx_set_reg(FX_EAX,U32(fx_reg(FX_EDI),0));fx_set_reg(FX_EDX,U32(s,0x18));fx_set_reg(FX_ECX,U32(s,4));SU32(s,0x1c,0);
 if((int32_t)fx_reg(FX_EAX)<(int32_t)fx_reg(FX_EDX)){
  SU32(s,0x20,fx_reg(FX_EBX));fx_set_reg(FX_EDX,U32(fx_reg(FX_ECX),0));fx_set_reg(FX_EDX,U32(fx_reg(FX_EDX),8));uint32_t n=fx_reg(FX_EAX);fx_set_reg(FX_EAX,U32(s,0xc));w2(B,0x80f9e8,fx_reg(FX_EDX),fx_reg(FX_EAX),n,2);
  if(fx_reg(FX_EAX)){fx_set_reg(FX_EAX,U32(fx_reg(FX_EDI),0));goto ordinary_feed;}
  fx_set_reg(FX_ECX,U32(s,4));fx_set_reg(FX_EAX,U32(fx_reg(FX_ECX),0));fx_set_reg(FX_EDX,U32(fx_reg(FX_EAX),0xc));w0(B,0x80f9f6,fx_reg(FX_EDX));SU32(s,0x28,fx_reg(FX_EAX));fx_set_reg(FX_EAX,U32(s,8));fx_set_reg(FX_EDI,U32(fx_reg(FX_EAX),0));
 }else{
  fx_set_reg(FX_EAX,U32(fx_reg(FX_ECX),0));fx_set_reg(FX_EAX,U32(fx_reg(FX_EAX),8));uint32_t n=fx_reg(FX_EDX);fx_set_reg(FX_EDX,U32(s,0xc));w2(B,0x80fa10,fx_reg(FX_EAX),fx_reg(FX_EDX),n,2);
  if(fx_reg(FX_EAX)){fx_set_reg(FX_EAX,U32(s,0x18));goto ordinary_feed;}
  fx_set_reg(FX_ECX,U32(s,4));SU32(s,0x20,fx_reg(FX_EBX));fx_set_reg(FX_EDX,U32(fx_reg(FX_ECX),0));fx_set_reg(FX_EAX,U32(fx_reg(FX_EDX),0xc));w0(B,0x80fa21,fx_reg(FX_EAX));fx_set_reg(FX_ECX,U32(s,8));SU32(s,0x28,fx_reg(FX_EAX));fx_set_reg(FX_EDI,U32(fx_reg(FX_ECX),0));
 }
 // Original intentionally queries source lastRead a SECOND time on short data.
 fx_set_reg(FX_ECX,U32(s,4));fx_set_reg(FX_EDX,U32(fx_reg(FX_ECX),0));fx_set_reg(FX_EAX,U32(fx_reg(FX_EDX),0xc));fx_set_reg(FX_EDI,fx_reg(FX_EDI)+0x24);w0(B,0x80fa36,fx_reg(FX_EAX));
 fx_set_reg(FX_ECX,U32(s,0x1c));fx_set_reg(FX_EDX,U32(s,0xc+fx_reg(FX_ECX)*4));fx_set_reg(FX_ECX,U32(s,8));{uint32_t n=fx_reg(FX_EAX);fx_set_reg(FX_EAX,U32(fx_reg(FX_EDI),0));w2(B,0x80fa46,fx_reg(FX_EAX),fx_reg(FX_EDX),n,2);}goto done;
ordinary_feed:
 fx_set_reg(FX_ECX,U32(s,8));SU32(s,0x28,fx_reg(FX_EAX));fx_set_reg(FX_EDX,U32(fx_reg(FX_ECX),0));fx_set_reg(FX_EDX,U32(fx_reg(FX_EDX),0x24));{uint32_t n=fx_reg(FX_EAX);fx_set_reg(FX_EAX,U32(s,0x1c));fx_set_reg(FX_EAX,U32(s,0xc+fx_reg(FX_EAX)*4));w2(B,0x80fa67,fx_reg(FX_EDX),fx_reg(FX_EAX),n,2);}
done:
 {uint32_t result=fx_reg(FX_EBX);fx_restore(saved);fx_set_reg(FX_EAX,result);rx_set_sp(E);return result;}
}
uint32_t rx_guest_compressed_read(rx_addr s,rx_addr dst,uint32_t count){
 const rx_addr E=rx_sp(),B=E-24;fx_saved saved=fx_save();SU32(E-12,0,saved.ebp);fx_set_reg(FX_EBP,count);SU32(E-16,0,saved.esi);fx_set_reg(FX_ESI,s);
 if(!count){fx_set_reg(FX_EAX,1);fx_set_reg(FX_ESI,saved.esi);fx_set_reg(FX_EBP,saved.ebp);rx_set_sp(E);return 1;}
 fx_set_reg(FX_ECX,U32(s,8));fx_set_reg(FX_EAX,U32(fx_reg(FX_ECX),0));fx_set_reg(FX_EDX,dst);fx_set_reg(FX_EAX,U32(fx_reg(FX_EAX),0x1c));SU32(E-20,0,saved.ebx);SU32(E-24,0,saved.edi);
 w2(B,0x80fc3c,fx_reg(FX_EAX),dst,count,2);
 fx_set_reg(FX_ECX,U32(s,8));fx_set_reg(FX_EDX,U32(fx_reg(FX_ECX),0));fx_set_reg(FX_EAX,U32(fx_reg(FX_EDX),0x20));w0(B,0x80fc46,fx_reg(FX_EAX));fx_set_reg(FX_EBX,1);
 if(!fx_reg(FX_EAX))goto complete;
process:
 fx_set_reg(FX_ECX,U32(s,8));fx_set_reg(FX_EDX,U32(fx_reg(FX_ECX),0));fx_set_reg(FX_EDX,U32(fx_reg(FX_EDX),0x2c));fx_set_reg(FX_EAX,E-4);
 {uint32_t a[]={E-4,E-8,E+4,E+8};wc(B,0x80fc71,fx_reg(FX_EDX),4,a,4);}
 fx_set_reg(FX_ECX,U32(s,8));if(!fx_reg(FX_EAX))goto terminal;
 fx_set_reg(FX_EAX,U32(fx_reg(FX_ECX),0));fx_set_reg(FX_EDX,U32(fx_reg(FX_EAX),0x28));w0(B,0x80fc83,fx_reg(FX_EDX));
 if(fx_reg(FX_EAX)||U32(s,0x20)!=fx_reg(FX_EAX))goto remaining;
 fx_set_reg(FX_EDI,U32(s,0x24)-U32(s,0x28));fx_set_reg(FX_EAX,U32(s,0x18));fx_set_reg(FX_ECX,U32(s,4));
 if((int32_t)fx_reg(FX_EDI)<(int32_t)fx_reg(FX_EAX)){
  fx_set_reg(FX_EDX,U32(s,0x1c));SU32(s,0x20,fx_reg(FX_EBX));fx_set_reg(FX_EAX,U32(fx_reg(FX_ECX),0));fx_set_reg(FX_EDX,U32(s,0xc+fx_reg(FX_EDX)*4));fx_set_reg(FX_EAX,U32(fx_reg(FX_EAX),8));w2(B,0x80fcb7,fx_reg(FX_EAX),fx_reg(FX_EDX),fx_reg(FX_EDI),2);
  if(fx_reg(FX_EAX))goto fed_chunk;
  fx_set_reg(FX_ECX,U32(s,4));fx_set_reg(FX_EDX,U32(fx_reg(FX_ECX),0));fx_set_reg(FX_EAX,U32(fx_reg(FX_EDX),0xc));w0(B,0x80fcc5,fx_reg(FX_EAX));SU32(s,0x28,U32(s,0x28)+fx_reg(FX_EAX));fx_set_reg(FX_EAX,U32(s,0x1c)+fx_reg(FX_EBX));wring(s);fx_set_reg(FX_ECX,U32(s,8));SU32(s,0x1c,fx_reg(FX_EDX));fx_set_reg(FX_EDI,U32(fx_reg(FX_ECX),0));goto short_feed;
 }
 fx_set_reg(FX_EDX,U32(fx_reg(FX_ECX),0));fx_set_reg(FX_EDX,U32(fx_reg(FX_EDX),8));{uint32_t n=fx_reg(FX_EAX);fx_set_reg(FX_EAX,U32(s,0x1c));fx_set_reg(FX_EAX,U32(s,0xc+fx_reg(FX_EAX)*4));w2(B,0x80fd05,fx_reg(FX_EDX),fx_reg(FX_EAX),n,2);}
 if(fx_reg(FX_EAX)){fx_set_reg(FX_EDI,U32(s,0x18));goto fed_chunk;}
 fx_set_reg(FX_ECX,U32(s,4));fx_set_reg(FX_EAX,U32(fx_reg(FX_ECX),0));fx_set_reg(FX_EDX,U32(fx_reg(FX_EAX),0xc));w0(B,0x80fd13,fx_reg(FX_EDX));SU32(s,0x28,U32(s,0x28)+fx_reg(FX_EAX));fx_set_reg(FX_EAX,U32(s,0x1c)+fx_reg(FX_EBX));wring(s);fx_set_reg(FX_EAX,U32(s,8));SU32(s,0x20,fx_reg(FX_EBX));SU32(s,0x1c,fx_reg(FX_EDX));fx_set_reg(FX_EDI,U32(fx_reg(FX_EAX),0));
short_feed:
 // Repeated real lastRead call is intentional; stateful source methods observe it.
 fx_set_reg(FX_ECX,U32(s,4));fx_set_reg(FX_EDX,U32(fx_reg(FX_ECX),0));fx_set_reg(FX_EAX,U32(fx_reg(FX_EDX),0xc));fx_set_reg(FX_EDI,fx_reg(FX_EDI)+0x24);w0(B,0x80fce6,fx_reg(FX_EAX));fx_set_reg(FX_ECX,U32(s,0x1c));fx_set_reg(FX_EDX,U32(s,0xc+fx_reg(FX_ECX)*4));fx_set_reg(FX_ECX,U32(s,8));{uint32_t n=fx_reg(FX_EAX);fx_set_reg(FX_EAX,U32(fx_reg(FX_EDI),0));w2(B,0x80fd4e,fx_reg(FX_EAX),fx_reg(FX_EDX),n,2);}goto remaining;
fed_chunk:
 fx_set_reg(FX_EAX,U32(s,0x1c));SU32(s,0x28,U32(s,0x28)+fx_reg(FX_EDI));fx_set_reg(FX_EAX,fx_reg(FX_EAX)+fx_reg(FX_EBX));wring(s);fx_set_reg(FX_ECX,U32(s,8));SU32(s,0x1c,fx_reg(FX_EDX));fx_set_reg(FX_EAX,U32(fx_reg(FX_ECX),0));fx_set_reg(FX_EAX,U32(fx_reg(FX_EAX),0x24));fx_set_reg(FX_EDX,U32(s,0xc+fx_reg(FX_EDX)*4));w2(B,0x80fd4e,fx_reg(FX_EAX),fx_reg(FX_EDX),fx_reg(FX_EDI),2);
remaining:
 fx_set_reg(FX_ECX,U32(s,8));fx_set_reg(FX_EDX,U32(fx_reg(FX_ECX),0));fx_set_reg(FX_EAX,U32(fx_reg(FX_EDX),0x20));w0(B,0x80fd58,fx_reg(FX_EAX));if(fx_reg(FX_EAX))goto process;
complete:
 {uint32_t result=fx_reg(FX_EBX);SU32(s,0x2c,fx_reg(FX_EBP));fx_restore(saved);fx_set_reg(FX_EAX,result);rx_set_sp(E);return result;}
terminal:
 SU32(s,0x20,fx_reg(FX_EBX));fx_set_reg(FX_EDX,U32(fx_reg(FX_ECX),0));fx_set_reg(FX_EAX,U32(fx_reg(FX_EDX),0x20));w0(B,0x80fd79,fx_reg(FX_EAX));fx_set_reg(FX_ECX,fx_reg(FX_EBP)-fx_reg(FX_EAX));SU32(s,0x2c,fx_reg(FX_ECX));{uint32_t result=fx_reg(FX_ECX)==fx_reg(FX_EBP);fx_restore(saved);fx_set_reg(FX_EAX,result);rx_set_sp(E);return result;}
}
uint32_t rx_guest_compressed_skip(rx_addr s,int32_t count){
 const rx_addr E=rx_sp(),A=E-0x404,B=A-16;fx_saved saved=fx_save();fx_set_reg(FX_EAX,U32(0,0x9b45e4)^A);SU32(A,0x400,fx_reg(FX_EAX));SU32(A-4,0,saved.ebx);fx_set_reg(FX_EBX,(uint32_t)count);fx_set_reg(FX_EAX,(uint32_t)count);fx_set_reg(FX_EDX,count<0?0xffffffffu:0);SU32(A-8,0,saved.ebp);fx_set_reg(FX_EDX,fx_reg(FX_EDX)&0x3ff);SU32(A-12,0,saved.esi);fx_set_reg(FX_EAX,fx_reg(FX_EAX)+fx_reg(FX_EDX));fx_set_reg(FX_ESI,fx_reg(FX_EAX));SU32(A-16,0,saved.edi);fx_set_reg(FX_EDI,s);fx_set_reg(FX_EBP,U32(s,0x2c));fx_set_reg(FX_ESI,(uint32_t)((int32_t)fx_reg(FX_ESI)>>10));fx_set_reg(FX_EBX,fx_reg(FX_EBX)&0x800003ffu);if((int32_t)fx_reg(FX_EBX)<0)fx_set_reg(FX_EBX,((fx_reg(FX_EBX)-1)|0xfffffc00u)+1);
 while((int32_t)fx_reg(FX_ESI)>0){fx_set_reg(FX_EAX,U32(s,0));fx_set_reg(FX_EDX,U32(fx_reg(FX_EAX),8));fx_set_reg(FX_ECX,s);w2(B,0x80fe03,fx_reg(FX_EDX),A,0x400,2);if(!fx_reg(FX_EAX))goto finish;fx_set_reg(FX_ESI,fx_reg(FX_ESI)-1);}
 fx_set_reg(FX_EAX,U32(s,0));fx_set_reg(FX_EDX,U32(fx_reg(FX_EAX),8));fx_set_reg(FX_ECX,s);w2(B,0x80fe1b,fx_reg(FX_EDX),A,fx_reg(FX_EBX),2);if(fx_reg(FX_EAX))fx_set_reg(FX_EAX,1);
finish:
 {uint32_t result=fx_reg(FX_EAX);fx_set_reg(FX_ECX,U32(A,0x400));SU32(s,0x2c,fx_reg(FX_EBP));fx_restore(saved);fx_set_reg(FX_ECX,fx_reg(FX_ECX)^A);w0(A,0x80fe39,0x8f9efe);rx_set_sp(E);return result;}
}
