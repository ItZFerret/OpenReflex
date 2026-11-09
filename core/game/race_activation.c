/* race_activation.c -- the Free Ride activation areas and the career Free Ride's discovery collectables.
 *
 * A Free Ride level ("<level>FreeRide" scene group) has "StartActivity" MXTriggers: +0xc0 the trigger type name, +0x104 the area's
 * TrackId, +0x108 the activity it starts ("HillClimb", "Trailblazer", "TargetPractice", ...). The trigger update (fx_entity.c
 * 0040a8a0 / 0040a980) posts "VehicleEntered" / "VehicleExited" {vehicle, trigger} on the vehicle trigger channel [0x00a89bc8].
 * The UI manager listens to both (MXUIManager::vf42 005999f0 subscribes them through 005bc930, proxy vtable 0x0094e8d8):
 *   005a3b60 MXUIManager_OnTriggerEnter: in "discovery mode" (the Free Ride activity's vf54: the UI property InCareerMode, one local
 *            human, the event world) a StartActivity trigger with an activity whose vehicle class the player's vehicle has marks the
 *            area in the vehicle's MXPlayer (+0x190): +0xa0 = 1, +0xa4 the activity's name, +0x1a8 the TrackId, and sets the UI
 *            properties EventTypeActivation (the activity) and TrackId (the AAA notification page reads them). A vehicle of another
 *            class sets ChallengeRequiredVehicleClass instead (the notification names the class it needs).
 *   005a3d00 MXUIManager_OnTriggerExit: leaving a StartActivity trigger: +0xa0 = 0, +0xa4 "FreeRide", +0x1a8 0, EventTypeActivation
 *            "FreeRide", TrackId 0, ChallengeRequiredVehicleClass -1.
 * The pause key in an area (MXUIManager_OnPauseRequest 00598e40, UI) reads the MXPlayer fields back and starts the activity through
 * the AAA loading page: in the product that is the host's (the fields are exported by rx_session_activation).
 * Callees: 00596540 (the challenge activity of a name), 00410830 (the player's vehicle has the class the activity's spec requires: the
 * Player List row of the controller 004e78b0, the vehicle record 00542ff0, the VehicleClassType rows of the class 004199d0), 004109d0
 * (the class it requires). The spec is the level's MXChallengesSpecEntity (world + 0x22140): a class index (0 .. 9) per activity type
 * and track at +4 * (3 * (type + 0x4e) + track); the index names one of the VehicleClassType classes 1 2 3 15 16 17 19 21 22 27.
 *
 * UI BOUNDARY (core/plan/QUEUE.md "w-activation"): the UI property writes go to the UI manager's store (oracle: the original
 * UIProperty_SetString 005dbf50 / SetInt 005dbfb0) or, in the product, to the owned property store (career_properties.c: kept for a
 * career session, dropped for an exhibition one). Nothing the Free Ride world runs reads EventTypeActivation, TrackId or
 * ChallengeRequiredVehicleClass back: their readers are the UI pages (00598e40, MXUIAAALoadingPage::vf4 005a8030, MXUIManager::vf34
 * 0059a2f0), the next event's setup (SetupEvent 0059a970 / StartEvent 0059ade0 / 0059f2d0: a new session in the product, whose spec
 * the host fills from the MXPlayer fields) and the network spectator paths (0054db80 / 0054de80, online only).
 *
 * The career Free Ride (vf54 = 1): MXActivityFreeRide::vf37 calls 00585520 instead of hiding the collectables: the area's jump
 * collectables (scene type 0x106d) already found by the player (0054d640: the profile's PlayerCollectables row of the location,
 * bit `index` of HitDiscoveryCollectables; the row is created when missing) are marked found (+0x140 = 1) and hidden with the
 * "DISC_" markers within 20 units of them. The "AAA_" area markers stay shown (00561820 is not called in discovery mode).
 *
 * Every function is a hand transliteration of the disassembly; frames at the original's stack addresses. */
#include "race_activation.h"
#include "build_activity_util.h"
#include "build_activity_freeride_run.h"
#include "build_activity_int.h"
#include "build_tables.h"
#include "build_vehicle.h"
#include "fx.h"
#include "loop_player.h"
#include "race_activity_ui.h"
#include "world_misc.h"
#include "crt.h"
#include <string.h>
#ifndef RX_ORACLE
#include "build_ui_props.h"
#include "career_properties.h"
#include "rider_cap.h"
#endif

