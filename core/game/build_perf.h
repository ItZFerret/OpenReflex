/* build_perf.h -- construction unit "asset_data" (2d): PerformanceAsset ("perfdat": the animation clip blocks of an animation set).
 *
 * PerformanceAsset (vtable 0x00975fec, 0x34 bytes; PerformanceAssetIO::vf3 0083ff90 -> FUN_0083f670): a BXML document
 * <Performance><AnimBlocks num=N><AnimBlock name start duration timeScale additive looping override cropFront cropBack addMax addIn addOut/>..
 * </AnimBlocks><HashMarks num=M><HashMark tick type duration/>..</HashMarks><Attributes Mirrored/></Performance>.
 *   +0x10 Mirrored (bool), +0x14 N, +0x18 the AnimBlock records (0x70 each: +0 name[0x40], +0x40 start * (1/30), +0x44 duration * (1/30),
 *   +0x48 timeScale, +0x4c cropFront * (1/30), +0x50 cropBack * (1/30), +0x54 floor((start + timeScale * duration) * 1000) * 0.001,
 *   +0x58 addMax, +0x5c addIn, +0x60 addOut, +0x64 additive, +0x68 override, +0x6c looping), +0x1c M, +0x20 the HashMark records (0xc each:
 *   +0 duration * (1/30) (0.25 when absent), +4 tick * (1/30), +8 type), +0x24 / +0x28 / +0x30 floor(max block end * 1000) * 0.001 (FUN_0083fe30),
 *   +0x2c 0.0; then +0x2c / +0x28 / +0x30 take the tick of the last hash mark of type 10 / 11 / 12. */
#pragma once
#include "rx_game.h"
#include "build_stream.h"
#ifdef __cplusplus
extern "C" {
#endif
rx_addr rx_perf_asset_io_create(rx_stream* s);       /* PerformanceAssetIO::vf3 0083ff90 */
#ifdef __cplusplus
}
#endif
