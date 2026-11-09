/* build_activity_event.c -- construction unit 8: MXUIManager::SetupEvent 0059a970 as spec-driven C (build_activity.h): the event's
 * properties, the activity row (005a33b0, 004df0f0, ActivityTable_AddRow 004dedc0), the players pruned and the AI riders added:
 * MXUIManager_SetupAIRiders 0059f2d0, MXUIManager_BuildAIRoster 0059f3e0 (the AI riders drawn with the game's generator 0x009cb860
 * per difficulty bucket), MXUIManager_AddAIRiderOfClass 0059ebc0, MXUIManager_AddAIPlayer 0059e960. Transliterated from the
 * disassembly; frames at the original's stack addresses. */
#include "build_activity_util.h"
#include "career_properties.h"
#include "race_misc.h"
#include "build_activity_int.h"
#include <string.h>

/* ======================================================================================================================== */
/* UI properties (the UI manager's property store, ui + 0x134): oracle the original accessors, product the event spec          */

/* UIProperty_GetInt 005dc0c0 (EAX = key; ui, &value; RET 8) -> found */
static int act_prop_int(rx_addr ui, rx_addr key, rx_addr out) {
#ifdef RX_ORACLE
    rx_regs in, o; memset(&in, 0, sizeof in); in.r[0] = key;
    const uint32_t a[2] = {ui, out};
    rx_call_original_ex(0x005dc0c0, &in, &o, 2, a);
    return o.r[0] != 0;
#else
    (void)ui;
    return (int)rx_owned_property_get_int(key,out,0);
#endif
}
/* UIProperty_GetString 005dc060 (EAX = key, EDI = buffer, ESI = size; ui; RET 4) -> found */
static int act_prop_str(rx_addr ui, rx_addr key, rx_addr buf, uint32_t size) {
#ifdef RX_ORACLE
    rx_regs in, o; memset(&in, 0, sizeof in); in.r[0] = key; in.r[7] = buf; in.r[6] = size;
    const uint32_t a[1] = {ui};
    rx_call_original_ex(0x005dc060, &in, &o, 1, a);
    return o.r[0] != 0;
#else
    (void)ui;
    return (int)rx_owned_property_get_string(key,buf,size);
#endif
}
/* UIProperty_SetInt 005dbfb0 (ECX = ui; key, value; RET 8): the UI's store (its property callbacks run in the oracle); the product
 * keeps the values gameplay reads back (EventRiders, AIDifficulty) in the spec */
static void act_prop_set_int(rx_addr ui, rx_addr key, int32_t v) {
#ifdef RX_ORACLE
    const uint32_t a[2] = {key, (uint32_t)v};
    rx_call_original(0x005dbfb0, RX_THISCALL, ui, 2, a);
#else
    (void)ui;
    rx_owned_property_set_int(key,(uint32_t)v);
#endif
}

/* the enum parsers: the index of the name (_stricmp) in a table of C strings, else the count */
static uint32_t act_enum(rx_addr names, uint32_t n, rx_addr s) {
    for (uint32_t i = 0; i < n; i++) if (crt_stricmp(s, U32(names, 4 * i)) == 0) return i;
    return n;
}
uint32_t rx_event_type_of(rx_addr s) { return act_enum(0x009d2220, 0x3e, s); }       /* 005964b0 (cdecl) */
uint32_t rx_duration_type_of(rx_addr s) { return act_enum(0x009d2368, 3, s); }       /* 00596610 */
uint32_t rx_activity_mode_of(rx_addr s) { return act_enum(0x009d2344, 9, s); }       /* 00596580 */
RX_FUNCTION(0x005964b0, rx_event_type_of)
RX_FUNCTION(0x00596610, rx_duration_type_of)
RX_FUNCTION(0x00596580, rx_activity_mode_of)
/* MXGame_PlayerMode 0075ec80 is the race unit's (race_misc_game.c rx_game_player_mode) */
/* FUN_005560f0 (EAX = event type - 6): the default event length kind: 1 for event types 6, 0x21, 0x30, 0x31, else 3 (the byte table
 * at 0x00556114 in .text, copied here) */
static uint32_t default_duration(uint32_t t) { return (t == 6 || t == 0x21 || t == 0x30 || t == 0x31) ? 1 : 3; }

/* the game's generator (0x009cb860, its call count 0x009cb864): (seed * 0x41c64e6d + 0x3039) >> 16 & 0x7fff */
static uint32_t act_rand(void) {
    uint32_t seed = U32(0, 0x009cb860) * 0x41c64e6du + 0x3039u;
    SU32(0, 0x009cb860, seed);
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
    return (seed >> 16) & 0x7fff;
}

/* ======================================================================================================================== */
/* the activity row                                                                                                         */

static void crit_int(rx_addr vec, rx_addr crit, rx_addr name, uint32_t value) {
    rx_tbl_crit_init_int(crit, name, 0, value);
    rx_tbl_crit_push(vec, crit);
    rx_tbl_crit_destroy(crit);
}
/* ActivityTable_AddRow 004dedc0 (table, type, area (signed char), mode; RET 0x10) -> the new row */
uint32_t rx_activity_table_add_row(rx_addr table, uint32_t type, uint32_t area, uint32_t mode) {
    ACT_FRAME_EBP(L, 8, 0xdc, 3);
    const rx_addr vec = L + 0x10, crit = L + 0x28, out = L + 0xdc;
    rx_tbl_crit_vec_init(vec);
    crit_int(vec, crit, 0x0093dd90, type);                            /* "ActivityType" (004f6d80) */
    SU32(L, 0xc, (uint32_t)(int32_t)(int8_t)area);
    crit_int(vec, crit, 0x0093dda0, (uint32_t)(int32_t)(int8_t)area); /* "ActivityArea" */
    crit_int(vec, crit, 0x00936e30, mode);                            /* "ActivityMode" (004f70a0) */
    ACT_CALL(2, rx_tbl_add_row(table, out, vec));                     /* 004ec950 */
    const uint32_t row = rx_tbl_row_count(table) - 1;
    rx_tbl_crit_vec_destroy(vec);
    ACT_END(L);
    return row;
}
RX_FUNCTION(0x004dedc0, rx_activity_table_add_row)
/* FUN_004df0f0 (ECX = table, EAX = type; area; RET 4): the first row of the type (searchable index) whose ActivityArea is the area,
 * else -1 */
int32_t rx_activity_find_row(rx_addr table, uint32_t type, int32_t area) {
    ACT_FRAME_EBP(L, 8, 0xc, 3);
    SU32(L, 0xc, type);
    rx_addr set = rx_tbl_rows(act_column(table, 0x0093dd90), (int32_t)type);   /* "ActivityType" (00418e50) */
    int32_t r = -1;
    for (rx_addr n = LEFT_OF(set); n != U32(set, 0x18); n = rx_tbl_rowset_next(n)) {
        const rx_addr t = U32(n, 0x14);
        const uint32_t row = U32(n, 0xc);
        if ((int32_t)U32(rx_tbl_cell_word(t, rx_tbl_column_index(t, 0x0093dda0), row), 0) == area) { r = (int32_t)row; break; }   /* "ActivityArea" */
    }
    ACT_END(L);
    return r;
}
/* FUN_005a33b0 (ECX = type, EAX = mode; ui, area (char), laps, duration (float s), spectator; RET 0x14): the activity row of the type
 * and area (added when missing) gets the mode, laps, duration and spectator flag; NumLaps / Duration changes count in the activity
 * table's change counter (0x00ae1b80); then the UI event "ActivityRulesChanged" (UI only). The original keeps the mode in the area's
 * argument slot. */
