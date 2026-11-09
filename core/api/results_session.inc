/* results_session.inc -- the race results page's table (include/rx_results.h; w-resultsrng), included by rx_session.c. Copies only:
 * the game's table "Activity Results" (0x00ae18c0) as App.PopulateResultsTable left it (game/race_results_table.c). */
#include "rx_results.h"
#include "../game/race_results_table.h"
#include "../game/build_tables.h"
#include "../game/build_vehicle.h"

/* the column of the table by name (the host-side compare of the column objects' names, +0 char[0x20]); -1 absent */
static int32_t results_column(rx_addr t, const char* name) {
    const uint32_t n = (U32(t, 0x14) - U32(t, 0x10)) >> 2;
    for (uint32_t c = 0; c < n && c < 64; c++) {
        const rx_addr col = U32(U32(t, 0x10), 4 * c);
        uint32_t i = 0;
        for (; i < 0x1f && name[i] && U8(col, i) == (uint8_t)name[i]; i++) {}
        if (!name[i] && U8(col, i) == 0) return (int32_t)c;
    }
    return -1;
}
static void results_string(char* out, uint32_t cap, rx_addr t, int32_t c, uint32_t row) {
    out[0] = 0;
    if (c < 0) return;
    const rx_addr s = rx_tbl_cell_string(U32(U32(t, 0x10), 4u * (uint32_t)c), row);
    const rx_addr p = rx_tbl_str_cstr(s);
    const uint32_t n = U32(s, 0x14);
    uint32_t i = 0;
    for (; i < n && i + 1 < cap; i++) out[i] = (char)U8(p, i);
    out[i] = 0;
}

int rx_session_results_page(struct rx_session* s, rx_results_page* out) {
    const int v1 = out && out->version == 1u && out->size == RX_RESULTS_V1_SIZE;   /* (w-rc-core) the 16-row layout of version 1 */
    if (!session_ok(s) || !out || (!v1 && (out->size != sizeof *out || out->version != RX_RESULTS_VERSION))) return 1;
    rx_results_page v;
    memset(&v, 0, sizeof v);
    v.size = sizeof v; v.version = RX_RESULTS_VERSION; v.activity_type = -1;
    rx_addr act = 0;
    v.populated = rx_results_page_populated(&act);
    if (v.populated && act) {
        const rx_addr t = 0x00ae18c0;   /* "Activity Results" */
        v.activity_type = I32(act, 0x23c);
        const int32_t ci = results_column(t, "Index"), cn = results_column(t, "Name");
        int32_t cc[3], cs[3];
        for (int k = 0; k < 3; k++) {
            char b[32];
            snprintf(b, sizeof b, "Column%d", k); cc[k] = results_column(t, b);
            snprintf(b, sizeof b, "Column%d_SortValue", k); cs[k] = results_column(t, b);
        }
        const uint32_t rows = ci >= 0 ? rx_tbl_row_count(t) : 0;
        const rx_addr local = local_player();
        for (uint32_t r = 0; r < rows && r < 64; r++) {
            const int32_t index = I32(rx_tbl_cell_word(t, (uint32_t)ci, r), 0);
            if (index < 0) {   /* row 0: the column titles (Index -1) */
                for (int k = 0; k < 3; k++) {
                    results_string(v.title[k], sizeof v.title[k], t, cc[k], r);
                    if (v.title[k][0]) v.columns = k + 1;
                }
                continue;
            }
            if (v.rows >= RX_RESULTS_MAX_ROWS) continue;
            rx_results_row* w = &v.row[v.rows++];
            w->index = index; w->slot = -1;
            if (index < I32(act, 0x26c) && index < (int32_t)RX_RC_MAX) {
                const rx_addr d = rx_stand(act, (uint32_t)index), p = d ? U32(d, 0x74) : 0;
                w->slot = rx_seat_of(s->game + 0x7460, p);
                w->local = p != 0 && p == local;
            }
            results_string(w->name, sizeof w->name, t, cn, r);
            for (int k = 0; k < 3; k++) {
                results_string(w->cell[k], sizeof w->cell[k], t, cc[k], r);
                w->sort[k] = cs[k] >= 0 ? F32(rx_tbl_cell_word(t, (uint32_t)cs[k], r), 0) : 0.0f;
            }
        }
        /* the page's order (the Lua sorts the rows by Index) */
        for (int32_t i = 1; i < v.rows; i++)
            for (int32_t j = i; j > 0 && v.row[j - 1].index > v.row[j].index; j--) { rx_results_row x = v.row[j]; v.row[j] = v.row[j - 1]; v.row[j - 1] = x; }
    }
    if (v1) {   /* the version-1 struct: the same fields, 16 rows */
        if (v.rows > RX_RESULTS_V1_ROWS) v.rows = RX_RESULTS_V1_ROWS;
        v.size = RX_RESULTS_V1_SIZE; v.version = 1u;
        memcpy(out, &v, RX_RESULTS_V1_SIZE);
        return 0;
    }
    *out = v;
    return 0;
}
