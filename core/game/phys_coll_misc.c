/* phys_coll_misc.c -- small container / vector helpers of the collision code (see phys_coll_misc.h). */
#include "phys_coll_misc.h"
#include "build_alloc.h"
#include "crt.h"
#include <stdio.h>
#include <stdlib.h>

/* ---- MSVC 9 std::vector<T*>: _Insert_n (0080d280) and its _Reallocate (0080d860) --------------------------------------------
 * Layout of the vector object (as the construction code uses it, build_world.c / race_activity.c): +0x00 the container proxy
 * (checked iterators hold that pointer, and [+0] is the vector address), +0x0c _Myfirst, +0x10 _Mylast, +0x14 _Myend.
 * A checked iterator is {proxy +0, position +4}. The element type is a 4-byte pointer.
 *
 * The three generic element helpers the two functions call (their bodies are small and inlined here; each one is a separate
 * function in the exe):
 *   00776d20 [EDI, EAX) -> EDX, EAX the end: std::_Umove, memmove_s(dst, bytes, src, bytes) (IAT 0x916200).
 *   006c8230 ESI copies of [EDX] at EDI, EAX the end: std::_Uninitialized_fill_n (the same one 006c8230 is registered as).
 *   007cfd80 [arg, ECX) with [EDX], EAX = ECX: std::_Fill (assignment fill).
 * Allocations go through ReflexAlignedAlloc(4 * n) (0042bf30 = _aligned_malloc(4 * n, 16)) / ReflexAlignedFree (0078fc50).
 */
static rx_addr vec_move(rx_addr dst, rx_addr first, rx_addr last) {
    const uint32_t bytes = (uint32_t)((int32_t)(last - first) >> 2) * 4;   /* 00776d20: n = (EAX - EDI) sar 2; bytes = n * 4 */
    if (bytes != 0) crt_memmove_s(dst, bytes, first, bytes);
    return dst + bytes;
}
static rx_addr vec_fill_n(rx_addr dst, uint32_t n, uint32_t value) {
    for (uint32_t i = 0; i < n; i++) SU32(dst, 4 * i, value);                /* 006c8230 ESI copies of [EDX] */
    return dst + 4 * n;
}
static void vec_fill(rx_addr first, rx_addr last, uint32_t value) {
    for (rx_addr p = first; p != last; p += 4) SU32(p, 0, value);           /* 007cfd80 [arg, ECX) */
}

/* 0080d860 std::vector<T*>::_Reallocate (stdcall: vector, value*, proxy, where; RET 0x10): grows to
 * capacity + capacity / 2 (at least size + 1), allocates, moves [first, where) and [where, last) around one uninitialized
 * value, frees the old block, updates the three pointers; with room it moves the tail up one slot through memmove_s and
 * fills the hole (the value copy lives in the second argument slot, as the original's [EBP+0xC]). A full vector
 * (FUN_007754f0 _Xlen) throws std::length_error in the original; not reachable here. The original's SEH record and its
 * stack slots are not reproduced (their bytes are dead after the call). */
void phys_vector_reallocate(rx_addr vec, rx_addr value_p, rx_addr proxy, rx_addr where) {
    (void)proxy;
    const uint32_t first = U32(vec, 0xc);
    const uint32_t capacity = first == 0 ? 0 : (uint32_t)(I32(vec, 0x14) - (int32_t)first) >> 2;
    const uint32_t last = U32(vec, 0x10);
    const uint32_t size = (uint32_t)((int32_t)(last - first) >> 2);
    if (0x3fffffffu - size < 1) {                                           /* 0080d8b4: 0x3fffffff - size < 1 -> FUN_007754f0 (_Xlen) */
        fprintf(stderr, "rxcore: vector too long (std::length_error)\n");
        abort();
    }
    const uint32_t need = size + 1;
    if (capacity < need) {
        const uint32_t half = capacity >> 1;
        uint32_t ncap = 0x3fffffffu - half >= capacity ? capacity + half : 0;
        if (ncap < need) ncap = need;
        if (ncap > 0x3fffffffu) {                                           /* 0042bf30's length_error */
            fprintf(stderr, "rxcore: vector too long (std::length_error)\n");
            abort();
        }
        const rx_addr newbuf = rx_reflex_aligned_alloc(ncap * 4);           /* 0042bf30 -> 0078fbe0 */
        const rx_addr e = vec_move(newbuf, first, where);
        const rx_addr e2 = vec_fill_n(e, 1, U32(value_p, 0));               /* 006c8230, 1 element */
        vec_move(e2, where, last);
        const uint32_t nsize = (uint32_t)((int32_t)(U32(vec, 0x10) - U32(vec, 0xc)) >> 2) + 1;
        const rx_addr old_first = U32(vec, 0xc);
        if (old_first != 0) rx_reflex_aligned_free(old_first);              /* 0078fc50 */
        SU32(vec, 0x14, newbuf + 4 * ncap);
        SU32(vec, 0x10, newbuf + 4 * nsize);
        SU32(vec, 0xc, newbuf);
        return;
    }
    const uint32_t tail = (uint32_t)((int32_t)(last - where) >> 2);
    const uint32_t value = U32(value_p, 0);                                 /* copied to [EBP+0xC] first */
    if (tail >= 1) {
        SU32(vec, 0x10, vec_move(last, last - 4, last));                    /* copy the last element to the new end */
        const uint32_t n = (uint32_t)((int32_t)((last - 4) - where) >> 2);
        if ((int32_t)n > 0) crt_memmove_s(where + 4, n * 4, where, n * 4); /* memmove_s (IAT 0x916200) */
        vec_fill(where, where + 4, value);                                  /* 007cfd80 [where, where + 1) */
    } else {
        vec_move(where + 4, where, last);                                   /* (empty when where == last) */
        const uint32_t old_last = U32(vec, 0x10);
        vec_fill_n(old_last, 1u - (uint32_t)((int32_t)(old_last - where) >> 2), value);
        SU32(vec, 0x10, U32(vec, 0x10) + 4);
        vec_fill(where, U32(vec, 0x10) - 4, value);                         /* [where, new end - 1) */
    }
}
RX_FUNCTION(0x0080d860, phys_vector_reallocate)

