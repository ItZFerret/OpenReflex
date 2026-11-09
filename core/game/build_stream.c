/* build_stream.c -- see build_stream.h. */
#include "build_stream.h"

void rx_stream_read_host(rx_stream* s, void* dst, uint32_t n) {
    uint8_t* d = (uint8_t*)dst;
    for (uint32_t k = 0; k < n; k++) {
        if (s->pos < s->size) d[k] = s->data[s->pos++];
        else { d[k] = 0; s->short_read = 1; }
    }
}
void rx_stream_read(rx_stream* s, rx_addr dst, uint32_t n) {
    for (uint32_t k = 0; k < n; k++) {
        uint8_t b = 0;
        if (s->pos < s->size) b = s->data[s->pos++]; else s->short_read = 1;
        rx_wr8(dst + k, b);
    }
}
uint32_t rx_stream_u32(rx_stream* s) { uint8_t b[4]; rx_stream_read_host(s, b, 4); return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; }
