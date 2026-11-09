/* Full control-flow hand draft of original008ea630. All guest state and input
 * ownership remain original. NOT registered/accepted; see DISPATCH_STATUS.md. */
#include "zlib_inflate.h"
#include "zlib_inflate_helpers.h"
#include "zlib_crc.h"
#include "zlib_fixed.h"
#include "zlib_fast.h"
#include "zlib_table.h"
#include "zlib_stackvars.h"
#include "zlib_lifecycle.h"
#include "fx_util.h"
#include "crt.h"
#define Q(o) U32(bp,-(o))
#define W(o,v) SU32(bp,-(o),(v))
#define ST(o,v) SU32(state,(o),(v))
#define NEED(n) do{while(Q(0x50)<(uint32_t)(n)){if(!Q(0x2c))goto suspend;pull(bp);}}while(0)
#define DROP(n) do{uint32_t z=(n);W(0x44,Q(0x44)>>(z&31));W(0x50,Q(0x50)-z);}while(0)
#define BAD(msg) do{SU32(stream,0x18,(msg));ST(0,27);}while(0)
static uint32_t one(uint32_t n){return 1u<<(n&31);}
static uint32_t swap(uint32_t a){return (a>>24)+((a>>8)&0xff00)+((a&0xff00)<<8)+((a&255)<<24);}
static void pull(rx_addr bp){W(0x2c,Q(0x2c)-1);W(0x44,Q(0x44)+((uint32_t)U8(Q(0x14),0)<<(Q(0x50)&31)));W(0x14,Q(0x14)+1);W(0x50,Q(0x50)+8);}
static void call_frame(rx_addr b,rx_addr ra,unsigned n,const uint32_t*a){rx_addr e=b-4*(n+1);SU32(e,0,ra);for(unsigned i=0;i<n;i++)SU32(e,4+4*i,a[i]);rx_set_sp(e);}
static uint32_t checksum_at(rx_addr b,rx_addr ra,int crc,uint32_t check,rx_addr bytes,uint32_t len){uint32_t args[3]={check,bytes,len};call_frame(b,ra,3,args);uint32_t r=crc?rx_guest_crc32(check,bytes,len):rx_guest_adler32(check,bytes,len);rx_set_sp(b);return r;}
static uint32_t table_at(rx_addr b,rx_addr ra,uint32_t kind,rx_addr lens,uint32_t codes,rx_addr next,rx_addr bits,rx_addr work){uint32_t args[6]={kind,lens,codes,next,bits,work};call_frame(b,ra,6,args);uint32_t r=(uint32_t)rx_guest_inflate_table(kind,lens,codes,next,bits,work);rx_set_sp(b);return r;}
static void export_stream(rx_addr bp,rx_addr stream,rx_addr state){SU32(stream,0xc,Q(0x20));SU32(stream,0x10,Q(0x38));SU32(stream,0,Q(0x14));SU32(stream,4,Q(0x2c));SU32(state,0x30,Q(0x44));SU32(state,0x34,Q(0x50));}
static void import_stream(rx_addr bp,rx_addr stream,rx_addr state){W(0x20,U32(stream,0xc));W(0x38,U32(stream,0x10));W(0x14,U32(stream,0));W(0x2c,U32(stream,4));W(0x44,U32(state,0x30));W(0x50,U32(state,0x34));}
/* Decode root/subtable entry, retaining actual here/last guarded words. A lack
 * of input returns0 without inventing an invalid code or consuming extra bits. */
