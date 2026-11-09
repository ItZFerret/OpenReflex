/* Complete wheel effects: UTV004c7ed0, Buggy004ad6b0, Truck004744f0.
 * Families share algorithms, not object layouts. Every family offset, stack
 * buffer and static parameter table is derived from its complete original.
 * Existing native owners implement deformation, grip, water and track storage. */
#include "fourwheel_wheel_effects.h"
#include "world_veh.h"
#include "world_veh_util.h"
#include "world_veh_water.h"
#include "world_rider.h"
#include "world_node.h"
#include "vehicle_atv_wheels.h"
#include "phys_crt.h"
#ifdef RX_WHEEL_EFFECTS_COUNTERS
extern void rx_wheel_effects_observe(unsigned role);
#define WHEEL_OBSERVE(role) rx_wheel_effects_observe(role)
#else
#define WHEEL_OBSERVE(role) ((void)0)
#endif

typedef struct {
 uint32_t sim, early, late, wheel_delta, frame;
 uint32_t water, params, axis, turn_axis, half;
 uint32_t point[4],normal[4];
 uint32_t height_first,height_second,height_previous;
 uint32_t track_scales,track_slots,track_group,track_state;
 rx_addr static_first;
 float first_height,second_height,track_width;
} wheel_layout;
static const wheel_layout layouts[3]={
 {0x1540,0,0,0,0x334,0xc,0x20,0x1f0,0x1e0,0x1d0,{0x230,0x210,0x200,0x220},{0x2a0,0x260,0x2c0,0x280},0x1d0,0x1d0,0x1b0,0xd0,0x20,0x1b0,0x138,0xdceb00,1,1,.4f},
 {0x15c0,0x4c,0x50,0x10,0x394,0x18,0x58,0x230,0x240,0x270,{0x2b0,0x290,0x280,0x2a0},{0x2e0,0x2f0,0x2d0,0x2c0},0x270,0x220,0x20,0x108,0x58,0x20,0x1a8,0xdcec40,2.5f,2,.7f},
 {0x15d0,0x88,0x90,0,0x334,0xc,0x20,0x1f0,0x1e0,0x1d0,{0x230,0x210,0x200,0x220},{0x2a0,0x260,0x2c0,0x280},0x1d0,0x1d0,0x1b0,0xd0,0x20,0x1b0,0x138,0xdceec0,2,2,1}
};
static const uint32_t objects[4]={0x8030,0x8034,0x8028,0x802c};
static const uint32_t contacts[4]={0x8420,0x8570,0x8180,0x82d0};
static const uint32_t axes[4]={0x8440,0x8590,0x81a0,0x82f0};
static const uint32_t records[4]={0x8320,0x8470,0x8080,0x81d0};
static rx_addr simulation(rx_addr v,const wheel_layout*l){return U32(v,l->sim);}
static float dot3(rx_v4 a,rx_v4 b){rx_v4 p=v4_mul(a,b);return phys_add(phys_add(p.v[0],p.v[1]),p.v[2]);}
static rx_v4 face_axis(rx_v4 a,rx_v4 n){return v4_all3(v4_cmplt(v4_splat(dot3(n,a)),v4_ld(WORLD_ZERO4)))?v4_sub(v4_zero(),a):a;}
static void parameters(rx_addr P,rx_addr S,const wheel_layout*l){
 rx_v4 half=v4_ld(S+l->half);
 const rx_v4 values[]={v4_splat(.4f),v4_splat(F32(0,0x9cf5b0)),v4_splat(30),v4_splat(.1f),v4_splat(F32(0,0x9cc37c)),v4_splat(.2f),v4_splat(l->early?750:250),v4_splat(l->early?2000:F32(0,0x9cf5b0)),half,v4_splat(15),half,v4_splat(F32(0,0x9cc8a8)),v4_splat(.4f),v4_splat(F32(0,0x9d78dc)),v4_splat(.1f),v4_splat(3),v4_zero(),v4_splat(1),v4_splat(6)};
 for(unsigned i=0;i<19;i++){uint32_t g=U32(l->static_first,16);if(!(g&(1u<<i))){SU32(l->static_first,16,g|(1u<<i));v4_st(l->static_first-16*i,values[i]);}}
 for(unsigned i=0;i<2;i++)SU32(P,0x48+4*i,l->static_first-16*i);
 SU32(P,0x50,U32(0,0x9cc8a8));
 for(unsigned i=2;i<15;i++)SU32(P,0x4c+4*i,l->static_first-16*i);
 SF32(P,0x88,1);SF32(P,0x8c,3);
 for(unsigned i=15;i<19;i++)SU32(P,0x54+4*i,l->static_first-16*i);
}
static void deform(rx_addr v,rx_addr S,float dt,const wheel_layout*l){
 const rx_addr early=v+l->early,late=v+l->late,P=S+l->params;unsigned water[4];
 for(unsigned i=0;i<4;i++){rx_addr obj=U32(simulation(v,l),objects[i]-l->wheel_delta);water[i]=obj&&WORLD_VCALL0(obj,0x58)==5;}
 rx_addr sim=simulation(v,l);
 for(unsigned i=0;i<4;i++)if(U32(sim,contacts[i]-l->wheel_delta)&&!water[i])SU32(early,0x13d4+4*i,1);
 for(unsigned i=0;i<4;i++){
  sim=simulation(v,l);rx_v4 axis=v4_ld(sim+axes[i]-l->wheel_delta);
  v4_st(S+l->axis,face_axis(axis,v4_ld(S+l->normal[i])));
  if(i<2)v4_st(S+l->turn_axis,v4_all3(v4_cmplt(v4_splat(F32(sim,i?0x9c0:0x920)),v4_ld(WORLD_ZERO4)))?v4_sub(v4_zero(),axis):axis);
  SU32(P,0,sim+records[i]-l->wheel_delta);SU32(P,4,S+l->point[i]);SU32(P,0xc,S+l->axis);SU32(P,0x14,S+l->normal[i]);
  SU32(P,0x20,U32(early,i<2?0x1400:0x13fc));SU32(P,0x24,U32(early,i<2?0x1408:0x1404));
  SF32(P,0x28,1);SF32(P,0x2c,.5f);SU32(P,0x34,U32(early,0x13c4+4*i));SU32(P,0x3c,early+0x13d4+4*i);SU32(P,0x40,early+0x13e4+4*i);SU32(P,0x44,i<2);
  SU32(P,8,i<2?late+0x1410+16*i:0);SU32(P,0x10,i<2?S+l->turn_axis:0);SU32(P,0x18,i<2?late+0x1460+4*i:0);SU32(P,0x1c,i<2?late+0x1430+4*i:0);SU32(P,0x30,i<2?late+0x1438+20*i:0);SU32(P,0x38,i<2?early+0x13f4+4*i:0);
  if(i==0)parameters(P,S,l);
  WHEEL_OBSERVE(0);
  WORLD_CALL_SP(12,rx_world_wheel_deform_and_track_marks(P,v,U32(S,l->water),rx_bf(world_x87_pass(rx_fb(dt)))));
 }
 SU32(early,0x13e4,U32(early,0x13d4));SU32(early,0x13f0,U32(early,0x13e0));SU32(early,0x13e8,U32(early,0x13d8));SU32(early,0x13ec,U32(early,0x13dc));
 sim=simulation(v,l);for(unsigned i=0;i<4;i++)if(!U32(sim,contacts[i]-l->wheel_delta)||water[i])SU32(early,0x13d4+4*i,0);
}
static void grip(rx_addr v,rx_addr S,const wheel_layout*l){
 float g[4],a[4];
 /* All four grip calls precede all four layer calls. No extra object queries. */
 for(unsigned i=0;i<4;i++)WORLD_CALL_SP(4,g[i]=rx_world_terrain_grip(S+l->point[i],U32(S,l->water),v));
 for(unsigned i=0;i<4;i++){rx_v4 q;WORLD_CALL_SP(0,q=rx_world_terrain_layer1_factor(S+l->point[i],U32(S,l->water)));a[i]=q.v[0];}
 const float G=phys_mul(phys_add(phys_add(phys_add(g[1],g[0]),g[2]),g[3]),.25f);
 const float A=phys_mul(phys_add(a[3],phys_add(phys_add(a[1],a[0]),a[2])),.25f);
 const uint32_t out[]={0x8430,0x8580,0x8190,0x82e0};
 for(unsigned i=0;i<4;i++)SF32(simulation(v,l),out[i]-l->wheel_delta,G);
 for(unsigned i=0;i<4;i++)SF32(simulation(v,l),out[i]+4-l->wheel_delta,A);
}
static void lift(rx_addr v,rx_addr S,const wheel_layout*l){
 const rx_addr late=v+l->late;const unsigned order[]={3,2,1,0};
 for(unsigned i=0;i<4;i++)v4_st(late+0x14c0+16*i,v4_ld(S+l->point[order[i]]));
 rx_addr sim=simulation(v,l);float front=phys_mul(F32(sim,0x8090-l->wheel_delta),2),rear=phys_mul(F32(sim,0x8330-l->wheel_delta),2);
 const uint32_t old[]={0x146c,0x1468,0x1474,0x1470},trigger[]={0x147c,0x1478,0x1484,0x1480},scale[]={0x148c,0x1488,0x1494,0x1490};
 for(unsigned i=0;i<4;i++){
  sim=simulation(v,l);uint32_t force=0x8630-16*i-l->wheel_delta;v4_st(sim+force,v4_ld(WORLD_ZERO4));
  if(!U32(late,0x14a0+4*i)){SU32(late,trigger[i],0);SU32(late,old[i],0);}
  else {float k=phys_div(F32(late,0x14b0+4*i),i<2?front:rear);if(k>1)k=1;
   sim=simulation(v,l);uint32_t prior=U32(late,old[i]);float strength=phys_mul(phys_mul(F32(late,0x1514),k),k);
   SF32(late,scale[i],k);SU32(late,old[i],1);SU32(late,trigger[i],prior==0);v4_st(sim+force,v4_mul(v4_splat(strength),v4_ld(S+l->normal[order[i]])));
  }
 }
}
static void water_init(void){
 if(!(U32(0,0xd72aa8)&1)){SU32(0,0xd72aa8,U32(0,0xd72aa8)|1);WORLD_CALL_SP(0,(void)rx_water_heights_init());
#ifdef RX_ORACLE
 WORLD_ORIG(0x008f9fbf,RX_CDECL,0,0x00911e90);
#else
 extern uint32_t rx_crt_atexit(uint32_t);WORLD_CALL_SP(4,(void)rx_crt_atexit(0x911e90));
#endif
 }
}
static void water(rx_addr v,rx_addr S,float dt,const wheel_layout*l){
 rx_addr sim=simulation(v,l),late=v+l->late;const rx_v4 q=v4_ld(sim+0x730),wq=v4_ld(sim+0x7c0);
 const uint32_t anchor[]={0x400,0x360,0x220,0x2c0},offset[]={0x8500,0x83b0,0x8110,0x8260};
 rx_v4 p[4];for(unsigned i=0;i<4;i++)p[i]=v4_add(veh_rotate(wq,v4_and(v4_ld(sim+offset[i]-l->wheel_delta),v4_ld(0x92db80))),v4_ld(sim+anchor[i]));
 rx_v4 rear=v4_add(p[0],p[1]);
 rx_v4 sum=l->wheel_delta?v4_add(v4_add(p[2],rear),p[3]):v4_add(v4_add(rear,p[2]),p[3]);
 rx_v4 mid=v4_mul(sum,v4_splat(.25f));
 rx_v4 up=veh_rotate(q,v4(0,l->first_height,0,0));
 /* Buggy sums the rotated offset after the midpoint; UTV/Truck before it. */
 rx_v4 probe=l->wheel_delta?v4_add(mid,up):v4_add(up,mid);
 v4_st(late+0x1500,probe);v4_st(S+l->height_first,probe);water_init();unsigned found,splash=0;
 WORLD_CALL_SP(4,found=rx_world_water_set_height(0xd72aac,S+l->height_first));
 if(found){float d=phys_sub(F32(S,l->height_first+4),F32(late,0x1504));if(v4_all3(v4_cmplt(v4_ld(WORLD_ZERO4),v4_splat(d)))){float c=phys_div(d,3);if(c>1)c=1;float strength=phys_mul(phys_mul(F32(late,0x1518),c),c);WORLD_CALL_SP(4,rx_atv_vehicle_add_water_force(v,strength));splash=1;}}
 WORLD_CALL_SP(12,rx_world_water_wake(U32(S,l->water),v,late+0x14a0,rx_bf(world_x87_pass(rx_fb(dt)))));SF32(v,0x131c,0);
 if(splash){
  /* Buggy second probe is above the rear axle, not the four-wheel mean. */
  rx_v4 base=l->wheel_delta?v4_mul(rear,v4_ld(0xd615d0)):mid;
  rx_v4 pos=v4_add(veh_rotate(q,v4(0,l->second_height,0,0)),base);v4_st(S+l->height_second,pos);v4_st(S+l->height_previous,pos);water_init();
  WORLD_CALL_SP(4,found=rx_world_water_set_height(0xd72aac,S+l->height_second));
  if(found){float d=phys_sub(F32(S,l->height_second+4),F32(S,l->height_previous+4));SF32(v,0x131c,d);float c=phys_div(d,3);SU32(v,0x1310,1);if(!(c>=1))SF32(v,0x131c,c<=0?0:c);else SF32(v,0x131c,1);}
 }
 if(!(F32(v,0x131c)>.5f)){SU32(v,0x1310,0);SF32(v,0x1318,0);}
 else {SU32(v,0x1310,1);if(!U32(v,0x1314)&&(F32(v,0x1318)>3||F32(v,0x131c)>=1)){SU32(v,0x1314,1);SU32(U32(v,0x1a0),0x5b568,0x16);SF32(v,0x1318,3);}SF32(v,0x1318,l->sim==0x15d0?phys_add(dt,F32(v,0x1318)):phys_add(F32(v,0x1318),dt));}
}
static void tracks(rx_addr v,rx_addr S,float dt,const wheel_layout*l){
 rx_addr sim=simulation(v,l);if(U32(sim,0x2e80))return;WHEEL_OBSERVE(1);
 for(unsigned i=0;i<25;i++)SF32(S,l->track_scales+4*i,(i==15||i==18||i==19)?2:1);
 WORLD_CALL_SP(28,rx_world_tracks_update_contact_slots(v+0x1020,S+l->track_slots,v,I32(sim,0x1318),sim+0x1bb8,sim+0x1320,sim+0x17d0,v+0x11b0,S+l->track_state));
 if(!U32(v,0x1320)){
  static const int32_t groups[8][6]={{2,3,10,11,-1,4},{0,1,17,8,9,5},{4,5,18,12,13,5},{6,7,22,24,-1,4},{14,15,21,23,-1,4},{16,-1,-1,-1,-1,1},{20,-1,-1,-1,-1,1},{19,-1,-1,-1,-1,1}};
  for(unsigned i=0;i<8;i++){
   for(unsigned j=0;j<5;j++)if(groups[i][j]>=0)SU32(S+l->track_group,4*j,(uint32_t)groups[i][j]);SU32(S+l->track_group,0x18,groups[i][5]);
   WORLD_CALL_SP(32,rx_world_tracks_add_contact_points(U32(S,l->water),simulation(v,l)+0x1320,v+0x1020,v+0x11b0,rx_bf(world_x87_pass(rx_fb(l->track_width))),S+l->track_slots,S+l->track_group,S+l->track_state,S+l->track_scales,rx_bf(world_x87_pass(rx_fb(dt)))));
  }
 }
 sim=simulation(v,l);for(unsigned i=0;i<0x190;i+=4)SU32(v+0x1020,i,U32(sim+0x1320,i));WORLD_CALL_SP(12,crt_memset(sim+0x1bb8,0,100));SU32(v,0x1320,0);
}
static uint32_t update(rx_addr v,float dt,unsigned family){
 const wheel_layout*l=&layouts[family];world_frame f=world_frame_enter(l->frame,3,1);rx_addr S=f.esp;SU32(S,l->frame+8,U32(0,0x9b45e4)^(S+12));
 rx_addr sim=simulation(v,l);
 if(U32(sim,0xb0)){
  if(!(U32(0,0x9e30e0)&1)){SU32(0,0x9e30e0,U32(0,0x9e30e0)|1);SU32(0,0x9e30dc,0);}
  rx_addr owner;WORLD_CALL_SP(0,owner=rx_world_find_by_controller(0x9e30d4));SU32(S,l->water,owner);sim=simulation(v,l);
  const uint32_t centers[]={0x360,0x400,0x220,0x2c0},offsets[]={0x8390,0x84e0,0x80f0,0x8240},radii[]={0x8350,0x84a0,0x80b0,0x8200},normals[]={0x2e0,0x380,0x1a0,0x240};
  for(unsigned i=0;i<4;i++){rx_v4 base=v4_add(v4_ld(sim+offsets[i]-l->wheel_delta),v4_ld(sim+centers[i])),radius=v4_mul(v4_ld(sim+radii[i]-l->wheel_delta),v4_splat(i<2?.5f:.25f));v4_st(S+l->point[i],i%2?v4_sub(base,radius):v4_add(radius,base));v4_st(S+l->normal[i],v4_ld(sim+normals[i]));}
  v4_st(S+l->half,v4_splat(.5f));
  if(!U32(sim,0x2e80))deform(v,S,dt,l);
  grip(v,S,l);lift(v,S,l);water(v,S,dt,l);tracks(v,S,dt,l);
 }
 uint32_t cookie=U32(S,l->frame+8)^(S+12);rx_set_sp(S+12);WORLD_CALL_SP(0,phys_security_check_cookie(cookie));world_frame_leave(&f);return cookie;
}
uint32_t rx_fourwheel_wheel_effects_utv(rx_addr v,float dt){return update(v,dt,0);}
uint32_t rx_fourwheel_wheel_effects_buggy(rx_addr v,float dt){return update(v,dt,1);}
uint32_t rx_fourwheel_wheel_effects_truck(rx_addr v,float dt){return update(v,dt,2);}
RX_FUNCTION(0x004c7ed0,rx_fourwheel_wheel_effects_utv)
RX_FUNCTION(0x004ad6b0,rx_fourwheel_wheel_effects_buggy)
RX_FUNCTION(0x004744f0,rx_fourwheel_wheel_effects_truck)