/* the strings the original passes */
#define STR_START_ACTIVITY        0x0094d0d4u   /* "StartActivity" (the trigger type) */
#define STR_EVENT_TYPE            0x00935e08u   /* "EventType" */
#define STR_EVENT_TYPE_ACTIVATION 0x0094c320u   /* "EventTypeActivation" */
#define STR_TRACK_ID              0x00936e40u   /* "TrackId" */
#define STR_REQUIRED_CLASS        0x0094d0e4u   /* "ChallengeRequiredVehicleClass" */
#define STR_FREE_RIDE             0x00935ddcu   /* "FreeRide" */
#define STR_CONTROLLER            0x0093ded0u   /* "Controller" */
#define STR_VEHICLE_ID            0x00936cb4u   /* "VehicleId" */
#define STR_VEHICLE_CLASS_TYPE    0x00936cc0u   /* "VehicleClassType" (the default database's table) */
#define STR_VEHICLE_CLASS_ID      0x00936cd4u   /* "VehicleClassId" */
#define STR_VEHICLE_TYPE_ID       0x00936ce4u   /* "VehicleTypeId" */
#define STR_EVENT_LOCATION_ID     0x00936e20u   /* "EventLocationId" (UI property and column) */
#define STR_PLAYER_COLLECTABLES   0x009416c0u   /* "PlayerCollectables" (the profile database's table) */
#define STR_HIT_DISCOVERY         0x00941870u   /* "HitDiscoveryCollectables" */
#define STR_TARGET_PRACTICE       0x0094188cu   /* "TargetPracticeCollectables" */
#define STR_MXGAME                0x0093e42cu   /* "MXGame" (the tuning record) */
#define STR_DISCOVERY_RADIUS      0x009428dcu   /* "DiscoveryCollectibleCollisionRadius" */
#define STR_DISC_PREFIX           0x00941facu   /* "DISC_" */
#define CHALLENGE_NAMES           0x009d2318u   /* the 11 challenge activity names ("BigAir" ...) */
#define PLAYER_LIST               0x00ae0e08u   /* "Player List" (the static player table; its column vector at +0x10 / +0x14) */
#define DEFAULT_DATABASE          0x00ae0910u
#define PROFILE_DATABASE(n)       (0x00ae10f0u + (uint32_t)(n) * 0x1f4u)
#define DISCOVERY_RADIUS_GLOBAL   0x009ca32cu   /* the collision radius (read from the tuning, then squared) */

/* The column object `index` of a table: the checked vector's range test (_invalid_parameter_noinfo, IAT 0x916224, never fires on a
 * valid table) and the element. */
static rx_addr column_at(rx_addr table, uint32_t index) {
    if (index >= ((U32(table, 0x14) - U32(table, 0x10)) >> 2)) crt_invalid_parameter_noinfo();
    return U32(U32(table, 0x10), 4 * index);
}

/* ---- the UI property store (the UI manager's: oracle the original accessors; product the owned property store) ----------------
 * Called at the caller's ESP (the original's before its pushes): rx_call_original pushes the arguments itself. */

/* UIProperty_GetString 005dc060 (EAX = key, EDI = buffer, ESI = size; stdcall ui; RET 4) -> found */
static uint32_t ui_get_string(rx_addr ui, rx_addr key, rx_addr buffer, uint32_t size) {
#ifdef RX_ORACLE
    rx_regs in, out;
    memset(&in, 0, sizeof in);
    memset(&out, 0, sizeof out);
    in.r[0] = key;
    in.r[7] = buffer;
    in.r[6] = size;
    const uint32_t a[1] = {ui};
    rx_call_original_ex(0x005dc060, &in, &out, 1, a);
    return out.r[0];
#else
    (void)ui;
    return rx_ui_prop_get_string(key, buffer, size);
#endif
}

/* UIProperty_SetString 005dbf50 (EDI = key, ESI = ui; stdcall value; RET 4) */
static void ui_set_string(rx_addr ui, rx_addr key, rx_addr value) {
#ifdef RX_ORACLE
    rx_regs in, out;
    memset(&in, 0, sizeof in);
    memset(&out, 0, sizeof out);
    in.r[7] = key;
    in.r[6] = ui;
    const uint32_t a[1] = {value};
    rx_call_original_ex(0x005dbf50, &in, &out, 1, a);
#else
    (void)ui;
    rx_owned_property_set_string(key, value);
#endif
}

#ifndef RX_ORACLE
/* ChallengeRequiredVehicleClass as the handlers last set it: the product's copy for the host's AAA notification (the exhibition
 * session has no UI property store to keep it in; a career session's owned store keeps it too). */
