#ifndef RX_ORACLE
/* Scope selector for reviewed typed PC guest loader. No generic8252c0
 * registration and no host-stream reinterpretation or eager decompression. */
#include "fourwheel_guest_reader_binding.h"
#include "native_bridge.h"
#include <stdlib.h>
void rx_fourwheel_reader_unresolved(rx_addr fn){rx_regs in={0};rx_call_original_ex(fn,&in,0,0,0);abort();}
uint32_t rx_fourwheel_guest_reader_load(rx_addr reader,rx_addr stream){
 if(!reader||!stream||U32(reader,0x45c)||U32(stream,0)!=0x974d1c)
  /* BOUNDARY (unsupported input): a tuning document whose stream / header the native reader does not support stops here by design (named fatal); the shell's tuning documents are modernPC bxml in a memory stream. */
  rx_fourwheel_reader_unresolved(0x8252c0);
 uint32_t size=U32(stream,8),pos=U32(stream,0xc),data=U32(stream,4);
 if(pos>size||(!data&&size)||U32(U32(stream,0),8)!=0x8109b0||U32(U32(stream,0),0xc)!=0x829e40)
  /* BOUNDARY (unsupported input): a tuning document whose stream / header the native reader does not support stops here by design (named fatal); the shell's tuning documents are modernPC bxml in a memory stream. */
  rx_fourwheel_reader_unresolved(0x8252c0);
 /* A genuine short initial header retains the original virtual-read failure.
  * Complete headers require the exact supported modernPC format. */
 if(size-pos>=32){
  rx_addr p=data+pos;
  if(U32(p,0)!=0x4c4d5842||U16(p,4)!=1002||U16(p,6)>1)
   /* BOUNDARY (unsupported input): a tuning document whose stream / header the native reader does not support stops here by design (named fatal); the shell's tuning documents are modernPC bxml in a memory stream. */
   rx_fourwheel_reader_unresolved(0x8252c0);
 }
 return rx_native_reader_load_pc(reader,stream);
}

#endif /* native-only typed composition; original oracle callers remain original */
