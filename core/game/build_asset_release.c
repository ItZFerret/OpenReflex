/* build_asset_release.c -- the destruction of the asset classes a released package frees (Database vf31 0084af30 / the package
 * unload: rx_database_release_asset, build_asset.c): the asset's AssetIO (vtable slot 2) and the asset's scalar deleting destructor
 * (slot 0, called by AssetIO::vf4 007d3ee0). Reached when a rider or vehicle leaves a running world (the Heat format's activity area
 * re-creation removes the riders that are not in the next round; their packages are unloaded by the package cache). The texture and
 * surface assets are in build_render_asset_texture.c / build_render_asset_surface.c. */
#include "build_asset_release.h"
#include "build_activity_util.h"
#include "anim.h"
#include "fx_util.h"
#include "loop_platform.h"
#include "crt.h"
#include "rx_vcall.h"
#include "race_misc.h"
#include "fx.h"
#include "vehicle_atv_destroy.h"

/* ModelAsset::sdtor 007d4010 (thiscall; flags; RET 4): the model's destructor (007d4030, ESI = the asset), freed with flag 1.
 * Frame: push esi; the calls' return addresses (007d4018 / 007d4025) and ReflexAlignedFree's frame as the original leaves them
 * (the package unload runs this on the database thread: build_render_asset_surface.c); entered at the return slot. */
rx_addr rx_model_asset_sdtor(rx_addr asset, uint32_t flags) {
    const rx_addr E = rx_sp();
    const fx_saved in = fx_save();
    SU32(E, -4, in.esi); SU32(E, -8, 0x007d4018);
    fx_set_reg(FX_ESI, asset);
    rx_set_sp(E - 8);
    rx_model_instance_destroy(asset, asset);
    rx_set_sp(E - 4);
    if (flags & 1) {
        SU32(E, -8, asset); SU32(E, -0xc, 0x007d4025);
        const crt_regs4 r = {fx_reg(FX_EBX), fx_reg(FX_EBP), asset, fx_reg(FX_EDI)};
        rx_reflex_aligned_free_at(E - 0xc, r, asset);
    }
    fx_restore(in);
    rx_set_sp(E);
    return asset;
}
RX_FUNCTION(0x007d4010, rx_model_asset_sdtor)

/* ModelAsset::vf2 007d40b0: the model AssetIO (the static object 0x00da682c, its vtable 0x009728c0 written on the first call: guard
 * 0x00da681c; the startup registration has made it) */
rx_addr rx_model_asset_get_io(rx_addr asset) {
    (void)asset;
    if (!(U8(0, 0x00da681c) & 1)) {
        SU32(0, 0x00da681c, U32(0, 0x00da681c) | 1);
        SU32(0, 0x00da682c, 0x009728c0);
    }
    return 0x00da682c;
}
RX_FUNCTION(0x007d40b0, rx_model_asset_get_io)

/* ---- the other asset classes' vf2 (w-vtunported): Database vf31 (rx_database_release_asset) asks the asset that lost its last reference
 * for its AssetIO through vtable slot 2 and aborts when that slot is not ported. Every one of them is the same 48-byte MSVC function-local
 * static: "mov eax, 1; test [guard], al; jne done; or [guard], eax; push dtor; mov [io], vtable; call _atexit; add esp, 4; done: mov eax,
 * io; ret". Read off the disassembly (each address checked against that exact instruction sequence by a script, the constants taken
 * from it); like ModelAsset::vf2 above the atexit registration is left out (the product never runs the guest's exit list). */
