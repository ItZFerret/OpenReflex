/* Four-wheel collision composite placement (vehicle vf62).
 * 004c0e70 UTV, 004a3170 Buggy, 0046b160 TrophyTruck.
 * The layouts below retain each original aligned frame and callee-visible
 * scratch address. Wheel order is the original model-node order. */
#include "vehicle_fourwheel_composite.h"
#include "world_veh.h"
#include "world_veh_util.h"
#include "world.h"
#include "world_extern.h"
#include "phys_coll_shape.h"
#include "phys_crt.h"

typedef struct {
 unsigned frame, velocity, angular, position, rotation, axis, sine, cosine, spin;
 unsigned wheel_position, wheel_rotation, nodes, indices, simulation;
 unsigned child_a[4], child_b[4], corners, high, low;
 unsigned bounds[6]; /* +x,-x,+y,-y,+z,-z constant addresses */
 unsigned node_offsets[4];
} composite_layout;
static const composite_layout truck={
 0x154,0xb0,0xa0,0x80,0x40,0x50,0x20,0x30,0x30,
 0x90,0x60,0x20,0x1620,0x15d0,
 {0x140,0x140,0x140,0x140},{0x70,0x70,0x70,0x70},0xc0,0x20,0x70,
 {0x935890,0x97739c,0x9322c0,0x9776d8,0x92fd88,0x9d78b8},
 {0x1370,0x1374,0x1368,0x136c}};
static const composite_layout buggy={
 0x184,0xc0,0xd0,0xb0,0x60,0x70,0x40,0x50,0x50,
 0x90,0x80,0x40,0x1610,0x15c0,
 {0x170,0x170,0x170,0x170},{0x160,0x160,0x160,0x160},0xe0,0x50,0x40,
 {0x9cd9e4,0x977db8,0x9322c0,0x9776d8,0x930004,0x977dbc},
 {0x13a0,0x13c0,0x1378,0x1390}};
static const composite_layout utv={
 0x1b4,0xa0,0x90,0x60,0x20,0x80,0x50,0x40,0x50,
 0x30,0x10,0,0x1590,0x1540,
 {0x1a0,0x190,0x140,0x180},{0x150,0x130,0x170,0x160},0xb0,0x40,0x70,
 {0x9322c0,0x9776d8,0x9d789c,0x97784c,0x977650,0x977844},
 {0x1370,0x1388,0x13a0,0x13b8}};

static rx_v4 composite_axis(void){
 rx_v4 a=v4_ld(0xd61680),p=v4_mul(a,a);
 rx_v4 q=veh_normalize_len2(a,phys_add(phys_add(p.v[0],p.v[1]),p.v[2]));
 return v4_or(v4_and(q,v4_ld(0x92db80)),v4_ld(0x92db60));
}
/* These sites add the x and w products in the opposite order from the
 * common helper. Retain the SSE operand preference for distinct NaNs. */
