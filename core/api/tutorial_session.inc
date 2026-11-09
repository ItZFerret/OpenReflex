/* tutorial_session.inc -- the MotoSkills tutorials (include/rx_tutorial.h), included by rx_session.c after test_drive_session.inc.
 * The game's own sequence (game/fx_tutorial_manager.c), as MainMenu:OnTutorialKeypress, the Tutorial Load Page and the tutorial pages' Lua
 * run it in the front end:
 *   the front end's local player (EnsureLocalPlayers with the UI world's Free Ride activity type 8), App.EnableTutorialMode( TRUE )
 *   (MXUIManager_EnableTutorialMode 00599820), App.SyncDatabaseWithManagers (Game_SetupActivityArea), the loading page's
 *   Game_SetupActivityArea (MXUIPlayersLoadingPage::vf11 005a82f0); then the world's frames spawn the local slot in the UI world's FreeRide
 *   and MXUIWorld::vf5 updates the TutorialManager every frame.
 * PRODUCT ONLY (not original logic; core/plan/QUEUE.md "w-tutorials"):
 *   - the UI manager's words (ui+0x280 / +0x284 / +0x298) live in a block of the session, as the test drive's (the product has no UI manager);
 *   - the engine's pause word and the UI world's start at 0, as the front end's pages leave them (the title / menu pages' App.Pause( false ));
 *   - the UI's answers: the popup stack (00743120) from rx_session_tutorial_popup, the pages' SuspendPlayerInput from
 *     rx_session_tutorial_suspend_input; the events / DYN_ keys / voice paths the manager hands the UI are recorded (fx_tutorial_ui.c) for
 *     rx_session_tutorial;
 *   - the UI variables TutorialOwnerVehicleId / TutorialVehicleOwner (the vehicle equipped before, MainMenu:OnTutorialKeypress) live in the
 *     session's tutorial block.
 * The vehicle is the original's (w-tutcore): MainMenu:OnTutorialKeypress's MXTableHelper:GetEquippedVehicle (004e7c60) and App.ChangeVehicle(
 * row, 2 ) (005a0670: the player's Favorite MX Lite, else the MX Lite preset); the quit's MXTableHelper:EquipVehicle (004e7930) of the vehicle
 * equipped before. */
#include "../include/rx_tutorial.h"
#include "../game/fx_tutorial.h"
#include "../game/fx_tutorial_ui.h"
#include "../game/anim.h"
#include "../game/loop_input_suspend.h"
#include "../game/build_vehicle_select.h"
#include "../game/race_activation.h"
_Static_assert(sizeof(rx_tutorial_state) == 4 * 24 + 32 + 5 * 64 + 8 + 128 + 4, "tutorial ABI size");

static struct {
    struct rx_session* s; rx_addr ui; uint32_t active; uint64_t start_frame;
    int has_vehicle; uint32_t owner_vehicle_id, vehicle_owner;     /* UI TutorialOwnerVehicleId / TutorialVehicleOwner */
    int loading;                                                   /* the Tutorial Load Page is up */
} g_tutorial;

/* the exclusive device's Player List row (MXTableHelper:GetPlayerTableRow( 0 ):rowIndex( ), 004e78b0) */
static uint32_t tutorial_player_row(void) {
    const rx_addr sp = rx_sp();
    rx_set_sp(sp - 8);
    const rx_addr ref = rx_player_table_row(0);
    rx_set_sp(sp);
    return U32(ref, 0);
}

static int tutorial_spawning(const struct rx_session* s) {
    if (g_tutorial.s != s || g_tutorial.active != 1) return 0;
    const rx_addr p = local_player();
    return !(p && U32(p, 0x1ac) && U32(p, 0x1b4));
}
/* after each frame: the Tutorial Load Page's exit once the local slot has its vehicle and rider (MXUIPlayersLoadingPage::vf5 005a82b0:
 * SuspendPlayerInput 0, the players' input groups back through 005a3b00) */
static void tutorial_loading_page_frame(struct rx_session* s) {
    if (g_tutorial.s != s || g_tutorial.active != 1 || !g_tutorial.loading || tutorial_spawning(s)) return;
    g_tutorial.loading = 0;
    rx_ui_suspend_input_set(g_tutorial.ui, 0);
}
static rx_addr tutorial_manager(const struct rx_session* s) {
    const rx_addr w = s->game ? U32(s->game, 0x20c8) : 0;
    return w ? U32(w, 0x22180) : 0;
}

