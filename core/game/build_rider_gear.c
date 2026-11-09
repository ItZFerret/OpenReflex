/* build_rider_gear.c -- construction unit 7 (riders, core/notes/construction.md section 13): the gear of a rider entry put on its
 * model. VehicleLoader_Assemble 0053ddc0 (the parts' packages: their colour materials over the model materials, VehicleLoader_
 * OverrideMaterials 0053d8b0 / MaterialKey 00543fc0; the LOD levels of the parts' models (0053c... helpers 00543410 / 005434c0 /
 * 005435a0 / 00544e70 / 00544f70) as LOD records of the rider's model copy, each part node listed in its LOD with the node's bounds
 * grown over the part's), and the containers it uses: the model's LOD vector (std::vector<LodRecord>, 0x1c records: +0 the LOD
 * threshold, +4 a std::vector of 16-byte entries {+0 part, +4 node index, +8 model handle, +0xc material}) with its copy / grow /
 * destroy (007f92d0 family, 007f99a0, 00545120), the entries' push_back (00416f10 / 00417e60) and the Rdb int arrays {data, count,
 * capacity, growth, max} (007cff20). Hand transliterations of the disassembly.
 *
 * The rider entry L (the rider manager's record of a rider; build_rider.h): +0x14 the high-resolution flag, +0x1c.. the 8 current parts,
 * +0x3c.. the 8 pending parts. A part P: +0 std::string the material package, +0x1c std::string the model package, +0x38 the part has
 * a high-resolution package ("HR_<name>"), +0x40 its gear slot, +0x48 references. */
#include "build_rider.h"
#include "build_asset.h"
#include "build_vehicle.h"
#include "build_rider_util.h"
#include "crt.h"
#include "anim.h"

#define RR_LOD_TABLE 0x009c54a8u   /* the 8 LOD distance thresholds (int) */
#define RR_HR_FORMAT 0x00940560u   /* "HR_%s" */
#define RR_ALLOC_CS  0x00d61704u   /* the allocator's critical section (taken when [0x00da67d8]) */

static void invalid_parameter(void) { rr_fatal("_invalid_parameter_noinfo (an iterator / index check failed)", 0); }
static uint32_t lod_count(rx_addr vec) { return (uint32_t)((int32_t)(U32(vec, 0x10) - U32(vec, 0xc)) / 0x1c); }
/* a std::string's characters (msvcp90: +4 the inline buffer or the pointer, +0x18 the capacity) */
static rx_addr str_chars(rx_addr s) { return U32(s, 0x18) >= 0x10 ? U32(s, 4) : s + 4; }

/* 0078fd60 (EDI = buffer, ESI = size; cdecl format, ...): _vsnprintf, always terminated. Here: "HR_%s" of one name (the argument
 * at a game address: `arg`). */
void rx_gear_hr_name(rx_addr buf, rx_addr name) {
    RX_FRAME_ENTER(arg, 0x10);   /* the argument (as the original's pushed one, below its frame; the CRT call runs below it) */
    SU32(arg, 0, name);
    int32_t n = crt_vsnprintf(buf, 0x40, RR_HR_FORMAT, arg);
    if (n > 0 && (uint32_t)n < 0x40) SU8(buf, (uint32_t)n, 0);
    else SU8(buf, 0x3f, 0);
    RX_FRAME_LEAVE(arg, 0x10);
}

/* the gear loader's package cache (0x00d92ef4 + 8): VehicleLoader_FindPackage 00543db0 (EAX = cache, EDI = name) of unit 6 */
static rx_addr find_package(rx_addr name) {
    rx_addr cache = U32(0, RR_GEARLOADER) + 8;
    return rx_vehloader_find_package(cache, name);
}

/* an asset handle's type tag (its vf1, EDX:EAX). Every handle class's vf1 is `mov eax, lo; mov edx, hi; ret` (model 007d1440,
 * material 00793a70 ...); BXMLAssetHandle 008221a0 returns +0x50 / +0x54. Dispatched through the vtable: build_asset_types.c. */
uint32_t rx_asset_handle_is(rx_addr h, uint32_t lo, uint32_t hi) {
    uint64_t t = rx_asset_handle_type(h);   /* the virtual call (build_asset_types.c) */
    return (uint32_t)t == lo && (uint32_t)(t >> 32) == hi;
}
#define TAG_MODEL_LO    0x65646f6du   /* "model" */
#define TAG_MODEL_HI    0x0000006cu
#define TAG_MATERIAL_LO 0x6574616du   /* "material" */
#define TAG_MATERIAL_HI 0x6c616972u
/* an asset handle's name (vf2: AssetHandle 004dce30 = handle + 4) */
static rx_addr handle_name(rx_addr h) { return rr_vcall(h, 8, 0, NULL); }

