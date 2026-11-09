/* build_scene_model.c -- ModelEntity (the base of the model-carrying entities) and the ModelInstance embedded at +0xa0. */
#include "build_scene_int.h"
#include "loop_jobs.h"

/* 007f11f0 ModelInstance constructor (stack: instance; RET 4; returns nothing the callers read): the instance of a ModelEntity is the
 * block at entity + 0xa0. Fields: +0 byte 0xff, +1 byte 0, +2 byte 1, +4 0, +8 -1, +0xc 0, +0x10 flags 0, +0x20 identity matrix (the
 * 4x4 at +0x20..+0x5f), +0x60..+0x88 zero (model handle +0x60, per-LOD tables +0x64.., material overrides +0x84), +0x90 the zero vector
 * [0x00a95d50]. */
uint32_t rx_model_instance_construct(rx_addr m) {
    SU32(m, 0x84, 0);
    for (uint32_t k = 0x64; k <= 0x80; k += 4) SU32(m, k, 0);
    SU32(m, 0xc, 0); SU8(m, 1, 0); SU32(m, 4, 0); SU32(m, 8, 0xffffffff);
    SU32(m, 0x10, 0);
    for (uint32_t k = 0; k < 16; k += 4) SU32(m, 0x90 + k, U32(0, 0x00a95d50 + k));
    SU32(m, 0x88, 0);
    SU8(m, 0, 0xff); SU8(m, 2, 1);
    static const float id[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    for (uint32_t k = 0; k < 16; k++) SF32(m, 0x20 + 4 * k, id[k]);
    return 0;
}
RX_FUNCTION(0x007f11f0, rx_model_instance_construct)

/* 00401db0 ModelEntity constructor (stack: entity; RET 4; returns the entity): the Entity fields, the IRenderable vtable at +0x90
 * (+0x94 0xf0), vtable 0x009370d4 / 0x0093714c, the ModelInstance at +0xa0. Subclasses inline this and then store their own vtables. */
rx_addr rx_model_entity_construct(rx_addr e) {
    rx_entity_construct(e, 0x009370d4);
    SU32(e, 0x90, 0x0093708c); SU32(e, 0x94, 0xf0);
    SU32(e, 0x90, 0x0093714c);
    rx_model_instance_construct(e + 0xa0);
    return e;
}
RX_FUNCTION(0x00401db0, rx_model_entity_construct)

/* ---------------------------------------------------------------------------------------------------------------------------
 * ModelInstance_Init 007f12c0 (stdcall instance; RET 4): the model of the entity's RSData ("ModelAsset"), its flags and the material
 * overrides of "MaterialList". The instance: +0x60 the ModelAsset handle, +0x10 word flags (bit 0 CastsShadow, bit 3 / 4 AlwaysVisible:
 * (flags & ~8) | 0x10), +0xc GroupNumber, +0x84 the override table of 8-byte entries {u16 binding index, handle} (allocated by the CRT's
 * _aligned_malloc(total * 8, 16); total = the overrides of every LOD), +0x64..+0x7c seven pointers: per LOD the start of its entries
 * (the slots after the last LOD point at the end of the table). An override is a MaterialList handle that differs from the model's own
 * material handle of that binding (ModelAsset LOD vector +0x130 / +0x134, LodEntry 0x1c bytes: bindings begin +0x10 / end +0x14, Binding
 * 0x10 bytes: material handle at +0xc); LOD 0 sets the limit of overrides per LOD. The model handle's byte +0x4c is 1 while the
 * tables are built. */
void rx_model_instance_init(rx_addr m) {
    rx_addr handle = rs_asset("ModelAsset");
    SU32(m, 0x60, handle);
    uint32_t always = rs_bool("AlwaysVisible");
    if (always) SU16(m, 0x10, (U16(m, 0x10) & 0xfff7) | 0x10);
    if (rs_has("CastsShadow", RS_BOOL)) {
        uint32_t b = rs_bool("CastsShadow");
        SU16(m, 0x10, U16(m, 0x10) ^ (((U16(m, 0x10) ^ b) & 1)));
    }
    if (rs_has("GroupNumber", RS_INT)) SU32(m, 0xc, (uint32_t)rs_int("GroupNumber"));
    /* the MaterialList: handles in a growable array (25 slots, growth 25) */
    rx_addr L = rx_reflex_aligned_alloc(0x19 * 4);   /* the list header lives on the stack in the original: {data, count, cap, growth, max} */
    uint32_t hdr[5] = { L, 0, 0x19, 0x19, 0x7fffffff };
    RX_FRAME_ENTER(F, 0x20);
    for (uint32_t k = 0; k < 5; k++) SU32(F, 4 * k, hdr[k]);
    rx_addr asset = U32(handle, 0x44);
    SU32(handle, 0x4c, 1);
    scene_rs_asset_list("MaterialList", F);
    int32_t n = I32(F, 4);
    rx_addr list = U32(F, 0);
    uint32_t lods = U32(asset, 0x118);
    rx_addr lodv = U32(asset, 0x130);
    uint32_t total = 0, lod0 = 0;
    /* LOD 0 */
    for (int32_t i = 0; i < n; i++) {
        uint32_t nb = (U32(lodv, 0x14) - U32(lodv, 0x10)) >> 4;
        if ((uint32_t)i >= nb) break;
        if (U32(U32(lodv, 0x10) + 0x10u * (uint32_t)i, 0xc) != U32(list, 4u * (uint32_t)i)) { total++; lod0++; }
    }
    /* LODs 1.. */
    for (uint32_t b = 1; b < lods; b++) {
        rx_addr lod = lodv + 0x1c * b;
        uint32_t cnt = 0;
        for (int32_t i = 0; i < n; i++) {
            if (U32(list, 4u * (uint32_t)i) == 0) continue;
            uint32_t nb = (U32(lod, 0x14) - U32(lod, 0x10)) >> 4;
            if ((uint32_t)i >= nb) break;
            if (cnt >= lod0) break;
            if (U32(U32(lod, 0x10) + 0x10u * (uint32_t)i, 0xc) != U32(list, 4u * (uint32_t)i)) { total++; cnt++; }
        }
    }
    SU32(handle, 0x4c, 0);
    SU32(m, 0x84, 0);
    rx_addr tab = 0;
    if (total * 8) { tab = rx_crt_aligned_malloc(total * 8, 16); SU32(m, 0x84, tab); }
    if (total) {
        SU32(handle, 0x4c, 1);
        rx_addr w = tab;
        uint32_t l;
        uint32_t last_count = 0;
        for (l = 0; l < lods; l++) {
            rx_addr lod = lodv + 0x1c * l;
            SU32(m, 0x64 + 4 * l, w);
            uint32_t cnt = 0;
            for (int32_t i = 0; i < n; i++) {
                if (U32(list, 4u * (uint32_t)i) == 0) continue;
                uint32_t nb = (U32(lod, 0x14) - U32(lod, 0x10)) >> 4;
                if ((uint32_t)i >= nb) break;
                if ((cnt & 0xffff) >= lod0) break;
                if (U32(U32(lod, 0x10) + 0x10u * (uint32_t)i, 0xc) != U32(list, 4u * (uint32_t)i)) {
                    rx_addr ent = w + 8 * (cnt & 0xffff);
                    SU16(ent, 0, i);
                    SU32(ent, 4, U32(list, 4u * (uint32_t)i));
                    cnt++;
                    // the original advances the running pointer by 8 per entry
                }
            }
            w = w + 8 * (cnt & 0xffff);
            last_count = cnt;
        }
        rx_addr endp = U32(m, 0x60 + 4 * lods) + 8 * (last_count & 0xffff);
        for (uint32_t k = lods; k < 7; k++) SU32(m, 0x64 + 4 * k, endp);
        SU32(handle, 0x4c, 0);
    }
    rx_crt_aligned_free(U32(F, 0));
    RX_FRAME_LEAVE(F, 0x20);
}

/* ModelEntity::vf5 00401e60 (ECX = e, stack reader; RET 4): the instance, the Entity part, the matrix copied to +0xc0 */
uint32_t rx_model_entity_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    rx_model_instance_init(e + 0xa0);
    rx_entity_vf5(e);
    scene_copy_words(e + 0xc0, e + 0x30, 16);
    return 0;
}
RX_FUNCTION(0x00401e60, rx_model_entity_vf5)

