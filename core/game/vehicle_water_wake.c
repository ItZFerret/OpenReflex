/* Original wake pressure queues. Geometry and append order are original, not
 * a renderer substitute; simulation consumes these existing fixed buffers. */
#include "vehicle_water_wake.h"
#include "world_rider_util.h"
#include "phys_mech.h"
#include "bike_math.h"
#include "../src/rx_os.h"
void rx_wake_append_center(rx_addr array,uint32_t x,uint32_t z){
 rx_addr p=array+8*U32(array,0x180);SU32(p,0,x);SU32(p,4,z);SU32(array,0x180,U32(array,0x180)+1);
}
RX_FUNCTION(0x007f9510,rx_wake_append_center)
void rx_wake_append_float(rx_addr array,uint32_t bits){
 SU32(array,4*U32(array,0x1680),bits);SU32(array,0x1680,U32(array,0x1680)+1);
}
RX_FUNCTION(0x007f9550,rx_wake_append_float)
static int32_t wake_quality(void){rx_addr g=U32(0,0xa95844);return I32(U32(g,0x1c+4*U32(g,4)),0x19d0);}
static void wake_float(rx_addr array,float f){WORLD_CALL_SP(0,rx_wake_append_float(array,rx_fb(f)));}
static void wake_vertex(rx_addr array,float x,float z,uint32_t strength,float u,float v){
 wake_float(array,x);wake_float(array,phys_sub(F32(0,0x9774a8),z));
 WORLD_CALL_SP(0,rx_wake_append_float(array,strength));wake_float(array,u);wake_float(array,v);
}
static void wake_finish(rx_addr wake,int32_t quality,rx_addr center,int x87_copy){
 SU32(wake,0x390,U32(wake,0x390)+2);
 if(quality>1){
  WORLD_CALL_SP(8,rx_wake_append_center(wake+0x208,U32(center,0),U32(center,8)));
  rx_addr source=wake+0x3d0+4*(U32(wake,0x1a50)-30);
  for(unsigned i=0;i<30;i++){WORLD_CALL_SP(0,rx_wake_append_float(wake+0x1a54,x87_copy?world_x87_pass(U32(source,4*i)):U32(source,4*i)));}
  SU32(wake,0x394,U32(wake,0x394)+2);
 }else{SU32(wake,0x388,0);SU32(wake,0x30d4,0);SU32(wake,0x394,0);}
}
/* 007ec280, aligned E8-byte frame and two saved registers. Point order in
 * registers is c/d and stack a/b; actual native caller retains that mapping. */
