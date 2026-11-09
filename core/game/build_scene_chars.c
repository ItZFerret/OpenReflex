/* build_scene_chars.c -- the animated props of the track side: CrewChief (0x103b), TrackSideFlagHolder (0x1038), ScoreTowerFlagHolder
 * (0x1039), ThirtySecondBoard (0x103a, a ScenarioAnimEntity). All are CharacterEntity classes (constructor 00839a40, the rider unit's)
 * whose vf5 clones the ModelAsset into +0x200, initialises the Character (+0x148) on its node set (+0x210), loads the animations
 * into the character's motion slots and registers the event handlers of the race (see build_scene_props.c).
 * Not ported: the per-frame ticks, the race event callbacks (the registered handler addresses are the originals') and the
 * CrewChief job. */
#include "build_scene_int.h"
#include "build_rider.h"
#include <math.h>

static void cs_init(rx_addr cs) {   /* InitializeCriticalSection as the reference boot leaves it: {0, -1, 0, 0, 0, 0} */
    SU32(cs, 0, 0); SU32(cs, 4, 0xffffffffu); SU32(cs, 8, 0); SU32(cs, 0xc, 0); SU32(cs, 0x10, 0); SU32(cs, 0x14, 0);
}

/* the dword the flag holders and the board keep after the animations: Character +0x24 / +0x28 / +0x2c / +0x3c */
static void character_slot_state(rx_addr c, rx_addr slot) {
    SU32(c, 0x24, 0); SU32(c, 0x28, 0); SU32(c, 0x2c, slot); SF32(c, 0x3c, 0.0f);
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * CrewChief 006bd490 (0x3a0): +0x350 1, +0x354 0 (HiResModel), the critical section +0x358, a std::map at +0x374 (proxy, head, size) */
rx_addr rx_crew_chief_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0x3a0);
    rx_character_entity_construct(e);
    SU32(e, 0, 0x0095f4e4); SU32(e, 0x90, 0x0095f564); SU32(e, 0x140, 0x0095f5a4);
    SU32(e, 0x350, 1); SU32(e, 0x354, 0);
    cs_init(e + 0x358);
    rx_addr px = rx_reflex_aligned_alloc(4);
    if (px) SU32(px, 0, e + 0x374);
    SU32(e, 0x374, px);
    rx_addr head = rx_reflex_aligned_alloc(0x18);   /* 00706df0: the empty map's head node */
    if (head) SU32(head, 0, 0);
    SU32(head, 4, 0); SU32(head, 8, 0); SU8(head, 0x14, 1); SU8(head, 0x15, 0);
    SU32(e, 0x38c, head);
    SU8(head, 0x15, 1);
    SU32(U32(e, 0x38c), 4, U32(e, 0x38c)); SU32(U32(e, 0x38c), 0, U32(e, 0x38c)); SU32(U32(e, 0x38c), 8, U32(e, 0x38c));
    SU32(e, 0x390, 0);
    return e;
}
/* CrewChief::vf5 006bd5c0 */
uint32_t rx_crew_chief_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    rx_addr model = rs_asset("ModelAsset");
    SU32(e, 0x370, model);
    SU32(model, 0x4c, 1);
    rx_model_asset_copy(e + 0x200, U32(model, 0x44));
    SU32(U32(e, 0x370), 0x4c, 0);
    SU32(e, 0x354, rs_bool("HiResModel"));
    rx_character_anim_init(e + 0x210, e + 0x148);
    rx_model_instance_init(e + 0xa0);
    rx_entity_vf5(e);
    scene_copy_words(e + 0xc0, e + 0x30, 16);
    SU32(e, 0x70, rs_bool("EnableTick"));
    SU16(e, 0xb0, (U16(e, 0xb0) & 0xffef) | 8);
    SU32(e, 0xac, 0);
    return 0;
}
RX_FUNCTION(0x006bd5c0, rx_crew_chief_vf5)

/* ---------------------------------------------------------------------------------------------------------------------------
 * TrackSideFlagHolder 006c2a40 (0x390) / ScoreTowerFlagHolder 006c15b0 (0x380) */
