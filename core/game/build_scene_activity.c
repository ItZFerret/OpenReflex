/* build_scene_activity.c -- the entities the challenge activities place in a level: MXChallengesSpecEntity (ChallengeSpecsRSData 0x1019:
 * the medal criteria of every challenge), the collectables (MXChallengeCollectable 0x106c, MXHitDiscoveryCollectable 0x106d,
 * MXTargetPracticeCollectable 0x106e) and EggHuntEggEntity (0x1044). Transliterated from the disassembly. */
#include "build_scene_int.h"

/* ---------------------------------------------------------------------------------------------------------------------------
 * MXChallengesSpecEntity (0x450): FUN_0040fba0: Entity, vtable 0x009379bc, 0x84 bytes at +0x324 zeroed; vf5 0040fc10 */
rx_addr rx_challenge_specs_ctor(rx_addr e) {
    rx_entity_construct(e, 0x0095a2e4);
    SU32(e, 0, 0x009379bc);
    for (uint32_t k = 0; k < 0x84; k += 4) SU32(e, 0x324 + k, 0);
    return e;
}
RX_FUNCTION(0x0040fba0, rx_challenge_specs_ctor)
rx_addr rx_challenge_specs_create(void) { return rx_challenge_specs_ctor(rx_reflex_aligned_alloc(0x450)); }

