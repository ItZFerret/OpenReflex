/* build_forest.h -- construction unit "asset_forest" (2f), the ForestAsset ("forest"; XenonForestAsset, vtable 0x0095b554, 0x24b10 bytes).
 * ForestAssetIO::vf3 00661f10 (ReflexAlignedAlloc(0x24b10), the constructor FUN_0065cbb0, XenonForestAsset_Parse 0065cd90 = vf3 of the asset,
 * ForestAsset_ParseTreeTypes 0065dd30, ForestTreeType_BuildLodRecords 0065eb00). The race reads ~42 KB of it (the ForestSystem's per-type
 * instance lists, the collision shapes of the trunks: scene unit build_scene_forest.c; world + 0xcc9c).
 *
 * File: a BXML. Root attribute `version`; children BillboardAtlas {diffuse, normal: texture names}, TreeTypes {numTypes; per Type: TreeAsset,
 * BranchMaterial, FrondMaterial, LeafMaterial, 3DLeafMaterial, Shadow {texture, invProj}, PhysicsBxmlAsset, CollisionMAsset, CollisionFAsset,
 * CrownCollisionAsset, audioMaterial, audioSubclass, CrownAudioMaterial, CrownAudioSubclass, CrownParticleEmitter, TrunkParticleEmitter,
 * TrunkParticleVelocityThreshold, Billboard {width, height, uStartOffset, vStartOffset, uScale, vScale, uStep, count}}, Clusters
 * {numClusters, totalNumTrees, totalBBCenter, totalBBHalf; per Cluster: numTrees, boundingBoxCenter, boundingBoxHalf, per type (in order)
 * TreesOfType {numTrees, Positions (vec3 each), Rotations, Scales (2 floats each, [1] is used)}}}.
 * Layout:
 *   +0x10 + 0x80 c   cluster records (c < +0x1c210, at most 900): +0 pointer to the cluster's tree records (inside the block +0x24ae8),
 *                    +4 byte offset of the cluster in the instance vertex buffer (+0x24adc), +8 tree count, +0xc + 4 t first tree of
 *                    type t (numTypes + 1 entries: the last is the total), +0x60 boundingBoxCenter, +0x70 boundingBoxHalf (vec3 in 16)
 *   +0x1c210         cluster count
 *   +0x1d028 / +0x1d02c   ShadowBBVertexShader / ShadowBBPixelShader of the "TreeShader" shader asset (AddRef'ed D3D objects)
 *   +0x1d030 + 0x5f0 t    the tree types (t < +0x24ad0, at most 20): +0 tree handle, +4 branch / +8 frond / +0xc leaf / +0x10 3D leaf material
 *                    handles, +0x14 billboard start, +0x20 / +0x40 branch / frond fade constants (8 floats), +0x60 + 0x70 k LOD records
 *                    (count +0x4c0): +0 branch / +4 frond / +8 leaf / +0xc 3D leaf LOD, +0x10 transition flag, +0x14 distance, +0x20 / +0x40
 *                    8 leaf alpha floats each; +0x4c4 tree count; +0x4d0 / +0x4e0 the tree's BoundingBox centre / half; +0x4f4 shadow texture
 *                    handle, +0x500 shadow invProj matrix, +0x540 PhysicsBxml / +0x544 CollisionM / +0x548 CollisionF / +0x554 Crown collision
 *                    handles, +0x54c audioMaterial, +0x550 audioSubclass, +0x558 / +0x55c crown audio, +0x560 / +0x5a1 emitter names
 *                    (0x40 bytes each), +0x5e4 trunk velocity threshold
 *   +0x246f0 + 0x30 t     g_BBTreeTypes rows (3 vec4: width, height, transitionPercent / uStep ...)
 *   +0x24ab0 .. +0x24acc  constants of the constructor, +0x24ad0 type count, +0x24ad4 / +0x24ad8 BBVertexShader / BBPixelShader,
 *                    +0x24adc instance vertex buffer (0x30 per tree), +0x24ae0 / +0x24ae4 atlas diffuse / normal texture handles,
 *                    +0x24ae8 tree records (0x30 each: x, y, z, type + 0.5, scale, rotation quantised to 45 degrees, sin, cos, 16 bytes 0),
 *                    +0x24aec total tree count, +0x24af0 totalBBHalf, +0x24b00 totalBBCenter.
 * Render globals created on first use (and AddRef'ed by every forest): [0x00a94194] / [0x00a941a0] vertex declarations, [0x00a94198] (0x60 bytes)
 * / [0x00a9419c] (0x960) / [0x00a941a4] (0x40, the billboard corner quad) vertex buffers, [0x00a941a8] its index buffer. */
#pragma once
#include "rx_game.h"
#include "build_stream.h"
#ifdef __cplusplus
extern "C" {
#endif
rx_addr rx_forest_asset_io_create(rx_stream* s);   /* ForestAssetIO::vf3 00661f10 */
#ifdef __cplusplus
}
#endif
