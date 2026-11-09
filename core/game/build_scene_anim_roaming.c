/* LandAnimalProximity (scene type 0x1057). Despite the class name, the shipped
 * stadium scenes also use it for camera operators and walking pit/crowd people.
 * Construction/load/unload are 006b8d90, 006cfbe0, 006cfd80, 006d0670 and
 * 006cfc80; the existing loop_entity/fx owners implement vf19, vf20 and vf29.
 * Source: original decompile and disassembly, including the 0x890 allocation at
 * 0040528b. No animation, movement or proximity updates are suppressed. */
#include "build_scene_int.h"
#include "build_boundary_audio.h"
#include "build_rider.h"
#include "build_ui_world.h"
#include "build_mxgame.h"
#include "anim_scenario.h"
#include "anim.h"
#include "fx.h"
#include "fx_util.h"
#include "crt.h"
#include "world_misc.h"
#include "world_node.h"
#include "race.h"
#include "loop_platform.h"
#include "race_activity_misc.h"
#include <strings.h>

/* 006b8d90 (ESI): CharacterEntity, then 35 records whose +0x18 is zero. */
static rx_addr animal_proximity_construct(rx_addr e) {
    rx_character_entity_construct(e);
    SU32(e, 0, 0x0095f5ac); SU32(e, 0x90, 0x0095f62c); SU32(e, 0x140, 0x0095f66c);
    SU32(e, 0x388, 0);
    for (uint32_t i = 0; i < 0x23; ++i) SU32(e, 0x39c + 0x1c * i + 0x18, 0);
    return e;
}
RX_FUNCTION(0x006b8d90, animal_proximity_construct)

/* 006cfbe0 (EAX): the proximity base and the land-animal interfaces. */
static rx_addr animal_construct(rx_addr e) {
    animal_proximity_construct(e);
    SU32(e, 0x77c, 0); SU32(e, 0x86c, 0); SU32(e, 0x870, 0); SU32(e, 0x874, 0);
    SU32(e, 0, 0x00960dec); SU32(e, 0x90, 0x00960e6c); SU32(e, 0x140, 0x00960eac);
    SU32(e, 0x28, 1);
    return e;
}
RX_FUNCTION(0x006cfbe0, animal_construct)
rx_addr rx_land_animal_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0x890);
    return e ? animal_construct(e) : 0;
}

/* rs_asset returns the same AssetHandle type as the other scene loaders. Its
 * vf2 returns the inline name at +4; build_scene_anim.c uses this same invariant. */
static void animal_handle_name(rx_addr h, char name[264]) {
    uint32_t i;
    for (i = 0; i < 263 && U8(h, 4 + i); ++i) name[i] = (char)U8(h, 4 + i);
    name[i] = 0;
}

/* 006cfb40 (cdecl Activity*): query the activity world's scene, then disable the
 * animals for activity category 3. The original list starts at capacity/grow 300. */
static void animal_activity_loaded(rx_addr activity) {
    RX_FRAME_ENTER(F, 0x20);
    SU32(F, 0, rx_reflex_aligned_alloc(0x4b0)); SU32(F, 4, 0);
    SU32(F, 8, 0x12c); SU32(F, 0xc, 0x12c); SU32(F, 0x10, 0x7fffffff);
    rx_scene_query_typed(U32(U32(activity, 0x234), 0x20138), F, 0x1057);
    for (int32_t i = 0; i < I32(F, 4); ++i) {
        rx_addr e = U32(U32(F, 0), 4 * (uint32_t)i);
        if (I32(activity, 0x238) == 3) { SU32(e, 0x77c, 1); SU8(e, 0xa2, 0); }
    }
    rx_reflex_aligned_free(U32(F, 0));
    RX_FRAME_LEAVE(F, 0x20);
}
RX_FUNCTION(0x006cfb40, animal_activity_loaded)

/* vf5 uses the core's RSData reader, like the other scene construction owners.
 * It is product-only: oracle construction comparison loads the original reader. */
