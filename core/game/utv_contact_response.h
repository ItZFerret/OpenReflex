#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/*004cdd20 ECXself,stackdt RET4, exact10005; no product registration. */
void rx_utv_contact_response_exact(rx_addr simulation,float dt);
enum rx_utv_response_role {RX_UR_SHARED,RX_UR_CLAMP,RX_UR_NODES,RX_UR_HELPER,RX_UR_ENTER,RX_UR_LEAVE,RX_UR_VELOCITY};
/* Actual RA/arguments already written at rx_sp; finite roles with validated
 * exact original call sites/RET. ENTER/LEAVE actual dynamic vf25/vf26 targets
 * reside in EDX. Typed/native composition is a separate explicit boundary. */
void rx_utv_contact_response_dependency(enum rx_utv_response_role role,rx_addr expected);
void* rx_utv_contact_response_bind(void* machine);
#ifdef __cplusplus
}
#endif
