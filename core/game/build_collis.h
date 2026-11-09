/* build_collis.h -- construction unit "asset_collis" (2c): CollisionAsset objects from "collis" assets.
 *
 * CollisionAsset (vtable 0x00974f28, 0x14 bytes): +8 / +0xc zeroed (the Database sets +4 serial, +8 references, +0xc package), +0x10 the
 * collision geometry built by FUN_0081b460 from the stream. The file: u32 1234, "col\0", u32 version (1..3), u32 kind, then (version >= 2)
 * a u32 whose low byte becomes the geometry's flag at +8, then the kind's body. Every geometry has +0 vtable, +4 its kind number, +8 the
 * flag byte. Kinds the install uses: 22 triangle groups (FUN_0081b090, "SplitTriangles"), 15 composite of primitives (FUN_0081ac90),
 * 2 boxes list (FUN_00819f90), 11 oriented box (FUN_0081a790), 16 sphere (FUN_0081a740); kinds 0 (FUN_0081a1e0) and 17 (cylinder,
 * FUN_0081aaf0, used inside composites) -- kind 0 is not in any file and aborts. */
#pragma once
#include "rx_game.h"
#include "build_stream.h"
#ifdef __cplusplus
extern "C" {
#endif
/* CollisionAssetIO::vf3 00818ef0: the asset (0 when the header test fails the geometry pointer is 0 but the asset exists) */
rx_addr rx_collision_asset_io_create(rx_stream* s);
/* FUN_0081b460 (EAX = stream): the geometry, 0 when the file header does not match */
rx_addr rx_collision_geometry_load(rx_stream* s);
#ifdef __cplusplus
}
#endif
