/* anim_rider_dtors.c -- the destructors of the vehicle / rider / scenario entity classes the activity teardown deletes (2026-10-26,
 * agent w-stunts): the scalar deleting destructors reached through vf0 (flags; bit 0 = free the object) and the RiderAnimation
 * destructor chain behind them.
 *   RavenVehicle::sdtor              0045d3a0   (the vehicle base destructor 0045d3c0, ReflexAlignedFree)
 *   RavenVehicleMotocross::sdtor     00462560   (the Motocross vtables, 0045d3c0, the locked CRT free)
 *   RiderAnimation::sdtor            00618c30   (00618cd0 below, the locked CRT free)
 *   ScenarioAnimEntity::sdtor        006b7720   (the lock at +0x3e0 deleted, 006248c0, the locked CRT free)
 * The RiderAnimation destructor 00618cd0 (the rider's animation object, the vehicle's +0x1a0) releases, in the original's order:
 * the instance table slot, the clip slot arrays (006b74d0 / 006b70b0), the pose job object (+0x5b6dc, vf0), the tree +0x5bb6c, the
 * three owned arrays, the physics listener (006372a0), the four name maps (00618c80), the registry entry (00844170), the sound
 * emitter set (004245f0), twelve std::vector-like blocks, and finally the character entity base (0082d100). The sound-instance
 * stop 00804e40 is an AUDIO BOUNDARY: its only effect outside the sound engine is the instance state (+0, +4, +0x144); the FMOD
 * channel stop is called only when the instance holds a channel (+0x144), which only the sound engine creates (the product plays
 * no sound: build_boundary_audio.c), so the product skips the call and the oracle runs the original import. */
#include "anim_rider_dtors.h"
#include "build_activity_util.h"
#include "anim_scenario.h"
#include "anim_scenario_misc.h"
#include "build_activity_teardown.h"
#include "anim.h"
#include "anim_extern.h"
#include "race_misc.h"
#include "loop_vehicle.h"
#include "loop_platform.h"
#include "vehicle_atv_lifecycle.h"
#include "world_veh_transfer.h"
#include "phys_coll_misc.h"
#include "phys_terrain.h"
#include "vehicle_atv_destroy.h"
#include "build_vehicle.h"
#include "crt_atexit.h"
#include "fx.h"
#include "build_alloc.h"
#include "crt.h"
#include "../src/rx_os.h"
#include "rider_cap.h"

/* operator delete as the compiler inlined it for the scalar deleting destructors: the CRT free (IAT 0x916350) under the allocator
 * lock 0xd61704 while the lock flag [0xda67d8] is set. The pointer is the call's argument slot. */
static void delete_locked(rx_addr p) {
    if (U8(0, 0x00da67d8) != 0) loop_enter_cs(0x00d61704);
    ANIM_CALL_FRAME(4);
    SU32(rx_sp(), 4, p);
    crt_aligned_free(p);
    ANIM_CALL_FRAME_END(4);
    if (U8(0, 0x00da67d8) != 0) loop_leave_cs(0x00d61704);
}
/* ReflexAlignedFree 0078fc50 (cdecl, one argument) */
static void engine_free(rx_addr p) {
    ANIM_CALL_FRAME(4);
    SU32(rx_sp(), 4, p);
    rx_reflex_aligned_free(p);
    ANIM_CALL_FRAME_END(4);
}

/* ---- RavenVehicle / Motocross ---- */

/* 0045d3a0 RavenVehicle::sdtor (thiscall; flags; RET 4) -> this. Frame: push esi. */
rx_addr rx_raven_vehicle_sdtor(rx_addr v, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    ANIM_CALL_FRAME(0); rx_vehicle_base_destruct(v); ANIM_CALL_FRAME_END(0);
    if (flags & 1) engine_free(v);
    rx_set_sp(sp0);
    return v;
}
RX_FUNCTION(0x0045d3a0, rx_raven_vehicle_sdtor)

/* 00462560 RavenVehicleMotocross::sdtor (thiscall; flags; RET 4) -> this: the Motocross vtables (+0 0x93cd84, +0x90 0x93ce88,
 * +0x94 0x93ce9c, +0x9c 0x93cedc), the base destructor 0045d3c0, freed under the allocator lock. Frame: push esi. */
rx_addr rx_motocross_vehicle_sdtor(rx_addr v, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    SU32(v, 0, 0x0093cd84);
    SU32(v, 0x90, 0x0093ce88);
    SU32(v, 0x94, 0x0093ce9c);
    SU32(v, 0x9c, 0x0093cedc);
    ANIM_CALL_FRAME(0); rx_vehicle_base_destruct(v); ANIM_CALL_FRAME_END(0);
    if (flags & 1) delete_locked(v);
    rx_set_sp(sp0);
    return v;
}
RX_FUNCTION(0x00462560, rx_motocross_vehicle_sdtor)

/* RavenVehicleMotocross::vf9 00462960 (thiscall): the vehicle leaves its world. When the world's terrain (found by the vehicle's scene
 * [+0x188] +0x20138 in the terrain list 0x9e30d4 / count 0x9e30dc, lazily initialised under the guard bit 0x9e30e0) exists: the four
 * wheel track slots (+0x1578 .. +0x1584) are freed in the ring they come from (flag 0x1000: [0xa95774], 0x2000: [0xa95778], else
 * [0xa9577c], 007c3400 with the slot) and the 25 + 25 track strips (+0x14c0 / +0x11b0 words) are cut (007c4560) and returned to the
 * third ring (the strip's flag +0x98 = 1, the ring's used count +0x28 - 1). Then the vehicle's simulation (+0x16d4) leaves the physics
 * world (+0x20134; 00769110), is deleted (vf0 (1)), +0x16d4 and +0x234 are cleared, a held object +0x178 (when +0x174 is set) is
 * told (vf1), and RavenVehicle::vf9 (00460230) finishes. Frame: sub esp, 8; push ebx / esi (/ ebp / edi for the track work). */
