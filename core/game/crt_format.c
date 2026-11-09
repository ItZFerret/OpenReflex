/* crt_format.c -- the game's formatting wrapper over the CRT (unit 10 addition, core/notes/crt.md): 0078fd60. */
#include "crt.h"

/* 0078fd60 (EDI = buffer, ESI = size; cdecl format, ...): _vsnprintf(buffer, size, format, the variadic arguments) through msvcr90's
 * import (crt_printf.c's port), then the terminator at the result when 0 < result < size, else at buffer[size - 1]. */
void rx_format_buffer(rx_addr buf, uint32_t size, rx_addr fmt, rx_addr args) {
    const int32_t n = crt_vsnprintf(buf, size, fmt, args);
    if (n > 0 && (uint32_t)n < size) SU8(buf, (uint32_t)n, 0);
    else SU8(buf, size - 1, 0);
}
RX_FUNCTION(0x0078fd60, rx_format_buffer)
