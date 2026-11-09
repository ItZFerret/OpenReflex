/* build_particle.h -- the particle asset (AssetIO tag "particle": ParticleAssetIO::vf3 007e3a10, the ParticleAsset object 007e1820 /
 * ParticleAsset::Parse 007e1a40, its 12 curve basis functions 007e3510 .. 007e3940 and the frame rect builder 007e3460).
 *
 * The asset (0x80 bytes): +0 vtable 0x00972934, +0x10 the emitter records (count at +0x14; 0x1f0 bytes each, name at +0), +0x18 the
 * frame table (count +0x1c; 8 bytes per frame: half u0, v0, height, width), +0x20 the 16 colour-map palette words, +0x60 the shadow
 * luminance (4 floats), +0x70 the particle luminance (4 floats). Emitter record offsets are in build_particle.c. */
#pragma once
#include "rx_game.h"
#include "build_asset.h"
#ifdef __cplusplus
extern "C" {
#endif

rx_addr rx_particle_asset_construct(rx_addr a);                 /* 007e1820 (ECX = a; the curve LUT at 0xa817b0) */
rx_addr rx_particle_asset_create(rx_stream* s);                 /* ParticleAssetIO::vf3 007e3a10: 0x80-byte asset + Parse */
void    rx_particle_asset_parse(rx_addr a, rx_stream* s);       /* ParticleAsset::Parse 007e1a40 */
void    rx_particle_asset_destroy(rx_addr a);                   /* ParticleAsset::vf0_o0 (007e18f0) */

#ifdef __cplusplus
}
#endif