static int decode(rx_addr bp,rx_addr table,uint32_t root,int distance){
 /* Original stores the WHOLE root entry before checking bits, including on
  * starvation. Decompiler byte-field temporaries must not narrow this write. */
 for(;;){W(0x8c,U32(table,4*(Q(0x44)&(one(root)-1))));if(Q(0x50)>=U8(bp-0x8c,1))break;if(!Q(0x2c))return 0;pull(bp);}
 if((U8(bp-0x8c,0)&0xf0)==0&&(distance||U8(bp-0x8c,0))){
  W(0x98,Q(0x8c));
  /* LAST remains the original root throughout this subtable refill loop;
   * HERE is overwritten in full before every combined-bit availability test. */
  for(;;){
   uint32_t prefix=U8(bp-0x98,1),op=U8(bp-0x98,0);
   W(0x8c,U32(table,4*(U16(bp-0x98,2)+((Q(0x44)&(one(prefix+op)-1))>>(prefix&31)))));
   if(prefix+U8(bp-0x8c,1)<=Q(0x50)){DROP(prefix);break;}
   if(!Q(0x2c))return 0;pull(bp);
  }
 }
 DROP(U8(bp-0x8c,1));return 1;
}
int32_t rx_guest_inflate(rx_addr stream,uint32_t flush){
 const rx_addr e=rx_sp(),bp=e-4,base=bp-0x184,b=base-12;fx_saved saved=fx_save();
 SU32(bp,0,saved.ebp);SU32(base-4,0,saved.ebx);SU32(base-8,0,saved.esi);SU32(base-12,0,saved.edi);fx_set_reg(FX_EBP,bp);
 for(unsigned i=0;i<0x184;i+=4)SU32(base,i,0xcccccccc);fx_set_reg(FX_EDI,bp);fx_set_reg(FX_ECX,0);rx_set_sp(b);
 int32_t result=-2;rx_addr state=0;
 if(!stream||!U32(stream,0x1c)||!U32(stream,0xc)||(!U32(stream,0)&&U32(stream,4)))goto finish;
 state=U32(stream,0x1c);W(8,state);if(U32(state,0)==11)ST(0,12);import_stream(bp,stream,state);W(0x5c,Q(0x2c));W(0x68,Q(0x38));W(0xb0,0);
 for(;;){
  W(0x184,U32(state,0));if(Q(0x184)<=28)fx_set_reg(FX_EDX,Q(0x184));
  switch(Q(0x184)){
  case 0:
   if(!U32(state,8)){ST(0,12);break;}NEED(16);
   if((U32(state,8)&2)&&Q(0x44)==0x8b1f){
    ST(0x14,checksum_at(b,0x8ea78c,1,0,0,0));SU8(bp-0xbc,0,(uint8_t)Q(0x44));SU8(bp-0xbc,1,(uint8_t)(Q(0x44)>>8));ST(0x14,checksum_at(b,0x8ea7bf,1,U32(state,0x14),bp-0xbc,2));W(0x44,0);W(0x50,0);ST(0,1);break;
   }
   ST(0x10,0);
   if(!(U32(state,8)&1)||(((Q(0x44)&255)<<8)+(Q(0x44)>>8))%31){BAD(0x927858);break;}
   if((Q(0x44)&15)!=8){BAD(0x92783c);break;}DROP(4);
   if((Q(0x44)&15)+8>U32(state,0x1c)){BAD(0x927828);break;}
   ST(0x14,checksum_at(b,0x8ea8a6,0,0,0,0));SU32(stream,0x30,U32(state,0x14));ST(0,(Q(0x44)&0x200)?9:11);W(0x44,0);W(0x50,0);break;
  case 1:
   NEED(16);ST(0x10,Q(0x44));if((U32(state,0x10)&255)!=8){BAD(0x92783c);break;}if(U32(state,0x10)&0xe000){BAD(0x92780c);break;}
   if(U32(state,0x10)&0x200){SU8(bp-0xbc,0,(uint8_t)Q(0x44));SU8(bp-0xbc,1,(uint8_t)(Q(0x44)>>8));ST(0x14,checksum_at(b,0x8ea9c0,1,U32(state,0x14),bp-0xbc,2));}W(0x44,0);W(0x50,0);ST(0,2);
   /* fall through */
  case 2:
   NEED(32);if(U32(state,0x10)&0x200){SU32(bp,-0xbc,Q(0x44));ST(0x14,checksum_at(b,0x8eaa7f,1,U32(state,0x14),bp-0xbc,4));}W(0x44,0);W(0x50,0);ST(0,3);
   /* fall through */
  case 3:
   NEED(16);if(U32(state,0x10)&0x200){SU16(bp-0xbc,0,(uint16_t)Q(0x44));ST(0x14,checksum_at(b,0x8eab26,1,U32(state,0x14),bp-0xbc,2));}W(0x44,0);W(0x50,0);ST(0,4);
   /* fall through */
  case 4:
   if(U32(state,0x10)&0x400){NEED(16);ST(0x38,Q(0x44));if(U32(state,0x10)&0x200){SU16(bp-0xbc,0,(uint16_t)Q(0x44));ST(0x14,checksum_at(b,0x8eabe8,1,U32(state,0x14),bp-0xbc,2));}W(0x44,0);W(0x50,0);}ST(0,5);
   /* fall through */
  case 5:
   if(U32(state,0x10)&0x400){W(0x74,U32(state,0x38));if(Q(0x74)>Q(0x2c))W(0x74,Q(0x2c));if(Q(0x74)){if(U32(state,0x10)&0x200)ST(0x14,checksum_at(b,0x8eac5d,1,U32(state,0x14),Q(0x14),Q(0x74)));W(0x2c,Q(0x2c)-Q(0x74));W(0x14,Q(0x14)+Q(0x74));ST(0x38,U32(state,0x38)-Q(0x74));}if(U32(state,0x38))goto suspend;}ST(0,6);
   /* fall through */
  case 6:
   if(U32(state,0x10)&0x800){if(!Q(0x2c))goto suspend;W(0x74,0);do{W(0xa4,U8(Q(0x14),Q(0x74)));W(0x74,Q(0x74)+1);if(!Q(0xa4))break;}while(Q(0x74)<Q(0x2c));if(U32(state,0x10)&0x2000)ST(0x14,checksum_at(b,0x8ead0d,1,U32(state,0x14),Q(0x14),Q(0x74)));W(0x2c,Q(0x2c)-Q(0x74));W(0x14,Q(0x14)+Q(0x74));if(Q(0xa4))goto suspend;}ST(0,7);
   /* fall through */
  case 7:
   if(U32(state,0x10)&0x1000){if(!Q(0x2c))goto suspend;W(0x74,0);do{W(0xa4,U8(Q(0x14),Q(0x74)));W(0x74,Q(0x74)+1);if(!Q(0xa4))break;}while(Q(0x74)<Q(0x2c));if(U32(state,0x10)&0x2000)ST(0x14,checksum_at(b,0x8eadae,1,U32(state,0x14),Q(0x14),Q(0x74)));W(0x2c,Q(0x2c)-Q(0x74));W(0x14,Q(0x14)+Q(0x74));if(Q(0xa4))goto suspend;}ST(0,8);
   /* fall through */
  case 8:
   if(U32(state,0x10)&0x200){NEED(16);if(Q(0x44)!=(U32(state,0x14)&0xffff)){BAD(0x9277f8);break;}W(0x44,0);W(0x50,0);}ST(0x14,checksum_at(b,0x8eae7f,1,0,0,0));SU32(stream,0x30,U32(state,0x14));ST(0,11);break;
  case 9:
   NEED(32);ST(0x14,swap(Q(0x44)));fx_set_reg(FX_EDX,U32(state,0x14));SU32(stream,0x30,U32(state,0x14));W(0x44,0);W(0x50,0);ST(0,10);
   /* fall through */
  case 10:
   if(!U32(state,0xc)){export_stream(bp,stream,state);result=2;goto finish;}
   ST(0x14,checksum_at(b,0x8eafa2,0,0,0,0));SU32(stream,0x30,U32(state,0x14));ST(0,11);
   /* fall through */
  case 11:if(flush==5)goto suspend;
   /* fall through */
  case 12:
   if(U32(state,4)){DROP(Q(0x50)&7);ST(0,24);break;}
   NEED(3);ST(4,Q(0x44)&1);DROP(1);W(0x184,Q(0x44)&3);
   switch(Q(0x184)){
   case 0:ST(0,13);break;
   case 1:{uint32_t arg=state;call_frame(b,0x8eb0a0,1,&arg);rx_guest_inflate_fixed_tables(state);rx_set_sp(b);ST(0,18);break;}
   case 2:ST(0,15);break;
   default:BAD(0x9277e4);break;
   }DROP(2);break;
  case 13:
   DROP(Q(0x50)&7);NEED(32);if((Q(0x44)&0xffff)!=((Q(0x44)>>16)^0xffff)){BAD(0x9277c4);break;}ST(0x38,Q(0x44)&0xffff);W(0x44,0);W(0x50,0);ST(0,14);
   /* fall through */
  case 14:
   W(0x74,U32(state,0x38));if(!Q(0x74)){ST(0,11);break;}if(Q(0x74)>Q(0x2c))W(0x74,Q(0x2c));if(Q(0x74)>Q(0x38))W(0x74,Q(0x38));if(!Q(0x74))goto suspend;
   {uint32_t args[3]={Q(0x20),Q(0x14),Q(0x74)};call_frame(b,0x8eb1ee,3,args);
#ifdef RX_ORACLE
    rx_set_sp(b);rx_call_original(0x8fa070,RX_CDECL,0,3,args);SU32(b-16,0,0x8eb1ee);
#else
    crt_memcpy(args[0],args[1],args[2]);
#endif
    rx_set_sp(b);}
   W(0x2c,Q(0x2c)-Q(0x74));W(0x14,Q(0x14)+Q(0x74));W(0x38,Q(0x38)-Q(0x74));W(0x20,Q(0x20)+Q(0x74));ST(0x38,U32(state,0x38)-Q(0x74));break;
  case 15:
   NEED(14);ST(0x58,(Q(0x44)&31)+257);ST(0x5c,((Q(0x44)>>5)&31)+1);ST(0x54,((Q(0x44)>>10)&15)+4);DROP(14);
   if(U32(state,0x58)>=287||U32(state,0x5c)>=31){BAD(0x9277a0);break;}ST(0x60,0);ST(0,16);
   /* fall through */
  case 16:
   while(U32(state,0x60)<U32(state,0x54)){NEED(3);SU16(state+0x68,2*U16(0x927688,2*U32(state,0x60)),(uint16_t)(Q(0x44)&7));ST(0x60,U32(state,0x60)+1);DROP(3);}
   while(U32(state,0x60)<19){SU16(state+0x68,2*U16(0x927688,2*U32(state,0x60)),0);ST(0x60,U32(state,0x60)+1);}
   ST(0x64,state+0x528);ST(0x44,U32(state,0x64));ST(0x4c,7);W(0xb0,table_at(b,0x8eb44b,0,state+0x68,19,state+0x64,state+0x4c,state+0x2e8));
   if(Q(0xb0)){BAD(0x927784);break;}ST(0x60,0);ST(0,17);
   /* fall through */
  case 17:
   while(U32(state,0x60)<U32(state,0x58)+U32(state,0x5c)){
    uint32_t here;for(;;){here=U32(U32(state,0x44),4*(Q(0x44)&(one(U32(state,0x4c))-1)));W(0x8c,here);if(((here>>8)&255)<=Q(0x50))break;if(!Q(0x2c))goto suspend;pull(bp);}W(0x8c,here);
    uint32_t bits=(here>>8)&255,val=here>>16;
    if(val<16){NEED(bits);DROP(bits);SU16(state+0x68,2*U32(state,0x60),(uint16_t)val);ST(0x60,U32(state,0x60)+1);continue;}
    if(val==16){NEED(bits+2);DROP(bits);if(!U32(state,0x60)){BAD(0x927768);break;}W(0xa4,U16(state+0x66,2*U32(state,0x60)));W(0x74,(Q(0x44)&3)+3);DROP(2);}
    else if(val==17){NEED(bits+3);DROP(bits);W(0xa4,0);W(0x74,(Q(0x44)&7)+3);DROP(3);}
    else{NEED(bits+7);DROP(bits);W(0xa4,0);W(0x74,(Q(0x44)&127)+11);DROP(7);}
    if(U32(state,0x60)+Q(0x74)>U32(state,0x58)+U32(state,0x5c)){BAD(0x927768);break;}
    for(;;){W(0x184,Q(0x74));W(0x74,Q(0x74)-1);if(!Q(0x184))break;SU16(state+0x68,2*U32(state,0x60),(uint16_t)Q(0xa4));ST(0x60,U32(state,0x60)+1);}
   }
   /* Original proceeds here even after the repeat loop sets BAD; do not add a
    * modern-zlib guard that changes its malformed-input instruction route. */
   ST(0x64,state+0x528);ST(0x44,U32(state,0x64));ST(0x4c,9);W(0xb0,table_at(b,0x8eb8a8,1,state+0x68,U32(state,0x58),state+0x64,state+0x4c,state+0x2e8));if(Q(0xb0)){BAD(0x92774c);break;}
   ST(0x48,U32(state,0x64));ST(0x50,6);W(0xb0,table_at(b,0x8eb91b,2,state+0x68+2*U32(state,0x58),U32(state,0x5c),state+0x64,state+0x50,state+0x2e8));if(Q(0xb0)){BAD(0x927734);break;}ST(0,18);
   /* fall through */
  case 18:
   if(Q(0x2c)>=6&&Q(0x38)>=258){export_stream(bp,stream,state);uint32_t args[2]={stream,Q(0x68)};call_frame(b,0x8eb9ab,2,args);rx_guest_inflate_fast(stream,Q(0x68));rx_set_sp(b);import_stream(bp,stream,state);break;}
   if(!decode(bp,U32(state,0x44),U32(state,0x4c),0))goto suspend;ST(0x38,Q(0x8c)>>16);
   if(!U8(bp-0x8c,0)){ST(0,23);break;}if(Q(0x8c)&32){ST(0,11);break;}if(Q(0x8c)&64){BAD(0x927718);break;}ST(0x40,Q(0x8c)&15);ST(0,19);
   /* fall through */
  case 19:
   if(U32(state,0x40)){NEED(U32(state,0x40));ST(0x38,U32(state,0x38)+(Q(0x44)&(one(U32(state,0x40))-1)));DROP(U32(state,0x40));}ST(0,20);
   /* fall through */
  case 20:
   if(!decode(bp,U32(state,0x48),U32(state,0x50),1))goto suspend;if(Q(0x8c)&64){BAD(0x927700);break;}ST(0x3c,Q(0x8c)>>16);ST(0x40,Q(0x8c)&15);ST(0,21);
   /* fall through */
  case 21:
   if(U32(state,0x40)){NEED(U32(state,0x40));ST(0x3c,U32(state,0x3c)+(Q(0x44)&(one(U32(state,0x40))-1)));DROP(U32(state,0x40));}
   if(U32(state,0x3c)>Q(0x68)+U32(state,0x24)-Q(0x38)){BAD(0x9276e0);break;}ST(0,22);
   /* fall through */
  case 22:
   if(!Q(0x38))goto suspend;
   if(U32(state,0x3c)>Q(0x68)-Q(0x38)){
    W(0x74,U32(state,0x3c)-(Q(0x68)-Q(0x38)));if(Q(0x74)>U32(state,0x28)){W(0x74,Q(0x74)-U32(state,0x28));W(0x80,U32(state,0x2c)+U32(state,0x20)-Q(0x74));}else W(0x80,U32(state,0x2c)+U32(state,0x28)-Q(0x74));if(Q(0x74)>U32(state,0x38))W(0x74,U32(state,0x38));
   }else{W(0x80,Q(0x20)-U32(state,0x3c));W(0x74,U32(state,0x38));}
   if(Q(0x74)>Q(0x38))W(0x74,Q(0x38));W(0x38,Q(0x38)-Q(0x74));ST(0x38,U32(state,0x38)-Q(0x74));do{SU8(Q(0x20),0,U8(Q(0x80),0));W(0x20,Q(0x20)+1);W(0x80,Q(0x80)+1);W(0x74,Q(0x74)-1);}while(Q(0x74));if(!U32(state,0x38))ST(0,18);break;
  case 23:
   if(!Q(0x38))goto suspend;SU8(Q(0x20),0,(uint8_t)U32(state,0x38));W(0x20,Q(0x20)+1);W(0x38,Q(0x38)-1);ST(0,18);break;
  case 24:
   if(U32(state,8)){
    NEED(32);W(0x68,Q(0x68)-Q(0x38));SU32(stream,0x14,U32(stream,0x14)+Q(0x68));ST(0x18,U32(state,0x18)+Q(0x68));
    if(Q(0x68)){W(0x184,checksum_at(b,U32(state,0x10)?0x8ec0c2:0x8ec0e4,U32(state,0x10)!=0,U32(state,0x14),Q(0x20)-Q(0x68),Q(0x68)));ST(0x14,Q(0x184));SU32(stream,0x30,Q(0x184));}W(0x68,Q(0x38));W(0x184,U32(state,0x10)?Q(0x44):swap(Q(0x44)));if(Q(0x184)!=U32(state,0x14)){BAD(0x9276c8);break;}W(0x44,0);W(0x50,0);
   }ST(0,25);
   /* fall through */
  case 25:
   if(U32(state,8)&&U32(state,0x10)){NEED(32);if(Q(0x44)!=U32(state,0x18)){BAD(0x9276b0);break;}W(0x44,0);W(0x50,0);}ST(0,26);
   /* fall through */
  case 26:W(0xb0,1);goto suspend;
  case 27:W(0xb0,0xfffffffd);goto suspend;
  case 28:result=-4;goto finish;
  default:result=-2;goto finish;
  }
 }
suspend:
 export_stream(bp,stream,state);
 if(U32(state,0x20)||(I32(state,0)<24&&Q(0x68)!=U32(stream,0x10))){uint32_t args[2]={stream,Q(0x68)};call_frame(b,0x8ec2c5,2,args);uint32_t failed=(uint32_t)rx_guest_inflate_updatewindow(stream,Q(0x68));rx_set_sp(b);if(failed){ST(0,28);result=-4;goto finish;}}
 W(0x5c,Q(0x5c)-U32(stream,4));W(0x68,Q(0x68)-U32(stream,0x10));SU32(stream,8,U32(stream,8)+Q(0x5c));SU32(stream,0x14,U32(stream,0x14)+Q(0x68));ST(0x18,U32(state,0x18)+Q(0x68));
 if(U32(state,8)&&Q(0x68)){W(0x184,checksum_at(b,U32(state,0x10)?0x8ec356:0x8ec37b,U32(state,0x10)!=0,U32(state,0x14),U32(stream,0xc)-Q(0x68),Q(0x68)));ST(0x14,Q(0x184));SU32(stream,0x30,Q(0x184));}
 fx_set_reg(FX_EDX,U32(state,0)==11?128:0);SU32(stream,0x2c,U32(state,0x34)+(U32(state,4)?64:0)+fx_reg(FX_EDX));if(((!Q(0x5c)&&!Q(0x68))||flush==4)&&!Q(0xb0))W(0xb0,0xfffffffb);result=(int32_t)Q(0xb0);
finish:
 fx_set_reg(FX_EAX,(uint32_t)result);SU32(b-4,0,fx_reg(FX_EDX));SU32(b-8,0,(uint32_t)result);fx_set_reg(FX_ECX,bp);fx_set_reg(FX_EDX,0x8ec420);SU32(b-12,0,0x8ec407);rx_set_sp(b-12);rx_guest_inflate_check_stackvars(bp,0x8ec420);fx_set_reg(FX_EAX,U32(b-8,0));fx_set_reg(FX_EDX,U32(b-4,0));fx_restore(saved);fx_set_reg(FX_EBP,bp);rx_set_sp(bp);rx_guest_zlib_rtc(bp,0x8ec419,bp,rx_sp());fx_set_reg(FX_EBP,saved.ebp);rx_set_sp(e);return result;
}