static void track_slot_free(uint32_t word, rx_addr ring1, rx_addr ring2, rx_addr ring3) {
    if (word & 0x1000) { ANIM_CALL_FRAME(0); rx_tracks_free_slot(ring1, (int32_t)(word ^ 0x1000)); ANIM_CALL_FRAME_END(0); }
    else if (word & 0x2000) { ANIM_CALL_FRAME(0); rx_tracks_free_slot(ring2, (int32_t)(word ^ 0x2000)); ANIM_CALL_FRAME_END(0); }
    else { ANIM_CALL_FRAME(0); rx_tracks_free_slot(ring3, (int32_t)(word ^ 0x4000)); ANIM_CALL_FRAME_END(0); }
}
static void track_strip_cut(rx_addr ring3, uint32_t word) {
    if ((int32_t)word < 0) return;
    const int32_t index = (int32_t)(word ^ 0x4000);
    if (index >= I32(ring3, 0xc) || index < 0) return;
    ANIM_CALL_FRAME(0); phys_tracks_cut_strip(ring3, index); ANIM_CALL_FRAME_END(0);
    SU32(U32(ring3, 0x24), 0xc0u * (uint32_t)index + 0x98, 1);
    SU32(ring3, 0x28, U32(ring3, 0x28) - 1);
}
void rx_motocross_vehicle_detach(rx_addr v) {
    const rx_addr entry0 = rx_sp();
    rx_set_sp(entry0 - 16);                                     /* sub esp, 8; push ebx; push esi */
    const rx_addr scene = U32(U32(v, 0x188), 0x20138);
    if ((U32(0, 0x009e30e0) & 1) == 0) {
        SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1);
        SU32(0, 0x009e30dc, 0);
    }
    rx_addr terrain = 0;
    for (uint32_t i = 0; i < U32(0, 0x009e30dc); i++) {
        const rx_addr t = U32(0, 0x009e30d4 + 4 * i);
        if (t != 0 && scene == U32(t, 0x44a8)) { terrain = t; break; }
    }
    if (terrain != 0) {
        rx_set_sp(entry0 - 24);                                 /* push ebp; push edi */
        const rx_addr ring1 = U32(0, 0x00a95774), ring2 = U32(0, 0x00a95778), ring3 = U32(0, 0x00a9577c);
        for (uint32_t k = 0; k < 4; k++) track_slot_free(U32(v, 0x1578 + 4 * k), ring1, ring2, ring3);
        for (uint32_t k = 0; k < 25; k++) {
            track_strip_cut(ring3, U32(v, 0x14c0 + 4 * k));
            track_strip_cut(ring3, U32(v, 0x11b0 + 4 * k));
        }
        rx_set_sp(entry0 - 16);
    }
    const rx_addr sim = U32(v, 0x16d4);
    SU32(rx_sp(), -4, sim);
    ANIM_CALL_FRAME(4); rx_world_physics_remove_simulation(U32(U32(v, 0x188), 0x20134), sim); ANIM_CALL_FRAME_END(4);
    if (U32(v, 0x16d4) != 0) { const uint32_t one = 1; act_vcall(U32(v, 0x16d4), 0, 1, &one); }
    SU32(v, 0x16d4, 0);
    SU32(v, 0x234, 0);
    if (U8(v, 0x174) != 0) act_vcall(v + 0x178, 4, 0, NULL);
    rx_set_sp(entry0);                                          /* pop esi / ebx; add esp, 8; jmp 00460230 */
    rx_vehicle_base_detach(v);
}
RX_FUNCTION(0x00462960, rx_motocross_vehicle_detach)

/* ---- RavenSimulationMotocross (the Motocross vehicle's simulation, +0x16d4 of the vehicle) ---- */

/* 0043c040 (ECX = the simulation): vtable 0x93c8a4; six buffers freed (+0x8e4c .. +0x8e54, +0x8e40 .. +0x8e48); the embedded record
 * +0x8e10 gets vtable 0x976750 and leaves its registry (00844170); three buffers (+0x8b40 .. +0x8b48) freed; the two wheel blocks
 * (+0x7760, +0x6e00; 0042f9f0) and the chassis block (+0x5ff0; 0042fd50) destructed; eleven curve slots (+0x5f60 .. +0x5d40) get
 * the curve vtable 0x93c9e0 and +0x5cd0 the tuning vtable 0x93cb44; the tuning registry (790400) is created once (guard bit 1 of
 * 0xd72ad4; its destructor 0x911ed0 goes to atexit); [0xd92cd4] = 0; vtable 0x93cc14; the record +0x20d0 leaves its registry; the
 * vector +0x1300 (0081f6e0) and the mechanism +0xd0 (0085c390) destructed; the final vtable 0x93c9d0. Frame: push esi. */
void rx_motocross_sim_destruct(rx_addr sim) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    SU32(sim, 0, 0x0093c8a4);
    {   /* six frees, the arguments pushed one after the other and popped together */
        static const uint16_t first_six[6] = {0x8e4c, 0x8e50, 0x8e54, 0x8e40, 0x8e44, 0x8e48};
        const rx_addr s = rx_sp();
        for (int i = 0; i < 6; i++) {
            rx_set_sp(s - 4u * (uint32_t)(i + 1));
            SU32(rx_sp(), 0, U32(sim, first_six[i]));
            ANIM_CALL_FRAME(0); rx_reflex_aligned_free(U32(sim, first_six[i])); ANIM_CALL_FRAME_END(0);
        }
        rx_set_sp(s);
    }
    SU32(sim, 0x8e10, 0x00976750);
    ANIM_CALL_FRAME(0); rx_registry_remove(sim + 0x8e10); ANIM_CALL_FRAME_END(0);
    {
        static const uint16_t second_three[3] = {0x8b40, 0x8b44, 0x8b48};
        const rx_addr s = rx_sp();
        for (int i = 0; i < 3; i++) {
            rx_set_sp(s - 4u * (uint32_t)(i + 1));
            SU32(rx_sp(), 0, U32(sim, second_three[i]));
            ANIM_CALL_FRAME(0); rx_reflex_aligned_free(U32(sim, second_three[i])); ANIM_CALL_FRAME_END(0);
        }
        rx_set_sp(s);
    }
    ANIM_CALL_FRAME(0); rx_vehicle_wheel_block_destruct(sim + 0x7760); ANIM_CALL_FRAME_END(0);
    ANIM_CALL_FRAME(0); rx_vehicle_wheel_block_destruct(sim + 0x6e00); ANIM_CALL_FRAME_END(0);
    ANIM_CALL_FRAME(0); rx_vehicle_chassis_block_destruct(sim + 0x5ff0); ANIM_CALL_FRAME_END(0);
    static const uint16_t curve_slots[11] = {0x5f60, 0x5f20, 0x5ee0, 0x5ea0, 0x5e60, 0x5e50, 0x5e10, 0x5dd0, 0x5d90, 0x5d50, 0x5d40};
    for (int i = 0; i < 11; i++) SU32(sim, curve_slots[i], 0x0093c9e0);
    SU32(sim, 0x5cd0, 0x0093cb44);
    if ((U32(0, 0x00d72ad4) & 1) == 0) {
        SU32(0, 0x00d72ad4, U32(0, 0x00d72ad4) | 1);
        ANIM_CALL_FRAME(0); rx_veh_tuning_registry_init(); ANIM_CALL_FRAME_END(0);
        RX_ATEXIT(0x00911ed0);
    }
    SU32(0, 0x00d92cd4, 0);
    SU32(sim, 0, 0x0093cc14);
    ANIM_CALL_FRAME(0); rx_registry_remove(sim + 0x20d0); ANIM_CALL_FRAME_END(0);
    ANIM_CALL_FRAME(0); rx_vehicle_sim_vector_destruct(sim + 0x1300); ANIM_CALL_FRAME_END(0);
    ANIM_CALL_FRAME(0); rx_vehicle_mechanism_destruct(sim + 0xd0); ANIM_CALL_FRAME_END(0);
    SU32(sim, 0, 0x0093c9d0);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0043c040, rx_motocross_sim_destruct)

