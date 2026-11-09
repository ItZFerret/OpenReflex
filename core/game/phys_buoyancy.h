#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif
/*0084f700 ESI=kinematic angular state, stackbody; RET4. */
void phys_buoyancy_angular_correction(rx_addr state,rx_addr body);
#ifdef __cplusplus
}
#endif
