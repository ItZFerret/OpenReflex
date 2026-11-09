/* Exact original dependency frames for00452a90; no original fallback or host
 * math substitute. Both008fa0d0 dispatch branches are modeled completely. */
#include "contact_dependencies.h"
#include "fx_util.h"
#include "phys_vec.h"
#include "phys_mech.h"
#include "bike_curve.h"
#include "phys_crt.h"
#include "crt.h"
#include <stdlib.h>
static int site(uint32_t ra,const uint32_t*a,unsigned n){for(unsigned i=0;i<n;i++)if(ra==a[i])return 1;return 0;}
static rx_f80 copysign_frame(rx_addr b,rx_addr ra){
 const uint32_t ebp=fx_reg(FX_EBP);SU32(b-4,0,ra);SU32(b-8,0,ebp);
 const uint64_t x=rx_rd64(b),y=rx_rd64(b+8);
 const double value=crt_copysign(rx_bd(x),rx_bd(y));
 /* Both PUSH ECX locals are completely overwritten by FLDZ/FSTP and the
  * two source dwords before the final FLD. */
 rx_wr64(b-16,rx_db(value));fx_set_reg(FX_EAX,(uint32_t)(rx_db(value)>>32));
 return rx_f80_from_f64(value);
}
static int32_t ftol_frame(rx_addr b,rx_addr ra,rx_f80 input){
 const rx_addr entry=b-4,bp=entry-4;SU32(entry,0,ra);SU32(bp,0,fx_reg(FX_EBP));
 int32_t result;
 if(U32(0,0xdcf760)){
  rx_addr local=(bp-8)&~7u;rx_wr64(local,rx_db(rx_f80_to_f64(input,0)));
  result=phys_ftol2_sse(input,FX_X87_BITS);fx_set_reg(FX_EAX,(uint32_t)result);
 }else{
  /* Actual _ftol2 at008fa106: same entry/return, no additional CALL. */
  rx_addr local=(bp-0x20)&~15u;
  uint32_t sign=rx_fb(rx_f80_to_f32(input,0));SU32(local,0x18,sign);
  int64_t nearest=rx_f80_to_int(input,0);rx_wr64(local+0x10,(uint64_t)nearest);
  uint32_t lo=(uint32_t)nearest,hi=(uint32_t)((uint64_t)nearest>>32);
  if(lo==0&&(hi&0x7fffffffu)==0){
   /* FSTP integer-as-float, then original input, both to+18. */
   SF32(local,0x18,rx_f80_to_f32(rx_f80_from_int(nearest),0));
   SF32(local,0x18,rx_f80_to_f32(input,0));
  }else{
   rx_f80 difference=rx_f80_sub(input,rx_f80_from_int(nearest),FX_X87_BITS,0);
   uint32_t bits=rx_fb(rx_f80_to_f32(difference,0));SU32(local,0,bits);
   uint32_t ecx=((int32_t)sign<0?bits^0x80000000u:bits)+0x7fffffffu;
   fx_set_reg(FX_ECX,ecx);
  }
  int64_t exact=phys_ftol2(input,FX_X87_BITS);
  result=(int32_t)(uint32_t)exact;fx_set_reg(FX_EAX,(uint32_t)exact);fx_set_reg(FX_EDX,(uint32_t)((uint64_t)exact>>32));
 }
 return result;
}
float rx_contact_indexed_frame(rx_addr table,float input){
 const rx_addr entry=rx_sp(),b=entry-0x24;const uint32_t edi=fx_reg(FX_EDI);
 const float u=phys_mul(input,F32(0,0x92e330));SF32(b,0x14,u);
 const double shifted=(double)u-rx_bd(rx_rd64(0x977558));rx_wr64(b+0x18,rx_db(shifted));
 SU32(b,0x10,edi);rx_wr64(b+8,rx_db(rx_f80_to_f64(rx_f80_from_f64(shifted),0)));rx_wr64(b,rx_rd64(0x9774e8));
 rx_f80 signedHalf=copysign_frame(b,0x43910d);
 rx_f80 sum=rx_f80_add(signedHalf,rx_f80_from_f64(rx_bd(rx_rd64(b+0x18))),FX_X87_BITS,0);
 float low=(float)ftol_frame(b,0x439116,sum);SF32(b,0x18,low);
 rx_wr64(b+8,rx_db((double)low));rx_wr64(b,rx_rd64(0x9774e8));signedHalf=copysign_frame(b,0x439139);
 sum=rx_f80_add(signedHalf,rx_f80_from_f32(F32(b,0x18)),FX_X87_BITS,0);
 int32_t left=rx_cvtt((float)ftol_frame(b,0x439142,sum));fx_set_reg(FX_EDI,(uint32_t)left);
 rx_wr64(b+8,rx_db((double)F32(b,0x14)));rx_wr64(b,rx_rd64(0x9774e8));signedHalf=copysign_frame(b,0x439165);
 sum=rx_f80_add(signedHalf,rx_f80_from_f32(F32(b,0x14)),FX_X87_BITS,0);
 int32_t right=rx_cvtt((float)ftol_frame(b,0x43916e,sum));fx_set_reg(FX_EAX,(uint32_t)right);
 float t=phys_sub(F32(b,0x14),F32(b,0x18));
 fx_x87_copy(b+0xc,table+4u*(uint32_t)right);fx_x87_copy(b+8,table+4u*(uint32_t)left);
 SU32(b+4,0,0x439197); /* The actual retained cdecl Lerp frame. */
 float value=bike_lerp_clamped(t,F32(b,8),F32(b,0xc));
 fx_set_reg(FX_EDI,edi);if(rx_sp()!=entry)abort();return value;
}
rx_addr rx_contact_force_at_point_frame(rx_addr force,rx_addr mechanism,rx_addr point,uint32_t index){
 const rx_addr entry=rx_sp(),bp=entry-4,A=bp&~15u;SU32(bp,0,fx_reg(FX_EBP));
 /* Original loads these table pages even though the semantic owner spells
  * immutable vector constants inline. Preserve those actual memory reads. */
 rx_v4 mask=rx_ld4(0x92db80),q=rx_ld4(mechanism+160u*index+0x660),F=rx_ld4(force),r=rx_ld4(point);
 volatile rx_v4 c0=rx_ld4(0x93a1f0),c1=rx_ld4(0x93a200),c2=rx_ld4(0x93a210);
 (void)c0;(void)c1;(void)c2;
 rx_v4 conjugate=v4_mul(rx_ld4(0x93a220),q);
 rx_v4 local=phys_quat_mul(conjugate,phys_quat_mul(v4_and(F,mask),q));
 rx_v4 cross=v4_and(phys_cross_ba(local,r),mask);
 rx_addr result=phys_mech_add_force_at_point(force,mechanism,point,index);
 if(U32(0,0xda68bc)){rx_st4(A-0x10,F);rx_st4(A-0x10,cross);}
 fx_set_reg(FX_EAX,result);if(rx_sp()!=entry)abort();return result;
}
float rx_contact_dependency_exact(rx_addr target,rx_addr expected,float input){
 const rx_addr entry=rx_sp();uint32_t ra=U32(entry,0);float value=input;
 static const uint32_t curves[]={0x452afe,0x452b17,0x452be8,0x452c01,0x452cd2,0x452ceb,0x452dbc,0x452dd5};
 static const uint32_t forces[]={0x452f22,0x452f2d,0x452f38,0x452f43,0x452f4e};
 if(target==0x4390d0){if(expected!=entry+4||!site(ra,curves,8))abort();value=rx_contact_indexed_frame(fx_reg(FX_ESI),input);}
 else if(target==0x85c770){if(expected!=entry+8||!site(ra,forces,5))abort();rx_contact_force_at_point_frame(fx_reg(FX_ECX),fx_reg(FX_EDX),fx_reg(FX_ESI),U32(entry,4));}
 else if(target==0x8f9efe){if(expected!=entry+4||ra!=0x452f9a)abort();phys_security_check_cookie(fx_reg(FX_ECX));}
 else abort();
 if(rx_sp()!=entry)abort();rx_set_sp(expected);return value;
}
