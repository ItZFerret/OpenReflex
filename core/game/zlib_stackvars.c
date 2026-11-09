/* Exact checked CRT stack-descriptor walker used by inflate_table/fast.
 * No registration and no claim to port _RTC_StackFailure diagnostics. */
#include "zlib_stackvars.h"
#include "phys_crt.h"
#include "fx_util.h"
void rx_guest_inflate_check_stackvars(rx_addr frame,rx_addr desc){
 const rx_addr entry=rx_sp(),bp=entry-4;fx_saved in=fx_save();
 SU32(bp,0,in.ebp);SU32(bp,-4,frame);SU32(bp,-8,frame);SU32(bp,-12,in.ebx);SU32(bp,-16,in.esi);
 fx_set_reg(FX_EBP,bp);fx_set_reg(FX_ESI,desc);fx_set_reg(FX_EBX,0);fx_set_reg(FX_EDX,frame);SU32(bp,-8,frame);SU32(bp,-4,0);rx_set_sp(bp-16);
 if(I32(desc,0)>0){
  SU32(bp,-20,in.edi);fx_set_reg(FX_EDI,0xcccccccc);rx_set_sp(bp-20);
  do{
   rx_addr item=U32(desc,4)+fx_reg(FX_EBX);fx_set_reg(FX_EAX,item);uint32_t offset=U32(item,0);fx_set_reg(FX_ECX,offset);
   int failed=U32(frame+offset,-4)!=0xcccccccc;
   if(!failed){uint32_t end=U32(item,4)+offset;fx_set_reg(FX_EAX,end);failed=U32(frame+end,0)!=0xcccccccc;}
   if(failed){
    /* Original calls the actual report routine, passing callsite then variable
     * name as cdecl words. No fabricated successful malformed-state result. */
    item=U32(desc,4)+fx_reg(FX_EBX);fx_set_reg(FX_EAX,U32(desc,4));uint32_t args[2]={U32(entry,0),U32(item,8)};rx_addr caller=rx_sp();
    SU32(caller,-4,args[1]);SU32(caller,-8,args[0]);SU32(caller,-12,0x8fa425);rx_set_sp(caller);
    rx_set_sp(caller-12);rx_rtc_stack_failure(args[0],args[1]);SU32(caller,-12,0x8fa425);rx_set_sp(caller);fx_set_reg(FX_EDX,U32(bp,-8));
    fx_set_reg(FX_ECX,U32(caller,-8));fx_set_reg(FX_ECX,U32(caller,-4));
   }
   SU32(bp,-4,U32(bp,-4)+1);fx_set_reg(FX_EAX,U32(bp,-4));fx_set_reg(FX_EBX,fx_reg(FX_EBX)+12);
  }while((int32_t)fx_reg(FX_EAX)<I32(desc,0));
  fx_set_reg(FX_EDI,in.edi);
 }
 fx_restore(in);rx_set_sp(entry);
}
