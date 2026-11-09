/* build_packdata.h -- construction unit "asset_data" (2d): PackedDataAsset ("packdata": the terrain's packed friction / surface maps).
 *
 * PackedDataAsset (vtable 0x00972ba4, 0xa8 bytes; PackedDataAssetIO::vf3 007f8f30 -> ctor FUN_007f84d0 -> PackedDataAsset::vf3 007f8630): a
 * BXML document with <Interleaved> (bool) and either one map (Width Height NumBits1..4 NumWords Data) or four (Width1..4 Height1..4
 * NumBits1..4 NumWords1..4 Data1..4), each value on its own child node:
 *   +0x10 interleaved, +0x14 total bits per texel (the sum of the NumBits), +0x18 / +0x1c / +0x20 / +0x24 widths (uint), +0x28 .. +0x34 heights,
 *   +0x38 .. +0x44 widths (float), +0x48 .. +0x54 heights (float), +0x58 .. +0x64 words per map, +0x68 .. +0x74 bits per map,
 *   +0x78 .. +0x84 bit offsets (interleaved: running sum of the bits), +0x88 .. +0x94 the data blocks (words * 4 bytes), +0x98 .. +0xa4 the channel
 *   order (0 1 2 3; a map with 0 bits becomes 5 and the following entries move down by one). */
#pragma once
#include "rx_game.h"
#include "build_stream.h"
#ifdef __cplusplus
extern "C" {
#endif
rx_addr rx_packdata_asset_io_create(rx_stream* s);       /* PackedDataAssetIO::vf3 007f8f30 */
#ifdef __cplusplus
}
#endif
