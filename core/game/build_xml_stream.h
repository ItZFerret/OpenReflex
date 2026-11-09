/* build_xml_stream.h -- the XMLReader's loader from a guest InputStream object (construction unit "asset_data", see build_xml.h).
 *
 * build_xml.c's rx_xml_reader_load is the host-stream level (the construction readers hand it the decoded package bytes). The
 * original loader FUN_008252c0 takes a guest InputStream OBJECT (MemoryInputStream 0x974d1c, CRTFileInputStream, ...) and reads
 * through its vtable; the tuning reload paths (0043b640 / 0048d320 / 00646a50 / the four-wheel vf8 bodies) pass the pending
 * stream object [0x009e3100]. This file is that loader, complete (allocator variants, Xbox big-endian data, the external data
 * section, the pre-1002 value records), plus the functions it calls that were not in the core: the CompressedInputStream wrapper
 * (init / read / close; its decompressor ZLibCompression and zlib's inflate run through their vtable: not ported).
 *
 * Reader fields (build_xml.h): +4 strings, +8 string bytes, +0xc string pointers, +0x10 string hash table, +0x14 / +0x18 data and
 * its size, +0x1c / +0x20 external data and its size, +0x24 the value + node block, +0x28 nodes, +0x2c node array, +0x30 values,
 * +0x34 value array, +0x38 / +0x3c root / current node, +0x2c8 the CompressedInputStream wrapper, +0x2f8 its ZLibCompression,
 * +0x33c the owned stream, +0x45c the allocator (IMemoryAllocator: vf1 alloc(size), vf2 free(p); 0 = the default 16-byte aligned
 * CRT heap under the allocator lock). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

/* 008252c0 (stdcall reader, stream; RET 8): load a BXML document from the guest stream object into the reader. 1 loaded, 0 failed. */
uint32_t rx_xml_reader_load_stream(rx_addr reader, rx_addr stream);

/* 0080fb80 CompressedInputStream::vf1 (ECX = the wrapper; RET 0): close. The decompressor's vf4 when a source is bound, the owned
 * chunk buffers freed, the state fields cleared (the chunk slots +0xc.. and the owner flag +0x10 stay). */
void rx_compressed_stream_close(rx_addr wrapper);

/* 0080f920 CompressedInputStream init (ESI = the wrapper, EAX = the source, ECX = the decompressor; stack the chunk size; RET 4):
 * binds the source and the decompressor (its vf3), allocates the chunk buffer, reads the compressed length and the first chunk.
 * 1 ready, 0 failed. */
uint32_t rx_compressed_stream_init(rx_addr wrapper, rx_addr source, rx_addr compression, uint32_t chunk_size);
/* 0080fc10 CompressedInputStream::vf2 (ECX = the wrapper; stack dst, count; RET 8): count decompressed bytes into dst. 1 read. */
uint32_t rx_compressed_stream_read(rx_addr wrapper, rx_addr dst, uint32_t count);

/* 007105c0 (cdecl p; RET 0): _aligned_free(p) under the allocator lock (critical section 0x00d61704 while [0x00da67d8]). */
void rx_aligned_free_locked(rx_addr p);

#ifdef __cplusplus
}
#endif
