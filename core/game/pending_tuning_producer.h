#pragma once
#include "rx_game.h"
/*004391a0, cdecl(size,payload),RET0. Creates genuine standalone MemoryInputStream
 * with borrowed payload only when no pending object exists. No registration. */
void rx_pending_tuning_receive(uint32_t size,rx_addr payload);
/* Private dependency roles keep native typed ownership explicit. */
enum rx_pending_role {RX_PENDING_ENTER,RX_PENDING_ALLOC,RX_PENDING_COMPARE,RX_PENDING_DELETE,RX_PENDING_LEAVE};
void rx_pending_dependency(enum rx_pending_role role,rx_addr target,rx_addr expected_return_esp,unsigned nargs,const uint32_t* args);
void rx_pending_allocation_fault(void);