void rx_wake_add_quad(rx_addr wake,rx_addr c,rx_addr d,rx_addr a,rx_addr b,rx_addr velocity,uint32_t strength,float dt){
 world_frame frame=world_frame_enter(0xe8,2,1);rx_addr F=frame.esp;
 if(U32(wake,0x1a50)>=0x5a0){world_frame_leave(&frame);return;}
 int32_t quality=wake_quality();float scale=(float)(quality>1?128:256);
 for(unsigned k=0;k<3;k++){
  float sum=phys_add(phys_add(k==0?phys_add(F32(a,4*k),F32(b,4*k)):phys_add(F32(b,4*k),F32(a,4*k)),F32(c,4*k)),F32(d,4*k));
  SF32(F,0x80+4*k,phys_mul(sum,F32(0,0x92ff84)));
 }
 const rx_addr points[]={a,b,c,d};
 for(unsigned i=0;i<4;i++)for(unsigned k=0;k<3;k++)SF32(F,0x30+16*i+4*k,phys_sub(F32(points[i],4*k),F32(F,0x80+4*k)));
 // Original x87 (CW53) horizontal speed-square, including its explicit zero.
 rx_f80 vx=rx_f80_from_f32(F32(velocity,0)),vz=rx_f80_from_f32(F32(velocity,8));
 rx_f80 speed2x=rx_f80_mul(vx,vx,53,0),speed2z=rx_f80_mul(vz,vz,53,0);
 rx_f80 speed2=rx_f80_add(rx_f80_add(speed2x,rx_f80_from_f32(0),53,0),speed2z,53,0);
 float square=rx_f80_to_f32(speed2,0),speed;WORLD_CALL_SP(4,speed=phys_sqrtf(square));
 SF32(F,0x2c,speed);
 if(speed>F32(0,0x9322ac)){
  for(unsigned k=0;k<3;k++)SF32(F,0x10+4*k,F32(F,0x30+4*k));
  if(square>F32(0,0x92e12c)){
   float root;WORLD_CALL_SP(4,root=phys_sqrtf(square));float inv=phys_div(F32(0,0x931f24),root);
   SF32(F,0x10,phys_mul(inv,F32(velocity,0)));SF32(F,0x14,phys_mul(inv,F32(0,0x92e12c)));SF32(F,0x18,phys_mul(F32(velocity,8),inv));
  }
  float angle=(float)(0x1.921fb6p+0-(double)rider_asin(F32(F,0x18)));
  SF32(F,8,angle);
  if(bike_fabs(angle)>F32(0,0x97743c)){
   const float x=F32(F,0x10),y=F32(F,0x14),z=F32(F,0x18);
   const float gx=F32(0,0x9cb4c0),gy=F32(0,0x9cb4c4),gz=F32(0,0x9cb4c8);
   float ax=phys_sub(phys_mul(y,gz),phys_mul(gy,z));
   float ay=phys_sub(phys_mul(gx,z),phys_mul(x,gz));
   float az=phys_sub(phys_mul(x,gy),phys_mul(y,gx));
   float n=phys_add(phys_add(phys_mul(ay,ay),phys_mul(ax,ax)),phys_mul(az,az));
   if(n>F32(0,0x92e12c)){float root;WORLD_CALL_SP(4,root=phys_sqrtf(n));float inv=phys_div(F32(0,0x931f24),root);ax=phys_mul(ax,inv);ay=phys_mul(ay,inv);az=phys_mul(az,inv);}
   SF32(F,0x10,ax);SF32(F,0x14,ay);SF32(F,0x18,az);
   for(unsigned i=0;i<4;i++){WORLD_CALL_SP(4,phys_vec3_rotate_about_axis(F+0xb0+16*i,F+0x30+16*i,F+0x10,angle));}
   float maxx=F32(F,0xb0)>F32(F,0xc0)?F32(F,0xb0):F32(F,0xc0);
   if(!(maxx>F32(F,0xd0)))maxx=F32(F,0xd0);if(!(maxx>F32(F,0xe0)))maxx=F32(F,0xe0);
   float maxz=F32(F,0xb8)>F32(F,0xc8)?F32(F,0xb8):F32(F,0xc8);
   if(!(maxz>F32(F,0xd8)))maxz=F32(F,0xd8);if(!(maxz>F32(F,0xe8)))maxz=F32(F,0xe8);
   float minx=F32(F,0xc0)>F32(F,0xb0)?F32(F,0xb0):F32(F,0xc0);
   if(!(F32(F,0xd0)>minx))minx=F32(F,0xd0);if(!(F32(F,0xe0)>minx))minx=F32(F,0xe0);
   float minz=F32(F,0xc8)>F32(F,0xb8)?F32(F,0xb8):F32(F,0xc8);
   if(!(F32(F,0xd8)>minz))minz=F32(F,0xd8);if(!(F32(F,0xe8)>minz))minz=F32(F,0xe8);
   minz=phys_sub(minz,phys_mul(speed,dt));
   SF32(F,0x30,maxx);SF32(F,0x38,maxz);SF32(F,0x40,minx);SF32(F,0x48,maxz);
   SF32(F,0x50,maxx);SF32(F,0x58,minz);SF32(F,0x60,minx);SF32(F,0x68,minz);
   angle=phys_sub(F32(0,0x9774a8),angle);SF32(F,8,angle);
   for(unsigned i=0;i<4;i++){WORLD_CALL_SP(4,phys_vec3_rotate_about_axis(F+0x30+16*i,F+0x30+16*i,F+0x10,angle));}
  }
 }
 for(unsigned i=0;i<4;i++)for(unsigned k=0;k<=8;k+=8)SF32(F,0x30+16*i+k,phys_mul(phys_div(F32(F,0x30+16*i+k),scale),F32(0,0x9322a4)));
 RX_OS(0x0091600c,wake+0x30d8);
 WORLD_CALL_SP(8,rx_wake_append_center(wake+0x80,U32(F,0x80),U32(F,0x88)));
 const float half=F32(0,0x9322cc);
 wake_vertex(wake+0x3d0,F32(F,0x40),F32(F,0x48),strength,0,0);
 wake_vertex(wake+0x3d0,F32(F,0x60),F32(F,0x68),strength,0,half);
 wake_vertex(wake+0x3d0,F32(F,0x30),F32(F,0x38),strength,half,0);
 wake_vertex(wake+0x3d0,F32(F,0x30),F32(F,0x38),strength,half,0);
 wake_vertex(wake+0x3d0,F32(F,0x60),F32(F,0x68),strength,0,half);
 wake_vertex(wake+0x3d0,F32(F,0x50),F32(F,0x58),strength,half,half);
 wake_finish(wake,quality,F+0x80,0);RX_OS(0x009160f8,wake+0x30d8);world_frame_leave(&frame);
}
RX_FUNCTION(0x007ec280,rx_wake_add_quad)
/*007ecd80: circular impact splat, original six vertices and mixed division /
 * reciprocal products retained (they need not round to the same float). */
