/* Read-only copy of the Moto event's working player table (PlayerSeriesEventPlayersTour: the riders, their state and the two
 * motos' scores). No classification or outcome is synthesised here. */
#include "career_moto_observation.h"
#include "career_players.h"
#include "career_manager.h"
#include "build_activity_util.h"
#include "build_tables.h"
#include <string.h>

static uint32_t value(rx_addr t, rx_addr name, uint32_t row) {
    return act_cell(t, name, row);
}

int rx_career_moto_observe(rx_addr event, rx_moto_observation* out) {
    if (!out || out->size != sizeof *out || out->version != RX_MOTO_OBSERVATION_VERSION) return 1;
    /* the current event of a loaded Tour series of the Moto format (manager + 0x10 = 0: the event 005b6900 selects is + 0x288) */
    const rx_addr manager = 0x9e2188;
    if (!event || U32(manager, 0x18c) != event || event != manager + 0x288 || U32(event, 0) != 0x94e2fc) return 2;
    if (U32(manager, 0x190) != 1 || U32(manager, 0x10) != 0) return 2;
    /* the practice (1) or one of the two motos (6, 7) */
    const uint32_t phase = U32(event, 0xc);
    if (phase != 1 && phase != 6 && phase != 7) return 2;
    const rx_addr table = rx_career_player_table(manager, 2);
    if (!table || rx_tbl_row_count(table) != RX_MOTO_OBSERVATION_ROWS) return 3;
    rx_moto_observation copy = {0};
    copy.size = sizeof copy;
    copy.version = RX_MOTO_OBSERVATION_VERSION;
    copy.series_id = U32(manager, 0x20);
    copy.sub_event = U32(event, 4);
    copy.phase = phase;
    copy.next_phase = U32(event, 0x10);
    copy.post_complete = U32(event, 0x30);
    copy.row_count = RX_MOTO_OBSERVATION_ROWS;
    for (uint32_t i = 0; i < copy.row_count; i++) {
        rx_moto_score_row* r = &copy.rows[i];
        r->row = (int32_t)i;
        r->player_type = (int32_t)value(table, 0x936ef4, i);     /* PlayerType */
        r->player_state = (int32_t)value(table, 0x94d650, i);    /* PlayerState */
        r->score_one = (int32_t)value(table, 0x94d78c, i);       /* ScoreOne */
        r->score_two = (int32_t)value(table, 0x94d798, i);       /* ScoreTwo */
        const uint32_t bits = value(table, 0x94d6cc, i);         /* SortValue */
        memcpy(&r->sort_value, &bits, sizeof bits);
        const rx_addr text = rx_tbl_str_cstr(rx_tbl_cell_string(act_column(table, 0x936f00), i));   /* PlayerName */
        uint32_t j;
        for (j = 0; j < RX_MOTO_OBSERVATION_NAME; j++) {
            r->name[j] = (char)U8(text, j);
            if (!r->name[j]) break;
        }
        if (j == RX_MOTO_OBSERVATION_NAME) return 3;
    }
    *out = copy;
    return 0;
}
