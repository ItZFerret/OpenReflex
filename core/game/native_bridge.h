#pragma once
#include "rx_game.h"
/* Execute the already written original callee entry. Typed native decode only;
 * advances the original RET exactly, never re-pushes arguments. */
void rx_native_compressed_dependency(rx_addr target,rx_addr expected_return_esp);
uint32_t rx_native_reader_load_pc(rx_addr reader,rx_addr source);

uint32_t rx_native_compressed_count(unsigned index); /*0 init,1 close,2 process,3 inflate,4 sourceRead,5 lock,6 free*/
