/* reflex_hud_session.inc -- the wreck-recovery arrows (include/rx_reflex_hud.h, game/reflex_hud.c), included by rx_session.c after
 * stunt_hud_session.inc (it uses local_player() and session_ok()). Copies only. */
#include "rx_reflex_hud.h"
_Static_assert(sizeof(rx_reflex_hud_state) == 124, "reflex hud ABI size");

int rx_session_reflex_hud(struct rx_session* s, rx_reflex_hud_state* out) {
    if (!session_ok(s) || !out || out->size != sizeof *out || out->version != RX_REFLEX_HUD_VERSION) return 1;
    rx_reflex_hud_state v;
    memset(&v, 0, sizeof v);
    v.size = sizeof v;
    v.version = RX_REFLEX_HUD_VERSION;
    (void)rx_reflex_hud_fill(local_player(), &v);   /* 0 = the local player has no activity: the empty state */
    *out = v;
    return 0;
}
