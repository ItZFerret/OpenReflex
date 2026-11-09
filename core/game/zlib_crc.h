#pragma once
#include "rx_game.h"
/* Original cdecl(checksum,guest_bytes,length),RET0; unregistered drafts. */
uint32_t rx_guest_crc32(uint32_t checksum,rx_addr bytes,uint32_t length); /*008f0730 mask10005*/
uint32_t rx_guest_crc32_little(uint32_t checksum,rx_addr bytes,uint32_t length); /*008f0960 mask5*/
uint32_t rx_guest_crc32_big(uint32_t checksum,rx_addr bytes,uint32_t length); /*008f0d70 mask5*/
