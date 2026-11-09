#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Private caller-frame observers, not new semantic owners/registrations.
 * rx_sp is the actual original callee-entry return-address slot. */
rx_addr rx_query_lookup_frame(rx_addr registry);
void rx_query_probe_setup_frame(rx_addr base,rx_addr up,rx_addr probe,rx_addr target,rx_addr q,rx_addr rootq);
#ifdef __cplusplus
}
#endif
