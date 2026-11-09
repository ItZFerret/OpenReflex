/* The observed dispatcher's pair roles (0085ddd0 entered with its real frame by the finite native closures: the UTV / buggy /
 * truck vf13 wheel pass, the BODY probe 00435b00 and the composite closure of 00872860). Ten roles keep their typed closures
 * below (triangle / segment, triangle / circle, composite / segment, segment / box and circle / box in both orientations,
 * with their packed XMM0 model); every other case of the dispatcher runs its core generator through the case table
 * (phys_contact_pair_observed, phys_contact.c). */
#include "collision_dispatch_observed.h"
#include "collision_live.h"
#include "triangles_segments_observed.h"
#include "composite_pair_observed.h"
#include "rays_box_observed.h"
#include "rodbox_live.h"
#include "phys_pair2.h"
#include "phys_contact.h"
#include "fx_util.h"
#include <stdio.h>
#include <stdlib.h>
static void triangle_pair_role(rx_addr target,rx_addr ra,unsigned nargs,rx_addr expected,uint32_t compare_left,uint32_t compare_right,unsigned compare_width,rx_v4*live){
 if(target!=0x86fac0||(ra!=0x85e3ba&&ra!=0x85e4f9))abort(); /* routed here only for its two sites */
 const rx_addr entry=rx_sp(),bp=fx_reg(FX_EBP);
 if(nargs!=5||U32(entry,0)!=ra||expected!=entry+24||bp<4||((bp&~7u)-16)!=expected)abort();
 /* Real retained85ddd0 input frame, not guessed current volatile registers.
  * Its PUSH ECX is at A-4=callerESP+12, beyond all three saved registers. */
 const rx_addr M=U32(expected,12),A=U32(bp,8),QA=U32(bp,12),PA=U32(bp,16),B=U32(bp,20),QB=U32(bp,24),PB=U32(bp,28);
 const uint32_t flag=U32(bp,32),ta=U32(A,8),tb=U32(B,8);
 const int first=ra==0x85e3ba;
 if((first&&(ta!=16||tb!=32))||(!first&&(ta!=32||tb!=16)))abort();
 if(((U32(A,12)&tb)|(U32(B,12)&ta))||(U32(B,16)&U32(A,20))||(U32(B,20)&U32(A,16)))abort();
 if(fx_reg(FX_EAX)!=A||fx_reg(FX_EBX)!=0||fx_reg(FX_ESI)!=M||fx_reg(FX_EDI)!=M)abort();
 const rx_addr PS=first?PB:PA,QT=first?QA:QB,T=first?A:B,PT=first?PA:PB,S=first?B:A,QS=first?QB:QA;
 const uint32_t passed_flag=first?flag:(uint32_t)((flag&255u)==0);
 const uint32_t args[]={T,PT,S,QS,passed_flag};
 if(fx_reg(FX_ECX)!=PS||fx_reg(FX_EDX)!=QT)abort();
 for(unsigned i=0;i<5;i++)if(U32(entry,4+4*i)!=args[i])abort();
 /* Source final comparisons:16/32 CMP32(tb-2,1e);32/16 CMP8(flag,0).
  * Preserve this metadata contract. Typed outer has no entry-EFLAGS use;
  * first cookie XOR overwrites arithmetic flags before its own branches.
  * Complete escaping EFLAGS is outside both original recorded masks. */
 if(first){if(compare_width!=32||compare_left!=30||compare_right!=30)abort();}
 else if(compare_width!=8||compare_left!=(flag&255u)||compare_right!=0)abort();

#ifdef RX_DISPATCH_LIVE_COUNTERS
 if(live){extern void rx_dispatch_live_count(unsigned,uint32_t);rx_dispatch_live_count(0,ra);}
#endif
 if(live)phys_pair_triangles_segments_observed_live(PS,QT,M,T,PT,S,QS,passed_flag,live);else phys_pair_triangles_segments_observed(PS,QT,M,T,PT,S,QS,passed_flag);
 /* Exact typed owner preserves enteredSP/nonvolatiles. Do not hide drift or
  * remirror consumed words. Model only original RET14 after validating it. */
 if(rx_sp()!=entry||U32(entry,0)!=ra||fx_reg(FX_EBX)!=0||fx_reg(FX_ESI)!=M||fx_reg(FX_EDI)!=M||fx_reg(FX_EBP)!=bp)abort();
 rx_set_sp(expected);
}

