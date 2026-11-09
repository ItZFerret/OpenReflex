#ifndef RX_ORACLE
/* Isolated native dependency composition. No registration or original fallback
 * on any supported target. Unsupported mode1/FILE/custom source is named fatal. */
#include "native_bridge.h"
#include "compressed_input.h"
#include "guest_reader_destroy.h"
#include "guest_reader_load.h"
#include "zlib_lifecycle.h"
#include "zlib_inflate.h"
#include "compression_leaves.h"
#include "build_activity.h"
#include "build_alloc.h"
#include "race_misc.h"
#include "phys_crt.h"
#include "crt.h"
#include "fx_util.h"
#include "../src/rx_os.h"
#include <stdio.h>
#include <stdlib.h>
static uint32_t counts[7];
uint32_t rx_native_compressed_count(unsigned i){return i<7?counts[i]:0;}
static void unresolved(rx_addr fn){rx_regs in={0};rx_call_original_ex(fn,&in,0,0,0);abort();}
void rx_native_compressed_dependency(rx_addr fn,rx_addr expected){
 rx_addr e=rx_sp(),self=fx_reg(FX_ECX);uint32_t a=U32(e,4),b=U32(e,8),c=U32(e,12),d=U32(e,16),ret=0,pop=0;int returns=1;
 switch(fn){
 case 0x829d10:counts[0]++;ret=rx_guest_zcompression_init(self);break;
 case 0x829d40:counts[1]++;ret=rx_guest_zcompression_close(self);break;
 case 0x829e90:counts[2]++;ret=rx_guest_zcompression_process(self,a,b,c,d);pop=16;break;
 case 0x8ea5e0:ret=(uint32_t)rx_guest_inflate_init(a,b,c);break;
 case 0x8ec710:ret=(uint32_t)rx_guest_inflate_end(a);break;
 case 0x8ea630:counts[3]++;ret=(uint32_t)rx_guest_inflate(a,b);break;
 case 0x8e6bd0:rx_compression_set_output(self,a,b);returns=0;pop=8;break;
 case 0x829e40:ret=rx_compression_last_read(self);break;
 case 0x829e50:rx_compression_set_input(self,a,b);returns=0;pop=8;break;
 case 0x810870:ret=rx_compression_input_remaining(self);break;
 case 0x8109b0:counts[4]++;if(U32(self,0)!=0x974d1c)unresolved(fn);ret=rx_memory_stream_read(self,a,b);pop=8;break;
 case 0x80fc10:ret=rx_guest_compressed_read(self,a,b);pop=8;break;
 case 0x80fb80:rx_guest_compressed_close(self);returns=0;break;
 case 0x80f920:ret=rx_guest_compressed_init(fx_reg(FX_ESI),fx_reg(FX_EAX),self,a);pop=4;break;
 case 0x8fa06a:ret=crt_memset(a,(int32_t)b,c);break;
 case 0x8f9efe:phys_security_check_cookie(self);returns=0;break;
 case 0x7fa1e0:rx_name_hash_free(fx_reg(FX_EAX));returns=0;break;
 case 0x78fc50:rx_reflex_aligned_free(a);returns=0;break;
 default:
  if(fn==U32(0,0x916358))ret=rx_crt_aligned_malloc(a,b);
  else if(fn==U32(0,0x916350)){rx_crt_aligned_free(a);counts[6]++;returns=0;}
  else if(fn==U32(0,0x91600c)||fn==U32(0,0x9160f8)){
   if(a!=0xd61704)unresolved(fn);uint32_t slot=fn==U32(0,0x91600c)?0x91600c:0x9160f8;counts[5]++;ret=rx_os_call(slot,1,&a);pop=4;
  }else unresolved(fn); /* includes8ecdf0/8ed680/owned guest FILE; no no-op */
 }
 if(rx_sp()!=e||e+4+pop!=expected){fprintf(stderr,"native compressed exact call frame mismatch target=%08x entry=%08x actual=%08x expected=%08x\n",fn,e,rx_sp(),expected);abort();}
 if(returns)fx_set_reg(FX_EAX,ret);rx_set_sp(expected);
}
void rx_wrapper_dependency(rx_addr fn,rx_addr expected){rx_native_compressed_dependency(fn,expected);}
void rx_guest_destroy_dependency(rx_addr fn,rx_addr expected){rx_native_compressed_dependency(fn,expected);}
void rx_wrapper_divide_error(void){fprintf(stderr,"native compressed original IDIV fault\n");abort();}

#endif /* native-only typed composition; original oracle callers remain original */
