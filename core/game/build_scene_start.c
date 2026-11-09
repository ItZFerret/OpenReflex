/* build_scene_start.c -- the StartEntity family (the places an activity starts from): FreeRide / FreeRideStar / FreeRideLine /
 * Freestyle / Supermoto / Opencross (+0x90 the start Index, vf5 0040adc0), SnakeTag (0040adf0), the challenge starts (BigAir, HillClimb,
 * Jumper, Pathfinder, Stoppie, TargetPractice, TrailBlazer, Wheelie, StepUp x4, Graffiti, Hockey: vf5 0040af10), Capture / CaptureBase
 * (0040ae50 / 0040ae90), CustomizeVehicle (0040aed0) and TagBallStart (Entity::vf5). Entity only: +0x90 the index (0 from the constructor),
 * the 0xb0-byte classes have +0xa0 / +0xa4 more. The constructors are the originals' (alloc 0078fbe0 + the Entity fields + the vtable). */
#include "build_scene_int.h"

#define START_CTOR(addr, name, vt, withidx)                      \
    rx_addr name(rx_addr e) {                                      \
        rx_entity_construct(e, 0x0095a2e4);                        \
        if (withidx) SU32(e, 0x90, 0);                             \
        SU32(e, 0, vt);                                            \
        return e;                                                  \
    }                                                              \
    RX_FUNCTION(addr, name)

START_CTOR(0x00406150, rx_start_customize_ctor, 0x009386a4, 1)
START_CTOR(0x004061b0, rx_start_freestyle_ctor, 0x00937d44, 1)
START_CTOR(0x00406210, rx_start_supermoto_ctor, 0x00937c54, 1)
START_CTOR(0x00406270, rx_start_opencross_ctor, 0x00937bdc, 1)
START_CTOR(0x004062d0, rx_start_freeride_ctor, 0x00937ccc, 1)
START_CTOR(0x00406330, rx_start_freeride_star_ctor, 0x00937dbc, 1)
START_CTOR(0x00406390, rx_start_freeride_line_ctor, 0x00937e34, 1)
START_CTOR(0x004063f0, rx_start_snaketag_ctor, 0x00937eac, 1)
START_CTOR(0x00406450, rx_start_tagball_ctor, 0x00938b24, 0)
START_CTOR(0x004064a0, rx_start_bigair_ctor, 0x0093808c, 1)
START_CTOR(0x00406500, rx_start_hillclimb_ctor, 0x00938104, 1)
START_CTOR(0x00406560, rx_start_jumper_ctor, 0x0093817c, 1)
START_CTOR(0x004065c0, rx_start_pathfinder_ctor, 0x009381f4, 1)
START_CTOR(0x00406620, rx_start_stoppie_ctor, 0x0093844c, 1)
START_CTOR(0x00406680, rx_start_targetpractice_ctor, 0x009384c4, 1)
START_CTOR(0x004066e0, rx_start_trailblazer_ctor, 0x0093853c, 1)
START_CTOR(0x00406740, rx_start_wheelie_ctor, 0x009385b4, 1)
START_CTOR(0x004067a0, rx_start_stepup_ctor, 0x0093826c, 1)
START_CTOR(0x00406800, rx_start_stepup_bronze_ctor, 0x009382e4, 1)
START_CTOR(0x00406860, rx_start_stepup_silver_ctor, 0x0093835c, 1)
START_CTOR(0x004068c0, rx_start_stepup_gold_ctor, 0x009383d4, 1)
START_CTOR(0x00406920, rx_start_graffiti_ctor, 0x00937f24, 1)
START_CTOR(0x00406980, rx_start_hockey_ctor, 0x00937f9c, 1)
START_CTOR(0x004069e0, rx_start_capture_ctor, 0x00938014, 1)
START_CTOR(0x00406a40, rx_start_capturebase_ctor, 0x0093862c, 1)