void rx_activity_setup_row(uint32_t type, uint32_t mode, rx_addr ui, uint32_t area, uint32_t laps, float duration, uint32_t spectator) {
    ACT_FRAME(L, 0, 4);
    const rx_addr a_area = L + 0x18, a_laps = L + 0x1c, a_dur = L + 0x20, a_spec = L + 0x24;
    SU32(L, 0x14, ui); SU32(a_area, 0, area); SU32(a_laps, 0, laps); SF32(a_dur, 0, duration); SU32(a_spec, 0, spectator);   /* the argument slots
                                                                          (a C caller does not push them; the code reads them back) */
    int32_t row;
    ACT_CALLR(row, 1, rx_activity_find_row(ACT_ACTIVITY_TBL, type, (int32_t)(int8_t)area));   /* 004df0f0 */
    if (row == -1) ACT_CALLR(row, 4, (int32_t)rx_activity_table_add_row(ACT_ACTIVITY_TBL, type, area, mode));   /* 004dedc0 */
    SU32(a_area, 0, mode);
    if (mode != act_cell(ACT_ACTIVITY_TBL, 0x00936e30, (uint32_t)row)) SU32(0, 0x00ae1b80, U32(0, 0x00ae1b80) + 1);   /* "ActivityMode" */
    ACT_CALL(3, rx_tbl_set_int(act_column(ACT_ACTIVITY_TBL, 0x00936e30), (uint32_t)row, a_area));
    if (U32(a_laps, 0) != act_cell(ACT_ACTIVITY_TBL, 0x0093dd64, (uint32_t)row)) SU32(0, 0x00ae1b80, U32(0, 0x00ae1b80) + 1);   /* "NumLaps" */
    ACT_CALL(3, rx_tbl_set_int(act_column(ACT_ACTIVITY_TBL, 0x0093dd64), (uint32_t)row, a_laps));
    {   /* ucomiss; lahf; test ah, 0x44; jnp: counts unless exactly equal */
        float o = rx_bf(act_cell(ACT_ACTIVITY_TBL, 0x0093dd58, (uint32_t)row)), n = F32(a_dur, 0);   /* "Duration" */
        if (!(n == o)) SU32(0, 0x00ae1b80, U32(0, 0x00ae1b80) + 1);
    }
    ACT_CALL(3, rx_tbl_set_float(act_column(ACT_ACTIVITY_TBL, 0x0093dd58), (uint32_t)row, a_dur));   /* 004f5650 */
    if (U32(a_spec, 0) != act_cell(ACT_ACTIVITY_TBL, 0x0093df20, (uint32_t)row)) SU32(0, 0x00ae1b80, U32(0, 0x00ae1b80) + 1);   /* "Spectator" */
    ACT_CALL(3, rx_tbl_set_int(act_column(ACT_ACTIVITY_TBL, 0x0093df20), (uint32_t)row, a_spec));

#ifdef RX_ORACLE
    {   /* the UI event "ActivityRulesChanged": 005db500 (ESI = ui; name; RET 4) finds it, then FUN_004020c0 (EAX = ui + 0x98, the event
         * manager) dispatches it with the event as the payload, kept in the laps argument slot: UI only */
        rx_regs in, o; memset(&in, 0, sizeof in); in.r[6] = ui;
        const uint32_t k[1] = {0x0094c614};
        rx_call_original_ex(0x005db500, &in, &o, 1, k);
        if (o.r[0]) {
            SU32(a_laps, 0, o.r[0]);
            uint32_t node = 0x009e1a7c;
            rx_call_original(U32(0, 0x00916388), RX_THISCALL, 0x009cdb68, 1, &node);   /* type_info::_name_internal_method */
            const uint32_t ev = o.r[0];
            memset(&in, 0, sizeof in); in.r[0] = U32(ui, 0x98); in.r[6] = U32(ui, 0x98); in.r[7] = ev;   /* EAX = ESI = the manager, EDI = the event */
            const uint32_t a[2] = {a_laps, 4};
            rx_call_original_ex(0x004020c0, &in, &o, 2, a);
        }
    }
#endif
    ACT_END(L);
}
/* FUN_004de3f0: the number of human players (PlayerType 0) in the Player List */
uint32_t rx_player_list_humans(void) {
    uint32_t n = 0;
    for (uint32_t r = 0;; r++) {
        uint32_t count = U32(ACT_PLAYER_LIST, 0x14) == U32(ACT_PLAYER_LIST, 0x10) ? 0 : rx_tbl_column_rows(U32(U32(ACT_PLAYER_LIST, 0x10), 0));
        if (r >= count) break;
        if (act_cell(ACT_PLAYER_LIST, 0x00936ef4, r) == 0) n++;        /* "PlayerType" */
    }
    return n;
}
RX_FUNCTION(0x004de3f0, rx_player_list_humans)

/* ======================================================================================================================== */
/* the AI riders                                                                                                            */

/* MXUIManager_AddAIPlayer 0059e960 (EAX = gear set, ECX = AI rider; vehicle (AIVehicleId), name, activity type, area (char),
 * difficulty bucket; RET 0x14): Rider List (4, rider, gear set), Vehicle List (4, vehicle), a Player List row (AI, local, the name,
 * team = rows & 1) -> the row */
uint32_t rx_ai_player_add(uint32_t gear, uint32_t rider, uint32_t vehicle, rx_addr name, uint32_t activity, uint32_t area, uint32_t bucket) {
    ACT_FRAME(L, 0x48, 2);
    const rx_addr rec = L + 8;
    uint32_t rid, vid;
    ACT_CALLR(rid, 3, rx_tbl_rider_list_add_row(4, rider, gear));     /* 004dea90 */
    ACT_CALLR(vid, 2, rx_tbl_vehicle_list_add_row(4, vehicle));       /* 004de810 */
    SU32(rec, 0x1c, vid);
    SU32(rec, 0x24, 0xffffffffu); SU32(rec, 0x28, 0xffffffffu); SU32(rec, 0x2c, 0xffffffffu); SU32(rec, 0x34, 0xffffffffu);
    SU32(rec, 0, 1); SU32(rec, 4, 1);
    SU32(rec, 8, name); SU32(rec, 0xc, name); SU32(rec, 0x10, 0);
    SU32(rec, 0x14, activity); SU8(rec, 0x18, area);
    SU32(rec, 0x20, rid); SU32(rec, 0x30, 1);
    SU32(rec, 0x38, rx_tbl_row_count(ACT_PLAYER_LIST) & 1);           /* 004021d0 */
    SU32(rec, 0x3c, 0); SU32(rec, 0x40, bucket);
    uint32_t row;
    ACT_CALLR(row, 1, rx_player_list_add_row(rec));                   /* 004dd460 */
    ACT_END(L);
    return row;
}
/* MXUIManager_AddAIRiderOfClass 0059ebc0 (AI rider, vehicle class, activity type, area, difficulty bucket; RET 0x14): one of the
 * class's VehicleClassType rows drawn uniformly with the game's generator, redrawn (that row dropped) until the rider has an
 * AIRiderVehicle of its VehicleTypeId; the rider's AIVehicleId / DefaultRiderGearSetId / DisplayName make the AI player.
 * Returns its Player List row, -1 for an unknown rider. */
