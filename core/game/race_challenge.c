/* race_challenge.c -- the profile's challenge awards.
 *
 * A Free Ride jump (005851f0) or discovery (00585330) that beats a medal level calls 0054e670 (player, kind, medal, value). The
 * profile database of the player (0xae10f0 + 0x1f4 * number) keeps one row per event location in its table "PlayerChallenges":
 *     EventTypeLocationId | Bronze | Silver | Gold | Param
 * (the medal counts and the best value, a float). The location is found in the default database's "EventTypeLocations" table from
 * the event type of the activity kind (0x15 + kind: 004e7780), the session's UI properties "EventLocationId" / "TrackId".
 *
 *   004ea0a0  the EventTypeLocationId of {EventTypeId, EventLocationId, TrackId} (-1: none or ambiguous)
 *   004ea6c0  reads the location's row: Bronze, Silver, Gold, Param (the outputs stay as they were without a row)
 *   004ea3a0  writes the row (counts the changed cells in the table's revision +0x68), or adds the row when the location has none
 *   006d8390  career progress: in career mode the highlight tracking counts two more events (0x15, 0x16)
 *   00554d40  raises "ChallengeAwardAchieved" (player, kind, medal) on the activity's event manager
 *   0055ec70 / 0055ed20  the challenge activities' vf63: compares a new value with the stored one (-1 / 0 / +1)
 * Every function is a hand transliteration of the disassembly; the frames and the stack depth of each call are the original's
 * (callees keep addresses of the argument slots: 004ea3a0 passes the address of its own stack arguments to the cell writers). */
#include "race_challenge.h"
#include "race.h"
#include "race_extern.h"
#include "crt.h"
#include "build_activity_util.h"
#include "build_activity_freeride_run.h"
#include "build_tables.h"
#include "build_vehicle.h"
#include "fx.h"
#include "highlight_progress.h"
#include "race_activity_ui.h"
#include "world_veh.h"
#include <string.h>

/* the strings the original passes */
#define STR_EVENT_LOCATION_ID    0x00936e20u   /* "EventLocationId" (UI property and column) */
#define STR_TRACK_ID             0x00936e40u   /* "TrackId" */
#define STR_EVENT_TYPE_ID        0x00936e90u   /* "EventTypeId" */
#define STR_EVENT_TYPE_LOCATIONS 0x00936e7cu   /* "EventTypeLocations" (default database) */
#define STR_ETL_ID               0x0093e228u   /* "EventTypeLocationId" */
#define STR_PLAYER_CHALLENGES    0x0093e7e0u   /* "PlayerChallenges" (profile database) */
#define STR_BRONZE               0x0093ddf4u
#define STR_SILVER               0x0093ddecu
#define STR_GOLD                 0x0093dde4u
#define STR_PARAM                0x0093e7f4u
#define STR_IN_CAREER_MODE       0x00941f6cu   /* "InCareerMode" */
#define DEFAULT_DATABASE         0x00ae0910u
#define PROFILE_DATABASE(n)      (0x00ae10f0u + (n) * 0x1f4u)

/* fld dword; fstp dword: the bits, a signalling NaN quieted */
static uint32_t fcopy(uint32_t bits) {
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x007fffffu) != 0) bits |= 0x00400000u;
    return bits;
}
static float bits_float(uint32_t bits) { float f; memcpy(&f, &bits, sizeof f); return f; }

/* The column object `index` of a table; the checked vector's range test (the original's _invalid_parameter_noinfo call). */
static rx_addr column_at(rx_addr table, uint32_t index) {
    if (index >= ((U32(table, 0x14) - U32(table, 0x10)) >> 2)) crt_invalid_parameter_noinfo();
    return U32(U32(table, 0x10), 4 * index);
}

/* ---- 004ea0a0 ------------------------------------------------------------------------------------------------------------------- */

/* 004ea0a0 (stdcall type id, location, track; RET 0xc) -> EAX: the EventTypeLocationId of the one row of "EventTypeLocations"
 * (default database) whose EventTypeId / EventLocationId / TrackId equal the arguments, else -1 (no row, or several).
 * Frame: sub 0xf0; push ebx / ebp / esi / edi. Locals: the criteria vector at +0x38, the criterion at +0x50, the row set at +0x10. */
