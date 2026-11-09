/* build_anim.h -- construction unit "asset_anim": AnimAsset objects from the Animations database's "anim" assets
 * (core/notes/construction.md).
 *
 * AnimAsset (vtable 0x00975ebc, 0x68 bytes, an _aligned_malloc block of 0x7b): the Database creates it through AnimAssetIO::vf3
 * 0082cc70: ReflexAlignedAlloc(0x68), the constructor FUN_0082bb90, the stream loader FUN_0082bf10 (versions: field layouts by the
 * stream's version word; version >= 5 with the marker 0x12345678 is the packed format, FUN_0082bda0: a 0x40-byte header object at
 * +0x64 holding the packed data block at its +0x3c). Fields the loaders do not write, set by the Database after vf3 (Database::vf30
 * 0084ad10, unit asset_system): +4 the database's load serial ([database + 0xe8]++), +8 the reference count (+1), +0xc the package
 * record. Layout (offsets into the object):
 *   +0x10 n names, +0x14 names (n x 0x20, freed after loading in some versions), +0x18 / +0x1c bone tracks (0x14 each),
 *   +0x20 / +0x24 position tracks (0x18 each), +0x28 / +0x2c, +0x30 / +0x34 / +0x38 float-key tracks (0x14 each), +0x3c / +0x40
 *   per-bone key tracks, +0x48, +0x4c duration, +0x50 = min(+0x4c * 0.25, 0.5), +0x58 one block for the new layout's arrays,
 *   +0x5c its fill offset, +0x60 its size, +0x64 the packed header object. */
#pragma once
#include "rx_game.h"
#include "build_stream.h"
#ifdef __cplusplus
extern "C" {
#endif
rx_addr rx_anim_asset_construct(rx_addr self);            /* 0082bb90: the constructor (EAX = self); returns self */
void    rx_anim_asset_load(rx_addr self, rx_stream* s);  /* 0082bf10: the stream loader (ECX = self, EAX = stream) */
void    rx_anim_asset_load_packed(rx_addr self, rx_stream* s);   /* 0082bda0: the packed format (EDI = self, ESI = stream) */
rx_addr rx_anim_asset_io_create(rx_stream* s);            /* AnimAssetIO::vf3 0082cc70: the new asset (its out parameter) */
#ifdef __cplusplus
}
#endif
