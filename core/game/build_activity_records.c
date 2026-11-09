/* build_activity_records.c -- construction unit 11: the first local human joining an activity (core/notes/construction.md
 * section 16): Activity::vf50 0055a4c0 ("First Human Joined Activity" on the activity manager channel, the scene entities' start
 * states 00557fe0, then the class's vf52 -- MXActivityRace::vf52 00565640 is the loop unit's), and the human's records row
 * MXRecords_Join 00764170 (the profile's PlayerRecords: the event's working row, found / copied / initialised).
 * Only what the race reads or writes of the records is built: the working rows and their record columns. */
#include "phys_studio.h"
#include "map_scale.h"
#include "build_activity_int.h"
#include "world_veh.h"
#include "world.h"
#include "loop_player.h"
#include "anim.h"
#include "world_dropgate.h"
#include "alive_gate.h"
#include <string.h>

/* ======================================================================================================================== */
/* the scene entities' start states (00557fe0)                                                                              */

/* FUN_00559da0 (ECX = activity): the activity has a human driver (a driver's player +0x10 == 0) */
static uint32_t activity_has_human(rx_addr A) {
    for (int32_t i = 0; i < I32(A, 0x26c); i++)
        if (U32(U32(U32(U32(A, 0x268), 4u * (uint32_t)i), 0x74), 0x10) == 0) return 1;
    return 0;
}
static rx_addr body_of(rx_addr handle) { rx_addr b; ACT_CALLR(b, 0, rx_world_physics_handle_body(handle)); return b; }   /* 00418f30 (EAX) */
static void copy16(rx_addr d, rx_addr s) { for (uint32_t k = 0; k < 16; k += 4) SU32(d, k, U32(s, k)); }

/* the two bale classes' start (006c0f80 PopUpBale / 006c0760 PopDownBale; ECX = activity, ESI = entity): with a human in the
 * activity (or the AI-only registry override [0x00ae1b90]) the bale is up (+0x1c0) or down (+0x1d0) by the mode, its body's two
 * positions (+0x40 / +0x60) set and the kinematic deactivated (down) or flagged (+0xb0 1, up); +0x60 the entity's position.
 * up_modes: the modes that start the bale in its first (+0x1c0) pose */
static void bale_start(rx_addr A, rx_addr e, int popdown) {
    ACT_FRAME_EBP(L, 16, 0x1c, 1);
    if (activity_has_human(A) || U32(0, 0x00ae1b90) != 0) {
        const uint32_t m = U32(A, 0x238);
        int alt;   /* the "+0x1d0 first" branch of 006c0760 / the "+0x1c0 first" of 006c0f80 */
        if (popdown) alt = U32(A, 0x23c) == 8 || m == 2 || m == 3 || m == 4;
        else alt = U32(A, 0x23c) == 8 || !(m == 2 || m == 3);
        const rx_addr down = popdown ? e + 0x1d0 : e + 0x1c0, up = popdown ? e + 0x1c0 : e + 0x1d0;
        rx_addr pose;
        if (alt) {
            if (U32(e, 0x158)) copy16(body_of(U32(e, 0x158)) + 0x40, down);
            if (U32(e, 0x158)) copy16(body_of(U32(e, 0x158)) + 0x60, down);
            if (U32(e, 0x158)) {
                const rx_addr w = U32(U32(e, 0x158), 4);
                const rx_addr b = body_of(U32(e, 0x158));
                ACT_CALL(1, rx_world_kinematic_deactivate(b, w));            /* 0076a8b0 (EDX = body; world) */
                SU8(body_of(U32(e, 0x158)), 0xb0, 0);
            }
            pose = down;
            if (popdown) { SU32(e, 0x178, 1); SU32(e, 0x174, 1); }
            else { SU32(e, 0x174, 1); SU32(e, 0x170, 0); }
            SU8(e, 0xa2, 0);
        } else {
            if (U32(e, 0x158)) copy16(body_of(U32(e, 0x158)) + 0x40, up);
            if (U32(e, 0x158)) copy16(body_of(U32(e, 0x158)) + 0x60, up);
            if (U32(e, 0x158)) SU8(body_of(U32(e, 0x158)), 0xb0, 1);
            pose = up;
            if (popdown) { SU32(e, 0x178, 1); SU32(e, 0x174, 0); }
            else { SU32(e, 0x174, 1); SU32(e, 0x170, 1); }
            SU8(e, 0xa2, 1);
        }
        if (popdown) SU32(e, 0x170, A);
        copy16(L + 0x10, pose);
        SU32(e, 0x60, U32(L, 0x10)); SU32(e, 0x64, U32(L, 0x14)); SU32(e, 0x68, U32(L, 0x18));
    }
    ACT_END(L);
}

