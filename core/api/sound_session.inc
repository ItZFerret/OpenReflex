/* sound_session.inc -- the sound managers' inputs besides the engine emitters (include/rx_sound.h, game/sound_state.c; w-sfx), included
 * by rx_session.c. Copies only. */
#include "rx_sound.h"
#include "sound_state.h"

int rx_session_sound(struct rx_session* s, rx_sound_state* out) {
    const int v1 = out && out->version == 1u && out->size == RX_SOUND_V1_SIZE;   /* (w-rc-core) version 1: 16 riders */
    if (!session_ok(s) || !out || (!v1 && (out->size != sizeof *out || out->version != RX_SOUND_VERSION))) return 1;
    static rx_sound_state v;   /* 29 KB: not on the host stack (session owner thread only) */
    memset(&v, 0, sizeof v);
    v.size = sizeof v;
    v.version = RX_SOUND_VERSION;
    v.valid = (uint32_t)rx_sound_fill(sound_game(s), &v);   /* (vehicle_audio_session.inc: a level, or the UI world's test drive / tutorial) */
    if (v1) {
        if (v.rider_count > RX_SOUND_RIDERS) v.rider_count = RX_SOUND_RIDERS;
        v.size = RX_SOUND_V1_SIZE; v.version = 1u;
        memcpy(out, &v, RX_SOUND_V1_SIZE);
        return 0;
    }
    *out = v;
    return 0;
}

/* 1 while the local player's view is the first-person camera (class 0x1002, pose valid): the camera sound effect's first-person reverb and the
   foley emitter's rider vocal position (0068bfb0). 0 otherwise (no world, no local player). Read only; session owner thread, between advances. */
int rx_session_first_person_view(struct rx_session* s) {
    if (!session_ok(s) || !sound_game(s)) return 0;
    return (int)rx_sound_first_person_view(local_player());
}
