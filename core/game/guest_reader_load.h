#pragma once
#include "rx_game.h"
/* Complete compressed-stream leaves and a scoped uncompressed parse body.
 * All remain deliberately unregistered in the product. */
void rx_guest_compressed_construct(rx_addr stream); /*0080f850 EAX,noargs,RET0*/
void rx_guest_compressed_close(rx_addr stream);     /*0080fb80 ECX,noargs,RET0*/
/* Prefix fragment at original loader caller-base: reset + ONE virtual read32
 * into base+40, retaining stream cursor/short-read effects. Caller owns actual
 * prologue/epilogue and continuation; return is read result, NOT parse success.
 * base=((entry-4)&~7)-0x80. No allocation/prefetch/host-stream conversion. */
uint32_t rx_guest_reader_load_header_at(rx_addr reader,rx_addr source,rx_addr base);
/* Modern PC BXML parse body, initial oracle scope uncompressed only.
 * Oracle precondition: default reader allocator and header read failure, or
 * version1002/flags0. Compressed init/read remain unported explicit dependencies. This narrow
 * routine is not registered as008252c0. Oracle classification must establish
 * format before selecting it; no format is rewritten or partially accepted. */
uint32_t rx_guest_reader_load_pc_uncompressed(rx_addr reader,rx_addr source);