static int32_t g_required_class = -1;
int32_t rx_activation_required_class(void) { return g_required_class; }
#endif

/* UIProperty_SetInt 005dbfb0 (ECX = ui; stdcall key, value; RET 8) */
static void ui_set_int(rx_addr ui, rx_addr key, int32_t value) {
#ifdef RX_ORACLE
    const uint32_t a[2] = {key, (uint32_t)value};
    rx_call_original(0x005dbfb0, RX_THISCALL, ui, 2, a);
#else
    (void)ui;
    if (key == STR_REQUIRED_CLASS) g_required_class = value;
    rx_owned_property_set_int(key, (uint32_t)value);
#endif
}

/* ======================================================================================================================== */
/* the challenge's activity and vehicle class                                                                                */

/* 00596540 (cdecl name) -> the index of the name among the 11 challenge activity names (0x009d2318: _stricmp), 0xb when none */
uint32_t rx_challenge_activity_index(rx_addr name) {
    uint32_t i = 0;
    do {
        int32_t d;
        ACT_CALLR(d, 2, crt_stricmp(name, U32(0, CHALLENGE_NAMES + 4 * i)));
        if (d == 0) return i;
        i++;
    } while (i < 0xb);
    return 0xb;
}
RX_FUNCTION(0x00596540, rx_challenge_activity_index)

/* The vehicle classes a spec's class index names (both 00410830 and 004109d0 build this table on their stack). */
static const uint32_t g_challenge_classes[10] = {1, 2, 3, 0xf, 0x10, 0x11, 0x13, 0x15, 0x16, 0x1b};

/* 004109d0 (EAX = type, ECX = track; stdcall spec; RET 4) -> the vehicle class the activity's area requires: the spec's class index
 * of {type, track} (+4 * (3 * (type + 0x4e) + track)) through the class table, -1 when the index is out of 0 .. 9.
 * Frame: sub 0x28 (the class table at +0 .. +0x24). */
int32_t rx_challenge_required_class(uint32_t type, uint32_t track, rx_addr spec) {
    ACT_FRAME(L, 0x28, 0);
    const uint32_t slot = track + 3u * (type + 0x4eu);
    const int32_t index = I32(spec, 4u * slot);
    for (uint32_t k = 0; k < 10; k++) SU32(L, 4 * k, g_challenge_classes[k]);
    int32_t result = -1;
    if (index >= 0 && index < 10) result = (int32_t)g_challenge_classes[index];
    ACT_END(L);
    return result;
}
RX_FUNCTION(0x004109d0, rx_challenge_required_class)

/* 004e78b0 MXTableHelper_GetPlayerTableRow (stdcall controller; RET 4) -> the row reference {+0 row, +8 the store} of the first Player
 * List row whose Controller is the controller (the column's non-unique index 00418e50; the key is the argument slot), else the shared
 * "no row" reference 0x9c9ff4. Frame: push esi; push edi. */
rx_addr rx_player_table_row(int32_t controller) {
    const rx_addr key_slot = rx_sp() + 4;
    SU32(key_slot, 0, (uint32_t)controller);                                    /* the caller's push (the index's key is its address) */
    ACT_FRAME(L, 0, 2);
    uint32_t c;
    ACT_CALLR(c, 1, rx_tbl_column_index(PLAYER_LIST, STR_CONTROLLER));          /* 00402160 */
    const rx_addr column = column_at(PLAYER_LIST, c);
    rx_addr set;
    ACT_CALLR(set, 1, rx_tbl_rows(column, I32(key_slot, 0)));                  /* 00418e50 (EAX = &key) */
    rx_addr result = 0x009c9ff4u;
    if (U32(set, 0x1c) != 0) {
        const rx_addr first = U32(U32(set, 0x18), 0);                           /* begin(): the head's left */
        const rx_addr proxy = U32(set, 0);
        if (proxy == 0) crt_invalid_parameter_noinfo();
        else if (first == U32(U32(proxy, 0), 0x18)) crt_invalid_parameter_noinfo();   /* dereferencing end() */
        result = first + 0xc;
    }
    ACT_END(L);
    return result;
}
RX_FUNCTION(0x004e78b0, rx_player_table_row)

/* 00542ff0 (EAX = the vehicle list (the game's +0x154b0), EDI = a VehicleId) -> the first record of the list's array (+0x28, count
 * +0x2c) whose +4 is the id, else 0. Frame: push esi. */
