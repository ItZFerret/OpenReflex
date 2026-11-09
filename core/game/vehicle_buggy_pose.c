/* RavenVehicleBuggy visual suspension, original 004a4270.
 * Hand transliteration of the decompile, checked against SSE disassembly.
 * Scratch vectors passed to nodes retain the original aligned frame addresses. */
#include "vehicle_buggy_pose.h"
#include "world_veh.h"
#include "world_veh_util.h"
#include "world_node.h"
#include "world_rider_util.h"
#include "phys_math.h"
#include "phys_crt.h"

static rx_v4 buggy_conjugate(rx_v4 q){return v4_mul(q,v4(-1,-1,-1,1));}
/* Same sign tables as veh_quat_mul. The right addend is reversed at several
 * sites in vf60; preserve that operand preference for NaN inputs as well. */
static rx_v4 buggy_mul(rx_v4 a,rx_v4 b,int reversed){
    rx_v4 a0=v4_mul(v4_mul(v4_lane(a,0),v4_perm(b,3,2,1,0)),v4(1,-1,1,-1));
    rx_v4 a1=v4_mul(v4_mul(v4_lane(a,1),v4_perm(b,2,3,0,1)),v4(1,1,-1,-1));
    rx_v4 a2=v4_mul(v4_mul(v4_perm(b,1,0,3,2),v4_lane(a,2)),v4(-1,1,1,-1));
    rx_v4 a3=v4_mul(v4_lane(a,3),b);
    return v4_add(v4_add(a2,a1),reversed?v4_add(a0,a3):v4_add(a3,a0));
}
static rx_v4 buggy_unit(rx_v4 p){
    rx_v4 s=v4_mul(p,p);
    return veh_normalize_len2(p,phys_add(phys_add(s.v[0],s.v[1]),s.v[2]));
}
/* Vec3_Cross uses b.y*a.z - b.z*a.y, etc.; its callers supply the
 * arguments through ECX/EAX, so the apparently reversed spelling is deliberate. */
static rx_v4 buggy_cross(rx_v4 b,rx_v4 a){
    return v4(phys_sub(phys_mul(a.v[2],b.v[1]),phys_mul(a.v[1],b.v[2])),
              phys_sub(phys_mul(a.v[0],b.v[2]),phys_mul(b.v[0],a.v[2])),
              phys_sub(phys_mul(b.v[0],a.v[1]),phys_mul(a.v[0],b.v[1])),0);
}
/* Scalar Float_Abs + the game's range reduction and degree-ten fit. */
static float buggy_cos(float angle){
    float x=rx_bf(rx_fb(angle)&0x7fffffffu);
    x=phys_sub(F32(0,0x977498),phys_sub(x,phys_mul((float)rx_cvtt(phys_mul(x,F32(0,0x9774a0))),F32(0,0x97749c))));
    float x2=phys_mul(x,x),x4=phys_mul(x2,x2);
    float p=phys_mul(phys_add(phys_mul(x2,F32(0,0x935d50)),F32(0,0x935d48)),x4);
    p=phys_mul(phys_add(p,phys_add(phys_mul(x2,F32(0,0x935d44)),F32(0,0x935d4c))),x4);
    return phys_add(p,phys_add(phys_mul(x2,F32(0,0x935d40)),F32(0,0x935d3c)));
}
static rx_v4 buggy_axis(unsigned axis,float half){
    rx_v4 q=v4_zero();q.v[axis]=buggy_cos(phys_add(half,F32(0,0x977508)));
    q.v[3]=phys_sub(-0.0f,buggy_cos(half));return q;
}
static void buggy_set(rx_addr v,uint32_t off,rx_addr scratch,rx_v4 q){
    v4_st(scratch,q);veh_node_set_world_rotation(U32(v,off),scratch,WCTX(.edi=v));
}
/* All twelve matrix conversions use ordered tests: ties select w, x, z, y.
 * vf60's trace is (row1.y + (row0.x + 1)) + row2.z. */
