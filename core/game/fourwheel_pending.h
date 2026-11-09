#pragma once
#include "rx_game.h"
/* Complete caller bodies; deliberately unregistered until the guest reader and
 * stream teardown dependencies are closed and composed against the original. */
void rx_utv_pending_tuning(rx_addr simulation);   /*004cbf20 ECX, RET0*/
void rx_buggy_pending_tuning(rx_addr simulation); /*004b16f0 ECX, RET0*/
void rx_truck_pending_tuning(rx_addr simulation); /*00450370 ECX, RET0*/
