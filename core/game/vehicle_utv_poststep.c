/* UTV V61 and simulation poststep/debug geometry. Original 004c6e80,
 * 004d8160..004d9455. Hand port from disassembly and decompile.
 * The release executable retains geometry evaluation after removing its debug
 * renderer calls: those results overwrite a local vector, not vehicle state.
 * The sole simulation store in this entire 0x12f6-byte body is +8020=0.
 * Node virtual slot4 calls at the start are real simulation work. */
#include "vehicle_utv_poststep.h"
#include "vehicle_poststep_helpers.h"
#include "world_veh_util.h"

/* The debug code uses q*(v*conjugate(q)), with the same operation order as
 * veh_rotate. Individual inlined copies alternate A0+A3 vs A3+A0. Retain that
 * operand order where it differs, including distinct NaN payload preference. */
static rx_v4 poststep_quat_mul(rx_v4 a,rx_v4 b,int x_first){
    rx_v4 x=v4_mul(v4_mul(v4_lane(a,0),v4_perm(b,3,2,1,0)),v4(1,-1,1,-1));
    rx_v4 y=v4_mul(v4_mul(v4_lane(a,1),v4_perm(b,2,3,0,1)),v4(1,1,-1,-1));
    rx_v4 z=v4_mul(v4_mul(v4_perm(b,1,0,3,2),v4_lane(a,2)),v4(-1,1,1,-1));
    rx_v4 w=v4_mul(v4_lane(a,3),b);
    return v4_add(v4_add(z,y),x_first?v4_add(x,w):v4_add(w,x));
}
static rx_v4 poststep_rotate(rx_v4 q,rx_v4 v,int x_first){
    rx_v4 conj=v4_mul(q,v4_ld(0x0093a220));
    return poststep_quat_mul(q,veh_quat_mul(v4_xyz(v),conj),x_first);
}
/* Evaluate the three debug endpoints per wheel at 004d8204..004d8f2d.
 * No draw call survives in the original: each endpoint is placed in ESP+70,
 * then overwritten by the next. Other locals remain ordinary C values. */
static void utv_debug_corner(rx_addr s,rx_addr S,rx_v4 q,rx_v4 centre,rx_v4 base,
                             rx_v4 pivot,rx_v4 width,uint32_t wheel,uint32_t quat,
                             uint32_t contact,int right,int reverse_sum){
    if(right)pivot=v4_mul(v4_ld(S+0x120),pivot);
    rx_v4 p=v4_xyz(v4_sub(pivot,centre));
    v4_st(S+0x70,v4_add(poststep_rotate(q,p,reverse_sum),base));
    rx_v4 wheel_offset=poststep_rotate(v4_ld(s+quat),width,0);
    rx_v4 wheel_position=v4_ld(s+wheel);
    v4_st(S+0x70,right?v4_sub(wheel_position,wheel_offset):v4_add(wheel_offset,wheel_position));
    if(right)v4_st(S+0x70,wheel_position);
    v4_st(S+0x70,v4_add(poststep_rotate(q,v4_ld(s+contact),reverse_sum),base));
}
void rx_utv_simulation_poststep(rx_addr s){
    world_frame f=world_frame_enter(0x130,0,1);rx_addr S=f.esp;
    WORLD_CALL_SP(4,rx_vehicle_mechanism_node_poststep(s+0xd0));
    WORLD_CALL_SP(8,rx_vehicle_wheel_offset_debug(s,s+0x8080,phys_mul(F32(s,0x42d4),F32(0,0x009322cc))));
    WORLD_CALL_SP(8,rx_vehicle_wheel_offset_debug(s,s+0x81d0,phys_mul(F32(s,0x42d4),F32(0,0x009774b0))));
    WORLD_CALL_SP(8,rx_vehicle_wheel_offset_debug(s,s+0x8320,phys_mul(F32(s,0x4774),F32(0,0x009322cc))));
    WORLD_CALL_SP(8,rx_vehicle_wheel_offset_debug(s,s+0x8470,phys_mul(F32(s,0x4774),F32(0,0x009774b0))));
    rx_v4 mirror=v4(F32(0,0x009cd9e8),F32(0,0x00931f24),F32(0,0x00931f24),F32(0,0x00931f24));
    rx_v4 base=v4_ld(s+0x180),q=v4_ld(s+0x7c0),centre=v4_ld(s+0x36e0);
    float zero=F32(0,0x0092e12c);
    rx_v4 front_width=v4(F32(s,0x42d4),zero,zero,zero);
    rx_v4 rear_width=v4(F32(s,0x4774),zero,zero,zero);
    v4_st(S+0x120,mirror);v4_st(S+0x20,base);v4_st(S+0x30,v4_splat(zero));
    v4_st(S+0x40,centre);v4_st(S+0x50,q);
    utv_debug_corner(s,S,q,centre,base,v4_ld(s+0x3f90),front_width,0x220,0x860,0x5b10,0,1);
    utv_debug_corner(s,S,q,centre,base,v4_ld(s+0x3f90),front_width,0x2c0,0x900,0x6470,1,0);
    utv_debug_corner(s,S,q,centre,base,v4_ld(s+0x41f0),rear_width,0x360,0x9a0,0x6dd0,0,1);
    utv_debug_corner(s,S,q,centre,base,v4_ld(s+0x41f0),rear_width,0x400,0xa40,0x7730,1,1);
    /* 004d8f38 and004d9071: two optional debug flags use different offsets.
     * Preserve the original local point followed by its quaternion overwrite. */
    if(U32(s,0x90)){
        rx_v4 up=v4(zero,F32(0,0x009322c0),zero,zero);
        v4_st(S+0x80,v4_add(poststep_rotate(q,up,0),base));v4_st(S+0x80,q);
    }
    if(U32(s,0x8020)){
        rx_v4 up=v4(zero,F32(0,0x0092fe48),zero,zero);
        v4_st(S+0x80,v4_add(poststep_rotate(q,up,0),base));v4_st(S+0x80,q);
    }
    int32_t count=I32(s,0x1318);SU32(s,0x8020,0);
    /* 004d9195..004d938c: both contact debug endpoints are evaluated even
     * though each overwrites the other in the same local vector. */
    for(int32_t i=0;i<count;i++){
        v4_st(S+0x70,v4_ld(s+0x1320+16u*(uint32_t)i));
        rx_addr points=s+0x1c20+32u*(uint32_t)i;
        v4_st(S+0x70,v4_add(poststep_rotate(q,v4_ld(points),0),base));
        v4_st(S+0x70,v4_add(poststep_rotate(q,v4_ld(points+16),0),base));
    }
    /* 004d9392..004d944d: all sixteen debug linkage endpoints, in order. */
    for(uint32_t off=0x8750;off<=0x8840;off+=16)v4_st(S+0x70,v4_ld(s+off));
    world_frame_leave(&f);
}
RX_FUNCTION(0x004d8160,rx_utv_simulation_poststep)
void rx_utv_vehicle_poststep(rx_addr v){
    world_frame f=world_frame_flat(1,0);
    int32_t phase=I32(0,0x00ad3ed4)%3;
    if(phase==1||phase==2)WORLD_CALL_SP(0,rx_utv_simulation_poststep(U32(v,0x1540)));
    world_frame_leave(&f);
}
RX_FUNCTION(0x004c6e80,rx_utv_vehicle_poststep)
