/* TrophyTruck model/suspension pose, 0046c090. Hand transliteration of the
 * decompile, with SIMD association and stack pointers checked in disassembly.
 * Main-thread x87 control word 027f (53-bit precision). */
#include "vehicle_truck_pose.h"
#include "world_veh_util.h"
#include "bike_math.h"
#include "phys_crt.h"
static float truck_cos_fit(float angle){
    float x=rx_bf(rx_fb(angle)&0x7fffffffu);
    x=phys_sub(x,phys_mul((float)rx_cvtt(phys_mul(x,F32(0,0x9774a0))),F32(0,0x97749c)));
    float u=phys_sub(F32(0,0x977498),x),u2=phys_mul(u,u),u4=phys_mul(u2,u2);
    float a=phys_add(phys_mul(u2,F32(0,0x935d50)),F32(0,0x935d48));
    a=phys_mul(a,u4);
    a=phys_add(a,phys_add(phys_mul(u2,F32(0,0x935d44)),F32(0,0x935d4c)));
    a=phys_mul(a,u4);
    return phys_add(a,phys_add(phys_mul(u2,F32(0,0x935d40)),F32(0,0x935d3c)));
}
static rx_v4 truck_axis_half(unsigned axis,float half){
    rx_v4 q=v4_zero();q.v[axis]=truck_cos_fit(phys_add(half,F32(0,0x977508)));
    q.v[3]=phys_sub(-0.0f,truck_cos_fit(half));return q;
}
static rx_v4 truck_normalize3(rx_v4 a){
    rx_v4 p=v4_mul(a,a);
    return veh_normalize_len2(a,phys_add(phys_add(p.v[0],p.v[1]),p.v[2]));
}
/* Vec3_Cross preserves the original three scalar stores and y*x operand order. */
static rx_v4 truck_cross3(rx_v4 b,rx_v4 a){
    return v4(phys_sub(phys_mul(a.v[2],b.v[1]),phys_mul(a.v[1],b.v[2])),
              phys_sub(phys_mul(a.v[0],b.v[2]),phys_mul(b.v[0],a.v[2])),
              phys_sub(phys_mul(b.v[0],a.v[1]),phys_mul(a.v[0],b.v[1])),0);
}
/* The scalar normalized first basis row and four-case matrix-to-quaternion
 * conversion recur eleven times in V60. Equality ties select w, then x, then z.
 * right_sum retains the alternate front-right/rear trace addition ordering. */
