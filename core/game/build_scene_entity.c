/* build_scene_entity.c -- 0080ddf0 (the scene Entity's base description) and 007f1810 (ModelInstance: bind a model handle, with the
 * per-LOD material override lists). Hand transliterations of the disassembly; core/notes/construction.md (scene, rider). */
#include "rx_game.h"
#include "build_alloc.h"
#include "build_scene_entity.h"
#include "build_scene_int.h"
#include "build_rider_util.h"
#include "crt.h"
#include "anim.h"
#include "rx_vcall.h"
#include "anim_extern.h"
#include "loop_platform.h"

/* 0080ddf0 (stdcall entity, desc {+0 scene, +4 name, +8 flag}; RET 8): entity + 0x10 = the scene, +0x14 / +0x18 / +0x20 / +0x24 cleared,
 * +0x30 the identity matrix 0x9cb510 (64 bytes), +0x1c = the flag, the name copied to a ReflexAlignedAlloc block (+0x18) when the flag is set */
void rx_entity_describe(rx_addr e, rx_addr desc) {
    SU32(e, 0x10, U32(desc, 0));
    SU32(e, 0x18, 0);
    SU32(e, 0x14, 0);
    SF32(e, 0x20, 0.0f);
    SF32(e, 0x24, 0.0f);
    for (uint32_t k = 0; k < 0x10; k++) SU32(e, 0x30 + 4 * k, U32(0, 0x009cb510 + 4 * k));
    SU32(e, 0x1c, U32(desc, 8));
    if (U32(desc, 8) != 0) {
        const uint32_t len = crt_strlen(U32(desc, 4));
        const rx_addr p = rx_reflex_aligned_alloc(len + 1);
        SU32(e, 0x18, p);
        crt_strncpy(p, U32(desc, 4), len);
        SU8(p, len, 0);
    }
}
RX_FUNCTION(0x0080ddf0, rx_entity_describe)

/* the LOD table of a model: +0x130 the LOD records (0x1c bytes: +0x10 / +0x14 the begin / end of 16-byte entries {.., .., .., +0xc material}), +0x118 the LOD count */
static void lod_checks(rx_addr model, uint32_t lod, uint32_t entry) {
    const uint32_t b = U32(model, 0x130);
    if (lod >= (U32(model, 0x134) - b) / 0x1cu) crt_invalid_parameter_noinfo();
    const rx_addr rec = U32(model, 0x130) + 0x1cu * lod;
    if (entry >= ((U32(rec, 0x14) - U32(rec, 0x10)) >> 4)) crt_invalid_parameter_noinfo();
}

/* 007f1810 ModelInstance::SetModel (EAX = the model handle, ECX = flags; stack: the instance, the override list {+0 materials, +4 count}; RET 8).
 * Instance: +0x10 u16 (bits 3-4: flags & 3), +0x60 the handle, +0x64.. seven per-LOD override lists (records {u16 slot, u32 material}), +0x84 their
 * block. A material slot is listed when it is set in the override list and differs from the model's own material of LOD 0 (+0xc of the entry).
 * With no override list nothing else is built. (A model with no LODs would leave the final fill's count uninitialized in the original: not reached.) */
