/* UTV rider publication, virtual telemetry and two simulation kernels.
 * Hand transliterated from original decompile and disassembly; no lifted code.
 * Original addresses/ABIs in vehicle_utv_runtime.h. */
#include "vehicle_utv_runtime.h"
#include "world_veh_util.h"
#include "world_node.h"
#include "anim.h"
#include "fx_util.h"
#include "fourwheel_project_frames.h"
#include <stdlib.h>

/*004c6780 RavenVehicleUTV V35. The rider locator's evaluated matrix is passed
 * at the original aligned-frame address; the rider virtual can mutate it. */
void rx_utv_publish_rider(rx_addr v,float dt){
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
    rx_addr sim=U32(v,0x1540);
    SU32(U32(v,0x1a0),0x5bb30,U32(sim,0x90)?0:U32(v,0x150));
    if(!(U8(0,0x00db46b0)&1)){
        SU32(0,0x00db46b0,U32(0,0x00db46b0)|1);
        SU32(0,0x00db46a0,0);SU32(0,0x00db46a4,0);SU32(0,0x00db46a8,0);
    }
    float dy=phys_sub(py,F32(0,0x00db46a4));
    float dx=phys_sub(px,F32(0,0x00db46a0));
    float dz=phys_sub(pz,F32(0,0x00db46a8));
    float distance2=phys_add(phys_add(phys_mul(dz,dz),phys_mul(dy,dy)),phys_mul(dx,dx));
    rider=U32(v,0x1a0);uint32_t moving=0;
    if(0>F32(rider,0x5bb18)){
        sim=U32(v,0x1540);
        if((F32(sim,0x1c)==0||!(F32(sim,0x20)==0))&&U32(sim,0x90)==0&&distance2>F32(0,0x009cc8a4))moving=1;
    }
    SU32(rider,0x5bb54,moving);
    for(unsigned i=0;i<4;i++){
        sim=U32(v,0x1540);rider=U32(v,0x1a0);
        SU32(rider,0x5b528+4*i,world_x87_pass(U32(sim,0x2c+4*i)));
    }
    world_vcall_f(U32(v,0x1a0),0x4c,rx_bf(world_x87_pass(rx_fb(dt))));
    world_frame_leave(&f);
}
RX_FUNCTION(0x004c6780,rx_utv_publish_rider)

/*004c6ec0..004c6f60: aligned MOVAPS vector copies, including the fourth lane. */
static void utv_copy_vector(rx_addr v,rx_addr out,unsigned offset){
    world_frame f=world_frame_enter(0,0,1);v4_st(out,v4_ld(U32(v,0x1540)+offset));world_frame_leave(&f);
}
void rx_utv_position(rx_addr v,rx_addr out){utv_copy_vector(v,out,0x180);}
RX_FUNCTION(0x004c6ec0,rx_utv_position)
void rx_utv_rotation(rx_addr v,rx_addr out){utv_copy_vector(v,out,0x7c0);}
RX_FUNCTION(0x004c6ee0,rx_utv_rotation)
void rx_utv_velocity(rx_addr v,rx_addr out){utv_copy_vector(v,out,0x100);}
RX_FUNCTION(0x004c6f00,rx_utv_velocity)
void rx_utv_angular_velocity(rx_addr v,rx_addr out){utv_copy_vector(v,out,0x740);}
RX_FUNCTION(0x004c6f20,rx_utv_angular_velocity)
void rx_utv_body_velocity(rx_addr v,rx_addr out){utv_copy_vector(v,out,0x160);}
RX_FUNCTION(0x004c6f40,rx_utv_body_velocity)
void rx_utv_body_angular_velocity(rx_addr v,rx_addr out){utv_copy_vector(v,out,0x7a0);}
RX_FUNCTION(0x004c6f60,rx_utv_body_angular_velocity)
/* Main-thread x87 PC=53: retain the extended return, without rounding to f32. */
rx_f80 rx_utv_engine_rpm(rx_addr v){return rx_f80_mul(rx_f80_from_f32(F32(U32(v,0x1540),0x865c)),rx_f80_from_f32(F32(0,0x009774d0)),53,0);}
RX_FUNCTION(0x004c6f80,rx_utv_engine_rpm)
rx_f80 rx_utv_max_rpm(rx_addr v){return rx_f80_from_f32(F32(U32(v,0x1540),0x4d4c));}
RX_FUNCTION(0x004c6fa0,rx_utv_max_rpm)
uint32_t rx_utv_gear(rx_addr v){return U32(U32(v,0x1540),0x8660)+1u;}
RX_FUNCTION(0x004c6fb0,rx_utv_gear)
uint32_t rx_utv_rider_trigger(rx_addr v){return U32(U32(v,0x1540),0x8700);}
RX_FUNCTION(0x004c6fc0,rx_utv_rider_trigger)
uint32_t rx_utv_airborne(rx_addr v){return U32(U32(v,0x1540),0x8708);}
RX_FUNCTION(0x004c6fd0,rx_utv_airborne)
uint32_t rx_utv_all_wheels_contact(rx_addr v){rx_addr s=U32(v,0x1540);return U32(s,0x8180)&&U32(s,0x82d0)&&U32(s,0x8420)&&U32(s,0x8570);}
RX_FUNCTION(0x004c6fe0,rx_utv_all_wheels_contact)
uint32_t rx_utv_rider_flag(rx_addr v){return U32(U32(v,0x1540),0x56fc)==2;}
RX_FUNCTION(0x004c7040,rx_utv_rider_flag)

