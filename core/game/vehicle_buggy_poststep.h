#pragma once
#include "../src/rx_mem.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_buggy_poststep(rx_addr vehicle); /*004ac520 ECX vehicle RET0*/
void rx_buggy_simulation_poststep(rx_addr simulation); /*004bd860 ESI simulation RET0*/
#ifdef __cplusplus
}
#endif
