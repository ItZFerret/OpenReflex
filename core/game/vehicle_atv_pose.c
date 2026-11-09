/* ATV visual/composite placement. Original RavenVehicleATV V62 0047cfa0;
 * original stack addresses retained because child updates take scratch pointers. */
#include "vehicle_atv_pose.h"
#include "world_veh.h"
#include "world_veh_util.h"
#include "world.h"
#include "world_extern.h"
#include "phys_math.h"
#include "phys_coll_shape.h"
#include "phys_crt.h"
#define ATV_XYZ 0x0092db80u
static rx_v4 atv_axis_unit(rx_addr address){
    rx_v4 a=v4_ld(address),p=v4_mul(a,a);
    float length2=phys_add(phys_add(p.v[0],p.v[1]),p.v[2]);
    rx_v4 l2=v4_splat(length2),length=v4_sqrt(l2),finite=v4_cmpneq(l2,v4_ld(WORLD_INF4));
    rx_v4 q=v4_and(v4_and(v4_div(a,length),v4_cmpneq(length,v4_zero())),finite);
    q=v4_or(q,v4_andn(finite,v4_ld(WORLD_NAN4)));
    return v4_or(v4_and(q,v4_ld(ATV_XYZ)),v4_ld(0x0092db60));
}
void rx_atv_place_composite(rx_addr v){
    world_frame f=world_frame_enter(0x154,3,1);rx_addr S=f.esp;
    SU32(S,0x15c,U32(0,0x009b45e4)^(S+0xc));
    WORLD_VCALL(v,0x94,S+0xa0);WORLD_VCALL(v,0x98,S+0xb0);
    v4_st(S+0x70,v4(F32(v,0xc0),F32(v,0xc4),F32(v,0xc8),F32(v,0xc8)));
    v4_st(S+0x30,v4_ld(v+0xe0));
    WORLD_CALL_SP(0x14,rx_world_dynobj_set_pose(v+0x178,S+0x70,S+0x30,S+0xa0,S+0xb0));
    v4_st(S+0x40,atv_axis_unit(0x00d61680));
    WORLD_CALL_SP(4,phys_vec4_sincos(v4_splat(F32(0,0x009776e0)),S+0x20,S+0x60));
    rx_v4 spin=v4_mul(v4_or(v4_and(v4_ld(S+0x20),v4_ld(ATV_XYZ)),
        v4_and(v4_ld(S+0x60),v4_ld(0x0092db90))),v4_ld(S+0x40));
    v4_st(S+0x60,spin);
    rx_addr comp=U32(world_handle_object(U32(v,0x17c),WCTX(.ebx=v)),0x2c);
    WORLD_CALL_SP(4,rx_world_node_world_pose(U32(v,0x1550),S+0x80,S+0x50));
    int32_t child=(int32_t)WORLD_VCALL(comp,0x20,0x0093bc94);
    if(child>=0){
        rx_v4 q=v4_ld(S+0x50);
        rx_v4 offset=v4_and(v4(F32(0,0x0092e12c),F32(0,0x00931f24),F32(0,0x009cd9dc),F32(0,0x009cd9dc)),v4_ld(ATV_XYZ));
        v4_st(S+0x90,v4_add(veh_rotate(q,offset),v4_ld(S+0x80)));
        v4_st(S+0x20,q);
        WORLD_CALL_SP(8,phys_composite_set_child_position(comp,S+0x90,S+0x70,(uint32_t)child,S+0x30));
        WORLD_CALL_SP(4,phys_composite_set_child_rotation(comp,S+0x20,(uint32_t)child,S+0x30));
    }
    const uint32_t nodes[]={0x1538,0x1548,0x157c,0x1580};
    for(unsigned i=0;i<4;i++)SU32(S,0x20+4*i,U32(v,nodes[i]));
    SU32(S,0x1c,0);SU32(S,0x18,v+0x1750);
    v4_st(S+0x40,v4_perm(spin,3,2,1,0));
    for(unsigned i=0;i<4;i++){
        WORLD_CALL_SP(4,rx_world_node_world_pose(U32(S,0x20+4*i),S+0x80,S+0x50));
        uint32_t k=U32(U32(S,0x18),0);
        v4_st(S+0x50,veh_quat_mul(v4_ld(S+0x50),v4_ld(S+0x60)));
        WORLD_CALL_SP(8,phys_composite_set_child_position(comp,S+0x80,S+0x70,k,S+0x30));
        WORLD_CALL_SP(4,phys_composite_set_child_rotation(comp,S+0x50,k,S+0x30));
        world_bounds_check(k,(U32(comp,0x30)-U32(comp,0x2c))>>2);
        WORLD_VCALL(U32(U32(comp,0x2c),4*k),8,S+0x50,S+0x80,S+0x140,S+0x90);
        SU32(S,0x18,U32(S,0x18)+4);SU32(S,0x1c,i+1);
    }
    float positive=F32(0,0x0092e330),negative=F32(0,0x009d7b30),front=F32(0,0x0092fe48),rear=F32(0,0x009cc380);
    for(unsigned i=0;i<8;i++){
        static const unsigned ix[]={0,0,0,1,0,1,1,1},iy[]={0,0,1,0,1,0,1,1},iz[]={0,1,0,0,1,1,0,1};
        float z=iz[i]?rear:front;
        v4_st(S+0xc0+16*i,v4(ix[i]?negative:positive,iy[i]?negative:positive,z,z));
    }
    rx_v4 high=v4_splat(F32(0,0x00977550)),low=v4_splat(F32(0,0x00977494));
    for(unsigned i=0;i<8;i++){
        rx_v4 p=v4_add(veh_rotate(v4_ld(S+0x30),v4_and(v4_ld(S+0xc0+16*i),v4_ld(ATV_XYZ))),v4_ld(S+0x70));
        high=v4_max(high,p);low=v4_min(low,p);
    }
    v4_st(S+0x20,high);v4_st(S+0x90,low);
    rx_addr body=U32(v,0x17c);
    world_keyed_refresh(U32(body,8),U32(body,4)+0x1954,WCTX(.ebx=v));
    WORLD_CALL_SP(8,rx_world_vehicle_collect_nearby_statics(U32(v,0x1700)+0x1300,S+0x20,U32(U32(v,0x188),0x20134),S+0x90));
    rx_addr input=WORLD_VCALL(U32(0,0x00a95844),8);
    if(WORLD_VCALL(input,0x7c,0x33,0)){
        SU32(0,0x009cc8ac,U32(0,0x009cc8ac)==0);
        WORLD_VCALL(v,0x80,U32(0,0x009cc8ac));
    }
    phys_security_check_cookie(U32(S,0x15c)^(S+0xc));world_frame_leave(&f);
}
RX_FUNCTION(0x0047cfa0,rx_atv_place_composite)