enum { TUT_OP_START, TUT_OP_PAUSE, TUT_OP_LESSON, TUT_OP_DISABLE, TUT_OP_RETRY, TUT_OP_END, TUT_OP_SUSPEND };
struct tutorial_call { struct rx_session* s; rx_event_spec spec; int op, arg, rc; };
static void tutorial_run(void* p) {
    struct tutorial_call* c = (struct tutorial_call*)p;
    struct rx_session* s = c->s;
    const rx_addr g = s->game;
    switch (c->op) {
    case TUT_OP_PAUSE: rx_app_pause_active_world(c->arg ? 1u : 0u); c->rc = 0; return;      /* App.PauseActiveWorld */
    case TUT_OP_LESSON: rx_tutorial_app_enable(c->arg); c->rc = 0; return;                    /* App.EnableTutorial( lesson ) */
    case TUT_OP_DISABLE: rx_tutorial_app_disable(); c->rc = 0; return;                        /* App.DisableTutorial( ) */
    case TUT_OP_RETRY: rx_tutorial_app_retry(); c->rc = 0; return;                            /* "RetryTutorial" */
    case TUT_OP_SUSPEND: rx_ui_suspend_input_set(g_tutorial.ui, c->arg ? 1u : 0u); c->rc = 0; return;   /* SuspendPlayerInput */
    case TUT_OP_END:
        /* TutorialPauseMenu:OnQuit / TutorialConclusionPopup:End: App.EnableTutorialMode( FALSE ), SuspendPlayerInput FALSE (005a3b00); then
         * the vehicle equipped before (TutorialOwnerVehicleId / TutorialVehicleOwner): MXTableHelper:EquipVehicle( row, id, owner ) */
        rx_ui_enable_tutorial_mode(g_tutorial.ui, 0);
        rx_ui_suspend_input_set(g_tutorial.ui, 0);
        if (g_tutorial.has_vehicle) {
            g_tutorial.has_vehicle = 0;
            const uint32_t row = tutorial_player_row();
            const rx_addr sp = rx_sp();
            rx_set_sp(sp - 12);
            rx_table_equip_vehicle(g_tutorial.vehicle_owner, row, g_tutorial.owner_vehicle_id);
            rx_set_sp(sp);
        }
        g_tutorial.active = 2;
        c->rc = 0;
        return;
    default: break;
    }
    s->spec = c->spec;
    rx_event_set_spec(&s->spec);
    if (U32(g, 0x18) != 2) SU32(g, 0x18, 2);
    rx_ensure_local_players(0, 8);                                  /* the front end's local player, requesting the UI world's Free Ride */
    rx_ensure_local_players(0, 8);
    const rx_addr ui = rx_reflex_aligned_alloc(0x300);              /* PRODUCT: the UI manager's words (see the header) */
    if (!ui) { session_error(s, "tutorial: no memory for the UI state"); c->rc = 1; return; }
    crt_memset(ui, 0, 0x300);
    SU32(ui, 0x284, 1);                                             /* the main menu: the front end's players idle */
    g_tutorial.ui = ui;
    SU32(U32(U32(0, 0x00a95844), 0x10), 0xc, 0);                    /* PRODUCT: the front end's pages leave the engine unpaused */
    if (U32(U32(g, 0x20c8), 0x20014) != 0) SU32(U32(g, 0x20c8), 0x20014, 0);
    rx_tutorial_ui_reset();
    rx_ui_suspend_input_reset();
    {   /* the voice's event paths: the original's audio start-up (AudioPackagesList.xml: DataBase AudioLocalizedTutorial, Package XML) has
         * TutorialAudioMappings loaded long before a lesson; the product loads that one package here (no sound bank: the voice is the host's) */
        const rx_addr db = U32(U32(0, 0x00a95844), 8);
        rx_database_load(db, s->install, "AudioLocalizedTutorial", "Database\\", 0, 0);
        (void)rx_database_load_package(db, s->install, "AudioLocalizedTutorial", "XML", 0, 0);
    }
    {   /* MainMenu:OnTutorialKeypress: the vehicle equipped now kept (MXTableHelper:GetEquippedVehicle 004e7c60: TutorialOwnerVehicleId /
         * TutorialVehicleOwner when its id is not -1), then "Pick an MX Lite": App.ChangeVehicle( row, 2 ) (005a0670) */
        const uint32_t row = tutorial_player_row();
        const rx_addr sp = rx_sp(), o = sp - 8;
        SU32(o, 0, 0xffffffffu); SU32(o, 4, 0);
        rx_set_sp(o - 12);
        const uint32_t ok = rx_table_get_equipped_vehicle(row, o, o + 4);
        rx_set_sp(sp);
        g_tutorial.has_vehicle = ok != 0 && U32(o, 0) != 0xffffffffu;
        g_tutorial.owner_vehicle_id = U32(o, 0);
        g_tutorial.vehicle_owner = U32(o, 4);
        rx_set_sp(sp - 12);
        rx_change_vehicle(row, 2);
        rx_set_sp(sp);
    }
    rx_game_setup_activity_area(g);                                 /* App.SyncDatabaseWithManagers( ) */
    rx_ui_enable_tutorial_mode(ui, 1);                              /* App.EnableTutorialMode( TRUE ) */
    /* "GotoLoadingPage": the Tutorial Load Page (MXUIPlayersLoadingPage): its entry (vf3 005a8280) clears ui + 0x28c and sets
     * SuspendPlayerInput (the player is suspended by the UI manager's update once it exists), its update (vf11 005a82f0) sets the
     * activity area up; its exit (vf5 005a82b0, when the slot has spawned: tutorial_loading_page_frame) clears SuspendPlayerInput */
    SU32(ui, 0x28c, 0);
    rx_ui_suspend_input_set(ui, 1);
    g_tutorial.loading = 1;
    rx_game_setup_activity_area(g);                                 /* the Tutorial Load Page (MXUIPlayersLoadingPage::vf11) */
    g_tutorial.s = s;
    g_tutorial.active = 1;
    g_tutorial.start_frame = s->frames;
    s->spec_loaded = 1;
    s->state = RX_SESSION_LOADING;
    c->rc = 0;
}

