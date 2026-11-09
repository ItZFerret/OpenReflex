#pragma once
#include "../src/rx_mem.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_buggy_publish_rider(rx_addr vehicle,float dt); /*004abe20 ECX V, stack float dt RET4*/
#ifdef __cplusplus
}
#endif
