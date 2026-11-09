/* activation_session.inc -- the Free Ride activation areas (game/rx_activation_api.h), included by rx_session.c after the career
 * includes (it uses their session fields). Read-only copies of the MXPlayer fields the game's trigger handlers keep
 * (game/race_activation.c), and the exhibition's InCareerMode before the event load (game/career_properties.c). */
#include "race_activation.h"
_Static_assert(sizeof(rx_activation_state) == 48, "activation ABI size");

int rx_session_activation(struct rx_session* s, rx_activation_state* out) {
    if (!session_ok(s) || !out || out->size != sizeof *out || out->version != RX_ACTIVATION_API_VERSION) return 1;
    rx_activation_state v;
    memset(&v, 0, sizeof v);
    v.size = sizeof v;
    v.version = RX_ACTIVATION_API_VERSION;
    v.required_class = rx_activation_required_class();
    const rx_addr p = local_player();
    if (p) {
        v.valid = 1;
        v.active = U32(p, 0xa0);
        v.track_id = I32(p, 0x1a8);
        for (uint32_t i = 0; i + 1 < sizeof v.activity; i++) {
            const uint8_t c = U8(p, 0xa4 + i);
            if (c == 0) break;
            v.activity[i] = (char)c;
        }
    }
    *out = v;
    return 0;
}

int rx_session_event_in_career_mode(struct rx_session* s, int32_t value) {
    if (!session_ok(s) || s->state == RX_SESSION_ERROR) return 1;
    if (s->spec_loaded || s->frames || s->career_phase || rx_owned_properties_active()) return 2;
    rx_owned_property_set_in_career_mode(value != 0);
    return 0;
}