/* 00842fa0 (ESI = the iterator {proxy, ptr}; stack n; RET 4): a checked vector iterator advanced by n items (4 bytes each). The
 * checks read the proxy's container (+0x10 end, +0xc begin) as the original does (_invalid_parameter_noinfo through EBX when the
 * new position is past the end or before the begin, or the proxy is 0): the reads keep the original's pages resident (the
 * object registry's proxy lives on the boot's lazily-loaded world-image page 0x1002b000: known-differences 2). */
void phys_iterator_advance(rx_addr it, int32_t n) {
    const rx_addr proxy = U32(it, 0);
    if (proxy == 0) crt_invalid_parameter_noinfo();
    const rx_addr cont = proxy ? U32(proxy, 0) : 0;
    const uint32_t p = U32(it, 4) + (uint32_t)n * 4;
    if (p > U32(cont, 0x10)) crt_invalid_parameter_noinfo();
    if (proxy == 0) crt_invalid_parameter_noinfo();
    if (p < U32(proxy ? U32(proxy, 0) : 0, 0xc)) crt_invalid_parameter_noinfo();
    SU32(it, 4, p);
}
RX_FUNCTION(0x00842fa0, phys_iterator_advance)

/* 0080d280 std::vector<T*>::_Insert_n (EAX = vector; stack: the result iterator buffer, value*, the _Where proxy, the _Where
 * position; RET 0x10): the checked-iterator tests (when the element range is non-empty: _Myfirst <= _Mylast, and the _Where
 * proxy is non-zero and is the vector's own proxy), the element index (where - _Myfirst) / 4, then _Reallocate (0080d860) and
 * the result iterator {proxy, new _Myfirst + index} advanced through 00842fa0. Returns the result buffer. */
rx_addr phys_vector_insert_n(rx_addr vec, rx_addr out_it, rx_addr value_p, rx_addr proxy, rx_addr where) {
    const uint32_t begin = U32(vec, 0xc), end = U32(vec, 0x10);
    uint32_t index = 0;
    if (((end - begin) & 0xfffffffc) != 0) {                                /* test ecx, 0xfffffffc */
        if (begin > end) crt_invalid_parameter_noinfo();
        if (proxy == 0 || proxy != U32(vec, 0)) crt_invalid_parameter_noinfo();
        index = (uint32_t)((int32_t)(where - begin) >> 2);
    }
    phys_vector_reallocate(vec, value_p, proxy, where);
    if (U32(vec, 0xc) > U32(vec, 0x10)) crt_invalid_parameter_noinfo();
    SU32(out_it, 0, U32(vec, 0));
    SU32(out_it, 4, U32(vec, 0xc));
    phys_iterator_advance(out_it, (int32_t)index);                          /* 00842fa0 */
    return out_it;
}
RX_FUNCTION(0x0080d280, phys_vector_insert_n)

/* 008423d0 vector<T*>::push_back: stores the value at the end when there is room, else grows through the exe's insert helper
 * 0080d280 (EAX vector; stack scratch iterator*, value*, allocator proxy, end; RET 0x10). Frame: sub esp, 8; push ebx / esi.
 * The scratch iterator is the original's local at [entry ESP - 8]. */