int32_t rx_ai_rider_add_of_class(uint32_t rider, uint32_t cls, uint32_t activity, uint32_t area, uint32_t bucket) {
    ACT_FRAME(L, 0x138, 4);
    const rx_addr a_rider = L + 0x14c, a_cls = L + 0x150;
    SU32(a_rider, 0, rider); SU32(a_cls, 0, cls); SU32(L, 0x154, activity); SU32(L, 0x158, area); SU32(L, 0x15c, bucket);   /* the argument slots */
    const rx_addr vct = rx_tbl_find(0x00ae0910, 0x00936cc0);          /* "VehicleClassType" */
    SU32(L, 0x18, vct);
    rx_tbl_find(0x00ae0910, 0x00940718);                               /* "AIVehicle" */
    rx_tbl_find(0x00ae0910, 0x0094cfa0);                               /* "AIRiderGear" */
    const rx_addr airider = rx_tbl_find(0x00ae0910, 0x0094063c);      /* "AIRider" */
    const rx_addr arv = rx_tbl_find(0x00ae0910, 0x0094cf60);          /* "AIRiderVehicle" */
    SU32(L, 0x3c, arv);
    const rx_addr ref = rx_tbl_row(act_column(airider, 0x0094cf70), I32(a_rider, 0));   /* "AIRiderId" (00418ec0) */
    SU32(L, 0x14, ref);
    if (!U32(ref, 8) || U32(ref, 0) == 0xffffffffu) { ACT_END(L); return -1; }
    const rx_addr set = L + 0x68, vec = L + 0x1c, crit = L + 0x90, out = L + 0x50;
    rx_tbl_rowset_copy(set, rx_tbl_rows(act_column(vct, 0x00936cd4), I32(a_cls, 0)));   /* "VehicleClassId": 00418e50, 004ebce0 */
    for (;;) {
        uint32_t r = act_rand() % U32(set, 0x1c);
        rx_addr n = LEFT_OF(set);
        SU32(L, 0x34, U32(set, 0)); SU32(L, 0x38, n);                 /* the iterator */
        for (; r; r--) { n = rx_tbl_rowset_next(n); SU32(L, 0x38, n); }   /* 00417df0 */
        const uint32_t row = U32(n, 0xc);
        SU32(L, 0x10, row);
        const uint32_t type = act_cell(vct, 0x00936ce4, row);          /* "VehicleTypeId" */
        rx_tbl_crit_vec_init(vec);
        crit_int(vec, crit, 0x00936ce4, type);                         /* VehicleTypeId == the type */
        crit_int(vec, crit, 0x0094cf70, U32(a_rider, 0));              /* AIRiderId == the rider */
        SU32(out, 0, 0xffffffffu); SU32(out, 8, 0);
        ACT_CALL(3, rx_tbl_find_first(arv, vec, out));                 /* 004ed410 */
        if (U32(out, 8) && U32(out, 0) != 0xffffffffu) break;
        SU32(L, 0x5c, row); SU32(L, 0x60, set); SU32(L, 0x64, U32(set, 0x24));
        rx_tbl_rowset_erase(set, row);                                 /* 004f3c60 + 004faa00 + 004f1eb0 */
        rx_tbl_crit_vec_destroy(vec);
    }
    const uint32_t vid = act_cell(U32(out, 8), 0x0094cf7c, U32(out, 0));   /* "AIVehicleId" */
    SU32(L, 0x18, vid);
    rx_tbl_crit_vec_destroy(vec);                                      /* 00417590 */
    const uint32_t gear = act_cell(U32(ref, 8), 0x0094cf88, U32(ref, 0));   /* "DefaultRiderGearSetId" */
    SU32(L, 0x10, gear); SU32(L, 0x14, U32(ref, 0));
    rx_addr dn = rx_tbl_cell_string(act_column(U32(ref, 8), 0x0093deac), U32(ref, 0));   /* "DisplayName" (0041a480) */
    int32_t pr;
    ACT_CALLR(pr, 5, (int32_t)rx_ai_player_add(gear, U32(a_rider, 0), vid, rx_tbl_str_cstr(dn), U32(L, 0x154), U32(L, 0x158), U32(L, 0x15c)));   /* 0059e960 */
    rx_tbl_rowset_destroy(set);
    ACT_END(L);
    return pr;
}
RX_FUNCTION(0x0059ebc0, rx_ai_rider_add_of_class)

/* the int vector of the roster (std::vector<int>: +0 proxy, +0xc begin, +0x10 end, +0x14 capacity) */
static void ivec_init(rx_addr v) {
    rx_addr px = rx_reflex_aligned_alloc(4);
    if (px) SU32(px, 0, v);
    SU32(v, 0, px); SU32(v, 0xc, 0); SU32(v, 0x10, 0); SU32(v, 0x14, 0);
}
static void ivec_reserve(rx_addr v, uint32_t n) {                      /* 005ba080 (ECX = vector; n; RET 4) */
    if (n > 0x3fffffffu) act_unreached(0x005ba0b8, "vector _Xlen");
    uint32_t cap = U32(v, 0xc) ? (U32(v, 0x14) - U32(v, 0xc)) >> 2 : 0;
    if (cap >= n) return;
    rx_addr p = rx_reflex_aligned_alloc(4 * n);                         /* 0042bf30 */
    uint32_t size = (U32(v, 0x10) - U32(v, 0xc)) >> 2;
    if (size) crt_memmove(p, U32(v, 0xc), 4 * size);                   /* 00775700 _Umove */
    if (U32(v, 0xc)) rx_reflex_aligned_free(U32(v, 0xc));
    SU32(v, 0x14, p + 4 * n); SU32(v, 0x10, p + 4 * size); SU32(v, 0xc, p);
}
static void ivec_push(rx_addr v, uint32_t x) {                         /* 004f04e0 (EDI = vector; &value; RET 4) */
    uint32_t size = (U32(v, 0x10) - U32(v, 0xc)) >> 2, cap = U32(v, 0xc) ? (U32(v, 0x14) - U32(v, 0xc)) >> 2 : 0;
    if (size < cap) { SU32(U32(v, 0x10), 0, x); SU32(v, 0x10, U32(v, 0x10) + 4); return; }
    uint32_t half = cap >> 1, n = (0x3fffffffu - half < cap) ? 0 : cap + half;   /* 004f1b60 (_Insert_n): capacity * 1.5, at least size + 1 */
    if (n < size + 1) n = size + 1;
    rx_addr p = rx_reflex_aligned_alloc(4 * n);
    if (size) crt_memmove(p, U32(v, 0xc), 4 * size);
    SU32(p, 4 * size, x);
    if (U32(v, 0xc)) rx_reflex_aligned_free(U32(v, 0xc));
    SU32(v, 0x14, p + 4 * n); SU32(v, 0x10, p + 4 * (size + 1)); SU32(v, 0xc, p);
}
static void ivec_destroy(rx_addr v) {
    if (U32(v, 0xc)) rx_reflex_aligned_free(U32(v, 0xc));
    rx_addr px = U32(v, 0);
    SU32(v, 0xc, 0); SU32(v, 0x10, 0); SU32(v, 0x14, 0);
    rx_reflex_aligned_free(px);
}
/* FUN_005c1360 (cdecl first, last): std::random_shuffle with the CRT's rand (msvcr90 0x0091624c) */
static void random_shuffle(rx_addr first, rx_addr last) {
    uint32_t i = 2;
    for (rx_addr p = first + 4; p != last; p += 4, i++) {
        uint32_t r = (uint32_t)crt_rand() & 0x7fff;
        if (i > 0x7fff) act_unreached(0x005c1393, "random_shuffle of more than 0x7fff elements");
        rx_addr q = first + 4 * (r % i);
        if (p != q) { uint32_t t = U32(q, 0); SU32(q, 0, U32(p, 0)); SU32(p, 0, t); }
    }
}
/* round(x) as the original: _copysign(0.5, x) + x in x87 (53-bit precision on the main thread: exact here), then _ftol2_sse
 * (truncation), int -> float -> int */
static int32_t round_half_away(float x) {
    double r = crt_copysign(0.5, (double)x) + (double)x;
    int32_t i = (int32_t)r;
    return rx_cvtt((float)i);
}
/* MXUIManager_BuildAIRoster 0059f3e0 (EAX = AIDifficulty; ui, count, activity type, area, vehicle class; RET 0x14): the riders split
 * into the four difficulty buckets by the AI difficulty (80..94 all in bucket 3; 95..104 a random 18..35 % in bucket 3, the rest in
 * bucket 2; 105..119 a random 9..26 % in bucket 2, the rest in bucket 1, with "Pro" riders; 120+ all in bucket 0, "Pro"), the AIRider
 * rows of the "Pro" value drawn at random (riders already racing skipped) and kept when they have an AIRiderVehicle of one of the
 * class's VehicleClassType types, until enough; shuffled (CRT rand), then added from the back, bucket 0 first
 * (MXUIManager_AddAIRiderOfClass). The original counts the remaining riders down in its count argument slot. */