static rx_addr asset_io_static(rx_addr guard, rx_addr io, uint32_t vtable) {
    if (!(U8(0, guard) & 1)) {
        SU32(0, guard, U32(0, guard) | 1);
        SU32(0, io, vtable);
    }
    return io;
}
/* TableDataAsset::vf2 004eef70: the TableDataAssetIO (static object 0x00d72a90, vtable 0x0093a184 written on the first call: guard bit 1 of 0x00d72a94; atexit 0x00911e50 not registered, as ModelAsset::vf2) */
rx_addr rx_table_data_asset_get_io(rx_addr asset) { (void)asset; return asset_io_static(0x00d72a94, 0x00d72a90, 0x0093a184); }
RX_FUNCTION(0x004eef70, rx_table_data_asset_get_io)
/* TDFAsset::vf2 0053a030: the TDFAssetIO (static object 0x00d72a8c, vtable 0x0093a1a0 written on the first call: guard bit 1 of 0x00d72a88; atexit 0x00911e60 not registered, as ModelAsset::vf2) */
rx_addr rx_tdf_asset_get_io(rx_addr asset) { (void)asset; return asset_io_static(0x00d72a88, 0x00d72a8c, 0x0093a1a0); }
RX_FUNCTION(0x0053a030, rx_tdf_asset_get_io)
/* BinkAsset::vf2 005940a0: the BinkAssetIO (static object 0x00d72a80, vtable 0x0093a1bc written on the first call: guard bit 1 of 0x00d72a84; atexit 0x00911e70 not registered, as ModelAsset::vf2) */
rx_addr rx_bink_asset_get_io(rx_addr asset) { (void)asset; return asset_io_static(0x00d72a84, 0x00d72a80, 0x0093a1bc); }
RX_FUNCTION(0x005940a0, rx_bink_asset_get_io)
/* LocalizationAsset::vf2 005c3650: the LocalizationAssetIO (static object 0x00d72aa4, vtable 0x0093a06c written on the first call: guard bit 1 of 0x00d72aa0; atexit 0x00911db0 not registered, as ModelAsset::vf2) */
rx_addr rx_localization_asset_get_io(rx_addr asset) { (void)asset; return asset_io_static(0x00d72aa0, 0x00d72aa4, 0x0093a06c); }
RX_FUNCTION(0x005c3650, rx_localization_asset_get_io)
/* XenonForestAsset::vf2 00661ee0: the XenonForestAssetIO (static object 0x00d72a5c, vtable 0x0093a0c0 written on the first call: guard bit 1 of 0x00d72a70; atexit 0x00911de0 not registered, as ModelAsset::vf2) */
rx_addr rx_xenon_forest_asset_get_io(rx_addr asset) { (void)asset; return asset_io_static(0x00d72a70, 0x00d72a5c, 0x0093a0c0); }
RX_FUNCTION(0x00661ee0, rx_xenon_forest_asset_get_io)
/* XenonGrassAsset::vf2 0066c6f0: the XenonGrassAssetIO (static object 0x00d72a74, vtable 0x0093a088 written on the first call: guard bit 1 of 0x00d72a6c; atexit 0x00911dc0 not registered, as ModelAsset::vf2) */
rx_addr rx_xenon_grass_asset_get_io(rx_addr asset) { (void)asset; return asset_io_static(0x00d72a6c, 0x00d72a74, 0x0093a088); }
RX_FUNCTION(0x0066c6f0, rx_xenon_grass_asset_get_io)
/* XenonTreeAsset::vf2 00672000: the XenonTreeAssetIO (static object 0x00d72a68, vtable 0x0093a0a4 written on the first call: guard bit 1 of 0x00d72a58; atexit 0x00911dd0 not registered, as ModelAsset::vf2) */
rx_addr rx_xenon_tree_asset_get_io(rx_addr asset) { (void)asset; return asset_io_static(0x00d72a58, 0x00d72a68, 0x0093a0a4); }
RX_FUNCTION(0x00672000, rx_xenon_tree_asset_get_io)
/* ParticleAttribAsset::vf2 006937c0: the ParticleAttribAssetIO (static object 0x00d72a54, vtable 0x0093a1d8 written on the first call: guard bit 1 of 0x00d72a50; atexit 0x00911e80 not registered, as ModelAsset::vf2) */
rx_addr rx_particle_attrib_asset_get_io(rx_addr asset) { (void)asset; return asset_io_static(0x00d72a50, 0x00d72a54, 0x0093a1d8); }
RX_FUNCTION(0x006937c0, rx_particle_attrib_asset_get_io)
/* AdvAsset::vf2 006ce7c0: the AdvAssetIO (static object 0x00d72a4c, vtable 0x0093a130 written on the first call: guard bit 1 of 0x00d72a48; atexit 0x00911e20 not registered, as ModelAsset::vf2) */
rx_addr rx_adv_asset_get_io(rx_addr asset) { (void)asset; return asset_io_static(0x00d72a48, 0x00d72a4c, 0x0093a130); }
RX_FUNCTION(0x006ce7c0, rx_adv_asset_get_io)
/* TerrainCollisionAsset::vf2 006d89e0: the TerrainCollisionAssetIO (static object 0x00d72a38, vtable 0x0093a0f8 written on the first call: guard bit 1 of 0x00d72a40; atexit 0x00911e00 not registered, as ModelAsset::vf2) */
rx_addr rx_terrain_collision_asset_get_io(rx_addr asset) { (void)asset; return asset_io_static(0x00d72a40, 0x00d72a38, 0x0093a0f8); }
RX_FUNCTION(0x006d89e0, rx_terrain_collision_asset_get_io)
/* XenonTerrainAsset::vf2 006d9a60: the XenonTerrainAssetIO (static object 0x00d72a3c, vtable 0x0093a114 written on the first call: guard bit 1 of 0x00d72a44; atexit 0x00911e10 not registered, as ModelAsset::vf2) */
rx_addr rx_xenon_terrain_asset_get_io(rx_addr asset) { (void)asset; return asset_io_static(0x00d72a44, 0x00d72a3c, 0x0093a114); }
RX_FUNCTION(0x006d9a60, rx_xenon_terrain_asset_get_io)
/* LuaAsset::vf2 0070ccf0: the LuaAssetIO (static object 0x00d72a9c, vtable 0x0093a14c written on the first call: guard bit 1 of 0x00d72a98; atexit 0x00911e30 not registered, as ModelAsset::vf2) */
rx_addr rx_lua_asset_get_io(rx_addr asset) { (void)asset; return asset_io_static(0x00d72a98, 0x00d72a9c, 0x0093a14c); }
RX_FUNCTION(0x0070ccf0, rx_lua_asset_get_io)
/* XenonMorphAsset::vf2 007a9ab0: the XenonMorphAssetIO (static object 0x00da67dc, vtable 0x00971af0 written on the first call: guard bit 1 of 0x00da67ec; atexit 0x00914060 not registered, as ModelAsset::vf2) */
rx_addr rx_xenon_morph_asset_get_io(rx_addr asset) { (void)asset; return asset_io_static(0x00da67ec, 0x00da67dc, 0x00971af0); }
RX_FUNCTION(0x007a9ab0, rx_xenon_morph_asset_get_io)
/* SubSceneAsset::vf2 007d3e60: the SubSceneAssetIO (static object 0x00da6834, vtable 0x00972854 written on the first call: guard bit 1 of 0x00da6838; atexit 0x00914140 not registered, as ModelAsset::vf2) */
rx_addr rx_sub_scene_asset_get_io(rx_addr asset) { (void)asset; return asset_io_static(0x00da6838, 0x00da6834, 0x00972854); }
RX_FUNCTION(0x007d3e60, rx_sub_scene_asset_get_io)
/* BannerAsset::vf2 007d5550: the BannerAssetIO (static object 0x00da680c, vtable 0x00972918 written on the first call: guard bit 1 of 0x00da6814; atexit 0x00914160 not registered, as ModelAsset::vf2) */
rx_addr rx_banner_asset_get_io(rx_addr asset) { (void)asset; return asset_io_static(0x00da6814, 0x00da680c, 0x00972918); }
RX_FUNCTION(0x007d5550, rx_banner_asset_get_io)
/* ParticleAsset::vf2 007e1a10: the ParticleAssetIO (static object 0x00da6818, vtable 0x00972970 written on the first call: guard bit 1 of 0x00da6820; atexit 0x00914170 not registered, as ModelAsset::vf2) */
rx_addr rx_particle_asset_get_io(rx_addr asset) { (void)asset; return asset_io_static(0x00da6820, 0x00da6818, 0x00972970); }
RX_FUNCTION(0x007e1a10, rx_particle_asset_get_io)
/* XenonWaterAsset::vf2 007f02f0: the XenonWaterAssetIO (static object 0x00da6810, vtable 0x00972a88 written on the first call: guard bit 1 of 0x00da6828; atexit 0x00914180 not registered, as ModelAsset::vf2) */
rx_addr rx_xenon_water_asset_get_io(rx_addr asset) { (void)asset; return asset_io_static(0x00da6828, 0x00da6810, 0x00972a88); }
RX_FUNCTION(0x007f02f0, rx_xenon_water_asset_get_io)
/* PackedDataAsset::vf2 007f86d0: the PackedDataAssetIO (static object 0x00da6824, vtable 0x00972bb8 written on the first call: guard bit 1 of 0x00da6830; atexit 0x00914190 not registered, as ModelAsset::vf2) */
rx_addr rx_packed_data_asset_get_io(rx_addr asset) { (void)asset; return asset_io_static(0x00da6830, 0x00da6824, 0x00972bb8); }
RX_FUNCTION(0x007f86d0, rx_packed_data_asset_get_io)
/* CellAsset::vf2 007fa7a0: the CellAssetIO (static object 0x00da6848, vtable 0x00972d24 written on the first call: guard bit 1 of 0x00da6854; atexit 0x009141a0 not registered, as ModelAsset::vf2) */
rx_addr rx_cell_asset_get_io(rx_addr asset) { (void)asset; return asset_io_static(0x00da6854, 0x00da6848, 0x00972d24); }
RX_FUNCTION(0x007fa7a0, rx_cell_asset_get_io)
/* SoundAsset::vf2 00808640: the SoundAssetIO (static object 0x00da686c, vtable 0x00973bb8 written on the first call: guard bit 1 of 0x00da6860; atexit 0x00914330 not registered, as ModelAsset::vf2) */
rx_addr rx_sound_asset_get_io(rx_addr asset) { (void)asset; return asset_io_static(0x00da6860, 0x00da686c, 0x00973bb8); }
RX_FUNCTION(0x00808640, rx_sound_asset_get_io)
/* SoundBankAsset::vf2 00808980: the SoundBankAssetIO (static object 0x00da6864, vtable 0x00973bd4 written on the first call: guard bit 1 of 0x00da685c; atexit 0x00914340 not registered, as ModelAsset::vf2) */
rx_addr rx_sound_bank_asset_get_io(rx_addr asset) { (void)asset; return asset_io_static(0x00da685c, 0x00da6864, 0x00973bd4); }
RX_FUNCTION(0x00808980, rx_sound_bank_asset_get_io)
/* MilesSoundDatAsset::vf2 00808af0: the MilesSoundDatAssetIO (static object 0x00da6868, vtable 0x00973bf0 written on the first call: guard bit 1 of 0x00da6858; atexit 0x00914350 not registered, as ModelAsset::vf2) */
rx_addr rx_miles_sound_dat_asset_get_io(rx_addr asset) { (void)asset; return asset_io_static(0x00da6858, 0x00da6868, 0x00973bf0); }
RX_FUNCTION(0x00808af0, rx_miles_sound_dat_asset_get_io)
/* CollisionAsset::vf2 00818ec0: the CollisionAssetIO (static object 0x00da6884, vtable 0x00974f38 written on the first call: guard bit 1 of 0x00da6880; atexit 0x00914d20 not registered, as ModelAsset::vf2) */
rx_addr rx_collision_asset_get_io(rx_addr asset) { (void)asset; return asset_io_static(0x00da6880, 0x00da6884, 0x00974f38); }
RX_FUNCTION(0x00818ec0, rx_collision_asset_get_io)
/* ActionAsset::vf2 0082af80: the ActionAssetIO (static object 0x00da68ac, vtable 0x00975e50 written on the first call: guard bit 1 of 0x00da68b0; atexit 0x00914da0 not registered, as ModelAsset::vf2) */
rx_addr rx_action_asset_get_io(rx_addr asset) { (void)asset; return asset_io_static(0x00da68b0, 0x00da68ac, 0x00975e50); }
RX_FUNCTION(0x0082af80, rx_action_asset_get_io)
/* AnimAsset::vf2 0082bd70: the AnimAssetIO (static object 0x00da68a4, vtable 0x00975ecc written on the first call: guard bit 1 of 0x00da689c; atexit 0x00914dd0 not registered, as ModelAsset::vf2) */
rx_addr rx_anim_asset_get_io(rx_addr asset) { (void)asset; return asset_io_static(0x00da689c, 0x00da68a4, 0x00975ecc); }
RX_FUNCTION(0x0082bd70, rx_anim_asset_get_io)
/* PerformanceAsset::vf2 0083f600: the PerformanceAssetIO (static object 0x00da68a0, vtable 0x00976204 written on the first call: guard bit 1 of 0x00da68a8; atexit 0x00914e60 not registered, as ModelAsset::vf2) */
rx_addr rx_performance_asset_get_io(rx_addr asset) { (void)asset; return asset_io_static(0x00da68a8, 0x00da68a0, 0x00976204); }
RX_FUNCTION(0x0083f600, rx_performance_asset_get_io)