/* Declared-output path ONLY: the four vf13 wheel callers discard all pair
 * SIMD/GPR scratch before reuse. The original8716b0 adapter declaresECX/EDX.
 * Packed BODY/composite callers remain unsupported for this new pair; no
 * invented EAX or XMM0 residue is returned as an exact packed value. */
static void triangle_circle_declared(rx_addr target,rx_addr ra,unsigned nargs,rx_addr expected,
                                     uint32_t left,uint32_t right,unsigned width){
 const rx_addr entry=rx_sp(),bp=fx_reg(FX_EBP);
 if(target!=0x8716b0||(ra!=0x85e422&&ra!=0x85eba5)||nargs!=6||
    U32(entry,0)!=ra||expected!=entry+28||bp<4||((bp&~7u)-16)!=expected)abort();
 const rx_addr M=U32(expected,12),A=U32(bp,8),QA=U32(bp,12),PA=U32(bp,16),B=U32(bp,20),QB=U32(bp,24),PB=U32(bp,28);
 const uint32_t ta=U32(A,8),tb=U32(B,8),flag=U32(bp,32);const int forward=ra==0x85e422;
 if((forward&&(ta!=0x10||tb!=0x400))||(!forward&&(ta!=0x400||tb!=0x10)))abort();
 if((U32(A,12)&tb)||(U32(B,12)&ta)||(U32(B,16)&U32(A,20))||(U32(B,20)&U32(A,16)))abort();
 if(forward){if(width!=32||left!=0x400||right!=0x400)abort();}
 else if(width!=8||left!=(flag&255u)||right!=0)abort();
 const rx_addr QC=forward?QB:QA,T=forward?A:B,QT=forward?QA:QB,PT=forward?PA:PB,C=forward?B:A,PC=forward?PB:PA;
 const uint32_t args[]={M,QT,PT,C,PC,forward?flag:(uint32_t)((flag&255u)==0)};
 if(fx_reg(FX_EAX)!=(forward?A:QB)||fx_reg(FX_ECX)!=QC||fx_reg(FX_EDX)!=T||
    fx_reg(FX_EBX)!=0||fx_reg(FX_ESI)!=B||fx_reg(FX_EDI)!=M)abort();
 for(unsigned i=0;i<6;++i)if(U32(entry,4+4*i)!=args[i])abort();
 const uint32_t edx=phys_pair_triangles_circle(QC,T,M,QT,PT,C,PC,args[5]);
 if(rx_sp()!=entry||U32(entry,0)!=ra||fx_reg(FX_EBX)!=0||fx_reg(FX_ESI)!=B||fx_reg(FX_EDI)!=M||fx_reg(FX_EBP)!=bp)abort();
 fx_set_reg(FX_EDX,edx);fx_set_reg(FX_ECX,U32(0,0x9b45e4));rx_set_sp(expected);
}

/* Additional measured BODY route: composite8000/segments32, recursively32/box4.
 * No generic pair function or original game fallback is available here. */
