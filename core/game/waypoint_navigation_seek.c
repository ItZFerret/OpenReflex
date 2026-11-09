/*00536080: steer toward the selected route entrance; actual SSE/pow operand order.*/
#include "waypoint_driver.h"
#include "build_activity_util.h"
#include "race.h"
#include "crt.h"
#include <string.h>
static double load_double(rx_addr p){uint64_t bits=rx_rd64(p);double value;memcpy(&value,&bits,8);return value;}
static float root(float v){return rx_sqrts(rx_f80_to_f32(rx_f80_from_f32(v),0));}
uint32_t rx_waypoint_navigation_seek(rx_addr o){
 ACT_FRAME_EBP(F,16,0x40,0);rx_addr v=U32(U32(U32(o,8),0x74),0x1ac);float px=F32(v,0xc0),pz=F32(v,0xc8),vx=F32(v,0x130),vz=F32(v,0x138);SF32(F,0x20,px);SF32(F,0x24,pz);SF32(F,0x10,vx);SF32(F,0x14,vz);float n=rx_adds(rx_muls(vz,vz),rx_muls(vx,vx));SF32(F,8,n);
 if(n!=0){float length;ACT_CALLR(length,1,root(n));float inv=rx_divs(F32(0,0x931f24),length);vx=rx_muls(inv,F32(F,0x10));SF32(F,0x10,vx);vz=rx_muls(F32(F,0x14),inv);SF32(F,0x14,vz);}
 rx_addr line=U32(o,0xa8);float tx=F32(line,0),tz=F32(line,8),dx=rx_subs(F32(F,0x20),tx),dz=rx_subs(F32(F,0x24),tz);SF32(F,0x18,dx);SF32(F,0x1c,dz);SF32(F,0x28,tx);SF32(F,0x2c,tz);
 double power;ACT_CALLR(power,0,crt_libm_sse2_pow(load_double(0x977638),0.0));float first=rx_divs(0,(float)power);SF32(F,8,first);ACT_CALLR(power,0,crt_libm_sse2_pow(load_double(0x977638),load_double(0x977ae8)));float gain=rx_subs(F32(F,8),rx_divs(F32(0,0x9cd9e4),(float)power));SF32(F,0xc,gain);
 n=rx_adds(rx_muls(dx,dx),rx_muls(dz,dz));SF32(F,8,n);if(n!=0){float length;ACT_CALLR(length,1,root(n));float inv=rx_divs(F32(0,0x931f24),length);dx=rx_muls(inv,F32(F,0x18));dz=rx_muls(inv,F32(F,0x1c));}
 v=U32(U32(U32(o,8),0x74),0x1ac);SF32(F,0x30,rx_muls(dx,gain));SF32(F,0x34,0);SF32(F,0x38,rx_muls(dz,gain));ACT_CALL(0,rx_race_to_vehicle_frame(v,F+0x30));
 dx=rx_subs(F32(F,0x28),F32(F,0x20));dz=rx_subs(F32(F,0x2c),F32(F,0x24));n=rx_adds(rx_muls(dz,dz),rx_muls(dx,dx));SF32(F,0x18,dx);SF32(F,0x1c,dz);SF32(F,0xc,n);if(n!=0){float length;ACT_CALLR(length,1,root(n));float inv=rx_divs(F32(0,0x931f24),length);dx=rx_muls(F32(F,0x18),inv);dz=rx_muls(F32(F,0x1c),inv);}
 float dot=rx_adds(rx_muls(dz,F32(F,0x14)),rx_muls(dx,F32(F,0x10))),half=F32(0,0x9322cc);if(!(dot>half)&&0>dot)dot=0;else if(dot>half)dot=half;
 rx_addr d=U32(o,8);float push=F32(F,0x30);SF32(d,0x60,rx_adds(rx_muls(rx_subs(F32(0,0x931f24),dot),push),F32(d,0x60)));d=U32(o,8);if(F32(o,0xe0)>F32(d,0x130))SF32(d,0x60,rx_muls(rx_subs(F32(0,0x931f24),rx_divs(rx_subs(F32(o,0xe0),F32(d,0x130)),F32(o,0xdc))),F32(d,0x60)));
 /*XMM1 still holds the rotated x push at536367; the completion test uses it, not the pre-rotation gain.*/
 uint32_t result=F32(0,0x93005c)>rx_adds(rx_muls(push,push),rx_muls(F32(F,0x38),F32(F,0x38)));ACT_END(F);return result;
}
RX_FUNCTION(0x00536080,rx_waypoint_navigation_seek)
