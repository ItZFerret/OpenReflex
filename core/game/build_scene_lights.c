/* build_scene_lights.c -- the scene's render container (scene + 0x1a18, FUN_007df410 / FUN_007dfba0: the light / renderable lists, the
 * octree +0x10b4 and the culling grid +0x10f0): an entity whose vf25 (+0x64) answers an IRenderable is given to Lights_Add 007dfdd0
 * (anim_scene.c: the animation unit's port, registered there) by SceneManager_AddEntity.
 * The container is render state (visibility culling, light selection; no race frame reads it). Its constructor belongs to the
 * SceneManager (plan unit other_render, not a core construction), so until that is built the grid pointer (+0x10f0) is 0 and the
 * registration is skipped: the lists stay empty. With the grid present the call goes to the animation unit's Lights_Add. */
#include "build_scene_int.h"

void rx_lights_add(rx_addr L, rx_addr e);   /* 007dfdd0 (anim.h) */

void rx_scene_lights_add(rx_addr lights, rx_addr e) {
    if (U32(lights, 0x10f0) == 0) return;
    rx_lights_add(lights, e);
}
