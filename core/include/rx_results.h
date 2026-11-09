#ifndef RX_RESULTS_H
#define RX_RESULTS_H
#include "rx_api.h"
#ifdef __cplusplus
extern "C" {
#endif
/* The race results page's table (agent w-resultsrng, 2026-10-27). Additive: a new query and structs, RX_SESSION_API_VERSION
 * unchanged. The original's results page (the InGame page PostEventResults; its Lua's Start calls App.PopulateResultsTable 00742c40)
 * shows the game's table "Activity Results" (0x00ae18c0): row 0 the column titles, then a row per rider in the standings order with
 * the cells Column0..2 (a race: TOTAL, PENALTY, BEST LAP, "MM:SS.cc" by the game's own formatting) and their sort values. A rider
 * still racing when the race ended has a PROJECTED total there (00613880: the gap behind the leader raised by a random 0..20 s over
 * the rider ahead; the same draw sets a best lap for a rider without one in a race of more than one lap), which no other export
 * holds. The product runs that page start itself where the original's page does (game/race_results_table.c: 14 frames after "Show
 * Activity Results", or at the next UI tick when the finish scenario had put the UI on the Event Outro page, where the UI manager's
 * update runs in Engine::Update), so the table is there from then on, whatever the presenter's page timing. Since w-wpfill every
 * offline mode's table: the race types (the Eliminator's columns ELIMINATED / TOTAL / BEST LAP: ELIMINATED is the localized ordinal
 * "%i%ls" with the place as its sort value; the product has no UI language table and writes the English text the oracle recorded,
 * "1ST" / "2ND" / "3RD" / "<n>TH" by the original's rule 1 / 2 / 3 / any other place (so 21 is "21TH"), and a presenter localizes STR_FIRST /
 * _SECOND / _THIRD / _FOURTH from the sort value), Waypoint (TOTAL TIME: a rider still racing has the original's projected total, which also becomes its gap), Freestyle
 * (TOTAL SCORE, BEST TRICK SCORE "%3.2f", BEST TRICK = the trick's LocString key) and Section Race (SECTIONS "1,250", BEST SECTION
 * "%i", BEST SECTION TIME).
 *   populated       how many times the page start ran for the current activity (0: not yet; the table is empty or a previous race's)
 *   activity_type   A+0x23c of the activity the table was filled for (-1 none)
 *   columns         the column count (row 0's titles, at most 3 here; the race types have 3)
 *   rows            the riders' rows written (at most RX_RESULTS_MAX_ROWS)
 *   title[k]        Column<k>'s title key (STR_ACTIVITYRESULTSCOLUMN_TOTALTIME / _PENALTYTIME / _BESTLAPTIME, ...)
 *   row[i]          index = the row's Index (the place in the standings, 0-based), slot = that driver's player slot (-1 unknown),
 *                   local = the local player's, name = "Name", cell[k] = Column<k> (truncated to 15 characters), sort[k] =
 *                   Column<k>_SortValue (a time's is -t, the original's descending sort; an empty cell's 0) */
#define RX_RESULTS_VERSION 2u     /* 2 (w-rc-core): 32 rows (RX_MAX_RIDERS), 4008 bytes; version 1 (16 rows, 2088 bytes) is still served */
#define RX_RESULTS_MAX_ROWS 32
#define RX_RESULTS_V1_ROWS 16
#define RX_RESULTS_V1_SIZE 2088u
typedef struct rx_results_row {
    int32_t index, slot, local;
    char name[48];
    char cell[3][16];
    float sort[3];
} rx_results_row;               /* 120 bytes */
typedef struct rx_results_page {
    uint32_t size, version;
    uint32_t populated;
    int32_t activity_type, columns, rows;
    char title[3][48];
    rx_results_row row[RX_RESULTS_MAX_ROWS];
} rx_results_page;              /* 4008 bytes (version 1: 2088, 16 rows) */
/* Caller sets size and version (2, or 1 with the version-1 size: then the first 16 rows). 1 on bad arguments (output untouched), else 0. Session owner thread, between advances. Reads only. */
RX_API int rx_session_results_page(struct rx_session* session, rx_results_page* out);
#ifdef __cplusplus
}
#endif
#endif
