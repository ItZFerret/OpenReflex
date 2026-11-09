/* build_rider_tree.c -- construction unit 7 (riders): the animation tree of an AnimationEntity from its BXML document (FUN_0082dd00:
 * the tree's root node "animation", every child an animator built by the entity's vf28 factory dispatch (AnimationEntity::vf28
 * 0082dbd0: blend / motion / switch / performance / additive; RiderAnimation::vf28 006b5e50 adds ragdoll / inversekinematics /
 * featherstone / expression / procedural), the root set on the Character (0082e3a0: the frame count walk, the pose pool, the animator
 * name registry walk) and the bone parent map (0082e560). The tree walks are virtual (Animator::vf10 per class with a walker object
 * on the stack: vf0 on entry, vf1 on exit). Hand transliterations of the disassembly. */
#include "build_rider.h"
#include "build_rider_util.h"
#include "build_asset.h"
#include "crt.h"
#include "race.h"
#include "world_misc.h"
#include "phys_coll_misc.h"
#include "anim.h"

/* the XMLReader virtuals (build_xml.c registers them; the core's when registered, else the original) */
static uint32_t xv(rx_addr r, uint32_t off, int n, uint32_t a0, uint32_t a1, uint32_t a2) {
    uint32_t a[3] = {a0, a1, a2};
    return rr_vcall(r, off, n, a);
}
#define X_ROOT(r)              xv(r, 0x04, 0, 0, 0, 0)
#define X_CHILD(r, n, name)    xv(r, 0x1c, 2, n, name, 0)
#define X_FIRST(r, n, nm, out) xv(r, 0x20, 3, n, nm, out)
#define X_RELEASE(r, c)        xv(r, 0x24, 1, c, 0, 0)
#define X_NEXT(r, c)           xv(r, 0x28, 1, c, 0, 0)
#define X_NAME(r, n, b, sz)    xv(r, 0xa0, 3, n, b, sz)
#define X_ATTR(r, n, name)     xv(r, 0xe8, 2, n, name, 0)
#define X_FLOATS(r, e, d, k)   xv(r, 0x130, 3, e, d, k)
#define X_STR(r, e, b, sz)     xv(r, 0x13c, 3, e, b, sz)

static rx_addr anim_name(rx_addr a) { return rr_vcall(a, 4, 0, NULL); }    /* Animator::vf1 (006d4bf0: a + 8) */

/* ---------------------------------------------------------------------------------------------------------------------------
 * the animator constructors (the Animator base 0x0095ef10: +4 type, +8 the name (0x20, strncpy 0x1f)) */

static void animator_base(rx_addr a, rx_addr name) {
    SU32(a, 0, 0x0095ef10);
    crt_strncpy(a + 8, name, 0x1f);
    SU8(a + 8, 0x1f, 0);
}
/* the global animator serial [0x00a9595c] (kept modulo 30) */
static uint32_t animator_serial(void) {
    SU32(0, 0x00a9595c, U32(0, 0x00a9595c) + 1);
    uint32_t v = U32(0, 0x00a9595c);
    SU32(0, 0x00a9595c, v % 0x1e);
    return v;
}
/* 00832f50 (ESI = a, EAX = name; s0 weight, s1 first, s2 second child; RET 0xc): BlendAnimator (vtable 0x00975f1c, type 0) */
rx_addr rx_anim_blend_construct(rx_addr a, rx_addr name, float weight, rx_addr c0, rx_addr c1) {
    animator_base(a, name);
    SU32(a, 4, 0xb);
    SU32(a, 0, 0x00975f1c);
    rr_copy(a + 0x30, RR_ZERO3, 12);
    rr_copy(a + 0x40, RR_ZERO3, 12);
    SU32(a, 0x58, c0); SU32(a, 0x5c, c1);
    SF32(a, 0x68, weight);
    SU32(a, 0x50, 0); SU32(a, 0x54, 0); SU32(a, 0x60, 0);
    SU32(a, 0x64, animator_serial());
    SU32(a, 4, 0);
    return a;
}
/* 008345c0 (ESI = a, EAX = name; s0 source, s1 additive child; RET 8): AdditiveAnimator (0x00975f50, type 2) */
rx_addr rx_anim_additive_construct(rx_addr a, rx_addr name, rx_addr src, rx_addr add) {
    animator_base(a, name);
    SU32(a, 0, 0x00975f50);
    SU32(a, 0x2c, src); SU32(a, 0x30, add);
    SU32(a, 4, 2);
    return a;
}
/* 0083b330 (ESI = a, EAX = name, EDI = the track binding {handle, ?, bone map}; s0 time scale, s1 root bone; RET 8): MotionAnimator
 * (0x00975ee8, type 1) */
rx_addr rx_anim_motion_construct(rx_addr a, rx_addr name, rx_addr binding, float time_scale, uint32_t bone) {
    animator_base(a, name);
    SU32(a, 4, 0xb);
    SU32(a, 0, 0x00975ee8);
    SU32(a, 0x28, 0); SU32(a, 0x2c, 0); SU32(a, 0x30, 0);
    SF32(a, 0x38, 0);
    SU32(a, 0x40, 0);
    SU32(a, 0x34, bone);
    SF32(a, 0x3c, time_scale);
    SU32(a, 0x28, U32(binding, 0)); SU32(a, 0x2c, U32(binding, 4)); SU32(a, 0x30, U32(binding, 8));
    SU32(a, 4, 1);
    return a;
}