/* factory (FUN_004046a0): ids 3 / 4 (LWModel / Model): 0x140 bytes + 00401db0 */
rx_addr rx_model_entity_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0x140);
    return rx_model_entity_construct(e);
}
/* 0x104a MassiveAdEntity: 00401db0 then vtables 0x0093930c / 0x00939384; vf5 = ModelEntity::vf5 */
rx_addr rx_massive_ad_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0x140);
    rx_model_entity_construct(e);
    SU32(e, 0, 0x0093930c); SU32(e, 0x90, 0x00939384);
    return e;
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * HFBumpModelEntity (0x11; 0x140 bytes, vtables 0x009393c4 / 0x0093943c): vf5 00404070 = the ModelEntity part, SortBias, and the entity put
 * into the render-sort list HFBump_ListInsert 007c8af0 (per scene index [scene + 0x2cc4]: lists of 100 entities at 0x009e2768 + index *
 * 0x190, their keys at +0x4b0 (never written: 0), the counts at 0x009e30c8). Render-only state. */
#define BUMP_BASE 0x009e2768u
static void hfbump_clear_lists(void) {
    rx_addr li = BUMP_BASE;
    for (rx_addr c = 0x009e30c8; c < 0x009e30d4; c += 4) {
        for (uint32_t k = 0; k < 0x190; k += 4) { SU32(li, k, 0); SU32(li + 0x4b0, k, 0); }
        SU32(c, 0, 0);
        li += 0x190;
    }
}
static void hfbump_list_insert(rx_addr e, float bias, uint32_t idx) {
    rx_addr base = BUMP_BASE, lst = base + idx * 0x190, keys = base + 0x4b0 + idx * 0x190, cnt = base + 0x960 + idx * 4;
    int32_t n = I32(cnt, 0);
    for (int32_t i = 0; i < I32(cnt, 0); i++) {
        if (U32(lst, 4u * (uint32_t)i) == e) {
            for (int32_t k = i; k < I32(cnt, 0) - 1; k++) SU32(lst, 4u * (uint32_t)k, U32(lst, 4u * (uint32_t)k + 4));
            SU32(cnt, 0, U32(cnt, 0) - 1);
        }
    }
    n = I32(cnt, 0);
    if (n >= 100) return;
    if (n == 0) { SU32(lst, 0, e); SU32(cnt, 0, 1); return; }
    if (bias > F32(keys, 4u * (uint32_t)(n - 1))) { SU32(lst, 4u * (uint32_t)n, e); SU32(cnt, 0, U32(cnt, 0) + 1); return; }
    int32_t i = n - 1;
    while (i > 0 && !(F32(keys, 4u * (uint32_t)i) <= bias)) i--;
    int32_t pos = i + 1;
    for (int32_t k = n; k > pos; k--) SU32(lst, 4u * (uint32_t)k, U32(lst, 4u * (uint32_t)k - 4));
    SU32(lst, 4u * (uint32_t)pos, e);
    SU32(cnt, 0, U32(cnt, 0) + 1);
}
rx_addr rx_hfbump_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0x140);
    rx_model_entity_construct(e);
    SU32(e, 0, 0x009393c4); SU32(e, 0x90, 0x0093943c);
    return e;
}
uint32_t rx_hfbump_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    rx_model_instance_init(e + 0xa0);
    rx_entity_vf5(e);
    scene_copy_words(e + 0xc0, e + 0x30, 16);
    uint32_t scene_index = U32(U32(e, 0x10), 0x2cc4);
    float bias = rs_float("SortBias");
    if (!(U32(0, 0x009e2760) & 1)) { SU32(0, 0x009e2760, U32(0, 0x009e2760) | 1); hfbump_clear_lists(); }
    hfbump_list_insert(e, bias, scene_index);
    return 0;
}
RX_FUNCTION(0x00404070, rx_hfbump_vf5)