/* the factory side: ReflexAlignedAlloc(size) + the constructor of the id */
rx_addr rx_start_entity_create(uint32_t id) {
    static const struct { uint32_t id, size; rx_addr (*ctor)(rx_addr); } t[] = {
        { 0x1010, 0xb0, rx_start_customize_ctor },  { 0x1011, 0xa0, rx_start_freestyle_ctor }, { 0x1012, 0xa0, rx_start_supermoto_ctor },
        { 0x1013, 0xa0, rx_start_opencross_ctor },  { 0x1014, 0xa0, rx_start_freeride_ctor },  { 0x1015, 0xa0, rx_start_freeride_star_ctor },
        { 0x1016, 0xa0, rx_start_freeride_line_ctor }, { 0x1017, 0xb0, rx_start_snaketag_ctor }, { 0x1018, 0x90, rx_start_tagball_ctor },
        { 0x101b, 0xa0, rx_start_bigair_ctor },     { 0x101c, 0xa0, rx_start_hillclimb_ctor }, { 0x101d, 0xa0, rx_start_jumper_ctor },
        { 0x101e, 0xa0, rx_start_pathfinder_ctor }, { 0x101f, 0xa0, rx_start_stoppie_ctor },   { 0x1020, 0xa0, rx_start_targetpractice_ctor },
        { 0x1021, 0xa0, rx_start_trailblazer_ctor }, { 0x1022, 0xa0, rx_start_wheelie_ctor },  { 0x1023, 0xa0, rx_start_stepup_ctor },
        { 0x1024, 0xa0, rx_start_stepup_bronze_ctor }, { 0x1025, 0xa0, rx_start_stepup_silver_ctor }, { 0x1026, 0xa0, rx_start_stepup_gold_ctor },
        { 0x103c, 0xa0, rx_start_graffiti_ctor },   { 0x103d, 0xa0, rx_start_hockey_ctor },    { 0x103e, 0xb0, rx_start_capture_ctor },
        { 0x103f, 0xb0, rx_start_capturebase_ctor },
    };
    for (unsigned k = 0; k < sizeof t / sizeof *t; k++)
        if (t[k].id == id) return t[k].ctor(rx_reflex_aligned_alloc(t[k].size));
    return 0;
}

/* vf5 0040adc0 (ECX = entity, stack reader): the six free-ride style starts */
uint32_t rx_start_vf5_index(rx_addr e, uint32_t reader) { (void)reader; rx_entity_vf5(e); SU32(e, 0x90, rs_uint("Index")); return 0; }
RX_FUNCTION(0x0040adc0, rx_start_vf5_index)
/* vf5 0040adf0: SnakeTagStartEntity */
uint32_t rx_start_snaketag_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    SU32(e, 0x90, 0);
    rx_entity_vf5(e);
    SF32(e, 0xa0, rs_float("SnakeAreaRadius"));
    SU32(e, 0xa4, rs_bool("UseWaterAsBoundary"));
    return 0;
}
RX_FUNCTION(0x0040adf0, rx_start_snaketag_vf5)
/* vf5 0040af10 (ECX = entity; RET 4): +0x90 = 0, then Entity::vf5 */
uint32_t rx_start_vf5_plain(rx_addr e, uint32_t reader) { (void)reader; SU32(e, 0x90, 0); rx_entity_vf5(e); return 0; }
RX_FUNCTION(0x0040af10, rx_start_vf5_plain)
/* CaptureStartEntity::vf5 0040ae50 / CaptureBaseEntity::vf5 0040ae90 */
uint32_t rx_start_capture_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    SU32(e, 0x90, 0); SU32(e, 0xa0, 0);
    SU32(e, 0xa0, rs_uint("Index"));
    rx_entity_vf5(e);
    return 0;
}
RX_FUNCTION(0x0040ae50, rx_start_capture_vf5)
uint32_t rx_start_capturebase_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    SU32(e, 0x90, 0); SU32(e, 0xa0, 0);
    SU32(e, 0xa0, rs_uint("CaptureBaseIndex"));
    rx_entity_vf5(e);
    return 0;
}
RX_FUNCTION(0x0040ae90, rx_start_capturebase_vf5)
/* CustomizeVehicleStartEntity::vf5 0040aed0 */
uint32_t rx_start_customize_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    SU32(e, 0x90, 0);
    SU32(e, 0xa0, (uint32_t)rs_enum("VehicleType"));
    rx_entity_vf5(e);
    return 0;
}
RX_FUNCTION(0x0040aed0, rx_start_customize_vf5)

/* AIAvoidanceSphere (id 0x1073, 0xa0 bytes): the Entity base 00403460 (vtable 0x0093871c), vf5 004034c0 (Radius at +0x90). The AI steers
 * round it; the entity is also flagged by AIAvoid on props (PhysicsEntity +0x74 bit 3). */
rx_addr rx_ai_avoid_sphere_ctor(rx_addr e) { rx_entity_construct(e, 0x0095a2e4); SU32(e, 0, 0x0093871c); return e; }
RX_FUNCTION(0x00403460, rx_ai_avoid_sphere_ctor)
rx_addr rx_ai_avoid_sphere_create(void) { return rx_ai_avoid_sphere_ctor(rx_reflex_aligned_alloc(0xa0)); }
uint32_t rx_ai_avoid_sphere_vf5(rx_addr e, uint32_t reader) { (void)reader; rx_entity_vf5(e); SF32(e, 0x90, rs_float("Radius")); return 0; }
RX_FUNCTION(0x004034c0, rx_ai_avoid_sphere_vf5)
