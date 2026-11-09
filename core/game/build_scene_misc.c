/* build_scene_misc.c -- ScrollingTextureEntity (0x1078, 0xac0 bytes), CameraFacingModelEntity (0x1079, 0x150) and WaterEntity (0xf, 0xa0):
 * ModelEntity / Entity classes of the level's scenery. Transliterated from the disassembly.
 * Not built: the render side of the water (WaterSurface_LoadMaterial 007ee240: the surface material) and the water asset itself (the
 * "water" AssetIO has no loader yet: without its data the entity only keeps its handle). */
#include "build_scene_int.h"

/* ScrollingTextureEntity 00410b80 (ESI = entity) */
rx_addr rx_scrolling_texture_ctor(rx_addr e) {
    rx_model_entity_construct(e);
    SU32(e, 0x140, 0x009370cc);
    SU32(e, 0, 0x00939ba4); SU32(e, 0x90, 0x00939c1c); SU32(e, 0x140, 0x00939c5c);
    SU8(e, 0xa90, 0); SU8(e, 0xa91, 0);
    return e;
}
RX_FUNCTION(0x00410b80, rx_scrolling_texture_ctor)
rx_addr rx_scrolling_texture_create(void) { return rx_scrolling_texture_ctor(rx_reflex_aligned_alloc(0xac0)); }

/* ScrollingTextureEntity::vf5 00410c70: the model, the matrix copy, the wrap times (negated when reversed) and the material override
 * "gTextureOffset" (name table +0x150 + 0x40 (n + 0x12), value pointers +0xa10, sizes +0x9d0, flags +0xa50, count byte +0xa91) */
uint32_t rx_scrolling_texture_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    rx_model_instance_init(e + 0xa0);
    rx_entity_vf5(e);
    scene_copy_words(e + 0xc0, e + 0x30, 16);
    SU32(e, 0x70, 1);
    SF32(e, 0xaa0, 0.0f); SF32(e, 0xaa4, 0.0f); SF32(e, 0xab8, 0.0f); SF32(e, 0xabc, 0.0f);
    SF32(e, 0xab0, rs_float("U Seconds For Wrap"));
    SF32(e, 0xab4, rs_float("V Seconds For Wrap"));
    if (rs_bool("Reverse U")) SF32(e, 0xab0, rx_muls(F32(e, 0xab0), F32(0, 0x009cd9e8)));
    if (rs_bool("Reverse V")) SF32(e, 0xab4, rx_muls(F32(e, 0xab4), F32(0, 0x009cd9e8)));
    uint32_t n = U8(e, 0xa91);
    rx_addr dst = e + 0x150 + (n + 0x12) * 0x40;
    const char* nm = "gTextureOffset";
    for (uint32_t k = 0; k < 0x3f; k++) SU8(dst, k, k <= strlen(nm) ? (uint8_t)nm[k] : 0);
    SU8(dst, 0x3f, 0);
    SU32(e, 0xa10 + 4 * U8(e, 0xa91), e + 0xaa0);
    SU32(e, 0x9d0 + 4 * U8(e, 0xa91), 1);
    SU32(e, 0xa50 + 4 * U8(e, 0xa91), 0);
    SU8(e, 0xa91, U8(e, 0xa91) + 1);
    return 0;
}
RX_FUNCTION(0x00410c70, rx_scrolling_texture_vf5)