int32_t rx_challenge_event_location(uint32_t type_id, uint32_t location, uint32_t track) {
    const rx_addr args = rx_sp() + 4;                                        /* the three stack arguments: the criteria read their values there */
    ACT_FRAME(F, 0xf0, 4);
    const rx_addr set = F + 0x10, vec = F + 0x38, crit = F + 0x50;
    static const rx_addr names[3] = {STR_EVENT_TYPE_ID, STR_EVENT_LOCATION_ID, STR_TRACK_ID};
    SU32(args, 0, type_id); SU32(args, 4, location); SU32(args, 8, track);
    int32_t result = -1;

    rx_tbl_crit_vec_init(vec);                                               /* the vector's proxy block (ReflexAlignedAlloc 4) */
    for (int k = 0; k < 3; k++) {
        ACT_CALL(1, (void)rx_fx_crit_init_int(crit, 0, args + 4 * k, names[k]));   /* 00419a70 (ESI = crit, EAX = 0, ECX = &value) */
        ACT_CALL(1, rx_tbl_crit_push(vec, crit));                           /* 004175e0 */
        rx_tbl_crit_destroy(crit);                                           /* inline: the path vector, its proxy, 004eedc0 */
    }
    rx_tbl_rowset_init(set, 0);                                              /* 004f4a30 / 004f4ea0, size / sort / table = 0 */
    rx_addr table;
    ACT_CALLR(table, 1, rx_tbl_find(DEFAULT_DATABASE, STR_EVENT_TYPE_LOCATIONS));   /* 00402220 */
    ACT_CALL(1, rx_tbl_select(table, set, vec));                             /* 004ed2f0 (ECX = table, EAX = the set; &criteria) */
    if (U32(set, 0x1c) == 1) {
        const rx_addr head = U32(set, 0x18);
        const rx_addr node = U32(head, 0);                                   /* the first (only) row reference */
        if (U32(set, 0) == 0) crt_invalid_parameter_noinfo();                /* checked iterator: the owner proxy */
        if (node == U32(U32(set, 0), 0x18)) crt_invalid_parameter_noinfo();  /* ... and not the end */
        const uint32_t row = U32(node, 0xc);
        const rx_addr row_table = U32(node, 0x14);
        uint32_t column;
        ACT_CALLR(column, 1, rx_tbl_column_index(row_table, STR_ETL_ID));    /* 00402160 */
        rx_addr cell;
        ACT_CALLR(cell, 1, rx_tbl_cell_word(row_table, column, row));        /* 0041aaf0 */
        result = (int32_t)U32(cell, 0);
    }
    rx_tbl_rowset_destroy(set);                                              /* 004ebe10 (sorted set), 004f2220, 004dc0f0, the proxy freed */
    ACT_CALL(0, rx_fx_crit_vec_destroy(vec));                                /* 00417590 */
    ACT_END(F);
    return result;
}
RX_FUNCTION(0x004ea0a0, rx_challenge_event_location)

/* ---- 004ea6c0 ------------------------------------------------------------------------------------------------------------------- */

/* 004ea6c0 (EAX = the profile number; stdcall etl, &bronze, &silver, &gold, &param; RET 0x14): the profile's "PlayerChallenges"
 * row of the location (the unique index of EventTypeLocationId, 00418ec0) read into the four outputs, in the order Bronze, Silver,
 * Gold, Param (the float copied with fld / fstp). A location without a row leaves the outputs alone.
 * Frame: push ebx / ebp / esi / edi. */
