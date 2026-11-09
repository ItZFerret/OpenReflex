#include "buggy_engine_phase.h"
#include "vehicle_fourwheel_leaves.h"
#include "fx_util.h"
#include "phys_vec.h"
/* Engine-speed-dependent timer and four-lane phase/toggle update. The visual
 * consumer is not yet identified; no guessed physical force is introduced.
 * Both original getters and the FDIVR use the vehicle/main53-bit nearest CW. */
void rx_buggy_engine_phase(rx_addr vehicle,float dt){
    fx_frame frame;fx_frame_enter0(&frame,0x20,16);
    fx_set_reg(FX_EAX,U32(vehicle,0));fx_set_reg(FX_EDX,U32(fx_reg(FX_EAX),0xbc));fx_set_reg(FX_ECX,vehicle);
    rx_addr sp=fx_push(0,0,0x4ad595);
    const rx_f80 rpm=rx_buggy_engine_rpm(vehicle);fx_pop(sp);
    SF32(frame.E,0xc,rx_f80_to_f32(rpm,0));
    fx_set_reg(FX_EAX,U32(vehicle,0));fx_set_reg(FX_EDX,U32(fx_reg(FX_EAX),0xc0));fx_set_reg(FX_ECX,vehicle);
    sp=fx_push(0,0,0x4ad5a5);const rx_f80 limit=rx_buggy_engine_limit(vehicle);fx_pop(sp);
    fx_set_reg(FX_EAX,U32(vehicle,0x15c0));
    const rx_f80 ratio=rx_f80_div(rx_f80_from_f32(F32(frame.E,0xc)),limit,53,0);
    SF32(frame.E,0xc,rx_f80_to_f32(ratio,0));
    const float threshold=phys_mul(phys_sub(F32(0,0x931f24),F32(frame.E,0xc)),F32(vehicle,0x1590));
    if(F32(vehicle,0x1594)>threshold){
        SF32(vehicle,0x1594,phys_sub(F32(vehicle,0x1594),threshold));
        const float step=F32(0,0x9cc8b4);
        rx_v4 phase=v4_add(v4(step,step,step,step),rx_ld4(vehicle+0x15a0));
        rx_st4(vehicle+0x15a0,phase);
        rx_v4 one=rx_ld4(0xd61690),mask=v4_cmplt(one,phase);rx_st4(frame.E+0x10,mask);
        fx_set_reg(FX_EAX,v4_lb(mask,2));
        const uint32_t sum=((v4_lb(mask,3)+v4_lb(mask,2))+v4_lb(mask,1))+v4_lb(mask,0);
        fx_set_reg(FX_ECX,sum);
        if(sum==0xfffffffcu){phase=v4_sub(phase,one);rx_st4(vehicle+0x15a0,phase);one=rx_ld4(0xd61690);}
        const rx_v4 zero=rx_ld4(0xd61660);mask=v4_cmpeq(rx_ld4(vehicle+0x15b0),zero);rx_st4(frame.E+0x10,mask);
        fx_set_reg(FX_EDX,v4_lb(mask,2));
        const uint32_t equal=((v4_lb(mask,3)+v4_lb(mask,2))+v4_lb(mask,1))+v4_lb(mask,0);fx_set_reg(FX_EAX,equal);
        rx_st4(vehicle+0x15b0,equal==0xfffffffcu?one:zero);
    }
    SF32(vehicle,0x1594,phys_add(F32(vehicle,0x1594),dt));fx_frame_leave(&frame);
}
RX_FUNCTION(0x004ad580,rx_buggy_engine_phase)
