#pragma once
#include "rx_game.h"
/* Product-owned passive observation; not an original guest proxy/callback. */
typedef void (*rx_career_post_witness)(void*,uint64_t,rx_addr);
uint32_t rx_career_completion_bind(void*,uint64_t,rx_addr,rx_addr,rx_career_post_witness);
void rx_career_completion_unbind(void*);
void rx_career_completion_observe(rx_addr,rx_addr,rx_addr,uint32_t);
void rx_career_completion_enable_tour(rx_addr);
