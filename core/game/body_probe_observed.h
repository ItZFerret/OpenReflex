#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ESIself, RET0, recorded5. Caller models RET; semantic kernel retains entrySP.
 * Existing ordinary435b00 and wheel438260 owners stay in the same TU. */
void bike_body_probe_contacts_observed(rx_addr simulation);
enum rx_body_probe_role {RX_BP_PROBES,RX_BP_TERRAIN,RX_BP_COLLISION,RX_BP_BOUNDS,RX_BP_COOKIE};
/* All arguments/RA already written at enteredSP. Finite role/site bridge only.
 * No native implementation is supplied until every actual callee is audited. */
rx_v4 rx_body_probe_dependency(enum rx_body_probe_role role,rx_addr target,rx_addr expected,rx_v4 xmm0);
void* rx_body_probe_bind(void* machine);
#ifdef __cplusplus
}
#endif