/* FUN_006baee0 (ESI = a lap marker (type 0x1052); stdcall activity; RET 4): a marker with a successor name (+0x384) and none
 * linked yet (+0x39c) links the next marker of the same name (the +0x370 name objects' vf2, _stricmp) farther along (+0x360):
 * +0x39c it, +0x388 the distance; none: both cleared */
static void lap_marker_link(rx_addr m, rx_addr A) {
    ACT_FRAME(L, 0x408, 1);
    if (U32(m, 0x384) != 0 && U32(m, 0x39c) == 0) {
        const rx_addr arr = L + 0x10, ov = L + 0xc;
        for (uint32_t i = 0; i < 0x3f8; i++) SU8(arr + 4, i, 0);
        SU32(arr, 0, 0);
        int32_t n;
        {
            const rx_addr sp = rx_sp(); rx_set_sp(sp - 0x14);
            n = (int32_t)rx_scene_manager_collect_typed(U32(U32(A, 0x234), 0x20138), 0x1052, arr, 0xff, ov);
            rx_set_sp(sp);
        }
        float best = F32(m, 0x360);
        rx_addr pick = 0;
        if (n > 0) {
            ACT_FRAME(R, 0, 2);                                          /* push ebx / edi (006baf5e) around the scan */
            for (int32_t i = 0; i < n; i++) {
                const rx_addr o = U32(arr, 4u * (uint32_t)i);
                if (o == m) continue;
                const rx_addr on = act_vcall(U32(o, 0x370), 8, 0, 0);
                const rx_addr mn = act_vcall(U32(m, 0x370), 8, 0, 0);
                if (crt_stricmp(mn, on) == 0 && F32(o, 0x360) > best) { best = F32(o, 0x360); pick = o; }
            }
            ACT_END(R);
        }
        if (pick) { SU32(m, 0x39c, pick); SF32(m, 0x388, best - F32(m, 0x360)); ACT_END(L); return; }
        SU32(m, 0x384, 0); SU32(m, 0x39c, 0); SF32(m, 0x388, 0.0f);
    }
    ACT_END(L);
}

/* the scene's entities of a type (at most 255) into the frame array L + 0x18 (007d2920; overflow flag L + 0xc) */
static int32_t collect(rx_addr L, rx_addr scene, uint32_t type, int clear) {
    if (clear) for (uint32_t i = 0; i < 0x3fc; i++) SU8(L + 0x18, i, 0);
    const rx_addr sp = rx_sp(); rx_set_sp(sp - 0x14);
    const int32_t n = (int32_t)rx_scene_manager_collect_typed(scene, type, L + 0x18, 0xff, L + 0xc);
    rx_set_sp(sp);
    return n;
}
#define ENT(i) U32(L + 0x18, 4u * (uint32_t)(i))

/* FUN_00557fe0 (stdcall activity; RET 4): the start states of the scene's activity entities (types 0x1028 / 0x1033 gates / 0x1031
 * and 0x1032 bales / 0x1034 / 0x1053 / 0x1052 lap markers / 0x1050 / 0x1062 / 0x106b) */
