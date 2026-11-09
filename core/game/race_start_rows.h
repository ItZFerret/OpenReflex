#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ECX assignments, ESI row, stack(previousRow, first, max), RET12. */
int32_t rx_race_start_fill_row(rx_addr assignments, rx_addr previous_row, uint32_t first, uint32_t maximum, rx_addr row);
/* EAX position, ECX node, XMM6 distance, stack(outRow), RET4. */
void rx_race_start_next_row(rx_addr position, rx_addr node, float distance, rx_addr row);
#ifdef __cplusplus
}
#endif