void rx_challenge_profile_read(uint32_t profile, uint32_t etl, rx_addr bronze, rx_addr silver, rx_addr gold, rx_addr param) {
    ACT_FRAME(F, 0, 4);
    rx_addr table;
    ACT_CALLR(table, 1, rx_tbl_find(PROFILE_DATABASE(profile), STR_PLAYER_CHALLENGES));
    uint32_t index;
    ACT_CALLR(index, 1, rx_tbl_column_index(table, STR_ETL_ID));
    const rx_addr column = column_at(table, index);
    rx_addr ref;
    ACT_CALLR(ref, 1, rx_tbl_row(column, (int32_t)etl));                     /* 00418ec0 (EAX = &etl) -> the row reference {row, set, table} */
    rx_addr store = U32(ref, 8);
    if (store != 0 && U32(ref, 0) != 0xffffffffu) {
        static const rx_addr names[4] = {STR_BRONZE, STR_SILVER, STR_GOLD, STR_PARAM};
        const rx_addr outs[4] = {bronze, silver, gold, param};
        for (int k = 0; k < 4; k++) {
            if (k > 0) store = U32(ref, 8);                                  /* the original reads the reference again each time */
            const uint32_t row = U32(ref, 0);
            uint32_t col;
            ACT_CALLR(col, 1, rx_tbl_column_index(store, names[k]));
            rx_addr cell;
            ACT_CALLR(cell, 1, rx_tbl_cell_word(store, col, row));
            SU32(outs[k], 0, k == 3 ? fcopy(U32(cell, 0)) : U32(cell, 0));
        }
    }
    ACT_END(F);
}
RX_FUNCTION(0x004ea6c0, rx_challenge_profile_read)

/* ---- 004ea3a0 ------------------------------------------------------------------------------------------------------------------- */

/* One cell write of 004ea3a0: the revision counter of the table (+0x68) counts a changed value, then the column's setter. */
static void write_cell_int(rx_addr store, rx_addr name, uint32_t row, uint32_t value, rx_addr value_slot) {
    uint32_t index;
    ACT_CALLR(index, 1, rx_tbl_column_index(store, name));
    rx_addr cell;
    ACT_CALLR(cell, 1, rx_tbl_cell_word(store, index, row));
    if (value != U32(cell, 0)) SU32(store, 0x68, U32(store, 0x68) + 1);
    const rx_addr column = column_at(store, index);
    ACT_CALL(3, rx_tbl_set_int(column, row, value_slot));                    /* 004f5330 (column, row, &value) */
}

/* 004ea3a0 (EAX = the profile number; stdcall etl, bronze, silver, gold, param; RET 0x14): the location's row of the profile's
 * "PlayerChallenges" updated cell by cell (the setters get the address of the stack arguments: +8 .. +0x14 above the return
 * address). A location without a row gets one: a criteria vector {EventTypeLocationId, Bronze, Silver, Gold, Param} is added as a
 * row (004ec950) when the location exists in the default database's "EventTypeLocations".
 * Frame: sub 0xd8; push ebx / ebp / esi / edi. */