#define RF(name, off) SF32(e, off, rs_float(name))
#define RI(name, off) SU32(e, off, (uint32_t)rs_int(name))
#define RE(name, off) SU32(e, off, (uint32_t)rs_enum(name))
uint32_t rx_challenge_specs_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    RF("WheelieBronzeDistance", 0x240); RF("WheelieSilverDistance", 0x24c); RF("WheelieGoldDistance", 0x258);
    RF("StoppieBronzeDistance", 0x180); RF("StoppieSilverDistance", 0x18c); RF("StoppieGoldDistance", 0x198);
    RF("JumperBronzeDistance", 0xf0); RF("JumperSilverDistance", 0xfc); RF("JumperGoldDistance", 0x108);
    RF("HillClimbBronzeHeight", 0xc0); RF("HillClimbSilverHeight", 0xcc);
    SF32(e, 0xd8, rs_has("HillClimbGoldHeight", 0) ? rs_float("HillClimbGoldHeight") : 100.0f /* 0x93005c */);
    if (rs_has("HillClimbBBronzeHeight", 0)) {
        RF("HillClimbBBronzeHeight", 0xc4); RF("HillClimbBSilverHeight", 0xd0);
        RF("HillClimbCBronzeHeight", 0xc8); RF("HillClimbCSilverHeight", 0xd4);
        SF32(e, 0xdc, 100.0f); SF32(e, 0xe0, 100.0f);
        if (rs_has("HillClimbBGoldHeight", 0)) { RF("HillClimbBGoldHeight", 0xdc); RF("HillClimbCGoldHeight", 0xe0); }
    } else {
        SU32(e, 0xc4, 0x41200000); SU32(e, 0xd0, 0x41a00000); SU32(e, 0xdc, 0x42c80000);
        SU32(e, 0xc8, 0x41200000); SU32(e, 0xd4, 0x41a00000); SU32(e, 0xe0, 0x42c80000);
    }
    if (rs_has("HillClimbARequiredVehicleSpecID", 0)) {
        RE("HillClimbARequiredVehicleSpecID", 0x3b4); RE("HillClimbBRequiredVehicleSpecID", 0x3b8); RE("HillClimbCRequiredVehicleSpecID", 0x3bc);
    } else { SU32(e, 0x3b4, 0xffffffff); SU32(e, 0x3b8, 0xffffffff); SU32(e, 0x3bc, 0xffffffff); }
    SF32(e, 0x330, rs_has("HillClimbTimeLimit", 0) ? rs_float("HillClimbTimeLimit") : 60.0f /* 0x931da0 */);
    SF32(e, 0x334, rs_has("HillClimbBTimeLimit", 0) ? rs_float("HillClimbBTimeLimit") : 60.0f);
    SF32(e, 0x338, rs_has("HillClimbCTimeLimit", 0) ? rs_float("HillClimbCTimeLimit") : 60.0f);
    for (uint32_t o = 0x330; o <= 0x338; o += 4) if (F32(e, o) <= 0.0f) SF32(e, o, 60.0f);
    RF("StepUpBronzeHeight", 0x150); RF("StepUpSilverHeight", 0x15c); RF("StepUpGoldHeight", 0x168);
    if (rs_has("StepUpRequiredVehicleSpecID", 0)) RE("StepUpRequiredVehicleSpecID", 0x3d8); else SU32(e, 0x3d8, 0xffffffff);
    RF("BigAirABronzeScore", 0x90); RF("BigAirASilverScore", 0x9c); RF("BigAirAGoldScore", 0xa8);
    RF("BigAirBBronzeScore", 0x94); RF("BigAirBSilverScore", 0xa0); RF("BigAirBGoldScore", 0xac);
    RF("BigAirCBronzeScore", 0x98); RF("BigAirCSilverScore", 0xa4); RF("BigAirCGoldScore", 0xb0);
    if (rs_has("BigAirARequiredVehicleSpecID", 0)) {
        RE("BigAirARequiredVehicleSpecID", 0x3a8); RE("BigAirBRequiredVehicleSpecID", 0x3ac); RE("BigAirCRequiredVehicleSpecID", 0x3b0);
    } else { SU32(e, 0x3a8, 0xffffffff); SU32(e, 0x3ac, 0xffffffff); SU32(e, 0x3b0, 0xffffffff); }
    RF("PathfinderABronzeDistance", 0x120); RF("PathfinderASilverDistance", 0x12c); RF("PathfinderAGoldDistance", 0x138);
    RF("PathfinderBBronzeDistance", 0x124); RF("PathfinderBSilverDistance", 0x130); RF("PathfinderBGoldDistance", 0x13c);
    RF("PathfinderCBronzeDistance", 0x128); RF("PathfinderCSilverDistance", 0x134); RF("PathfinderCGoldDistance", 0x140);
    if (rs_has("BigAirARequiredVehicleSpecID", 0)) {   /* (the original tests the BigAir name again) */
        RE("PathfinderARequiredVehicleSpecID", 0x3cc); RE("PathfinderBRequiredVehicleSpecID", 0x3d0); RE("PathfinderCRequiredVehicleSpecID", 0x3d4);
    } else { SU32(e, 0x3cc, 0xffffffff); SU32(e, 0x3d0, 0xffffffff); SU32(e, 0x3d4, 0xffffffff); }
    if (rs_has("PathfinderAAISpecID", 0)) {
        RI("PathfinderAAISpecID", 0x42c); RI("PathfinderBAISpecID", 0x430); RI("PathfinderCAISpecID", 0x434);
        RI("PathfinderAAIVehicleSpecID", 0x438); RI("PathfinderBAIVehicleSpecID", 0x43c); RI("PathfinderCAIVehicleSpecID", 0x440);
        RI("PathfinderAAIDifficulty", 0x444); RI("PathfinderBAIDifficulty", 0x448); RI("PathfinderCAIDifficulty", 0x44c);
    } else for (uint32_t o = 0x42c; o <= 0x44c; o += 4) SU32(e, o, 0xffffffff);
    if (rs_has("TargetPracticeBronzeTargets", 0)) SF32(e, 0x210, (float)rs_int("TargetPracticeBronzeTargets"));
    if (rs_has("TargetPracticeSilverTargets", 0)) SF32(e, 0x21c, (float)rs_int("TargetPracticeSilverTargets"));
    if (rs_has("TargetPracticeGoldTargets", 0)) SF32(e, 0x228, (float)rs_int("TargetPracticeGoldTargets"));
    if (rs_has("TargetPracticeRequiredVehicleSpecID", 0)) RE("TargetPracticeRequiredVehicleSpecID", 0x408);
    if (rs_has("TargetPracticeDuration", 0)) RF("TargetPracticeDuration", 0x384);
    RF("TrailblazerABronzeTime", 0x1b0); RF("TrailblazerASilverTime", 0x1bc); RF("TrailblazerAGoldTime", 0x1c8);
    RF("TrailblazerBBronzeTime", 0x1b4); RF("TrailblazerBSilverTime", 0x1c0); RF("TrailblazerBGoldTime", 0x1cc);
    RF("TrailblazerCBronzeTime", 0x1b8); RF("TrailblazerCSilverTime", 0x1c4); RF("TrailblazerCGoldTime", 0x1d0);
    if (rs_has("TrailblazerARequiredVehicleSpecID", 0)) {
        RE("TrailblazerARequiredVehicleSpecID", 0x3f0); RE("TrailblazerBRequiredVehicleSpecID", 0x3f4); RE("TrailblazerCRequiredVehicleSpecID", 0x3f8);
    } else { SU32(e, 0x3f0, 0xffffffff); SU32(e, 0x3f4, 0xffffffff); SU32(e, 0x3f8, 0xffffffff); }
    RI("HillClimbARewardLevel", 0x2ac); RI("TrailblazerARewardLevel", 0x2e8); RI("HitDiscoveryARewardLevel", 0x2f4);
    RI("HillClimbBRewardLevel", 0x2b0); RI("TrailblazerBRewardLevel", 0x2ec); RI("HitDiscoveryBRewardLevel", 0x2f8);
    RI("HillClimbCRewardLevel", 0x2b4); RI("TrailblazerCRewardLevel", 0x2f0); RI("HitDiscoveryCRewardLevel", 0x2fc);
    RI("TargetPracticeRewardLevel", 0x300);
    SU32(e, 0x304, U32(e, 0x300)); SU32(e, 0x308, U32(e, 0x300));
    return 0;
}
RX_FUNCTION(0x0040fc10, rx_challenge_specs_vf5)

