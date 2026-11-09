/* world_body.c -- the physics world's body helpers (0x00767f80-0x00777fff): pose / contact-result accessors of the generic
 * dynamic objects, the contact impulses of rigid bodies, the contact responses, and the STL algorithms of the handle lists.
 * Layouts: world_phys.c and core/notes/world.md. */
#include <stdio.h>
#include <stdlib.h>
#include "world.h"
#include "crt.h"
#include "world_util.h"
#include "world_misc.h"
#include "world_extern.h"
#include "build_world.h"
#include "build_alloc.h"
#include "build_rider.h"

/* 00767f80 Recip_Eax (EAX = float*; RET 0): xmm0 = (1 / *p, 0, 0, 0). */
float rx_world_recip(rx_addr p) { return phys_div(1.0f /* 0x931f24 */, F32(p, 0)); }
RX_FUNCTION(0x00767f80, rx_world_recip)

/* 00767f90 Vec3_ReciprocalOf (ECX = object with a scale vector at +0x10; stack out; RET 4; EAX = out): out = (1 / sx, 1 / sy,
 * 1 / sz, 1 / sz). */
void rx_world_vec3_reciprocal_of(rx_addr obj, rx_addr out) {
    const float y = phys_div(1.0f, F32(obj, 0x14)), z = phys_div(1.0f, F32(obj, 0x18)), x = phys_div(1.0f, F32(obj, 0x10));
    v4_st(out, v4(x, y, z, z));
}
RX_FUNCTION(0x00767f90, rx_world_vec3_reciprocal_of)

/* 00768460 DynObj_SetPose (stdcall handle, position, orientation, velocity, angular velocity; RET 0x14): the generic object of
 * the handle's body (0042bd90) gets +0x40 / +0x50 / +0x60 / +0x70 from the four vectors, its contact accumulators +0xb0 / +0xc0
 * cleared (nothing for a null handle body). */
void rx_world_dynobj_set_pose(rx_addr handle, rx_addr pos, rx_addr rot, rx_addr vel, rx_addr angvel) {
    const rx_addr body = U32(handle, 4);
    if (body == 0) return;
    world_frame f = world_frame_enter(0, 0, 1);
    const rx_addr O = world_handle_object(body, WCTX(.ecx = 0));
    v4_st(O + 0x40, v4_ld(pos));
    v4_st(O + 0x50, v4_ld(rot));
    v4_st(O + 0x60, v4_ld(vel));
    v4_st(O + 0x70, v4_ld(angvel));
    v4_st(O + 0xb0, v4_ld(WORLD_ZERO4));
    v4_st(O + 0xc0, v4_ld(WORLD_ZERO4));
    world_frame_leave(&f);
}
RX_FUNCTION(0x00768460, rx_world_dynobj_set_pose)

/* 007684c0 DynObj_TakeContactResults (ESI = out displacement, EDI = out second; stdcall handle, out push; RET 8): the generic
 * object's contact results +0x80 / +0x90 / +0xa0 copied out and cleared; a null handle body gives three zero vectors. */