void rx_ai_roster_build(uint32_t difficulty, rx_addr ui, uint32_t count, uint32_t activity, uint32_t area, uint32_t cls) {
    if (!difficulty) return;
    ACT_FRAME(L, 0x178, 4);
    const rx_addr a_count = L + 0x190;
    SU32(L, 0x18c, ui); SU32(a_count, 0, count); SU32(L, 0x194, activity); SU32(L, 0x198, area); SU32(L, 0x19c, cls);   /* the argument slots */
    const int32_t n = I32(a_count, 0);
    SU32(L, 0x14, 0);                                                  /* "Pro" */
    if (difficulty - 0x50u <= 0xe) {
        SU32(L, 0x3c, (uint32_t)n); SU32(L, 0x30, 0); SU32(L, 0x38, 0); SU32(L, 0x34, 0);
    } else if (difficulty - 0x5fu <= 9) {
        float x = (float)(int32_t)(act_rand() % 0x12 + 0x12) * 0.01f /* 0x9322ac */ * (float)n;
        int32_t k = round_half_away(x);
        SU32(L, 0x3c, (uint32_t)k); SU32(L, 0x38, (uint32_t)(n - k)); SU32(L, 0x30, 0); SU32(L, 0x34, 0);
    } else if (difficulty - 0x69u <= 0xe) {
        float x = (float)(int32_t)(act_rand() % 0x12 + 9) * 0.01f * (float)n;
        int32_t k = round_half_away(x);
        SU32(L, 0x3c, 0); SU32(L, 0x38, (uint32_t)k); SU32(L, 0x34, (uint32_t)(n - k)); SU32(L, 0x30, 0); SU32(L, 0x14, 1);
    } else if ((int32_t)difficulty >= 0x78) {
        SU32(L, 0x3c, 0); SU32(L, 0x30, (uint32_t)n); SU32(L, 0x14, 1); SU32(L, 0x38, 0); SU32(L, 0x34, 0);
    }                                                                  /* else: the buckets keep the stack's words (as the original) */
    /* the AI riders already racing (Player List rows of PlayerType 1: their rider's OwnerRiderId) */
    const rx_addr racing = L + 0x58, types = L + 0x40, chosen = L + 0x18, crits = L + 0xb4, crit = L + 0xd8, rows = L + 0x88;
    ivec_init(racing);
    ACT_CALL(1, ivec_reserve(racing, (U32(ACT_PLAYER_LIST, 0x14) - U32(ACT_PLAYER_LIST, 0x10)) >> 2));
    for (uint32_t r = 0;; r++) {
        uint32_t nr = U32(ACT_PLAYER_LIST, 0x14) == U32(ACT_PLAYER_LIST, 0x10) ? 0 : rx_tbl_column_rows(U32(U32(ACT_PLAYER_LIST, 0x10), 0));
        if ((int32_t)r >= (int32_t)nr) break;
        if (act_cell(ACT_PLAYER_LIST, 0x00936ef4, r) == 1) {
            /*0059f624..6d8: preserve authored AI while excluding their real
             * owner AIRiderIds from the later random candidate draw. Requested
             * rider fallback is signed-negative, NOT only the -1 sentinel.*/
            uint32_t rider = act_cell(ACT_PLAYER_LIST, 0x0093df70, r);
            SU32(L, 0x10, rider);
            if ((int32_t)rider < 0) {
                rider = act_cell(ACT_PLAYER_LIST, 0x0093dec8, r);
                SU32(L, 0x10, rider);
            }
            const rx_addr ref = rx_tbl_row(act_column(ACT_RIDER_LIST, 0x0093dec8), (int32_t)rider);
            const rx_addr store = U32(ref, 8);
            const uint32_t rider_row = U32(ref, 0);
            const uint32_t owner_id = act_cell(store, 0x0093dfb0, rider_row);
            SU32(L, 0x10, owner_id);
            ACT_CALL(1, ivec_push(racing, owner_id));
        }
    }
    /* the class's vehicle types */
    ivec_init(types);
    const rx_addr vct = rx_tbl_find(0x00ae0910, 0x00936cc0);          /* "VehicleClassType" */
    rx_addr set = rx_tbl_rows(act_column(vct, 0x00936cd4), I32(L, 0x19c));   /* "VehicleClassId" == the class */
    ACT_CALL(1, ivec_reserve(types, U32(set, 0x1c)));
    for (rx_addr nd = LEFT_OF(set); nd != U32(set, 0x18); nd = rx_tbl_rowset_next(nd)) {
        SU32(L, 0x70, U32(set, 0)); SU32(L, 0x74, nd);
        const rx_addr t = U32(nd, 0x14);
        uint32_t ty = U32(rx_tbl_cell_word(t, rx_tbl_column_index(t, 0x00936ce4), U32(nd, 0xc)), 0);   /* "VehicleTypeId" */
        SU32(L, 0x10, ty);
        if ((U32(types, 0x10) - U32(types, 0xc)) >> 2 >= (U32(types, 0x14) - U32(types, 0xc)) >> 2) act_unreached(0x0059f850, "types vector growth");
        SU32(U32(types, 0x10), 0, ty); SU32(types, 0x10, U32(types, 0x10) + 4);
    }
    /* the AIRider rows of the "Pro" value */
    ivec_init(chosen);
    const rx_addr airider = rx_tbl_find(0x00ae0910, 0x0094063c);      /* "AIRider" */
    SU32(L, 0x10, airider);
    const rx_addr arv = rx_tbl_find(0x00ae0910, 0x0094cf60);          /* "AIRiderVehicle" */
    SU32(L, 0xb0, arv);
    rx_tbl_crit_vec_init(crits);
    rx_tbl_crit_init_int(crit, 0x0093dbd8, 0, U32(L, 0x14));          /* "Pro" == the flag */
    rx_tbl_crit_push(crits, crit);
    rx_tbl_crit_destroy(crit);
    rx_tbl_rowset_init(rows, 0);                                       /* proxy, head (004f4ea0); size, sort column, table 0 */
    ACT_CALL(1, rx_tbl_select(airider, rows, crits));                  /* 004ed2f0 */
    ACT_CALL(1, ivec_reserve(chosen, U32(rows, 0x1c)));
    while (U32(rows, 0x1c)) {
        uint32_t r = act_rand() % U32(rows, 0x1c);
        rx_addr nd = LEFT_OF(rows);
        SU32(L, 0x80, U32(rows, 0)); SU32(L, 0x84, nd);
        for (; r; r--) { nd = rx_tbl_rowset_next(nd); SU32(L, 0x84, nd); }   /* 00417df0 */
        const uint32_t row = U32(nd, 0xc);
        SU32(L, 0x70, row);
        const uint32_t rid = act_cell(airider, 0x0094cf70, row);       /* "AIRiderId" */
        SU32(L, 0x14, rid);
        rx_addr p = U32(racing, 0xc);
        while (p != U32(racing, 0x10) && U32(p, 0) != rid) p += 4;
        if (p == U32(racing, 0x10)) {
            rx_addr rs = rx_tbl_rows(act_column(arv, 0x0094cf70), (int32_t)rid);   /* AIRiderVehicle rows of the rider */
            for (rx_addr m = LEFT_OF(rs); m != U32(rs, 0x18); m = rx_tbl_rowset_next(m)) {
                SU32(L, 0x78, U32(rs, 0)); SU32(L, 0x7c, m);
                const rx_addr t = U32(m, 0x14);
                const uint32_t ty = U32(rx_tbl_cell_word(t, rx_tbl_column_index(t, 0x00936ce4), U32(m, 0xc)), 0);   /* "VehicleTypeId" */
                uint32_t k = 0, nt = (U32(types, 0x10) - U32(types, 0xc)) >> 2;
                while ((int32_t)k < (int32_t)nt && U32(U32(types, 0xc), 4 * k) != ty) k++;
                if ((int32_t)k < (int32_t)nt) {
                    ACT_CALL(1, ivec_push(chosen, rid));               /* 004f04e0 */
                    SU32(a_count, 0, U32(a_count, 0) - 1);
                    break;
                }
            }
        }
        SU32(L, 0xcc, row); SU32(L, 0xd0, rows); SU32(L, 0xd4, U32(rows, 0x24));
        rx_tbl_rowset_erase(rows, row);                                /* 004f0800 */
        if (I32(a_count, 0) <= 0) break;
    }
    if (U32(chosen, 0xc) != U32(chosen, 0x10)) ACT_CALL(2, random_shuffle(U32(chosen, 0xc), U32(chosen, 0x10)));   /* 005c1360 */
    for (uint32_t b = 0; b < 4; b++) {
        const rx_addr bucket = L + 0x30 + 4 * b;
        while (I32(bucket, 0) > 0 && U32(chosen, 0x10) != U32(chosen, 0xc)) {
            const uint32_t id = U32(U32(chosen, 0x10), -4);
            int32_t pr;
            ACT_CALLR(pr, 5, rx_ai_rider_add_of_class(id, U32(L, 0x19c), U32(L, 0x194), U32(L, 0x198), b));   /* 0059ebc0 */
            (void)pr;
            if ((U32(chosen, 0x10) - U32(chosen, 0xc)) & 0xfffffffcu) SU32(chosen, 0x10, U32(chosen, 0x10) - 4);
            SU32(bucket, 0, U32(bucket, 0) - 1);
        }
    }
    rx_tbl_rowset_destroy(rows);
    rx_tbl_crit_vec_destroy(crits);                                    /* 00417590 */
    ivec_destroy(chosen);
    ivec_destroy(types);
    ivec_destroy(racing);
    ACT_END(L);
}