/* ---------------------------------------------------------------------------------------------------------------------------
 * the collectables: ModelEntity (00401db0) with +0x74 |= 2, +0x140 0, +0x70 1 */
rx_addr rx_collectable_ctor(rx_addr e) {   /* 0040e470 */
    rx_model_entity_construct(e);
    SU32(e, 0x74, U32(e, 0x74) | 2);
    SU32(e, 0, 0x00938e04); SU32(e, 0x90, 0x00938e7c);
    SU32(e, 0x140, 0); SU32(e, 0x70, 1);
    return e;
}
RX_FUNCTION(0x0040e470, rx_collectable_ctor)
rx_addr rx_hitdiscovery_ctor(rx_addr e) {  /* 0040fb60 */
    rx_model_entity_construct(e);
    SU32(e, 0x74, U32(e, 0x74) | 2);
    SU32(e, 0x140, 0); SU32(e, 0x70, 1);
    SU32(e, 0, 0x00938f7c); SU32(e, 0x90, 0x00938ff4);
    return e;
}
RX_FUNCTION(0x0040fb60, rx_hitdiscovery_ctor)
rx_addr rx_targetpractice_collectable_ctor(rx_addr e) {   /* 0040e500 */
    rx_model_entity_construct(e);
    SU32(e, 0x74, U32(e, 0x74) | 2);
    SU32(e, 0x140, 0); SU32(e, 0x70, 1);
    SU32(e, 0x150, 0x009370cc);
    SU32(e, 0, 0x00938ebc); SU32(e, 0x90, 0x00938f34); SU32(e, 0x150, 0x00938f74);
    SU8(e, 0xaa0, 0); SU8(e, 0xaa1, 0);
    return e;
}
RX_FUNCTION(0x0040e500, rx_targetpractice_collectable_ctor)
rx_addr rx_egghunt_egg_ctor(rx_addr e) {   /* 0040e3d0 */
    rx_model_entity_construct(e);
    SU32(e, 0x140, 0); SU32(e, 0x144, 0); SU32(e, 0x148, 0);
    SU32(e, 0, 0x00938d4c); SU32(e, 0x90, 0x00938dc4);
    return e;
}
RX_FUNCTION(0x0040e3d0, rx_egghunt_egg_ctor)

rx_addr rx_collectable_create(uint32_t id) {
    switch (id) {
    case 0x106c: return rx_collectable_ctor(rx_reflex_aligned_alloc(0x150));
    case 0x106d: return rx_hitdiscovery_ctor(rx_reflex_aligned_alloc(0x150));
    case 0x106e: return rx_targetpractice_collectable_ctor(rx_reflex_aligned_alloc(0xad0));
    case 0x1044: return rx_egghunt_egg_ctor(rx_reflex_aligned_alloc(0x150));
    }
    return 0;
}

/* MXChallengeCollectable::vf5 0040e4a0 (vf5 of MXHitDiscoveryCollectable 0040fb90 jumps here): the model instance, the Entity part, the
 * matrix copied to +0xc0, the instance flags, +0xac 0 */
uint32_t rx_collectable_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    rx_model_instance_init(e + 0xa0);
    rx_entity_vf5(e);
    scene_copy_words(e + 0xc0, e + 0x30, 16);
    SU16(e, 0xb0, (U16(e, 0xb0) & 0xffef) | 8);
    SU32(e, 0xac, 0);
    return 0;
}
RX_FUNCTION(0x0040e4a0, rx_collectable_vf5)
uint32_t rx_hitdiscovery_vf5(rx_addr e, uint32_t reader) { return rx_collectable_vf5(e, reader); }
RX_FUNCTION(0x0040fb90, rx_hitdiscovery_vf5)