/* ---- the scalar deleting destructors of the part / gear package assets (w-vtunported): AssetIO::vf4 (rx_asset_io_destroy) deletes an asset
 * through its slot 0 and aborts when it is not ported. A vehicle part or rider gear package the package cache unloads (build_vehicle_pkgcache.c,
 * 500 ms after its last user) frees these. Read off the disassembly; thiscall (flags; RET 4) -> EAX = the object. */

/* ReflexAlignedFree 0078fc50 (cdecl) called at the current ESP, its argument in the slot the original pushes */
static void asset_free(rx_addr p) {
    ACT_CALL(1, (SU32(rx_sp(), 4, p), rx_reflex_aligned_free(p)));
}

/* operator delete as the compiler inlined it: _aligned_free (IAT 0x916350) between the allocator lock's enter / leave while [0xda67d8] is set */
static void asset_delete_locked(rx_addr p) {
    if (U8(0, 0x00da67d8) != 0) loop_enter_cs(0x00d61704);
    ACT_CALL(1, (SU32(rx_sp(), 4, p), crt_aligned_free(p)));
    if (U8(0, 0x00da67d8) != 0) loop_leave_cs(0x00d61704);
}

/* IAsset::vf0 007d5530 (TableDataAsset's, the interfaces' own): the IAsset vtable 0x0093eb78, freed with flag 1 */
rx_addr rx_iasset_sdtor(rx_addr a, uint32_t flags) {
    SU32(a, 0, 0x0093eb78);
    if (flags & 1) asset_free(a);
    return a;
}
RX_FUNCTION(0x007d5530, rx_iasset_sdtor)

