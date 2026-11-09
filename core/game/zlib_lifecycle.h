#pragma once
#include "rx_game.h"
/* All six originals cdecl, explicit stack arguments, RET0. No registrations. */
int32_t rx_guest_inflate_reset(rx_addr stream); /*008ea380*/
int32_t rx_guest_inflate_init2(rx_addr stream,int32_t window_bits,rx_addr version,uint32_t stream_size); /*008ea460*/
int32_t rx_guest_inflate_init(rx_addr stream,rx_addr version,uint32_t stream_size); /*008ea5e0*/
int32_t rx_guest_inflate_end(rx_addr stream); /*008ec710*/
rx_addr rx_guest_zalloc(rx_addr opaque,uint32_t items,uint32_t size); /*008ef290*/
void rx_guest_zfree(rx_addr opaque,rx_addr pointer); /*008ef2f0*/

/* Original RTC conditional boundary: caller ESP/return site and actual CMP operands.
 * Oracle invokes real008fa3bd with exact subtraction flags. Product equal branch
 * is original bareRET; unequal is an explicit unported failure boundary. */
void rx_guest_zlib_rtc(rx_addr caller,rx_addr return_site,uint32_t left,uint32_t right);

#ifdef RX_ORACLE
/* Oracle-only opaque Machine binding; returns previous binding for RAII restore.
 * Dependency adapters may establish it alongside their FxBind. */
void* rx_zlib_lifecycle_bind(void* machine);
void rx_zlib_lifecycle_compare(uint32_t left,uint32_t right);
uint32_t rx_zlib_lifecycle_callback(rx_addr target,unsigned nargs,const uint32_t* args);
#endif
