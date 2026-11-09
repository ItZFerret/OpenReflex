/* Complete0085d750 collector, retaining real broadphase/narrowphase/terrain.
 * No replacement raycast, fabricated contacts or approximate collision kernel. */
#include "waypoint_avoid_collect.h"
#include "build_activity_util.h"
#include "build_world.h"
#include "phys_vec.h"
#include "phys_math.h"
#include "phys_coll_shape.h"
#include "phys_coll_grid.h"
#include "phys_contact.h"
#include "phys_crt.h"
#include "world_wreck.h"
#include "loop_vehicle.h"
#include "fx.h"
#include "../src/rx_x87.h"

rx_addr rx_avoid_cylinder_construct(rx_addr shape,float length){
 ACT_FRAME(f,4,1);
 ACT_CALL(0,rx_collision_shape_construct(shape,8));
 SF32(shape,0x20,F32(0,0x9cd994));
 SF32(shape,0x24,length);
 SU32(shape,0,0x976704);
 ACT_END(f);return shape;
}
RX_FUNCTION(0x00847730,rx_avoid_cylinder_construct)

/*0042c210 is only an argument-copy wrapper around canonical0041c4b0.
 * Preserve its rounded float result before caller's extended half multiply. */
float rx_avoid_atan2f(float y,float x){
 const rx_addr entry=rx_sp();
 rx_set_sp(entry-16);SF32(entry-12,0,y);SF32(entry-8,0,x);
 const float a=rx_fx_atan2f(y,x);
 rx_set_sp(entry);return a;
}
RX_FUNCTION(0x0042c210,rx_avoid_atan2f)
static float sum_xzy(rx_v4 v){return phys_add(phys_add(v.v[0],v.v[2]),v.v[1]);}
static void reset_set(rx_addr f){
 SU32(f,0x790,0);SU32(f,0xf0,0);
 for(uint32_t i=0;i<30;i++){SU32(f,0x6a0+4*i,0xffffffffu);SU32(f,0x718+4*i,0xffffffffu);}
}
static void append_set(rx_addr f,rx_addr output,int32_t*used){
 if(!U32(f,0x790)||I32(f,0xf0)<=0)return;
 rx_addr src=f+0x100;
 for(int32_t i=0;i<I32(f,0xf0);i++,src+=0x30){
  if(*used<12){const rx_addr dst=output+0x30u*(uint32_t)*used;
   v4_st(dst,v4_ld(src));v4_st(dst+0x10,v4_ld(src+0x10));v4_st(dst+0x20,v4_ld(src+0x20));(*used)++;
  }
 }
}
int32_t rx_waypoint_avoid_collect(rx_addr world,rx_addr output){
 const rx_addr entry=rx_sp(),ebp=entry-4,aligned=ebp&~15u;
 const rx_addr f=aligned-0x7a4-12;
 rx_set_sp(f);SU32(f,0x7ac,U32(0,0x9b45e4)^(f+12));
 SU32(f,0x10,output);SU32(f,0x28,world);
 for(uint32_t i=0;i<12;i++){
  const rx_addr dst=output+0x30*i;
  v4_st(dst,v4_splat(F32(0,0x977b88)));
  v4_st(dst+0x10,v4_ld(0xd61670));
  v4_st(dst+0x20,v4_splat(F32(0,0x977d60)));
 }
 const rx_v4 start=v4_ld(0xdcf050),end=v4_ld(0xdcf040);
 v4_st(f+0x80,v4_scale(v4_add(start,end),F32(0,0x9322cc)));
 v4_st(f+0x70,v4_ld(0xd61640));SF32(f,0x14,0);
 if(!(start.v[0]==end.v[0]&&start.v[1]==end.v[1]&&start.v[2]==end.v[2])){
  const rx_v4 delta=v4_sub(end,start),squares=v4_mul(delta,delta);
  SF32(f,0x14,phys_sqrt(sum_xzy(squares)));
  const rx_v4 direction=phys_normalize3(delta),up=v4_ld(0xd616a0);
  const rx_v4 axis=phys_cross_ba(direction,up),axis_squares=v4_mul(axis,axis);
  const float cross_length=phys_sqrt(sum_xzy(axis_squares));
  const float dot=phys_dot3(direction,up);
  SF32(f,0x2c,dot);SF32(f,0x20,cross_length);
  /*Original requires allthree comparisonmasklanes true, NaN rejects.*/
  if(F32(0,0x9775a4)<cross_length){
   const rx_v4 first=phys_normalize3(axis),second=phys_normalize3(first);
   rx_v4 unit=v4_or(v4_and(second,v4_ld(0x92db80)),v4_ld(0x92db60));
   v4_st(f+0xd0,unit);
   float angle;SF32(f-8,0,cross_length);SF32(f-4,0,dot);
   ACT_CALLR(angle,2,rx_avoid_atan2f(cross_length,dot));
   const float half=rx_f80_to_f32(rx_f80_mul(rx_f80_from_f32(angle),rx_f80_from_f32(F32(0,0x9322cc)),64,0),0);
   SF32(f,0x18,half);
   const rx_addr prior=rx_sp();rx_set_sp(f-16);SU32(f-12,0,f+0xc0);
   phys_vec4_sincos(v4_splat(half),f+0xc0,f+0x60);rx_set_sp(prior);
   const rx_v4 c=v4_and(v4_ld(f+0x60),v4_ld(0x92db90));
   const rx_v4 s=v4_and(v4_ld(f+0xc0),v4_ld(0x92db80));
   v4_st(f+0x70,v4_mul(v4_or(s,c),unit));
  }
 }
 ACT_CALL(1,(void)rx_avoid_cylinder_construct(f+0x30,F32(f,0x14)));
 for(uint32_t i=0;i<30;i++){SU32(f,0x6a0+4*i,0xffffffffu);SU32(f,0x718+4*i,0xffffffffu);}
 ACT_CALL(4,phys_dyncyl_bounds(f+0x30,0xd61640,f+0x80,f+0x90,f+0xe0));
 const rx_addr cursor=U32(world,0x19cc),grid=U32(cursor,0x30);
 ACT_CALL(1,phys_grid_query_boxes(grid,f+0x90,f+0xe0));
 SU32(cursor,0,0);int32_t used=0;
 for(;;){
  const rx_addr cur=U32(world,0x19cc),items=U32(cur,0x30);const int32_t index=I32(cur,0);
  if(index>=I32(items,0x400))break;
  const uint32_t next=(uint32_t)index+1;SU32(cur,0,next);
  const uint32_t id=U32(items,4*next-4);if(id==0xffffffffu)break;
  const uint32_t count=(uint32_t)((int32_t)(U32(world,0x19c4)-U32(world,0x19c0))>>6);
  if(id>=count)crt_invalid_parameter_noinfo();
  const rx_addr desc=U32(world,0x19c0)+0x40*id;
  v4_st(f+0xa0,v4_ld(desc));v4_st(f+0xb0,v4_ld(desc+0x10));reset_set(f);
  ACT_CALL(7,phys_contact_dispatch(f+0xf0,f+0x30,f+0x70,f+0x80,U32(desc,0x28),f+0xb0,f+0xa0,0));
  append_set(f,output,&used);
 }
 for(int32_t i=0;i<used;i++){
  const rx_addr normal=output+0x30u*(uint32_t)i+0x10;
  v4_st(normal,v4_sub(v4_splat(0),v4_ld(normal)));
 }
 reset_set(f);SU32(f,0x24,0);SU32(f,0x1c,0);
 const rx_addr terrain=U32(world,0xcc9c);
 if(terrain){ACT_CALL(6,rx_world_grid_report_overlaps(f+0x30,f+0xf0,U32(terrain,0x31ec),f+0x80,f+0x70,f+0x24,0,f+0x1c));}
 append_set(f,output,&used);
 ACT_CALL(0,rx_registry_remove(f+0x30));
 rx_set_sp(f+12); /* popEDI/ESI/EBX before the original cookie call */
 ACT_CALL(0,phys_security_check_cookie(U32(f,0x7ac)^(f+12)));
 rx_set_sp(entry);return used;
}
RX_FUNCTION(0x0085d750,rx_waypoint_avoid_collect)