/* ==== Rdb int arrays {+0 data, +4 count, +8 capacity, +0xc growth, +0x10 max} ==== */
/* 007cff20 (ESI = array, s0 = capacity; RET 4): reallocated to `capacity` (the old capacity's words copied) */
void rx_iarr_reserve(rx_addr a, uint32_t cap) {
    rx_addr d = rr_alloc(rx_mul_sat(cap, 4));
    crt_memcpy(d, U32(a, 0), U32(a, 8) * 4);
    rx_reflex_aligned_free(U32(a, 0));
    SU32(a, 0, d);
    SU32(a, 8, cap);
}
void rx_iarr_push(rx_addr a, uint32_t v) {
    if ((int32_t)U32(a, 4) >= (int32_t)U32(a, 8)) rx_iarr_reserve(a, U32(a, 0xc) + U32(a, 8));
    SU32(U32(a, 0), 4 * U32(a, 4), v);
    SU32(a, 4, U32(a, 4) + 1);
}
/* 00544e70 (ESI = array; cdecl value): inserted in ascending order unless present */
void rx_iarr_insert_sorted(rx_addr a, int32_t v) {
    int32_t n = (int32_t)U32(a, 4), i = 0;
    for (; i < n; i++) {
        int32_t x = (int32_t)U32(U32(a, 0), 4 * (uint32_t)i);
        if (x == v) return;
        if (x > v) break;
    }
    if (n >= (int32_t)U32(a, 8)) rx_iarr_reserve(a, U32(a, 0xc) + U32(a, 8));   /* the same steps inline */
    if (i < n) {
        if ((uint32_t)i != U32(a, 4)) crt_memmove(U32(a, 0) + 4 * (uint32_t)i + 4, U32(a, 0) + 4 * (uint32_t)i, (U32(a, 4) - (uint32_t)i) * 4);
        SU32(U32(a, 0), 4 * (uint32_t)i, (uint32_t)v);
    } else SU32(U32(a, 0), 4 * U32(a, 4), (uint32_t)v);
    SU32(a, 4, U32(a, 4) + 1);
}
/* 00544f70 (EAX = out; cdecl in, lo, hi): the indices of in's values in [lo, hi] (ascending: stops at the first above hi) */
void rx_iarr_indices_in_range(rx_addr out, rx_addr in, int32_t lo, int32_t hi) {
    for (int32_t i = 0; i < (int32_t)U32(in, 4); i++) {
        int32_t v = (int32_t)U32(U32(in, 0), 4 * (uint32_t)i);
        if (v < lo) continue;
        if (v > hi) break;
        rx_iarr_push(out, (uint32_t)i);
    }
}
void rx_iarr_init(rx_addr a, uint32_t cap) {
    rr_zero(a, 0x14);
    SU32(a, 0, rr_alloc(rx_mul_sat(cap, 4)));
    SU32(a, 4, 0); SU32(a, 8, cap); SU32(a, 0xc, cap); SU32(a, 0x10, 0x7fffffffu);
}

