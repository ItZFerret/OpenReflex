/* career_heat.c -- MXSeriesEventHeat (vtable 0x0094e28c): the virtual functions of the Heat format's series event that are its own
 * (vf1, vf2, vf10, vf12, vf13, vf18; the subscriptions vf7 / vf8 are in career_heat_events.c). The other slots are the base
 * MXSeriesEvent's (career_single*.c) and the Freestyle / Moto event's shared vf5 (005ac8f0).
 *
 * The format (RulesPopup.lua "HeatPractice", "Heat", "LCQ", "HeatMainEvent"; _re/notes/career.md "Heat / Moto"): 24 riders, two heats of
 * 12 (the local player rides one, the other is decided by the table order), places 1-5 of a heat go straight to the main event, 6-11 to
 * the LCQ, 12 is out; the LCQ's first two join the main event. Each race is 3 laps (the original's constant).
 * Every function reads the series manager's player table (005b6840 selector 2, "SeriesEventPlayers") as the original does, even on the
 * paths that do not use it. Dependencies (the manager, its player table and states, the standings, the UI properties) come from the
 * composition (career_single.h). */
#include "career_heat.h"
#include "career_moto.h"
#include "career_single_events.h"
#include "build_activity_util.h"
#include "crt.h"

/* the column names */
#define COL_PLAYER_TYPE      0x00936ef4u   /* "PlayerType" */
#define COL_PLAYER_LOCAL     0x0093de90u   /* "PlayerLocal" */
#define COL_PLAYER_STATE     0x0094d650u   /* "PlayerState" */
#define COL_REQUESTED_STATE  0x0094d670u   /* "RequestedPlayerState" */
#define COL_SORT_VALUE       0x0094d6ccu   /* "SortValue" */
/* the UI property names and values */
#define KEY_SERIES_EVENT_STATUS 0x0094c99cu   /* "SeriesEventStatus" */
#define KEY_ALLOW_RESTART       0x0094d9e4u   /* "SeriesAllowRestart" */
#define KEY_ROUTING_MESSAGE     0x0094cc74u   /* "UIRoutingMessage" */
#define KEY_NEXT_SERIES         0x0094ca38u   /* "EnablePostEventNextSeries" */
#define TEXT_HEAT_OR_LCQ        0x0094d7b4u   /* "HeatOrLCQ" */
#define TEXT_FAILED             0x0094beacu   /* "Failed" */
#define TEXT_MAIN_EVENT         0x0094c118u   /* "MainEvent" */
#define TEXT_GOTO_POST_EVENT    0x0094d7a4u   /* "GotoPostEvent" */
/* the constants */
#define FLOAT_MAX_BITS          0x7f7fffffu   /* FLT_MAX (0x00977494) */
#define FLOAT_ONE               1.0f          /* 0x00931f24 */
#define FLOAT_HUNDRED           100.0f        /* 0x0093005c */
/* the activity's sorted drivers (MXActivityRace + 0x2d0, count + 0x26c) */
#define ACTIVITY_DRIVERS        0x2d0u
#define ACTIVITY_DRIVER_COUNT   0x26cu

static void need(int available, uint32_t target) {
    if (!available) act_unreached(target, "unclosed career Heat dependency");
}
static rx_addr manager(const rx_career_single_dependencies* d) {
    need(d && d->manager, 0x00413d20);
    return d->manager();
}
/* 005b6840 (EDX = the manager, ECX = 2): the series' player table */
static rx_addr player_table(const rx_career_single_dependencies* d) {
    const rx_addr m = manager(d);
    need(d->player_table != 0, 0x005b6840);
    return d->player_table(m, 2);
}
static void ui_string(const rx_career_single_dependencies* d, rx_addr key, rx_addr value) {
    need(d->ui_string != 0, 0x005dbf50);
    d->ui_string(key, value);
}
static void ui_int(const rx_career_single_dependencies* d, rx_addr key, uint32_t value) {
    need(d->ui_int != 0, 0x005dbfb0);
    d->ui_int(key, value);
}
/* UIRoutingMessage "GotoPostEvent", then the UI event "GotoHub" (005db500 + the typed raise 004020c0: the in-game page graph's hub) */
static void goto_post_event(const rx_career_single_dependencies* d) {
    ui_string(d, KEY_ROUTING_MESSAGE, TEXT_GOTO_POST_EVENT);
    need(d->ui_goto_hub != 0, 0x005db500);
    d->ui_goto_hub();
}

