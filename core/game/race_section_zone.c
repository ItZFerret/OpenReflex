/* race_section_zone.c -- SectionRaceZone (scene entity type 0x1042, RSData SectionRaceTriggerRSData; vtable 0x00938c5c; 0x290 bytes): a
 * trigger volume of the Section Race scene "<level>SectionRace[<track>]". The class derives from MXTrigger (the occupant list +0x90, the
 * volume +0xb8, the world-to-local matrix +0x240, ...) and adds
 *   +0x280  StartTrigger  (RSData "StartTrigger"): 1 for the zone that starts a section, 0 for the one that ends it
 *   +0x284  TriggerIndex  (RSData "TriggerIndex"): the section's number
 * Every player whose vehicle is inside the volume is an occupant; a new occupant raises the generic "VehicleEntered" (00419820) and then
 * "VehicleEnteredSection" {vehicle, TriggerIndex, StartTrigger} (004198e0) on the trigger registry [0xa89bc8]; SectionRaceDriver
 * (race_section_driver.c) times the sections from it. Hand-transliterated from the original (the address of each function is in its
 * comment). */
#include "race_section.h"
#include "build_scene_int.h"
#include "build_activity_int.h"
#include "anim.h"
#include "fx.h"
#include "world_misc.h"
#include "world_rider.h"
#include "fx_util.h"
#include "race_extern.h"

void rx_waypoint_trigger_destroy(rx_addr e);   /* 00409990 MXTrigger's destruction (waypoint_gate.c) */

/* the factory arm of 004046a0 for the type 0x1042 (00404f95): the MXTrigger constructor (00409800), the vtable, no start flag, no index */
rx_addr rx_section_zone_create(void) {
    rx_addr zone = rx_reflex_aligned_alloc(0x290);
    if (zone == 0) return 0;
    rx_trigger_construct(zone);
    SU32(zone, 0, 0x00938c5c);
    SU32(zone, 0x280, 0);
    SU32(zone, 0x284, 0xffffffffu);
    return zone;
}

/* 0040aa00 slot 0 (thiscall; flags; RET 4) -> this: the zone leaves its scene (007d3040), MXTrigger's destruction (00409990), freed when
 * flags & 1. Frame: push esi. */
rx_addr rx_section_zone_sdtor(rx_addr zone, uint32_t flags) {
    ACT_FRAME(F, 0, 1);
    SU32(zone, 0, 0x00938c5c);
    ACT_CALL(1, rx_scene_manager_remove_entity(zone, U32(zone, 0x10)));
    ACT_CALL(0, rx_waypoint_trigger_destroy(zone));
    if (flags & 1) ACT_CALL(1, rx_reflex_aligned_free(zone));
    ACT_END(F);
    return zone;
}
RX_FUNCTION(0x0040aa00, rx_section_zone_sdtor)

/* 0040aa30 slot 5 (thiscall; the RSData reader; RET 4): Entity::vf5, the trigger's defaults (+0xb8 shape 0 = box, +0xbc watches the players,
 * the name "none" at +0xc0, tick period 1), StartTrigger and TriggerIndex from the RSData, and the event type "VehicleEnteredSection"
 * (void(RavenVehicle*, int, int), 0041a7c0) on the trigger registry when it has none yet. Frame: push ebp; and esp, -8; sub esp, 8;
 * push esi / edi. */
void rx_section_zone_load(rx_addr zone, rx_addr reader) {
    (void)reader;
    ACT_FRAME_EBP(F, 8, 8, 2);
    ACT_CALL(1, rx_entity_vf5(zone));
    SU32(zone, 0xb8, 0);
    SU32(zone, 0xbc, 0);
    SU32(zone, 0xc0, U32(0, 0x00936138));
    SU8(zone, 0xc4, U8(0, 0x0093613c));
    SU32(zone, 0x20c, 1);
    SU32(zone, 0x280, rs_bool("StartTrigger"));
    SU32(zone, 0x284, rs_uint("TriggerIndex"));
    rx_addr existing;
    ACT_CALLR(existing, 2, rx_world_registry_find(U32(0, 0x00a89bc8), 0x00936160 /* "VehicleEnteredSection" */));
    if (existing == 0) act_event_type(0x0041a7c0, U32(0, 0x00a89bc8), 0);
    ACT_END(F);
}
RX_FUNCTION(0x0040aa30, rx_section_zone_load)

/* 004198e0 (ECX = TriggerIndex, EDX = StartTrigger; stdcall vehicle; RET 4): "VehicleEnteredSection" {vehicle, index, flag} (12 bytes) on
 * the trigger registry [0xa89bc8]. Frame: sub esp, 0x10 (the /GS cookie at E - 4, the payload at E - 0x10); push esi / edi. */