/* 0043b450 RavenSimulationMotocross::sdtor (thiscall; flags; RET 4) -> this: 0043c040, freed under the allocator lock. Frame: push esi. */
rx_addr rx_motocross_sim_sdtor(rx_addr sim, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    ANIM_CALL_FRAME(0); rx_motocross_sim_destruct(sim); ANIM_CALL_FRAME_END(0);
    if (flags & 1) delete_locked(sim);
    rx_set_sp(sp0);
    return sim;
}
RX_FUNCTION(0x0043b450, rx_motocross_sim_sdtor)

/* ---- ScenarioAnimEntity ---- */

/* 006b7720 ScenarioAnimEntity::sdtor (thiscall; flags; RET 4) -> this: the three vtables (+0 0x96cee4, +0x90 0x96cf64, +0x140
 * 0x96cfa4), the entity's lock (+0x3e0) deleted, the character release 006248c0 (EDI = this), freed under the allocator lock.
 * Frame: push edi. */
rx_addr rx_scenario_anim_entity_sdtor(rx_addr e, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    SU32(e, 0, 0x0096cee4);
    SU32(e, 0x90, 0x0096cf64);
    SU32(e, 0x140, 0x0096cfa4);
    { const rx_addr s = rx_sp(); rx_set_sp(s - 4); SU32(rx_sp(), 0, e + 0x3e0); RX_OS(0x00916010, e + 0x3e0); rx_set_sp(s); }
    ANIM_CALL_FRAME(0); rx_rider_character_release_animation(e); ANIM_CALL_FRAME_END(0);
    if (flags & 1) delete_locked(e);
    rx_set_sp(sp0);
    return e;
}
RX_FUNCTION(0x006b7720, rx_scenario_anim_entity_sdtor)

/* AnimationEntity::vf9 00410fb0 (thiscall): a thunk, `jmp 00401ea0` (ModelEntity::vf9, fx_rdb.c): the entity leaves the scene and
 * the renderer lists. */
void rx_animation_entity_vf9(rx_addr e) {
    rx_fx_model_entity_vf9(e);
}
RX_FUNCTION(0x00410fb0, rx_animation_entity_vf9)

/* ---- the camera entities ---- */

/* 0063da30 (ESI = a camera entity): vtable 0x95a35c; the entity leaves the global camera list (doubly linked through +0x2d0 previous
 * / +0x2d4 next, the list's ends [0xa94184] / [0xa94188]); the name string +0x2d8 destroyed; the entity base vtables (+0x90
 * 0x95a2d8, +0 0x95a2e4) and both entity registries ([0xa95824] / [0xa95828] vf1 with the entity) told. */
void rx_camera_entity_destroy(rx_addr e) {
    const rx_addr entry0 = rx_sp();
    const rx_addr prev = U32(e, 0x2d0);
    SU32(e, 0, 0x0095a35c);
    if (prev != 0) SU32(prev, 0x2d4, U32(e, 0x2d4));
    else SU32(0, 0x00a94188, U32(e, 0x2d4));
    const rx_addr next = U32(e, 0x2d4);
    if (next != 0) SU32(next, 0x2d0, U32(e, 0x2d0));
    else SU32(0, 0x00a94184, U32(e, 0x2d0));
    SU32(e, 0x2d4, 0);
    SU32(e, 0x2d0, 0);
    ANIM_CALL_FRAME(0); crt_str_dtor(e + 0x2d8); ANIM_CALL_FRAME_END(0);
    SU32(e, 0x90, 0x0095a2d8);
    SU32(e, 0, 0x0095a2e4);
    for (int k = 0; k < 2; k++) {
        const rx_addr registry = U32(0, k ? 0x00a95828 : 0x00a95824);
        if (registry != 0) {
            const uint32_t arg = e;
            SU32(rx_sp(), -4, e);
            act_vcall(registry, 4, 1, &arg);
        }
    }
    rx_set_sp(entry0);
}
RX_FUNCTION(0x0063da30, rx_camera_entity_destroy)

/* 00646590 MXCameraEntity::sdtor (thiscall; flags; RET 4) -> this: 0063da30, freed (ReflexAlignedFree) when bit 0 is set. Frame: push esi. */
rx_addr rx_camera_entity_sdtor(rx_addr e, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    ANIM_CALL_FRAME(0); rx_camera_entity_destroy(e); ANIM_CALL_FRAME_END(0);
    if (flags & 1) engine_free(e);
    rx_set_sp(sp0);
    return e;
}
RX_FUNCTION(0x00646590, rx_camera_entity_sdtor)

/* 00649b70 MXThirdPersonCamera::sdtor (thiscall; flags; RET 4) -> this: 0063da30, freed under the allocator lock. Frame: push esi. */
rx_addr rx_third_person_camera_sdtor(rx_addr e, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    ANIM_CALL_FRAME(0); rx_camera_entity_destroy(e); ANIM_CALL_FRAME_END(0);
    if (flags & 1) delete_locked(e);
    rx_set_sp(sp0);
    return e;
}
RX_FUNCTION(0x00649b70, rx_third_person_camera_sdtor)

/* 0063db40 MXCameraEntity::vf9 (thiscall; shared by MXThirdPersonCamera): the entity's vf29 (slot 0x74); a scene-linked entity
 * (+0x2f8) leaves its scene (+0x10; 007d3040 with ESI = the entity); +0x70 = 0; both entity registries told (vf1); the block +0x18
 * freed. Frame: push esi. */
void rx_camera_entity_vf9(rx_addr e) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    act_vcall(e, 0x74, 0, NULL);
    if (U32(e, 0x2f8) != 0) {
        const rx_addr scene = U32(e, 0x10);
        SU32(rx_sp(), -4, scene);
        ANIM_CALL_FRAME(4); rx_scene_manager_remove_entity(e, scene); ANIM_CALL_FRAME_END(4);
    }
    SU32(e, 0x70, 0);
    for (int k = 0; k < 2; k++) {
        const rx_addr registry = U32(0, k ? 0x00a95828 : 0x00a95824);
        if (registry != 0) {
            const uint32_t arg = e;
            SU32(rx_sp(), -4, e);
            act_vcall(registry, 4, 1, &arg);
        }
    }
    engine_free(U32(e, 0x18));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0063db40, rx_camera_entity_vf9)

/* ---- the RiderAnimation destructor chain ---- */

/* 006b74d0 (EAX = base = the rider + 0xc940): the three clip slot arrays (0xe10-byte slots at +0x1708 / +0x3a918 / +0x3e158;
 * counts +0x4b4b0 / +0x4b4b4 / +0x4b4b8) have each slot's +0x58 and +0 cleared, then the counts are cleared. */
