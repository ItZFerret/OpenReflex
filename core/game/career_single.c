/* Single-event semantic checkpoint. No registration/activation until the named
 * base/manager dependencies are implemented and genuine-original proof passes.
 * Full005affb0 through finalRET005b03d5 was read, not the truncated1055B listing.
 * Table/index/string/driver math uses existing canonical owners once. */
#include "career_single.h"
#include "career_single_events.h"
#include "build_activity_util.h"
#include "race_activity.h"
#include <stdlib.h>
#include "rider_cap.h"

static void need(int available, uint32_t target) {
    if (!available) act_unreached(target, "unclosed career Single dependency");
}
static rx_addr manager(const rx_career_single_dependencies* d) {
    need(d && d->manager, 0x00413d20); return d->manager();
}
static rx_addr player_table(const rx_career_single_dependencies* d) {
    rx_addr m = manager(d); need(d->player_table != 0, 0x005b6840);
    return d->player_table(m, 2);
}
static void ui_string(const rx_career_single_dependencies* d, rx_addr key, rx_addr value) {
    need(d->ui_string != 0, 0x005dbf50); d->ui_string(key, value);
}
static void ui_int(const rx_career_single_dependencies* d, rx_addr key, uint32_t value) {
    need(d->ui_int != 0, 0x005dbfb0); d->ui_int(key, value);
}

uint32_t rx_career_single_player_state(uint32_t phase) {
    return phase == 1 ? 4u : phase == 5 ? 8u : 0u;
}

/*005ab030 searches the supplied rowset in its real ordered iteration order.
 * Player identity is type plus case-insensitive name, never driver ordinal. */
int32_t rx_career_event_driver_row(rx_addr driver, rx_addr rows, rx_addr ordinal_out) {
    const rx_addr table = U32(rows, 0x24), proxy = U32(rows, 0);
    rx_addr node = U32(U32(rows, 0x18), 0);
    uint32_t ordinal = 0;
    for (;;) {
        if (!proxy || proxy != U32(rows, 0)) act_unreached(0x005ab030, "invalid iterator proxy");
        if (node == U32(rows, 0x18)) return -1;
        if (node == U32(U32(proxy, 0), 0x18)) act_unreached(0x005ab030, "end iterator dereference");
        const uint32_t row = U32(node, 0xc);
        uint32_t col = rx_tbl_column_index(table, 0x00936ef4); /*PlayerType*/
        rx_addr cell = rx_tbl_cell_word(table, col, row);
        const uint32_t type = U32(cell, 0);
        rx_addr column = act_column(table, 0x00936f00);
        const rx_addr name = rx_tbl_cell_string(column, row);
        const rx_addr text = U32(name, 0x18) < 0x10 ? name + 4 : U32(name, 4);
        const rx_addr player = U32(driver, 0x74);
        if (U32(player, 0x10) == type && crt_stricmp(player + 0x14, text) == 0) {
            if (ordinal_out) SU32(ordinal_out, 0, ordinal);
            return (int32_t)U32(node, 0xc);
        }
        node = rx_tbl_rowset_next(node); ++ordinal;
    }
}

void rx_career_single_reset(rx_addr event, const rx_career_single_dependencies* d) {
    rx_career_event_base_reset(event, d);
    SU32(event, 0xc, 5); SU32(event, 0x10, 5);
}

void rx_career_single_advance(rx_addr event, const rx_career_single_dependencies* d) {
    (void)player_table(d); /*original performs the getter even when no transition*/
    if (U32(event, 0x10) == 1) {
        if (U32(event, 4) == 0) return;
        ui_string(d, 0x0094c99c, 0x0094c128); /*GateSelection, actual005aff87 push*/
        const uint32_t arg = 0;
        act_vcall(event, 0x28, 1, &arg);
        return;
    }
    if (U32(event, 0x10) == 5) {
        const uint32_t arg = 1;
        act_vcall(event, 8, 1, &arg);
        rx_career_event_setup_activity(event, 8, U32(event, 0x20), U32(event, 0xb4), U32(event, 0xb0), d);
    }
}

