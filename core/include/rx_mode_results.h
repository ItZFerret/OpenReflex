#ifndef RX_MODE_RESULTS_H
#define RX_MODE_RESULTS_H
#include "rx_api.h"
#ifdef __cplusplus
extern "C" {
#endif
/* The values the original's results table fillers read for the modes whose table the product does not run (w-pausemodes, 2026-10-27):
 * Waypoint (00615450), Freestyle (00614bf0) and Section Race (006152e0). Since w-wpfill the product runs those fillers too, where the
 * original's results page starts, and rx_session_results_page (rx_results.h) carries their cells (the presenter prefers them once the
 * page started); this query stays for the live values before that (a pause page's tiles) and the full trick key. It gives the source
 * values read-only from the standings (activity + 0x2d0, count + 0x26c,
 * the order the original's table walks), at any time between advances: live for a pause page's score tiles, final once the activity
 * is in Post. Copies only, no call, no draw; product only (the oracle build gets a stub).
 *
 * Per standings driver D (player P = D + 0x74): slot (the game's player slot), local, place (D + 0x78 + 1), finished (D + 0x70),
 * total_time D + 0x134 (the driver's clock, stops at its finish), best_lap = the Waypoint driver's D + 0x10b0.
 *   Freestyle (A + 0x23c == 5): score = P + 0xcec ("TotalScore", "%3.2f"), best_trick_score = 0050de10's largest of P + 0xc84 / 0xc94 /
 *     0xc88 / 0xc8c / 0xc90 when P + 0x10a0 (the best trick record) is set (else has_best_trick 0), best_trick = the record's LocString key
 *     (record + 0x40: "STR_TWISTER"), localized by the presenter (MXRaven_Stunts).
 *   Section Race (== 12): sections_won = D + 0x1134 (the "Sections" column), best_section / best_section_time = 005133f0 (the lowest of
 *     the D + 0x1144 times, D + 0x1138 of them: index -1 and FLT_MAX with none).
 * header: activity_type (A + 0x23c), activity_mode (A + 0x238), race_state (A + 0x21c), section_count (A + 0x13e650, Section Race). Every
 * field is 0 / -1 outside the kind it belongs to. */
#define RX_MODE_RESULTS_VERSION 1u
#define RX_MODE_RESULTS_ROWS 12
#define RX_MODE_RESULTS_KEY 48
typedef struct rx_mode_result_row {
    int32_t slot, local, place, finished;
    float total_time, best_lap;
    float score, best_trick_score;
    int32_t sections_won, best_section;
    float best_section_time;
    int32_t has_best_trick;
    char best_trick[RX_MODE_RESULTS_KEY];
} rx_mode_result_row;                                   /* 96 bytes */
typedef struct rx_mode_results {
    uint32_t size, version, valid;
    int32_t activity_type, activity_mode, race_state, section_count;
    uint32_t count;
    int32_t reserved[4];
    rx_mode_result_row rows[RX_MODE_RESULTS_ROWS];
} rx_mode_results;                                      /* 48 + 12 * 96 = 1200 bytes */
/* Caller sets size and version (1). 1 on bad arguments (output untouched), else 0 (valid 0: the local player has no activity). Session owner thread, between advances. */
RX_API int rx_session_mode_results(struct rx_session* session, rx_mode_results* out);
#ifdef __cplusplus
}
#endif
#endif
