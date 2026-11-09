/* Complete record-derived scan of007647f0..00764b82. The four original
 * scratch bitsets have no consumer: they must NOT become invented rewards.
 * Only an actual race holeshot advances highlight index37, once at leave.
 * Functional native composition; incidental CPU/dead-stack parity unproved. */
#include "records_leave_highlights.h"
#include "build_tables.h"
#include "build_vehicle.h"
#include "highlight_progress.h"
#include "race_extern.h"
#include "crt.h"
#include "fx_util.h"
#ifdef RX_ORACLE
/* Oracle: the two calls of the loop run the originals with the original's registers at the frame's ESP (the callee's pushes and
 * return address land where the original's do). 004eb830 (EAX = column; the table lock around the row count): EBX = the column
 * vector's begin, ECX = the slot record, EDX = the column count, EBP = the row, ESI = the proxy, EDI = the
 * _invalid_parameter_noinfo pointer [0x916224]. */
static uint32_t rx_records_original_count(rx_addr frame,rx_addr table,rx_addr slot_record,uint32_t row){
 const uint32_t begin=U32(table,0x10);
 rx_regs in=fx_regs(),out;
 in.r[FX_EAX]=U32(begin,0);in.r[FX_ECX]=slot_record;in.r[FX_EDX]=(uint32_t)((int32_t)(U32(table,0x14)-begin)>>2);
 in.r[FX_EBX]=begin;in.r[FX_EBP]=row;in.r[FX_ESI]=U32(U32(table,4),0);in.r[FX_EDI]=U32(0,0x00916224);
 const rx_addr sp=rx_sp();rx_set_sp(frame);
 rx_call_original_ex(0x004eb830,&in,&out,0,0);
 rx_set_sp(sp);
 return out.r[FX_EAX];
}
/* 00764b60: MXHighlightTracking_AddProgress 006d7980 (stdcall tracking [0xd92edc], slot, 0x25) with EAX = the slot record,
 * ECX = slot, EDX = the tracking object, EBX / ESI / EDI from the model, EBP = the row count reached. */