void rx_challenge_profile_write(uint32_t profile, uint32_t etl, uint32_t bronze, uint32_t silver, uint32_t gold, uint32_t param_bits) {
    const rx_addr args = rx_sp() + 4;                                        /* the five stack arguments */
    ACT_FRAME(F, 0xd8, 4);
    SU32(args, 0, etl); SU32(args, 4, bronze); SU32(args, 8, silver); SU32(args, 12, gold); SU32(args, 16, param_bits);
    rx_addr table;
    ACT_CALLR(table, 1, rx_tbl_find(PROFILE_DATABASE(profile), STR_PLAYER_CHALLENGES));
    uint32_t index;
    ACT_CALLR(index, 1, rx_tbl_column_index(table, STR_ETL_ID));
    const rx_addr column = column_at(table, index);
    rx_addr ref;
    ACT_CALLR(ref, 1, rx_tbl_row(column, (int32_t)etl));
    if (U32(ref, 8) != 0 && U32(ref, 0) != 0xffffffffu) {
        /* the row exists: Bronze, Silver, Gold (ints), then Param (a float; the change test is ucomiss, NaN counts as changed) */
        write_cell_int(U32(ref, 8), STR_BRONZE, U32(ref, 0), bronze, args + 4);
        write_cell_int(U32(ref, 8), STR_SILVER, U32(ref, 0), silver, args + 8);
        write_cell_int(U32(ref, 8), STR_GOLD, U32(ref, 0), gold, args + 12);
        const rx_addr store = U32(ref, 8);
        const uint32_t row = U32(ref, 0);
        uint32_t col;
        ACT_CALLR(col, 1, rx_tbl_column_index(store, STR_PARAM));
        rx_addr cell;
        ACT_CALLR(cell, 1, rx_tbl_cell_word(store, col, row));
        if (bits_float(param_bits) != bits_float(U32(cell, 0))) SU32(store, 0x68, U32(store, 0x68) + 1);
        const rx_addr param_column = column_at(store, col);
        ACT_CALL(3, rx_tbl_set_float(param_column, row, args + 16));         /* 004f5650 */
    } else {
        /* no row yet: the five values as criteria, added to the profile's table when the location is a known one */
        const rx_addr vec = F + 0x10, crit = F + 0x38, out = F + 0x2c;
        rx_tbl_crit_vec_init(vec);
        static const rx_addr names[5] = {STR_ETL_ID, STR_BRONZE, STR_SILVER, STR_GOLD, STR_PARAM};
        for (int k = 0; k < 5; k++) {
            if (k < 4) {
                ACT_CALL(1, (void)rx_fx_crit_init_int(crit, 0, args + 4 * k, names[k]));     /* 00419a70 */
            } else {
                ACT_CALL(1, (void)rx_fx_crit_init_float(crit, 0, args + 16, names[k]));      /* 004f73b0 */
            }
            ACT_CALL(1, rx_tbl_crit_push(vec, crit));                        /* 004175e0 */
            ACT_CALL(0, rx_fx_crit_destroy(crit));                           /* 00415320 */
        }
        rx_addr locations;
        ACT_CALLR(locations, 1, rx_tbl_find(DEFAULT_DATABASE, STR_EVENT_TYPE_LOCATIONS));
        uint32_t lindex;
        ACT_CALLR(lindex, 1, rx_tbl_column_index(locations, STR_ETL_ID));
        const rx_addr lcolumn = column_at(locations, lindex);
        rx_addr lref;
        ACT_CALLR(lref, 1, rx_tbl_row(lcolumn, (int32_t)etl));
        if (U32(lref, 8) != 0 && U32(lref, 0) != 0xffffffffu) {
            ACT_CALL(2, (void)rx_tbl_add_row(table, out, vec));              /* 004ec950 (EAX = the profile's table; out reference, criteria) */
        }
        ACT_CALL(0, rx_fx_crit_vec_destroy(vec));                            /* 00417590 */
    }
    ACT_END(F);
}
RX_FUNCTION(0x004ea3a0, rx_challenge_profile_write)

/* ---- 006d8390 / 00554d40 -------------------------------------------------------------------------------------------------------- */

/* 006d7980 (stdcall tracking, controller, index; RET 0xc): the highlight tracking's profile persistence. The oracle build keeps the
 * original (highlight_progress.c needs a Steam provider there: oracle_highlight.cpp), whose call pushes the arguments itself; the
 * product runs the port at the original's depth. */
static void highlight_progress(rx_addr tracking, uint32_t profile, uint32_t index) {
#ifdef RX_ORACLE
    const uint32_t a[3] = {tracking, profile, index};
    (void)rx_call_original(0x006d7980, RX_STDCALL, 0, 3, a);
#else
    ACT_CALL(3, (void)rx_highlight_add_progress(tracking, profile, index));
#endif
}

/* 006d8390 (EDI = the highlight tracking [0xd92edc], ESI = the profile number; plain ret): in career mode (UI property
 * "InCareerMode" non-zero) the highlight tracking counts the two events 0x15 and 0x16 for the profile. Frame: push ecx. */
void rx_challenge_career_progress(rx_addr tracking, uint32_t profile) {
    ACT_FRAME(F, 0, 1);
    SU32(F, 0, 0);
    RACE_CORE_CALL(2, (void)rx_ui_session_get_int(STR_IN_CAREER_MODE, F));
    if (U32(F, 0) != 0) {
        highlight_progress(tracking, profile, 0x15);
        highlight_progress(tracking, profile, 0x16);
    }
    ACT_END(F);
}
RX_FUNCTION(0x006d8390, rx_challenge_career_progress)

