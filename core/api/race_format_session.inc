/* race_format_session.inc -- the race format's HUD values (include/rx_race_format.h), included by rx_session.c after
 * powercross_hud_session.inc (it uses local_player() and session_ok()). Copies only. */
#include "rx_race_format.h"
_Static_assert(sizeof(rx_race_format_state) == 48, "race format ABI size");

int rx_session_race_format(struct rx_session* s, rx_race_format_state* out) {
    if (!session_ok(s) || !out || out->size != sizeof *out || out->version != RX_RACE_FORMAT_VERSION) return 1;
    rx_race_format_state v;
    memset(&v, 0, sizeof v);
    v.size = sizeof v;
    v.version = RX_RACE_FORMAT_VERSION;
    v.section = -1;
    const rx_addr player = local_player();
    const rx_addr activity = player ? U32(player, 0x94) : 0;
    if (activity && (U32(activity, 0) == 0x00943554 || U32(activity, 0) == 0x00944a74)) {
        v.valid = 1;
        v.activity_mode = I32(activity, 0x238);
        v.activity_state = I32(activity, 0x21c);
        v.drivers = I32(activity, 0x26c);
        v.riders_left = I32(activity, 0xff088);
        v.section = I32(player, 0x11ac);
        v.section_time = F32(player, 0x11b4);
    }
    *out = v;
    return 0;
}
