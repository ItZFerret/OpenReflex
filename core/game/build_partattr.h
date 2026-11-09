/* build_partattr.h -- the ParticleAttrib asset (AssetIO tag "partattr"): ParticleAttribAssetIO::vf3 006940a0, the BXML parser
 * FUN_00693870 and the record helpers FUN_00691a50 / 00691a90 / 00691ad0 / 00691b10 / 00691b50 / 00694010, plus the destructor
 * FUN_00693810 / sdtor 006937f0.
 *
 * The asset (0x18 bytes, vtable 0x0095defc): +0 vtable, +4 the IAsset serial (Database::vf30), +8 the ref count, +0xc the loading
 * package, +0x10 the emitter record count, +0x14 the records (0xa0 bytes each). Record offsets: +0 name (an allocated string),
 * +4 emitter shape, +8 particle zone, +0xc four distance-zone bytes, +0x10 offset (vec3, lane 3 unwritten), +0x20 vehicle type,
 * +0x24 engine class, +0x28 tire, +0x2c body flag, +0x30 location (0..3), +0x34 the five trigger blocks (TireRollSlip /
 * TireSideSlip / VehicleVelocity / SuspensionForce / TireRotationSpeed: float value, +4 Inverse, +5 Enabled, 8 bytes each),
 * +0x5c TireInAir and +0x64 TireInWater (int value, +4 Enabled), then the five emit modifiers at +0x6c .. +0x8c (same 8-byte
 * layout).  fx_worldparticles.c binds them into the vehicle effect sets. */
#pragma once
#include "rx_game.h"
#include "build_asset.h"
#ifdef __cplusplus
extern "C" {
#endif

rx_addr rx_particle_attrib_asset_create(rx_stream* s);          /* ParticleAttribAssetIO::vf3 006940a0 */
void    rx_particle_attrib_asset_parse(rx_addr a, rx_stream* s);/* FUN_00693870 (ECX = the stream, stack: the asset) */
void    rx_particle_attrib_asset_destroy(rx_addr a);            /* FUN_00693810 */
rx_addr rx_particle_attrib_asset_sdtor(rx_addr a, uint32_t flags);   /* ParticleAttribAsset::sdtor 006937f0 */

#ifdef __cplusplus
}
#endif