void rx_career_single_post_event(rx_addr event, const rx_career_single_dependencies* d) {
    ACT_FRAME_EBP(B, 8, 0x18, 4);
    const rx_addr scratch = B + 0x18;
    rx_addr table = player_table(d);
    if (U32(event, 4) == 0 && U32(event, 0xc) == 1) {
        const int32_t count = I32(U32(event, 0x14), 0x26c);
        const rx_addr rows = rx_tbl_rows(act_column(table, 0x0094d650), 4);
        for (int32_t i = 0; i < count; ++i) {
            const rx_addr driver = rx_stand(U32(event, 0x14), (uint32_t)i);
            int32_t row = rx_career_event_driver_row(driver, rows, 0);
            if (row < 0) continue;
            SF32(scratch, 0, (float)i);
            const rx_addr race_driver = crt_rtdynamiccast(driver, 0, 0x009cd798, 0x009cd6ec, 0);
            if (race_driver) SU32(scratch, 0, U32(race_driver, 0x10c4));
            rx_tbl_set_cell_float(table, 0x0094d6cc, (uint32_t)row, scratch);
            SU32(B, 0x20, 8);
            rx_tbl_set_cell_int(table, 0x0094d670, (uint32_t)row, B + 0x20);
        }
        SU32(event, 0x28, 1); SU32(event, 0x10, 5);
        rx_career_event_gate(event, d);
        ACT_END(B); return;
    }
    if (U32(event, 0xc) != 5) { ACT_END(B); return; }
    rx_addr routing = 0x0094d7a4; /*GotoPostEvent*/
    if (U32(event, 8) == 7) {
        rx_addr driver = rx_activity_driver_of_kind(U32(event, 0x14), 0, 0);
        driver = crt_rtdynamiccast(driver, 0, 0x009cd798, 0x009cd6ec, 0);
        const int main_event = driver && U32(driver, 0x1128) == 0;
        routing = 0x0094d7c0; /*GotoPostEventMenuPage*/
        ui_string(d, 0x0094c99c, main_event ? 0x0094c118 : 0x0094beac);
        uint32_t next = 0;
        if (main_event) next = U32(manager(d), 0x188) == 0;
        ui_int(d, 0x0094ca38, next);
    } else {
        const int32_t count = I32(U32(event, 0x14), 0x26c);
        const uint32_t phase = 5;
        const uint32_t state = act_vcall(event, 4, 1, &phase);
        const rx_addr rows = rx_tbl_rows(act_column(table, 0x0094d650), (int32_t)state);
        for (int32_t i = 0; i < count; ++i) {
            int32_t row = rx_career_event_driver_row(rx_stand(U32(event, 0x14), (uint32_t)i), rows, 0);
            if (row < 0) continue;
            SU32(scratch, 0, (uint32_t)i);
            rx_tbl_set_cell_int(table, 0x0094d78c, (uint32_t)row, scratch); /*ScoreOne*/
            rx_tbl_set_cell_int(table, 0x0094d798, (uint32_t)row, scratch); /*ScoreTwo*/
            SF32(B, 0x20, (float)i);
            rx_tbl_set_cell_float(table, 0x0094d6cc, (uint32_t)row, B + 0x20);
        }
        ui_string(d, 0x0094c99c, 0x0094c118); /*MainEvent*/
        (void)manager(d);
        ui_int(d, 0x0094ca38, U32(0, 0x009e2310) == 0);
    }
    need(d->refresh_standings != 0, 0x005b43c0);
    d->refresh_standings(manager(d));
    (void)manager(d);
    const int32_t source_series = (U32(0, 0x009e2318) != 1) - 2;
    need(d->standings != 0, 0x005b55d0);
    d->standings(manager(d), -3, source_series, 1);
    ui_string(d, 0x0094cc74, routing);
    need(d->ui_goto_hub != 0, 0x005db500); d->ui_goto_hub();
    (void)manager(d);
    ui_int(d, 0x0094d9e4, 1);
    SU32(event, 0x30, 1);
    ACT_END(B);
}

/*005a87b0 plain event reset, including all16 sentinel slots and12 gate slots. */
void rx_career_event_reset_fields(rx_addr event) {
    SU32(event, 0x14, 0);
    for (uint32_t off = 0x70; off <= 0xac; off += 4) SU32(event, off, 0x7fffffffu);
    for (uint32_t off = 0x40; off <= 0x6c; off += 4) SU32(event, off, 0xffffffffu);
    SU32(event, 0x34, 0); SU32(event, 0x38, 0); SU32(event, 0x3c, 0);
    SU32(event, 0xe8, 0xffffffffu);
    SU32(event, 0x28, 0); SU32(event, 0x24, 0); SU32(event, 0x30, 0);
}

