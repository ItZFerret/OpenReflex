#include "powercross_countdown.h"
#ifndef RX_ORACLE
#include "race_hud.h"
#include "career_completion_host.h"
#include "career_owned_results.h"
#include "race_results_table.h"
#endif
/* world_veh_entity.c -- the physics entity glue of the vehicle code (0x00402xxx-0x0041cxxx): PhysicsEntity virtuals (the
 * physics write-back into the entity matrix, type id, small getters), the physics handle -> body record lookup and the body's
 * kinematic velocity, the player lookup by controller id and the named-event dispatch along a handler chain.
 *
 * A physics handle (entity +0x158): +4 the physics world, +8 the index of its record in the world's keyed-object vector
 * (world +0x1994 header, records of 0xe0 bytes from +0x19a0, end +0x19a4; record +0x80 the kinematic body index, +0xb0
 * enabled). The kinematic velocities are at world +0x1d10 + 0x50 * k (core/notes/world.md). */
#include "world_veh.h"
#include <stdio.h>
#include <stdlib.h>
#include "crt.h"
#include "world_misc.h"
#include "world_veh_util.h"
#include "world_util.h"
#include "build_activity_util.h"
#include "world.h"
#include "world_veh_transfer.h"
#include "phys_body.h"
#include "build_alloc.h"
#include "loop_platform.h"

/* 004039a0 PhysicsEntity::vf30 (thiscall entity, position, quaternion; RET 8; ECX = 0 after rep movsd): the physics write-back:
 * the entity's world matrix (+0x30, 4x4) = the rotation of q (veh_quat_rows) with the translation row (p, 1). */
void rx_world_physics_entity_write_back(rx_addr entity, rx_addr pos, rx_addr q) {
    const veh_rot R = veh_quat_rows(F32(q, 0), F32(q, 4), F32(q, 8), F32(q, 0xc));
    const rx_addr m = entity + 0x30;
    for (uint32_t r = 0; r < 3; r++) {
        for (uint32_t c = 0; c < 3; c++) SF32(m, 0x10 * r + 4 * c, R.m[r][c]);
        SU32(m, 0x10 * r + 0xc, 0);
    }
    SU32(m, 0x30, U32(pos, 0)); SU32(m, 0x34, U32(pos, 4)); SU32(m, 0x38, U32(pos, 8));
    SF32(m, 0x3c, 1.0f /* 0x931f24 */);
}
RX_FUNCTION(0x004039a0, rx_world_physics_entity_write_back)

/* 004037c0 PhysicsEntity::vf28 (thiscall entity, position, quaternion; RET 8): the entity placed at the pose: the 4x4 matrix of the
 * quaternion (the original's own products and sums, in its order) with the translation row (p, 1), handed to vf16 (slot 0x40, set the
 * world matrix). Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x50; the matrix at F + 0x10 (w-partymodes: the Tag ball). */
void rx_physics_entity_place(rx_addr entity, rx_addr pos, rx_addr q) {
    const rx_addr sp0 = rx_sp();
    const rx_addr F = ((sp0 - 4) & ~15u) - 0x50;
    rx_set_sp(F);
    const float x = F32(q, 0), y = F32(q, 4), z = F32(q, 8), w = F32(q, 0xc);
    const float two = 2.0f /* 0x92e330 */, one = 1.0f /* 0x931f24 */;
    const float z2 = z * two, x2 = x * two, y2 = y * two;
    const float zz = z * z2, wx = w * x2, wz = w * z2, wy = w * y2, xx = x * x2, yy = y * y2, xy = y * x2, yz = z * y2, xz = z * x2;
    const rx_addr m = F + 0x10;
    SF32(m, 0x00, one - (zz + yy));
    SF32(m, 0x10, xy - wz);
    SF32(m, 0x04, xy + wz);
    SF32(m, 0x14, one - (zz + xx));
    SF32(m, 0x24, yz - wx);
    SF32(m, 0x28, one - (yy + xx));
    SU32(m, 0x30, U32(pos, 0));
    SU32(m, 0x34, U32(pos, 4));
    SF32(m, 0x08, xz - wy);
    SF32(m, 0x20, xz + wy);
    SU32(m, 0x38, U32(pos, 8));
    SF32(m, 0x0c, 0.0f);
    SF32(m, 0x18, yz + wx);
    SF32(m, 0x1c, 0.0f);
    SF32(m, 0x2c, 0.0f);
    SF32(m, 0x3c, one);
    const uint32_t a[1] = {m};
    (void)act_vcall(entity, 0x40, 1, a);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x004037c0, rx_physics_entity_place)

