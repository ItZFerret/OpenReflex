/* build_scene_crowd.c -- BillboardCrowdEntity (scene type 0x1041, BillboardCrowdRSData): the stadium / grandstand crowd sections.
 *
 * What it is. A ModelEntity (0x190 bytes: vtables 0x00960654 / 0x009606cc) whose model is one crowd SECTION: a block of seat billboards
 * (one camera-facing quad per seat, four corners sharing the seat position). The original draws it with the "Crowd" shader asset
 * (StaticVertexShader + CrowdPixelShader) through XenonBillboardCrowdRenderEvent 006c6cd0, a singleton event object ([0x00a942f4], made by
 * the first crowd, freed by the last: [0x00a942f8] counts the crowds) that gathers the draws of all the sections, and animates the fans
 * by the wave phase +0x140 and the cheering flashes (drawn by 006c7880, queued by 006c7fe0; rx_session_crowd_flashes exports them). The simulation part is 006c6650 (loop_entity.c): the phase
 * and the flash records, which draw the CRT rand() stream. Stadiums (Supercross / Freestyle) and the National / Powercross / Terracross
 * grandstands carry 8 .. 245 sections each (SX_FortDodge 245).
 *
 * Until 2026-10-27 the core did not build this class ("render, outside the core": build_scene.c) so the product had no stands crowd at all
 * (the playtest: "NPCs missing, like in the stands") and the CRT rand stream of the product lacked the draws of 006c6650. The state the
 * renderer needs (the wave phase, the tier, the empty-chair ratio, the flash records) is exported through rx_session_ambient_entities
 * (kind 8, BillboardCrowd) and drawn by Unity (OriginalAmbient + Reflex/Original Crowd Billboard).
 *
 * Fields (entity): +0x140 wave phase (float), +0x144 "Is Non Animated" (bool), +0x148 "Tier Number" (uint), +0x14c the CameraFlash texture
 * handle (0 when the level has none), +0x154 the ModelAsset handle, +0x158 .. +0x16c shader objects (+0x15c / +0x158 the Crowd program's
 * vertex shader and its second object, +0x160 the pixel shader, +0x168 / +0x164 the CameraFlash vertex shader, +0x16c the CameraFlash
 * pixel shader; refcounted D3D objects), +0x170 the flash vector (16-byte records: +0 active, +4 age, +8 length, +0xc frame; one per
 * binding of the model's LOD 0 when the CameraFlash texture exists).
 *
 * Functions (verified against the original, group rx_build_crowd; oracle adapters core/oracle/build_scene_crowd.cpp):
 *   006c5df0 render event constructor (EDI)     006c5f30 constructor (ESI = the 0x190 block)     006c6180 vf5 (thiscall, RSData reader)
 *   006c6940 BillboardCrowd_InitFlashes (stdcall vector, count; the MSVC vector<16 bytes>::resize 006c6990 / _Insert_n 006c6a40)
 *   006c5c80 render event destructor (ESI)      006c6040 entity destroy (EDI)                     006c5ff0 sdtor (thiscall, flags; RET 4) */
#include "build_scene_int.h"
#include "build_activity_util.h"
#include "build_load.h"
#include "build_render.h"
#include "crt.h"
#include "loop_platform.h"
#include "../src/rx_d3d.h"

#define CROWD_EVENT      0x00a942f4u   /* the shared render event, made by the first crowd */
#define CROWD_COUNT      0x00a942f8u   /* crowds alive */
#define CROWD_SIZE       0x190u
#define EVENT_SIZE       0xd0u

/* ---- the render event (XenonBillboardCrowdRenderEvent): eight std::vector headers at +0x10 + 0x18 k ---- */

/* 006c5df0 (EDI = the 0xd0 block; plain ret) -> EAX = the block: vtable 0x0096070c, the eight vectors (proxy allocation of 4 bytes each,
 * +0xc / +0x10 / +0x14 cleared), +4 = 0. */
rx_addr rx_billboard_crowd_event_construct(rx_addr ev) {
    SU32(ev, 0, 0x0096070c);
    for (uint32_t k = 0; k < 8; k++) rx_ld_vec_init(ev + 0x10 + 0x18 * k);
    SU32(ev, 4, 0);
    return ev;
}
RX_FUNCTION(0x006c5df0, rx_billboard_crowd_event_construct)

