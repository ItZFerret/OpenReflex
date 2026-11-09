/* build_scene_anim.c -- the Character-based entities of the scene (CharacterEntity 00839a40 is the rider unit's, build_rider.h): the model
 * copy a character animates (ModelAsset clone 007d4d60), CharacterAnim_Init 00834de0 and the motion slots (CharacterAnim_SetAnimation
 * 00834eb0), then AnimEntitySingleAnimLoop, AnimEntitySplineAnimLoop, ScenarioAnimEntity, CrewChief, InstancedAnimationEntity ...
 * Private copies (documented): the node set clone with a flag (FUN_007ddb40 flag 1: appends a root node), Node_UpdateSubtreeBounds
 * 007dbde0 / ModelAsset_RootBounds 007dd450 (build_model.c keeps them static; anim_node.c's start their walk through the original). */
#include "build_scene_int.h"
#include "crt_util.h"
#include "build_rider.h"
#include "build_model.h"
#include "build_asset.h"
#include "world_node.h"
#include "anim.h"
#include "crt.h"
#include "fx_bind.h"

#define ZERO_VEC 0x00d61670u
#define IDENT    0x00d61600u

/* ModelAsset_RootBounds 007dd450 (EAX = record; stack: out pointers): the subtree bounds updated first; the three values it copies out
 * are not used by the clone (only the update's side effect on the new set) */

/* the model copy of a character / instanced entity is the rider unit's rx_model_asset_copy (007d4d60, build_rider.h) */
static void scene_model_asset_clone(rx_addr dst, rx_addr src) { rx_model_asset_copy(dst, src); }

/* ---------------------------------------------------------------------------------------------------------------------------
 * the character's motion slots (Character = CharacterEntity + 0x148: +4 slot array, +8 count, +0xc capacity, +0x10 growth, +0x18 node
 * set, +0x1c bone array, +0x78 / +0x7c owned helper, +0x8c bone name cache) */

/* 00843350 (EAX = map, EDI = &name, stack byte out; RET 4): the bone name -> index byte of the cache */
static int bone_cache_find(rx_addr map, rx_addr name_cell, rx_addr out) {
    rx_addr key = U32(name_cell, 0);
    uint32_t h = rx_string_hash_nocase(key);
    for (rx_addr e = U32(U32(map, 0xc), 4 * (h % U32(map, 0))); e; e = U32(e, 0xc)) {
        if (U32(e, 8) != h) continue;
        rx_addr a = U32(e, 0), b = key;
        while (U8(a, 0) && (U8(a, 0) | 0x20) == (U8(b, 0) | 0x20)) { a++; b++; }
        if ((U8(a, 0) | 0x20) == (U8(b, 0) | 0x20) && !U8(a, 0) == !U8(b, 0)) { SU8(out, 0, U8(e, 4)); return 1; }
    }
    return 0;
}
/* 008433b0 (EDI = map, stack &name cell, &byte; RET 8): inserted at the head of the bucket (the byte only: the node's other value bytes stay) */
static void bone_cache_insert(rx_addr map, rx_addr name_cell, rx_addr byte_ptr) {
    float load = (float)I32(map, 8) / (float)I32(map, 0);
    if (load > F32(map, 4)) rx_strtab_rehash(map, (uint32_t)rx_cvtt((float)I32(map, 0) * 1.5f /* 0x9322a4 */));
    rx_addr key = U32(name_cell, 0);
    uint32_t h = rx_string_hash_nocase(key), b = h % U32(map, 0);
    for (rx_addr e = U32(U32(map, 0xc), 4 * b); e; e = U32(e, 0xc)) {
        if (U32(e, 8) != h) continue;
        rx_addr x = U32(e, 0), y = key;
        while (U8(x, 0) && (U8(x, 0) | 0x20) == (U8(y, 0) | 0x20)) { x++; y++; }
        if ((U8(x, 0) | 0x20) == (U8(y, 0) | 0x20) && !U8(x, 0) == !U8(y, 0)) { SU8(e, 4, U8(byte_ptr, 0)); return; }
    }
    rx_addr e = rx_std_allocator_alloc(U32(map, 0x18), 0x10);
    if (e) SU32(e, 0xc, 0);
    SU32(e, 0, key); SU8(e, 4, U8(byte_ptr, 0)); SU32(e, 8, h);
    SU32(e, 0xc, U32(U32(map, 0xc), 4 * b));
    SU32(U32(map, 0xc), 4 * b, e);
    SU32(map, 8, U32(map, 8) + 1);
}
/* 007de9e0 (EDI = node set, stack name; RET 4): the node record of a name, 0 when none */
static rx_addr hierarchy_find_node(rx_addr set, rx_addr name) {
    SU32(set, 0x30, 0);
    for (uint32_t i = 0; i < U32(set, 0xc); i++) {
        rx_addr rec = U32(set, 8) + 16 * i;
        uint32_t w = U16(rec, 8);
        if (w == 0xffff) continue;
        rx_addr a = U32(U32(rec, 0), 0x10) + w, b = name;
        while (U8(a, 0) && (U8(a, 0) | 0x20) == (U8(b, 0) | 0x20)) { a++; b++; }
        if ((U8(a, 0) | 0x20) == (U8(b, 0) | 0x20) && !U8(a, 0) == !U8(b, 0)) return rec;
    }
    return 0;
}

/* 00834eb0 CharacterAnim_SetAnimation (stdcall C, motion handle; EAX = loops; RET 8): a motion slot {handle, 0, bone map}; the bone map
 * has one byte per motion bone (the character bone of that name through the cache, else the node set's record index, 0xff none).
 * (private product-runnable copy of anim_character.c's rx_character_anim_set_animation) */
rx_addr scene_character_set_animation(rx_addr c, rx_addr handle, uint32_t loops) {
    RX_FRAME_ENTER(F, 0x18);
    const rx_addr name_local = F + 0x14;
    rx_addr m = U32(handle, 0x44);
    SU32(handle, 0x4c, 1);
    SU32(m, 0x54, loops);
    rx_addr slot = rx_reflex_aligned_alloc(0xc);
    if (slot) { SU32(slot, 0, 0); SU32(slot, 4, 0); SU32(slot, 8, 0); }
    SU32(slot, 0, handle); SU32(slot, 4, 0);
    SU32(slot, 8, rx_reflex_aligned_alloc(U32(m, 0x10)));
    for (int32_t i = 0; i < I32(m, 0x10); i++) {
        rx_addr name = U32(m, 0x14) + 0x20u * (uint32_t)i;
        SU32(name_local, 0, name);
        if (bone_cache_find(c + 0x8c, name_local, U32(slot, 8) + (uint32_t)i)) continue;
        rx_addr set = U32(c, 0x18);
        rx_addr rec = hierarchy_find_node(set, name);
        SU8(U32(slot, 8), i, rec ? (uint8_t)((int32_t)(rec - U32(set, 8)) >> 4) : 0xff);
        SU32(name_local, 0, name);
        bone_cache_insert(c + 0x8c, name_local, U32(slot, 8) + (uint32_t)i);
    }
    SU32(handle, 0x4c, 0);
    if (!(I32(c, 8) < I32(c, 0xc))) {
        uint32_t cap = U32(c, 0x10) + U32(c, 0xc);
        rx_addr list = rx_reflex_aligned_alloc(rx_mul_sat(cap, 4));
        for (uint32_t k = 0; k < U32(c, 0xc); k++) SU32(list, 4 * k, U32(U32(c, 4), 4 * k));
        rx_reflex_aligned_free(U32(c, 4));
        SU32(c, 4, list); SU32(c, 0xc, cap);
    }
    SU32(U32(c, 4), 4u * U32(c, 8), slot);
    SU32(c, 8, U32(c, 8) + 1);
    RX_FRAME_LEAVE(F, 0x18);
    return slot;
}