static int tutorial_call(struct rx_session* s, struct tutorial_call* call, const char* what) {
    char captured[4096];
    const int sig = run_guarded(tutorial_run, call, captured, sizeof captured);
    if (sig) {
        char msg[512];
        describe_fault(msg, sizeof msg, what, sig, captured);
        session_error(s, "%s", msg);
        return 1;
    }
    return call->rc;
}
static int tutorial_op(struct rx_session* s, int op, int arg, const char* what) {
    if (!session_ok(s) || s->state == RX_SESSION_ERROR) return 1;
    if (g_tutorial.s != s || g_tutorial.active != 1 || !tutorial_manager(s)) { snprintf(s->error, sizeof s->error, "no tutorial is running"); return 2; }
    struct tutorial_call call;
    memset(&call, 0, sizeof call);
    call.s = s; call.op = op; call.arg = arg; call.rc = 1;
    return tutorial_call(s, &call, what);
}

int rx_session_tutorial_start(struct rx_session* s, const rx_event_spec* spec) {
    if (!session_ok(s) || s->state == RX_SESSION_ERROR) return 1;
    if (s->spec_loaded || s->career_phase || s->records_finalized || !s->game || !s->install) {
        snprintf(s->error, sizeof s->error, "the tutorial needs a fresh session (no event, no career)");
        rx_ms_set(1.0f);
        return 2;
    }
    if (!U32(s->game, 0x20c8) || !U32(s->game, 0x15b54) || !tutorial_manager(s)) {
        snprintf(s->error, sizeof s->error, "the tutorial needs the UI world and its TutorialManager (MXGame + 0x20c8 ready)");
        return 2;
    }
    struct tutorial_call call;
    memset(&call, 0, sizeof call);
    call.s = s;
    call.op = TUT_OP_START;
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
    return tutorial_call(s, &call, "rx_session_tutorial_start");
}
int rx_session_tutorial_pause(struct rx_session* s, int paused) { return tutorial_op(s, TUT_OP_PAUSE, paused, "rx_session_tutorial_pause"); }
int rx_session_tutorial_lesson(struct rx_session* s, int lesson) {
    if (lesson < 0 || lesson > 5) { if (session_ok(s)) snprintf(s->error, sizeof s->error, "lesson %d (0..5)", lesson); return 2; }
    return tutorial_op(s, TUT_OP_LESSON, lesson, "rx_session_tutorial_lesson");
}
int rx_session_tutorial_disable(struct rx_session* s) { return tutorial_op(s, TUT_OP_DISABLE, 0, "rx_session_tutorial_disable"); }
int rx_session_tutorial_retry(struct rx_session* s) { return tutorial_op(s, TUT_OP_RETRY, 0, "rx_session_tutorial_retry"); }
int rx_session_tutorial_popup(struct rx_session* s, int up) {
    if (!session_ok(s)) return 1;
    rx_tutorial_ui_set_popup(up);
    return 0;
}
int rx_session_tutorial_suspend_input(struct rx_session* s, int suspended) {
    return tutorial_op(s, TUT_OP_SUSPEND, suspended, "rx_session_tutorial_suspend_input");
}
int rx_session_tutorial_end(struct rx_session* s) {
    const int r = tutorial_op(s, TUT_OP_END, 0, "rx_session_tutorial_end");
    rx_tutorial_ui_set_popup(0);
    return r;
}