/* 00403940 PhysicsEntity::vf29 (thiscall entity, position, quaternion; RET 8): placed (vf28, slot 0x70), then the body's previous pose
 * (+0x60 the position, +0x70 the orientation: 16 bytes each) set to it too, so the body does not move back. Frame: push ebp; mov ebp,
 * esp; and esp, -16; push ecx / ebx / esi / edi. */
void rx_physics_entity_teleport(rx_addr entity, rx_addr pos, rx_addr q) {
    const rx_addr sp0 = rx_sp();
    const rx_addr F = ((sp0 - 4) & ~15u) - 16;
    rx_set_sp(F);
    const uint32_t a[2] = {pos, q};
    (void)act_vcall(entity, 0x70, 2, a);
    if (U32(entity, 0x158) != 0) {
        const rx_addr body = rx_world_physics_handle_body(U32(entity, 0x158));
        for (uint32_t k = 0; k < 16; k += 4) SU32(body + 0x60, k, U32(pos, k));
    }
    if (U32(entity, 0x158) != 0) {
        const rx_addr body = rx_world_physics_handle_body(U32(entity, 0x158));
        for (uint32_t k = 0; k < 16; k += 4) SU32(body + 0x70, k, U32(q, k));
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00403940, rx_physics_entity_teleport)

/* 00430560 VehicleDamagePhysicsEntity::vf30: the body is centred away from
 * the render origin by local offset+170. Rotate that XYZ offset by the given
 * quaternion, add the supplied position, and perform the base write-back.
 * Original aligned20 frame, temporary position at ESP. */
void rx_world_damage_physics_entity_write_back(rx_addr entity, rx_addr pos, rx_addr q) {
    world_frame f = world_frame_enter(0x20, 0, 1);
    v4_st(f.esp, v4_add(veh_rotate(v4_ld(q), v4_ld(entity + 0x170)), v4_ld(pos)));
    WORLD_CALL_SP(8, rx_world_physics_entity_write_back(entity, f.esp, q));
    world_frame_leave(&f);
}
RX_FUNCTION(0x00430560, rx_world_damage_physics_entity_write_back)

/* 00430670 VehicleDamagePhysicsEntity::vf31 (thiscall entity, position, quaternion; RET 8): the pose of a detached part: the base pose
 * (00403b20), then the position moved back by the body-centre offset (+0x170) rotated by the quaternion (vf30 adds it). Frame: push ebp;
 * and esp, -16; push ecx / ebx / esi / edi. */
void rx_world_damage_physics_entity_get_pose(rx_addr entity, rx_addr pos, rx_addr q) {
    world_frame f = world_frame_enter(4, 3, 1);
    WORLD_CALL_SP(8, rx_physics_entity_get_pose(entity, pos, q));
    v4_st(pos, v4_sub(v4_ld(pos), veh_rotate(v4_ld(q), v4_ld(entity + 0x170))));
    world_frame_leave(&f);
}
RX_FUNCTION(0x00430670, rx_world_damage_physics_entity_get_pose)

/* 0041cc60 PhysicsEntity::vf19 (thiscall entity, one argument; RET 4): the entity's audio emitter (+0x160) follows the entity: with no emitter
 * nothing. The emitter exists only for a prop whose data names a sound asset (PhysicsEntity::vf5 0041c8a0 builds it through the audio layer);
 * the product has no audio system (world_util.h AUDIO_BOUNDARY) and vf5 leaves +0x160 at 0, and the detached parts of a vehicle never have one,
 * so the emitter path (the emitter's own update 00678980 and the audio thread's queue 0080ae10 / 0080c280) is not reached. */
void rx_world_physics_entity_vf19(rx_addr entity, uint32_t arg) {
    (void)arg;
    if (U32(entity, 0x160) == 0) return;
    fprintf(stderr, "rxcore: PhysicsEntity::vf19 with an audio emitter (%08x): the audio layer is not part of the product\n", U32(entity, 0x160));
    abort();
}
RX_FUNCTION(0x0041cc60, rx_world_physics_entity_vf19)

void rx_replay_physics_entity_deactivate(rx_addr entity);   /* 0041ce60 (ESI = entity), replay_damage.c */
/* 0041cbe0 PhysicsEntity::vf2 (ECX = entity; plain RET): the entity disabled: with the body flag (+0x15c) and a body handle the body is taken out
 * of the world and its enabled byte cleared (the same code as 0041ce60); the audio emitter (+0x160) is stopped (not present in the product, see vf19). */
void rx_world_physics_entity_disable(rx_addr entity) {
    ACT_FRAME(F, 0, 2);
    if (U8(entity, 0x15c) != 0 && U32(entity, 0x158) != 0) ACT_CALL(0, rx_replay_physics_entity_deactivate(entity));
    if (U32(entity, 0x160) != 0) { fprintf(stderr, "rxcore: PhysicsEntity::vf2 with an audio emitter (%08x): the audio layer is not part of the product\n", U32(entity, 0x160)); abort(); }
    ACT_END(F);
}
RX_FUNCTION(0x0041cbe0, rx_world_physics_entity_disable)

/* 00403b90 PhysicsEntity::vf12 / 00403ba0 (thiscall): the getters of +0x148 / +0x14c. */
uint32_t rx_world_physics_entity_get148(rx_addr entity) { return U32(entity, 0x148); }
RX_FUNCTION(0x00403b90, rx_world_physics_entity_get148)
uint32_t rx_world_physics_entity_get14c(rx_addr entity) { return U32(entity, 0x14c); }
RX_FUNCTION(0x00403ba0, rx_world_physics_entity_get14c)

/* 0041cce0 PhysicsEntity::vf22: the type id 0x1029. */
uint32_t rx_world_physics_entity_type_id(void) { return 0x1029; }
RX_FUNCTION(0x0041cce0, rx_world_physics_entity_type_id)

/* 00402090 Rainbow::EventHandler::vf6 (thiscall): +4 == 1. */
uint32_t rx_world_event_handler_is_state1(rx_addr self) { return U32(self, 4) == 1; }
RX_FUNCTION(0x00402090, rx_world_event_handler_is_state1)

/* 00418f30 PhysicsHandle_Body (EAX = handle): the handle's keyed-object record, bounds-checked against the vector size
 * ((end - begin) / 0xe0, a signed division as the original). */
rx_addr rx_world_physics_handle_body(rx_addr handle) {
    const rx_addr world = U32(handle, 4);
    const uint32_t index = U32(handle, 8);
    const int32_t count = (I32(world, 0x19a4) - I32(world, 0x19a0)) / 0xe0;
    if (index >= (uint32_t)count) crt_invalid_parameter_noinfo();
    return index * 0xe0 + U32(world + 0x1994, 0xc);
}
RX_FUNCTION(0x00418f30, rx_world_physics_handle_body)

/* 0041cea0 (ESI = entity, EDI = out): the entity's kinematic velocity: with a handle whose record is enabled (+0xb0) and has a
 * kinematic body (+0x80 >= 0), the body's velocity world +0x1d10 + 0x50 k; else the writable zero vector. */
void rx_world_physics_entity_velocity(rx_addr entity, rx_addr out) {
    world_frame f = world_frame_enter(0x10, 0, 1);
    if (U32(entity, 0x158) != 0 && U8(rx_world_physics_handle_body(U32(entity, 0x158)), 0xb0) != 0 && U32(entity, 0x158) != 0) {
        const int32_t k = I32(rx_world_physics_handle_body(U32(entity, 0x158)), 0x80);
        if (k >= 0) {
            v4_st(out, v4_ld(U32(U32(entity, 0x158), 4) + 0x1d10 + 0x50 * (uint32_t)k));
            world_frame_leave(&f);
            return;
        }
    }
    v4_st(out, v4_ld(WORLD_ZERO4));
    world_frame_leave(&f);
}
RX_FUNCTION(0x0041cea0, rx_world_physics_entity_velocity)

/* 0041cf30 (thiscall entity, stdcall out; RET 4): *out = |velocity| (0041cea0 into a stack vector; |v|^2 summed (x + z) + y). */
void rx_world_physics_entity_speed(rx_addr entity, rx_addr out) {
    world_frame f = world_frame_enter(0x24, 3, 1);
    const rx_addr v = f.esp + 0x10;
    WORLD_CALL_SP(0, rx_world_physics_entity_velocity(entity, v));
    SF32(out, 0, v4_len_xzy(v4_ld(v)));
    world_frame_leave(&f);
}
RX_FUNCTION(0x0041cf30, rx_world_physics_entity_speed)

/* 00402dc0 (EDX = list: pointers, count +8): the first entry whose controller id (+0x44a8) is the active controller's (the
 * input manager [0xa95844]: slot +4, ids from +0x1c), else 0. */
rx_addr rx_world_find_by_controller(rx_addr list) {
    const uint32_t n = U32(list, 8);
    for (uint32_t i = 0; i < n; i++) {
        const rx_addr e = U32(list, 4 * i);
        if (e == 0) continue;
        const rx_addr input = U32(0, 0x00a95844);
        if (U32(input, 4 * U32(input, 4) + 0x1c) == U32(e, 0x44a8)) return e;
    }
    return 0;
}
RX_FUNCTION(0x00402dc0, rx_world_find_by_controller)

/* 004020c0 (EAX = handler chain, EDI = event name, stdcall a, b; RET 8): the named event to the first handler and on along the
 * chain (+4 next) while each next handler has it: 00593220 (stdcall map, name: the handler's callback for the name, or 0) and
 * 00592d80 (EAX = that callback, stdcall a, b). EAX = 0 after. */
void rx_world_event_dispatch(rx_addr chain, rx_addr name, uint32_t a, uint32_t b) {
#ifndef RX_ORACLE
    rx_powercross_countdown_activity_lifecycle(chain,name,a,b);
#endif
    world_frame f = veh_frame_align8(2);   /* push ebp; mov ebp, esp; and esp, -8; push ebx; push esi */
    rx_addr node = chain;
    uint32_t cb;
    WORLD_CALL_SP(8, cb = rx_world_registry_find(node, name));
    WORLD_CALL_SP(8, rx_world_handler_list_call(cb, a, b));
    for (rx_addr next = U32(node, 4); next != 0; next = U32(node, 4)) {
        uint32_t has;
        WORLD_CALL_SP(8, has = rx_world_registry_find(next, name));
        if (has == 0) break;
        node = next;
        WORLD_CALL_SP(8, cb = rx_world_registry_find(node, name));
        WORLD_CALL_SP(8, rx_world_handler_list_call(cb, a, b));
    }
    world_frame_leave(&f);
#ifndef RX_ORACLE
    /* Explicit host observation after the canonical listeners, including when
     * the registered event has zero guest listeners. Never completes gameplay. */
    rx_career_owned_results_dispatch(chain, name, a, b);
    rx_career_completion_observe(chain, name, a, b);
    rx_race_hud_observe(chain, name, a, b);   /* the HUD components' event copies (race_hud.c); host copies only */
    rx_results_page_observe(chain, name, a, b);   /* the results page's routing (race_results_table.c, w-resultsrng) */
#endif
}
RX_FUNCTION(0x004020c0, rx_world_event_dispatch)

/* ---- PhysicsEntity's destruction and the removal of its keyed body ------------------------------------------------------------------
 * (2026-10-27, w-partdtor: a vehicle damage part entity (VehicleDamagePhysicsEntity, vtable 0x93ccac) is deleted through vf0 when its entry
 * of the damage manager is released (00479dd0, build_activity_clear.c): the player's vehicle clear 0054a500 -> 0047bbe0.) */

/* 00417c80 (ESI = destination, EDI = source; EAX = destination): a keyed-object record (0xe0 bytes, world +0x1994) assigned: the floats
 * +0 / +0x10 / +0x14 / +0x18 through the x87 (fld / fstp), +0x20 / +0x24, the collision shape reference +0x2c (the old one released: vf0(1)
 * at its last reference; the new one referenced), +0x30 .. +0x38, the 16-byte blocks +0x40 .. +0x70, +0x80 / +0x84 (the kinematic slot),
 * +0x90 / +0xa0, the bytes +0xb0 .. +0xb2, +0xb4 / +0xb8, +0xc0, the bytes +0xd0 / +0xd1. */
rx_addr rx_world_keyed_record_assign(rx_addr d, rx_addr s) {
    SU32(d, 0, world_x87_pass(U32(s, 0)));
    for (uint32_t o = 0x10; o <= 0x18; o += 4) SU32(d, o, world_x87_pass(U32(s, o)));
    SU32(d, 0x20, U32(s, 0x20)); SU32(d, 0x24, U32(s, 0x24));
    const rx_addr old = U32(d, 0x2c);
    if (U32(s, 0x2c) != old) {
        if (old != 0) {
            SU32(old, 4, U32(old, 4) - 1);
            if (U32(old, 4) == 0 && U32(d, 0x2c) != 0) { const uint32_t one = 1; act_vcall(U32(d, 0x2c), 0, 1, &one); }
        }
        SU32(d, 0x2c, U32(s, 0x2c));
        if (U32(s, 0x2c) != 0) SU32(U32(s, 0x2c), 4, U32(U32(s, 0x2c), 4) + 1);
    }
    SU32(d, 0x30, U32(s, 0x30)); SU32(d, 0x34, U32(s, 0x34)); SU32(d, 0x38, U32(s, 0x38));
    for (uint32_t o = 0x40; o <= 0x70; o += 0x10) v4_st(d + o, v4_ld(s + o));
    SU32(d, 0x80, U32(s, 0x80)); SU32(d, 0x84, U32(s, 0x84));
    v4_st(d + 0x90, v4_ld(s + 0x90)); v4_st(d + 0xa0, v4_ld(s + 0xa0));
    SU8(d, 0xb0, U8(s, 0xb0)); SU8(d, 0xb1, U8(s, 0xb1)); SU8(d, 0xb2, U8(s, 0xb2));
    SU32(d, 0xb4, U32(s, 0xb4)); SU32(d, 0xb8, U32(s, 0xb8));
    v4_st(d + 0xc0, v4_ld(s + 0xc0));
    SU8(d, 0xd0, U8(s, 0xd0)); SU8(d, 0xd1, U8(s, 0xd1));
    return d;
}
RX_FUNCTION(0x00417c80, rx_world_keyed_record_assign)

/* 00417830 (stdcall the keyed vector, index, source record; RET 0xc): the source record assigned to the record at the index (00417c80),
 * its broadphase box updated (0042b000), and its tracking entry (+0x30: {refs, world, index}) re-pointed: the world's lists told of the
 * record's new address (0076a9a0) when the entry is this vector's world's with another index; an entry of another world first leaves that
 * world's lists (0076aa50); then +4 = the vector's world (+0x1c), +8 = the index. Frame: push ebp; mov ebp, esp; and esp, -8; push ecx;
 * push ebx / esi / edi (the world at [esp + 0xc]). */
void rx_world_keyed_records_move_slot(rx_addr vec, uint32_t index, rx_addr src) {
    world_frame f; f.entry = rx_sp(); f.ebp = f.entry - 4; f.esp = (f.ebp & ~7u) - 16u; rx_set_sp(f.esp);
    world_bounds_check(index, (uint32_t)((I32(vec, 0x10) - I32(vec, 0xc)) / 0xe0));
    const rx_addr dst = index * 0xe0u + U32(vec, 0xc);
    WORLD_CALL_SP(0, (void)rx_world_keyed_record_assign(dst, src));
    WORLD_CALL_SP(0, phys_body_update_grid(vec, (int32_t)index));
    const rx_addr world = U32(vec, 0x1c);
    const rx_addr t = U32(src, 0x30);
    SU32(f.esp, 0xc, world);
    if (world != 0 && world == U32(t, 4) && index != U32(t, 8)) WORLD_CALL_SP(4, rx_world_keyed_record_moved(U32(t, 8), world, index));
    if (U32(t, 4) != 0 && world != U32(t, 4)) WORLD_CALL_SP(0, rx_world_keyed_record_unlist(U32(t, 8), U32(t, 4)));
    SU32(t, 8, index);
    SU32(t, 4, world);
    world_frame_leave(&f);
}
RX_FUNCTION(0x00417830, rx_world_keyed_records_move_slot)

/* 00416fe0 (ESI = the keyed vector; stack index; RET 4): the record at the index replaced by the last (00417830) unless it is the last, the
 * last one's broadphase entry removed (008825f0), the last record destroyed (0042c080) and dropped. Frame: push ebx / edi. */
void rx_world_keyed_records_remove(rx_addr vec, uint32_t index) {
    world_frame f = world_frame_flat(2, 0);
    const uint32_t count = (uint32_t)((I32(vec, 0x10) - I32(vec, 0xc)) / 0xe0);
    const uint32_t last = count - 1;
    if (index != last) {
        world_bounds_check(last, count);
        WORLD_PUSHED(0x00417023, vec, index, last * 0xe0u + U32(vec, 0xc));
        WORLD_CALL_SP(0xc, rx_world_keyed_records_move_slot(vec, index, last * 0xe0u + U32(vec, 0xc)));
    }
    WORLD_CALL_SP(0, rx_grid_slot_clear(last, U32(U32(vec, 0x18), 0x30), 0));
    const rx_addr end = U32(vec, 0x10);
    if ((I32(vec, 0x10) - I32(vec, 0xc)) / 0xe0 != 0) {
        void (*destroy)(rx_addr, rx_addr) = RX_FN(0x0042c080, void (*)(rx_addr, rx_addr));
        if (!destroy) act_unreached(0x0042c080, "the records' destructors");
        WORLD_PUSHED(0x0041705e, end, index);
        WORLD_CALL_SP(8, destroy(end - 0xe0, end));
        SU32(vec, 0x10, U32(vec, 0x10) - 0xe0);
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x00416fe0, rx_world_keyed_records_remove)

/* 00402360 (stdcall the physics world, EDI = the index; RET 4): a keyed body leaves the world: taken out of the active kinematic list
 * (Kinematic_Deactivate 0076a8b0) and its record removed from the keyed vector (00416fe0). Frame: push ebx / esi. */
void rx_world_keyed_body_remove(rx_addr world, uint32_t index) {
    world_frame f = world_frame_flat(2, 0);
    world_bounds_check(index, (uint32_t)((I32(world, 0x19a4) - I32(world, 0x19a0)) / 0xe0));
    const rx_addr rec = index * 0xe0u + U32(world + 0x1994, 0xc);
    WORLD_PUSHED(0x004023a6, world);
    WORLD_CALL_SP(4, rx_world_kinematic_deactivate(rec, world));
    WORLD_PUSHED(0x004023ac, index);
    WORLD_CALL_SP(4, rx_world_keyed_records_remove(world + 0x1994, index));
    world_frame_leave(&f);
}
RX_FUNCTION(0x00402360, rx_world_keyed_body_remove)

/* the tracking entry {refs, world, index} of a physics object handle released: at the last reference its body leaves the world (00402360)
 * and the entry is freed (ReflexAlignedFree) */
static void tracking_entry_free(rx_addr t, uint32_t ret) {
    WORLD_PUSHED(ret, U32(t, 4));
    WORLD_CALL_SP(4, rx_world_keyed_body_remove(U32(t, 4), U32(t, 8)));
    WORLD_PUSHED(ret + 6, t);
    WORLD_CALL_SP(4, rx_reflex_aligned_free(t));
}

/* 00416f90 IntrusiveHandle<ObjectMonitor<VPhysicsObject>>::vf1 (ECX = the handle {vtable, +4 the tracking entry}; plain ret): the entry
 * loses a reference (the last one: its body leaves the world, the entry is freed) and the handle is cleared. Frame: push ebp / esi
 * (+ ebx / edi at the last reference). */
void rx_physics_object_handle_reset(rx_addr h) {
    world_frame f = world_frame_flat(2, 0);
    const rx_addr t = U32(h, 4);
    if (t != 0) {
        SU32(t, 0, U32(t, 0) - 1);
        if (U32(t, 0) == 0 && U32(h, 4) != 0) {
            rx_set_sp(f.esp - 8);                                           /* push ebx; push edi */
            tracking_entry_free(U32(h, 4), 0x00416fb7);
            rx_set_sp(f.esp);
        }
    }
    SU32(h, 4, 0);
    world_frame_leave(&f);
}
RX_FUNCTION(0x00416f90, rx_physics_object_handle_reset)

/* 0041c7b0 (ECX = the entity; plain ret): PhysicsEntity's destructor: the vtables +0 0x93743c / +0x90 0x9374c4; the audio emitter (+0x160,
 * 0041c850; never made in the product, see vf19) released and cleared; the body handle (+0x154 vtable 0x937510, +0x158 the tracking entry)
 * released (the last reference removes the body from its world, 00402360, and frees the entry); the ModelEntity / Entity vtables (+0x90
 * 0x93708c, +0 0x95a2e4) and both entity registries ([0xa95824] / [0xa95828] vf1, null in the product: anim_entity_dtors.c) told.
 * Frame: push ebx / esi / edi. */
void rx_physics_entity_destroy(rx_addr e) {
    world_frame f = world_frame_flat(3, 0);
    SU32(e, 0, 0x0093743c);
    SU32(e, 0x90, 0x009374c4);
    if (U32(e, 0x160) != 0) {
#ifdef RX_ORACLE
        rx_regs in = world_regs(), out;
        in.r[WORLD_EAX] = U32(e, 0x160); in.r[WORLD_ESI] = e;
        rx_call_original_ex(0x0041c850, &in, &out, 0, 0);                    /* AUDIO BOUNDARY: the emitter's stop and release */
#else
        fprintf(stderr, "rxcore: PhysicsEntity destructor with an audio emitter (%08x): the audio layer is not part of the product\n", U32(e, 0x160));
        abort();
#endif
        SU32(e, 0x160, 0);
    }
    SU32(e, 0x154, 0x00937510);
    const rx_addr t = U32(e, 0x158);
    if (t != 0) {
        SU32(t, 0, U32(t, 0) - 1);
        if (U32(t, 0) == 0 && U32(e, 0x158) != 0) tracking_entry_free(U32(e, 0x158), 0x0041c80d);
    }
    SU32(e, 0x90, 0x0093708c);
    SU32(e, 0, 0x0095a2e4);
    for (int k = 0; k < 2; k++) {
        const rx_addr registry = U32(0, k ? 0x00a95828 : 0x00a95824);
        if (registry != 0) { const uint32_t arg = e; SU32(rx_sp(), -4, e); (void)act_vcall(registry, 4, 1, &arg); }
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0041c7b0, rx_physics_entity_destroy)

/* 00403bb0 PhysicsEntity::sdtor (thiscall; flags; RET 4) -> this: the destructor (0041c7b0), then with bit 0 the memory freed (_aligned_free
 * under the allocator lock while [0xda67d8] is set). Frame: push esi. */
rx_addr rx_physics_entity_sdtor(rx_addr e, uint32_t flags) {
    world_frame f = world_frame_flat(1, 0);
    WORLD_CALL_SP(0, rx_physics_entity_destroy(e));
    if (flags & 1) {
        if (U8(0, 0x00da67d8) != 0) loop_enter_cs(0x00d61704);
        WORLD_PUSHED(0x00403bda, e);
        WORLD_CALL_SP(4, crt_aligned_free(e));
        if (U8(0, 0x00da67d8) != 0) loop_leave_cs(0x00d61704);
    }
    world_frame_leave(&f);
    return e;
}
RX_FUNCTION(0x00403bb0, rx_physics_entity_sdtor)