/* 00834d80 (ESI = C): the slots freed, the bone array and the owned helper dropped (a character that was initialised before) */
static void character_anim_release(rx_addr c) {
    for (int32_t i = 0; i < I32(c, 8); i++) { rx_addr s = U32(U32(c, 4), 4u * (uint32_t)i); rx_reflex_aligned_free(U32(s, 8)); rx_reflex_aligned_free(s); }
    SU32(c, 8, 0);
    if (U32(c, 0x1c)) rx_reflex_aligned_free(U32(c, 0x1c));
    SU32(c, 0x1c, 0);
    if (U32(c, 0x78) && U32(c, 0x7c)) { fprintf(stderr, "rxcore: scene: helper object of a re-initialised character is not built\n"); abort(); }
    SU32(c, 0x78, 0);
}
/* 00834de0 CharacterAnim_Init (EAX = node set, stack C; RET 4): the character on a node set: 0xe0 bytes per node (the local matrix at the
 * start), the owned helper {vtable 0x00975e8c, C, 0}. */
void rx_character_anim_init(rx_addr set, rx_addr c) {
    character_anim_release(c);
    SU32(c, 8, 0);
    SU32(c, 0x18, set);
    SU32(c, 0x20, U32(set, 0xc));
    rx_addr bones = rx_reflex_aligned_alloc(rx_mul_sat(U32(c, 0x20), 0xe0));
    SU32(c, 0x1c, bones);
    for (int32_t i = 0; i < I32(c, 0x20); i++) {
        rx_addr rec = U32(set, 8) + 16u * (uint32_t)i;
        rx_addr src = U32(U32(rec, 0), 0x18) + ((uint32_t)U8(rec, 0xb) << 6);
        scene_copy_words(bones + 0xe0u * (uint32_t)i, src, 16);
    }
    rx_addr helper = rx_reflex_aligned_alloc(0xc);
    SU32(helper, 4, 0); SU32(helper, 8, 0); SU32(helper, 0, 0x00975e8c);
    SU32(c, 0x78, helper); SU32(c, 0x7c, 1);
    SU32(helper, 4, c);
}
RX_FUNCTION(0x00834de0, rx_character_anim_init)

/* ---------------------------------------------------------------------------------------------------------------------------
 * AnimEntitySingleAnimLoop (id 0x1050; 0x750 bytes): ctor 006b9ad0 (ESI = e): CharacterEntity (00839a40), vtables 0x0095f674 / 0x0095f6f4 /
 * 0x0095f734 (at +0x140), 35 elements of 0x1c bytes at +0x36c (+0x18 = 0), +0x740 1, +0x744 0 */
static rx_addr anim_loop_construct(rx_addr e, uint32_t vt0, uint32_t vt90, uint32_t vt140) {
    rx_character_entity_construct(e);
    SU32(e, 0, vt0); SU32(e, 0x90, vt90); SU32(e, 0x140, vt140);
    for (uint32_t i = 0; i < 0x23; i++) SU32(e, 0x36c + 0x1c * i + 0x18, 0);
    SU32(e, 0x740, 1); SU32(e, 0x744, 0);
    return e;
}
rx_addr rx_anim_single_loop_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0x750);
    return anim_loop_construct(e, 0x0095f674, 0x0095f6f4, 0x0095f734);
}
/* the model copy and character of vf5 (006b9b90 / the inlined form of InstancedAnimationEntity::vf5): the handle's asset is cloned into
 * the entity's ModelAsset (+0x200), the character initialised on its node set (+0x210) */
void scene_character_load_model(rx_addr e, rx_addr handle) {
    SU32(handle, 0x4c, 1);
    scene_model_asset_clone(e + 0x200, U32(handle, 0x44));
    SU32(handle, 0x4c, 0);
    rx_character_anim_init(e + 0x210, e + 0x148);
}
/* msvcr90 rand() (IAT 0x0091624c): the per-thread seed. These readers run on the original's level loader thread (the subscene load;
 * rx_scene_load_subscene switches the core's CRT thread to it), whose seed starts at _initptd's 1 and is shared with the other loader
 * draws (the ocean spectrum 007ea9e0 ...). w-rng: was a private generator of this file (seed 1), which left out the loader's other
 * draws. */
static int32_t scene_rand(void) { return crt_rand(); }

/* AnimEntitySingleAnimLoop::vf5 006b9b90 (ECX = e; RET 4) */
uint32_t rx_anim_single_loop_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    rx_addr model = rs_asset("ModelAsset");
    scene_character_load_model(e, model);
    rx_model_instance_init(e + 0xa0);
    rx_entity_vf5(e);
    scene_copy_words(e + 0xc0, e + 0x30, 16);
    rx_addr anim = rs_asset("LoopingAnimation");
    SU32(e, 0x144, anim);
    SU32(e, 0x1f4, scene_character_set_animation(e + 0x148, anim, 0));
    SF32(e, 0x354, rs_float("LoopingSpeed"));
    SU32(e, 0x368, 0); SF32(e, 0x360, 0.0f); SF32(e, 0x364, 0.0f); SU32(e, 0x35c, 0);
    if (rs_field("SoundActions") >= 0) { fprintf(stderr, "rxcore: scene: AnimEntitySingleAnimLoop SoundActions (FUN_006b9d50) is not built\n"); abort(); }
    rx_addr a = U32(e, 0x144);
    SU32(a, 0x4c, 1);
    SU32(U32(a, 0x44), 0x54, 1);
    SU32(U32(e, 0x144), 0x4c, 0);
    SU32(e, 0x350, 1);
    if (rs_has("RandomAnimStart", RS_BOOL)) SU32(e, 0x350, rs_bool("RandomAnimStart"));
    if (U32(e, 0x350)) SF32(e, 0x358, (float)scene_rand() * 3.0518509447574615e-05f /* 0x92e054 */);
    else SF32(e, 0x358, 0.0f);
    SU16(e, 0xb0, (U16(e, 0xb0) & 0xffef) | 8);
    return 0;
}
RX_FUNCTION(0x006b9b90, rx_anim_single_loop_vf5)

/* ---------------------------------------------------------------------------------------------------------------------------
 * AnimEntitySplineAnimLoop (id 0x1052; 0x790 bytes): ctor 006ba570, vtables 0x0095f83c / 0x0095f8bc / 0x0095f8fc, +0x364 / +0x368 0.0, +0x370 0,
 * 35 elements of 0x1c at +0x3b0 (then the whole 0x3d4 block zeroed), +0x784 / +0x788 0.0 */