/* 006c5c80 (ESI = the event; plain ret): the vtable 0x0096070c stored again, then for the vectors at +0xb8 .. +0x10 (downwards): the element block
 * freed when there is one, the three pointers cleared, the proxy freed. */
void rx_billboard_crowd_event_destroy(rx_addr ev) {
    SU32(ev, 0, 0x0096070c);
    for (int32_t k = 7; k >= 0; k--) {
        const rx_addr v = ev + 0x10 + 0x18 * (uint32_t)k;
        const rx_addr data = U32(v, 0xc), proxy = U32(v, 0);
        if (data != 0) rx_reflex_aligned_free(data);
        SU32(v, 0xc, 0); SU32(v, 0x10, 0); SU32(v, 0x14, 0);
        rx_reflex_aligned_free(proxy);
    }
}
RX_FUNCTION(0x006c5c80, rx_billboard_crowd_event_destroy)

/* ---- the constructor ---- */

/* 006c5f30 (ESI = the entity; plain ret) -> EAX = the entity: ModelEntity (00401db0), the vtables, the flash vector header (+0x170: proxy
 * allocation of 4 bytes), one more crowd alive, the fields cleared, and the shared render event made when there is none yet. */
rx_addr rx_billboard_crowd_construct(rx_addr e) {
    rx_model_entity_construct(e);
    SU32(e, 0, 0x00960654); SU32(e, 0x90, 0x009606cc);
    rx_ld_vec_init(e + 0x170);
    SU32(0, CROWD_COUNT, U32(0, CROWD_COUNT) + 1);
    SU32(e, 0x144, 0); SF32(e, 0x20, 0.0f); SU32(e, 0x148, 0); SF32(e, 0x140, 0.0f);
    for (uint32_t k = 0x158; k <= 0x16c; k += 4) SU32(e, k, 0);
    if (U32(0, CROWD_EVENT) == 0) {
        const rx_addr ev = rx_reflex_aligned_alloc(EVENT_SIZE);
        SU32(0, CROWD_EVENT, ev ? rx_billboard_crowd_event_construct(ev) : 0);
    }
    return e;
}
RX_FUNCTION(0x006c5f30, rx_billboard_crowd_construct)

/* the factory (004046a0 case 0x1041: FUN_00401f90(0x190) then 006c5f30) */
rx_addr rx_billboard_crowd_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(CROWD_SIZE);
    return rx_billboard_crowd_construct(e);
}

/* ---- the flash vector: BillboardCrowd_InitFlashes ---- */

/* std::vector<16-byte record>::_Insert_n(end, n, value) as 006c6a40 does it for an append (the only use): the capacity grows to
 * max(size + n, capacity + capacity / 2) (FUN_00419be0: ReflexAlignedAlloc(16 * capacity)), the old records are copied (007cfda0), the n values
 * filled (007d03a0), the old block freed; with room enough the n values are written at the end. */
static void flashes_append(rx_addr v, uint32_t n, const uint32_t value[4]) {
    const rx_addr begin = U32(v, 0xc), end = U32(v, 0x10);
    const uint32_t cap = begin ? (U32(v, 0x14) - begin) >> 4 : 0, size = (end - begin) >> 4;
    if (n == 0) return;
    if (0x0fffffffu - size < n) act_unreached(0x00851560, "vector<> too long");
    rx_addr dst = begin, tail = end;
    if (cap < size + n) {
        uint32_t newcap = cap > 0x0fffffffu - (cap >> 1) ? 0 : cap + (cap >> 1);
        if (newcap < size + n) newcap = size + n;
        if (0xffffffffu / newcap < 0x10) act_unreached(0x00419c15, "vector<> allocation failure (bad_alloc)");
        dst = rx_reflex_aligned_alloc(newcap * 16);
        for (uint32_t i = 0; i < size; i++) for (uint32_t k = 0; k < 16; k += 4) SU32(dst + 16 * i, k, U32(begin + 16 * i, k));
        tail = dst + 16 * size;
        for (uint32_t i = 0; i < n; i++) for (uint32_t k = 0; k < 4; k++) SU32(tail + 16 * i, 4 * k, value[k]);
        if (begin) rx_reflex_aligned_free(begin);
        SU32(v, 0x14, dst + 16 * newcap);
        SU32(v, 0x10, dst + 16 * (size + n));
        SU32(v, 0xc, dst);
        return;
    }
    for (uint32_t i = 0; i < n; i++) for (uint32_t k = 0; k < 4; k++) SU32(tail + 16 * i, 4 * k, value[k]);
    SU32(v, 0x10, end + 16 * n);
}

