/* scene_viewports.c -- the scene manager's render-sync / viewport list (FUN_007d3290 SceneManager_AddViewport, FUN_00794a10 the
 * record constructor, FUN_00794b80 the name) as MXGame viewports 00757660 creates it: one "Shared Viewport" in the scene at
 * engine + 0x24 and one "ViewPort %d" per view in the scene managers at +0x1c and +0x20.
 *
 * The list is gameplay-relevant: particles read it and0063dba0 copies the
 * actual selected camera into view+0. Original794b80 owns a CPU Camera through
 * XenonCore::vf10, not a UI/font object. GPU resources remain host-owned;
 * CPU Camera/parameter ownership and reconfiguration cleanup are retained. */

#include "scene_fx_entities.h"
#include "build_scene.h"
#include "build_alloc.h"
#include "anim.h"
#include "view_parameters.h"
#include "view_camera.h"
#include "build_activity_util.h"
#include <stdio.h>
#include <string.h>

/* 00794a10 (EAX = the 0x84-byte record): the viewport rectangle / flags ctor */
static void viewport_record_construct(rx_addr r) {
    SU16(r, 6, 0); SU16(r, 4, 0); SU16(r, 8, 0x8000); SU16(r, 10, 0x8000); SU16(r, 12, 0xffff); SU16(r, 14, 0xffff);
    SU32(r, 0x10, 1);
    SU32(r, 0, 0);
    SU8(r, 0x18, 0);
    SU32(r, 0x58, r);                                   /* +0x16 * 4 */
    SU32(r, 0x5c, 0xff808080u);
    SU32(r, 0x74, 0); SU32(r, 0x78, 1); SU32(r, 0x7c, 0);
    SU32(r, 0x14, 0);
    SU32(r, 0x64, 0); SU32(r, 0x68, 0); SU32(r, 0x60, 0); SU32(r, 0x70, 0);
    SU32(r, 0x6c, 0);
    SU32(r, 0x80, 0);
}

/*00794b80: EngineContext vf10 allocates/constructs the owned CPU Camera at+0,
 * then copies the name at+18 (0x3f chars plus terminator). */
static void viewport_record_name(rx_addr r, const char* name) {
    rx_addr camera=rx_view_camera_create();
    if(!camera){rx_reflex_aligned_free(r);act_unreached(0x00843aa0,"viewport Camera allocation failed");}
    SU32(r, 0, camera);
    SU32(r, 0x10, 1);
    uint32_t i = 0;
    for (; name[i] && i < 0x3f; i++) SU8(r, 0x18 + i, (uint8_t)name[i]);
    SU8(r, 0x18 + i, 0);
    SU8(r, 0x57, 0);
}

/*794a80's attached render-record invalidation (also performed first by7d3140). */
static void viewport_clear_render_reference(rx_addr r) {
    rx_addr link=U32(r,0x80);if(!link)return;
    static const uint8_t offsets[]={0,0x60,0x64,0x68,0x10,0x14,0x18,0x20,0x24,0x28,0x30,0x34,0x38};
    for(unsigned i=0;i<sizeof offsets;i++)SU32(link,offsets[i],0);
}
static void viewport_require_cpu_owners(rx_addr r) {
    if(!U32(r,0x14)&&(U32(r,0x60)||U32(r,0x64)||U32(r,0x68)||U32(r,0x6c)))
        act_unreached(0x00794a80,"native GPU viewport owner is outside the installed managed renderer");
}
void rx_viewport_cpu_release(rx_addr r) {
    viewport_require_cpu_owners(r);
    if(!U32(r,0x14)) { /* original borrowed/shared-owner guard */
        SU32(r,0x60,0);SU32(r,0x64,0);SU32(r,0x68,0);
        rx_addr camera=U32(r,0);
        if(camera){SU32(camera,0,0x0095a2d8);rx_reflex_aligned_free(camera);}
        rx_addr parameters=U32(r,0x70);
        if(parameters){
            SU32(parameters,0xb0,0x00971b14);SU32(parameters,0x70,0x00971b14);SU32(parameters,0x10,0x00971b14);
            rx_reflex_aligned_free(parameters);
        }
    }
    viewport_clear_render_reference(r);
}
static void scene_clear_cpu_viewports(rx_addr scene) {
    if(!scene)return;
    int32_t count=I32(scene,0x19d0);
    if(count<0||count>6)act_unreached(0x007d3140,"invalid viewport list during CPU owner release");
    /* Product scene_add_viewport never inserts a native name-map/GPU entry.
     * This is its paired owner, not a claim to arbitrary native GPU teardown. */
    for(int32_t i=count-1;i>=0;i--){
        rx_addr r=U32(scene,0x19b8+4u*(uint32_t)i);
        if(r){
            viewport_clear_render_reference(r);SU32(r,0x80,0);
            rx_viewport_cpu_release(r);rx_reflex_aligned_free(r);
        }
        SU32(scene,0x19b8+4u*(uint32_t)i,0);SU32(scene,0x19d0,(uint32_t)i);
    }
}