void rx_clip_slots_clear(rx_addr base) {
    static const struct { uint32_t first, count; } arrays[3] = {{0x1708, 0x4b4b0}, {0x3a918, 0x4b4b4}, {0x3e158, 0x4b4b8}};
    for (int a = 0; a < 3; a++) {
        rx_addr slot = base + arrays[a].first;
        for (uint32_t i = 0; i < U32(base, arrays[a].count); i++, slot += 0xe10) {
            SU32(slot, 0x58, 0);
            SU32(slot, 0, 0);
        }
    }
    SU32(base, 0x4b4b0, 0);
    SU32(base, 0x4b4b4, 0);
    SU32(base, 0x4b4b8, 0);
}
RX_FUNCTION(0x006b74d0, rx_clip_slots_clear)

/* 006b70b0 (ESI = base = the rider + 0xc940): the slots below the three array ends cleared, walking down: 16 slots below +0x4c258,
 * 4 below +0x3e158 and 65 below +0x3a918 (each +0x58 and +0 = 0). */
void rx_clip_slots_clear_tail(rx_addr base) {
    static const struct { uint32_t end, count; } runs[3] = {{0x4c258, 16}, {0x3e158, 4}, {0x3a918, 65}};
    for (int r = 0; r < 3; r++) {
        rx_addr slot = base + runs[r].end;
        for (uint32_t i = 0; i < runs[r].count; i++) {
            slot -= 0xe10;
            SU32(slot, 0x58, 0);
            SU32(slot, 0, 0);
        }
    }
}
RX_FUNCTION(0x006b70b0, rx_clip_slots_clear_tail)

/* 006372a0 (ESI = the pose job record = the rider + 0x5b54c): the record's vtable is set to 0x9587ec; it leaves the physics world
 * of the view the engine is in (00769900 with the record on the world +0x20134 of the holder): the game's holder [0xa946b8] +0x5a54
 * when the engine's frame object ([0xa95844]) mode object +0x20 is in mode 1 (+0x34), else its holder +0x20c8 when the +0x1c mode
 * object is in mode 1, else the original reads the field at address 0 (no holder); then the final vtable 0x93cbf4. */
void rx_rider_pose_job_release(rx_addr rec) {
    SU32(rec, 0, 0x009587ec);
    const rx_addr frame = U32(0, 0x00a95844);
    rx_addr holder;
    if (U32(U32(frame, 0x20), 0x34) == 1) holder = U32(U32(0, 0x00a946b8), 0x5a54);
    else if (U32(U32(frame, 0x1c), 0x34) == 1) holder = U32(U32(0, 0x00a946b8), 0x20c8);
    else holder = 0;
    const rx_addr pw = U32(holder, 0x20134);
    SU32(rx_sp(), -4, rec);
    ANIM_CALL_FRAME(4); rx_world_physics_remove_listener(pw, rec); ANIM_CALL_FRAME_END(4);
    SU32(rec, 0, 0x0093cbf4);
}
RX_FUNCTION(0x006372a0, rx_rider_pose_job_release)

/* 00618c80 (ESI = a name map {+0 the allocator block, +0x18 the header node, +0x1c the count}): the map emptied (00705070 over the
 * whole range {allocator, first node} .. {allocator, header}), its header (+0x18) freed and +0x18 / +0x1c cleared, then the
 * allocator block (+0) freed. Frame: push ebp; mov ebp, esp; and esp, -8; sub esp, 8 (the erase's out pair). */
void rx_name_map_release(rx_addr map) {
    const rx_addr entry0 = rx_sp();
    const rx_addr E = ((entry0 - 4u) & ~7u) - 8u;
    rx_set_sp(E);
    const rx_addr header = U32(map, 0x18);
    const rx_addr alloc = U32(map, 0);
    const rx_addr first = U32(header, 0);
    ANIM_CALL_FRAME(20); rx_channel_map_erase_range(map, E, alloc, first, alloc, header); ANIM_CALL_FRAME_END(20);
    engine_free(U32(map, 0x18));
    const rx_addr block = U32(map, 0);
    SU32(map, 0x18, 0);
    SU32(map, 0x1c, 0);
    engine_free(block);
    rx_set_sp(entry0);
}
RX_FUNCTION(0x00618c80, rx_name_map_release)

/* 00804e40 (ESI = a sound instance): not started (+0 == 0): nothing; otherwise, unless already stopped (+0 == 3), the state becomes
 * 3 and a held FMOD channel (+0x144) is stopped (AUDIO BOUNDARY, see the file header; the error report 00805570 with a zero
 * flag does nothing) and cleared; the instance gets flag bit 2 (+4). */
void rx_sound_instance_stop(rx_addr s) {
    const uint32_t state = U32(s, 0);
    if (state == 0) return;
    if (state != 3) {
        const rx_addr channel = U32(s, 0x144);
        SU32(s, 0, 3);
        if (channel != 0) {
#ifdef RX_ORACLE
            const uint32_t a[1] = {channel};
            rx_call_original(0x00900306, RX_STDCALL, 0, 1, a);   /* FMOD::Channel::stop */
#endif
            SU32(s, 0x144, 0);
        }
    }
    SU32(s, 4, U32(s, 4) | 4);
}
RX_FUNCTION(0x00804e40, rx_sound_instance_stop)

/* 004245f0 (EAX = a rider's sound emitter set = the rider + 0x5d0): the buffer +0xa80c freed (locked CRT free, unconditional);
 * the sixteen sub-objects (8 bytes from +8) destroyed through their vf1 (slot 4); a sound instance +0x2f0 stopped (00804e40) and
 * detached (+0x2f4 = -1, +0x2f0 = 0); the sixteen shared records (pairs {vtable, counted pointer} below +0x88) get their vtable
 * 0x93a650 and drop a reference: the last reference removes the record's body from its world (0042af70 on the world's +0x1954
 * vector with the record's index +8) and frees the record. Frame: push ebx / ebp / esi / edi. */
void rx_rider_sound_set_release(rx_addr set) {
    const rx_addr entry0 = rx_sp();
    rx_set_sp(entry0 - 16);
    delete_locked(U32(set, 0xa80c));
    for (uint32_t i = 0; i < 16; i++) act_vcall(set + 8 + 8 * i, 4, 0, NULL);
    const rx_addr sound = U32(set, 0x2f0);
    if (sound != 0) {
        ANIM_CALL_FRAME(0); rx_sound_instance_stop(sound); ANIM_CALL_FRAME_END(0);
        SU32(set, 0x2f4, 0xffffffffu);
        SU32(set, 0x2f0, 0);
    }
    for (rx_addr rec = set + 0x80; rec >= set + 8; rec -= 8) {
        const rx_addr counted = U32(rec, 4);
        SU32(rec, 0, 0x0093a650);
        if (counted != 0) {
            const uint32_t left = U32(counted, 0) - 1;
            SU32(counted, 0, left);
            if (left == 0) {
                const rx_addr body = U32(rec, 4);
                if (body != 0) {
                    const rx_addr vec = U32(body, 4) + 0x1954;
                    SU32(rx_sp(), -4, U32(body, 8));
                    ANIM_CALL_FRAME(4); rx_dyn_records_remove(vec, U32(body, 8)); ANIM_CALL_FRAME_END(4);
                    engine_free(body);
                }
            }
        }
    }
    rx_set_sp(entry0);
}
RX_FUNCTION(0x004245f0, rx_rider_sound_set_release)