static rx_addr anim_spline_loop_construct(rx_addr e) {
    rx_character_entity_construct(e);
    SU32(e, 0, 0x0095f83c); SU32(e, 0x90, 0x0095f8bc); SU32(e, 0x140, 0x0095f8fc);
    SF32(e, 0x364, 0.0f); SF32(e, 0x368, 0.0f); SU32(e, 0x370, 0);
    for (uint32_t k = 0; k < 0x3d4; k += 4) SU32(e, 0x3b0 + k, 0);
    SF32(e, 0x784, 0.0f); SF32(e, 0x788, 0.0f);
    return e;
}
rx_addr rx_anim_spline_loop_create(void) { return anim_spline_loop_construct(rx_reflex_aligned_alloc(0x790)); }

/* 006bbde0 (EAX = entity): the spline-loop base, two additional words and its three derived vtables. */
rx_addr rx_anim_spline_particles_construct(rx_addr e) {
    anim_spline_loop_construct(e);
    SU32(e, 0x790, 0); SU32(e, 0x794, 0);
    SU32(e, 0, 0x0095f904); SU32(e, 0x90, 0x0095f984); SU32(e, 0x140, 0x0095f9c4);
    return e;
}
RX_FUNCTION(0x006bbde0, rx_anim_spline_particles_construct)
rx_addr rx_anim_spline_particles_create(void) { return rx_anim_spline_particles_construct(rx_reflex_aligned_alloc(0x798)); }

/* AnimEntitySplineAnimLoop::vf5 006ba690 */
uint32_t rx_anim_spline_loop_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    rx_addr model = rs_asset("ModelAsset");
    scene_character_load_model(e, model);
    rx_model_instance_init(e + 0xa0);
    rx_entity_vf5(e);
    scene_copy_words(e + 0xc0, e + 0x30, 16);
    SF32(e, 0x36c, rs_float("SplineSpeed"));
    rx_addr anim = rs_asset("LoopingAnimation");
    SU32(e, 0x144, anim);
    /* the handle's name (vf2) differs from "Editor_Default": the animation is set */
    char nm[64]; nm[0] = 0;
    for (uint32_t k = 0; k < 0x3f; k++) { nm[k] = (char)U8(anim, 4 + k); if (!nm[k]) break; }
    nm[63] = 0;
    if (strcasecmp(nm, "Editor_Default") != 0) SU32(e, 0x1f4, scene_character_set_animation(e + 0x148, anim, 0));
    else SU32(e, 0x1f4, 0);
    SU32(U32(e, 0x144), 0x4c, 1);
    SU32(U32(U32(e, 0x144), 0x44), 0x54, 1);
    SU32(U32(e, 0x144), 0x4c, 0);
    SF32(e, 0x374, rs_float("LoopingSpeed"));
    SU32(e, 0x3ac, 0); SF32(e, 0x3a4, 0.0f); SF32(e, 0x3a8, 0.0f); SU32(e, 0x3a0, 0);
    if (rs_has("SoundActions", 0)) SU32(e, 0x3a0, rs_asset("SoundActions"));
    SU32(e, 0x38c, 0); SF32(e, 0x394, 0.0f); SF32(e, 0x398, 0.0f); SU32(e, 0x390, 0);
    if (rs_has("RepeatDelay", 0)) {
        float d = rs_float("RepeatDelay");
        SF32(e, 0x394, d);
        if (d > 0.001f /* 0x97743c */) { SU32(e, 0x38c, 1); SF32(e, 0x398, 0.0f); SU32(e, 0x390, 0); }
    }
    rx_addr spline = rs_asset("AnimationSpline");
    SU32(e, 0x370, spline);
    SU32(spline, 0x4c, 1);
    SU32(e, 0x368, scene_x87c(U32(U32(spline, 0x44), 0x4c)));
    SU32(U32(e, 0x370), 0x4c, 0);
    float pos = rs_float("SplinePosition");
    SF32(e, 0x360, pos);
    if (0.0f > pos) SF32(e, 0x360, ((float)scene_rand() * 3.0518509447574615e-05f /* 0x92e054 */) * F32(e, 0x368));
    else if (pos > 1.0f) {
        float p = F32(e, 0x360);
        do { p = p - 1.0f; SF32(e, 0x360, p); } while (p > 1.0f);
    }
    SU32(e, 0x364, scene_x87c(U32(e, 0x360)));
    SU32(e, 0x384, rs_has("LockToLeader", 0) ? rs_bool("LockToLeader") : 0);
    SF32(e, 0x388, 0.0f); SU32(e, 0x39c, 0);
    uint32_t a1 = e + 0x350;
    scene_vcall(e, 0x44, 1, &a1);   /* vf17: the root position */
    RX_FRAME_ENTER(F, 0x40);
    uint32_t a2 = F;
    scene_vcall(e, 0x3c, 1, &a2);   /* vf15: the root matrix */
    float m[16];
    for (int k = 0; k < 16; k++) m[k] = F32(F, 4 * k);
    RX_FRAME_LEAVE(F, 0x40);
    SF32(e, 0x378, rx_sqrts((m[0] * m[0] + m[4] * m[4]) + m[8] * m[8]));
    SF32(e, 0x37c, rx_sqrts((m[1] * m[1] + m[5] * m[5]) + m[9] * m[9]));
    SF32(e, 0x380, rx_sqrts((m[2] * m[2] + m[6] * m[6]) + m[10] * m[10]));
    SU16(e, 0xb0, (U16(e, 0xb0) & 0xffef) | 8);
    if (U32(e, 0x3a0)) { fprintf(stderr, "rxcore: scene: AnimEntitySplineAnimLoop SoundActions (FUN_006baae0) is not built\n"); abort(); }
    return 0;
}
RX_FUNCTION(0x006ba690, rx_anim_spline_loop_vf5)

/* 006c39a0 / 006c3a00 (ESI = effect, stack node; RET 4): same holder as 0063bff0,
 * with the original YRing / Spherical emitter and effect vtables. */
rx_addr rx_attachable_ring_construct(rx_addr e, rx_addr node) {
    rx_rider_lens_effect_construct(e,node);
    SU32(e+0x10,0,0x0095e074); SU32(e,0,0x0095f804);
    return e;
}
RX_FUNCTION(0x006c39a0, rx_attachable_ring_construct)
rx_addr rx_attachable_sphere_construct(rx_addr e, rx_addr node) {
    rx_rider_lens_effect_construct(e,node);
    SU32(e+0x10,0,0x0095e080); SU32(e+0x10,0x9c,1); SU32(e,0,0x0095f820);
    return e;
}
RX_FUNCTION(0x006c3a00, rx_attachable_sphere_construct)

/* 006bbe20 AnimEntitySplineAnimLoopWithParticles::vf5: the normal spline loader,
 * named hierarchy node, and the scene's existing particle entity / emitter name. */