/* ==== the LOD entries: std::vector of 16-byte entries (object: +0 proxy, +0xc begin, +0x10 end, +0x14 capacity end) ==== */
/* 00419be0 (ECX = n): the buffer of n entries */
static rx_addr ent_alloc(uint32_t n) {
    if (n && 0xffffffffu / n < 0x10) rr_fatal("std::vector<entry> allocation too large (bad_alloc)", n);
    return rr_alloc(n << 4);
}
/* 00416f10 (EAX = vector, s0 = &entry; RET 4): push_back (00417e60 the grow: capacity * 1.5, at least size + 1) */
void rx_lod_entries_push_back(rx_addr v, rx_addr val) {
    rx_addr b = U32(v, 0xc), e = U32(v, 0x10);
    uint32_t cap = b ? (U32(v, 0x14) - b) >> 4 : 0, size = (e - b) >> 4;
    if (size < cap) { rr_copy(e, val, 0x10); SU32(v, 0x10, e + 0x10); return; }
    if (0x0fffffffu - size < 1) rr_fatal("std::vector<entry> too long", v);
    uint32_t half = cap >> 1, ncap = 0x0fffffffu - half < cap ? 0 : cap + half;
    if (ncap < size + 1) ncap = size + 1;
    rx_addr nb = ent_alloc(ncap);
    rx_addr p = nb;
    for (rx_addr s = b; s != e; s += 0x10, p += 0x10) rr_copy(p, s, 0x10);   /* 005452a0 [begin, where) */
    rr_copy(p, val, 0x10);                                                      /* 00417810 the new one */
    uint32_t nsize = ((U32(v, 0x10) - U32(v, 0xc)) >> 4) + 1;
    if (U32(v, 0xc)) rx_reflex_aligned_free(U32(v, 0xc));
    SU32(v, 0x14, nb + (ncap << 4));
    SU32(v, 0x10, nb + (nsize << 4));
    SU32(v, 0xc, nb);
}
/* 007f9c90 (stdcall dst, src; RET 8): copy constructor (a new proxy; the buffer exactly the source's size) */
static void ent_copy_construct(rx_addr d, rx_addr s) {
    rx_addr p = rr_alloc(4);
    if (p) SU32(p, 0, d);
    SU32(d, 0, p);
    uint32_t n = (U32(s, 0x10) - U32(s, 0xc)) >> 4;
    SU32(d, 0xc, 0); SU32(d, 0x10, 0); SU32(d, 0x14, 0);
    if (!n) return;
    if (n > 0x0fffffffu) rr_fatal("std::vector<entry> too long", n);
    rx_addr nb = ent_alloc(n);
    SU32(d, 0xc, nb); SU32(d, 0x10, nb); SU32(d, 0x14, nb + (n << 4));
    rx_addr q = nb;
    for (rx_addr x = U32(s, 0xc); x != U32(s, 0x10); x += 0x10, q += 0x10) rr_copy(q, x, 0x10);   /* 0041b010 */
    SU32(d, 0x10, q);
}

/* ==== the LOD vector: std::vector<LodRecord> (object: +0 proxy, +0xc begin, +0x10 end, +0x14 capacity end) ==== */
/* 0042bde0 (ECX = n): the buffer of n records */
static rx_addr lod_alloc(uint32_t n) {
    if (n && 0xffffffffu / n < 0x1c) rr_fatal("std::vector<LodRecord> allocation too large (bad_alloc)", n);
    return rr_alloc(n * 0x1c);
}
/* a record's copy constructor (007fa3a0 / 007fa440 body): the threshold, the entries */
static void lod_copy_construct(rx_addr d, rx_addr s) { SU32(d, 0, U32(s, 0)); ent_copy_construct(d + 4, s + 4); }
/* 007fa440 (via 007fa2e0: EDX = dest; s0 first, s1 last): uninitialised copy -> the end of the copies */
static rx_addr lod_uninit_copy(rx_addr first, rx_addr last, rx_addr dest) {
    for (; first != last; first += 0x1c, dest += 0x1c) if (dest) lod_copy_construct(dest, first);
    return dest;
}
/* 00545120 (EAX = first; cdecl last): the records' destructors (the entries' buffer and proxy freed) */
static void lod_destroy(rx_addr first, rx_addr last) {
    for (rx_addr r = first; r != last; r += 0x1c) {
        if (U32(r, 0x10)) rx_reflex_aligned_free(U32(r, 0x10));
        SU32(r, 0x10, 0); SU32(r, 0x14, 0); SU32(r, 0x18, 0);
        rx_reflex_aligned_free(U32(r, 4));
    }
}
/* 005443e0 erase(begin, end) / 005440b0 clear: nothing to move down; the records destroyed, end = begin */
void rx_model_lods_clear(rx_addr v) {
    rx_addr b = U32(v, 0xc), e = U32(v, 0x10);
    if (b == e) return;
    lod_destroy(b, e);
    SU32(v, 0x10, b);
}
/* 007f9450 (s0 = vector, s1 = &record; RET 8): push_back; without room 007f9880 -> 007f99a0 _Insert_n(end, 1, record): a buffer of
 * capacity * 1.5 (at least size + 1), the records copied over (copy constructors), the old ones destroyed and their buffer freed */