void rx_activity_entities_start(rx_addr A) {
    ACT_FRAME_EBP(L, 8, 0x40c, 3);
    SU32(L, 0x18, 0);
    for (uint32_t i = 4; i < 0x3fc; i++) SU8(L + 0x18, i, 0);
    const rx_addr scene = U32(U32(A, 0x234), 0x20138);
    SU32(L, 0x14, scene);
    int32_t n = collect(L, scene, 0x1028, 0);
    for (int32_t i = 0; i < n; i++) { SU32(ENT(i), 0x94, 0); SU32(ENT(i), 0xa8, 0); }
    n = collect(L, scene, 0x1033, 1);
    /* 006bdbe0 (ECX = gate, EAX = activity): the gate's start state. The owner's port (world_dropgate.c) has both branches: closed
     * for a gated race, dropped for free ride / mode 0 / mode 6 (the Overtake career races) / a spectated session. */
    for (int32_t i = 0; i < n; i++) ACT_CALL(0, rx_drop_gate_reset(ENT(i), A));
    rx_alive_gate_start(A);   /* PORT ADDITION (alive_gate.h): an Alive race's gate (a ScenarioAnimEntity) takes the same start state */
    n = collect(L, scene, 0x1031, 1);
    for (int32_t i = 0; i < n; i++) ACT_CALL(0, bale_start(A, ENT(i), 0));         /* 006c0f80 */
    n = collect(L, scene, 0x1032, 1);
    for (int32_t i = 0; i < n; i++) ACT_CALL(0, bale_start(A, ENT(i), 1));         /* 006c0760 */
    n = collect(L, scene, 0x1034, 1);
    if (n > 0) {
        SU32(0, 0x00a942f0, 0);
        for (int32_t i = 0; i < n; i++) { SU32(ENT(i), 0x1a4, 1); SU32(ENT(i), 0x1a8, 0); }
    }
    n = collect(L, scene, 0x1053, 1);
    if (n > 0) act_unreached(0x005581e5, "type 0x1053 entities' start (006bc170)");
    n = collect(L, scene, 0x1052, 1);
    for (int32_t i = 0; i < n; i++) {
        const rx_addr e = ENT(i);
        SU32(e, 0x364, U32(e, 0x360)); SU32(e, 0x398, 0); SU32(e, 0x390, 0);
    }
    for (int32_t i = 0; i < n; i++) ACT_CALL(1, lap_marker_link(ENT(i), A));       /* 006baee0 (ESI = marker; activity) */
    n = collect(L, scene, 0x1050, 1);
    for (int32_t i = 0; i < n; i++) if (U32(ENT(i), 0x350) == 0) SU32(ENT(i), 0x358, 0);
    n = collect(L, scene, 0x1062, 1);
    if (n > 0) act_unreached(0x00558404, "type 0x1062 entities' start (00555e00)");
    n = collect(L, scene, 0x106b, 1);
    for (int32_t i = 0; i < n; i++) { SU32(ENT(i), 0x140, 0); SU32(ENT(i), 0x174, 0); SU32(ENT(i), 0x170, 0); }
    ACT_END(L);
}
RX_FUNCTION(0x00557fe0, rx_activity_entities_start)

/* Activity::vf50 0055a4c0 (ECX = activity): "First Human Joined Activity" (void(Activity*)) on the activity manager channel
 * [0x00a93f74] (the payload in the frame), the entities' start, then a tail call of vf52 (+0xd0) */
void rx_activity_vf50(rx_addr A) {
    ACT_FRAME(L, 4, 3);
    SU32(L, 0xc, A);
    act_raise(U32(0, 0x00a93f74), 0x00941df4 /* "First Human Joined Activity" */, 0x009cbd24, L + 0xc, 4);
    ACT_CALL(1, rx_activity_entities_start(A));
    ACT_END(L);
    {   /* jmp [vtable + 0xd0] with this function's return address (a core vf52 enters there; the original is called) */
        const rx_addr fn = U32(U32(A, 0), 0xd0);
        void (*f)(rx_addr) = RX_FN(fn, void (*)(rx_addr));
        if (f) { const rx_addr sp = rx_sp(); rx_set_sp(sp); f(A); rx_set_sp(sp); }
        else rx_call_original(fn, RX_THISCALL, A, 0, 0);
    }
}
RX_FUNCTION(0x0055a4c0, rx_activity_vf50)

/* ======================================================================================================================== */
/* MXRecords_Join 00764170                                                                                                  */