/* 006c6940 BillboardCrowd_InitFlashes (stdcall vector, count; RET 8): resize the flash vector to `count` records. The record it fills
 * is built on the original's stack: byte 0 = 0, +4 and +8 = 0.0f, and the other bytes are the DEAD STACK of that frame: bytes 1 .. 3 of
 * the word at S - 0x14 and the word at S - 8 (S = ESP at entry), which the original copies into every record (the dead-stack trap,
 * notes/loop.md). Fewer records than present: the tail is erased (00544300 on the end of the vector). */
void rx_billboard_flashes_init(rx_addr v, uint32_t count) {
    const rx_addr S = rx_sp();
    const uint32_t value[4] = { U32(S, -0x14) & ~0xffu, 0, 0, U32(S, -8) };
    const uint32_t size = (U32(v, 0x10) - U32(v, 0xc)) >> 4;
    if (count > size) flashes_append(v, count - size, value);
    else if (count < size) SU32(v, 0x10, U32(v, 0xc) + 16 * count);
}
RX_FUNCTION(0x006c6940, rx_billboard_flashes_init)

/* ---- vf5: the RSData ---- */

/* 006c6180 BillboardCrowdEntity::vf5 (thiscall e, reader; RET 4): the model instance (ModelInstance_Init), the Entity base, the matrix copied to
 * the instance (+0xc0), "ModelAsset" (+0x154), "Tier Number" (+0x148), "Is Non Animated" (+0x144), +0x70 = 1, the CameraFlash texture
 * (+0x14c) and, when the level has one, one flash record per binding of the model's LOD 0; then the Crowd shader asset's programs
 * (StaticVertexShader, CrowdPixelShader or PixelShader, CameraFlashVS, CameraFlashPS). The model handle's +0x4c is held at 1 while the asset
 * is read (and left at 1 on the shader asset: the original never clears it there). Frame: push ebp, and esp -8; push ecx, ebx, esi, edi. */
uint32_t rx_billboard_crowd_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    ACT_FRAME_EBP(F, 8, 0, 4);
    ACT_CALL(1, rx_model_instance_init(e + 0xa0));
    ACT_CALL(1, rx_entity_vf5(e));
    scene_copy_words(e + 0xc0, e + 0x30, 16);
    SU32(e, 0x154, rs_asset("ModelAsset"));
    SU32(e, 0x148, rs_uint("Tier Number"));
    SU32(e, 0x144, rs_bool("Is Non Animated"));
    SU32(e, 0x70, 1);
    /* Database::vf27 ("CameraFlash", "text", "ure", 0): the four pushed words, the return address in the dead stack (the word 0x10 below it,
     * F - 0x20 = A - 0x30, is a local of vf27's frame it never writes: whatever the helpers before it left; the upper three bytes are what
     * the flash records of InitFlashes keep, and they are 0 in every crowd of the original's boots (crowd_probe.cpp)) */
    SU32(F, -4, 0); SU32(F, -8, 0x657275); SU32(F, -12, 0x74786574); SU32(F, -16, 0x009605ec); SU32(F, -20, 0x006c622a); SU32(F, -0x20, 0);
    const rx_addr flash = rx_load_find_handle_c("texture", "CameraFlash");
    SU32(e, 0x14c, flash);
    if (flash) {
        const rx_addr handle = U32(e, 0x154), asset = U32(handle, 0x44);
        SU32(handle, 0x4c, 1);
        if (((U32(asset, 0x134) - U32(asset, 0x130)) / 0x1c) == 0) act_unreached(0x00916224, "LOD vector empty (_invalid_parameter_noinfo)");
        const rx_addr lod0 = U32(asset, 0x130);
        const uint32_t bindings = (U32(lod0, 0x14) - U32(lod0, 0x10)) >> 4;
        SU32(F, -4, bindings); SU32(F, -8, e + 0x170);
        const rx_addr sp_ = rx_sp(); rx_set_sp(F - 12);
        rx_billboard_flashes_init(e + 0x170, bindings);
        rx_set_sp(sp_);
        SU32(U32(e, 0x154), 0x4c, 0);
    }
    /* Database::vf27 ("Crowd", "shad", "er", 1) */
    SU32(F, -4, 1); SU32(F, -8, 0x7265); SU32(F, -12, 0x64616873); SU32(F, -16, 0x0095b7e0); SU32(F, -20, 0x006c62b6);
    const rx_addr handle = rx_load_find_handle_c("shader", "Crowd");
    if (!handle || !U32(handle, 0x44)) act_unreached(0x006c62b6, "the Crowd shader asset is not loaded");
    const rx_addr sh = U32(handle, 0x44);
    SU32(handle, 0x4c, 1);
    rx_shader_find_vs(sh, "StaticVertexShader", e + 0x15c, e + 0x158);
    rx_shader_find_ps(sh, "CrowdPixelShader", e + 0x160, 0);
    if (U32(e, 0x160) == 0) rx_shader_find_ps(sh, "PixelShader", e + 0x160, e + 0x158);
    rx_shader_find_vs(sh, "CameraFlashVS", e + 0x168, e + 0x164);
    rx_shader_find_ps(sh, "CameraFlashPS", e + 0x16c, 0);
    ACT_END(F);
    return 0;
}
RX_FUNCTION(0x006c6180, rx_billboard_crowd_vf5)

