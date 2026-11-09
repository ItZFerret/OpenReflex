/* world_veh_damage_hit.c -- the vehicle damage behaviour's contact handler 0047a970 and the vehicle effect emission it uses
 * (00691c30 -> 00697b10 -> 006971f0), plus the hit-level counter 0047ba80.
 *
 * Damage behaviour (world_veh_damage.c, 12 elements of 0x410 bytes in the manager 0xdb1410): +0 the part count, +4 the part name
 * table (0x350 bytes per part: +0 the part node's name, +0x104 the second node's name), +0xc + 4i the part state (0 intact, 1 first
 * break, 2 second break, 3 gone), +0x110 + 4i the accumulated hit points, +0x370 + 4i the part's node in the vehicle model, +0x3b0 + 4i
 * the effect id (2), +0x3fc the vehicle. VehicleDamageBehaviorManager::vf0 (0047bd10, world_veh_raven.c) calls 0047a970 for every
 * behaviour with a vehicle that appears in a contact event of the physics step.
 * Contact event (64 bytes): +0 / +4 the two objects, +0x10 the contact point, +0x20 a flag (0: a real contact), +0x24 its strength.
 * Vehicle effect set (0xc11e90 + 0x17c30 * k, 12 of them; 00691cc0 binds one to a vehicle): +0 the vehicle, +0x220 the particle
 * manager it is bound to, +0x224 the number of effect records, +0x230 + 0x3f0 * j the records (+0x6c the effect id, +0x80 / +0x84 /
 * +0x85 a strength threshold, its direction and whether it applies, +0xd0 the offset in the effect's frame, +0x60 the emitter).
 * Hand transliterations of the disassembly. */
#include "world_util.h"
#include "rider_cap.h"
#include "world_veh_damage.h"
#include "world_node.h"
#include "replay_damage.h"
#include "anim.h"
#include "phys_crt.h"
#include "scene_fx_entities.h"

/* 006971f0 (ESI = the effect record, EAX = the orientation quaternion, ECX = the offset vector; stack position, strength^2; RET 8): an
 * effect record is started at a hit: when the record has a threshold (+0x85) the strength must be on its side (+0x84 clear: at least
 * the threshold, set: at most; NaN fails); the rotation matrix of the quaternion (x, y, z, w) goes to +0x10 .. +0x38 (rows of 16 bytes,
 * last column 0), the translation +0x40 = position + R * (+0xd0 .. +0xd8) (+ the old +0x40, which was just cleared), the offset vector
 * is copied through the x87 to +0x50 .., the emitter (+0x60) is told vf5 (1) and vf3 (0.33, 0x9d78d4).
 * Frame: push ebp; and esp, -16; sub esp, 0x10 (temporaries at +0 .. +0xc). */
