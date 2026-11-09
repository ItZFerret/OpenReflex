/* race_misc_util.c -- small engine container helpers on the race frame's path (std::vector-like arrays of records, Quazal
 * handles). */
#include "race.h"
#include "race_extern.h"
#include "race_misc.h"
#include "race_net_session.h"
#include "race_extern.h"
#include "race_activity_misc.h"
#include "crt.h"
#include "build_vehicle.h"
#include "build_asset.h"
#include "build_alloc.h"
#include "build_activity_util.h"
#include "fx.h"
#include "phys_mech.h"
#include "phys_crt.h"

/* 007fa6a0 (EAX = record) -> EAX = record + 0x14. */
rx_addr rx_field_14(rx_addr p) { return p + 0x14; }
RX_FUNCTION(0x007fa6a0, rx_field_14)
/* 007fa6c0 (EAX = record) -> EAX = record +4. */
uint32_t rx_field_get4(rx_addr p) { return U32(p, 4); }
RX_FUNCTION(0x007fa6c0, rx_field_get4)

/* 007f9e40 (ECX = array {data, count}, arg = item; RET 4) -> EAX: the index of the 0x20-byte record whose +0x10 is item's +0x10,
 * or -1. */
int32_t rx_array20_find(rx_addr arr, rx_addr item) {
    const int32_t n = I32(arr, 4);
    for (int32_t i = 0; i < n; i++)
        if (U32(U32(arr, 0) + 0x20 * (uint32_t)i, 0x10) == U32(item, 0x10)) return i;
    return -1;
}
RX_FUNCTION(0x007f9e40, rx_array20_find)

/* 007f9ee0 (ECX = index, EDX = array {data, count}): remove a 0x20-byte record by moving the last one into its place. */
void rx_array20_remove(uint32_t index, rx_addr arr) {
    rx_addr data = U32(arr, 0);
    rx_addr last = data + 0x20 * U32(arr, 4) - 0x20, dst = data + 0x20 * index;
    SF32(dst, 0, F32(last, 0)); SF32(dst, 4, F32(last, 4)); SF32(dst, 8, F32(last, 8));
    SU32(dst, 0x10, U32(last, 0x10));
    SF32(dst, 0x14, F32(last, 0x14)); SF32(dst, 0x18, F32(last, 0x18));
    SU32(arr, 4, U32(arr, 4) - 1);
}
RX_FUNCTION(0x007f9ee0, rx_array20_remove)

/* 007f9f80 (EAX = array {data, count}, arg = item; RET 4) -> EAX: the index of the 0x40-byte record matching the item (by +0x10,
 * or by +0x30 when the record's +0x10 is 0), or -1. */
int32_t rx_array40_find(rx_addr arr, rx_addr item) {
    const int32_t n = I32(arr, 4);
    for (int32_t i = 0; i < n; i++) {
        rx_addr e = U32(arr, 0) + 0x40 * (uint32_t)i;
        if (U32(e, 0x10) != 0 ? U32(e, 0x10) == U32(item, 0x10) : U32(e, 0x30) == U32(item, 0x30)) return i;
    }
    return -1;
}
RX_FUNCTION(0x007f9f80, rx_array40_find)

/* 007f9140 (ESI = vector {+0x00 owner, +0x0c first, +0x10 last}, EDI = out iterator, args: (unused), position; RET 8) -> EAX = out:
 * std::vector<dword>::erase(position): the tail moved down (memmove_s), the iterator {owner, position} (checked iterators:
 * _invalid_parameter_noinfo when out of range). */
rx_addr rx_vector_erase(rx_addr vec, rx_addr out, rx_addr pos) {
    const int32_t n = (int32_t)(U32(vec, 0x10) - (pos + 4)) >> 2;
    if (n > 0) crt_memmove_s(pos, (uint32_t)n * 4, pos + 4, (uint32_t)n * 4);
    SU32(vec, 0x10, U32(vec, 0x10) - 4);
    SU32(out, 0, 0);
    if (U32(vec, 0xc) > pos || pos > U32(vec, 0x10)) crt_invalid_parameter_noinfo();
    SU32(out, 0, U32(vec, 0));
    SU32(out, 4, pos);
    return out;
}
RX_FUNCTION(0x007f9140, rx_vector_erase)

