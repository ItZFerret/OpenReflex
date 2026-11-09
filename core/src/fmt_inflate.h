/* fmt_inflate.h -- zlib / deflate decompression (RFC 1950 / 1951), written for the BXML containers so the readers do not
 * depend on a zlib build. Output size must be known by the caller (the containers store it). */
#ifndef FMT_INFLATE_H
#define FMT_INFLATE_H

#include "fmt_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Raw deflate stream -> exactly out_len bytes. *consumed gets the input bytes used (may be NULL). 0 / -1. */
int fmt_inflate(const uint8_t* in, size_t in_len, uint8_t* out, size_t out_len, size_t* consumed);
/* zlib stream (2-byte header, deflate, Adler-32) -> exactly out_len bytes; the stream must end exactly at in_len. */
int fmt_zlib_decode(const uint8_t* in, size_t in_len, uint8_t* out, size_t out_len);
uint32_t fmt_adler32(const uint8_t* data, size_t n);

#ifdef __cplusplus
}
#endif
#endif