/* ---- the Animator objects a character data block owns (+0x224: an ExpressionBoneAnimator, which owns an InverseKinematicsAnimator
 * at +0x28) ---- */

/* 006afc00 (ECX = an inverse-kinematics chain record, 0x80 bytes): its +0x60 index is invalidated (-1) and its two buffers (+0x68,
 * +0x6c) freed. Frame: push esi; both arguments stay pushed until the end. */
void rx_ik_chain_destroy(rx_addr chain) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);                                         /* push esi */
    SU32(chain, 0x60, 0xffffffffu);
    const rx_addr s = rx_sp();
    rx_set_sp(s - 4); SU32(rx_sp(), 0, U32(chain, 0x68)); ANIM_CALL_FRAME(0); rx_reflex_aligned_free(U32(chain, 0x68)); ANIM_CALL_FRAME_END(0);
    rx_set_sp(s - 8); SU32(rx_sp(), 0, U32(chain, 0x6c)); ANIM_CALL_FRAME(0); rx_reflex_aligned_free(U32(chain, 0x6c)); ANIM_CALL_FRAME_END(0);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006afc00, rx_ik_chain_destroy)

/* 006afcf0 (ECX = an InverseKinematicsAnimator): vtable 0x95efd8; the chain array (+0x64: an `new[]` block with its count at -0x10,
 * 0x80-byte records) destroyed last to first (vector destructor iterator 00402500 with 006afc00) and its block (at -0x10) freed
 * under the allocator lock (007105c0); the buffer +0x74 freed; the owned object +0x5c deleted (vf0 (1)); then the base Animator
 * vtable 0x95ef10 and +0x5c = 0. Frame: push esi (push edi around the array release). */
void rx_ik_animator_destroy(rx_addr a) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    const rx_addr chains = U32(a, 0x64);
    SU32(a, 0, 0x0095efd8);
    if (chains != 0) {
        const rx_addr s = rx_sp();
        rx_set_sp(s - 4);                                       /* push edi */
        const uint32_t count = U32(chains, (uint32_t)-0x10);
        ANIM_CALL_FRAME(16); rx_fx_vector_dtor_iterator(chains, 0x80, (int32_t)count, 0x006afc00); ANIM_CALL_FRAME_END(16);
        delete_locked(chains - 0x10);
        rx_set_sp(s);
    }
    engine_free(U32(a, 0x74));
    const rx_addr owned = U32(a, 0x5c);
    if (owned != 0) { const uint32_t one = 1; act_vcall(owned, 0, 1, &one); }
    SU32(a, 0, 0x0095ef10);
    SU32(a, 0x5c, 0);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006afcf0, rx_ik_animator_destroy)

/* 006afcd0 InverseKinematicsAnimator::sdtor (thiscall; flags; RET 4) -> this: 006afcf0, freed (ReflexAlignedFree) when bit 0 is set.
 * Frame: push esi. */
rx_addr rx_ik_animator_sdtor(rx_addr a, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    ANIM_CALL_FRAME(0); rx_ik_animator_destroy(a); ANIM_CALL_FRAME_END(0);
    if (flags & 1) engine_free(a);
    rx_set_sp(sp0);
    return a;
}
RX_FUNCTION(0x006afcd0, rx_ik_animator_sdtor)

/* 006aca10 (ECX = an ExpressionBoneAnimator): vtable 0x95ef44; the owned IK animator (+0x28) deleted (vf0 (1)); the pointer vector
 * at +0x44 {proxy +0, first +0xc, last +0x10, end +0x14} destroyed as MSVC 9 does with checked iterators: every element freed (the
 * iterator checks call _invalid_parameter_noinfo), the whole range erased (007cf450), the storage and the proxy freed; then the base
 * Animator vtable 0x95ef10. Frame: push ebp; and esp, -8; sub esp, 0x10; push ebx / ebp / esi / edi. */
void rx_expression_animator_destroy(rx_addr a) {
    const rx_addr entry0 = rx_sp();
    const rx_addr F = ((entry0 - 4u) & ~7u) - 0x10u;
    rx_set_sp(F - 16);
    SU32(a, 0, 0x0095ef44);
    const rx_addr owned = U32(a, 0x28);
    if (owned != 0) { const uint32_t one = 1; act_vcall(owned, 0, 1, &one); }
    SU32(a, 0x28, 0);
    const rx_addr vec = a + 0x44;
    rx_addr cursor = U32(vec, 0xc);
    if (cursor > U32(vec, 0x10)) crt_invalid_parameter_noinfo();
    const rx_addr proxy = U32(vec, 0);
    rx_addr last = U32(vec, 0x10);
    if (U32(vec, 0xc) > last) crt_invalid_parameter_noinfo();
    if (proxy == 0 || proxy != U32(vec, 0)) crt_invalid_parameter_noinfo();
    while (cursor != last) {
        if (proxy == 0) crt_invalid_parameter_noinfo();
        if (!(cursor < U32(proxy != 0 ? U32(proxy, 0) : 0, 0x10))) crt_invalid_parameter_noinfo();
        engine_free(U32(cursor, 0));
        if (proxy == 0) crt_invalid_parameter_noinfo();
        if (!(cursor < U32(proxy != 0 ? U32(proxy, 0) : 0, 0x10))) crt_invalid_parameter_noinfo();
        cursor += 4;
        last = U32(vec, 0x10);
        if (U32(vec, 0xc) > last) crt_invalid_parameter_noinfo();
        if (proxy == 0 || proxy != U32(vec, 0)) crt_invalid_parameter_noinfo();
    }
    const rx_addr range_last = U32(vec, 0x10);
    if (U32(vec, 0xc) > range_last) crt_invalid_parameter_noinfo();
    const rx_addr range_first = U32(vec, 0xc);
    if (range_first > U32(vec, 0x10)) crt_invalid_parameter_noinfo();
    ANIM_CALL_FRAME(20); (void)phys_vector_erase(vec, F + 8, range_first, range_last); ANIM_CALL_FRAME_END(20);
    const rx_addr storage = U32(vec, 0xc);
    if (storage != 0) engine_free(storage);
    const rx_addr proxy_block = U32(vec, 0);
    SU32(vec, 0xc, 0);
    SU32(vec, 0x10, 0);
    SU32(vec, 0x14, 0);
    engine_free(proxy_block);
    SU32(a, 0, 0x0095ef10);
    rx_set_sp(entry0);
}
RX_FUNCTION(0x006aca10, rx_expression_animator_destroy)

/* 006ac9f0 ExpressionBoneAnimator::sdtor (thiscall; flags; RET 4) -> this: 006aca10, freed (ReflexAlignedFree) when bit 0 is set.
 * Frame: push esi. */
rx_addr rx_expression_animator_sdtor(rx_addr a, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    ANIM_CALL_FRAME(0); rx_expression_animator_destroy(a); ANIM_CALL_FRAME_END(0);
    if (flags & 1) engine_free(a);
    rx_set_sp(sp0);
    return a;
}
RX_FUNCTION(0x006ac9f0, rx_expression_animator_sdtor)

