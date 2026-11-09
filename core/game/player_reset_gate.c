/* 0054e470: reset input eligibility. COMISS/JBE rejects unordered values. */
#include "player_reset_gate.h"
#include "phys_vec.h"
uint32_t rx_player_reset_input_allowed(rx_addr player) {
    uint32_t allowed=0;
    const rx_addr activity=U32(player,0x94);
    if(activity && U32(activity,0x21c)==2) {
        if(U32(activity,0x23c)==8) {
            if(U32(U32(0,0xa93fcc),0x298)==0)allowed=1;
        } else if(U32(activity,0x238)==2)allowed=1;
        else {
            const float timer=F32(player,0x1168);
            if(timer>F32(0,0x92e12c) && phys_sub(F32(0,0x9c5430),F32(0,0x9c5434))>timer)allowed=1;
        }
    }
    if(U32(0,0xaec048)!=0)allowed=1;
    return allowed;
}
RX_FUNCTION(0x0054e470,rx_player_reset_input_allowed)