/* The cell `row` of a float column set to *value (FUN_004f5650), counting a change in the table's revision (+0x68) first: the
 * original's inline "ucomiss value, cell; lahf; test ah, 0x44; jnp" increments unless the two are equal (unordered increments). */
static void set_float_counted(rx_addr table, rx_addr column_name, uint32_t row, rx_addr value) {
    const uint32_t column = rx_tbl_column_index(table, column_name);
    const rx_addr cell = rx_tbl_cell_word(table, column, row);
    if (!(F32(cell, 0) == F32(value, 0))) SU32(table, 0x68, U32(table, 0x68) + 1);
    if (column >= ((U32(table, 0x14) - U32(table, 0x10)) >> 2)) crt_invalid_parameter_noinfo();
    ACT_CALL(3, rx_tbl_set_float(U32(U32(table, 0x10), 4 * column), row, value));
}

/* The name of a driver's player as the series' player table has it (the inline block of vf13 at 005adf44 / 005ae440): the player's
 * name (+0x14), at most 0x43 bytes; for a local human player (+4 set, +0x10 PlayerType 0) the profile's display name in UTF-8. */
static void driver_player_name(rx_addr driver, rx_addr name, rx_addr wide) {
    const rx_addr player = U32(driver, 0x74);
    SU8(name, 0, 0);
    crt_memset(name + 1, 0, 0x43);
    crt_strncpy(name, player + 0x14, 0x43);
    SU8(name, 0x43, 0);
    if (U32(player, 4) != 0 && U32(player, 0x10) == 0) {
        SU16(wide, 0, 0);
        crt_memset(wide + 2, 0, 0x86);
        rx_profile_display_name(I32(player, 0x1f0), wide);              /* MXProfile_GetDisplayName 00616840 */
        rx_wide_to_utf8(wide, name, 0x44);                               /* WideCharToMultiByte(CP_UTF8) */
        SU8(name, 0x43, 0);
    }
}

/* ---- vf1 005ad840 (thiscall, phase; RET 4) -> the PlayerState of the riders of a phase --------------------------------------------- */
uint32_t rx_career_heat_player_state(uint32_t phase) {
    if (phase == 1 || phase == 3) return 4;   /* the practice qualifier and the heat: the local player's heat */
    if (phase == 4) return 6;                 /* the LCQ */
    if (phase == 5) return 8;                 /* the main event */
    return 0;
}

/* ---- vf2 005aee00 (thiscall, enable; RET 4): the "HoleshotActivated" subscription ------------------------------------------------- */
void rx_career_heat_holeshot(rx_addr event, uint32_t enable, const rx_career_single_dependencies* d) {
    ACT_FRAME(F, 0, 2);                                                   /* push ebx; push esi */
    if (enable) {
        if (U32(event, 0xf0) != 0) { ACT_END(F); return; }
        /* 005bdfc0 (EAX = the manager's channel +0x180; stdcall event): the member proxy 0x0094eb38 of 005c3310 */
        ACT_CALL(1, rx_career_member_proxy_bind(U32(manager(d), 0x180), 0x009420d0, event, 0x005c3310, 0x0094eb38, 0x009cbeb4, 1));
        SU32(event, 0xf0, 1);
        SU32(event, 0xec, 0);
        ACT_END(F);
        return;
    }
    if (U32(event, 0xf0) == 0) { ACT_END(F); return; }
    ACT_CALL(1, rx_career_member_proxy_bind(U32(manager(d), 0x180), 0x009420d0, event, 0x005c3310, 0x0094eb38, 0x009cbeb4, 0));   /* 005be080 */
    SU32(event, 0xf0, 0);
    SU32(event, 0xec, 0);
    ACT_END(F);
}

