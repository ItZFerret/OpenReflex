/* build_model.h -- construction unit "asset_model" (2b): ModelAsset objects from "model" assets (magic 0x88aabb00, versions 3 and 4).
 *
 * ModelAsset (vtable 0x00972844, 0x140 bytes; ModelAssetIO::vf3 007d53c0 -> ctor FUN_007d3f30, FUN_007d40e0 reads the 8-byte prefix
 * {magic, version} and dispatches: version 1 FUN_007d4f00, 2 ModelAsset_LoadVariant, else ModelAsset_LoadStatic 007d4650 with the version
 * (3: header 0xa0 bytes, 4: 0xa4)):
 *   +0x10 the node set of the file (NodeSet: +0 root record, +4 walk cursor, +8 node records (16 each: +0 the set, +8 u16 name offset in
 *     the pool, +0xb own index, +0xc flags, +0xd previous sibling, +0xe last child, +0xf parent; 0xff = none), +0xc count, +0x10 name
 *     pool, +0x14 its size, +0x18 local matrices, +0x1c world matrices, +0x20 bounds (0x50 per node), +0x24 inverse world matrices
 *     (count > 1), +0x28 spare matrices, +0x34 dirty, +0x38 / +0x3a u16, +0x3c the one 128-aligned block holding all of it),
 *   +0x50 a second node set (copy of the first, FUN_007dd930 / FUN_007ddb40), +0x90 -1, +0x94 header flags (0 | 1), +0x118 LOD count, +0x98 the model's name
 *   (0x7f), +0x11c binding count, +0x120 draw distance (header word 40, or trunc(root radius * 135) when 0; at most 0xffff),
 *   +0x124 std::vector<LodEntry> (proxy +0x124, begin +0x130, end +0x134, capacity +0x138; LodEntry 0x1c: +0 distance, +4 a
 *   std::vector<Binding> (proxy +4, begin +0x10, end +0x14, cap +0x18); Binding 0x10: surface slot, node index, surface handle, material
 *   handle), +0x13c the model's own surface handle (the "surface" asset of the same name). */
#pragma once
#include "rx_game.h"
#include "build_stream.h"
#ifdef __cplusplus
extern "C" {
#endif
rx_addr rx_model_asset_io_create(rx_stream* s);       /* ModelAssetIO::vf3 007d53c0 */
/* node sets and bounds the scene unit's ModelAsset clone (007d4d60) needs: FUN_007ddb40 (destination set, source set, flag) and
 * Node_UpdateSubtreeBounds 007dbde0 (the record; also what ModelAsset_RootBounds 007dd450 does before it reads the box) */
void rx_nodeset_clone(rx_addr dst, rx_addr src, int flag);
void rx_model_update_subtree_bounds(rx_addr record);
/* the subtree of a named node copied into a fresh node set (the detachable body parts' models): 007dda20 (ESI = source set; stack
 * destination set, node name; RET 8) with its helpers 007ddfa0 (ECX = source; stack destination, map; RET 8), 007dd890 (ESI = set),
 * 007dea70 (ECX = set, EDI = first record) */
void rx_nodeset_extract_subtree(rx_addr src, rx_addr dst, rx_addr name);
void rx_nodeset_extract(rx_addr src, rx_addr dst, rx_addr map);
void rx_nodeset_reset(rx_addr set);                    /* 007dd890 */
void rx_nodeset_walk_from(rx_addr set, rx_addr start);
#ifdef __cplusplus
}
#endif
