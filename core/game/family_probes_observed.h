#ifndef RX_FAMILY_PROBES_OBSERVED_H
#define RX_FAMILY_PROBES_OBSERVED_H
#include "rx_game.h"
/* ECX=self, original RET0 belongs to adapter; exact owner retains entryESP.
 * live_xmm0 is the original packed endpoint, including nonpositive-count paths. */
void rx_sim_buggy_update_probes_observed(rx_addr self,rx_v4*live_xmm0);
void rx_sim_truck_update_probes_observed(rx_addr self,rx_v4*live_xmm0);
#endif