/* ---- the 8-byte element vector (std::vector of {proxy, node} list iterators: hash_map buckets) ---- */
/* 00821580 (ECX = n): ReflexAlignedAlloc(n * 8) (n over 0x1fffffff throws: not reached) */
static rx_addr vec8_alloc(uint32_t n) {
    if (n && 0xffffffffu / n < 8) rr_fatal("vector<8> allocation too large", n);
    return rr_alloc(n * 8);
}
/* 00843130 (ECX = n, EDX = vector; s0 &value, s1 where, s2 container; RET 0xc): _Insert_n(where, n, value) */
void rx_rider_vec8_insert_n(rx_addr v, uint32_t n, rx_addr value, rx_addr where) {
    rx_addr b = U32(v, 0xc);
    uint32_t cap = b ? (U32(v, 0x14) - b) >> 3 : 0;
    if (!n) return;
    uint32_t size = (U32(v, 0x10) - b) >> 3;
    if (0x1fffffffu - size < n) rr_fatal("vector<8> too long", n);
    uint32_t need = size + n;
    if (cap < need) {
        uint32_t nc = 0x1fffffffu - (cap >> 1) < cap ? 0 : cap + (cap >> 1);
        if (nc < need) nc = need;
        rx_addr nb = vec8_alloc(nc);
        rx_addr d = nb;
        for (rx_addr s = U32(v, 0xc); s != where; s += 8, d += 8) { SU32(d, 0, U32(s, 0)); SU32(d, 4, U32(s, 4)); }   /* 0063c5d0 */
        for (uint32_t k = 0; k < n; k++, d += 8) { SU32(d, 0, U32(value, 0)); SU32(d, 4, U32(value, 4)); }           /* 00843740 */
        for (rx_addr s = where; s != U32(v, 0x10); s += 8, d += 8) { SU32(d, 0, U32(s, 0)); SU32(d, 4, U32(s, 4)); }
        uint32_t total = n + ((U32(v, 0x10) - U32(v, 0xc)) >> 3);
        if (U32(v, 0xc)) rx_reflex_aligned_free(U32(v, 0xc));
        SU32(v, 0x14, nb + nc * 8);
        SU32(v, 0x10, nb + total * 8);
        SU32(v, 0xc, nb);
        return;
    }
    rr_fatal("vector<8> insert into spare capacity (not reached in construction)", n);
}
/* 00842df0: a std::list head node (0x10: next = prev = itself) */
static rx_addr list_head_alloc(void) {
    rx_addr n = rr_alloc(0x10);
    if (n) SU32(n, 0, n);
    if (n + 4) SU32(n + 4, 0, n);
    return n;
}
/* 00842430 (EAX = &traits byte, s0 m; RET 4): stdext::hash_map (+4 the traits byte, +8 the list (proxy, head +0x14, size +0x18),
 * +0x24 the bucket vector of 9 list iterators {list proxy's container, head}, +0x3c mask 7, +0x40 buckets 8, +0x44 max load 4.0f) */
rx_addr rx_rider_hash_map_construct(rx_addr m, uint8_t traits) {
    SU8(m, 4, traits);
    rx_addr l = m + 8;
    rx_addr p = rr_alloc(4);
    if (p) SU32(p, 0, l);
    SU32(l, 0, p);
    SU32(l, 0x14, list_head_alloc());
    SU32(l, 0x18, 0);
    rr_vec_init(m + 0x24);
    SF32(m, 0x44, 4.0f);   /* [0x9322c0] */
    RX_FRAME_ENTER(F, 8);
    SU32(F, 0, U32(l, 0));    /* the list iterator {container proxy, head} */
    SU32(F, 4, U32(l, 0x14));
    /* 00842f30 (ECX = the vector, EAX = &value): erase everything (008430a0: nothing here), then insert 9 copies at the end */
    rx_addr v = m + 0x24;
    if (U32(v, 0x10) != U32(v, 0xc)) rr_fatal("hash_map bucket vector not empty", m);
    rx_rider_vec8_insert_n(v, 9, F, U32(v, 0x10));
    RX_FRAME_LEAVE(F, 8);
    SU32(m, 0x3c, 7);
    SU32(m, 0x40, 8);
    return m;
}
/* 0083c590 (EAX = name; s0 a, s1 &branches vector; RET 8): SwitchAnimator (0x00975fb8, type 3): a BlendAnimator base without
 * children, the parameter hash_map +0x70, the branch vector +0xb8 */
rx_addr rx_anim_switch_construct(rx_addr a, rx_addr name, rx_addr branches) {
    rx_anim_blend_construct(a, name, 0.0f, 0, 0);
    SU32(a, 0, 0x00975fb8);
    /* the hash_compare traits byte: the original passes the address of its own argument slot (an empty object), so the byte is the
     * low byte of `a` */
    rx_rider_hash_map_construct(a + 0x70, (uint8_t)a);
    rr_vec_init(a + 0xb8);
    SU32(a, 4, 3);
    uint32_t n = (U32(branches, 0x10) - U32(branches, 0xc)) >> 2;
    RX_FRAME_ENTER(G, 4);
    for (uint32_t k = 0; k < n; k++) {
        SU32(G, 0, U32(U32(branches, 0xc), 4 * k));
        if (((U32(a, 0xb8 + 0x10) - U32(a, 0xb8 + 0xc)) & ~3u) == 0) { SU16(a, 0xd0, 0); SU16(a, 0xd2, 0); }
        phys_vector_push_back(a + 0xb8, G);
    }
    RX_FRAME_LEAVE(G, 4);
    return a;
}
/* 0083cf90 (ECX = a, EAX = name; s0 the perfdat handle; RET 4): PerformanceAnimator (0x00976024, type 9): the clip slots (+0x74, one
 * per clip), the events (+0x78: the perfdat's events except types 10 / 11, a zero length nudged to 1e-4) */