uint32_t phys_vector_push_back(rx_addr vec, rx_addr value_p) {
    const uint32_t begin = U32(vec, 0xc);
    const uint32_t capacity = begin == 0 ? 0 : (uint32_t)(I32(vec, 0x14) - (int32_t)begin) >> 2;
    const uint32_t end = U32(vec, 0x10);
    if ((uint32_t)((int32_t)(end - begin) >> 2) < capacity) {        /* edx < eax, unsigned */
        const uint32_t value = U32(value_p, 0);
        SU32(end, 0, value);
        SU32(vec, 0x10, end + 4);
        return value;
    }
    const rx_addr entry_sp = rx_sp();
    const rx_addr out_it = entry_sp - 8;
    return phys_vector_insert_n(vec, out_it, value_p, U32(vec, 0), end);
}
RX_FUNCTION(0x008423d0, phys_vector_push_back)

/* 0084fe70: items of 0xd0 bytes at +0xc..+0x10 (count n) and of 12 bytes at +0x24..+0x28 (count m). With a <= b (swapped if not,
 * unsigned) the pair index is ((2n - a - 1) * a) / 2 + b -- the position of (a, b) when the pairs of n items are listed row by
 * row -- clamped to [0, m]: negative gives 0. */
uint32_t phys_pair_index(rx_addr obj, uint32_t a, uint32_t b) {
    if (a > b) { const uint32_t t = a; a = b; b = t; }
    const int32_t n = (I32(obj, 0x10) - I32(obj, 0xc)) / 208;
    const int32_t m = (I32(obj, 0x28) - I32(obj, 0x24)) / 12;
    const uint32_t index = (((uint32_t)(2 * n) - a - 1) * a >> 1) + b;
    if ((int32_t)index < 0) return 0;
    return (int32_t)index > m ? (uint32_t)m : index;
}
RX_FUNCTION(0x0084fe70, phys_pair_index)

/* 0084fe00: with d = n . v (the products summed (x + y) + z, n = plane+0x10): when the plane is set and d is below every lane of the
 * runtime zero vector 0xd61670, v = v - n * d. */
void phys_plane_clip(rx_addr plane, rx_addr vp) {
    if (U32(plane, 0) == 0) return;
    const rx_v4 n = v4_ld(plane + 0x10), v = v4_ld(vp);
    const rx_v4 d = v4_splat(phys_dot3(v, n));
    const rx_v4 below = v4_cmplt(d, v4_ld(0xd61670));
    if (v4_lb(below, 2) + v4_lb(below, 1) + v4_lb(below, 0) != 0xfffffffdu) return;
    v4_st(vp, v4_sub(v, v4_mul(n, d)));
}
RX_FUNCTION(0x0084fe00, phys_plane_clip)

/* 00690e00: the exe's integer hash (Jenkins-style: h += ~(h << 15); h ^= h >> 10 (arithmetic); h *= 9; h ^= h >> 6; h += ~(h << 11);
 * h ^= h >> 16) selects the bucket hash % bucket count (+0); a node is {key +0, value +4, hash +8, next +0xc}, buckets at +0xc. */
uint32_t phys_hash_lookup(rx_addr table, rx_addr key_p, rx_addr out_p) {
    const uint32_t key = U32(key_p, 0);
    uint32_t h = key;
    h += ~(h << 15);
    h ^= (uint32_t)((int32_t)h >> 10);
    h *= 9;
    h ^= (uint32_t)((int32_t)h >> 6);
    h += ~(h << 11);
    h ^= (uint32_t)((int32_t)h >> 16);
    const uint32_t bucket = h % U32(table, 0);
    for (rx_addr node = U32(U32(table, 0xc), bucket * 4); node != 0; node = U32(node, 0xc)) {
        if (U32(node, 8) == h && U32(node, 0) == key) {
            SU32(out_p, 0, U32(node, 4));
            return 1;
        }
    }
    return 0;
}
RX_FUNCTION(0x00690e00, phys_hash_lookup)

/* 007cf450: erase [first, last): the items from last to the end move down to first (memmove_s), the end becomes first + their size. */
rx_addr phys_vector_erase(rx_addr vec, rx_addr out, rx_addr first_pos, rx_addr last_pos) {
    SU32(out, 0, 0);
    SU32(out, 0, U32(vec, 0));
    SU32(out, 4, first_pos);
    if (first_pos != last_pos) {
        const int32_t items = (I32(vec, 0x10) - (int32_t)last_pos) >> 2;
        const uint32_t bytes = (uint32_t)items * 4;
        const rx_addr new_end = bytes + first_pos;
        if (items > 0) {
            if (first_pos <= last_pos) for (uint32_t k = 0; k < bytes; k++) SU8(first_pos, k, U8(last_pos, k));
            else for (uint32_t k = bytes; k > 0; k--) SU8(first_pos, k - 1, U8(last_pos, k - 1));
        }
        SU32(vec, 0x10, new_end);
    }
    return out;
}
RX_FUNCTION(0x007cf450, phys_vector_erase)
