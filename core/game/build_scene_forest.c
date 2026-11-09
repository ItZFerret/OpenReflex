/* build_scene_forest.c -- ForestSystemEntity (id 0x100f, 0x31f0 bytes): the trees of a level. Constructor 00662420, vf5 00662930
 * (ForestAsset + ForestSystem_Init 006626c0: the per-type instance lists), vf7 00662980 (the collision info 0x1d4 object at +0x31ec),
 * and what the physics world does with it: FUN_00772940 (the world's forest = the scene's first ForestSystem) and FUN_00667cc0 (the
 * trunk collision shapes of every tree type).
 * Layout: Entity (0x90), IRenderable +0x90 / IRenderEvent +0x98, +0x9c the ForestAsset handle (asset data A = handle+0x44: tree types
 * at A + 0x1d4f4 + 0x5f0 k, their count at A + 0x24ad0), +0xa8 + 0x264 k the per-type instance records, +0x3070 the type count, +0x3074 /
 * +0x3078 the nearest billboard start / farthest 3D distance of all types, +0x3084 + 0x10 k the per-type list capacities (+0x307c /
 * +0x3080 their two arrays), +0x31bc the instance storage block (+0x31c0 its size), +0x31d0 the shadow render event (vtable 0x95b654),
 * +0x31ec the collision info object.
 * Not built (render / OS): the three events "ForestSystemStart / End / Kill" and the culling thread the constructor starts (+0x31d8 ..
 * +0x31e8 stay 0; the thread culls the tree instances for the renderer), and vf7's colour sampling (ForestAsset_SampleColorRefs from the
 * AmbientDirectionalLight's colour reference map: tints the trees of the asset, render only). */
#include "phys_shape_dtors.h"
#include "build_scene_int.h"


static void fatal(const char* what, uint32_t a) { fprintf(stderr, "rxcore: forest: %s %08x\n", what, a); abort(); }

/* 00667b60 (ESI = the 0x1d4 block): 0x28 empty CollisionShapeHandles (8 bytes), the tree radius cell 20.0, grid fields */
static rx_addr collision_info_init(rx_addr o) {
    for (uint32_t i = 0; i < 0x28; i++) { SU32(o, 0x20 + 8 * i + 4, 0); SU32(o, 0x20 + 8 * i, 0x009765a8); }
    SF32(o, 0x14, 20.0f /* 0x935aac */);
    SU32(o, 0, 0); SU32(o, 4, 0); SU32(o, 0xc, 0x14); SU32(o, 0x10, 0x190);
    for (uint32_t k = 0x160; k < 0x188; k += 4) SU32(o, k, 0);
    return o;
}

rx_addr rx_forest_system_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0x31f0);
    rx_entity_construct(e, 0x0095b594);
    SU32(e, 0x90, 0x0093708c); SU32(e, 0x94, 0xf0); SU32(e, 0x98, 0x009370cc);
    SU32(e, 0x90, 0x0095b60c); SU32(e, 0x98, 0x0095b64c);
    SU32(e, 0x31d0, 0x0095b654);
    SU32(e, 0x31d8, 0); SU32(e, 0x31dc, 0); SU32(e, 0x31e0, 0); SU32(e, 0x31e4, 0);   /* (events and thread: not built) */
    SU32(e, 0x31d4, e);
    rx_addr o = rx_reflex_aligned_alloc(0x1d4);
    SU32(e, 0x31ec, o ? collision_info_init(o) : 0);
    return e;
}

