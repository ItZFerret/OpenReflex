/* Two complete27-byte original vf3 leaves. Four explicit original DWORD
 * stores, no callees/stack/math/float conversion or original fallback. */
#include "fourwheel_contact_clear.h"
#include "fx_util.h"
uint32_t rx_fourwheel_clear_contact_words(rx_addr s){
 fx_set_reg(FX_EAX,0);
 SU32(s,0x8184,0);SU32(s,0x82d4,0);SU32(s,0x8424,0);SU32(s,0x8574,0);
 return 0;
}
RX_FUNCTION(0x004c0340,rx_fourwheel_clear_contact_words)
uint32_t rx_buggy_clear_contact_words(rx_addr s){
 fx_set_reg(FX_EAX,0);
 SU32(s,0x8174,0);SU32(s,0x82c4,0);SU32(s,0x8414,0);SU32(s,0x8564,0);
 return 0;
}
RX_FUNCTION(0x004a24f0,rx_buggy_clear_contact_words)