static uint32_t animal_load(rx_addr e, uint32_t reader) {
    (void)reader;
    scene_character_load_model(e, rs_asset("ModelAsset"));
    rx_model_instance_init(e + 0xa0);
    rx_entity_vf5(e);
    scene_copy_words(e + 0xc0, e + 0x30, 16);

    char idle_name[264], run_name[264], mid_name[264];
    rx_addr idle = rs_asset("LoopingAnimation"); SU32(e, 0x144, idle);
    animal_handle_name(idle, idle_name);
    rx_addr idle_slot = scene_character_set_animation(e + 0x148, idle, 0);
    SU32(e, 0x1f4, idle_slot); SU32(e, 0x354, idle_slot);
    SU32(idle, 0x4c, 1);
    SU32(U32(idle, 0x44), 0x54, 1);
    SU32(e, 0x83c, scene_x87c(U32(U32(idle, 0x44), 0x4c)));
    SU32(idle, 0x4c, 0);

    rx_addr run = rs_asset("MoveForwardAnimation"); SU32(e, 0x350, run);
    animal_handle_name(run, run_name);
    rx_addr run_slot = !strcasecmp(idle_name, run_name) ? idle_slot : scene_character_set_animation(e + 0x148, run, 0);
    SU32(e, 0x358, run_slot);
    rx_addr mid = rs_asset("MoveForwardMidAnimation"); SU32(e, 0x778, mid);
    animal_handle_name(mid, mid_name);
    rx_addr mid_slot;
    if (!strcasecmp(idle_name, mid_name)) mid_slot = idle_slot;
    else if (!strcasecmp(run_name, mid_name) || !strcasecmp(mid_name, "Editor_Default")) mid_slot = run_slot;
    else mid_slot = scene_character_set_animation(e + 0x148, mid, 0);
    SU32(e, 0x774, mid_slot);

    SU32(run, 0x4c, 1);
    SU32(e, 0x84c, scene_x87c(U32(U32(run, 0x44), 0x4c)));
    SU32(run, 0x4c, 0);
    SU32(mid, 0x4c, 1);
    rx_addr animation = U32(mid, 0x44);
    SU32(e, 0x844, scene_x87c(U32(animation, 0x4c)));
    SU32(mid, 0x4c, 0);
    SU32(e, 0x870, rx_reflex_aligned_alloc(rx_mul_sat(U32(animation, 0x18), 0x10)));
    SU32(e, 0x874, rx_crt_aligned_malloc(rx_mul_sat(U32(animation, 0x20), 0x10), 0x10));
    SF32(e, 0x850, rs_float("SlowToFastSpeed"));
    SF32(e, 0x834, rs_float("MaxRunSpeed"));
    float distance = rs_float("ProximityDistance");
    SF32(e, 0x368, distance); SF32(e, 0x36c, rx_muls(distance, distance));
    SU32(e, 0x370, 0); SU32(e, 0x854, 0);
    if (rs_has("LockToTerrainNormal", 0)) SU32(e, 0x854, rs_bool("LockToTerrainNormal"));
    SF32(e, 0x35c, 0); SU32(e, 0x770, 2);

    RX_FRAME_ENTER(F, 0x40);
    uint32_t arg = F; scene_vcall(e, 0x3c, 1, &arg);
    for (uint32_t i = 0; i < 3; ++i) {
        float x = F32(F, 4 * i), y = F32(F, 0x10 + 4 * i), z = F32(F, 0x20 + 4 * i);
        SF32(e, 0x7e0 + 4 * i, rx_sqrts(rx_adds(rx_adds(rx_muls(x, x), rx_muls(y, y)), rx_muls(z, z))));
        SU32(e, 0x780 + 4 * i, U32(F, 0x30 + 4 * i));
        SU32(e, 0x790 + 4 * i, scene_x87c(U32(e, 0x780 + 4 * i)));
        SU32(e, 0x7a0 + 4 * i, scene_x87c(U32(e, 0x780 + 4 * i)));
    }
    SU32(e, 0x7b0, 0);
    float home[3];
    if (rs_has("Home2InUse", RS_BOOL) && rs_bool("Home2InUse")) { rs_vec3("Home2", home); for (uint32_t i = 0; i < 3; ++i) SF32(e, 0x790 + 4 * i, home[i]); }
    if (rs_has("Home3InUse", RS_BOOL) && rs_bool("Home3InUse")) { rs_vec3("Home3", home); for (uint32_t i = 0; i < 3; ++i) SF32(e, 0x7a0 + 4 * i, home[i]); }
    float x = F32(F, 0x20), y = F32(F, 0x24), z = F32(F, 0x28);
    SF32(e, 0x7d0, x); SF32(e, 0x7d4, y); SF32(e, 0x7d8, z);
    float length2 = rx_adds(rx_adds(rx_muls(x, x), rx_muls(y, y)), rx_muls(z, z));
    if (length2 > 0) {
        float reciprocal = rx_divs(1.0f, rx_sqrts(length2));
        SF32(e, 0x7d0, rx_muls(reciprocal, x)); SF32(e, 0x7d4, rx_muls(y, reciprocal)); SF32(e, 0x7d8, rx_muls(z, reciprocal));
    }
    RX_FRAME_LEAVE(F, 0x40);
    SU16(e, 0xb0, (U16(e, 0xb0) & 0xffef) | 8);
    SF32(e, 0x7c0, 0); SF32(e, 0x7c4, 0); SF32(e, 0x7c8, 0); SF32(e, 0x830, 0);
    for (uint32_t k = 0; k < 4; ++k) for (uint32_t i = 0; i < 3; ++i)
        SU32(e, 0x7f0 + 0x10 * k + 4 * i, scene_x87c(U32(e, 0x7d0 + 4 * i)));
    float phase = rx_muls((float)crt_rand(), 3.0518509447574615e-05f /* 0092e054 */);
    SF32(e, 0x838, rx_divs(phase, F32(e, 0x83c)));
    SF32(e, 0x840, rx_divs(phase, F32(e, 0x844)));
    SF32(e, 0x848, rx_divs(phase, F32(e, 0x84c)));
    if (rs_has("ShadowRadius", RS_FLOAT)) {
        SF32(e, 0x860, rs_float("ShadowRadius")); SF32(e, 0x864, rs_float("ShadowStretch")); SU32(e, 0x85c, (uint32_t)rs_int("ShadowTexSelect"));
    }
    /* 006d05a0 (ESI = the RSData reader [0xa9583c], EDI = the entity): an "AudioEmitter" field in the animal's RSData makes its sound emitter: an
     * FMOD event object (0x1e0 bytes: FMODEvent construct 0080a540, its position / "SoundAsset" set up by 00678cf0 / 0080ec70) stored at +0x86c.
     * AUDIO BOUNDARY (build_boundary_audio.h): the host plays no sound from the core. The only readers of +0x86c are animal_destroy below (stop and release
     * the emitter) and LandAnimalProximity::vf20 (fx_animal.c: copies the animal's position into the emitter and posts the audio command 10), both
     * guarded by "+0x86c != 0"; vf20's position query writes only the scratch vector +0x40, which it rebuilds every frame (cross product). With the
     * emitter absent nothing of the animal's own state differs. The tested stadium roamers have no such field. The oracle runs the original. */
    if (rs_has("AudioEmitter", 0)) {
        rx_regs in; memset(&in, 0, sizeof in); in.r[6] = U32(0, 0x00a9583c); in.r[7] = e;
        rx_audio_call_ex(0x006d05a0, &in, 0, 0);
    }
    if (!U32(0, 0x00a94320)) { rx_activity_loaded_handler_add(0x006cfb40); SU32(0, 0x00a94320, 1); }
    return 0;
}
#ifndef RX_ORACLE
RX_FUNCTION(0x006cfd80, animal_load)
#endif

