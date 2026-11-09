/* Exact small vtable dependencies. No host inflater, fabricated state or RTC
 * bypass. These sources do not register/activate a partial compressed stream. */
#include "compression_leaves.h"
#ifdef RX_ORACLE
extern void rx_compression_leaf_reg(unsigned,uint32_t);
#define REG(i,v) rx_compression_leaf_reg(i,v)
#else
#define REG(i,v) ((void)0)
#endif
uint32_t rx_compression_last_read(rx_addr s){return U32(s,0x18);}
uint32_t rx_compression_input_remaining(rx_addr s){return U32(s,0xc);}
void rx_compression_set_output(rx_addr s,rx_addr dst,uint32_t n){REG(0,dst);REG(2,n);SU32(s,0x14,dst);SU32(s,0x18,n);}
void rx_compression_set_input(rx_addr s,rx_addr src,uint32_t n){
 if(U32(s,4)==1){REG(0,src);REG(2,0);
  if(!src){SU32(s,0xc,0);SU32(s,0x40,1);SU32(s,8,0);return;}
  SU32(s,8,src);REG(0,n);SU32(s,0xc,n);return;
 }
 REG(2,src);REG(0,n);SU32(s,0xc,n);SU32(s,8,src);
}