uint32_t rx_anim_spline_particles_vf5(rx_addr e, uint32_t reader) {
    rx_anim_spline_loop_vf5(e,reader);
    RX_FRAME_ENTER(F,0x210);
    const char* hierarchy=rs_str("HierarchyNode");
    uint32_t i=0;
    for(;hierarchy[i]&&i<0x103;i++) SU8(F,i,(uint8_t)hierarchy[i]);
    SU8(F,i,0);
    const rx_addr set=scene_vcall(e,0x70,0,NULL);
    const rx_addr node=hierarchy_find_node(set,F);
    const char* name=rs_str("EmitterName");
    for(i=0;name[i]&&i<0x103;i++) SU8(F+0x104,i,(uint8_t)name[i]);
    SU8(F+0x104,i,0);
    const char* type=rs_enum_text("EmitterType");
    const rx_addr effect_set=U32(0,0x00a94264+4*U32(U32(e,0x10),0x2cc4));
    if(effect_set&&node&&type) {
        rx_addr effect=0;
        if(!strcmp(type,"Box")) { effect=rx_reflex_aligned_alloc(0x110); if(effect) effect=rx_rider_lens_effect_construct(effect,node); }
        else if(!strcmp(type,"Ring")) { effect=rx_reflex_aligned_alloc(0x110); if(effect) effect=rx_attachable_ring_construct(effect,node); }
        else if(!strcmp(type,"Sphere")) { effect=rx_reflex_aligned_alloc(0x110); if(effect) effect=rx_attachable_sphere_construct(effect,node); }
        else { RX_FRAME_LEAVE(F,0x210); return 0; }
        SU32(e,0x790,effect);
        rx_fx_effect_bind(effect,effect_set,F+0x104);
    }
    RX_FRAME_LEAVE(F,0x210);
    return 0;
}
RX_FUNCTION(0x006bbe20, rx_anim_spline_particles_vf5)


/* ---------------------------------------------------------------------------------------------------------------------------
 * ScenarioAnimEntity (id 0x104f; 0x480 bytes): ctor 0074a810: CharacterEntity, vtables 0x0096cee4 / 0x0096cf64 / 0x0096cfa4, the CRITICAL_SECTION
 * at +0x3e0, floats at +0x350.. (1.0 at +0x350 / 0x364 / 0x378 / 0x38c, the rest 0.0), +0x144 / +0x1f4 0, +0x478 -1.0 ([0x009cd9e8]) */
/* 0074a810 (EAX = entity) */
rx_addr rx_scenario_anim_construct(rx_addr e) {
    rx_character_entity_construct(e);
    SU32(e, 0, 0x0096cee4); SU32(e, 0x90, 0x0096cf64); SU32(e, 0x140, 0x0096cfa4);
    rx_addr cs = e + 0x3e0;   /* InitializeCriticalSection as the reference boot leaves it */
    SU32(cs, 0, 0); SU32(cs, 4, 0xffffffffu); SU32(cs, 8, 0); SU32(cs, 0xc, 0); SU32(cs, 0x10, 0); SU32(cs, 0x14, 0);
    static const uint16_t z[] = {0x354, 0x358, 0x35c, 0x360, 0x368, 0x36c, 0x370, 0x374, 0x37c, 0x380, 0x384, 0x388};
    for (unsigned k = 0; k < sizeof z / sizeof *z; k++) SF32(e, z[k], 0.0f);
    SF32(e, 0x350, 1.0f); SF32(e, 0x364, 1.0f); SF32(e, 0x378, 1.0f); SF32(e, 0x38c, 1.0f);
    SU32(e, 0x144, 0); SU32(e, 0x1f4, 0);
    SU32(e, 0x478, U32(0, 0x009cd9e8));
    return e;
}
rx_addr rx_scenario_anim_create(void) { return rx_scenario_anim_construct(rx_reflex_aligned_alloc(0x480)); }
/* ScenarioAnimEntity::vf5 0074a8f0 */
uint32_t rx_scenario_anim_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    rx_addr model = rs_asset("ModelAsset");
    scene_character_load_model(e, model);
    rx_model_instance_init(e + 0xa0);
    rx_entity_vf5(e);
    scene_copy_words(e + 0xc0, e + 0x30, 16);
    SF32(e, 0x3d8, rs_float("ShadowRadius"));
    SF32(e, 0x3dc, rs_float("ShadowStretch"));
    SU32(e, 0x3d4, (uint32_t)rs_int("ShadowTexSelect"));
    SU16(e, 0xb0, (U16(e, 0xb0) & 0xffef) | 8);
    uint32_t a = e + 0x350;
    scene_vcall(e, 0x3c, 1, &a);   /* vf15: the root matrix into +0x350 */
    return 0;
}
RX_FUNCTION(0x0074a8f0, rx_scenario_anim_vf5)

/* ---------------------------------------------------------------------------------------------------------------------------
 * InstancedAnimationEntity (id 0x1051; 0xa70 bytes) */

/* the transposition of FUN_00401760 on byte-offset indexed locals: out[4 * (4 * k + i)] = in[4 * (4 * i + k)] */
static void scene_transpose4(float* in, float* out) {
    float t[16 * 4];
    for (int i = 0; i < 4; i++) for (int k = 0; k < 4; k++) t[4 * (4 * k + i)] = in[4 * (4 * i + k)];
    for (int j = 0; j < 16; j++) out[4 * j] = t[4 * j];
}

/* The instance transform of an entity whose (model, animation) pair has a master (InstancedAnimationEntity::vf5 0041253f .. 004131ef, the
 * float code translated line by line from the disassembly): the inverse of the master's root matrix (cofactor expansion, 401760 transpose,
 * scaled by 1 / determinant) multiplied by this entity's matrix. st[] is the original's frame (byte-offset indexed): the master's matrix
 * is at st[0x40 ..], the product ends at st[0xd0 ..]. */