void rx_model_instance_set_model(rx_addr inst, rx_addr handle, uint32_t flags, rx_addr ov) {
    SU16(inst, 0x10, (uint16_t)(((flags & 3u) * 8u) | (U16(inst, 0x10) & 0xffe6u)));
    SU32(inst, 0x60, handle);
    if (ov == 0) return;
    SU32(handle, 0x4c, 1);
    rx_addr model = U32(handle, 0x44);
    uint32_t count = 0;
    const int32_t n = I32(ov, 4);
    for (int32_t i = 0; i < n; i++) {
        if ((U32(model, 0x134) - U32(model, 0x130)) / 0x1cu == 0) crt_invalid_parameter_noinfo();
        const rx_addr lod0 = U32(model, 0x130);
        if ((uint32_t)i >= ((U32(lod0, 0x14) - U32(lod0, 0x10)) >> 4)) crt_invalid_parameter_noinfo();
        if (U32(U32(lod0, 0x10) + 0x10u * (uint32_t)i, 0xc) != U32(U32(ov, 0), 4u * (uint32_t)i)) count++;
    }
    uint32_t size = U32(model, 0x118) * count * 8u;
    SU32(U32(inst, 0x60), 0x4c, 0);
    SU32(inst, 0x84, 0);
    rx_addr buf = 0;
    if (size != 0) {
        if (U8(0, 0x00da67d8)) rr_os_cs_enter(0x00d61704);
        buf = rx_crt_aligned_malloc(size, 16);
        if (U8(0, 0x00da67d8)) rr_os_cs_leave(0x00d61704);
        SU32(inst, 0x84, buf);
    }
    if (count == 0) return;
    const rx_addr h = U32(inst, 0x60);
    model = U32(h, 0x44);
    SU32(h, 0x4c, 1);
    uint32_t lod = 0, listed = 0;
    for (; lod < U32(model, 0x118); lod++) {
        SU32(inst, 0x64 + 4 * lod, buf);
        const rx_addr base = buf;
        buf += count * 8u;
        listed = 0;
        for (int32_t j = 0; j < I32(ov, 4); j++) {
            const uint32_t m = U32(U32(ov, 0), 4u * (uint32_t)j);
            if (m == 0) continue;
            lod_checks(model, lod, (uint32_t)j);
            const rx_addr rec = U32(model, 0x130) + 0x1cu * lod;
            if (U32(U32(rec, 0x10) + 0x10u * (uint32_t)j, 0xc) != m) {
                const rx_addr r = base + (listed & 0xffffu) * 8u;
                SU16(r, 0, (uint16_t)j);
                SU32(r, 4, m);
                listed++;
            }
        }
    }
    const rx_addr fill = U32(inst, 0x60 + 4 * U32(model, 0x118)) + (listed & 0xffffu) * 8u;
    for (uint32_t k = lod; k < 7; k++) SU32(inst, 0x64 + 4 * k, fill);
    SU32(U32(inst, 0x60), 0x4c, 0);
}

/* 0063dae0 (EAX = the camera entity, ESI = the scene): the entity's base description {scene, no name, flag 0} (0080ddf0), its scene id (+0x14) taken from the
 * scene's counter +0x19f4 under the scene's lock, the scene adds it (007d2f70), +0x2f8 = 1 */
void rx_camera_entity_add_to_scene(rx_addr cam, rx_addr scene) {
    RX_FRAME_ENTER(F, 0x14);
    SU32(F, 8, scene); SU32(F, 0xc, 0); SU32(F, 0x10, 0);
    rx_entity_describe(cam, F + 8);
    rr_os_cs_enter(scene + 4);
    const uint32_t id = U32(scene, 0x19f4);
    SU32(scene, 0x19f4, id + 1);
    SU32(cam, 0x14, id);
    rr_os_cs_leave(scene + 4);
    RX_FRAME_LEAVE(F, 0x14);
    rx_scene_manager_add_entity(cam, scene);
    SU32(cam, 0x2f8, 1);
}
RX_FUNCTION(0x0063dae0, rx_camera_entity_add_to_scene)

/* 004045c0 InstancedAnimationEntity::vf7_o90 (ECX = the entity's +0x90 sub-object, which is entity + 0x90; the original reads [ECX + 0x9d4]): the light range, as the x87 result */
float rx_instanced_anim_light_range(rx_addr self) { return F32(self, 0x9d4); }
RX_FUNCTION(0x004045c0, rx_instanced_anim_light_range)

/* 004045d0 InstancedAnimationEntity::vf6_o90 (thiscall ECX = the +0x90 sub-object, stdcall out a, b, c; RET 0xc): the light's shape: a = (+0x9b0, +0x9b4, +0x9b8) the position,
 * b = (+0x9c0, +0x9c4, +0x9c8) the direction, c = +0x9d0 the range; each float through the x87 (fld / fstp) */