/* ForestSystem_Init 006626c0 (ESI = entity) */
static void forest_init(rx_addr e) {
    rx_addr handle = U32(e, 0x9c);
    SU32(e, 0x70, 1);
    SU32(e, 0x31c4, 1); SU32(e, 0x31c8, 0); SU32(e, 0x31cc, 0);
    SU32(handle, 0x4c, 1);
    rx_addr A = U32(handle, 0x44);
    SU32(e, 0x3074, 0x7f7fffff); SU32(e, 0x3078, 0xff7fffff);
    SU32(e, 0x31c0, 0);
    int32_t n = I32(A, 0x24ad0);
    SU32(e, 0x3070, (uint32_t)n);
    for (int32_t k = 0; k < n; k++) {
        rx_addr rec = e + 0xa8 + 0x264u * (uint32_t)k;      /* the instance record; rec + 0x70 the sub-list count */
        rx_addr T = A + 0x1d4f4 + 0x5f0u * (uint32_t)k;      /* the type in the asset */
        rx_addr cap = e + 0x3084 + 0x10u * (uint32_t)k;
        SU32(rec, 0x74, 0);
        int32_t subs = I32(T, -4);
        SU32(rec, 0x70, (uint32_t)subs);
        for (int32_t i = 0; i < subs; i++) {
            SU32(rec, 12u * (uint32_t)i, 0);
            uint32_t c = U32(T, 0);
            SU32(rec, 12u * (uint32_t)i - 4, c);
            SU32(e, 0x31c0, U32(e, 0x31c0) + c * 8);
        }
        float mn = F32(T, -0x4b0), cur = F32(e, 0x3074);
        SF32(e, 0x3074, mn > cur ? cur : mn);
        float far = F32(e, 0x3078), val = F32(T, (uint32_t)subs * 0x70u - 0x4c0u);
        SF32(e, 0x3078, far > val ? far : val);
        int32_t c = I32(T, 0);
        if (c < 0x14) c = 0x14;
        SU32(cap, 0, (uint32_t)c);
        SU32(cap, 4, 0);
        SU32(e, 0x31c0, (U32(e, 0x31c0) + 0xf) & 0xfffffff0u);
        SU32(e, 0x31c0, U32(cap, 0) * 0x20u + U32(e, 0x31c0));
    }
    rx_addr block = rx_crt_aligned_malloc(U32(e, 0x31c0), 16);
    SU32(e, 0x31bc, block);
    rx_addr p = block;
    for (int32_t k = 0; k < n; k++) {
        rx_addr rec = e + 0xa8 + 0x264u * (uint32_t)k;
        rx_addr cap = e + 0x3084 + 0x10u * (uint32_t)k;
        SU32(rec, 0x78, 0);
        for (int32_t i = 0; i < I32(rec, 0x70); i++) {
            rx_addr ent = rec - 8 + 12u * (uint32_t)i;   /* {+0 pointer, +4 count, +8 0} */
            uint32_t cnt = U32(ent, 4);
            SU32(ent, 0, p);
            p += cnt * 8;
            for (uint32_t x = 0; x < cnt; x++) { SU32(U32(ent, 0), 8 * x, 0); SU32(U32(ent, 0), 8 * x + 4, 0); }
        }
        p = (p + 0xf) & 0xfffffff0u;
        SU32(cap, (uint32_t)-8, p);
        p += U32(cap, 0) << 4;
        SU32(cap, (uint32_t)-4, p);
        p += U32(cap, 0) << 4;
    }
    SU32(handle, 0x4c, 0);
}

/* ForestSystem::vf5 00662930 */
uint32_t rx_forest_system_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    SU32(e, 0x9c, rs_asset("ForestAsset"));
    forest_init(e);
    rx_entity_vf5(e);
    scene_list_push_unique(U32(e, 0x10) + 0x2a68, e);   /* the scene's receiver list (render container + 0x1050) */
    return 0;
}
RX_FUNCTION(0x00662930, rx_forest_system_vf5)

/* FUN_00667c30 (EAX = the collision info): the grid of the asset's instance cells */
static void collision_grid(rx_addr o) {
    rx_addr handle = U32(U32(o, 0), 0x9c);
    SU32(handle, 0x4c, 1);
    rx_addr A = U32(handle, 0x44);
    float r = sqrtf((float)I32(A, 0x1c210));
    int32_t n = (int32_t)r;
    SU32(o, 0xc, (uint32_t)n);
    SU32(o, 0x10, (uint32_t)(n * n));
    float fn = (float)n;
    SF32(o, 0x14, fn);
    SF32(o, 0x18, rx_divs(fn, rx_muls(F32(A, 0x24af8), 2.0f)));
    SF32(o, 0x1c, rx_divs(F32(o, 0x14), rx_muls(F32(A, 0x24af0), 2.0f)));
    SU32(U32(U32(o, 0), 0x9c), 0x4c, 0);
}

/* ForestSystem::vf7 00662980 (ECX = entity): the colour sampling (render) is not built; then the collision info object points at the
 * asset: +0 the entity, +4 the asset data + 0x10, +8 the type count, and its grid */
uint32_t rx_forest_system_vf7(rx_addr e) {
    rx_addr o = U32(e, 0x31ec);
    SU32(o, 0, e);
    rx_addr handle = U32(e, 0x9c);
    SU32(handle, 0x4c, 1);
    rx_addr A = U32(handle, 0x44);
    SU32(o, 4, A + 0x10);
    SU32(o, 8, U32(A, 0x24ad0));
    SU32(U32(U32(o, 0), 0x9c), 0x4c, 0);
    collision_grid(o);
    return 0;
}
RX_FUNCTION(0x00662980, rx_forest_system_vf7)

