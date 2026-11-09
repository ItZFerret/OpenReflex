/* phys_static_objects.c -- the removal of static objects from the physics world (2026-10-26, agent w-teardown).
 *
 * The world keeps its static collision objects (the collision geometry of the scene: CollisionEntity, the static assets) in a checked
 * vector of 0x40-byte records at world + 0x19b4:
 *   +0 position, +0x10 orientation (16 bytes each), +0x20 a word, +0x24 the shape handle (vtable 0x9765b4), +0x28 its shape,
 *   +0x2c the TRACKING record {+0 reference count, +4 the world, +8 the record's index}, +0x30 the owner entity, +0x34, +0x38 a byte.
 * The vector header: +0xc begin, +0x10 end, +0x18 the broadphase grid object (its +0x30 the grid), +0x1c the world tag.
 * A StaticObjectHandle (vtable 0x96ddc4, 8 bytes: +4 = the tracking record) holds a reference to the tracking record; the last one
 * released removes the record from the vector (00773a60: the last record moved into its slot, its grid box removed, the last record
 * destroyed = its shape released) and frees the tracking record. The same layout, scaled, as the dynamic records of
 * world_veh_transfer.c (rx_dyn_records_remove). */
#include "phys_static_objects.h"
#include "phys_shape_dtors.h"
#include "phys_coll_shape.h"
#include "phys_coll_grid.h"
#include "world_veh_transfer.h"
#include "build_activity_util.h"
#include "build_alloc.h"
#include "crt.h"

/* the record count, (end - begin) >> 6 (signed shift) */
static int32_t static_count(rx_addr vec) { return ((int32_t)(U32(vec, 0x10) - U32(vec, 0xc))) >> 6; }

/* ReflexAlignedFree 0078fc50 (cdecl, one argument), called at the current ESP */
static void engine_free(rx_addr p) {
    ACT_CALL(1, (SU32(rx_sp(), 4, p), rx_reflex_aligned_free(p)));
}

/* 00774d10 (ESI = destination record, EDI = source record) -> ESI: the 0x40-byte record assigned: position and orientation (+0 .. +0x1f),
 * the word +0x20, the shape (+0x28: the old one's reference dropped, at 0 deleted through vf0 (1); the new one's taken, but only
 * when it is a different shape), the tracking record +0x2c, the words +0x30 / +0x34 and the byte +0x38. */
rx_addr rx_static_record_assign(rx_addr d, rx_addr s) {
    rx_st4(d, rx_ld4(s));
    rx_st4(d + 0x10, rx_ld4(s + 0x10));
    SU32(d, 0x20, U32(s, 0x20));
    const rx_addr old = U32(d, 0x28);
    if (U32(s, 0x28) != old) {
        rx_collision_shape_release(old);
        SU32(d, 0x28, U32(s, 0x28));
        if (U32(s, 0x28) != 0) SU32(U32(s, 0x28), 4, U32(U32(s, 0x28), 4) + 1);
    }
    SU32(d, 0x2c, U32(s, 0x2c));
    SU32(d, 0x30, U32(s, 0x30));
    SU32(d, 0x34, U32(s, 0x34));
    SU8(d, 0x38, U8(s, 0x38));
    return d;
}
RX_FUNCTION(0x00774d10, rx_static_record_assign)

/* 00773ad0 (ESI = the vector, ECX = the index): the record's box in the broadphase grid (the vector's +0x18) updated: skipped without a
 * grid or for an index past the end; the shape's world bounds at the record's pose (orientation +0x10, position +0: vfunc 3) into a
 * box on the stack, BroadphaseGrid_Update (index, grid object, min*, max*). Frame: push ebp; and esp, -16; sub esp, 0x38 (cookie +0x34,
 * box max +8, box min +0x18); push ebx; push edi. */
void rx_static_body_update_grid(rx_addr vec, int32_t index) {
    const rx_addr sp0 = rx_sp(), F = ((sp0 - 4) & ~15u) - 0x38;
    if (U32(vec, 0x18) == 0) return;
    const int32_t count = static_count(vec);
    if (index >= count) return;
    if ((uint32_t)index >= (uint32_t)count) crt_invalid_parameter_noinfo();
    rx_set_sp(F - 8);
    const rx_addr rec = ((uint32_t)index << 6) + U32(vec, 0xc), shape = U32(rec, 0x28);
    const rx_addr box_max = F + 0x08, box_min = F + 0x18;
    phys_shape_bounds(U32(U32(shape, 0), 0xc), shape, rec + 0x10, rec, box_min, box_max);
    ACT_CALL(3, phys_grid_update((uint32_t)index, U32(vec, 0x18), box_min, box_max));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00773ad0, rx_static_body_update_grid)

/* 007747f0 (stdcall: the vector, the index, the source record; RET 0xc): the source record assigned to the record at the index (00774d10),
 * its grid box updated (00773ad0), and the source's tracking record (+0x2c) re-pointed: +8 = the index, +4 = the vector's world tag (+0x1c). */