void rx_world_dynobj_take_contact_results(rx_addr handle, rx_addr out1, rx_addr out2, rx_addr out3) {
    world_frame f = world_frame_enter(0xc, 1, 1);
    const rx_addr body = U32(handle, 4);
    if (body != 0) {
        const rx_addr O = world_handle_object(body, WCTX(.ebx = out3, .esi = out1, .edi = out2));
        v4_st(out1, v4_ld(O + 0x80));
        v4_st(out2, v4_ld(O + 0x90));
        v4_st(out3, v4_ld(O + 0xa0));
        v4_st(O + 0x80, v4_ld(WORLD_ZERO4));
        v4_st(O + 0x90, v4_ld(WORLD_ZERO4));
        v4_st(O + 0xa0, v4_ld(WORLD_ZERO4));
    } else {
        v4_st(out1, v4_ld(WORLD_ZERO4));
        v4_st(out2, v4_ld(WORLD_ZERO4));
        v4_st(out3, v4_ld(WORLD_ZERO4));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x007684c0, rx_world_dynobj_take_contact_results)

/* 00768550 PtrVector_PushIfRoom (EAX = vector {+0xc begin, +0x10 end, +0x14 capacity end}; stdcall value; RET 4): push_back
 * (008423d0, EDI = vector, the address of the argument slot) unless the vector is full. */
void rx_world_ptr_vector_push_if_room(rx_addr v, uint32_t value) {
    const rx_addr slot = rx_sp() + 4;   /* the argument slot: 008423d0 copies the value from there */
    SU32(slot, 0, value);
    world_frame f = world_frame_flat(2, 0);
    const uint32_t begin = U32(v, 0xc);
    const uint32_t cap = begin == 0 ? 0 : (uint32_t)((int32_t)(U32(v, 0x14) - begin) >> 2);
    const uint32_t size = (uint32_t)((int32_t)(U32(v, 0x10) - begin) >> 2);
    if (size != cap) WORLD_CALL_SP(4, phys_vector_push_back(v, slot));
    world_frame_leave(&f);
}
RX_FUNCTION(0x00768550, rx_world_ptr_vector_push_if_room)

/* 00768590 PtrVector_ClearValue (EDI = vector {+0xc begin, +0x10 end}; stdcall value; RET 4): every entry equal to value is
 * set to 0. The count is taken at entry; bounds checks as the original. */
void rx_world_ptr_vector_clear_value(rx_addr v, uint32_t value) {
    const int32_t n = (int32_t)((I32(v, 0x10) - I32(v, 0xc)) >> 2);
    for (int32_t i = 0; i < n; i++) {
        world_bounds_check((uint32_t)i, (uint32_t)((I32(v, 0x10) - I32(v, 0xc)) >> 2));
        if (U32(U32(v, 0xc), 4u * (uint32_t)i) != value) continue;
        world_bounds_check((uint32_t)i, (uint32_t)((I32(v, 0x10) - I32(v, 0xc)) >> 2));
        SU32(U32(v, 0xc), 4u * (uint32_t)i, 0);
    }
}
RX_FUNCTION(0x00768590, rx_world_ptr_vector_clear_value)

/* 007685f0 PtrVector_ReplaceValue (EDI = vector {+0xc begin, +0x10 end}; stdcall old, new; RET 8): every entry equal to old becomes
 * new. The count is taken at entry; bounds checks as the original. Frame: push ebx / ebp / esi. */
void rx_world_ptr_vector_replace_value(rx_addr v, uint32_t old, uint32_t value) {
    const int32_t n = (int32_t)((I32(v, 0x10) - I32(v, 0xc)) >> 2);
    for (int32_t i = 0; i < n; i++) {
        world_bounds_check((uint32_t)i, (uint32_t)((I32(v, 0x10) - I32(v, 0xc)) >> 2));
        if (U32(U32(v, 0xc), 4u * (uint32_t)i) != old) continue;
        world_bounds_check((uint32_t)i, (uint32_t)((I32(v, 0x10) - I32(v, 0xc)) >> 2));
        SU32(U32(v, 0xc), 4u * (uint32_t)i, value);
    }
}
RX_FUNCTION(0x007685f0, rx_world_ptr_vector_replace_value)

/* 00768650 HandlerList_NextActive (ESI = list {+0xc begin, +0x10 end of a pointer vector, +0x18 cursor}; plain ret) -> EAX:
 * from the cursor on, the first non-null entry (the cursor is left on it), or 0 when the cursor runs off the end. Both the
 * "is it null" test and the final read are vector indexings with the CRT bounds check, the size taken afresh each time. */
rx_addr rx_world_handler_list_next_active(rx_addr list) {
    const int32_t count = (int32_t)((I32(list, 0x10) - I32(list, 0xc)) >> 2);
    while (I32(list, 0x18) < count) {
        const uint32_t index = U32(list, 0x18);
        world_bounds_check(index, (uint32_t)((I32(list, 0x10) - I32(list, 0xc)) >> 2));
        if (U32(U32(list, 0xc), 4u * index) != 0) {
            const uint32_t again = U32(list, 0x18);
            world_bounds_check(again, (uint32_t)((I32(list, 0x10) - I32(list, 0xc)) >> 2));
            return U32(U32(list, 0xc), 4u * again);
        }
        SU32(list, 0x18, U32(list, 0x18) + 1u);
    }
    return 0;
}
RX_FUNCTION(0x00768650, rx_world_handler_list_next_active)

/* 00417070 KeyedObjects_At (ESI = the keyed-object vector header {+0xc begin, +0x10 end; 0xe0-byte records}, EDI = index; plain
 * ret) -> EAX = the record's address, with the CRT bounds check (index < (end - begin) / 0xe0). */
rx_addr rx_world_keyed_object_at(rx_addr header, uint32_t index) {
    const int32_t count = (int32_t)(I32(header, 0x10) - I32(header, 0xc)) / (int32_t)0xe0;
    world_bounds_check(index, (uint32_t)count);
    return index * 0xe0u + U32(header, 0xc);
}
RX_FUNCTION(0x00417070, rx_world_keyed_object_at)

/* 007686b0 Kinematic_IsSlower (EDX = A; stdcall &world, B; RET 8; AL = the result): |v_A| < |v_B| with the kinematic velocities
 * world +0x1d10 + 0x50 * (body +0x80), the sort comparator of the contact handler list. */
int rx_world_kinematic_is_slower(rx_addr world, rx_addr A, rx_addr B) {
    const rx_v4 va = v4_ld(world + (U32(A, 0x80) + 0x5d) * 0x50);
    const rx_v4 vb = v4_ld(world + (U32(B, 0x80) + 0x5d) * 0x50);
    const float a = v4_len_xzy(va), b = v4_len_xzy(vb);
    return v4_all3(v4_cmplt(v4_splat(a), v4_splat(b)));
}
RX_FUNCTION(0x007686b0, rx_world_kinematic_is_slower)

/* ---- the Xenon-style quaternion code of the rigid-body impulses -------------------------------------------------------
 * The product a * b as the original evaluates it (an AltiVec idiom: b's lanes permuted, times a's lanes splatted and the sign
 * masks 0x93a1f0 / 0x93a200 / 0x93a210): ((a.z * b.yxwz * S2) + (a.y * b.zwxy * S1)) + ((a.w * b) + (a.x * b.wzyx * S0)). */
static rx_v4 xq_mul(rx_v4 a, rx_v4 b) {
    const rx_v4 s0 = v4(1.0f, -1.0f, 1.0f, -1.0f) /* 0x93a1f0 */, s1 = v4(1.0f, 1.0f, -1.0f, -1.0f) /* 0x93a200 */;
    const rx_v4 s2 = v4(-1.0f, 1.0f, 1.0f, -1.0f) /* 0x93a210 */;
    const rx_v4 b0 = v4_shuf(b, b, 0x1b), b1 = v4_shuf(b0, b0, 0xb1), b2 = v4_shuf(b1, b1, 0x1b);
    const rx_v4 tx = v4_mul(v4_mul(v4_lane(a, 0), b0), s0);
    const rx_v4 ty = v4_mul(v4_mul(v4_lane(a, 1), b1), s1);
    const rx_v4 tz = v4_mul(v4_mul(b2, v4_lane(a, 2)), s2);
    return v4_add(v4_add(tz, ty), v4_add(v4_mul(v4_lane(a, 3), b), tx));
}
static rx_v4 quat_conj(rx_v4 q) { return v4_mul(q, v4(-1.0f, -1.0f, -1.0f, 1.0f) /* 0x93a220 */); }
/* body frame -> world: q * v * q^-1; world -> body: q^-1 * v * q */
static rx_v4 to_world(rx_v4 v, rx_v4 q) { return xq_mul(q, xq_mul(v, quat_conj(q))); }
static rx_v4 to_body(rx_v4 v, rx_v4 q) { return xq_mul(quat_conj(q), xq_mul(v, q)); }
/* a x b as the original's shuffles compute it: b.zxy * a.yzx - b.yzx * a.zxy, w masked off */
static rx_v4 cross_xyz(rx_v4 a, rx_v4 b) {
    const rx_v4 a_yzx = v4_shuf(a, a, 0xc9), a_zxy = v4_shuf(a_yzx, a_yzx, 0xc9);
    const rx_v4 b_zxy = v4_shuf(b, b, 0xd2), b_yzx = v4_shuf(b_zxy, b_zxy, 0xd2);
    return v4_xyz(v4_sub(v4_mul(b_zxy, a_yzx), v4_mul(b_yzx, a_zxy)));
}

/* 007705b0 RigidBody_ContactImpulse (ECX = linear velocity in / out, EDX = inverse inertia (body frame); stdcall inverse mass,
 * orientation q, centre, angular velocity w (body frame, in / out), restitution, friction, contact point, normal; RET 0x20):
 * the impulse of one contact on a rigid body. The contact point's velocity v + w x r against the normal; a separating contact
 * changes nothing. Otherwise the normal impulse j = -(1 + e) vn / (1/m + n.((I^-1 (r x n)) x r)) along -n, plus a friction
 * impulse against the tangential velocity when it exceeds 0.01 (magnitude mu |vt| / denominator), and the velocity changes:
 * v += J / m, w += I^-1 (r x J) in the body frame. */
void rx_world_rigid_body_contact_impulse(rx_addr vel, rx_addr inv_inertia, rx_addr inv_mass, rx_addr orient, rx_addr centre,
                                          rx_addr angvel, rx_addr restitution, rx_addr friction, rx_addr point, rx_addr normal) {
    const rx_v4 q = v4_ld(orient);
    const rx_v4 r = v4_sub(v4_ld(point), v4_ld(centre));
    const rx_v4 w_world = to_world(v4_xyz(v4_ld(angvel)), q);
    const rx_v4 v = v4_ld(vel);
    const rx_v4 neg_vp = v4_sub(v4_zero(), v4_add(cross_xyz(w_world, r), v));   /* -(w x r + v) */
    const rx_v4 n = v4_ld(normal);
    const rx_v4 vn = v4_splat(phys_hsum3(v4_mul(n, neg_vp)));
    if (!v4_all3(v4_cmplt(vn, v4_ld(WORLD_ZERO4)))) return;
    /* the angular share of the effective mass: n . ((I^-1 (r x n)) x r) */
    const rx_v4 ang = to_world(v4_xyz(v4_mul(to_body(cross_xyz(r, n), q), v4_ld(inv_inertia))), q);
    const float k = phys_hsum3(v4_mul(cross_xyz(ang, r), n));
    const rx_v4 denom = v4_add(v4_splat(k), v4_ld(inv_mass));
    const rx_v4 j = v4_div(v4_mul(v4_sub(v4_zero(), v4_add(v4_ld(restitution), v4_ld(0x00d61650))), vn), denom);
    const rx_v4 jn = v4_mul(v4_sub(v4_zero(), j), n);
    const rx_v4 tangent = v4_sub(v4_mul(n, vn), neg_vp);
    const rx_v4 tlen = v4_sqrt(v4_splat(v4_len2_xzy(tangent)));
    rx_v4 jt = v4_ld(WORLD_ZERO4);
    if (v4_all3(v4_cmplt(v4_splat(0.01f /* 0x9322ac */), tlen)))
        jt = v4_mul(v4_div(v4_mul(v4_sub(v4_zero(), v4_ld(friction)), tlen), denom), v4_div(tangent, tlen));
    const rx_v4 J = v4_add(jt, jn);
    v4_st(vel, v4_add(v4_mul(v4_ld(inv_mass), J), v));
    const rx_v4 dw = v4_mul(to_body(cross_xyz(r, J), q), v4_ld(inv_inertia));
    v4_st(angvel, v4_add(dw, v4_ld(angvel)));
}
RX_FUNCTION(0x007705b0, rx_world_rigid_body_contact_impulse)

/* ---- the STL algorithms of the contact-handler list (checked iterators: {container, pointer} pairs, aborts as the original).
 * Some write into their own argument slots (the caller's memory): `args` is the entry stack pointer, args + 4 the first. */
static void crt_abort(void) { crt_invalid_parameter_noinfo(); }
/* the bounds (+0x10, the end) of a checked iterator's container; a null container reads address 0x10 as the original */
static uint32_t container_end(rx_addr c) { return U32(c ? U32(c, 0) : 0, 0x10); }

/* 00776920 PtrRemove (cdecl result, &value, first.c, first.p, last.c, last.p; EAX = result): std::remove(first, last, value)
 * on pointers. The found position is written back into first.p's argument slot (and first.c's), as the original does; the rest
 * is remove_copy (007774f0). */
rx_addr rx_world_ptr_remove(rx_addr result, rx_addr value, rx_addr first_c, rx_addr first_p, rx_addr last_c, rx_addr last_p) {
    const rx_addr args = rx_sp();
    world_frame f = world_frame_flat(4, 0xc);
    rx_addr p = first_p;
    if (p != last_p) { const uint32_t v = U32(value, 0); while (U32(p, 0) != v) { p += 4; if (p == last_p) break; } }
    SU32(args, 0xc, first_c); SU32(args, 0x10, p);
    if (first_c == 0 || first_c != last_c) crt_abort();
    if (p == last_p) {
        SU32(result, 4, p); SU32(result, 0, first_c);
    } else {
        SU32(f.esp, 0x14, first_c);
        if (first_c == 0) crt_abort();
        if (p >= container_end(first_c)) crt_abort();
        SU8(f.esp, 0x14, 0);
        const uint32_t tag = U32(f.esp, 0x14);
        WORLD_CALL_SP(0x1c, rx_world_ptr_remove_copy(result, p + 4, last_p, value, tag, first_c, p));
    }
    world_frame_leave(&f);
    return result;
}
RX_FUNCTION(0x00776920, rx_world_ptr_remove)

/* 007774f0 PtrRemoveCopy (cdecl result, first, last, &value, (unused), dest.c, dest.p; EAX = result): std::remove_copy for
 * pointers; dest.p's argument slot advances as the original's does. Result = {dest.c, dest.p}. */
rx_addr rx_world_ptr_remove_copy(rx_addr result, rx_addr first, rx_addr last, rx_addr value, uint32_t unused, rx_addr dest_c,
                                 rx_addr dest_p) {
    const rx_addr args = rx_sp();
    (void)unused;
    for (rx_addr p = first; p != last; p += 4) {
        if (U32(p, 0) == U32(value, 0)) continue;
        const rx_addr dp = dest_p;
        if (dest_c == 0) crt_abort();
        if (dp >= container_end(dest_c)) crt_abort();
        dest_p = dp + 4;
        SU32(args, 0x1c, dest_p);   /* the original advances dest.p in its argument slot */
        if (dest_c == 0) crt_abort();
        if (dp >= container_end(dest_c)) crt_abort();
        SU32(dp, 0, U32(p, 0));
    }
    SU32(result, 0, dest_c);
    SU32(result, 4, dest_p);
    return result;
}
RX_FUNCTION(0x007774f0, rx_world_ptr_remove_copy)

/* the comparator call of the sorts: 007686b0 (EDX = a; stdcall &comparator (the world), b) */
static int world_is_slower(rx_addr world, rx_addr a, rx_addr b) {
    int r;
    WORLD_CALL_SP(8, r = rx_world_kinematic_is_slower(world, a, b));
    return r;
}

/* 00777db0 BodySort_Insertion (cdecl first, last, comparator (by value: the world)): insertion sort of the handler pointers by
 * speed (comparator 007686b0 with the address of the comparator's argument slot); runs move with memmove_s (IAT 0x916200). */
void rx_world_body_sort_insertion(rx_addr first, rx_addr last, rx_addr world) {
    world_frame f = world_frame_flat(4, 8);
#define SLOWER(a, b) world_is_slower(world, (a), (b))
    if (first != last && first + 4 != last) {
        SU32(f.esp, 0x14, 4 - first);
        for (rx_addr cur = first + 4; cur != last; cur += 4) {
            const uint32_t val = U32(cur, 0);
            SU32(f.esp, 0x10, val);
            if (SLOWER(val, U32(first, 0))) {
                const int32_t count = (int32_t)(U32(f.esp, 0x14) + cur - 4) >> 2;
                if (count > 0) {
                    const uint32_t bytes = (uint32_t)count * 4;
                    crt_memmove_s(cur - bytes + 4, bytes, first, bytes);   /* memmove_s (IAT 0x916200) */
                }
                SU32(first, 0, val);
            } else {
                rx_addr hole = cur, prev = cur - 4;
                if (SLOWER(val, U32(prev, 0))) {
                    uint32_t moved = U32(prev, 0);
                    for (;;) {
                        SU32(hole, 0, moved);
                        const uint32_t next = U32(prev, -4);
                        hole = prev; prev -= 4;
                        if (!SLOWER(val, next)) break;
                        moved = next;
                    }
                }
                SU32(hole, 0, val);
            }
        }
    }
#undef SLOWER
    world_frame_leave(&f);
}
RX_FUNCTION(0x00777db0, rx_world_body_sort_insertion)

/* ---- the introsort pieces of the contact-handler sort (MSVC std::sort on the handler pointers, comparator 007686b0 on the
 * world). All take the comparator as the world pointer; every compare is Kinematic_IsSlower(A, B) = |v_A| < |v_B|. Elements
 * are 4-byte handler pointers; "first" / "last" are pointers into the array. ---- */
#define SLOWER(a, b) world_is_slower(world, (a), (b))

/* swap the dwords at two element pointers (the original's iter_swap, always guarded by a != b) */
static void body_swap(rx_addr a, rx_addr b) {
    const uint32_t t = U32(a, 0);
    SU32(a, 0, U32(b, 0));
    SU32(b, 0, t);
}

/* 00778190 (ESI = a, EDI = b; stack c, comparator; plain RET): _Med3 - order three elements so that *b <= *a <= *c in the
 * |velocity| order: swap(a, b) if *a is slower than *b; swap(c, a) if *c is slower than *a; swap(a, b) again. Frame: push ecx,
 * ebx, ebp. */
void rx_world_body_med3(rx_addr a, rx_addr b, rx_addr c, rx_addr world) {
    world_frame f = world_frame_flat(3, 4);
    uint32_t vb = U32(b, 0), va = U32(a, 0);
    if (SLOWER(va, vb) && a != b) {
        SU32(a, 0, vb);
        SU32(b, 0, va);
    }
    vb = U32(a, 0);
    va = U32(c, 0);
    if (SLOWER(va, vb) && c != a) {
        SU32(c, 0, vb);
        SU32(a, 0, va);
    }
    vb = U32(b, 0);
    va = U32(a, 0);
    if (SLOWER(va, vb) && a != b) {
        SU32(a, 0, vb);
        SU32(b, 0, va);
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x00778190, rx_world_body_med3)

/* 00777c70 (EAX = first; stack mid, last1 (= last - 1), comparator; plain RET): _Median - the pivot goes to *mid. More than 40
 * elements: the "ninther" (Tukey): med3 over three groups of three elements spaced step = (n + 1) / 8 apart (at first, mid and
 * last1), then med3 over the three group medians; otherwise one med3(mid, first, last1). Frame: sub esp, 8; push ebx / ebp /
 * esi / edi. */
void rx_world_body_median(rx_addr first, rx_addr mid, rx_addr last1, rx_addr world) {
    world_frame f = world_frame_flat(4, 8);
    int32_t n = (int32_t)(last1 - first) >> 2;
    if (n > 0x28) {
        const int32_t n1 = n + 1;
        const int32_t step = (n1 + ((n1 >> 31) & 7)) >> 3;
        const uint32_t s4 = (uint32_t)step * 4u, s8 = (uint32_t)step * 8u;
        WORLD_CALL_SP(8, rx_world_body_med3(first + s4, first, first + s8, world));
        WORLD_CALL_SP(8, rx_world_body_med3(mid, mid - s4, mid + s4, world));
        WORLD_CALL_SP(8, rx_world_body_med3(last1 - s4, last1 - s8, last1, world));
        WORLD_CALL_SP(8, rx_world_body_med3(mid, first + s4, last1 - s4, world));
    } else {
        WORLD_CALL_SP(8, rx_world_body_med3(mid, first, last1, world));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x00777c70, rx_world_body_median)

/* 00777580 BodySort_Partition (cdecl out, first, last, comparator; EAX = out): std::_Unguarded_partition with the equal-range
 * handling. The pivot is the median of [first, last) (at mid); the equal run around it grows to [pfirst, plast); elements
 * slower-compare-wise on either side are partitioned in a loop that swaps toward the ends; the result {pfirst, plast} (the equal
 * range) is written to out[0..1]. Frame: sub esp, 0xc; push ebx / ebp / esi / edi. */
void rx_world_body_partition(rx_addr out, rx_addr first, rx_addr last, rx_addr world) {
    world_frame f = world_frame_flat(4, 0xc);
    int32_t count = (int32_t)(last - first) >> 2;
    count = (count + (count < 0 ? 1 : 0)) >> 1;
    const rx_addr mid = first + (uint32_t)count * 4u;
    WORLD_CALL_SP(0xc, rx_world_body_median(first, mid, last - 4, world));

    rx_addr pfirst = mid;
    rx_addr plast = mid + 4;
    /* the equal run below and above the pivot */
    while (first < pfirst) {
        const uint32_t cur = U32(pfirst, 0), prev = U32(pfirst - 4, 0);
        if (SLOWER(prev, cur)) break;
        if (SLOWER(cur, prev)) break;
        pfirst -= 4;
    }
    if (plast < last) {
        const uint32_t pivot = U32(pfirst, 0);
        while (plast < last) {
            const uint32_t cur = U32(plast, 0);
            if (SLOWER(cur, pivot)) break;
            if (SLOWER(pivot, cur)) break;
            plast += 4;
        }
    }
    rx_addr gfirst = plast;
    rx_addr glast = pfirst;
    for (;;) {
        /* gfirst climbs over elements slower than the pivot; equal ones join the equal range */
        while (gfirst < last) {
            const uint32_t cur = U32(gfirst, 0);
            if (SLOWER(U32(pfirst, 0), cur)) {
                /* *pfirst slower than *gfirst: stays above */
            } else if (SLOWER(cur, U32(pfirst, 0))) {
                break;
            } else {
                const rx_addr eq = plast;
                plast += 4;
                if (eq != gfirst) body_swap(eq, gfirst);
            }
            gfirst += 4;
        }
        /* glast descends over the elements below */
        while (first < glast) {
            const uint32_t pivot = U32(pfirst, 0);
            const uint32_t cur = U32(glast - 4, 0);
            if (SLOWER(cur, pivot)) {
                /* stays below */
            } else if (SLOWER(pivot, cur)) {
                break;
            } else {
                pfirst -= 4;
                if (pfirst != glast - 4) body_swap(pfirst, glast - 4);
            }
            glast -= 4;
        }
        if (glast == first) {
            if (gfirst == last) {
                SU32(out, 0, pfirst);
                SU32(out, 4, plast);
                world_frame_leave(&f);
                return;
            }
            /* no room at the bottom: rotate the pivot upward */
            if (plast != gfirst) body_swap(pfirst, plast);
            plast += 4;
            if (pfirst != gfirst) body_swap(pfirst, gfirst);
            pfirst += 4;
            gfirst += 4;
        } else {
            glast -= 4;
            if (gfirst == last) {
                /* no room at the top: rotate the pivot downward */
                pfirst -= 4;
                if (glast != pfirst) body_swap(glast, pfirst);
                plast -= 4;
                if (pfirst != plast) body_swap(pfirst, plast);
            } else {
                if (gfirst != glast) body_swap(gfirst, glast);
                gfirst += 4;
            }
        }
    }
}
RX_FUNCTION(0x00777580, rx_world_body_partition)

/* 00778660 (EAX = hole; stack first, top, value, comparator; plain RET): std::_Push_heap_0 - sift `value` up from `hole` toward
 * `top` (parents (hole - 1) / 2, truncated toward zero) while the parent is slower than the value. Frame: push ecx / ebx / ebp /
 * esi / edi. */
void rx_world_body_push_heap(int32_t hole, rx_addr first, int32_t top, uint32_t value, rx_addr world) {
    world_frame f = world_frame_flat(5, 0);
    int32_t parent = hole - 1;
    parent = (parent - (parent >> 31)) >> 1;
    if (top < hole) {
        for (;;) {
            const uint32_t pv = U32(first, 4u * (uint32_t)parent);
            if (!SLOWER(pv, value)) {
                SU32(first, 4u * (uint32_t)hole, value);
                world_frame_leave(&f);
                return;
            }
            SU32(first, 4u * (uint32_t)hole, pv);
            hole = parent;
            parent = parent - 1;
            parent = (parent - (parent >> 31)) >> 1;
            if (!(top < hole)) {
                SU32(first, 4u * (uint32_t)hole, value);
                world_frame_leave(&f);
                return;
            }
        }
    }
    SU32(first, 4u * (uint32_t)hole, value);
    world_frame_leave(&f);
}
RX_FUNCTION(0x00778660, rx_world_body_push_heap)

/* 00778200 (EAX = hole; stack first, n, value, comparator; plain RET): std::_Adjust_heap - move the hole down to a leaf (always
 * to a leaf (at each level the child that is not slower than its sibling moves up), then sift `value` up from the leaf toward
 * the starting index (00778660). The original reuses the `n` argument slot for the starting index (`top`). Frame: push ecx / ebx / ebp / esi / edi. */
void rx_world_body_adjust_heap(int32_t hole, rx_addr first, int32_t n, uint32_t value, rx_addr world) {
    const rx_addr args = rx_sp();
    world_frame f = world_frame_flat(5, 0);
    const int32_t top = hole;
    SU32(args, 8, (uint32_t)top);                      /* the n slot reused as `top` */
    int32_t child = 2 * hole + 2;
    while (child < n) {
        if (SLOWER(U32(first, 4u * (uint32_t)child), U32(first, 4u * (uint32_t)(child - 1)))) child--;
        SU32(first, 4u * (uint32_t)hole, U32(first, 4u * (uint32_t)child));
        hole = child;
        child = 2 * child + 2;
    }
    if (child == n) {
        SU32(first, 4u * (uint32_t)hole, U32(first, 4u * (uint32_t)(n - 1)));
        hole = n - 1;
    }
    WORLD_CALL_SP(0x10, rx_world_body_push_heap(hole, first, top, value, world));
    world_frame_leave(&f);
}
RX_FUNCTION(0x00778200, rx_world_body_adjust_heap)

/* 00777d20 (EAX = last, EDI = first; stack comparator; plain RET): std::make_heap - adjust_heap from the last parent down to 0.
 * Frame: push ebx / esi. */
void rx_world_body_make_heap(rx_addr first, rx_addr last, rx_addr world) {
    world_frame f = world_frame_flat(2, 0);
    const int32_t n = (int32_t)(last - first) >> 2;
    int32_t i = (n + (n < 0 ? 1 : 0)) >> 1;
    while (i > 0) {
        const uint32_t value = U32(first, 4u * (uint32_t)(i - 1));
        i--;
        WORLD_CALL_SP(0x10, rx_world_body_adjust_heap(i, first, n, value, world));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x00777d20, rx_world_body_make_heap)

/* 00777d60 (EAX = last, EDI = first; stack comparator; plain RET): std::sort_heap - while more than one element: the last element
 * is the value, the first moves to the end, adjust_heap(0, ..., n - 1, value) re-heaps the rest. Frame: push ecx / esi. */
void rx_world_body_sort_heap(rx_addr first, rx_addr last, rx_addr world) {
    world_frame f = world_frame_flat(2, 0);
    int32_t bytes = (int32_t)(last - first);
    while ((bytes >> 2) > 1) {
        const uint32_t value = U32(first, (uint32_t)bytes - 4u);
        const uint32_t top = U32(first, 0);
        SU32(first, (uint32_t)bytes - 4u, top);
        WORLD_CALL_SP(0x10, rx_world_body_adjust_heap(0, first, (bytes - 4) >> 2, value, world));
        bytes -= 4;
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x00777d60, rx_world_body_sort_heap)
#undef SLOWER

/* 00776ef0 BodySort_Loop (cdecl first, last, ideal, comparator): std::sort's introsort loop over the handler pointers. While
 * more than 32 elements: the median partition 00777580 into a {first, second} pair on the stack and recursion on the smaller
 * side (with ideal / 2 + ideal / 4 computed from the ideal ARGUMENT, which the original reloads each round); ideal exhausted:
 * heap sort (00777d20 make_heap when more than one element, 00777d60 sort_heap; EAX = last, EDI = first); the remainder by
 * insertion sort. */
void rx_world_body_sort_loop(rx_addr first, rx_addr last, int32_t ideal, rx_addr world) {
    world_frame f = world_frame_flat(4, 0xc);
    const rx_addr mid = f.esp + 0x10;
    int32_t n = (int32_t)(last - first) >> 2;
    while (n > 32) {
        const int32_t arg_ideal = ideal;   /* the original reloads the argument each round: never the halved value */
        if (arg_ideal <= 0) {
            if (((int32_t)(last - first) & ~3) > 4) WORLD_CALL_SP(4, rx_world_body_make_heap(first, last, world));
            WORLD_CALL_SP(4, rx_world_body_sort_heap(first, last, world));
            world_frame_leave(&f);
            return;
        }
        WORLD_CALL_SP(0x10, rx_world_body_partition(mid, first, last, world));
        const rx_addr mid_first = U32(mid, 0), mid_second = U32(mid, 4);
        const int32_t half = arg_ideal / 2;
        const int32_t next_ideal = half + half / 2;
        if (((int32_t)(mid_first - first) & ~3) < ((int32_t)(last - mid_second) & ~3)) {
            WORLD_CALL_SP(0x10, rx_world_body_sort_loop(first, mid_first, next_ideal, world));
            first = mid_second;
        } else {
            WORLD_CALL_SP(0x10, rx_world_body_sort_loop(mid_second, last, next_ideal, world));
            last = mid_first;
        }
        n = (int32_t)(last - first) >> 2;
    }
    if (n > 1) WORLD_CALL_SP(0xc, rx_world_body_sort_insertion(first, last, world));
    world_frame_leave(&f);
}
RX_FUNCTION(0x00776ef0, rx_world_body_sort_loop)

/* 00777110 BodyRecord_FillN (cdecl dest, n, value): std::uninitialized_fill_n for the 0x50-byte nearby-object records:
 * 004dbfa0 (EAX = dest, ECX = value) per record; the argument slots dest / n advance as the original's do (they are the
 * caller's memory). The original's SEH frame (fs:[0] linked in and restored) leaves nothing behind and is not reproduced.
 * Returns EAX = the last copy's result (or the frame address ebp - 0xc with n = 0). */
uint32_t rx_world_body_record_fill_n(rx_addr dest, uint32_t n, rx_addr value) {
    const rx_addr args = rx_sp();
    world_frame f = world_frame_flat(9, 0);
    const rx_addr ebp = args - 4;
    uint32_t eax = ebp - 0xc;
    while (n != 0) {
        WORLD_CALL_SP(0, rx_world_record_construct(dest, value));   /* 004dbfa0 */
        eax = dest;
        n--; SU32(args, 8, n);
        dest += 0x50; SU32(args, 4, dest);
    }
    world_frame_leave(&f);
    return eax;
}
RX_FUNCTION(0x00777110, rx_world_body_record_fill_n)

/* ---- the kinematic bodies' integration helpers ---------------------------------------------------------------------- */

/* 00769bd0 Kinematic_Accelerations (ECX = k, EDX = acceleration out, ESI = orientation rate out; stdcall world, material,
 * velocity out, angular acceleration out; RET 0x10): one substep's derivatives of kinematic body k:
 *   acceleration = accumulated force (pose +0x20) * inverse mass (+0x40) + gravity (0xd61590) - material +0x50 * velocity;
 *   velocity out = the pose velocity; orientation rate = 0.5 * q * (w, 0xd61660.w) (the Xenon quaternion product);
 *   angular acceleration = torque (+0x5ba0) * inverse inertia (+0x5bc0) - material +0x60 * angular velocity.
 * The 0.5 vector is a function static (0xdb4500, initialized on first use; flag 0xdb4510). */
void rx_world_kinematic_accelerations(uint32_t k, rx_addr acc, rx_addr qdot, rx_addr world, rx_addr mat, rx_addr vel,
                                      rx_addr ang_acc) {
    world_frame f = world_frame_enter(0x28, 2, 1);
    if (!(U8(0, 0x00db4510) & 1)) { SU32(0, 0x00db4510, U32(0, 0x00db4510) | 1); v4_st(0x00db4500, v4_splat(0.5f /* 0x9322cc */)); }
    const rx_addr pose = world + 0x1d00 + 0x50 * k, orient = world + 0x5b80 + 0x50 * k;
    rx_v4 a = v4_add(v4_mul(v4_ld(pose + 0x40), v4_ld(pose + 0x20)), v4_ld(0x00d61590));
    v4_st(acc, a);
    a = v4_sub(a, v4_mul(v4_ld(mat + 0x50), v4_ld(pose + 0x10)));
    v4_st(acc, a);
    v4_st(vel, v4_ld(pose + 0x10));
    /* the angular velocity as a quaternion (its w lane from the runtime vector 0xd61660), in the original's frame */
    const rx_addr omega = f.esp + 0x10;
    v4_st(f.esp + 0x20, v4_ld(0x00d61660));
    v4_st(omega, v4_ld(orient + 0x10));
    SU32(omega, 0xc, U32(f.esp, 0x2c));
    const rx_v4 rate = xq_mul(v4_ld(orient), v4_ld(omega));
    v4_st(qdot, rate);
    v4_st(qdot, v4_mul(rate, v4_ld(0x00db4500)));
    rx_v4 aa = v4_mul(v4_ld(orient + 0x40), v4_ld(orient + 0x20));
    v4_st(ang_acc, aa);
    v4_st(ang_acc, v4_sub(aa, v4_mul(v4_ld(mat + 0x60), v4_ld(orient + 0x10))));
    world_frame_leave(&f);
}
RX_FUNCTION(0x00769bd0, rx_world_kinematic_accelerations)

/* ---- contact responses --------------------------------------------------------------------------------------------- */

/* The material pair table (0xda6ad0: 12-byte entries from +0x24, end +0x28): restitution +0, friction +4 for the pair
 * (material id a, material id b) found by 0084fe70; the table is built on first use (00850010, flag 0xdce7d4, atexit 0x914ee0). */
static rx_addr material_pair(uint32_t id_a, uint32_t id_b) {
    (void)rx_phys_props_get();   /* the registry's one-time build (00850010: flag 0xdce7d4 bit 0, its _atexit is not simulated) */
    uint32_t idx;
    WORLD_CALL_SP(8, idx = phys_pair_index(0x00da6ad0, id_a, id_b));
    world_bounds_check(idx, (uint32_t)((I32(0, 0x00da6af8) - I32(0, 0x00da6af4)) / 12));
    return U32(0, 0x00da6af4) + idx * 12;
}
/* the inverse inertia of a generic object as a vector: (1 / Ix, 1 / Iy, 1 / Iz, 1 / Iz) * 1 (+0x10 .. +0x18) */
static rx_v4 inverse_inertia(rx_addr A) {
    const float x = phys_div(1.0f, F32(A, 0x10)), y = phys_div(1.0f, F32(A, 0x14)), z = phys_div(1.0f, F32(A, 0x18));
    return v4_mul(v4(x, y, z, z), v4_splat(1.0f /* 0x931f24 */));
}

/* 0076efc0 Static_ContactResponse (ECX = static object S, EDI = generic object A; stdcall mean contact (point, normal); RET 4):
 * A's rigid-body impulse against the static world with the material pair (A +0x24, S +0x20): its velocity +0x60 / angular
 * velocity +0x70 change through 007705b0 (inverse mass 1 / A +0, inverse inertia from A +0x10), and the changes are added to
 * A's contact results +0x80 / +0x90. */
void rx_world_static_contact_response(rx_addr S, rx_addr A, rx_addr contact_mean) {
    world_frame f = world_frame_enter(0x78, 2, 1);
    const rx_addr sp = f.esp, vel = sp + 0x10, angvel = sp + 0x20, friction = sp + 0x30, inv_mass = sp + 0x40;
    const rx_addr restitution = sp + 0x50, inv_inertia = sp + 0x60;
    const rx_addr pair = material_pair(U32(A, 0x24), U32(S, 0x20));
    v4_st(restitution, v4_splat(F32(pair, 0)));
    v4_st(friction, v4_splat(F32(pair, 4)));
    v4_st(vel, v4_ld(A + 0x60));
    v4_st(angvel, v4_ld(A + 0x70));
    v4_st(inv_mass, v4_splat(rx_world_recip(A)));
    v4_st(inv_inertia, inverse_inertia(A));
    WORLD_CALL_SP(0x20, rx_world_rigid_body_contact_impulse(vel, inv_inertia, inv_mass, A + 0x50, A + 0x40, angvel, restitution,
                                                            friction, contact_mean, contact_mean + 0x10));
    const rx_v4 v = v4_ld(vel), w = v4_ld(angvel);
    v4_st(A + 0x80, v4_add(v4_sub(v, v4_ld(A + 0x60)), v4_ld(A + 0x80)));
    v4_st(A + 0x90, v4_add(v4_sub(w, v4_ld(A + 0x70)), v4_ld(A + 0x90)));
    v4_st(A + 0x60, v);
    v4_st(A + 0x70, w);
    world_frame_leave(&f);
}
RX_FUNCTION(0x0076efc0, rx_world_static_contact_response)

/* 0076f160 Terrain_ContactResponse (EDI = generic object A; stdcall mean contact (point, normal); RET 4): A's rigid-body impulse
 * against the terrain - 0076efc0's body with the material pair (A +0x24, 0) (the terrain's material id is 0; there is no static
 * object) and its inverse inertia merged without the extra * 1 (the original's shufps at 0076f25d..0076f271): A's velocity +0x60 /
 * angular velocity +0x70 change through 007705b0 (inverse mass 1 / A +0, inverse inertia from A +0x10), added to A's contact
 * results +0x80 / +0x90. Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x78; push ebx, esi. */
void rx_world_terrain_contact_response(rx_addr A, rx_addr contact_mean) {
    world_frame f = world_frame_enter(0x78, 2, 1);
    const rx_addr sp = f.esp, vel = sp + 0x10, angvel = sp + 0x20, friction = sp + 0x30, inv_mass = sp + 0x40;
    const rx_addr restitution = sp + 0x50, inv_inertia = sp + 0x60;
    const rx_addr pair = material_pair(U32(A, 0x24), 0);
    v4_st(restitution, v4_splat(F32(pair, 0)));
    v4_st(friction, v4_splat(F32(pair, 4)));
    v4_st(vel, v4_ld(A + 0x60));
    v4_st(angvel, v4_ld(A + 0x70));
    v4_st(inv_mass, v4_splat(rx_world_recip(A)));
    const float ix = phys_div(1.0f /* 0x931f24 */, F32(A, 0x10));
    const float iy = phys_div(1.0f /* 0x931f24 */, F32(A, 0x14));
    const float iz = phys_div(1.0f /* 0x931f24 */, F32(A, 0x18));
    v4_st(inv_inertia, v4(ix, iy, iz, iz));
    WORLD_CALL_SP(0x20, rx_world_rigid_body_contact_impulse(vel, inv_inertia, inv_mass, A + 0x50, A + 0x40, angvel, restitution,
                                                            friction, contact_mean, contact_mean + 0x10));
    const rx_v4 v = v4_ld(vel), w = v4_ld(angvel);
    v4_st(A + 0x80, v4_add(v4_sub(v, v4_ld(A + 0x60)), v4_ld(A + 0x80)));
    v4_st(A + 0x90, v4_add(v4_sub(w, v4_ld(A + 0x70)), v4_ld(A + 0x90)));
    v4_st(A + 0x60, v);
    v4_st(A + 0x70, w);
    world_frame_leave(&f);
}
RX_FUNCTION(0x0076f160, rx_world_terrain_contact_response)

/* ---- std::vector<BodyRecord> (0x50-byte records, the nearby-object records) reallocating insert --------------------------
 * 00774c30 insert-at-iterator -> 00775800 _Insert (grow or shift, one value) -> the leaves below, then 00775b10 builds the
 * returned iterator. Every record copy moves the first 0x48 bytes only (movaps x4 + two dwords), as the original's copies do.
 * The container "proxy" pointer of a checked iterator is vector +0 (the iterator's container field points to it, its first
 * dword is the vector). The original's SEH frames (fs:[0] linked in and restored, the unwinding state words) leave nothing
 * behind and are not reproduced; std::length_error (00851560) and a failed allocation are not reachable. */
static void body_record_copy48(rx_addr dst, rx_addr src) {
    for (uint32_t i = 0; i < 0x48; i += 4) SU32(dst, i, U32(src, i));
}

/* 004db640 BodyRecordVector_Alloc (EAX = count; plain ret) -> EAX: _aligned_malloc(count * 0x50, 16), under the allocator lock
 * [0xda67d8] / 0xd61704 that the single-threaded core does not take (as StdMemoryAllocator::vf1). */
rx_addr rx_world_body_record_alloc(uint32_t count) { return rx_crt_aligned_malloc(count * 0x50u, 16); }
RX_FUNCTION(0x004db640, rx_world_body_record_alloc)

/* 00430030 (ECX = source, EAX = destination; plain ret): one record copied (0x48 bytes). */
void rx_world_body_record_copy(rx_addr dst, rx_addr src) { body_record_copy48(dst, src); }
RX_FUNCTION(0x00430030, rx_world_body_record_copy)

/* 00774ce0 (EDX = value, ESI = count, EDI = destination; plain ret) -> EAX = destination + count * 0x50: BodyRecord_FillN
 * (00777110) with the original's argument pushes (the fifth and fourth are the low-byte-zeroed slot read twice). */
rx_addr rx_world_body_record_fill_end(rx_addr dest, uint32_t n, rx_addr value) {
    const rx_addr entry = rx_sp();
    rx_set_sp(entry - 4);                      /* push ecx */
    SU8(entry - 4, 0, 0);                      /* mov byte [esp], 0 */
    WORLD_CALL_SP(0x14, rx_world_body_record_fill_n(dest, n, value));
    rx_set_sp(entry);
    return n * 0x50u + dest;
}
RX_FUNCTION(0x00774ce0, rx_world_body_record_fill_end)

/* 004dbf20 (cdecl first, last, destination (+ two unused words); plain ret) -> EAX = destination end: std::uninitialized_copy
 * of the records through 004dbfa0 (a null destination is skipped there). */
rx_addr rx_world_body_record_copy_construct(rx_addr first, rx_addr last, rx_addr dest) {
    const rx_addr args = rx_sp();
    while (first != last) {
        WORLD_CALL_SP(0, rx_world_record_construct(dest, first));
        dest += 0x50;
        SU32(args, 0xc, dest);      /* the argument slots (the caller's memory) advance with the loop: [ebp + 0x10] / [ebp + 8] */
        first += 0x50;
        SU32(args, 4, first);
    }
    return dest;
}
RX_FUNCTION(0x004dbf20, rx_world_body_record_copy_construct)

/* 00776d50 (EDX = destination; stdcall first, last; RET 8) -> EAX = destination end: 004dbf20 with the original's pushes
 * (first, last, destination, last, the zeroed tag slot, and the reserved slot it pushed first). */
rx_addr rx_world_body_record_copy_uninit(rx_addr dest, rx_addr first, rx_addr last) {
    const rx_addr entry = rx_sp();
    rx_set_sp(entry - 4);                      /* push ecx */
    SU8(entry - 4, 0, 0);
    rx_addr end;
    WORLD_CALL_SP(0x14, end = rx_world_body_record_copy_construct(first, last, dest));
    rx_set_sp(entry);
    return end;
}
RX_FUNCTION(0x00776d50, rx_world_body_record_copy_uninit)

/* 00777400 (ECX = source record, EDX = end; cdecl destination; plain ret) -> EAX = end: the record assigned to every slot of
 * [destination, end) (nothing when destination == end). */
rx_addr rx_world_body_record_assign_range(rx_addr src, rx_addr end, rx_addr dest) {
    if (dest != end) {
        do {
            body_record_copy48(dest, src);
            dest += 0x50;
        } while (dest != end);
    }
    return dest;
}
RX_FUNCTION(0x00777400, rx_world_body_record_assign_range)

/* 00778020 (ECX = last, EDX = first; cdecl destination end; plain ret) -> EAX = destination end - (last - first): the records
 * of [first, last) copied backwards so that the last one lands just below the destination end. */
rx_addr rx_world_body_record_copy_backward(rx_addr first, rx_addr last, rx_addr dest_end) {
    if (first != last) {
        do {
            last -= 0x50;
            dest_end -= 0x50;
            body_record_copy48(dest_end, last);
        } while (last != first);
    }
    return dest_end;
}
RX_FUNCTION(0x00778020, rx_world_body_record_copy_backward)

/* 00777a60 (EAX = last, EDI = destination end; cdecl first (+ three tag words); plain ret) -> EAX = destination end minus
 * (last - first) / 0x50 records: 00778020 with the same arguments (it saves and restores ESI, EBX and ECX; it zeroes the low byte of the `first` argument slot). */
rx_addr rx_world_body_record_move_backward(rx_addr first, rx_addr last, rx_addr dest_end) {
    const rx_addr entry = rx_sp();
    SU8(entry, 4, 0);                          /* the low byte of its own argument slot is zeroed (a tag word passed on) */
    rx_set_sp(entry - 8);                      /* push ecx; push ebx */
    WORLD_CALL_SP(0x10, rx_world_body_record_copy_backward(first, last, dest_end));
    rx_set_sp(entry);
    return dest_end - (uint32_t)((int32_t)(last - first) / 0x50) * 0x50u;
}
RX_FUNCTION(0x00777a60, rx_world_body_record_move_backward)

/* 00775b10 (EAX = iterator {+0 container, +4 pointer}; stdcall out, offset; RET 8) -> EAX = out: out = the iterator advanced
 * by `offset` records, with the checked-iterator tests (container set, result within [begin, end] of the vector). */
rx_addr rx_world_body_record_iterator_advance(rx_addr iter, rx_addr out, int32_t offset) {
    const rx_addr container = U32(iter, 0);
    rx_addr ptr = U32(iter, 4);
    rx_addr vec;
    if (container == 0) {
        crt_invalid_parameter_noinfo();
        vec = 0;
    } else {
        vec = U32(container, 0);
    }
    ptr += (uint32_t)offset * 0x50u;
    if (ptr > U32(vec, 0x10)) {
        crt_invalid_parameter_noinfo();
    } else {
        vec = container == 0 ? 0 : U32(container, 0);
        if (ptr < U32(vec, 0xc)) crt_invalid_parameter_noinfo();
    }
    SU32(out, 4, ptr);
    SU32(out, 0, container);
    return out;
}
RX_FUNCTION(0x00775b10, rx_world_body_record_iterator_advance)

/* 00775800 BodyRecordVector_Insert (ECX = vector {+0 proxy, +0xc begin, +0x10 end, +0x14 capacity end}; stdcall value,
 * container, where; RET 0xc): vector::insert(where, value) of one record. Full: the capacity grows by half (or to size + 1;
 * 0 on overflow), the new buffer takes the value at the insert index and the old records either side (00776d50 twice), the
 * old buffer is freed. Room left: at the end, the value is copied to a stack temporary and filled in (00774ce0, 00777400);
 * in the middle, the last record is copied up, [where, last) moved up by one (00777a60) and the temporary assigned at
 * `where`. The temporary (ebp - 0x80) copies the value first (it may point into the vector).
 * Frame: push ebx; mov ebx, esp; sub esp, 8; and esp, -16; add esp, 4; push ebp (= the aligned ebp), SEH record and the
 * 0x90 locals, push esi, edi, eax. The value is copied at 0x48 bytes. ECX at the return is the security cookie. */
void rx_world_body_record_vector_insert(rx_addr vec, rx_addr value, rx_addr container, rx_addr where) {
    (void)container;
    const rx_addr entry = rx_sp();
    const rx_addr ebp = (entry - 12u) & ~15u;
    const rx_addr tmp = ebp - 0x80;
    rx_set_sp(ebp - 0xb0);
    uint32_t cap = U32(vec, 0xc) == 0 ? 0 : (uint32_t)((int32_t)(U32(vec, 0x14) - U32(vec, 0xc)) / 0x50);
    const rx_addr end = U32(vec, 0x10);
    const uint32_t size = (uint32_t)((int32_t)(end - U32(vec, 0xc)) / 0x50);
    if (0x3333333u - size < 1u) { fprintf(stderr, "rxcore: vector too long (std::length_error, 00851560)\n"); abort(); }
    const uint32_t new_size = size + 1;
    if (cap < new_size) {
        const uint32_t half = cap >> 1;
        cap = (0x3333333u - half >= cap) ? cap + half : 0;
        if (cap < new_size) cap = new_size;
        rx_addr buf;
        WORLD_CALL_SP(0, buf = rx_world_body_record_alloc(cap));
        const uint32_t index = (uint32_t)((int32_t)(where - U32(vec, 0xc)) / 0x50);
        WORLD_CALL_SP(0, rx_world_body_record_fill_end(index * 0x50u + buf, 1, value));
        WORLD_CALL_SP(8, rx_world_body_record_copy_uninit(buf, U32(vec, 0xc), where));
        WORLD_CALL_SP(8, rx_world_body_record_copy_uninit((index + 1u) * 0x50u + buf, where, U32(vec, 0x10)));
        const rx_addr old_begin = U32(vec, 0xc);
        const uint32_t old_count = (uint32_t)((int32_t)(U32(vec, 0x10) - old_begin) / 0x50) + 1u;
        if (old_begin != 0) WORLD_CALL_SP(4, rx_std_allocator_free(0, old_begin));
        SU32(vec, 0x14, cap * 0x50u + buf);
        SU32(vec, 0x10, old_count * 0x50u + buf);
        SU32(vec, 0xc, buf);
    } else {
        const uint32_t after = (uint32_t)((int32_t)(end - where) / 0x50);
        if (after < 1u) {
            WORLD_CALL_SP(0, rx_world_body_record_copy(tmp, value));
            WORLD_CALL_SP(8, rx_world_body_record_copy_uninit(where + 0x50, where, U32(vec, 0x10)));
            const rx_addr e = U32(vec, 0x10);
            const uint32_t have = (uint32_t)((int32_t)(e - where) / 0x50);
            WORLD_CALL_SP(0, rx_world_body_record_fill_end(e, 1u - have, tmp));
            SU32(vec, 0x10, U32(vec, 0x10) + 0x50);
            WORLD_CALL_SP(4, rx_world_body_record_assign_range(tmp, U32(vec, 0x10) - 0x50, where));
        } else {
            WORLD_CALL_SP(0, rx_world_body_record_copy(tmp, value));
            const rx_addr last = end - 0x50;
            rx_addr moved;
            WORLD_CALL_SP(8, moved = rx_world_body_record_copy_uninit(end, last, end));
            SU32(vec, 0x10, moved);
            WORLD_CALL_SP(0x10, rx_world_body_record_move_backward(where, last, end));
            WORLD_CALL_SP(4, rx_world_body_record_assign_range(tmp, where + 0x50, where));
        }
    }
    rx_set_sp(entry);
}
RX_FUNCTION(0x00775800, rx_world_body_record_vector_insert)

/* 00774c30 BodyRecordVector_InsertAt (ESI = vector; stdcall out iterator*, value, container, where; RET 0x10) -> EAX = out:
 * the insert of one record at the iterator `where`, then out = {container, begin + index * 0x50} (index of `where` before the
 * insert; 0 for an empty vector). The checked-iterator tests: begin <= end, container set and equal to the vector's proxy.
 * Frame: sub esp, 0x10; push ebx, ebp, edi (a temporary iterator {container, begin} sits at entry esp - 0xc). */
rx_addr rx_world_body_record_vector_insert_at(rx_addr vec, rx_addr out, rx_addr value, rx_addr container, rx_addr where) {
    const rx_addr entry = rx_sp();
    rx_set_sp(entry - 0x1c);
    const rx_addr begin = U32(vec, 0xc), end = U32(vec, 0x10);
    uint32_t offset;
    if ((int32_t)(end - begin) / 0x50 == 0) {
        offset = 0;
    } else {
        if (begin > end) crt_invalid_parameter_noinfo();
        if (container == 0 || container != U32(vec, 0)) crt_invalid_parameter_noinfo();
        offset = (uint32_t)((int32_t)(where - begin) / 0x50);
    }
    WORLD_CALL_SP(0xc, rx_world_body_record_vector_insert(vec, value, container, where));
    const rx_addr new_begin = U32(vec, 0xc);
    if (new_begin > U32(vec, 0x10)) crt_invalid_parameter_noinfo();
    const rx_addr iter = entry - 0xc;
    SU32(iter, 0, U32(vec, 0));
    SU32(iter, 4, new_begin);
    WORLD_CALL_SP(8, rx_world_body_record_iterator_advance(iter, out, (int32_t)offset));
    rx_set_sp(entry);
    return out;
}
RX_FUNCTION(0x00774c30, rx_world_body_record_vector_insert_at)

/* 00773d10 BodyRecordVector_PushBack (EAX = vector {+0 container, +0xc begin, +0x10 end, +0x14 capacity end}; stdcall record;
 * RET 4): a copy of the 0x50-byte record appended in place while below capacity (BodyRecord_FillN with n = 1; the original
 * zeroes the low byte of its own argument slot and passes that dword twice as the fill's allocator tag), else the
 * reallocating insert 00774c30 (ESI = vector). */
void rx_world_body_record_vector_push_back(rx_addr v, rx_addr rec) {
    const rx_addr args = rx_sp();
    world_frame f = world_frame_flat(4, 8);
    const rx_addr begin = U32(v, 0xc);
    const uint32_t cap = begin == 0 ? 0 : (uint32_t)((int32_t)(U32(v, 0x14) - begin) / 0x50);
    const rx_addr end = U32(v, 0x10);
    if ((uint32_t)((int32_t)(end - begin) / 0x50) < cap) {
        SU8(args, 4, 0);   /* the original zeroes the low byte of its argument slot: the fill's (unused) allocator tag */
        WORLD_CALL_SP(0x14, rx_world_body_record_fill_n(end, 1, rec));
        SU32(v, 0x10, end + 0x50);
    } else {
        if (begin > end) crt_invalid_parameter_noinfo();
        WORLD_CALL_SP(0x10, rx_world_body_record_vector_insert_at(v, f.esp + 0x10, rec, U32(v, 0), end));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x00773d10, rx_world_body_record_vector_push_back)

/* 0076e460 Dynamic_ContactResponse (EDI = generic object A; stdcall B, mean contact (point, normal); RET 8): the two-body
 * impulse between generic objects A and B (0076f440) with their material pair (A +0x24, B +0x24), on copies of both velocities;
 * the changes are added to each object's contact results +0x80 / +0x90 (the velocities themselves are not written). */
void rx_world_dynamic_contact_response(rx_addr A, rx_addr B, rx_addr contact_mean) {
    world_frame f = world_frame_enter(0xb8, 2, 1);
    const rx_addr sp = f.esp;
    const rx_addr wa = sp + 0x10, vb = sp + 0x20, va = sp + 0x30, wb = sp + 0x40, friction = sp + 0x50, inv_mass_b = sp + 0x60;
    const rx_addr restitution = sp + 0x70, inv_inertia_a = sp + 0x80, inv_inertia_b = sp + 0x90, inv_mass_a = sp + 0xa0;
    SU32(sp, 0xc, contact_mean);   /* [esp+4] before the pushes: the argument copy the original reads back */
    const rx_addr pair = material_pair(U32(A, 0x24), U32(B, 0x24));
    v4_st(va, v4_ld(A + 0x60));
    v4_st(wa, v4_ld(A + 0x70));
    v4_st(vb, v4_ld(B + 0x60));
    v4_st(wb, v4_ld(B + 0x70));
    v4_st(restitution, v4_splat(F32(pair, 0)));
    v4_st(friction, v4_splat(F32(pair, 4)));
    v4_st(inv_mass_b, v4_splat(rx_world_recip(B)));
    v4_st(inv_mass_a, v4_splat(rx_world_recip(A)));
    v4_st(inv_inertia_b, inverse_inertia(B));
    v4_st(inv_inertia_a, inverse_inertia(A));
    WORLD_CALL_SP(0x3c, rx_world_rigid_body_contact_impulse_pair(va, inv_mass_a, inv_inertia_a, A + 0x50, A + 0x40, wa, inv_mass_b,
                                                                 inv_inertia_b, B + 0x50, B + 0x40, wb, vb, restitution, friction,
                                                                 contact_mean, contact_mean + 0x10));
    v4_st(A + 0x80, v4_add(v4_sub(v4_ld(va), v4_ld(A + 0x60)), v4_ld(A + 0x80)));
    v4_st(A + 0x90, v4_add(v4_sub(v4_ld(wa), v4_ld(A + 0x70)), v4_ld(A + 0x90)));
    v4_st(B + 0x80, v4_add(v4_sub(v4_ld(vb), v4_ld(B + 0x60)), v4_ld(B + 0x80)));
    v4_st(B + 0x90, v4_add(v4_sub(v4_ld(wb), v4_ld(B + 0x70)), v4_ld(B + 0x90)));
    world_frame_leave(&f);
}
RX_FUNCTION(0x0076e460, rx_world_dynamic_contact_response)

/* the raw reciprocal inertia vector (1 / Ix, 1 / Iy, 1 / Iz, 1 / Iz) of a keyed body */
static rx_v4 reciprocal_inertia(rx_addr K) {
    const float x = phys_div(1.0f, F32(K, 0x10)), y = phys_div(1.0f, F32(K, 0x14)), z = phys_div(1.0f, F32(K, 0x18));
    return v4(x, y, z, z);
}

/* 0076e090 KeyedObj_ContactResponse (ECX = contact (point, normal), EDI = generic object A; stdcall world, keyed object K;
 * RET 8): the two-body impulse (0076f440) between A and the keyed / kinematic body K, with K's velocities taken (and changed)
 * in place in the kinematic pose arrays (world +0x1d10 / +0x5b90 + 0x50 * K +0x80). An integrating K (+0x84 bit 1) takes the
 * impulse directly. A sleeping one only when A moves faster than K's material +0xb0 (squared speed): a pushable K (bit 5) is
 * woken to state 0x23, another one through Kinematic_Activate (nothing when that refuses). A's velocity changes go to its
 * contact results +0x80 / +0x90 and its velocities +0x60 / +0x70. */
void rx_world_keyed_obj_contact_response(rx_addr contact_pt, rx_addr A, rx_addr world, rx_addr K) {
    world_frame f = world_frame_enter(0xa8, 2, 1);
    const rx_addr sp = f.esp;
    SU32(sp, 8, world);         /* the original keeps its argument copies in the frame */
    SU32(sp, 0xc, contact_pt);
    const rx_addr pair = material_pair(U32(A, 0x24), U32(K, 0x24));
    const uint32_t state = U32(K, 0x84);
    rx_addr va, wa;
    if (state & 2) {
        va = sp + 0x10; wa = sp + 0x20;
        const rx_addr restitution = sp + 0x30, inv_mass_k = sp + 0x40, inv_inertia_a = sp + 0x50, inv_inertia_k = sp + 0x60;
        const rx_addr inv_mass_a = sp + 0x70, friction = sp + 0x80;
        v4_st(va, v4_ld(A + 0x60));
        v4_st(wa, v4_ld(A + 0x70));
        v4_st(restitution, v4_splat(F32(pair, 0)));
        v4_st(friction, v4_splat(F32(pair, 4)));
        v4_st(inv_mass_k, v4_splat(rx_world_recip(K)));
        v4_st(inv_mass_a, v4_splat(rx_world_recip(A)));
        v4_st(inv_inertia_k, reciprocal_inertia(K));
        v4_st(inv_inertia_a, inverse_inertia(A));
        const rx_addr kpose = world + 0x50 * U32(K, 0x80);
        WORLD_CALL_SP(0x3c, rx_world_rigid_body_contact_impulse_pair(va, inv_mass_a, inv_inertia_a, A + 0x50, A + 0x40, wa,
                                                                     inv_mass_k, inv_inertia_k, K + 0x50, K + 0x40, kpose + 0x5b90,
                                                                     kpose + 0x1d10, restitution, friction, contact_pt,
                                                                     contact_pt + 0x10));
    } else {
        const float speed2 = v4_len2_xyz(v4_ld(A + 0x60));
        const rx_v4 mask = v4_cmplt(v4_ld(U32(K, 0x20) + 0xb0), v4_splat(speed2));
        v4_st(sp + 0x10, mask);
        if (!v4_all3(mask)) { world_frame_leave(&f); return; }
        if (state & 0x20) SU32(K, 0x84, 0x23);
        else {
            int ok;
            WORLD_CALL_SP(8, ok = rx_world_kinematic_activate(K, world, WORLD_ZERO4));
            if (!ok) { world_frame_leave(&f); return; }
        }
        va = sp + 0x20; wa = sp + 0x10;
        const rx_addr inv_mass_a = sp + 0x30, inv_mass_k = sp + 0x40, restitution = sp + 0x50, inv_inertia_a = sp + 0x60;
        const rx_addr friction = sp + 0x70, recip_a = sp + 0x80, inv_inertia_k = sp + 0x90;
        v4_st(va, v4_ld(A + 0x60));
        v4_st(wa, v4_ld(A + 0x70));
        v4_st(restitution, v4_splat(F32(pair, 0)));
        v4_st(friction, v4_splat(F32(pair, 4)));
        v4_st(inv_mass_k, v4_splat(rx_world_recip(K)));
        v4_st(inv_mass_a, v4_splat(rx_world_recip(A)));
        const rx_addr kpose = world + 0x50 * U32(K, 0x80);
        WORLD_CALL_SP(4, rx_world_vec3_reciprocal_of(A, recip_a));
        v4_st(inv_inertia_a, v4_mul(v4_splat(1.0f /* 0x931f24 */), v4_ld(recip_a)));
        WORLD_CALL_SP(4, rx_world_vec3_reciprocal_of(K, inv_inertia_k));
        WORLD_CALL_SP(0x3c, rx_world_rigid_body_contact_impulse_pair(va, inv_mass_a, inv_inertia_a, A + 0x50, A + 0x40, wa,
                                                                     inv_mass_k, inv_inertia_k, K + 0x50, K + 0x40, kpose + 0x5b90,
                                                                     kpose + 0x1d10, restitution, friction, contact_pt,
                                                                     contact_pt + 0x10));
    }
    const rx_v4 v = v4_ld(va), w = v4_ld(wa);
    v4_st(A + 0x80, v4_add(v4_sub(v, v4_ld(A + 0x60)), v4_ld(A + 0x80)));
    v4_st(A + 0x90, v4_add(v4_sub(w, v4_ld(A + 0x70)), v4_ld(A + 0x90)));
    v4_st(A + 0x60, v);
    v4_st(A + 0x70, w);
    world_frame_leave(&f);
}
RX_FUNCTION(0x0076e090, rx_world_keyed_obj_contact_response)

/* ---- the active kinematic body list (world +0x19d4 count, +0x19d8 bodies, at most 200) ---------------------------------- */

/* 0076a8b0 Kinematic_Deactivate (EDX = body; stdcall world; RET 4): removes the body from the active kinematic list (index
 * +0x80 = -1, state +0x84 = 0); the last body takes its slot (its index, and its pose / orientation records, 0x50 bytes each,
 * copied down); the body is cleared from the contact-handler list (+0xcc00) and its water state reset (+0x9a00 + 0x20 i = 0,
 * push direction = 0xd616a0). */
void rx_world_kinematic_deactivate(rx_addr body, rx_addr world) {
    world_frame f = world_frame_enter(0, 4, 1);
    const int32_t idx = I32(body, 0x80);
    SU32(f.esp, 0xc, (uint32_t)idx);
    SU32(body, 0x80, 0xffffffffu);
    SU32(body, 0x84, 0);
    if (idx >= 0) {
        const int32_t last = I32(world, 0x19d4) - 1;
        if (idx == last) SU32(world, 0x19d4, (uint32_t)last);
        else {
            const rx_addr moved = U32(world, 0x19d4 + 4u * U32(world, 0x19d4));   /* the last body (index count - 1) */
            SU32(moved, 0x80, (uint32_t)idx);
            SU32(world, 0x19d8 + 4u * (uint32_t)idx, moved);
            const uint32_t from = 0x50u * U32(world, 0x19d4), to = 0x50u * (uint32_t)idx;
            for (uint32_t b = 0; b < 0x50; b += 4) SU32(world, 0x1d00 + to + b, U32(world, 0x1cb0 + from + b));
            const uint32_t from2 = 0x50u * U32(world, 0x19d4);
            for (uint32_t b = 0; b < 0x50; b += 4) SU32(world, 0x5b80 + to + b, U32(world, 0x5b30 + from2 + b));
            SU32(world, 0x19d4, U32(world, 0x19d4) - 1);
        }
        WORLD_CALL_SP(4, rx_world_ptr_vector_clear_value(world + 0xcc00, body));
        SU32(world, ((uint32_t)idx + 0x4d0) << 5, 0);
        v4_st(world + ((uint32_t)idx << 5) + 0x9a10, v4_ld(0x00d616a0));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0076a8b0, rx_world_kinematic_deactivate)

/* the keyed-object record at an index (world +0x1994: records of 0xe0 bytes from +0x19a0, end +0x19a4), bounds-checked as the original */
static rx_addr keyed_record(rx_addr world, uint32_t index) {
    world_bounds_check(index, (uint32_t)((I32(world, 0x19a4) - I32(world, 0x19a0)) / 0xe0));
    return U32(world, 0x19a0) + index * 0xe0u;
}

/* 0076a9a0 (EAX = the old index, ESI = the physics world; stdcall the new index; RET 4): a keyed record moved from the old slot to the
 * new one (the keyed vector's swap-with-last removal, 00417830): the contact-handler list (+0xcc00, 007685f0) and the active kinematic
 * list (+0x19d8, count +0x19d4) point to the record's new address. Frame: push ebx / ebp / edi. */
void rx_world_keyed_record_moved(uint32_t old_index, rx_addr world, uint32_t new_index) {
    world_frame f = world_frame_flat(3, 0);
    const rx_addr from = keyed_record(world, old_index);
    const rx_addr to = keyed_record(world, new_index);
    WORLD_CALL_SP(8, rx_world_ptr_vector_replace_value(world + 0xcc00, from, to));
    for (int32_t i = 0; i < I32(world, 0x19d4); i++)
        if (U32(world, 0x19d8 + 4u * (uint32_t)i) == from) SU32(world, 0x19d8 + 4u * (uint32_t)i, to);
    world_frame_leave(&f);
}
RX_FUNCTION(0x0076a9a0, rx_world_keyed_record_moved)

/* 0076aa50 (ECX = the index, ESI = the physics world; plain ret): the keyed record at the index leaves the world's lists: cleared from the
 * contact-handler list (+0xcc00, 00768590); every slot of the active kinematic list holding it takes the last body (its pointer and its
 * pose / orientation records, 0x50 bytes each at +0x1d00 / +0x5b80) unless it is the last, the last slot's water state is reset
 * (+0x9a00 + 0x20 k = 0, push direction 0xd616a0), and the count drops. Unlike Kinematic_Deactivate the bodies' own index fields
 * (+0x80 / +0x84) are left as they are. Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 8; push ebx / edi (the record at
 * [esp + 0xc]). */
void rx_world_keyed_record_unlist(uint32_t index, rx_addr world) {
    world_frame f = world_frame_enter(8, 2, 1);
    const rx_addr rec = keyed_record(world, index);
    SU32(f.esp, 0xc, rec);
    WORLD_CALL_SP(4, rx_world_ptr_vector_clear_value(world + 0xcc00, rec));
    for (int32_t i = 0; i < I32(world, 0x19d4); i++) {
        if (U32(world, 0x19d8 + 4u * (uint32_t)i) != rec) continue;
        const int32_t last = I32(world, 0x19d4) - 1;
        if (i < last) {
            SU32(world, 0x19d8 + 4u * (uint32_t)i, U32(world, 0x19d8 + 4u * (uint32_t)last));
            const uint32_t from = 0x50u * (uint32_t)last, to = 0x50u * (uint32_t)i;
            for (uint32_t b = 0; b < 0x50; b += 16) v4_st(world + 0x1d00 + to + b, v4_ld(world + 0x1d00 + from + b));
            for (uint32_t b = 0; b < 0x50; b += 16) v4_st(world + 0x5b80 + to + b, v4_ld(world + 0x5b80 + from + b));
            SU32(world, ((uint32_t)last + 0x4d0) << 5, 0);
            v4_st(world + ((uint32_t)last << 5) + 0x9a10, v4_ld(0x00d616a0));
        }
        SU32(world, 0x19d4, U32(world, 0x19d4) - 1);
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0076aa50, rx_world_keyed_record_unlist)

/* 00770b00 Kinematic_Activate (ECX = keyed body; stdcall world, initial velocity; RET 8; AL = 1 when active afterwards): makes
 * the body an active kinematic one. Already active (+0x80 >= 0): 1. With the list full (200), the body 00768650 picks from the
 * contact-handler list is evicted first (Kinematic_Deactivate, its pose restored from +0x60 / +0x70 and sent to its render
 * object, its keyed broad-phase entry refreshed). Then, if there is room: the body's slot gets its pose (position +0x40,
 * velocity, inverse mass 1 / +0, orientation +0x50, zero rates, inverse inertia), its water state reset, index +0x80, state
 * 0x23, timer +0xa0 from the material +0x80, it joins the contact-handler list, and the count grows. */
int rx_world_kinematic_activate(rx_addr body, rx_addr world, rx_addr velocity) {
    if (I32(body, 0x80) >= 0) return 1;
    world_frame f = world_frame_enter(0x14, 3, 1);
    SU32(f.esp, 0x1c, body);
    if (I32(world, 0x19d4) >= 200) {
        const rx_addr victim = rx_world_handler_list_next_active(world + 0xcc00);
        SU32(f.esp, 0x18, victim);
        if (victim != 0) {
            WORLD_CALL_SP(4, rx_world_kinematic_deactivate(victim, world));
            v4_st(victim + 0x40, v4_ld(victim + 0x60));
            v4_st(victim + 0x50, v4_ld(victim + 0x70));
            const rx_addr ro = U32(victim, 0x34);
            WORLD_CTX(U32(U32(ro, 0), 0x78), &WCTX(.ecx = ro, .eax = victim + 0x40, .edx = victim + 0x50, .ebx = world, .esi = ro,
                                                    .edi = U32(ro, 0)), 0, victim + 0x40, victim + 0x50);
            const rx_addr keyed_begin = rx_world_keyed_object_at(world + 0x1994, 0);
            const uint32_t idx = (uint32_t)((int32_t)(victim - keyed_begin) / (int32_t)0xe0);
            world_keyed_refresh(idx, world + 0x1994, WCTX(.ebx = world, .edi = 0));
        }
    }
    const int32_t n = I32(world, 0x19d4);
    if (n >= 200) { world_frame_leave(&f); return 0; }
    const uint32_t k = (uint32_t)n;
    SU32(world, 0x19d8 + 4 * k, body);
    const rx_addr pose = world + 0x1d00 + 0x50 * k, orient = world + 0x5b80 + 0x50 * k;
    v4_st(pose + 0x20, v4_ld(WORLD_ZERO4));
    v4_st(pose + 0x30, v4_ld(WORLD_ZERO4));
    v4_st(pose, v4_ld(body + 0x40));
    v4_st(pose + 0x10, v4_ld(velocity));
    v4_st(pose + 0x40, v4_splat(phys_div(1.0f /* 0x931f24 */, F32(body, 0))));
    v4_st(orient + 0x20, v4_ld(WORLD_ZERO4));
    v4_st(orient + 0x30, v4_ld(WORLD_ZERO4));
    v4_st(orient, v4_ld(body + 0x50));
    v4_st(orient + 0x10, v4_ld(WORLD_ZERO4));
    v4_st(orient + 0x40, reciprocal_inertia(body));
    SU32(world, (k + 0x4d0) << 5, 0);
    v4_st(world + (k << 5) + 0x9a10, v4_ld(0x00d616a0));
    SU32(body, 0x80, k);
    SU32(body, 0x84, 0x23);
    v4_st(body + 0xa0, v4_ld(U32(body, 0x20) + 0x80));
    WORLD_CALL_SP(4, rx_world_ptr_vector_push_if_room(world + 0xcc00, body));
    SU32(world, 0x19d4, U32(world, 0x19d4) + 1);
    world_frame_leave(&f);
    return 1;
}
RX_FUNCTION(0x00770b00, rx_world_kinematic_activate)

/* the axis lock applied to two of a kinematic body's records, one lane group at a time as the original does */
static void lock_records(rx_addr a, rx_addr b, uint32_t bits) {
    for (uint32_t bit = 1; bit <= 4; bit <<= 1) {
        if (!(bits & bit)) continue;
        v4_st(a, world_apply_lock(v4_ld(a), bit, v4_ld(WORLD_ZERO4)));
        v4_st(b, world_apply_lock(v4_ld(b), bit, v4_ld(WORLD_ZERO4)));
    }
}
/* 00769d40 Kinematic_ApplyLinearLock (ECX = world, ESI = body; stdcall k; RET 4): the body's locked axes (+0xb4 bits 0..2)
 * zeroed in its kinematic velocity and accumulated force. */
void rx_world_kinematic_apply_linear_lock(rx_addr world, rx_addr body, uint32_t k) {
    if (U32(body, 0xb4) == 0) return;
    lock_records(world + 0x1d10 + 0x50 * k, world + 0x1d20 + 0x50 * k, U32(body, 0xb4) & 7);
}
RX_FUNCTION(0x00769d40, rx_world_kinematic_apply_linear_lock)
/* 00769e40 Kinematic_ApplyAngularLock (ECX = world, ESI = body; stdcall k; RET 4): bits 3..5 of +0xb4 on the angular velocity
 * and the torque. */
void rx_world_kinematic_apply_angular_lock(rx_addr world, rx_addr body, uint32_t k) {
    if (U32(body, 0xb4) == 0) return;
    lock_records(world + 0x5b90 + 0x50 * k, world + 0x5ba0 + 0x50 * k, (U32(body, 0xb4) >> 3) & 7);
}
RX_FUNCTION(0x00769e40, rx_world_kinematic_apply_angular_lock)

/* 00769f40 Vector_ApplyAxisLock (ECX = a 16-byte vector, EDX = the kinematic body; plain ret): the body's locked axes
 * (+0xb4 bits 0, 1, 2) replaced in the vector by the lanes of the zero vector 0xd61670 (bit 2 also sets lane 3, from the zero
 * vector's lane 2, as the original's shufps 0xa4 leaves it). No lock bits, nothing is touched. The same lane rules as
 * world_apply_lock, in place. */
void rx_world_vector_apply_axis_lock(rx_addr vec, rx_addr body) {
    const uint32_t lock = U32(body, 0xb4);
    if (lock == 0) return;
    if (lock & 1) v4_st(vec, world_apply_lock(v4_ld(vec), 1, v4_ld(WORLD_ZERO4)));
    if (U8(body, 0xb4) & 2) v4_st(vec, world_apply_lock(v4_ld(vec), 2, v4_ld(WORLD_ZERO4)));
    if (U8(body, 0xb4) & 4) v4_st(vec, world_apply_lock(v4_ld(vec), 4, v4_ld(WORLD_ZERO4)));
}
RX_FUNCTION(0x00769f40, rx_world_vector_apply_axis_lock)

/* 0076ee60 (ECX = kinematic body O, EDX = static object S; stdcall world, contact record (point at +0, normal at +0x10); RET 8):
 * one contact of a kinematic body against a static object: the rigid-body impulse 007705b0 on the body's kinematic velocity
 * records (world + 0x1d10 / world + 0x5b90 + 0x50 * O +0x80) with the material pair (O +0x24, S +0x20), against the contact's own
 * normal. No contact-result accumulation: the impulse changes the kinematic pose records in place (the collide-static loop adds
 * the poses itself). Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x54; push ebx, esi, edi. The material lookup is
 * 0084fe70 (pair index) on the table the static guard 00850010 builds; the reciprocal mass / inertia are 00767f80 and the
 * inline 1 / O +0x10..0x18. */
void rx_world_kinematic_static_contact_impulse(rx_addr O, rx_addr S, rx_addr world, rx_addr contact) {
    world_frame f = world_frame_enter(0x54, 3, 1);
    const rx_addr sp = f.esp;
    const rx_addr pair = material_pair(U32(O, 0x24), U32(S, 0x20));
    v4_st(sp + 4, v4_splat(F32(pair, 0)));           /* restitution, [esp + 4] */
    v4_st(sp + 0x24, v4_splat(F32(pair, 4)));        /* friction, [esp + 0x24] */
    v4_st(sp + 0x14, v4_splat(rx_world_recip(O)));   /* inverse mass, [esp + 0x14] */
    v4_st(sp + 0x34, reciprocal_inertia(O));         /* inverse inertia, [esp + 0x34] */
    const rx_addr kpose = world + 0x50 * U32(O, 0x80);
    WORLD_CALL_SP(0x20, rx_world_rigid_body_contact_impulse(kpose + 0x1d10, sp + 0x34, sp + 0x14, O + 0x50, O + 0x40,
                                                            kpose + 0x5b90, sp + 4, sp + 0x24, contact, contact + 0x10));
    world_frame_leave(&f);
}
RX_FUNCTION(0x0076ee60, rx_world_kinematic_static_contact_impulse)

/* 0076ea00 (ECX = contact (point at +0, normal at +0x10), EDX = keyed body K; stdcall world, kinematic body O; RET 8): one
 * contact of the kinematic body O against the keyed body K (the per-contact impulse of 0076cd00 Kinematic_CollideKeyed).
 * Inactive O (+0x80 < 0): nothing. Material pair (O +0x24, K +0x24). By K's state (+0x84):
 *  - bit 1 set (K integrates): the two-body impulse 0076f440 on both bodies' kinematic records (velocity world +0x1d10 + 0x50 k,
 *    angular velocity world +0x5b90 + 0x50 k), inverse masses 1 / [body +0] and the inline inverse inertias of +0x10..+0x18;
 *  - else: only when O's speed^2 exceeds all of the first three lanes of K's material +0xb0..+0xb8 (lane +0xbc is not tested):
 *    wake K (bit 5 sets K +0x84 = 0x23, else Kinematic_Activate on it, and O's activity is re-read), then both active: the same
 *    two-body impulse with the inverse inertias through 00767f90; only K active: a one-body impulse (007705b0) on K against the
 *    NEGATED normal; only O active: a one-body impulse on O with the contact's normal; activation refused and O inactive: nothing.
 * Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x84; push ebx, esi, edi. The pair's restitution / friction live at
 * [esp + 0x14] / [esp + 0x44] in the first branch and swapped in the second, as the original loads them. */
void rx_world_kinematic_keyed_contact_impulse(rx_addr contact, rx_addr K, rx_addr world, rx_addr O) {
    world_frame f = world_frame_enter(0x84, 3, 1);
    const rx_addr sp = f.esp;
    if (I32(O, 0x80) < 0) { world_frame_leave(&f); return; }
    const rx_addr pair = material_pair(U32(O, 0x24), U32(K, 0x24));
    const rx_addr l80 = sp + 0x14, l70 = sp + 0x24, l60 = sp + 0x34, l50 = sp + 0x44, l40 = sp + 0x54, l30 = sp + 0x64;
    const uint32_t kstate = U32(K, 0x84);
    if (kstate & 2) {
        v4_st(l80, v4_splat(F32(pair, 0)));             /* restitution */
        v4_st(l50, v4_splat(F32(pair, 4)));             /* friction */
        v4_st(l40, v4_splat(rx_world_recip(K)));        /* inverse mass K */
        v4_st(l70, v4_splat(rx_world_recip(O)));        /* inverse mass O */
        v4_st(l30, reciprocal_inertia(K));              /* inverse inertia K */
        v4_st(l60, reciprocal_inertia(O));              /* inverse inertia O */
        const rx_addr ko = world + 0x50 * U32(O, 0x80), kk = world + 0x50 * U32(K, 0x80);
        WORLD_CALL_SP(0x3c, rx_world_rigid_body_contact_impulse_pair(ko + 0x1d10, l70, l60, O + 0x50, O + 0x40,
                                                                    ko + 0x5b90, l40, l30, K + 0x50, K + 0x40,
                                                                    kk + 0x5b90, kk + 0x1d10, l80, l50, contact,
                                                                    contact + 0x10));
        world_frame_leave(&f);
        return;
    }
    const rx_addr ko = world + 0x50 * U32(O, 0x80);
    const float speed2 = v4_len2_xyz(v4_ld(ko + 0x1d10));
    const rx_v4 mask = v4_cmplt(v4_ld(U32(K, 0x20) + 0xb0), v4_splat(speed2));
    v4_st(l80, mask);
    if (!v4_all3(mask)) { world_frame_leave(&f); return; }
    int active_k;
    if (kstate & 0x20) { SU32(K, 0x84, 0x23); active_k = 1; }
    else { int r; WORLD_CALL_SP(8, r = rx_world_kinematic_activate(K, world, WORLD_ZERO4)); active_k = r; }
    const int active_o = I32(O, 0x80) >= 0;
    v4_st(l50, v4_splat(F32(pair, 0)));                 /* restitution */
    v4_st(l80, v4_splat(F32(pair, 4)));                 /* friction */
    if (!active_k) {
        if (!active_o) { world_frame_leave(&f); return; }
        v4_st(l70, v4_splat(rx_world_recip(O)));        /* inverse mass O */
        WORLD_CALL_SP(4, rx_world_vec3_reciprocal_of(O, l60));   /* inverse inertia O */
        WORLD_CALL_SP(0x20, rx_world_rigid_body_contact_impulse(ko + 0x1d10, l60, l70, O + 0x50, O + 0x40, ko + 0x5b90,
                                                                l50, l80, contact, contact + 0x10));
    } else if (!active_o) {
        v4_st(l70, v4_sub(v4_zero(), v4_ld(contact + 0x10)));    /* the negated normal */
        v4_st(l60, v4_splat(rx_world_recip(K)));        /* inverse mass K */
        WORLD_CALL_SP(4, rx_world_vec3_reciprocal_of(K, l40));   /* inverse inertia K */
        const rx_addr kk = world + 0x50 * U32(K, 0x80);
        WORLD_CALL_SP(0x20, rx_world_rigid_body_contact_impulse(kk + 0x1d10, l40, l60, K + 0x50, K + 0x40, kk + 0x5b90,
                                                                l50, l80, contact, l70));
    } else {
        v4_st(l70, v4_splat(rx_world_recip(K)));        /* inverse mass K */
        v4_st(l60, v4_splat(rx_world_recip(O)));        /* inverse mass O */
        WORLD_CALL_SP(4, rx_world_vec3_reciprocal_of(K, l40));   /* inverse inertia K */
        WORLD_CALL_SP(4, rx_world_vec3_reciprocal_of(O, l30));   /* inverse inertia O */
        const rx_addr kk = world + 0x50 * U32(K, 0x80);
        WORLD_CALL_SP(0x3c, rx_world_rigid_body_contact_impulse_pair(ko + 0x1d10, l60, l30, O + 0x50, O + 0x40,
                                                                    ko + 0x5b90, l70, l40, K + 0x50, K + 0x40,
                                                                    kk + 0x5b90, kk + 0x1d10, l50, l80, contact,
                                                                    contact + 0x10));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0076ea00, rx_world_kinematic_keyed_contact_impulse)

/* 0076dec0 DynObj_WaterContact (ECX = world; stdcall contact (point, normal); EDI = generic object A; RET 4): the buoyancy impulse
 * of one water contact on A: 007705b0 on copies of A's velocity (+0x60) and angular velocity (+0x70), with the material pair
 * (A +0x24, the water's world +0xccac), against the reversed contact normal. What the impulse changed is added to A's contact
 * results (+0x80 velocity, +0x90 angular velocity) and the new velocities replace A's.
 * Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0xa8; push ebx, esi. Locals from the ESP after the pushes (S): +0x10 the
 * angular velocity copy, +0x20 the velocity copy, +0x30 / +0x80 the restitution / friction (splat), +0x40 / +0x50 A's +0x40 /
 * +0x50 (centre / orientation), +0x60 the negated normal, +0x70 the inverse mass (splat), +0x90 the inverse inertia
 * (1 / Ix, 1 / Iy, 1 / Iz, 1 / Iz), +0xac the stack cookie. */
void rx_world_dynobj_water_contact(rx_addr world, rx_addr contact, rx_addr A) {
    world_frame f = world_frame_enter(0xa8, 2, 1);
    const rx_addr S = f.esp;
    const rx_addr angvel = S + 0x10, vel = S + 0x20, restitution = S + 0x30, centre = S + 0x40, orient = S + 0x50;
    const rx_addr normal = S + 0x60, inv_mass = S + 0x70, friction = S + 0x80, inv_inertia = S + 0x90;
    SU32(S, 0xac, U32(0, 0x009b45e4) ^ (S + 8));
    const rx_addr pair = material_pair(U32(A, 0x24), U32(world, 0xccac));
    v4_st(restitution, v4_splat(F32(pair, 0)));
    v4_st(friction, v4_splat(F32(pair, 4)));
    v4_st(normal, v4_sub(v4_zero(), v4_ld(contact + 0x10)));
    v4_st(centre, v4_ld(A + 0x40));
    v4_st(orient, v4_ld(A + 0x50));
    v4_st(vel, v4_ld(A + 0x60));
    v4_st(angvel, v4_ld(A + 0x70));
    v4_st(inv_mass, v4_splat(rx_world_recip(A)));
    v4_st(inv_inertia, reciprocal_inertia(A));
    WORLD_CALL_SP(0x20, rx_world_rigid_body_contact_impulse(vel, inv_inertia, inv_mass, orient, centre, angvel, restitution,
                                                            friction, contact, normal));
    const rx_v4 new_angvel = v4_ld(angvel), new_vel = v4_ld(vel);
    v4_st(A + 0x90, v4_add(v4_sub(new_angvel, v4_ld(A + 0x70)), v4_ld(A + 0x90)));
    v4_st(A + 0x80, v4_add(v4_sub(new_vel, v4_ld(A + 0x60)), v4_ld(A + 0x80)));
    v4_st(A + 0x60, new_vel);
    v4_st(A + 0x70, new_angvel);
    world_frame_leave(&f);
}
RX_FUNCTION(0x0076dec0, rx_world_dynobj_water_contact)

/* 0076e6c0 RiderBody_KeyedContactImpulse (ECX = contact (point, normal), EDX = keyed body K; stdcall world, rider body R; RET 8):
 * the two-body impulse (0076f440) between the rider body R (the 0x80-byte records at world +0x1980: velocity +0x60, angular
 * velocity +0x70) and the keyed body K, applied to K only: K's velocities are taken (and changed) in place in the kinematic pose
 * arrays (world +0x1d10 / +0x5b90 + 0x50 * K +0x80), R's are copies that are never written back. An integrating K (+0x84 bit 1)
 * takes the impulse directly. A sleeping one only when R moves faster than K's material +0xb0 (squared speed): a pushable K
 * (bit 5) is woken to state 0x23, another one through Kinematic_Activate (nothing when that refuses). The sibling for generic
 * objects (0076e090) writes the changes back; the inverse inertias here are the raw (1 / Ix, 1 / Iy, 1 / Iz, 1 / Iz).
 * Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0xa4; push ebx, esi, edi; the world / contact arguments are kept at
 * S + 0x18 / S + 0x1c. The two branches lay their locals out differently (as the original does). */
void rx_world_rider_keyed_contact_impulse(rx_addr contact, rx_addr K, rx_addr world, rx_addr R) {
    world_frame f = world_frame_enter(0xa4, 3, 1);
    const rx_addr S = f.esp;
    SU32(S, 0xac, U32(0, 0x009b45e4) ^ (S + 0xc));
    SU32(S, 0x18, world);
    SU32(S, 0x1c, contact);
    const rx_addr pair = material_pair(U32(R, 0x24), U32(K, 0x24));
    const uint32_t state = U32(K, 0x84);
    if (state & 2) {
        const rx_addr friction = S + 0x20, inv_inertia_k = S + 0x30, angvel_r = S + 0x40, inv_inertia_r = S + 0x50;
        const rx_addr inv_mass_k = S + 0x60, inv_mass_r = S + 0x70, vel_r = S + 0x80, restitution = S + 0x90;
        v4_st(vel_r, v4_ld(R + 0x60));
        v4_st(angvel_r, v4_ld(R + 0x70));
        v4_st(restitution, v4_splat(F32(pair, 0)));
        v4_st(friction, v4_splat(F32(pair, 4)));
        v4_st(inv_mass_k, v4_splat(rx_world_recip(K)));
        v4_st(inv_mass_r, v4_splat(rx_world_recip(R)));
        v4_st(inv_inertia_k, reciprocal_inertia(K));
        v4_st(inv_inertia_r, reciprocal_inertia(R));
        const rx_addr kpose = world + 0x50 * U32(K, 0x80);
        WORLD_CALL_SP(0x3c, rx_world_rigid_body_contact_impulse_pair(vel_r, inv_mass_r, inv_inertia_r, R + 0x50, R + 0x40, angvel_r,
                                                                     inv_mass_k, inv_inertia_k, K + 0x50, K + 0x40, kpose + 0x5b90,
                                                                     kpose + 0x1d10, restitution, friction, contact, contact + 0x10));
    } else {
        const float speed2 = v4_len2_xyz(v4_ld(R + 0x60));
        const rx_v4 mask = v4_cmplt(v4_ld(U32(K, 0x20) + 0xb0), v4_splat(speed2));
        v4_st(S + 0x20, mask);
        if (!v4_all3(mask)) { world_frame_leave(&f); return; }
        if (state & 0x20) SU32(K, 0x84, 0x23);
        else {
            int ok;
            WORLD_CALL_SP(8, ok = rx_world_kinematic_activate(K, world, WORLD_ZERO4));
            if (!ok) { world_frame_leave(&f); return; }
        }
        const rx_addr vel_r = S + 0x30, angvel_r = S + 0x40, inv_mass_k = S + 0x50, inv_mass_r = S + 0x60;
        const rx_addr restitution = S + 0x70, friction = S + 0x80, inv_inertia_k = S + 0x90, inv_inertia_r = S + 0x20;
        v4_st(vel_r, v4_ld(R + 0x60));
        v4_st(angvel_r, v4_ld(R + 0x70));
        v4_st(restitution, v4_splat(F32(pair, 0)));
        v4_st(friction, v4_splat(F32(pair, 4)));
        v4_st(inv_mass_k, v4_splat(rx_world_recip(K)));
        v4_st(inv_mass_r, v4_splat(rx_world_recip(R)));
        const rx_addr kpose = world + 0x50 * U32(K, 0x80);
        WORLD_CALL_SP(4, rx_world_vec3_reciprocal_of(K, inv_inertia_k));
        WORLD_CALL_SP(4, rx_world_vec3_reciprocal_of(R, inv_inertia_r));
        WORLD_CALL_SP(0x3c, rx_world_rigid_body_contact_impulse_pair(vel_r, inv_mass_r, inv_inertia_r, R + 0x50, R + 0x40, angvel_r,
                                                                     inv_mass_k, inv_inertia_k, K + 0x50, K + 0x40, kpose + 0x5b90,
                                                                     kpose + 0x1d10, restitution, friction, contact, contact + 0x10));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0076e6c0, rx_world_rider_keyed_contact_impulse)

/* 0076f2f0 Kinematic_TerrainContactImpulse (ECX=O; stack world, contact; RET8).
 * Terrain material0, restitution/friction from the real pair registry. Unlike
 * generic-object terrain response, velocity and angular velocity live in the
 * indexed world kinematic arrays and are changed in place. The inertia vector
 * is assembled with raw DIVSS (x,y,z,z), without the generic response's *1.
 * Frame: aligned sub54, saved EBX/ESI/EDI; contact saved at final ESP+c. */
void rx_world_kinematic_terrain_contact_impulse(rx_addr O, rx_addr world, rx_addr contact) {
    world_frame f = world_frame_enter(0x54, 3, 1);
    const rx_addr sp = f.esp;
    SU32(sp, 0xc, contact);
    /* The native registry constructor implements00850010/008501a0 and its
     * first-use guard; a loaded world normally already owns this registry. */
    (void)rx_phys_props_get();
    const rx_addr pair = material_pair(U32(O, 0x24), 0);
    v4_st(sp + 0x10, v4_splat(F32(pair, 0)));
    v4_st(sp + 0x30, v4_splat(F32(pair, 4)));
    v4_st(sp + 0x20, v4_splat(rx_world_recip(O)));
    v4_st(sp + 0x40, reciprocal_inertia(O));
    const rx_addr kpose = world + 0x50 * U32(O, 0x80);
    WORLD_CALL_SP(0x20, rx_world_rigid_body_contact_impulse(kpose + 0x1d10, sp + 0x40, sp + 0x20,
        O + 0x50, O + 0x40, kpose + 0x5b90, sp + 0x10, sp + 0x30, contact, contact + 0x10));
    world_frame_leave(&f);
}
RX_FUNCTION(0x0076f2f0, rx_world_kinematic_terrain_contact_impulse)

/* 0076dd50 Kinematic_WaterContact (ECX = kinematic body O; stdcall world, contact (point, normal); RET 8): the buoyancy impulse of
 * one water contact on the body: 007705b0 on its kinematic velocities in place (world +0x1d10 / +0x5b90 + 0x50 * O +0x80) with
 * the material pair (O +0x24, the water's world +0xccac), against the reversed contact normal. */
void rx_world_kinematic_water_contact(rx_addr O, rx_addr world, rx_addr contact_pt) {
    world_frame f = world_frame_enter(0x64, 3, 1);
    const rx_addr sp = f.esp, friction = sp + 0x10, restitution = sp + 0x20, inv_mass = sp + 0x30, normal = sp + 0x40;
    const rx_addr inv_inertia = sp + 0x50;
    SU32(sp, 0xc, contact_pt);
    const rx_addr pair = material_pair(U32(O, 0x24), U32(world, 0xccac));
    v4_st(restitution, v4_splat(F32(pair, 0)));
    v4_st(friction, v4_splat(F32(pair, 4)));
    v4_st(normal, v4_sub(v4_zero(), v4_ld(contact_pt + 0x10)));
    v4_st(inv_mass, v4_splat(rx_world_recip(O)));
    v4_st(inv_inertia, reciprocal_inertia(O));
    const rx_addr kpose = world + 0x50 * U32(O, 0x80);
    WORLD_CALL_SP(0x20, rx_world_rigid_body_contact_impulse(kpose + 0x1d10, inv_inertia, inv_mass, O + 0x50, O + 0x40, kpose + 0x5b90,
                                                            restitution, friction, contact_pt, normal));
    world_frame_leave(&f);
}
RX_FUNCTION(0x0076dd50, rx_world_kinematic_water_contact)

/* 00773110 World_BoxFreeOfDynamics (EDI = world, EDX = box min, EAX = box max; RET 0; AL = 1 when free): no generic dynamic
 * object overlaps the box in the broad phase except those whose shape has +0x10 bit 6. */
int rx_world_box_free_of_dynamics(rx_addr world, rx_addr box_min, rx_addr box_max) {
    world_frame f = world_frame_flat(3, 0);
    const rx_addr cursor = U32(world, 0x196c);
    world_broadphase_query_boxes(U32(cursor, 0x30), box_min, box_max, WCTX(.eax = box_max, .ebx = cursor, .edi = world));
    SU32(cursor, 0, 0);
    int blocked = 0;
    for (uint32_t idx = world_next_pair(U32(world, 0x196c)); idx != 0xffffffffu; idx = world_next_pair(U32(world, 0x196c))) {
        world_bounds_check(idx, world_obj_count(world));
        const rx_addr shape = U32(U32(world, 0x1960) + idx * 0xe0, 0x2c);
        if (!((U32(shape, 0x10) >> 6) & 1)) { blocked = 1; break; }
    }
    world_frame_leave(&f);
    return !blocked;
}
RX_FUNCTION(0x00773110, rx_world_box_free_of_dynamics)

/* the angular share of a contact on a body: to_world(I^-1 to_body(r x n)) (w masked off) */
static rx_v4 angular_term(rx_v4 r, rx_v4 n, rx_v4 q, rx_addr inv_inertia) {
    return to_world(v4_xyz(v4_mul(to_body(cross_xyz(r, n), q), v4_ld(inv_inertia))), q);
}

/* 0076f440 RigidBody_ContactImpulsePair (ECX = velocity of A; stdcall inverse mass A, inverse inertia A, orientation A, centre A,
 * angular velocity A, inverse mass B, inverse inertia B, orientation B, centre B, angular velocity B, velocity B, restitution,
 * friction, contact point, normal; RET 0x3c): the impulse of one contact between two rigid bodies. The relative velocity of the
 * contact point rel = (vB + wB x rB) - (vA + wA x rA), vn = n . rel. Approaching (vn < 0): the normal impulse
 * j = -(1 + e) vn / (1/mA + 1/mB + n.((I_A^-1 (rA x n)) x rA + (I_B^-1 (rB x n)) x rB)) along -n, plus friction mu |vt| / that
 * denominator along the tangential velocity vt = rel - n vn when |vt| > 0.01. Separating: the friction impulse alone (when
 * |vt| > 0.01). J goes to A (v += J / mA, w += I_A^-1 (rA x J)) and -J to B. The two branches sum their terms in different
 * orders (kept). */
void rx_world_rigid_body_contact_impulse_pair(rx_addr vel_a, rx_addr inv_mass_a, rx_addr inv_inertia_a, rx_addr orient_a,
                                              rx_addr centre_a, rx_addr angvel_a, rx_addr inv_mass_b, rx_addr inv_inertia_b,
                                              rx_addr orient_b, rx_addr centre_b, rx_addr angvel_b, rx_addr vel_b,
                                              rx_addr restitution, rx_addr friction, rx_addr point, rx_addr normal) {
    const rx_v4 qa = v4_ld(orient_a), qb = v4_ld(orient_b);
    const rx_v4 ra = v4_sub(v4_ld(point), v4_ld(centre_a)), rb = v4_sub(v4_ld(point), v4_ld(centre_b));
    const rx_v4 wa_world = to_world(v4_xyz(v4_ld(angvel_a)), qa), wb_world = to_world(v4_xyz(v4_ld(angvel_b)), qb);
    const rx_v4 va = v4_ld(vel_a);
    const rx_v4 vpa = v4_add(cross_xyz(wa_world, ra), va);
    const rx_v4 rel = v4_sub(v4_add(cross_xyz(wb_world, rb), v4_ld(vel_b)), vpa);
    const rx_v4 n = v4_ld(normal);
    const rx_v4 vn = v4_splat(phys_hsum3(v4_mul(n, rel)));
    const rx_v4 tangent = v4_sub(rel, v4_mul(n, vn));
    const rx_v4 tlen = v4_sqrt(v4_splat(v4_len2_xzy(tangent)));
    const int sliding = v4_all3(v4_cmplt(v4_splat(0.01f /* 0x9322ac */), tlen));
    rx_v4 J;
    const int approaching = v4_all3(v4_cmplt(vn, v4_ld(WORLD_ZERO4)));
    if (approaching) {
        const rx_v4 ang_a = angular_term(ra, n, qa, inv_inertia_a), ang_b = angular_term(rb, n, qb, inv_inertia_b);
        const float k = phys_hsum3(v4_mul(v4_add(cross_xyz(ang_a, ra), cross_xyz(ang_b, rb)), n));
        const rx_v4 denom = v4_add(v4_splat(k), v4_add(v4_ld(inv_mass_a), v4_ld(inv_mass_b)));
        const rx_v4 j = v4_div(v4_mul(v4_sub(v4_zero(), v4_add(v4_ld(restitution), v4_ld(0x00d61650))), vn), denom);
        const rx_v4 jn = v4_mul(v4_sub(v4_zero(), j), n);
        rx_v4 jt = v4_ld(WORLD_ZERO4);
        if (sliding) jt = v4_mul(v4_div(v4_mul(v4_ld(friction), tlen), denom), v4_div(tangent, tlen));
        J = v4_add(jt, jn);
        v4_st(vel_a, v4_add(v4_mul(v4_ld(inv_mass_a), J), va));
    } else {
        J = v4_ld(WORLD_ZERO4);
        if (sliding) {
            const rx_v4 ang_a = angular_term(ra, n, qa, inv_inertia_a), ang_b = angular_term(rb, n, qb, inv_inertia_b);
            const float k = phys_hsum3(v4_mul(v4_add(cross_xyz(ang_b, rb), cross_xyz(ang_a, ra)), n));
            const rx_v4 denom = v4_add(v4_splat(k), v4_add(v4_ld(inv_mass_b), v4_ld(inv_mass_a)));
            J = v4_mul(v4_div(v4_mul(v4_ld(friction), tlen), denom), v4_div(tangent, tlen));
        }
        v4_st(vel_a, v4_add(v4_mul(J, v4_ld(inv_mass_a)), va));
    }
    const rx_v4 neg_j = v4_sub(v4_zero(), J);
    v4_st(angvel_a, v4_add(v4_mul(to_body(cross_xyz(ra, J), qa), v4_ld(inv_inertia_a)), v4_ld(angvel_a)));
    if (approaching) v4_st(vel_b, v4_add(v4_mul(v4_ld(inv_mass_b), neg_j), v4_ld(vel_b)));
    else v4_st(vel_b, v4_add(v4_mul(neg_j, v4_ld(inv_mass_b)), v4_ld(vel_b)));
    v4_st(angvel_b, v4_add(v4_mul(to_body(cross_xyz(rb, neg_j), qb), v4_ld(inv_inertia_b)), v4_ld(angvel_b)));
}
RX_FUNCTION(0x0076f440, rx_world_rigid_body_contact_impulse_pair)