/* ---- vf10 005ad960 (thiscall, practice; RET 4): the CareerPracticePopup's answer (MXSeriesManager::AdvancePractice) ---------------
 * PRACTICE: the qualifier (phase 1) is set up: the heat's riders (state 4), 3 laps. SKIP: the local players' SortValue reset to
 * FLT_MAX (the gate order then falls back to the table's), the heat (phase 3) next and its gate selection (vf9). */
void rx_career_heat_gate_result(rx_addr event, uint32_t practice, const rx_career_single_dependencies* d) {
    ACT_FRAME_EBP(F, 8, 0x1c, 3);
    if (practice) {
        SU32(event, 0x10, 1);
        ACT_CALL(5, rx_career_event_setup_activity(event, 4, 1, 3, U32(event, 0xb0), d));   /* 005a9290 */
        ACT_END(F);
        return;
    }
    const rx_addr table = player_table(d);
    const rx_addr value = F + 0xc;
    const rx_addr humans = rx_tbl_rows(act_column(table, COL_PLAYER_TYPE), 0);   /* 00418e50: PlayerType 0 */
    for (rx_addr node = LEFT_OF(humans); node != U32(humans, 0x18); node = rx_tbl_rowset_next(node)) {
        SU32(value, 0, FLOAT_MAX_BITS);
        set_float_counted(table, COL_SORT_VALUE, U32(node, 0xc), value);
    }
    SU32(event, 0x28, 1);
    SU32(event, 0x10, 3);
    act_vcall(event, 0x24, 0, 0);                                         /* vf9: the gate selection */
    ACT_END(F);
}

/* ---- vf12 005adb00 (thiscall; RET 0): MXSeriesManager::AdvanceEvent, the next race set up ------------------------------------------ */
void rx_career_heat_advance(rx_addr event, const rx_career_single_dependencies* d) {
    ACT_FRAME_EBP(F, 8, 0x20, 4);
    const rx_addr table = player_table(d);
    const uint32_t phase = U32(event, 0x10);
    if (U32(event, 4) != 0) {
        /* a sub-event other than the first (not reached in the career: +4 stays 0) */
        if (phase == 1) {
            need(d->apply_player_states && d->request_player_states, 0x005b39a0);
            ACT_CALL(0, d->apply_player_states(manager(d)));                         /* 005b39a0 */
            d->request_player_states(manager(d), 4, 8);                   /* 005b3890: 4 -> 8 */
            SU32(event, 0x28, 1);
            SU32(event, 0x10, 5);
            act_vcall(event, 0x24, 0, 0);
        } else if (phase == 5) {
            const uint32_t on = 1;
            act_vcall(event, 8, 1, &on);                                  /* vf2(1): the holeshot */
            ACT_CALL(5, rx_career_event_setup_activity(event, 8, 1, 3, U32(event, 0xb0), d));
        }
        ACT_END(F);
        return;
    }
    if (phase == 3) {
        /* The other heat (PlayerState 5) is decided by the table order: its first five to the main event (requested 8, SortValue
         * FLT_MAX), the next six to the LCQ (6, FLT_MAX), the rest out (9). Then the local player's heat (state 4) is set up. */
        const rx_addr requested = F + 0x14, sort_value = F + 0x18;
        const rx_addr other = rx_tbl_rows(act_column(table, COL_PLAYER_STATE), 5);   /* 004199d0: PlayerState 5 */
        int32_t place = 0;
        for (rx_addr node = LEFT_OF(other); node != U32(other, 0x18); node = rx_tbl_rowset_next(node)) {
            const uint32_t row = U32(node, 0xc);
            if (place < 5) {
                SU32(requested, 0, 8);
                ACT_CALL(2, rx_tbl_set_cell_int(table, COL_REQUESTED_STATE, row, requested));     /* Table_SetCellInt 004f6e60 */
                SU32(sort_value, 0, FLOAT_MAX_BITS);
                ACT_CALL(2, rx_tbl_set_cell_float(table, COL_SORT_VALUE, row, sort_value));       /* 005063c0 */
            } else if (place < 11) {
                SU32(requested, 0, 6);
                ACT_CALL(2, rx_tbl_set_cell_int(table, COL_REQUESTED_STATE, row, requested));
                SU32(F + 0x1c, 0, FLOAT_MAX_BITS);
                ACT_CALL(2, rx_tbl_set_cell_float(table, COL_SORT_VALUE, row, F + 0x1c));
            } else {
                SU32(requested, 0, 9);
                ACT_CALL(2, rx_tbl_set_cell_int(table, COL_REQUESTED_STATE, row, requested));
            }
            place++;
        }
        ACT_CALL(5, rx_career_event_setup_activity(event, 4, 1, 3, U32(event, 0xb0), d));   /* the heat */
        ACT_END(F);
        return;
    }
    if (phase == 4) {
        ACT_CALL(5, rx_career_event_setup_activity(event, 6, 1, 3, U32(event, 0xb0), d));   /* the LCQ */
    } else if (phase == 5) {
        const uint32_t on = 1;
        act_vcall(event, 8, 1, &on);
        ACT_CALL(5, rx_career_event_setup_activity(event, 8, 1, 3, U32(event, 0xb0), d));   /* the main event */
    }
    ACT_END(F);
}

