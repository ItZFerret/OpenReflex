#pragma once
#include "rx_game.h"
rx_addr rx_cue_map_equal_range(rx_addr map, rx_addr out, rx_addr key);                                   /* 00705f70 (stdcall; RET 0xc) -> out */
void    rx_cue_range_count(rx_addr count, uint32_t fproxy, uint32_t fnode, uint32_t lproxy, uint32_t lnode);   /* 00709390 (EDI = &count; stdcall; RET 0x10) */
