/* anim_scene_vec.c -- the scene's sorted entity vectors' insert (vector<Entity*>::insert(pos, value): 007f90a0, with its _Insert_n-like helper 007f95a0 for the
 * growth): the same template instantiation as the collision code's vector<T*> (phys_coll_misc.c 0080d280 / 0080d860: identical algorithm, the checked iterator
 * tests, the capacity + capacity / 2 growth, memmove_s of the prefix and the suffix, the old block freed), so they are those functions at these addresses. */
#include "phys_coll_misc.h"

/* 007f90a0 (EAX = vector; stack result iterator*, value*, _Where proxy, _Where position; RET 0x10) -> the result buffer */
rx_addr rx_scene_vector_insert(rx_addr vec, rx_addr out_it, rx_addr value_p, rx_addr proxy, rx_addr where) {
    return phys_vector_insert_n(vec, out_it, value_p, proxy, where);
}
RX_FUNCTION(0x007f90a0, rx_scene_vector_insert)

/* 007f95a0 (stack vector, value*, proxy, position; RET 0x10): the grow / in-place insert */
void rx_scene_vector_reallocate(rx_addr vec, rx_addr value_p, rx_addr proxy, rx_addr where) {
    phys_vector_reallocate(vec, value_p, proxy, where);
}
RX_FUNCTION(0x007f95a0, rx_scene_vector_reallocate)

/* ---- the same insert with the original's frames (SceneManager_Link 007d3580's sorted lists) -------------------------------------
 * Lights_Add 007dfdd0, called next from SceneManager_AddEntity at the same depth as Link, copies never-written locals of its frame
 * (the light position's w lane) into the scene's light event records, so the dead stack Link's inserts leave there is the
 * original's: every push, argument, return address and local of 007f90a0 / 007f95a0 / 00776d20 / 006c8230 / 007cfd80 / 00842fa0 /
 * 0042bf30, ReflexAlignedAlloc / Free with their frames (build_alloc.c), the SEH record of 007f95a0, and memmove_s (the original in
 * the oracle with the original's registers; the core's CRT in the product). e = the call's return slot, r = EBX / EBP / ESI / EDI
 * at the call. */
#include "build_alloc.h"
#include "crt.h"
#include "anim_scene_vec.h"
#include <stdio.h>
#include <stdlib.h>
#define VEC_TEB rx_teb()   /* fs:[0] (the world runtime's TEB) */

/* `push count, src, size, dst; call [memmove_s]` with the return slot e */
static void vec_memmove_s(rx_addr e, crt_regs4 r, rx_addr d, uint32_t size, rx_addr s, uint32_t n, rx_addr ret) {
    SU32(e, 4, d); SU32(e, 8, size); SU32(e, 0xc, s); SU32(e, 0x10, n); SU32(e, 0, ret);
    const rx_addr sp = rx_sp();
    rx_set_sp(e + 0x14);
#ifdef RX_ORACLE
    const uint32_t a[4] = {d, size, s, n};
    rx_call_original_regs(U32(0, 0x00916200), r.ebx, r.ebp, r.esi, r.edi, 4, a);
#else
    (void)r; crt_memmove_s(d, size, s, n);
#endif
    rx_set_sp(sp);
    SU32(e, 0, ret);
}
/* 00776d20 (EAX = end, EDI = first, EDX = dest; ESI saved) at the return slot e: [first, end) moved to dest -> dest + n */
static rx_addr vec_move_at(rx_addr e, crt_regs4 r, rx_addr dest, rx_addr first, rx_addr end, rx_addr ret) {
    SU32(e, 0, ret);
    const uint32_t n4 = ((uint32_t)((int32_t)(end - first) >> 2)) * 4u;
    SU32(e, -4, r.esi);
    const crt_regs4 m = {r.ebx, r.ebp, n4 + dest, first};
    if (n4 != 0) vec_memmove_s(e - 0x18, m, dest, n4, first, n4, 0x00776d3e);
    return dest + n4;
}