/*005afd00 actual gate result. Nonzero is the original practice setup path;
 * zero resets local-player sort values before applying/requesting state4→8. */
void rx_career_single_gate_result(rx_addr event, uint32_t value, const rx_career_single_dependencies* d) {
    ACT_FRAME_EBP(B, 8, 0x1c, 3);
    if (value) {
        SU32(event, 0x10, 1);
        rx_career_event_setup_activity(event, 4, 1, 3, U32(event, 0xb0), d);
        ACT_END(B); return;
    }
    if (U32(event, 4) == 0) {
        const rx_addr table = player_table(d);
        const rx_addr rows = rx_tbl_rows(act_column(table, 0x00936ef4), 0);
        rx_addr node = U32(U32(rows, 0x18), 0);
        while (node != U32(rows, 0x18)) {
            const uint32_t row = U32(node, 0xc);
            const uint32_t col = rx_tbl_column_index(table, 0x0094d6cc);
            const rx_addr old = rx_tbl_cell_word(table, col, row);
            SU32(B, 0xc, U32(0, 0x00977494));
            /*Original UCOMISS: unordered also increments table revision. */
            if (!(F32(old, 0) == F32(B, 0xc))) SU32(table, 0x68, U32(table, 0x68) + 1);
            need(col < ((U32(table, 0x14) - U32(table, 0x10)) >> 2), 0x00916224);
            rx_tbl_set_float(U32(U32(table, 0x10), 4 * col), row, B + 0xc);
            node = rx_tbl_rowset_next(node);
        }
    }
    SU32(event, 0x28, 1);
    need(d->apply_player_states != 0, 0x005b39a0);
    ACT_CALL(0, d->apply_player_states(manager(d)));
    need(d->request_player_states != 0, 0x005b3890);
    d->request_player_states(manager(d), 4, 8);
    SU32(event, 0x10, 5);
    rx_career_event_gate(event, d);
    ACT_END(B);
}

/*Single vf7/vf8. Callback ordering is the authored subscription ordering. */
void rx_career_single_bind(rx_addr event, rx_addr channel, uint32_t attach, const rx_career_single_dependencies* d) {
    (void)d;
    rx_career_practice_binding(channel, event, attach);
    rx_career_advance_binding(channel, event, 0x0094d6fc, 0x00592ab0, attach);
    rx_career_advance_binding(channel, event, 0x0094d71c, 0x00592b20, attach);
    rx_career_event_binding(channel, event, 0x0094d740, 0x005a9a30, attach);
    rx_career_event_binding(channel, event, 0x0094d768, 0x00592bc0, attach);
    rx_career_base_binding(event, channel, attach);
}

void rx_career_single_holeshot(rx_addr event, uint32_t enable, const rx_career_single_dependencies* d) {
    if (enable) {
        if (U32(event, 0xf0) != 0) return;
        const rx_addr m = manager(d);
        rx_career_holeshot_binding(U32(m, 0x180), event, 1);
        SU32(event, 0xf0, 1);
    } else {
        if (U32(event, 0xf0) == 0) return;
        const rx_addr m = manager(d);
        rx_career_holeshot_binding(U32(m, 0x180), event, 0);
        SU32(event, 0xf0, 0); SU32(event, 0xec, 0);
    }
}

/*005aac00: actual conjunction on PlayerType and MXProfileName(type0) or
 * PlayerName(other types); canonical first-in-index-order query, not a guessed
 * sequential player index or a new Player List row. */
uint32_t rx_career_find_player_row(rx_addr name, uint32_t type) {
    /* 005aac00 (cdecl name, type). Frame: sub esp, 0xe0; push ebx / esi / edi: the criteria vector +0x10, the found row +0x28, the
     * criterion +0x38; each callee at the original's depth (004175e0 RET 4, 004f6c60 RET 8, 004ed410 RET 0xc). */
    ACT_FRAME(B, 0xe0, 3);
    const rx_addr vec = B + 0x10, out = B + 0x28, crit = B + 0x38;
    rx_tbl_crit_vec_init(vec);
    rx_tbl_crit_init_int(crit, 0x00936ef4, 0, type);                    /* 004f6bf0 (PlayerType) */
    ACT_CALL(1, rx_tbl_crit_push(vec, crit));
    rx_tbl_crit_destroy(crit);
    ACT_CALL(2, rx_tbl_crit_init_string(crit, type == 0 ? 0x0093deb8 : 0x00936f00, 0, name));   /* MXProfileName / PlayerName */
    ACT_CALL(1, rx_tbl_crit_push(vec, crit));
    rx_tbl_crit_destroy(crit);
    SU32(out, 0, 0xffffffffu);
    ACT_CALL(3, rx_tbl_find_first(ACT_PLAYER_LIST, vec, out));
    const uint32_t row = U32(out, 0);
    rx_tbl_crit_vec_destroy(vec);
    ACT_END(B);
    return row;
}

