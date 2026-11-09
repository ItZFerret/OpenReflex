/* test_drive_session.inc -- the garage's TEST DRIVE (include/rx_test_drive.h), included by rx_session.c after the other session includes.
 * The game's own sequence (game/garage_test_drive.c), as GarageHUB:OnStartPressed and the Test Drive Loading Page run it in the front end:
 *   the front end's local players (EnsureLocalPlayers twice, as the event load), MXUIManager_EnableTestDrive( TRUE ), App.SyncDatabaseWithManagers
 *   (Game_SetupActivityArea), MXGarage_TestDriveStart; then the world's own frames (MXGame::Update state 2 -> the UI world's MXWorld::vf5) spawn the
 *   local slot's vehicle and rider in the UI world's FreeRide activity.
 * PRODUCT ONLY (not original logic; documented in core/plan/QUEUE.md "w-testride"):
 *   - the UI manager's words (ui+0x280 / +0x284 / +0x290 / +0x294) live in a block of the session (the product has no UI manager: MXGame + 0x745c
 *     stays 0, so the game's other readers keep their front-end answer); it starts as the garage leaves it: the players idle (+0x284 = 1).
 *   - the engine's pause word (timing +0xc) and the UI world's (+0x20014) are 0, as the front end's pages leave them before the garage (the
 *     oracle's test-drive boot: 0 at the garage, after EnableTestDrive, after TestDriveStart and while riding; the product's create sets the
 *     start-up value 1, which the title / menu pages' App.Pause( false ) clears in the original).
 *   - the spec only names the player's mode / display name for EnsureLocalPlayers (UI properties in the original); the event type and the
 *     location are the test drive's: FreeRide at the UI level (MXRegistry UISceneName). No level is opened (the UI world is the session's). */
#include "../include/rx_test_drive.h"
#include "../game/garage_test_drive.h"
#include "../game/loop_input_suspend.h"
_Static_assert(sizeof(rx_test_drive_state) == 32, "test drive ABI size");

static struct { struct rx_session* s; rx_addr ui; uint32_t active; uint64_t start_frame; int loading; } g_test_drive;

/* the loading page's wait: a started test drive whose local slot has no vehicle and rider yet */
static int test_drive_spawning(const struct rx_session* s) {
    if (g_test_drive.s != s || g_test_drive.active != 1) return 0;
    const rx_addr p = local_player();
    return !(p && U32(p, 0x1ac) && U32(p, 0x1b4));
}
/* after each frame: the Test Drive Loading Page's exit once the local slot has spawned (MXUIPlayersLoadingPage::vf5 005a82b0:
 * SuspendPlayerInput 0, the players' input groups back through 005a3b00; w-tutcore) */
static void test_drive_loading_page_frame(struct rx_session* s) {
    if (g_test_drive.s != s || g_test_drive.active != 1 || !g_test_drive.loading || test_drive_spawning(s)) return;
    g_test_drive.loading = 0;
    rx_ui_suspend_input_set(g_test_drive.ui, 0);
}

struct test_drive_call { struct rx_session* s; rx_event_spec spec; int end, rc; };
static void test_drive_run(void* p) {
    struct test_drive_call* c = (struct test_drive_call*)p;
    struct rx_session* s = c->s;
    const rx_addr g = s->game;
    if (c->end) {
        rx_ui_enable_test_drive(g_test_drive.ui, 0);                /* the pause request in a test drive: EnableTestDrive( FALSE ) */
        rx_garage_test_drive_end(g + 0x15570);                      /* the Test Drive Exit Page: mxGarage:TestDriveEnd( ) */
        g_test_drive.active = 2;
        c->rc = 0;
        return;
    }
    s->spec = c->spec;
    rx_event_set_spec(&s->spec);
    if (U32(g, 0x18) != 2) SU32(g, 0x18, 2);
    rx_ensure_local_players(0, 0);                                  /* the front end's local player (as rx_session_event_load) */
    rx_ensure_local_players(0, 0);
    const rx_addr ui = rx_reflex_aligned_alloc(0x300);              /* PRODUCT: the UI manager's words (see the header) */
    if (!ui) { session_error(s, "test drive: no memory for the UI state"); c->rc = 1; return; }
    crt_memset(ui, 0, 0x300);
    SU32(ui, 0x284, 1);                                             /* the garage: the front end's players idle */
    g_test_drive.ui = ui;
    SU32(U32(U32(0, 0x00a95844), 0x10), 0xc, 0);                    /* PRODUCT: the front end's pages leave the engine unpaused */
    if (U32(U32(g, 0x20c8), 0x20014) != 0) SU32(U32(g, 0x20c8), 0x20014, 0);
    rx_ui_enable_test_drive(ui, 1);                                 /* App.EnableTestDrive( TRUE ) */
    rx_game_setup_activity_area(g);                                 /* App.SyncDatabaseWithManagers( ) */
    /* "GotoLoadingPage": the Test Drive Loading Page is an MXUIPlayersLoadingPage: its entry (vf3 005a8280) clears ui + 0x28c and sets
     * SuspendPlayerInput (the UI manager's update suspends the player once it exists); its exit (vf5 005a82b0, when the slot has spawned:
     * test_drive_loading_page_frame) clears it (w-tutcore) */
    rx_ui_suspend_input_reset();
    SU32(ui, 0x28c, 0);
    rx_ui_suspend_input_set(ui, 1);
    g_test_drive.loading = 1;
    rx_garage_test_drive_start(g + 0x15570);                        /* TestDriveTransitionPage:Present: mxGarage:TestDriveStart( ) */
    g_test_drive.s = s;
    g_test_drive.active = 1;
    g_test_drive.start_frame = s->frames;
    s->spec_loaded = 1;
    s->state = RX_SESSION_LOADING;
    c->rc = 0;
}

