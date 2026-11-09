/* anim_entity_dtors.c -- the destructors and release virtuals of the scene entity classes (2026-10-26, agent w-teardown). The activity
 * area's teardown (Activity::vf6 -> rx_scene_subscene_release 007d1470, build_activity_teardown.c) takes each entity of the area's
 * subscene out of the scene (007d3040), calls its vf9 (the release) and its scalar deleting destructor (vf0 (1)); a career "Continue"
 * or the next event of a session does this for a Free Ride / Waypoint / race area. Every class ends in the Entity base: the vtable
 * 0x95a2e4 stored and both entity registries ([0xa95824] / [0xa95828], the renderer's lists) told (vf1 with the entity).
 *
 *   Entity::vf0 00406ff0 (AnchorEntity, GeneralStorage, MudManager, FreeRideStart, ReflectionMapGen, LevelInfo, DistanceFog, ShadowMap, HDR,
 *                         OverlayColor, ChallengeSpecs, Water ...)    ModelEntity::sdtor 00401fd0 (+ HFBump, MassiveAd, Sky, VolumeFog ...)
 *   Entity::vf9 00793930                                              TDFEntity 00407110 / 004072c0    MusicTrackListEntity 00407130
 *   MXTrigger 00409970 (00409990 in waypoint_gate.c)                  MXCameraPath 0063d560 / 0063e6a0
 *   MXReplayTowerCamera 00648dd0 / 00648df0                           particle effects 00692190, 00692410, 00696580, 006965a0
 *   ShadowMapEntity::vf9 0040ad60
 *   AnimEntitySplineAnimLoop 006ba5f0 and its particles vf9 006bbff0  CrewChief 006bd520 / 006bd570
 *   CollisionEntity 00819000 / 00819050 / 00819100 (the static objects it added to the physics world are removed: phys_static_objects.c)
 *
 * PRODUCT BOUNDARIES (the oracle keeps running the originals; evidence in core/plan/QUEUE.md, w-teardown):
 *   - LocalizedPackageReferenceEntity (00416950 / 00416c30 / 00416d70) and the activity audio manager entities (ActivityAudioManagerEntity
 *     0068d9d0 / 0068dcc0 / 0068dce0 and the per-activity classes): their type ids (0x1094, 0x1089 .. 0x1093) are in build_scene.c's
 *     outside_core list, so the factory returns no object for them and the subscene never holds one in the product.
 *   - the audio managers [0xa95824] (AudioCollisionManager::vf1 0067b920) and [0xa95828] (VehicleAudioManager::vf1 00680e80): the registries
 *     are told through act_vcall; both are null in the product: FUN_0067da10 (the audio world setup) is their only writer (decompile: DAT_00a95824 / DAT_00a95828) and the core never runs it.
 *   - the UI event-handler proxies (005ef970 / 005effa0 / 0060edb0 / 006104e0): UI pages' subscriptions to the activity channels; the product
 *     has no UI manager ([0xa93fcc] = 0), nothing of that kind is ever subscribed.
 *   - a particle effect's sound emitter (+0x294): never made in the product (scene_fx_entities.c stores 0). */
#include "anim_entity_dtors.h"
#include "anim_rider_dtors.h"
#include "anim_scenario.h"
#include "anim_scenario_misc.h"
#include "anim.h"
#include "fx.h"
#include "fx_util.h"
#include "race.h"
#include "world_node.h"
#include "loop_platform.h"
#include "vehicle_atv_destroy.h"
#include "phys_static_objects.h"
#include "build_activity_util.h"
#include "build_boundary_audio.h"
#include "build_alloc.h"
#include "crt.h"
#include "../src/rx_os.h"
#include <string.h>

void rx_waypoint_trigger_destroy(rx_addr e);                     /* 00409990 (waypoint_gate.c) */

/* ReflexAlignedFree 0078fc50 (cdecl, one argument), called at the current ESP */
static void engine_free(rx_addr p) {
    ACT_CALL(1, (SU32(rx_sp(), 4, p), rx_reflex_aligned_free(p)));
}