/* CameraFacingModelEntity 00410f80 */
rx_addr rx_camera_facing_ctor(rx_addr e) {
    rx_model_entity_construct(e);
    SU32(e, 0x140, 0x009370cc);
    SU32(e, 0, 0x00939c64); SU32(e, 0x90, 0x00939cdc); SU32(e, 0x140, 0x00939d1c);
    return e;
}
RX_FUNCTION(0x00410f80, rx_camera_facing_ctor)
rx_addr rx_camera_facing_create(void) { return rx_camera_facing_ctor(rx_reflex_aligned_alloc(0x150)); }
/* vf5 00410fc0: "Axis To Align" +0x144, "StartEnabled" false clears the sort kind and the instance visibility byte */
uint32_t rx_camera_facing_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    rx_model_instance_init(e + 0xa0);
    rx_entity_vf5(e);
    scene_copy_words(e + 0xc0, e + 0x30, 16);
    SU32(e, 0x144, (uint32_t)rs_enum("Axis To Align"));
    if (rs_has("StartEnabled", RS_BOOL) && !rs_bool("StartEnabled")) { SU32(e, 0x70, 0); SU8(e, 0xa2, 0); }
    return 0;
}
RX_FUNCTION(0x00410fc0, rx_camera_facing_vf5)

/* WaterEntity 007e3f40 (EAX = entity): the Entity base, vtable 0x009729dc, +0x94 .. +0x9c 0, sort kind 1 */
rx_addr rx_water_entity_ctor(rx_addr e) {
    rx_entity_construct(e, 0x0095a2e4);
    SU32(e, 0, 0x009729dc);
    SU32(e, 0x94, 0); SU32(e, 0x98, 0); SU32(e, 0x9c, 0);
    SU32(e, 0x70, 1);
    return e;
}
RX_FUNCTION(0x007e3f40, rx_water_entity_ctor)
rx_addr rx_water_entity_create(void) { return rx_water_entity_ctor(rx_reflex_aligned_alloc(0xa0)); }
/* WaterEntity::vf5 007e3fb0: the water asset (+0x90; its data: +0x60 the entity, +0x48c the scene), the flow rate, the scene's receiver
 * list, the collision audio material (+0x94, 0x25 by default) */
uint32_t rx_water_entity_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    rx_entity_vf5(e);
    rx_addr water = rs_asset("WaterAsset");
    SU32(e, 0x90, water);
    rx_addr data = U32(water, 0x44);
    SU32(water, 0x4c, 1);
    rx_addr material = rs_asset("MaterialAsset");
    (void)material;   /* WaterSurface_LoadMaterial 007ee240 (render): not built */
    if (data) {
        SU32(data, 0x48c, U32(e, 0x10));
        SF32(data, 0x60, rs_float("FlowRate"));
    } else if (!getenv("RX_SCENE_ALLOW_UNBUILT")) {
        fprintf(stderr, "rxcore: scene: the water asset is not built (WaterEntity %08x)\n", e); abort();
    }
    SU32(U32(e, 0x90), 0x4c, 0);
    scene_list_push_unique(U32(e, 0x10) + 0x2a68, e);
    if (rs_has("CollisionAudioMaterial", RS_ENUMVALUE)) SU32(e, 0x94, (uint32_t)rs_enum("CollisionAudioMaterial"));
    else SU32(e, 0x94, 0x25);
    return 0;
}
RX_FUNCTION(0x007e3fb0, rx_water_entity_vf5)

/* WaterEntity::vf25 007e41f0 (ECX = entity): the water surface (the asset's IRenderable at data + 0x10) when it has its geometry
 * (data + 0x4a8), else 0. With the water asset not built (no data) the answer is 0. WaterEntity_Reload (007e4090: +0x98 set by the
 * editor's reload) is not ported. */
uint32_t rx_water_entity_vf25(rx_addr e) {
    if (U32(e, 0x98) != 0) { fprintf(stderr, "rxcore: scene: WaterEntity_Reload (007e4090) is not built\n"); abort(); }
    rx_addr h = U32(e, 0x90);
    SU32(h, 0x4c, 1);
    rx_addr data = U32(h, 0x44);
    uint32_t r = (data && U32(data, 0x4a8) != 0) ? data + 0x10 : 0;
    SU32(U32(e, 0x90), 0x4c, 0);
    return r;
}
RX_FUNCTION(0x007e41f0, rx_water_entity_vf25)
