/* build_scene_simple.c -- the small Entity-derived classes of the scene (Entity base only, no model): AnchorEntity, GeneralStorageEntity,
 * DistanceFogEntity, ShadowMapEntity, HDRSettingsEntity, OverlayColorEntity, MudManager, TDFEntity, LevelInfoEntity, MusicTrackListEntity,
 * and the other settings entities. Each class: the constructor the factory FUN_004046a0 calls (allocation + FUN_004xxxxx) and the
 * vf5 (RSData loader) / vf7 (post-load) the scene loader runs. Hand transliteration from the disassembly (addresses in the comments);
 * single-precision SSE arithmetic in the original's order; x87 `fld / fstp` dword copies quiet a signalling NaN. */
#include "build_scene_int.h"

#define x87c scene_x87c
static void copy_f32(rx_addr dst, uint32_t doff, rx_addr src, uint32_t soff, uint32_t n) {   /* n x (fld dword; fstp dword) */
    for (uint32_t k = 0; k < n; k++) SU32(dst, doff + 4 * k, x87c(U32(src, soff + 4 * k)));
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * AnchorEntity (0x90 bytes): the factory: ReflexAlignedAlloc(0x90) + 00403e90; vf22 00403ee0 = 0x101a. Entity only. */
rx_addr rx_anchor_entity_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0x90);
    rx_entity_construct(e, 0x00937944);
    return e;
}

/* GeneralStorageEntity (0x90): 00406040, vtable 0x00938874 */
rx_addr rx_general_storage_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0x90);
    rx_entity_construct(e, 0x00938874);
    return e;
}

/* DistanceFogEntity (0xd0): 00405ff0, vtable 0x009373c4; vf5 007daed0 */
rx_addr rx_distance_fog_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0xd0);
    rx_entity_construct(e, 0x009373c4);
    return e;
}
uint32_t rx_distance_fog_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    rx_entity_vf5(e);
    scene_rs_color_to("ColorStart", e + 0xb0);
    SF32(e, 0xbc, rs_float("AmountStart"));
    scene_rs_color_to("ColorEnd", e + 0xc0);
    SF32(e, 0xcc, rs_float("AmountEnd"));
    SF32(e, 0x90, rs_float("DistanceStart"));
    SF32(e, 0x94, rs_float("DistanceEnd"));
    if (rs_has("HeightStartDistance", RS_FLOAT)) {
        SF32(e, 0x98, rs_float("HeightStartDistance"));
        SF32(e, 0x9c, rs_float("HeightEndDistance"));
        SF32(e, 0xa0, rs_float("HeightStart"));
        SF32(e, 0xa4, rs_float("HeightEnd"));
    } else {
        SF32(e, 0x98, 0.0f); SF32(e, 0x9c, 1.0f /* 0x931f24 */); SF32(e, 0xa0, 0.0f); SF32(e, 0xa4, 0.0f);
    }
    return 0;
}
RX_FUNCTION(0x007daed0, rx_distance_fog_vf5)

/* ShadowMapEntity (0xa0): 00406c70, vtable 0x009398cc; vf5 0040acf0, vf7 0040ad40 */
rx_addr rx_shadow_map_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0xa0);
    rx_entity_construct(e, 0x009398cc);
    return e;
}
uint32_t rx_shadow_map_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    rx_entity_vf5(e);
    SU32(e, 0x90, 0);
    SU32(e, 0x94, rs_asset("StaticShadowMapAsset"));
    SU32(e, 0x98, rs_asset("VolumeShadowMapAsset"));
    return 0;
}
RX_FUNCTION(0x0040acf0, rx_shadow_map_vf5)
/* vf7: the scene's volume shadow map handle (scene + 0x2d10) */
uint32_t rx_shadow_map_vf7(rx_addr e) { SU32(U32(e, 0x10), 0x2d10, U32(e, 0x98)); return 0; }
RX_FUNCTION(0x0040ad40, rx_shadow_map_vf7)

/* HDRSettingsEntity (0xc0): 00404350, vtable 0x00939794 (+0xb0 = 0); vf5 0040c7b0. The first entity (+0xb0 = 0 on entry) only sets the
 * flag; the second one would also publish its values in the renderer's globals. */
