/*00522eb0 /00522fc0 /00523230 /005232a0 /00523300. Source: reflex_003_0050c500.c;
 * instruction order checked against original disassembly. All stored math is SSE single precision.
 * The sole x87 fld/fstp copies an already computed positive length squared into SqrtF's argument;
 * it cannot introduce extra precision. No host min/max/negation substitutes for COMISS or -0 SUBSS. */
#include "race_braketurn.h"
#include "race_beh1_extern.h"
#include "phys_crt.h"

static rx_addr tuning(rx_addr b) { return 0x009e3648u + 0x1cu * b1_tuning_index(U32(b,8)); }
static void reset(rx_addr b) {
    rx_addr fn=U32(U32(b,0),0x18);
    void (*f)(rx_addr)=RX_FN(fn,void(*)(rx_addr));
    if(f) f(b); else rx_call_original(fn,RX_THISCALL,b,0,0);
}
static void disable(rx_addr b) {
    rx_addr fn=U32(U32(b,0),0x1c);
    void (*f)(rx_addr,uint32_t)=RX_FN(fn,void(*)(rx_addr,uint32_t));
    if(f) f(b,0); else {const uint32_t zero=0;rx_call_original(fn,RX_THISCALL,b,1,&zero);}
}
void rx_brake_turn_enable(rx_addr b,uint32_t enabled) {
    SU32(b,4,enabled);
    if(enabled){SU32(b,0x10,1);SU32(b,0x20,0);SU32(b,0x24,0x3f800000u);}
    else reset(b);
}
RX_FUNCTION(0x00522eb0,rx_brake_turn_enable)

uint32_t rx_brake_turn_steer(rx_addr b) {
    float amount=F32(b,0x20);
    if(1.0f>amount){
        SF32(b,0x20,rx_adds(F32(tuning(b),0),amount));
        SF32(b,0x24,rx_subs(F32(b,0x24),F32(tuning(b),0)));
    }
    const float negzero=rx_bf(0x80000000u); /*009774a8*/
    rx_addr d=U32(b,8);
    SF32(d,0x60,I32(b,0x18)==-1?rx_subs(negzero,F32(b,0x20)):F32(b,0x20));
    SF32(U32(b,8),0x3e0,rx_subs(negzero,F32(b,0x24)));
    SF32(U32(b,8),0x3d8,rx_subs(negzero,F32(b,0x24)));
    SU32(U32(b,8),0x64,0x3f800000u);
    d=U32(b,8);
    rx_addr v=b1_vehicle(d);
    float x=rx_subs(F32(b,0x30),F32(v,0xc0));
    float z=rx_subs(F32(b,0x38),F32(v,0xc8));
    float y=rx_subs(F32(b,0x34),F32(v,0xc4));
    float len=rx_adds(rx_adds(rx_muls(z,z),rx_muls(x,x)),rx_muls(y,y));
    if(len>0.0f){
        float inv=rx_divs(1.0f,phys_sqrtf(len));
        y=rx_muls(inv,y);x=rx_muls(x,inv);z=rx_muls(z,inv);
    }
    v=b1_vehicle(d);
    float dot=rx_adds(rx_adds(rx_muls(F32(v,0x138),z),rx_muls(F32(v,0x134),y)),rx_muls(F32(v,0x130),x));
    if(dot>=F32(tuning(b),4)||F32(tuning(b),8)>F32(v,0x150)){
        SU32(b,0x10,2);
        SF32(b,0x14,rx_adds(F32(tuning(b),12),F32(d,0x130)));
    }
    return 1;
}
RX_FUNCTION(0x00522fc0,rx_brake_turn_steer)

uint32_t rx_brake_turn_clutch(rx_addr b) {
    SU32(U32(b,8),0x3d0,1);
    if(U32(b,0xc)){
        SF32(b,0x14,rx_adds(F32(tuning(b),16),F32(U32(b,8),0x130)));
        SU32(b,0x10,4);
    }else disable(b);
    return 0;
}
RX_FUNCTION(0x00523230,rx_brake_turn_clutch)

uint32_t rx_brake_turn_wait_pullback(rx_addr b) {
    float now=F32(U32(b,8),0x130);
    if(now>F32(b,0x14)){
        SF32(b,0x14,rx_adds(F32(tuning(b),20),now));
        SU32(b,0x10,5);
    }
    return 0;
}
RX_FUNCTION(0x005232a0,rx_brake_turn_wait_pullback)

uint32_t rx_brake_turn_pullback(rx_addr b) {
    SF32(U32(b,8),0x3e0,rx_subs(rx_bf(0x80000000u),F32(tuning(b),24)));
    SF32(U32(b,8),0x3d8,rx_subs(rx_bf(0x80000000u),F32(tuning(b),24)));
    if(F32(U32(b,8),0x130)>F32(b,0x14)) disable(b);
    return 0;
}
RX_FUNCTION(0x00523300,rx_brake_turn_pullback)