/* small ModelEntity virtuals: vf25 00401ef0 (thiscall ECX = entity: the IRenderable sub-object, entity + 0x90, or 0), vf10 00401e50
 * (the +0xac word), vf23 00401e40 (the ModelInstance, entity + 0xa0) */
uint32_t rx_model_entity_renderable(rx_addr e) { return e ? e + 0x90 : 0; }
RX_FUNCTION(0x00401ef0, rx_model_entity_renderable)
uint32_t rx_model_entity_vf10(rx_addr e) { return U32(e, 0xac); }
RX_FUNCTION(0x00401e50, rx_model_entity_vf10)
uint32_t rx_model_entity_vf23(rx_addr e) { return e + 0xa0; }
RX_FUNCTION(0x00401e40, rx_model_entity_vf23)

/* ---------------------------------------------------------------------------------------------------------------------------
 * CollisionEntity (id 5; 0x1a0 bytes): the constructor 00818f60 (ESI = entity) after 00401db0; vf5 00819130 = ModelInstance_Init, Entity::vf5,
 * then the collision part (build_collision.c: rx_collision_entity_load, which also runs vf28 and the drive-on flags). */
/* 00818f60 (ESI = entity) */
rx_addr rx_collision_entity_construct(rx_addr e) {
    rx_model_entity_construct(e);
    SU32(e, 0, 0x00974f84); SU32(e, 0x90, 0x00974ffc);
    SU32(e, 0x148, 0); SU32(e, 0x14c, 0); SU32(e, 0x150, 5);
    SU32(e, 0x160, 0); SU32(e, 0x15c, 0x0096ddb8);
    rx_vec_proxy_init16(e + 0x164); SU32(e, 0x170, 0); SU32(e, 0x174, 0); SU32(e, 0x178, 0);
    rx_vec_proxy_init(e + 0x17c); SU32(e, 0x188, 0); SU32(e, 0x18c, 0); SU32(e, 0x190, 0);
    SU8(e, 0x194, 0); SU32(e, 0x70, 0); SU8(e, 0x19c, 1);
    return e;
}
RX_FUNCTION(0x00818f60, rx_collision_entity_construct)
rx_addr rx_collision_entity_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0x1a0);
    return rx_collision_entity_construct(e);
}
uint32_t rx_collision_entity_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    rx_model_instance_init(e + 0xa0);
    rx_entity_vf5(e);
    rx_collision_entity_load(e, rx_scene_rs());
    return 0;
}
RX_FUNCTION(0x00819130, rx_collision_entity_vf5)

