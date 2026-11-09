/* Complete handwritten004d3350/004d35c0, including original dead frame.
 * The routines intentionally evaluate every ratio's two real parameter curves.
 * There is no new gearbox policy or parameter default. */
#include "utv_force_gears.h"
#include "phys_vec.h"
#include "fx_util.h"
static rx_v4 gear_product(rx_v4 a,rx_v4 b){
 rx_v4 shuffle=v4_shuf(b,b,0x1b);
 rx_v4 x=v4_mul(v4_mul(v4_lane(a,0),shuffle),rx_ld4(0x93a1f0));
 shuffle=v4_shuf(shuffle,shuffle,0xb1);
 rx_v4 y=v4_mul(v4_mul(v4_lane(a,1),shuffle),rx_ld4(0x93a200));
 shuffle=v4_shuf(shuffle,shuffle,0x1b);
 rx_v4 z=v4_mul(v4_mul(shuffle,v4_lane(a,2)),rx_ld4(0x93a210));
 return v4_add(v4_add(z,y),v4_add(v4_mul(v4_lane(a,3),b),x));
}
static float curve(rx_addr b,rx_addr address,rx_addr ra,float value){
 fx_set_reg(FX_ESI,address);SU32(b-4,0,ra);rx_set_sp(b-4);
 return rx_utv_force_gear_curve(b,value);
}
typedef struct {unsigned index,ratios,timer,up,down;rx_addr power,coast;} GearLayout;
static void choose_gear(rx_addr s,int power,const GearLayout*l){
 fx_frame frame;fx_frame_enter(&frame,0x38,16,FX_EBX,FX_ESI);rx_addr b=frame.E;
 rx_v4 root=rx_ld4(s+0x730),velocity=v4_and(rx_ld4(s+0x100),rx_ld4(0x92db80));
 /* Original MULPS has the constant as LEFT operand in the conjugate. */
 rx_v4 local=gear_product(v4_mul(rx_ld4(0x93a220),root),gear_product(velocity,root));
 rx_st4(b+0x30,local);
 float normalized=phys_div(F32(b,0x38),F32(s,0x472c));SF32(b,0xc,normalized);
 if(F32(0,0x9322cc)>normalized){normalized=F32(0,0x9322cc);SF32(b,0xc,normalized);}
 SF32(b,0x2c,phys_mul(F32(s,0x4d4c),F32(0,0x977870)));
 if(!U32(s,0x2eb0)){
  uint32_t current=U32(s,l->index);fx_set_reg(FX_EAX,current);
  SF32(b,0x24,F32(s,l->ratios+4*current));SF32(b,0x14,0);
  SF32(b,0x18,F32(s,0x4ddc));fx_set_reg(FX_EAX,s+l->ratios);
  SU32(b,8,s+l->ratios);fx_set_reg(FX_EBX,0);
  for(uint32_t gear=0;gear<5;gear++){
   rx_addr ratio=U32(b,8);fx_set_reg(FX_ECX,ratio);
   float value=F32(ratio,0);SF32(b,0x1c,value);
   float input=phys_mul(value,F32(b,0xc));
   /* COMISS/JB retains the product on unordered, not MINSS semantics. */
   if(input>=F32(b,0x2c))input=F32(b,0x2c);
   if(input<=0)input=0;SF32(b,0x10,input);
   float grip=curve(b,s+0x4e24,(power?l->power:l->coast)+0x1b7,input);
   grip=phys_mul(phys_mul(grip,F32(b,0x18)),F32(b,0x24));
   SF32(b,0x20,phys_sub(F32(0,0x9774a8),grip));
   float effort=curve(b,s+0x4c1c,(power?l->power:l->coast)+0x1e9,F32(b,0x10));
   effort=phys_mul(effort,F32(b,0x1c));
   /* SUBSS then ADDSS is deliberate and can round; do not simplify. */
   effort=phys_add(phys_sub(effort,F32(b,0x20)),F32(b,0x20));
   if(effort>F32(b,0x14)||gear==0){SF32(b,0x14,effort);SU32(b,0x28,gear);}
   SU32(b,8,U32(b,8)+4);fx_set_reg(FX_EBX,gear+1);
  }
  current=U32(s,l->index);uint32_t selected=U32(b,0x28);
  if(power){
   fx_set_reg(FX_ECX,current);fx_set_reg(FX_EAX,selected);
   if((int32_t)selected!=(int32_t)current){
    rx_addr threshold=s+((int32_t)selected>(int32_t)current?l->up:l->down);
    if(F32(s,l->timer)>F32(threshold,0)){SF32(s,l->timer,0);SU32(s,l->index,selected);}
   }
  }else{
   fx_set_reg(FX_EAX,current);fx_set_reg(FX_ECX,selected);
   if((int32_t)selected<(int32_t)current&&F32(s,l->timer)>F32(s,l->down)){
    SU32(s,l->index,selected);SF32(s,l->timer,0);
   }
  }
 }
 fx_frame_leave(&frame);
}
static const GearLayout utv={0x8660,0x8664,0x8684,0x867c,0x8680,0x4d3350,0x4d35c0};
static const GearLayout buggy={0x8650,0x8654,0x8674,0x866c,0x8670,0x4b8ba0,0x4b8e10};
static const GearLayout truck={0x8664,0x8668,0x868c,0x8684,0x8688,0x455200,0x455470};
void rx_utv_select_power_gear(rx_addr s){choose_gear(s,1,&utv);}
void rx_utv_select_coast_gear(rx_addr s){choose_gear(s,0,&utv);}
void rx_buggy_select_power_gear(rx_addr s){choose_gear(s,1,&buggy);}
void rx_buggy_select_coast_gear(rx_addr s){choose_gear(s,0,&buggy);}
void rx_truck_select_power_gear(rx_addr s){choose_gear(s,1,&truck);}
void rx_truck_select_coast_gear(rx_addr s){choose_gear(s,0,&truck);}
