#pragma once
#include "rx_game.h"
/* Complete private original-call frame observers. No address registration;
 * canonical CRT/physics semantic owners remain unique. */
float rx_contact_dependency_exact(rx_addr target,rx_addr expected,float xmm0);
float rx_contact_indexed_frame(rx_addr table,float input);
rx_addr rx_contact_force_at_point_frame(rx_addr force,rx_addr mechanism,rx_addr point,uint32_t index);
