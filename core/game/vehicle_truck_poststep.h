#pragma once
#include "../src/rx_mem.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t rx_sim_truck_poststep_regs(rx_addr simulation,uint32_t *edx_out);
uint32_t rx_sim_truck_poststep(rx_addr simulation); /*00459fa0 ECX=S RET0*/
uint32_t rx_truck_poststep_regs(rx_addr vehicle,uint32_t *edx_out);
uint32_t rx_truck_poststep(rx_addr vehicle); /*00473450 ECX=V RET0; tailcalls sim*/
#ifdef __cplusplus
}
#endif