/* MXTargetPracticeCollectable::vf5 0040e5d0 */
uint32_t rx_targetpractice_collectable_vf5(rx_addr e, uint32_t reader) {
    rx_collectable_vf5(e, reader);
    SU32(e, 0x154, (uint32_t)rs_enum("Axis To Align"));
    SF32(e, 0xab0, 0.0f); SF32(e, 0xab4, 0.0f); SF32(e, 0xac8, 0.0f); SF32(e, 0xacc, 0.0f);
    SF32(e, 0xac0, rs_float("U Seconds For Wrap"));
    SF32(e, 0xac4, rs_float("V Seconds For Wrap"));
    if (rs_bool("Reverse U")) SF32(e, 0xac0, rx_muls(F32(e, 0xac0), F32(0, 0x009cd9e8)));
    if (rs_bool("Reverse V")) SF32(e, 0xac4, rx_muls(F32(e, 0xac4), F32(0, 0x009cd9e8)));
    rx_addr s = e + 0x160;
    SU8(e, 0xaa1, 0);
    rx_addr dst = s + ((uint32_t)U8(s, 0x941) + 0x12) * 0x40;
    const char* nm = "gTextureOffset";
    for (uint32_t k = 0; k < 0x3f; k++) SU8(dst, k, k <= strlen(nm) ? (uint8_t)nm[k] : 0);   /* strncpy(.., 0x3f): zero padded */
    SU8(dst, 0x3f, 0);
    SU32(s, (uint32_t)U8(s, 0x941) * 4 + 0x8c0, e + 0xab0);
    SU32(s, (uint32_t)U8(s, 0x941) * 4 + 0x880, 1);
    SU32(s, (uint32_t)U8(s, 0x941) * 4 + 0x900, 0);
    SU8(s, 0x941, U8(s, 0x941) + 1);
    return 0;
}
RX_FUNCTION(0x0040e5d0, rx_targetpractice_collectable_vf5)

/* MXTargetPracticeCollectable::vf19 0040e730 (ECX = e; dt; RET 4): the target's texture scroll. Outside the UI pause (the UI manager's
 * +0x288), the engine pause ([[0xa95844] + 0x10] + 0xc) and once hit (+0x140): for U (+0xac0 seconds per wrap) and V (+0xac4) when not
 * zero, the clock (+0xac8 / +0xacc) advances by dt, wraps at the period's magnitude, and the offset gTextureOffset (+0xab0 / +0xab4)
 * is clock / period. Only the target's shader reads the offset. */
void rx_targetpractice_collectable_vf19(rx_addr e, float dt) {
    if (U32(U32(U32(0, 0x00a946b8), 0x745c), 0x288) != 0) return;
    if (U32(U32(U32(0, 0x00a95844), 0x10), 0xc) != 0) return;
    if (U32(e, 0x140) != 0) return;
    static const uint32_t period[2] = {0xac0, 0xac4}, clock[2] = {0xac8, 0xacc}, offset[2] = {0xab0, 0xab4};
    for (int k = 0; k < 2; k++) {
        const float p = F32(e, period[k]);
        if (p == 0.0f) continue;   /* ucomiss 0; lahf; test ah, 0x44; jnp: skipped only when exactly zero */
        const float t = F32(e, clock[k]) + dt;
        const float wrap = rx_bf(rx_fb(p) & 0x7fffffffu);   /* andps 0x977570 */
        SF32(e, clock[k], t);
        if (t >= wrap) SF32(e, clock[k], t - wrap);
        SF32(e, offset[k], rx_divs(F32(e, clock[k]), p));
    }
}
RX_FUNCTION(0x0040e730, rx_targetpractice_collectable_vf19)

/* EggHuntEggEntity::vf5 0040e400 */
uint32_t rx_egghunt_egg_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    rx_model_instance_init(e + 0xa0);
    rx_entity_vf5(e);
    scene_copy_words(e + 0xc0, e + 0x30, 16);
    if (rs_has("PointValue", 0)) SU32(e, 0x140, rs_uint("PointValue"));
    return 0;
}
RX_FUNCTION(0x0040e400, rx_egghunt_egg_vf5)

/* MXStepUpUprightEntity (id 0x1070, 0x140 bytes): a ModelEntity (the factory's inline constructor, vtables 0x009390fc / 0x00939174), vf5
 * 00410b10: the model asset's draw distance (asset + 0x120) set to 15000 */
rx_addr rx_stepup_upright_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0x140);
    rx_model_entity_construct(e);
    SU32(e, 0, 0x009390fc); SU32(e, 0x90, 0x00939174);
    return e;
}
uint32_t rx_stepup_upright_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    rx_model_instance_init(e + 0xa0);
    rx_entity_vf5(e);
    scene_copy_words(e + 0xc0, e + 0x30, 16);
    rx_addr h = U32(e, 0x100);
    SU32(h, 0x4c, 1);
    SU32(U32(h, 0x44), 0x120, 15000);
    SU32(U32(e, 0x100), 0x4c, 0);
    return 0;
}
RX_FUNCTION(0x00410b10, rx_stepup_upright_vf5)