rx_addr rx_trackside_flag_holder_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0x390);
    rx_character_entity_construct(e);
    SU32(e, 0x378, 0x437a0000); SU32(e, 0x350, 0); SU32(e, 0x360, 0); SU32(e, 0x370, 0);
    SU32(e, 0, 0x00960214); SU32(e, 0x90, 0x00960294); SU32(e, 0x140, 0x009602d4);
    SU32(e, 0x37c, 0x42c80000); SU32(e, 0x380, 0); SU32(e, 0x384, 0);
    return e;
}
rx_addr rx_scoretower_flag_holder_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0x380);
    rx_character_entity_construct(e);
    SU32(e, 0x350, 0); SU32(e, 0x360, 0); SU32(e, 0x364, 0);
    SU32(e, 0, 0x00960094); SU32(e, 0x90, 0x00960114); SU32(e, 0x140, 0x00960154);
    SU32(e, 0x368, 0); SU32(e, 0x36c, 0);
    return e;
}
/* the shared part of the two flag holders' vf5 (006c2b20 / 006c1670): the model, WaveFlag and IdleAnimation, EnableTick, the
 * Character's slot state */
static void flag_holder_load(rx_addr e) {
    rx_addr model = rs_asset("ModelAsset");
    scene_character_load_model(e, model);
    rx_model_instance_init(e + 0xa0);
    rx_entity_vf5(e);
    scene_copy_words(e + 0xc0, e + 0x30, 16);
    rx_addr c = e + 0x148;
    SU32(e, 0x35c, scene_character_set_animation(c, rs_asset("WaveFlag"), 0));
    rx_addr idle = rs_asset("IdleAnimation");
    SU32(e, 0x144, idle);
    rx_addr slot = scene_character_set_animation(c, idle, 0);
    SU32(e, 0x1f4, slot); SU32(e, 0x358, slot);
    rx_addr a = U32(e, 0x144);
    SU32(a, 0x4c, 1);
    SU32(U32(a, 0x44), 0x54, 1);
    SU32(U32(e, 0x144), 0x4c, 0);
    SU32(e, 0x70, rs_bool("EnableTick"));
    SU16(e, 0xb0, (U16(e, 0xb0) & 0xffef) | 8);
    character_slot_state(c, U32(e, 0x358));
    SU32(c, 0x24, 0); SU32(c, 0x28, 0);
}
/* TrackSideFlagHolder::vf5 006c2b20 */
uint32_t rx_trackside_flag_holder_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    flag_holder_load(e);
    if (rs_has("PassingFlag", 0)) SU32(e, 0x380, rs_asset("PassingFlag"));
    if (rs_has("YellowFlag", 0)) SU32(e, 0x384, rs_asset("YellowFlag"));
    SU32(e, 0x388, rs_asset("ModelAsset"));
    /* 006c2cf0 */
    rx_event_register_global(0x00941df4 /* "First Human Joined Activity" */, e, 0x006c18e0, 0);
    rx_event_register_global(0x00935ea4 /* "Activity Init" */, e, 0x006c2d90, 0);
    rx_event_register_global(0x00941d90 /* "Activity UnInit" */, e, 0x006c2e60, 0);
    return 0;
}
RX_FUNCTION(0x006c2b20, rx_trackside_flag_holder_vf5)
/* ScoreTowerFlagHolder::vf5 006c1670 */
uint32_t rx_scoretower_flag_holder_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    flag_holder_load(e);
    if (rs_has("CheckeredFlag", 0)) SU32(e, 0x368, rs_asset("CheckeredFlag"));
    if (rs_has("WhiteFlag", 0)) SU32(e, 0x36c, rs_asset("WhiteFlag"));
    SU32(e, 0x370, rs_asset("ModelAsset"));
    /* 006c1840 */
    rx_event_register_global_b(0x00941df4, e, 0x006c18e0, 0);
    rx_event_register_global_b(0x00935ea4, e, 0x006c1910, 0);
    rx_event_register_global_b(0x00941d90, e, 0x006c1990, 0);
    return 0;
}
RX_FUNCTION(0x006c1670, rx_scoretower_flag_holder_vf5)

/* ---------------------------------------------------------------------------------------------------------------------------
 * ThirtySecondBoard 006b76d0 (0x510): the ScenarioAnimEntity constructor (0074a810), +0x480 / +0x4a0 / +0x4f0 0, +0x484 5 */