static void complete_pair_role(rx_addr target,rx_addr ra,unsigned nargs,rx_addr expected,uint32_t left,uint32_t right,unsigned width,rx_v4*live){
 if(target==0x8716b0&&!live&&(ra==0x85e422||ra==0x85eba5)){triangle_circle_declared(target,ra,nargs,expected,left,right,width);return;}
 if(target==0x86fac0&&(ra==0x85e3ba||ra==0x85e4f9)){triangle_pair_role(target,ra,nargs,expected,left,right,width,live);return;}
 const int composite=target==0x872860&&(ra==0x85e590||ra==0x85e9a6||ra==0x85ebe0||ra==0x85ead6);
 const int box=target==0x86a1e0&&(ra==0x85e4a2||ra==0x85e09a);
 const int rodbox=target==0x86b7f0&&(ra==0x85e152||ra==0x85eb4f);
 /* Every other role, and the packed-only closures below without a live XMM0: the case table's core generator. */
 if(!live||(!composite&&!box&&!rodbox)){
#ifdef RX_COLLISION_NATIVE_COUNTS
  {extern void rx_collision_native_provider_count(uint32_t,uint32_t);rx_collision_native_provider_count(target,ra);}
#endif
  phys_contact_pair_observed(target,ra,nargs,expected);
  return;
 }
 const rx_addr entry=rx_sp(),bp=fx_reg(FX_EBP);
 if(nargs!=((composite||rodbox)?6u:5u)||U32(entry,0)!=ra||expected!=entry+4+4*nargs||bp<4||((bp&~7u)-16)!=expected)abort();
 const rx_addr M=U32(expected,12),A=U32(bp,8),QA=U32(bp,12),PA=U32(bp,16),B=U32(bp,20),QB=U32(bp,24),PB=U32(bp,28);
 const uint32_t flag=U32(bp,32),ta=U32(A,8),tb=U32(B,8),notflag=(flag&255u)==0;
 if((U32(A,12)&tb)||(U32(B,12)&ta)||(U32(B,16)&U32(A,20))||(U32(B,20)&U32(A,16)))abort();
 uint32_t args[6]={},ecx,edx,eax=A;
 if(ra==0x85e590||ra==0x85ebe0){
  if(ta!=(ra==0x85e590?32u:0x400u)||tb!=0x8000||width!=32||left!=tb||right!=tb)abort();
  ecx=QA;edx=PA;const uint32_t v[]={M,A,B,QB,PB,flag};for(unsigned i=0;i<6;i++)args[i]=v[i];
 }else if(ra==0x85e9a6||ra==0x85ead6){
  if(ta!=0x8000||tb!=(ra==0x85e9a6?32u:0x400u)||width!=8||left!=(flag&255u)||right!=0)abort();
  ecx=QB;edx=PB;const uint32_t v[]={M,B,A,QA,PA,notflag};for(unsigned i=0;i<6;i++)args[i]=v[i];
 }else if(ra==0x85e152){
  if(ta!=4||tb!=0x400||width!=32||left!=tb||right!=tb)abort();
  ecx=QB;edx=PB;const uint32_t v[]={M,A,QA,PA,B,flag};for(unsigned i=0;i<6;i++)args[i]=v[i];
 }else if(ra==0x85eb4f){
  if(ta!=0x400||tb!=4||width!=8||left!=(flag&255u)||right!=0)abort();
  eax=PB;ecx=QA;edx=PA;const uint32_t v[]={M,B,QB,PB,A,notflag};for(unsigned i=0;i<6;i++)args[i]=v[i];
 }else if(ra==0x85e4a2){
  if(ta!=32||tb!=4||width!=8||left!=(flag&255u)||right!=0)abort();
  ecx=A;edx=PA;const uint32_t v[]={B,QB,PB,QA,notflag};for(unsigned i=0;i<5;i++)args[i]=v[i];
 }else{
  if(ta!=4||tb!=32||width!=32||left!=30||right!=30)abort();
  ecx=B;edx=PB;const uint32_t v[]={A,QA,PA,QB,flag};for(unsigned i=0;i<5;i++)args[i]=v[i];
 }
 if(fx_reg(FX_EAX)!=eax||fx_reg(FX_ECX)!=ecx||fx_reg(FX_EDX)!=edx||fx_reg(FX_EBX)!=0||fx_reg(FX_ESI)!=B||fx_reg(FX_EDI)!=M)abort();
 for(unsigned i=0;i<nargs;i++)if(U32(entry,4+4*i)!=args[i])abort();
#ifdef RX_COLLISION_NATIVE_COUNTS
 {extern void rx_collision_native_provider_count(uint32_t,uint32_t);rx_collision_native_provider_count(target,ra);}
#endif
 if(composite)fx_set_reg(FX_EAX,phys_pair_shape_vs_composite_b_observed(ecx,edx,args[0],args[1],args[2],args[3],args[4],args[5],live));
 else if(rodbox)rx_collision_0400_box_live(ecx,edx,args[0],args[1],args[2],args[3],args[4],args[5],live);
 else rx_collision_rays_vs_box_observed(ecx,args[0],args[1],args[2],args[3],args[4]&255u,edx,M,live);
 if(rx_sp()!=entry||U32(entry,0)!=ra||fx_reg(FX_EBX)!=0||fx_reg(FX_ESI)!=B||fx_reg(FX_EDI)!=M||fx_reg(FX_EBP)!=bp)abort();
 rx_set_sp(expected);
}
void rx_collision_dispatch_pair_observed(rx_addr target,rx_addr ra,unsigned nargs,rx_addr expected,uint32_t left,uint32_t right,unsigned width){complete_pair_role(target,ra,nargs,expected,left,right,width,0);}
void rx_collision_dispatch_pair_observed_live(rx_addr target,rx_addr ra,unsigned nargs,rx_addr expected,uint32_t left,uint32_t right,unsigned width,rx_v4*live){if(!live)abort();complete_pair_role(target,ra,nargs,expected,left,right,width,live);}
