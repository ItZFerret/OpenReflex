#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Exact distinct3003B ECXself/stackdt/RET4 bodies. No product registration. */
void rx_buggy_contact_response_exact(rx_addr simulation,float dt);
void rx_truck_contact_response_exact(rx_addr simulation,float dt);
/* Actual shared451bf0 EDIself/RET0,2653B; entered guest SP retained. */
void rx_family_contact_response_helper_exact(rx_addr simulation);
enum rx_family_response_role {RX_FR_SHARED,RX_FR_CLAMP,RX_FR_NODES,RX_FR_HELPER,RX_FR_ENTER,RX_FR_LEAVE,RX_FR_VELOCITY};
void rx_family_contact_response_dependency(enum rx_family_response_role role,rx_addr expected);
void* rx_family_contact_response_bind(void* machine);
float rx_family_contact_response_curve(float input,rx_addr expected);
void* rx_family_contact_response_curve_bind(void* machine);
#ifdef __cplusplus
}
#endif
