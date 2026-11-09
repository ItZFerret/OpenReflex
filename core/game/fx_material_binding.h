#pragma once
#include "rx_game.h"
#include <stddef.h>

#ifndef RX_ORACLE
/* Decoded actual material stream, synchronous host metadata only.
 * Invalid/incompatible reloads clear the old binding; no guest writes. */
void rx_fx_material_binding_capture(rx_addr handle, const void* bytes, size_t size);
#endif
