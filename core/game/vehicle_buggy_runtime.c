/* RavenVehicleBuggy V35, original004abe20..004ac0ee. Driver/rider publication.
 * Hand translation of decompile and SSE/x87 disassembly. */
#include "vehicle_buggy_runtime.h"
#include "world_veh_util.h"
#include "world_node.h"
#include "world_rider.h"
#include "anim.h"

void rx_buggy_publish_rider(rx_addr v,float dt){
    /* PUSH EBP, align16, SUB58, PUSH ESI/EDI. Locator callback sees the
     * original matrix address and can modify its translation before use. */
    world_frame frame=world_frame_enter(0x58,2,1);rx_addr S=frame.esp,node;
    WORLD_CALL_SP(4,node=rx_hierarchy_find_node(U32(v,0x1328)+0x10,0x0093bff4));
    WORLD_CALL_SP(4,rx_world_node_evaluate_world(node,0));
    rx_addr matrix=U32(U32(node,0),0x1c)+64u*U8(node,0xb);
    for(unsigned i=0;i<4;i++)v4_st(S+0x20+16*i,v4_ld(matrix+16*i));
    WORLD_VCALL(U32(v,0x1a0),0x40,S+0x20);
    float locator_y=F32(S,0x54),locator_z=F32(S,0x58),locator_x=F32(S,0x50);
    SF32(S,0x14,locator_y);SF32(S,0x18,locator_z);
    float vz=F32(v,0x168),vy=F32(v,0x164),vx=F32(v,0x160);
    float x=phys_add(phys_add(phys_add(phys_mul(vz,F32(v,0xf8)),phys_mul(vy,F32(v,0xf4))),phys_mul(vx,F32(v,0xf0))),F32(v,0xfc));
    float y=phys_add(phys_add(phys_add(phys_mul(vz,F32(v,0x108)),phys_mul(vy,F32(v,0x104))),phys_mul(vx,F32(v,0x100))),F32(v,0x10c));
    float z=phys_add(phys_add(phys_add(phys_mul(vz,F32(v,0x118)),phys_mul(vx,F32(v,0x110))),phys_mul(vy,F32(v,0x114))),F32(v,0x11c));
    rx_addr rider=U32(v,0x1a0);
    SF32(rider,0x5bb14,y);SF32(rider,0x5bb10,x);SF32(rider,0x5bb18,z);
    rx_addr sim=U32(v,0x15c0);
    SU32(U32(v,0x1a0),0x5bb30,U32(sim,0x90)?0:U32(v,0x150));
    if(!(U8(0,0x00db46d0)&1)){
        SU32(0,0x00db46d0,U32(0,0x00db46d0)|1);
        SU32(0,0x00db46c0,0);SU32(0,0x00db46c4,0);SU32(0,0x00db46c8,0);
    }
    float dy=phys_sub(locator_y,F32(0,0x00db46c4));
    float dx=phys_sub(locator_x,F32(0,0x00db46c0));
    float dz=phys_sub(locator_z,F32(0,0x00db46c8));
    float distance2=phys_add(phys_add(phys_mul(dz,dz),phys_mul(dy,dy)),phys_mul(dx,dx));
    rider=U32(v,0x1a0);uint32_t moving=0;
    /* COMISS zero,z JBE skips the branch for unordered z. UCOMISS/L AH
     * parity distinguishes NaNs from ordered equality for the two pedals. */
    if(0>F32(rider,0x5bb18)){
        sim=U32(v,0x15c0);
        if((F32(sim,0x1c)==0||!(F32(sim,0x20)==0))&&U32(sim,0x90)==0&&distance2>F32(0,0x009cc8a4))moving=1;
    }
    SU32(rider,0x5bb54,moving);
    for(unsigned i=0;i<4;i++){
        sim=U32(v,0x15c0);rider=U32(v,0x1a0);
        SU32(rider,0x5b528+4*i,world_x87_pass(U32(sim,0x2c+4*i)));
    }
    /* Virtual rider vf19 is (this,float), authoritative world_rider.h.
     * Integer dispatch would leave a stale XMM float argument on the host. */
    (void)world_vcall_f(U32(v,0x1a0),0x4c,rx_bf(world_x87_pass(rx_fb(dt))));
    world_frame_leave(&frame);
}
RX_FUNCTION(0x004abe20,rx_buggy_publish_rider)
