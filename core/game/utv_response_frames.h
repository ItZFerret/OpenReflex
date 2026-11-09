#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Scoped typed frame owners, source-only/unregistered. These are NOT a full
 * rx_utv_contact_response_dependency implementation: SHARED/NODES and the
 * genuine enter/leave-rest callback chains remain explicitly unclosed. */
void rx_utv_response_clamp_frame(rx_addr simulation,rx_addr expected_sp);
void rx_utv_response_velocity_frame(rx_addr mechanism,rx_addr vector,rx_addr expected_sp);
void rx_utv_response_helper_frame(rx_addr simulation,rx_addr expected_sp);
#ifdef __cplusplus
}
#endif