static void lod_push_back(rx_addr v, rx_addr val) {
    rx_addr b = U32(v, 0xc), e = U32(v, 0x10);
    uint32_t cap = b ? (uint32_t)((int32_t)(U32(v, 0x14) - b) / 0x1c) : 0, size = lod_count(v);
    if (size < cap) { lod_copy_construct(e, val); SU32(v, 0x10, e + 0x1c); return; }
    if (0x09249249u - size < 1) rr_fatal("std::vector<LodRecord> too long", v);
    uint32_t half = cap >> 1, ncap = 0x09249249u - half < cap ? 0 : cap + half;
    if (ncap < size + 1) ncap = size + 1;
    rx_addr nb = lod_alloc(ncap);
    rx_addr p = lod_uninit_copy(b, e, nb);      /* [begin, where) */
    lod_copy_construct(p, val);                 /* 007f9970: the new record */
    p += 0x1c;
    lod_uninit_copy(e, e, p);                   /* [where, end): none */
    uint32_t nsize = lod_count(v) + 1;
    if (U32(v, 0xc)) {
        lod_destroy(U32(v, 0xc), U32(v, 0x10));
        rx_reflex_aligned_free(U32(v, 0xc));
    }
    SU32(v, 0x14, nb + ncap * 0x1c);
    SU32(v, 0x10, nb + nsize * 0x1c);
    SU32(v, 0xc, nb);
}
/* 007f92d0 (EAX = destination, s0 source; RET 4): std::vector<LodRecord> assignment. The forms construction reaches: an empty
 * source (005440b0: the destination cleared) and a destination without a buffer (007f9940: exactly the source's size, then the
 * copies 007fa2e0). Assigning over existing records (00544ff0) is not ported. */
void rx_model_lods_assign(rx_addr dv, rx_addr sv) {
    if (dv == sv) return;
    uint32_t ns = lod_count(sv);
    if (ns == 0) { rx_model_lods_clear(dv); return; }
    uint32_t nd = lod_count(dv);
    uint32_t cap = U32(dv, 0xc) ? (uint32_t)((int32_t)(U32(dv, 0x14) - U32(dv, 0xc)) / 0x1c) : 0;
    if (ns <= nd || ns <= cap || U32(dv, 0xc)) rr_fatal("LOD vector assignment over records (00544ff0) not ported", dv);
    SU32(dv, 0xc, 0); SU32(dv, 0x10, 0); SU32(dv, 0x14, 0);
    if (ns > 0x09249249u) rr_fatal("std::vector<LodRecord> too long", ns);
    rx_addr nb = lod_alloc(ns);
    SU32(dv, 0xc, nb); SU32(dv, 0x10, nb); SU32(dv, 0x14, nb + ns * 0x1c);
    SU32(dv, 0x10, lod_uninit_copy(U32(sv, 0xc), U32(sv, 0x10), nb));
}

/* 007d4ce0 (EDI = model): an empty LOD record pushed (a stack temporary {0, a new proxy, no entries}), the model's LOD count + 1 */
void rx_model_push_empty_lod(rx_addr m) {
    RX_FRAME_ENTER(F, 0x24);
    rx_addr T = F + 8;
    rx_addr p = rr_alloc(4);
    if (p) SU32(p, 0, T + 4); else p = 0;
    SU32(T, 4, p);
    SU32(T, 0x10, 0); SU32(T, 0x14, 0); SU32(T, 0x18, 0);
    SU32(T, 0, 0);
    lod_push_back(m + 0x124, T);
    if (U32(T, 0x10)) rx_reflex_aligned_free(U32(T, 0x10));
    SU32(T, 0x10, 0); SU32(T, 0x14, 0); SU32(T, 0x18, 0);
    rx_reflex_aligned_free(U32(T, 4));
    SU32(m, 0x118, U32(m, 0x118) + 1);
    RX_FRAME_LEAVE(F, 0x24);
}