static void rx_records_original_highlight(rx_addr frame,rx_addr slot_record,uint32_t slot,uint32_t rows){
 rx_regs in=fx_regs();
 in.r[FX_EAX]=slot_record;in.r[FX_ECX]=slot;in.r[FX_EDX]=U32(0,0x00d92edc);in.r[FX_EBP]=rows;
 const uint32_t a[3]={U32(0,0x00d92edc),slot,0x25};
 const rx_addr sp=rx_sp();rx_set_sp(frame);
 rx_call_original_ex(0x006d7980,&in,0,3,a);
 rx_set_sp(sp);
}
#endif
static uint32_t col(rx_addr t,rx_addr name){uint32_t c;RACE_CORE_CALL(1,c=rx_tbl_column_index(t,name));return c;}
static rx_addr value(rx_addr t,rx_addr name,uint32_t row){const uint32_t c=col(t,name);rx_addr p;RACE_CORE_CALL(1,p=rx_tbl_cell_word(t,c,row));return p;}
void rx_records_leave_highlights(rx_addr records,uint32_t slot){
 RACE_FRAME_PLAIN(F,0x308c,4);
 /* push ebx / ebp / esi / edi below the alloca (restored on return: callee-saved) */
 const fx_saved saved=fx_save();
 SU32(F,0xc,saved.ebx);SU32(F,8,saved.ebp);SU32(F,4,saved.esi);SU32(F,0,saved.edi);
 /* Actual alloca layout after four saved registers. */
 crt_memset(F+0x28,0,0x190);crt_memset(F+0x1b8,0,0xfa0);
 crt_memset(F+0x20f8,0,0xfa0);crt_memset(F+0x1158,0,0xfa0);
 const rx_addr rs=records+slot*0x18;SU32(F,0x18,rs);
#ifdef RX_ORACLE
 fx_set_reg(FX_EBP,0); /* actual XOR EBP,EBP after array initialization */
#endif
 uint32_t row=0;
 for(;;++row){
  const rx_addr t=U32(rs,0);const uint32_t begin=U32(t,0x10),end=U32(t,0x14);
#ifdef RX_ORACLE
  fx_set_reg(FX_ECX,rs); /* actual MOV ECX,[ESP+18] each loop, including final exit */
  fx_set_reg(FX_ESI,t);fx_set_reg(FX_EDX,(uint32_t)((int32_t)(end-begin)>>2));
#endif
  if(((end-begin)>>2)==0)break;
  if(begin>end||!U32(t,4))crt_invalid_parameter_noinfo();
  const rx_addr proxy=U32(U32(t,4),0);
  if(begin>=U32(proxy,0x10))crt_invalid_parameter_noinfo();
  uint32_t n;
#ifdef RX_ORACLE
  n=rx_records_original_count(F,t,rs,row);
#else
  RACE_CORE_CALL(0,n=rx_tbl_column_rows(U32(begin,0)));
#endif
  if(row>=n)break;
  const uint32_t ci=col(t,0x93e228); /* EventTypeLocationId */
  if(ci>=((U32(t,0x14)-U32(t,0x10))>>2))crt_invalid_parameter_noinfo();
  rx_addr ref;RACE_CORE_CALL(0,ref=rx_tbl_column_parent_ref(U32(U32(t,0x10),4*ci),row));
  if(!U32(ref,8)||U32(ref,0)==0xffffffffu)continue;
  (void)value(t,0x93e228,row);(void)value(t,0x936ce4,row);
  const rx_addr authored=U32(ref,8);const uint32_t ar=U32(ref,0);
  const int32_t location=I32(value(authored,0x936e20,ar),0);
  const int32_t score=I32(value(authored,0x9626b0,ar),0);
  const int32_t combo=I32(value(authored,0x96daa4,ar),0);
  const float lap=F32(value(authored,0x957c28,ar),0);
  SU32(F,0x24,(uint32_t)location);SU32(F,0x10,(uint32_t)score);SU32(F,0x14,(uint32_t)combo);SF32(F,0x1c,lap);
  if(I32(value(t,0x936ce4,row),0)<0){
   (void)value(t,0x96da1c,row);(void)value(t,0x96da2c,row);
   (void)value(t,0x96da40,row);(void)value(t,0x96da54,row);
   (void)value(t,0x96da6c,row);(void)value(t,0x96da80,row);
  }else{
   const rx_addr loc=F+0x28+4u*(uint32_t)location;if(U32(loc,0)==0)SU32(loc,0,1);
   /* Original COMISS/JBE: positive authored time and strictly smaller record;
    * unordered comparisons do not mark the private flag. */
   if(lap>0.0f){const float best=F32(value(t,0x957c28,row),0);const rx_addr p=F+0x1b8+4u*ar;if(lap>best&&U32(p,0)==0)SU32(p,0,1);}
   if(combo>0){const int32_t best=I32(value(t,0x96daa4,row),0);const rx_addr p=F+0x1158+4u*ar;if(best>combo&&U32(p,0)==0)SU32(p,0,1);}
   if(score>0){const int32_t best=I32(value(t,0x9626b0,row),0);const rx_addr p=F+0x20f8+4u*ar;if(best>score&&U32(p,0)==0)SU32(p,0,1);}
   (void)value(t,0x96da08,row); /* TotalHoleshotsWon: read, not a grant. */
  }
 }
 if(U32(rs,0x14)!=0){
#ifdef RX_ORACLE
  rx_records_original_highlight(F,rs,slot,row);
#else
  RACE_CORE_CALL(3,(void)rx_highlight_add_progress(U32(0,0xd92edc),slot,0x25));
#endif
 }
 fx_restore(saved);
 RACE_FRAME_END(F);
}
RX_FUNCTION(0x007647f0,rx_records_leave_highlights)