/* the record columns (MXRecords_InitRow 00763a50 / MXRecords_CopyRow 00763600): name, float, the initial value */
typedef struct { uint32_t name; int is_float; uint32_t init; } rec_col;
static const rec_col k_init_cols[] = {
    {0x00957c1c, 1, 0}, {0x0096d990, 0, 0}, {0x0096d99c, 0, 0}, {0x00957c04, 0, 0}, {0x0096d9a8, 0, 0}, {0x0096d9b8, 0, 0},
    {0x0096d9cc, 0, 0}, {0x0096d9e0, 0, 0}, {0x0096d9f0, 0, 0}, {0x0096da08, 0, 0}, {0x0096da1c, 0, 0}, {0x0096da40, 0, 0},
    {0x0096da6c, 0, 0}, {0x00957c28, 1, 0x7f7fffffu}, {0x0096da98, 0, 10000000}, {0x009626b0, 0, 0}, {0x0096daa4, 0, 0},
    {0x0096dab0, 0, 0}, {0x0096dabc, 0, 0}, {0x0096dacc, 0, 0}, {0x0096dadc, 0, 0}};

/* MXRecords_InitRow 00763a50 (EAX = &row ref {row, , table}): the record columns to their initial values (the table's revision
 * +0x68 counted for every changed cell: floats compared with ucomiss, NaN = changed) */
static void records_init_row(rx_addr ref) {
    ACT_FRAME(L, 0xc, 4);
    for (size_t k = 0; k < sizeof k_init_cols / sizeof k_init_cols[0]; k++) {
        const rx_addr t = U32(ref, 8);
        const uint32_t row = U32(ref, 0);
        SU32(L, 0x14, k_init_cols[k].init);
        const uint32_t c = rx_tbl_column_index(t, k_init_cols[k].name);
        const rx_addr cell = rx_tbl_cell_word(t, c, row);
        const int changed = k_init_cols[k].is_float ? !(F32(cell, 0) == rx_bf(k_init_cols[k].init)) : U32(cell, 0) != k_init_cols[k].init;
        if (changed) SU32(t, 0x68, U32(t, 0x68) + 1);
        if (c >= (U32(t, 0x14) - U32(t, 0x10)) >> 2) crt_invalid_parameter_noinfo();
        if (k_init_cols[k].is_float) ACT_CALL(3, rx_tbl_set_float(U32(U32(t, 0x10), 4 * c), row, L + 0x14));
        else ACT_CALL(3, rx_tbl_set_int(U32(U32(t, 0x10), 4 * c), row, L + 0x14));
    }
    ACT_END(L);
}
/* MXRecords_CopyRow 00763600 (stdcall &destination ref, &source ref; RET 8): every record column copied (005063c0 floats,
 * Table_SetCellInt ints) */
static const rec_col k_copy_cols[] = {
    {0x00957c1c, 1, 0}, {0x0096d990, 0, 0}, {0x0096d99c, 0, 0}, {0x00957c04, 0, 0}, {0x0096d9a8, 0, 0}, {0x0096d9b8, 0, 0},
    {0x0096d9cc, 0, 0}, {0x0096d9e0, 0, 0}, {0x0096d9f0, 0, 0}, {0x0096da08, 0, 0}, {0x0096da1c, 0, 0}, {0x0096da2c, 0, 0},
    {0x0096da40, 0, 0}, {0x0096da54, 0, 0}, {0x0096da6c, 0, 0}, {0x0096da80, 0, 0}, {0x00957c28, 1, 0}, {0x0096da98, 0, 0},
    {0x009626b0, 0, 0}, {0x0096daa4, 0, 0}, {0x0096dab0, 0, 0}, {0x0096dabc, 0, 0}, {0x0096dacc, 0, 0}, {0x0096dadc, 0, 0}};
