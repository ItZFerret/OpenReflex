/* Complete0047bd80/0047be30. One shared copy algorithm; the sole proven
 * difference is selected damage-entry source offset c versus110. */
#include "clip_damage_copy.h"
#include "rider_cap.h"
static void damage_copy(rx_addr vehicle,rx_addr manager,rx_addr out,uint32_t offset) {
    for(uint32_t i=0;i<rx_dmg_n();i++) {   /* (w-rc-core) + the side entries made */
        if(vehicle!=U32(rx_dmg_entry(manager,i),0x3fc))continue;
        const rx_addr record=rx_dmg_entry(manager,i);
        /* Original rereads unsigned count after each write, including aliases. */
        for(uint32_t n=0;n<U32(record,0);n++)SU32(out,4*n,U32(record,offset+4*n));
        return;
    }
}
void rx_clip_damage_copy_positions(rx_addr vehicle,rx_addr manager,rx_addr out) {damage_copy(vehicle,manager,out,0xc);}
RX_FUNCTION(0x0047bd80,rx_clip_damage_copy_positions)
void rx_clip_damage_copy_states(rx_addr vehicle,rx_addr manager,rx_addr out) {damage_copy(vehicle,manager,out,0x110);}
RX_FUNCTION(0x0047be30,rx_clip_damage_copy_states)