static rx_v4 buggy_basis(rx_v4 row1,rx_v4 row2){
    rx_v4 row0=buggy_cross(row1,row2);
    float l2=phys_add(phys_add(phys_mul(row0.v[1],row0.v[1]),phys_mul(row0.v[0],row0.v[0])),phys_mul(row0.v[2],row0.v[2]));
    if(l2>0){float root;WORLD_CALL_SP(4,root=phys_sqrtf(l2));row0=v4_mul(v4_splat(phys_div(1,root)),row0);}
    float x2=phys_mul(phys_sub(phys_sub(phys_add(row0.v[0],1),row1.v[1]),row2.v[2]),.25f);
    float y2=phys_mul(phys_sub(phys_add(row1.v[1],phys_sub(1,row0.v[0])),row2.v[2]),.25f);
    float z2=phys_mul(phys_add(phys_sub(phys_sub(1,row0.v[0]),row1.v[1]),row2.v[2]),.25f);
    float w2=phys_mul(phys_add(phys_add(row1.v[1],phys_add(row0.v[0],1)),row2.v[2]),.25f);
    float largest=y2<x2?x2:y2;if(!(largest>z2))largest=z2;if(!(largest>w2))largest=w2;
    float root,scale;rx_v4 q;
    if(largest==w2){
        WORLD_CALL_SP(4,root=phys_sqrtf(w2));scale=phys_div(.25f,root);
        q=v4(phys_mul(phys_sub(row1.v[2],row2.v[1]),scale),phys_mul(phys_sub(row2.v[0],row0.v[2]),scale),phys_mul(phys_sub(row0.v[1],row1.v[0]),scale),root);
    }else{
        float next=z2<y2?y2:z2;if(!(next>x2))next=x2;
        if(next==x2){
            WORLD_CALL_SP(4,root=phys_sqrtf(x2));scale=phys_div(.25f,root);
            q=v4(root,phys_mul(phys_add(row1.v[0],row0.v[1]),scale),phys_mul(phys_add(row0.v[2],row2.v[0]),scale),phys_mul(phys_sub(row1.v[2],row2.v[1]),scale));
        }else if(!(y2>z2)){
            WORLD_CALL_SP(4,root=phys_sqrtf(z2));scale=phys_div(.25f,root);
            q=v4(phys_mul(phys_add(row0.v[2],row2.v[0]),scale),phys_mul(phys_add(row2.v[1],row1.v[2]),scale),root,phys_mul(phys_sub(row0.v[1],row1.v[0]),scale));
        }else{
            WORLD_CALL_SP(4,root=phys_sqrtf(y2));scale=phys_div(.25f,root);
            q=v4(phys_mul(phys_add(row0.v[1],row1.v[0]),scale),root,phys_mul(phys_add(row1.v[2],row2.v[1]),scale),phys_mul(phys_sub(row2.v[0],row0.v[2]),scale));
        }
    }
    return q;
}
static rx_v4 buggy_front_basis(rx_v4 p){
    rx_v4 row2=buggy_unit(p),row1=buggy_unit(buggy_cross(row2,v4_ld(0x00d615c0)));
    return buggy_basis(row1,row2);
}
/* 004a4364..004a7212: front suspension. Each side is solved independently;
 * the x coordinate of its two anchors is fixed at +/-2.13, not the sim wheel x. */
