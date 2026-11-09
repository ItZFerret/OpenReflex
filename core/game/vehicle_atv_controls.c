/* Original ATV controls: distinct ATV field layout and branch logic. */
#include "vehicle_atv_controls.h"
#include "bike_slot12.h"
#include "bike_math.h"
#include "bike_curve.h"
#include "phys_crt.h"
#include "world_util.h"
#include "crt.h"
#include "free_air.h"
static float atv_control_pass(float x){return rx_f80_to_f32(rx_f80_from_f32(x),0);}
/*00497a70 S12. Frame align16/sub24 +3 pushes; no cookie. */
void rx_atv_control_update(rx_addr s,float dt){
    world_frame frame=world_frame_enter(0x24,3,1);rx_addr F=frame.esp;
    if(U32(s,0x2374)){
        WORLD_CALL_SP(4,bike_clamp_root_spin(s,NULL,NULL));
        world_frame_leave(&frame);return;
    }
    if(U32(s,0x99a8)){
        SF32(s,0x9780,phys_add(dt,F32(s,0x9780)));
        WORLD_CALL_SP(8,rx_atv_air_control(s,atv_control_pass(dt)));
        world_frame_leave(&frame);return;
    }
    WORLD_CALL_SP(4,rx_atv_ground_control(s,atv_control_pass(dt)));
    SU32(s,0x9780,0);SU32(s,0x97a4,0);SU32(s,0x97ac,0);SU32(s,0x97c0,0);SU32(s,0x97d4,0);SU32(s,0x97e8,0);
    WORLD_CALL_SP(8,bike_stick_circle_to_square(atv_control_pass(F32(s,0x5960)),atv_control_pass(F32(s,0x5964)),s+0x978c,s+0x9790));
    WORLD_CALL_SP(8,bike_stick_circle_to_square(atv_control_pass(F32(s,0x5968)),atv_control_pass(F32(s,0x596c)),s+0x9794,s+0x9798));
    float x=F32(s,0x5968),y=F32(s,0x596c);
    SF32(F,0x14,x);SF32(F,0x18,y);
    float angle=rx_f80_to_f32(crt_ci_atan2(rx_f80_from_f32(y),rx_f80_from_f32(x),CRT_CW_MAIN),0);
    SF32(F,0x10,angle);
    angle=rx_f80_to_f32(rx_f80_mul(rx_f80_from_f32(angle),rx_f80_from_f32(F32(0,0x009322c0)),53,0),0);
    SF32(F,0x10,angle);
    float magnitude;WORLD_CALL_SP(4,magnitude=phys_sqrtf(phys_add(phys_mul(y,y),phys_mul(x,x))));
    float result=phys_mul(magnitude,phys_sub(1,phys_sub(-0.0f,bike_cos_poly2(angle))));
    SF32(s,0x979c,result);if(x<0)SF32(s,0x979c,phys_sub(-0.0f,result));
    SU32(s,0x9784,0x3f800000);SU32(s,0x9788,0x3f800000);
    rx_st4(s+0x9800,rx_ld4(s+0x730));
    SU32(s,0x9870,0);SU32(s,0x9874,0);SU32(s,0x9878,0);
    SU32(s,0x98e0,0);SU32(s,0x98e4,0);SU32(s,0x98e8,0);
    rx_st4(s+0x9810,rx_ld4(s+0x9800));
    rx_v4 n=bike_normalize(rx_ld4(s+0x100));
    float ny2=phys_mul(n.v[1],n.v[1]);
    rx_v4 vertical=v4_sqrt(v4_splat(phys_add(phys_add(ny2,ny2),ny2)));
    rx_v4 mask=v4_cmplt(vertical,v4_splat(F32(0,0x009cd9ac)));rx_st4(F+0x20,mask);
    if(bike_all3(mask))rx_st4(s+0x9820,bike_normalize(bike_cross(rx_ld4(0x00d616a0),n)));
    else rx_st4(s+0x9820,bike_rotate(rx_ld4(s+0x9800),rx_ld4(0x00d615c0)));
    SU32(s,0x9954,0);SU32(s,0x9960,600);SU32(s,0x996c,0);SU32(s,0x997c,600);
    world_frame_leave(&frame);
}
RX_FUNCTION(0x00497a70,rx_atv_control_update)