/* the swap of a shape into a handle slot: the slot's old shape loses a reference, the new gains one */
static void slot_set_shape(rx_addr slot, rx_addr shape) {
    rx_addr old = U32(slot, 0);
    if (shape == old) return;
    rx_collision_shape_release(old);
    SU32(slot, 0, shape);
    if (shape) SU32(shape, 4, U32(shape, 4) + 1);
}

/* FUN_00667cc0 (stack: the collision info; RET 4): the trunk collision shapes of every tree type (the asset's two collis handles at
 * type + 0x1d578 / + 0x1d584), kept in the info's handle slots with their "present" bytes (+0x160 + 2 k, +0x161 + 2 k); the shape's
 * vf6 (IsValid: a pure test) is not called */
void rx_forest_collision_shapes(rx_addr o) {
    rx_addr handle = U32(U32(o, 0), 0x9c);
    SU32(handle, 0x4c, 1);
    rx_addr A = U32(handle, 0x44);
    for (int32_t k = 0; k < I32(o, 8); k++) {
        rx_addr T = A + 0x1d584 + 0x5f0u * (uint32_t)k;
        RX_FRAME_ENTER(F, 0x10);
        rx_addr h1 = U32(T, (uint32_t)-0xc);
        if (h1) {
            rx_shape_handle_get(F, h1, 1);
            slot_set_shape(o + 0x24 + 0x10u * (uint32_t)k, U32(F, 4));
            SU32(F, 0, 0x009765b4);
            if (U32(F, 4)) { SU32(U32(F, 4), 4, U32(U32(F, 4), 4) - 1); if (!U32(U32(F, 4), 4)) fatal("667cc0: shape freed", U32(F, 4)); }
            SU8(o, 0x160 + 2u * (uint32_t)k, 1);
        }
        rx_addr h2 = U32(T, 0);
        if (h2) {
            rx_shape_handle_get(F, h2, 1);
            slot_set_shape(o + 0x2c + 0x10u * (uint32_t)k, U32(F, 4));
            SU32(F, 0, 0x009765b4);
            if (U32(F, 4)) { SU32(U32(F, 4), 4, U32(U32(F, 4), 4) - 1); if (!U32(U32(F, 4), 4)) fatal("667cc0: shape freed", U32(F, 4)); }
            SU8(o, 0x161 + 2u * (uint32_t)k, 1);
        }
        RX_FRAME_LEAVE(F, 0x10);
    }
    SU32(U32(U32(o, 0), 0x9c), 0x4c, 0);
}
RX_FUNCTION(0x00667cc0, rx_forest_collision_shapes)

/* FUN_00772940 (EDI = physics world): the world's forest: the first entity of type 0x100f in the world's scene (the used subscene
 * records, scene + 0x38 + 0x110 k, array +0x104 / count +0x108 / used +0x10c), unless the world has one; its collision shapes are
 * made. The first thing the world's finalize (FUN_00769030, rx_physics_world_finalize) does. The original collects every entity of that
 * type into a list {data, count, capacity 1, growth 1} (FUN_007d25c0: ReflexAlignedAlloc(4) first, grown by 1 by alloc + copy + free) and
 * keeps that block alive while the shapes are made (00667cc0 allocates meanwhile), then frees it: the heap order depends on it. */
void rx_physics_world_install_forest(rx_addr world) {
    rx_addr scene = U32(world, 0xcca4);
    rx_addr data = rx_reflex_aligned_alloc(4);
    uint32_t count = 0, cap = 1, grow = 1;
    for (uint32_t k = 0; k < 0x18; k++) {
        rx_addr rec = scene + 0x38 + 0x110u * k;
        if (!U32(rec, 0x10c)) continue;
        for (uint32_t i = 0; i < U32(rec, 0x108); i++) {
            rx_addr en = U32(U32(rec, 0x104), 4 * i);
            if (!en || scene_vcall(en, 0x58, 0, 0) != 0x100f) continue;   /* (the core's arrays hold 0 for the classes it does not build) */
            if (cap <= count) {
                uint32_t ncap = grow + cap;
                rx_addr nb = rx_reflex_aligned_alloc(ncap * 4);
                for (uint32_t j = 0; j < cap; j++) SU32(nb, 4 * j, U32(data, 4 * j));
                rx_reflex_aligned_free(data);
                cap = ncap; data = nb;
            }
            SU32(data, 4 * count, en);
            count++;
        }
    }
    if (count == 0 || U32(world, 0xcc9c)) { rx_reflex_aligned_free(data); return; }
    rx_addr found = U32(data, 0);
    SU32(world, 0xcc9c, found);
    rx_forest_collision_shapes(U32(found, 0x31ec));
    rx_reflex_aligned_free(data);
}
RX_FUNCTION(0x00772940, rx_physics_world_install_forest)