void rx_world_effect_record_start(rx_addr rec, rx_addr quat, rx_addr offset, rx_addr position, float strength2) {
    world_frame f = world_frame_enter(0x10, 0, 1);
    const rx_addr T = f.esp;
    if (U8(rec, 0x85) != 0) {
        const float limit = F32(rec, 0x80);
        if (U8(rec, 0x84) == 0) { if (!(strength2 >= limit)) goto done; }
        else if (!(limit >= strength2)) goto done;
    }
    {
        const float x = F32(quat, 0), y = F32(quat, 4), z = F32(quat, 8), w = F32(quat, 0xc);
        const float two = F32(0, 0x0092e330);
        const float z2 = phys_mul(z, two), y2 = phys_mul(y, two), x2 = phys_mul(x, two);
        const float xx = phys_mul(x, x2), zz = phys_mul(z, z2);
        SF32(T, 0, z2);
        const float wx = phys_mul(w, x2);
        SF32(T, 0xc, wx);
        const float wz = phys_mul(w, z2);
        const float wy = phys_mul(w, y2);
        SF32(T, 4, wy);
        SF32(T, 8, xx);
        const float xy = phys_mul(y, x2);
        const float xz = phys_mul(z, x2);
        const float zy = phys_mul(z, y2);
        const float yy = phys_mul(y, y2);
        const float one = F32(0, 0x00931f24);
        SF32(T, 0, wz);
        SF32(rec, 0x10, phys_sub(one, phys_add(zz, yy)));
        SF32(rec, 0x20, phys_sub(xy, wz));
        SF32(rec, 0x14, phys_add(xy, wz));
        SF32(rec, 0x24, phys_sub(one, phys_add(zz, xx)));
        SF32(rec, 0x34, phys_sub(zy, wx));
        SF32(rec, 0x18, phys_sub(xz, wy));
        SF32(rec, 0x1c, 0.0f); SF32(rec, 0x2c, 0.0f); SF32(rec, 0x3c, 0.0f);
        SF32(rec, 0x40, 0.0f); SF32(rec, 0x44, 0.0f); SF32(rec, 0x48, 0.0f);
        SF32(rec, 0x30, phys_add(xz, wy));
        SF32(rec, 0x4c, one);
        SF32(rec, 0x28, phys_add(zy, wx));
        SF32(rec, 0x38, phys_sub(one, phys_add(yy, xx)));
    }
    {
        const float d0 = F32(rec, 0xd0), d4 = F32(rec, 0xd4), d8 = F32(rec, 0xd8);
        float t0 = phys_add(phys_mul(F32(rec, 0x20), d4), phys_mul(F32(rec, 0x30), d8));
        t0 = phys_add(t0, phys_mul(F32(rec, 0x10), d0));
        t0 = phys_add(t0, F32(rec, 0x40));
        float t1 = phys_add(phys_mul(F32(rec, 0x14), d0), phys_mul(F32(rec, 0x24), d4));
        t1 = phys_add(t1, phys_mul(F32(rec, 0x34), d8));
        t1 = phys_add(t1, F32(rec, 0x44));
        float t2 = phys_add(phys_mul(F32(rec, 0x18), d0), phys_mul(F32(rec, 0x28), d4));
        t2 = phys_add(t2, phys_mul(F32(rec, 0x38), d8));
        t2 = phys_add(t2, F32(rec, 0x48));
        SF32(rec, 0x40, phys_add(F32(position, 0), t0));
        SF32(rec, 0x44, phys_add(F32(position, 4), t1));
        SF32(rec, 0x48, phys_add(F32(position, 8), t2));
    }
    SU32(rec, 0x50, world_x87_pass(U32(offset, 0)));
    SU32(rec, 0x54, world_x87_pass(U32(offset, 4)));
    SU32(rec, 0x58, world_x87_pass(U32(offset, 8)));
    {
        const rx_addr emitter = U32(rec, 0x60);
        WORLD_VCALL(emitter, 0x14, 1);
        world_vcall_f(emitter, 0xc, F32(0, 0x009d78d4));
    }
done:
    world_frame_leave(&f);
}
RX_FUNCTION(0x006971f0, rx_world_effect_record_start)

/* 00697b10 (stdcall effect set, effect id, position, quaternion, offset vector, strength^2; RET 0x18): every record of the set with that
 * effect id is started (006971f0), unless the set is empty (+0 null), the position's x is NaN, or the set has no records. Frame: push
 * ecx, ebx, ebp, esi, edi. */