/* operator delete as the compiler inlined it in the scalar deleting destructors: _aligned_free (IAT 0x916350) between the allocator lock's
 * enter / leave (IAT 0x91600c / 0x9160f8) while [0xda67d8] is set */
static void delete_locked(rx_addr p) {
    if (U8(0, 0x00da67d8) != 0) loop_enter_cs(0x00d61704);
    ACT_CALL(1, (SU32(rx_sp(), 4, p), crt_aligned_free(p)));
    if (U8(0, 0x00da67d8) != 0) loop_leave_cs(0x00d61704);
}

/* the Entity base's tail: both entity registries told that the entity is gone (vf1 with the entity; null in the product) */
static void registries_told(rx_addr e) {
    for (int k = 0; k < 2; k++) {
        const rx_addr registry = U32(0, k ? 0x00a95828 : 0x00a95824);
        if (registry != 0) {
            const uint32_t arg = e;
            SU32(rx_sp(), -4, e);
            (void)act_vcall(registry, 4, 1, &arg);
        }
    }
}

/* ---- the Entity base classes ---- */

/* 00406ff0 Entity::vf0 (thiscall; flags; RET 4) -> this: the base vtable 0x95a2e4, the registries told, freed (ReflexAlignedFree) with bit 0.
 * Frame: push esi. */
rx_addr rx_entity_sdtor(rx_addr e, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    SU32(e, 0, 0x0095a2e4);
    registries_told(e);
    if (flags & 1) engine_free(e);
    rx_set_sp(sp0);
    return e;
}
RX_FUNCTION(0x00406ff0, rx_entity_sdtor)

/* 00401fd0 ModelEntity::sdtor: the vtables +0x90 0x93708c, +0 0x95a2e4; the registries told; freed under the allocator lock. Frame: push esi. */
rx_addr rx_model_entity_sdtor(rx_addr e, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    SU32(e, 0x90, 0x0093708c);
    SU32(e, 0, 0x0095a2e4);
    registries_told(e);
    if (flags & 1) delete_locked(e);
    rx_set_sp(sp0);
    return e;
}
RX_FUNCTION(0x00401fd0, rx_model_entity_sdtor)

/* 00410c10 (ECX = the entity; plain ret): ScrollingTextureEntity's destroy: its vtable +0x140 0x939c5c, the ModelEntity base +0x90 0x93708c, the
 * Entity base +0 0x95a2e4 and both registries told. Frame: push esi. The scrolling textures of the HillClimb challenge areas (ids 0x1078). */
void rx_scrolling_texture_destroy(rx_addr e) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    SU32(e, 0x140, 0x00939c5c);
    SU32(e, 0x90, 0x0093708c);
    SU32(e, 0, 0x0095a2e4);
    registries_told(e);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00410c10, rx_scrolling_texture_destroy)

/* 00410bc0 ScrollingTextureEntity::sdtor (thiscall; flags; RET 4) -> this: 00410c10, freed under the allocator lock with bit 0. Frame: push esi. */
rx_addr rx_scrolling_texture_sdtor(rx_addr e, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    ACT_CALL(0, rx_scrolling_texture_destroy(e));
    if (flags & 1) delete_locked(e);
    rx_set_sp(sp0);
    return e;
}
RX_FUNCTION(0x00410bc0, rx_scrolling_texture_sdtor)

/* 0040e550 MXTargetPracticeCollectable::sdtor (thiscall; flags; RET 4) -> this: the vtable +0x150 0x938f74, the ModelEntity base +0x90 0x93708c, the
 * Entity base +0 0x95a2e4, both registries told, freed under the allocator lock with bit 0 (the collectables of the TargetPractice areas, id 0x106e).
 * Frame: push esi. */
