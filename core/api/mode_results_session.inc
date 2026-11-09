/* mode_results_session.inc -- the Waypoint / Freestyle / Section Race results' source values (include/rx_mode_results.h, game/mode_results.c),
 * included by rx_session.c after race_format_session.inc (it uses local_player() and session_ok()). Copies only. */
#include "rx_mode_results.h"
#include "../game/mode_results.h"
_Static_assert(sizeof(rx_mode_results) == 1200, "mode results ABI size");

int rx_session_mode_results(struct rx_session* s, rx_mode_results* out) {
    if (!session_ok(s) || !out || out->size != sizeof *out || out->version != RX_MODE_RESULTS_VERSION) return 1;
    rx_mode_results v;
    memset(&v, 0, sizeof v);
    v.size = sizeof v;
    v.version = RX_MODE_RESULTS_VERSION;
    v.activity_type = v.activity_mode = v.race_state = v.section_count = -1;
    (void)rx_mode_results_fill(s->game, local_player(), &v);   /* 0 = the local player has no activity: the empty state */
    *out = v;
    return 0;
}