#include "phys_mech.h"
#include "world_rider_util.h"
#include "phys_misc.h"
#define ATV_K958 F32(0,0x009cd958)
#define ATV_K970 F32(0,0x009cd970)
#define ATV_K9AC F32(0,0x009cd9ac)
static float atv_control_angle(float a){return phys_sub(F32(0,0x00977508),(float)(0x1.921fb6p+0-(double)a));}
static float atv_control_lerp(float t,float a,float b){return bike_lerp_clamped(t,atv_control_pass(a),atv_control_pass(b));}
static void atv_control_damp(rx_addr s,rx_v4 q,rx_v4 axis,float factor){
    rx_v4 world=bike_rotate(q,rx_ld4(s+0x740));
    float dot=bike_dot3(axis,world);
    rx_v4 remove=v4_mul(v4_splat(phys_sub(1,factor)),v4_mul(v4_splat(dot),axis));
    rx_st4(s+0x740,bike_rotate_inv(q,v4_xyz(v4_sub(world,remove))));
}
/*00496880: four-wheel grounded angular damping and front/rear tipping limits. */
void rx_atv_ground_control(rx_addr s,float dt){
    world_frame frame=world_frame_enter(0x8c,1,1);rx_addr F=frame.esp;
    SU32(F,0x8c,U32(0,0x009b45e4)^(F+4));
    rx_v4 q=rx_ld4(s+0x730),tilt=bike_quat_mul(q,rx_ld4(0x00ad66b0));
    rx_v4 side=bike_rotate(tilt,rx_ld4(0x00d615c0)),forward=bike_rotate(tilt,rx_ld4(0x00d61680));
    rx_st4(F+0x60,side);rx_st4(F+0x70,forward);rx_st4(F+0x50,q);
    rx_v4 velocity=rx_ld4(s+0x100);
    float speed=phys_sqrt(bike_hsum_021(v4_mul(velocity,velocity)));
    float k=phys_div(speed,30);if(k>1)k=1;
    float align=bike_dot3(bike_normalize(velocity),forward);
    align=phys_max(phys_sub(0,align),align);
    const rx_v4 negative_unit=v4_sub(v4_zero(),rx_ld4(0x00d61650));
    int fast_front=bike_all3(v4_cmplt(v4_splat(F32(s,0x9c0)),negative_unit));
    int fast_rear=bike_all3(v4_cmplt(v4_splat(F32(s,0xa60)),negative_unit));
    if(!(fast_front&&fast_rear)&&
        ((!U32(s,0x94a4)&&!U32(s,0x95f4))||(!U32(s,0x9204)&&!U32(s,0x9354)))){
        uint32_t rear_contact=U32(s,0x94e0);
        if(!rear_contact&&!U32(s,0x9630)){
            float factor=ATV_K958;
            if(align>0)factor=phys_add(phys_mul(phys_sub(1,align),ATV_K958),phys_mul(align,F32(0,0x009d7894)));
            rx_v4 axis=rx_ld4(s+(U32(s,0x9240)?0x9270:0x93c0));
            atv_control_damp(s,q,axis,factor);
        }else{
            float factor=atv_control_lerp(k,1,ATV_K958);
            if(align>0)factor=phys_add(phys_mul(atv_control_lerp(k,1,F32(0,0x00977a18)),align),phys_mul(phys_sub(1,align),factor));
            atv_control_damp(s,q,rx_ld4(s+(rear_contact?0x9510:0x9660)),factor);
        }
    }
    float speed_ratio=phys_div(speed,60);if(speed_ratio>1)speed_ratio=1;
    float timer=phys_mul(phys_sub(F32(s,0x99b4),1),.5f);timer=bike_clamp01(timer);
    float low_speed=atv_control_lerp(speed_ratio,.6000000238418579f,ATV_K970);
    float high_speed=atv_control_lerp(speed_ratio,ATV_K970,1);
    float back=phys_sub(-0.0f,F32(s,0x38));if(back<0)back=0;
    float nose_low=atv_control_lerp(timer,ATV_K958,1);
    float nose_high=atv_control_lerp(timer,ATV_K958,F32(0,0x009cd9bc));
    float blended=atv_control_lerp(timer,atv_control_lerp(back,low_speed,high_speed),1);
    float suspension=phys_div(phys_sub(F32(s,0x4e4c),F32(s,0x96e8)),F32(s,0x4e4c));
    float nose_factor=atv_control_lerp(bike_clamp01(suspension),1,blended);
    SU32(s,0x2eb8,0);
    if((!U32(s,0x94e0)&&!U32(s,0x9630))||U32(s,0x9240)||U32(s,0x9390))SU32(s,0x99b4,0);
    else{
        float angle=atv_control_angle(rider_asin(bike_dot3(forward,rx_ld4(0x00d616a0))));
        rx_v4 w=rx_ld4(s+0x740);int pitching=bike_all3(v4_cmplt(v4_splat(w.v[0]),rx_ld4(0x00d61670)));
        float t=phys_div(bike_fabs(phys_sub(angle,.3490658104419708f)),.2617993652820587f);if(t>1)t=1;
        float factor=1;
        if(pitching){
            factor=atv_control_lerp(t,nose_low,nose_factor);
            if(angle>=.5235987305641174f){float edge=phys_div(phys_sub(.6981316208839417f,angle),.1745329052209854f);if(edge<0)edge=0;
                factor=atv_control_lerp(edge,nose_high,factor);}
        }
        float x=phys_mul(factor,w.v[0]);if(pitching)x=phys_max(x,-1.5f);
        rx_st4(s+0x740,v4(x,w.v[1],w.v[2],w.v[3]));
        SF32(s,0x99b4,phys_add(F32(s,0x99b4),dt));if(F32(s,0x99b4)>.25f)SU32(s,0x2eb8,1);
    }
    float rear_timer=bike_clamp01(phys_div(phys_sub(F32(s,0x99b8),1),3));
    float rear_low=atv_control_lerp(rear_timer,ATV_K9AC,1),rear_high=atv_control_lerp(rear_timer,.75f,1);
    SU32(s,0x2eb4,0);
    if((!U32(s,0x9240)&&!U32(s,0x9390))||U32(s,0x94e0)||U32(s,0x9630))SU32(s,0x99b8,0);
    else{
        float angle=phys_sub(-0.0f,atv_control_angle(rider_asin(bike_dot3(forward,rx_ld4(0x00d616a0)))));
        rx_v4 w=rx_ld4(s+0x740);int pitching=bike_all3(v4_cmplt(rx_ld4(0x00d61670),v4_splat(w.v[0])));
        float t=phys_div(bike_fabs(phys_sub(angle,.6108651757240295f)),.43633225560188293f);if(t>1)t=1;
        float factor=1;
        if(pitching){factor=atv_control_lerp(t,rear_low,1);
            if(angle>=.7417648434638977f){float edge=phys_div(phys_sub(.7853980660438538f,angle),.04363322630524635f);if(edge<0)edge=0;
                factor=atv_control_lerp(edge,rear_high,factor);}}
        rx_st4(s+0x740,v4(phys_mul(factor,w.v[0]),w.v[1],w.v[2],w.v[3]));
        SF32(s,0x99b8,phys_add(dt,F32(s,0x99b8)));if(F32(s,0x99b8)>.25f)SU32(s,0x2eb4,1);
    }
    rx_v4 angular=rx_ld4(s+0x740);float contact_align=0;int damp=0;
    if(U32(s,0x9240)&&U32(s,0x94e0)&&!U32(s,0x9390)&&!U32(s,0x9630)){
        contact_align=bike_dot3(rx_ld4(s+0x9280),side);damp=bike_all3(v4_cmplt(rx_ld4(0x00d61670),v4_splat(angular.v[2])));
    }else if(U32(s,0x9390)&&U32(s,0x9630)&&!U32(s,0x9240)&&!U32(s,0x94e0)){
        contact_align=bike_dot3(rx_ld4(s+0x93d0),side);damp=bike_all3(v4_cmplt(v4_splat(angular.v[2]),rx_ld4(0x00d61670)));
    }
    if(damp){float factor=1;if(contact_align>0)factor=phys_add(phys_mul(phys_sub(1,contact_align),.5f),phys_mul(contact_align,ATV_K9AC));
        angular.v[2]=phys_mul(factor,angular.v[2]);angular.v[3]=angular.v[2];rx_st4(F+0x30,angular);
        WORLD_CALL_SP(4,phys_mech_set_node_angular_velocity(s+0xd0,F+0x30,0));}
    phys_security_check_cookie(U32(F,0x8c)^(F+4));world_frame_leave(&frame);
}
RX_FUNCTION(0x00496880,rx_atv_ground_control)

