/* rx_x86.c -- RCPSS / RSQRTSS approximations (see rx_x86.h). */
#include "rx_x86.h"
#include "rx_rsqrt_rcp_tables.inc"

uint32_t rx_rsqrt_bits(uint32_t x) {
    uint32_t s = x >> 31, e = (x >> 23) & 255, m = x & 0x7fffff;
    if (e == 255) return m ? (x | 0x400000) : (s ? 0xffc00000u : 0u);
    if (e == 0) return s ? 0xff800000u : 0x7f800000u;                 /* denormals count as zero */
    if (s) return 0xffc00000u;
    uint32_t ref = (e & 1) ? 127 : 126;
    uint32_t r = (e & 1) ? RSQRT_127[m >> 13] : RSQRT_126[m >> 13];
    int32_t re = (int32_t)((r >> 23) & 255) + ((int32_t)ref - (int32_t)e) / 2;
    return ((uint32_t)re << 23) | (r & 0x7fffff);
}

uint32_t rx_rcp_bits(uint32_t x) {
    uint32_t s = x >> 31, e = (x >> 23) & 255, m = x & 0x7fffff;
    if (e == 255) return m ? (x | 0x400000) : (s << 31);
    if (e == 0) return (s << 31) | 0x7f800000u;
    uint32_t r = RCP_127[m >> 12];
    int32_t re = (int32_t)((r >> 23) & 255) + (127 - (int32_t)e);
    if (re <= 0) return s << 31;                                         /* tiny results flush to zero */
    return (s << 31) | ((uint32_t)re << 23) | (r & 0x7fffff);
}
