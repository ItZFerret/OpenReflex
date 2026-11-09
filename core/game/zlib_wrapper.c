/* Complete original object methods. Deflate branch stays a real dependency;
 * it is not removed merely because the reader decode path uses mode2. */
#include "wrapper_call.h"
uint32_t rx_guest_zcompression_init(rx_addr s){
 rx_addr E=rx_sp(),B=E-4;uint32_t si=fx_reg(FX_ESI);SU32(B,0,si);fx_set_reg(FX_ESI,s);
 if(U32(s,4)){fx_set_reg(FX_EAX,0);fx_set_reg(FX_ESI,si);rx_set_sp(E);return 0;}
 fx_set_reg(FX_EAX,s+8);uint32_t a[]={s+8,0x9751dc,0x38};wc(B,0x829d2d,0x8ea5e0,3,a,0);
 SU32(s,4,2);fx_set_reg(FX_EAX,1);fx_set_reg(FX_ESI,si);rx_set_sp(E);return 1;
}
uint32_t rx_guest_zcompression_close(rx_addr s){
 rx_addr E=rx_sp(),B=E-4;uint32_t si=fx_reg(FX_ESI);SU32(B,0,si);fx_set_reg(FX_ESI,s);
 if(U32(s,4)!=2){fx_set_reg(FX_EAX,0);fx_set_reg(FX_ESI,si);rx_set_sp(E);return 0;}
 uint32_t bx=fx_reg(FX_EBX),di=fx_reg(FX_EDI);SU32(E-8,0,bx);fx_set_reg(FX_EBX,U32(s,0x1c));SU32(E-12,0,di);fx_set_reg(FX_EDI,s+8);B=E-12;
 w1(B,0x829d5b,0x8ec710,s+8,0);uint32_t a[]={s+8,0,0x38};wc(B-4,0x829d65,0x8fa06a,3,a,0); // previous cdecl argument retained until addESP10
 uint32_t result=fx_reg(FX_EBX);fx_set_reg(FX_EDI,di);fx_set_reg(FX_EAX,result);fx_set_reg(FX_EBX,bx);SU32(s,4,0);SU32(s,0x40,0);fx_set_reg(FX_ESI,si);rx_set_sp(E);return result;
}
uint32_t rx_guest_zcompression_process(rx_addr s,rx_addr out,rx_addr in,rx_addr totalout,rx_addr totalin){
 const rx_addr E=rx_sp(),B=E-16;fx_saved saved=fx_save();SU32(E-4,0,saved.ebx);SU32(E-8,0,saved.ebp);SU32(E-12,0,saved.esi);fx_set_reg(FX_ESI,s);fx_set_reg(FX_EAX,U32(s,4));fx_set_reg(FX_EBX,U32(s,0xc));SU32(E-16,0,saved.edi);fx_set_reg(FX_EDI,U32(s,0x18));fx_set_reg(FX_EBP,1);
 if(fx_reg(FX_EAX)==2){fx_set_reg(FX_EAX,s+8);w2(B,0x829eb4,0x8ea630,s+8,0,0);if(fx_reg(FX_EAX))fx_set_reg(FX_EBP,0);}
 /* BOUNDARY (not reached): compression mode 1 (deflate, a writer): the core's only ZCompression initialiser (rx_guest_zcompression_init, 00829d10) sets mode 2 (inflate), so mode 1 never exists in the product and the original's reference world executes 0 of these bytes. The inflate side (mode 2) is native (zlib_inflate.c, native_bridge.c). */
 else if(fx_reg(FX_EAX)==1){uint32_t flush;if(U32(s,0x40)){flush=4;fx_set_reg(FX_ECX,s+8);}else{flush=0;fx_set_reg(FX_EDX,s+8);}w2(B,0x829ed4,0x8ecdf0,s+8,flush,0);if(fx_reg(FX_EAX))fx_set_reg(FX_EBP,0);}
 out=U32(E,4);fx_set_reg(FX_EAX,out);if(out){fx_set_reg(FX_EDI,fx_reg(FX_EDI)-U32(s,0x18));SU32(out,0,fx_reg(FX_EDI));}
 in=U32(E,8);fx_set_reg(FX_EAX,in);if(in){fx_set_reg(FX_EBX,fx_reg(FX_EBX)-U32(s,0xc));SU32(in,0,fx_reg(FX_EBX));}
 totalout=U32(E,0xc);fx_set_reg(FX_EAX,totalout);if(totalout){fx_set_reg(FX_ECX,U32(s,0x1c));SU32(totalout,0,fx_reg(FX_ECX));}
 totalin=U32(E,0x10);fx_set_reg(FX_EAX,totalin);if(totalin){fx_set_reg(FX_EDX,U32(s,0x10));SU32(totalin,0,fx_reg(FX_EDX));}
 uint32_t result=fx_reg(FX_EBP);fx_restore(saved);fx_set_reg(FX_EAX,result);rx_set_sp(E);return result;
}