/* 008953c0 (ECX unused) -> AL: the state of the session's Quazal object (handle 0x9df938): 0 none, 1 ours / valid, 2 not
 * (a reference taken and released around the checks: 0088bcd0 / 00885050 / 00884950 / 0088bac0 - race_net_session.c, the Quazal
 * boundary). Frame: sub 0xc; push ecx (the handle H at +4; the pushed slot holds the object argument of 0088bcd0, which pops it). */
uint32_t rx_net_session_state(void) {
    RACE_FRAME_PLAIN(F, 0xc, 1);
    SU32(F, 0, U32(0, 0x9df938));
    const rx_addr h = F + 4;
    RACE_CORE_CALL(0, (void)rx_quazal_handle_init(h, U32(F, 0)));
    rx_set_sp(F + 4);
    uint32_t ready, state;
    RACE_CORE_CALL(0, ready = rx_quazal_ref_ready(h));
    if ((ready & 0xff) == 0) {
        RACE_CORE_CALL(0, rx_quazal_handle_release(h));
        state = 0;
    } else {
        RACE_CORE_CALL(0, ready = rx_quazal_ref_ready(h));
        uint32_t valid;
        RACE_CORE_CALL(0, valid = rx_net_ref_valid((ready & 0xff) != 0 ? U32(h, 0) : 0));   /* 00884950 */
        RACE_CORE_CALL(0, rx_quazal_handle_release(h));
        state = (valid & 0xff) != 0 ? 1 : 2;
    }
    RACE_FRAME_END(F);
    return state;
}
RX_FUNCTION(0x008953c0, rx_net_session_state)

/* 00460020 (ESI = vehicle, args a, b; RET 8): its sound / effect component's (+0x234) vf20 query (a, b) -> three vectors, handed
 * to FUN_0045f600 (vehicle, ...) (not in the core). */
void rx_vehicle_effect_query(rx_addr v, uint32_t a, uint32_t b) {
    RACE_FRAME16_ENTER(F, 0x30);
    rx_addr c = U32(v, 0x234);
    uint32_t args[5] = {a, b, F + 0x10, F, F + 0x20};
    void (*vf20)(rx_addr, rx_addr, rx_addr, rx_addr, rx_addr, rx_addr) = RX_FN(race_vfn(c, 20), void (*)(rx_addr, rx_addr, rx_addr, rx_addr, rx_addr, rx_addr));
    if (vf20) RACE_CORE_CALL(5, vf20(c, a, b, F + 0x10, F, F + 0x20));   /* 0043d1f0 (bike_setup.c) */
    else rx_call_original(race_vfn(c, 20), RX_THISCALL, c, 5, args);
    RACE_CORE_CALL(3, rx_vehicle_place(v, F + 0x10, F, F + 0x20));
    RACE_FRAME16_LEAVE(F);
}
RX_FUNCTION(0x00460020, rx_vehicle_effect_query)

/* ---- containers --------------------------------------------------------------------------------------------------------------- */

/* 007fa250 (EAX = a name hash map {+0 bucket count, +0xc buckets; node: +0 key, +4 value, +8 hash, +0xc next}, EDI = the key
 * string pointer's address, arg: value out; RET 4) -> EAX: found (String_HashNoCase EAX = the key; bucket = hash % count; the
 * node with that hash and _stricmp-equal key): *out = its value. */
uint32_t rx_name_hash_find(rx_addr map, rx_addr keyp, rx_addr out) {
    RACE_FRAME_PLAIN(F, 0, 3);
    uint32_t h;
    RACE_CORE_CALL(0, h = rx_string_hash_nocase(U32(keyp, 0)));
    uint32_t r = 0;
    for (rx_addr n = U32(U32(map, 0xc), 4 * (h % U32(map, 0))); n != 0; n = U32(n, 0xc)) {
        if (U32(n, 8) == h && crt_stricmp(U32(n, 0), U32(keyp, 0)) == 0) { SU32(out, 0, U32(n, 4)); r = 1; break; }
    }
    RACE_FRAME_END(F);
    return r;
}
RX_FUNCTION(0x007fa250, rx_name_hash_find)