/* ---- vf13 005adde0 (thiscall; RET 0): MXSeriesManager::AdvancePostEvent, the results of the race the event set up ---------------
 * Frame: and esp, -8; sub esp, 0x1d4; push ebx / esi / edi. The local name buffers: the player name at +0x58 (wide +0xa0), the LCQ's
 * second pass at +0xa0 (wide +0x128); the criteria query's set at +0x58, its vector at +0x30 and criterion at +0x128. */
static void heat_post_practice(rx_addr event, rx_addr table, const rx_career_single_dependencies* d);
static void heat_post_heat(rx_addr event, rx_addr table, rx_addr F, const rx_career_single_dependencies* d);
static void heat_post_lcq(rx_addr event, rx_addr table, rx_addr F, const rx_career_single_dependencies* d);
static void heat_post_main_event(rx_addr event, rx_addr table, rx_addr F, const rx_career_single_dependencies* d);

void rx_career_heat_post_event(rx_addr event, const rx_career_single_dependencies* d) {
    ACT_FRAME_EBP(F, 8, 0x1d4, 3);
    const rx_addr table = player_table(d);
    if (U32(event, 4) == 0) {
        const uint32_t phase = U32(event, 0xc);
        if (phase == 1) heat_post_practice(event, table, d);
        else if (phase == 3) heat_post_heat(event, table, F, d);
        else if (phase == 4) heat_post_lcq(event, table, F, d);
    }
    if (U32(event, 0xc) == 5) heat_post_main_event(event, table, F, d);
    ACT_END(F);
}

/* the practice qualifier (phase 1): each driver's best lap (RaceDriver + 0x10c4) is its SortValue (the gate order), the heat next */
static void heat_post_practice(rx_addr event, rx_addr table, const rx_career_single_dependencies* d) {
    (void)d;
    const rx_addr activity = U32(event, 0x14);
    const int32_t count = I32(activity, ACTIVITY_DRIVER_COUNT);
    const rx_addr rows = rx_tbl_rows(act_column(table, COL_PLAYER_STATE), 4);   /* the heat's riders */
    for (int32_t i = 0; i < count; i++) {
        const rx_addr driver = U32(U32(event, 0x14), ACTIVITY_DRIVERS + 4u * (uint32_t)i);
        int32_t row;
        ACT_CALLR(row, 3, rx_career_event_driver_row(driver, rows, 0));   /* 005ab030 */
        if (row < 0) continue;
        ACT_CALL(2, rx_tbl_set_cell_float(table, COL_SORT_VALUE, (uint32_t)row, driver + 0x10c4));
    }
    SU32(event, 0x28, 1);
    SU32(event, 0x10, 3);
    act_vcall(event, 0x24, 0, 0);                                         /* vf9: the heat's gate selection */
}

