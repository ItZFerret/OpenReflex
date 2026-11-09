/* challenge_session.inc -- the Free Ride challenges' end-of-challenge page (game/rx_challenge_api.h), included by rx_session.c after
 * activation_session.inc (it uses local_player() and session_ok()). Read-only copies of the challenge activity's medal state
 * (game/race_challenge_state.c) and the game's restart of the activity (game/race_restart.c). */
#include "race_challenge_state.h"
#include "race_restart.h"
#include "race_challenge_payout.h"
_Static_assert(sizeof(rx_challenge_state) == 88, "challenge ABI size");

_Static_assert(sizeof(rx_challenge_payout) == 68, "challenge payout ABI size");
struct challenge_payout_call { rx_addr player; int32_t series_id; rx_challenge_payout* out; int ok; };
static void challenge_payout_run(void* opaque) {
    struct challenge_payout_call* c = opaque;
    c->ok = rx_challenge_payout_apply(c->player, c->series_id, c->out);
}

int rx_session_challenge(struct rx_session* s, rx_challenge_state* out) {
    if (!session_ok(s) || !out || out->size != sizeof *out || out->version != RX_CHALLENGE_API_VERSION) return 1;
    rx_challenge_state v;
    memset(&v, 0, sizeof v);
    v.size = sizeof v;
    v.version = RX_CHALLENGE_API_VERSION;
    const rx_addr p = local_player();
    if (p) (void)rx_challenge_state_read(U32(p, 0x94), &v);
    *out = v;
    return 0;
}

int rx_session_challenge_restart(struct rx_session* s) {
    if (!session_ok(s) || s->state == RX_SESSION_ERROR) return 1;
    const rx_addr p = local_player();
    rx_challenge_state v;
    memset(&v, 0, sizeof v);
    if (!p || !rx_challenge_state_read(U32(p, 0x94), &v)) return 2;
    rx_game_restart_activity();
    s->challenge_paid_activity = 0;   /* the next Post pays again (PostChallengePopup:Start runs again) */
    return 0;
}

struct restart_call { int ok; };
static void restart_activity_run(void* opaque) {
    struct restart_call* c = opaque;
    rx_game_restart_activity();
    c->ok = 1;
}

/* The pause menu's RESTART of any event (rx_challenge_api.h). In a career the activity and every career object stay: only the
 * activity's own restart (Activity::vf16 and the class's) runs, so nothing of the attempt reaches the series or the profile. */
int rx_session_restart_activity(struct rx_session* s) {
    if (!session_ok(s) || s->state == RX_SESSION_ERROR) return 1;
    const rx_addr p = local_player();
    if (!p || !U32(p, 0x94)) return 2;
    if (s->records_finalized || s->career_continue_submitted) return 3;
    struct restart_call call = { 0 };
    char captured[4096];
    const int sig = run_guarded(restart_activity_run, &call, captured, sizeof captured);
    if (sig) {
        char message[512];
        describe_fault(message, sizeof message, "rx_session_restart_activity", sig, captured);
        session_error(s, "%s", message);
        return 1;
    }
    s->challenge_paid_activity = 0;   /* a restarted challenge's next Post pays again */
    return call.ok ? 0 : 2;
}

/* PostChallengePopup:Start's payout (race_challenge_payout.c), once per Post of the session's challenge. */
int rx_session_challenge_payout(struct rx_session* s, int32_t series_id, rx_challenge_payout* out) {
    if (!session_ok(s) || s->state == RX_SESSION_ERROR || !out || out->size != sizeof *out || out->version != RX_CHALLENGE_PAYOUT_VERSION) return 1;
    const rx_addr p = local_player();
    rx_challenge_state v;
    memset(&v, 0, sizeof v);
    if (!p || !rx_challenge_state_read(U32(p, 0x94), &v) || v.state != 4) return 2;
    if (s->challenge_paid_activity == U32(p, 0x94)) {
        *out = s->challenge_payout;
        out->repeat = 1;
        return 0;
    }
    rx_challenge_payout copy;
    memset(&copy, 0, sizeof copy);
    copy.size = sizeof copy;
    copy.version = RX_CHALLENGE_PAYOUT_VERSION;
    copy.series_best_place = -1;
    struct challenge_payout_call call = { p, series_id, &copy, 0 };
    char captured[4096];
    const int sig = run_guarded(challenge_payout_run, &call, captured, sizeof captured);
    if (sig) {
        char message[512];
        describe_fault(message, sizeof message, "rx_session_challenge_payout", sig, captured);
        session_error(s, "%s", message);
        return 1;
    }
    if (!call.ok) return 2;
    s->challenge_paid_activity = U32(p, 0x94);
    s->challenge_payout = copy;
    *out = copy;
    return 0;
}

int rx_session_wait_for_ui(struct rx_session* s, int32_t hold) {
    if (!session_ok(s) || s->state == RX_SESSION_ERROR || rx_owned_properties_active()) return 1;
    rx_owned_property_set_wait_for_ui(hold != 0);
    return 0;
}
