#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Entered callee ESP is retained; the typed caller alone models RET4/RET0.
 * Supported observed-domain output must be outside this callee's private
 * frame and genuine heap/body records must be separate from the guest stack. */
rx_addr rx_contact_wheel_correction_exact(rx_addr wheel,rx_addr simulation,rx_addr output);
void rx_contact_body_average_exact(rx_addr simulation);
void* rx_contact_frames_bind(void* previous);
/* Notifications of canonical already-computed results, not extra math. */
int rx_contact_frames_all3(rx_v4 mask);
void rx_contact_frames_body_moved(rx_addr body);
void rx_contact_frames_average_begin(void);
#ifdef __cplusplus
}
#endif