/* IAssetIO::vf0 007f84b0 (the AssetIO base): the IAssetIO vtable 0x0093a038, freed with flag 1 */
rx_addr rx_iasset_io_sdtor(rx_addr io, uint32_t flags) {
    SU32(io, 0, 0x0093a038);
    if (flags & 1) asset_free(io);
    return io;
}
RX_FUNCTION(0x007f84b0, rx_iasset_io_sdtor)

/* PerformanceAsset::vf0 0083f630: its data block (+0x18, read before the vtable 0x00975fec is stored) freed, then the IAsset vtable
 * 0x0093eb78; the object freed with flag 1 */
rx_addr rx_performance_asset_sdtor(rx_addr a, uint32_t flags) {
    const rx_addr data = U32(a, 0x18);
    SU32(a, 0, 0x00975fec);
    asset_free(data);
    SU32(a, 0, 0x0093eb78);
    if (flags & 1) asset_free(a);
    return a;
}
RX_FUNCTION(0x0083f630, rx_performance_asset_sdtor)

/* CollisionAsset::vf0 00818e80: the vtable 0x00974f28, its collision object (+0x10) deleted through that object's slot 0 (flag 1), the
 * IAsset vtable 0x0093eb78; the asset freed with flag 1 */
rx_addr rx_collision_asset_sdtor(rx_addr a, uint32_t flags) {
    const rx_addr c = U32(a, 0x10);
    SU32(a, 0, 0x00974f28);
    if (c != 0) { const uint32_t one = 1; (void)rx_vcall(c, 0, 1, &one); }
    SU32(a, 0, 0x0093eb78);
    if (flags & 1) asset_free(a);
    return a;
}
RX_FUNCTION(0x00818e80, rx_collision_asset_sdtor)

