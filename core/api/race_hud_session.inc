/* race_hud_session.inc -- the race HUD components' values (include/rx_race_hud.h, game/race_hud.c), included by rx_session.c after
 * race_format_session.inc (it uses local_player() and session_ok()). Copies only. */
#include "rx_race_hud.h"
_Static_assert(sizeof(rx_race_hud_state) == 620, "race hud ABI size");

int rx_session_race_hud(struct rx_session* s, rx_race_hud_state* out) {
    if (!session_ok(s) || !out || out->size != sizeof *out || out->version != RX_RACE_HUD_VERSION) return 1;
    rx_race_hud_state v;
    memset(&v, 0, sizeof v);
    v.size = sizeof v;
    v.version = RX_RACE_HUD_VERSION;
    v.eliminated_slot = v.eliminator_slot = v.section_current = -1;
    (void)rx_race_hud_fill(s->game, local_player(), &v);   /* 0 = the local player has no activity: the empty state */
    *out = v;
    return 0;
}
