#pragma once
#include "rx_game.h"
/*008efd70 cdecl(kind,lens,ncodes,table**,rootbits*,work),RET0,mask10005.
 * code is exactly four guest bytes: op,bits,u16val. No product registration. */
int32_t rx_guest_inflate_table(uint32_t kind,rx_addr lengths,uint32_t codes,
                              rx_addr table_pointer,rx_addr root_bits,rx_addr work);