/* 00554d40 (ECX = kind, EDX = medal, ESI = the activity's event manager; stdcall player; RET 4): the event "ChallengeAwardAchieved"
 * (0x942870) with the payload {player, kind, medal} (12 bytes, the first local of the frame) dispatched after the handler
 * signature (void(MXPlayer*, MXChallengeType, MXChallengeAwardType), type 0x9ce508) is named (type_info::_name_internal_method).
 * Frame: sub 0x10 (the /GS cookie at +0xc); push edi. */
void rx_challenge_award_event(uint32_t kind, uint32_t medal, rx_addr events, rx_addr player) {
    const rx_addr entry = rx_sp();
    const rx_addr locals = entry - 0x10, frame = entry - 0x14;
    rx_set_sp(frame);
    SU32(entry, -4, U32(0, 0x009b45e4) ^ locals);                            /* the /GS cookie */
    SU32(locals, 0, player);
    SU32(locals, 4, kind);
    SU32(locals, 8, medal);
    RACE_CORE_CALL(1, (void)crt_type_info_name(0x009ce508, 0x009e1a7c));     /* push 0x9e1a7c; ECX = the type; call [0x916388] */
    RACE_CORE_CALL(2, rx_world_event_dispatch(events, 0x00942870, locals, 0xc));   /* 004020c0 (EAX = the manager, EDI = the name) */
    rx_set_sp(entry);
}
RX_FUNCTION(0x00554d40, rx_challenge_award_event)

/* ---- 0054e670 ------------------------------------------------------------------------------------------------------------------- */

/* 0054e670 (stdcall player, kind, medal, value; RET 0x10): the profile (player +0x1f0, negative: none) records an award of the
 * activity kind (1 .. 8, the event type 0x15 + kind): medal 0 / 1 / 2 = Bronze / Silver / Gold, 3 = none.
 *   - the location comes from the UI properties EventLocationId (default -1) and TrackId (default 0) through 004ea0a0;
 *   - 004ea6c0 reads the stored counts and the stored value Param (default -1.0);
 *   - the medal's count is incremented (clamped to 1 .. 0x7ffffffe; a first Gold - count 0 - also counts the two career
 *     highlights, 006d8390); the other counts stay;
 *   - a location without a stored value (Param == -1.0) is written at once. Otherwise only the kinds 7 (a discovery count) are
 *     written at once, the kinds 1 / 6 / 8 ask the activity's vf63 (0xfc: new, old) and are written when it answers <= 0; the
 *     other kinds are not written;
 *   - then (medal != 3) "ChallengeAwardAchieved" (00554d40) is raised on the activity's event manager (+0x94 -> +0x290).
 * Locals (above the frame): +0x18 the location, then the stored value; +0x1c the track, then the EventTypeLocationId; +0x14 Bronze;
 * +0x10 Silver; +0xc Gold. Frame: push ebp; and esp, -8; sub 0x14; push ebx / esi / edi. */