/* the heat (phase 3): by place, 1-5 the main event (requested 8), 6-11 the LCQ (6, the local human 7), the rest out (9, the local human
 * 10); SortValue = place + 1. Then: nobody local in the LCQ or out (or the local human in the main event and another one out): the
 * LCQ is decided by the table order (the other heat's first two LCQ riders, PlayerState 6, to the main event) and the main event is
 * next; a local human in the LCQ: the LCQ next; else the local human is out: "Failed". */
static void heat_post_heat(rx_addr event, rx_addr table, rx_addr F, const rx_career_single_dependencies* d) {
    const rx_addr name = F + 0x58, wide = F + 0xa0, state = F + 0x2c, sort_value = F + 0x28;
    const int32_t count = I32(U32(event, 0x14), ACTIVITY_DRIVER_COUNT);
    uint32_t local_in_main = 0;
    for (int32_t place = 0; place < count; place++) {
        const rx_addr driver = U32(U32(event, 0x14), ACTIVITY_DRIVERS + 4u * (uint32_t)place);
        driver_player_name(driver, name, wide);
        int32_t row;
        ACT_CALLR(row, 2, rx_career_event_player_by_name(name, table));     /* 005aab30 (cdecl name, table) */
        /* the original reads the Player List (0x00ae0e08) at the series table's row */
        const uint32_t type = act_cell(ACT_PLAYER_LIST, COL_PLAYER_TYPE, (uint32_t)row);
        const uint32_t local = act_cell(ACT_PLAYER_LIST, COL_PLAYER_LOCAL, (uint32_t)row);
        const int human = type == 0 && local != 0;
        uint32_t next;
        if (place < 5) {
            next = 8;
            if (human) local_in_main = 1;
        } else if (place < 11) {
            next = human ? 7 : 6;
        } else {
            next = human ? 10 : 9;
        }
        SU32(state, 0, next);
        ACT_CALL(2, rx_tbl_set_cell_int(table, COL_REQUESTED_STATE, (uint32_t)row, state));
        SF32(sort_value, 0, (float)place + FLOAT_ONE);                   /* cvtsi2ss; addss 1.0 */
        ACT_CALL(2, rx_tbl_set_cell_float(table, COL_SORT_VALUE, (uint32_t)row, sort_value));
    }
    const rx_addr lcq_local = rx_tbl_rows(act_column(table, COL_REQUESTED_STATE), 7);
    const rx_addr out_local = rx_tbl_rows(act_column(table, COL_REQUESTED_STATE), 10);
    const uint32_t lcq_count = U32(lcq_local, 0x1c), out_count = U32(out_local, 0x1c);
    if ((lcq_count == 0 && out_count == 0) || (local_in_main && out_count != 0)) {
        const rx_addr other_lcq = rx_tbl_rows(act_column(table, COL_PLAYER_STATE), 6);
        int32_t promoted = 0;
        for (rx_addr node = LEFT_OF(other_lcq); node != U32(other_lcq, 0x18) && promoted < 2; node = rx_tbl_rowset_next(node)) {
            SU32(F + 0xc, 0, 8);
            ACT_CALL(2, rx_tbl_set_cell_int(table, COL_REQUESTED_STATE, U32(node, 0xc), F + 0xc));
            SU32(sort_value, 0, FLOAT_MAX_BITS);
            ACT_CALL(2, rx_tbl_set_cell_float(table, COL_SORT_VALUE, U32(node, 0xc), sort_value));
            promoted++;
        }
        SU32(event, 0x10, 5);
        ui_string(d, KEY_SERIES_EVENT_STATUS, TEXT_HEAT_OR_LCQ);
        (void)manager(d);
        ui_int(d, KEY_ALLOW_RESTART, 0);                                  /* the variant set (005c3590 / 005dc290) */
        SU32(event, 0x28, 1);
    } else if (lcq_count != 0) {
        SU32(event, 0x10, 4);
        ui_string(d, KEY_SERIES_EVENT_STATUS, TEXT_HEAT_OR_LCQ);
        (void)manager(d);                                                 /* 00413d20 */
        ui_int(d, KEY_ALLOW_RESTART, 0);
        SU32(event, 0x28, 1);
    } else if (out_count != 0) {
        ui_string(d, KEY_SERIES_EVENT_STATUS, TEXT_FAILED);
        (void)manager(d);
        ui_int(d, KEY_ALLOW_RESTART, 1);
    }
    goto_post_event(d);
}

