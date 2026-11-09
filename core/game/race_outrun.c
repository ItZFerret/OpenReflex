/* race_outrun.c -- the Outrun format's timed sections (activity mode 8: the career's Outrun series 9, 56, 64, 83, 88).
 *
 * TimedRaceSection (scene entity type 0x107b, RSData TimedSectionTriggerRSData, vtable 0x00937b64, 0x290 bytes) is a trigger volume of
 * the level's race scene. It derives from MXTrigger (the occupant list +0x90, the volume +0xb8, the world-to-local matrix +0x240 ...)
 * and adds
 *   +0x280  SectionStart   (RSData Bool): the section gives time when a rider enters it
 *   +0x284  SectionTime    (RSData Float): the time it gives, seconds
 *   +0x288  SectionNumber  (RSData Int, 0 when absent): its order along the course
 * Its tick (TimedRaceSection::vf19 00413a80, fx_entity.c) posts "EnteredTimedSection" {vehicle, section} on the trigger registry
 * [0xa89bc8] for a new occupant (00419a10). An Outrun race subscribes MXActivityRace's handler 00569550 to it (build_activity_start.c,
 * mode 8): the rider's player takes the section's number and time (0054f4b0: +0x11ac the section, +0x11b0 the previous one, +0x11b4
 * the time left) and "TimedSectionEntered" {activity, driver, number} is raised on the activity's channel (00554df0). The player's
 * update counts +0x11b4 down while the race is on (loop_player.c), and the race's state-8 check (race_activity.c) ends the race for
 * the leader whose time ran out (+0x1128 = 1: the Single event's vf13 then reports "Failed").
 * Hand-transliterated from the original (the address of each function is in its comment). */
#include "race_outrun.h"
#include "race_activity.h"
#include "build_scene_int.h"
#include "build_activity_int.h"
#include "anim.h"
#include "fx.h"
#include "world_misc.h"
#include "race_extern.h"

void rx_waypoint_trigger_destroy(rx_addr e);   /* 00409990 MXTrigger's destruction (waypoint_gate.c) */

/* x87 fld / fstp of a dword: a signalling NaN comes out quiet, every other value unchanged */
static uint32_t x87_copy(uint32_t bits) {
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x007fffffu) && !(bits & 0x00400000u)) bits |= 0x00400000u;
    return bits;
}

/* ---- TimedRaceSection ---------------------------------------------------------------------------------------------------------- */

/* the factory arm of 004046a0 for the type 0x107b (0040571f): 0x290 bytes, the MXTrigger constructor (00409800), the vtable, the
 * start flag 0 and the time 0.0 (+0x288 is set by vf5) */
rx_addr rx_timed_section_create(void) {
    const rx_addr section = rx_reflex_aligned_alloc(0x290);
    if (section == 0) return 0;
    rx_trigger_construct(section);
    SU32(section, 0, 0x00937b64);
    SU32(section, 0x280, 0);
    SF32(section, 0x284, 0.0f);
    return section;
}

/* 00413960 slot 0 (thiscall; flags; RET 4) -> this: the section leaves its scene (007d3040), MXTrigger's destruction (00409990),
 * freed when flags & 1. Frame: push esi. */
rx_addr rx_timed_section_sdtor(rx_addr section, uint32_t flags) {
    ACT_FRAME(F, 0, 1);
    SU32(section, 0, 0x00937b64);
    ACT_CALL(1, rx_scene_manager_remove_entity(section, U32(section, 0x10)));
    ACT_CALL(0, rx_waypoint_trigger_destroy(section));
    if (flags & 1) ACT_CALL(1, rx_reflex_aligned_free(section));
    ACT_END(F);
    return section;
}
RX_FUNCTION(0x00413960, rx_timed_section_sdtor)

/* 00413990 slot 5 (thiscall; the RSData reader; RET 4): Entity::vf5, the trigger's defaults (+0xb8 shape 0 = box, +0xbc watches the
 * players, the name "none" at +0xc0, tick period 1), SectionStart / SectionTime / SectionNumber (0 when the field is absent) from the
 * RSData, and the event type "EnteredTimedSection" (void(RavenVehicle*, const TimedRaceSection*), 0041a9e0, name [0x9c617c]) on the
 * trigger registry when it has none yet. Frame: push ebp; and esp, -8; sub esp, 8; push esi / edi. */