/* ==== the LOD levels of the parts ==== */
/* 00543410 (EDI = obj): the threshold set {vtable 0x00940790, Rdb int array of capacity 8 holding 0} */
void rx_lod_set_init(rx_addr obj) {
    SU32(obj, 0, 0x00940790u);
    rx_iarr_init(obj + 4, 8);
    rx_iarr_push(obj + 4, 0);
}
/* 005434c0 (stdcall set, package; RET 8): the thresholds of every LOD of the package's models (the handles stay marked loading) */
void rx_lod_set_add_package(rx_addr obj, rx_addr pkg) {
    for (uint32_t j = 0; j < U32(pkg, 0x84); j++) {
        rx_addr h = U32(U32(pkg, 0x80) + 0x1c * j, 0);
        if (!rx_asset_handle_is(h, TAG_MODEL_LO, TAG_MODEL_HI)) continue;
        rx_addr m = U32(h, 0x44);
        SU32(h, 0x4c, 1);
        for (uint32_t lod = 0; lod < U32(m, 0x118); lod++) {
            if (lod >= lod_count(m + 0x124)) invalid_parameter();
            rx_iarr_insert_sorted(obj + 4, (int32_t)U32(0, RR_LOD_TABLE + 4 * (lod < 7 ? lod : 7)));
        }
    }
}
/* 005435a0 (stdcall set, model; RET 8): the model's LOD records rebuilt: one per threshold, the threshold in +0, no entries */
void rx_lod_set_apply(rx_addr obj, rx_addr m) {
    /* the original's frame (sub esp, 0x24; push ebx, ebp, esi, edi): its calls of 007d4ce0 enter at the original's ESP, so the empty
     * record's container proxy keeps the original's stack address of the record (a stale pointer the heap block holds) */
    const rx_addr e = rx_sp(), B = e - 0x34;
    rx_set_sp(B);
    SU32(m, 0x120, 0);
    SU32(m, 0x118, 0);
    rx_model_lods_clear(m + 0x124);
    while (U32(m, 0x118) < U32(obj, 8)) {   /* EDI = m; call 007d4ce0 (return 0054360e) */
        SU32(B, -4, 0x0054360eu); rx_set_sp(B - 4);
        rx_model_push_empty_lod(m);
        rx_set_sp(B);
    }
    rx_set_sp(e);
    for (uint32_t i = 0; (int32_t)i < (int32_t)U32(m, 0x118); i++) {
        if (i >= lod_count(m + 0x124)) invalid_parameter();
        rx_addr R = U32(m, 0x130) + 0x1c * i;
        SU32(R, 0x14, U32(R, 0x10));   /* 00544300 erase(begin, end) of the entries */
        SU32(R, 0, 0);
        SU32(R, 0, U32(U32(obj, 4), 4 * i));
    }
}

/* ==== the materials ==== */
/* 00543fc0 MaterialKey (EAX = size, ESI = out; cdecl name): the name without digits, runs of '_' as one, cut before the second '_'
 * (the third after an "HR_" prefix) */
void rx_material_key(rx_addr out, uint32_t size, rx_addr name) {
    crt_strncpy(out, name, size - 1);
    SU8(out, size - 1, 0);
    int32_t len = (int32_t)crt_strlen(name);
    if (len >= (int32_t)size) len = (int32_t)size;
    uint32_t limit = (U8(name, 0) == 'H' && U8(name, 1) == 'R' && U8(name, 2) == '_') ? 3 : 2;
    uint32_t d = 0, seps = 0;
    if (len <= 0) { SU8(out, d, 0); return; }
    for (int32_t s = 0; s < len; s++) {
        uint8_t c = U8(out, (uint32_t)s);
        if (c >= '0' && c <= '9') continue;
        if (d && U8(out, d - 1) == '_' && c == '_') continue;
        SU8(out, d, c);
        if (c == '_' && ++seps == limit) break;
        d++;
    }
    SU8(out, d, 0);
}

/* 0053d8b0 VehicleLoader_OverrideMaterials (stdcall entry, a, b; RET 0xc): the materials of part b's material package put on the
 * entries of part a's models whose material has the same key (prefix match on b's keys). 0 when part a is absent. */