void rx_instanced_anim_light_shape(rx_addr self, rx_addr a, rx_addr b, rx_addr c) {
    anim_x87_copy_f32(a, self + 0x9b0); anim_x87_copy_f32(a + 4, self + 0x9b4); anim_x87_copy_f32(a + 8, self + 0x9b8);
    anim_x87_copy_f32(b, self + 0x9c0); anim_x87_copy_f32(b + 4, self + 0x9c4); anim_x87_copy_f32(b + 8, self + 0x9c8);
    anim_x87_copy_f32(c, self + 0x9d0);
}
RX_FUNCTION(0x004045d0, rx_instanced_anim_light_shape)

/* 00404620 InstancedAnimationEntity::vf12_o90 (ECX = the sub-object): a tail jump to virtual 12 (+0x30) of the +0x90 sub-object of the entity at +0x960 (else +0x964) */
uint32_t rx_instanced_anim_vf12_o90(rx_addr self) {
    const rx_addr child = U32(self, 0x960) != 0 ? U32(self, 0x960) : U32(self, 0x964);
    return rx_vcall(child + 0x90, 0x30, 0, NULL);
}
RX_FUNCTION(0x00404620, rx_instanced_anim_vf12_o90)

/* 00404590 InstancedAnimationEntity::vf11_o90 (ECX = the sub-object): a tail jump to the entity's virtual 14 (+0x38) */
uint32_t rx_instanced_anim_vf11_o90(rx_addr self) { return rx_vcall(self - 0x90, 0x38, 0, NULL); }
RX_FUNCTION(0x00404590, rx_instanced_anim_vf11_o90)

/* 004045b0 InstancedAnimationEntity::vf14 -> 0x009cb510 (the identity matrix constant) */
rx_addr rx_instanced_anim_vf14(void) { return 0x009cb510; }
RX_FUNCTION(0x004045b0, rx_instanced_anim_vf14)

/* 00617f60 CharacterEntity::vf6_o90 (ECX = the IRenderable at entity + 0x90; stdcall centre, extents, radius; RET 0xc; shared by 18 classes): the model's bounds (Model_Bounds
 * 007f7f40: the asset this + 0x170, the instance this + 0x10), then the centre = the translation row (+0x30 .. +0x38) of the root node's matrix (the node [this + 0x1c8]'s
 * +0xb-th 0x40-byte matrix of its set's array at +0x18) */
void rx_character_entity_vf6_o90(rx_addr self, rx_addr ctr, rx_addr ext, rx_addr rad) {
    const rx_addr sp0 = rx_sp(), F = ((sp0 - 4u) & ~15u) - 0x44u - 12u;
    rx_set_sp(F);
    { const rx_addr sp = rx_sp(); rx_set_sp(sp - 24u); scene_model_bounds(self + 0x170, self + 0x10, ctr, ext, rad); rx_set_sp(sp); }
    const rx_addr node = U32(self, 0x1c8);
    const rx_addr m = U32(U32(node, 0), 0x18) + ((uint32_t)U8(node, 0xb) << 6);
    for (uint32_t k = 0; k < 16; k++) SU32(F, 0x10 + 4 * k, U32(m, 4 * k));
    SU32(ctr, 0, U32(F, 0x40)); SU32(ctr, 4, U32(F, 0x44)); SU32(ctr, 8, U32(F, 0x48));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00617f60, rx_character_entity_vf6_o90)

/* 007f1aa0 (ESI = the model instance): its mesh block (+0x84) freed (_aligned_free 0x916350) under the allocation lock 0xd61704
 * while 0xda67d8 is set; +0x64 .. +0x7c cleared. Frame: push edi. */
void rx_model_instance_release(rx_addr inst) {
    const rx_addr entry0 = rx_sp(), E = entry0 - 4u;           /* push edi */
    rx_set_sp(E);
    if (U8(0, 0x00da67d8) != 0) loop_enter_cs(0x00d61704);
    const rx_addr p = U32(inst, 0x84);
    { const rx_addr s = rx_sp(); rx_set_sp(s - 4u); crt_aligned_free(p); rx_set_sp(s); }
    if (U8(0, 0x00da67d8) != 0) loop_leave_cs(0x00d61704);
    for (uint32_t k = 0; k < 0x1c; k += 4) SU32(inst, 0x64 + k, 0);
    rx_set_sp(entry0);
}
RX_FUNCTION(0x007f1aa0, rx_model_instance_release)
