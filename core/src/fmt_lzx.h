/* fmt_lzx.h -- the LZX decompressor of the XMem resources, and the resource wrapper around it.
 *
 * LZX (Microsoft's cabinet / XNB / XMem codec; window 17 bits = 128 KiB in the Reflex packages): the stream is a sequence of
 * 32768-byte output frames; blocks (verbatim, aligned-offset, uncompressed) may span frames, the three-entry repeated-offset
 * queue, the Huffman trees' length deltas and the window persist inside one stream; the bit stream is 16-bit little-endian
 * words read most significant bit first and is re-aligned to a 16-bit boundary at the end of every frame. The decoder is
 * written from the format (public LZX description, MS-PATCH / cabinet documentation), not derived from any library, and gives
 * exactly the output of libmspack's lzxd for the streams in the packages (tested against the Python reference).
 *
 * Wrapper of a package resource, as laid out in the .package file (heap region of a database asset):
 *     u32 region_end (= size - 4)
 *     chunks, up to region_end:  u32 allocation (65536), u32 packed size, packed bytes
 *     u32 logical length of the last chunk (at region_end)
 *   a chunk whose packed size equals its allocation is stored. Otherwise it is XMem frames, ending in five zero bytes:
 *     frame = u16 BE packed size  (32768 output bytes)            or
 *             0xFF, u16 BE output size, u16 BE packed size        (a short frame)
 *   The frames of a chunk are ONE LZX stream (window 17); every chunk starts a new stream. */
#ifndef FMT_LZX_H
#define FMT_LZX_H

#include "fmt_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fmt_lzx fmt_lzx;

fmt_lzx* fmt_lzx_new(int window_bits);                       /* 15..21 */
void     fmt_lzx_reset(fmt_lzx* z);                          /* back to the state of a new stream */
void     fmt_lzx_free(fmt_lzx* z);
/* Decodes one stream: in (the frames' packed bytes, concatenated) -> exactly out_len bytes. 0 or -1. */
int      fmt_lzx_decode(fmt_lzx* z, const uint8_t* in, size_t in_len, uint8_t* out, size_t out_len);

/* One-shot helper. */
int      fmt_lzx_decode_once(int window_bits, const uint8_t* in, size_t in_len, uint8_t* out, size_t out_len);

/* A whole package resource (see above) -> its uncompressed bytes. max_out bounds the result (0: 256 MiB). */
int      fmt_xmem_resource(const uint8_t* data, size_t size, size_t max_out, fmt_buf* out);

#ifdef __cplusplus
}
#endif
#endif
