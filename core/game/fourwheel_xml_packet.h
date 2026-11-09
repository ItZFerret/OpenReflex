/* Isolated candidate. Copy to core/game only after reference acceptance. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/*00401f90 cdecl(size),RET0,EAX allocation. */
rx_addr rx_tuning_buffer_alloc(uint32_t bytes);
/*007905d0 ESI=queue owner,stack packet,RET4. Returns live ECX for oracle
 * adapter; the semantic routine has no result and native callers ignore it. */
uint32_t rx_tuning_packet_queue_append(rx_addr owner,rx_addr packet);
/*00790540 stdcall(sink,type,payload_size,payload),RET16. */
void rx_tuning_packet_publish(rx_addr sink,uint32_t type,uint32_t size,rx_addr payload);
/* Same implementation, exposing only the original live EAX/EDX. Disabled
 * branch leaves these input register values unchanged. Not a second algorithm. */
void rx_tuning_packet_publish_observe(rx_addr sink,uint32_t type,uint32_t size,rx_addr payload,uint32_t* eax_io,uint32_t* edx_io);
#ifdef __cplusplus
}
#endif
