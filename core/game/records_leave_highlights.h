#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* 007647f0, stdcall(records, controller), RET8. */
void rx_records_leave_highlights(rx_addr records,uint32_t controller);
/* Thin public entry to the existing canonical parent_ref kernel,004eb920. */
rx_addr rx_tbl_column_parent_ref(rx_addr column,uint32_t row);
#ifdef __cplusplus
}
#endif