rx_addr rx_thirty_second_board_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0x510);
    rx_scenario_anim_construct(e);
    SU32(e, 0x480, 0); SU32(e, 0x4a0, 0); SU32(e, 0x4f0, 0);
    SU32(e, 0, 0x0095f41c); SU32(e, 0x90, 0x0095f49c); SU32(e, 0x140, 0x0095f4dc);
    SU32(e, 0x484, 5);
    return e;
}
/* ThirtySecondBoard::vf5 006b7790 */
uint32_t rx_thirty_second_board_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    rx_addr model = rs_asset("ModelAsset");
    scene_character_load_model(e, model);
    rx_model_instance_init(e + 0xa0);
    rx_entity_vf5(e);
    scene_copy_words(e + 0xc0, e + 0x30, 16);
    rx_addr c = e + 0x148;
    SU32(e, 0x48c, scene_character_set_animation(c, rs_asset("ShowSign"), 0));
    rx_addr run = rs_asset("RunAnim");
    SU32(e, 0x494, scene_character_set_animation(c, run, 0));
    SU32(U32(run, 0x44), 0x54, 1); SU32(run, 0x4c, 0);
    rx_addr walk = rs_asset("WalkAnim");
    SU32(e, 0x490, scene_character_set_animation(c, walk, 0));
    SU32(U32(walk, 0x44), 0x54, 1); SU32(walk, 0x4c, 0);
    rx_addr idle = rs_asset("IdleAnimation");
    SU32(e, 0x144, idle);
    rx_addr slot = scene_character_set_animation(c, idle, 0);
    SU32(e, 0x1f4, slot); SU32(e, 0x488, slot);
    rx_addr a = U32(e, 0x144);
    SU32(a, 0x4c, 1);
    SU32(U32(a, 0x44), 0x54, 1);
    SU32(U32(e, 0x144), 0x4c, 0);
    if (rs_has("ShowSignPosition", 0)) {
        SU32(e, 0x4f0, 1);
        float v[3];
        rs_vec3("ShowSignPosition", v);
        SF32(e, 0x4e0, v[0]); SF32(e, 0x4e4, v[1]); SF32(e, 0x4e8, v[2]);
    }
    SF32(e, 0x498, 7.0f /* 0x930004 */); SF32(e, 0x49c, 30.0f /* 0x931d98 */);
    if (rs_has("WalkOnSpeed", 0)) SF32(e, 0x498, rs_float("WalkOnSpeed"));
    if (rs_has("WalkOffSpeed", 0)) SF32(e, 0x49c, rs_float("WalkOffSpeed"));
    SU32(e, 0x70, rs_bool("EnableTick"));
    SU16(e, 0xb0, (U16(e, 0xb0) & 0xffef) | 8);
    rx_event_register_first_human(e, 0x006b7b90, 0);
    RX_FRAME_ENTER(F, 0x40);
    uint32_t a1[1] = { F };
    scene_vcall(e, 0x3c, 1, a1);   /* vf15: the matrix */
    const float m0 = F32(F, 0), m1 = F32(F, 4), m2 = F32(F, 8), m4 = F32(F, 0x10), m5 = F32(F, 0x14), m6 = F32(F, 0x18);
    const float m8 = F32(F, 0x20), m9 = F32(F, 0x24), m10 = F32(F, 0x28), m12 = F32(F, 0x30), m13 = F32(F, 0x34), m14 = F32(F, 0x38);
    SF32(e, 0x4b4, m9); SF32(e, 0x4b8, m10); SF32(e, 0x4c0, m12); SF32(e, 0x4c4, m13); SF32(e, 0x4c8, m14); SF32(e, 0x4b0, m8);
    SF32(e, 0x4f8, sqrtf(rx_adds(rx_adds(rx_muls(m0, m0), rx_muls(m4, m4)), rx_muls(m8, m8))));
    SF32(e, 0x4fc, sqrtf(rx_adds(rx_adds(rx_muls(m1, m1), rx_muls(m5, m5)), rx_muls(m9, m9))));   /* (the original reads these two after a push: the next column) */
    SF32(e, 0x500, sqrtf(rx_adds(rx_adds(rx_muls(m2, m2), rx_muls(m6, m6)), rx_muls(m10, m10))));
    RX_FRAME_LEAVE(F, 0x40);
    character_slot_state(c, U32(e, 0x488));
    SU32(c, 0x24, 0); SU32(c, 0x28, 0);
    SU32(c, 0x2c, U32(e, 0x488));
    return 0;
}
RX_FUNCTION(0x006b7790, rx_thirty_second_board_vf5)
