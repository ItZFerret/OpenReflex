/* build_stream.h -- the asset input stream of construction (core/notes/construction.md, unit "asset_system").
 *
 * The original's Database hands each AssetIO::vf3 an InputStream (MemoryInputStream / CompressedInputStream: the asset's bytes from
 * its package, LZX decoded) and the loaders pull fields with its vf2, read(dst, n). The stream object itself is transient (not part
 * of the world), so the core's is a plain host struct over the decoded bytes (src/fmt_db.h fmt_install_read); what matters is that
 * each loader issues the same reads in the same order into the same game memory. A read past the end fills zeros and sets `short_read`
 * (the original's stream would return fewer bytes; the loaders do not check). */
#pragma once
#include "rx_game.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { const uint8_t* data; size_t size, pos; int short_read; } rx_stream;
static inline void rx_stream_init(rx_stream* s, const void* data, size_t size) { s->data = (const uint8_t*)data; s->size = size; s->pos = 0; s->short_read = 0; }
void     rx_stream_read(rx_stream* s, rx_addr dst, uint32_t n);      /* vf2(dst, n) into game memory */
void     rx_stream_read_host(rx_stream* s, void* dst, uint32_t n);   /* the same into a local of the loader */
uint32_t rx_stream_u32(rx_stream* s);                                 /* a 4-byte read into a local */
static inline float rx_stream_f32(rx_stream* s) { return rx_bf(rx_stream_u32(s)); }
#ifdef __cplusplus
}
#endif