void rx_wake_add_splat(rx_addr wake,rx_addr point,uint32_t strength,float size){
 world_frame frame=world_frame_enter(0x48,2,1);
 if(U32(wake,0x1a50)>=0x5a0){world_frame_leave(&frame);return;}
 int32_t quality=wake_quality();float scale=(float)(quality>1?128:256);
 float r=phys_mul(size,F32(0,0x9776c8)),negative=phys_sub(F32(0,0x9774a8),r);
 float direct=phys_div(r,scale),inv=phys_div(F32(0,0x931f24),scale);
 float neg=phys_mul(inv,negative),pos=phys_mul(inv,r),half=F32(0,0x9322cc);
 RX_OS(0x0091600c,wake+0x30d8);
 uint32_t n=U32(wake,0x200);SU32(wake,0x80+8*n,U32(point,0));SU32(wake,0x84+8*n,U32(point,8));SU32(wake,0x200,n+1);
 wake_vertex(wake+0x3d0,neg,pos,strength,half,0);
 wake_vertex(wake+0x3d0,neg,neg,strength,half,half);
 wake_vertex(wake+0x3d0,direct,direct,strength,1,0);
 wake_vertex(wake+0x3d0,direct,direct,strength,1,0);
 wake_vertex(wake+0x3d0,neg,neg,strength,half,half);
 wake_vertex(wake+0x3d0,pos,neg,strength,1,half);
 wake_finish(wake,quality,point,1);RX_OS(0x009160f8,wake+0x30d8);world_frame_leave(&frame);
}
RX_FUNCTION(0x007ecd80,rx_wake_add_splat)

/*007efed0: water-set wake pointer guard, EAX=set,ECX=point,XMM0=size,
 * stack strength RET4. The original push ECX/push ESI precedes the two
 * arguments to007ecd80; FLD/FSTP quiets strength before passing it on. */
void rx_wake_add_guarded_splat(rx_addr set,rx_addr point,uint32_t strength,float size){
 world_frame f=world_frame_flat(2,0);SU32(f.esp,4,point);
 rx_addr wake=U32(set,0);
 /* Original FLD m32/FSTP m32 quiets signaling NaNs and retains payload.
  * Do not use rx_f80_from/to_f32 here: the oracle F80 model discards NaN
  * payloads, producing 0x7fffffff instead of hardware's 0x7fe12345. */
 if(wake){WORLD_CALL_SP(8,rx_wake_add_splat(wake,point,world_x87_pass(strength),size));}
 world_frame_leave(&f);
}
RX_FUNCTION(0x007efed0,rx_wake_add_guarded_splat)