rx_addr rx_anim_performance_construct(rx_addr a, rx_addr name, rx_addr h) {
    rx_anim_blend_construct(a, name, 0.0f, 0, 0);
    SF32(a, 0x94, 0); SF32(a, 0x98, 0);
    SF32(a, 0x7c, 1.0f);
    SU32(a, 0, 0x00976024);
    SU32(a, 0x74, 0); SU32(a, 0x78, 0); SU32(a, 0x8c, 0);
    SU32(a, 0x80, U32(0, 0x009cd9e8));
    SU32(a, 0x84, 0);
    SU32(a, 0x70, U32(h, 0x44));
    SU32(h, 0x4c, 1);
    rr_zero8(a + 0x9c, 0x40);
    rx_addr nm = rr_vcall(h, 8, 0, NULL);
    for (uint32_t k = 0;; k++) { uint8_t c = U8(nm, k); SU8(a + 0x9c, k, c); if (!c) break; }
    rx_addr pd = U32(a, 0x70);
    SU32(a, 0x88, U32(pd, 0x10));
    uint32_t nclip = U32(pd, 0x14);
    SU32(a, 0x8c, nclip);
    rx_addr slots = rr_alloc(rx_mul_sat(nclip, 4));
    if (slots) for (uint32_t k = 0; k < nclip; k++) SU32(slots, 4 * k, 0);   /* 004dc090 per element */
    SU32(a, 0x74, slots);
    pd = U32(a, 0x70);
    SU32(a, 0x94, rr_x87_copy(U32(pd, 0x24)));
    int32_t nev = (int32_t)U32(pd, 0x1c);
    SU32(a, 0x90, (uint32_t)nev);
    for (int32_t k = 0; k < nev; k++) {
        int32_t t = (int32_t)U32(U32(pd, 0x20) + 0xcu * (uint32_t)k, 8);
        if (t == 10 || t == 11) SU32(a, 0x90, U32(a, 0x90) - 1);
    }
    int32_t ne = (int32_t)U32(a, 0x90);
    if (ne > 0) {
        rx_addr ev = rr_alloc(rx_mul_sat((uint32_t)ne, 0xc));
        if (ev) for (int32_t k = 0; k < ne; k++) { SF32(ev + 0xcu * (uint32_t)k, 4, 0); SU32(ev + 0xcu * (uint32_t)k, 8, 0xffffffffu); }
        SU32(a, 0x78, ev);
        uint32_t j = 0;
        for (int32_t k = 0; k < nev; k++) {
            rx_addr s = U32(U32(a, 0x70), 0x20) + 0xcu * (uint32_t)k;
            int32_t t = (int32_t)U32(s, 8);
            if (t >= 10 && t <= 11) continue;
            rx_addr d = U32(a, 0x78) + 0xc * j;
            SU32(d, 8, (uint32_t)t);
            SU32(d, 4, rr_x87_copy(U32(s, 4)));
            SU32(d, 0, rr_x87_copy(U32(s, 0)));
            float b = F32(d, 4);
            if (b == 0.0f) SF32(d, 4, b + 1e-4f /* [0x9775a4] */);
            j++;
        }
    }
    SU32(h, 0x4c, 0);
    SU32(a, 4, 9);
    return a;
}
/* 006afa90 + 006afa10 (ESI = a, EAX = name, s0 child; RET 4): FeatherstoneAnimator (0x0095efa0; _aligned_malloc(0xa940, 16)): the IK
 * data at +0x40 (00424410) */
