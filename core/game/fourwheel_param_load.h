#pragma once
#include "rx_game.h"
/* Isolated replacement bodies. NO RX_FUNCTION registrations: existing product
 * symbols at these addresses remain intact until explicit integration review. */
void rx_param_load_utv_exact(rx_addr parameters);   /*004cb360 stackP,RET4*/
void rx_param_load_buggy_exact(rx_addr parameters); /*004b0b30 stackP,RET4*/
void rx_param_load_truck_exact(rx_addr parameters,uint32_t engine_class); /*0044f690 EDI=P,stackclass,RET4*/
