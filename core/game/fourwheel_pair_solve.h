#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/*004d1680/004b6ed0: stack simulation, RET4; recorded10005 EAX/EDX/x87.
 * Closed leaves: no private dependency callbacks or address registration. */
void rx_utv_truck_pair_angular_solve(rx_addr simulation);
void rx_buggy_pair_angular_solve(rx_addr simulation);
#ifdef __cplusplus
}
#endif
