/* RavenVehicleUTV V60 (004c1fa0): independently evaluated four wheel
 * suspension and shock model rotations. Hand port of decompile/disassembly. */
#include "vehicle_utv_pose.h"
#include "world_veh_util.h"
#include "phys_crt.h"

static float utv_cos_fit(float angle){
    float x=rx_bf(rx_fb(angle)&0x7fffffffu);
    x=phys_sub(x,phys_mul((float)rx_cvtt(phys_mul(x,F32(0,0x9774a0))),F32(0,0x97749c)));
    float u=phys_sub(F32(0,0x977498),x),u2=phys_mul(u,u),u4=phys_mul(u2,u2);
    float a=phys_add(phys_mul(u2,F32(0,0x935d50)),F32(0,0x935d48));
    a=phys_mul(a,u4);
    a=phys_add(a,phys_add(phys_mul(u2,F32(0,0x935d44)),F32(0,0x935d4c)));
    a=phys_mul(a,u4);
    return phys_add(a,phys_add(phys_mul(u2,F32(0,0x935d40)),F32(0,0x935d3c)));
}
static rx_v4 utv_axis_half(unsigned axis,float half){
    rx_v4 q=v4_zero();q.v[axis]=utv_cos_fit(phys_add(half,F32(0,0x977508)));
    q.v[3]=phys_sub(-0.0f,utv_cos_fit(half));return q;
}
static rx_v4 utv_conjugate(rx_v4 q){return v4_mul(q,v4(-1,-1,-1,1));}
static rx_v4 utv_normalize3(rx_v4 a){
    rx_v4 p=v4_mul(a,a);
    return veh_normalize_len2(a,phys_add(phys_add(p.v[0],p.v[1]),p.v[2]));
}
/* Vec3_Cross preserves the original three scalar stores and y*x operand order. */
static rx_v4 utv_cross3(rx_v4 b,rx_v4 a){
    return v4(phys_sub(phys_mul(a.v[2],b.v[1]),phys_mul(a.v[1],b.v[2])),
              phys_sub(phys_mul(a.v[0],b.v[2]),phys_mul(b.v[0],a.v[2])),
              phys_sub(phys_mul(b.v[0],a.v[1]),phys_mul(a.v[0],b.v[1])),0);
}
/* The scalar normalized first basis row and four-case matrix-to-quaternion
 * conversion recur eight times in V60. Equality ties select w, then x, then z. */