/* PackedDataAsset::sdtor 007f8590 (its body 007f85b0, stdcall asset; RET 4): the vtable 0x00972ba4, the four data blocks +0x88 .. +0x94
 * deleted (operator delete under the allocator lock) and cleared, the IAsset vtable 0x0093eb78; the object freed with flag 1 */
static void packed_data_asset_dtor(rx_addr a) {
    SU32(a, 0, 0x00972ba4);
    for (uint32_t k = 0; k < 4; k++) {
        const rx_addr p = U32(a, 0x88 + 4 * k);
        if (p == 0) continue;
        asset_delete_locked(p);
        SU32(a, 0x88 + 4 * k, 0);
    }
    SU32(a, 0, 0x0093eb78);
}
rx_addr rx_packed_data_asset_sdtor(rx_addr a, uint32_t flags) {
    packed_data_asset_dtor(a);
    if (flags & 1) asset_free(a);
    return a;
}
RX_FUNCTION(0x007f8590, rx_packed_data_asset_sdtor)

/* ---- the collision geometries a released CollisionAsset deletes (its +0x10 through slot 0, flag 1; the SplitTriangles kind deletes its
 * children the same way). Read off the disassembly (w-vtunported); the Composite kind below (w-vtunported2). */

/* Collision::CollisionGeometry::vf0 00767ec0: the base vtable 0x0096dd50; ReflexAlignedFree with flag 1 */
rx_addr rx_collision_geometry_sdtor(rx_addr g, uint32_t flags) {
    SU32(g, 0, 0x0096dd50);
    if (flags & 1) asset_free(g);
    return g;
}
RX_FUNCTION(0x00767ec0, rx_collision_geometry_sdtor)

/* Collision::RCollisionGeometryBox::sdtor 00818c00: the base vtable 0x0096dd50; operator delete (under the allocator lock) with flag 1 */
rx_addr rx_collision_geometry_box_sdtor(rx_addr g, uint32_t flags) {
    SU32(g, 0, 0x0096dd50);
    if (flags & 1) asset_delete_locked(g);
    return g;
}
RX_FUNCTION(0x00818c00, rx_collision_geometry_box_sdtor)

/* the Rays / Triangles kinds' destructor body (00818a30 / 00818b30, ESI = the geometry): the kind's vtable, its data block (+0xc, read
 * first) deleted under the allocator lock, the base vtable 0x0096dd50 */
static void geometry_data_dtor(rx_addr g, uint32_t vtable) {
    const rx_addr data = U32(g, 0xc);
    SU32(g, 0, vtable);
    asset_delete_locked(data);
    SU32(g, 0, 0x0096dd50);
}

