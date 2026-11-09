/* 00487b30: original ATV four-wheel contact/deformation, grip, water and tracks.
 * Original frame: align16, sub324, three pushes. No motorcycle offset substitution.
 * Sources: original disassembly 00487b30..00489c7f and matching decompile.
 */
#include "vehicle_atv_wheels.h"
#include "world_veh.h"
#include "world_veh_util.h"
#include "world_veh_water.h"
#include "world_node.h"
#include "world_rider.h"
#include "phys_terrain.h"
#include "phys_crt.h"
uint32_t rx_crt_atexit(uint32_t handler);
static void atv_water_atexit(void) {
#ifdef RX_ORACLE
 WORLD_ORIG(0x008f9fbf,RX_CDECL,0,0x00911e90);
#else
 WORLD_CALL_SP(4,(void)rx_crt_atexit(0x00911e90));
#endif
}
#define XYZ_MASK 0x0092db80u
/* 00424340 preserves the shared guard and lazy initialization exactly. */
rx_addr rx_atv_water_singleton(void) {
    const world_frame f=world_frame_flat(1,0); /* push ecx */
    if (!(U32(0,0x00d72aa8)&1u)) {
        SU32(0,0x00d72aa8,U32(0,0x00d72aa8)|1u);
        WORLD_CALL_SP(0,(void)rx_water_heights_init());
        atv_water_atexit();
    }
    world_frame_leave(&f);
    return 0x00d72aacu;
}
RX_FUNCTION(0x00424340,rx_atv_water_singleton)
void rx_atv_vehicle_add_water_force(rx_addr v,float amount) {
    const rx_addr s=U32(v,0x234);
    SF32(s,0x2f00,phys_add(F32(s,0x2f00),amount));
}
RX_FUNCTION(0x0045f500,rx_atv_vehicle_add_water_force)
static float atv_dot3(rx_v4 a,rx_v4 b){rx_v4 p=v4_mul(a,b);return phys_add(phys_add(p.v[0],p.v[1]),p.v[2]);}
static rx_v4 atv_axis_facing(rx_v4 a,rx_v4 n){return v4_all3(v4_cmplt(v4_splat(atv_dot3(a,n)),v4_ld(WORLD_ZERO4)))?v4_sub(v4_zero(),a):a;}
static int atv_on_water(rx_addr obj){return obj&&WORLD_VCALL0(obj,0x58)==5;}
static void atv_wheel_static(uint32_t bit,rx_addr at,rx_v4 value){uint32_t g=U32(0,0xdced90);if(!(g&bit)){SU32(0,0xdced90,g|bit);v4_st(at,value);}}
static void atv_wheel_parameters(rx_addr P,rx_addr S){
 const rx_v4 h=v4_ld(S+0x1d0);
 const rx_v4 values[]={v4_splat(.4f),v4_splat(F32(0,0x9cf5b0)),v4_splat(30),v4_splat(.1f),v4_splat(F32(0,0x9cc37c)),v4_splat(.2f),v4_splat(250),v4_splat(F32(0,0x9cf5b0)),h,v4_splat(15),h,v4_splat(F32(0,0x9cc8a8)),v4_splat(.4f),v4_splat(F32(0,0x9d78dc)),v4_splat(.1f),v4_splat(3),v4_zero(),v4_splat(1),v4_splat(6)};
 for(unsigned i=0;i<19;i++)atv_wheel_static(1u<<i,0xdced80-16*i,values[i]);
 for(unsigned i=0;i<2;i++)SU32(P,0x48+4*i,0xdced80-16*i);
 SU32(P,0x50,U32(0,0x9cc8a8));
 for(unsigned i=2;i<15;i++)SU32(P,0x4c+4*i,0xdced80-16*i);
 SF32(P,0x88,1);SF32(P,0x8c,3);
 for(unsigned i=15;i<19;i++)SU32(P,0x54+4*i,0xdced80-16*i);
}
static void atv_wheel_deformation(rx_addr v,rx_addr S,float dt){
 /* Exact call order: rear left, rear right, front left, front right. */
 const uint32_t object[]={0x90f0,0x90f4,0x90e8,0x90ec},contact[]={0x94e0,0x9630,0x9240,0x9390};
 const uint32_t axis[]={0x9500,0x9650,0x9260,0x93b0},normal[]={0x270,0x250,0x290,0x260},point[]={0x200,0x1f0,0x210,0x220};
 const uint32_t record[]={0x93e0,0x9530,0x9140,0x9290};
 unsigned water[4];
 for(unsigned i=0;i<4;i++){rx_addr s=U32(v,0x1700);water[i]=atv_on_water(U32(s,object[i]));}
 rx_addr s=U32(v,0x1700);
 for(unsigned i=0;i<4;i++)if(U32(s,contact[i])&&!water[i])SU32(v,0x1594+4*i,1);
 const rx_addr P=S+0x28;
 for(unsigned i=0;i<4;i++){
  s=U32(v,0x1700);v4_st(S+0x1c0,atv_axis_facing(v4_ld(s+axis[i]),v4_ld(S+normal[i])));
  if(i<2){const float turn=F32(s,i?0xa60:0x9c0);rx_v4 a=v4_ld(s+axis[i]);v4_st(S+0x1e0,v4_all3(v4_cmplt(v4_splat(turn),v4_ld(WORLD_ZERO4)))?v4_sub(v4_zero(),a):a);}
  SU32(P,0,s+record[i]);SU32(P,4,S+point[i]);SU32(P,0xc,S+0x1c0);SU32(P,0x14,S+normal[i]);
  SU32(P,0x20,U32(v,i<2?0x15c0:0x15bc));SU32(P,0x24,U32(v,i<2?0x15c8:0x15c4));
  SF32(P,0x28,1);SF32(P,0x2c,.5f);SU32(P,0x34,U32(v,0x1584+4*i));SU32(P,0x3c,v+0x1594+4*i);SU32(P,0x40,v+0x15a4+4*i);SU32(P,0x44,i<2);
  SU32(P,8,i<2?v+(i?0x15e0:0x15d0):0);SU32(P,0x10,i<2?S+0x1e0:0);SU32(P,0x18,i<2?v+0x1620+4*i:0);SU32(P,0x1c,i<2?v+0x15f0+4*i:0);SU32(P,0x30,i<2?v+(i?0x160c:0x15f8):0);SU32(P,0x38,i<2?v+0x15b4+4*i:0);
  if(i==0)atv_wheel_parameters(P,S);
  WORLD_CALL_SP(12,rx_world_wheel_deform_and_track_marks(P,v,U32(S,0xc),dt));
 }
 SU32(v,0x15a4,U32(v,0x1594));SU32(v,0x15b0,U32(v,0x15a0));SU32(v,0x15a8,U32(v,0x1598));SU32(v,0x15ac,U32(v,0x159c));
 s=U32(v,0x1700);for(unsigned i=0;i<4;i++)if(!U32(s,contact[i])||water[i])SU32(v,0x1594+4*i,0);
}
static void atv_terrain_grip(rx_addr v,rx_addr S){
 const uint32_t obj[]={0x90f0,0x90f4,0x90e8,0x90ec},point[]={0x200,0x1f0,0x210,0x220};float grip[4],layer[4];
 for(unsigned i=0;i<4;i++){
  if(atv_on_water(U32(U32(v,0x1700),obj[i]))){grip[i]=.5f;layer[i]=0;}
  else{WORLD_CALL_SP(4,grip[i]=rx_world_terrain_grip(S+point[i],U32(S,0xc),v));rx_v4 x;WORLD_CALL_SP(0,x=rx_world_terrain_layer1_factor(S+point[i],U32(S,0xc)));layer[i]=x.v[0];}
 }
 const float g=phys_mul(phys_add(phys_add(phys_add(grip[1],grip[0]),grip[2]),grip[3]),.25f);
 const float l=phys_mul(phys_add(phys_add(phys_add(layer[1],layer[0]),layer[2]),layer[3]),.25f);
 const uint32_t out[]={0x94f0,0x9640,0x9250,0x93a0};
 for(unsigned i=0;i<4;i++)SF32(U32(v,0x1700),out[i],g);
 for(unsigned i=0;i<4;i++)SF32(U32(v,0x1700),out[i]+4,l);
}
static void atv_wheel_lift(rx_addr v,rx_addr S){
 const uint32_t point[]={0x220,0x210,0x1f0,0x200};
 for(unsigned i=0;i<4;i++)v4_st(v+0x1680+16*i,v4_ld(S+point[i]));
 rx_addr s=U32(v,0x1700);const float front=phys_mul(F32(s,0x9150),2),rear=phys_mul(F32(s,0x93f0),2);
 const uint32_t force[]={0x96c0,0x96b0,0x96a0,0x9690},old[]={0x162c,0x1628,0x1634,0x1630},trigger[]={0x163c,0x1638,0x1644,0x1640},scale[]={0x164c,0x1648,0x1654,0x1650},normal[]={0x260,0x290,0x250,0x270};
 for(unsigned i=0;i<4;i++){
  s=U32(v,0x1700);v4_st(s+force[i],v4_ld(WORLD_ZERO4));
  if(!U32(v,0x1660+4*i)){SU32(v,trigger[i],0);SU32(v,old[i],0);}
  else{
   float k=phys_div(F32(v,0x1670+4*i),i<2?front:rear);
   if(k>1)k=1;
   s=U32(v,0x1700);const uint32_t prior=U32(v,old[i]);const float strength=phys_mul(phys_mul(F32(v,0x16d4),k),k);
   SF32(v,scale[i],k);SU32(v,old[i],1);SU32(v,trigger[i],prior==0);v4_st(s+force[i],v4_mul(v4_splat(strength),v4_ld(S+normal[i])));
  }
 }
}
static void atv_track_points(rx_addr v,rx_addr S,rx_addr pos,rx_addr previous,rx_addr slots,float scale,float dt){
 WORLD_CALL_SP(32,rx_world_tracks_add_contact_points(U32(S,0xc),pos,previous,slots,scale,S+0x110,S+0x1a0,S+0x150,S+0xc8,dt));
}
static void atv_track_group(rx_addr G,const int32_t* c){for(unsigned i=0;i<3;i++)if(c[i]>=0)SU32(G,4*i,c[i]);SU32(G,0x18,c[3]);}
static void atv_track_contacts(rx_addr v,rx_addr S,float dt){
 rx_addr s=U32(v,0x1700);if(U32(s,0x2e80))return;
 for(unsigned i=0;i<13;i++)SF32(S,0xc8+4*i,i<6?1:2);
 WORLD_CALL_SP(28,rx_world_tracks_update_contact_slots(v+0x1020,S+0x110,v,I32(s,0x1318),s+0x1bb8,s+0x1320,s+0x17d0,v+0x11b0,S+0x150));
 if(!U32(v,0x1320)){
  static const int32_t group[4][4]={{0,1,2,6},{8,10,-1,2},{9,11,-1,2},{6,7,12,3}};
  /* First group writes six indices; later groups deliberately retain unmodified words. */
  for(unsigned i=0;i<6;i++)SU32(S+0x1a0,4*i,i);
  SU32(S+0x1a0,0x18,6);
  atv_track_points(v,S,U32(v,0x1700)+0x1320,v+0x1020,v+0x11b0,.25f,dt);
  for(unsigned i=1;i<4;i++){atv_track_group(S+0x1a0,group[i]);atv_track_points(v,S,U32(v,0x1700)+0x1320,v+0x1020,v+0x11b0,.25f,dt);}
 }
 s=U32(v,0x1700);for(unsigned i=0;i<0x190;i+=4)SU32(v+0x1020,i,U32(s+0x1320,i));crt_memset(s+0x1bb8,0,100);
 rx_addr ra=U32(v,0x1a0);
 if(!U32(ra,0xaf00)){
  const rx_addr tt=U32(0,0xa9577c);SU32(S,0x18,0);
  for(int32_t i=0;i<I32(U32(v,0x1a0),0x670);i++){
   rx_addr p=v+0x14c0+4*i;SU32(S,0x1c,p);int32_t strip=I32(p,0);
   if(strip>=0){int32_t index=strip^0x4000;if(index>=0&&index<I32(tt,0xc)){WORLD_CALL_SP(0,phys_tracks_cut_strip(tt,index));SU32(U32(tt,0x24)+(uint32_t)index*0xc0,0x98,1);SU32(tt,0x28,U32(tt,0x28)-1);}SU32(p,0,0xffffffff);}
   SU32(S,0x18,i+1);SU32(S,0x1c,p+4);
  }
 }else{
  for(unsigned i=0;i<16;i++)SF32(S,0xc8+4*i,1);
  WORLD_CALL_SP(28,rx_world_tracks_update_contact_slots(v+0x1330,S+0x110,v,I32(ra,0x670),ra+0x674,ra+0x6c0,ra+0x7c0,v+0x14c0,S+0x150));
  if(!U32(v,0x1320)){
   static const int32_t group[6][4]={{2,6,12,3},{1,7,13,3},{5,3,-1,2},{4,8,10,3},{11,14,-1,2},{9,15,-1,2}};
   for(unsigned i=0;i<6;i++){atv_track_group(S+0x1a0,group[i]);atv_track_points(v,S,U32(v,0x1a0)+0x6c0,v+0x1330,v+0x14c0,.1f,dt);}
  }
  ra=U32(v,0x1a0);crt_memcpy(v+0x1330,ra+0x6c0,U32(ra,0x670)<<4);
 }
 ra=U32(v,0x1a0);crt_memset(ra+0x674,0,U32(ra,0x670)*4);SU32(v,0x1320,0);
}
static void atv_water_contact(rx_addr v,rx_addr S,float dt){
 rx_addr s=U32(v,0x1700);const rx_v4 q=v4_ld(s+0x730),q2=v4_ld(s+0x7c0),cj=v4(-1,-1,-1,1);
 /* ADDPS stream combines rear right, rear left, front left, front right before the quarter factor. */
 const uint32_t anchor[]={0x4a0,0x400,0x2c0,0x360},offset[]={0x95c0,0x9470,0x91d0,0x9320};
 rx_v4 mid=v4_zero();
 for(unsigned i=0;i<4;i++){
  rx_v4 p=v4_add(veh_rotate(q2,v4_and(v4_ld(s+offset[i]),v4_ld(XYZ_MASK))),v4_ld(s+anchor[i]));
  mid=i?v4_add(mid,p):p;
 }
 mid=v4_mul(mid,v4_splat(.25f));v4_st(S+0x2c0,mid);v4_st(S+0x280,v4(0,1,0,0));v4_st(S+0x2b0,q);
 const rx_v4 up=veh_quat_mul(q,veh_quat_mul(v4_and(v4_ld(S+0x280),v4_ld(XYZ_MASK)),v4_mul(q,cj)));
 v4_st(v+0x16c0,v4_add(up,mid));v4_st(S+0x1d0,v4_ld(v+0x16c0));
 if(!(U32(0,0xd72aa8)&1)){SU32(0,0xd72aa8,U32(0,0xd72aa8)|1);WORLD_CALL_SP(0,(void)rx_water_heights_init());atv_water_atexit();}
 unsigned splash=0,found;WORLD_CALL_SP(4,found=rx_world_water_set_height(0xd72aac,S+0x1d0));
 if(found){const float dy=phys_sub(F32(S,0x1d4),F32(v,0x16c4));if(v4_all3(v4_cmplt(v4_ld(WORLD_ZERO4),v4_splat(dy)))){float c=phys_div(dy,F32(0,0x9d789c));if(c>1)c=1;const float k=phys_mul(phys_mul(F32(v,0x16d8),c),c);WORLD_CALL_SP(4,rx_atv_vehicle_add_water_force(v,k));splash=1;}}
 WORLD_CALL_SP(12,rx_world_water_wake(U32(S,0xc),v,v+0x1660,dt));SF32(v,0x131c,0);
 if(splash){
  const rx_v4 p=v4_add(up,mid);v4_st(S+0x1d0,p);v4_st(S+0x1a0,p);
  rx_addr set;WORLD_CALL_SP(8,set=rx_atv_water_singleton());
  WORLD_CALL_SP(4,found=rx_world_water_set_height(set,S+0x1d0));
  if(found){const float d=phys_sub(F32(S,0x1d4),F32(S,0x1a4));SF32(v,0x131c,d);const float c=phys_div(d,F32(0,0x9d789c));SU32(v,0x1310,1);if(!(c>=1))SF32(v,0x131c,c<=0?0:c);else SF32(v,0x131c,1);}
 }
 if(!(F32(v,0x131c)>.5f)){SU32(v,0x1310,0);SF32(v,0x1318,0);}
 else{
  SU32(v,0x1310,1);
  if(!U32(v,0x1314)&&(F32(v,0x1318)>3||F32(v,0x131c)>=1)){SU32(v,0x1314,1);SU32(U32(v,0x1a0),0x5b568,0x16);SF32(v,0x1318,3);}
  SF32(v,0x1318,phys_add(F32(v,0x1318),dt));
 }
}
void rx_atv_vehicle_wheels(rx_addr v,float dt){
 world_frame f=world_frame_enter(0x324,3,1);rx_addr S=f.esp;SU32(S,0x32c,U32(0,0x9b45e4)^(S+0xc));
 float dtp=rx_bf(world_x87_pass(rx_fb(dt)));rx_addr s=U32(v,0x1700);
 if(U32(s,0xb0)&&U32(v,0x1a0)){
  if(!(U32(0,0x9e30e0)&1)){SU32(0,0x9e30e0,U32(0,0x9e30e0)|1);SU32(0,0x9e30dc,0);}
  rx_addr water;WORLD_CALL_SP(0,water=rx_world_find_by_controller(0x9e30d4));SU32(S,0xc,water);s=U32(v,0x1700);
  v4_st(S+0x200,v4_add(v4_mul(v4_ld(s+0x9410),v4_splat(.5f)),v4_add(v4_ld(s+0x9450),v4_ld(s+0x400))));v4_st(S+0x270,v4_ld(s+0x380));
  v4_st(S+0x1f0,v4_sub(v4_add(v4_ld(s+0x95a0),v4_ld(s+0x4a0)),v4_mul(v4_ld(s+0x9560),v4_splat(.5f))));v4_st(S+0x250,v4_ld(s+0x420));v4_st(S+0x1d0,v4_splat(.5f));
  v4_st(S+0x210,v4_add(v4_mul(v4_ld(s+0x9170),v4_splat(.25f)),v4_add(v4_ld(s+0x91b0),v4_ld(s+0x2c0))));v4_st(S+0x290,v4_ld(s+0x240));
  v4_st(S+0x220,v4_sub(v4_add(v4_ld(s+0x9300),v4_ld(s+0x360)),v4_mul(v4_ld(s+0x92c0),v4_splat(.25f))));v4_st(S+0x260,v4_ld(s+0x2e0));v4_st(S+0x1a0,v4_splat(.25f));
  if(!U32(s,0x2e80))atv_wheel_deformation(v,S,dtp);
  atv_terrain_grip(v,S);atv_wheel_lift(v,S);atv_water_contact(v,S,dtp);atv_track_contacts(v,S,dtp);
 }
 phys_security_check_cookie(U32(S,0x32c)^(S+0xc));world_frame_leave(&f);
}

RX_FUNCTION(0x00487b30,rx_atv_vehicle_wheels)
