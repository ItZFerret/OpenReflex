#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Complete004cf4c0/004b43c0: ESI simulation, stackdt,RET4,recorded5 EAX/EDX. */
void rx_utv_truck_filtered_contact(rx_addr simulation,float dt);
void rx_buggy_filtered_contact(rx_addr simulation,float dt);
/* Real body vf6 ECX input; original RA already present at entered rx_sp. */
void rx_fourwheel_filtered_publish(rx_addr body,rx_addr return_pc);
#ifdef __cplusplus
}
#endif
