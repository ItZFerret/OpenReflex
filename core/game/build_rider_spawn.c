/* build_rider_spawn.c -- construction unit 7 (riders, core/notes/construction.md section 13): a rider made from its rider entry.
 * FUN_0053e920 (a race rider: RiderAnimation 0x5c4f0 on the player's vehicle) and FUN_0053eab0 (an intro / podium rider:
 * RiderCharacter 0xd30), with the animation tree of the vehicle type (0053cd40), the gear put on the model (0053dd20 / 0053dd70 ->
 * VehicleLoader_Assemble, build_rider_gear.c) and the scene registration. Hand transliterations of the disassembly.
 *
 * The rider manager (MXGame + 0x15504): +0x14 the rider entries (pointer array), +0x18 their count, +0x28 the spawned riders' entry
 * slots (Rdb pointer array, 00544cc0). A rider entry: +4 the rider id, +0x14 / +0x1c.. its gear (build_rider_gear.c), +0x18 the
 * rider made from it. */
#include "build_rider.h"
#include "build_rider_util.h"
#include "build_vehicle.h"
#include "build_scene.h"
#include "world_misc.h"
#include "anim.h"
#include "crt.h"

/* the stack pointer of a call as the original makes it (`drop` bytes of pushes and the return address below the current one), so the
 * callee's frame lands at the original's addresses */
#define RR_SP_CALL(drop, stmt) do { const rx_addr sp_ = rx_sp(); rx_set_sp(sp_ - (drop)); stmt; rx_set_sp(sp_); } while (0)
#define RR_ALLOC_LOCKED 0x00da67d8u   /* byte: the allocator's lock is used */
#define RR_ALLOC_CS     0x00d61704u

/* _aligned_malloc(size, 16) under the allocator's lock (the inline form of the original's allocations of these objects) */
static rx_addr locked_aligned_malloc(uint32_t size) {
    if (U8(0, RR_ALLOC_LOCKED)) rr_os_cs_enter(RR_ALLOC_CS);
    rx_addr p = rx_crt_aligned_malloc(size, 0x10);
    if (U8(0, RR_ALLOC_LOCKED)) rr_os_cs_leave(RR_ALLOC_CS);
    return p;
}
/* the entry of a rider id (the first match; the original reads [0 + 0x18] when there is none) */
static rx_addr rider_entry(rx_addr mgr, uint32_t id) {
    for (int32_t i = 0; i < (int32_t)U32(mgr, 0x18); i++) {
        rx_addr e = U32(U32(mgr, 0x14), 4 * (uint32_t)i);
        if (U32(e, 4) == id) return e;
    }
    rr_fatal("rider entry not found (the original faults)", id);
    return 0;
}
/* the scene's entity serial (+0x19f4, under the scene's lock +4) into the entity's +0x14, then SceneManager_AddEntity 007d2f70 */
static void scene_add(rx_addr scene, rx_addr e, uint32_t drop) {
    rr_os_cs_enter(scene + 4);
    uint32_t n = U32(scene, 0x19f4);
    SU32(scene, 0x19f4, n + 1);
    SU32(e, 0x14, n);
    rr_os_cs_leave(scene + 4);
    const rx_addr sp = rx_sp();   /* the call's stack pointer as the original's (the scene code's frames land at its addresses) */
    rx_set_sp(sp - drop);
    rx_scene_manager_add_entity(e, scene);
    rx_set_sp(sp);
}

/* Database vf27 (0x6c): the handle of a BXML asset by name ("bxml", 0, 1) */
static rx_addr bxml_handle(rx_addr name) {
    uint32_t a[4] = {name, 0x6c6d7862u, 0, 1};
    return rr_vcall(rr_database(), 0x6c, 4, a);
}
/* 00542ff0 (EAX = list, EDI = id): the entry {+4 id} of a pointer list (+0x28 data, +0x2c count), 0 when none */
static rx_addr id_list_find(rx_addr list, uint32_t id) {
    for (int32_t i = 0; i < (int32_t)U32(list, 0x2c); i++) {
        rx_addr e = U32(U32(list, 0x28), 4 * (uint32_t)i);
        if (e && U32(e, 4) == id) return e;
    }
    return 0;
}
/* 0053cd40 (ECX = vehicle; cdecl world, unused): the rider animation tree of the vehicle type (+0x194) */
rx_addr rx_rider_tree_handle(rx_addr veh) {
    switch (U32(veh, 0x194)) {
    case 0: return bxml_handle(0x00940568u);   /* "MX_RavenRiderAnimationTree" (both forms of +0x198) */
    case 1: return bxml_handle(0x00940584u);   /* "ATV_RavenRiderAnimationTree" */
    case 2: case 4: case 6: return bxml_handle(0x009405a0u);   /* "TrophyTruck_RavenRiderAnimationTree" */
    case 3: {
        rx_addr e = id_list_find(U32(0, RR_GAME) + 0x154b0, U32(veh, 0x18c));
        if (!e) rr_fatal("vehicle id not in the vehicle list (the original faults)", U32(veh, 0x18c));
        uint32_t t = U32(U32(e, 8), 4);
        if (t == 0xf) return bxml_handle(0x009405a0u);   /* "TrophyTruck_RavenRiderAnimationTree" */
        if (t == 0xe) return bxml_handle(0x009405c4u);   /* "TrophyTruckLite_RavenRiderAnimationTree" */
        return 0;
    }
    case 5: return bxml_handle(0x009405ecu);   /* "SandBuggy_RavenRiderAnimationTree" */
    case 7: return bxml_handle(0x00940610u);   /* "UTV_RavenRiderAnimationTree" */
    }
    return 0;
}
/* 0053c7d0 (EAX = &a, ECX = &b; cdecl vehicle type): the two ranges of the type (b: 900 MX / ATV, 2000 truck, 1300 others; a 900);
 * nothing for types above 7 */
