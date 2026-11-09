/* build_tree.h -- construction unit "asset_forest" (2f): TreeAsset ("tree"; XenonTreeAsset, vtable 0x0095b51c, 0x970 bytes; TreeAssetIO::vf3
 * 00671ee0 -> XenonTreeAsset_Parse 00672030). File ('EERT' v1): a 0x2ec-byte header {+8 .., four geometry blocks of 0xb8 bytes at +0xc: +0 index
 * bytes, +4 vertex bytes, +0xc stride (u16), +0xe LOD count (u16), +0x10 ten LOD starts (u16), +0x24 ten counts (u16), +0x38 the vertex
 * declaration elements}, then the BXML (the Lods: near / far / dropToBillboard ... BoundingBox ShadowFitPoints), then per block the vertex
 * bytes and, for blocks 0 (when it has indices) / 1 / 3, the index bytes. Asset fields:
 *   +0x10 near, +0x14 far, +0x20 ShadowFitPoints (vec3 in 16-byte slots, at most 0x80), +0x820 their count, +0x824 dropToBillboard (uint),
 *   +0x828 targetAlphaValue, +0x82c transitionFactor, +0x830 leafLodMethod (uint), +0x834 leafLodTransitionRadius, +0x838 leafLodCurveExponent,
 *   +0x83c leafTransitionFactor, +0x840 crestWidthBB, +0x844 crestWidth, +0x848 cycleLengthBB, +0x84c cycleLength, +0x850 transitionWidth,
 *   +0x854 branchFadeDistance (15000.0 default), +0x858 frondFadeDistance (15000.0), +0x85c transitionPercent (0), +0x860 BoundingBox center,
 *   +0x870 half extents; the geometry groups: block 0 at +0x880 {+0 LOD count, +4 vertex declaration, +8 vertex buffer, +0xc stride, +0x10 index
 *   buffer, +0x14 ten LOD starts (u16), +0x28 ten counts}, block 1 at +0x8bc (same layout), block 2 at +0x8f8 {+0 count, +4 decl, +8 stride, +0xc vertex
 *   buffer, +0x10 starts, +0x24 counts}, block 3 at +0x930 (block 0's layout). D3D buffers: managed, usage 8, INDEX16. */
#pragma once
#include "rx_game.h"
#include "build_stream.h"
#ifdef __cplusplus
extern "C" {
#endif
rx_addr rx_tree_asset_io_create(rx_stream* s);
#ifdef __cplusplus
}
#endif