/* 007fa1e0 (EAX = a name hash map; +0x18 its allocator, +0x1c owned): frees every node and the bucket array (the allocator's
 * vf2), then an owned allocator is deleted (vf0 (1)). */
void rx_name_hash_free(rx_addr map) {
    RACE_FRAME_PLAIN(F, 0, 3);
    for (int32_t i = 0; i < I32(map, 0); i++) {
        rx_addr n = U32(U32(map, 0xc), 4 * i);
        while (n != 0) {
            const rx_addr next = U32(n, 0xc);
            const rx_addr al = U32(map, 0x18);
            race_thiscall(race_vfn(al, 2), al, 1, n, 0, 0, 0);
            n = next;
        }
    }
    const rx_addr al = U32(map, 0x18);
    race_thiscall(race_vfn(al, 2), al, 1, U32(map, 0xc), 0, 0, 0);
    if (U8(map, 0x1c) != 0 && U32(map, 0x18) != 0) {
        const rx_addr a2 = U32(map, 0x18);
        race_thiscall(race_vfn(a2, 0), a2, 1, 1, 0, 0, 0);
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x007fa1e0, rx_name_hash_free)

/* 007fa2b0 (EAX = end, EDI = begin, EDX = destination; RET 0) -> EAX = destination + the copied bytes: memmove_s of the range when
 * it is not empty. Frame: push ecx. */
rx_addr rx_vector_move_range(rx_addr end, rx_addr begin, rx_addr dst) {
    const rx_addr entry0 = rx_sp();
    rx_set_sp(entry0 - 4u);
    const int32_t n = (int32_t)(end - begin) >> 2;
    rx_set_sp(entry0);
    if (n > 0) crt_memmove_s(dst, 4u * (uint32_t)n, begin, 4u * (uint32_t)n);
    return dst + 4u * (uint32_t)n;
}
RX_FUNCTION(0x007fa2b0, rx_vector_move_range)
/* 007f9570 (EDI = the count, ESI = the vector {+0xc first, +0x10 last, +0x14 end}) -> 1: the element storage allocated (0042bf30,
 * count * 4) and the three pointers set; a count above 0x3fffffff is MSVC's "vector<T> too long" (00851560: a throw, not
 * ported; it cannot occur for a game vector). */
uint32_t rx_vector_storage_alloc(rx_addr self, uint32_t n) {
    const rx_addr entry0 = rx_sp();
    if (n > 0x3fffffffu) act_unreached(0x00851560, "the MSVC vector<T> too long throw");
    rx_set_sp(entry0 - 4u);                                   /* push ecx (the callee's frame: 0042bf30 takes the count in ECX) */
    const rx_addr p = rx_fx_vector_alloc(n);   /* 0042bf30 */
    rx_set_sp(entry0);
    SU32(self, 0xc, p);
    SU32(self, 0x10, p);
    SU32(self, 0x14, p + n * 4u);
    return 1;
}
RX_FUNCTION(0x007f9570, rx_vector_storage_alloc)
/* 007f8fa0 std::vector<T*>::operator= (EAX = this {+0xc first, +0x10 last, +0x14 end}, arg: other; RET 4) -> EAX = this; ECX is
 * dead at return (the only caller, SceneManager_Tick 007d23f4, reloads it at 007d2426 before any read; after the copy paths it is
 * the CRT's residue from memmove_s in 00776d20, which this model does not reproduce): empty: clear (FUN_004fb0e0 EAX = this); fits in the size: memmove_s, ECX = first;
 * fits in the capacity: assign (FUN_007fa2b0: EAX = mid, EDI = from, EDX = to) + construct the rest (FUN_00776d20); else the
 * storage is freed (ReflexAlignedFree), reallocated (FUN_007f9570 EDI = count, ESI = this) and filled. */
rx_addr rx_vector_assign(rx_addr self, rx_addr other, uint32_t ecx_in, uint32_t* ecx_out) {
    RACE_FRAME_PLAIN(F, 0, 4);
    uint32_t ecx = ecx_in;
    if (self != other) {
        rx_regs in, o;
        const rx_addr of = U32(other, 0xc), ol = U32(other, 0x10);
        const uint32_t n = (uint32_t)((int32_t)(ol - of) >> 2);
        ecx = ol;
        if (n == 0) {
            RACE_CORE_CALL(0, ecx = rx_vector_clear(self));   /* 004fb0e0 (race_activity_misc.c) */
        } else {
            const rx_addr sf = U32(self, 0xc);
            const uint32_t size = (uint32_t)((int32_t)(U32(self, 0x10) - sf) >> 2);
            if (n <= size) {
                const int32_t cnt = (int32_t)(ol - of) >> 2;
                if (cnt > 0) crt_memmove_s(sf, 4u * (uint32_t)cnt, of, 4u * (uint32_t)cnt);
                ecx = U32(self, 0xc);
                SU32(self, 0x10, ecx + 4 * (uint32_t)((int32_t)(U32(other, 0x10) - U32(other, 0xc)) >> 2));
            } else {
                const uint32_t cap = sf != 0 ? (uint32_t)((int32_t)(U32(self, 0x14) - sf) >> 2) : 0;
                if (n <= cap) {
                    const rx_addr mid = of + 4 * size;
                    RACE_CORE_CALL(0, (void)rx_vector_move_range(mid, of, sf));   /* 007fa2b0 */
                    rx_addr e = 0;
                    RACE_CORE_CALL(0, e = rx_fx_vector_umove(U32(other, 0x10), mid, U32(self, 0x10)));   /* 00776d20 (ECX: the CRT residue, ignored here) */
                    SU32(self, 0x10, e);
                    ecx = e;
                } else {
                    if (sf != 0) RACE_CORE_CALL(1, rx_reflex_aligned_free(sf));   /* ReflexAlignedFree */
                    const uint32_t n2 = (uint32_t)((int32_t)(U32(other, 0x10) - U32(other, 0xc)) >> 2);
                    SU32(self, 0xc, 0); SU32(self, 0x10, 0); SU32(self, 0x14, 0);
                    if (n2 != 0) {
                        memset(&in, 0, sizeof in); in.r[7] = n2; in.r[6] = self;
                        const uint32_t ok = rx_vector_storage_alloc(self, n2);
                        o.r[0] = ok; o.r[1] = ecx;
                        if ((ok & 0xff) != 0) {
                            rx_addr e = 0;
                            RACE_CORE_CALL(0, e = rx_fx_vector_umove(U32(other, 0x10), U32(other, 0xc), U32(self, 0xc)));   /* 00776d20 */
                            SU32(self, 0x10, e);
                            ecx = e;
                        }
                    }
                }
            }
        }
    }
    if (ecx_out) *ecx_out = ecx;
    RACE_FRAME_END(F);
    return self;
}
RX_FUNCTION(0x007f8fa0, rx_vector_assign)

/* The cell index of a coordinate as the original computes it: u = (float)(((c - origin) + size / 2) * count / size) (x87),
 * i = ftol(copysign(0.5, u) + u) (rounding half away from zero), then through a float (cvtsi2ss / cvttss2si). */
static int32_t scene_cell_index(float c, float origin, float size, uint32_t count) {
    rx_f80 s = rx_f80_sub(rx_f80_from_f32(c), rx_f80_from_f32(origin), 53, 0);
    s = rx_f80_add(s, rx_f80_mul(rx_f80_from_f64(0.5 /* 0x9322cc */), rx_f80_from_f32(size), 53, 0), 53, 0);
    rx_f80 n = rx_f80_from_int((int32_t)count);
    if ((int32_t)count < 0) n = rx_f80_add(n, rx_f80_from_f32(4294967296.0f /* 0x97731c */), 53, 0);
    s = rx_f80_mul(s, n, 53, 0);
    const float u = rx_f80_to_f32(rx_f80_div(s, rx_f80_from_f32(size), 53, 0), 0);
    const double half = crt_copysign(0.5 /* 0x9774e8 */, (double)u);
    const int32_t i = phys_ftol2_sse(rx_f80_add(rx_f80_from_f64(half), rx_f80_from_f32(u), 53, 0), 53);
    const float fi = (float)i;
    return fi != fi || fi >= 2147483648.0f || fi < -2147483648.0f ? (int32_t)0x80000000 : (int32_t)fi;
}
/* A grid cell's height ((hi * 256 + lo) / 65535 * scale + offset). */
static float scene_cell_h(rx_addr cell, float scale, float offset) {
    const float h = rx_adds((float)((uint32_t)U8(cell, 2) << 8), (float)U8(cell, 3));
    return rx_adds(rx_muls(rx_divs(h, 65535.0f /* 0x97759c */), scale), offset);
}

/* 007ea020 (stdcall patch, point in / out, normal out, grid, scale, offset; RET 0x18) -> EAX: the patch object (b) under the
 * point. Outside the patch's x / z box -> 0. The vertical segment through the point (y 1e6 down to -1e6) meets the patch plane
 * (Segment_PlaneIntersect into the point); the normal starts as the plane's (+0x80). With a 16-bit grid (format 0x15) the point
 * moves along the normal by the cell's height ((hi * 256 + lo) / 65535 * scale + offset); an invalid cell -> 0. The normal is
 * then the cell's surface normal: the z / x height differences (-dz, size x / count x, dx) in the patch's (col0, plane normal,
 * col2) basis, normalized (SqrtF; not when the squared length is <= 0 or NaN). */
uint32_t rx_scene_patch_height(rx_addr b, rx_addr p, rx_addr normal_out, rx_addr g, float scale, float offset) {
    const float px = F32(p, 0);
    if (F32(b, 0xb0) > px || px > F32(b, 0xb8)) return 0;
    const float pz = F32(p, 8);
    if (F32(b, 0xb4) > pz || pz > F32(b, 0xbc)) return 0;
    const float nx = F32(b, 0x80), ny = F32(b, 0x84), nz = F32(b, 0x88);
    SF32(normal_out, 0, nx); SF32(normal_out, 4, ny); SF32(normal_out, 8, nz);
    RACE_ORIG_FRAME_ENTER(F, 0x34, 3);
    SF32(F, 0x30, px); SF32(F, 0x34, 1000000.0f /* 0x977658 */); SF32(F, 0x38, pz);
    SF32(F, 0x20, px); SF32(F, 0x24, -1000000.0f /* 0x9777d8 */); SF32(F, 0x28, pz);
    uint32_t hit;
    RACE_CORE_CALL(2, hit = phys_segment_plane_intersect(F + 0x30, F + 0x20, b + 0xa0, p, b + 0x80));
    uint32_t r = hit;
    if (hit != 0 && g != 0) {
        const uint32_t w = U32(g, 4), hgt = U32(g, 8);
        int32_t ix = scene_cell_index(px, F32(b, 0xa0), F32(b, 0x14), w);
        int32_t iz = scene_cell_index(pz, F32(b, 0xa8), F32(b, 0x18), hgt);
        const int32_t mx = (int32_t)w - 1, mz = (int32_t)hgt - 1;
        if (ix < 0) ix = 0; else if (ix > mx) ix = mx;
        if (iz < 0) iz = 0; else if (iz > mz) iz = mz;
        r = 1;
        if (U32(g, 0x10) == 0x15) {
            const rx_addr cell = U32(g, 0xc) * (uint32_t)iz + 4u * (uint32_t)ix + U32(g, 0);
            if (U8(cell, 1) == 0) {
                r = 0;
            } else {
                const float h = scene_cell_h(cell, scale, offset);
                SF32(p, 0, rx_adds(F32(p, 0), rx_muls(nx, h)));
                SF32(p, 4, rx_adds(F32(p, 4), rx_muls(ny, h)));
                SF32(p, 8, rx_adds(F32(p, 8), rx_muls(nz, h)));
                /* slope x: cell size x / count x (x87, main thread 53-bit) */
                rx_f80 wf = rx_f80_from_int((int32_t)w);
                if ((int32_t)w < 0) wf = rx_f80_add(wf, rx_f80_from_f32(4294967296.0f /* 0x97731c */), 53, 0);
                const float sl = rx_f80_to_f32(rx_f80_div(rx_f80_from_f32(F32(b, 0x14)), wf, 53, 0), 0);
                const uint32_t stride = U32(g, 0xc);
                rx_addr zc0 = cell, zc1 = cell, xc0 = cell, xc1 = cell;
                if (iz != 0) zc0 -= stride;
                if (iz != mz) zc1 += stride;
                if (ix != 0) xc0 -= 4;
                if (ix != mx) xc1 += 4;
                const float dz = rx_subs(scene_cell_h(zc1, scale, offset), scene_cell_h(zc0, scale, offset));
                const float dx = rx_subs(scene_cell_h(xc1, scale, offset), scene_cell_h(xc0, scale, offset));
                float a = rx_subs(-0.0f /* 0x9774a8 */, dz);
                float c = sl, d = dx;
                const float sum = rx_adds(rx_adds(rx_muls(a, a), rx_muls(c, c)), rx_muls(d, d));
                if (sum > 0.0f) {
                    float s;
                    RACE_CORE_CALL(1, s = phys_sqrtf(sum));
                    const float inv = rx_divs(1.0f /* 0x931f24 */, s);
                    a = rx_muls(a, inv); c = rx_muls(c, inv); d = rx_muls(d, inv);
                }
                SF32(normal_out, 0, rx_adds(rx_adds(rx_muls(F32(b, 0x70), a), rx_muls(F32(b, 0x80), c)), rx_muls(F32(b, 0x90), d)));
                SF32(normal_out, 4, rx_adds(rx_adds(rx_muls(F32(b, 0x74), a), rx_muls(F32(b, 0x84), c)), rx_muls(F32(b, 0x94), d)));
                SF32(normal_out, 8, rx_adds(rx_adds(rx_muls(F32(b, 0x78), a), rx_muls(F32(b, 0x88), c)), rx_muls(F32(b, 0x98), d)));
            }
        }
    }
    RACE_ORIG_FRAME_LEAVE(F);
    return r;
}
RX_FUNCTION(0x007ea020, rx_scene_patch_height)

/* 007f0050 (ESI = the scene's collidable objects {+0x10 first, +0x14 last}, args: point (in: the probe, out: the highest hit),
 * normal out; RET 8) -> EAX: any hit. Each object of the current view set (+0x47c; engine 0xa95844 +0x1c + 4 * +4) is probed
 * (FUN_007ea020 (+0x54, probe, normal, +0x3c, +0x490, +0x494): the callee replaces the probe by its hit and the normal); the
 * highest hit (y) is kept; the normal is the last one written (default the .data vector 0x9cb4b0). Frame: and esp, -16; sub
 * 0x28; 2 pushes (the probe at +0x10, the normal at +0x20, found at +8). */
uint32_t rx_scene_ground_probe(rx_addr list, rx_addr point, rx_addr normal_out) {
    RACE_FRAME_EBP(F, 16, 0x28, 2);
    uint32_t found = 0;
    if (((U32(list, 0x14) - U32(list, 0x10)) & 0xfffffffcu) != 0) {
        for (uint32_t i = 0; i < 12; i += 4) { SU32(F + 0x10, i, U32(point, i)); SU32(F + 0x20, i, U32(0, 0x9cb4b0 + i)); }
        for (uint32_t i = 0; i < 12; i += 4) SF32(point, i, -3.4028234663852886e+38f /* 0x977550 */);
        SU32(F, 8, 0);
        for (uint32_t i = 0; i < (uint32_t)((int32_t)(U32(list, 0x14) - U32(list, 0x10)) >> 2); i++) {
            const rx_addr obj = U32(U32(list, 0x10), 4 * i);
            if (obj == 0) continue;
            const rx_addr e = U32(0, 0xa95844);
            if (U32(e, 0x1c + 4 * U32(e, 4)) != U32(obj, 0x47c)) continue;
            uint32_t hit;
            RACE_CORE_CALL(6, hit = rx_scene_patch_height(U32(obj, 0x54), F + 0x10, F + 0x20, obj + 0x3c,
                                                          rx_bf(U32(obj, 0x490)), rx_bf(U32(obj, 0x494))));
            if (hit == 0) continue;
            const float y = F32(F, 0x14);
            if (y > F32(point, 4)) {
                SF32(point, 4, y);
                SU32(point, 0, U32(F, 0x10));
                SU32(point, 8, U32(F, 0x18));
                SU32(F, 8, 1);
                found = 1;
            }
        }
        for (uint32_t i = 0; i < 12; i += 4) SU32(normal_out, i, U32(F + 0x20, i));
    }
    RACE_FRAME_END(F);
    return found;
}
RX_FUNCTION(0x007f0050, rx_scene_ground_probe)