/* 007f95a0 at the return slot E (stack vector, value*, proxy, where; RET 0x10); r: EBX = first, EBP = the result iterator,
 * ESI = the vector, EDI = the index (007f90a0's at the call) */
static void vec_reallocate_at(rx_addr E, crt_regs4 r, rx_addr vec, rx_addr value_p, rx_addr where) {
    const rx_addr ebp = E - 4;
    SU32(E, -4, r.ebp); SU32(E, -8, 0xffffffffu); SU32(E, -0xc, 0x00902150); SU32(E, -0x10, U32(VEC_TEB, 0));
    SU32(E, -0x24, r.ebx); SU32(E, -0x28, r.esi); SU32(E, -0x2c, r.edi);
    SU32(E, -0x30, U32(0, 0x009b45e4) ^ ebp);
    SU32(VEC_TEB, 0, ebp - 0xc);
    SU32(ebp, -0x10, E - 0x30);
    const rx_addr first = U32(vec, 0xc), last = U32(vec, 0x10);
    const uint32_t cap = first == 0 ? 0 : (uint32_t)(I32(vec, 0x14) - (int32_t)first) >> 2;
    const uint32_t size = (uint32_t)((int32_t)(last - first) >> 2);
    if (0x3fffffffu - size < 1) { fprintf(stderr, "rxcore: 007f95a0: vector too long (std::length_error)\n"); abort(); }
    const uint32_t need = size + 1;
    if (cap < need) {
        const uint32_t half = cap >> 1, esi_g = 0x3fffffffu - half;
        uint32_t ncap = esi_g < cap ? 0 : cap + half;
        if (ncap < need) ncap = need;
        SU32(E, 4, ncap);                                         /* [ebp + 8] */
        if (ncap > 0x3fffffffu) { fprintf(stderr, "rxcore: 007f95a0: vector too long (std::length_error)\n"); abort(); }
        /* call 0042bf30 (ECX = the count; return 007f9632): sub esp, 0x10; push size; call ReflexAlignedAlloc (return 0042bf46) */
        const rx_addr E4 = E - 0x34;
        SU32(E4, 0, 0x007f9632);
        SU32(E4, -0x14, ncap * 4u); SU32(E4, -0x18, 0x0042bf46);
        const crt_regs4 ra = {vec, ebp, esi_g, 0};
        const rx_addr nb = rx_reflex_aligned_alloc_at(E4 - 0x18, ra, ncap * 4u);
        SU32(ebp, -0x14, nb); SU32(ebp, -4, 0);
        const crt_regs4 m1 = {vec, ebp, esi_g, first};
        const rx_addr d1 = vec_move_at(E - 0x34, m1, nb, first, where, 0x007f9645);
        /* 006c8230 (ESI = 1, EDI = dest, EDX = value*; return 007f9654): push ebx */
        SU32(E, -0x34, 0x007f9654); SU32(E, -0x38, vec);
        SU32(d1, 0, U32(value_p, 0));
        const crt_regs4 m2 = {vec, ebp, 1, where};
        (void)vec_move_at(E - 0x34, m2, d1 + 4, where, U32(vec, 0x10), 0x007f9661);
        SU32(ebp, -4, 0xffffffffu);
        const rx_addr old = U32(vec, 0xc);
        const uint32_t nsize = (uint32_t)((int32_t)(U32(vec, 0x10) - old) >> 2) + 1;
        if (old != 0) {
            SU32(E, -0x34, old); SU32(E, -0x38, 0x007f967e);
            const crt_regs4 rf = {vec, ebp, nsize, where};
            rx_reflex_aligned_free_at(E - 0x38, rf, old);
        }
        SU32(vec, 0x14, nb + 4u * U32(E, 4));
        SU32(vec, 0x10, nb + 4u * nsize);
        SU32(vec, 0xc, nb);
    } else {
        const uint32_t value = U32(value_p, 0);
        SU32(E, 8, value);                                        /* [ebp + 0xc]: the value copy */
        if ((uint32_t)((int32_t)(last - where) >> 2) >= 1) {
            const crt_regs4 m1 = {vec, ebp, last, last - 4};
            SU32(vec, 0x10, vec_move_at(E - 0x34, m1, last, last - 4, last, 0x007f9736));
            const int32_t k = (int32_t)((last - 4) - where) >> 2;
            uint32_t esi = last;
            if (k > 0) {
                esi = last - 4u * (uint32_t)k;
                const crt_regs4 m2 = {vec, ebp, esi, (uint32_t)k};
                vec_memmove_s(E - 0x44, m2, esi, 4u * (uint32_t)k, where, 4u * (uint32_t)k, 0x007f9758);
            }
            /* 007cfd80 (stack first; ECX = end, EDX = &value; return 007f976a): push esi */
            SU32(E, -0x34, where); SU32(E, -0x38, 0x007f976a);
            SU32(E, -0x3c, esi);
            SU32(where, 0, U32(E, 8));
        } else {
            const crt_regs4 m1 = {vec, ebp, last, where};
            (void)vec_move_at(E - 0x34, m1, where + 4, where, last, 0x007f96e0);
            SU32(ebp, -4, 2);
            rx_addr c = U32(vec, 0x10);
            uint32_t n = 1u - (uint32_t)((int32_t)(c - where) >> 2);
            SU32(ebp, -0x18, n); SU32(ebp, -0x1c, c);
            while (n > 0) { SU32(c, 0, U32(E, 8)); n--; c += 4; SU32(ebp, -0x18, n); SU32(ebp, -0x1c, c); }
            SU32(ebp, -4, 0xffffffffu);
            SU32(vec, 0x10, U32(vec, 0x10) + 4);
            /* 007cfd80 over [where, new end - 4) (empty here: where is the old end) */
            SU32(E, -0x34, where); SU32(E, -0x38, 0x007f976a);
        }
    }
    SU32(VEC_TEB, 0, U32(ebp, -0xc));
}