/* ---------------------------------------------------------------------------------------------------------------------------
 * Model_Bounds 007f7f40 (stack: asset or 0, &centre, &extents; EDX = &radius out, ECX = ModelInstance; RET 0xc): the model's box in the
 * space of its root node: the root's subtree bounds (ModelAsset_RootBounds 007dd450) give centre / extents / radius, the eight corners
 * (extents * (+-1 pattern) + centre) go through the root's local matrix and the box of the corners is stored back (centre = (min + max) / 2,
 * extents = max - centre). The sign vectors are built on first use (guard bits of [0x00dcd500]); lane 3 follows the x86 min / max rules. */
static rx_v4 bnd_max(rx_v4 a, rx_v4 b) { rx_v4 r; for (int i = 0; i < 4; i++) r.v[i] = rx_maxss(a.v[i], b.v[i]); return r; }
static rx_v4 bnd_min(rx_v4 a, rx_v4 b) { rx_v4 r; for (int i = 0; i < 4; i++) r.v[i] = rx_minss(a.v[i], b.v[i]); return r; }
static rx_v4 bnd_mul(rx_v4 a, rx_v4 b) { rx_v4 r; for (int i = 0; i < 4; i++) r.v[i] = a.v[i] * b.v[i]; return r; }
static rx_v4 bnd_add(rx_v4 a, rx_v4 b) { rx_v4 r; for (int i = 0; i < 4; i++) r.v[i] = a.v[i] + b.v[i]; return r; }
static rx_v4 bnd_splat(float x) { rx_v4 r = {{x, x, x, x}}; return r; }
static void bounds_constants(void) {
    uint32_t g = U32(0, 0x00dcd500);
    const float a = 1.0f /* 0x931f24 */, b = F32(0, 0x009cd9e8) /* -1.0 */;
    rx_v4 c[8] = { {{a, a, a, a}}, {{a, a, b, b}}, {{a, b, a, a}}, {{a, b, b, b}}, {{b, a, a, a}}, {{b, a, b, b}}, {{b, b, a, a}}, {{b, b, b, b}} };
    static const uint32_t at[8] = { 0x00dcd4f0, 0x00dcd4e0, 0x00dcd4d0, 0x00dcd4c0, 0x00dcd4b0, 0x00dcd4a0, 0x00dcd490, 0x00dcd480 };
    for (int k = 0; k < 8; k++) if (!(g & (1u << k))) { g |= 1u << k; SU32(0, 0x00dcd500, g); rx_st4(at[k], c[k]); }
    if (!(g & 0x100)) { g |= 0x100; SU32(0, 0x00dcd500, g); rx_st4(0x00dcd470, bnd_splat(0.5f /* 0x9322cc */)); }
}
/* The render sync's parallel workers (loop_jobs.h) reach this for entities that share a model asset (their handle, its node set):
 * the handle's in-use flag is set and cleared by every one (SU32J: it ends 0 as in one run), the node set's bounds update (its
 * visited bits, its world matrices and boxes: the same values from whichever instance computes them) and the reads of its box run
 * under the node set's lock; the sign vectors' one-time init under the jobs' lock. The results go to the worker's own frame. */