/* Scalar cosine fit inlined throughout 0047e850 (0047ead3 onward).
 * This is the original range reduction/polynomial, not host trigonometry. */
static float atv_cos_fit(float angle){
    float x=rx_bf(rx_fb(angle)&0x7fffffffu);
    x=phys_sub(x,phys_mul((float)rx_cvtt(phys_mul(x,F32(0,0x9774a0))),F32(0,0x97749c)));
    float u=phys_sub(F32(0,0x977498),x),u2=phys_mul(u,u),u4=phys_mul(u2,u2);
    float a=phys_add(phys_mul(u2,F32(0,0x935d50)),F32(0,0x935d48));
    a=phys_mul(a,u4);
    a=phys_add(a,phys_add(phys_mul(u2,F32(0,0x935d44)),F32(0,0x935d4c)));
    a=phys_mul(a,u4);
    return phys_add(a,phys_add(phys_mul(u2,F32(0,0x935d40)),F32(0,0x935d3c)));
}
static rx_v4 atv_axis_half(unsigned axis,float half){
    rx_v4 q=v4_zero();q.v[axis]=atv_cos_fit(phys_add(half,F32(0,0x977508)));
    q.v[3]=phys_sub(-0.0f,atv_cos_fit(half));return q;
}
static rx_v4 atv_conjugate(rx_v4 q){return v4_mul(q,v4(-1,-1,-1,1));}
static rx_v4 atv_normalize3(rx_v4 a){
    rx_v4 p=v4_mul(a,a);
    return veh_normalize_len2(a,phys_add(phys_add(p.v[0],p.v[1]),p.v[2]));
}
/* Vec3_Cross preserves the original three scalar stores and y*x operand order. */
static rx_v4 atv_cross3(rx_v4 b,rx_v4 a){
    return v4(phys_sub(phys_mul(a.v[2],b.v[1]),phys_mul(a.v[1],b.v[2])),
              phys_sub(phys_mul(a.v[0],b.v[2]),phys_mul(b.v[0],a.v[2])),
              phys_sub(phys_mul(b.v[0],a.v[1]),phys_mul(a.v[0],b.v[1])),0);
}
/* The scalar normalized first basis row and four-case matrix-to-quaternion
 * conversion recur eleven times in V60. Equality ties select w, then x, then z.
 * right_sum retains the alternate front-right/rear trace addition ordering. */
