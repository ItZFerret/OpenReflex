#pragma once
#include "rx_game.h"
/* Complete unregistered destructor bodies, entry register self and RET0.
 * No outer-object free: scalar deleting destructors remain separate owners. */
void rx_guest_reader_destroy_exact(rx_addr reader); /*00824fe0 ESI,10007*/
void rx_guest_compressed_destroy_exact(rx_addr stream); /*0080f8a0 EDI,10007*/
/* Dependency boundary executes an already-written genuine callee entry frame.
 * Oracle preserves all actual original register/FPU/stack effects. Product
 * deliberately fails at an unresolved boundary; no approximate host ownership. */
void rx_guest_destroy_dependency(rx_addr target,rx_addr expected_return_esp);