/* the LCQ (phase 4): its first two to the main event (requested 8, SortValue place + 1). A local human among them: the main event
 * next. Else every other local human of the LCQ is out (10), and the main event is next if a human is in it (PlayerState 8 and
 * PlayerType 0, by a table query), "Failed" otherwise. */
static void heat_post_lcq(rx_addr event, rx_addr table, rx_addr F, const rx_career_single_dependencies* d) {
    const rx_addr name = F + 0x58, wide = F + 0xa0, state = F + 0xc, sort_value = F + 0x28;
    uint32_t local_qualified = 0;
    SU32(state, 0, 8);
    for (uint32_t place = 0; place < 2; place++) {
        const rx_addr driver = U32(U32(event, 0x14), ACTIVITY_DRIVERS + 4u * place);
        const rx_addr player = U32(driver, 0x74);
        if (U32(player, 4) != 0 && U32(player, 0x10) == 0) local_qualified = 1;
        driver_player_name(driver, name, wide);
        int32_t row;
        ACT_CALLR(row, 2, rx_career_event_player_by_name(name, table));
        ACT_CALL(2, rx_tbl_set_cell_int(table, COL_REQUESTED_STATE, (uint32_t)row, state));
        SF32(sort_value, 0, (float)(int32_t)place + FLOAT_ONE);
        ACT_CALL(2, rx_tbl_set_cell_float(table, COL_SORT_VALUE, (uint32_t)row, sort_value));
    }
    goto_post_event(d);
    if (local_qualified) {
        SU32(event, 0x10, 5);
        ui_string(d, KEY_SERIES_EVENT_STATUS, TEXT_HEAT_OR_LCQ);
        (void)manager(d);
        ui_int(d, KEY_ALLOW_RESTART, 0);
        SU32(event, 0x28, 1);
        return;
    }
    const int32_t count = I32(U32(event, 0x14), ACTIVITY_DRIVER_COUNT);
    if (count > 2) {
        const rx_addr out_name = F + 0xa0, out_wide = F + 0x128, out_state = F + 0x28;
        for (int32_t place = 2; place < count; place++) {
            const rx_addr player = U32(U32(U32(event, 0x14), ACTIVITY_DRIVERS + 4u * (uint32_t)place), 0x74);
            if (U32(player, 4) == 0 || U32(player, 0x10) != 0) continue;
            SU8(out_name, 0, 0);
            crt_memset(out_name + 1, 0, 0x43);
            SU16(out_wide, 0, 0);
            crt_memset(out_wide + 2, 0, 0x86);
            rx_profile_display_name(I32(player, 0x1f0), out_wide);
            rx_wide_to_utf8(out_wide, out_name, 0x44);
            SU8(out_name, 0x43, 0);
            int32_t row;
            ACT_CALLR(row, 2, rx_career_event_player_by_name(out_name, table));
            SU32(out_state, 0, 10);
            ACT_CALL(2, rx_tbl_set_cell_int(table, COL_REQUESTED_STATE, (uint32_t)row, out_state));
        }
    }
    /* the humans in the main event: the rows with PlayerState 8 and PlayerType 0 (004f4a30 / 004f4ea0 the set, the vector, the two
     * criteria 004f6d80 / 004f6bf0 pushed (004175e0) and destroyed (00415320), 004ed2f0 the selection) */
    const rx_addr set = F + 0x58, criteria = F + 0x30, criterion = F + 0x128, key = F + 0xc;
    rx_tbl_rowset_init(set, 0);
    rx_tbl_crit_vec_init(criteria);
    SU32(key, 0, 8);
    rx_tbl_crit_init_int(criterion, COL_PLAYER_STATE, 0, U32(key, 0));
    ACT_CALL(1, rx_tbl_crit_push(criteria, criterion));
    rx_tbl_crit_destroy(criterion);
    SU32(key, 0, 0);
    rx_tbl_crit_init_int(criterion, COL_PLAYER_TYPE, 0, U32(key, 0));
    ACT_CALL(1, rx_tbl_crit_push(criteria, criterion));
    rx_tbl_crit_destroy(criterion);
    ACT_CALL(1, rx_tbl_select(table, set, criteria));
    if (U32(set, 0x1c) != 0) {
        SU32(event, 0x10, 5);
        ui_string(d, KEY_SERIES_EVENT_STATUS, TEXT_HEAT_OR_LCQ);
        (void)manager(d);
        ui_int(d, KEY_ALLOW_RESTART, 0);
        SU32(event, 0x28, 1);
    } else {
        ui_string(d, KEY_SERIES_EVENT_STATUS, TEXT_FAILED);
        (void)manager(d);
        ui_int(d, KEY_ALLOW_RESTART, 1);
    }
    rx_tbl_crit_vec_destroy(criteria);
    rx_tbl_rowset_destroy(set);
}