static void instanced_transform(rx_addr e, float* st) {
    float x0, x1, x2, x3, x4, x5, x6, x7;
    x6 = st[124];
    x3 = st[120];
    x4 = st[104];
    x2 = st[108];
    x5 = st[92];
    x0 = x6;
    x1 = x3;
    x1 = x1 * x2;
    x2 = x2 * st[88];
    x7 = x3;
    x3 = x3 * st[76];
    x7 = x7 * x5;
    x0 = x0 * x4;
    x0 = x0 - x1;
    x1 = x6;
    x6 = x6 * st[72];
    x1 = x1 * st[88];
    x1 = x1 - x7;
    x6 = x6 - x3;
    x3 = st[108];
    x3 = x3 * st[72];
    x4 = x4 * x5;
    x2 = x2 - x4;
    x4 = st[84];
    st[20] = x6;
    x6 = x6 * st[100];
    x5 = x0;
    x5 = x5 * x4;
    x7 = x1;
    x7 = x7 * st[100];
    x5 = x5 - x7;
    st[12] = x3;
    x3 = st[104];
    x3 = x3 * st[76];
    x7 = x2;
    x7 = x7 * st[116];
    x5 = x5 + x7;
    x7 = st[12];
    x7 = x7 - x3;
    x3 = st[68];
    st[40] = x7;
    x7 = x0;
    x7 = x7 * x3;
    st[16] = x7;
    x7 = x7 - x6;
    x6 = st[40];
    x6 = x6 * st[116];
    st[16] = x7;
    x7 = x7 + x6;
    x6 = st[72];
    x6 = x6 * st[92];
    st[12] = x6;
    x6 = st[88];
    x6 = x6 * st[76];
    st[24] = x7;
    x7 = st[12];
    x7 = x7 - x6;
    st[36] = x7;
    x7 = st[20];
    x7 = x7 * x4;
    x6 = x1;
    x6 = x6 * x3;
    x6 = x6 - x7;
    st[12] = x6;
    x6 = st[36];
    x6 = x6 * st[116];
    x7 = st[12];
    x7 = x7 + x6;
    st[140] = x5;
    st[32] = x7;
    x7 = st[40];
    x7 = x7 * x4;
    x6 = x2;
    x6 = x6 * x3;
    x6 = x6 - x7;
    st[12] = x6;
    x7 = st[12];
    x6 = st[36];
    x6 = x6 * st[100];
    x7 = x7 + x6;
    x6 = x5;
    x6 = x6 * st[64];
    x5 = st[24];
    x5 = x5 * st[80];
    x6 = x6 - x5;
    x5 = st[32];
    x5 = x5 * st[96];
    x6 = x6 + x5;
    x5 = x7;
    x5 = x5 * st[112];
    x6 = x6 - x5;
    x5 = F32(0, 0x00931f24);
    x5 = x5 / x6;
    st[48] = x5;
    x5 = st[140];
    st[208] = x5;
    x5 = x0;
    x5 = x5 * st[80];
    x6 = x1;
    x6 = x6 * st[96];
    x5 = x5 - x6;
    x6 = x2;
    x6 = x6 * st[112];
    x5 = x5 + x6;
    x6 = F32(0, 0x009774a8);
    x6 = x6 - x5;
    x5 = st[124];
    x5 = x5 * st[100];
    st[212] = x6;
    x6 = st[116];
    x6 = x6 * st[108];
    x5 = x5 - x6;
    x6 = st[124];
    x6 = x6 * x4;
    st[12] = x6;
    x6 = st[116];
    x6 = x6 * st[92];
    st[52] = x7;
    x7 = st[12];
    x7 = x7 - x6;
    st[28] = x7;
    x6 = x4;
    x6 = x6 * st[108];
    st[12] = x6;
    x6 = st[100];
    x6 = x6 * st[92];
    x7 = st[12];
    x7 = x7 - x6;
    x6 = st[28];
    x6 = x6 * st[96];
    st[44] = x5;
    x5 = x5 * st[80];
    x5 = x5 - x6;
    x6 = x7;
    x6 = x6 * st[112];
    x5 = x5 + x6;
    x6 = st[116];
    x6 = x6 * st[104];
    st[216] = x5;
    x5 = st[120];
    x5 = x5 * st[100];
    x5 = x5 - x6;
    x6 = st[120];
    x6 = x6 * x4;
    st[12] = x6;
    x6 = st[116];
    x6 = x6 * st[88];
    st[56] = x7;
    x7 = st[12];
    st[16] = x5;
    x7 = x7 - x6;
    st[60] = x7;
    x6 = x4;
    x6 = x6 * st[104];
    st[12] = x6;
    x7 = st[12];
    x6 = st[100];
    x6 = x6 * st[88];
    x7 = x7 - x6;
    st[140] = x7;
    x6 = x5;
    x6 = x6 * st[80];
    x5 = st[60];
    x5 = x5 * st[96];
    x6 = x6 - x5;
    x5 = x7;
    x5 = x5 * st[112];
    x6 = x6 + x5;
    x5 = F32(0, 0x009774a8);
    x7 = x5;
    x7 = x7 - x6;
    x5 = x5 - st[24];
    x6 = st[20];
    x6 = x6 * st[96];
    st[220] = x7;
    x7 = st[116];
    st[224] = x5;
    x5 = st[64];
    x0 = x0 * x5;
    x0 = x0 - x6;
    x6 = st[40];
    x6 = x6 * st[112];
    x0 = x0 + x6;
    x6 = st[76];
    x7 = x7 * x6;
    st[228] = x0;
    x0 = st[124];
    x0 = x0 * x3;
    x0 = x0 - x7;
    x7 = st[100];
    x7 = x7 * x6;
    x6 = st[44];
    x6 = x6 * x5;
    st[24] = x0;
    x0 = x3;
    x0 = x0 * st[108];
    x0 = x0 - x7;
    x7 = st[24];
    x7 = x7 * st[96];
    x6 = x6 - x7;
    st[284] = x0;
    x0 = x0 * st[112];
    x6 = x6 + x0;
    x0 = F32(0, 0x009774a8);
    x7 = x0;
    x7 = x7 - x6;
    x6 = st[120];
    x6 = x6 * x3;
    st[12] = x6;
    x6 = st[116];
    x6 = x6 * st[72];
    st[232] = x7;
    x7 = st[12];
    x7 = x7 - x6;
    st[44] = x7;
    x6 = x3;
    x6 = x6 * st[104];
    st[12] = x6;
    x6 = st[100];
    x6 = x6 * st[72];
    x7 = st[12];
    x7 = x7 - x6;
    x6 = st[16];
    x6 = x6 * x5;
    st[16] = x6;
    x6 = st[44];
    x6 = x6 * st[96];
    st[12] = x7;
    x7 = st[16];
    x7 = x7 - x6;
    st[16] = x7;
    x7 = st[12];
    x7 = x7 * st[112];
    x6 = st[16];
    x6 = x6 + x7;
    st[236] = x6;
    x6 = st[32];
    st[240] = x6;
    x6 = st[20];
    x6 = x6 * st[80];
    x1 = x1 * x5;
    x1 = x1 - x6;
    x6 = st[36];
    x6 = x6 * st[112];
    x1 = x1 + x6;
    x6 = x0;
    x6 = x6 - x1;
    st[244] = x6;
    x6 = x4;
    x6 = x6 * st[76];
    x4 = x4 * st[72];
    x1 = x3;
    x1 = x1 * st[92];
    x3 = x3 * st[88];
    x1 = x1 - x6;
    x6 = st[28];
    x6 = x6 * x5;
    st[16] = x6;
    x6 = st[24];
    x6 = x6 * st[80];
    x7 = st[16];
    x7 = x7 - x6;
    st[16] = x7;
    x6 = st[16];
    x3 = x3 - x4;
    x4 = st[60];
    x4 = x4 * x5;
    x7 = x1;
    x7 = x7 * st[112];
    x6 = x6 + x7;
    x7 = st[44];
    st[248] = x6;
    x6 = st[80];
    x7 = x7 * x6;
    x4 = x4 - x7;
    x7 = x3;
    x7 = x7 * st[112];
    x4 = x4 + x7;
    x7 = x0;
    x7 = x7 - x4;
    x4 = x0;
    x4 = x4 - st[52];
    st[256] = x4;
    x4 = st[40];
    x2 = x2 * x5;
    x4 = x4 * x6;
    x2 = x2 - x4;
    x4 = st[36];
    st[252] = x7;
    x7 = st[96];
    x4 = x4 * x7;
    x2 = x2 + x4;
    x4 = st[284];
    st[260] = x2;
    x2 = st[56];
    x2 = x2 * x5;
    x1 = x1 * x7;
    x4 = x4 * x6;
    x2 = x2 - x4;
    x2 = x2 + x1;
    x1 = st[12];
    x0 = x0 - x2;
    st[264] = x0;
    x0 = st[140];
    x0 = x0 * x5;
    x1 = x1 * x6;
    x0 = x0 - x1;
    x3 = x3 * x7;
    x0 = x0 + x3;
    st[268] = x0;
    scene_transpose4(st + 208, st + 144);
    x0 = st[48];
    x1 = st[160];
    x1 = x1 * x0;
    st[160] = x1;
    x1 = st[164];
    x1 = x1 * x0;
    st[164] = x1;
    x1 = st[168];
    x1 = x1 * x0;
    st[168] = x1;
    x1 = st[172];
    x1 = x1 * x0;
    st[172] = x1;
    x1 = st[176];
    x1 = x1 * x0;
    st[176] = x1;
    x1 = st[180];
    x1 = x1 * x0;
    st[180] = x1;
    x1 = st[184];
    x1 = x1 * x0;
    x5 = st[148];
    x2 = F32(e, 0x50);
    x7 = F32(e, 0x40);
    x6 = st[152];
    x3 = st[156];
    st[184] = x1;
    x1 = st[188];
    x4 = st[144];
    x1 = x1 * x0;
    st[188] = x1;
    x1 = st[192];
    x1 = x1 * x0;
    st[192] = x1;
    x1 = st[196];
    x1 = x1 * x0;
    st[196] = x1;
    x1 = st[200];
    x1 = x1 * x0;
    st[200] = x1;
    x1 = st[204];
    x1 = x1 * x0;
    st[44] = x7;
    st[204] = x1;
    x1 = F32(e, 0x60);
    st[60] = x2;
    st[28] = x1;
    x5 = x5 * x0;
    x7 = x7 * x5;
    x6 = x6 * x0;
    x2 = x2 * x6;
    x7 = x7 + x2;
    x2 = F32(e, 0x54);
    x3 = x3 * x0;
    x1 = x1 * x3;
    x7 = x7 + x1;
    x4 = x4 * x0;
    x0 = F32(e, 0x30);
    x1 = x0;
    x1 = x1 * x4;
    x7 = x7 + x1;
    x1 = F32(e, 0x34);
    st[40] = x1;
    x1 = F32(e, 0x64);
    st[208] = x7;
    st[32] = x2;
    st[36] = x1;
    x1 = F32(e, 0x44);
    x2 = x2 * x6;
    x7 = x1;
    x7 = x7 * x5;
    x7 = x7 + x2;
    x2 = st[36];
    x2 = x2 * x3;
    x7 = x7 + x2;
    x2 = st[40];
    x2 = x2 * x4;
    x7 = x7 + x2;
    st[148] = x5;
    st[156] = x3;
    st[212] = x7;
    x2 = F32(e, 0x38);
    st[20] = x2;
    x2 = F32(e, 0x68);
    st[52] = x2;
    x2 = F32(e, 0x58);
    st[56] = x2;
    x2 = F32(e, 0x48);
    x7 = x2;
    x7 = x7 * x5;
    x5 = st[56];
    x5 = x5 * x6;
    x7 = x7 + x5;
    x5 = st[52];
    x5 = x5 * x3;
    x3 = st[20];
    x3 = x3 * x4;
    x7 = x7 + x5;
    x5 = F32(e, 0x5c);
    x7 = x7 + x3;
    x3 = F32(e, 0x3c);
    st[48] = x3;
    x3 = F32(e, 0x6c);
    st[216] = x7;
    st[16] = x3;
    x3 = F32(e, 0x4c);
    st[24] = x5;
    x5 = x5 * x6;
    x7 = x3;
    x7 = x7 * st[148];
    x7 = x7 + x5;
    x5 = st[16];
    x5 = x5 * st[156];
    x7 = x7 + x5;
    x5 = st[48];
    x6 = x5;
    x6 = x6 * x4;
    x7 = x7 + x6;
    x6 = st[44];
    x6 = x6 * st[164];
    x4 = x0;
    x4 = x4 * st[160];
    x4 = x4 + x6;
    x6 = st[60];
    x6 = x6 * st[168];
    x4 = x4 + x6;
    x6 = st[28];
    x6 = x6 * st[172];
    x4 = x4 + x6;
    x6 = st[32];
    x6 = x6 * st[168];
    st[224] = x4;
    x4 = x1;
    x4 = x4 * st[164];
    x4 = x4 + x6;
    x6 = st[36];
    x6 = x6 * st[172];
    x4 = x4 + x6;
    x6 = st[40];
    x6 = x6 * st[160];
    x4 = x4 + x6;
    x6 = st[56];
    x6 = x6 * st[168];
    st[228] = x4;
    x4 = x2;
    x4 = x4 * st[164];
    x4 = x4 + x6;
    x6 = st[52];
    x6 = x6 * st[172];
    x4 = x4 + x6;
    x6 = st[20];
    x6 = x6 * st[160];
    x4 = x4 + x6;
    x6 = st[24];
    x6 = x6 * st[168];
    st[232] = x4;
    x4 = x3;
    x4 = x4 * st[164];
    x4 = x4 + x6;
    x6 = st[16];
    x6 = x6 * st[172];
    st[220] = x7;
    x4 = x4 + x6;
    x6 = x5;
    x6 = x6 * st[160];
    x4 = x4 + x6;
    x6 = st[44];
    x6 = x6 * st[180];
    st[236] = x4;
    x4 = x0;
    x4 = x4 * st[176];
    x4 = x4 + x6;
    x6 = st[60];
    x6 = x6 * st[184];
    x4 = x4 + x6;
    x6 = st[28];
    x6 = x6 * st[188];
    x4 = x4 + x6;
    x6 = st[32];
    x6 = x6 * st[184];
    st[240] = x4;
    x4 = x1;
    x4 = x4 * st[180];
    x4 = x4 + x6;
    x6 = st[36];
    x6 = x6 * st[188];
    x4 = x4 + x6;
    x6 = st[40];
    x6 = x6 * st[176];
    x4 = x4 + x6;
    x6 = st[56];
    x6 = x6 * st[184];
    st[244] = x4;
    x7 = st[28];
    x1 = x1 * st[196];
    x4 = x2;
    x4 = x4 * st[180];
    x4 = x4 + x6;
    x6 = st[52];
    x6 = x6 * st[188];
    x4 = x4 + x6;
    x6 = st[20];
    x6 = x6 * st[176];
    x4 = x4 + x6;
    x6 = st[24];
    x6 = x6 * st[184];
    st[248] = x4;
    x4 = x3;
    x4 = x4 * st[180];
    x4 = x4 + x6;
    x6 = st[16];
    x6 = x6 * st[188];
    x4 = x4 + x6;
    x6 = x5;
    x6 = x6 * st[176];
    x4 = x4 + x6;
    x6 = st[44];
    x6 = x6 * st[196];
    st[252] = x4;
    x4 = st[192];
    x0 = x0 * x4;
    x0 = x0 + x6;
    x6 = st[60];
    x6 = x6 * st[200];
    x0 = x0 + x6;
    x6 = st[204];
    x7 = x7 * x6;
    x0 = x0 + x7;
    x7 = st[200];
    st[256] = x0;
    x0 = st[32];
    x0 = x0 * x7;
    x1 = x1 + x0;
    x0 = st[36];
    x0 = x0 * x6;
    x1 = x1 + x0;
    x0 = st[40];
    x0 = x0 * x4;
    x1 = x1 + x0;
    x0 = st[56];
    st[260] = x1;
    x1 = st[196];
    x2 = x2 * x1;
    x0 = x0 * x7;
    x2 = x2 + x0;
    x0 = st[52];
    x0 = x0 * x6;
    x2 = x2 + x0;
    x0 = st[20];
    x0 = x0 * x4;
    x2 = x2 + x0;
    x0 = st[24];
    x3 = x3 * x1;
    x0 = x0 * x7;
    x3 = x3 + x0;
    x0 = st[16];
    x0 = x0 * x6;
    x3 = x3 + x0;
    x5 = x5 * x4;
    x3 = x3 + x5;
    
    st[0x108] = x2; st[0x10c] = x3;
    (void)x0; (void)x1; (void)x2; (void)x3; (void)x4; (void)x5; (void)x6; (void)x7;
}