/*005a9290: series-selected player rows and authored activity, never exhibition
 * setup under a career label. TrackId comes from the owned real UI property
 * boundary. Unavailable property bindings must fail closed in composition. */
void rx_career_event_setup_activity(rx_addr event, uint32_t requested, uint32_t mode, uint32_t length, uint32_t limit_kind, const rx_career_single_dependencies* d) {
    /* Frame: sub esp, 0x10; push ebx / ebp / esi / edi. The stack arguments (event +0x24, requested +0x28, mode +0x2c, length +0x30,
     * limit kind +0x34) are reused: the Duration value goes to the mode's slot, the PlayerState key to the length's. Locals: +0x14 the
     * TrackId, +0x18 the laps. Every callee at the original's depth. */
    ACT_FRAME(B, 0x10, 4);
    need(d && d->apply_player_states, 0x005b39a0);
    ACT_CALL(0, d->apply_player_states(manager(d)));                    /* 005b39a0 (EDX = manager) */
    SU32(event, 0x14, 0);
    SU32(event, 0xc, U32(event, 0x10));
    SU32(event, 0x10, 8);
    ACT_CALL(1, rx_tbl_table_clear_rows(ACT_ACTIVITY_TBL));             /* 004ecdb0 */
    ACT_CALL(0, rx_game_setup_activity_area(U32(0, 0x00a946b8)));       /* Game_SetupActivityArea 0075c430 (ESI = game) */
    need(d->ui_get_int != 0, 0x005dc0c0);
    SU32(B, 0x14, 0);
    rx_set_sp(B - 8);                                                    /* UIProperty_GetInt's two pushes */
    const uint32_t area = d->ui_get_int(0x00936e40, 0); /*TrackId*/
    rx_set_sp(B);
    SU32(B, 0x14, area);
    const uint32_t activity_type = act_vcall(event, 0, 0, 0);            /* vf0: the activity type */
    SU32(event, 0x18, activity_type);
    ACT_CALL(4, (void)rx_activity_table_add_row(ACT_ACTIVITY_TBL, activity_type, area, mode));   /* ActivityTable_AddRow 004dedc0 */
    int32_t row;
    ACT_CALLR(row, 1, rx_activity_find_row(ACT_ACTIVITY_TBL, U32(event, 0x18), (int8_t)area));   /* 004df0f0 */
    uint32_t laps = 0;
    float duration = 0.0f;
    if (limit_kind == 0) laps = length;
    else if (limit_kind == 1) duration = (float)(int32_t)length * F32(0, 0x00931da0);
    SU32(B, 0x18, laps);
    {   /* NumLaps: the inline Table_SetCellInt (a change counts in the activity table's revision 0x00ae1b80), 004f5330 */
        const uint32_t column = rx_tbl_column_index(ACT_ACTIVITY_TBL, 0x0093dd64);
        if (laps != U32(rx_tbl_cell_word(ACT_ACTIVITY_TBL, column, (uint32_t)row), 0)) SU32(0, 0x00ae1b80, U32(0, 0x00ae1b80) + 1);
        if (column >= ((U32(ACT_ACTIVITY_TBL, 0x14) - U32(ACT_ACTIVITY_TBL, 0x10)) >> 2)) crt_invalid_parameter_noinfo();
        ACT_CALL(3, rx_tbl_set_int(U32(U32(ACT_ACTIVITY_TBL, 0x10), 4 * column), (uint32_t)row, B + 0x18));
    }
    SF32(B, 0x2c, duration);
    {   /* Duration (ucomiss: unordered counts too), 004f5650 */
        const uint32_t column = rx_tbl_column_index(ACT_ACTIVITY_TBL, 0x0093dd58);
        if (!(duration == F32(rx_tbl_cell_word(ACT_ACTIVITY_TBL, column, (uint32_t)row), 0))) SU32(0, 0x00ae1b80, U32(0, 0x00ae1b80) + 1);
        if (column >= ((U32(ACT_ACTIVITY_TBL, 0x14) - U32(ACT_ACTIVITY_TBL, 0x10)) >> 2)) crt_invalid_parameter_noinfo();
        ACT_CALL(3, rx_tbl_set_float(U32(U32(ACT_ACTIVITY_TBL, 0x10), 4 * column), (uint32_t)row, B + 0x2c));
    }
    const rx_addr table = player_table(d);
    SU32(B, 0x30, requested);
    rx_addr rows;
    ACT_CALLR(rows, 1, rx_tbl_rows(act_column(table, 0x0094d650), (int32_t)requested));   /* 00418e50 (EAX = &key) */
    rx_addr node = U32(U32(rows, 0x18), 0);
    while (node != U32(rows, 0x18)) {
        const uint32_t player_row = U32(node, 0xc);
        uint32_t col = rx_tbl_column_index(table, 0x00936ef4);
        const uint32_t type = U32(rx_tbl_cell_word(table, col, player_row), 0);
        SU32(B, 0x30, type);
        const rx_addr string = rx_tbl_cell_string(act_column(table, 0x00936f00), player_row);
        const rx_addr name = U32(string, 0x18) < 0x10 ? string + 4 : U32(string, 4);
        uint32_t target;
        ACT_CALLR(target, 2, rx_career_find_player_row(name, type));      /* 005aac00 (cdecl name, type) */
        ACT_CALL(2, rx_player_list_request_activity(target, U32(event, 0x18), area));   /* 004dde70 (ESI = row; type, area) */
        node = rx_tbl_rowset_next(node);
    }
    ACT_CALL(0, rx_game_setup_activity_area(U32(0, 0x00a946b8)));
    ACT_END(B);
}

