/*00493770: contact-dependent resistance plus external body forces. */
#include "vehicle_atv_physics.h"
#include "build_vehicle_util.h"
#include "bike_math.h"
#include "bike_curve.h"
#include "phys_mech.h"
void rx_atv_apply_resistance(rx_addr s){
 const rx_addr sp=rx_sp(),A=(sp-4)&~15u,F=A-0x80;rx_set_sp(F);veh_cookie(F+4,0x78);
 const uint32_t flag[]={0x94e0,0x9630,0x9240,0x9390};
 const uint32_t rate[]={0x94f4,0x9644,0x9254,0x93a4};
 const uint32_t load[]={0x94c0,0x9610,0x9220,0x9370};
 const uint32_t velocity[]={0x380,0x420,0x240,0x2e0};
 const uint32_t forces[]={0x40,0x30,0x20,0x50};
 for(unsigned i=0;i<4;i++)rx_st4(F+forces[i],rx_ld4(0x00d61670));
 for(unsigned i=0;i<4;i++)if(U32(s,flag[i])){
  float a=bike_indexed_curve(s+(i<2?0x46b4:0x4214),F32(s,rate[i]));
  float b=bike_indexed_curve(s+(i<2?0x4704:0x4264),F32(s,rate[i]));
  rx_v4 v=rx_ld4(s+velocity[i]),n=bike_normalize(v);
  rx_v4 f=v4_add(v4_mul(n,v4_sub(v4_zero(),v4_splat(a))),v4_mul(v4_sub(v4_zero(),v4_splat(b)),v));
  float factor=phys_div(F32(s,load[i]),rx_bf(0x42e16667));
  if(factor>=2)factor=2;else if(factor<=0)factor=0;
  rx_st4(F+forces[i],v4_mul(v4_splat(factor),f));
 }
 rx_st4(F+0x10,v4_zero());
 rx_v4 root=v4_mul(v4_sub(v4_zero(),rx_ld4(s+0x100)),v4_splat(F32(s,0x2f00)));
 root=v4_add(root,rx_ld4(s+0x58c0));
 root=v4_add(root,rx_ld4(F+0x40));root=v4_add(root,rx_ld4(F+0x30));root=v4_add(root,rx_ld4(F+0x20));root=v4_add(root,rx_ld4(F+0x50));
 for(unsigned i=0;i<4;i++)rx_st4(F+forces[i],v4_add(rx_ld4(s+0x9690+16*i),rx_ld4(0x00d61670)));
 rx_st4(F+0x60,root);
 const uint32_t body[]={0,4,5,2,3};
 const uint32_t where[]={0x60,0x40,0x30,0x20,0x50};
 for(unsigned i=0;i<5;i++){
  rx_set_sp(F-8);SU32(F-8,0,0);SU32(F-4,0,body[i]);
  phys_mech_add_force_at_point(F+where[i],s+0xd0,0x00d61670,body[i]);rx_set_sp(F);
 }
 rx_v4 damping=v4_mul(v4_sub(rx_ld4(F+0x10),v4_splat(1)),rx_ld4(s+0x740));
 rx_st4(s+0x780,v4_add(damping,rx_ld4(s+0x780)));rx_set_sp(sp);
}
RX_FUNCTION(0x00493770,rx_atv_apply_resistance)