/* the light-receiver list of a scene (scene + 0x1a18 + 0x10c8: records of 0x20 bytes, direction / position, entity at +0x10, range): FUN_007f9e70
 * (EAX = list {data, count, capacity, growth}, ESI = record): appended unless the entity is already in the list; grown by FUN_007fa340 */
uint32_t scene_light_list_add(rx_addr list, rx_addr rec) {
    for (int32_t i = 0; i < I32(list, 4); i++) if (U32(U32(list, 0) + 0x20u * (uint32_t)i, 0x10) == U32(rec, 0x10)) return 0;
    if (I32(list, 4) >= I32(list, 8)) {
        uint32_t n = U32(list, 0xc) + U32(list, 8);
        rx_addr nb = rx_reflex_aligned_alloc(rx_mul_sat(n, 0x20));
        if (nb) for (uint32_t k = 0; k < n; k++) {   /* Fn_007d1360 */
            rx_addr el = nb + 0x20 * k;
            SU32(el, 0x10, 0); SF32(el, 0x14, 0.0f);
            SU32(el, 0, U32(0, 0x00a95d50)); SU32(el, 4, U32(0, 0x00a95d54)); SU32(el, 8, U32(0, 0x00a95d58));
        }
        for (uint32_t k = 0; k < U32(list, 8) * 0x20; k += 4) SU32(nb, k, U32(U32(list, 0), k));
        rx_reflex_aligned_free(U32(list, 0));
        SU32(list, 0, nb);
        SU32(list, 8, n);
    }
    uint32_t c = U32(list, 4);
    rx_addr el = U32(list, 0) + 0x20 * c;
    SU32(list, 4, c + 1);
    SU32(el, 0, scene_x87c(U32(rec, 0))); SU32(el, 4, scene_x87c(U32(rec, 4))); SU32(el, 8, scene_x87c(U32(rec, 8)));
    SU32(el, 0x10, U32(rec, 0x10));
    SU32(el, 0x14, scene_x87c(U32(rec, 0x14))); SU32(el, 0x18, scene_x87c(U32(rec, 0x18)));
    return 1;                                                 /* EAX: 1 when added, 0 when the entity was in the list */
}
RX_FUNCTION(0x007f9e70, scene_light_list_add)
/* The same add with the original's frames (e = its return slot; EBX / EBP / EDI the caller's: Lights_Add's lights, frame pointer,
 * entity), for Lights_Add whose next event record copies a never-written local (dead stack). 007f9e70: push ebx, edi (EBX = the
 * last compared record id); the grow 007fa340 (n; return 007f9ea3): push ebp, esi; ReflexAlignedAlloc (return 007fa35e: push esi;
 * the allocator lock around it when 0xda67d8 is set; push 0x10, size; call [_aligned_malloc], return 0078fc14); the element
 * constructor iterator 004022d0 (stdcall nb, 0x20, n, 007d1360; return 007fa375: push edi, ebx, ebp, esi; each element's call
 * returns 004022f4); memcpy (return 007fa389); ReflexAlignedFree 0078fc50 (return 007fa391: push esi; push p; call [_aligned_free],
 * return 0078fc82). In the oracle the heap calls run the originals at the original's ESP. */