rx_addr rx_target_practice_collectable_sdtor(rx_addr e, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    SU32(e, 0x150, 0x00938f74);
    SU32(e, 0x90, 0x0093708c);
    SU32(e, 0, 0x0095a2e4);
    registries_told(e);
    if (flags & 1) delete_locked(e);
    rx_set_sp(sp0);
    return e;
}
RX_FUNCTION(0x0040e550, rx_target_practice_collectable_sdtor)

/* 00793930 Entity::vf9 (thiscall; plain ret): +0x70 = 0, the registries told, the name block (+0x18) freed. Frame: push esi. */
void rx_entity_vf9(rx_addr e) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    SU32(e, 0x70, 0);
    registries_told(e);
    engine_free(U32(e, 0x18));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00793930, rx_entity_vf9)

/* ---- TDFEntity / MusicTrackListEntity / MXTrigger ---- */

/* 004072c0 (ESI = the entity; plain ret): the block at +0x90 freed with +0x94 cleared first; the Entity base. */
void rx_tdf_entity_destroy(rx_addr e) {
    const rx_addr block = U32(e, 0x90);
    SU32(e, 0x94, 0);
    engine_free(block);
    SU32(e, 0, 0x0095a2e4);
    registries_told(e);
}
RX_FUNCTION(0x004072c0, rx_tdf_entity_destroy)

/* 00407110 TDFEntity::sdtor. Frame: push esi. */
rx_addr rx_tdf_entity_sdtor(rx_addr e, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    ACT_CALL(0, rx_tdf_entity_destroy(e));
    if (flags & 1) engine_free(e);
    rx_set_sp(sp0);
    return e;
}
RX_FUNCTION(0x00407110, rx_tdf_entity_sdtor)

/* 00407130 MusicTrackListEntity::sdtor: the block at +0x90 freed, the Entity base, freed with bit 0. Frame: push esi. */
rx_addr rx_music_track_list_sdtor(rx_addr e, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    engine_free(U32(e, 0x90));
    SU32(e, 0, 0x0095a2e4);
    registries_told(e);
    if (flags & 1) engine_free(e);
    rx_set_sp(sp0);
    return e;
}
RX_FUNCTION(0x00407130, rx_music_track_list_sdtor)

/* 00409970 MXTrigger::sdtor: the trigger's destroy 00409990 (EAX = the entity), freed with bit 0. Frame: push esi. */
rx_addr rx_trigger_sdtor(rx_addr e, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    ACT_CALL(0, rx_waypoint_trigger_destroy(e));
    if (flags & 1) engine_free(e);
    rx_set_sp(sp0);
    return e;
}
RX_FUNCTION(0x00409970, rx_trigger_sdtor)

/* ---- cameras ---- */

/* 0063e6a0 (ESI = the entity; plain ret): the MXCameraPath vtable 0x95a3f4; a path of a world (+0x98) leaves the world's doubly linked path
 * list (+0x90 previous / +0x94 next; the world's tail +0x21dec / head +0x21de8); the Entity base. */
void rx_camera_path_destroy(rx_addr e) {
    const rx_addr world = U32(e, 0x98);
    SU32(e, 0, 0x0095a3f4);
    if (world != 0) {
        const rx_addr prev = U32(e, 0x90);
        if (prev != 0) SU32(prev, 0x94, U32(e, 0x94));
        else SU32(world, 0x21dec, U32(e, 0x94));
        const rx_addr next = U32(e, 0x94);
        if (next != 0) SU32(next, 0x90, U32(e, 0x90));
        else SU32(U32(e, 0x98), 0x21de8, U32(e, 0x90));
        SU32(e, 0x94, 0);
        SU32(e, 0x90, 0);
    }
    SU32(e, 0, 0x0095a2e4);
    registries_told(e);
}
RX_FUNCTION(0x0063e6a0, rx_camera_path_destroy)

/* 0063d560 MXCameraPath::sdtor. Frame: push esi. */
rx_addr rx_camera_path_sdtor(rx_addr e, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    ACT_CALL(0, rx_camera_path_destroy(e));
    if (flags & 1) engine_free(e);
    rx_set_sp(sp0);
    return e;
}
RX_FUNCTION(0x0063d560, rx_camera_path_sdtor)

