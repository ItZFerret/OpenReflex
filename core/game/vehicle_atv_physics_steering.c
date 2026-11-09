/*00499220 ESI=S RET0: original Ackermann front-wheel steering quaternions. */
#include "vehicle_atv_physics_forces.h"
#include "build_vehicle_util.h"
#include "bike_math.h"
static float steer_length(rx_v4 a,rx_v4 b){rx_v4 d=v4_sub(a,b);return phys_sqrt(bike_hsum_021(v4_mul(d,d)));}
static rx_v4 steer_quat(float angle){float h=phys_mul(angle,.5f);return v4(0,bike_cos_poly2(phys_add(h,rx_bf(0x3fc90fdb))),0,phys_sub(rx_bf(0x80000000),bike_cos_poly2(h)));}
void rx_atv_steering_quaternions(rx_addr s){
 float angle=F32(s,0x5898);rx_v4 direct=steer_quat(angle);float adjusted=angle;
 if(bike_fabs(angle)>rx_bf(0x3727c5ac)){
  float sign=angle<0?-1:1;
  rx_v4 front=rx_ld4(s+(angle<0?0x230:0x2d0)),other=rx_ld4(s+(angle<0?0x2d0:0x230));
  rx_v4 rear=rx_ld4(s+(angle<0?0x370:0x410)),otherRear=rx_ld4(s+(angle<0?0x410:0x370));
  float u=phys_mul(sign,angle);u=phys_sub(u,phys_mul((float)rx_cvtt(phys_mul(u,rx_bf(0x3e22f983))),rx_bf(0x40c90fdb)));u=phys_sub(rx_bf(0x40490fdb),u);
  float u2=phys_mul(u,u),u4=phys_mul(u2,u2);
  float sine=phys_add(phys_mul(phys_add(phys_mul(u2,rx_bf(0xb2aee90a)),rx_bf(0x36352cc0)),u4),phys_add(phys_mul(u2,rx_bf(0xb94fb563)),rx_bf(0x3c0884a2)));
  sine=phys_add(phys_mul(sine,u4),phys_add(phys_mul(u2,rx_bf(0xbe2aaa5c)),rx_bf(0x3f7ffff9)));
  sine=phys_mul(phys_mul(sine,u),sign);
  float cosine=phys_sub(rx_bf(0x80000000),bike_cos_poly2(phys_mul(sign,angle)));
  float tangent=bike_fabs(phys_div(sine,cosine));
  float ratio=phys_div(steer_length(other,otherRear),phys_add(phys_div(steer_length(front,rear),tangent),steer_length(otherRear,rear)));
  float direction=ratio>=0?1:-1,x=bike_fabs(ratio);float z=x>=1?phys_div(1,x):x,z2=phys_mul(z,z);
  float denominator=phys_add(phys_mul(phys_add(phys_mul(z2,rx_bf(0x3d6ee04d)),rx_bf(0x3f43b24e)),z2),1);
  float numerator=phys_mul(phys_add(phys_mul(z2,rx_bf(0x3edcf805)),1),z);
  float value=phys_mul(phys_div(1,denominator),numerator);
  if(x>=1)value=phys_sub(rx_bf(0x3fc90fdb),value);
  adjusted=phys_mul(phys_mul(value,direction),sign);
 }
 rx_v4 indirect=steer_quat(adjusted);
 if(!(angle>=0)){rx_st4(s+0x58b0,direct);rx_st4(s+0x58a0,indirect);}
 else{rx_st4(s+0x58a0,direct);rx_st4(s+0x58b0,indirect);}
}
RX_FUNCTION(0x00499220,rx_atv_steering_quaternions)
