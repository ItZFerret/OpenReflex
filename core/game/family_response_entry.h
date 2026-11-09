#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* No registration here. Use only after complete composed proofs. */
void rx_buggy_contact_response_native_entry(rx_addr simulation,float dt);
void rx_truck_contact_response_native_entry(rx_addr simulation,float dt);
#ifdef __cplusplus
}
#endif