static rx_v4 atv_basis_rotation(rx_v4 row1,rx_v4 row2,int right_sum){
    rx_v4 row0=atv_cross3(row1,row2);
    float l2=phys_add(phys_add(phys_mul(row0.v[1],row0.v[1]),phys_mul(row0.v[0],row0.v[0])),phys_mul(row0.v[2],row0.v[2]));
    if(l2>0){float root;WORLD_CALL_SP(4,root=phys_sqrtf(l2));row0=v4_mul(v4_splat(phys_div(1,root)),row0);}
    float x2=phys_mul(phys_sub(phys_sub(phys_add(row0.v[0],1),row1.v[1]),row2.v[2]),.25f);
    float y2=phys_mul(phys_sub(right_sum?phys_add(row1.v[1],phys_sub(1,row0.v[0])):phys_add(phys_sub(1,row0.v[0]),row1.v[1]),row2.v[2]),.25f);
    float z2=phys_mul(phys_add(phys_sub(phys_sub(1,row0.v[0]),row1.v[1]),row2.v[2]),.25f);
    float w2=phys_mul(phys_add(right_sum?phys_add(row1.v[1],phys_add(row0.v[0],1)):phys_add(phys_add(row0.v[0],1),row1.v[1]),row2.v[2]),.25f);
    float largest=y2<x2?x2:y2;if(!(largest>z2))largest=z2;if(!(largest>w2))largest=w2;
    rx_v4 q;float root,scale;
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
            q=v4(phys_mul(phys_add(row1.v[0],row0.v[1]),scale),root,phys_mul(phys_add(row2.v[1],row1.v[2]),scale),phys_mul(phys_sub(row2.v[0],row0.v[2]),scale));
        }
    }
    return q;
}
static void atv_set_rotation(rx_addr vehicle,uint32_t node_offset,rx_addr scratch,rx_v4 q){
    v4_st(scratch,q);veh_node_set_world_rotation(U32(vehicle,node_offset),scratch,WCTX(.ebx=vehicle));
}

