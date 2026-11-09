/* build_scene_camera.c -- MXCameraPath (id 0x104b, 0x130 bytes): a camera target / path point of the level (the Entity base 0063e490 is
 * the constructor the MXWorld's embedded camera target uses too: build_mxworld.c keeps its copy static; this one is the scene's, linked
 * into the current MXWorld's target list the same way) with the label and the path's animation clip.
 * Fields: +0x90 / +0x94 the next / previous target of the world's list, +0x98 the world, +0x9c the label (0x40 bytes), +0xdc the clip
 * name (0x40), +0x11c the clip's handle, +0x120 its length (x87 copy of the animation asset's +0x4c), +0x120 initially 0.
 * The replay cameras (MXReplayTower / ZipLine / Spline / Animated, 0x1046 .. 0x1049) are loop_cam_replay.c's; the animated one makes its own path
 * here (rx_camera_path_construct_named, 0063e590). */
#include "build_scene_int.h"
#include "build_asset.h"

/* 0075c530: the current MXWorld by the game state */
static rx_addr current_world(void) {
    rx_addr g = U32(0, 0x00a946b8);
    switch (U32(g, 0x18)) {
    case 1: return U32(g, 0x20c8);
    case 2: case 3: {
        rx_addr core = U32(0, 0x00a95844);
        if (U32(U32(core, 0x20), 0x34) == 1) return U32(g, 0x5a54);
        if (U32(U32(core, 0x1c), 0x34) == 1) return U32(g, 0x20c8);
        return 0;
    }
    default: return U32(g, 0x5a54);
    }
}

/* 0063e490 (stdcall entity): linked at the tail of the current world's +0x21de8 / +0x21dec list */
rx_addr rx_camera_path_create(void) {
    rx_addr t = rx_reflex_aligned_alloc(0x130);
    rx_entity_construct(t, 0x0095a3f4);
    SU32(t, 0x90, 0); SU32(t, 0x94, 0); SF32(t, 0x120, 0.0f);
    for (uint32_t k = 0; k < 0x40; k += 4) { SU32(t, 0x9c + k, 0); SU32(t, 0xdc + k, 0); }
    rx_addr w = current_world();
    SU32(t, 0x98, w);
    if (!w) return t;
    rx_addr tail = U32(w, 0x21dec);
    if (!tail) { SU32(w, 0x21de8, t); SU32(U32(t, 0x98), 0x21dec, t); SU32(t, 0x94, 0); SU32(t, 0x90, 0); return t; }
    SU32(tail, 0x90, t);
    SU32(t, 0x94, U32(U32(t, 0x98), 0x21dec));
    SU32(U32(t, 0x98), 0x21dec, t);
    SU32(t, 0x90, 0);
    return t;
}

/* 0063e7c0 (EDI = the entity; stack the clip name; ret 4): the path's clip: the name (0x40 bytes) copied to +0xdc, its animation asset
 * ("anim") requested by name through the asset system and kept as the handle +0x11c; the clip's length (x87 copy of the animation asset's
 * +0x4c) goes to +0x120. The handle is marked busy (+0x4c = 1) while its asset is read. */
static void camera_path_load_clip(rx_addr e, const char* name) {
    for (uint32_t k = 0; k < 0x40; k += 4) SU32(e, 0xdc + k, 0);
    for (uint32_t k = 0; ; k++) { SU8(e, 0xdc + k, (uint8_t)name[k]); if (!name[k]) break; }
    rx_addr h = rx_asset_handle("anim", name);
    SU32(e, 0x11c, h);
    if (h) {
        rx_addr a = U32(h, 0x44);
        SU32(h, 0x4c, 1);
        SU32(e, 0x120, scene_x87c(U32(a, 0x4c)));
        SU32(U32(e, 0x11c), 0x4c, 0);
    }
}

/* 0063e590 (stdcall entity, label, clip name; ret 0xc) -> the entity: the MXCameraPath on a block the caller allocated (0x130 bytes),
 * built like 0063e490 (the Entity base with the path vtable, both names cleared, linked at the tail of the current MXWorld's +0x21de8 /
 * +0x21dec list; this one without the null test of the world) and then the label (an unbounded copy to +0x9c) and the clip
 * (0063e7c0). The MXAnimatedReplayCamera's vf5 builds its path with it. */
rx_addr rx_camera_path_construct_named(rx_addr t, const char* label, const char* clip) {
    rx_entity_construct(t, 0x0095a3f4);
    SU32(t, 0x90, 0);
    SU32(t, 0x94, 0);
    SF32(t, 0x120, 0.0f);
    for (uint32_t k = 0; k < 0x40; k += 4) {
        SU32(t, 0x9c + k, 0);
        SU32(t, 0xdc + k, 0);
    }
    rx_addr w = current_world();
    SU32(t, 0x98, w);
    rx_addr tail = U32(w, 0x21dec);
    if (!tail) {
        SU32(w, 0x21de8, t);
        SU32(U32(t, 0x98), 0x21dec, t);
        SU32(t, 0x94, 0);
        SU32(t, 0x90, 0);
    } else {
        SU32(tail, 0x90, t);
        SU32(t, 0x94, U32(U32(t, 0x98), 0x21dec));
        SU32(U32(t, 0x98), 0x21dec, t);
        SU32(t, 0x90, 0);
    }
    for (uint32_t k = 0; k < 0x40; k += 4) SU32(t, 0x9c + k, 0);
    for (uint32_t k = 0;; k++) {
        SU8(t, 0x9c + k, (uint8_t)label[k]);
        if (!label[k]) break;
    }
    camera_path_load_clip(t, clip);
    return t;
}
RX_FUNCTION(0x0063e590, rx_camera_path_construct_named)

/* MXCameraPath::vf5 0063e740 (+ FUN_0063e7c0: EDI = entity, stack the clip name) */
uint32_t rx_camera_path_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    rx_entity_vf5(e);
    const char* label = rs_str("Camera Path Label");
    for (uint32_t k = 0; k < 0x40; k += 4) SU32(e, 0x9c + k, 0);
    for (uint32_t k = 0; ; k++) { SU8(e, 0x9c + k, (uint8_t)label[k]); if (!label[k]) break; }
    rx_addr clip = rs_asset("Animation Asset");
    char name[0x40];
    for (uint32_t k = 0; k < 0x40; k++) name[k] = (char)U8(clip, 4 + k);   /* AssetHandle::vf2: the handle's name (+4) */
    camera_path_load_clip(e, name);
    return 0;
}
RX_FUNCTION(0x0063e740, rx_camera_path_vf5)