/* 006d0670: the model unload and the free-function handler key of 004197c0. */
static void animal_unload(rx_addr e) {
    rx_fx_model_entity_vf9(e);
    if (U32(0, 0x00a94320)) {
        RX_FRAME_ENTER(F, 0x18);
        rx_addr channel = rx_world_registry_find(U32(0, 0x00a93f74), 0x00936038);
        SU32(F, 0, 0x00939f6c); SU32(F, 4, 0); SU32(F, 0xc, 0x006cfb40);
        SU32(F, 8, crt_type_info_name(0x009cbd24, 0x009e1a7c));
        rx_channel_remove_listener(channel, F);
        RX_FRAME_LEAVE(F, 0x18);
        SU32(0, 0x00a94320, 0);
    }
}
RX_FUNCTION(0x006d0670, animal_unload)

/* 006cfc80 (EAX): animation scratch buffers, proximity-owned object, character.
 * Audio-owned objects retain their actual original teardown boundary. */
static void animal_destroy(rx_addr e) {
    SU32(e, 0, 0x00960dec); SU32(e, 0x90, 0x00960e6c); SU32(e, 0x140, 0x00960eac);
    rx_addr emitter = U32(e, 0x86c);
    if (emitter) {
        if (U32(0, 0x00d67d1c) == loop_get_current_thread_id()) rx_sound_stop_now(emitter);
        else rx_world_event_queue(1, emitter, 0, 0, fx_reg(FX_EBP));
        if (U32(e, 0x86c)) {
            /* 0041c850 (EAX = the sound object): stops it (0080ae10 on the audio thread, else the queued command 1) and releases it (the FMODEvent
             * destructor 0080a660, ReflexAlignedFree). AUDIO BOUNDARY: the emitter exists only when 006d05a0 made it (above). */
            rx_regs in; memset(&in, 0, sizeof in); in.r[0] = U32(e, 0x86c);
            rx_audio_call_ex(0x0041c850, &in, 0, 0);
        }
        SU32(e, 0x86c, 0);
    }
    rx_reflex_aligned_free(U32(e, 0x870)); SU32(e, 0x870, 0);
    rx_crt_aligned_free(U32(e, 0x874)); SU32(e, 0x874, 0);
    SU32(e, 0, 0x0095f5ac); SU32(e, 0x90, 0x0095f62c); SU32(e, 0x140, 0x0095f66c);
    if (U32(e, 0x388)) {   /* the animal's second sound object (+0x388: cleared at construction, nothing in the core writes it): AUDIO BOUNDARY as above */
        rx_regs in; memset(&in, 0, sizeof in); in.r[0] = U32(e, 0x388);
        rx_audio_call_ex(0x0041c850, &in, 0, 0);
    }
    rx_rider_character_release_animation(e);
}
RX_FUNCTION(0x006cfc80, animal_destroy)
static rx_addr animal_sdtor(rx_addr e, uint32_t flags) {
    animal_destroy(e);
    if (flags & 1) rx_crt_aligned_free(e);
    return e;
}
RX_FUNCTION(0x006cfc30, animal_sdtor)
static rx_addr animal_sdtor_renderable(rx_addr renderable, uint32_t flags) { return animal_sdtor(renderable - 0x90, flags); }
RX_FUNCTION(0x006d3260, animal_sdtor_renderable)