#include "world_rider_util.h"
#include "world_node.h"
static rx_v4 atv_front_member(rx_v4 direction,int cross_axis,int right_sum,int reverse){
    rx_v4 row1=atv_normalize3(direction);
    if(reverse)row1=v4_sub(v4_zero(),row1);
    if(cross_axis)row1=atv_normalize3(atv_cross3(v4_ld(0x00d61680),row1));
    rx_v4 row2=v4(F32(0,0x009cb4c0),F32(0,0x009cb4c4),F32(0,0x009cb4c8),0);
    return atv_basis_rotation(row1,row2,right_sum);
}
/*0047ead3..00482790: independently solved left/right front linkage. */
static void atv_front_linkage(rx_addr v,rx_addr S,rx_v4 body_q,rx_v4 base,
                              rx_v4 centre,rx_v4 wheel_position,rx_v4 wheel_q,int right){
    rx_addr sim=U32(v,0x1700);
    rx_v4 mirror=v4(-1,1,1,1);
    rx_v4 anchor=v4_sub(v4(F32(0,0x009cd95c),-1.1f,F32(0,0x009cd98c),F32(0,0x009cd98c)),centre);
    rx_v4 fixed=v4_sub(v4(F32(0,0x009cd95c),-.65f,F32(0,0x009cd98c),F32(0,0x009cd98c)),centre);
    rx_v4 offset=v4(phys_add(F32(sim,0x41cc),F32(0,0x009d3bfc)),phys_add(F32(0,0x009cd95c),0),0,0);
    if(right){anchor=v4_mul(mirror,anchor);fixed=v4_mul(mirror,fixed);offset=v4_mul(mirror,offset);}
    rx_v4 steering=v4_ld(sim+(right?0x58a0:0x58b0));
    rx_v4 local_wheel=veh_rotate(atv_conjugate(body_q),v4_sub(wheel_position,base));
    rx_v4 pivot=v4_add(local_wheel,veh_rotate(steering,offset));
    rx_v4 delta=v4_sub(pivot,fixed),square=v4_mul(delta,delta);
    float length=phys_sqrt(phys_add(phys_add(square.v[0],square.v[2]),square.v[1]));
    float cosine=phys_div(phys_sub(phys_add(phys_mul(length,length),F32(0,0x00977cec)),F32(0,0x00977ce8)),
                          phys_mul(phys_mul(length,F32(0,0x0092e330)),F32(0,0x009776f0)));
    float sine_angle=rider_asin(cosine);
    float half=phys_mul(right?phys_sub(sine_angle,F32(0,0x00977508)):phys_sub(F32(0,0x00977508),sine_angle),.5f);
    rx_v4 joint=v4_add(v4_mul(veh_rotate(atv_axis_half(2,half),v4_sub(fixed,pivot)),v4_splat(phys_div(F32(0,0x009776f0),length))),pivot);
    /* The source writes duplicate endpoints for separate rendered links. */
    const uint32_t offsets_left[]={0x9ad0,0x9ae0,0x9b10,0x9b20,0x9b50,0x9b60};
    const uint32_t offsets_right[]={0x9af0,0x9b00,0x9b30,0x9b40,0x9b70,0x9b80};
    rx_v4 points[]={pivot,joint,joint,fixed,pivot,anchor};
    for(unsigned i=0;i<6;i++)v4_st(U32(v,0x1700)+(right?offsets_right[i]:offsets_left[i]),v4_add(veh_rotate(body_q,points[i]),base));
    float tilt_half=right?.3f:-.3f;
    rx_v4 first=atv_front_member(v4_sub(anchor,pivot),1,0,right);
    first=veh_quat_mul(body_q,veh_quat_mul(atv_axis_half(2,tilt_half),first));
    atv_set_rotation(v,right?0x153c:0x152c,S+0xf0,first);
    rx_v4 second=atv_front_member(v4_sub(fixed,joint),1,0,right);
    rx_v4 tilt=atv_axis_half(2,tilt_half);
    atv_set_rotation(v,right?0x1540:0x1530,S+0xf0,veh_quat_mul(body_q,veh_quat_mul(tilt,second)));
    rx_v4 third=atv_front_member(v4_sub(joint,pivot),0,right,0);
    third=veh_quat_mul(third,v4_ld(U32(v,0x1700)+(right?0x58a0:0x58b0)));
    third=veh_quat_mul(atv_axis_half(2,right?-0.0f:0.0f),third);
    atv_set_rotation(v,right?0x1544:0x1534,S+0xf0,veh_quat_mul(body_q,third));
    atv_set_rotation(v,right?0x1548:0x1538,S+(right?0x1a0:0x200),wheel_q);
}
/*00482790..00483356: lower of the two rear attachment points determines
 * the rear frame. Both vectors are transformed even when one is not selected. */
static void atv_rear_frame(rx_addr v,rx_addr S,rx_v4 body_q,rx_v4 base,rx_v4 left,rx_v4 right){
    rx_v4 inv=atv_conjugate(body_q);
    rx_v4 a=veh_rotate(inv,v4_sub(base,left)),b=veh_rotate(inv,v4_sub(base,right));
    rx_v4 chosen=a.v[1]<b.v[1]?a:b;
    rx_addr sim=U32(v,0x1700);
    rx_v4 target=v4(phys_add(F32(0,0x00d61670),F32(sim,0x40a0)),
        phys_add(chosen.v[1],F32(sim,0x40a4)),phys_add(chosen.v[2],F32(sim,0x40a8)),0);
    rx_v4 row2=atv_normalize3(veh_rotate(body_q,target));
    rx_v4 up=veh_rotate(body_q,v4_ld(0x00d615c0));
    rx_v4 row1=atv_normalize3(atv_cross3(up,row2));
    rx_v4 basis=atv_basis_rotation(row1,row2,1);
    rx_v4 zturn=v4(0,0,atv_cos_fit(F32(0,0x00977498)),phys_sub(-0.0f,atv_cos_fit(F32(0,0x00977508))));
    /*004832da..004832e0 UNPCK: sine in x, negated cosine fit in w. */
    rx_v4 xturn=v4(atv_cos_fit(F32(0,0x00977ce0)),0,0,phys_sub(-0.0f,atv_cos_fit(F32(0,0x009d78b4))));
    atv_set_rotation(v,0x154c,S+0x120,veh_quat_mul(veh_quat_mul(basis,zturn),xturn));
}
/* Scalar normalization used for shocks: unlike the packed normalizations it
 * leaves zero/NaN vectors untouched when the ordered positive test fails. */