/* 006b5820 ProceduralAnimator::sdtor (thiscall; flags; RET 4) -> this: vtable 0x95f00c; the owned object +0x28 deleted (vf0 (1));
 * +0x28 and +0x68 cleared; the base Animator vtable 0x95ef10; freed (ReflexAlignedFree) when bit 0 is set. Frame: push esi. */
rx_addr rx_procedural_animator_sdtor(rx_addr a, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    const rx_addr owned = U32(a, 0x28);
    SU32(a, 0, 0x0095f00c);
    if (owned != 0) { const uint32_t one = 1; act_vcall(owned, 0, 1, &one); }
    SU32(a, 0x28, 0);
    SU32(a, 0x68, 0);
    SU32(a, 0, 0x0095ef10);
    if (flags & 1) engine_free(a);
    rx_set_sp(sp0);
    return a;
}
RX_FUNCTION(0x006b5820, rx_procedural_animator_sdtor)

/* 006afad0 FeatherstoneAnimator::sdtor (thiscall; flags; RET 4) -> this: vtable 0x95efa0; the owned object +0x30 deleted (vf0 (1));
 * the sound emitter set at +0x40 released (004245f0, EAX = the set); the base Animator vtable 0x95ef10; freed under the allocator
 * lock when bit 0 is set. Frame: push esi. */
rx_addr rx_featherstone_animator_sdtor(rx_addr a, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    const rx_addr owned = U32(a, 0x30);
    SU32(a, 0, 0x0095efa0);
    if (owned != 0) { const uint32_t one = 1; act_vcall(owned, 0, 1, &one); }
    ANIM_CALL_FRAME(0); rx_rider_sound_set_release(a + 0x40); ANIM_CALL_FRAME_END(0);
    SU32(a, 0, 0x0095ef10);
    if (flags & 1) delete_locked(a);
    rx_set_sp(sp0);
    return a;
}
RX_FUNCTION(0x006afad0, rx_featherstone_animator_sdtor)

/* 0083c6e0 (ECX = a SwitchAnimator): vtable 0x975fb8; +0x58 / +0x5c cleared; the child animators (a pointer vector +0xc4 .. +0xc8,
 * checked index) deleted (vf0 (1)) and their slots cleared, the vector storage (+0xc4) and its proxy (+0xb8) freed; the second
 * vector (+0xa0 .. , proxy +0x94) freed likewise; the circular node list (header +0x8c, size +0x90) emptied - every node freed
 * under the allocator lock taken twice - and its header and the table +0x78 freed; vtable 0x975f1c; the two owned objects
 * (+0x58, +0x5c: null here, cleared above) deleted if set; the base Animator vtable 0x95ef10. Frame: push ebx / ebp / esi / edi. */
void rx_switch_animator_destroy(rx_addr a) {
    const rx_addr entry0 = rx_sp();
    rx_set_sp(entry0 - 16);
    SU32(a, 0, 0x00975fb8);
    SU32(a, 0x58, 0);
    SU32(a, 0x5c, 0);
    uint32_t i = 0;
    if (((U32(a, 0xc8) - U32(a, 0xc4)) >> 2) != 0) {
        do {
            if (!(i < ((U32(a, 0xc8) - U32(a, 0xc4)) >> 2))) crt_invalid_parameter_noinfo();
            const rx_addr child = U32(U32(a, 0xc4), 4 * i);
            if (child != 0) { const uint32_t one = 1; act_vcall(child, 0, 1, &one); }
            if (!(i < ((U32(a, 0xc8) - U32(a, 0xc4)) >> 2))) crt_invalid_parameter_noinfo();
            SU32(U32(a, 0xc4), 4 * i, 0);
            i++;
        } while (i < ((U32(a, 0xc8) - U32(a, 0xc4)) >> 2));
    }
    if (U32(a, 0xc4) != 0) engine_free(U32(a, 0xc4));
    const rx_addr proxy1 = U32(a, 0xb8);
    SU32(a, 0xc4, 0);
    SU32(a, 0xc8, 0);
    SU32(a, 0xcc, 0);
    engine_free(proxy1);
    if (U32(a, 0xa0) != 0) engine_free(U32(a, 0xa0));
    const rx_addr proxy2 = U32(a, 0x94);
    SU32(a, 0xa0, 0);
    SU32(a, 0xa4, 0);
    SU32(a, 0xa8, 0);
    engine_free(proxy2);
    const rx_addr header = U32(a, 0x8c);
    rx_addr node = U32(header, 0);
    SU32(header, 0, header);
    SU32(U32(a, 0x8c), 4, U32(a, 0x8c));
    SU32(a, 0x90, 0);
    while (node != U32(a, 0x8c)) {
        const rx_addr next = U32(node, 0);
        if (U8(0, 0x00da67d8) != 0) loop_enter_cs(0x00d61704);
        if (U8(0, 0x00da67d8) != 0) loop_enter_cs(0x00d61704);
        ANIM_CALL_FRAME(4); SU32(rx_sp(), 4, node); crt_aligned_free(node); ANIM_CALL_FRAME_END(4);
        if (U8(0, 0x00da67d8) != 0) loop_leave_cs(0x00d61704);
        if (U8(0, 0x00da67d8) != 0) loop_leave_cs(0x00d61704);
        node = next;
    }
    engine_free(U32(a, 0x8c));
    const rx_addr table = U32(a, 0x78);
    SU32(a, 0x8c, 0);
    engine_free(table);
    SU32(a, 0, 0x00975f1c);
    if (U32(a, 0x58) != 0) { const uint32_t one = 1; act_vcall(U32(a, 0x58), 0, 1, &one); SU32(a, 0x58, 0); }
    if (U32(a, 0x5c) != 0) { const uint32_t one = 1; act_vcall(U32(a, 0x5c), 0, 1, &one); SU32(a, 0x5c, 0); }
    SU32(a, 0, 0x0095ef10);
    rx_set_sp(entry0);
}
RX_FUNCTION(0x0083c6e0, rx_switch_animator_destroy)

/* 0083c6c0 SwitchAnimator::sdtor (thiscall; flags; RET 4) -> this: 0083c6e0, freed (ReflexAlignedFree) when bit 0 is set. Frame: push esi. */
rx_addr rx_switch_animator_sdtor(rx_addr a, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    ANIM_CALL_FRAME(0); rx_switch_animator_destroy(a); ANIM_CALL_FRAME_END(0);
    if (flags & 1) engine_free(a);
    rx_set_sp(sp0);
    return a;
}
RX_FUNCTION(0x0083c6c0, rx_switch_animator_sdtor)

/* 00833020 BlendAnimator::sdtor (thiscall; flags; RET 4) -> this: vtable 0x975f1c; the two input animators (+0x58, +0x5c) deleted
 * (vf0 (1)) and cleared; the base Animator vtable 0x95ef10; freed (ReflexAlignedFree) when bit 0 is set. Frame: push esi. */