void rx_challenge_award(rx_addr player, uint32_t kind, uint32_t medal, uint32_t value_bits) {
    RACE_FRAME_EBP(F, 8, 0x14, 3);
    const rx_addr location = F + 0x18, track_etl = F + 0x1c, bronze = F + 0x14, silver = F + 0x10, gold = F + 0xc, param = F + 0x18;

    SU32(location, 0, 0xffffffffu);
    RACE_CORE_CALL(2, (void)rx_ui_session_get_int(STR_EVENT_LOCATION_ID, location));
    SU32(track_etl, 0, 0);
    RACE_CORE_CALL(2, (void)rx_ui_session_get_int(STR_TRACK_ID, track_etl));
    uint32_t type_id;
    RACE_CORE_CALL(1, type_id = rx_event_type_id_of(kind + 0x15));           /* 004e7780 */
    int32_t etl;
    RACE_CORE_CALL(3, etl = rx_challenge_event_location(type_id, U32(location, 0), U32(track_etl, 0)));   /* 004ea0a0 */
    SU32(track_etl, 0, (uint32_t)etl);

    if (I32(player, 0x1f0) >= 0) {
        SU32(param, 0, 0xbf800000u /* -1.0f, 0x9cd9e8 */);
        SU32(bronze, 0, 0); SU32(silver, 0, 0); SU32(gold, 0, 0);
        RACE_CORE_CALL(5, rx_challenge_profile_read(U32(player, 0x1f0), (uint32_t)etl, bronze, silver, gold, param));   /* 004ea6c0 */

        uint32_t gold_new;                                                   /* EBX: the Gold count passed on */
        if (medal == 0 || medal == 1) {
            const rx_addr slot = medal == 0 ? bronze : silver;
            int32_t v = I32(slot, 0) + 1;
            SU32(slot, 0, (uint32_t)v);
            if (v < 1) SU32(slot, 0, 1);
            else if (v > 0x7ffffffe) SU32(slot, 0, 0x7ffffffe);
            gold_new = U32(gold, 0);
        } else if (medal == 2) {
            gold_new = U32(gold, 0);
            if (gold_new == 0) {
                RACE_CORE_CALL(0, rx_challenge_career_progress(U32(0, 0x00d92edc), U32(player, 0x1f0)));   /* 006d8390 (EDI, ESI) */
            }
            gold_new = gold_new + 1;
            if ((int32_t)gold_new < 1) gold_new = 1;
            else if ((int32_t)gold_new > 0x7ffffffe) gold_new = 0x7ffffffe;
        } else {
            gold_new = U32(gold, 0);
        }

        int write_row = 0;
        if (bits_float(0xbf800000u) == bits_float(U32(param, 0))) {
            write_row = 1;                                                   /* nothing stored yet: write at once */
        } else if (kind == 7) {
            write_row = 1;
        } else if (kind == 1 || kind == 6 || kind == 8) {
            const rx_addr activity = U32(player, 0x94);
            const rx_addr fn = race_vfn(activity, 63);                       /* [vtable + 0xfc](new, old) */
            typedef int32_t (*compare_t)(rx_addr, uint32_t, uint32_t);
            compare_t compare = RX_FN(fn, compare_t);
            int32_t answer;
            if (compare != 0) { RACE_CORE_CALL(2, answer = compare(activity, fcopy(value_bits), fcopy(U32(param, 0)))); }
            else answer = (int32_t)race_thiscall(fn, activity, 2, fcopy(value_bits), fcopy(U32(param, 0)), 0, 0);
            write_row = answer <= 0;
        }
        if (write_row) {
            RACE_CORE_CALL(5, rx_challenge_profile_write(U32(player, 0x1f0), U32(track_etl, 0), U32(bronze, 0), U32(silver, 0),
                                                         gold_new, fcopy(value_bits)));                 /* 004ea3a0 */
        }
    }
    if (medal != 3) {
        const rx_addr events = U32(U32(player, 0x94), 0x290);
        RACE_CORE_CALL(1, rx_challenge_award_event(kind, medal, events, player));                       /* 00554d40 */
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0054e670, rx_challenge_award)

/* ---- the challenge activities' vf63 ---------------------------------------------------------------------------------------------- */

/* 0055ec70 (thiscall, unused this; stack new, old; RET 8) -> EAX: -1 when the old value is above the new, +1 when below, else 0
 * (comiss: unordered counts as equal). The challenges whose best value is the lowest (times). */
int32_t rx_challenge_compare_lower_better(rx_addr activity, uint32_t new_bits, uint32_t old_bits) {
    (void)activity;
    const float value = bits_float(new_bits), stored = bits_float(old_bits);
    if (stored > value) return -1;
    if (value > stored) return 1;
    return 0;
}
RX_FUNCTION(0x0055ec70, rx_challenge_compare_lower_better)

/* 0055ed20: the same with the roles swapped (-1 when the new value is above the old: the best value is the highest). */
int32_t rx_challenge_compare_higher_better(rx_addr activity, uint32_t new_bits, uint32_t old_bits) {
    (void)activity;
    const float value = bits_float(new_bits), stored = bits_float(old_bits);
    if (value > stored) return -1;
    if (stored > value) return 1;
    return 0;
}
RX_FUNCTION(0x0055ed20, rx_challenge_compare_higher_better)