void rx_rider_type_ranges(uint32_t type, rx_addr a, rx_addr b) {
    if (type > 7) return;
    SU32(b, 0, type < 2 ? 900u : type == 2 ? 2000u : 1300u);
    SU32(a, 0, 900);
}

/* 0068f300 (ECX = map, s0 node; RET 4): a std::map subtree (0x18-byte nodes, isnil +0x15) freed, right subtrees first */
void rx_rider_map18_erase(rx_addr map, rx_addr n) {
    while (!U8(n, 0x15)) {
        rx_rider_map18_erase(map, U32(n, 8));
        rx_addr l = U32(n, 0);
        rx_reflex_aligned_free(n);
        n = l;
    }
}
/* the map (head at +0x18, size +0x1c) emptied */
static void map18_clear(rx_addr map) {
    rx_rider_map18_erase(map, U32(U32(map, 0x18), 4));
    rx_addr h = U32(map, 0x18);
    SU32(h, 4, h);
    SU32(map, 0x1c, 0);
    SU32(h, 0, h);
    SU32(h, 8, h);
}

/* 0053dd20 (EDI = ra; stdcall manager, entry; RET 8): the map at +0x5bb6c emptied, the gear assembled on the model copy (+0x170) */
void rx_rider_assemble_gear(rx_addr ra, rx_addr mgr, rx_addr entry) {
    map18_clear(ra + 0x5bb6c);
    RR_SP_CALL(0xc, rx_vehloader_assemble(ra + 0x170, entry, mgr));   /* push esi, push manager, call */
}
/* 0053dd70 (EDI = rc; stdcall manager, entry; RET 8): the same for a RiderCharacter (map +0x380, model copy +0x200) */
void rx_rider_character_assemble_gear(rx_addr rc, rx_addr mgr, rx_addr entry) {
    map18_clear(rc + 0x380);
    RR_SP_CALL(0xc, rx_vehloader_assemble(rc + 0x200, entry, mgr));
}

/* 0053e920 (EAX = rider id; stdcall manager, world, vehicle, callback; RET 0x10): the race rider of an entry on a vehicle */
rx_addr rx_rider_spawn(uint32_t id, rx_addr mgr, rx_addr world, rx_addr veh, rx_addr cb) {
    rx_addr entry = rider_entry(mgr, id);
    rx_addr mem = locked_aligned_malloc(0x5c4f0);
    rx_addr ra = mem ? rx_rider_animation_construct(mem) : 0;
    SU32(entry, 0x18, ra);
    SU32(ra, 0x5b51c, id);
    rx_addr scene = U32(world, 0x20138);
    rx_addr hier = U32(U32(0, RR_GEARLOADER), 4);
    rx_rider_animation_init(hier, U32(entry, 0x18), scene, rx_rider_tree_handle(veh), veh);
    ra = U32(entry, 0x18);
    RR_SP_CALL(0x20, rx_rider_assemble_gear(ra, mgr, entry));   /* five saved registers, two arguments, call */
    RX_FRAME_ENTER(F, 8);   /* the original's two out slots (its saved ecx slot and the entry argument slot) */
    SU32(F, 4, 0);
    rx_rider_type_ranges(U32(veh, 0x194), F + 4, F);
    uint32_t r = U32(F, 4);
    RX_FRAME_LEAVE(F, 8);
    SU32(U32(entry, 0x18), 0x290, (int32_t)r < 0xffff ? r : 0xffffu);
    SU32(veh, 0x1a0, U32(entry, 0x18));
    rx_addr x = U32(entry, 0x18);
    SU16(x, 0xb0, (U16(x, 0xb0) & 0xffef) | 8);
    if (cb) { uint32_t a = U32(entry, 0x18); rr_vcall(cb, 0, 1, &a); }
    scene_add(U32(world, 0x20138), U32(entry, 0x18), 0x1c);   /* five saved registers, the scene, call */
    rx_world_ptr_vector_push_unique(mgr + 0x28, entry + 0x18);
    return U32(entry, 0x18);
}
RX_FUNCTION(0x0053e920, rx_rider_spawn)