/* Collision::RCollisionGeometryRays::sdtor 00818a80 (body 00818a30, vtable 0x00974f54); ReflexAlignedFree with flag 1 */
rx_addr rx_collision_geometry_rays_sdtor(rx_addr g, uint32_t flags) {
    geometry_data_dtor(g, 0x00974f54);
    if (flags & 1) asset_free(g);
    return g;
}
RX_FUNCTION(0x00818a80, rx_collision_geometry_rays_sdtor)

/* Collision::RCollisionGeometryTriangles::sdtor 00818b90 (body 00818b30, vtable 0x00974f5c); ReflexAlignedFree with flag 1 */
rx_addr rx_collision_geometry_triangles_sdtor(rx_addr g, uint32_t flags) {
    geometry_data_dtor(g, 0x00974f5c);
    if (flags & 1) asset_free(g);
    return g;
}
RX_FUNCTION(0x00818b90, rx_collision_geometry_triangles_sdtor)

/* Collision::RCollisionGeometrySplitTriangles::sdtor 00818e60 (body 00818de0, ESI = the geometry): the vtable 0x00974f7c, each of the +0x14
 * children (the array +0x10) deleted through its slot 0 (flag 1), the array freed (ReflexAlignedFree), the data block +0xc deleted under the
 * allocator lock, the base vtable 0x0096dd50; ReflexAlignedFree with flag 1 */
rx_addr rx_collision_geometry_split_sdtor(rx_addr g, uint32_t flags) {
    SU32(g, 0, 0x00974f7c);
    for (int32_t i = 0; i < I32(g, 0x14); i++) {
        const rx_addr c = U32(U32(g, 0x10), 4u * (uint32_t)i);
        if (c != 0) { const uint32_t one = 1; (void)rx_vcall(c, 0, 1, &one); }
    }
    asset_free(U32(g, 0x10));
    asset_delete_locked(U32(g, 0xc));
    SU32(g, 0, 0x0096dd50);
    if (flags & 1) asset_free(g);
    return g;
}
RX_FUNCTION(0x00818e60, rx_collision_geometry_split_sdtor)

/* ---- w-vtunported2: the rest of the package-unload destructors (Database vf31 -> AssetIO::vf4 -> the asset's slot 0) ---------------- */

/* Collision::RCollisionGeometryComposite::sdtor 00819dc0 (body 00819ea0, ESI = the geometry): the vtable 0x00974f74; each child of the
 * +0x48 vector (count re-read per child; the bounds check never fails) deleted through its slot 0 (flag 1); the node vectors +0x84 / +0x6c /
 * +0x54 destroyed (0081f6e0); the +0x48 block freed when allocated, then its proxy +0x3c (after +0x48 .. +0x50 = 0); the +0x30 block and its
 * proxy +0x24 the same way; the +0x18 block deallocated (allocator::deallocate = operator delete, IAT 0x916198) when allocated, its words
 * cleared and its proxy +0xc freed; the base vtable 0x0096dd50. ReflexAlignedFree with flag 1. */
rx_addr rx_collision_geometry_composite_sdtor(rx_addr g, uint32_t flags) {
    SU32(g, 0, 0x00974f74);
    for (uint32_t i = 0; i < ((U32(g, 0x4c) - U32(g, 0x48)) >> 2); i++) {
        const rx_addr c = U32(U32(g, 0x48), 4 * i);
        if (c != 0) { const uint32_t one = 1; (void)rx_vcall(c, 0, 1, &one); }
    }
    ACT_CALL(0, rx_vehicle_sim_vector_destruct(g + 0x84));
    ACT_CALL(0, rx_vehicle_sim_vector_destruct(g + 0x6c));
    ACT_CALL(0, rx_vehicle_sim_vector_destruct(g + 0x54));
    if (U32(g, 0x48) != 0) asset_free(U32(g, 0x48));
    SU32(g, 0x48, 0); SU32(g, 0x4c, 0); SU32(g, 0x50, 0);
    asset_free(U32(g, 0x3c));
    if (U32(g, 0x30) != 0) asset_free(U32(g, 0x30));
    SU32(g, 0x30, 0); SU32(g, 0x34, 0); SU32(g, 0x38, 0);
    asset_free(U32(g, 0x24));
    if (U32(g, 0x18) != 0) ACT_CALL(2, crt_operator_delete(U32(g, 0x18)));
    SU32(g, 0x18, 0); SU32(g, 0x1c, 0); SU32(g, 0x20, 0);
    asset_free(U32(g, 0xc));
    SU32(g, 0, 0x0096dd50);
    if (flags & 1) asset_free(g);
    return g;
}
RX_FUNCTION(0x00819dc0, rx_collision_geometry_composite_sdtor)

/* 00839ec0 / 00839f00 (thiscall element; the `vector destructor iterator' 00402500 calls them on an AnimAsset's arrays): the element's
 * block (+0 / +0x10) deleted under the allocator lock when allocated */