/* 00648df0 (EAX = the entity, moved to ESI): the MXReplayTowerCamera vtable 0x95ab04, the block at +0x3e0 freed under the allocator lock,
 * the vtable 0x95a504, the camera entity base (0063da30 with ESI = the entity). Frame: push esi, edi. */
void rx_replay_tower_camera_destroy(rx_addr e) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 8);
    const rx_addr block = U32(e, 0x3e0);
    SU32(e, 0, 0x0095ab04);
    delete_locked(block);
    SU32(e, 0, 0x0095a504);
    ACT_CALL(0, rx_camera_entity_destroy(e));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00648df0, rx_replay_tower_camera_destroy)

/* 00648dd0 MXReplayTowerCamera::sdtor. Frame: push esi. */
rx_addr rx_replay_tower_camera_sdtor(rx_addr e, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    ACT_CALL(0, rx_replay_tower_camera_destroy(e));
    if (flags & 1) engine_free(e);
    rx_set_sp(sp0);
    return e;
}
RX_FUNCTION(0x00648dd0, rx_replay_tower_camera_sdtor)

/* ---- particle effect entities ---- */

/* 006965a0 (ECX = the entity; plain ret): the effect base's destructor. The vtables +0 0x95e32c, +0x90 0x95e3a0. A sound emitter (+0x294) is
 * stopped (on the audio thread [0xd67d1c] now, else queued as command 1) and released (0041c850): AUDIO BOUNDARY, the oracle runs the originals;
 * the product never makes the emitter (+0x294 stays 0: scene_fx_entities.c). The effect manager (+0x94) is told to drop the effect's record
 * (+0x110) through its vf5 (slot 0x14); +0x90 0x9588ec, +0x160 0x95df34, the poison word +0x124 0xdeaddead, +0x90 0x9587fc; the Entity base.
 * Frame: push ebx, esi, edi. */
void rx_particle_effect_destroy(rx_addr e) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 12);
    const rx_addr sound = U32(e, 0x294);
    SU32(e, 0, 0x0095e32c);
    SU32(e, 0x90, 0x0095e3a0);
    if (sound != 0) {
        if (U32(0, 0x00d67d1c) == loop_get_current_thread_id()) rx_sound_stop_now(sound);
        else rx_world_event_queue(1, sound, 0, 0, fx_reg(FX_EBP));
        if (U32(e, 0x294) != 0) {
            rx_regs in;
            memset(&in, 0, sizeof in);
            in.r[0] = U32(e, 0x294);
            rx_audio_call_ex(0x0041c850, &in, 0, 0);
        }
        SU32(e, 0x294, 0);
    }
    const rx_addr manager = U32(e, 0x94);
    SU32(e, 0x90, 0x009588ec);
    if (manager != 0) {
        const uint32_t record = e + 0x110;
        (void)act_vcall(manager, 0x14, 1, &record);
    }
    SU32(e, 0x160, 0x0095df34);
    SU32(e, 0x124, 0xdeaddeadu);
    SU32(e, 0x90, 0x009587fc);
    SU32(e, 0, 0x0095a2e4);
    registries_told(e);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006965a0, rx_particle_effect_destroy)

/* 00692190 FireworksParticleEffect::sdtor: the vtables +0 0x95e29c, +0x90 0x95e310, the effect base, freed with bit 0. Frame: push esi. */
rx_addr rx_fireworks_effect_sdtor(rx_addr e, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    SU32(e, 0, 0x0095e29c);
    SU32(e, 0x90, 0x0095e310);
    ACT_CALL(0, rx_particle_effect_destroy(e));
    if (flags & 1) engine_free(e);
    rx_set_sp(sp0);
    return e;
}
RX_FUNCTION(0x00692190, rx_fireworks_effect_sdtor)

