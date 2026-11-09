#pragma once
#include "rx_game.h"
rx_addr rx_tuning_registry_get(void);           /*0042fed0, no args, EAX, RET0*/
void rx_tuning_registry_queue_clear(rx_addr q); /*004db6d0, ESI, RET0; frees nodes, not payloads*/
uint32_t rx_tuning_registry_cleanup(void);      /*0042ff00, no args, observed final EAX, RET0*/
uint32_t rx_tuning_registry_exit(void);         /*00911ed0 tail to0042ff00*/