void rx_section_zone_post(uint32_t index, uint32_t start, rx_addr vehicle) {
    ACT_FRAME(F, 0x10, 2);
    const rx_addr payload = F + 8;
    SU32(payload, 0, vehicle);
    SU32(payload, 4, index);
    SU32(payload, 8, start);
    act_raise(U32(0, 0x00a89bc8), 0x00936160 /* "VehicleEnteredSection" */, 0x009cbbdc, payload, 0xc);
    ACT_END(F);
}
RX_FUNCTION(0x004198e0, rx_section_zone_post)

/* 0040ac80 (EAX = zone, ESI = player): the player is inside. A known occupant raises "stay" (0x93609c) and is marked seen; a new one
 * is added and its vehicle raises "enter" (0x935e84) and then "VehicleEnteredSection". Frame: push ecx / ebx / edi / esi. */
void rx_section_zone_player_inside(rx_addr zone, rx_addr player) {
    ACT_FRAME(F, 0, 4);
    rx_addr entry;
    ACT_CALLR(entry, 1, rx_fx_trigger_find(player, zone));
    if (entry != 0) {
        const rx_addr vehicle = U32(U32(entry, 0), 0x1ac);
        if (vehicle != 0) ACT_CALL(1, rx_fx_trigger_post(vehicle, zone, 0x0093609c));
        SU32(entry, 4, 1);
    } else {
        ACT_CALL(1, rx_fx_trigger_add(player, zone));
        const rx_addr vehicle = U32(player, 0x1ac);
        if (vehicle != 0) {
            ACT_CALL(1, rx_fx_trigger_post(vehicle, zone, 0x00935e84));
            ACT_CALL(1, rx_section_zone_post(U32(zone, 0x284), U32(zone, 0x280), vehicle));
        }
    }
    ACT_END(F);
}
RX_FUNCTION(0x0040ac80, rx_section_zone_player_inside)

/* 0040aae0 slot 19 (thiscall; dt; RET 4): the zone's tick. The occupants' seen flags cleared; every player whose vehicle is not wrecked
 * (+0x280) and has its position (vf17) in the unit volume of the zone (the position through the world-to-local matrix +0x240 /
 * +0x250 / +0x260 with the translation +0x270, then 00403cf0) is handled (0040ac80) with the hit flag +0x218 set; the occupants not
 * seen leave (0040a980). Frame: push ebp; and esp, -16; sub esp, 0x34; push ebx / esi / edi; locals: the player index F + 0x14, the
 * players F + 0x18, their count F + 0x1c, the position F + 0x20, the local point F + 0x30. */
void rx_section_zone_tick(rx_addr zone, float dt) {
    (void)dt;
    ACT_FRAME_EBP(F, 16, 0x34, 3);
    for (int32_t i = 0; i < I32(zone, 0x94); i++) SU32(U32(zone, 0x90), 8u * (uint32_t)i + 4, 0);
    const rx_addr game = U32(0, 0x00a946b8);
    const uint32_t count = U8(game, 0x1541c);
    const rx_addr players = game + 0x7460;
    SU32(F, 0x18, players);
    SU32(F, 0x1c, count);
    SU32(F, 0x14, 0);
    for (uint32_t i = 0; (int32_t)i < (int32_t)count; i++) {
        rx_addr player;
        ACT_CALLR(player, 2, rx_world_players_find(players, i, 3));
        const rx_addr vehicle = U32(player, 0x1ac);
        if (vehicle != 0 && U32(vehicle, 0x280) == 0) {
            uint32_t out = F + 0x20;
            act_vcall(vehicle, 0x44, 1, &out);
            const float x = F32(F, 0x20), y = F32(F, 0x24), z = F32(F, 0x28);
            const float lx = rx_adds(rx_adds(rx_adds(rx_muls(F32(zone, 0x250), y), rx_muls(F32(zone, 0x260), z)), rx_muls(F32(zone, 0x240), x)), F32(zone, 0x270));
            const float ly = rx_adds(rx_adds(rx_adds(rx_muls(F32(zone, 0x244), x), rx_muls(F32(zone, 0x254), y)), rx_muls(F32(zone, 0x264), z)), F32(zone, 0x274));
            const float lz = rx_adds(rx_adds(rx_adds(rx_muls(F32(zone, 0x248), x), rx_muls(F32(zone, 0x258), y)), rx_muls(F32(zone, 0x268), z)), F32(zone, 0x278));
            SF32(F, 0x30, lx);
            SF32(F, 0x34, ly);
            SF32(F, 0x38, lz);
            uint32_t inside;
            ACT_CALLR(inside, 0, rx_fx_trigger_inside(zone, F + 0x30) & 0xff);
            if (inside != 0) {
                SU8(zone, 0x218, 1);
                ACT_CALL(0, rx_section_zone_player_inside(zone, player));
            }
        }
        SU32(F, 0x14, i + 1);
    }
    ACT_CALL(0, rx_fx_trigger_prune(zone));
    ACT_END(F);
}
RX_FUNCTION(0x0040aae0, rx_section_zone_tick)