rx_addr rx_hdr_settings_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0xc0);
    rx_entity_construct(e, 0x00939794);
    SU32(e, 0xb0, 0);
    return e;
}
uint32_t rx_hdr_settings_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    rx_entity_vf5(e);
    SF32(e, 0x90, rs_float("Bloom Attenuation"));
    SF32(e, 0x94, rs_float("HDR Adaptation Rate"));
    SF32(e, 0x98, rs_float("Max Camera Adaptation"));
    SF32(e, 0x9c, rs_float("Min Camera Adaptation"));
    SF32(e, 0xa0, rs_float("Max Bloom Luminance"));
    SF32(e, 0xa4, rs_float("Min Bloom Luminance"));
    SF32(e, 0xac, rs_float("Key Value"));
    SF32(e, 0xa8, rs_float("WholeSceneBloomAmount"));
    if (U32(e, 0xb0) != 0) {   /* a later HDR entity overwrites the renderer's HDR globals */
        SU32(0, 0x00a8139c, U32(e, 0x90)); SU32(0, 0x00a7fb8c, U32(e, 0x94)); SU32(0, 0x00a813a4, U32(e, 0x98));
        SU32(0, 0x00a81390, U32(e, 0x9c)); SU32(0, 0x00a7f780, U32(e, 0xa0)); SU32(0, 0x00a81398, U32(e, 0xa4));
        SU32(0, 0x00a7fb88, U32(e, 0xac)); SU32(0, 0x00a813a0, U32(e, 0xa8));
    }
    SU32(e, 0xb0, 1);
    return 0;
}
RX_FUNCTION(0x0040c7b0, rx_hdr_settings_vf5)

/* OverlayColorEntity (0x1a0): 004042e0, vtable 0x0093971c (+0x108 = 0); vf5 0040b240. ApplyToScene (0040b490: the renderer's overlay
 * colours, render state) is not built. */
rx_addr rx_overlay_color_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0x1a0);
    rx_entity_construct(e, 0x0093971c);
    SU32(e, 0x108, 0);
    return e;
}
uint32_t rx_overlay_color_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    SU32(e, 0x70, 1);
    rx_entity_vf5(e);
    for (uint32_t k = 0x90; k <= 0xbc; k += 4) SF32(e, k, 0.0f);
    SU32(e, 0xc0, 0x3f2aaaabu /* 0x977714 */); SU32(e, 0xc4, 0x3eaaaaabu /* 0x92e214 */);
    if (rs_has("OverlayColorTop", RS_COLOR)) scene_rs_color_to("OverlayColorTop", e + 0x90);
    if (rs_has("OverlayColorMiddle", RS_COLOR)) scene_rs_color_to("OverlayColorMiddle", e + 0xa0);
    if (rs_has("OverlayColorBottom", RS_COLOR)) scene_rs_color_to("OverlayColorBottom", e + 0xb0);
    if (rs_has("OverlayTopMidpoint", RS_FLOAT)) SF32(e, 0xc0, rs_float("OverlayTopMidpoint"));
    if (rs_has("OverlayBottomMidpoint", RS_FLOAT)) SF32(e, 0xc4, rs_float("OverlayBottomMidpoint"));
    if (F32(e, 0xc4) > F32(e, 0xc0)) { uint32_t t = U32(e, 0xc0); SU32(e, 0xc0, U32(e, 0xc4)); SU32(e, 0xc4, t); }
    copy_f32(e, 0xd0, e, 0x90, 4); copy_f32(e, 0xe0, e, 0xa0, 4); copy_f32(e, 0xf0, e, 0xb0, 4); copy_f32(e, 0x100, e, 0xc0, 2);
    /* if (+0x108) OverlayColor_ApplyToScene (render state, not built) */
    SU32(e, 0x10c, 0);
    SU32(e, 0x108, 1);
    return 0;
}
RX_FUNCTION(0x0040b240, rx_overlay_color_vf5)

/* MudManager (0x130): 00408700, vtable 0x00937a34; vf5 004087b0. The constructor's fields +0x90.. are the default mud colour and rate */
rx_addr rx_mud_manager_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0x130);
    rx_entity_construct(e, 0x00937a34);
    SU32(e, 0x90, 0x3e830165u /* 0x9777c0 */); SU32(e, 0x94, 0x3e2eac86u /* 0x9777bc */); SU32(e, 0x98, 0x3df0ed3eu /* 0x9777b8 */);
    SF32(e, 0x9c, 1.0f /* 0x931f24 */);
    SU32(e, 0xa0, 0x3dcccccdu /* 0x931d94 */);
    return e;
}
uint32_t rx_mud_manager_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    rx_entity_vf5(e);
    float k = rx_bf(0x3b808081u) /* 0x009775a0 = 1/255 */;
    SF32(e, 0x90, rs_float("MudR")); SF32(e, 0x94, rs_float("MudG")); SF32(e, 0x98, rs_float("MudB"));
    float a = rs_float("MudA");
    SF32(e, 0x90, F32(e, 0x90) * k); SF32(e, 0x94, F32(e, 0x94) * k); SF32(e, 0x98, F32(e, 0x98) * k);
    SF32(e, 0x9c, a * k);
    float rate = rs_float("WaterCleanRate");
    rate = rate * F32(0, 0x009cd9b4);
    SF32(e, 0xa0, rate);
    SF32(e, 0xa4, rate * 2.0f /* 0x92e330 */);
    for (uint32_t i = 0; i < 16; i++) {
        char nm[64];
        snprintf(nm, sizeof nm, "MaxMud%u", i); SF32(e, 0xa8 + 4 * i, rs_float(nm));
        snprintf(nm, sizeof nm, "MudRate%u", i); SF32(e, 0xe8 + 4 * i, rs_float(nm));
        snprintf(nm, sizeof nm, "Clean%u", i);
        if (rs_bool(nm)) SF32(e, 0xe8 + 4 * i, F32(e, 0xe8 + 4 * i) * F32(0, 0x009cd9e8) /* -1.0 */);
    }
    return 0;
}
RX_FUNCTION(0x004087b0, rx_mud_manager_vf5)