/* 00624750 (ESI = memory): RiderCharacter: CharacterEntity, vtables 0x0095881c / +0x90 0x009588a4 / +0x140 0x009588e4, the map at
 * +0x380 (proxy, head 00706df0), the mud / plate fields */
rx_addr rx_ridercharacter_construct(rx_addr c) {
    rx_character_entity_construct(c);
    SU32(c, 0, 0x0095881cu); SU32(c, 0x90, 0x009588a4u); SU32(c, 0x140, 0x009588e4u);
    rx_addr m = c + 0x380;
    rx_addr p = rr_alloc(4);
    if (p) SU32(p, 0, m);
    SU32(m, 0, p);
    rx_addr h = rx_veh_map_head_alloc();
    SU32(m, 0x18, h);
    SU8(h, 0x15, 1);
    SU32(U32(m, 0x18), 4, U32(m, 0x18));
    SU32(U32(m, 0x18), 0, U32(m, 0x18));
    SU32(U32(m, 0x18), 8, U32(m, 0x18));
    SU32(m, 0x1c, 0);
    SU8(c, 0xd20, 0); SU8(c, 0xd21, 0);
    SU32(c, 0x350, 0xffffffffu);
    SU32(c, 0x358, 0); SU32(c, 0x35c, 0);
    SF32(c, 0x370, 1.0f); SF32(c, 0x374, 1.0f);
    SU32(c, 0x378, 0);
    SU32(c, 0x3a0, 1);
    for (uint32_t o = 0x3b0; o <= 0x3dc; o += 4) SF32(c, o, 0.0f);
    return c;
}
/* one named float slot of the character's script table (+0x3e0: 64-byte names, +0x400 kinds, +0x440 value pointers, +0xd20 count) */
static void character_slot(rx_addr c, rx_addr name, rx_addr value) {
    rx_addr t = c + 0x3e0;
    rx_addr d = t + ((uint32_t)U8(t, 0x940) << 6);
    crt_strncpy(d, name, 0x3f);
    SU8(d, 0x3f, 0);
    SU32(t, 0x440 + 4 * (uint32_t)U8(t, 0x940), value);
    SU32(t, 0x400 + 4 * (uint32_t)U8(t, 0x940), 1);
    SU8(t, 0x940, U8(t, 0x940) + 1);
}
/* 00624990 RiderCharacter_BindMud (EAX = rc; stdcall scene, hierarchy handle; RET 8): the model copy (+0x200), the character on its
 * node set, the model instance, three float slots (0x9c6180 -> +0x3c0, 0x9c6078 -> +0x3b0, 0x9c6288 -> +0x3d0) */
void rx_rider_character_bind(rx_addr rc, rx_addr scene, rx_addr hier) {
    SU32(rc, 0x37c, hier);
    SU32(hier, 0x4c, 1);
    rx_model_asset_copy(rc + 0x200, U32(hier, 0x44));
    SU32(U32(rc, 0x37c), 0x4c, 0);
    rx_character_anim_init(rc + 0x210, rc + 0x148);
    rx_entity_attach_model(scene, 0, rc, hier, 2);
    character_slot(rc, 0x009c6180u, rc + 0x3c0);
    character_slot(rc, 0x009c6078u, rc + 0x3b0);
    character_slot(rc, 0x009c6288u, rc + 0x3d0);
}

/* 0053eab0 (stdcall manager, world, rider id; RET 0xc): an intro / podium rider of an entry */
rx_addr rx_rider_spawn_character(rx_addr mgr, rx_addr world, uint32_t id) {
    rx_addr entry = rider_entry(mgr, id);
    rx_addr mem = locked_aligned_malloc(0xd30);
    rx_addr rc = mem ? rx_ridercharacter_construct(mem) : 0;
    SU32(rc, 0x350, id);
    rx_rider_character_bind(rc, U32(world, 0x20138), U32(U32(0, RR_GEARLOADER), 4));
    RR_SP_CALL(0x1c, rx_rider_character_assemble_gear(rc, mgr, entry));   /* four saved registers, two arguments, call */
    SU32(rc, 0x320, 1000);
    SU16(rc, 0xb0, (U16(rc, 0xb0) & 0xffef) | 8);
    scene_add(U32(world, 0x20138), rc, 0x18);   /* four saved registers, the scene, call */
    return rc;
}
RX_FUNCTION(0x0053eab0, rx_rider_spawn_character)