rx_addr rx_vehicle_record_find(rx_addr list, int32_t vehicle_id) {
    const int32_t n = I32(list, 0x2c);
    for (int32_t i = 0; i < n; i++) {
        const rx_addr r = U32(U32(list, 0x28), 4u * (uint32_t)i);
        if (r != 0 && I32(r, 4) == vehicle_id) return r;
    }
    return 0;
}
RX_FUNCTION(0x00542ff0, rx_vehicle_record_find)

/* 004199d0 (ESI = table, EAX = column name; stdcall &key; RET 4) -> the rows of the column's non-unique index with the key (00418e50).
 * Frame: push ecx; push edi. */
rx_addr rx_tbl_rows_by_name(rx_addr table, rx_addr column_name, rx_addr key_slot) {
    ACT_FRAME(L, 0, 2);
    uint32_t c;
    ACT_CALLR(c, 1, rx_tbl_column_index(table, column_name));                   /* 00402160 */
    const rx_addr column = column_at(table, c);
    rx_addr set;
    ACT_CALLR(set, 1, rx_tbl_rows(column, I32(key_slot, 0)));                  /* 00418e50 */
    ACT_END(L);
    return set;
}
RX_FUNCTION(0x004199d0, rx_tbl_rows_by_name)

/* 00410830 (EAX = type, ECX = track; stdcall spec, controller; RET 8) -> 1 when the controller's vehicle may ride the activity's area:
 * the spec's class index of {type, track} (as 004109d0) names a class (0 .. 9, else 0); the controller's Player List row
 * (004e78b0) gives its VehicleId, the game's vehicle list (+0x154b0: 00542ff0) the record, whose spec (+8) gives the vehicle type
 * (+4); 1 when a VehicleClassType row of the class (the default database's table, by VehicleClassId) has that VehicleTypeId.
 * The original keeps the class id in the spec argument's slot and the vehicle type in the controller's (the key of 004199d0 is
 * that slot's address). Frame: sub 0x3c; ebx / ebp / esi / edi: the class table at +0x20, the row-set iterator {+0x10 proxy,
 * +0x14 node}, the end node at +0x1c. */
uint32_t rx_challenge_vehicle_allowed(uint32_t type, uint32_t track, rx_addr spec, int32_t controller) {
    const rx_addr args = rx_sp() + 4;
    ACT_FRAME(L, 0x3c, 4);
    const uint32_t slot = track + 3u * (type + 0x4eu);
    const int32_t index = I32(spec, 4u * slot);
    for (uint32_t k = 0; k < 10; k++) SU32(L, 0x20 + 4 * k, g_challenge_classes[k]);
    uint32_t result = 0;
    if (index >= 0 && index < 10) {
        SU32(args, 0, g_challenge_classes[index]);                              /* the class, in the spec argument's slot */
        rx_addr ref;
        ACT_CALLR(ref, 1, rx_player_table_row(controller));                     /* 004e78b0 */
        const rx_addr store = U32(ref, 8);
        const uint32_t row = U32(ref, 0);
        uint32_t c;
        ACT_CALLR(c, 1, rx_tbl_column_index(store, STR_VEHICLE_ID));            /* 00402160 */
        rx_addr cell;
        ACT_CALLR(cell, 1, rx_tbl_cell_word(store, c, row));                    /* 0041aaf0 */
        const int32_t vehicle_id = I32(cell, 0);
        const rx_addr record = rx_vehicle_record_find(U32(0, 0x00a946b8) + 0x154b0, vehicle_id);   /* 00542ff0 */
        const rx_addr vehicle_spec = U32(record, 8);
        if (vehicle_spec != 0) {
            const uint32_t vehicle_type = U32(vehicle_spec, 4);
            SU32(args, 4, vehicle_type);                                        /* the vehicle type, in the controller's slot */
            rx_addr table;
            ACT_CALLR(table, 1, rx_tbl_find(DEFAULT_DATABASE, STR_VEHICLE_CLASS_TYPE));   /* 00402220 */
            rx_addr set;
            ACT_CALLR(set, 1, rx_tbl_rows_by_name(table, STR_VEHICLE_CLASS_ID, args));    /* 004199d0 (the class slot) */
            /* the iterator {proxy, begin()} and the end node */
            SU32(L, 0x14, U32(U32(set, 0x18), 0));
            SU32(L, 0x10, U32(set, 0));
            for (;;) {
                const rx_addr end = U32(set, 0x18);
                SU32(L, 0x1c, end);
                const rx_addr proxy = U32(L, 0x10);
                const rx_addr node = U32(L, 0x14);
                if (proxy == 0 || proxy != U32(set, 0)) crt_invalid_parameter_noinfo();
                if (node == end) break;                                         /* no row has the vehicle's type */
                if (proxy == 0) crt_invalid_parameter_noinfo();
                else if (node == U32(U32(proxy, 0), 0x18)) crt_invalid_parameter_noinfo();
                const uint32_t class_row = U32(node, 0xc);
                uint32_t tc;
                ACT_CALLR(tc, 1, rx_tbl_column_index(table, STR_VEHICLE_TYPE_ID));   /* 00402160 */
                rx_addr type_cell;
                ACT_CALLR(type_cell, 1, rx_tbl_cell_word(table, tc, class_row));    /* 0041aaf0 */
                if (U32(type_cell, 0) == U32(args, 4)) { result = 1; break; }
                ACT_CALL(0, rx_fx_rowset_next(L + 0x10));                       /* 00417df0 (ESI = the iterator) */
            }
        }
    }
    ACT_END(L);
    return result;
}
RX_FUNCTION(0x00410830, rx_challenge_vehicle_allowed)