static rx_v4 atv_scalar_unit3(rx_v4 p){
    float n=phys_add(phys_add(phys_mul(p.v[0],p.v[0]),phys_mul(p.v[1],p.v[1])),phys_mul(p.v[2],p.v[2]));
    if(n>0){float root;WORLD_CALL_SP(4,root=phys_sqrtf(n));float inv=phys_div(1,root);
        p.v[0]=phys_mul(p.v[0],inv);p.v[1]=phys_mul(inv,p.v[1]);p.v[2]=phys_mul(inv,p.v[2]);}
    return p;
}
/*004836f5..004847ba: three shock triples share an orientation but use their
 * own evaluated endpoints. They cannot be replaced by simulation body poses. */
static void atv_shock(rx_addr v,rx_addr S,rx_v4 body_q,uint32_t node,int last){
    WORLD_CALL_SP(0,rx_world_node_get_world_position(U32(v,node),S+0x100));
    WORLD_CALL_SP(0,rx_world_node_get_world_position(U32(v,node+8),S+0xf0));
    rx_v4 direction=v4_sub(v4_ld(S+0xf0),v4_ld(S+0x100));
    rx_v4 row1=atv_scalar_unit3(direction);
    /* The rear shock differs from the front two: 004842d8 uses the
     * rotated X axis, then 0048439b..00484413 normalizes axis x direction.
     * Its second basis row is this cross, not the rotated Z axis. */
    rx_v4 row2=last?atv_normalize3(atv_cross3(veh_rotate(body_q,v4_ld(0x00d615c0)),row1))
                  :veh_rotate(body_q,v4_ld(0x00d61680));
    rx_v4 q=atv_basis_rotation(row1,row2,0);
    atv_set_rotation(v,node,S+0x120,q);
    atv_set_rotation(v,node+4,S+0x120,q);
    atv_set_rotation(v,node+8,S+0x120,q);
}
void rx_atv_update_model_pose(rx_addr v){
    world_frame f=world_frame_enter(0x234,3,1);rx_addr S=f.esp;
    SU32(S,0x23c,U32(0,0x009b45e4)^(S+0xc));
    veh_model_node_set_matrix(U32(v,0x1328)+0x10,v+0xf0,WCTX(.ebx=v));
    rx_addr sim=U32(v,0x1700);
    rx_v4 rear_left=v4_ld(sim+0x400),body_q=v4_ld(sim+0x7c0),centre=v4_ld(sim+0x3640);
    rx_v4 rear_right=v4_ld(sim+0x4a0),front_left=v4_ld(sim+0x2c0),front_right=v4_ld(sim+0x360);
    rx_v4 rear_q=v4_ld(sim+0xae0),left_q=v4_ld(sim+0x900),right_q=v4_ld(sim+0x9a0);
    v4_st(S+0x150,rear_left);v4_st(S+0x130,rear_right);v4_st(S+0xe0,front_left);v4_st(S+0x1d0,front_right);
    v4_st(S+0x220,rear_q);v4_st(S+0x200,left_q);v4_st(S+0x1a0,right_q);v4_st(S+0x110,body_q);
    rx_v4 base=v4_sub(v4_ld(sim+0x180),veh_rotate(body_q,centre));v4_st(S+0xd0,base);
    atv_set_rotation(v,0x1528,S+0x120,veh_quat_mul(body_q,v4_ld(0x00ad66b0)));
    veh_node_set_world_position(U32(v,0x1528),v+0xc0,WCTX(.ebx=v));
    atv_front_linkage(v,S,body_q,base,centre,front_left,left_q,0);
    atv_front_linkage(v,S,body_q,base,centre,front_right,right_q,1);
    atv_rear_frame(v,S,body_q,base,rear_left,rear_right);
    float steer_half=phys_mul(F32(U32(v,0x1700),0x5898),.5f);
    rx_v4 handlebars=veh_quat_mul(atv_axis_half(0,-.15f),atv_axis_half(1,steer_half));
    atv_set_rotation(v,0x1550,S+0x120,veh_quat_mul(body_q,handlebars));
    atv_set_rotation(v,0x1554,S+0x220,rear_q);
    atv_shock(v,S,body_q,0x1558,0);
    atv_shock(v,S,body_q,0x1564,0);
    atv_shock(v,S,body_q,0x1570,1);
    phys_security_check_cookie(U32(S,0x23c)^(S+0xc));world_frame_leave(&f);
}
RX_FUNCTION(0x0047e850,rx_atv_update_model_pose)
