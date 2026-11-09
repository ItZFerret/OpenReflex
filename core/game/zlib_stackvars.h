#pragma once
#include "rx_game.h"
/*008fa3e0 actual ECXframe/EDXdescriptor; explicit typed API, not cdecl stackABI.
 * Checks original before/after CC sentinels; failures stay real dependencies. */
void rx_guest_inflate_check_stackvars(rx_addr frame,rx_addr descriptor);