/*005ac5d0: actual actor gate-selection reset; native v49 must be available
 * for each genuine vehicle subtype before this branch can be activated. */
void rx_career_gate_reset_actors(void) {
    ACT_FRAME(B, 4, 4);
    for (uint32_t i = 0; i < U8(U32(0, 0x00a946b8), 0x1541c); ++i) {
        const rx_addr player = rx_player_active(U32(0, 0x00a946b8) + 0x7460, i, 3);
        rx_addr vehicle = U32(player, 0x1ac);
        if (vehicle) {
            SU8(U32(vehicle, 0x1328), 0x14e, 0);
            const uint32_t zero = 0;
            act_vcall(vehicle, 0xc4, 1, &zero);
            vehicle = U32(player, 0x1ac);
            const rx_addr link = U32(vehicle, 0x17c);
            if (link) {
                const rx_addr owner = U32(link, 4);
                const uint32_t index = U32(link, 8);
                const rx_addr begin = U32(owner, 0x1960), end = U32(owner, 0x1964);
                need(index < (end - begin) / 0xe0u, 0x00916224);
                SU32(U32(begin + index * 0xe0, 0x2c), 0x14, 0xffffffffu);
            }
        }
        const rx_addr rider = U32(player, 0x1b4);
        if (rider) {
            SU32(rider, 0x5bb5c, 0); SU8(rider, 0xa2, 0);
            SU8(rider, 0x5bb64, 0); SU32(rider, 0x5b6d0, 0);
        }
    }
    ACT_END(B);
}

void rx_career_event_gate(rx_addr event, const rx_career_single_dependencies* d) {
    /* 005a8a10 (thiscall; RET 0). Frame: push ebp; and esp, -8; push ecx (the GotoHub payload slot); push ebx / esi / edi. Every
     * callee at the original's depth (w-heat). */
    ACT_FRAME_EBP(B, 8, 4, 3);
    if (U32(event, 0x28)) {
        ACT_CALL(0, rx_career_event_prepare_gate(event, d));           /* 005a9640 */
        SU32(event, 0x28, 0);
    }
    ui_string(d, 0x0094c99c, 0x0094c128); /*SeriesEventStatus=GateSelection*/
    if (act_vcall(event, 0x4c, 0, 0) == 0) {
        ACT_CALL(0, rx_career_event_process_gate(event, d));           /* 005a9a30 */
    } else {
        ACT_CALL(0, rx_career_gate_reset_actors());                    /* 005ac5d0 */
        ACT_CALL(1, rx_tbl_table_clear_rows(ACT_ACTIVITY_TBL));        /* 004ecdb0 */
        ACT_CALL(0, rx_game_setup_activity_area(U32(0, 0x00a946b8)));  /* Game_SetupActivityArea 0075c430 (ESI = game) */
        ui_int(d, 0x0094d620, 0xffffffffu); /*GateSelectionDeviceIndex*/
        ui_string(d, 0x0094cc74, 0x0094d63c); /*UIRoutingMessage=Goto GateSelection*/
        need(d->ui_goto_hub != 0, 0x005db500); d->ui_goto_hub();
    }
    ACT_END(B);
}