static rx_v4 composite_mul_xw(rx_v4 a,rx_v4 b){
 rx_v4 x=v4_mul(v4_mul(v4_lane(a,0),v4_perm(b,3,2,1,0)),v4(1,-1,1,-1));
 rx_v4 y=v4_mul(v4_mul(v4_lane(a,1),v4_perm(b,2,3,0,1)),v4(1,1,-1,-1));
 rx_v4 z=v4_mul(v4_mul(v4_perm(b,1,0,3,2),v4_lane(a,2)),v4(-1,1,1,-1));
 return v4_add(v4_add(z,y),v4_add(x,v4_mul(v4_lane(a,3),b)));
}
static void refresh(rx_addr v){
 rx_addr body=U32(v,0x17c);
 world_keyed_refresh(U32(body,8),U32(body,4)+0x1954,WCTX(.ebx=v));
}
static void place(rx_addr v,const composite_layout *l,int is_buggy){
 world_frame f=world_frame_enter(l->frame,3,1);rx_addr S=f.esp;
 SU32(S,l->frame+8,U32(0,0x9b45e4)^(S+12));
 if(U8(v,0x174)){
  WORLD_VCALL(v,0x94,S+l->velocity);WORLD_VCALL(v,0x98,S+l->angular);
  v4_st(S+l->position,v4(F32(v,0xc0),F32(v,0xc4),F32(v,0xc8),F32(v,0xc8)));
  v4_st(S+l->rotation,v4_ld(v+0xe0));
  WORLD_CALL_SP(20,rx_world_dynobj_set_pose(v+0x178,S+l->position,S+l->rotation,S+l->velocity,S+l->angular));
  v4_st(S+l->axis,composite_axis());
  WORLD_CALL_SP(4,phys_vec4_sincos(v4_splat(F32(0,0x9776e0)),S+l->sine,S+l->cosine));
  rx_v4 spin=v4_mul(v4_or(v4_and(v4_ld(S+l->sine),v4_ld(0x92db80)),v4_and(v4_ld(S+l->cosine),v4_ld(0x92db90))),v4_ld(S+l->axis));
  v4_st(S+l->spin,spin);
  rx_addr comp=U32(world_handle_object(U32(v,0x17c),WCTX(.ebx=v)),0x2c);
  if(l->nodes)for(unsigned i=0;i<4;i++)SU32(S,l->nodes+4*i,U32(v,l->node_offsets[i]));
  if(is_buggy && !(U8(0,0xdce7d0)&1)){
   SU32(0,0xdce7d0,U32(0,0xdce7d0)|1);
   float zero=F32(0,0x92e12c);
   rx_v4 a=v4(F32(0,0x9774dc),zero,zero,zero),b=v4(F32(0,0x931da8),zero,zero,zero);
   v4_st(0xdce790,a);v4_st(0xdce7a0,b);v4_st(0xdce7b0,a);v4_st(0xdce7c0,b);
  }
  for(unsigned i=0;i<4;i++){
   rx_addr node=l->nodes?U32(S,l->nodes+4*i):U32(v,l->node_offsets[i]);
   WORLD_CALL_SP(4,rx_world_node_world_pose(node,S+l->wheel_position,S+l->wheel_rotation));
   if(is_buggy){
    rx_v4 p=v4_add(veh_rotate(v4_ld(S+l->wheel_rotation),v4_ld(0xdce790+16*i)),v4_ld(S+l->wheel_position));
    v4_st(S+l->wheel_position,p);
   }
   rx_v4 wheel_q=v4_ld(S+l->wheel_rotation);
   v4_st(S+l->wheel_rotation,is_buggy?composite_mul_xw(wheel_q,spin):veh_quat_mul(wheel_q,spin));
   uint32_t index=U32(v,l->indices+4*i);
   WORLD_CALL_SP(8,phys_composite_set_child_position(comp,S+l->wheel_position,S+l->position,index,S+l->rotation));
   WORLD_CALL_SP(4,phys_composite_set_child_rotation(comp,S+l->wheel_rotation,index,S+l->rotation));
   world_bounds_check(index,(U32(comp,0x30)-U32(comp,0x2c))>>2);
   WORLD_VCALL(U32(U32(comp,0x2c),4*index),8,S+l->wheel_rotation,S+l->wheel_position,S+l->child_a[i],S+l->child_b[i]);
  }
  if(!is_buggy)refresh(v);
  const unsigned ix[]={0,0,0,1,0,1,1,1},iy[]={0,0,1,0,1,0,1,1},iz[]={0,1,0,0,1,1,0,1};
  for(unsigned i=0;i<8;i++){
   float z=F32(0,l->bounds[4+iz[i]]);
   v4_st(S+l->corners+16*i,v4(F32(0,l->bounds[ix[i]]),F32(0,l->bounds[2+iy[i]]),z,z));
  }
  rx_v4 q=v4_ld(S+l->rotation),base=v4_ld(S+l->position);
  rx_v4 high=v4_splat(F32(0,0x977550)),low=v4_splat(F32(0,0x977494));
  for(unsigned i=0;i<8;i++){
   rx_v4 corner=v4_ld(S+l->corners+16*i);
   if(is_buggy)corner=v4_add(v4(F32(0,0x92e12c),F32(0,0x92e12c),F32(0,0x931f24),F32(0,0x931f24)),corner);
   rx_v4 rotated;
   if(l==&utv)rotated=veh_rotate(q,corner);
   else rotated=composite_mul_xw(q,veh_quat_mul(v4_xyz(corner),v4_mul(v4(-1,-1,-1,1),q)));
   rx_v4 p=v4_add(rotated,base);high=v4_max(high,p);low=v4_min(low,p);
  }
  v4_st(S+l->high,high);v4_st(S+l->low,low);
  if(is_buggy)refresh(v);
  WORLD_CALL_SP(8,rx_world_vehicle_collect_nearby_statics(U32(v,l->simulation)+0x1300,S+l->high,U32(U32(v,0x188),0x20134),S+l->low));
 }
 phys_security_check_cookie(U32(S,l->frame+8)^(S+12));world_frame_leave(&f);
}
void rx_utv_place_composite(rx_addr v){place(v,&utv,0);}
RX_FUNCTION(0x004c0e70,rx_utv_place_composite)
void rx_buggy_place_composite(rx_addr v){place(v,&buggy,1);}
RX_FUNCTION(0x004a3170,rx_buggy_place_composite)
void rx_truck_place_composite(rx_addr v){place(v,&truck,0);}
RX_FUNCTION(0x0046b160,rx_truck_place_composite)