void scene_model_bounds(rx_addr asset, rx_addr instance, rx_addr ctr, rx_addr ext, rx_addr rad) {
    if (rx_jobs_parallel() && (U32(0, 0x00dcd500) & 0x1ffu) != 0x1ffu) { rx_jobs_cs_enter(); bounds_constants(); rx_jobs_cs_leave(); }
    else bounds_constants();                                  /* (built: reads only) */
    rx_addr set_slot;
    if (!asset) {
        rx_addr h = U32(instance, 0x60);
        SU32J(h, 0x4c, 1);
        set_slot = U32(h, 0x44) + 0x50;
        SU32J(h, 0x4c, 0);
    } else set_slot = asset + 0x50;
    rx_addr rec = U32(set_slot, 0);
    const rx_addr set = U32(rec, 0);
    rx_jobs_lock_key(set);
    rx_addr lm = U32(U32(rec, 0), 0x18) + ((uint32_t)U8(rec, 0xb) << 6);
    /* ModelAsset_RootBounds 007dd450: update, then copy the box out (x87 dword copies) */
    rx_model_update_subtree_bounds(rec);
    rx_addr bb = U32(U32(rec, 0), 0x20) + 0x50u * U8(rec, 0xb);
    for (uint32_t k = 0; k < 12; k += 4) SU32(ctr, k, scene_x87c(U32(bb, 0x20 + k)));
    for (uint32_t k = 0; k < 12; k += 4) SU32(ext, k, scene_x87c(U32(bb, 0x30 + k)));
    SU32(rad, 0, scene_x87c(U32(bb, 0x44)));
    rx_v4 E = rx_ld4(ext), Ctr = rx_ld4(ctr);
    rx_v4 r0 = rx_ld4(lm), r1 = rx_ld4(lm + 0x10), r2 = rx_ld4(lm + 0x20), r3 = rx_ld4(lm + 0x30);
    rx_jobs_unlock_key(set);
    static const uint32_t at[8] = { 0x00dcd4f0, 0x00dcd4e0, 0x00dcd4d0, 0x00dcd4c0, 0x00dcd4b0, 0x00dcd4a0, 0x00dcd490, 0x00dcd480 };
    rx_v4 T[8];
    for (int k = 0; k < 8; k++) {
        rx_v4 p = bnd_add(bnd_mul(E, rx_ld4(at[k])), Ctr);
        rx_v4 t = bnd_add(bnd_mul(bnd_splat(p.v[1]), r1), bnd_mul(bnd_splat(p.v[0]), r0));
        t = bnd_add(t, bnd_mul(bnd_splat(p.v[2]), r2));
        T[k] = bnd_add(t, r3);
    }
    rx_v4 mx = bnd_max(T[0], T[1]), mn = bnd_min(T[0], T[1]);
    rx_v4 mx23 = bnd_max(T[2], T[3]), mn23 = bnd_min(T[2], T[3]);
    mn = bnd_min(mn, mn23);
    rx_v4 mx45 = bnd_max(T[4], T[5]), mn45 = bnd_min(T[4], T[5]);
    rx_v4 mx67 = bnd_max(T[6], T[7]), mn67 = bnd_min(T[6], T[7]);
    mx = bnd_max(mx, mx23);
    mx45 = bnd_max(mx45, mx67);
    mx = bnd_max(mx, mx45);
    mn45 = bnd_min(mn45, mn67);
    mn = bnd_min(mn, mn45);
    rx_v4 c = bnd_mul(bnd_add(mn, mx), rx_ld4(0x00dcd470));
    rx_st4(ext, bnd_add(mx, bnd_mul(c, bnd_splat(-1.0f))));
    rx_v4 e2 = {{mx.v[0] - c.v[0], mx.v[1] - c.v[1], mx.v[2] - c.v[2], mx.v[3] - c.v[3]}};
    rx_st4(ext, e2);
    rx_st4(ctr, c);
}
/* ModelEntity::vf6_o90 00401f50 (this = the IRenderable at entity + 0x90; stack centre, extents, radius; RET 0xc) */
uint32_t rx_model_entity_vf6_o90(rx_addr this90, rx_addr ctr, rx_addr ext, rx_addr rad) {
    scene_model_bounds(0, this90 + 0x10, ctr, ext, rad);
    return 0;
}
RX_FUNCTION(0x00401f50, rx_model_entity_vf6_o90)
