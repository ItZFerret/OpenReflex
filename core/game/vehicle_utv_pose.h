#pragma once
#include "../src/rx_mem.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_utv_update_model_pose(rx_addr vehicle); /*004c1fa0 ECX=V, unused stack dt RET4*/
/* Caller register values are used only in Node_Invalidate's saved stack slots. */
void rx_utv_update_model_pose_regs(rx_addr vehicle,uint32_t caller_ebx,uint32_t caller_edi);
#ifdef __cplusplus
}
#endif
