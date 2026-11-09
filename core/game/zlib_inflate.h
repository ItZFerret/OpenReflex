#pragma once
#include "rx_game.h"
/*008ea630 cdecl(stream,flush),RET0,mask10005. Source draft only: unregistered,
 * requires independent frame/branch review and original fixture acceptance. */
int32_t rx_guest_inflate(rx_addr stream,uint32_t flush);