/* FUN_007d3290 (EAX = the scene, stack name; RET 4): a new record appended to scene + 0x19b8 (at most six) */
static rx_addr scene_add_viewport(rx_addr scene, const char* name) {
    if (!scene || I32(scene, 0x19d0) > 5) return 0;
    rx_addr r = rx_reflex_aligned_alloc(0x84);
    if (!r) return 0;
    viewport_record_construct(r);
    viewport_record_name(r, name);
#ifndef RX_ORACLE
    /* Original00757660 calls00794c10 for EVERY shared/player view before
     * game resets/scenarios can write the existing parameter block. The CPU
     * owner is required even though the host supplies the GPU presentation. */
    rx_addr parameters=rx_view_parameters_create();
    if(!parameters){rx_viewport_cpu_release(r);rx_reflex_aligned_free(r);act_unreached(0x00794c10,"viewport parameter allocation failed");}
    SU32(r,0x70,parameters);
#endif
    const uint32_t n = U32(scene, 0x19d0);
    SU32(scene, 0x19b8 + 4u * n, r);
    SU32(scene, 0x19d0, n + 1);
    return r;
}

/* 00757660's viewport list part: the shared viewport in engine + 0x24 and one viewport per view in the two scene managers */
void rx_scene_viewports_create(rx_addr game, uint32_t views) {
    const rx_addr ctx = U32(0, 0x00a95844);
    if (!ctx) return;
    if (U32(game, 0x15b24) == views && U32(game, 0x15b2c) == U32(game, 0x14)) return;
    /*757660 removes previous views in all three scenes before publishing any
     * replacement. Validate the managed-GPU boundary before releasing owners. */
    for(uint32_t offset=0x1c;offset<0x28;offset+=4){
        rx_addr scene=U32(ctx,offset);if(!scene)continue;
        int32_t count=I32(scene,0x19d0);
        if(count<0||count>6)act_unreached(0x007d3140,"invalid viewport list during replacement");
        for(int32_t i=0;i<count;i++){rx_addr r=U32(scene,0x19b8+4u*(uint32_t)i);if(r)viewport_require_cpu_owners(r);}
    }
    for(uint32_t offset=0x1c;offset<0x28;offset+=4)scene_clear_cpu_viewports(U32(ctx,offset));
    scene_add_viewport(U32(ctx, 0x24), "Shared Viewport");
    for (uint32_t v = 1; v <= views; v++) {
        char name[32];
        snprintf(name, sizeof name, "ViewPort %d", v);
        scene_add_viewport(U32(ctx, 0x20), name);
        scene_add_viewport(U32(ctx, 0x1c), name);
    }
    SU32(game, 0x15b24, views);
    SU32(game, 0x15b2c, U32(game, 0x14));
}