/* ---- destruction ---- */

/* the Entity base's tail: both entity registries told that the entity is gone (vf1 with the entity; null in the product) */
static void crowd_registries_told(rx_addr e) {
    for (int k = 0; k < 2; k++) {
        const rx_addr registry = U32(0, k ? 0x00a95828 : 0x00a95824);
        if (registry != 0) {
            const uint32_t arg = e;
            SU32(rx_sp(), -4, e);
            (void)act_vcall(registry, 4, 1, &arg);
        }
    }
}

/* 006c6040 (EDI = the entity; plain ret): the vtables stored, the six shader objects released (vtable slot 2) and cleared, one crowd less -
 * the last one destroys and frees the shared render event - the flash records freed with their vector's proxy, the ModelEntity and Entity
 * vtables (0x0093708c, 0x0095a2e4) and both registries told. */
void rx_billboard_crowd_destroy(rx_addr e) {
    SU32(e, 0, 0x00960654); SU32(e, 0x90, 0x009606cc);
    static const uint32_t shaders[6] = {0x158, 0x15c, 0x160, 0x164, 0x168, 0x16c};
    for (int k = 0; k < 6; k++) {
        const rx_addr o = U32(e, shaders[k]);
        if (o != 0) {
            const uint32_t a[1] = {o};
            rx_d3d_call(o, 2, 1, a);   /* Release */
            SU32(e, shaders[k], 0);
        }
    }
    SU32(0, CROWD_COUNT, U32(0, CROWD_COUNT) - 1);
    if (U32(0, CROWD_COUNT) == 0) {
        const rx_addr ev = U32(0, CROWD_EVENT);
        if (ev != 0) { rx_billboard_crowd_event_destroy(ev); rx_reflex_aligned_free(ev); }
        SU32(0, CROWD_EVENT, 0);
    }
    if (U32(e, 0x17c) != 0) rx_reflex_aligned_free(U32(e, 0x17c));
    const rx_addr proxy = U32(e, 0x170);
    SU32(e, 0x17c, 0); SU32(e, 0x180, 0); SU32(e, 0x184, 0);
    rx_reflex_aligned_free(proxy);
    SU32(e, 0x90, 0x0093708c); SU32(e, 0, 0x0095a2e4);
    crowd_registries_told(e);
}
RX_FUNCTION(0x006c6040, rx_billboard_crowd_destroy)

/* 006c5ff0 BillboardCrowdEntity::sdtor (thiscall; flags; RET 4) -> this: 006c6040, then freed under the allocator lock with bit 0. */
rx_addr rx_billboard_crowd_sdtor(rx_addr e, uint32_t flags) {
    rx_billboard_crowd_destroy(e);
    if (flags & 1) {
        if (U8(0, 0x00da67d8) != 0) loop_enter_cs(0x00d61704);
        ACT_CALL(1, (SU32(rx_sp(), 4, e), crt_aligned_free(e)));
        if (U8(0, 0x00da67d8) != 0) loop_leave_cs(0x00d61704);
    }
    return e;
}
RX_FUNCTION(0x006c5ff0, rx_billboard_crowd_sdtor)
