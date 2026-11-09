/* build_activity_freeride_run.c -- MXActivityFreeRide's run-time methods (the class of the UI world's activity, vtable 0x0094329c) and their helpers.
 *   vf5 00560110: the activity area (the base, the free ride's .tdf07 line kind 7, the scene's "StartActivity" triggers of this area into +0xff190,
 *   FUN_005605e0, "Activity Basic Essentials Loaded" on the activity channel).
 * Helpers: 007d2820 (the scene's entities of a type in the named group), 005555a0 (the event type's index by name), 005605e0 (which tracks exist for the
 * triggers' events), 0056aed0 (the first kind of the race tracks the database has). core/notes/product_chain.md. */
#include "build_activity_int.h"
#include "rider_cap.h"
#include "world_veh_damage.h"
#include "build_tables.h"
#include "build_vehicle.h"
#include "build_ui_props.h"
#include "build_scene.h"
#include "build_rider_util.h"
#include "anim_extern.h"
#include "race.h"
#include "build_rider.h"
#include "anim.h"
#include "world_veh.h"
#include "race_activity.h"
#include "world_rider.h"
#include "world_misc.h"
#include "race_activity_misc.h"
#include "phys_body.h"
#include <math.h>
#include "build_activity_freeride_run.h"
#include "freeride_any_level.h"
#include "music_player.h"

/* the session's static initialiser (006de1f0 under bit 0 of 0x00dcf304) has run at startup */
static void session_static(void) {
    if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "the session's static initialiser has not run");
}

/* 007d2820 (stdcall scene, type, out list, capacity, group name, overflow flag ptr or 0; RET 0x18) -> the count: under the scene's lock (+4) the first of the 24
 * groups (0x110 bytes from +0x38: the name inline, entities +0x13c / count +0x140, +0x10c non-zero when the group is in use) named `name`; the entities of
 * that group whose vf22 (+0x58) is `type`, up to the capacity (then *flag = 1). 0 when no group has the name. */
uint32_t rx_scene_group_query(rx_addr scene, uint32_t type, rx_addr out, int32_t cap, rx_addr name, rx_addr flag) {
    rr_os_cs_enter(scene + 4);
    uint32_t u = 0;
    for (; u < 0x18; u++) {
        const rx_addr rec = scene + 0x38 + 0x110 * u;
        if (U32(rec, 0x10c) != 0 && anim_strcmp(name, rec) == 0) break;
    }
    if (u == 0x18) { rr_os_cs_leave(scene + 4); return 0; }
    if (flag) SU32(flag, 0, 0);
    const rx_addr g = scene + u * 0x110;
    int32_t n = 0;
    int full = 0;
    if (I32(g, 0x140) > 0) {
        int32_t i = 0;
        for (;;) {
            if (n >= cap) { full = 1; break; }
            if (rr_vcall(U32(U32(g, 0x13c), 4u * (uint32_t)i), 0x58, 0, NULL) == type) { SU32(out, 4u * (uint32_t)n, U32(U32(g, 0x13c), 4u * (uint32_t)i)); n++; }
            i++;
            if (i >= I32(g, 0x140)) break;
        }
    }
    if (full || n >= cap) { if (flag) SU32(flag, 0, 1); }
    rr_os_cs_leave(scene + 4);
    return (uint32_t)n;
}
RX_FUNCTION(0x007d2820, rx_scene_group_query)

/* 005555a0 (cdecl name) -> the index of the event type whose name (the table of 0x3e pointers at 0x9cf448) equals it (_stricmp), 0x3e when none */
uint32_t rx_event_type_index(rx_addr name) {
    uint32_t i = 0;
    do {
        if (crt_stricmp(name, U32(0, 0x009cf448 + 4 * i)) == 0) return i;
        i++;
    } while (i < 0x3e);
    return 0x3e;
}
RX_FUNCTION(0x005555a0, rx_event_type_index)

/* 0056aed0 (EDI = level, ESI = area) -> the first of the track kinds 0, 1, 9, 10, 12 (in that order) the database has as .tdf07 for the area, else 5 when
 * kind 5 has it, else 0 */
static uint32_t first_track_kind(rx_addr level, uint32_t area) {
    static const uint8_t kinds[5] = {0, 1, 9, 10, 12};
    for (int k = 0; k < 5; k++) {
        uint32_t has;
        ACT_CALLR(has, 1, rx_track_has_asset(kinds[k], level, area));
        if (has) return kinds[k];
    }
    uint32_t has;
    ACT_CALLR(has, 1, rx_track_has_asset(5, level, area));
    return has ? 5 : 0;
}

/* 005605e0 (stdcall activity; RET 4): the "StartActivity" triggers' events (the 0x18 slots at +0xff190): online, the slots' +0x210 cleared; offline in a
 * world that has not started (+0x20018 == 0) each slot's event type (+0x108: the type names) with its area (+0x104) decides which tracks exist: result 0
 * clears the trigger's +0x210, result 1 adds the activity row (type, area, mode 0) to the activity table when the type has none and the UI roster build
 * (MXUIManager_BuildAIRoster 0059f3e0: UI) is asked for it. */