#include "phys_math.h"
#include "phys_quat.h"
static int atv_held(float x){return bike_fabs(x)>F32(0,0x009d3bfc);}
static float atv_stick_rate(float value,float previous,float dt){return bike_fabs(value)>bike_fabs(previous)?phys_div(phys_sub(value,previous),dt):0;}
static float atv_shaped_rate(rx_addr curve,float rate){float t=phys_div(bike_fabs(rate),100);if(t>1)t=1;return phys_mul(bike_param_curve(curve,t),rate);}
/* the four normalizations of the settle / hold paths (004960bb.., 0049606a.., 004961a3.., 004961f4..) all sum the squares in pairs:
 * shufps 0xee, addps, shufps 0x40, shufps 0xf0, addps: (p3 + p1) + (p2 + p0) (w-parity4w: the settle path summed ((p3 + p1) + p2) + p0,
 * 1 ulp off in about one call in 190 of an airborne ATV: the paired NAT_Bedford ride's AI 10 at event frame 1622) */
static rx_v4 atv_air_quat_normalize(rx_v4 q){
    rx_v4 p=v4_mul(q,q);
    float length2=phys_add(p.v[3],p.v[1]);
    length2=phys_add(length2,phys_add(p.v[2],p.v[0]));
    rx_v4 l2=v4_splat(length2),len=v4_sqrt(l2),finite=v4_cmpneq(l2,rx_ld4(0x0092dc20));
    rx_v4 n=v4_and(v4_and(v4_div(q,len),v4_cmpneq(len,v4_zero())),finite);
    return v4_or(n,v4_andn(finite,rx_ld4(0x0092dc30)));
}
/*0049597a..00495bcd: alternate trace sums of the ATV flight basis. */
static inline rx_v4 atv_air_matrix_to_quat(const float m[3][3]) {
    const float tx = phys_mul(phys_sub(phys_sub(phys_add(m[0][0], 1.0f), m[1][1]), m[2][2]), 0.25f);
    const float ty = phys_mul(phys_sub(phys_add(m[1][1],phys_sub(1.0f,m[0][0])), m[2][2]), 0.25f);
    const float tw = phys_mul(phys_add(phys_add(m[1][1], phys_add(m[0][0], 1.0f)), m[2][2]), 0.25f);
    const float tz = phys_mul(phys_add(phys_sub(phys_sub(1.0f, m[0][0]), m[1][1]), m[2][2]), 0.25f);
    float mx = tx > ty ? tx : ty;
    if (!(mx > tz)) mx = tz;
    if (!(mx > tw)) mx = tw;
    rx_v4 q;
    if (mx == tw) {
        float r = phys_sqrt(tw), k = phys_div(0.25f, r);
        q = v4(phys_mul(phys_sub(m[1][2], m[2][1]), k), phys_mul(phys_sub(m[2][0], m[0][2]), k), phys_mul(phys_sub(m[0][1], m[1][0]), k), r);
    } else {
        float my = ty > tz ? ty : tz;
        if (!(my > tx)) my = tx;
        if (my == tx) {
            float r = phys_sqrt(tx), k = phys_div(0.25f, r);
            q = v4(r, phys_mul(phys_add(m[1][0], m[0][1]), k), phys_mul(phys_add(m[2][0], m[0][2]), k), phys_mul(phys_sub(m[1][2], m[2][1]), k));
        } else if (ty > tz) {
            float r = phys_sqrt(ty), k = phys_div(0.25f, r);
            q = v4(phys_mul(phys_add(m[1][0], m[0][1]), k), r, phys_mul(phys_add(m[2][1], m[1][2]), k), phys_mul(phys_sub(m[2][0], m[0][2]), k));
        } else {
            float r = phys_sqrt(tz), k = phys_div(0.25f, r);
            q = v4(phys_mul(phys_add(m[2][0], m[0][2]), k), phys_mul(phys_add(m[2][1], m[1][2]), k), r, phys_mul(phys_sub(m[0][1], m[1][0]), k));
        }
    }
    return q;
}
/*00494910: every airborne control phase, original ATV-specific coefficients. */
void rx_atv_air_control(rx_addr s,float dt){
    world_frame frame=world_frame_enter(0x124,3,1);rx_addr F=frame.esp;
    SU32(F,0x12c,U32(0,0x009b45e4)^(F+0xc));
    const float deg=F32(0,0x00977678);rx_v4 up=rx_ld4(0x00d616a0),forward=rx_ld4(0x00d61680);
    rx_st4(s+0x9a70,rx_ld4(s+0x780));
    rx_v4 push_axis=bike_normalize(bike_cross(up,rx_ld4(s+0x100)));
    float lean=phys_mul(F32(s,0x50d4),F32(s,0x5960));
    rx_v4 push=v4_mul(v4_mul(v4_mul(push_axis,v4_splat(lean)),rx_ld4(s+0x120)),v4_splat(dt));
    for(int32_t i=0;i<I32(s,0xec);i++){rx_addr body=s+0x100+0xa0u*(uint32_t)i;rx_st4(body,v4_add(push,rx_ld4(body)));}
    const float a=F32(s,0x9784),b=F32(s,0x9788);
    const float yawA=phys_add(phys_mul(a,-1),1),rollA=phys_sub(phys_mul(a,1.2f),1.2f);
    const float zA=phys_add(phys_mul(a,0),0),pitchA=phys_add(phys_mul(a,-.75f),.75f);
    const float rollB=phys_add(phys_mul(b,-1.2f),0),mixB=phys_sub(-.9f,phys_mul(b,.30000007152557373f));
    const float yawB=phys_add(phys_mul(b,.06000000238418579f),.5400000214576721f);
    const float pitchRateB=phys_add(phys_mul(b,1.35f),0),pitchB=phys_add(phys_mul(b,.7999999523162842f),.5f);
    const float rollRateA=phys_add(phys_mul(a,-.15f),0),yawRateA=phys_add(phys_mul(a,.5f),0);
    const float yawMagnitudeRate=phys_add(phys_mul(b,.2f),0),zB=phys_add(phys_mul(b,0),0);
    rx_v4 q=rx_ld4(s+0x730),angular=rx_ld4(s+0x740);
    SU32(s,0x9950,U32(s,0x9958)!=0&&!U32(s,0x9970));
    SU32(s,0x9968,!U32(s,0x9958)&&U32(s,0x9970)!=0);
    if(U32(s,0x9954)&&I32(s,0x9960)>0)SU32(s,0x9960,U32(s,0x9960)-1);
    if(U32(s,0x996c)&&I32(s,0x997c)>0)SU32(s,0x997c,U32(s,0x997c)-1);
    if(U32(s,0x9950)&&I32(s,0x9960)>0){
        float strength=F32(s,0x995c);rx_v4 axis=bike_rotate_inv(q,rx_ld4(s+0x9820));
        SU32(s,0x9954,1);
        angular=v4_add(v4_mul(axis,v4_splat(phys_mul(phys_mul(strength,-5),deg))),angular);
        float decay=phys_mul(dt,F32(s,0x596c)>ATV_K970?F32(0,0x009cd960):.1f);
        float value=phys_sub(strength,decay);SF32(s,0x995c,value>.3f?value:.3f);
    }else{SU32(s,0x9958,0);SU32(s,0x9950,0);SU32(s,0x9954,0);}
    if(U32(s,0x9968)&&I32(s,0x997c)>0){
        float x=F32(s,0x9974),y=F32(s,0x9978),direction=F32(s,0x9988);
        float ticks=(float)I32(s,0x997c),yaw,roll,pitch;
        if(ticks>400){yaw=phys_mul(phys_mul(x,-.5f),direction);roll=phys_mul(phys_mul(x,-.25f),direction);pitch=phys_mul(y,-8);}
        else if(ticks>350){yaw=phys_mul(phys_mul(x,-.5f),direction);roll=phys_mul(phys_mul(x,-.25f),direction);pitch=phys_mul(y,0);}
        else{yaw=phys_mul(phys_mul(x,F32(0,0x009cd9b0)),direction);roll=phys_mul(phys_mul(x,-5),direction);pitch=phys_mul(y,2);}
        rx_v4 axis=bike_rotate_inv(q,rx_ld4(s+0x9820));
        rx_v4 delta=v4_mul(axis,v4_splat(phys_mul(pitch,deg)));
        delta=v4_add(delta,v4_mul(v4_splat(phys_mul(yaw,deg)),forward));
        delta=v4_add(delta,v4_mul(v4_splat(phys_mul(roll,deg)),up));angular=v4_add(delta,angular);
        SU32(s,0x996c,1);float decay=phys_mul(dt,.1f),xx=phys_sub(x,decay),yy=phys_sub(y,decay);
        SF32(s,0x9974,xx>.15f?xx:.15f);SF32(s,0x9978,yy>.15f?yy:.15f);
    }else{SU32(s,0x9970,0);SU32(s,0x9968,0);SU32(s,0x996c,0);}
    int pitch_trick=U32(s,0x9870)||U32(s,0x9874)||U32(s,0x9878);
    int roll_trick=U32(s,0x98e0)||U32(s,0x98e4)||U32(s,0x98e8);
    const uint32_t free_air=rx_fa_for(s);   /* PORT ADDITION (free_air.h): FREER AIR for a local player's ATV; 0 = the original */
    int hold=!(free_air&RX_FREE_AIR_NO_HOLD)&&((pitch_trick&&F32(s,0x5960)<-.9f)||(roll_trick&&F32(s,0x5960)>ATV_K970));
    int any=atv_held(F32(s,0x5960))||atv_held(F32(s,0x5964))||atv_held(F32(s,0x5968))||atv_held(F32(s,0x596c));
    int active=any&&!U32(s,0x3c)&&!hold&&!U32(s,0x9954)&&!U32(s,0x996c);
    float fadeA=phys_sub(a,phys_mul(dt,(atv_held(F32(s,0x5960))||atv_held(F32(s,0x5964)))?2:F32(0,0x009d789c)));
    float fadeB=phys_sub(b,phys_mul(dt,(atv_held(F32(s,0x5968))||atv_held(F32(s,0x596c)))?1.5f:F32(0,0x009d789c)));
    SF32(s,0x9784,fadeA>0?fadeA:0);SF32(s,0x9788,fadeB>0?fadeB:0);
    if(U32(s,0x9838)){if(U32(s,0x983c)){SU32(s,0x9788,0x3f800000);SU32(s,0x9784,0x3f800000);}if(active)SU32(s,0x983c,0);}
    WORLD_CALL_SP(8,bike_stick_circle_to_square(atv_control_pass(F32(s,0x5960)),atv_control_pass(F32(s,0x5964)),F+0x30,F+0x24));
    WORLD_CALL_SP(8,bike_stick_circle_to_square(atv_control_pass(F32(s,0x5968)),atv_control_pass(F32(s,0x596c)),F+0x34,F+0x18));
    float A1=F32(F,0x30),B1=F32(F,0x24),A2=F32(F,0x34),B2=F32(F,0x18);
    float rateA1=0,rateB1=0,rateA2=0,rateB2=0;
    if(U32(s,0x97a8)){rateA1=atv_stick_rate(A1,F32(s,0x978c),dt);rateB1=atv_stick_rate(B1,F32(s,0x9790),dt);
        rateA2=atv_stick_rate(A2,F32(s,0x9794),dt);rateB2=atv_stick_rate(B2,F32(s,0x9798),dt);}
    SF32(s,0x9ac0,rateB1);SF32(s,0x9abc,rateA1);SF32(s,0x9ac4,rateA2);SF32(s,0x9ac8,rateB2);
    rateA1=atv_shaped_rate(s+0x54a4,rateA1);rateB1=atv_shaped_rate(s+0x5544,rateB1);
    rateA2=atv_shaped_rate(s+0x55e4,rateA2);rateB2=atv_shaped_rate(s+0x5684,rateB2);
    SF32(s,0x978c,A1);SF32(s,0x9790,B1);SF32(s,0x9794,A2);SF32(s,0x9798,B2);
    float x=F32(s,0x5968),y=F32(s,0x596c);
    float angle=rx_f80_to_f32(crt_ci_atan2(rx_f80_from_f32(y),rx_f80_from_f32(x),CRT_CW_MAIN),0);
    float magnitude;WORLD_CALL_SP(4,magnitude=phys_sqrtf(phys_add(phys_mul(y,y),phys_mul(x,x))));
    float m=phys_mul(magnitude,phys_sub(1,phys_sub(-0.0f,bike_cos_poly2(phys_mul(angle,4)))));if(x<0)m=phys_sub(-0.0f,m);
    float magnitude_rate=atv_stick_rate(m,F32(s,0x979c),dt);SF32(s,0x979c,m);SF32(s,0x97a0,magnitude_rate);
    rx_v4 n=bike_normalize(rx_ld4(s+0x100));float ny2=phys_mul(n.v[1],n.v[1]);
    if(bike_all3(v4_cmplt(v4_sqrt(v4_splat(phys_add(phys_add(ny2,ny2),ny2))),v4_splat(F32(0,0x009d7894))))){
        rx_v4 side=bike_normalize(bike_cross(up,n)),u=bike_normalize(bike_cross(n,side));
        float first[3];bike_vec3_cross(first,u.v,n.v);
        float l2=phys_add(phys_add(phys_mul(first[1],first[1]),phys_mul(first[0],first[0])),phys_mul(first[2],first[2]));
        if(l2>0){float root;WORLD_CALL_SP(4,root=phys_sqrtf(l2));float inv=phys_div(1,root);for(unsigned i=0;i<3;i++)first[i]=phys_mul(inv,first[i]);}
        float rows[3][3]={{first[0],first[1],first[2]},{u.v[0],u.v[1],u.v[2]},{n.v[0],n.v[1],n.v[2]}};
        rx_st4(s+0x9810,atv_air_matrix_to_quat(rows));
    }
    if(active){
        SF32(s,0x97a4,phys_add(F32(s,0x97a4),dt));
        float pitch1=phys_add(phys_mul(B1,pitchA),phys_mul(A1,zA));pitch1=phys_add(pitch1,phys_mul(rateA1,zA));pitch1=phys_add(pitch1,phys_mul(rateB1,zA));
        float pitch2=phys_add(phys_mul(B2,pitchB),phys_mul(A2,zB));pitch2=phys_add(pitch2,phys_mul(rateA2,zB));pitch2=phys_add(pitch2,phys_mul(rateB2,pitchRateB));
        if(pitch1>0)pitch1=phys_mul(pitch1,F32(0,0x009cc8a8));if(pitch2>0)pitch2=phys_mul(pitch2,F32(0,0x009cc8a8));
        float yaw=phys_add(phys_mul(B2,zB),phys_mul(A2,yawB));yaw=phys_add(yaw,phys_mul(A1,yawA));yaw=phys_add(yaw,phys_mul(B1,zA));
        yaw=phys_add(yaw,phys_mul(rateA1,yawRateA));yaw=phys_add(yaw,phys_mul(rateB1,zA));yaw=phys_add(yaw,phys_mul(magnitude_rate,yawMagnitudeRate));yaw=phys_add(yaw,phys_mul(m,zB));
        float roll=phys_add(phys_mul(B2,zB),phys_mul(A2,mixB));roll=phys_add(roll,phys_mul(A1,rollA));roll=phys_add(roll,phys_mul(B1,zA));
        roll=phys_add(roll,phys_mul(rateA2,rollB));roll=phys_add(roll,phys_mul(rateB2,zB));roll=phys_add(roll,phys_mul(rateA1,rollRateA));roll=phys_add(roll,phys_mul(rateB1,zA));
        float pitch=bike_fabs(pitch2)<bike_fabs(pitch1)?pitch1:pitch2;
        if(free_air&RX_FA_LOOSE){   /* AIR LOOSENESS (free_air.h): quicker lag filters */
            roll=rx_fa_lag_update(s+0x97ac,dt,atv_control_pass(roll));pitch=rx_fa_lag_update(s+0x97c0,dt,atv_control_pass(pitch));yaw=rx_fa_lag_update(s+0x97d4,dt,atv_control_pass(yaw));
        }else{
        WORLD_CALL_SP(4,roll=phys_lag_filter_update(s+0x97ac,dt,atv_control_pass(roll)));
        WORLD_CALL_SP(4,pitch=phys_lag_filter_update(s+0x97c0,dt,atv_control_pass(pitch)));
        WORLD_CALL_SP(4,yaw=phys_lag_filter_update(s+0x97d4,dt,atv_control_pass(yaw)));
        }
        rx_v4 axis=bike_rotate_inv(q,rx_ld4(s+0x9820)),delta=v4_mul(axis,v4_splat(phys_mul(pitch,deg)));
        delta=v4_add(delta,v4_mul(v4_splat(phys_mul(roll,deg)),forward));delta=v4_add(delta,v4_mul(v4_splat(phys_mul(yaw,deg)),up));
        if(free_air&RX_FA_LOOSE)delta=v4_mul(delta,v4_splat(rx_fa_loose_gain()));   /* AIR LOOSENESS: stick authority */
        angular=v4_add(delta,angular);
    }else{
        int settle=F32(s,0x97a4)>.25f&&!U32(s,0x996c)&&!U32(s,0x9954)&&!(free_air&RX_FREE_AIR_NO_SETTLE);   /* FREER AIR */
        if(settle||hold){
            rx_v4 relative=atv_air_quat_normalize(bike_quat_mul(bike_quat_conj(q),rx_ld4(s+0x9810))),turn;
            float blend=hold?F32(0,0x00977418):F32(0,0x0097736c);
            if(hold?(free_air&RX_FA_PART_HOLD):(free_air&RX_FA_PART_SETTLE))blend=hold?rx_fa_hold_rate(blend):rx_fa_settle_rate(blend);   /* AUTO LEVEL / COUNTER-STEER SNAP below 100 % (free_air.h) */
            WORLD_CALL_SP(0,turn=phys_quat_slerp(rx_ld4(0x00d61640),relative,v4_splat(blend)));
            turn=atv_air_quat_normalize(turn);rx_st4(F+0x100,turn);WORLD_CALL_SP(0,phys_mech_rotate(s+0xd0,F+0x100));
        }
    }
    rx_v4 length=v4_sqrt(v4_splat(bike_hsum_021(v4_mul(angular,angular))));
    if(bike_all3(v4_cmplt(v4_splat(10),length)))angular=v4_div(v4_mul(angular,v4_splat(10)),length);
    rx_v4 scale=rx_ld4(s+0x5130);
    if(free_air&RX_FA_LOOSE){   /* AIR LOOSENESS: the damping eased (free_air.h) */
        rx_v4 takeoff=rx_ld4(s+0x5190);
        const int coast=!active&&F32(s,0x97a4)>0;   /* the coast after stick use only: a hands-off jump keeps the original's damping */
        for(int i=0;i<3;i++){if(coast)scale.v[i]=rx_fa_loose_damping(scale.v[i]);takeoff.v[i]=rx_fa_loose_takeoff(takeoff.v[i]);}
        if(U32(s,0x99ac))scale=v4_mul(takeoff,scale);
    }else if(U32(s,0x99ac))scale=v4_mul(rx_ld4(s+0x5190),scale);
    rx_st4(s+0x740,v4_mul(v4_perm(scale,1,2,0,3),angular));
    phys_security_check_cookie(U32(F,0x12c)^(F+0xc));world_frame_leave(&frame);
}
RX_FUNCTION(0x00494910,rx_atv_air_control)
