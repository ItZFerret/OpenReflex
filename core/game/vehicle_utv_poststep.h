#pragma once
#include "../src/rx_mem.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_utv_vehicle_poststep(rx_addr vehicle); /*004c6e80 ECX=V RET0*/
void rx_utv_simulation_poststep(rx_addr simulation); /*004d8160 ESI=S RET0*/
#ifdef __cplusplus
}
#endif