void rx_free_ride_triggers_refresh(rx_addr A) {
    ACT_FRAME_EBP(L, 8, 0x14, 3);
    session_static();
    const rx_addr s = U32(0, 0x00da6cb0);
    if (U32(s, 0x1c4c) != 0 && U32(s, 0x1b0) != 1) {
        rx_addr p = A + 0xff194;
        for (int k = 0; k < 4; k++) {
            for (int j = -1; j <= 4; j++) { rx_addr e = U32(p, 4 * j); if (e) SU8(e, 0x210, 0); }
            p += 0x18;
        }
        ACT_END(L);
        return;
    }
    const rx_addr world = U32(A, 0x234);
    if (U32(world, 0x20018) == 0) {
        for (uint32_t k = 0; k < 0x18; k++) {
            const rx_addr t = U32(A, 0xff190 + 4 * k);
            uint32_t result = 0;
            if (t == 0) continue;
            const uint32_t type = rx_event_type_index(t + 0x108);
            const uint32_t area = U32(t, 0x104);
            const rx_addr level = world + 0x20030;
            uint32_t has = 0;
            switch (type) {
            case 0: ACT_CALLR(has, 1, rx_track_has_asset(0, level, area)); if (has) result = 1; break;
            case 1: ACT_CALLR(has, 1, rx_track_has_asset(1, level, area)); if (has) result = 1; break;
            case 2: ACT_CALLR(has, 1, rx_track_has_asset(3, level, area)); if (has) result = 1; break;
            case 3: ACT_CALLR(has, 1, rx_track_has_asset(2, level, area)); if (has) result = 1; break;
            case 4: ACT_CALLR(has, 1, rx_track_has_asset(5, level, area)); if (has) result = 1; break;
            case 5: case 20: ACT_CALLR(has, 1, rx_track_has_asset(6, level, area)); if (has) result = 1; break;
            case 6: ACT_CALLR(has, 1, rx_track_has_asset(4, level, area)); if (has) result = 2; break;
            case 7: ACT_CALLR(has, 1, rx_track_has_asset(9, level, area)); if (has) result = 1; break;
            case 12: {
                const uint32_t kind = first_track_kind(level, area);
                ACT_CALLR(has, 1, rx_track_has_asset(kind, level, area));
                if (has) result = 2;
                break;
            }
            case 17: ACT_CALLR(has, 1, rx_track_has_asset(10, level, area)); if (has) result = 1; break;
            case 18: ACT_CALLR(has, 1, rx_track_has_asset(11, level, area)); if (has) result = 1; break;
            case 19: ACT_CALLR(has, 1, rx_track_has_asset(12, level, area)); if (has) result = 1; break;
            case 21: case 22: case 23: case 24: case 25: case 26: case 27: case 29: case 30: result = 2; break;
            default: break;   /* 8 .. 11, 13 .. 16, 28 and the unknown name (0x3e): nothing */
            }
            if (result == 0) { SU8(t, 0x210, 0); continue; }
            if (result == 1 && U32(A, 0xff1f4) == 0) {
                int32_t row;
                ACT_CALLR(row, 1, rx_activity_find_row(ACT_ACTIVITY_TBL, type, (int32_t)(int8_t)U8(t, 0x104)));
                if (row == -1) {
                    ACT_CALL(4, rx_activity_table_add_row(ACT_ACTIVITY_TBL, type, (uint32_t)(int32_t)(int8_t)U8(t, 0x104), 0));
                    if (U32(0, 0x00ae1b8c) != 0)   /* MXUIManager_BuildAIRoster 0059f3e0 (UI: the roster of the activity's AI) */
                        act_ui_call(0x0059f3e0, RX_STDCALL, 0, 0, 0);
                } else if (U32(0, 0x00ae1b8c) != 0)
                    act_ui_call(0x0059f3e0, RX_STDCALL, 0, 0, 0);
            }
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x005605e0, rx_free_ride_triggers_refresh)

/* MXActivityFreeRide::vf5 00560110 (thiscall) */
void rx_free_ride_vf5(rx_addr A) {
    ACT_FRAME(L, 0x68, 4);
    ACT_CALL(0, rx_activity_vf5(A));
    SU32(A, 0xff1f4, 0);
    ACT_CALL(3, rx_track_load(U32(A, 0x234) + 0x20030, A + 0x890, 7, U32(A, 0x220)));
    crt_memset(A + 0xff190, 0, 0x60);
    crt_memset(L + 0x1c, 0, 0x5c);
    SU32(L, 0x18, 0);
    const uint32_t n = rx_scene_group_query(U32(U32(A, 0x234), 0x20138), 0x1028, L + 0x18, 0xc, A + 0x118, 0);
    for (uint32_t i = 0; i < n; i++) {
        const rx_addr e = U32(L, 0x18 + 4 * i);
        if (crt_stricmp(e + 0xc0, 0x0092ed10 /* "StartActivity" */) == 0 && crt_strlen(e + 0x108) != 0) SU32(A, 0xff190 + 4 * i, e);
    }
    ACT_CALL(1, rx_free_ride_triggers_refresh(A));
    const uint32_t payload[1] = {A};
    race_raise_event(U32(0, 0x00a93f74), 0x00936038 /* "Activity Basic Essentials Loaded" */, 0x009cbd24, payload, 1);
    ACT_END(L);
}
RX_FUNCTION(0x00560110, rx_free_ride_vf5)

/* 0055fce0 (EAX = the "PositionMarker_Flag" entity, EDI = the activity's flag entity A + 0x1143e0; stdcall scene; RET 4): the flag entity takes the model
 * of the found one: its handle (+0x100) marked in use, the entity's model instance attached (0041c700: the scene, the "CustomResetMarker_Flag" name, the
 * handle, flags 2; a 100-byte block allocated and freed around it) and +0xb0 (u16) = (+0xb0 & 0xffef) | 8 */
static void flag_entity_attach(rx_addr found, rx_addr scene, rx_addr flag_e) {
    ACT_FRAME(L, 0, 2);
    const rx_addr handle = U32(found, 0x100);
    SU32(handle, 0x4c, 1);
    const rx_addr tmp = rx_reflex_aligned_alloc(rx_mul_sat(0x19, 4));
    ACT_CALL(3, rx_entity_attach_model(scene, 0x00941f3c /* "CustomResetMarker_Flag" */, flag_e, handle, 2));
    SU32(handle, 0x4c, 0);
    SU16(flag_e, 0xb0, (uint16_t)((U16(flag_e, 0xb0) & 0xffefu) | 8u));
    rx_reflex_aligned_free(tmp);
    ACT_END(L);
}

/* 0055ffd0 (ESI = the activity): +0x114520 = 0; when the scene (the activity's world +0x20138) has the entity "PositionMarker_Flag": it is hidden (+0xa2 = 0),
 * the flag entity (+0x1143e0) takes its model (above), gets the scene's next entity id (+0x19f4, under the lock) in +0x1143f4, the scene adds it (007d2f70),
 * its +0x114482 = 0 and the activity's +0x114520 = 1 */
void rx_free_ride_flag_marker(rx_addr A) {
    rx_addr found;
    ACT_CALLR(found, 2, rx_scene_manager_find_named(U32(U32(A, 0x234), 0x20138), 0x00941f28 /* "PositionMarker_Flag" */));
    SU32(A, 0x114520, 0);
    if (found) {
        SU8(found, 0xa2, 0);
        ACT_FRAME(R, 0, 3);                                             /* push ebx / ebp / edi (0055fff5 .. 0056000a) up to the scene's add */
        const rx_addr flag_e = A + 0x1143e0;
        ACT_CALL(1, flag_entity_attach(found, U32(U32(A, 0x234), 0x20138), flag_e));   /* 0055fce0 (stdcall scene; RET 4) */
        const rx_addr scene = U32(U32(A, 0x234), 0x20138);
        rr_os_cs_enter(scene + 4);
        const uint32_t id = U32(scene, 0x19f4);
        SU32(scene, 0x19f4, id + 1);
        SU32(flag_e, 0x14, id);
        rr_os_cs_leave(scene + 4);
        ACT_CALL(1, rx_scene_manager_add_entity(flag_e, scene));
        ACT_END(R);
        SU8(A, 0x114482, 0);
        SU32(A, 0x114520, 1);
    }
}
RX_FUNCTION(0x0055ffd0, rx_free_ride_flag_marker)

/* ResetMarkerFlagEntity::vf28 0055fd50 (thiscall ECX = the flag entity, stack = the spot vector; RET 4): the flag stands at the spot,
 * 3 ft lower (subss [0x92fe48]): +0x60 = x, +0x64 = y - 3, +0x68 = z. Reached from the "save spot" key in Free Ride
 * (Player_HandleResetInput, slot 0x70); EAX = the vector */
uint32_t rx_reset_marker_flag_set_spot(rx_addr flag_e, rx_addr spot) {
    const float x = F32(spot, 0), y = F32(spot, 4), z = F32(spot, 8);
    SF32(flag_e, 0x60, x);
    SF32(flag_e, 0x64, y - F32(0, 0x0092fe48));
    SF32(flag_e, 0x68, z);
    return spot;
}
RX_FUNCTION(0x0055fd50, rx_reset_marker_flag_set_spot)



/* MXActivityFreeRide::vf2 0055ff70 (thiscall arg; RET 4): Activity::vf2, "Activity Init" on the activity channel, +0xff1f0 = 0, the start slots (00560cd0),
 * the flag marker (0055ffd0) */
void rx_free_ride_vf2(rx_addr A, rx_addr arg) {
    ACT_FRAME(L, 0, 3);
    ACT_CALL(1, rx_activity_vf2(A, arg));
    const uint32_t payload[1] = {A};
    race_raise_event(U32(0, 0x00a93f74), 0x00935ea4 /* "Activity Init" */, 0x009cbd24, payload, 1);
    SU32(A, 0xff1f0, 0);
    ACT_CALL(1, rx_free_ride_start_slots(A));
    ACT_CALL(0, rx_free_ride_flag_marker(A));
    ACT_END(L);
}
RX_FUNCTION(0x0055ff70, rx_free_ride_vf2)

/* 00560cd0 (stdcall activity; RET 4): the activity's twelve 0x30-byte start slots (A + 0x340 + 0x30 k: position, direction at +0x10, +0x28 users) from the scene's
 * entities in the activity's area group (A + 0x118): the free ride starts (0x1014), the star starts (0x1015) and the line starts (0x1016), queried into three
 * lists of twelve on the frame (L + 0x100, +0x90, +0xc0).
 *   two or more 0x1014: each entity's vf15 (+0x3c) fills a record (position at +0x30, direction at +0x20 of its out buffer L + 0xc0) and the entity's +0x90 - 1
 *   is the slot, taken once; slots from the entity count to 11 get the default (x = 2 k, y = z = 0, direction (1, 0, 0)).
 *   fewer than two 0x1014 (00560f26; a single 0x1014 is ignored): exactly one 0x1015 star start: twelve slots on a circle around it (NOT PORTED, abort);
 *   then exactly one 0x1016 line start: twelve slots along its line (NOT PORTED, abort); neither: the all-default fill (005615b0: slot k at (2 k, 0, 0),
 *   direction (1, 0, 0)). The UI scene of the install has twelve 0x1014 starts; no level of the install has a star or a line start.
 *   Product only: after the all-default fill, rx_fr_any_level_spawn (freeride_any_level.c, RESTORATION, not original code) gives a race level without
 *   free ride starts a spawn (the level's own free ride or arena starts, else its race track's start). */
void rx_free_ride_start_slots(rx_addr A) {
    ACT_FRAME_EBP(L, 16, 0x124, 3);
    const rx_addr scene = U32(U32(A, 0x234), 0x20138), name = A + 0x118;
    const rx_addr l14 = L + 0x100, l15 = L + 0x90, l16 = L + 0xc0, out = L + 0xc0;
    crt_memset(l14 + 4, 0, 0x2c); SU32(l14, 0, 0);
    crt_memset(l15 + 4, 0, 0x2c); SU32(l15, 0, 0);
    crt_memset(l16 + 4, 0, 0x2c); SU32(l16, 0, 0);
    uint32_t n14, n15, n16;
    ACT_CALLR(n14, 6, rx_scene_group_query(scene, 0x1014, l14, 0xc, name, 0));
    ACT_CALLR(n15, 6, rx_scene_group_query(scene, 0x1015, l15, 0xc, name, 0));
    ACT_CALLR(n16, 6, rx_scene_group_query(scene, 0x1016, l16, 0xc, name, 0));
    if (n14 < 2) {   /* 00560f26 */
        if (n15 == 1) act_unreached(0x00560f3a, "a free ride start from one 0x1015 star start (the circle pattern)");
        if (n16 == 1) act_unreached(0x00561313, "a free ride start from one 0x1016 line start (the line pattern)");
        /* 00561584 .. 005615f3: the all-default fill (x += 2.0 [0x92e330]; y, z += 0 * 2.0; direction (1.0 [0x931f24], 0, 0)) */
        const float two = F32(0, 0x0092e330), one = F32(0, 0x00931f24), zero = 0.0f, step = zero * two;
        float x = zero, y = zero, z = zero;
        for (uint32_t k = 0; k < 0xc; k++) {
            const rx_addr d = A + 0x340 + 0x30 * k;
            SF32(d, 0x00, x); SF32(d, 0x04, y); SF32(d, 0x08, z);
            SF32(d, 0x10, one); SF32(d, 0x14, zero); SF32(d, 0x18, zero);
            y = step + y; z = step + z; x = x + two;
        }
#ifndef RX_ORACLE
        rx_fr_any_level_spawn(A);   /* RESTORATION (freeride_any_level.c): a spawn on a level the original gives none */
#endif
        ACT_END(L);
        return;
    }
    const rx_addr used = L + 0x90;
    for (uint32_t k = 0; k < 0x30; k += 4) SU32(used, k, 0);
    uint32_t i = 0;
    for (; i < n14; i++) {
        const rx_addr e = U32(l14, 4 * i);
        { const uint32_t a[1] = {out}; rr_vcall(e, 0x3c, 1, a); }
        const int32_t slot = (int32_t)U32(e, 0x90) - 1;
        if (slot >= 0 && U32(used, 4u * (uint32_t)slot) == 0) {
            const rx_addr d = A + 0x340 + 0x30u * (uint32_t)slot;
            SU32(d, 0x00, U32(out, 0x30)); SU32(d, 0x04, U32(out, 0x34)); SU32(d, 0x08, U32(out, 0x38));
            SU32(d, 0x10, U32(out, 0x20)); SU32(d, 0x14, U32(out, 0x24)); SU32(d, 0x18, U32(out, 0x28));
            SU32(used, 4u * (uint32_t)slot, 1);
        }
    }
    if (n14 < 0xc) {
        float x = 0.0f, y = 0.0f, z = 0.0f;
        const float two = F32(0, 0x0092e330), one = F32(0, 0x00931f24), zero = 0.0f, step = zero * two;
        for (uint32_t k = i; k < 0xc; k++) {
            const rx_addr d = A + 0x340 + 0x30 * k;
            SF32(d, 0x00, x); SF32(d, 0x04, y); SF32(d, 0x08, z);
            SF32(d, 0x10, one); SF32(d, 0x14, zero); SF32(d, 0x18, zero);
            y = step + y; z = step + z; x = x + two;
        }
    }
    (void)n15; (void)n16;
    ACT_END(L);
}
RX_FUNCTION(0x00560cd0, rx_free_ride_start_slots)

/* MXActivityFreeRide::vf4 00560070 (ECX = the activity): the event type "Collectible Obtained" on the activity's channel (+0x290): 00591f20 with
 * EAX = the channel (a tail jump) */
void rx_free_ride_vf4(rx_addr A) {
    act_event_type(0x00591f20, U32(A, 0x290), 0);
}
RX_FUNCTION(0x00560070, rx_free_ride_vf4)

/* 00584450 (the audio logics' vf29 of the classes without their own: a tail jump to ActivityAudioLogic::vf29 00580e70; thiscall activity; RET 4) */
void rx_audio_logic_vf29_shared(rx_addr l, rx_addr A) { rx_audio_logic_vf29(l, A); }
RX_FUNCTION(0x00584450, rx_audio_logic_vf29_shared)

/* MXActivityFreeRide::vf44 00560220 (thiscall; stdcall player; RET 4) -> a new FreeRideDriver: _aligned_malloc(0xf20, 16) (under the allocation lock 0xd61704 when
 * the lock flag 0xda67d8 is set), NetworkDriver's constructor (0050e1b0, EDI = the block), the FreeRideDriver vtable 0x0093fa1c, +0xee8 = 0 */
rx_addr rx_free_ride_vf44(rx_addr A, rx_addr player) {
    (void)A;
    if (U8(0, 0x00da67d8)) rr_os_cs_enter(0x00d61704);
    ACT_FRAME(L, 0, 1);                                                 /* push edi (after the lock) */
    const rx_addr d = rx_crt_aligned_malloc(0xf20, 16);
    if (U8(0, 0x00da67d8)) rr_os_cs_leave(0x00d61704);
    if (!d) { ACT_END(L); return 0; }
    ACT_CALL(1, (void)rx_network_driver_construct(d, player));
    SU32(d, 0, 0x0093fa1c);
    SU32(d, 0xee8, 0);
    ACT_END(L);
    return d;
}
RX_FUNCTION(0x00560220, rx_free_ride_vf44)

/* MXActivityFreeRide::vf52 00561620 (thiscall, no arguments used): [0x009c3edc] = 0, [0x009c3ed8] = 0.0, and [0x009c47c0] = -1 unless [0x009c47bc] is -1 */
void rx_free_ride_vf52(rx_addr A) {
    (void)A;
    SU32(0, 0x009c3edc, 0);
    SF32(0, 0x009c3ed8, 0.0f);
    if (U32(0, 0x009c47bc) != 0xffffffffu) SU32(0, 0x009c47c0, 0xffffffffu);
}
RX_FUNCTION(0x00561620, rx_free_ride_vf52)

/* Activity::vf17 00557cd0 (ECX = activity; shared by 18 classes): unless already in state 5: state 5 (Activity_SetState 005586c0), "Activity Inactive" on the
 * activity channel [0x00a93f74] and "End" on the activity's own channel (+0x290) with the activity, +0x260 = 1, the damage behaviour manager singleton (0047bb40's
 * construction, inlined) and 0047a880 for each of its 12 behaviours with a target (+0x3fc) */
void rx_activity_vf17(rx_addr A) {
    ACT_FRAME_EBP(L, 8, 8, 4);
    if (U32(A, 0x21c) != 5) {
        ACT_CALL(2, rx_activity_set_state(A, 5));
        uint32_t payload[1] = {A};
        race_raise_event(U32(0, 0x00a93f74), 0x00941da0 /* "Activity Inactive" */, 0x009cbd24, payload, 1);
        const rx_addr ch = U32(A, 0x290);
        SU32(A, 0x260, 1);
        race_raise_event(ch, 0x00941b9c /* "End" */, 0x009cbd24, payload, 1);
        (void)rx_world_damage_manager_instance();
        for (uint32_t k = 0; k < rx_dmg_n(); k++) {   /* (w-rc-core) + the side entries made */
            const rx_addr b = rx_dmg_entry(0x00db1410u, k);
            if (U32(b, 0x3fc) != 0) ACT_CALL(1, rx_world_damage_reset_parts(b));   /* 0047a880 */
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x00557cd0, rx_activity_vf17)

/* 0050abe0 (ESI = driver): with a vehicle (player +0x1ac): the driver's +0x3c0 / +0x3cc = 0.0; the vehicle's body handle (+0x17c) record's
 * +0x2c object gets +0x14 = -1 when the iterator has a container; the vehicle's body +0x1328 byte +0x14e = 0; the vehicle's vf49 (+0xc4) (0); the rider's
 * +0x5bb5c = 0, +0xa2 = 0, +0x5bb64 = 0, +0x5b6d0 = 0; driver +0xeec = 0 */
void rx_driver_reset_vehicle(rx_addr d) {
    const rx_addr player = U32(d, 0x74);
    if (U32(player, 0x1ac) == 0) return;
    SF32(d, 0x3c0, 0.0f); SF32(d, 0x3cc, 0.0f);
    rx_addr veh = U32(player, 0x1ac);
    if (U32(veh, 0x17c) != 0) {
        const rx_addr e = phys_body_record(U32(veh, 0x17c));   /* EAX = the handle the vehicle's +0x17c holds */
        SU32(U32(e, 0x2c), 0x14, 0xffffffffu);
    }
    veh = U32(U32(d, 0x74), 0x1ac);
    SU8(U32(veh, 0x1328), 0x14e, 0);
    { const uint32_t a[1] = {0}; rr_vcall(U32(U32(d, 0x74), 0x1ac), 0xc4, 1, a); }
    const rx_addr rider = U32(U32(d, 0x74), 0x1b4);
    if (rider) { SU32(rider, 0x5bb5c, 0); SU8(rider, 0xa2, 0); SU8(rider, 0x5bb64, 0); SU32(rider, 0x5b6d0, 0); }
    SU32(d, 0xeec, 0);
}
RX_FUNCTION(0x0050abe0, rx_driver_reset_vehicle)

/* Activity::vf15 00557a70 (ECX = activity; shared by 9 classes): in state 5 (inactive): the engine frame mark, vf18 (+0x48), Activity_SetState 0, +0x260 = 0;
 * with a session result (+0xfe8): each spectator driver (+0x27c, count +0x280) whose player is set (+4) gets vf45 (+0xb4) with the first racing driver (+0x268, count
 * +0x26c) whose player's +0x1e4 is 0; then every spectator driver's reset (0050abe0). Frame: push ecx / ebx / ebp / esi / edi = 5 words (the push ecx
 * is the frame's only slot, so no extra `sub`: the state change 005586c0 enters at the original's ESP) */
void rx_activity_vf15(rx_addr A) {
    ACT_FRAME(L, 0, 5);
    if (U32(A, 0x21c) == 5) {
        ACT_CALL(0, rx_engine_frame_mark(U32(U32(0, 0x00a95844), 0x10)));
        act_vcall(A, 0x48, 0, 0);
        ACT_CALL(2, rx_activity_set_state(A, 0));
        SU32(A, 0x260, 0);
    }
    if (U32(A, 0xfe8) != 0) {
        for (int32_t i = 0; i < I32(A, 0x280); i++) {
            const rx_addr s = U32(U32(A, 0x27c), 4u * (uint32_t)i);
            if (U32(U32(s, 0x74), 4) == 0) continue;
            for (int32_t j = 0; j < I32(A, 0x26c); j++) {
                const rx_addr r = U32(U32(A, 0x268), 4u * (uint32_t)j);
                if (U32(U32(r, 0x74), 0x1e4) == 0) { const uint32_t a[2] = {s, r}; act_vcall(A, 0xb4, 2, a); break; }
            }
        }
    }
    for (int32_t i = 0; i < I32(A, 0x280); i++) rx_driver_reset_vehicle(U32(U32(A, 0x27c), 4u * (uint32_t)i));
    ACT_END(L);
}
RX_FUNCTION(0x00557a70, rx_activity_vf15)

/* the terrain registry entry (0x009e30d4: entries, count 0x009e30dc, its static guard bit 0 of 0x009e30e0) whose +0x44a8 is the scene (0042d960: EDX = the registry,
 * EDI = the scene), 0 when none */
static rx_addr terrain_of_scene(rx_addr scene) {
    if (!(U8(0, 0x009e30e0) & 1)) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
    for (uint32_t i = 0; i < U32(0, 0x009e30dc); i++) {
        const rx_addr t = U32(0, 0x009e30d4 + 4 * i);
        if (t && U32(t, 0x44a8) == scene) return t;
    }
    return 0;
}

/* 00767df0 (EAX = the world; stdcall float; RET 4): with a scene (+0x20138), its terrain's +0xbc = clamp(value, 0, 1) * 2 (a NaN value is 1.0 for the terrain) */
void rx_world_set_terrain_wetness(rx_addr w, float v) {
    if (U32(w, 0x20138) == 0) return;
    if (v >= 1.0f) v = 1.0f;
    else if (!(0.0f < v) && !(v != v)) v = 0.0f;           /* x <= 0 (not NaN) -> 0 */
    const rx_addr t = terrain_of_scene(U32(w, 0x20138));
    if (!t) return;
    float r;
    if (v != v) r = 1.0f;                                    /* ucomiss: unordered */
    else if (!isfinite((double)v) || !(v < 1.0f)) r = 1.0f;  /* _finite / the upper clamp */
    else if (0.0f < v) r = v;
    else r = 0.0f;
    SF32(t, 0xbc, r * 2.0f);
}
RX_FUNCTION(0x00767df0, rx_world_set_terrain_wetness)

/* 00409380 / 00409470: the amount (+0 .. +0xf) / the wetness (+0x10 .. +0x1f) of every mud record of the twelve (0xa96d90 + 0x40 k) that has an owner (+0x30) cleared */
void rx_mud_records_clear_amounts(void) {
    for (uint32_t k = 0; k < rx_mud_n(); k++) { const rx_addr r = rx_mud_rec(k); if (U32(r, 0x30)) for (uint32_t o = 0; o < 0x10; o += 4) SU32(r, o, 0); }   /* (w-rc-core) + the side records */
}
RX_FUNCTION(0x00409380, rx_mud_records_clear_amounts)
void rx_mud_records_clear_wetness(void) {
    for (uint32_t k = 0; k < rx_mud_n(); k++) { const rx_addr r = rx_mud_rec(k); if (U32(r, 0x30)) for (uint32_t o = 0x10; o < 0x20; o += 4) SU32(r, o, 0); }   /* (w-rc-core) + the side records */
}
RX_FUNCTION(0x00409470, rx_mud_records_clear_wetness)

/* Activity::vf18 00557de0 (ECX = activity; the restart): +0x294 = 0.0, +0x2a4 = -FLT_MAX (0x977550), +0x300 = 0, +0xfec / +0xff0 = 0.0; each racing driver (+0x268) reset
 * (vf3) with its controller view's input record (the current scene's view of the player's +8 slot: the controller kind picks +0x10 / +0xb0 / +0x70) cleared at +4 and,
 * for a local human (+4 set, +0x10 clear), MXRecords_Reset; each spectator driver (+0x27c) reset; +0xbc0 = 0; the world's terrain +0x449c = 0; the terrain wetness
 * (00767df0) 1.0; the terrain +0x44b8 = 1; the mud records' amounts and wetness cleared; +0xff06c = 0 */
void rx_activity_vf18(rx_addr A) {
    ACT_FRAME_EBP(L, 8, 0, 4);
    SF32(A, 0x294, 0.0f);
    SU32(A, 0x2a4, U32(0, 0x00977550));
    SU32(A, 0x300, 0);
    SF32(A, 0xfec, 0.0f); SF32(A, 0xff0, 0.0f);
    for (int32_t i = 0; i < I32(A, 0x26c); i++) {
        const rx_addr d = U32(U32(A, 0x268), 4u * (uint32_t)i);
        act_vcall(d, 0xc, 0, 0);
        const int32_t slot = I32(U32(d, 0x74), 8);
        if (slot > -1) {
            const rx_addr ctx = U32(0, 0x00a95844), scene = U32(ctx, 0x1c + 4 * U32(ctx, 4));
            if (slot < I32(scene, 0x19d0)) {
                const rx_addr ctl = U32(U32(scene, 0x19b8 + 4 * (uint32_t)slot), 0x70);
                const uint32_t k = U32(ctl, 0);
                const rx_addr p = k == 1 ? ctl + 0xb0 : k == 2 ? ctl + 0x70 : ctl + 0x10;
                SU32(p, 4, 0);
            }
        }
        const rx_addr pl = U32(d, 0x74);
        if (U32(pl, 4) != 0 && U32(pl, 0x10) == 0) ACT_CALL(0, rx_records_reset(U32(0, 0x00a946c0), U32(pl, 0x1f0)));
    }
    for (int32_t i = 0; i < I32(A, 0x280); i++) act_vcall(U32(U32(A, 0x27c), 4u * (uint32_t)i), 0xc, 0, 0);
    const rx_addr w = U32(A, 0x234);
    SU32(A, 0xbc0, 0);
    if (U32(w, 0x20138) != 0) { const rx_addr t = terrain_of_scene(U32(w, 0x20138)); if (t) SU32(t, 0x449c, 0); }
    ACT_CALL(1, rx_world_set_terrain_wetness(w, 1.0f));
    if (U32(w, 0x20138) != 0) { const rx_addr t = terrain_of_scene(U32(w, 0x20138)); if (t) SU32(t, 0x44b8, 1); }
    rx_mud_records_clear_amounts();
    rx_mud_records_clear_wetness();
    SU32(A, 0xff06c, 0);
    ACT_END(L);
}
RX_FUNCTION(0x00557de0, rx_activity_vf18)

/* MXActivityFreeRide::vf18 00561600 (ECX = activity): the start slots (00560cd0), then Activity::vf18 (a tail jump) */
void rx_free_ride_vf18(rx_addr A) {
    ACT_FRAME(L, 0, 1);
    ACT_CALL(1, rx_free_ride_start_slots(A));
    ACT_END(L);
    rx_activity_vf18(A);
}
RX_FUNCTION(0x00561600, rx_free_ride_vf18)

/* ActivityAudioLogic::vf25 00582100 (ECX = logic; two arguments unused; RET 8): the "Inactive Entered" cue (005811a0) */
void rx_audio_logic_vf25(rx_addr l) {
    ACT_FRAME(L, 0, 1);
    ACT_CALL(4, rx_audio_cue(l, 0x0094284c /* "Inactive Entered" */, 0, 0));
    ACT_END(L);
}
RX_FUNCTION(0x00582100, rx_audio_logic_vf25)

/* ActivityAudioLogic::vf26 00582120 (ECX = logic; two arguments unused; RET 8): the "Inactive Exited" cue (005811a0), then the logic's vf35 (+0x8c) */
void rx_audio_logic_vf26(rx_addr l) {
    ACT_FRAME(L, 0, 1);
    ACT_CALL(4, rx_audio_cue(l, 0x00942860 /* "Inactive Exited" */, 0, 0));
    act_vcall(l, 0x8c, 0, 0);
    ACT_END(L);
}
RX_FUNCTION(0x00582120, rx_audio_logic_vf26)

#ifndef RX_ORACLE
/* a tail jump to a virtual (`mov eax, [ecx]; jmp [eax + off]`): the callee is entered at the thunk's own stack pointer (4 * n + 4 bytes above act_vcall's pushes) */
static uint32_t vslot_tail(rx_addr obj, uint32_t off, int n, const uint32_t* a) {
    const rx_addr sp = rx_sp();
    rx_set_sp(sp + 4u * (uint32_t)n + 4u);
#ifdef RX_ORACLE
    const uint32_t r = rx_call_original(U32(U32(obj, 0), off), RX_THISCALL, obj, n, a);
#else
    const uint32_t r = act_vcall(obj, off, n, a);
#endif
    rx_set_sp(sp);
    return r;
}

#endif
/* 00592ba0 (ECX = an object; `mov eax, [ecx]; jmp [eax + 0x68]`: the object's virtual 26 with the delegate's two argument dwords; RET 8 by the callee) */
#ifndef RX_ORACLE   /* a tail-jump thunk: product only (the oracle runs the original jmp) */
void rx_vslot26_thunk(rx_addr obj, uint32_t a0, uint32_t a1) {
    const uint32_t a[2] = {a0, a1};
    vslot_tail(obj, 0x68, 2, a);
}
RX_FUNCTION(0x00592ba0, rx_vslot26_thunk)
#endif

/* 0075c530 (-> EAX): the game's current world: the UI world (+0x20c8) in game state 1; in states 2 / 3 the UI world when the event scene (the engine's +0x20) is not
 * active and the UI scene (+0x1c) is, 0 when neither; else the event world (+0x5a54) */
rx_addr rx_game_current_world(void) {
    const rx_addr g = U32(0, 0x00a946b8), ctx = U32(0, 0x00a95844);
    const uint32_t st = U32(g, 0x18);
    if (st == 1) return U32(g, 0x20c8);
    if ((st == 2 || st == 3) && U32(U32(ctx, 0x20), 0x34) != 1) return U32(U32(ctx, 0x1c), 0x34) == 1 ? U32(g, 0x20c8) : 0;
    return U32(g, 0x5a54);
}
RX_FUNCTION(0x0075c530, rx_game_current_world)

/* 0055e560 (stdcall activity; RET 4) -> EAX: the activity's music entity (type 0x107c): in the scene of its world the one in the activity's area group (+0x118), else
 * the one in the group "<current world's level>Base" (the level name +0x20030 of the game's current world, "" without one) */
rx_addr rx_activity_music_entity(rx_addr A) {
    ACT_FRAME(L, 0x10c, 1);
    SU32(L, 0x10c, U32(0, 0x009b45e4) ^ (L + 4));                       /* the /GS cookie */
    const rx_addr list = L + 4, buf = L + 8;
    SU32(list, 0, 0);
    uint32_t n;
    ACT_CALLR(n, 6, rx_scene_group_query(U32(U32(A, 0x234), 0x20138), 0x107c, list, 1, A + 0x118, 0));
    if (n == 0) {
        ACT_FRAME(R, 0, 2);                                             /* push esi / edi (0055e5af) */
        rx_addr w;
        ACT_CALLR(w, 0, rx_game_current_world());
        const rx_addr nm = w ? w + 0x20030 : 0x00935d6c;
        const rx_addr sp = rx_sp();
        SU32(sp, -8, 0x0093f350); SU32(sp, -4, nm);
        rx_set_sp(sp - 12);
        rx_format_buffer(buf, 0x104, 0x0093f350 /* "%sBase" */, sp - 4);
        rx_set_sp(sp);
        ACT_CALL(6, (void)rx_scene_group_query(U32(U32(A, 0x234), 0x20138), 0x107c, list, 1, buf, 0));
        ACT_END(R);
    }
    const rx_addr r = U32(list, 0);
    ACT_END(L);
    return r;
}
RX_FUNCTION(0x0055e560, rx_activity_music_entity)

/* ActivityAudioLogic::vf35 005821a0 (ECX = logic): once (+0x94): with the activity's music entity the music player (MXGame + 0x15c6c) is started on its tracks
 * (00677440: EDI = the entity + 0x90's list). PRODUCT BOUNDARY (core/plan/QUEUE.md "chain"): the music player (the sound engine's music playlist: nothing in the
 * simulation reads it) is not built; the oracle runs the original */
void rx_audio_logic_vf35(rx_addr l) {
    if (U32(l, 0x94) != 0) return;
    SU32(l, 0x94, 1);
    rx_addr e;
    ACT_CALLR(e, 1, rx_activity_music_entity(U32(l, 0x50)));
    if (e != 0) {
#ifdef RX_ORACLE
        ACT_FRAME(L, 0, 1);                                             /* push edi (005821c6) */
        rx_regs in; memset(&in, 0, sizeof in); in.r[7] = e + 0x90;
        const uint32_t a = U32(0, 0x00a946b8) + 0x15c6c;
        rx_call_original_ex(0x00677440, &in, 0, 1, &a);
        ACT_END(L);
#else
        rx_music_tracks(e + 0x90);   /* w-sfx: the playlist and its draws of the game's LCG (game/music_player.c) */
#endif
    }
}
RX_FUNCTION(0x005821a0, rx_audio_logic_vf35)

/* 0055e620 (stdcall activity; RET 4) -> EAX: the activity's second music entity (type 0x107d), found like 0055e560's (the group "<current world's level>Base" without a
 * null test of the world) */
rx_addr rx_activity_music_entity2(rx_addr A) {
    ACT_FRAME(L, 0x10c, 1);
    SU32(L, 0x10c, U32(0, 0x009b45e4) ^ (L + 4));                       /* the /GS cookie */
    const rx_addr list = L + 4, buf = L + 8;
    SU32(list, 0, 0);
    uint32_t n;
    ACT_CALLR(n, 6, rx_scene_group_query(U32(U32(A, 0x234), 0x20138), 0x107d, list, 1, A + 0x118, 0));
    if (n == 0) {
        ACT_FRAME(R, 0, 2);                                             /* push esi / edi (before the 0075c530 call) */
        rx_addr w;
        ACT_CALLR(w, 0, rx_game_current_world());
        const rx_addr sp = rx_sp();
        SU32(sp, -8, 0x0093f350); SU32(sp, -4, w + 0x20030);
        rx_set_sp(sp - 12);
        rx_format_buffer(buf, 0x104, 0x0093f350 /* "%sBase" */, sp - 4);
        rx_set_sp(sp);
        ACT_CALL(6, (void)rx_scene_group_query(U32(U32(A, 0x234), 0x20138), 0x107d, list, 1, buf, 0));
        ACT_END(R);
    }
    const rx_addr r = U32(list, 0);
    ACT_END(L);
    return r;
}
RX_FUNCTION(0x0055e620, rx_activity_music_entity2)

/* ActivityAudioLogic::vf15 00581f50 (ECX = logic): the owner name (vf34), the localized sound list's owner stopped when the list exists ([0x00a957d4]: 0080c880: audio),
 * the "Pre Entered" cue, vf36 (+0x90) */
void rx_audio_logic_vf15(rx_addr l) {
    ACT_FRAME(L, 0, 1);
    const uint32_t owner = act_vcall(l, 0x88, 0, 0);
    if (U32(0, 0x00a957d4) != 0) { const uint32_t a[1] = {owner}; act_ui_call(0x0080c880, RX_STDCALL, 0, 1, a); }
    ACT_CALL(4, rx_audio_cue(l, 0x009427a4 /* "Pre Entered" */, 0, 0));
    act_vcall(l, 0x90, 0, 0);
    ACT_END(L);
}
RX_FUNCTION(0x00581f50, rx_audio_logic_vf15)

/* ActivityAudioLogic::vf36 005821e0 (ECX = logic): the game's +0x161e4 = 0; with the activity's second music entity of tracks (+0x94 > 0) the music player (MXGame + 0x15c6c)
 * stopped (006770d0: EDI = entity + 0x90). PRODUCT BOUNDARY: the music player (as vf35's) */
void rx_audio_logic_vf36(rx_addr l) {
    rx_addr e;
    ACT_CALLR(e, 1, rx_activity_music_entity2(U32(l, 0x50)));
    const rx_addr g = U32(0, 0x00a946b8);
    SU32(g, 0x161e4, 0);
    if (e != 0 && I32(e, 0x94) > 0) {
#ifdef RX_ORACLE
        ACT_FRAME(L, 0, 1);                                             /* push edi (00582204) */
        rx_regs in; memset(&in, 0, sizeof in); in.r[7] = e + 0x90;
        const uint32_t a = g + 0x15c6c;
        rx_call_original_ex(0x006770d0, &in, 0, 1, &a);
        ACT_END(L);
#else
        rx_music_scores(e + 0x90);   /* w-sfx (game/music_player.c) */
#endif
    }
}
RX_FUNCTION(0x005821e0, rx_audio_logic_vf36)

/* ActivityAudioLogic::vf37 00582220 (ECX = logic): the game's +0x161e4 = 0; with the activity's second music entity's tracks (+0xa8 > 0) the music player
 * (MXGame + 0x15c6c) stopped (006770d0: EDI = entity + 0xa4). PRODUCT BOUNDARY: the music player (as vf35's) */
void rx_audio_logic_vf37(rx_addr l) {
    rx_addr e;
    ACT_CALLR(e, 1, rx_activity_music_entity2(U32(l, 0x50)));
    const rx_addr g = U32(0, 0x00a946b8);
    SU32(g, 0x161e4, 0);
    if (e != 0 && I32(e, 0xa8) > 0) {
#ifdef RX_ORACLE
        ACT_FRAME(L, 0, 1);                                             /* push edi (00582244) */
        rx_regs in; memset(&in, 0, sizeof in); in.r[7] = e + 0xa4;
        const uint32_t a = g + 0x15c6c;
        rx_call_original_ex(0x006770d0, &in, 0, 1, &a);
        ACT_END(L);
#else
        rx_music_scores(e + 0xa4);   /* w-sfx (game/music_player.c) */
#endif
    }
}
RX_FUNCTION(0x00582220, rx_audio_logic_vf37)

/* 00592a60 (ECX = an object; `mov eax, [ecx]; jmp [eax + 0x3c]`: its virtual 15 with the delegate's two argument dwords; RET 8 by the callee) */
#ifndef RX_ORACLE   /* a tail-jump thunk: product only (the oracle runs the original jmp) */
void rx_vslot15_thunk(rx_addr obj, uint32_t a0, uint32_t a1) {
    const uint32_t a[2] = {a0, a1};
    vslot_tail(obj, 0x3c, 2, a);
}
RX_FUNCTION(0x00592a60, rx_vslot15_thunk)
#endif

/* 004dd3d0 (stdcall slot; RET 4) -> EAX: whether a Player List row has the controller `slot` (the "Controller" column's index of that value (00418e50) holds a row),
 * cached in 0x00ae0e80 + 4 slot and rebuilt for the four slots when the Player List's change counter (0x00ae0e70) moved from the cached one (0x00ae0e7c) */
uint32_t rx_player_controller_present(uint32_t slot) {
    ACT_FRAME(L, 0, 4);
    if (U32(0, 0x00ae0e70) != U32(0, 0x00ae0e7c)) {
        SU32(0, 0x00ae0e7c, U32(0, 0x00ae0e70));
        for (uint32_t i = 0; i < 4; i++) {
            SU32(L, 0x14, i);
            uint32_t c;
            ACT_CALLR(c, 1, rx_tbl_column_index(ACT_PLAYER_LIST, 0x0093ded0 /* "Controller" */));
            if (c >= (U32(0, 0x00ae0e1c) - U32(0, 0x00ae0e18)) >> 2) crt_invalid_parameter_noinfo();
            rx_addr set;
            ACT_CALLR(set, 1, rx_tbl_rows(U32(U32(0, 0x00ae0e18), 4 * c), (int32_t)i));
            SU32(0, 0x00ae0e80 + 4 * i, U32(set, 0x1c) != 0);
        }
    }
    const uint32_t r = U32(0, 0x00ae0e80 + 4 * slot);
    ACT_END(L);
    return r;
}
RX_FUNCTION(0x004dd3d0, rx_player_controller_present)

/* Fn_00756040 (EAX = the name, ESI = the level config): the config's name (+0 .. +0x103) = the name (strncpy 0x103), terminated at +0x103 */
void rx_level_config_set_name(rx_addr cfg, rx_addr name) {
    crt_strncpy(cfg, name, 0x103);
    SU8(cfg, 0x103, 0);
}
RX_FUNCTION(0x00756040, rx_level_config_set_name)

/* Fn_00753200 / Fn_00753250 (EAX = the save-game / records object [0x00d92f10]): for each of the four controller slots in use (+0x3c + 4 k): the slot's counter
 * (+0x138 / +0x1270 / +0x23a8 / +0x34e0; for the second function +8 each) incremented and its working value (+0x45a4 / +0x4e40 / +0x56dc / +0x5f78; +4 each) cleared */
static void records_slot_counters(rx_addr o, uint32_t cnt_delta, uint32_t val_delta) {
    static const uint16_t cnt[4] = {0x138, 0x1270, 0x23a8, 0x34e0}, val[4] = {0x45a4, 0x4e40, 0x56dc, 0x5f78};
    for (uint32_t k = 0; k < 4; k++) {
        if (U32(o, 0x3c + 4 * k) == 0) continue;
        SU32(o, cnt[k] + cnt_delta, U32(o, cnt[k] + cnt_delta) + 1);
        SU32(o, val[k] + val_delta, 0);
    }
}
void rx_records_slots_start(rx_addr o) { records_slot_counters(o, 0, 0); }
RX_FUNCTION(0x00753200, rx_records_slots_start)
void rx_records_slots_attempt(rx_addr o) { records_slot_counters(o, 8, 4); }
RX_FUNCTION(0x00753250, rx_records_slots_attempt)

/* the save-game / records object's current activity (Fn 00753760: thiscall object, stdcall activity; RET 4): an activity of the event world (+0x20018 == 0) other than
 * the current one (+0x1c) becomes it, +0x67a0 = -1, then the EventTypeLocations row of {EventTypeId (the activity kind's: 004e7780), EventLocationId (the UI
 * property: the host's spec), TrackId (the activity's area +0x220)} (004ed410) sets +0x67a0 = its EventTypeLocationId */
/* the registers the original leaves for its caller (live: EAX, EDX): the world (+0x234) when only the first test passed, EDX = the criteria vector's end when it was destroyed */
static uint32_t g_sa_eax, g_sa_edx, g_sa_flags;
uint32_t rx_records_set_activity_regs(uint32_t which, uint32_t* valid) {
    *valid = (g_sa_flags >> which) & 1u;
    return which == 0 ? g_sa_eax : g_sa_edx;
}
void rx_records_set_activity(rx_addr rec, rx_addr act) {
    ACT_FRAME_EBP(L, 8, 0xe4, 3);
    g_sa_flags = 0;
    if (act != U32(rec, 0x1c)) { g_sa_eax = U32(act, 0x234); g_sa_flags |= 1u; }
    if (act != U32(rec, 0x1c) && U32(U32(act, 0x234), 0x20018) == 0) {
        g_sa_flags &= ~1u;
        SU32(rec, 0x1c, act);
        SU32(rec, 0x67a0, 0xffffffffu);
        rx_addr table;
        ACT_CALLR(table, 1, rx_tbl_find(0x00ae0910, 0x00936e7c /* "EventTypeLocations" */));
        uint32_t kind, etid;
        ACT_CALLR(kind, 0, rx_activity_ui_kind(act));
        ACT_CALLR(etid, 1, rx_event_type_id_of(kind));
        const rx_addr loc = L + 0x10, vec = L + 0x28, crit = L + 0x40, out = L + 0x18, tmp = L + 0xc;   /* the locals: +0xc the type id, then the track id; +0x10 the location */
        SU32(tmp, 0, etid);
        SU32(loc, 0, 0xffffffffu);
#ifdef RX_ORACLE
        { rx_regs in, o; memset(&in, 0, sizeof in); in.r[0] = 0x00936e20; const uint32_t a[2] = {U32(0, 0x00a93fcc), loc}; rx_call_original_ex(0x005dc0c0, &in, &o, 2, a); }
#else
        (void)rx_ui_prop_get_int(0x00936e20 /* "EventLocationId" */, loc);
#endif
        ACT_CALL(0, rx_tbl_crit_vec_init(vec));
        static const uint32_t names[3] = {0x00936e90 /* "EventTypeId" */, 0x00936e20 /* "EventLocationId" */, 0x00936e40 /* "TrackId" */};
        const uint32_t vals[3] = {etid, U32(loc, 0), U32(act, 0x220)};
        for (int k = 0; k < 3; k++) {
            if (k == 2) SU32(tmp, 0, U32(act, 0x220));
            ACT_CALL(1, rx_tbl_crit_init_int(crit, names[k], 0, vals[k]));
            ACT_CALL(1, rx_tbl_crit_push(vec, crit));
            ACT_CALL(0, rx_tbl_crit_destroy(crit));
        }
        SU32(out, 0, 0xffffffffu); SU32(out, 8, 0);
        ACT_CALL(3, rx_tbl_find_first(table, vec, out));
        if (U32(out, 8) != 0 && U32(out, 0) != 0xffffffffu) {
            const rx_addr t = U32(out, 8);
            uint32_t col;
            ACT_CALLR(col, 1, rx_tbl_column_index(t, 0x0093e228 /* "EventTypeLocationId" */));
            rx_addr cell;
            ACT_CALLR(cell, 1, rx_tbl_cell_word(t, col, U32(out, 0)));
            SU32(rec, 0x67a0, U32(cell, 0));
        }
        g_sa_edx = U32(vec, 0xc) ? U32(vec, 0x10) : 0; g_sa_flags |= U32(vec, 0xc) ? 2u : 0u;
        ACT_CALL(0, rx_tbl_crit_vec_destroy(vec));
    }
    ACT_END(L);
}
RX_FUNCTION(0x00753760, rx_records_set_activity)

/* 00402540 (EAX = the session object [0x00da6cb0]) -> EAX: 1 for a networked session (+0x1c4c set and the mode +0x1b0 not 1) */
uint32_t rx_session_is_networked(rx_addr s) { return U32(s, 0x1c4c) != 0 && U32(s, 0x1b0) != 1; }
RX_FUNCTION(0x00402540, rx_session_is_networked)

/* FUN_00766930 (stdcall world; RET 4): the world's activities' vf3 (0xc) when it is the event world (+0x20018 == 0: +0x21da4, count +0x21da8), then every local-kind
 * (3) player whose vehicle (+0x1ac) belongs to the world (+0x188) and who has an activity (+0x94) leaves it: Activity_OnPlayerLeft (the activity, the player's
 * driver: the racing list +0x268, or the waiting list +0x27c for +0x908 != 0, at the player's index in it (0055a050)) */
void rx_world_players_leave(rx_addr w) {
    ACT_FRAME(L, 0, 4);
    if (U32(w, 0x20018) == 0) {
        const int32_t n = I32(w, 0x21da8);
        for (int32_t i = 0; i < n; i++) act_vcall(U32(U32(w, 0x21da4), 4u * (uint32_t)i), 0xc, 0, 0);
    }
    const rx_addr list = U32(0, 0x00a946b8) + 0x7460;
    for (uint32_t i = 0;; i++) {
        uint32_t cnt;
        ACT_CALLR(cnt, 0, rx_players_count(list, 3));
        if ((int32_t)i >= (int32_t)cnt) break;
        rx_addr p;
        ACT_CALLR(p, 2, rx_world_players_find(list, i, 3));
        const rx_addr veh = U32(p, 0x1ac), act = U32(p, 0x94);
        if (veh != 0 && w == U32(veh, 0x188) && act != 0) {
            int32_t idx;
            ACT_CALLR(idx, 0, rx_world_group_rider_index(act, p));
            const rx_addr d = U32(p, 0x908) == 0 ? U32(U32(act, 0x268), 4u * (uint32_t)idx) : U32(U32(act, 0x27c), 4u * (uint32_t)idx);
            ACT_CALL(2, rx_activity_on_player_left(act, d));
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x00766930, rx_world_players_leave)

/* 005d86e0 UITrigger::vf3 (thiscall; shared by 6 classes; the activities' vf51 "Last Human Left Activity" among them): +0x5c = 0 */
void rx_shared_clear_5c(rx_addr self) { SU32(self, 0x5c, 0); }
RX_FUNCTION(0x005d86e0, rx_shared_clear_5c)

/* 0055f630 (EAX = the activity manager {+4 the activities, +8 their count}, EDI = the kind, stack the area; RET 4) -> EAX: the first activity of that kind (+0x23c) and area (+0x220), else 0 */
rx_addr rx_activity_find_by_kind_area(rx_addr mgr, uint32_t kind, uint32_t area) {
    for (int32_t i = 0; i < I32(mgr, 8); i++) {
        const rx_addr a = U32(U32(mgr, 4), 4u * (uint32_t)i);
        if (kind == U32(a, 0x23c) && area == U32(a, 0x220)) return a;
    }
    return 0;
}
RX_FUNCTION(0x0055f630, rx_activity_find_by_kind_area)