void rx_anim_track_free0(rx_addr e) { if (U32(e, 0) != 0) asset_delete_locked(U32(e, 0)); }
RX_FUNCTION(0x00839ec0, rx_anim_track_free0)
void rx_anim_track_free10(rx_addr e) { if (U32(e, 0x10) != 0) asset_delete_locked(U32(e, 0x10)); }
RX_FUNCTION(0x00839f00, rx_anim_track_free10)

/* an AnimAsset's counted array (the element count in the word before it): each element destroyed last to first (00402500), the block
 * (from the count word) freed */
static void anim_array_free(rx_addr a, uint32_t size, rx_addr dtor) {
    if (a == 0) return;
    ACT_CALL(4, rx_fx_vector_dtor_iterator(a, size, I32(a, -4), dtor));
    asset_free(a - 4);
}
/* 0083ffd0 (ESI = the block): its +0x3c buffer deleted under the allocator lock; +8 / +0xc / +0x24 / +0x28 / +0x3c / +0x30 / +0x2c / +0x1c /
 * +4 = 0, +0x14 / +0x10 = 0.0 */
static void anim_extra_clear(rx_addr b) {
    if (U32(b, 0x3c) != 0) asset_delete_locked(U32(b, 0x3c));
    static const uint8_t z[] = {8, 0xc, 0x24, 0x28, 0x3c, 0x30, 0x2c, 0x1c, 4};
    for (size_t i = 0; i < sizeof z; i++) SU32(b, z[i], 0);
    SF32(b, 0x14, 0.0f); SF32(b, 0x10, 0.0f);
}
/* AnimAsset::sdtor 0082bbf0 (body 0082bc10, EDI = the asset): the vtable 0x00975ebc, the +0x14 block freed; a shared clip block (+0x58:
 * +0x5c = 0, deleted under the lock) or else the two track arrays +0x1c (0x14-byte elements) and +0x24 (0x18-byte elements, both 00839ec0);
 * the arrays +0x2c / +0x34 / +0x38 / +0x40 (0x14-byte elements, 00839f00); the extra block +0x64 (0083ffd0, freed, +0x64 = 0); the IAsset
 * vtable 0x0093eb78. ReflexAlignedFree with flag 1. */
rx_addr rx_anim_asset_sdtor(rx_addr a, uint32_t flags) {
    const rx_addr data = U32(a, 0x14);
    SU32(a, 0, 0x00975ebc);
    asset_free(data);
    const rx_addr shared = U32(a, 0x58);
    if (shared != 0) {
        SU32(a, 0x5c, 0);
        asset_delete_locked(shared);
    } else {
        anim_array_free(U32(a, 0x1c), 0x14, 0x00839ec0);
        anim_array_free(U32(a, 0x24), 0x18, 0x00839ec0);
    }
    anim_array_free(U32(a, 0x2c), 0x14, 0x00839f00);
    anim_array_free(U32(a, 0x34), 0x14, 0x00839f00);
    anim_array_free(U32(a, 0x38), 0x14, 0x00839f00);
    anim_array_free(U32(a, 0x40), 0x14, 0x00839f00);
    const rx_addr extra = U32(a, 0x64);
    if (extra != 0) {
        anim_extra_clear(extra);
        asset_free(extra);
        SU32(a, 0x64, 0);
    }
    SU32(a, 0, 0x0093eb78);
    if (flags & 1) asset_free(a);
    return a;
}
RX_FUNCTION(0x0082bbf0, rx_anim_asset_sdtor)

/* 004eed70 (stdcall table): the table leaves the loaded-table list [0xa89cfc] (+4 the array, +8 the count): the first slot holding it gets
 * the last entry and the count drops -> EAX = its index (the count when absent) */
uint32_t rx_table_list_remove(rx_addr table) {
    const rx_addr list = U32(0, 0x00a89cfc);
    const int32_t n = I32(list, 8);
    int32_t i = 0;
    if (n <= 0) return 0;
    const rx_addr arr = U32(list, 4);
    for (; i < n; i++) if (U32(arr, 4u * (uint32_t)i) == table) break;
    if (i == n) return (uint32_t)i;
    SU32(arr, 4u * (uint32_t)i, U32(arr, 4u * (uint32_t)(n - 1)));
    SU32(list, 8, U32(list, 8) - 1);
    return (uint32_t)i;
}
RX_FUNCTION(0x004eed70, rx_table_list_remove)

/* TableDataAssetIO::vf4 004f01b0 (thiscall io, asset; RET 4): a TableDataAsset (__RTDynamicCast from IAsset 0x9ccfd4 to TableDataAsset
 * 0x9ccfec) leaves the loaded-table list (004eed70); the asset is deleted through its slot 0 (flag 1) */