/*005a9a30: actual local-player turn or AI LCG gate pick. Both real dispatch
 * events and original integer-sort owner remain mandatory typed dependencies. */
void rx_career_event_process_gate(rx_addr event, const rx_career_single_dependencies* d) {
    ACT_FRAME_EBP(B, 8, 8, 4);
    const rx_addr value = B + 0x10;
    if (act_vcall(event, 0x4c, 0, 0) == 0) {
        rx_addr gate = event + 0x70;
        while (I32(event, 0x34) < I32(event, 0x38)) {
            const uint32_t start = U32(gate, 0); gate += 4;
            const uint32_t row = U32(event, 0x40 + 4 * U32(event, 0x34));
            SU32(event, 0x34, U32(event, 0x34) + 1);
            SU32(value, 0, start);
            rx_tbl_set_cell_int(ACT_PLAYER_LIST, 0x0093df00, row, value); /*StartZone actual005a9a78 push*/
        }
        SU32(event, 0x24, 0);
        rx_career_subscribe_fade(event);
        rx_career_emit_fade_out();
        ACT_END(B); return;
    }
    SU32(event, 0x24, 1);
    while (I32(event, 0x34) < I32(event, 0x38)) {
        const uint32_t row = U32(event, 0x40 + 4 * U32(event, 0x34));
        if (act_cell(ACT_PLAYER_LIST, 0x00936ef4, row) == 0) {
            (void)act_cell(ACT_PLAYER_LIST, 0x0093de90, row); /*PlayerLocal originalread retained*/
            ui_int(d, 0x0094d620, act_cell(ACT_PLAYER_LIST, 0x0093ded0, row));
            ACT_END(B); return;
        }
        uint32_t index, start;
        do {
            const uint32_t seed = U32(0, 0x009cb860) * 0x41c64e6du + 0x3039u;
            const int32_t remaining = I32(event, 0x3c);
            need(remaining > 0, 0x005a9bb6); /*original IDIV domain, never invented gate*/
            index = ((seed >> 16) & 0x7fffu) % (uint32_t)remaining;
            SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
            SU32(0, 0x009cb860, seed);
            start = U32(event, 0x70 + 4 * index);
        } while (start == 0xffffffffu || start == 0x7fffffffu);
        SU32(value, 0, start);
        rx_tbl_set_cell_int(ACT_PLAYER_LIST, 0x0093df00, row, value);
        SU32(event, 0x70 + 4 * index, 0x7fffffffu);
        SU32(event, 0x3c, U32(event, 0x3c) - 1);
        rx_career_sort_gate_values(event + 0x70, event + 0xb0, 16);
        SU32(event, 0x34, U32(event, 0x34) + 1);
    }
    ui_int(d, 0x0094d620, 0xffffffffu);
    SU32(event, 0x24, 0);
    ACT_END(B);
}

/*005b0590: original compares second SortValue > first SortValue (COMISS/JBE).
 * Unordered is false; no ordinal tie breaker is introduced. */
uint32_t rx_career_gate_row_less(uint32_t first, uint32_t second, const rx_career_single_dependencies* d) {
    const rx_addr table = player_table(d);
    uint32_t column = rx_tbl_column_index(table, 0x0094d6cc);
    const float a = F32(rx_tbl_cell_word(table, column, first), 0);
    column = rx_tbl_column_index(table, 0x0094d6cc);
    const float b = F32(rx_tbl_cell_word(table, column, second), 0);
    return b > a;
}

static uint32_t collect_gate_rows(rx_addr table, uint32_t state, rx_addr output, uint32_t count) {
    const rx_addr rows = rx_tbl_rows(act_column(table, 0x0094d650), (int32_t)state);
    rx_addr node = U32(U32(rows, 0x18), 0);
    while (node != U32(rows, 0x18)) {
        SU32(output, 4 * count, U32(node, 0xc)); ++count;
        node = rx_tbl_rowset_next(node);
    }
    return count;
}