/* 00696580 PlaceableParticleEffect::sdtor. Frame: push esi. */
rx_addr rx_placeable_effect_sdtor(rx_addr e, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    ACT_CALL(0, rx_particle_effect_destroy(e));
    if (flags & 1) engine_free(e);
    rx_set_sp(sp0);
    return e;
}
RX_FUNCTION(0x00696580, rx_placeable_effect_sdtor)

/* 00692410 TriggeredPlaceableParticleEffect::sdtor: the vtables +0 0x95e20c, +0x90 0x95e280, the effect base, freed with bit 0 (the effect of the
 * Supercross / FMX / Dalton waypoint areas). Frame: push esi. */
rx_addr rx_triggered_effect_sdtor(rx_addr e, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    SU32(e, 0, 0x0095e20c);
    SU32(e, 0x90, 0x0095e280);
    ACT_CALL(0, rx_particle_effect_destroy(e));
    if (flags & 1) engine_free(e);
    rx_set_sp(sp0);
    return e;
}
RX_FUNCTION(0x00692410, rx_triggered_effect_sdtor)

/* 0040ad60 ShadowMapEntity::vf9 (thiscall): +0x70 = 0, the registries told, the name block (+0x18) freed, the words +0x90 / +0x94 cleared.
 * Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0xc; push esi. */
void rx_shadow_map_vf9(rx_addr e) {
    ACT_FRAME_EBP(L, 16, 0xc, 1);
    SU32(e, 0x70, 0);
    registries_told(e);
    engine_free(U32(e, 0x18));
    SU32(e, 0x90, 0);
    SU32(e, 0x94, 0);
    ACT_END(L);
}
RX_FUNCTION(0x0040ad60, rx_shadow_map_vf9)

/* ---- animation entities ---- */

/* 006ba5f0 AnimEntitySplineAnimLoop::sdtor: the vtables +0 0x95f83c, +0x90 0x95f8bc, +0x140 0x95f8fc; the three loop floats (+0x350 ..
 * +0x358) set from the globals 0xa95d50 .. 0xa95d58; the rider character release (006248c0 with EDI = the entity); freed under the allocator
 * lock with bit 0. Frame: push edi. */
rx_addr rx_spline_loop_sdtor(rx_addr e, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    SU32(e, 0, 0x0095f83c);
    SU32(e, 0x90, 0x0095f8bc);
    SU32(e, 0x140, 0x0095f8fc);
    SU32(e, 0x350, U32(0, 0x00a95d50));
    SU32(e, 0x354, U32(0, 0x00a95d54));
    SU32(e, 0x358, U32(0, 0x00a95d58));
    ACT_CALL(0, rx_rider_character_release_animation(e));
    if (flags & 1) delete_locked(e);
    rx_set_sp(sp0);
    return e;
}
RX_FUNCTION(0x006ba5f0, rx_spline_loop_sdtor)

/* 006bbff0 AnimEntitySplineAnimLoopWithParticles::vf9 (thiscall): the character animation (+0x148) released (00834d80 with ESI = it), the
 * ModelEntity release (00401ea0), the particle emitter object (+0x790): its vf2, then deleted (vf0 (1)), +0x790 = 0. Frame: push esi, edi. */
void rx_spline_loop_particles_vf9(rx_addr e) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 8);
    ACT_CALL(0, rx_character_anim_release(e + 0x148));
    ACT_CALL(0, rx_fx_model_entity_vf9(e));
    if (U32(e, 0x790) != 0) {
        (void)act_vcall(U32(e, 0x790), 8, 0, NULL);
        if (U32(e, 0x790) != 0) {
            const uint32_t one = 1;
            (void)act_vcall(U32(e, 0x790), 0, 1, &one);
        }
        SU32(e, 0x790, 0);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006bbff0, rx_spline_loop_particles_vf9)

/* 006bd570 (EAX = the entity, moved to EDI; plain ret): the crew chief's node map (+0x374: its nodes freed 0068f300 from the root of the
 * head +0x38c, the head reset), the tree released (006893b0 with ESI = the map), the lock +0x358 deleted (DeleteCriticalSection), the rider
 * character release (006248c0). Frame: push esi, edi. */