static void buggy_front(rx_addr v,rx_addr S,rx_v4 body_q,rx_v4 base,int right){
    rx_addr sim=U32(v,0x15c0);rx_v4 centre=v4_ld(sim+0x36e0);
    rx_v4 joint_offset=v4_sub(v4_ld(sim+0x41f0),centre);
    rx_v4 wheel=v4_ld(sim+(right?0x400:0x360));
    rx_v4 local=veh_rotate(buggy_conjugate(body_q),v4_sub(wheel,base));
    rx_v4 direction=v4_sub(joint_offset,local);direction.v[0]=F32(0,0x00d61670);
    rx_v4 arm=buggy_front_basis(direction),fixed=buggy_axis(0,.045f);
    buggy_set(v,right?0x137c:0x1364,S+0xf0,buggy_mul(body_q,buggy_mul(arm,fixed,0),1));
    const rx_v4 offsets[]={v4(0,.3f,-1.03f,-1.03f),v4(0,.4f,-1.53f,-1.53f)};
    const rx_v4 anchors[]={v4(0,2.025f,-.68f,-.68f),v4(0,2.35f,-1,-1)};
    const float angles[]={.5325f,.5275f};
    for(unsigned i=0;i<2;i++){
        rx_v4 endpoint=v4_add(veh_rotate(arm,offsets[i]),joint_offset);
        endpoint.v[0]=right?2.13f:-2.13f;
        rx_v4 anchor=v4_sub(anchors[i],centre);anchor.v[0]=endpoint.v[0];
        rx_v4 a=v4_add(veh_rotate(body_q,endpoint),base),b=v4_add(veh_rotate(body_q,anchor),base);
        v4_st(sim+(i?0x8780:0x8740)+(right?0x20:0),a);
        v4_st(sim+(i?0x8790:0x8750)+(right?0x20:0),b);
        rx_v4 d=v4_sub(anchor,endpoint);d.v[0]=F32(0,0x00d61670);
        rx_v4 q=buggy_mul(body_q,buggy_mul(buggy_front_basis(d),buggy_axis(0,angles[i]),0),1);
        uint32_t node=(right?0x1380:0x1368)+i*8;
        buggy_set(v,node,S+0xf0,q);buggy_set(v,node+4,S+0xf0,q);
    }
    buggy_set(v,right?0x1390:0x1378,S+(right?0x280:0x270),v4_ld(sim+(right?0xa40:0x9a0)));
}
/* 004a7217..004ab23a: rear linkage, including the separate shock towers. */
static rx_v4 buggy_rear_basis(rx_v4 direction,int cross_axis,int reverse){
    rx_v4 row1=buggy_unit(direction);
    if(reverse)row1=v4_sub(v4_zero(),row1);
    if(cross_axis)row1=buggy_unit(buggy_cross(v4_ld(0x00d61680),row1));
    return buggy_basis(row1,v4(F32(0,0x009cb4c0),F32(0,0x009cb4c4),F32(0,0x009cb4c8),0));
}
static void buggy_rear(rx_addr v,rx_addr S,rx_v4 body_q,rx_v4 base,int right){
    rx_addr sim=U32(v,0x15c0);rx_v4 centre=v4_ld(sim+0x36e0),mirror=v4(-1,1,1,1);
    rx_v4 a=v4_sub(v4(F32(0,0x009cd9b4),-1.074f,6.362f,6.362f),centre);
    rx_v4 b=v4_sub(v4(-1.98f,-1.574f,6.612f,6.612f),centre);
    rx_v4 c=v4_sub(v4(-.94f,.776f,6.607f,6.607f),centre);
    rx_v4 fixed=v4_sub(v4(-.4f,-.06f,6.362f,6.362f),centre);
    rx_v4 attachment=v4_sub(v4_ld(sim+0x3f90),centre);
    rx_v4 offset=v4(F32(sim,0x42d4),0,0,0);
    if(right){a=v4_mul(mirror,a);b=v4_mul(mirror,b);c=v4_mul(mirror,c);
        fixed=v4_mul(mirror,fixed);attachment=v4_mul(mirror,attachment);offset=v4_mul(mirror,offset);}
    rx_v4 wheel=v4_ld(sim+(right?0x2c0:0x220));
    rx_v4 local=veh_rotate(buggy_conjugate(body_q),v4_sub(wheel,base));
    rx_v4 target=v4_add(local,veh_rotate(v4_ld(sim+0x5620),offset));
    rx_v4 d=v4_sub(target,fixed),s=v4_mul(d,d);
    float length=phys_sqrt(phys_add(phys_add(s.v[0],s.v[2]),s.v[1]));
    float cosine=phys_div(phys_sub(phys_add(phys_mul(length,length),1.20912f),4.281589f),phys_mul(phys_mul(length,2),1.0996f));
    float angle=rider_asin(cosine);
    float half=phys_mul(right?phys_sub(angle,F32(0,0x977508)):phys_sub(F32(0,0x977508),angle),.5f);
    rx_v4 joint=v4_add(v4_mul(veh_rotate(buggy_axis(2,half),v4_sub(fixed,target)),v4_splat(phys_div(1.0996f,length))),target);
    if(!right){
        v4_st(sim+0x87c0,v4_add(veh_rotate(body_q,target),base));
        v4_st(sim+0x87d0,v4_add(veh_rotate(body_q,joint),base));
        v4_st(sim+0x8800,v4_add(veh_rotate(body_q,joint),base));
        v4_st(sim+0x8810,v4_add(veh_rotate(body_q,fixed),base));
    }
    rx_v4 basis=buggy_rear_basis(v4_sub(attachment,target),1,right);
    rx_v4 upper=buggy_mul(buggy_axis(2,right?.06315f:-.06315f),basis,1);
    buggy_set(v,right?0x13b4:0x1394,S+0xf0,buggy_mul(body_q,upper,1));
    rx_v4 lower=buggy_rear_basis(v4_sub(fixed,joint),1,right);
    lower=buggy_mul(buggy_axis(2,right?.0675f:-.0675f),lower,0);
    buggy_set(v,right?0x13b8:0x1398,S+0xf0,buggy_mul(body_q,lower,1));
    rx_v4 strut=buggy_rear_basis(v4_sub(joint,target),0,0);
    strut=buggy_mul(buggy_axis(2,right?-.1468f:.1468f),strut,0);
    buggy_set(v,right?0x13bc:0x139c,S+0xf0,buggy_mul(body_q,strut,1));
    buggy_set(v,right?0x13c0:0x13a0,S+(right?0x260:0x1f0),v4_ld(sim+(right?0x900:0x860)));
    rx_v4 tower=v4_add(veh_rotate(upper,v4_sub(b,a)),a);
    v4_st(sim+(right?0x8820:0x87e0),v4_add(veh_rotate(body_q,tower),base));
    v4_st(sim+(right?0x8830:0x87f0),v4_add(veh_rotate(body_q,c),base));
    rx_v4 delta=v4_sub(c,tower);
    rx_f80 azimuth=crt_ci_atan2(rx_f80_from_f32(delta.v[0]),rx_f80_from_f32(delta.v[1]),CRT_CW_MAIN);
    float tower_angle;
    if(right){
        /* 0041c4b0 rounds atan2 to float before the caller's extended FSUBR. */
        tower_angle=rx_f80_to_f32(rx_f80_sub(rx_f80_from_f32(-.4166f),rx_f80_from_f32(rx_f80_to_f32(azimuth,0)),53,0),0);
    }else tower_angle=phys_sub(.4166f,rx_f80_to_f32(azimuth,0));
    rx_v4 shock=buggy_mul(body_q,buggy_axis(2,phys_mul(tower_angle,.5f)),1);
    for(unsigned i=0;i<4;i++)buggy_set(v,(right?0x13c4:0x13a4)+4*i,S+0xf0,shock);
}

