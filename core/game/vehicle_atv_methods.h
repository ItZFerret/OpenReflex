#pragma once
#include "../src/rx_mem.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_sim_atv_apply_tuning(rx_addr s); /*0048cde0 ECX=s*/
void rx_sim_atv_update_frames(rx_addr s); /*004997f0 ECX=s*/
void rx_sim_atv_reset(rx_addr s,rx_addr position,rx_addr orientation); /*0049c8d0 thiscall RET8*/
#ifdef __cplusplus
}
#endif

void rx_sim_atv_update_probes(rx_addr s); /*004a1a10*/
