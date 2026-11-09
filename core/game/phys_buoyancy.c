/*0084f700: buoyant kinematic body's angular restoring force and damping.
 * Original SSE quaternion/cross/normalization and scalar-double acos rational
 * approximation, with each CVTSD2SS rounding retained. No retained x87 math.
 * Frame pushEBP/align16/sub0x70. */
#include "phys_buoyancy.h"
#include "phys_coll_quat.h"
#include "phys_crt.h"

static float abs32(float x){return rx_bf(rx_fb(x)&0x7fffffffu);}
/* SSE scalar-double arithmetic has the same first-NaN/real-indefinite rules
 * as the existing scalar-single helpers. Do not rely on ARM's generated NaN. */
static double sd_nan(double result,double a,double b){
    if(result==result)return result;
    if(a!=a)return rx_bd(rx_db(a)|0x0008000000000000ull);
    if(b!=b)return rx_bd(rx_db(b)|0x0008000000000000ull);
    return rx_bd(0xfff8000000000000ull);
}
static double sd_mul(double a,double b){volatile double rhs=b;return sd_nan(a*rhs,a,b);}
static double sd_sub(double a,double b){return sd_nan(a-b,a,b);}
static double sd_add(double a,double b){return sd_nan(a+b,a,b);}
static double sd_div(double a,double b){return sd_nan(a/b,a,b);}
static float correction_angle(float dot,rx_addr F){
    const double half_pi=rx_bd(0x3ff921fb60000000ull);
    const float magnitude=abs32(dot);
    const int wide=magnitude>=F32(0,0x009ddfc8);
    float x=magnitude;
    if(wide){
        const float square=(float)sd_mul((double)dot,(double)dot);
        const float remainder=(float)sd_sub(1.0,(double)square);
        SF32(F,-4,abs32(remainder));SU32(F,-8,0);rx_set_sp(F-8);
        x=phys_sqrtf(abs32(remainder));rx_set_sp(F);
    }
    const float x2=(float)sd_mul((double)x,(double)x);
    const float x4=(float)sd_mul((double)x2,(double)x2);
    const float den_hi=(float)sd_sub(rx_bd(0x3fe212eb00000000ull),sd_mul((double)x2,rx_bd(0x3fa9903ac0000000ull)));
    const float den_lo=(float)sd_sub(1.0,sd_mul((double)x2,rx_bd(0x3ff716ac60000000ull)));
    const float inv=(float)sd_div(1.0,sd_add(sd_mul((double)den_hi,(double)x4),(double)den_lo));
    const float num_hi=(float)sd_sub(rx_bd(0x3fd98e2fe0000000ull),sd_mul((double)x2,rx_bd(0x3f93d7f000000000ull)));
    const float num_lo=(float)sd_sub(1.0,sd_mul((double)x2,rx_bd(0x3ff46c01a0000000ull)));
    const float num=(float)sd_mul(sd_add(sd_mul((double)num_hi,(double)x4),(double)num_lo),(double)x);
    float value=(float)sd_mul((double)inv,(double)num);
    if(wide)value=(float)sd_sub(half_pi,(double)value);
    if(!(dot>=0.0f))value=phys_sub(-0.0f,value); /*JB includes unordered*/
    return (float)sd_sub(half_pi,(double)value);
}
void phys_buoyancy_angular_correction(rx_addr state,rx_addr body){
    const rx_addr entry=rx_sp(),F=((entry-4)&~15u)-0x70;rx_set_sp(F);
    const rx_v4 strength=v4_splat(F32(0,0x009df494));
    const rx_v4 damping=v4_splat(F32(0,0x009df498));
    const rx_v4 decay=v4_splat(F32(0,0x009df49c));
    const rx_v4 qa=v4_ld(body+0x50),qb=v4_ld(body+0x70),up=v4_xyz(v4_ld(0x00d616a0));
    const rx_v4 a=coll_quat_mul(qa,coll_quat_mul(up,v4_mul(qa,COLL_QCONJ)));
    const rx_v4 b=coll_quat_mul(qb,coll_quat_mul(up,v4_mul(qb,COLL_QCONJ)));
    const float dot=phys_dot3(b,a);
    /*Exact shufps0xd2/0xc9 cross order: b.z*a.y-b.y*a.z,etc. */
    const rx_v4 cross=phys_cross_ba(b,a),axis=phys_normalize3(cross);
    const float angle=correction_angle(dot,F);
    /*Original first multiplies normalizedaxis*strength, then angle*splat. */
    rx_v4 torque=v4_mul(v4_splat(angle),v4_mul(axis,strength));
    torque=v4_xyz(v4_sub(torque,v4_mul(v4_splat(phys_dot3(torque,b)),b)));
    const rx_v4 local=coll_quat_mul(v4_mul(qa,COLL_QCONJ),coll_quat_mul(torque,qa));
    const rx_v4 force=v4_add(v4_sub(local,v4_mul(damping,v4_ld(state+0x10))),v4_ld(state+0x20));
    v4_st(state+0x20,force);
    v4_st(state+0x10,v4_mul(decay,v4_ld(state+0x10)));
    rx_set_sp(entry);
}
RX_FUNCTION(0x0084f700,phys_buoyancy_angular_correction)
