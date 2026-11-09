#pragma once
#include "../src/rx_mem.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct rx_node_poststep_regs{uint32_t eax,edx;}rx_node_poststep_regs;
rx_node_poststep_regs rx_node_debug_axes_regs(rx_addr node); /*00859b30 stack node RET4*/
rx_node_poststep_regs rx_node_mainbody_poststep_regs(rx_addr node,uint32_t eax,uint32_t edx); /*00859e40 ECX node RET0*/
rx_node_poststep_regs rx_node_rotor_poststep_regs(rx_addr node,uint32_t eax,uint32_t edx); /*0085b180 ECX node RET0*/
rx_node_poststep_regs rx_node_fixed_poststep_regs(rx_addr node,uint32_t eax,uint32_t edx); /*0085bed0 ECX node RET0*/
uint32_t rx_node_debug_axes(rx_addr node);
uint32_t rx_node_mainbody_poststep(rx_addr node);
uint32_t rx_node_rotor_poststep(rx_addr node);
uint32_t rx_node_fixed_poststep(rx_addr node);
#ifdef __cplusplus
}
#endif