#include "phys_misc.h"
#include "anim.h"
/* Packed polynomial differs from Float_Abs in its max(0-x,x) reduction. */
static float buggy_packed_cos(float x){
    x=rx_maxss(phys_sub(0,x),x);
    float turns=(float)rx_cvtt(phys_mul(F32(0,0x935ad0),x));
    float r=phys_sub(F32(0,0x935af0),phys_sub(x,phys_mul(F32(0,0x935ae0),turns)));
    float r2=phys_mul(r,r),r4=phys_mul(r2,r2);
    float p=phys_mul(phys_add(phys_mul(F32(0,0x935b60),r2),F32(0,0x935b50)),r4);
    p=phys_mul(phys_add(p,phys_add(phys_mul(F32(0,0x935b40),r2),F32(0,0x935b30))),r4);
    return phys_add(p,phys_add(phys_mul(F32(0,0x935b20),r2),F32(0,0x935b10)));
}
static rx_v4 buggy_engine_turn(float angle){
    float half=phys_mul(angle,.5f);
    rx_v4 axis=buggy_unit(v4(0,0,-1,-1));
    rx_v4 q=v4_mul(v4_splat(buggy_packed_cos(phys_add(half,F32(0,0x935b00)))),axis);
    q.v[3]=phys_sub(0,buggy_packed_cos(half));return q;
}
/* 004ab23f..004ab9d5: engine mount lag, acceleration kick and idle wobble.
 * The saved RPM is the lag filter result; the idle envelope uses raw RPM. */
