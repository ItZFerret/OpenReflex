/* 0042cb40..0042cccb, complete 396-byte Half4_ToFloat4.
 * This is the game's format, NOT IEEE half: exponent 31 remains finite.
 * Only the low eight input bytes participate; signed zero is preserved.
 */
#include "replay_decode.h"
#include "phys_vec.h"
#include <string.h>

static uint32_t replay_half_word(uint32_t h) {
    uint32_t fraction = h & 0x3ffu;
    int32_t exponent;
    if (h & 0x7c00u) exponent = (int32_t)((h >> 10) & 31u);
    else if (fraction) {
        exponent = 1;
        do { fraction += fraction; --exponent; } while (!(fraction & 0x400u));
        fraction &= 0x3ffu;
    } else exponent = -112;
    return (((h & 0x8000u) * 8u | fraction) << 13)
         | ((uint32_t)(exponent + 112) << 23);
}

rx_v4 rx_replay_half4_decode(rx_v4 packed) {
    uint16_t words[8];
    rx_v4 result;
    memcpy(words, &packed, sizeof words);
    for (int lane = 0; lane != 4; ++lane)
        result.v[lane] = rx_bf(replay_half_word(words[lane]));
    return result;
}
RX_FUNCTION(0x0042cb40, rx_replay_half4_decode)

replay_sample rx_replay_decode_record(rx_addr sample) {
    replay_sample d;
    rx_v4 p0 = rx_ld4(sample), p1 = rx_ld4(sample + 0x10);
    d.p = rx_replay_half4_decode(p0);
    rx_v4 packed_v = v4_shuf(p0, p0, 0x4e);
    d.v = rx_replay_half4_decode(packed_v);
    d.q = rx_replay_half4_decode(p1);
    rx_v4 packed_w = v4_shuf(p1, p1, 0x4e);
    d.w = rx_replay_half4_decode(packed_w);
    /* MOVSS/SHUFPS ED/F4 residual gather: (p.w,v.w,w.w,w.w). */
    rx_v4 residual = v4(d.p.v[3], d.v.v[3], d.w.v[3], d.w.v[3]);
    d.p = v4_add(residual, d.p);
    return d;
}