static int test_drive_call(struct rx_session* s, struct test_drive_call* call, const char* what) {
    char captured[4096];
    const int sig = run_guarded(test_drive_run, call, captured, sizeof captured);
    if (sig) {
        char msg[512];
        describe_fault(msg, sizeof msg, what, sig, captured);
        session_error(s, "%s", msg);
        return 1;
    }
    return call->rc;
}

int rx_session_test_drive_start(struct rx_session* s, const rx_event_spec* spec) {
    if (!session_ok(s) || s->state == RX_SESSION_ERROR) return 1;
    if (s->spec_loaded || s->career_phase || s->records_finalized || !s->game || !s->install) {
        snprintf(s->error, sizeof s->error, "the test drive needs a fresh session (no event, no career)");
        return 2;
    }
    rx_ms_set(1.0f);   /* MAP SCALE (port): the garage's test ride is always at 100 % */
    if (!U32(s->game, 0x20c8) || !U32(s->game, 0x15b54)) {
        snprintf(s->error, sizeof s->error, "the test drive needs the UI world (MXGame + 0x20c8 ready)");
        return 2;
    }
    struct test_drive_call call;
    memset(&call, 0, sizeof call);
    call.s = s;
    call.rc = 1;
    if (spec) call.spec = *spec; else rx_session_event_default(&call.spec);
    snprintf(call.spec.event_type, sizeof call.spec.event_type, "%s", "FreeRide");
    {   /* the UI level's name (MXGame + 0x34: MXRegistry UISceneName) */
        size_t i = 0;
        for (; i + 1 < sizeof call.spec.location; i++) { const char ch = (char)U8(s->game, 0x34 + (uint32_t)i); call.spec.location[i] = ch; if (!ch) break; }
        call.spec.location[i] = 0;
    }
    call.spec.event_activation[0] = 0;
    call.spec.duration = 0;
    call.spec.duration_type[0] = 0;
    call.spec.riders = 1;
    call.spec.vehicle_class = -1;
    if (!call.spec.player_mode[0]) snprintf(call.spec.player_mode, sizeof call.spec.player_mode, "%s", "SinglePlayer");
    rx_scenario_sounds_clear();
    return test_drive_call(s, &call, "rx_session_test_drive_start");
}

int rx_session_test_drive_end(struct rx_session* s) {
    if (!session_ok(s) || s->state == RX_SESSION_ERROR) return 1;
    if (g_test_drive.s != s || g_test_drive.active != 1) { snprintf(s->error, sizeof s->error, "no test drive is running"); return 2; }
    struct test_drive_call call;
    memset(&call, 0, sizeof call);
    call.s = s;
    call.end = 1;
    call.rc = 1;
    return test_drive_call(s, &call, "rx_session_test_drive_end");
}

int rx_session_test_drive(struct rx_session* s, rx_test_drive_state* out) {
    if (!session_ok(s) || !out || out->size != sizeof *out || out->version != RX_TEST_DRIVE_API_VERSION) return 1;
    rx_test_drive_state v;
    memset(&v, 0, sizeof v);
    v.size = sizeof v;
    v.version = RX_TEST_DRIVE_API_VERSION;
    v.vehicle_type = -1;
    if (g_test_drive.s == s && g_test_drive.active) {
        v.active = (int32_t)g_test_drive.active;
        v.frames = (uint32_t)(s->frames - g_test_drive.start_frame);
        v.enabled = I32(g_test_drive.ui, 0x290);
        v.players_idle = I32(g_test_drive.ui, 0x284);
        const rx_addr p = local_player();
        if (p && U32(p, 0x1ac) && U32(p, 0x1b4)) {
            v.spawned = 1;
            const rx_addr veh = U32(p, 0x1ac), sim = U32(veh, 0x234);
            const struct rx_vehicle_class* c = vehicle_class(U32(veh, 0), sim ? U32(sim, 0) : 0, 0);
            v.vehicle_type = c ? c->factory : -1;
        }
    }
    *out = v;
    return 0;
}