static void records_copy_row(rx_addr dst, rx_addr src) {
    ACT_FRAME(L, 4, 4);
    for (size_t k = 0; k < sizeof k_copy_cols / sizeof k_copy_cols[0]; k++) {
        const rx_addr st = U32(src, 8);
        const uint32_t c = rx_tbl_column_index(st, k_copy_cols[k].name);
        const rx_addr cell = rx_tbl_cell_word(st, c, U32(src, 0));
        if (k_copy_cols[k].is_float) ACT_CALL(2, rx_tbl_set_cell_float(U32(dst, 8), k_copy_cols[k].name, U32(dst, 0), cell));
        else ACT_CALL(2, rx_tbl_set_cell_int(U32(dst, 8), k_copy_cols[k].name, U32(dst, 0), cell));
    }
    ACT_END(L);
}

/* FUN_00541040 (ECX = the vehicle list (MXGame + 0x154b0); stdcall vehicle id; RET 4): the vehicle's model id (the made spec's
 * model record +8 -> +0), else from the Vehicle List row's owner tables (not in the reference events) */
/* Canonical full model resolver is shared with career before vehicle creation. */
uint32_t rx_career_vehicle_model_id(rx_addr list,uint32_t vehicle_id);
static uint32_t vehicle_model_id(rx_addr list,uint32_t vid) {
    return rx_career_vehicle_model_id(list,vid);
}

/* the criteria helpers (00419a70 / 004f6c60 / 004175e0 / 00415320 on the frame's criterion) */
static void crit_int(rx_addr vec, rx_addr crit, rx_addr name, uint32_t value) {
    rx_tbl_crit_init_int(crit, name, 0, value);
    rx_tbl_crit_push(vec, crit);
    rx_tbl_crit_destroy(crit);
}
static void crit_str(rx_addr vec, rx_addr crit, rx_addr name, rx_addr value) {
    ACT_CALL(2, rx_tbl_crit_init_string(crit, name, 0, value));
    rx_tbl_crit_push(vec, crit);
    rx_tbl_crit_destroy(crit);
}

/* FUN_004e7780 (stdcall activity type; RET 4): the EventType table's EventTypeId of the type's activity name (0x009ccd40[type]),
 * or -1 */
uint32_t rx_event_type_id_of(uint32_t type);
static uint32_t event_type_id_of(uint32_t type) { return rx_event_type_id_of(type); }
uint32_t rx_event_type_id_of(uint32_t type) {
    ACT_FRAME_EBP(L, 8, 0xdc, 3);
    const rx_addr t = rx_tbl_find(0x00ae0910, 0x00935e08 /* "EventType" */);
    const rx_addr vec = L + 0x20, crit = L + 0x38, out = L + 0x14;
    rx_tbl_crit_vec_init(vec);
    crit_str(vec, crit, 0x0093e52c /* "ActivityName" */, U32(0, 0x009ccd40 + 4 * type));
    SU32(out, 0, 0xffffffffu); SU32(out, 8, 0);
    ACT_CALL(3, rx_tbl_find_first(t, vec, out));
    uint32_t r = 0xffffffffu;
    if (U32(out, 8) != 0 && U32(out, 0) != 0xffffffffu)
        r = U32(rx_tbl_cell_word(U32(out, 8), rx_tbl_column_index(U32(out, 8), 0x00936e90 /* "EventTypeId" */), U32(out, 0)), 0);
    rx_tbl_crit_vec_destroy(vec);
    ACT_END(L);
    return r;
}
RX_FUNCTION(0x004e7780, rx_event_type_id_of)

/* MXRecords_Join 00764170 (EAX = the player's controller slot; stdcall records, vehicle id, activity type, area, level name; RET
 * 0x14): the slot's record set (records + 0x18 * slot: +0 the profile's PlayerRecords (database 0x00ae10f0 + 0x1f4 * slot), +4 the
 * event's working row, +8 the stored free-ride row, +0xc EventTypeLocationId, +0x10 VehicleTypeId): the old working rows deleted,
 * the event's EventTypeLocations row (EventTypeId, EventLocationId.Filename, TrackId), a new working row, its record columns from
 * the stored row of the location and vehicle type or initialised; a free ride (type 8) also finds its stored row */