rx_addr rx_blend_animator_sdtor(rx_addr a, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    SU32(a, 0, 0x00975f1c);
    if (U32(a, 0x58) != 0) { const uint32_t one = 1; act_vcall(U32(a, 0x58), 0, 1, &one); SU32(a, 0x58, 0); }
    if (U32(a, 0x5c) != 0) { const uint32_t one = 1; act_vcall(U32(a, 0x5c), 0, 1, &one); SU32(a, 0x5c, 0); }
    SU32(a, 0, 0x0095ef10);
    if (flags & 1) engine_free(a);
    rx_set_sp(sp0);
    return a;
}
RX_FUNCTION(0x00833020, rx_blend_animator_sdtor)

/* 00834600 AdditiveAnimator::sdtor (thiscall; flags; RET 4) -> this: vtable 0x975f50; the base (+0x2c) and additive (+0x30)
 * animators deleted (vf0 (1)); the base Animator vtable 0x95ef10; freed when bit 0 is set. Frame: push esi. */
rx_addr rx_additive_animator_sdtor(rx_addr a, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    const rx_addr base = U32(a, 0x2c);
    SU32(a, 0, 0x00975f50);
    if (base != 0) { const uint32_t one = 1; act_vcall(base, 0, 1, &one); }
    if (U32(a, 0x30) != 0) { const uint32_t one = 1; act_vcall(U32(a, 0x30), 0, 1, &one); }
    SU32(a, 0, 0x0095ef10);
    if (flags & 1) engine_free(a);
    rx_set_sp(sp0);
    return a;
}
RX_FUNCTION(0x00834600, rx_additive_animator_sdtor)

/* 0083b3a0 MotionAnimator::sdtor (thiscall; flags; RET 4) -> this: vtable 0x975ee8; the buffer +0x30 freed and cleared; the base
 * Animator vtable 0x95ef10; freed when bit 0 is set. Frame: push esi. */
rx_addr rx_motion_animator_sdtor(rx_addr a, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    SU32(a, 0, 0x00975ee8);
    engine_free(U32(a, 0x30));
    SU32(a, 0x30, 0);
    SU32(a, 0, 0x0095ef10);
    if (flags & 1) engine_free(a);
    rx_set_sp(sp0);
    return a;
}
RX_FUNCTION(0x0083b3a0, rx_motion_animator_sdtor)

/* 0083d360 (ECX = a PerformanceAnimator): vtable 0x976024; its clip block animators (the pointer array +0x74, count +0x8c) deleted
 * (vf0 (1)); the array and the table +0x78 freed; the count (+0x8c), +0x90, +0x58 and +0x5c cleared; the base Animator vtable
 * 0x95ef10. Frame: push ebx / esi / edi. */
void rx_performance_animator_destroy(rx_addr a) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 12);
    SU32(a, 0, 0x00976024);
    for (int32_t i = 0; i < I32(a, 0x8c); i++) {
        const rx_addr block = U32(U32(a, 0x74), 4u * (uint32_t)i);
        if (block != 0) { const uint32_t one = 1; act_vcall(block, 0, 1, &one); }
    }
    engine_free(U32(a, 0x74));
    const rx_addr table = U32(a, 0x78);
    SU32(a, 0x8c, 0);
    engine_free(table);
    SU32(a, 0x90, 0);
    SU32(a, 0x58, 0);
    SU32(a, 0x5c, 0);
    SU32(a, 0, 0x0095ef10);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0083d360, rx_performance_animator_destroy)

/* 0083cf70 PerformanceAnimator::sdtor (thiscall; flags; RET 4) -> this: 0083d360, freed when bit 0 is set. Frame: push esi. */
rx_addr rx_performance_animator_sdtor(rx_addr a, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    ANIM_CALL_FRAME(0); rx_performance_animator_destroy(a); ANIM_CALL_FRAME_END(0);
    if (flags & 1) engine_free(a);
    rx_set_sp(sp0);
    return a;
}
RX_FUNCTION(0x0083cf70, rx_performance_animator_sdtor)

/* 0082e040 (ESI = a character entity base): its buffers released: the object +0x224 (vf0 (1)), +0x220, +0x23c, the locked-free
 * block +0x228, +0x22c, +0x294, and, after the six fields are cleared, the locked-free block +0x1f8 and +0x1fc (both cleared). */
void rx_character_buffers_release(rx_addr c) {
    const rx_addr entry0 = rx_sp();
    rx_set_sp(entry0 - 4);                                      /* push ebx */
    const rx_addr obj = U32(c, 0x224);
    if (obj != 0) { const uint32_t one = 1; act_vcall(obj, 0, 1, &one); }
    if (U32(c, 0x220) != 0) engine_free(U32(c, 0x220));
    if (U32(c, 0x23c) != 0) engine_free(U32(c, 0x23c));
    rx_set_sp(entry0 - 12);                                     /* push ebp; push edi */
    if (U32(c, 0x228) != 0) delete_locked(U32(c, 0x228));
    if (U32(c, 0x22c) != 0) engine_free(U32(c, 0x22c));
    if (U32(c, 0x294) != 0) engine_free(U32(c, 0x294));
    const rx_addr block = U32(c, 0x1f8);
    SU32(c, 0x294, 0);
    SU32(c, 0x224, 0);
    SU32(c, 0x220, 0);
    SU32(c, 0x23c, 0);
    SU32(c, 0x228, 0);
    SU32(c, 0x22c, 0);
    if (block != 0) delete_locked(block);
    rx_set_sp(entry0 - 4);                                      /* pop edi; pop ebp */
    if (U32(c, 0x1fc) != 0) engine_free(U32(c, 0x1fc));
    SU32(c, 0x1fc, 0);
    SU32(c, 0x1f8, 0);
    rx_set_sp(entry0);
}
RX_FUNCTION(0x0082e040, rx_character_buffers_release)

/* 0082d100 (ECX = the character entity): the base destructor of the character entities: vtables +0 0x97605c, +0x90 0x9760dc; the
 * attached character data +0x144 released twice (0082e040), then its name map +0x248 freed (007fa1e0), its lock +0x208 deleted
 * and the block itself freed; the model instance +0x170 destroyed (007d4030), the name map +0x148 freed (007fa1e0); the entity
 * vtables 0x93708c / 0x95a2e4 and both entity registries ([0xa95824] / [0xa95828] vf1 with the entity). Frame: push esi, edi. */