rx_addr rx_anim_featherstone_alloc(void) { return rx_crt_aligned_malloc(0xa940, 0x10); }
rx_addr rx_anim_featherstone_construct(rx_addr a, rx_addr name, rx_addr child) {
    animator_base(a, name);
    SU32(a, 4, 0xb);
    SU32(a, 0, 0x0095efa0);
    SU32(a, 0x30, child);
    rx_rider_ik_data_init(a + 0x40);
    return a;
}
/* 006afc30 (ESI = a, EAX = name; s0 child, s1 the physics name; RET 8): InverseKinematicsAnimator (0x0095efd8, type 5) */
rx_addr rx_anim_ik_construct(rx_addr a, rx_addr name, rx_addr child, rx_addr physics) {
    animator_base(a, name);
    SU32(a, 4, 0xb);
    SU32(a, 0x28, 0); SU32(a, 0x2c, 0);
    SU32(a, 0, 0x0095efd8);
    uint32_t len = crt_strlen(physics) + 1;
    rx_addr s = rr_alloc(len);
    SU32(a, 0x74, s);
    crt_strncpy(s, physics, len - 1);
    SU8(s, len - 1, 0);
    SU32(a, 0x5c, child);
    SU32(a, 0x60, 0); SU32(a, 0x64, 0); SU32(a, 0x30, 0);
    SU32(a, 4, 5);
    SF32(a, 0x6c, 0);
    SU32(a, 0x70, 1);
    SU32(a, 0x7c, 0);
    SU32(a, 0x50, 0xffffffffu);
    return a;
}
/* 006ac980 (ESI = a, EAX = name; s0 child; RET 4): ExpressionBoneAnimator (0x0095ef44, type 8) */
rx_addr rx_anim_expression_construct(rx_addr a, rx_addr name, rx_addr child) {
    animator_base(a, name);
    SU32(a, 4, 0xb);
    SU32(a, 0, 0x0095ef44);
    rr_vec_init(a + 0x44);
    SU32(a, 0x2c, 0);
    SU8(a, 0x5c, 0);
    SU32(a, 4, 8);
    SU32(a, 0x28, child);
    SU32(a, 0x40, 0xffffffffu);
    return a;
}
/* 006b57c0 (ESI = a, EAX = name; s0 child; RET 4): ProceduralAnimator (0x0095f00c, type 0xa) */
rx_addr rx_anim_procedural_construct(rx_addr a, rx_addr name, rx_addr child) {
    animator_base(a, name);
    SU32(a, 0x2c, 0);
    SU32(a, 0, 0x0095f00c);
    SU32(a, 4, 0xa);
    SU32(a, 0x28, child);
    SU32(a, 0x54, 0); SU32(a, 0x58, 0); SU32(a, 0x5c, 0); SU32(a, 0x60, 0); SU32(a, 0x64, 0);
    SU32(a, 0x50, 0xffffffffu);
    SU8(a, 0x6c, 1);
    SF32(a, 0x30, 0);
    return a;
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * the entity's bone name cache (+0x148: the 00843480 table of {name, byte index, hash, next}) and the track bindings */

/* 00843350 (EAX = table, EDI = &name; s0 &out byte; RET 4): 1 and the cached byte when the name is in the table */
static int bone_cache_find(rx_addr t, rx_addr name_p, rx_addr out) {
    rx_addr name = U32(name_p, 0);
    uint32_t h = rx_string_hash_nocase(name);
    for (rx_addr n = U32(U32(t, 0xc), 4 * (h % U32(t, 0))); n; n = U32(n, 0xc))
        if (U32(n, 8) == h && crt_stricmp(U32(n, 0), name) == 0) { SU8(out, 0, U8(n, 4)); return 1; }
    return 0;
}
/* 008433b0 (EDI = table; s0 &name, s1 &byte; RET 8): insert or update (grows to (int)(n * 1.5f) when count / n > the max load) */
static void bone_cache_insert(rx_addr t, rx_addr name_p, rx_addr val_p) {
    float n = (float)(int32_t)U32(t, 0), c = (float)(int32_t)U32(t, 8);
    if (c / n > F32(t, 4)) rx_strtab_rehash(t, (uint32_t)rx_cvtt(n * 1.5f));
    rx_addr name = U32(name_p, 0);
    uint32_t h = rx_string_hash_nocase(name);
    uint32_t b = h % U32(t, 0);
    for (rx_addr e = U32(U32(t, 0xc), 4 * b); e; e = U32(e, 0xc))
        if (U32(e, 8) == h && crt_stricmp(U32(e, 0), name) == 0) { SU8(e, 4, U8(val_p, 0)); return; }
    uint32_t sz = 0x10;
    rx_addr e = rr_vcall(U32(t, 0x18), 4, 1, &sz);
    if (e) SU32(e, 0xc, 0);
    SU32(e, 0, name);
    SU8(e, 4, U8(val_p, 0));
    SU32(e, 8, h);
    SU32(e, 0xc, U32(U32(t, 0xc), 4 * b));
    SU32(U32(t, 0xc), 4 * b, e);
    SU32(t, 8, U32(t, 8) + 1);
}
/* 0082de00 (s0 entity, s1 out {handle, 0, the bone map}, s2 the anim handle; RET 0xc): per track the node index of its bone in the
 * entity's model (+0x180), cached by name in +0x148 */
rx_addr rx_anim_bind_tracks(rx_addr e, rx_addr out, rx_addr h) {
    rx_addr anim = U32(h, 0x44);
    SU32(h, 0x4c, 1);
    SU32(out, 8, 0);
    SU32(out, 0, h);
    SU32(out, 4, 0);
    SU32(out, 8, rr_alloc(U32(anim, 0x10)));
    if ((int32_t)U32(anim, 0x10) <= 0) { SU32(h, 0x4c, 0); return out; }
    RX_FRAME_ENTER(F, 8);
    for (uint32_t k = 0; (int32_t)k < (int32_t)U32(anim, 0x10); k++) {
        rx_addr dst = U32(out, 8) + k;
        SU32(F, 0, U32(anim, 0x14) + 0x20 * k);
        if (!bone_cache_find(e + 0x148, F, dst)) {
            rx_addr r = rx_hierarchy_find_node(e + 0x180, U32(anim, 0x14) + 0x20 * k);
            SU8(dst, 0, r ? (uint8_t)((int32_t)(r - U32(e + 0x180, 8)) >> 4) : 0xff);
            SU32(F, 0, U32(anim, 0x14) + 0x20 * k);
            bone_cache_insert(e + 0x148, F, dst);
        }
    }
    RX_FRAME_LEAVE(F, 8);
    SU32(h, 0x4c, 0);
    return out;
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * the factories (the reader r = the BXMLAsset's XMLReader; node = the animator's XML node) */

#define TAG_PERF_LO 0x66726570u
#define TAG_PERF_HI 0x00746164u
static rx_addr db_vf27(rx_addr name, uint32_t lo, uint32_t hi, uint32_t create) {
    uint32_t a[4] = {name, lo, hi, create};
    return rr_vcall(rr_database(), 0x6c, 4, a);
}
/* the "name" attribute into a 0x104 buffer of the frame; 0 when absent */
static int read_name(rx_addr r, rx_addr node, rx_addr attr, rx_addr buf) {
    rx_addr e = X_ATTR(r, node, attr);
    if (!e) return 0;
    return X_STR(r, e, buf, 0x104) != 0;
}

/* 0082db70 (ESI = reader, EDI = entity, s0 node; RET 4): the node's name, then the entity's vf28 (name, reader, node) */
rx_addr rx_anim_create_node(rx_addr e, rx_addr r, rx_addr node) {
    RX_FRAME_ENTER(F, 0x110);
    X_NAME(r, node, F + 4, 0x104);
    uint32_t a[3] = {F + 4, r, node};
    rx_addr res = rr_vcall(e, 0x70, 3, a);
    RX_FRAME_LEAVE(F, 0x110);
    return res;
}
/* 0082d280 (ECX = entity, EDX = reader; s0 node; RET 4): "blend": left / right children, blend_percent */
rx_addr rx_anim_factory_blend(rx_addr e, rx_addr r, rx_addr node) {
    RX_FRAME_ENTER(F, 0x120);
    rx_addr res = 0;
    if (read_name(r, node, 0x009362a4, F + 0x18)) {
        rx_addr l = X_CHILD(r, node, 0x009747c8), rt = X_CHILD(r, node, 0x009747d0);
        if (l && rt) {
            rx_addr l0 = X_CHILD(r, l, 0);
            if (l0) {
                rx_addr r0 = X_CHILD(r, rt, 0);
                if (r0) {
                    rx_addr a = rx_anim_create_node(e, r, l0);
                    rx_addr b = rx_anim_create_node(e, r, r0);
                    if (a && b) {
                        rx_addr bp = X_ATTR(r, node, 0x009758f0);
                        SF32(F, 0x14, 0);
                        if (bp) X_FLOATS(r, bp, F + 0x14, 1);
                        rx_addr o = rr_alloc(0x70);
                        if (o) res = rx_anim_blend_construct(o, F + 0x18, rx_bf(rr_x87_copy(U32(F, 0x14))), a, b);
                    }
                }
            }
        }
    }
    RX_FRAME_LEAVE(F, 0x120);
    return res;
}
/* 0082d620 (ECX = entity, EDX = reader; s0 node; RET 4): "additive": source / add children */
rx_addr rx_anim_factory_additive(rx_addr e, rx_addr r, rx_addr node) {
    RX_FRAME_ENTER(F, 0x110);
    rx_addr res = 0;
    if (read_name(r, node, 0x009362a4, F + 4)) {
        rx_addr s = X_CHILD(r, node, 0x0095ef04), ad = X_CHILD(r, node, 0x0097590c);
        if (s && ad) {
            rx_addr s0 = X_CHILD(r, s, 0);
            if (s0) {
                rx_addr a0 = X_CHILD(r, ad, 0);
                if (a0) {
                    rx_addr a = rx_anim_create_node(e, r, s0);
                    rx_addr b = rx_anim_create_node(e, r, a0);
                    if (a && b) {
                        rx_addr o = rr_alloc(0x34);
                        if (o) res = rx_anim_additive_construct(o, F + 4, a, b);
                    }
                }
            }
        }
    }
    RX_FRAME_LEAVE(F, 0x110);
    return res;
}
/* 0082d480 (ECX = reader; s0 entity, s1 node; RET 8): "motion": the anim asset, its track binding, time_scale (default 1 / the
 * anim's +0x4c), the entity's root bone */
rx_addr rx_anim_factory_motion(rx_addr r, rx_addr e, rx_addr node) {
    RX_FRAME_ENTER(F, 0x230);
    int32_t bone = -1;
    rx_addr root = U32(U32(e, 0x144), 0x220);
    if (root) {
        rx_addr rec = rx_hierarchy_find_node(e + 0x180, root);
        bone = rec ? (int32_t)(rec - U32(e + 0x180, 8)) >> 4 : -1;
    }
    rx_addr res = 0;
    if (read_name(r, node, 0x009362a4, F + 0x120) && read_name(r, node, 0x00970838, F + 0x18)) {
        rx_addr h = db_vf27(F + 0x18, 0x6d696e61, 0, 1);
        if (h) {
            rx_anim_bind_tracks(e, F + 4, h);
            rx_addr ts = X_ATTR(r, node, 0x00975900);
            rx_addr th = U32(F, 4);
            if (th) {
                rx_addr anim = U32(th, 0x44);
                SU32(th, 0x4c, 1);
                SF32(F, 0, 1.0f / F32(anim, 0x4c));
                SU32(th, 0x4c, 0);
                if (ts) X_FLOATS(r, ts, F, 1);
                rx_addr o = rr_alloc(0x44);
                if (o) res = rx_anim_motion_construct(o, F + 0x120, F + 4, rx_bf(rr_x87_copy(U32(F, 0))), (uint32_t)bone);
            }
        }
    }
    RX_FRAME_LEAVE(F, 0x230);
    return res;
}
/* 0082d730 (s0 entity, s1 reader, s2 node; RET 0xc): "switch": the branches (every child of "branches"), the transition table */
rx_addr rx_anim_factory_switch(rx_addr e, rx_addr r, rx_addr node) {
    RX_FRAME_ENTER(F, 0x350);
    rx_addr res = 0;
    if (read_name(r, node, 0x009362a4, F + 0x140)) {
        rx_addr br = X_CHILD(r, node, 0x00975910);
        if (br) {
            rx_addr v = F + 0x20;   /* the branch vector (005442c0) */
            rx_addr p = rr_alloc(4);
            if (p) { SU32(p, 0, v); SU32(v, 0, p); } else SU32(v, 0, 0);
            SU32(v, 0xc, 0); SU32(v, 0x10, 0); SU32(v, 0x14, 0);
            for (rx_addr c = X_FIRST(r, br, 0, F + 0x10); c; c = X_NEXT(r, U32(F, 0x10))) {
                SU32(F, 0x18, rx_anim_create_node(e, r, c));
                phys_vector_push_back(v, F + 0x18);
            }
            X_RELEASE(r, U32(F, 0x10));
            rx_addr o = rr_alloc(0xe0);
            res = o ? rx_anim_switch_construct(o, F + 0x140, v) : 0;
            rx_addr tt = X_CHILD(r, node, 0x0097591c);
            if (tt && X_FIRST(r, tt, 0, F + 0x10)) rr_fatal("switch transition_table (0083c960) not ported", node);
            /* 0081f4c0: the branch vector freed */
            if (U32(v, 0xc)) rx_reflex_aligned_free(U32(v, 0xc));
            SU32(v, 0xc, 0); SU32(v, 0x10, 0); SU32(v, 0x14, 0);
            rx_reflex_aligned_free(U32(v, 0));
        }
    }
    RX_FRAME_LEAVE(F, 0x350);
    return res;
}
/* 0082d960 (ECX = reader, EDX = node; s0 entity; RET 4): "performance": the perfdat asset, an animator per clip (MotionAnimator with
 * the clip's anim, its time scale through the anim handle's vf2), the clips' end flags */
rx_addr rx_anim_factory_performance(rx_addr r, rx_addr node, rx_addr e) {
    RX_FRAME_ENTER(F, 0x240);
    rx_addr res = 0;
    if (read_name(r, node, 0x009362a4, F + 0x138) && read_name(r, node, 0x00970838, F + 0x30)) {
        rx_addr h = db_vf27(F + 0x30, TAG_PERF_LO, TAG_PERF_HI, 1);
        if (h) {
            int32_t bone = -1;
            if (U32(U32(e, 0x144), 0x220)) rr_fatal("performance animator with a root bone (007dea50) not ported", e);
            rx_addr pd = U32(h, 0x44);
            SU32(h, 0x4c, 1);
            rx_addr o = rr_alloc(0xe0);
            rx_addr pa = o ? rx_anim_performance_construct(o, F + 0x138, h) : 0;
            for (uint32_t k = 0; (int32_t)k < (int32_t)U32(pa, 0x8c); k++) {
                rx_addr clip = U32(pd, 0x18) + 0x70 * k;
                rx_addr ah = db_vf27(clip, 0x6d696e61, 0, 1);
                rx_anim_bind_tracks(e, F + 0x24, ah);
                rx_addr m = rr_alloc(0x44);
                rx_addr ma = 0;
                if (m) {
                    rx_addr nm = rr_vcall(ah, 8, 0, NULL);   /* the anim handle's name (AssetHandle vf2 004dce30) */
                    ma = rx_anim_motion_construct(m, nm, F + 0x24, 0.0f /* fldz; fstp */, (uint32_t)bone);
                }
                rx_addr c = U32(pd, 0x18) + 0x70 * k;
                if (U32(c, 0x64) == 0 && U32(c, 0x68) == 0) SU32(ma, 0x40, U32(pa, 0x88));
                SU32(U32(pa, 0x74), 4 * k, ma);
            }
            SU32(h, 0x4c, 0);
            res = pa;
        }
    }
    RX_FRAME_LEAVE(F, 0x240);
    return res;
}
/* 006b6030 (ECX = entity, EDX = reader; s0 node; RET 4): "ragdoll" / "featherstone": physics name, the source child */
rx_addr rx_anim_factory_featherstone(rx_addr e, rx_addr r, rx_addr node) {
    RX_FRAME_ENTER(F, 0x220);
    rx_addr res = 0;
    if (read_name(r, node, 0x009362a4, F + 0xc) && read_name(r, node, 0x0095eefc, F + 0x110)) {
        rx_addr s = X_CHILD(r, node, 0x0095ef04);
        if (s) {
            rx_addr s0 = X_CHILD(r, s, 0);
            if (s0) {
                rx_addr child = rx_anim_create_node(e, r, s0);
                rx_addr o = rx_anim_featherstone_alloc();
                if (o) res = rx_anim_featherstone_construct(o, F + 0xc, child);
            }
        }
    }
    RX_FRAME_LEAVE(F, 0x220);
    return res;
}
/* 006b5f30 (ECX = entity, EDX = reader; s0 node; RET 4): "inversekinematics" */
rx_addr rx_anim_factory_ik(rx_addr e, rx_addr r, rx_addr node) {
    RX_FRAME_ENTER(F, 0x220);
    rx_addr res = 0;
    if (read_name(r, node, 0x009362a4, F + 0x110) && read_name(r, node, 0x0095eefc, F + 0xc)) {
        rx_addr s = X_CHILD(r, node, 0x0095ef04);
        if (s) {
            rx_addr s0 = X_CHILD(r, s, 0);
            if (s0) {
                rx_addr child = rx_anim_create_node(e, r, s0);
                rx_addr o = rr_alloc(0xd0);
                if (o) res = rx_anim_ik_construct(o, F + 0x110, child, F + 0xc);
            }
        }
    }
    RX_FRAME_LEAVE(F, 0x220);
    return res;
}
/* 006b6120 "expression" (0x60) / 006b61e0 "procedural" (0x70) (ECX = entity, EDX = reader; s0 node; RET 4) */
static rx_addr factory_single(rx_addr e, rx_addr r, rx_addr node, int procedural) {
    RX_FRAME_ENTER(F, 0x110);
    rx_addr res = 0;
    if (read_name(r, node, 0x009362a4, F + 0xc)) {
        rx_addr s = X_CHILD(r, node, 0x0095ef04);
        if (s) {
            rx_addr s0 = X_CHILD(r, s, 0);
            if (s0) {
                rx_addr child = rx_anim_create_node(e, r, s0);
                rx_addr o = rr_alloc(procedural ? 0x70 : 0x60);
                if (o) res = procedural ? rx_anim_procedural_construct(o, F + 0xc, child) : rx_anim_expression_construct(o, F + 0xc, child);
            }
        }
    }
    RX_FRAME_LEAVE(F, 0x110);
    return res;
}
rx_addr rx_anim_factory_expression(rx_addr e, rx_addr r, rx_addr node) { return factory_single(e, r, node, 0); }
rx_addr rx_anim_factory_procedural(rx_addr e, rx_addr r, rx_addr node) { return factory_single(e, r, node, 1); }

/* AnimationEntity::vf28 0082dbd0 (this = entity; name, reader, node; RET 0xc): the animator of a node by its element name */
rx_addr rx_animation_entity_vf28(rx_addr e, rx_addr name, rx_addr r, rx_addr node) {
    if (!crt_stricmp(name, 0x00975940)) return rx_anim_factory_blend(e, r, node);
    if (!crt_stricmp(name, 0x00975948)) return rx_anim_factory_motion(r, e, node);
    if (!crt_stricmp(name, 0x00975950)) {
        rx_addr n = X_ATTR(r, node, 0x009362a4);
        if (n) rr_fatal("the \"memory\" animator (0082d3e0) is not ported", node);
        return 0;
    }
    if (!crt_stricmp(name, 0x00975958)) return rx_anim_factory_switch(e, r, node);
    if (!crt_stricmp(name, 0x00975960)) return rx_anim_factory_performance(r, node, e);
    if (!crt_stricmp(name, 0x0097596c)) return rx_anim_factory_additive(e, r, node);
    return 0;
}
RX_FUNCTION(0x0082dbd0, rx_animation_entity_vf28)
/* RiderAnimation::vf28 006b5e50: ragdoll / inversekinematics / featherstone / expression / procedural, else AnimationEntity::vf28 */
rx_addr rx_rider_animation_vf28(rx_addr e, rx_addr name, rx_addr r, rx_addr node) {
    if (!crt_stricmp(name, 0x0095eeb8)) return rx_anim_factory_featherstone(e, r, node);
    if (!crt_stricmp(name, 0x0095eec0)) return rx_anim_factory_ik(e, r, node);
    if (!crt_stricmp(name, 0x0095eed4)) return rx_anim_factory_featherstone(e, r, node);
    if (!crt_stricmp(name, 0x0095eee4)) return rx_anim_factory_expression(e, r, node);
    if (!crt_stricmp(name, 0x0095eef0)) return rx_anim_factory_procedural(e, r, node);
    return rx_animation_entity_vf28(e, name, r, node);
}
RX_FUNCTION(0x006b5e50, rx_rider_animation_vf28)

/* ---------------------------------------------------------------------------------------------------------------------------
 * the tree walks */

/* Animator::vf10 per class (this = animator, s0 walker; RET 4): walker vf0(animator), the children's vf10, walker vf1(animator) */
static void walk_child(rx_addr child, rx_addr w) { uint32_t a = w; rr_vcall(child, 0x28, 1, &a); }
static void walk_enter(rx_addr w, rx_addr a) { uint32_t x = a; rr_vcall(w, 0, 1, &x); }
static void walk_leave(rx_addr w, rx_addr a) { uint32_t x = a; rr_vcall(w, 4, 1, &x); }
uint32_t rx_animator_walk(rx_addr a, rx_addr w) { walk_enter(w, a); walk_leave(w, a); return 0; }                         /* 0083c570 */
RX_FUNCTION(0x0083c570, rx_animator_walk)
uint32_t rx_blend_animator_walk(rx_addr a, rx_addr w) {                                                                     /* 00834530 */
    walk_enter(w, a); walk_child(U32(a, 0x58), w); walk_child(U32(a, 0x5c), w); walk_leave(w, a); return 0;
}
RX_FUNCTION(0x00834530, rx_blend_animator_walk)
uint32_t rx_additive_animator_walk(rx_addr a, rx_addr w) {                                                                  /* 00834c30 */
    walk_enter(w, a); walk_child(U32(a, 0x2c), w); walk_child(U32(a, 0x30), w); walk_leave(w, a); return 0;
}
RX_FUNCTION(0x00834c30, rx_additive_animator_walk)
uint32_t rx_expression_animator_walk(rx_addr a, rx_addr w) { walk_enter(w, a); walk_child(U32(a, 0x28), w); walk_leave(w, a); return 0; }   /* 006ad770 */
RX_FUNCTION(0x006ad770, rx_expression_animator_walk)
uint32_t rx_ik_animator_walk(rx_addr a, rx_addr w) { walk_enter(w, a); walk_child(U32(a, 0x5c), w); walk_leave(w, a); return 0; }           /* 006b5770 */
RX_FUNCTION(0x006b5770, rx_ik_animator_walk)
uint32_t rx_featherstone_animator_walk(rx_addr a, rx_addr w) { walk_enter(w, a); walk_child(U32(a, 0x30), w); walk_leave(w, a); return 0; } /* 006afa60 */
RX_FUNCTION(0x006afa60, rx_featherstone_animator_walk)
uint32_t rx_switch_animator_walk(rx_addr a, rx_addr w) {                                                                    /* 0083cd80 */
    walk_enter(w, a);
    for (rx_addr p = U32(a, 0xc4); p < U32(a, 0xc8); p += 4) walk_child(U32(p, 0), w);
    walk_leave(w, a);
    return 0;
}
RX_FUNCTION(0x0083cd80, rx_switch_animator_walk)
uint32_t rx_performance_animator_walk(rx_addr a, rx_addr w) {                                                               /* 0083f5a0 */
    walk_enter(w, a);
    for (int32_t k = 0; k < (int32_t)U32(a, 0x8c); k++) walk_child(U32(U32(a, 0x74), 4u * (uint32_t)k), w);
    walk_leave(w, a);
    return 0;
}
RX_FUNCTION(0x0083f5a0, rx_performance_animator_walk)
/* the animators' frame counts (vf2) and name (vf1) */
uint32_t rx_animator_frames1(rx_addr a) { (void)a; return 1; }   /* 00794f70 is the scene unit's; 00411040 2, 00403500 3 */
uint32_t rx_animator_frames2(rx_addr a) { (void)a; return 2; }
RX_FUNCTION(0x00411040, rx_animator_frames2)
uint32_t rx_animator_frames3(rx_addr a) { (void)a; return 3; }
RX_FUNCTION(0x00403500, rx_animator_frames3)
uint32_t rx_animator_name(rx_addr a) { return a + 8; }           /* 006d4bf0 Animator::vf1 */
uint32_t rx_asset_handle_name(rx_addr h) { return h + 4; }       /* 004dce30 AssetHandle vf2 (the handle's name) */
RX_FUNCTION(0x004dce30, rx_asset_handle_name)
RX_FUNCTION(0x006d4bf0, rx_animator_name)

/* FrameCountWalker (0x0097611c): vf0 0082e4e0 adds the animator's frames (max kept at +4), vf1 0082e500 subtracts them */
uint32_t rx_frame_count_walker_enter(rx_addr w, rx_addr a) {
    SU32(w, 8, U32(w, 8) + rr_vcall(a, 8, 0, NULL));
    if ((int32_t)U32(w, 8) > (int32_t)U32(w, 4)) SU32(w, 4, U32(w, 8));
    return 0;
}
RX_FUNCTION(0x0082e4e0, rx_frame_count_walker_enter)
uint32_t rx_frame_count_walker_leave(rx_addr w, rx_addr a) { SU32(w, 8, U32(w, 8) - rr_vcall(a, 8, 0, NULL)); return 0; }
RX_FUNCTION(0x0082e500, rx_frame_count_walker_leave)
/* 008434f0 (EDI = table; s0 &name, s1 &value; RET 8): the Character's animator registry (+0x248): insert or update by name
 * (005c3380 hash, strcmp) */
static void registry_insert(rx_addr t, rx_addr name_p, rx_addr val_p) {
    float n = (float)(int32_t)U32(t, 0), c = (float)(int32_t)U32(t, 8);
    if (c / n > F32(t, 4)) rx_strtab_rehash(t, (uint32_t)rx_cvtt(n * 1.5f));
    rx_addr name = U32(name_p, 0);
    uint32_t h = rx_world_elf_hash(name);
    uint32_t b = h % U32(t, 0);
    for (rx_addr e = U32(U32(t, 0xc), 4 * b); e; e = U32(e, 0xc))
        if (U32(e, 8) == h && crt_strcmp(U32(e, 0), name) == 0) { SU32(e, 4, U32(val_p, 0)); return; }
    uint32_t sz = 0x10;
    rx_addr e = rr_vcall(U32(t, 0x18), 4, 1, &sz);
    if (e) SU32(e, 0xc, 0);
    SU32(e, 0, U32(name_p, 0));
    SU32(e, 4, U32(val_p, 0));
    SU32(e, 8, h);
    SU32(e, 0xc, U32(U32(t, 0xc), 4 * b));
    SU32(U32(t, 0xc), 4 * b, e);
    SU32(t, 8, U32(t, 8) + 1);
}
/* AnimatorRegisterWalker vf0 0082e520 (this = walker {vtable 0x00976128, the Character}; animator): name -> animator */
uint32_t rx_animator_register_walker_enter(rx_addr w, rx_addr a) {
    RX_FRAME_ENTER(F, 8);
    SU32(F, 0, anim_name(a));
    SU32(F, 4, a);
    registry_insert(U32(w, 4) + 0x248, F, F + 4);
    RX_FRAME_LEAVE(F, 8);
    return 0;
}
RX_FUNCTION(0x0082e520, rx_animator_register_walker_enter)

static void pose_reset(rx_addr rec) { rx_race_pose_reset(rec); }   /* 005089d0 */
/* 0082e3a0 (ESI = character, EAX = bone count, ECX = root): the root, the frame count (+0x230), the pose pool ((frames + 2) * bones
 * records of 0x30 at +0x228, _aligned_malloc), the bone map +0x23c, the pool flags +0x22c (the first used), the registry walk */
void rx_character_set_root(rx_addr ch, uint32_t bones, rx_addr root) {
    SU32(ch, 0x234, bones);
    SU32(ch, 0x224, root);
    RX_FRAME_ENTER(F, 0x20);
    SU32(F, 0x10, 0x0097611c); SU32(F, 0x14, 1); SU32(F, 0x18, 0);
    walk_child(root, F + 0x10);
    uint32_t frames = U32(F, 0x14);
    uint32_t n = (frames + 2) * U32(ch, 0x234);
    SU32(ch, 0x230, frames);
    rx_addr pool = rx_crt_aligned_malloc(rx_mul_sat(n, 0x30), 0x10);
    if (pool) rx_rider_vector_ctor(pool, 0x30, (int32_t)n, pose_reset);
    SU32(ch, 0x228, pool);
    SU32(ch, 0x23c, rr_alloc(rx_mul_sat(U32(ch, 0x234), 4)));
    rx_addr flags = rr_alloc(rx_mul_sat(U32(ch, 0x230), 4));
    SU32(ch, 0x22c, flags);
    rr_zero8(flags, U32(ch, 0x230) * 4);
    SU32(U32(ch, 0x22c), 0, 1);
    SU32(F, 0xc, 0x00976128); SU32(F, 0x10, ch);
    walk_child(U32(ch, 0x224), F + 0xc);
    RX_FRAME_LEAVE(F, 0x20);
}
/* 0082e560 (ECX = character, EAX = node set): the bone parent map (+0x23c: the parent's index, -1 for the bones under the set's extra
 * root, whose record is kept at +0x244) */
void rx_character_bind_hierarchy(rx_addr ch, rx_addr ns) {
    if (!ns) return;
    SU32(ch, 0x240, ns);
    for (uint32_t i = 0; (int32_t)i < (int32_t)U32(ns, 0xc); i++) {
        rx_addr hs = U32(ch, 0x240);
        rx_addr rec = U32(hs, 8) + 16 * i;
        rx_addr p = ((uint32_t)U8(rec, 0xf) << 4) + U32(U32(rec, 0), 8);
        int32_t idx = p ? (int32_t)(p - U32(hs, 8)) >> 4 : -1;
        if ((uint32_t)idx == U32(ch, 0x234)) { SU32(ch, 0x244, rec); SU32(U32(ch, 0x23c), 4 * i, 0xffffffffu); }
        else SU32(U32(ch, 0x23c), 4 * i, (uint32_t)idx);
    }
}

/* 0082dd00 (ECX = entity): the tree of the entity's tree document (+0x140: a BXML handle; the reader = asset - 0x460): its root's
 * "animation" child's first child is the root animator (0082db70); a "null_bone" attribute names the Character's null bone (0082e350,
 * not in the riders' documents) */
void rx_animation_entity_load_tree(rx_addr e) {
    rx_addr h = U32(e, 0x140);
    SU32(h, 0x4c, 1);
    rx_addr r = U32(h, 0x44) ? U32(h, 0x44) - 0x460 : 0;
    RX_FRAME_ENTER(F, 0x140);
    rx_addr root = X_ROOT(r);
    X_NAME(r, root, F + 0x10, 0x20);
    if (crt_stricmp(F + 0x10, 0x00975978) == 0) {
        rx_addr first = X_CHILD(r, root, 0);
        if (first) {
            rx_addr nb = X_ATTR(r, root, 0x00975984);
            if (nb) rr_fatal("animation tree null_bone (0082e350) not ported", e);
            rx_addr a = rx_anim_create_node(e, r, first);
            if (a) rx_character_set_root(U32(e, 0x144), U32(e, 0x18c), a);
            SU32(U32(e, 0x140), 0x4c, 0);
        }
    }
    RX_FRAME_LEAVE(F, 0x140);
}