/* MXUIManager_SetupAIRiders 0059f2d0 (ui, count; RET 8): the AI roster for count riders with the UI's VehicleClass, AIDifficulty,
 * TrackId and event type (EventTypeActivation, or EventType when that is FreeRide) */
void rx_ai_riders_setup(rx_addr ui, int32_t count) {
    ACT_FRAME(L, 0x24, 4);
    const rx_addr g = U32(0, 0x00a93fcc), buf = L + 0x1c;
    (void)ui;
    SU8(buf, 0, 0); crt_memset(buf + 1, 0, 0xf);
    SU32(L, 0x10, 0); SU32(L, 0x14, 0); SU32(L, 0x18, 0xffffffffu);
    act_prop_int(g, 0x00935df8, L + 0x18);                             /* "VehicleClass" */
    act_prop_int(g, 0x00936e48, L + 0x14);                             /* "AIDifficulty" */
    act_prop_int(g, 0x00936e40, L + 0x10);                             /* "TrackId" */
    act_prop_str(g, 0x0094c320, buf, 0x10);                            /* "EventTypeActivation" */
    if (rx_event_type_of(buf) == 8) act_prop_str(g, 0x00935e08, buf, 0x10);   /* "EventType" */
    const uint32_t type = rx_event_type_of(buf);
    if (count > 0) ACT_CALL(5, rx_ai_roster_build(U32(L, 0x14), U32(L, 0x38), (uint32_t)count, type, U32(L, 0x10), U32(L, 0x18)));   /* 0059f3e0 */
    ACT_END(L);
}
RX_FUNCTION(0x0059f2d0, rx_ai_riders_setup)

/* MXUIManager::SetupEvent 0059a970 (ECX = ui): the event's activity row and AI riders from the UI's event properties (the core: the
 * event spec). EventType (EventTypeActivation, or EventType when that is FreeRide), the length (EventDurationType: laps or minutes,
 * EventDuration, defaults 005560f0 / 3 minutes), TrackId, ActivityMode; SpectatorMode = 0; the registry's AI override (0x00ae1b90:
 * EventRiders = [0x00ae1b94] + 1, AIDifficulty by [0x00ae1ba0]); the activity row (005a33b0); outside online play the AI / remote
 * players pruned (0059e7a0) and EventRiders minus the human players added as AI riders (0059f2d0). */
void rx_setup_event(rx_addr ui) {
    ACT_FRAME_EBP(L, 8, 0x6c, 3);
    const rx_addr act_buf = L + 0x44, dur_buf = L + 0x54, mode_buf = L + 0x64;
    SU32(L, 0x24, 0x50); act_prop_int(ui, 0x00936e48, L + 0x24);      /* "AIDifficulty" */
    SU32(L, 0x2c, 1); act_prop_int(ui, 0x0094bfcc, L + 0x2c);          /* "EventRiders" */
    SU32(L, 0x38, 0xffffffffu); act_prop_int(ui, 0x00935df8, L + 0x38);   /* "VehicleClass" */
    act_prop_set_int(ui, 0x009416d8, 0);                               /* "SpectatorMode" */
    for (uint32_t i = 0; i < 9; i++) SU8(act_buf, i, U8(0x00935ddc, i));   /* "FreeRide" */
    for (uint32_t i = 9; i < 0x10; i++) SU8(act_buf, i, 0);
    act_prop_str(ui, 0x0094c320, act_buf, 0x10);                       /* "EventTypeActivation" */
    SU32(L, 0x14, rx_event_type_of(act_buf));
    if (U32(L, 0x14) == 8) { act_prop_str(ui, 0x00935e08, act_buf, 0x10); SU32(L, 0x14, rx_event_type_of(act_buf)); }   /* "EventType" */
    SU32(dur_buf, 0, U32(0, 0x0092d688)); SU8(dur_buf, 4, U8(0, 0x0092d68c));   /* "Laps" */
    for (uint32_t i = 5; i < 0x10; i++) SU8(dur_buf, i, 0);
    act_prop_str(ui, 0x00936e58, dur_buf, 0x10);                       /* "EventDurationType" */
    SU32(L, 0x1c, rx_duration_type_of(dur_buf));
    SU32(L, 0x18, 0); act_prop_int(ui, 0x00936e40, L + 0x18);          /* "TrackId" */
    for (uint32_t i = 0; i < 0xb; i++) SU8(mode_buf, i, U8(0x0092d6d4, i));   /* "Exhibition" */
    for (uint32_t i = 0xb; i < 0x10; i++) SU8(mode_buf, i, 0);
    act_prop_str(ui, 0x00936e30, mode_buf, 0x10);                      /* "ActivityMode" */
    SU32(L, 0x40, rx_activity_mode_of(mode_buf));
    uint32_t pm;
    ACT_CALLR(pm, 0, rx_game_player_mode());                           /* 0075ec80 */
    SU32(L, 0x3c, pm);
    const uint32_t type = U32(L, 0x14);
    SU32(L, 0x10, 0xffffffffu);
    SU32(L, 0x20, type - 6u <= 0x2bu ? default_duration(type) : 3);
    SU32(L, 0x28, 3);
    if (act_prop_int(ui, 0x00936e6c, L + 0x10) && I32(L, 0x10) != -1) {   /* "EventDuration" */
        if (U32(L, 0x1c) == 1) SU32(L, 0x28, U32(L, 0x10));           /* minutes */
        else SU32(L, 0x20, U32(L, 0x10));                              /* laps */
    }
    SU32(L, 0x10, 0); SU32(L, 0x34, 0); SU32(L, 0x1c, 0); SU32(L, 0x30, 0xffffffffu);
    if (act_prop_int(ui, 0x0094caa0, L + 0x10) && U32(L, 0x10) == 1)  /* "IsWinningPlayer": the online event setup */
        act_unreached(0x0059ab93, "the online / playlist event setup");
    uint32_t riders = U32(L, 0x2c);
    if (U32(0, 0x00ae1b90)) {                                          /* the registry's AI settings */
        riders = U32(0, 0x00ae1b94) + 1;
        act_prop_set_int(ui, 0x0094bfcc, (int32_t)riders);             /* "EventRiders" */
        uint32_t d = U32(L, 0x24);
        switch (U32(0, 0x00ae1ba0)) { case 0: d = 0x78; break; case 1: d = 0x69; break; case 2: d = 0x5f; break; case 3: d = 0x50; break; default: break; }
        act_prop_set_int(ui, 0x00936e48, (int32_t)d);                  /* "AIDifficulty" */
    }
    const float seconds = (float)I32(L, 0x28) * 60.0f;                 /* 0x931da0 */
    ACT_CALL(5, rx_activity_setup_row(type, U32(L, 0x40), ui, U32(L, 0x18), U32(L, 0x20), seconds, 0));   /* 005a33b0 */
    if (pm != 2 && pm != 3) {
        ACT_CALL(0, rx_player_list_prune());                           /* 0059e7a0 */
        uint32_t humans;
        ACT_CALLR(humans, 0, rx_player_list_humans());                 /* 004de3f0 */
        const int32_t ai = (int32_t)(riders - humans);
        if (ai > 0) ACT_CALL(2, rx_ai_riders_setup(ui, ai));           /* 0059f2d0 */
    }
    ACT_END(L);
}
RX_FUNCTION(0x0059a970, rx_setup_event)