/* ======================================================================================================================== */
/* the UI manager's trigger handlers                                                                                        */

/* 005a3b60 MXUIManager_OnTriggerEnter (ECX = the UI manager; stdcall vehicle, trigger; RET 8): the session's EventType (read into a
 * 16-byte buffer, its enum looked up and dropped: 005964b0), then with the vehicle's player (+0x190) in a Free Ride activity's
 * discovery mode (the activity's vf54) a StartActivity trigger with a non-empty activity (+0x108): the challenge type of the name
 * (00596540); when the player's vehicle may ride it (00410830 with the level's spec world + 0x22140 and the player's controller
 * +0x1f0) the area is marked (+0xa0 = 1, the name copied to +0xa4, +0x1a8 = the TrackId +0x104) and EventTypeActivation / TrackId
 * are set; else the class it needs (004109d0, when there is one) goes to ChallengeRequiredVehicleClass.
 * Frame: push ebp; and esp, -8; sub 0x24 (the /GS cookie at +0x20); ebx / esi / edi: the trigger at +0x10, the UI manager at +0x14,
 * the activity name pointer at +0x18, the EventType buffer at +0x1c. */
void rx_ui_trigger_enter(rx_addr ui, rx_addr vehicle, rx_addr trigger) {
    ACT_FRAME_EBP(L, 8, 0x24, 3);
    SU32(L, 0x10, trigger);
    SU32(L, 0x14, ui);
    const rx_addr event_type = L + 0x1c;
    for (uint32_t i = 0; i < 0x10; i++) SU8(event_type, i, 0);
    {
        (void)ui_get_string(U32(0, 0x00a93fcc), STR_EVENT_TYPE, event_type, 0x10);   /* UIProperty_GetString */
        uint32_t kind;
        ACT_CALLR(kind, 1, rx_event_type_of(event_type));                       /* 005964b0: the result is not used */
        (void)kind;
    }
    const rx_addr player = U32(vehicle, 0x190);
    const uint32_t discovery = act_vcall(U32(player, 0x94), 0xd8, 0, 0);       /* the activity's vf54 */
    if (discovery != 0) {
        const rx_addr name = trigger + 0x108;
        SU32(L, 0x18, name);
        if (U8(trigger, 0x108) != 0) {
            int32_t d;
            ACT_CALLR(d, 2, crt_stricmp(trigger + 0xc0, STR_START_ACTIVITY));
            if (d == 0) {
                const rx_addr spec = U32(U32(U32(player, 0x94), 0x234), 0x22140);
                uint32_t type;
                ACT_CALLR(type, 1, rx_challenge_activity_index(name));         /* 00596540 */
                const uint32_t track = U32(trigger, 0x104);
                uint32_t allowed;
                ACT_CALLR(allowed, 2, rx_challenge_vehicle_allowed(type, track, spec, I32(player, 0x1f0)));   /* 00410830 */
                if (allowed != 0) {
                    SU32(player, 0xa0, 1);
                    act_strcpy(player + 0xa4, name);
                    SU32(player, 0x1a8, U32(trigger, 0x104));
                    ui_set_string(ui, STR_EVENT_TYPE_ACTIVATION, name);                       /* UIProperty_SetString */
                    ui_set_int(ui, STR_TRACK_ID, I32(trigger, 0x104));                        /* UIProperty_SetInt */
                } else {
                    int32_t required;
                    ACT_CALLR(required, 1, rx_challenge_required_class(type, track, spec));   /* 004109d0 */
                    if (required >= 0) ui_set_int(ui, STR_REQUIRED_CLASS, required);
                }
            }
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x005a3b60, rx_ui_trigger_enter)

/* 005a3d00 MXUIManager_OnTriggerExit (ECX = the UI manager; stdcall vehicle, trigger; RET 8): leaving a StartActivity trigger (any
 * vehicle, any activity name): the vehicle's player (+0x190) leaves the area: +0xa0 = 0, +0xa4 "FreeRide" (9 bytes), +0x1a8 = 0;
 * EventTypeActivation "FreeRide", TrackId 0, ChallengeRequiredVehicleClass -1. Frame: push ebp; and esp, -8; esi; edi. */
void rx_ui_trigger_exit(rx_addr ui, rx_addr vehicle, rx_addr trigger) {
    ACT_FRAME_EBP(L, 8, 0, 2);
    int32_t d;
    ACT_CALLR(d, 2, crt_stricmp(trigger + 0xc0, STR_START_ACTIVITY));
    if (d == 0) {
        const rx_addr player = U32(vehicle, 0x190);
        SU32(player, 0xa0, 0);
        for (uint32_t i = 0; i < 9; i++) SU8(player + 0xa4, i, U8(STR_FREE_RIDE, i));   /* "FreeRide" and its terminator */
        SU32(player, 0x1a8, 0);
        ui_set_string(ui, STR_EVENT_TYPE_ACTIVATION, STR_FREE_RIDE);
        ui_set_int(ui, STR_TRACK_ID, 0);
        ui_set_int(ui, STR_REQUIRED_CLASS, -1);
    }
    ACT_END(L);
}
RX_FUNCTION(0x005a3d00, rx_ui_trigger_exit)

#ifndef RX_ORACLE
/* 005bc930 (ECX = the event name; stdcall object, member function, this-adjustment; RET 0xc), as MXUIManager::vf42 calls it for the
 * two handlers: the event of the vehicle trigger channel [0x00a89bc8] (00593220), a 0x18-byte handler proxy {+0 vtable 0x0094e8d8,
 * +4 0, +8 the signature's type name (void(RavenVehicle*, const MXTrigger*), 0x9cbb7c), +0xc the object, +0x10 the member, +0x14 the
 * adjustment}, appended to the event's handler list (+8; the event's critical section +0x24: one thread here) when the signatures
 * match. The product has no UI manager: the object is 0 (the handlers use it only for the UI property calls, which the product
 * routes to the owned property store). */
static void subscribe_trigger_handler(rx_addr event_name, rx_addr member) {
    rx_addr event;
    ACT_CALLR(event, 2, rx_world_registry_find(U32(0, 0x00a89bc8), event_name));   /* 00593220 */
    const rx_addr proxy = rx_reflex_aligned_alloc(0x18);
    if (proxy == 0) return;
    SU32(proxy, 4, 0);
    SU32(proxy, 0, 0x0094e8d8u);
    SU32(proxy, 0xc, 0);
    SU32(proxy, 0x10, member);
    SU32(proxy, 0x14, 0);
    SU32(proxy, 8, crt_type_info_name(0x009cbb7cu, 0x009e1a7cu));             /* typeid(signature).name() */
    if (crt_strcmp(U32(event, 4), U32(proxy, 8)) == 0) rx_event_handler_list_push_back(event + 8, proxy);   /* 005b9ff0 */
}

void rx_activation_listen(void) {
    static int listening;
    if (listening) return;
    listening = 1;
    subscribe_trigger_handler(0x00935e84u /* "VehicleEntered" */, 0x005a3b60u);
    subscribe_trigger_handler(0x00935e94u /* "VehicleExited" */, 0x005a3d00u);
}
#endif

/* ======================================================================================================================== */
/* the career Free Ride's discovery collectables                                                                            */

/* 0054d640 (stdcall player, index; RET 8) -> 1 when the player's profile has found the location's collectable `index`: with the
 * player's controller (+0x1f0) and the session's EventLocationId (UI property, -1 when absent), the profile database's (0xae10f0 +
 * 0x1f4 * controller) table "PlayerCollectables"'s row of the location (00419ae0); a location without a row gets one (criteria
 * {EventLocationId, HitDiscoveryCollectables 0, TargetPracticeCollectables 0}: 004ec950); then bit `index` of the row's
 * HitDiscoveryCollectables. Frame: sub 0xe8; ebx / esi / edi: the location id at +0xc, the row reference at +0x10 .. +0x1b, the
 * criteria vector at +0x1c, the added row's reference at +0x34, the criterion at +0x40. */
uint32_t rx_player_discovery_found(rx_addr player, uint32_t index) {
    ACT_FRAME(L, 0xe8, 3);
    const rx_addr location = L + 0xc;
    const rx_addr ref = L + 0x10;
    const rx_addr vec = L + 0x1c;
    const rx_addr added = L + 0x34;
    const rx_addr crit = L + 0x40;
    SU32(location, 0, 0xffffffffu);
    ACT_CALL(2, (void)rx_ui_session_get_int(STR_EVENT_LOCATION_ID, location));   /* UIProperty_GetInt */
    uint32_t result = 0;
    const int32_t controller = I32(player, 0x1f0);
    if (controller >= 0 && I32(location, 0) != -1) {
        rx_addr table;
        ACT_CALLR(table, 1, rx_tbl_find(PROFILE_DATABASE(controller), STR_PLAYER_COLLECTABLES));   /* 00402220 */
        if (table == 0) {
            ACT_END(L);
            return 0;
        }
        rx_addr found;
        ACT_CALLR(found, 1, rx_fx_table_row_by(table, STR_EVENT_LOCATION_ID, location));   /* 00419ae0 */
        for (uint32_t k = 0; k < 0xc; k += 4) SU32(ref, k, U32(found, k));
        if (U32(ref, 8) == 0 || I32(ref, 0) == -1) {
            ACT_CALL(0, (void)rx_list_init(vec));                                /* 005442c0 (ESI = the criteria) */
            static const rx_addr names[3] = {STR_EVENT_LOCATION_ID, STR_HIT_DISCOVERY, STR_TARGET_PRACTICE};
            for (int k = 0; k < 3; k++) {
                if (k > 0) SU32(location, 0, 0);                                 /* the next two criteria are 0 (the same slot) */
                rx_addr c;
                ACT_CALLR(c, 1, rx_fx_crit_init_int(crit, 0, location, names[k]));   /* 00419a70 (ESI = crit, ECX = &value) */
                ACT_CALL(1, rx_tbl_crit_push(vec, c));                           /* 004175e0 (EDI = the criteria) */
                ACT_CALL(0, rx_fx_crit_destroy(crit));                           /* 00415320 */
            }
            rx_addr row;
            ACT_CALLR(row, 2, rx_tbl_add_row(table, added, vec));                /* 004ec950 (EAX = the table; out, &criteria) */
            for (uint32_t k = 0; k < 0xc; k += 4) SU32(ref, k, U32(row, k));
            ACT_CALL(0, rx_fx_crit_vec_destroy(vec));                            /* 00417590 */
        }
        rx_addr cell;
        ACT_CALLR(cell, 1, rx_fx_row_cell(ref, STR_HIT_DISCOVERY));              /* 004199a0 */
        result = ((1u << (index & 31)) & U32(cell, 0)) != 0;
    }
    ACT_END(L);
    return result;
}
RX_FUNCTION(0x0054d640, rx_player_discovery_found)

/* 00585520 (stdcall tracker (activity + 0xff080), activity; RET 8): the discovery collectables of the area: the tuning's
 * "MXGame" / "DiscoveryCollectibleCollisionRadius" (the registry [0x00a95844] +0xc: vf1 finds the record, vf17 reads the float into
 * 0x9ca32c), squared in place; the first local human player (the game's player slots +0x153ec, count +0x1541c; 0 when the first
 * slot is -1 or none is human); the tracker cleared (005857f0); the area's jump collectables (type 0x106d, up to 0x20) into the
 * tracker (+0 the count, +4 the list), +0x84 = 0; the scene's "DISC_" markers (up to 0x14, 007d29e0); each collectable the
 * player's profile has found (0054d640 (player, i)) is marked (+0x140 = 1), hidden (+0xa2 = 0, +0x70 = 0) and counted (+0x84),
 * and every DISC_ marker within 20 units of it (squared distance < 400, 0x977b50) is hidden; the others get +0x140 = 0.
 * Frame: push ebp; and esp, -16; sub 0x74; ebx / esi / edi: the player at +0x10, the marker count at +0x14, the index at +0x18,
 * the overflow flag then the list cursor at +0x1c, the found collectable's position at +0x20, the marker list at +0x30. */
void rx_free_ride_discoveries_setup(rx_addr tracker, rx_addr A) {
    ACT_FRAME_EBP(L, 16, 0x74, 3);
    {
        const rx_addr reg = U32(U32(0, 0x00a95844), 0xc);
        const uint32_t find[3] = {STR_MXGAME, STR_DISCOVERY_RADIUS, 1};
        const rx_addr record = act_vcall(reg, 4, 3, find);
        if (record != 0) {
            const uint32_t read[3] = {record, DISCOVERY_RADIUS_GLOBAL, 1};
            (void)act_vcall(reg, 0x44, 3, read);
            const float r = F32(0, DISCOVERY_RADIUS_GLOBAL);
            SF32(0, DISCOVERY_RADIUS_GLOBAL, rx_muls(r, r));
        }
    }
    /* the first local human player */
    const rx_addr base = U32(0, 0x00a946b8) + 0x7460;
    rx_addr player = 0;
    SU32(L, 0x10, 0);
    if (I32(base, 0xdf8c) != -1) {
        const uint32_t n = U8(base, 0xdfbc);
        for (uint32_t i = 0; i < n; i++) {
            if (U32(rx_seat_active(base, i), 0x10) == 0) {
                player = rx_seat_active(base, i);
                break;
            }
        }
    }
    SU32(L, 0x10, player);
    rx_addr world;
    ACT_CALLR(world, 0, rx_game_current_world());                                /* 0075c530 */
    const rx_addr scene = U32(world, 0x20138);
    rx_free_ride_jump_tracker_init(tracker);                                     /* 005857f0 (EAX = the tracker) */
    uint32_t count;
    ACT_CALLR(count, 6, rx_scene_group_query(scene, 0x106d, tracker + 4, 0x20, A + 0x118, 0));   /* 007d2820 */
    SU32(tracker, 0, count);
    SU32(tracker, 0x84, 0);
    const rx_addr markers = L + 0x30;
    SU32(markers, 0, 0);
    crt_memset(L + 0x34, 0, 0x4c);
    SU32(L, 0x1c, 0);
    uint32_t marker_count;
    ACT_CALLR(marker_count, 4, rx_scene_collect_prefixed(U32(U32(A, 0x234), 0x20138), STR_DISC_PREFIX, markers, 0x14, L + 0x1c));   /* 007d29e0 */
    SU32(L, 0x14, marker_count);
    SU32(L, 0x18, 0);
    if (I32(tracker, 0) > 0) {
        SU32(L, 0x1c, tracker + 4);
        for (int32_t i = 0; i < I32(tracker, 0); i++) {
            SU32(L, 0x18, (uint32_t)i);
            const rx_addr e = U32(tracker + 4, 4u * (uint32_t)i);
            uint32_t found;
            ACT_CALLR(found, 2, rx_player_discovery_found(player, (uint32_t)i));   /* 0054d640 */
            if (found == 0) {
                SU32(e, 0x140, 0);
            } else {
                SU32(e, 0x140, 1);
                SU8(e, 0xa2, 0);
                SU32(e, 0x70, 0);
                SU32(tracker, 0x84, U32(tracker, 0x84) + 1);
                const rx_addr m = act_vcall(e, 0x38, 0, 0);                     /* vf14: the world matrix */
                const float px = F32(m, 0x30), py = F32(m, 0x34), pz = F32(m, 0x38);
                SF32(L, 0x20, px);
                SF32(L, 0x24, py);
                SF32(L, 0x28, pz);
                for (uint32_t j = 0; j < marker_count; j++) {
                    const rx_addr d = U32(markers, 4 * j);
                    const rx_addr dm = act_vcall(d, 0x38, 0, 0);
                    const float dy = F32(dm, 0x34) - py;
                    const float dz = F32(dm, 0x38) - pz;
                    const float dx = F32(dm, 0x30) - px;
                    float sum = dy * dy;
                    sum = sum + dz * dz;
                    sum = sum + dx * dx;
                    if (400.0f /* 0x977b50 */ > sum) {
                        SU8(d, 0xa2, 0);
                        SU32(d, 0x70, 0);
                    }
                }
            }
            SU32(L, 0x1c, U32(L, 0x1c) + 4);
        }
        SU32(L, 0x18, (uint32_t)I32(tracker, 0));
    }
    ACT_END(L);
}
RX_FUNCTION(0x00585520, rx_free_ride_discoveries_setup)
