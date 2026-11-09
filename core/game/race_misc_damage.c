/* race_misc_damage.c -- the damage manager's per-frame entry update (the detachable parts of a vehicle: 12 entries of 0x410 bytes
 * from damage manager + 0x10, see replay_damage.c / build_activity_clear.c): 00479970 (an entry's tick), 0047b960 (a part that is
 * falling / hidden). Hand transliterations of the disassembly.
 *
 * Entry E: +0 the part count, +4 the node names (0x350 bytes per part: +0x104 the second name, ...), +0xc + 4i the part state (2 the
 * part is swinging, 3 it fell off, 4 it is gone), +0x2c + 4i the part entities, +0x230 / +0x250 + 4i the spring angle / velocity,
 * +0x370 + 4i the part's node in the vehicle's model, +0x3d0 + i active flags, +0x3d8 + 4i the part timers, +0x3f8 / +0x400 the
 * hit timer / flag, +0x3fc the vehicle.
 * SCOPE: entries are registered by the spawn (0047bb90) for the four-wheel vehicle types only (the vehicle's +0x194 is 3, 5 or 7:
 * trophy truck, buggy, UTV); a motocross bike has none. The spring simulation of a swinging part (0047aff0) is
 * world_veh_damage_spring.c. */
#include "race.h"
#include "race_extern.h"
#include "race_misc.h"
#include "build_activity_util.h"
#include "replay_damage.h"
#include "world_veh_damage.h"
#include "world_veh.h"
#include "anim.h"

/* fld dword; fstp dword: the bits, a signalling NaN quieted */
static uint32_t fcopy(uint32_t bits) {
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x007fffffu) != 0) bits |= 0x00400000u;
    return bits;
}

/* a node of the vehicle's model hierarchy (by name) hidden (+0xc |= 0x10) when it exists */
static void hide_node(rx_addr hierarchy, rx_addr name) {
    uint32_t exists;
    ACT_CALLR(exists, 1, rx_hierarchy_has_node(hierarchy, name));              /* 007de980 */
    if (exists) {
        rx_addr node;
        ACT_CALLR(node, 1, rx_hierarchy_find_node(hierarchy, name));           /* 007de9e0 */
        SU8(node, 0xc, U8(node, 0xc) | 0x10);
    }
}

/* 0047b960 (stdcall entry E, part index i; xmm1 = dt; RET 8): a falling part's timer (+0x3d8 + 4i) advanced by dt; the timer going
 * from negative to non-negative (the part's release) turns its physics body's flag +0xb0 on (the part entity's handle +0x158);
 * otherwise a timer above the ConVar "DamagePartLifetime" (its value 0x9c642c: 5.0 from 0x935890, static initializer 00903790) with
 * the entry's vehicle set ends the part: state 4, both of its nodes hidden, the node's rotation reset (0047afb0) and the part
 * deactivated (0047a830). Frame: push ecx / ebx / ebp / esi / edi. */
void rx_damage_part_fall(rx_addr e, uint32_t i, float dt) {
    ACT_FRAME(F, 0, 5);
    if (U32(e, 0x370 + 4 * i) != 0) {
        const float before = F32(e, 0x3d8 + 4 * i);
        const int negative = 0.0f > before;
        const float timer = rx_adds(dt, before);
        SF32(e, 0x3d8 + 4 * i, timer);
        if (negative && !(timer < 0.0f || timer != timer)) {
            const rx_addr part = U32(e, 0x2c + 4 * i);
            if (U32(part, 0x158) != 0) {
                rx_addr body;
                ACT_CALLR(body, 0, rx_world_physics_handle_body(U32(part, 0x158)));
                SU8(body, 0xb0, 1);
            }
        } else if (F32(e, 0x3d8 + 4 * i) > F32(0, 0x009c642c) && U32(e, 0x3fc) != 0) {   /* comiss: NaN is not above */
            SU32(e, 0xc + 4 * i, 4);
            const rx_addr names = U32(e, 4) + i * 0x350;
            const rx_addr hierarchy = U32(U32(e, 0x3fc), 0x1328) + 0x10;
            hide_node(hierarchy, names + 0x104);
            hide_node(U32(U32(e, 0x3fc), 0x1328) + 0x10, names);
            ACT_CALL(0, rx_replay_damage_reset_rotation(e, i));                 /* 0047afb0 (EAX = E, EDX = i) */
            ACT_CALL(1, rx_replay_damage_deactivate(e, i));                     /* 0047a830 (stack E; EDI = i) */
        }
    }
    ACT_END(F);
}
RX_FUNCTION(0x0047b960, rx_damage_part_fall)

/* 0047aff0 (stdcall entry E, part index i, dt; RET 0xc): a swinging part's spring (world_veh_damage_spring.c). */
static void damage_part_spring(rx_addr e, uint32_t i, float dt) {
    rx_world_damage_part_spring(e, i, dt);
}

/* 00479970 (ESI = entry E; stdcall dt; RET 4): each part by state: 2 -> the spring (0047aff0, dt through the x87), 3 -> the fall
 * (0047b960); then the hit timer (+0x3f8) advanced while the hit flag (+0x400) is set and both cleared once it passes 2.0
 * (0x92e330). Frame: push ebx; push edi. */
void rx_damage_entry_update(rx_addr e, float dt) {
    ACT_FRAME(F, 0, 2);
    const uint32_t count = U32(e, 0);
    for (uint32_t i = 0; i < count; i++) {
        const uint32_t state = U32(e, 0xc + 4 * i);
        if (state == 2) ACT_CALL(3, damage_part_spring(e, i, rx_bf(fcopy(rx_fb(dt)))));
        else if (state == 3) ACT_CALL(2, rx_damage_part_fall(e, i, dt));
    }
    if (U32(e, 0x400) != 0) {
        const float t = rx_adds(dt, F32(e, 0x3f8));
        SF32(e, 0x3f8, t);
        if (t > 2.0f) {
            SF32(e, 0x3f8, 0.0f);
            SU32(e, 0x400, 0);
        }
    }
    ACT_END(F);
}
RX_FUNCTION(0x00479970, rx_damage_entry_update)
