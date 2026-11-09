/* stunt_hud_session.inc -- the stunt feedback overlay's trick names (include/rx_stunt_hud.h, game/race_hud.c), included by rx_session.c after
 * race_hud_session.inc (it uses local_player() and session_ok()). Copies only. */
#include "rx_stunt_hud.h"
_Static_assert(sizeof(rx_stunt_hud_state) == 872, "stunt hud ABI size");

int rx_session_stunt_hud(struct rx_session* s, rx_stunt_hud_state* out) {
    if (!session_ok(s) || !out || out->size != sizeof *out || out->version != RX_STUNT_HUD_VERSION) return 1;
    rx_stunt_hud_state v;
    memset(&v, 0, sizeof v);
    v.size = sizeof v;
    v.version = RX_STUNT_HUD_VERSION;
    (void)rx_stunt_hud_fill(local_player(), &v);   /* 0 = the local player has no activity: the empty state */
    *out = v;
    return 0;
}
