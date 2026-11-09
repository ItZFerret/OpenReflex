// world_body.cpp -- oracle adapters for core/game/world_body.c (group rx_world).
#include "oracle.h"
extern "C" {
#include "../game/world.h"
}
using namespace reflex_port;
using rx_oracle::bits;

static void set_xmm(reflex_vm::Machine& m, int i, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    m.xmm[i].lo = a | (uint64_t(b) << 32); m.xmm[i].hi = c | (uint64_t(d) << 32);
}

// 00767f80 (EAX = float*; RET 0): xmm0 = (1 / *p, 0, 0, 0).
RX_PORT(0x00767f80, Recip_Eax, "rx_world", OUT_XMM0) {
    set_xmm(m, 0, bits(rx_world_recip(m.r[0])), 0, 0, 0);
    return m.ret(0);
}
// 00767f90 (ECX = object; stack out; RET 4): EAX = out.
RX_PORT(0x00767f90, Vec3_ReciprocalOf, "rx_world", OUT_EAX) {
    const uint32_t out = m.arg(0);
    rx_world_vec3_reciprocal_of(m.r[1], out);
    return m.ret(4, out);
}
// 00768460 (stdcall handle, pos, rot, vel, angvel; RET 0x14).
RX_PORT(0x00768460, DynObj_SetPose, "rx_world", 0) {
    rx_world_dynobj_set_pose(m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4));
    return m.ret(0x14);
}
// 007684c0 (ESI = out 1, EDI = out 2; stdcall handle, out 3; RET 8).
RX_PORT(0x007684c0, DynObj_TakeContactResults, "rx_world", 0) {
    rx_world_dynobj_take_contact_results(m.arg(0), m.r[6], m.r[7], m.arg(1));
    return m.ret(8);
}
// 00768550 (EAX = vector; stdcall value; RET 4).
RX_PORT(0x00768550, PtrVector_PushIfRoom, "rx_world", 0) {
    rx_world_ptr_vector_push_if_room(m.r[0], m.arg(0));
    return m.ret(4);
}
// 00768590 (EDI = vector; stdcall value; RET 4).
RX_PORT(0x00768590, PtrVector_ClearValue, "rx_world", 0) {
    rx_world_ptr_vector_clear_value(m.r[7], m.arg(0));
    return m.ret(4);
}
// 00768650 (ESI = list; plain ret): EAX = the next non-null entry or 0.
// ECX / EDX are live: ECX ends as the vector's size once the scan looked at an entry, EDX as the size when an entry is found.
RX_PORT(0x00768650, HandlerList_NextActive, "rx_world", OUT_EAX | OUT_ECX | OUT_EDX) {
    const uint32_t list = m.r[6];
    const int32_t size = (int32_t)((I32(list, 0x10) - I32(list, 0xc)) >> 2);
    const bool looked = I32(list, 0x18) < size;
    const uint32_t r = rx_world_handler_list_next_active(list);
    if (looked) m.r[1] = (uint32_t)size;
    if (r != 0) m.r[2] = (uint32_t)size;
    return m.ret(0, r);
}
// 00417070 (ESI = the keyed-object vector header, EDI = index; plain ret): EAX = the record.
RX_PORT(0x00417070, KeyedObjects_At, "rx_world", OUT_EAX) {
    const uint32_t r = rx_world_keyed_object_at(m.r[6], m.r[7]);
    return m.ret(0, r);
}
// 00769f40 (ECX = vector, EDX = kinematic body; plain ret).
RX_PORT(0x00769f40, Vector_ApplyAxisLock, "rx_world", 0) {
    rx_world_vector_apply_axis_lock(m.r[1], m.r[2]);
    return m.ret(0);
}
// ---- the BodyRecord vector insert (0x50-byte records) ----
// 004db640 (EAX = count; plain ret): EAX = the buffer.
RX_PORT(0x004db640, BodyRecord_Alloc, "rx_world", OUT_EAX) { return m.ret(0, rx_world_body_record_alloc(m.r[0])); }
// 00430030 (ECX = source, EAX = destination; plain ret): ECX ends as the source's last dword (+0x44).
RX_PORT(0x00430030, BodyRecord_Copy, "rx_world", OUT_ECX) {
    const uint32_t src = m.r[1];
    rx_world_body_record_copy(m.r[0], src);
    m.r[1] = U32(src, 0x44);
    return m.ret(0);
}
// 00774ce0 (EDX = value, ESI = count, EDI = destination; plain ret): EAX = destination + count * 0x50; ECX is the security
// cookie of 00777110's frame (ebp = this call's entry esp - 0x20).
RX_PORT(0x00774ce0, BodyRecord_FillEnd, "rx_world", OUT_EAX | OUT_ECX) {
    const uint32_t esp = m.r[4];
    const uint32_t eax = rx_world_body_record_fill_end(m.r[7], m.r[6], m.r[2]);
    m.r[1] = U32(0, 0x009b45e4) ^ (esp - 0x20);
    return m.ret(0, eax);
}
// 004dbf20 (cdecl first, last, destination, two unused words; plain ret): EAX = the destination end.
RX_PORT(0x004dbf20, BodyRecord_CopyConstruct, "rx_world", OUT_EAX) {
    return m.ret(0, rx_world_body_record_copy_construct(m.arg(0), m.arg(1), m.arg(2)));
}
// 00776d50 (EDX = destination; stdcall first, last; RET 8): EAX = the destination end.
RX_PORT(0x00776d50, BodyRecord_CopyUninit, "rx_world", OUT_EAX) {
    return m.ret(8, rx_world_body_record_copy_uninit(m.r[2], m.arg(0), m.arg(1)));
}
// 00777400 (ECX = source, EDX = end; cdecl destination; plain ret): EAX = end.
RX_PORT(0x00777400, BodyRecord_AssignRange, "rx_world", OUT_EAX | OUT_EDX) {
    return m.ret(0, rx_world_body_record_assign_range(m.r[1], m.r[2], m.arg(0)));
}
// 00778020 (ECX = last, EDX = first; cdecl destination end; plain ret): nothing live.
RX_PORT(0x00778020, BodyRecord_CopyBackward, "rx_world", 0) {
    rx_world_body_record_copy_backward(m.r[2], m.r[1], m.arg(0));
    return m.ret(0);
}
// 00777a60 (EAX = last, EDI = destination end; cdecl first and three tag words; plain ret): nothing live.
RX_PORT(0x00777a60, BodyRecord_MoveBackward, "rx_world", 0) {
    rx_world_body_record_move_backward(m.arg(0), m.r[0], m.r[7]);
    return m.ret(0);
}
// 00775b10 (EAX = iterator; stdcall out, offset; RET 8): EAX = out (EDX passes through).
RX_PORT(0x00775b10, BodyRecord_IteratorAdvance, "rx_world", OUT_EAX | OUT_EDX) {
    return m.ret(8, rx_world_body_record_iterator_advance(m.r[0], m.arg(0), (int32_t)m.arg(1)));
}
// 00775800 (ECX = vector; stdcall value, container, where; RET 0xc): ECX ends as the security cookie.
RX_PORT(0x00775800, BodyRecord_VectorInsert, "rx_world", OUT_ECX) {
    rx_world_body_record_vector_insert(m.r[1], m.arg(0), m.arg(1), m.arg(2));
    m.r[1] = U32(0, 0x009b45e4);
    return m.ret(0xc);
}
// 00774c30 (ESI = vector; stdcall out, value, container, where; RET 0x10): nothing live (EAX = out).
RX_PORT(0x00774c30, BodyRecord_VectorInsertAt, "rx_world", 0) {
    const uint32_t out = rx_world_body_record_vector_insert_at(m.r[6], m.arg(0), m.arg(1), m.arg(2), m.arg(3));
    return m.ret(0x10, out);
}
// 007686b0 (EDX = A; stdcall &world, B; RET 8): AL = the result; the rest of EAX is the upper bytes of B's velocity offset
// ((B +0x80 + 0x5d) * 0x50), as the original's address arithmetic leaves it.
RX_PORT(0x007686b0, Kinematic_IsSlower, "rx_world", OUT_EAX) {
    const uint32_t world = U32(m.arg(0), 0), A = m.r[2], B = m.arg(1);
    const int r = rx_world_kinematic_is_slower(world, A, B);
    const uint32_t offb = (U32(B, 0x80) + 0x5d) * 0x50;
    return m.ret(8, (offb & 0xffffff00u) | (r ? 1u : 0u));
}
// 007705b0 (ECX = velocity, EDX = inverse inertia; 8 stack args; RET 0x20): EAX = the angular velocity pointer (arg 3), EDX kept.
RX_PORT(0x007705b0, RigidBody_ContactImpulse, "rx_world", OUT_EAX | OUT_EDX) {
    rx_world_rigid_body_contact_impulse(m.r[1], m.r[2], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4), m.arg(5), m.arg(6), m.arg(7));
    return m.ret(0x20, m.arg(3));
}
// 00776920 (cdecl 6 args): EAX = result.
RX_PORT(0x00776920, PtrRemove, "rx_world", OUT_EAX) {
    return m.ret(0, rx_world_ptr_remove(m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4), m.arg(5)));
}
// 007774f0 (cdecl 7 args): EAX = result, ECX / EDX = dest.c / dest.p.
RX_PORT(0x007774f0, PtrRemoveCopy, "rx_world", OUT_EAX | OUT_ECX | OUT_EDX) {
    const uint32_t r = rx_world_ptr_remove_copy(m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4), m.arg(5), m.arg(6));
    m.r[1] = U32(r, 0); m.r[2] = U32(r, 4);
    return m.ret(0, r);
}
// 00777db0 (cdecl first, last, comparator).
RX_PORT(0x00777db0, BodySort_Insertion, "rx_world", 0) {
    rx_world_body_sort_insertion(m.arg(0), m.arg(1), m.arg(2));
    return m.ret(0);
}
// 00778190 (ESI = a, EDI = b; cdecl c, comparator) / 00777c70 (EAX = first; cdecl mid, last - 1, comparator).
RX_PORT(0x00778190, BodySort_Med3, "rx_world", 0) {
    rx_world_body_med3(m.r[6], m.r[7], m.arg(0), m.arg(1));
    return m.ret(0);
}
RX_PORT(0x00777c70, BodySort_Median, "rx_world", 0) {
    rx_world_body_median(m.r[0], m.arg(0), m.arg(1), m.arg(2));
    return m.ret(0);
}
// 00777580 (cdecl out, first, last, comparator): EAX = out.
RX_PORT(0x00777580, BodySort_Partition, "rx_world", OUT_EAX) {
    rx_world_body_partition(m.arg(0), m.arg(1), m.arg(2), m.arg(3));
    return m.ret(0, m.arg(0));
}
// 00778660 (EAX = hole; cdecl first, top, value, comparator).
RX_PORT(0x00778660, BodySort_PushHeap, "rx_world", 0) {
    rx_world_body_push_heap(int32_t(m.r[0]), m.arg(0), int32_t(m.arg(1)), m.arg(2), m.arg(3));
    return m.ret(0);
}
// 00778200 (EAX = hole; cdecl first, n, value, comparator): the n slot is overwritten with the start index.
RX_PORT(0x00778200, BodySort_AdjustHeap, "rx_world", 0) {
    rx_world_body_adjust_heap(int32_t(m.r[0]), m.arg(0), int32_t(m.arg(1)), m.arg(2), m.arg(3));
    return m.ret(0);
}
// 00777d20 / 00777d60 (EAX = last, EDI = first; cdecl comparator).
RX_PORT(0x00777d20, BodySort_MakeHeap, "rx_world", 0) {
    rx_world_body_make_heap(m.r[7], m.r[0], m.arg(0));
    return m.ret(0);
}
RX_PORT(0x00777d60, BodySort_SortHeap, "rx_world", 0) {
    rx_world_body_sort_heap(m.r[7], m.r[0], m.arg(0));
    return m.ret(0);
}
// 00776ef0 (cdecl first, last, ideal, comparator).
RX_PORT(0x00776ef0, BodySort_Loop, "rx_world", 0) {
    rx_world_body_sort_loop(m.arg(0), m.arg(1), int32_t(m.arg(2)), m.arg(3));
    return m.ret(0);
}
// 00777110 (cdecl dest, n, value): EAX = the last copy's result, ECX = the security cookie ([0x9b45e4] ^ EBP) the epilogue pops.
RX_PORT(0x00777110, BodyRecord_FillN, "rx_world", OUT_EAX | OUT_ECX) {
    const uint32_t ebp = m.r[4] - 4;
    const uint32_t eax = rx_world_body_record_fill_n(m.arg(0), m.arg(1), m.arg(2));
    m.r[1] = U32(0, 0x009b45e4) ^ ebp;
    return m.ret(0, eax);
}
// 00769bd0 (ECX = k, EDX = acc out, ESI = orientation rate out; stdcall world, material, vel out, ang acc out; RET 0x10):
// xmm2 = the angular velocity record as the original leaves it.
RX_PORT(0x00769bd0, Kinematic_Accelerations, "rx_world", OUT_XMM0 << 2) {
    const uint32_t k = m.r[1], world = m.arg(0);
    rx_world_kinematic_accelerations(k, m.r[2], m.r[6], world, m.arg(1), m.arg(2), m.arg(3));
    const uint32_t w = world + 0x5b90 + 0x50 * k;
    set_xmm(m, 2, U32(w, 0), U32(w, 4), U32(w, 8), U32(w, 0xc));
    return m.ret(0x10);
}
// 0076efc0 (ECX = static, EDI = object; stdcall mean contact; RET 4).
RX_PORT(0x0076efc0, Static_ContactResponse, "rx_world", 0) {
    rx_world_static_contact_response(m.r[1], m.r[7], m.arg(0));
    return m.ret(4);
}
// 00773d10 (EAX = vector; stdcall record; RET 4).
RX_PORT(0x00773d10, BodyRecordVector_PushBack, "rx_world", 0) {
    rx_world_body_record_vector_push_back(m.r[0], m.arg(0));
    return m.ret(4);
}
// 0076e460 (EDI = A; stdcall B, mean contact; RET 8).
RX_PORT(0x0076e460, Dynamic_ContactResponse, "rx_world", 0) {
    rx_world_dynamic_contact_response(m.r[7], m.arg(0), m.arg(1));
    return m.ret(8);
}
// 0076f160 (EDI = object A; stdcall mean contact; RET 4): the terrain pair (A +0x24, 0).
RX_PORT(0x0076f160, Terrain_ContactResponse, "rx_world", 0) {
    rx_world_terrain_contact_response(m.r[7], m.arg(0));
    return m.ret(4);
}
// 0076e090 (ECX = contact, EDI = A; stdcall world, K; RET 8).
RX_PORT(0x0076e090, KeyedObj_ContactResponse, "rx_world", 0) {
    rx_world_keyed_obj_contact_response(m.r[1], m.r[7], m.arg(0), m.arg(1));
    return m.ret(8);
}
// 0076a8b0 (EDX = body; stdcall world; RET 4). With the body active: EAX = index << 5, EDX = (index + 0x4d0) << 5 as the water
// state stores leave them; else EAX = its index (negative), EDX = the body.
RX_PORT(0x0076a8b0, Kinematic_Deactivate, "rx_world", OUT_EAX | OUT_EDX) {
    const uint32_t body = m.r[2];
    const int32_t idx = int32_t(U32(body, 0x80));
    rx_world_kinematic_deactivate(body, m.arg(0));
    if (idx >= 0) { m.r[2] = (uint32_t(idx) + 0x4d0) << 5; return m.ret(4, uint32_t(idx) << 5); }
    return m.ret(4, uint32_t(idx));
}
// 00770b00 (ECX = body; stdcall world, velocity; RET 8): AL = the result. EAX: already active: the entry EAX with AL = 1;
// activated: 1; no room: the count with AL = 0.
RX_PORT(0x00770b00, Kinematic_Activate, "rx_world", OUT_EAX) {
    const uint32_t body = m.r[1], world = m.arg(0), eax = m.r[0];
    const bool active = int32_t(U32(body, 0x80)) >= 0;
    const int r = rx_world_kinematic_activate(body, world, m.arg(1));
    if (active) return m.ret(8, (eax & 0xffffff00u) | 1u);
    return m.ret(8, r ? 1u : (U32(world, 0x19d4) & 0xffffff00u));
}
// 00769d40 / 00769e40 (ECX = world, ESI = body; stdcall k; RET 4).
RX_PORT(0x00769d40, Kinematic_ApplyLinearLock, "rx_world", 0) {
    rx_world_kinematic_apply_linear_lock(m.r[1], m.r[6], m.arg(0));
    return m.ret(4);
}
RX_PORT(0x00769e40, Kinematic_ApplyAngularLock, "rx_world", 0) {
    rx_world_kinematic_apply_angular_lock(m.r[1], m.r[6], m.arg(0));
    return m.ret(4);
}
// 0076f2f0 (ECX = body; world, contact; RET8); live result is memory only.
RX_PORT(0x0076f2f0, Kinematic_TerrainContactImpulse, "rx_KinematicTerrainContact", 0) {
    rx_world_kinematic_terrain_contact_impulse(m.r[1], m.arg(0), m.arg(1));
    return m.ret(8);
}
// 0076dd50 (ECX = body; stdcall world, contact; RET 8).
RX_PORT(0x0076dd50, Kinematic_WaterContact, "rx_world", 0) {
    rx_world_kinematic_water_contact(m.r[1], m.arg(0), m.arg(1));
    return m.ret(8);
}
// 0076ee60 (ECX = kinematic body, EDX = static; stdcall world, contact record (point, normal); RET 8).
RX_PORT(0x0076ee60, Kinematic_StaticContactImpulse, "rx_world", 0) {
    rx_world_kinematic_static_contact_impulse(m.r[1], m.r[2], m.arg(0), m.arg(1));
    return m.ret(8);
}
// 0076ea00 (ECX = contact, EDX = keyed body K; stdcall world, kinematic body O; RET 8).
RX_PORT(0x0076ea00, Kinematic_KeyedContactImpulse, "rx_world", 0) {
    rx_world_kinematic_keyed_contact_impulse(m.r[1], m.r[2], m.arg(0), m.arg(1));
    return m.ret(8);
}
// 0076dec0 (ECX = world, EDI = generic object A; stdcall contact; RET 4).
RX_PORT(0x0076dec0, DynObj_WaterContact, "rx_world", 0) {
    rx_world_dynobj_water_contact(m.r[1], m.arg(0), m.r[7]);
    return m.ret(4);
}
// 0076e6c0 (ECX = contact, EDX = keyed body K; stdcall world, rider body R; RET 8).
RX_PORT(0x0076e6c0, RiderBody_KeyedContactImpulse, "rx_world", 0) {
    rx_world_rider_keyed_contact_impulse(m.r[1], m.r[2], m.arg(0), m.arg(1));
    return m.ret(8);
}
// 00773110 (EDI = world, EDX = box min, EAX = box max; RET 0): EAX = 0 / 1.
RX_PORT(0x00773110, World_BoxFreeOfDynamics, "rx_world", OUT_EAX) {
    return m.ret(0, rx_world_box_free_of_dynamics(m.r[7], m.r[2], m.r[0]) ? 1u : 0u);
}
// 0076f440 (ECX = velocity A; 15 stack args; RET 0x3c): EAX = arg 5 (inverse mass B), EDX = arg 6 (inverse inertia B), ECX =
// arg 9 (angular velocity B): the pointers its last loads leave.
RX_PORT(0x0076f440, RigidBody_ContactImpulsePair, "rx_world", OUT_EAX | OUT_ECX | OUT_EDX) {
    uint32_t a[15];
    for (int i = 0; i < 15; i++) a[i] = m.arg(i);
    rx_world_rigid_body_contact_impulse_pair(m.r[1], a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11], a[12],
                                             a[13], a[14]);
    m.r[1] = a[9]; m.r[2] = a[6];
    return m.ret(0x3c, a[5]);
}
// 007685f0 (EDI = vector; stdcall old, new; RET 8) / 0076a9a0 (EAX = old index, ESI = world; stdcall new index; RET 4) /
// 0076aa50 (ECX = index, ESI = world): the keyed vector's swap-with-last removal (w-partdtor; core/oracle/wave_ctx_probe.cpp "release").
RX_PORT(0x007685f0, PtrVector_ReplaceValue, "rx_world", 0) { rx_world_ptr_vector_replace_value(m.r[7], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0076a9a0, KeyedRecord_Moved, "rx_world", 0) { rx_world_keyed_record_moved(m.r[0], m.r[6], m.arg(0)); return m.ret(4); }
RX_PORT(0x0076aa50, KeyedRecord_Unlist, "rx_world", 0) { rx_world_keyed_record_unlist(m.r[1], m.r[6]); return m.ret(0); }