static void buggy_engine(rx_addr v,rx_addr S,float dt){
    rx_addr sim=U32(v,0x15c0);
    rx_v4 q=v4_ld(v+0x1400);
    float rpm=phys_div(F32(sim,0x4d94),F32(0,0x9774d0)),filtered;
    WORLD_CALL_SP(4,filtered=phys_lag_filter_update(v+0x13d8,dt,F32(sim,0x864c)));
    float speed=phys_div(F32(sim,0x100),F32(0,0x92fd88));
    if(speed>=1)speed=1;else if(0>=speed)speed=0;
    /* DIVSS by dt as the original: the static placement's vf60(0) (0045f600) divides by zero, 0 / 0 after a reset gives the
     * default NaN, which passes both clamps (COMISS unordered) and leaves the engine node's rotation NaN until the next timed
     * update rewrites it from +0x1400 (verified on the career series boot: 8 placement calls). */
    float acceleration=phys_div(phys_sub(filtered,F32(v,0x13ec)),dt);
    if(0>acceleration)acceleration=0;
    float kick=phys_div(acceleration,F32(0,0x977564));if(kick>1)kick=1;
    q=buggy_mul(q,buggy_engine_turn(phys_mul(kick,phys_sub(1,speed))),0);
    float phase;
    WORLD_CALL_SP(8,phase=rx_world_float_fmod(phys_add(phys_mul(dt,F32(0,0x9cc37c)),F32(v,0x13f0)),F32(0,0x97749c)));
    SF32(v,0x13f0,phase);
    float envelope=phys_div(phys_sub(F32(sim,0x864c),rpm),F32(0,0x977b50));
    if(envelope>=1)envelope=1;else if(0>=envelope)envelope=0;
    float x=rx_bf(rx_fb(phase)&0x7fffffffu);
    x=phys_sub(F32(0,0x977498),phys_sub(x,phys_mul((float)rx_cvtt(phys_mul(x,F32(0,0x9774a0))),F32(0,0x97749c))));
    float x2=phys_mul(x,x),x4=phys_mul(x2,x2);
    float fit=phys_add(phys_mul(phys_add(phys_mul(x2,F32(0,0x93a71c)),F32(0,0x93a718)),x4),
                       phys_add(phys_mul(x2,F32(0,0x93a714)),F32(0,0x93a710)));
    /* MULSS first builds (1-envelope)*-.005, then multiplies the fit. */
    float amplitude=phys_mul(fit,phys_mul(phys_sub(1,envelope),F32(0,0x977c28)));
    q=buggy_mul(q,buggy_engine_turn(amplitude),0);
    v4_st(S+0x120,q);
    WORLD_CALL_SP(4,rx_node_set_local_rotation(U32(v,0x13d4),S+0x120));
    SF32(v,0x13ec,filtered);
}
void rx_buggy_update_model_pose(rx_addr v,float dt){
    world_frame f=world_frame_enter(0x2b8,2,1);rx_addr S=f.esp;
    SU32(S,0x2bc,U32(0,0x009b45e4)^(S+8));
    veh_model_node_set_matrix(U32(v,0x1328)+0x10,v+0xf0,WCTX(.edi=v));
    rx_addr sim=U32(v,0x15c0);rx_v4 base=v4_ld(sim+0x180),body_q=v4_ld(sim+0x7c0);
    buggy_set(v,0x1360,v+0xe0,v4_ld(v+0xe0));
    veh_node_set_world_position(U32(v,0x1360),v+0xc0,WCTX(.edi=v));
    buggy_front(v,S,body_q,base,0);buggy_front(v,S,body_q,base,1);
    buggy_rear(v,S,body_q,base,0);buggy_rear(v,S,body_q,base,1);
    buggy_engine(v,S,dt);
    phys_security_check_cookie(U32(S,0x2bc)^(S+8));world_frame_leave(&f);
}
RX_FUNCTION(0x004a4270,rx_buggy_update_model_pose)
