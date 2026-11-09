/* Original 0086b7f0 return boundary, reusing the existing verified numerical
 * owner. No copied collision math and no original/emulator fallback.
 * This supplements recorded mask6 with original EAX and packed XMM0. It does
 * not claim to reproduce dead private-stack residue of the older kernel. */
#include "rodbox_live.h"
#include "phys_pair_rodbox.h"
#include "fx_util.h"
#include <stdlib.h>

void rx_collision_0400_box_live(rx_addr qa,rx_addr pa,rx_addr manifold,
 rx_addr box,rx_addr qb,rx_addr pb,rx_addr radial,uint32_t flip,rx_v4 *live){
 const rx_addr entry=rx_sp(),frame=((entry-4u)&~15u)-0x84u-12u;
 const fx_saved saved=fx_save();
 if(!live)abort();
 uint32_t edx=pa;
 phys_pair_0400_box(qa,pa,manifold,box,qb,pb,radial,flip,&edx);
 if(rx_sp()!=entry||fx_reg(FX_EBX)!=saved.ebx||fx_reg(FX_EBP)!=saved.ebp||
    fx_reg(FX_ESI)!=saved.esi||fx_reg(FX_EDI)!=saved.edi)abort();
 /* 0086bafb MOVSS from memory clears upper lanes; COMISS/JBE uses the
  * same count written by face-sum. Positive path's final MOVAPS at86bd34
  * reloads the exact depth vector already written by the canonical owner. */
 if(F32(frame,0x10)>0.0f){
  *live=v4_ld(frame+0x60);
  fx_set_reg(FX_EAX,U32(manifold,0)); /* 86bd4d INC/STORE capped count. */
 }else{
  *live=v4(F32(frame,0x10),0.0f,0.0f,0.0f);
  /* 81eaac starts EAX=sumFrame+58; two batches add60 each (81ec82). */
  const rx_addr sum_entry=frame-20u;
  const rx_addr sum_frame=((sum_entry-4u)&~15u)-0x104u-12u;
  fx_set_reg(FX_EAX,sum_frame+0x118u);
 }
 fx_set_reg(FX_EDX,edx);
 fx_set_reg(FX_ECX,U32(0,0x9b45e4)); /* 86bd5a..66 checked cookie. */
}
