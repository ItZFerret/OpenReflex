/* vehicle_audio_session.inc -- the vehicle sound layer's simulation values and sound-layer cues (include/rx_vehicle_audio.h,
 * game/vehicle_audio.c), included by rx_session.c. Copies only. */
#include "rx_vehicle_audio.h"
#include "vehicle_audio.h"

/* The game whose world the sound queries read (this file's and sound_session.inc's): an event's level is open, or the session runs its
 * test drive / tutorial in the UI world (test_drive_session.inc / tutorial_session.inc: spec_loaded with no level opened: the UI world is
 * the session's). The original's sound managers run there as in a level (the garage's TEST DRIVE and the MotoSkills lessons have their
 * engines, foley and ambience); gating on the level alone left those rides silent. 0 before an event (the front end's pre-event session). */
static rx_addr sound_game(const struct rx_session* s) { return s->level || s->spec_loaded ? s->game : 0; }

int rx_session_vehicle_audio(struct rx_session* s, rx_vehicle_audio_state* out) {
    const int v1 = out && out->version == 1u && out->size == RX_VEHICLE_AUDIO_V1_SIZE;   /* (w-rc-core) version 1: 16 riders */
    if (!session_ok(s) || !out || (!v1 && (out->size != sizeof *out || out->version != RX_VEHICLE_AUDIO_VERSION))) return 1;
    rx_vehicle_audio_state v;
    memset(&v, 0, sizeof v);
    v.size = sizeof v;
    v.version = RX_VEHICLE_AUDIO_VERSION;
    v.valid = (uint32_t)rx_vehicle_audio_fill(sound_game(s), &v);
    if (v1) {
        if (v.rider_count > RX_VEHICLE_AUDIO_RIDERS) v.rider_count = RX_VEHICLE_AUDIO_RIDERS;
        v.size = RX_VEHICLE_AUDIO_V1_SIZE; v.version = 1u;
        memcpy(out, &v, RX_VEHICLE_AUDIO_V1_SIZE);
        return 0;
    }
    *out = v;
    return 0;
}

int rx_session_vehicle_audio_finished(struct rx_session* s, int32_t audio_slot) {
    if (!session_ok(s)) return 1;
    return rx_vehicle_audio_finished(audio_slot);
}
