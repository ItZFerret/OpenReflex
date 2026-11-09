#pragma once
#include "rx_game.h"
/* All APIs use guest32-bit addresses, never a host z_stream/state cast.
 * cdecl originals; unregistered until complete direct oracle evidence. */
uint32_t rx_guest_adler32(uint32_t checksum,rx_addr bytes,uint32_t length); /*008f04c0 mask10005*/
/* Proposed, not yet implemented/registered: original modifies circular state.
 * out_start is the caller's initial avail_out, not a pointer. */
int32_t rx_guest_inflate_updatewindow(rx_addr stream,uint32_t out_start); /*008ec530 mask5*/