void rx_static_records_move_slot(rx_addr vec, uint32_t index, rx_addr src) {
    if (index >= (uint32_t)static_count(vec)) crt_invalid_parameter_noinfo();
    const rx_addr dst = (index << 6) + U32(vec, 0xc);
    (void)rx_static_record_assign(dst, src);
    rx_static_body_update_grid(vec, (int32_t)index);
    const rx_addr tracking = U32(src, 0x2c);
    SU32(tracking, 8, index);
    SU32(tracking, 4, U32(vec, 0x1c));
}
RX_FUNCTION(0x007747f0, rx_static_records_move_slot)

/* 007772d0 (EAX = the first record; stack the end): the records' destructors: the shape handle's vtable (+0x24) 0x9765b4, the shape (+0x28)
 * released (the count - 1; at 0 deleted through vf0 (1)). Plain ret. */
void rx_static_records_destroy(rx_addr first, rx_addr last) {
    for (rx_addr e = first; e != last; e += 0x40) {
        SU32(e, 0x24, 0x009765b4);
        rx_collision_shape_release(U32(e, 0x28));
    }
}
RX_FUNCTION(0x007772d0, rx_static_records_destroy)

/* 00773a60 (ESI = the vector header; stack the index; RET 4): the static record at the index removed: unless it is the last, the last
 * record replaces it (007747f0); the last record's grid box is cleared (008825f0); the last record destroyed (007772d0, only while
 * the vector holds at least one record) and dropped. Frame: push ebx; push edi. */
void rx_static_records_remove(rx_addr vec, uint32_t index) {
    ACT_FRAME(L, 0, 2);
    const int32_t count = static_count(vec);
    const uint32_t last = (uint32_t)count - 1;
    if (index != last) {
        if (!(last < (uint32_t)count)) crt_invalid_parameter_noinfo();
        ACT_CALL(3, rx_static_records_move_slot(vec, index, (last << 6) + U32(vec, 0xc)));
    }
    ACT_CALL(0, rx_grid_slot_clear(last, U32(U32(vec, 0x18), 0x30), 0));
    const rx_addr end = U32(vec, 0x10);
    if (((end - U32(vec, 0xc)) & 0xffffffc0u) != 0) {
        ACT_CALL(1, rx_static_records_destroy(end - 0x40, end));
        SU32(vec, 0x10, U32(vec, 0x10) - 0x40);
    }
    ACT_END(L);
}
RX_FUNCTION(0x00773a60, rx_static_records_remove)

/* 00773e10 (EDI = a tracking record; plain ret) -> EDI: the record removed from its world's static vector (the world + 0x19b4; index +8)
 * and freed. Frame: push esi. */
void rx_static_record_free(rx_addr rec) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    const uint32_t index = U32(rec, 8);
    ACT_CALL(1, rx_static_records_remove(U32(rec, 4) + 0x19b4, index));
    engine_free(rec);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00773e10, rx_static_record_free)

/* the handle's reference to its tracking record dropped: the count (+0) - 1; at 0 (and the record still set) the record is removed and
 * freed (the code the compiler inlined in 007735e0, 00773db0 and 00768420) */
static void tracking_release(rx_addr h) {
    const rx_addr rec = U32(h, 4);
    if (rec == 0) return;
    SU32(rec, 0, U32(rec, 0) - 1);
    if (U32(rec, 0) != 0) return;
    const rx_addr again = U32(h, 4);
    if (again == 0) return;
    const uint32_t index = U32(again, 8);
    const rx_addr vec = U32(again, 4) + 0x19b4;
    ACT_CALL(1, rx_static_records_remove(vec, index));
    engine_free(again);
}

/* 007735e0 StaticObjectHandle::vf1 (thiscall, ECX = the handle; plain ret): the reference released as above and, when it was the last,
 * the handle's tracking pointer (+4) cleared. Frame: push ebp, edi (push ebx, esi in the removal). */
void rx_static_handle_release(rx_addr h) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 8);
    const rx_addr rec = U32(h, 4);
    if (rec != 0) {
        SU32(rec, 0, U32(rec, 0) - 1);
        if (U32(rec, 0) == 0) {
            const rx_addr again = U32(h, 4);
            if (again != 0) {
                rx_set_sp(sp0 - 16);
                const uint32_t index = U32(again, 8);
                const rx_addr vec = U32(again, 4) + 0x19b4;
                ACT_CALL(1, rx_static_records_remove(vec, index));
                engine_free(again);
                rx_set_sp(sp0 - 8);
            }
            SU32(h, 4, 0);
        }
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x007735e0, rx_static_handle_release)

/* 00773db0 StaticObjectHandle::sdtor (thiscall; flags; RET 4) -> this: vtable 0x96ddc4; the tracking reference released; freed with bit 0
 * (ReflexAlignedFree). Frame: push edi (push ebx, esi in the removal). */
rx_addr rx_static_handle_sdtor(rx_addr h, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    SU32(h, 0, 0x0096ddc4);
    rx_set_sp(sp0 - 12);
    tracking_release(h);
    rx_set_sp(sp0 - 4);
    if (flags & 1) engine_free(h);
    rx_set_sp(sp0);
    return h;
}
RX_FUNCTION(0x00773db0, rx_static_handle_sdtor)

