/* build_tdf.c -- see build_tdf.h. Transliteration of TDFAssetIO::vf3 0053a0c0, FUN_00539fa0, FUN_0053a060 and FUN_00810900. */
#include "build_tdf.h"
#include "build_alloc.h"

rx_addr rx_tdf_asset_io_create(rx_stream* s) {
    rx_addr a = rx_reflex_aligned_alloc(0x14);
    if (a) {                                              /* FUN_00539fa0 */
        SU32(a, 8, 0); SU32(a, 0xc, 0); SU32(a, 0, 0x00940298);
        rx_addr ms = rx_reflex_aligned_alloc(0x1c);
        if (ms) { SU32(ms, 4, 0); SU32(ms, 8, 0); SU32(ms, 0x10, 0); SU32(ms, 0, 0x00974d1c); SU32(ms, 0x14, 1); }
        SU32(a, 0x10, ms);
    }
    uint32_t size = rx_stream_u32(s);                     /* FUN_0053a060 */
    rx_addr ms = U32(a, 0x10);
    SU32(ms, 4, rx_crt_aligned_malloc(size, 16));         /* FUN_00810900 */
    SU32(ms, 8, size); SU32(ms, 0xc, 0); SU32(ms, 0x14, 1); SU32(ms, 0x18, 0);
    SU32(ms, 0x10, 1);
    rx_stream_read(s, U32(ms, 4), size);
    SU32(ms, 0x10, 0);
    return a;
}