/* ======================================================================================================================== */
/* MXUIManager::StartEvent 0059ade0 and the level request                                                                    */

/* UIProperty_SetString 005dbf50 (EDI = key, ESI = ui; stdcall value; RET 4): the UI's store; nothing in the core reads the two
 * strings StartEvent sets (EventType = the activation, EventTypeActivation = "FreeRide"): dropped in the product */
static void act_prop_set_str(rx_addr ui, rx_addr key, rx_addr value) {
#ifdef RX_ORACLE
    rx_regs in, o; memset(&in, 0, sizeof in); in.r[7] = key; in.r[6] = ui;
    rx_call_original_ex(0x005dbf50, &in, &o, 1, &value);
#else
    (void)ui; (void)key; (void)value;
#endif
}
/* the network session (Quazal; not core): oracle the original, product nothing */
static uint32_t act_net_call(rx_addr fn, enum rx_conv conv, uint32_t ecx, int n, const uint32_t* a) {
#ifdef RX_ORACLE
    return rx_call_original(fn, conv, ecx, n, a);
#else
    (void)fn; (void)conv; (void)ecx; (void)n; (void)a; return 0;
#endif
}
/* audio (not core): oracle the original, product nothing */
static void act_audio_call(rx_addr fn, enum rx_conv conv, uint32_t ecx, int n, const uint32_t* a) {
#ifdef RX_ORACLE
    rx_call_original(fn, conv, ecx, n, a);
#else
    (void)fn; (void)conv; (void)ecx; (void)n; (void)a;
#endif
}
static void world_pause(rx_addr w, uint32_t v) { ACT_CALL(0, rx_mxworld_set_paused(w, v)); }
/* the session's static initializer guard (0x00dcf304 bit 0: 006de1f0 + atexit at startup) */
static void session_static(void) {
    if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "the session's static initialiser has not run");
}

/* FUN_005063c0 (ESI = table, EAX = column name; row, &value; RET 8): Table_SetCellFloat (the change counter +0x68 when the value
 * differs: ucomiss, NaN counts as different) */
void rx_tbl_set_cell_float(rx_addr t, rx_addr name, uint32_t row, rx_addr valp) {
    ACT_FRAME(L, 0, 3);
    const uint32_t c = rx_tbl_column_index(t, name);
    const rx_addr cell = rx_tbl_cell_word(t, c, row);
    const float a = F32(valp, 0), b = F32(cell, 0);
    if (!(a == b)) SU32(t, 0x68, U32(t, 0x68) + 1);
    if (c >= (U32(t, 0x14) - U32(t, 0x10)) >> 2) act_unreached(0x005063fa, "column index (_invalid_parameter_noinfo)");
    ACT_CALL(3, rx_tbl_set_float(U32(U32(t, 0x10), 4 * c), row, valp));
    ACT_END(L);
}
RX_FUNCTION(0x005063c0, rx_tbl_set_cell_float)

/* FUN_004dde70 (ESI = row; type, area (char); RET 8): RequestedActivityType / RequestedActivityArea (the change counter 0x00ae0e70);
 * the original reuses its argument slots for the values */
void rx_player_list_request_activity(uint32_t row, uint32_t type, uint32_t area) {
    ACT_FRAME(L, 0, 3);
    const rx_addr s0 = L + 0x10, s1 = L + 0x14;
    SU32(s0, 0, type);
    if (type != act_cell(ACT_PLAYER_LIST, 0x0093df2c, row)) SU32(0, 0x00ae0e70, U32(0, 0x00ae0e70) + 1);   /* "RequestedActivityType" */
    ACT_CALL(3, rx_tbl_set_int(act_column(ACT_PLAYER_LIST, 0x0093df2c), row, s0));
    const uint32_t a = (uint32_t)(int32_t)(int8_t)area;
    SU32(s1, 0, a);
    if (a != act_cell(ACT_PLAYER_LIST, 0x0093df44, row)) SU32(0, 0x00ae0e70, U32(0, 0x00ae0e70) + 1);   /* "RequestedActivityArea" */
    ACT_CALL(3, rx_tbl_set_int(act_column(ACT_PLAYER_LIST, 0x0093df44), row, s1));
    ACT_END(L);
}
RX_FUNCTION(0x004dde70, rx_player_list_request_activity)

/* FUN_005a3590 (cdecl type, area, all): every Player List row: a local player (PlayerLocal == 1; the AI ones too unless `all` is 0)
 * requests the activity (004dde70); every row's StartZone = -1 */
void rx_player_list_request_event(uint32_t type, uint32_t area, uint32_t all) {
    ACT_FRAME(L, 8, 4);
    const rx_addr t = ACT_PLAYER_LIST;
    for (uint32_t r = 0;; r++) {
        if (((U32(t, 0x14) - U32(t, 0x10)) >> 2) == 0) break;
        if (r >= rx_tbl_column_rows(U32(U32(t, 0x10), 0))) break;
        const rx_addr cells = U32(act_column(t, 0x0093de90), 0x40);   /* "PlayerLocal" (under the table lock: one thread) */
        if (r >= (U32(cells, 0x10) - U32(cells, 0xc)) >> 2) act_unreached(0x005a3620, "row index (_invalid_parameter_noinfo)");
        if (U32(U32(cells, 0xc), 4 * r) == 1 && !(all == 0 && act_cell(t, 0x00936ef4, r) == 1))   /* "PlayerType" */
            ACT_CALL(2, rx_player_list_request_activity(r, type, area));
        SU32(L, 0x10, 0xffffffffu);
        if (act_cell(t, 0x0093df00, r) != 0xffffffffu) SU32(0, 0x00ae0e70, U32(0, 0x00ae0e70) + 1);   /* "StartZone" */
        ACT_CALL(3, rx_tbl_set_int(act_column(t, 0x0093df00), r, L + 0x10));
    }
    ACT_END(L);
}
RX_FUNCTION(0x005a3590, rx_player_list_request_event)

