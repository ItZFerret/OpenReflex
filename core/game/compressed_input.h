#pragma once
#include "rx_game.h"
uint32_t rx_guest_compressed_init(rx_addr wrapper,rx_addr source,rx_addr compression,uint32_t chunk_size); /*0080f920 ESI/EAX/ECX,stackchunk RET4,10007*/
uint32_t rx_guest_compressed_read(rx_addr wrapper,rx_addr dst,uint32_t count); /*0080fc10 ECX,RET8,10005*/
uint32_t rx_guest_compressed_skip(rx_addr wrapper,int32_t count); /*0080fda0 ECX,RET4,10005*/
uint32_t rx_guest_zcompression_init(rx_addr object); /*00829d10 ECX,RET0,10005*/
uint32_t rx_guest_zcompression_close(rx_addr object); /*00829d40 ECX,RET0,10005*/
uint32_t rx_guest_zcompression_process(rx_addr object,rx_addr produced,rx_addr consumed,rx_addr total_out,rx_addr total_in); /*00829e90 ECX,RET16,10005*/
/* Exact prewritten callee frame. No host stream/zlib substitute. */
void rx_wrapper_dependency(rx_addr target,rx_addr expected_return_esp);
void rx_wrapper_divide_error(void);