void rx_records_join(uint32_t slot, rx_addr rec, uint32_t vid, uint32_t type, uint32_t area, rx_addr level) {
    /* MAP SCALE (port, map_scale.h): a scaled track keeps no offline records: without a working row (+4 stays -1) every MXRecords
     * update, the leave's commit and its highlight scan do nothing (each of them tests the row) */
    if (rx_ms_records_off()) return;   /* (BASE SCALE: a track at its own default scale keeps them) */
    if (rx_ms_custom_track()) return;   /* MODS/TRACKS (port, map_scale.h): nor does a custom track */
    if (rx_ps_records_off()) return;   /* PHYSICS STUDIO (port, phys_studio.h): changed physics keeps no offline records either */
    const rx_addr entry = rx_sp();
    ACT_FRAME(L, 0x100, 4);
    if (slot == 0xffffffffu) { ACT_END(L); return; }
    const rx_addr S = rec + 0x18 * slot;
    SU32(S, 0, rx_tbl_find(0x00ae10f0 + 0x1f4 * slot, 0x0096dae8 /* "PlayerRecords" */));
    if (U32(rec, 0x78) == 0) {
        SU32(rec, 0x78, rx_tbl_find(0x00ae0910, 0x00936e7c /* "EventTypeLocations" */));
        SU32(rec, 0x7c, rx_tbl_find(0x00ae0910, 0x0093e7c4 /* "EventLocation" */));
        SU32(rec, 0x80, rx_tbl_find(0x00ae0910, 0x0093e180 /* "VehicleModel" */));
    }
    uint32_t mid;
    ACT_CALLR(mid, 1, vehicle_model_id(U32(0, 0x00a946b8) + 0x154b0, vid));
    SU32(L, 0x14, mid);
    {   /* the model's VehicleTypeId (the VehicleModel table's VehicleModelId index) */
        const rx_addr vm = U32(rec, 0x80);
        const uint32_t c = rx_tbl_column_index(vm, 0x0093e170 /* "VehicleModelId" */);
        if (c >= (U32(vm, 0x14) - U32(vm, 0x10)) >> 2) crt_invalid_parameter_noinfo();
        rx_addr ref;
        ACT_CALLR(ref, 1, rx_tbl_row(U32(U32(vm, 0x10), 4 * c), I32(L, 0x14)));
        const rx_addr t = U32(ref, 8);
        const uint32_t row = U32(ref, 0);
        SU32(S, 0x10, U32(rx_tbl_cell_word(t, rx_tbl_column_index(t, 0x00936ce4 /* "VehicleTypeId" */), row), 0));
    }
    const rx_addr v1 = L + 0x24, v2 = L + 0x48, crit = L + 0x60, out = L + 0x18, row2 = L + 0x3c;
    rx_tbl_crit_vec_init(v1);
    crit_int(v1, crit, 0x0093e228 /* "EventTypeLocationId" */, U32(0, 0x0092ec64));
    SU32(L, 0x14, 0xffffffffu);
    crit_int(v1, crit, 0x00936ce4 /* "VehicleTypeId" */, 0xffffffffu);
    ACT_CALL(2, rx_tbl_delete_rows(U32(S, 0), v1));                     /* the old working rows */
    uint32_t etid;
    ACT_CALLR(etid, 1, event_type_id_of(type));
    SU32(L, 0x14, etid);
    rx_tbl_crit_vec_clear(v1);                                          /* 004f1790 */
    crit_int(v1, crit, 0x00936e90 /* "EventTypeId" */, etid);
    crit_str(v1, crit, 0x0096daf8 /* "EventLocationId.Filename" */, level);
    crit_int(v1, crit, 0x00936e40 /* "TrackId" */, area);
    SU32(out, 0, 0xffffffffu); SU32(out, 8, 0);
    ACT_CALL(3, rx_tbl_find_first(U32(rec, 0x78), v1, out));
    if (U32(out, 8) != 0 && U32(out, 0) != 0xffffffffu) {
        const rx_addr t = U32(out, 8);
        SU32(S, 0xc, U32(rx_tbl_cell_word(t, rx_tbl_column_index(t, 0x0093e228), U32(out, 0)), 0));
        ACT_CALL(0, rx_list_init(v2));                                  /* 005442c0 (ESI) */
        crit_int(v2, crit, 0x0093e228 /* "EventTypeLocationId" */, U32(0, 0x0092ec64));
        crit_int(v2, crit, 0x00936ce4 /* "VehicleTypeId" */, U32(S, 0x10));
        ACT_CALL(2, rx_tbl_add_row(U32(S, 0), row2, v2));
        SU32(S, 4, U32(row2, 0));
        rx_tbl_crit_vec_clear(v1);
        crit_int(v1, crit, 0x0093e228, U32(S, 0xc));
        crit_int(v1, crit, 0x00936ce4, U32(S, 0x10));
        SU32(out, 0, 0xffffffffu); SU32(out, 4, 0); SU32(out, 8, 0);
        ACT_CALL(3, rx_tbl_find_first(U32(S, 0), v1, out));
        if (U32(out, 8) != 0 && U32(out, 0) != 0xffffffffu) ACT_CALL(2, records_copy_row(row2, out));   /* MXRecords_CopyRow */
        else ACT_CALL(0, records_init_row(row2));                                                        /* MXRecords_InitRow */
        if (type == 8) {
            rx_tbl_crit_vec_clear(v1);
            crit_int(v1, crit, 0x0093e228, U32(S, 0xc));
            SU32(L, 0x14, 0xffffffffu);
            crit_int(v1, crit, 0x00936ce4, 0xffffffffu);
            SU32(out, 0, 0xffffffffu); SU32(out, 8, 0);
            ACT_CALL(3, rx_tbl_find_first(U32(S, 0), v1, out));
            uint32_t r = 0xffffffffu;
            if (U32(out, 8) != 0 && U32(out, 0) != 0xffffffffu) r = U32(out, 0);
            SU32(S, 8, r);
        }
        rx_tbl_crit_vec_destroy(v2);
    }
    rx_tbl_crit_vec_destroy(v1);
    (void)entry;
    ACT_END(L);
}
RX_FUNCTION(0x00764170, rx_records_join)