/* the main event (phase 5): every SortValue reset to 100, the main event's riders (PlayerState 8) get their place; the standings
 * (005b43c0, 005b55d0 into the series' standings -3), SeriesEventStatus "MainEvent", the post event page, the event over (+0x30). */
static void heat_post_main_event(rx_addr event, rx_addr table, rx_addr F, const rx_career_single_dependencies* d) {
    const rx_addr hundred = F + 0xc, place_value = F + 0x1c;
    const uint32_t rows = rx_tbl_row_count(table);                       /* 004eb830 (0 without columns) */
    SF32(hundred, 0, FLOAT_HUNDRED);
    for (uint32_t row = 0; row < rows; row++) set_float_counted(table, COL_SORT_VALUE, row, hundred);
    const rx_addr main_riders = rx_tbl_rows(act_column(table, COL_PLAYER_STATE), 8);
    const int32_t count = I32(U32(event, 0x14), ACTIVITY_DRIVER_COUNT);
    for (int32_t place = 0; place < count; place++) {
        const rx_addr driver = U32(U32(event, 0x14), ACTIVITY_DRIVERS + 4u * (uint32_t)place);
        int32_t row;
        ACT_CALLR(row, 3, rx_career_event_driver_row(driver, main_riders, 0));
        if (row < 0) continue;
        SF32(place_value, 0, (float)place);
        set_float_counted(table, COL_SORT_VALUE, (uint32_t)row, place_value);
    }
    need(d->refresh_standings && d->standings, 0x005b43c0);
    d->refresh_standings(manager(d));                                     /* 005b43c0 */
    const int32_t source = (U32(0, 0x009e2318) != 1) - 2;
    d->standings(manager(d), -3, source, 1);                              /* 005b55d0 */
    ui_string(d, KEY_SERIES_EVENT_STATUS, TEXT_MAIN_EVENT);
    (void)manager(d);
    ui_int(d, KEY_NEXT_SERIES, U32(0, 0x009e2310) == 0);
    goto_post_event(d);
    (void)manager(d);
    ui_int(d, KEY_ALLOW_RESTART, 1);
    SU32(event, 0x30, 1);
}

/* ---- vf18 005aedd0 (thiscall; RET 0) -> the riders of the event: the event type's and the vehicle class's limit, the smaller,
 * doubled for the two heats (sub-event 0) ------------------------------------------------------------------------------------- */
uint32_t rx_career_heat_capacity(rx_addr event, const rx_career_single_dependencies* d) {
    ACT_FRAME(F, 0, 2);                                                   /* push esi; push edi */
    int32_t limit;
    ACT_CALLR(limit, 1, (int32_t)rx_career_event_rider_limit(U32(event, 0x1c), d));   /* 005a1330 */
    int32_t class_limit;
    ACT_CALLR(class_limit, 1, (int32_t)rx_career_class_rider_limit(-1));            /* 005a1290 */
    int32_t riders = class_limit < limit ? class_limit : limit;
    if (U32(event, 4) == 0) riders *= 2;
    ACT_END(F);
    return (uint32_t)riders;
}

