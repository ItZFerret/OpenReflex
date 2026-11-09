/* mode_results.c -- the source values of the Waypoint / Freestyle / Section Race results table (include/rx_mode_results.h): the standings of
 * the local player's activity, read-only. Product only (the oracle build has no use for it). */
#include "mode_results.h"
#include "restored_champ_weekend.h"   /* RESTORED CONTENT: rx_restored_champ_base_type */
#ifndef RX_ORACLE
#include <string.h>
#include <float.h>
#include "rider_cap.h"

static float max_f(float a, float b) { return a > b ? a : b; }

/* the player's slot in the game block (player slots +0x7460, 0x12a0 each, 16) */
static int32_t slot_of(rx_addr game, rx_addr player) {
    return rx_seat_of(game + 0x7460, player);   /* (w-rc-core) seats 0..31 */
}

int rx_mode_results_fill(rx_addr game, rx_addr player, rx_mode_results* v) {
    const rx_addr A = player ? U32(player, 0x94) : 0;
    if (!A) return 0;
    v->valid = 1;
    v->activity_type = I32(A, 0x23c);
    v->activity_mode = I32(A, 0x238);
    v->race_state = I32(A, 0x21c);
    v->section_count = v->activity_type == 12 ? I32(A, 0x13e650) : -1;
    const int32_t base = rx_restored_champ_base_type(v->activity_type);   /* RESTORED CONTENT: a championship stage reads as its discipline */
    int32_t n = I32(A, 0x26c);
    if (n < 0) n = 0;
    if (n > RX_MODE_RESULTS_ROWS) n = RX_MODE_RESULTS_ROWS;   /* the standings (A + 0x2d0) hold twelve */
    uint32_t k = 0;
    for (int32_t i = 0; i < n; i++) {
        const rx_addr d = rx_stand(A, (uint32_t)i);
        if (!d) continue;
        const rx_addr p = U32(d, 0x74);
        rx_mode_result_row* r = &v->rows[k++];
        memset(r, 0, sizeof *r);
        r->slot = p ? slot_of(game, p) : -1;
        r->local = p != 0 && p == player;
        r->place = I32(d, 0x78) + 1;
        r->finished = I32(d, 0x70) != 0;
        r->total_time = F32(d, 0x134);
        r->sections_won = 0; r->best_section = -1; r->best_section_time = FLT_MAX;
        if (base == 6) r->best_lap = F32(d, 0x10b0);
        if (base == 5 && p) {   /* FUN_00614bf0 */
            r->score = F32(p, 0xcec);
            const rx_addr record = U32(p, 0x10a0);
            if (record) {
                float m = F32(p, 0xc84);
                m = max_f(m, F32(p, 0xc94)); m = max_f(m, F32(p, 0xc88)); m = max_f(m, F32(p, 0xc8c)); m = max_f(m, F32(p, 0xc90));
                r->has_best_trick = 1;
                r->best_trick_score = m;
                const rx_addr key = U32(record, 0x40);
                for (uint32_t c = 0; key && c + 1 < RX_MODE_RESULTS_KEY; c++) {
                    const uint8_t ch = U8(key, c);
                    if (!ch) break;
                    r->best_trick[c] = (char)ch;
                }
            }
        }
        if (v->activity_type == 12) {   /* FUN_006152e0 */
            r->sections_won = I32(d, 0x1134);
            const rx_addr times = U32(d, 0x1144);
            if (times) {
                float best = FLT_MAX; int32_t index = -1;
                for (int32_t s = 0; s < I32(d, 0x1138) && s < 64; s++) {
                    const float t = F32(times, 4u * (uint32_t)s);
                    if (best > t) { best = t; index = s; }
                }
                r->best_section_time = best; r->best_section = index;
            }
        }
    }
    v->count = k;
    return 1;
}
#else
int rx_mode_results_fill(rx_addr game, rx_addr player, rx_mode_results* out) { (void)game; (void)player; (void)out; return 0; }
#endif