/* FUN_005a3750 (cdecl type, area (char)): the activity table's rows of other activities erased (from the last row) */
void rx_activity_rows_keep(uint32_t type, uint32_t area) {
    ACT_FRAME(L, 4, 4);
    const rx_addr t = ACT_ACTIVITY_TBL;
    const int32_t a = (int32_t)(int8_t)area;
    for (int32_t r = (int32_t)rx_tbl_row_count(t) - 1; r >= 0; r--) {
        const rx_addr cells = U32(act_column(t, 0x0093dd90), 0x40);   /* "ActivityType" */
        if ((uint32_t)r >= (U32(cells, 0x10) - U32(cells, 0xc)) >> 2) act_unreached(0x005a37a5, "row index (_invalid_parameter_noinfo)");
        if (U32(U32(cells, 0xc), 4 * (uint32_t)r) == type && I32(rx_tbl_cell_word(t, rx_tbl_column_index(t, 0x0093dda0), (uint32_t)r), 0) == a) continue;
        ACT_CALL(2, rx_tbl_erase_row(t, (uint32_t)r));
    }
    ACT_END(L);
}
RX_FUNCTION(0x005a3750, rx_activity_rows_keep)

/* MXGame_RequestLevel 0075c640 (EAX = level name): the pending level (+0x734c, +0x7450 = 0); from the front end (state 2) the game
 * goes to state 3 (the load), the spectator / split timers cleared (+0x15b34 .. +0x15b3c; online: [0x009cb208] / [0x009cb1dc]), each
 * local human player's network presence set (the session [0x00da6cb0] vf12: not core) */