void rx_crew_chief_destroy(rx_addr e) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 8);
    const rx_addr map = e + 0x374;
    ACT_CALL(1, rx_tree_free_subtree(map, U32(U32(e, 0x38c), 4)));
    const rx_addr head = U32(map, 0x18);
    SU32(head, 4, head);
    SU32(map, 0x1c, 0);
    SU32(head, 0, head);
    SU32(head, 8, head);
    ACT_CALL(0, rx_rider_tree_release(map));
    RX_OS(0x00916010, e + 0x358);
    ACT_CALL(0, rx_rider_character_release_animation(e));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006bd570, rx_crew_chief_destroy)

/* 006bd520 CrewChief::sdtor: 006bd570, freed under the allocator lock with bit 0. Frame: push esi. */
rx_addr rx_crew_chief_sdtor(rx_addr e, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    ACT_CALL(0, rx_crew_chief_destroy(e));
    if (flags & 1) delete_locked(e);
    rx_set_sp(sp0);
    return e;
}
RX_FUNCTION(0x006bd520, rx_crew_chief_sdtor)

/* ---- CollisionEntity ---- */

/* 00819050 (EDI = the entity; plain ret): the vtables +0 0x974f84, +0x90 0x974ffc; the vector at +0x17c freed (0081f6e0); the static object
 * handles at +0x164 deleted and their block freed (0081ff30), the proxy block [+0x164] freed; the static object handle at +0x15c (vtable
 * 0x96ddc4) drops its tracking reference (+0x160): the last one removes the static record from the world (world + 0x19b4; 00773a60) and
 * frees the tracking record; the ModelEntity / Entity bases (+0x90 0x93708c, +0 0x95a2e4) and the registries. Frame: push ecx, ebx, esi. */
void rx_collision_entity_destroy(rx_addr e) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 12);
    SU32(e, 0, 0x00974f84);
    SU32(e, 0x90, 0x00974ffc);
    ACT_CALL(0, rx_vehicle_sim_vector_destruct(e + 0x17c));
    ACT_CALL(1, rx_static_handle_vector_free(e + 0x164));
    engine_free(U32(e, 0x164));
    const rx_addr rec = U32(e, 0x160);
    SU32(e, 0x15c, 0x0096ddc4);
    if (rec != 0) {
        SU32(rec, 0, U32(rec, 0) - 1);
        if (U32(rec, 0) == 0) {
            const rx_addr again = U32(e, 0x160);
            if (again != 0) {
                const uint32_t index = U32(again, 8);
                ACT_CALL(1, rx_static_records_remove(U32(again, 4) + 0x19b4, index));
                engine_free(again);
            }
        }
    }
    SU32(e, 0x90, 0x0093708c);
    SU32(e, 0, 0x0095a2e4);
    registries_told(e);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00819050, rx_collision_entity_destroy)

/* 00819000 CollisionEntity::sdtor (ECX moved to EDI): 00819050, freed under the allocator lock with bit 0. Frame: push edi. */
rx_addr rx_collision_entity_sdtor(rx_addr e, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    ACT_CALL(0, rx_collision_entity_destroy(e));
    if (flags & 1) delete_locked(e);
    rx_set_sp(sp0);
    return e;
}
RX_FUNCTION(0x00819000, rx_collision_entity_sdtor)

/* 00819100 CollisionEntity::vf9 (thiscall): the static object handle at +0x15c released (its vf1 007735e0), the handle vector at +0x164
 * cleared (0081f690 with EAX = it), then the ModelEntity release (a jump to 00401ea0). Frame: push esi (popped before the jump). */
void rx_collision_entity_vf9(rx_addr e) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    (void)act_vcall(e + 0x15c, 4, 0, NULL);
    ACT_CALL(0, rx_static_handle_vector_clear(e + 0x164));
    rx_set_sp(sp0);
    rx_fx_model_entity_vf9(e);
}
RX_FUNCTION(0x00819100, rx_collision_entity_vf9)