void rx_table_data_asset_io_destroy(rx_addr io, rx_addr asset) {
    (void)io;
    const rx_addr t = crt_rtdynamiccast(asset, 0, 0x009ccfd4, 0x009ccfec, 0);
    if (t != 0) ACT_CALL(1, (void)rx_table_list_remove(t));
    if (asset != 0) { const uint32_t one = 1; (void)rx_vcall(asset, 0, 1, &one); }
}
RX_FUNCTION(0x004f01b0, rx_table_data_asset_io_destroy)

/* BXMLAssetIO::vf4 00822250 (thiscall io, asset; RET 4): the asset pointer is the BXMLAsset's IAsset sub-object at +0x460; the whole object
 * (asset - 0x460, when not null) is deleted through its slot 0 (flag 1: a tail jump with the argument slot rewritten) */
void rx_bxml_asset_io_destroy(rx_addr io, rx_addr asset) {
    (void)io;
    if (asset == 0 || asset - 0x460 == 0) return;
    const uint32_t one = 1;
    (void)rx_vcall(asset - 0x460, 0, 1, &one);
}
RX_FUNCTION(0x00822250, rx_bxml_asset_io_destroy)

/* XMLReader's destructor 00824fe0 (ESI = the reader), complete: the live-reader count [0xa95950] - 1, the vtable 0x009753dc; without an
 * allocator (+0x45c) the five blocks +8 / +0xc / +0x14 / +0x1c / +0x24 deleted under the allocator lock (null or not) and the string hash
 * (+0x10: 007fa1e0, then freed); with one only the hash's contents (007fa1e0). Then the members: the FileInputStream +0x340 (vtable
 * 0x00974c78; an open CRT file is closed: the product never opens one, it reads through fmt_vfs; vtable 0x0094dab4), the ZLibCompression
 * +0x2f8 (vtable 0x00975368; a live inflate / deflate state, mode +0x2fc 1 / 2: only the guest-stream loader of the tuning reload starts
 * one, never the product; vtable 0x00975334) and the CompressedInputStream +0x2c8 (0080f8a0: vtable 0x00974b48; with a bound source
 * (+4, the same loader) its decompressor's end and chunk frees; vtable 0x0094dab4); the base vtable 0x009751ec. */
void rx_xml_reader_dtor(rx_addr r) {
    SU32(0, 0x00a95950, U32(0, 0x00a95950) - 1);
    const uint32_t allocator = U32(r, 0x45c);
    SU32(r, 0, 0x009753dc);
    if (allocator == 0) {
        static const uint8_t blocks[5] = {8, 0xc, 0x14, 0x1c, 0x24};
        for (int k = 0; k < 5; k++) asset_delete_locked(U32(r, blocks[k]));
        const rx_addr t = U32(r, 0x10);
        if (t != 0) { ACT_CALL(0, rx_name_hash_free(t)); asset_free(t); }
    } else if (U32(r, 0x10) != 0) {
        ACT_CALL(0, rx_name_hash_free(U32(r, 0x10)));
    }
    const uint32_t file = U32(r, 0x344);
    SU32(r, 0x340, 0x00974c78);
    if (file != 0 && U32(r, 0x348) != 0) act_unreached(0x00825135, "an XMLReader with an open CRT file (fclose): the product reads files through fmt_vfs");
    SU32(r, 0x340, 0x0094dab4);
    const uint32_t mode = U32(r, 0x2fc);
    SU32(r, 0x2f8, 0x00975368);
    if (mode == 1 || mode == 2) act_unreached(mode == 1 ? 0x008ed680 : 0x008ec710, "an XMLReader's live zlib state (only the tuning-reload loader starts one)");
    SU32(r, 0x2f8, 0x00975334);
    SU32(r, 0x2c8, 0x00974b48);
    if (U32(r, 0x2c8 + 4) != 0) act_unreached(0x0080f8b5, "an XMLReader's CompressedInputStream with a bound source (only the tuning-reload loader binds one)");
    SU32(r, 0x2c8, 0x0094dab4);
    SU32(r, 0, 0x009751ec);
}
RX_FUNCTION(0x00824fe0, rx_xml_reader_dtor)

/* BXMLAsset::sdtor 00822170 (thiscall object, flags; RET 4): the IAsset sub-object's vtable (+0x460 = 0x0093eb78), the XMLReader destructor
 * (00824fe0), ReflexAlignedFree with flag 1 */
rx_addr rx_bxml_asset_sdtor(rx_addr o, uint32_t flags) {
    SU32(o, 0x460, 0x0093eb78);
    ACT_CALL(0, rx_xml_reader_dtor(o));
    if (flags & 1) asset_free(o);
    return o;
}
RX_FUNCTION(0x00822170, rx_bxml_asset_sdtor)