uint32_t rx_vehloader_override_materials(rx_addr L, uint32_t a, uint32_t b) {
    RX_FRAME_ENTER(F, 0x830);
    for (uint32_t k = 0; k < 6; k++) SU32(F, 0x40 + 4 * k, 0);
    rx_addr pa = U32(L, 0x1c + 4 * a);
    if (!pa) { RX_FRAME_LEAVE(F, 0x830); return 0; }
    rx_addr na = str_chars(pa + 0x1c);
    if (U32(L, 0x14) && U32(pa, 0x38)) { rx_gear_hr_name(F + 0xc8, na); na = F + 0xc8; }
    rx_addr pb = U32(L, 0x1c + 4 * b);
    rx_addr nb = str_chars(pb);
    if (U32(L, 0x14) && U32(pb, 0x38)) { rx_gear_hr_name(F + 0x88, nb); nb = F + 0x88; }
    rx_addr pkg_a = (na && U8(na, 0)) ? find_package(na) : 0;
    rx_addr pkg_b = (nb && U8(nb, 0)) ? find_package(nb) : 0;
    if (!pkg_a || !pkg_b) { RX_FRAME_LEAVE(F, 0x830); return 1; }
    /* the materials of b's package (at most 6: the original's array) and their keys (0x104 each at +0x210) */
    uint32_t n = 0;
    for (uint32_t j = 0; j < U32(pkg_b, 0x84); j++) {
        rx_addr h = U32(U32(pkg_b, 0x80) + 0x1c * j, 0);
        if (rx_asset_handle_is(h, TAG_MATERIAL_LO, TAG_MATERIAL_HI)) {
            if (n >= 6) rr_fatal("more than 6 materials in a part package (the original's array overflows)", pkg_b);
            SU32(F, 0x40 + 4 * n++, h);
        }
    }
    for (uint32_t k = 0; k < n; k++) {
        rx_addr key = F + 0x210 + 0x104 * k;
        rx_material_key(key, 0x104, handle_name(U32(F, 0x40 + 4 * k)));
        SU32(F, 0x58 + 4 * k, crt_strlen(key));
        /* the original then compares the key with the earlier ones both ways (_strnicmp) and discards the results */
    }
    for (uint32_t j = 0; j < U32(pkg_a, 0x84); j++) {
        rx_addr h = U32(U32(pkg_a, 0x80) + 0x1c * j, 0);
        if (!rx_asset_handle_is(h, TAG_MODEL_LO, TAG_MODEL_HI)) continue;
        rx_addr m = U32(h, 0x44);
        SU32(h, 0x4c, 1);
        for (uint32_t lod = 0; lod < U32(m, 0x118); lod++) {
            if (lod >= lod_count(m + 0x124)) invalid_parameter();
            rx_addr R = U32(m, 0x130) + 0x1c * lod;
            for (uint32_t e = 0; e < (U32(R, 0x14) - U32(R, 0x10)) >> 4; e++) {
                rx_addr E = U32(R, 0x10) + 0x10 * e;
                for (uint32_t k = 0; k < n; k++) {
                    rx_material_key(F + 0x108, 0x104, handle_name(U32(E, 0xc)));
                    if (crt_strnicmp(F + 0x108, F + 0x210 + 0x104 * k, U32(F, 0x58 + 4 * k)) != 0) continue;
                    SU32(F, 0x70 + 4 * k, 1);
                    SU32(E, 0xc, U32(F, 0x40 + 4 * k));
                }
            }
        }
        SU32(h, 0x4c, 0);
    }
    RX_FRAME_LEAVE(F, 0x830);
    return 1;
}

/* node bounds (0x50 records: +0 centre, +0x10 half extents, +0x40 radius) and the node's matrix (64 bytes): helpers of the grow */
static float row_len(rx_addr m, uint32_t o) {
    float z = F32(m, o + 8), y = F32(m, o + 4), x = F32(m, o);
    return sqrtf(z * z + y * y + x * x);
}