static void align_lock_frames(rx_addr r, uint32_t first, uint32_t second) {   /* the lock's two EnterCriticalSection pushes */
    if (!U8(0, 0x00da67d8)) return;
    SU32(r, -8, 0x00d61704u); SU32(r, -0xc, first); SU32(r, -8, 0x00d61704u); SU32(r, -0xc, second);
}
uint32_t scene_light_list_add_fr(rx_addr e, uint32_t ebx, uint32_t ebp, uint32_t edi, rx_addr list, rx_addr rec) {
    SU32(e, -4, ebx); SU32(e, -8, edi);
    if (I32(list, 4) > 0) ebx = U32(rec, 0x10);
    for (int32_t i = 0; i < I32(list, 4); i++) if (U32(U32(list, 0) + 0x20u * (uint32_t)i, 0x10) == U32(rec, 0x10)) return 0;
    if (I32(list, 4) >= I32(list, 8)) {
        const uint32_t n = U32(list, 0xc) + U32(list, 8), size = rx_mul_sat(n, 0x20);
        SU32(e, -0xc, n); SU32(e, -0x10, 0x007f9ea3u);
        const rx_addr G = e - 0x10, sp = rx_sp();
        SU32(G, -4, ebp); SU32(G, -8, rec);
        SU32(G, -0xc, size); SU32(G, -0x10, 0x007fa35eu);
        const rx_addr r = G - 0x10;
        SU32(r, -4, rec);
        align_lock_frames(r, 0x0078fbf7u, 0x0078fc07u);
        SU32(r, -8, 0x10); SU32(r, -0xc, size);
        rx_set_sp(r - 4);
        const rx_addr nb = rx_reflex_aligned_alloc(size);
        SU32(r, -0x10, 0x0078fc14u);
        if (U8(0, 0x00da67d8)) { SU32(r, -8, edi); SU32(r, -0xc, 0x00d61704u); SU32(r, -0x10, 0x0078fc30u); SU32(r, -0x10, 0x0078fc40u); }
        if (nb) {
            SU32(G, -0xc, 0x007d1360u); SU32(G, -0x10, n); SU32(G, -0x14, 0x20); SU32(G, -0x18, nb); SU32(G, -0x1c, 0x007fa375u);
            const rx_addr V = G - 0x1c;
            SU32(V, -4, list);
            if (n > 0) { SU32(V, -8, ebx); SU32(V, -0xc, n); SU32(V, -0x10, nb); SU32(V, -0x14, 0x004022f4u); }
            for (uint32_t k = 0; k < n; k++) {   /* 007d1360 */
                rx_addr el = nb + 0x20 * k;
                SU32(el, 0x10, 0); SF32(el, 0x14, 0.0f);
                SU32(el, 0, U32(0, 0x00a95d50)); SU32(el, 4, U32(0, 0x00a95d54)); SU32(el, 8, U32(0, 0x00a95d58));
            }
        }
        const rx_addr old = U32(list, 0);
        SU32(G, -0xc, U32(list, 8) * 0x20); SU32(G, -0x10, old); SU32(G, -0x14, nb); SU32(G, -0x18, 0x007fa389u);
        { const uint32_t cnt = U32(list, 8) * 0x20; rx_set_sp(G - 8);   /* (memcpy's frame at the original's: ESP G - 8 before its pushes) */
          CRT_WITH_REGS(ebx, n, nb, list, crt_memcpy(nb, old, cnt)); }
        SU32(G, -0x18, old); SU32(G, -0x1c, 0x007fa391u);           /* push p (over memcpy's return slot); call */
        const rx_addr F = G - 0x1c;
        SU32(F, -4, nb);
        align_lock_frames(F, 0x0078fc67u, 0x0078fc77u);
        SU32(F, -8, old);
        rx_set_sp(F - 4);
        rx_reflex_aligned_free(old);
        SU32(F, -0xc, 0x0078fc82u);
        if (U8(0, 0x00da67d8)) { SU32(F, -8, 0x00d61704u); SU32(F, -0xc, 0x0078fc9bu); SU32(F, -0xc, 0x0078fcabu); }
        rx_set_sp(sp);
        SU32(list, 0, nb);
        SU32(list, 8, n);
    }
    uint32_t c = U32(list, 4);
    rx_addr el = U32(list, 0) + 0x20 * c;
    SU32(list, 4, c + 1);
    SU32(el, 0, scene_x87c(U32(rec, 0))); SU32(el, 4, scene_x87c(U32(rec, 4))); SU32(el, 8, scene_x87c(U32(rec, 8)));
    SU32(el, 0x10, U32(rec, 0x10));
    SU32(el, 0x14, scene_x87c(U32(rec, 0x14))); SU32(el, 0x18, scene_x87c(U32(rec, 0x18)));
    return 1;
}