/* 008218c0 (cdecl: the source begin, the source end, the destination; three more words unused) -> the destination's end: the handles
 * [first, last) assigned over the destination's handles (8 bytes each: the tracking pointer at +4): for a different tracking record the
 * old one's reference is dropped (the last one removes the static record and frees the tracking record) and the new one's taken. */
rx_addr rx_static_handle_copy(rx_addr first, rx_addr last, rx_addr dest) {
    rx_addr src = first, dst = dest;
    while (src != last) {
        const rx_addr old = U32(dst, 4);
        if (U32(src, 4) != old) {
            if (old != 0) {
                SU32(old, 0, U32(old, 0) - 1);
                if (U32(old, 0) == 0) {
                    const rx_addr rec = U32(dst, 4);
                    if (rec != 0) {
                        ACT_CALL(1, rx_static_records_remove(U32(rec, 4) + 0x19b4, U32(rec, 8)));
                        engine_free(rec);
                    }
                }
            }
            SU32(dst, 4, U32(src, 4));
            if (U32(src, 4) != 0) SU32(U32(src, 4), 0, U32(U32(src, 4), 0) + 1);
        }
        src += 8;
        dst += 8;
    }
    return dst;
}
RX_FUNCTION(0x008218c0, rx_static_handle_copy)

/* 0081ff30 (stdcall the vector {+0xc begin, +0x10 end, +0x14 capacity end} of 8-byte handle objects; RET 4): every handle deleted
 * through vf0 (0), the block freed, the three pointers cleared. Frame: push ebx, esi (edi in the loop). */
void rx_static_handle_vector_free(rx_addr v) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 8);
    rx_addr cur = U32(v, 0xc);
    if (cur != 0) {
        const rx_addr end = U32(v, 0x10);
        if (cur != end) {
            do {
                const uint32_t zero = 0;
                (void)act_vcall(cur, 0, 1, &zero);
                cur += 8;
            } while (cur != end);
        }
        engine_free(U32(v, 0xc));
    }
    SU32(v, 0xc, 0);
    SU32(v, 0x10, 0);
    SU32(v, 0x14, 0);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0081ff30, rx_static_handle_vector_free)

/* 0081fe40 (EDI = the checked vector {+0 proxy, +0xc begin, +0x10 end}; stdcall: the result iterator out {proxy, element}, the first iterator
 * (proxy, element), the last iterator (proxy, element); RET 0x14) -> out: vector::erase([first, last)) of 8-byte handles with the debug
 * iterator checks: the elements after `last` assigned down over `first` (008218c0), the vacated tail (from first + (end - last)) deleted
 * through vf0 (0), the end pulled back. */
rx_addr rx_static_handle_vector_erase(rx_addr vec, rx_addr out, rx_addr first_proxy, rx_addr first, rx_addr last_proxy, rx_addr last) {
    (void)first_proxy;
    (void)last_proxy;
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 0x1c);                                       /* sub esp, 0xc; push ebx, ebp, esi */
    SU32(out, 0, 0);
    if (vec == 0 || U32(vec, 0xc) > first || first > U32(vec, 0x10)) crt_invalid_parameter_noinfo();
    SU32(out, 0, U32(vec, 0));
    SU32(out, 4, first);
    if (U32(vec, 0xc) > last || last > U32(vec, 0x10)) crt_invalid_parameter_noinfo();
    if (U32(out, 0) == 0 || U32(out, 0) != U32(vec, 0)) crt_invalid_parameter_noinfo();
    rx_addr new_end = U32(out, 4);
    if (new_end != last) {
        const rx_addr end = U32(vec, 0x10);
        ACT_CALL(6, (void)rx_static_handle_copy(last, end, new_end));
        new_end += ((end - last) >> 3) * 8u;
        const rx_addr old_end = U32(vec, 0x10);
        for (rx_addr p = new_end; p != old_end; p += 8) {
            const uint32_t zero = 0;
            (void)act_vcall(p, 0, 1, &zero);
        }
        SU32(vec, 0x10, new_end);
    }
    rx_set_sp(sp0);
    return out;
}
RX_FUNCTION(0x0081fe40, rx_static_handle_vector_erase)

/* 0081f690 (EAX = the checked vector; plain ret): vector::clear() = erase(begin, end) with the iterator checks (the result iterator on
 * the frame). Frame: push ebp; mov ebp, esp; and esp, -8; sub esp, 0xc; push ebx, esi, edi. */
void rx_static_handle_vector_clear(rx_addr vec) {
    ACT_FRAME_EBP(L, 8, 0xc, 3);
    const rx_addr end = U32(vec, 0x10);
    if (U32(vec, 0xc) > end) crt_invalid_parameter_noinfo();
    const rx_addr begin = U32(vec, 0xc), proxy = U32(vec, 0);
    if (begin > U32(vec, 0x10)) crt_invalid_parameter_noinfo();
    ACT_CALL(5, (void)rx_static_handle_vector_erase(vec, L + 0x10, proxy, begin, U32(vec, 0), end));
    ACT_END(L);
}
RX_FUNCTION(0x0081f690, rx_static_handle_vector_clear)