/*004cdb70: one projection correction from the first body is added to each body.
 * The dot uses xyz only, but the correction and stores retain all four lanes. */
static void utv_project_body_positions_kernel(rx_addr s,int observe){
    if(U32(s,0x90)){
        rx_v4 delta=v4_sub(v4_ld(s+0xf0),v4_ld(s+0x86a0));
        rx_v4 axis=v4_ld(s+0x8690),p=v4_mul(axis,delta);
        float dot=phys_add(phys_add(p.v[0],p.v[1]),p.v[2]);
        /* Original XOR EDX occurs even with a nonpositive body count. EAX
         * is only assigned by the actual positive loop's LEA/ADD sequence. */
        if(observe)fx_set_reg(FX_EDX,0);
        rx_v4 correction=v4_sub(v4_mul(v4_splat(dot),axis),delta);
        for(int32_t i=0;i<I32(s,0xec);i++){
            rx_addr body=s+0xf0+0xa0u*(uint32_t)i;
            v4_st(body,v4_add(v4_ld(body),correction));
            if(observe){fx_set_reg(FX_EDX,(uint32_t)i+1u);fx_set_reg(FX_EAX,body+0xa0u);}
        }
    }
}
void rx_utv_project_body_positions(rx_addr s){utv_project_body_positions_kernel(s,0);}
void rx_utv_project_body_positions_exact(rx_addr s){
    rx_addr entry=rx_sp();if(fx_reg(FX_ECX)!=s)abort();
    utv_project_body_positions_kernel(s,1);
    if(rx_sp()!=entry)abort();
}
RX_FUNCTION(0x004cdb70,rx_utv_project_body_positions)
/*004cdbe0: scalar length order x^2+z^2+y^2; packed comparisons require all
 * three xyz lanes. The original also scales w and records every compare mask. */
void rx_family_clamp_angular_velocity(rx_addr s,rx_addr wheel_limit,unsigned status_offset){
    world_frame f=world_frame_enter(0x14,3,1);
    rx_v4 limits[2]={v4_splat(F32(0,0x00935aac)),v4_splat(F32(0,wheel_limit))};
    int32_t count=I32(s,0xec);
    /* Chassis is always examined, even with a zero body count. */
    for(int32_t i=0;i==0||i<count;i++){
        rx_addr a=s+0x740+0xa0u*(uint32_t)i;
        rx_v4 v=v4_ld(a),p=v4_mul(v,v),limit=limits[i!=0];
        rx_v4 length=v4_sqrt(v4_splat(phys_add(phys_add(p.v[0],p.v[2]),p.v[1])));
        rx_v4 mask=v4_cmplt(limit,length);v4_st(f.esp+0x10,mask);
        if(U32(f.esp,0x10)+U32(f.esp,0x14)+U32(f.esp,0x18)==0xfffffffdu){
            v4_st(a,v4_div(v4_mul(v,limit),length));SU32(s,status_offset,1);
        }
        count=I32(s,0xec);
    }
    world_frame_leave(&f);
}
void rx_utv_clamp_angular_velocity(rx_addr s){rx_family_clamp_angular_velocity(s,0x009d7b34,0x8020);}
RX_FUNCTION(0x004cdbe0,rx_utv_clamp_angular_velocity)