rx_addr rx_scene_vector_insert_at(rx_addr e, crt_regs4 r, rx_addr vec, rx_addr out, rx_addr value_p, rx_addr proxy, rx_addr where) {
    SU32(e, 4, out); SU32(e, 8, value_p); SU32(e, 0xc, proxy); SU32(e, 0x10, where);
    SU32(e, -0x10, r.ebx); SU32(e, -0x14, r.ebp); SU32(e, -0x18, r.esi); SU32(e, -0x1c, r.edi);
    const rx_addr first = U32(vec, 0xc), last = U32(vec, 0x10);
    uint32_t index = 0;
    if (((last - first) & 0xfffffffcu) != 0) {
        if (first > last) crt_invalid_parameter_noinfo();
        if (proxy == 0 || proxy != U32(vec, 0)) crt_invalid_parameter_noinfo();
        index = (uint32_t)((int32_t)(where - first) >> 2);
    }
    SU32(e, -0x20, where); SU32(e, -0x24, proxy); SU32(e, -0x28, value_p); SU32(e, -0x2c, vec); SU32(e, -0x30, 0x007f90ff);
    const crt_regs4 rr = {first, out, vec, index};
    vec_reallocate_at(e - 0x30, rr, vec, value_p, where);
    const rx_addr nfirst = U32(vec, 0xc);
    if (nfirst > U32(vec, 0x10)) crt_invalid_parameter_noinfo();
    SU32(e, -0xc, U32(vec, 0));
    SU32(e, -0x20, index); SU32(e, -8, nfirst); SU32(e, -0x24, 0x007f9121);
    SU32(e, -0x28, nfirst); SU32(e, -0x2c, index);                   /* 00842fa0: push ebx; push edi */
    phys_iterator_advance(e - 0xc, (int32_t)index);
    SU32(out, 4, U32(e, -8)); SU32(out, 0, U32(e, -0xc));
    return out;
}