/* ---- the slots ----------------------------------------------------------------------------------------------------------------- */
uint32_t rx_career_heat_event_call(rx_addr event, uint32_t slot, uint32_t argument, const rx_career_single_dependencies* d) {
    if (U32(event, 0) != 0x0094e28c) act_unreached(0x005b6950, "not the original Heat event");
    switch (slot) {
    case 0:
    case 20: return rx_career_event_current_activity_type(event);         /* 005ad830 */
    case 1: return rx_career_heat_player_state(argument);
    case 2: rx_career_heat_holeshot(event, argument, d); break;
    case 3: return rx_career_event_scalar_delete(event, argument);        /* 005a8790 */
    case 4: rx_career_event_initialize(event); break;                      /* 005a8850 */
    case 5: rx_career_moto_reset(event, d); break;                         /* 005ac8f0 (shared with Freestyle / Moto) */
    case 6: rx_career_event_uninitialize(event, d); break;                 /* 005a8900 */
    case 7: rx_career_heat_bind(event, argument, 1, d); break;
    case 8: rx_career_heat_bind(event, argument, 0, d); break;
    case 9: rx_career_event_gate(event, d); break;                         /* 005a8a10 */
    case 10: rx_career_heat_gate_result(event, argument, d); break;
    case 11: rx_career_event_post_result(event, d); break;                 /* 005a9f40 */
    case 12: rx_career_heat_advance(event, d); break;
    case 13: rx_career_heat_post_event(event, d); break;
    case 14: rx_career_single_set_holeshot_player(event, argument); break; /* 005b0490 */
    case 15: SU32(event, 0x1c, argument); break;                           /* 0088cd60 */
    case 16: SU32(event, 8, argument); break;                              /* 008ba4a0 */
    case 17: return U32(event, 0xec);                                      /* 00598630 */
    case 18: return rx_career_heat_capacity(event, d);
    case 19: return 1;                                                     /* 00794f70 */
    case 21: return 0;                                                     /* 007a0760 */
    case 22: rx_career_event_result_sort_columns(argument); break;        /* 005aafc0 */
    case 23: rx_career_event_prepare(event, d); break;                     /* 005ab3e0 */
    default: act_unreached(0x005b6950, "Heat slot requires its typed arity");
    }
    return 0;
}

/* the registered slots (thiscall: the core signature (this, stack arguments)) */
extern const rx_career_single_dependencies* rx_career_event_dependencies(void);
static uint32_t heat_state(rx_addr event, uint32_t phase) { (void)event; return rx_career_heat_player_state(phase); }
static void heat_holeshot(rx_addr event, uint32_t enable) { rx_career_heat_holeshot(event, enable, rx_career_event_dependencies()); }
static void heat_bind(rx_addr event, rx_addr channel) { rx_career_heat_bind(event, channel, 1, rx_career_event_dependencies()); }
static void heat_unbind(rx_addr event, rx_addr channel) { rx_career_heat_bind(event, channel, 0, rx_career_event_dependencies()); }
static void heat_gate_result(rx_addr event, uint32_t practice) { rx_career_heat_gate_result(event, practice, rx_career_event_dependencies()); }
static void heat_advance(rx_addr event) { rx_career_heat_advance(event, rx_career_event_dependencies()); }
static void heat_post(rx_addr event) { rx_career_heat_post_event(event, rx_career_event_dependencies()); }
static uint32_t heat_capacity(rx_addr event) { return rx_career_heat_capacity(event, rx_career_event_dependencies()); }
RX_FUNCTION(0x005ad840, heat_state)
RX_FUNCTION(0x005aee00, heat_holeshot)
RX_FUNCTION(0x005ad880, heat_bind)
RX_FUNCTION(0x005ad8f0, heat_unbind)
RX_FUNCTION(0x005ad960, heat_gate_result)
RX_FUNCTION(0x005adb00, heat_advance)
RX_FUNCTION(0x005adde0, heat_post)
RX_FUNCTION(0x005aedd0, heat_capacity)