/* the masters table at 0x00a95d88: entries of 0x10 bytes {animation handle, model handle, master entity, +0xa68}, the count at 0x00a96d88, 0x100 slots */
#define MASTERS 0x00a95d88u
#define MASTER_COUNT 0x00a96d88u

/* 00412380 (EAX = e): Entity fields, IRenderable 0x93708c at +0x90 (+0x94 0xf0, +0x98 0x009370cc), vtables 0x00939d24 / +0x90 0x00939d9c /
 * +0x98 0x00939ddc, bytes +0x9e0 / +0x9e1 / +0xa6c 0, +0xa68 1, +0xa64 0.0 */
rx_addr rx_instanced_anim_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0xa70);
    rx_entity_construct(e, 0x00939d24);
    SU32(e, 0x90, 0x0093708c); SU32(e, 0x94, 0xf0); SU32(e, 0x98, 0x009370cc);
    SU32(e, 0x90, 0x00939d9c); SU32(e, 0x98, 0x00939ddc);
    SU8(e, 0x9e0, 0); SU8(e, 0x9e1, 0); SU8(e, 0xa6c, 0); SU32(e, 0xa68, 1); SF32(e, 0xa64, 0.0f);
    return e;
}

/* InstancedAnimationEntity::vf5 00412480 (ECX = e; RET 4). Entities sharing a (LoopingAnimation, ModelAsset) pair share one master (an
 * AnimEntitySingleAnimLoop made from the first one's RSData, +0x9f0 of that entity, registered in the masters table and in the scene's
 * light-receiver list); the others keep it in +0x9f4. Every entity gets its instance transform (+0xa00: its matrix times the inverse of the
 * master's root matrix) and its box (+0xa40 centre, +0xa50 extents, +0xa60 radius, moved by the entity's matrix). Whether the model's
 * materials know the shader constant g_InstanceTransform (which sets +0xa68 = 0 and registers the transform as a shader parameter) is a
 * render fact: the core does not build material effects and keeps +0xa68 = 1 (true for the Concord crowd). */
uint32_t rx_instanced_anim_vf5(rx_addr e, uint32_t reader) {
    rx_entity_vf5(e);
    rx_addr anim = rs_asset("LoopingAnimation"), model = rs_asset("ModelAsset");
    uint32_t n = U32(0, MASTER_COUNT), idx = 0;
    for (; idx < n; idx++) if (U32(0, MASTERS + 0x10 * idx) == anim && U32(0, MASTERS + 0x10 * idx + 4) == model) break;
    rx_addr master;
    if (idx < n) {
        SU32(e, 0x9f0, 0);
        master = U32(0, MASTERS + 0x10 * idx + 8);
        SU32(e, 0x9f4, master);
        SU32(e, 0xa68, U32(0, MASTERS + 0x10 * idx + 0xc));
        float st[0x120];
        memset(st, 0, sizeof st);
        RX_FRAME_ENTER(F, 0x40);
        uint32_t a = F;
        scene_vcall(master, 0x3c, 1, &a);   /* vf15: the master's root matrix */
        for (int k = 0; k < 16; k++) st[0x40 + 4 * k] = F32(F, 4 * k);
        RX_FRAME_LEAVE(F, 0x40);
        instanced_transform(e, st);
        SU32(e, 0x70, 0);   /* (an entity with a master is not in the sorted lists) */
        for (int k = 0; k < 16; k++) SF32(e, 0xa00 + 4 * k, st[0xd0 + 4 * k]);
    } else {
        /* no master yet: the model's materials are searched for the shader constant (render fact: not built, +0xa68 stays 1) */
        SU32(e, 0x70, 1);
        SU32(e, 0x9f4, 0);
        master = rx_reflex_aligned_alloc(0x750);
        anim_loop_construct(master, 0x0095f674, 0x0095f6f4, 0x0095f734);
        SU32(e, 0x9f0, master);
        scene_vcall(master, 0x14, 1, &reader);   /* master vf5: the same RSData */
        rx_addr scene = U32(e, 0x10);
        RX_FRAME_ENTER(F, 0x20);
        SU32(F, 0, U32(0, 0x00a95d50)); SU32(F, 4, U32(0, 0x00a95d54)); SU32(F, 8, U32(0, 0x00a95d58));
        SU32(F, 0x10, master); SF32(F, 0x14, 0.0f); SF32(F, 0x18, 15000.0f /* 0x977564 */);
        scene_light_list_add(scene + 0x1a18 + 0x10c8, F);
        RX_FRAME_LEAVE(F, 0x20);
        SU32(master, 0x10, scene);
        if (U32(0, MASTER_COUNT) < 0x100) {
            uint32_t c = U32(0, MASTER_COUNT);
            SU32(0, MASTERS + 0x10 * c, U32(master, 0x144)); SU32(0, MASTERS + 0x10 * c + 4, model);
            SU32(0, MASTERS + 0x10 * c + 8, master); SU32(0, MASTERS + 0x10 * c + 0xc, U32(e, 0xa68));
            SU32(0, MASTER_COUNT, c + 1);
        }
        SU32(e, 0x9f4, 0);
        SU32(master, 0x740, 1); SU32(master, 0x744, 1);
        static const float id[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
        for (int k = 0; k < 16; k++) SF32(e, 0xa00 + 4 * k, id[k]);
    }
    if (U32(e, 0xa68) == 0) { fprintf(stderr, "rxcore: scene: g_InstanceTransform shader parameter is not built\n"); abort(); }
    rx_addr m = U32(e, 0x9f0) ? U32(e, 0x9f0) : U32(e, 0x9f4);
    scene_model_bounds(master + 0x200, master + 0xa0, e + 0xa40, e + 0xa50, e + 0xa60);
    float x = F32(e, 0xa40), y = F32(e, 0xa44), z = F32(e, 0xa48);
    float nx = ((F32(e, 0x40) * y + F32(e, 0x50) * z) + F32(e, 0x30) * x) + F32(e, 0x60);
    float ny = ((F32(e, 0x34) * x + F32(e, 0x44) * y) + F32(e, 0x54) * z) + F32(e, 0x64);
    float nz = ((F32(e, 0x38) * x + F32(e, 0x48) * y) + F32(e, 0x58) * z) + F32(e, 0x68);
    SF32(e, 0xa48, nz); SF32(e, 0xa44, ny); SF32(e, 0xa40, nx);
    rx_addr sc = U32(U32(0, 0x00a95844), 0x1c + 4 * U32(U32(0, 0x00a95844), 4));
    uint32_t q = U32(m, 0x320) / U32(sc, 0x19d0);
    SF32(e, 0xa64, (float)q);
    return 0;
}
RX_FUNCTION(0x00412480, rx_instanced_anim_vf5)