void rx_career_event_prepare_gate(rx_addr event, const rx_career_single_dependencies* d) {
    ACT_FRAME_EBP(B, 8, 0x154, 3);
    need(d && d->apply_player_states, 0x005b39a0);
    ACT_CALL(0, d->apply_player_states(manager(d)));
    SU32(event, 0x3c, 16); SU32(event, 0x38, 0); SU32(event, 0x34, 0);
    for (uint32_t i = 0; i < 16; ++i) SU32(event, 0x70 + 4 * i, i);
    const int32_t player_count = (int32_t)rx_tbl_row_count(ACT_PLAYER_LIST);
    SU32(B, 0x1c, 0xffffffffu);
    for (int32_t i = 0; i < player_count; ++i)
        rx_tbl_set_cell_int(ACT_PLAYER_LIST, 0x0093df00, (uint32_t)i, B + 0x1c);
    const uint32_t phase = U32(event, 0x10);
    const uint32_t state = act_vcall(event, 4, 1, &phase);
    const rx_addr output = B + 0x28, name = B + 0x58;
    SU32(output, 0, 0xffffffffu);
    for (uint32_t i = 4; i < 0x30; ++i) SU8(output, i, 0);
    const rx_addr m = manager(d);
    const uint32_t controller = U32(m, 0x1c);
    for (uint32_t i = 0; i < 0x100; ++i) SU8(name, i, 0);
    SU32(B, 0x10, U32(0, 0x009d23bc)); /*SeriesEventPlayers*/
    SU32(B, 0x14, U32(0x009d2374, 4 * U32(0, 0x009e2318)));
    rx_format_buffer(name, 0x100, 0x0094d9d8, B + 0x10); /*Player%s%s*/
    const rx_addr table = rx_tbl_find(0x00ae10f0 + controller * 0x1f4, name);
    uint32_t count = collect_gate_rows(table, state, output, 0);
    if (state == 6 || state == 9) count = collect_gate_rows(table, state + 1, output, count);
    /*The exact native005c1980 algorithm is a required composition owner. No
     * host sort or invented ordering substitutes for it in this checkpoint. */
    rx_career_sort_gate_rows(output, output + count * 4, (int32_t)count, d);
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t end = U32(event, 0x38);
        SU32(event, 0x40 + end * 4, U32(output, i * 4));
        SU32(event, 0x38, end + 1);
    }
    ACT_END(B);
}

void rx_career_event_base_reset(rx_addr event, const rx_career_single_dependencies* d) {
    const rx_addr m = manager(d);
    need(d->seed_standings != 0, 0x005b4d30);
    d->seed_standings(m, -3);
    SU32(event, 0xc, 0); SU32(event, 0x10, 1);
    rx_career_event_reset_fields(event);
    const uint32_t zero = 0;
    act_vcall(event, 8, 1, &zero);
}

void rx_career_event_initialize(rx_addr event) {
    SU32(event, 0x2c, 1); SU32(event, 0x18, 0x3d);
    act_vcall(event, 0x14, 0, 0);
    for (uint32_t off = 0xb8; off <= 0xe4; off += 4) SU32(event, off, 0);
}

uint32_t rx_career_event_current_activity_type(rx_addr event) { return U32(event, 0x1c); }
void rx_career_single_set_holeshot_player(rx_addr event, rx_addr player) { SU32(event, 0xec, player); }
void rx_career_single_set_type(rx_addr event, uint32_t type) {
    SU32(event, 0x1c, type);
    SU32(event, 0xb0, type == 5 || type == 12);
    SU32(event, 0xb4, 3);
}
void rx_career_single_set_format(rx_addr event, uint32_t format) {
    SU32(event, 8, format); SU32(event, 0x20, 1);
    if (format == 4) SU32(event, 0x20, 6);
    else if (format == 5) SU32(event, 0x20, 5);
    else if (format == 6) SU32(event, 0x20, 7);
    else if (format == 7) SU32(event, 0x20, 8);
}
void rx_career_event_uninitialize(rx_addr event, const rx_career_single_dependencies* d) {
    SU32(event, 0x2c, 0);
    rx_career_event_reset_fields(event);
    const rx_addr m = manager(d);
    need(d->clear_standings != 0, 0x005b6370); d->clear_standings(m, -3);
    SU32(event, 8, 10);
}
