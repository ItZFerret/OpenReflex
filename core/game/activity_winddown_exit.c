/* Complete 00561ec0 shared Activity WindDown-exit owner. */
#include "activity_winddown_exit.h"

void rx_activity_winddown_exit(rx_addr activity, uint32_t new_state) {
    (void)activity;
    (void)new_state;
    if (U32(0, 0x00a942b4) == 1) {
        /* Preserve MOVSS payload bits, including NaNs and signed zero. The
         * recorder activity is authoritative, not the virtual-call receiver. */
        const uint32_t timer = U32(U32(0, 0x00a942b8), 0x294);
        SU32(0, 0x00a942b4, 0);
        SU32(0, 0x00a942a4, timer);
    }
}
RX_FUNCTION(0x00561ec0, rx_activity_winddown_exit)