/* TDFEntity (0xb0): 00406d60, vtable 0x00939ab4, +0x90 list {data (25 x 4 bytes), 0, 25, 25, 0x7fffffff}; vf5 0053a140 */
rx_addr rx_tdf_entity_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0xb0);
    rx_entity_construct(e, 0x00939ab4);
    rx_addr data = rx_reflex_aligned_alloc(0x19 * 4);
    SU32(e, 0x98, 0x19); SU32(e, 0x9c, 0x19); SU32(e, 0x90, data); SU32(e, 0x94, 0); SU32(e, 0xa0, 0x7fffffff);
    return e;
}
uint32_t rx_tdf_entity_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    rx_entity_vf5(e);
    scene_rs_asset_list("TDFAssetList", e + 0x90);
    return 0;
}
RX_FUNCTION(0x0053a140, rx_tdf_entity_vf5)

/* LevelInfoEntity (0xa0): 0041b240, vtable 0x00939fb4, +0x90 = 0, +0x94 = 1; vf5 0041b2a0 (no Entity::vf5: name and matrix stay unread) */
rx_addr rx_level_info_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0xa0);
    rx_entity_construct(e, 0x00939fb4);
    SU32(e, 0x90, 0); SU32(e, 0x94, 1); SU32(e, 0x70, 0);
    return e;
}
uint32_t rx_level_info_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    int32_t wt = rs_int("WorldType");
    if (wt >= 0 && wt < 2) SU32(e, 0x90, wt);
    if (rs_has("CameraFlashesEnabled", RS_BOOL)) SU32(e, 0x94, rs_bool("CameraFlashesEnabled"));
    else SU32(e, 0x94, 1);
    return 0;
}
RX_FUNCTION(0x0041b2a0, rx_level_info_vf5)

/* MusicTrackListEntity (0xb0): 00406e00, vtable 0x009375dc, +0x90..+0xa0 = 0; vf5 00413b80 (no Entity::vf5): +0x90 list {data (30 x 4), 0, 30,
 * 10, 255} of the strings of the RSData children's Enum values (FUN_0080eee0) */
rx_addr rx_music_track_list_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0xb0);
    rx_entity_construct(e, 0x009375dc);
    for (uint32_t k = 0x90; k <= 0xa0; k += 4) SU32(e, k, 0);
    return e;
}
uint32_t rx_music_track_list_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    rx_addr list = e + 0x90;
    SU32(list, 0, rx_reflex_aligned_alloc(0x1e * 4)); SU32(list, 4, 0); SU32(list, 8, 0x1e); SU32(list, 0xc, 0xa); SU32(list, 0x10, 0xff);
    const struct fmt_entity* rs = rx_scene_rs();
    int f = rs_field("MusicTrackList");
    if (f >= 0) {
        int n = fmt_bxml_child_count(rs->doc, f);
        for (int k = 0; k < n; k++) {
            int c = fmt_bxml_child(rs->doc, f, k);
            if (strcmp(fmt_bxml_tag(rs->doc, c), "RSData")) continue;
            int t = fmt_bxml_find_child(rs->doc, c, "Enum");
            fmt_bxml_value v;
            const char* s = "";
            if (t >= 0 && fmt_bxml_node_value(rs->doc, t, &v) == 0 && v.kind == FMT_BX_STRING) s = v.str;
            uint32_t len = (uint32_t)strlen(s);
            rx_addr str = rx_reflex_aligned_alloc(len + 1);
            for (uint32_t i = 0; i <= len; i++) SU8(str, i, (uint8_t)s[i]);
            scene_list_append(list, str);
        }
    }
    return 0;
}
RX_FUNCTION(0x00413b80, rx_music_track_list_vf5)