static rx_v4 utv_basis_rotation(rx_v4 row1,rx_v4 row2){
    rx_v4 row0=utv_cross3(row1,row2);
    float l2=phys_add(phys_add(phys_mul(row0.v[1],row0.v[1]),phys_mul(row0.v[0],row0.v[0])),phys_mul(row0.v[2],row0.v[2]));
    if(l2>0){float root;WORLD_CALL_SP(4,root=phys_sqrtf(l2));row0=v4_mul(v4_splat(phys_div(1,root)),row0);}
    float x2=phys_mul(phys_sub(phys_sub(phys_add(row0.v[0],1),row1.v[1]),row2.v[2]),.25f);
    float y2=phys_mul(phys_sub(phys_add(row1.v[1],phys_sub(1,row0.v[0])),row2.v[2]),.25f);
    float z2=phys_mul(phys_add(phys_sub(phys_sub(1,row0.v[0]),row1.v[1]),row2.v[2]),.25f);
    float w2=phys_mul(phys_add(phys_add(row1.v[1],phys_add(row0.v[0],1)),row2.v[2]),.25f);
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
/* Inverse rotation follows 004c23d8 exactly: v*q first, then conjugate(q).
 * Keeping the products avoids the rounding and signed-zero changes of a matrix. */
static rx_v4 utv_unrotate(rx_v4 q,rx_v4 p){
    return veh_quat_mul(utv_conjugate(q),veh_quat_mul(v4_xyz(p),q));
}
static void utv_set_rotation(rx_addr v,uint32_t node,rx_addr scratch,rx_v4 q,uint32_t ebx){
    v4_st(scratch,q);veh_node_set_world_rotation(U32(v,node),scratch,WCTX(.ebx=ebx,.esi=v));
}
/* 004c2500: SHUFPS cross with the global Z basis, then packed normalize. */
static rx_v4 utv_arm_direction(rx_v4 direction){
    rx_v4 axis=v4_ld(0x00d61680);
    rx_v4 a=v4_perm(direction,2,0,1,3),b=v4_perm(axis,1,2,0,3);
    rx_v4 c=v4_sub(v4_mul(a,b),v4_mul(v4_perm(a,2,0,1,3),v4_perm(b,1,2,0,3)));
    return utv_normalize3(v4_xyz(c));
}
/* The eight inlined matrix conversions all use this fixed second row. */
static rx_v4 utv_member(rx_v4 direction){
    return utv_basis_rotation(direction,v4(F32(0,0x009cb4c0),F32(0,0x009cb4c4),F32(0,0x009cb4c8),0));
}
/* One corner of 004c2242..004c6322. Front and rear each evaluate both sides;
 * model-node offsets are consecutive triples for arms, hubs, wheels and shocks. */
static void utv_corner(rx_addr v,rx_addr S,rx_v4 body,rx_v4 base,rx_v4 centre,
                      rx_v4 wheel,rx_v4 wheel_q,rx_v4 lower,rx_v4 upper,
                      rx_v4 offset,float length2,float tilt_cos,float tilt_sin,
                      float shock_half,uint32_t nodes,int rear,int right,uint32_t ebx){
    rx_addr sim=U32(v,0x1540);
    rx_v4 mirror=v4(-1,1,1,1);
    rx_v4 pivot=v4_ld(sim+(rear?0x41f0:0x3f90));
    if(right){pivot=v4_mul(pivot,mirror);lower=v4_mul(mirror,lower);upper=v4_mul(mirror,upper);offset=v4_mul(mirror,offset);}
    pivot=v4_sub(pivot,centre);
    rx_v4 local=utv_unrotate(body,v4_sub(wheel,base));
    float dy=right?phys_sub(local.v[1],pivot.v[1]):phys_sub(pivot.v[1],local.v[1]);
    /* The arm's horizontal span sqrt(length^2 - dy^2). The original (004c2242.. sqrtps) has no guard: a wheel farther above / below the
     * arm's pivot than the arm is long (a remote copy's wheel body pushed past its travel by the record corrections after a hard hit;
     * a local simulation keeps it within reach) gives sqrt of a negative, NaN, and NaN arm / shock nodes. PORT FIX (w-online5): the
     * rule the simulation's own suspension frames use for the same circle, UTV S vf16 004d4750 (vehicle_utv_geometry.c `travel`):
     * 0.1 when out of reach. Bit for bit the original whenever it is in reach (the only change is where the original writes NaN). */
    const float dy2=phys_mul(dy,dy);
    float span=length2<dy2?rx_bf(0x3dcccccd):phys_sqrt(phys_sub(length2,dy2));
    rx_v4 arm=utv_normalize3(v4(span,dy,F32(0,0x00d61678),F32(0,0x00d61678)));
    rx_v4 basis=utv_member(utv_arm_direction(arm));
    rx_v4 tilt=v4(0,0,utv_cos_fit(tilt_sin),phys_sub(-0.0f,utv_cos_fit(tilt_cos)));
    rx_v4 relative=veh_quat_mul(tilt,basis);
    rx_v4 world=veh_quat_mul(body,relative);
    utv_set_rotation(v,nodes,S+0x130,world,ebx);
    utv_set_rotation(v,nodes+4,S+0x130,world,ebx);
    if(rear){
        utv_set_rotation(v,nodes+8,S+0x140,body,ebx);
        utv_set_rotation(v,nodes+12,S+0x230,wheel_q,ebx);
    }else{
        rx_v4 hub=veh_quat_mul(body,v4_ld(U32(v,0x1540)+0x5630));
        utv_set_rotation(v,nodes+8,S+0x130,hub,ebx);
        utv_set_rotation(v,nodes+12,S+0x1f0,wheel_q,ebx);
    }
    /* 004c2a49 / 004c3a95 / 004c4a55 / 004c5a65 recompute the product.
     * Both points are stored even though the same two debug fields are reused. */
    rx_v4 moving=v4_add(veh_rotate(relative,offset),lower);
    sim=U32(v,0x1540);
    v4_st(sim+0x8760,v4_add(veh_rotate(body,upper),base));
    v4_st(U32(v,0x1540)+0x8750,v4_add(veh_rotate(body,moving),base));
    rx_v4 shock=utv_member(utv_normalize3(v4_sub(upper,moving)));
    shock=veh_quat_mul(body,veh_quat_mul(utv_axis_half(2,shock_half),shock));
    utv_set_rotation(v,nodes+16,S+0x130,shock,ebx);
    utv_set_rotation(v,nodes+20,S+0x130,shock,ebx);
}
static rx_v4 utv_constant3(rx_addr x,rx_addr y,rx_addr z){
    return v4(F32(0,x),F32(0,y),F32(0,z),F32(0,z));
}
void rx_utv_update_model_pose_regs(rx_addr v,uint32_t ebx,uint32_t edi){
    world_frame f=world_frame_enter(0x24c,1,1);rx_addr S=f.esp;
    SU32(S,0x24c,U32(0,0x009b45e4)^(S+4));
    veh_model_node_set_matrix(U32(v,0x1328)+0x10,v+0xf0,WCTX(.esi=v));
    rx_addr sim=U32(v,0x1540);
    rx_v4 base=v4_ld(sim+0x180),centre=v4_ld(sim+0x36e0),body=v4_ld(sim+0x7c0);
    rx_v4 front_left=v4_ld(sim+0x220),front_right=v4_ld(sim+0x2c0);
    rx_v4 rear_left=v4_ld(sim+0x360),rear_right=v4_ld(sim+0x400);
    rx_v4 front_q=v4_ld(sim+0x860),rear_q=v4_ld(sim+0x9a0);
    v4_st(S+0x150,base);v4_st(S+0x140,body);v4_st(S+0x1f0,front_q);v4_st(S+0x230,rear_q);
    v4_st(S+0x190,front_left);v4_st(S+0x1b0,front_right);v4_st(S+0x1d0,rear_left);v4_st(S+0x180,rear_right);
    rx_v4 front_offset=utv_constant3(0x977c18,0x977c1c,0x977c24);
    rx_v4 front_lower=v4_sub(utv_constant3(0x977c20,0x977c10,0x977c14),centre);
    rx_v4 front_upper=v4_sub(utv_constant3(0x977c04,0x977c08,0x977c14),centre);
    float front_length2=phys_mul(F32(sim,0x3fe4),F32(sim,0x3fe4));
    rx_v4 rear_offset=utv_constant3(0x977bf4,0x977bf8,0x977c0c);
    rx_v4 rear_lower=rear_right;
    int32_t mode=I32(sim,0x55f0);
    if(mode==0){SF32(S,0xac,0);rear_lower=utv_constant3(0x977c20,0x977bf0,0x977c00);}
    else if(mode==1){SF32(S,0xac,.5f);rear_lower=utv_constant3(0x977c20,0x9774b0,0x977c00);}
    rear_lower=v4_sub(rear_lower,centre);
    rx_v4 rear_upper=v4_sub(utv_constant3(0x977bfc,0x977c08,0x977c00),centre);
    float rear_length2=phys_mul(F32(sim,0x4244),F32(sim,0x4244));
    veh_node_set_world_rotation(U32(v,0x1360),v+0xe0,WCTX(.ebx=ebx,.esi=v,.edi=edi));
    veh_node_set_world_position(U32(v,0x1360),v+0xc0,WCTX(.ebx=ebx,.esi=v,.edi=edi));
    utv_corner(v,S,body,base,centre,front_left,front_q,front_lower,front_upper,front_offset,
        front_length2,F32(0,0x977bec),F32(0,0x977be8),0,0x1364,0,0,ebx);
    utv_corner(v,S,body,base,centre,front_right,front_q,front_lower,front_upper,front_offset,
        front_length2,F32(0,0x977be4),F32(0,0x977be0),0,0x137c,0,1,ebx);
    utv_corner(v,S,body,base,centre,rear_left,rear_q,rear_lower,rear_upper,rear_offset,
        rear_length2,F32(0,0x977bdc),F32(0,0x977bd8),phys_mul(F32(S,0xac),.5f),0x1394,1,0,ebx);
    utv_corner(v,S,body,base,centre,rear_right,rear_q,rear_lower,rear_upper,rear_offset,
        rear_length2,F32(0,0x977bd4),F32(0,0x977bd0),phys_mul(F32(S,0xac),-.5f),0x13ac,1,1,ebx);
    phys_security_check_cookie(U32(S,0x24c)^(S+4));world_frame_leave(&f);
}
void rx_utv_update_model_pose(rx_addr v){rx_utv_update_model_pose_regs(v,0,0);}
RX_FUNCTION(0x004c1fa0,rx_utv_update_model_pose)