void rx_timed_section_load(rx_addr section, rx_addr reader) {
    (void)reader;
    ACT_FRAME_EBP(F, 8, 8, 2);
    ACT_CALL(1, rx_entity_vf5(section));
    SU32(section, 0xb8, 0);
    SU32(section, 0xbc, 0);
    SU32(section, 0xc0, U32(0, 0x00936138));      /* "none" */
    SU8(section, 0xc4, U8(0, 0x0093613c));
    SU32(section, 0x20c, 1);
    SU32(section, 0x280, rs_bool("SectionStart"));
    SF32(section, 0x284, rs_float("SectionTime"));
    if (rs_has("SectionNumber", 0)) SU32(section, 0x288, (uint32_t)rs_int("SectionNumber"));
    else SU32(section, 0x288, 0);
    const rx_addr name = U32(0, 0x009c617c);      /* "EnteredTimedSection" */
    rx_addr existing;
    ACT_CALLR(existing, 2, rx_world_registry_find(U32(0, 0x00a89bc8), name));
    if (existing == 0) act_event_type(0x0041a9e0, U32(0, 0x00a89bc8), name);
    ACT_END(F);
}
RX_FUNCTION(0x00413990, rx_timed_section_load)

/* ---- the race's side ------------------------------------------------------------------------------------------------------------ */

/* 00554df0 (ESI = the activity's channel, ECX = driver, EDX = the section number; stdcall activity; RET 4): "TimedSectionEntered"
 * {activity, driver, number} (void(Activity*, BaseDriver*, int), type 0x9ce550). Frame: sub esp, 0x10 (the /GS cookie at +0xc, the
 * payload at +0); push edi. */
void rx_event_timed_section_entered(rx_addr channel, rx_addr driver, uint32_t number, rx_addr activity) {
    ACT_FRAME(F, 0x10, 1);
    const rx_addr payload = F + 4;
    SU32(payload, 0, activity);
    SU32(payload, 4, driver);
    SU32(payload, 8, number);
    act_raise(channel, 0x00941710 /* "TimedSectionEntered" */, 0x009ce550, payload, 0xc);
    ACT_END(F);
}
RX_FUNCTION(0x00554df0, rx_event_timed_section_entered)

/* 0054f4b0 (ECX = player, EAX = section): a section that gives time (+0x280) sets the player's clock: the first one (+0x11ac < 0)
 * starts it, a later one (a higher number than the player's) moves the previous number to +0x11b0; the player takes the section's
 * number (+0x11ac) and time (+0x11b4, an x87 copy). With the player in an activity (+0x94), "TimedSectionEntered" is raised for its
 * driver (00559fd0). Frame: push esi; then push edi. */
void rx_player_timed_section_entered(rx_addr player, rx_addr section) {
    ACT_FRAME(F, 0, 1);
    if (U32(section, 0x280) == 0) {
        ACT_END(F);
        return;
    }
    ACT_FRAME(G, 0, 1);
    const int32_t current = I32(player, 0x11ac);
    rx_addr activity;
    if (current < 0) {
        activity = U32(player, 0x94);
        SU32(player, 0x11ac, U32(section, 0x288));
        SU32(player, 0x11b4, x87_copy(U32(section, 0x284)));
        if (activity == 0) {
            ACT_END(G);
            ACT_END(F);
            return;
        }
    } else {
        if (I32(section, 0x288) <= current) {
            ACT_END(G);
            ACT_END(F);
            return;
        }
        activity = U32(player, 0x94);
        SU32(player, 0x11b0, (uint32_t)current);
        SU32(player, 0x11ac, U32(section, 0x288));
        SU32(player, 0x11b4, x87_copy(U32(section, 0x284)));
    }
    rx_addr driver;
    ACT_CALLR(driver, 0, rx_activity_player_driver(player, activity));
    ACT_CALL(1, rx_event_timed_section_entered(U32(activity, 0x290), driver, U32(player, 0x11ac), activity));
    ACT_END(G);
    ACT_END(F);
}
RX_FUNCTION(0x0054f4b0, rx_player_timed_section_entered)

/* 00569550 MXActivityRace's "EnteredTimedSection" handler (thiscall activity; vehicle, section; RET 8): in an Outrun race (mode 8)
 * the driver whose player rides the vehicle (the drivers +0x268, their player +0x74, its vehicle +0x1ac) enters the section
 * (0054f4b0). Frame: push esi / edi / ebx once there are drivers. */
void rx_race_entered_timed_section(rx_addr activity, rx_addr vehicle, rx_addr section) {
    if (U32(activity, 0x238) != 8) return;
    const int32_t count = I32(activity, 0x26c);
    if (count <= 0) return;
    ACT_FRAME(F, 0, 3);
    const rx_addr drivers = U32(activity, 0x268);
    for (int32_t i = 0; i < count; i++) {
        const rx_addr player = U32(U32(drivers, 4u * (uint32_t)i), 0x74);
        if (U32(player, 0x1ac) != vehicle) continue;
        ACT_CALL(0, rx_player_timed_section_entered(player, section));
        break;
    }
    ACT_END(F);
}
RX_FUNCTION(0x00569550, rx_race_entered_timed_section)
