#include "zlib_fixed.h"
#include "fx_util.h"
/* Fixed decode tables are shipped in the original image, not constructed from
 * a host zlib variant. This leaf has no call/RTC dependency. */
void rx_guest_inflate_fixed_tables(rx_addr state){
 rx_addr e=rx_sp(),bp=e-4,base=bp-0xc0;fx_saved saved=fx_save();
 SU32(bp,0,saved.ebp);SU32(base-4,0,saved.ebx);SU32(base-8,0,saved.esi);SU32(base-12,0,saved.edi);fx_set_reg(FX_EBP,bp);
 for(unsigned i=0;i<0xc0;i+=4)SU32(base,i,0xcccccccc);fx_set_reg(FX_EDI,bp);fx_set_reg(FX_ECX,0);rx_set_sp(base-12);
 fx_set_reg(FX_EAX,state);SU32(state,0x44,0x926e08);fx_set_reg(FX_EAX,state);SU32(state,0x4c,9);fx_set_reg(FX_EAX,state);SU32(state,0x48,0x927608);fx_set_reg(FX_EAX,state);SU32(state,0x50,5);
 fx_restore(saved);rx_set_sp(e);
}