/* PlayerRider.TutorialsCompleted of the controller's per-device database (as 004e8460 reads it) */
static int32_t tutorial_completed(uint32_t controller) {
    if (controller > 3) return -1;
    const rx_addr t = rx_tbl_find(0x00ae10f0u + controller * 0x1f4u, 0x0093e53c /* "PlayerRider" */);
    if (!t) return -1;
    const uint32_t col = rx_tbl_column_index(t, 0x0093e5d4 /* "TutorialsCompleted" */);
    if (col == 0xffffffffu) return -1;
    return I32(rx_tbl_cell_word(t, col, 0), 0);
}

int rx_session_tutorial(struct rx_session* s, rx_tutorial_state* out) {
    if (!session_ok(s) || !out || out->size != sizeof *out || out->version != RX_TUTORIAL_API_VERSION) return 1;
    rx_tutorial_state v;
    memset(&v, 0, sizeof v);
    v.size = sizeof v;
    v.version = RX_TUTORIAL_API_VERSION;
    v.lesson = -1; v.current_tutorial = -1; v.completed = -1; v.mode = -1;
    const rx_addr m = tutorial_manager(s);
    if (g_tutorial.s == s && g_tutorial.active) {
        v.active = (int32_t)g_tutorial.active;
        v.frames = (uint32_t)(s->frames - g_tutorial.start_frame);
        v.mode = I32(g_tutorial.ui, 0x298);
        const rx_addr p = local_player();
        if (p && U32(p, 0x1ac) && U32(p, 0x1b4)) v.spawned = 1;
        if (p) v.completed = tutorial_completed(U32(p, 0x1f0));
    }
    if (m) {
        v.manager_off = I32(m, 0); v.lesson = I32(m, 4); v.lesson_state = I32(m, 8); v.step = I32(m, 0x7c);
        v.passed = I32(m, 0x64); v.wrecked = I32(m, 0x70); v.boundary = I32(m, 0x74); v.good = I32(m, 0x6c); v.bad = I32(m, 0x68);
        v.valid = I32(m, 0x60);
        v.step_timer = F32(m, 0x88); v.lean_time = F32(m, 0x8c); v.hold_time = F32(m, 0x90); v.wreck_timer = F32(m, 0x84);
        v.lean_flags = (U32(m, 0xa0) ? 1 : 0) | (U32(m, 0xa4) ? 2 : 0) | (U32(m, 0xa8) ? 4 : 0) | (U32(m, 0xac) ? 8 : 0);
    }
    if (s->game && U32(s->game, 0x20c8)) v.world_paused = I32(U32(s->game, 0x20c8), 0x20014);
    const rx_tutorial_ui_record* r = rx_tutorial_ui_get();
    v.current_tutorial = r->current_tutorial;
    v.event_serial = r->serial;
    memcpy(v.events, r->events, sizeof v.events);
    memcpy(v.description, r->description, sizeof v.description);
    memcpy(v.activation, r->activation, sizeof v.activation);
    memcpy(v.hud_label, r->hud_label, sizeof v.hud_label);
    memcpy(v.completion, r->completion, sizeof v.completion);
    memcpy(v.failed, r->failed, sizeof v.failed);
    v.voice_serial = r->voice_serial; v.stop_serial = r->stop_serial;
    memcpy(v.voice, r->voice, sizeof v.voice);
    *out = v;
    return 0;
}

int rx_session_tutorial_objects(struct rx_session* s, rx_tutorial_object* out, uint32_t max) {
    if (!session_ok(s)) return -1;
    if (!out || max == 0 || !U32(0, 0x00a95844)) return 0;
    const rx_addr scene = U32(U32(0, 0x00a95844), 0x1c);   /* the UI scene (ShowLessonObjects' engine context + 0x1c) */
    if (!scene) return 0;
    uint32_t written = 0;
    static const uint32_t types[2] = {4, 0x1078};
    RX_FRAME_ENTER(F, 0xd0);
    for (int t = 0; t < 2; t++) {
        for (uint32_t i = 0; i < 0xc4; i++) SU8(F, i, 0);
        const int32_t n = (int32_t)rx_scene_manager_collect_typed(scene, types[t], F, 0x32, 0);
        for (int32_t i = 0; i < n && written < max; i++) {
            const rx_addr e = U32(F, 4u * (uint32_t)i);
            const rx_addr name = e ? U32(e, 0x18) : 0;
            if (!name || U8(name, 0) != 'T' || U8(name, 1) != 'U' || U8(name, 2) != 'T') continue;
            rx_tutorial_object* o = &out[written++];
            memset(o, 0, sizeof *o);
            o->entity_id = I32(e, 0x14);
            o->shown = U8(e, 0xa2);
            copy_guest_string(o->name, sizeof o->name, name);
        }
    }
    RX_FRAME_LEAVE(F, 0xd0);
    return (int32_t)written;
}
