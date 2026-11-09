#pragma once
#include "rx_game.h"
/* Complete, isolated leaves only. No product registrations or active inflater. */
uint32_t rx_compression_last_read(rx_addr self);       /*00829e40 RET0*/
uint32_t rx_compression_input_remaining(rx_addr self); /*00810870 RET0*/
void rx_compression_set_output(rx_addr self,rx_addr dst,uint32_t bytes); /*008e6bd0 RET8*/
void rx_compression_set_input(rx_addr self,rx_addr src,uint32_t bytes);  /*00829e50 RET8*/