void rx_character_entity_destroy(rx_addr e) {
    const rx_addr entry0 = rx_sp();
    rx_set_sp(entry0 - 8);
    rx_addr data = U32(e, 0x144);
    SU32(e, 0, 0x0097605c);
    SU32(e, 0x90, 0x009760dc);
    if (data != 0) { ANIM_CALL_FRAME(0); rx_character_buffers_release(data); ANIM_CALL_FRAME_END(0); }
    data = U32(e, 0x144);
    if (data != 0) {
        ANIM_CALL_FRAME(0); rx_character_buffers_release(data); ANIM_CALL_FRAME_END(0);
        { const rx_addr s = rx_sp(); rx_set_sp(s - 4); rx_name_hash_free(data + 0x248); rx_set_sp(s); }
        { const rx_addr s = rx_sp(); rx_set_sp(s - 8); SU32(rx_sp(), 4, data + 0x208); RX_OS(0x00916010, data + 0x208); rx_set_sp(s); }
        engine_free(data);
    }
    ANIM_CALL_FRAME(0); rx_model_instance_destroy(e + 0x170, 0); ANIM_CALL_FRAME_END(0);
    { const rx_addr s = rx_sp(); rx_set_sp(s - 4); rx_name_hash_free(e + 0x148); rx_set_sp(s); }
    SU32(e, 0x90, 0x0093708c);
    SU32(e, 0, 0x0095a2e4);
    for (int k = 0; k < 2; k++) {
        const rx_addr registry = U32(0, k ? 0x00a95828 : 0x00a95824);
        if (registry != 0) {
            const uint32_t arg = e;
            SU32(rx_sp(), -4, e);
            act_vcall(registry, 4, 1, &arg);
        }
    }
    rx_set_sp(entry0);
}
RX_FUNCTION(0x0082d100, rx_character_entity_destroy)

/* the (pointer, base) block pairs the RiderAnimation destructor frees at its end, in the original's order: the pointer array
 * (+p: freed when set, then +p, +p + 4, +p + 8 cleared) and its base block (+q: freed always) */
static const uint16_t rider_block_pairs[12][2] = {
    {0x554, 0x548}, {0x52c, 0x520}, {0x504, 0x4f8}, {0x4dc, 0x4d0}, {0x4b4, 0x4a8}, {0x498, 0x48c},
    {0x478, 0x46c}, {0x398, 0x38c}, {0x36c, 0x360}, {0x340, 0x334}, {0x314, 0x308}, {0x2e8, 0x2dc},
};

/* 00618cd0 (ECX = the rider animation): see the file header. Frame: push ebx / ebp / edi (+ push esi from the tree release to the
 * sound set); the exit is a tail jump to 0082d100. */
void rx_rider_animation_destroy(rx_addr ra) {
    const rx_addr entry0 = rx_sp();
    SU32(0, 0x00a94088, U32(0, 0x00a94088) - 1);
    rx_set_sp(entry0 - 12);
    const uint32_t slot = U32(ra, 0x2bc);
    SU32(ra, 0, 0x00958724);
    SU32(ra, 0x90, 0x009587a4);
    SU32(ra, 0x2b0, 0x009587e4);
    SU32(rx_rc_tab(0x00a9408c, 4, slot), 0, 0);   /* (w-rc-core) slots 12..31 in side storage */
    const rx_addr slots = ra + 0xc940;
    ANIM_CALL_FRAME(0); rx_clip_slots_clear(slots); ANIM_CALL_FRAME_END(0);
    const rx_addr job = U32(ra, 0x5b6dc);
    if (job != 0) { const uint32_t one = 1; act_vcall(job, 0, 1, &one); }
    /* the tree +0x5bb6c: its nodes freed from the header's first child, the header relinked to itself, then the release */
    const rx_addr tree = ra + 0x5bb6c;
    rx_set_sp(entry0 - 16);                                     /* push esi */
    {
        const rx_addr node = U32(U32(ra, 0x5bb84), 4);
        SU32(rx_sp(), -4, node);
        ANIM_CALL_FRAME(4); rx_tree_free_subtree(tree, node); ANIM_CALL_FRAME_END(4);
    }
    {
        rx_addr head = U32(tree, 0x18);
        SU32(head, 4, head);
        head = U32(tree, 0x18);
        SU32(tree, 0x1c, 0);
        SU32(head, 0, head);
        head = U32(tree, 0x18);
        SU32(head, 8, head);
    }
    ANIM_CALL_FRAME(0); rx_rider_tree_release(tree); ANIM_CALL_FRAME_END(0);
    {   /* three arrays freed, the arguments pushed one after the other */
        const rx_addr s = rx_sp();
        rx_set_sp(s - 4); SU32(rx_sp(), 0, U32(ra, 0x5b6bc)); ANIM_CALL_FRAME(0); rx_reflex_aligned_free(U32(ra, 0x5b6bc)); ANIM_CALL_FRAME_END(0);
        rx_set_sp(s - 8); SU32(rx_sp(), 0, U32(ra, 0x5b6c0)); ANIM_CALL_FRAME(0); rx_reflex_aligned_free(U32(ra, 0x5b6c0)); ANIM_CALL_FRAME_END(0);
        rx_set_sp(s - 12); SU32(rx_sp(), 0, U32(ra, 0x5b6c4)); ANIM_CALL_FRAME(0); rx_reflex_aligned_free(U32(ra, 0x5b6c4)); ANIM_CALL_FRAME_END(0);
        rx_set_sp(s);
    }
    ANIM_CALL_FRAME(0); rx_rider_pose_job_release(ra + 0x5b54c); ANIM_CALL_FRAME_END(0);
    ANIM_CALL_FRAME(0); rx_clip_slots_clear_tail(slots); ANIM_CALL_FRAME_END(0);
    for (int k = 0; k < 4; k++) {
        ANIM_CALL_FRAME(0); rx_name_map_release(ra + (k == 0 ? 0xafdc : k == 1 ? 0xafbc : k == 2 ? 0xaf9c : 0xaf7c)); ANIM_CALL_FRAME_END(0);
    }
    ANIM_CALL_FRAME(0); rx_registry_remove(ra + 0xaea0); ANIM_CALL_FRAME_END(0);
    ANIM_CALL_FRAME(0); rx_rider_sound_set_release(ra + 0x5d0); ANIM_CALL_FRAME_END(0);
    rx_set_sp(entry0 - 12);                                     /* pop esi */
    for (int i = 0; i < 12; i++) {
        const uint32_t p = rider_block_pairs[i][0], q = rider_block_pairs[i][1];
        const rx_addr arr = U32(ra, p);
        if (arr != 0) engine_free(arr);
        const rx_addr base = U32(ra, q);
        SU32(ra, p, 0);
        SU32(ra, p + 4, 0);
        SU32(ra, p + 8, 0);
        engine_free(base);
    }
    rx_set_sp(entry0);                                          /* pop edi / ebp / ebx; jmp 0082d100 */
    rx_character_entity_destroy(ra);
}
RX_FUNCTION(0x00618cd0, rx_rider_animation_destroy)

/* 00618c30 RiderAnimation::sdtor (thiscall; flags; RET 4) -> this: 00618cd0, freed under the allocator lock. Frame: push esi. */
rx_addr rx_rider_animation_sdtor(rx_addr ra, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    ANIM_CALL_FRAME(0); rx_rider_animation_destroy(ra); ANIM_CALL_FRAME_END(0);
    if (flags & 1) delete_locked(ra);
    rx_set_sp(sp0);
    return ra;
}
RX_FUNCTION(0x00618c30, rx_rider_animation_sdtor)