static rx_v4 truck_basis_rotation(rx_v4 row1,rx_v4 row2,int right_sum){
    rx_v4 row0=truck_cross3(row1,row2);
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
/* The output quaternion is at ESP+10 throughout the basis conversions. */
static void truck_set_q(rx_addr v,uint32_t node,rx_addr scratch,rx_v4 q,uint32_t ebx){
 v4_st(scratch,q);veh_node_set_world_rotation(U32(v,node),scratch,WCTX(.ebx=ebx));
}
static rx_v4 truck_point(rx_v4 q,rx_v4 p,rx_v4 local){return v4_add(veh_rotate(q,local),p);}
static rx_v4 truck_anchor(rx_v4 q,rx_v4 p,rx_v4 centre,float x,float y,float z){return truck_point(q,p,v4_sub(v4(x,y,z,z),centre));}
static float truck_atan(float y,float x){return rx_f80_to_f32(crt_ci_atan2(rx_f80_from_f32(y),rx_f80_from_f32(x),CRT_CW_MAIN),0);}
/* Rear members point along row2; their row1 is the perpendicular projection of
 * the body Y axis (first link) or row2 crossed with body X (second link). */
static rx_v4 truck_rear_member(rx_v4 delta,rx_v4 axis,int projected,int trace){
 rx_v4 row2=truck_normalize3(delta),row1;
 if(projected){rx_v4 side=truck_normalize3(truck_cross3(axis,row2));row1=truck_normalize3(truck_cross3(row2,side));}
 else row1=truck_normalize3(truck_cross3(row2,axis));
 return truck_basis_rotation(row1,row2,trace);
}
static void truck_rear_side(rx_addr v,rx_addr S,rx_v4 q,rx_v4 p,rx_v4 centre,rx_v4 axle_q,rx_v4 midpoint,
                            float width,float outer,float lower_y,float lower_z,float upper_y,float link_z,float trim,int right,uint32_t ebx){
 const float sign=right?1.0f:-1.0f;
 rx_v4 lower=truck_point(axle_q,midpoint,v4(phys_mul(sign,width),-.3f,0,0));
 rx_v4 upper=truck_point(axle_q,midpoint,v4(phys_mul(sign,outer),.5f,0,0));
 rx_v4 fixed1=truck_anchor(q,p,centre,phys_mul(sign,width),upper_y,-2.15f);
 rx_v4 fixed2=truck_anchor(q,p,centre,phys_mul(sign,width),lower_y,lower_z);
 rx_v4 centre_point=truck_anchor(q,p,centre,0,-.6f,-1.3f);
 if(!right){
 v4_st(U32(v,0x15d0)+0x88c0,fixed1);v4_st(U32(v,0x15d0)+0x8900,lower);
 v4_st(U32(v,0x15d0)+0x8920,upper);v4_st(U32(v,0x15d0)+0x88f0,fixed2);v4_st(U32(v,0x15d0)+0x8910,centre_point);
 }
 rx_v4 first=truck_rear_member(v4_sub(centre_point,upper),veh_rotate(q,v4_ld(0x00d616a0)),1,right);
 truck_set_q(v,right?0x13c8:0x13d0,S+0x10,first,ebx);
 rx_v4 second=truck_rear_member(v4_sub(fixed2,lower),veh_rotate(q,v4_ld(0x00d615c0)),0,right);
 truck_set_q(v,right?0x13cc:0x13d4,S+0x10,second,ebx);
 rx_v4 end=truck_point(second,fixed2,v4(0,.4f,link_z,link_z));
 v4_st(U32(v,0x15d0)+0x88b0,end);
 rx_v4 delta=bike_rotate_inv(q,v4_sub(fixed1,end));
 float angle=phys_add(truck_atan(delta.v[2],delta.v[1]),trim);
 rx_v4 shock=veh_quat_mul(q,truck_axis_half(0,phys_mul(angle,.5f)));
 for(unsigned i=0;i<4;i++)truck_set_q(v,(right?0x13b8:0x13a8)+4*i,S+0x130,shock,ebx);
}
static void truck_front_side(rx_addr v,rx_addr S,rx_v4 q,rx_v4 p,rx_v4 centre,rx_v4 wheel,rx_v4 wheel_q,
                            float inner_x,float inner_y,float front_z,float shock_x,float shock_y,float mount_x,int right,uint32_t ebx){
 rx_addr sim=U32(v,0x15d0);float sign=right?1.0f:-1.0f;
 rx_v4 arm_anchor=v4_ld(sim+0x3f90);if(right)arm_anchor=v4_mul(v4(-1,1,1,1),arm_anchor);
 rx_v4 fixed=truck_point(q,p,arm_anchor);
 rx_v4 direction=right?v4_sub(fixed,wheel):v4_sub(wheel,fixed);
 rx_v4 row2=veh_rotate(q,v4_ld(0x00d61680));
 rx_v4 row1=truck_normalize3(truck_cross3(truck_normalize3(direction),row2));
 rx_v4 arm=truck_basis_rotation(row1,row2,right);
 truck_set_q(v,right?0x1380:0x1378,S+0x10,arm,ebx);
 truck_set_q(v,right?0x1384:0x137c,S+0x10,arm,ebx);
 truck_set_q(v,right?0x13dc:0x13d8,S+0x210,q,ebx);
 truck_set_q(v,right?0x1374:0x1370,S+(right?0x140:0x1b0),wheel_q,ebx);
 rx_v4 base=truck_anchor(q,p,centre,phys_mul(sign,inner_x),inner_y,front_z);
 rx_v4 upper=truck_anchor(q,p,centre,phys_mul(sign,shock_x),shock_y,front_z);
 rx_v4 mount=truck_anchor(q,p,centre,phys_mul(sign,1.2f),F32(0,0x009cd958),front_z);
 rx_v4 end=truck_point(arm,base,v4(phys_mul(sign,mount_x),.2f,0,0));
 rx_v4 outer=truck_point(arm,base,v4(phys_mul(sign,1.65f),.2f,0,0));
 uint32_t off=right?0x8860:0x8810;
 v4_st(U32(v,0x15d0)+off,end);v4_st(U32(v,0x15d0)+off+0x10,upper);
 v4_st(U32(v,0x15d0)+off+0x30,mount);v4_st(U32(v,0x15d0)+off+0x20,outer);v4_st(U32(v,0x15d0)+off+0x40,base);
 rx_v4 delta=bike_rotate_inv(q,v4_sub(upper,end));
 float angle=phys_sub(right?-.02f:.02f,truck_atan(delta.v[0],delta.v[1]));
 rx_v4 shock=veh_quat_mul(q,truck_axis_half(2,phys_mul(angle,.5f)));
 for(unsigned i=0;i<4;i++)truck_set_q(v,(right?0x1398:0x1388)+4*i,S+0x130,shock,ebx);
}
/* Four radiator/fan nodes rotate their LOCAL matrices using the filtered
 * speed. The axis uses the original RSQRTPS refinement, not sqrt/divide. */
static rx_v4 truck_fan_axis(void){
 rx_v4 a=v4_ld(0x00d616a0),out;
 float dot=(float)(((double)a.v[1]*a.v[1]+(double)a.v[0]*a.v[0])+(double)a.v[2]*a.v[2]);
 float r=rx_rsqrt(dot),t=phys_mul(phys_sub(phys_mul(phys_mul(r,r),phys_mul(dot,.5f)),.5f),-1);
 t=phys_add(phys_mul(t,r),r);
 for(unsigned i=0;i<4;i++)out.v[i]=dot==0?dot:phys_mul(t,a.v[i]);
 return out;
}
static void truck_rotation_rows(rx_v4 a,float angle,rx_v4 r[3]){
 float s=truck_cos_fit(phys_add(angle,F32(0,0x977508)));
 float c=phys_sub(0,truck_cos_fit(angle)),omc=phys_sub(1,c),D[3],P[3],M[3];
 const unsigned c9[]={1,2,0},d2[]={2,0,1};
 for(unsigned i=0;i<3;i++){
  float off=phys_mul(phys_mul(a.v[c9[i]],omc),a.v[d2[i]]),sa=phys_mul(s,a.v[i]);
  D[i]=phys_add(phys_mul(phys_mul(omc,a.v[i]),a.v[i]),c);
  P[i]=phys_add(sa,off);M[i]=phys_mul(phys_sub(sa,off),-1);
 }
 r[0]=v4(D[0],P[2],M[1],0);r[1]=v4(M[2],D[1],P[0],0);r[2]=v4(P[1],M[0],D[2],0);
}
static rx_v4 truck_matrix_row(rx_v4 row,const rx_v4 old[4],int reverse){
 rx_v4 a=v4_add(v4_mul(v4_lane(row,3),old[3]),v4_mul(v4_lane(row,1),old[1]));
 rx_v4 b=v4_add(v4_mul(v4_lane(row,2),old[2]),v4_mul(v4_lane(row,0),old[0]));
 return reverse?v4_add(b,a):v4_add(a,b);
}
static float truck_clamp01(float x){if(x>=1)return 1;if(0>=x)return 0;return x;}
static void truck_fans(rx_addr v,rx_addr S,float dt,uint32_t ebx){
 float scale=phys_mul(dt,15);
 float value=truck_clamp01(phys_mul(phys_mul(phys_sub(F32(U32(v,0x15d0),0x865c),110),.001f),dt));
 value=truck_clamp01(phys_sub(phys_add(value,F32(v,0x13f0)),phys_mul(phys_mul(F32(v,0x13f4),.1f),dt)));
 SF32(v,0x13f0,value);if(value>.5f)SF32(v,0x13f4,1);else if(.2f>value)SF32(v,0x13f4,0);
 float angles[4];
 for(unsigned i=0;i<4;i++)angles[i]=phys_mul(veh_lag_filter_update(v+0x13f8+0x14*i,F32(v,0x13f4),dt,WCTX()),scale);
 for(unsigned i=0;i<4;i++){
  rx_addr node=U32(v,0x13e0+4*i),matrix=U32(U32(node,0),0x18)+64*U8(node,0xb);
  rx_v4 old[4],rows[3];for(unsigned j=0;j<4;j++){old[j]=v4_ld(matrix+16*j);v4_st(S+0x1c0+16*j,old[j]);}
  truck_rotation_rows(truck_fan_axis(),angles[i],rows);
  for(unsigned j=0;j<3;j++)v4_st(matrix+16*j,truck_normalize3(truck_matrix_row(rows[j],old,i==0&&j!=0)));
  v4_st(matrix+0x30,truck_matrix_row(v4_ld(0x00d61630),old,1));
  veh_node_invalidate(node,WCTX(.ebx=ebx,.esi=v,.edi=v+0xc0));
 }
}
void rx_truck_update_model_pose_at(rx_addr v,float dt,uint32_t ebx){
 world_frame f=world_frame_enter(0x328,2,1);rx_addr S=f.esp;
 SU32(S,0x32c,U32(0,0x009b45e4)^(S+8));
 veh_model_node_set_matrix(U32(v,0x1328)+0x10,v+0xf0,WCTX());
 rx_addr sim=U32(v,0x15d0);
 rx_v4 q=v4_ld(v+0xe0),p=v4_ld(v+0xc0),rear_left=v4_ld(sim+0x360),rear_right=v4_ld(sim+0x400);
 rx_v4 front_left=v4_ld(sim+0x220),front_right=v4_ld(sim+0x2c0),rear_lq=v4_ld(sim+0x9a0),rear_rq=v4_ld(sim+0xa40);
 rx_v4 front_lq=v4_ld(sim+0x860),front_rq=v4_ld(sim+0x900);
 v4_st(S+0x210,q);v4_st(S+0x200,p);v4_st(S+0x250,rear_left);v4_st(S+0x170,rear_right);
 v4_st(S+0x220,front_left);v4_st(S+0x160,front_right);v4_st(S+0x2e0,rear_lq);v4_st(S+0x2a0,rear_rq);
 v4_st(S+0x1b0,front_lq);v4_st(S+0x140,front_rq);v4_st(S+0x2c0,v4(-1,1,1,1));
 veh_node_set_world_rotation(U32(v,0x1360),v+0xe0,WCTX(.ebx=ebx));
 veh_node_set_world_position(U32(v,0x1360),v+0xc0,WCTX(.esi=v,.edi=v+0xc0));
 const int pro2=I32(U32(v,0x15d0),0x55f0)==7;
 float front_inner=pro2?.53f:.6f,front_y=pro2?-1.82f:-1.7f,front_z=pro2?5.58f:F32(0,0x009cd9e4);
 float front_mount=pro2?1.1f:.77f,link_z=pro2?-2.75f:-2.2f,front_shock=pro2?.7f:F32(0,0x009d78a0);
 float rear_width=F32(0,pro2?0x009cd9a8:0x009cd9a4),front_shock_y=pro2?.6f:.25f,rear_lower_y=pro2?-1.8f:-1.65f;
 float rear_upper_y=pro2?F32(0,0x009cd970):.4f,rear_outer=pro2?1.95f:1.65f,rear_lower_z=pro2?-.5f:-.7f,trim=pro2?-.01f:0;
 rx_v4 midpoint=v4_mul(v4_add(rear_left,rear_right),v4_ld(0x00d615d0));v4_st(S+0x270,midpoint);
 sim=U32(v,0x15d0);
 rx_v4 anchor=truck_point(q,p,v4(F32(0,0x00d61670),F32(sim,0x41f4),F32(sim,0x41f8),0));
 rx_v4 row2=truck_normalize3(v4_sub(anchor,midpoint));
 rx_v4 across=truck_normalize3(v4_sub(rear_right,rear_left));
 rx_v4 row1=truck_normalize3(truck_cross3(row2,across));
 rx_v4 axle_q=truck_basis_rotation(row1,row2,1);v4_st(S+0x190,axle_q);
 veh_node_set_world_position(U32(v,0x1364),S+0x270,WCTX(.esi=v,.edi=v+0xc0));
 truck_set_q(v,0x1364,S+0x10,axle_q,ebx);
 rx_v4 centre=v4_ld(U32(v,0x15d0)+0x36e0);
 truck_rear_side(v,S,q,p,centre,axle_q,midpoint,rear_width,rear_outer,rear_lower_y,rear_lower_z,rear_upper_y,link_z,trim,0,ebx);
 truck_rear_side(v,S,q,p,centre,axle_q,midpoint,rear_width,rear_outer,rear_lower_y,rear_lower_z,rear_upper_y,link_z,trim,1,ebx);
 truck_set_q(v,0x1368,S+0x2e0,rear_lq,ebx);truck_set_q(v,0x136c,S+0x2a0,rear_rq,ebx);
 truck_front_side(v,S,q,p,centre,front_left,front_lq,front_inner,front_y,front_z,front_shock,front_shock_y,front_mount,0,ebx);
 truck_front_side(v,S,q,p,centre,front_right,front_rq,front_inner,front_y,front_z,front_shock,front_shock_y,front_mount,1,ebx);
 truck_fans(v,S,dt,ebx);
 phys_security_check_cookie(U32(S,0x32c)^(S+8));world_frame_leave(&f);
}
void rx_truck_update_model_pose(rx_addr v,float dt){rx_truck_update_model_pose_at(v,dt,0);}
RX_FUNCTION(0x0046c090,rx_truck_update_model_pose)

#include "anim.h"
/*00472d50 RavenVehicleTrophyTruck vf35. Locator matrix is mutable across vf16;
 * the scalar movement test reads its post-call values at original ESP+50..58. */
void rx_truck_publish_rider(rx_addr v,float dt){
    world_frame f=world_frame_enter(0x58,2,1);rx_addr S=f.esp,node;
    WORLD_CALL_SP(4,node=rx_hierarchy_find_node(U32(v,0x1328)+0x10,0x0093bff4));
    WORLD_CALL_SP(4,rx_world_node_evaluate_world(node,0));
    rx_addr matrix=U32(U32(node,0),0x1c)+64u*U8(node,0xb);
    for(unsigned i=0;i<4;i++)v4_st(S+0x20+16*i,v4_ld(matrix+16*i));
    WORLD_VCALL(U32(v,0x1a0),0x40,S+0x20);
    float py=F32(S,0x54),pz=F32(S,0x58),px=F32(S,0x50);
    SF32(S,0x14,py);SF32(S,0x18,pz);
    float vz=F32(v,0x168),vy=F32(v,0x164),vx=F32(v,0x160);
    float x=phys_add(phys_add(phys_add(phys_mul(vz,F32(v,0xf8)),phys_mul(vy,F32(v,0xf4))),phys_mul(vx,F32(v,0xf0))),F32(v,0xfc));
    float y=phys_add(phys_add(phys_add(phys_mul(vz,F32(v,0x108)),phys_mul(vy,F32(v,0x104))),phys_mul(vx,F32(v,0x100))),F32(v,0x10c));
    float z=phys_add(phys_add(phys_add(phys_mul(vz,F32(v,0x118)),phys_mul(vx,F32(v,0x110))),phys_mul(vy,F32(v,0x114))),F32(v,0x11c));
    rx_addr rider=U32(v,0x1a0);
    SF32(rider,0x5bb14,y);SF32(rider,0x5bb10,x);SF32(rider,0x5bb18,z);
    rx_addr sim=U32(v,0x15d0);
    SU32(U32(v,0x1a0),0x5bb30,U32(sim,0x90)?0:U32(v,0x150));
    if(!(U8(0,0x00db4710)&1)){
        SU32(0,0x00db4710,U32(0,0x00db4710)|1);
        SU32(0,0x00db4700,0);SU32(0,0x00db4704,0);SU32(0,0x00db4708,0);
    }
    float dy=phys_sub(py,F32(0,0x00db4704));
    float dx=phys_sub(px,F32(0,0x00db4700));
    float dz=phys_sub(pz,F32(0,0x00db4708));
    float distance2=phys_add(phys_add(phys_mul(dz,dz),phys_mul(dy,dy)),phys_mul(dx,dx));
    rider=U32(v,0x1a0);uint32_t moving=0;
    if(0>F32(rider,0x5bb18)){
        sim=U32(v,0x15d0);
        if((F32(sim,0x1c)==0||!(F32(sim,0x20)==0))&&U32(sim,0x90)==0&&distance2>F32(0,0x009cc8a4))moving=1;
    }
    SU32(rider,0x5bb54,moving);
    for(unsigned i=0;i<4;i++){
        sim=U32(v,0x15d0);rider=U32(v,0x1a0);
        SU32(rider,0x5b528+4*i,world_x87_pass(U32(sim,0x2c+4*i)));
    }
    world_vcall_f(U32(v,0x1a0),0x4c,rx_bf(world_x87_pass(rx_fb(dt))));
    world_frame_leave(&f);
}
RX_FUNCTION(0x00472d50,rx_truck_publish_rider)