/* ---------------------------------------------------------------------------------------------------------------------------
 * ReflectionMapGenEntity (id 0x1f; 0x90 bytes): 0080f740, vtable 0x00973d44; vf5 0080f790 = a jump to Entity::vf5; vf22 0x1f */
rx_addr rx_reflection_map_gen_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0x90);
    rx_entity_construct(e, 0x00973d44);
    SU32(e, 0x70, 0);
    return e;
}
RX_FUNCTION(0x0080f790, rx_entity_base_vf5)

/* ---------------------------------------------------------------------------------------------------------------------------
 * SkyEntity (id 0xc; 0x150 bytes): ModelEntity (00401db0) + the render event vtable at +0x140: 007f1120; vf5 = ModelEntity::vf5 */
rx_addr rx_sky_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0x150);
    rx_model_entity_construct(e);
    SU32(e, 0x140, 0x009370cc);
    SU32(e, 0, 0x00972acc); SU32(e, 0x90, 0x00972b44); SU32(e, 0x140, 0x00972b84);
    return e;
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * LensFlareEntity (id 0x1061; 0x190 bytes): 006d32d0, vtables 0x00960fac / 0x00961024, +0x100 0x00960fa0; vf5 006d3550. Lists at +0xa4 (the
 * texture handles), +0xb8 / +0xcc (floats), +0xe0 (colours), each {data, count, capacity, growth, max}. */
static void lf_list(rx_addr e, uint32_t off, uint32_t cap, uint32_t elem) {
    SU32(e, off, rx_reflex_aligned_alloc(rx_mul_sat(cap, elem)));
    SU32(e, off + 4, 0); SU32(e, off + 8, cap); SU32(e, off + 0xc, cap); SU32(e, off + 0x10, 0x7fffffff);
}
rx_addr rx_lens_flare_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0x190);
    rx_entity_construct(e, 0x00960fac);
    SU32(e, 0x90, 0x0093708c); SU32(e, 0x94, 0xf0);
    SU32(e, 0x90, 0x00961024);
    lf_list(e, 0xa4, 0x19, 4);
    lf_list(e, 0xb8, 5, 4);
    lf_list(e, 0xcc, 5, 4);
    lf_list(e, 0xe0, 5, 16);
    SU32(e, 0x100, 0x00960fa0);
    for (uint32_t k = 0x138; k <= 0x14c; k += 4) SU32(e, k, 0xfffffffd);
    for (uint32_t k = 0x108; k <= 0x134; k += 4) SU32(e, k, 0);
    return e;
}
uint32_t rx_lens_flare_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    rx_entity_vf5(e);
    SF32(e, 0x98, rs_float("MaximumScale"));
    SF32(e, 0x9c, rs_float("VisibilityCheckRadius"));
    SU32(e, 0xa0, (uint32_t)rs_int("NumLensFlareTextures"));
    scene_rs_asset_list("LensFlareTextureList", e + 0xa4);
    scene_rs_float_list("LensFlareDistanceList", e + 0xb8);
    scene_rs_float_list("LensFlareScaleList", e + 0xcc);
    scene_rs_color_list("LensFlareColorList", e + 0xe0);
    SU32(e, 0xf8, rx_asset_handle("surface", "BillboardQuad"));
    SU32(e, 0xf4, rx_asset_handle("material", "LensFlare"));
    SU32(e, 0x174, 0); SU32(e, 0x178, 0); SU32(e, 0x15c, 0); SU32(e, 0x158, 0); SU32(e, 0x104, 0);
    SF32(e, 0x154, F32(e, 0x9c)); SF32(e, 0x150, F32(e, 0x98));
    SU32(e, 0x170, U32(e, 0xa0));
    SU32(e, 0x178, e + 0xb8);
    SU32(e, 0x158, U32(e, 0xf4)); SU32(e, 0x15c, U32(e, 0xf8));
    SU32(e, 0x174, e + 0xa4); SU32(e, 0x17c, e + 0xcc); SU32(e, 0x180, e + 0xe0);
    rx_addr m = scene_vcall(e, 0x38, 0, 0);   /* vf14: the matrix (Entity::vf14 = entity + 0x30) */
    float x = F32(m, 0x30), y = F32(m, 0x34);
    m = scene_vcall(e, 0x38, 0, 0); (void)m;
    SF32(e, 0x160, x); SF32(e, 0x164, y); SF32(e, 0x168, F32(scene_vcall(e, 0x38, 0, 0), 0x38));
    return 0;
}
RX_FUNCTION(0x006d3550, rx_lens_flare_vf5)