void rx_world_effect_set_start(rx_addr set, uint32_t id, rx_addr position, rx_addr quat, rx_addr offset, float strength2) {
    world_frame f = world_frame_flat(5, 0);
    const float px = F32(position, 0);
    if (U32(set, 0) != 0 && px == px && I32(set, 0x224) > 0) {
        for (int32_t i = 0; i < I32(set, 0x224); i++) {
            const rx_addr rec = set + 0x230 + 0x3f0 * (uint32_t)i;
            if (id != U32(rec, 0x6c)) continue;
            WORLD_CALL_SP(8, rx_world_effect_record_start(rec, quat, offset, position, rx_bf(world_x87_pass(rx_fb(strength2)))));
        }
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x00697b10, rx_world_effect_set_start)

/* 00691c30 (cdecl vehicle, effect id, position, quaternion, offset vector, strength): the vehicle's hit effects: the scene's particle
 * manager ([0xa94264 + 4 * scene +0x2cc4], where the scene is the engine context's current one) must exist; every effect set of the
 * 12 (0xc11e90 .. 0xd2f0d0) bound to the vehicle is (re)bound to that manager when it was not (00697870) and started (00697b10) with
 * the strength squared. Frame: push ecx, ebx, ebp, esi, edi. */
void rx_world_vehicle_effects_emit(rx_addr vehicle, uint32_t id, rx_addr position, rx_addr quat, rx_addr offset, float strength) {
    world_frame f = world_frame_flat(5, 0);
    const rx_addr ctx = U32(0, 0x00a95844);
    const rx_addr scene = U32(ctx, 0x1c + 4 * U32(ctx, 4));
    const rx_addr manager = U32(0, 0x00a94264 + 4 * U32(scene, 0x2cc4));
    if (manager != 0) {
        for (uint32_t k = 0; k < rx_rc_tab_n(0x00c11e90u); k++) {   /* (w-rc-core) + the side sets taken (rider_cap.h) */
            const rx_addr set = rx_rc_tab(0x00c11e90u, 0x17c30u, k);
            if (U32(set, 0) != vehicle) continue;
            const int changed = U32(set, 0x220) != manager;
            SU32(set, 0x220, manager);
            if (changed) WORLD_CALL_SP(0, rx_vehicle_effects_rebind(set));
            const float squared = phys_mul(strength, strength);
            WORLD_CALL_SP(0x18, rx_world_effect_set_start(set, id, position, quat, offset, squared));
        }
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x00691c30, rx_world_vehicle_effects_emit)

/* 0047ba80 (ECX = the behaviour, EAX = the part index, xmm0 = the hit strength; plain RET; EAX = 0): the part's hit points +0x110 + 4i
 * grow by 0x14 for a strength in (3, 10), 0x28 in (10, 25), 0x46 in (25, 35), 0x55 above 35 (strict comparisons, NaN adds nothing). */
void rx_world_damage_hit_points(rx_addr behaviour, uint32_t index, float strength) {
    const rx_addr cell = behaviour + 0x110 + 4 * index;
    if (strength > 3.0f && 10.0f > strength) { SU32(cell, 0, U32(cell, 0) + 0x14); return; }
    if (strength > 10.0f && 25.0f > strength) { SU32(cell, 0, U32(cell, 0) + 0x28); return; }
    if (strength > 25.0f && 35.0f > strength) { SU32(cell, 0, U32(cell, 0) + 0x46); return; }
    if (strength > 35.0f) SU32(cell, 0, U32(cell, 0) + 0x55);
}
RX_FUNCTION(0x0047ba80, rx_world_damage_hit_points)

/* a model node (by name) shown (clear bit 0x10 of +0xc) or hidden (set it) when the hierarchy has it */
static void damage_node_visibility(rx_addr hierarchy, rx_addr name, int hidden) {
    uint32_t exists;
    WORLD_CALL_SP(4, exists = rx_hierarchy_has_node(hierarchy, name));              /* 007de980 */
    if (!exists) return;
    rx_addr node;
    WORLD_CALL_SP(4, node = rx_hierarchy_find_node(hierarchy, name));              /* 007de9e0 */
    SU8(node, 0xc, hidden ? (U8(node, 0xc) | 0x10) : (U8(node, 0xc) & 0xef));
}

/* 0047a880 (stdcall behaviour; RET 4): the behaviour's parts are put back: for each part (while the behaviour has a vehicle) the state +0xc + 4i
 * is 0, the first node shown and the second hidden again, the part deactivated (0047a830, EDI = i) and its rotation reset (0047afb0,
 * EAX = the behaviour, EDX = i); the hit points +0x110 + 4i are 0 for every part. Called when a vehicle is released from a behaviour
 * and by the registration (00479a00). Frame: push ebp; and esp, -8; sub esp, 0xc; push ebx, esi, edi. */
void rx_world_damage_reset_parts(rx_addr behaviour) {
    const rx_addr entry = rx_sp();
    rx_set_sp((((entry - 4) & ~7u) - 0xc) - 12);                                   /* push ebp; and esp, -8; sub esp, 0xc; 3 pushes */
    for (uint32_t i = 0; i < U32(behaviour, 0); i++) {
        if (U32(behaviour, 0x3fc) != 0) {
            SU32(behaviour, 0xc + 4 * i, 0);
            const rx_addr names = U32(behaviour, 4) + 0x350 * i;
            const rx_addr hierarchy = U32(U32(behaviour, 0x3fc), 0x1328) + 0x10;
            damage_node_visibility(hierarchy, names, 0);
            damage_node_visibility(U32(U32(behaviour, 0x3fc), 0x1328) + 0x10, names + 0x104, 1);
            WORLD_CALL_SP(4, rx_replay_damage_deactivate(behaviour, i));                /* 0047a830 */
            WORLD_CALL_SP(0, rx_replay_damage_reset_rotation(behaviour, i));            /* 0047afb0 */
        }
        SU32(behaviour, 0x110 + 4 * i, 0);
    }
    rx_set_sp(entry);
}
RX_FUNCTION(0x0047a880, rx_world_damage_reset_parts)

/* 0047a970 (ECX = the contact event, stack the behaviour; RET 4): the vehicle (behaviour +0x3fc) is hit at the event's point: its own
 * effect (id 1, vf 0xcc of the vehicle gives its frame: position at +0x1d0, quaternion at +0x1e0 of the 0x1e0-byte block) when the
 * contact point is within 20 of the vehicle; then, per part (node +0x370 + 4i, evaluated in the world), the vector from the contact to the
 * part node's world position (+0x30 of its matrix row) and, within 5 of it and for a real contact (+0x20 == 0): the hit points grow
 * (0047ba80), and the part passes at most one stage per call (the hit points and state are read once before them) once its hit points exceed 0x28 (state 0 -> 1: effect, the second node shown, the first
 * hidden, the part deactivated and its rotation reset), 0x82 (1 -> 2: effect, the same visibility change, deactivated, motion reset) and
 * 0xd7 (2 -> 3: effect, both nodes hidden, activated). Each stage needs the vehicle still set after its effect.
 * Frame: push ebp; and esp, -16; sub esp, 0x274; push ebx, esi, edi: S + 0x30 the offset vector, + 0x40 the part's local rotation, + 0x50 its
 * world position, + 0x60 the vehicle's frame block (position at +0x230, quaternion at +0x240 from S). */
void rx_world_damage_apply_contact(rx_addr ev, rx_addr behaviour) {
    world_frame f = world_frame_enter(0x274, 3, 1);
    const rx_addr S = f.esp;
    SU32(S, 0x27c, U32(0, 0x009b45e4) ^ (S + 0xc));
    const rx_addr vehicle = U32(behaviour, 0x3fc);
    SU32(S, 0x14, ev);
    {   /* the vehicle's frame block: vf 0xcc(out) */
        const rx_addr out = S + 0x60;
        WORLD_VCALL(vehicle, 0xcc, out);
    }
    float dz = phys_sub(F32(S, 0x238), F32(ev, 0x18));
    float dy = phys_sub(F32(S, 0x234), F32(ev, 0x14));
    float dx = phys_sub(F32(S, 0x230), F32(ev, 0x10));
    SF32(S, 0x38, dz); SF32(S, 0x34, dy); SF32(S, 0x30, dx);
    float dist;
    {
        const float d2 = phys_add(phys_add(phys_mul(dz, dz), phys_mul(dy, dy)), phys_mul(dx, dx));
        WORLD_CALL_SP(4, dist = phys_sqrtf(d2));
    }
    const rx_addr pos = ev + 0x10;
    if (dist <= F32(0, 0x00935aac) && U8(ev, 0x20) == 0) {
        WORLD_CALL_SP(0x18, rx_world_vehicle_effects_emit(U32(behaviour, 0x3fc), 1, pos, S + 0x240, S + 0x30,
                                                         rx_bf(world_x87_pass(U32(ev, 0x24)))));
    }
    for (uint32_t i = 0; i < U32(behaviour, 0); i++) {
        const rx_addr node = U32(behaviour, 0x370 + 4 * i);
        if (node == 0) continue;
        WORLD_CALL_SP(4, rx_world_node_evaluate_world(node, 0));                    /* Node_EvaluateWorld */
        {
            const rx_addr row = U32(U32(node, 0), 0x1c) + ((uint32_t)U8(node, 0xb) << 6) + 0x30;
            v4_st(S + 0x50, v4_ld(row));
        }
        WORLD_CALL_SP(0, rx_node_local_rotation(U32(behaviour, 0x370 + 4 * i), S + 0x40));   /* 007dd380 (EAX = node, ESI = out) */
        dz = phys_sub(F32(S, 0x58), F32(ev, 0x18));
        dy = phys_sub(F32(S, 0x54), F32(ev, 0x14));
        dx = phys_sub(F32(S, 0x50), F32(ev, 0x10));
        SF32(S, 0x38, dz); SF32(S, 0x34, dy); SF32(S, 0x30, dx);
        {
            const float d2 = phys_add(phys_add(phys_mul(dz, dz), phys_mul(dy, dy)), phys_mul(dx, dx));
            WORLD_CALL_SP(4, dist = phys_sqrtf(d2));
        }
        if (!(dist <= F32(0, 0x00935890)) || U8(ev, 0x20) != 0) continue;
        const float strength = F32(ev, 0x24);
        WORLD_CALL_SP(0, rx_world_damage_hit_points(behaviour, i, strength));      /* 0047ba80 (EAX = i, xmm0 = strength) */
        const int32_t hits = I32(behaviour, 0x110 + 4 * i);                          /* (read once: the stages test these, not the updated words) */
        const uint32_t state = U32(behaviour, 0xc + 4 * i);
        const rx_addr names = U32(behaviour, 4) + 0x350 * i;
        const rx_addr hierarchy = U32(U32(behaviour, 0x3fc), 0x1328) + 0x10;
        const float effect_strength = rx_bf(world_x87_pass(U32(ev, 0x24)));
        if (hits > 0x28 && state == 0) {
            WORLD_CALL_SP(0x18, rx_world_vehicle_effects_emit(U32(behaviour, 0x3fc), U32(behaviour, 0x3b0 + 4 * i), pos, S + 0x40, S + 0x30,
                                                             effect_strength));
            if (U32(behaviour, 0x3fc) != 0) {
                SU32(behaviour, 0xc + 4 * i, 1);
                damage_node_visibility(hierarchy, names + 0x104, 0);
                damage_node_visibility(hierarchy, names, 1);
                WORLD_CALL_SP(4, rx_replay_damage_deactivate(behaviour, i));            /* 0047a830 (push E; EDI = i) */
                WORLD_CALL_SP(0, rx_replay_damage_reset_rotation(behaviour, i));        /* 0047afb0 */
            }
        }
        if (hits > 0x82 && state == 1) {
            WORLD_CALL_SP(0x18, rx_world_vehicle_effects_emit(U32(behaviour, 0x3fc), U32(behaviour, 0x3b0 + 4 * i), pos, S + 0x40, S + 0x30,
                                                             effect_strength));
            if (U32(behaviour, 0x3fc) != 0) {
                SU32(behaviour, 0xc + 4 * i, 2);
                damage_node_visibility(hierarchy, names + 0x104, 0);
                damage_node_visibility(hierarchy, names, 1);
                WORLD_CALL_SP(4, rx_replay_damage_deactivate(behaviour, i));
                WORLD_CALL_SP(8, rx_replay_damage_reset_motion(behaviour, i));          /* 0047af30 */
            }
        }
        if (hits > 0xd7 && state == 2) {
            WORLD_CALL_SP(0x18, rx_world_vehicle_effects_emit(U32(behaviour, 0x3fc), U32(behaviour, 0x3b0 + 4 * i), pos, S + 0x40, S + 0x30,
                                                             effect_strength));
            if (U32(behaviour, 0x3fc) != 0) {
                SU32(behaviour, 0xc + 4 * i, 3);
                damage_node_visibility(hierarchy, names + 0x104, 1);
                damage_node_visibility(hierarchy, names, 1);
                WORLD_CALL_SP(4, rx_replay_damage_activate(behaviour, i));              /* 0047a630 (push E; ECX = i) */
            }
        }
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0047a970, rx_world_damage_apply_contact)
