#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/*004b3370,108B through RET0. ECX simulation; no callees/allocations. */
void rx_buggy_project_body_positions_exact(rx_addr simulation);
#ifdef __cplusplus
}
#endif