/* MXRecords_Reset 00764710 (ECX = the records [0x00a946c0], EAX = the player's controller slot; Activity::vf18, the restart): the slot's working row set again from the
 * stored row of its EventTypeLocationId / VehicleTypeId (MXRecords_CopyRow) or initialised (MXRecords_InitRow); its holeshot (+0x14) = 0. Frame: sub esp, 0xe4;
 * push ebx / ebp / esi / edi; the row reference {row, 0, table} at +0x1c, the criteria list at +0x28, the found reference at +0x10 */
void rx_records_reset(rx_addr rec, uint32_t slot) {
    ACT_FRAME(L, 0xe4, 4);
    if (slot == 0xffffffffu) { ACT_END(L); return; }
    const rx_addr S = rec + 0x18 * slot;
    if (U32(S, 4) == 0xffffffffu) { ACT_END(L); return; }
    const rx_addr ref = L + 0x1c, vec = L + 0x28, crit = L + 0x40, out = L + 0x10;
    SU32(ref, 0, U32(S, 4)); SU32(ref, 4, 0); SU32(ref, 8, U32(S, 0));
    ACT_CALL(0, rx_list_init(vec));                                       /* 005442c0 (ESI) */
    crit_int(vec, crit, 0x0093e228 /* "EventTypeLocationId" */, U32(S, 0xc));
    crit_int(vec, crit, 0x00936ce4 /* "VehicleTypeId" */, U32(S, 0x10));
    SU32(out, 0, 0xffffffffu); SU32(out, 4, 0); SU32(out, 8, 0);
    ACT_CALL(3, rx_tbl_find_first(U32(S, 0), vec, out));
    if (U32(out, 8) != 0 && U32(out, 0) != 0xffffffffu) ACT_CALL(2, records_copy_row(ref, out));
    else ACT_CALL(0, records_init_row(ref));
    SU32(S, 0x14, 0);
    rx_tbl_crit_vec_destroy(vec);
    ACT_END(L);
}
RX_FUNCTION(0x00764710, rx_records_reset)