/* 0053ddc0 VehicleLoader_Assemble (ECX = model (the rider's model copy), EDX = entry; s0 = manager; RET 4) */
void rx_vehloader_assemble(rx_addr model, rx_addr L, rx_addr mgr) {
    /* the original's frame (push ebp; and esp, -16; sub esp, 0x174; three pushes) from the entry stack pointer (the return address
     * slot): two vectors it stores are read back from the frame with their w words never written (the frame's old contents) */
    const rx_addr sp0 = rx_sp();
    const rx_addr F = ((sp0 - 4u) & ~15u) - 0x180u;
    rx_set_sp(F);
    /* GetTickCount (0x0091601c) at the start and the end: results unused */
    for (uint32_t k = 0; k < 8; k++) {   /* the pending parts become current once their packages are there */
        rx_addr slot = L + 0x3c + 4 * k;
        if (!U32(slot, 0)) continue;
        if (!rx_rider_part_packages_ready(U32(slot, 0), U32(L, 0x14))) continue;
        rx_addr old = U32(slot, -0x20);
        if (old) {
            rx_rider_part_release_packages(old, U32(L, 0x14));   /* 0053d170 */
            SU32(old, 0x48, U32(old, 0x48) - 1);
            SU32(F, 0x24, old);
            if (!U32(old, 0x48)) {
                rx_rider_ptr_array_remove(mgr, F + 0x24);
                rx_rider_part_destroy(old);
                rx_reflex_aligned_free(old);
            }
        }
        SU32(slot, -0x20, U32(slot, 0));
        SU32(slot, 0, 0);
    }
    for (uint32_t k = 0; k < 8; k++) SU32(F, 0xc0 + 4 * k, rx_vehloader_override_materials(L, k, k));
    if (U32(F, 0xcc) && U32(F, 0xd0)) rx_vehloader_override_materials(L, 3, 4);
    if (U32(F, 0xc8) && U32(F, 0xd8)) rx_vehloader_override_materials(L, 2, 6);
    rx_addr set = F + 0xc0;   /* the LOD thresholds of all parts' models */
    rx_lod_set_init(set);
    for (uint32_t k = 0; k < 8; k++) {
        rx_addr P = U32(L, 0x1c + 4 * k);
        if (!P || !U32(P, 0x30)) continue;
        rx_addr nm = str_chars(P + 0x1c);
        rx_addr pkg;
        if (U32(L, 0x14) && U32(P, 0x38)) { rx_gear_hr_name(F + 0xf0, nm); nm = F + 0xf0; pkg = U8(nm, 0) ? find_package(nm) : 0; }
        else pkg = (nm && U8(nm, 0)) ? find_package(nm) : 0;
        rx_lod_set_add_package(set, pkg);
    }
    rx_lod_set_apply(set, model);
    for (uint32_t k = 0; k < 8; k++) {
        rx_addr P = U32(L, 0x1c + 4 * k);
        if (!P) continue;
        rx_addr nm = str_chars(P + 0x1c);
        if (U32(L, 0x14) && U32(P, 0x38)) { rx_gear_hr_name(F + 0x138, nm); nm = F + 0x138; }
        else if (!nm) continue;
        if (!U8(nm, 0)) continue;
        rx_addr pkg = find_package(nm);
        if (!pkg || !U32(pkg, 0x94)) continue;
        for (uint32_t j = 0; j < U32(pkg, 0x84); j++) {
            rx_addr h = U32(U32(pkg, 0x80) + 0x1c * j, 0);
            if (!rx_asset_handle_is(h, TAG_MODEL_LO, TAG_MODEL_HI) || !h) continue;
            SU32(h, 0x4c, 1);
            rx_addr M = U32(h, 0x44);
            for (uint32_t lod = 0; M && lod < U32(M, 0x118); lod++) {
                rx_addr vec = F + 0x98;   /* the model's LOD records this LOD's range covers */
                rx_iarr_init(vec, 10);
                int32_t lo = (int32_t)U32(0, RR_LOD_TABLE + 4 * (lod < 7 ? lod : 7)), hi;
                if (lod + 1 >= U32(M, 0x118)) hi = 500;
                else {
                    hi = (int32_t)U32(0, RR_LOD_TABLE + 4 * (lod + 1 < 7 ? lod + 1 : 7)) - 1;
                    if (hi == 0) hi = 0x7fffffff;
                }
                rx_iarr_indices_in_range(vec, set + 4, lo, hi);
                for (uint32_t i = 0; i < U32(vec, 4); i++) {
                    if (lod >= lod_count(M + 0x124)) invalid_parameter();
                    uint32_t ri = U32(U32(vec, 0), 4 * i);
                    rx_addr Ml = U32(M, 0x130) + 0x1c * lod;
                    if (ri >= lod_count(model + 0x124)) invalid_parameter();
                    rx_addr R = U32(model, 0x130) + 0x1c * ri;
                    for (uint32_t e = 0; e < (U32(Ml, 0x14) - U32(Ml, 0x10)) >> 4; e++) {
                        rx_addr E = U32(Ml, 0x10) + 0x10 * e;
                        rx_addr N = (U32(E, 4) << 4) + U32(M, 0x18);
                        rx_addr r = rx_hierarchy_find_node(model + 0x10, U32(U32(N, 0), 0x10) + U16(N, 8));
                        if (!r) continue;
                        int32_t idx = (int32_t)(r - U32(model, 0x18)) >> 4;
                        if (idx < 0) continue;
                        rx_addr h2 = U32(E, 8);
                        SU32(h2, 0x4c, 1);
                        uint32_t arg = U32(E, 0);
                        rx_addr t = rr_vcall(U32(h2, 0x44), 0x10, 1, &arg);
                        uint32_t t4 = U32(t, 4);
                        SU32(U32(E, 8), 0x4c, 0);
                        if (t4 == 0) {   /* the node's bounds grown over the part's (scaled by the matrices' row lengths) */
                            rx_addr N2 = U32(model, 0x18) + ((uint32_t)idx << 4);
                            rr_copy(F + 0xf0, U32(U32(N, 0), 0x18) + ((uint32_t)U8(N, 0xb) << 6), 0x40);
                            float sx = row_len(F + 0xf0, 0), sy = row_len(F + 0xf0, 0x10), sz = row_len(F + 0xf0, 0x20);
                            rr_copy(F + 0xf0, U32(U32(N2, 0), 0x18) + ((uint32_t)U8(N2, 0xb) << 6), 0x40);
                            float f0 = sx / row_len(F + 0xf0, 0);
                            float f1 = sy / row_len(F + 0xf0, 0x10);
                            float f2 = sz / row_len(F + 0xf0, 0x20);
                            rx_addr Bd = U32(U32(N2, 0), 0x20) + (uint32_t)U8(N2, 0xb) * 0x50;
                            rx_addr Cd = U32(U32(N, 0), 0x20) + (uint32_t)U8(N, 0xb) * 0x50;
                            float maxx = F32(Bd, 0x10) + F32(Bd, 0), minx = F32(Bd, 0) - F32(Bd, 0x10);
                            float maxy = F32(Bd, 0x14) + F32(Bd, 4), miny = F32(Bd, 4) - F32(Bd, 0x14);
                            float maxz = F32(Bd, 0x18) + F32(Bd, 8), minz = F32(Bd, 8) - F32(Bd, 0x18);
                            float cx = F32(Cd, 0) * f0, cy = F32(Cd, 4) * f1, cz = F32(Cd, 8) * f2;
                            float ex = F32(Cd, 0x10) * f0, ey = F32(Cd, 0x14) * f1, ez = F32(Cd, 0x18) * f2;
                            float t0 = ex + cx, t1 = ey + cy, t2 = ez + cz;
                            if (!(maxx > t0)) maxx = t0;
                            if (!(maxy > t1)) maxy = t1;
                            if (!(maxz > t2)) maxz = t2;
                            t0 = cx - ex; t1 = cy - ey; t2 = cz - ez;
                            if (!(t0 > minx)) minx = t0;
                            if (!(t1 > miny)) miny = t1;
                            if (!(t2 > minz)) minz = t2;
                            float ncx = (minx + maxx) * 0.5f, ncy = (miny + maxy) * 0.5f, ncz = (minz + maxz) * 0.5f;
                            float nex = maxx - ncx, ney = maxy - ncy, nez = maxz - ncz;
                            /* the two vectors are stored from the frame (their w words: the frame's old contents, +0x7c / +0x4c) */
                            SF32(F, 0x70, ncx); SF32(F, 0x74, ncy); SF32(F, 0x78, ncz);
                            rr_copy(Bd, F + 0x70, 0x10);
                            SF32(F, 0x40, nex); SF32(F, 0x44, ney); SF32(F, 0x48, nez);
                            rr_copy(Bd + 0x10, F + 0x40, 0x10);
                            SF32(Bd, 0x40, sqrtf(nez * nez + ney * ney + nex * nex));
                            SU8(N2, 0xc, U8(N2, 0xc) & 0xfb);
                        }
                        rx_addr rec = F + 0xe0;
                        SU32(rec, 0, U32(E, 0)); SU32(rec, 4, (uint32_t)idx); SU32(rec, 8, U32(E, 8)); SU32(rec, 0xc, U32(E, 0xc));
                        rx_lod_entries_push_back(R + 4, rec);
                    }
                }
                rx_reflex_aligned_free(U32(vec, 0));   /* inline: _aligned_free under the allocator's lock */
            }
            SU32(h, 0x4c, 0);
        }
    }
    rx_reflex_aligned_free(U32(set, 4));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0053d8b0, rx_vehloader_override_materials)

/* the helpers registered at their own addresses (C signatures in build_rider.h; unit 6's vehicle parts assembly 00541310 uses them) */
RX_FUNCTION(0x007cff20, rx_iarr_reserve)
RX_FUNCTION(0x00544e70, rx_iarr_insert_sorted)
RX_FUNCTION(0x00544f70, rx_iarr_indices_in_range)
RX_FUNCTION(0x00416f10, rx_lod_entries_push_back)
RX_FUNCTION(0x007d4ce0, rx_model_push_empty_lod)
RX_FUNCTION(0x00543410, rx_lod_set_init)
RX_FUNCTION(0x005434c0, rx_lod_set_add_package)
RX_FUNCTION(0x005435a0, rx_lod_set_apply)
RX_FUNCTION(0x00543fc0, rx_material_key)
