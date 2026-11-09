/* build_tdf.h -- construction unit "asset_data" (2d): TDFAsset ("tdf": a text data file kept in memory).
 *
 * TDFAsset (vtable 0x00940298, 0x14 bytes; TDFAssetIO::vf3 0053a0c0 -> FUN_00539fa0 / FUN_0053a060): +0x10 a MemoryInputStream (0x1c bytes:
 * vtable 0x00974d1c, +4 the data block, +8 its size, +0xc 0, +0x10 0 once loaded, +0x14 1, +0x18 0) over the asset's bytes after a u32 size. */
#pragma once
#include "rx_game.h"
#include "build_stream.h"
#ifdef __cplusplus
extern "C" {
#endif
rx_addr rx_tdf_asset_io_create(rx_stream* s);       /* TDFAssetIO::vf3 0053a0c0 */
#ifdef __cplusplus
}
#endif
