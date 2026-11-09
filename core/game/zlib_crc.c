/* Original checksum trio, real image tables and guest32-bit memory. No host
 * checksum library or architecture-dependent pointer cast. */
#include "zlib_crc.h"
#include "zlib_lifecycle.h"
#include "fx_util.h"
typedef struct{rx_addr e,bp,b;fx_saved saved;} crc_frame;
static crc_frame enter(uint32_t size){
 crc_frame f;f.e=rx_sp();f.bp=f.e-4;f.b=f.bp-size-12;f.saved=fx_save();
 SU32(f.bp,0,f.saved.ebp);SU32(f.bp-size-4,0,f.saved.ebx);SU32(f.bp-size-8,0,f.saved.esi);SU32(f.bp-size-12,0,f.saved.edi);fx_set_reg(FX_EBP,f.bp);
 for(unsigned i=0;i<size;i+=4)SU32(f.bp-size,i,0xcccccccc);fx_set_reg(FX_EDI,f.bp);fx_set_reg(FX_ECX,0);rx_set_sp(f.b);return f;
}
static uint32_t leave(crc_frame f,uint32_t result){fx_set_reg(FX_EAX,result);fx_restore(f.saved);rx_set_sp(f.e);return result;}
static uint32_t swap32(uint32_t a){return (a>>24)+((a>>8)&0xff00)+((a&0xff00)<<8)+((a&255)<<24);}
static uint32_t fold_little(uint32_t a){return U32(0x928700,4*(a&255))^U32(0x928300,4*((a>>8)&255))^U32(0x927f00,4*((a>>16)&255))^U32(0x927b00,4*(a>>24));}
static uint32_t fold_big(uint32_t a){return U32(0x928b00,4*(a&255))^U32(0x928f00,4*((a>>8)&255))^U32(0x929300,4*((a>>16)&255))^U32(0x929700,4*(a>>24));}
static uint32_t byte_little(uint32_t a,rx_addr p){return (a>>8)^U32(0x927b00,4*((U8(p,0)^a)&255));}
static uint32_t byte_big(uint32_t a,rx_addr p){return (a<<8)^U32(0x928b00,4*((a>>24)^U8(p,0)));}
uint32_t rx_guest_crc32_little(uint32_t checksum,rx_addr bytes,uint32_t length){
 crc_frame f=enter(0xd8);uint32_t crc=checksum;SU32(f.bp,-8,crc);crc=~crc;SU32(f.bp,-8,crc);
 while(length&&(bytes&3)){
  crc=byte_little(crc,bytes);fx_set_reg(FX_EDX,crc);SU32(f.bp,-8,crc);++bytes;SU32(f.e,8,bytes);--length;SU32(f.e,0xc,length);
 }
 rx_addr word=bytes;SU32(f.bp,-0x14,word);
 while(length>=32){
  /* Eight original unrolled 4-byte folds. Maintain each XOR-store and pointer
   * advance before folding; all input reads use actual guest image endian. */
  for(unsigned i=0;i<8;i++){
   crc^=U32(word,0);SU32(f.bp,-8,crc);word+=4;SU32(f.bp,-0x14,word);crc=fold_little(crc);fx_set_reg(FX_EDX,crc);SU32(f.bp,-8,crc);
  }
  length-=32;SU32(f.e,0xc,length);
 }
 while(length>=4){crc^=U32(word,0);SU32(f.bp,-8,crc);word+=4;SU32(f.bp,-0x14,word);crc=fold_little(crc);fx_set_reg(FX_EDX,crc);SU32(f.bp,-8,crc);length-=4;SU32(f.e,0xc,length);}
 bytes=word;SU32(f.e,8,bytes);
 while(length){crc=byte_little(crc,bytes);fx_set_reg(FX_EDX,crc);SU32(f.bp,-8,crc);++bytes;SU32(f.e,8,bytes);--length;SU32(f.e,0xc,length);}
 crc=~crc;SU32(f.bp,-8,crc);return leave(f,crc);
}
uint32_t rx_guest_crc32_big(uint32_t checksum,rx_addr bytes,uint32_t length){
 crc_frame f=enter(0xd8);fx_set_reg(FX_EDX,(checksum&0xff00)<<8);uint32_t crc=swap32(checksum);SU32(f.bp,-8,crc);crc=~crc;SU32(f.bp,-8,crc);
 while(length&&(bytes&3)){
  crc=byte_big(crc,bytes);SU32(f.bp,-8,crc);++bytes;fx_set_reg(FX_EDX,bytes);SU32(f.e,8,bytes);--length;SU32(f.e,0xc,length);
 }
 rx_addr word=bytes;SU32(f.bp,-0x14,word);word-=4;SU32(f.bp,-0x14,word);
 while(length>=32){
  for(unsigned i=0;i<8;i++){
   word+=4;SU32(f.bp,-0x14,word);crc^=U32(word,0);SU32(f.bp,-8,crc);crc=fold_big(crc);fx_set_reg(FX_EDX,crc);SU32(f.bp,-8,crc);
  }
  length-=32;SU32(f.e,0xc,length);
 }
 while(length>=4){word+=4;SU32(f.bp,-0x14,word);crc^=U32(word,0);SU32(f.bp,-8,crc);crc=fold_big(crc);fx_set_reg(FX_EDX,crc);SU32(f.bp,-8,crc);length-=4;SU32(f.e,0xc,length);}
 word+=4;SU32(f.bp,-0x14,word);bytes=word;SU32(f.e,8,bytes);
 while(length){crc=byte_big(crc,bytes);SU32(f.bp,-8,crc);++bytes;fx_set_reg(FX_EDX,bytes);SU32(f.e,8,bytes);--length;SU32(f.e,0xc,length);}
 crc=~crc;SU32(f.bp,-8,crc);fx_set_reg(FX_EDX,(crc&0xff00)<<8);fx_set_reg(FX_ECX,(crc&255)<<24);return leave(f,swap32(crc));
}
uint32_t rx_guest_crc32(uint32_t checksum,rx_addr bytes,uint32_t length){
 crc_frame f=enter(0xcc);uint32_t result=0;
 if(bytes){
  /* Executable MOV EAX,1 / TEST EAX,EAX then local byte1 selects little core.
   * No input/callback occurs before this constant endian probe. BE and scalar
   * fallback blocks have no reachable incoming edge on this image. */
  SU32(f.bp,-8,1);fx_set_reg(FX_EAX,length);fx_set_reg(FX_ECX,bytes);fx_set_reg(FX_EDX,checksum);
  SU32(f.b,-4,length);SU32(f.b,-8,bytes);SU32(f.b,-12,checksum);SU32(f.b,-16,0x8f0784);rx_set_sp(f.b-16);
  result=rx_guest_crc32_little(checksum,bytes,length);rx_set_sp(f.b);
 }
 fx_set_reg(FX_EAX,result);fx_restore(f.saved);fx_set_reg(FX_EBP,f.bp);rx_set_sp(f.bp);rx_guest_zlib_rtc(f.bp,0x8f0957,f.bp,rx_sp());fx_set_reg(FX_EBP,f.saved.ebp);rx_set_sp(f.e);return result;
}