void rx_game_request_level(rx_addr name) {
    ACT_FRAME(L, 0xc, 4);
    const rx_addr g = U32(0, 0x00a946b8);
    crt_strncpy(g + 0x734c, name, 0x103);
    SU8(g, 0x734c + 0x103, 0);
    SU32(g, 0x7450, 0);
    if (U32(g, 0x18) == 2) {
        SU32(g, 0x18, 3);
        uint32_t pm;
        ACT_CALLR(pm, 0, rx_game_player_mode());
        SU32(L, 0x18, pm);
        SF32(g, 0x15b38, 0.0f); SF32(g, 0x15b34, 0.0f); SF32(g, 0x15b3c, 0.0f);
        if (pm == 2 || pm == 3) {
            SU32(L, 0x14, 0);
            act_prop_int(U32(0, 0x00a93fcc), 0x009416d8, L + 0x14);   /* "SpectatorMode" */
            if (U32(L, 0x14) == 0) SU32(g, 0x15b38, U32(0, 0x009cb1dc));
            SU32(g, 0x15b34, U32(0, 0x009cb208));
        }
        /* the presence context: the UI manager's +0x290 / +0x298 (UI), else the session's state */
        const rx_addr ui = U32(0, 0x00a93fcc);
        uint32_t ctx = 0;
#ifdef RX_ORACLE
        if (U32(ui, 0x290)) ctx = 1;
        else if (U32(ui, 0x298)) ctx = 2;
        else {
            const rx_addr s = act_net_call(0x006de1c0, RX_CDECL, 0, 0, 0);
            if (U32(U32(s, 0x10), 0x1c4c) != 0 && U32(U32(s, 0x10), 0x1b0) != 1) ctx = U32(U32(s, 0x10), 0x1c48) == 0 ? 5 : 3;
        }
#else
        (void)ui;
#endif
        const rx_addr t = ACT_PLAYER_LIST;
        for (uint32_t r = 0;; r++) {
            if (((U32(t, 0x14) - U32(t, 0x10)) >> 2) == 0) break;
            if (r >= rx_tbl_column_rows(U32(U32(t, 0x10), 0))) break;
            const uint32_t type = act_cell(t, 0x00936ef4, r), local = act_cell(t, 0x0093de90, r);   /* "PlayerType" / "PlayerLocal" */
            if (type != 0 || local == 0) continue;
            const uint32_t ctrl = act_cell(t, 0x0093ded0, r);   /* "Controller" */
            session_static();
            const rx_addr s = U32(0, 0x00da6cb0);
            const uint32_t a1[3] = {ctrl, 8, pm}, a2[3] = {ctrl, 9, ctx}, a3[3] = {ctrl, 0, 7};
            act_net_call(U32(U32(s, 0), 0x30), RX_THISCALL, s, 3, a1);
            act_net_call(U32(U32(s, 0), 0x30), RX_THISCALL, s, 3, a2);
            act_net_call(U32(U32(s, 0), 0x30), RX_THISCALL, s, 3, a3);
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x0075c640, rx_game_request_level)

/* MXGame_ResumeFromUI 0075c8c0 (EAX = game; stdcall hide_ui; RET 4) -> 1 / 0: both scenes made active (007d20b0: the event scene
 * only when the game has no pending UI world, +0x7454), +0x15b20 = 1, sounds resumed (audio), then with the UI manager its
 * acceptance (00599230; UI) and the hub routing (UI), the worlds unpaused (00767480) */
uint32_t rx_game_resume_from_ui(rx_addr g, uint32_t hide_ui) {
    ACT_FRAME(L, 0, 2);
    const rx_addr core = U32(0, 0x00a95844);
    {
        ACT_CALL(1, rx_scene_set_active(U32(g, 0x7454), U32(core, 0x20)));   /* EAX = active, stdcall scene */
        ACT_CALL(1, rx_scene_set_active(U32(g, 0x7454) == 0, U32(core, 0x1c)));
    }
    SU8(g, 0x15b20, 1);
    { const uint32_t a[2] = {0, 0}; act_audio_call(0x00805000, RX_CDECL, 0, 2, a); }   /* Sound_PauseAll(0, 0) */
    uint32_t r = 1;
    if (U32(g, 0x745c) != 0) {
#ifdef RX_ORACLE
        rx_regs in, o; memset(&in, 0, sizeof in); in.r[0] = U32(g, 0x745c);
        act_ui_call_ex(0x00599230, &in, &o, 0, 0);   /* (EAX = the UI manager) */
        const uint32_t ok = o.r[0];
#else
        const uint32_t ok = 1;   /* the UI accepts (not core) */
#endif
        if (!ok) { ACT_END(L); return 0; }
        if (hide_ui) { rx_regs in, o; memset(&in, 0, sizeof in); in.r[0] = U32(g, 0x745c); act_ui_call_ex(0x0059b760, &in, &o, 0, 0); }
        if (U32(g, 0x7454) == 0 && U32(U32(g, 0x20c8), 0x20014) != 0) world_pause(U32(g, 0x20c8), 0);
        if (U32(g, 0x5a54) != 0 && U32(g, 0x7454) != 0 && U32(U32(g, 0x5a54), 0x20014) != 0) world_pause(U32(g, 0x5a54), 0);
    } else {
        const rx_addr ui_world = U32(g, 0x20c8);
        if (U32(ui_world, 0x20014) != 0) { SU32(U32(core, 0x10), 0xc, 0); if (U32(ui_world, 0x20014) != 0) SU32(ui_world, 0x20014, 0); }
        const rx_addr w = U32(g, 0x5a54);
        if (w && U32(w, 0x20014) != 0) { SU32(U32(core, 0x10), 0xc, 0); if (U32(w, 0x20014) != 0) SU32(w, 0x20014, 0); }
    }
    ACT_END(L);
    return r;
}
RX_FUNCTION(0x0075c8c0, rx_game_resume_from_ui)

/* UI_RequestConfiguredLevel 0059bd60 (stdcall ui, gameplay page; RET 8): EventLocation, the in-game UI page set up (005a43e0: the HUD
 * and its UI packages; UI, not core), the unreferenced Player List rows freed, the game resumed (0075c8c0 (0)), the level requested
 * (0075c640), the session's event description (0059cd40: network, not core) */
void rx_ui_request_level(rx_addr ui, uint32_t page) {
    ACT_FRAME(L, 0x110, 3);
    const rx_addr loc = L + 0x10;
    SU8(loc, 0, 0);
    for (uint32_t i = 1; i < 0x100; i++) SU8(loc, i, 0);
    act_prop_str(ui, 0x0093e7c4, loc, 0x100);                         /* "EventLocation" */
    if (page) act_ui_call(0x005a43e0, RX_THISCALL, ui, 0, 0);
#ifndef RX_ORACLE
    /* The loading page (005a43e0's UI packages / script) resumes the engine when it opens: its App.Pause(false) (Lua binding
     * 007424a0) writes the timing pause word 0 (and the UI world's +0x20014). The product's UI manager is absent (act_ui_call is
     * the boundary), so the engine pause the page clears is cleared here: the oracle's first load frame (MXGame state 3) already
     * has the FX gate's timing +0xc = 0 (measured: REFLEX_FX_TRACE, the frames of states 3..5 all pause 0). */
    SU32(U32(U32(0, 0x00a95844), 0x10), 0xc, 0);
#endif
    ACT_CALL(0, rx_player_list_free_unreferenced());                  /* 0059e4e0 */
    ACT_CALL(1, rx_game_resume_from_ui(U32(0, 0x00a946b8), 0));
    ACT_CALL(0, rx_game_request_level(loc));
    act_ui_call(0x0059cd40, RX_THISCALL, ui, 0, 0);
    ACT_END(L);
}
RX_FUNCTION(0x0059bd60, rx_ui_request_level)

/* MXUIManager::StartEvent 0059ade0 (ECX = ui): outside the quick / online event: the exclusive input device released (single
 * player), the event's type from EventTypeActivation (a Free Ride challenge) or EventType, EventTypeActivated, the location and
 * TrackId (the activity area), EventHasLaps / EventHasTimer / EventHasRounds: the activity row's Duration = -1 without a timer and
 * NumLaps = -1 without laps or rounds; the local players request the activity (005a3590), the other activity rows erased (005a3750),
 * the local human players made Active, the activity area set up (Game_SetupActivityArea) and the level requested (0059bd60). */
void rx_start_event(rx_addr ui) {
    ACT_FRAME_EBP(L, 8, 0x134, 3);
    ACT_CALL(0, rx_player_list_free_unreferenced());
    SU32(L, 0xc, 0);
    act_prop_int(ui, 0x0094c918, L + 0xc);                            /* "QuickEvent" */
    uint32_t pm;
    ACT_CALLR(pm, 0, rx_game_player_mode());
    SU32(L, 0x14, pm);
    if (U32(L, 0xc) != 0 && (pm == 2 || pm == 3)) act_unreached(0x0059ae37, "the quick online event");
    if (pm == 0) {
        while (U32(0, 0x009c4ee0) != 0xffffffffu) {   /* the exclusive device's references released */
            if (I32(0, 0x00a94070) > 0) {
                SU32(0, 0x00a94070, U32(0, 0x00a94070) - 1);
                if (U32(0, 0x00a94070) == 0) {
                    SU32(0, 0x009c4ee0, 0xffffffffu);
                    act_prop_set_int(U32(0, 0x00a93fcc), 0x0094c924, -1);   /* "ExclusiveDeviceIndex" */
                }
            }
        }
    }
    const rx_addr act = L + 0x28, loc = L + 0x38;
    SU8(loc, 0, 0);
    for (uint32_t i = 1; i < 0x100; i++) SU8(loc, i, 0);
    SU32(L, 0xc, 0); SU32(L, 0x20, 0); SU32(L, 0x1c, 0);
    for (uint32_t i = 0; i < 0x10; i++) SU8(act, i, 0);
    SU32(L, 0x24, 1);
    act_prop_str(ui, 0x0094c320, act, 0x10);                          /* "EventTypeActivation" */
    uint32_t type = rx_event_type_of(act);
    if (type == 8) {
        act_prop_str(ui, 0x00935e08, act, 0x10);                      /* "EventType" */
        act_prop_set_int(ui, 0x0094c964, 0);                          /* "EventTypeActivated" */
    } else {
        act_prop_set_str(ui, 0x00935e08, act);                        /* EventType = the activation */
        act_prop_set_str(ui, 0x0094c320, 0x00935ddc);                 /* EventTypeActivation = "FreeRide" */
        act_prop_set_int(ui, 0x0094c964, 1);
    }
    act_prop_str(ui, 0x0093e7c4, loc, 0x100);                         /* "EventLocation" */
    act_prop_int(ui, 0x0094bfcc, L + 0x10);                           /* "EventRiders" */
    act_prop_int(ui, 0x00936e40, L + 0xc);                            /* "TrackId" */
    act_prop_int(ui, 0x0093e108, L + 0x24);                           /* "EventHasLaps" */
    act_prop_int(ui, 0x0093e128, L + 0x20);                           /* "EventHasTimer" */
    act_prop_int(ui, 0x0093e118, L + 0x1c);                           /* "EventHasRounds" */
    type = rx_event_type_of(act);
    SU32(L, 0x10, type);
    int32_t row;
    ACT_CALLR(row, 1, rx_activity_find_row(ACT_ACTIVITY_TBL, type, (int32_t)(int8_t)U8(L, 0xc)));
    SU32(L, 0x18, (uint32_t)row);
    if (U32(L, 0x20) == 0) {
        SU32(L, 0x20, U32(0, 0x009cd9e8));                            /* -1.0f (.data) */
        ACT_CALL(2, rx_tbl_set_cell_float(ACT_ACTIVITY_TBL, 0x0093dd58, (uint32_t)row, L + 0x20));   /* "Duration" */
    }
    if (U32(L, 0x24) == 0 && U32(L, 0x1c) == 0) {
        SU32(L, 0x1c, 0xffffffffu);
        ACT_CALL(2, rx_tbl_set_cell_int(ACT_ACTIVITY_TBL, 0x0093dd64, (uint32_t)row, L + 0x1c));       /* "NumLaps" */
    }
    if (pm == 1) {   /* the exclusive device's network profile (the session's device table [0x00da6cac], 0x40 per device: +0x34 == 2) */
        const uint32_t dev = U32(0, 0x009c4ee0);
        session_static();
        if (dev <= 1 && U32(U32(0, 0x00da6cac), dev * 0x40 + 0x34) == 2) {
            act_net_call(0x006de1c0, RX_CDECL, 0, 0, 0);
            if ((int32_t)type >= 0 && (int32_t)type <= 0x14 && type != 8) act_unreached(0x0059b0d9, "the online profile event start (0059c950)");
        }
    }
    (void)act_cell(ACT_ACTIVITY_TBL, 0x00936e30, (uint32_t)row);      /* "ActivityMode" (read; unused) */
    uint32_t view;
    ACT_CALLR(view, 0, rx_game_is_view_track(U32(0, 0x00a946b8)));    /* 0075c5f0 */
    if (view) act_unreached(0x0059b111, "StartEvent while viewing a track");
    if (pm == 2 || pm == 3) act_unreached(0x0059b2a4, "the online event start");
    ACT_CALL(3, rx_player_list_request_event(type, U32(L, 0xc), 1));
    ACT_CALL(2, rx_activity_rows_keep(type, U32(L, 0xc)));
    const rx_addr t = ACT_PLAYER_LIST;
    for (uint32_t r = 0; r < rx_tbl_row_count(t); r++) {
        if (act_cell(t, 0x00936ef4, r) != 0) continue;                 /* "PlayerType": human */
        SU32(L, 0x10, 1);
        if (act_cell(t, 0x0093def8, r) != 1) SU32(0, 0x00ae0e70, U32(0, 0x00ae0e70) + 1);   /* "Active" */
        ACT_CALL(3, rx_tbl_set_int(act_column(t, 0x0093def8), r, L + 0x10));
    }
    ACT_CALL(0, rx_game_setup_activity_area(U32(0, 0x00a946b8)));     /* 0075c430 */
    ACT_CALL(2, rx_ui_request_level(ui, 1));
    ACT_END(L);
}
RX_FUNCTION(0x0059ade0, rx_start_event)
