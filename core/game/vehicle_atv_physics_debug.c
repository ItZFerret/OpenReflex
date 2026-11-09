/* 0049d7c0, called by V61: discarded debug geometry. The complete original
 * instruction ranges 0049d550+618,0049d7c0+4879,0049eae0+788 have exactly one
 * non-stack memory write, 0049e8c4 MOV [EDX+90e0],0. All line endpoints and
 * quaternion products die in stack temporaries; the caller consumes no return.
 * Preserve that sole simulation effect; this does not replace a force kernel.
 * See core/notes/atv-physics-2026-10-21.md for the access audit and validation. */
#include "build_vehicle_util.h"
void rx_atv_post_step(rx_addr s){SU32(s,0x90e0,0);}
RX_FUNCTION(0x0049d7c0,rx_atv_post_step)
