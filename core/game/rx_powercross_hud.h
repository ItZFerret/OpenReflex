#pragma once
#include "rx_api.h"
#ifdef __cplusplus
extern "C" {
#endif
/*60B copied native presentation component state. Raw activity fields remain diagnostic.*/
typedef struct rx_powercross_hud_state {
 uint32_t size,version,valid;
 int32_t activity_type,activity_mode,activity_state,next_countdown_step,board_present;
 float activity_clock;
 uint32_t component_bound,visible;
 int32_t displayed_step;
 uint32_t exited;
 float hold_remaining;
 uint32_t event_count;
} rx_powercross_hud_state;
RX_API int rx_session_powercross_hud(struct rx_session*,rx_powercross_hud_state*);
#ifdef __cplusplus
}
#endif
