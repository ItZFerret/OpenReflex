#include "../game/map_scale.h"
#include "../game/phys_studio.h"
#include "../game/free_air.h"
#include "../game/rider_cap.h"   /* w-rc-core: seats / standings past 12 */
#include "../game/free_ride_borders.h"
#include "../game/weather_grip.h"
#include "rx_waypoint_api.h"
#include "rx_activation_api.h"
#include "rx_challenge_api.h"
#include "rx_career_vehicle.h"
#include "rx_powercross_hud.h"
#include "race_hud.h"
#include "rx_mode_results.h"
#include "vehicle_audio.h"
#include "sound_state.h"
#include "rx_rng.h"
#include "rx_results.h"
#include "../game/powercross_countdown.h"
#include "career_player_native.h"
/* rx_session.c -- the OPEN REFLEX session API (core/include/rx_api.h).
 *
 * The session is the composition core/tools/build_event_test.c proves today: memory + image + startup (unit 12b), the
 * construction stand-ins that driver documents (the engine job pool, the session event channel, the front-end world), the
 * event spec and level (unit 8), EnsureLocalPlayers / SetupEvent / StartEvent, then MXGame::Update's load states and race
 * frames. This file is deliberately thin: it owns the handle, the event and the error string, and every getter is a read of
 * the game's own memory (no game logic is duplicated here). The game's unported functions still abort with their address;
 * the API cannot catch that (rx_call_original exits the process), so advance can only report errors it can return from.
 *
 * Threading: one session, one simulation thread (see rx_api.h). No locks: the core game state is not reentrant.
 * Owner: the lead (shared API); see core/notes/unity-api-surface.md. */
#include "rx_api.h"
#include "terrain_dirty_cache.h"
#include "rx_event_ui.h"
#include "career_properties.h"
#include "career_property_bridge.h"
#include "career_session_host.h"
#include "career_composition.h"
#include "rx_career_api.h"
#include "career_presentation_binding.h"
#include "rx_career_transitions.h"
#include "rx_career_checkpoint.h"
#include "rx_career_completion.h"
#include "rx_career_waypoint.h"
#include "career_completion_host.h"
#include "rx_replay_api.h"
#include "rx_podium_api.h"
#include "rx_scene_pages_api.h"
#include "rx_crowd_api.h"
#include "replay_host_bridge.h"
#include "career_transition_host.h"
#include "../game/build_activity_freeride_run.h"
#include "../game/anim_scenario_race_intro.h"

#include "../src/fmt_common.h"
#include "../src/fmt_db.h"
#include "../src/fmt_scene.h"
#include "../src/fmt_modtrack.h"
#include "../src/fmt_rxar.h"
#include "../src/rx_d3d.h"
#include "../src/rx_heap.h"
#include "../src/rx_image.h"
#include "../src/rx_os.h"
#include "../src/rx_reset.h"
#include "../game/anim.h"
#include "../game/build_activity.h"
#include "../game/build_activity_int.h"
#include "../game/build_alloc.h"
#include "../game/build_asset.h"
#include "../game/build_globals.h"
#include "../game/build_mxworld.h"
#include "../game/build_rider.h"
#include "../game/build_ui_world.h"
#include "../game/build_uiprofile.h"
#include "../game/player_snapshot.h"
#include "../game/race_activity_misc.h"
#include "../game/build_vehicle.h"
#include "../game/build_world.h"
#include "../game/crt.h"
#include "../game/fx.h"
#include "../game/fx_stunt_input.h"
#include "../game/fx_stunt_table.h"
#include "../game/loop_cam_build.h"
#include "../game/loop_engine.h"
#include "../game/loop_hostinput.h"
#include "../game/loop_player.h"
#include "../game/race_aidata.h"
#include "../game/race_misc.h"
#include "../game/world_node.h"
#include "../game/world_node.h"

#include <math.h>
#include <setjmp.h>
#include <signal.h>
#ifndef _WIN32
#include <pthread.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "activity_cues.h"
#include "music_player.h"

#define RX_SESSION_MAGIC 0x52585353u   /* 'RXSS' */
#define RX_STANDIN_JOBS  0x00d94ae0u   /* the engine's job pool (build_event_test.c) */
#define RX_STANDIN_JOBS_GUARD 0x00d94ad8u
#define RX_GAME_GLOBAL   0x00a946b8u
#define RX_UI_GLOBAL     0x00a93fccu
#define RX_ENGINE_GLOBAL 0x00d728d0u

/* the ambient scan's objects (class + object) and TextureHandles (TextureAsset data -> handle), host memory */
struct rx_ambient_object { rx_addr address; int32_t kind; };
struct rx_ambient_texture { uint32_t data, handle; };
struct rx_prop_object { rx_addr address; int32_t kind; float first[16]; };

struct rx_session {
    uint32_t magic;
    uint32_t version;
    fmt_install* install;
    fmt_level* level;
    rx_event_spec spec;
    int spec_loaded;
    int records_finalized; /* explicit terminal checkpoint; arena still readable/exportable */
    rx_addr game;
    uint64_t frames;
    float last_dt;
    double pace_acc;   /* rx_session_pace: real time not yet turned into frame steps (s) */
    rx_session_state state;
    char error[512];

    /* Renderer-only exact packed-grid cache; no guest state is modified. */
    struct terrain_dirty_cache terrain_cache;

    /* the ambient entity / texture-handle scan is one-shot (objects live as long as the world; rescan repeats it) */
    int ambient_scanned;
    int32_t ambient_count;
    struct rx_ambient_object* ambient_objects;
    int32_t ambient_objects_count, ambient_objects_cap;
    struct rx_ambient_texture* ambient_textures;
    int32_t ambient_textures_count, ambient_textures_cap;

    /* the event scene's entity list (rx_session_scene_objects / _object_matrix), host memory */
    struct rx_prop_object* prop_objects;
    int32_t prop_count;
    rx_addr prop_scene;
    uint32_t props_reported;
    rx_addr* scene_entities;
    int32_t scene_entities_count, scene_entities_cap;
    /* rx_session_scenario_models (podium_session.inc): each ModelEntity's scene id as first seen (ModelPosition re-adds an entity under a
     * new id), keyed by the entity's address and name pointer */
    struct rx_model_identity { rx_addr address, name; int32_t id; }* model_ids;
    int32_t model_ids_count, model_ids_cap;

    /* tyre-track flushes (rx_session_tracks): bumped once per advanced frame in which the game's terrain sync ran */
    uint32_t tracks_version;
    /* w-terrtiles: the decode of the tyre-track rings for one (version, frame, terrain): the totals, the strip descriptors and the first
     * `have` triangles, so rx_session_tracks (count), rx_session_tracks (triangles) and rx_session_track_ribbons of a frame decode once */
    struct rx_tracks_cache {
        int valid; uint32_t version; uint64_t frames; rx_addr terrain;
        int32_t total, strips, chunks, nribbons;
        rx_track_ribbon ribbons[3 * 64];
        float* tris; uint32_t cap, have;   /* triangles allocated / valid in tris (18 floats each) */
    } tracks_cache;

    /* Isolated owned career API: host storage, never guest addresses in ABI. */
    rx_career_presentation career_presentation;
    rx_replay_host_bridge replay_host;
    uint32_t replay_event; /* the host was begun for an exhibition event (rx_session_replay_begin_event), not a career */
    uint32_t career_phase,career_controller,career_capabilities;
    rx_career_vehicle_identity career_vehicle_selection;
    rx_career_transition_status career_transitions;
    rx_addr career_rules_activity; /*worker-private; never in a public DTO*/
    rx_career_completion_status career_completion;
    rx_addr career_completion_event,career_completion_activity; /*private identities*/
    uint32_t career_completion_receipts,career_completion_series;
    uint32_t career_busy,career_continue_submitted,career_checkpoint_error;
    uint32_t moto_first_submitted,moto_first_post_observed;rx_addr moto_first_event,moto_first_activity;
    /* the Heat format's current intermediate round (heat / LCQ): its results CONTINUE and Post witness (heat_session.inc) */
    uint32_t heat_round_submitted, heat_round_post_observed;
    rx_addr heat_round_event, heat_round_activity;
    rx_addr challenge_paid_activity;      /* the challenge whose Post was paid (rx_session_challenge_payout); 0 = armed */
    rx_challenge_payout challenge_payout;  /* that payout's copy */
    /* w-water: the water wake's splats taken at each frame's render boundary (water_session.inc), host memory */
    struct water_splat_ring* water_splats;
};

/* One session per process at a time; create -> destroy -> create is supported (rx_reset.h: destroy returns the host statics to
 * their loaded state, the arena is unmapped and re-read from the install). g_live marks the open session; g_dirty is the
 * platform without a statics restore (rx_reset_available() == 0): there the first session uses the process up. Both, and the
 * create error (it has no session to live in: the old plugin has the same split, reflex_world_error(NULL)), are RX_RESET_KEEP:
 * the restore must not touch them. */
static int g_live RX_RESET_KEEP;
static struct rx_session* g_career_live_session RX_RESET_KEEP;
static rx_career_host_queue g_career_public_queue RX_RESET_KEEP;
static uint32_t g_career_public_queue_initialized RX_RESET_KEEP;
static void career_public_detach(struct rx_session* s);
static int g_dirty RX_RESET_KEEP;
static char g_create_error[512] RX_RESET_KEEP;

static void set_create_error(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_create_error, sizeof g_create_error, fmt, ap);
    va_end(ap);
}

static void session_error(struct rx_session* s, const char* fmt, ...) {
    if (!s) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s->error, sizeof s->error, fmt, ap);
    va_end(ap);
    s->state = RX_SESSION_ERROR;
}

static int session_ok(const struct rx_session* s) {
    return s && s->magic == RX_SESSION_MAGIC && s->version == RX_SESSION_API_VERSION;
}

static void copy_floats(float* dst, rx_addr src, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) dst[i] = F32(src, 4u * i);
}

static void copy_vec3(float* dst, rx_addr src) { copy_floats(dst, src, 3); }
static void copy_vec4(float* dst, rx_addr src) { copy_floats(dst, src, 4); }

/* ---------------------------------------------------------------------------------------------------------------- fault guard */

/* An unported game function reaches rx_call_original, which prints "rxcore: unported game function XXXXXXXX reached" and abort()s,
 * and a bug in game code can fault. Inside Unity either would kill the editor. The calls that run game code (create, event_load,
 * advance) therefore run under a guard: the fault signals are caught on the calling thread and siglongjmp back here; the call then
 * fails, the session goes to RX_SESSION_ERROR and last_error names the function. The game state is not consistent after that:
 * the only valid calls are the getters, last_error and destroy. The unported stop's text (the only place the address exists) comes
 * from this thread's rx_stop_note, and the stop jumps back through rx_stop_hook without a signal (src/rx_mem.c). Signals raised on
 * OTHER threads are passed to the previous handlers. One simulation thread per session (the API's threading rule); since w-loadarena
 * two core IMAGES (two loaded copies of the library, each with its own arena and statics) may run guarded calls on two threads at
 * once (the client's test-ride arena while an event loads): nothing here swaps process-wide state per call any more (stderr was
 * redirected and the fault handlers installed and restored around every call, which two images interleaved into a lost stderr and a
 * handler chain that called itself). The POSIX handlers are installed once per image and stay, each image's chaining to the one
 * before it (the host's first). */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
static _Thread_local jmp_buf* t_guard_env;
static void (*g_guard_old_abort)(int) RX_RESET_KEEP;
static void guard_abort(int sig){
    if(t_guard_env){jmp_buf* env=t_guard_env;t_guard_env=NULL;longjmp(*env,sig);}
    void (*old)(int)=g_guard_old_abort;
    /* MSVCRT resets a signal disposition before invoking it. Forward under
     * that default (so a handler re-raising the signal does not recurse here),
     * then retain any disposition the handler installed before re-arming. */
    if(old==SIG_IGN){signal(sig,guard_abort);return;}
    if(old && old!=SIG_DFL && old!=SIG_ERR && old!=guard_abort){
        old(sig);g_guard_old_abort=signal(sig,guard_abort);return;
    }
    signal(sig,SIG_DFL);raise(sig);
}
static LONG CALLBACK guard_exception(EXCEPTION_POINTERS* e){
    int sig;
    switch(e->ExceptionRecord->ExceptionCode){
    case EXCEPTION_ACCESS_VIOLATION:case EXCEPTION_IN_PAGE_ERROR:sig=SIGSEGV;break;
    case EXCEPTION_ILLEGAL_INSTRUCTION:sig=SIGILL;break;
    case EXCEPTION_INT_DIVIDE_BY_ZERO:case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_INVALID_OPERATION:sig=SIGFPE;break;
    default:return EXCEPTION_CONTINUE_SEARCH;
    }
    if(t_guard_env){jmp_buf* env=t_guard_env;t_guard_env=NULL;longjmp(*env,sig);}
    return EXCEPTION_CONTINUE_SEARCH;
}
static void guard_stop(void){ if(t_guard_env){jmp_buf* env=t_guard_env;t_guard_env=NULL;longjmp(*env,SIGABRT);} }
/* The CRT's SIGABRT disposition is process-wide (one UCRT for every module): only the first core image of the process swaps it per
 * call; a second image (the client's test-ride arena) leaves it alone, so the two images never capture each other's handler as their
 * "previous" one and forward an abort back and forth. Its unported stops still come back through rx_stop_hook; the vectored
 * exception handlers are per call and per thread already. The owner is named in the process environment (shared by all modules). */
static int g_guard_abort_owner RX_RESET_KEEP;   /* 0 not decided, 1 this image swaps SIGABRT, 2 another image does */
static int guard_owns_abort(void){
    if(!g_guard_abort_owner){
        char me[32],cur[32];snprintf(me,sizeof me,"%p",(void*)&g_guard_abort_owner);
        DWORD n=GetEnvironmentVariableA("RXCORE_GUARD_ABORT_IMAGE",cur,sizeof cur);
        if(n==0){SetEnvironmentVariableA("RXCORE_GUARD_ABORT_IMAGE",me);g_guard_abort_owner=1;}
        else g_guard_abort_owner=strcmp(cur,me)==0?1:2;
    }
    return g_guard_abort_owner==1;
}
static int run_guarded(void (*fn)(void*),void* arg,char* captured,size_t captured_size){
    jmp_buf env;const rx_addr sp=rx_sp();char buf[4096]={0};
    rx_stop_hook=guard_stop;rx_stop_note[0]=0;
    const int owns_abort=guard_owns_abort();
    void (*old_abort)(int)=owns_abort?signal(SIGABRT,guard_abort):SIG_DFL;
    if(owns_abort)g_guard_old_abort=old_abort;
    void* handler=AddVectoredExceptionHandler(0,guard_exception);
    int sig=0;
    if(!handler || old_abort==SIG_ERR){
        sig=SIGSEGV;
        snprintf(buf,sizeof buf,"cannot install Windows session fault guard (%lu)",(unsigned long)GetLastError());
    }else{
        sig=setjmp(env);
        if(!sig){t_guard_env=&env;fn(arg);}
    }
    t_guard_env=NULL;
    if(handler)RemoveVectoredExceptionHandler(handler);
    if(owns_abort && old_abort!=SIG_ERR)signal(SIGABRT,g_guard_old_abort==SIG_ERR?SIG_DFL:g_guard_old_abort);
    if(sig)rx_set_sp(sp);
    if(rx_stop_note[0] && !buf[0])snprintf(buf,sizeof buf,"%s",rx_stop_note);
    if(captured&&captured_size){strncpy(captured,buf,captured_size-1);captured[captured_size-1]=0;}
    return sig;
}
#else
static _Thread_local sigjmp_buf* t_guard_env;
static const int k_guard_signals[] = {SIGABRT, SIGSEGV, SIGBUS, SIGILL, SIGFPE};
#define RX_GUARD_N (int)(sizeof k_guard_signals / sizeof k_guard_signals[0])
static struct sigaction g_guard_old[RX_GUARD_N] RX_RESET_KEEP;

static void guard_handler(int sig, siginfo_t* info, void* ctx) {
    if (t_guard_env) {
        sigjmp_buf* env = t_guard_env;
        t_guard_env = NULL;
        siglongjmp(*env, sig);
    }
    for (int i = 0; i < RX_GUARD_N; i++) {
        if (k_guard_signals[i] != sig) continue;
        const struct sigaction* old = &g_guard_old[i];
        if (old->sa_flags & SA_SIGINFO) { old->sa_sigaction(sig, info, ctx); return; }
        if (old->sa_handler != SIG_DFL && old->sa_handler != SIG_IGN) { old->sa_handler(sig); return; }
        break;
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

static void guard_stop(void) {
    if (t_guard_env) { sigjmp_buf* env = t_guard_env; t_guard_env = NULL; siglongjmp(*env, SIGABRT); }
}

/* The fault handlers, once per image (the previous dispositions: the host's, or another core image's handler, which chains on). */
static int g_guard_installed RX_RESET_KEEP;
static pthread_mutex_t g_guard_lock RX_RESET_KEEP = PTHREAD_MUTEX_INITIALIZER;
static void guard_install(void) {
    if (__atomic_load_n(&g_guard_installed, __ATOMIC_ACQUIRE)) return;
    pthread_mutex_lock(&g_guard_lock);
    if (!g_guard_installed) {
        struct sigaction sa;
        memset(&sa, 0, sizeof sa);
        sa.sa_sigaction = guard_handler;
        sa.sa_flags = SA_SIGINFO | SA_ONSTACK;   /* (installed for the process now: a host thread's stack overflow must still reach the host's handler on its alternate stack) */
        sigemptyset(&sa.sa_mask);
        for (int i = 0; i < RX_GUARD_N; i++) sigaction(k_guard_signals[i], &sa, &g_guard_old[i]);
        __atomic_store_n(&g_guard_installed, 1, __ATOMIC_RELEASE);
    }
    pthread_mutex_unlock(&g_guard_lock);
}

/* Runs fn(arg) under the guard. Returns 0, or the signal that ended it; *captured gets the unported stop's text (empty: none). */
static int run_guarded(void (*fn)(void*), void* arg, char* captured, size_t captured_size) {
    sigjmp_buf env;
    char buf[4096];
    buf[0] = 0;
    const rx_addr sp = rx_sp();
    int sig = 0;
    guard_install();
    rx_stop_hook = guard_stop;
    rx_stop_note[0] = 0;
    if ((sig = sigsetjmp(env, 1)) == 0) {
        t_guard_env = &env;
        fn(arg);
    }
    t_guard_env = NULL;
    if (sig) rx_set_sp(sp);
    if (rx_stop_note[0]) snprintf(buf, sizeof buf, "%s", rx_stop_note);
    if (captured && captured_size) { strncpy(captured, buf, captured_size - 1); captured[captured_size - 1] = 0; }
    return sig;
}

#endif

/* "unported game function 0083ce40 reached" from the captured abort message, else the signal. */
static void describe_fault(char* out, size_t n, const char* where, int sig, const char* captured) {
    const char* key = "unported game function ";
    const char* p = captured ? strstr(captured, key) : NULL;
    if (p) {
        unsigned addr = 0;
        if (sscanf(p + strlen(key), "%x", &addr) == 1) {
            snprintf(out, n, "%s: unported game function %08x reached (the core aborted; the session is unusable)", where, addr);
            return;
        }
    }
    const char* rd = captured ? strstr(captured, "rxcore: read ") : NULL;
    if (rd) {   /* (w-fb-customtrack) a game file the readers could not read (build_asset.c): its name and why */
        snprintf(out, n, "%s: could not read %s; the session is unusable", where, rd + strlen("rxcore: read "));
        return;
    }
    snprintf(out, n, "%s: the core faulted (signal %d, %s); the session is unusable", where, sig,
#ifdef _WIN32
        sig==SIGABRT?"abort":sig==SIGSEGV?"access violation":sig==SIGILL?"illegal instruction":sig==SIGFPE?"arithmetic fault":"unknown"
#else
        strsignal(sig)
#endif
    );
}

/* ---------------------------------------------------------------------------------------------------------------- lifecycle */

uint32_t rx_session_api_version(void) { return RX_SESSION_API_VERSION; }

/* w-rc-core: the rider cap (rx_api.h, game/rider_cap.h) */
_Static_assert(RX_MAX_RIDERS == RX_RC_MAX && RX_ORIGINAL_RIDERS == RX_RC_ORIG, "rx_api.h and rider_cap.h agree on the rider cap");
int rx_session_max_riders(void) { return RX_MAX_RIDERS; }
int rx_session_set_field_size(struct rx_session* s, int32_t riders) {
    if (!session_ok(s)) return 1;
    if (riders != 0 && (riders <= RX_ORIGINAL_RIDERS || riders > RX_MAX_RIDERS)) { session_error(s, "field size %d: 0 (the game tables) or %d .. %d", (int)riders, RX_ORIGINAL_RIDERS + 1, RX_MAX_RIDERS); return 2; }
    rx_rc_set_field_size((uint32_t)riders);
    return 0;
}
int rx_session_field_size(const struct rx_session* s) { return s ? (int)rx_rc_field_size() : 0; }

/* The local human player's MXPlayer slot P = G + 0x7460 + slot * 0x12a0 (G+0x153ec lists the slots, G+0x1541c the count;
 * the first with type 0). Copied from the old plugin's local_player() / build_event_test's slot walk. */
static rx_addr local_player(void) {
    const rx_addr g = U32(0, RX_GAME_GLOBAL);
    if (!g) return 0;
    const uint32_t count = U8(g, 0x1541c);
    for (uint32_t i = 0; i < count && i < 8; i++) {
        const uint32_t slot = rx_seat_order(g + 0x7460, i);
        if (slot >= RX_RC_MAX) continue;
        const rx_addr p = rx_seat(g + 0x7460, slot);
        if (U32(p, 0x10) == 0 && U32(p, 4) != 0) return p;   /* 0 human, 1 computer; +4 PlayerLocal (an online remote rider is a non-local human: w-online2) */
    }
    return 0;
}

/* The construction stand-in of build_event_test.c: the engine's job pool (00881a70 not ported). Product-only construction; no
 * game logic. */
static void build_job_pool(void) {
    if (!(U8(0, RX_STANDIN_JOBS_GUARD) & 1)) {
        crt_memset(RX_STANDIN_JOBS, 0, 0x88e4);
        SU32(RX_STANDIN_JOBS, 0x8844, 3);                       /* the constructor's two workers */
        SU8(0, RX_STANDIN_JOBS_GUARD, U8(0, RX_STANDIN_JOBS_GUARD) | 1);
    }
}

/* ---- CONSTRUCTION STAND-IN: the game init's package loaders (unit 12) ---- The original's init thread (0075ed60) allocates
 * the 0x40-byte FactoryGearLoader, constructs it (0053cbd0), then calls the vehicle list's parts loader init (00540610). The
 * product's rx_globals_init game phase (build_globals.c) loads MXRegistry / the DLC tables only, so both globals stay 0 and
 * the spawn path walks a null package cache (the parts loader loops in rx_pkgcache_find, the gear loader does the same for
 * rider gear). Mirror the original's order here; drop when the init thread's 0075ed60 port performs these calls. */
static int build_package_loaders(struct rx_session* s) {
    if (!U32(0, 0x00d92ef4u)) {                              /* the FactoryGearLoader */
        const rx_addr gl = rx_reflex_aligned_alloc(0x40);
        if (!gl) return 0;
        SU32(0, 0x00d92ef4u, rx_gear_loader_construct(gl));
        if (!U32(0, 0x00d92ef4u)) return 0;
    }
    rx_vehicle_list_init_loader(s->game + 0x154b0);          /* the FactoryPartsLoader, 00540610 */
    if (!U32(0, 0x00d92ef0u)) return 0;
    /* 006b68e0 (the init thread's 0075e... step): the "AnimationSets" bxml into the animation alias table [0xd5a9e8]. The race
     * intro's CameraAnimation handler (0055d290) resolves each step's AnimName through that table ("CameraStartLine" is a set
     * of per-level clips), so the intro's world camera path (world + 0x218f0) is only set with it. Then ClipRecorder_Init
     * 006aa660 (the init thread's next step): it sizes the sample ring [0x00a7f124] and registers the four rider-clip stunt
     * listeners (006ab290 / 380 / 410 / 4a0) on the stunt channel [0x00a94304], so they run in the product's race. */
    rx_anim_sets_load(fmt_install_vfs(s->install));
    rx_clip_init();
    /* 0063f240 at the init thread's 0075efdb: the camera tuning bxml files (Cameras\MX_Camera.bxml and the other vehicle
     * classes) into the class records 0xaf6d10 / ... The player camera entities made at the spawn call vf32 (0064a330),
     * which reads those records, so they must be loaded before the first spawn. */
    (void)rx_camera_tuning_load_all(fmt_install_vfs(s->install));
    {   /* FUN_0075ed60's game databases, the same EngineDependencies-style database + package pair: RiderPhysics declares the
         * rider's "RiderPhy" / "MXWreckingRagdoll" bxml assets; the rider spawn's 004246c0 reads MXWreckingRagdoll, whose handle
         * the database declares but whose data ([handle + 0x44]) only the package load fills. The original's list here is
         * Animations, RiderPhysics, CameraTweaks, AIData, SnakeDatabase, StuntDatabase, UI_Compound, Music, then the Animations,
         * RiderPhysics, SnakePackage, AIData, StuntData packages; the construction path needs Animations and RiderPhysics. */
        const rx_addr db = U32(U32(0, 0x00a95844), 8);
        /* the rider's animation trees ("MX_RavenRiderAnimationTree" and the other vehicle types) and the model hierarchy come from
         * Animations; the ragdoll / IK document from RiderPhysics */
        rx_database_load(db, s->install, "Animations", "Database\\", 0, 0);
        if (rx_database_load_package(db, s->install, "Animations", "Animations", 0, 0) < 0) return 0;
        rx_database_load(db, s->install, "RiderPhysics", "Database\\", 0, 0);
        if (rx_database_load_package(db, s->install, "RiderPhysics", "RiderPhysics", 0, 0) < 0) return 0;
        /* AIData: the AI personality / behaviour tables (0075ed60's list: after RiderPhysics). 0051e350 runs after the package at
         * 0075f01a, before the StuntData package; without it every AI's start chance is 0 and all of them launch into the gate. */
        rx_database_load(db, s->install, "AIData", "Database\\", 0, 0);
        if (rx_database_load_package(db, s->install, "AIData", "AIData", 0, 0) < 0) return 0;
        rx_aidata_load();
        /* SnakeDatabase / SnakePackage (0075ed60's list after AIData; the package after RiderPhysics): the "SnakeShader" asset whose
         * "SnakeWallVS" / "SnakeWallPS" the Snake and Infection boundary wall takes (0057efb0, w-snake) */
        rx_database_load(db, s->install, "SnakeDatabase", "Database\\", 0, 0);
        if (rx_database_load_package(db, s->install, "SnakeDatabase", "SnakePackage", 0, 0) < 0) return 0;
        /* StuntDatabase / StuntData (0075ed60 at 0075f059), then the stunt descriptor table (006c8480 / 006c8b70 /
         * 006ca5a0 at 0075f103): the StuntTable bxml rows are what the trick system looks a physics stunt up in
         * (006c8570 by name, 006cd000 type -> name); without the table rx_fx_trick_update's [0xd5ab38] gate
         * (ConVar "StuntsEnabled") is set but no stunt resolves, so no trick is counted and the race score
         * (MXPlayer + 0xcec, written by the scorer's refresh at the InProgress exit) stays 0. */
        rx_database_load(db, s->install, "StuntDatabase", "Database\\", 0, 0);
        if (rx_database_load_package(db, s->install, "StuntDatabase", "StuntData", 0, 0) < 0) return 0;
        rx_stunt_tables_init();
        rx_stunt_table_load();
        /* 006ca5a0 (0075ed60 at 0075f103, after the loaders above): the aerial / ground StuntMapping files into
         * [0xa94310] / [0xa94314]; the trick input machine's gesture matcher (006c9aa0) looks its match up there (the
         * aerial file has 36 mappings, the ground file none), so without it a right-stick gesture never resolves. */
        rx_fx_stunt_mappings_init(fmt_install_vfs(s->install));
    }
    return 1;
}

/* The front end's UI world (game + 0x20c8): built as the game's init thread builds it (game/build_ui_world.c). The "UIScene" level is kept open for the
 * session (its scene file serves the UI scene's subscenes). */
static struct fmt_level* g_ui_level;
static int build_ui_world(struct rx_session* s) {
    char name[0x104];
    for (uint32_t i = 0; i < sizeof name; i++) { name[i] = (char)U8(s->game, 0x34 + i); if (!name[i]) break; }
    name[sizeof name - 1] = 0;
    g_ui_level = fmt_level_open(s->install, name);
    if (!g_ui_level) { set_create_error("UI level %s: %s", name, fmt_error()); return 0; }
    if (!rx_ui_world_create(s->game, s->install, g_ui_level)) { set_create_error("the UI world's packages did not load"); return 0; }
    return 1;
}

/* Host side of a session's end, in the order the create built things, then the process statics (rx_reset.h). Used by destroy, by
 * a failed create and by a create the guard caught. Releases: the level, the deformation cache's host buffers, the install, the
 * recorded import slots, the heap's block table, the arena (unmapped: the next create re-reads .data / .rdata of the EXE into a
 * zero mapping, so the game's globals start pristine), then the statics are restored. */
void rx_fx_deform_release(void);   /* game/fx_deform.c */
static void core_release(struct fmt_level* level, fmt_install* install) {
    fmt_modtrack_clear();
    rx_powercross_countdown_unbind();
    rx_career_composition_dispose(); /* clear Single/provider pointers before static restore */
    rx_owned_properties_dispose();
    if (level) fmt_level_free(level);
    if (g_ui_level) { fmt_level_free(g_ui_level); g_ui_level = NULL; }
    rx_fx_deform_release();
    if (install) fmt_install_close(install);
    rx_image_reset();
    rx_heap_reset();
    rx_mem_shutdown();
    rx_reset_restore();
    g_live = 0;
}

struct create_call { const char* root; struct rx_session* s; int ok; };

static void create_impl(struct create_call* c) {
    struct rx_session* s = c->s;
    const char* install_root = c->root;
    c->ok = 0;
    rx_powercross_countdown_reset();
    rx_race_hud_reset();
    rx_vehicle_audio_reset();   /* the sound-layer cue ring (game/vehicle_audio.c) */
    rx_sound_state_reset();     /* the contact / reaction / cue rings (game/sound_state.c, w-sfx) */
    rx_activity_cues_reset();   /* the level's activity audio cues (game/activity_cues.c, w-sfx) */
    rx_music_reset();           /* the music player's product state (game/music_player.c, w-sfx) */
    rx_career_composition_dispose(); /* fresh provider lifetime, no stale Single deps */
    rx_owned_properties_dispose(); /* explicit fresh session, inactive exhibition default */
    if (!rx_mem_init()) { set_create_error("arena reservation failed"); return; }
    /* the host is the operating system: the device state, the performance counter and timeGetTime come from the session (loop_hostinput.h) */
    rx_host_input_install();
    rx_os_set_time_source(rx_host_clock_ms);

    s->install = fmt_install_open(install_root);
    if (!s->install) { set_create_error("install: %s", fmt_error()); goto fail; }
    if (rx_image_load(fmt_install_vfs(s->install)) != 0 || rx_image_load_crt(fmt_install_vfs(s->install)) != 0) {
        set_create_error("image: %s", fmt_error());
        goto fail;
    }
    if (rx_globals_init(s->install) != 0) {
        set_create_error("startup (rx_globals_init) failed");
        goto fail;
    }
    rx_database_set_install(s->install);
    /* TEST ONLY (tests/moto_career_flow --laps): REFLEX_TEST_LAPS=<n> sets the original game's own debug lap override
     * [0x00af6564], which MXActivityRaceStub::vf1 00568050 already applies ("when positive, NumLaps = it"). Unset: untouched. */
    {
        const char* laps = getenv("REFLEX_TEST_LAPS");
        if (laps && atoi(laps) > 0) SU32(0, 0x00af6564, (uint32_t)atoi(laps));
    }

    build_job_pool();
    /* the session's "Network" channel [0x00da6ca0] is the startup's (MXGame::Startup -> 006de390 / 006de4b0, build_mxgame_init.c) */

    s->game = U32(0, RX_GAME_GLOBAL);
    if (!s->game) { set_create_error("no MXGame after the startup"); goto fail; }
    if (U32(s->game, 0x18) != 2) SU32(s->game, 0x18, 2);   /* the front end's running state */
    if (!build_package_loaders(s)) { set_create_error("the game init's package loaders (0053cbd0 / 00540610) did not initialize"); goto fail; }

    if (!build_ui_world(s)) goto fail;

    c->ok = 1;
    return;

fail:
    core_release(NULL, s->install);
    s->install = NULL;
}

static void create_call_run(void* p) { create_impl((struct create_call*)p); }

struct rx_session* rx_session_create(const char* install_root) {
    g_create_error[0] = 0;
    if (!install_root || !*install_root) {
        set_create_error("no install root");
        return NULL;
    }
    if (g_live) {
        set_create_error("a session is already open in this process: destroy it first");
        return NULL;
    }
    const int can_reset = rx_reset_capture();   /* the pristine statics, recorded before the first session writes any */
    if (!can_reset) {
        if (g_dirty) {
            set_create_error("the core was already used in this process and cannot be started twice on this platform: restart the process");
            return NULL;
        }
        g_dirty = 1;
    }

    struct rx_session* s = (struct rx_session*)calloc(1, sizeof *s);
    if (!s) { set_create_error("out of memory (session)"); return NULL; }
    s->magic = RX_SESSION_MAGIC;
    s->version = RX_SESSION_API_VERSION;
    s->state = RX_SESSION_LOADING;

    struct create_call call = {install_root, s, 0};
    char captured[4096];
    g_live = 1;
    const int sig = run_guarded(create_call_run, &call, captured, sizeof captured);
    if (sig) {   /* the arena is left mapped by the fault: release everything so a new create can start */
        char msg[512];
        describe_fault(msg, sizeof msg, "rx_session_create", sig, captured);
        core_release(NULL, s->install);
        set_create_error("%s", msg);
        free(s);
        return NULL;
    }
    if (!call.ok) { g_live = 0; free(s); return NULL; }   /* create_impl released the core and set the error */
    g_career_live_session = s;
    return s;
}

void rx_session_destroy(struct rx_session* s) {
    if (!s) return;
    if (s->magic != RX_SESSION_MAGIC) return;
    rx_replay_host_end(&s->replay_host); /* before provider/static/arena teardown */
    career_public_detach(s);
    if (g_career_live_session == s) g_career_live_session = NULL;
    s->magic = 0;
    terrain_dirty_dispose(&s->terrain_cache);
    free(s->tracks_cache.tris);
    free(s->ambient_objects);
    free(s->ambient_textures);
    free(s->scene_entities);
    free(s->prop_objects);
    free(s->model_ids);
    free(s->water_splats);
    core_release(s->level, s->install);
    free(s);
}

uint32_t rx_session_version(const struct rx_session* s) { return s ? s->version : 0; }

/* ------------------------------------------------------------------------------------------------------------------- event */

void rx_session_event_default(rx_event_spec* spec) { rx_event_spec_default(spec); }

/* w-partylast: the three party modes no installed level holds the content of (EggHunt, Graffiti, Capture; core/plan/QUEUE.md "## w-partylast").
 * The original faults in their activities' start (MXActivity*::vf2) when the level's scene lacks the entities they look up; the session
 * refuses such an event before anything loads (return 3, last_error names what is missing, the session stays usable). NULL = the level
 * has what the mode's start needs (or the event is another mode). */
static int scene_has_named(fmt_scene* sc, int sub, const char* name) {
    if (sub < 0) return 0;
    int n = 0;
    const fmt_entity* e = fmt_scene_entities(sc, sub, &n);
    for (int i = 0; i < n; i++) { const char* nm = fmt_entity_string(&e[i], "Name"); if (nm && !strcmp(nm, name)) return 1; }
    return 0;
}
static int scene_has_type(fmt_scene* sc, int sub, const char* type) {
    return sub >= 0 && fmt_scene_find_type(sc, sub, type, 0) != NULL;
}
static const char* minigame_content_missing(fmt_install* in, const rx_event_spec* spec, char* why, size_t cap) {
    const char* t = spec->event_type;
    const int egg = !strcmp(t, "EggHunt"), gr = !strcmp(t, "Graffiti"), ctf = !strcmp(t, "Capture");
    if (!egg && !gr && !ctf) return NULL;
    fmt_level* lv = fmt_level_open(in, spec->location);
    if (!lv) { snprintf(why, cap, "level %s: %s", spec->location, fmt_error()); return why; }
    fmt_scene* sc = fmt_level_scene(lv);
    char area[160];
    snprintf(area, sizeof area, "%s%s", spec->location, egg ? "MiniGameGreed" : gr ? "MiniGameGraffiti" : "MiniGameCTF");
    const int sub = sc ? fmt_scene_find_subscene(sc, area) : -1;
    why[0] = 0;
    if (egg) {
        /* MXActivityEggHunt::vf2 0057492d: the "GreedModel" entity of the area; its places 00574dd0: a StartEntitySnakeTag there */
        if (!scene_has_named(sc, sub, "GreedModel")) snprintf(why, cap, "EggHunt needs a \"GreedModel\" entity in the subscene %s (no installed level has one)", area);
        else if (!scene_has_type(sc, sub, "StartEntitySnakeTagRSData")) snprintf(why, cap, "EggHunt needs a StartEntitySnakeTag entity in the subscene %s", area);
    } else if (ctf) {
        /* MXActivityCapture::vf2 005765f1 / 00576b60: the flags, spawns and bases of the area */
        static const char* const need[] = {"RedFlag", "BlueFlag", "RedSpawn", "BlueSpawn", "RedBase", "BlueBase"};
        for (int k = 0; k < 6 && !why[0]; k++)
            if (!scene_has_named(sc, sub, need[k])) snprintf(why, cap, "Capture needs a \"%s\" entity in the subscene %s (no installed level has one)", need[k], area);
    } else {
        /* MXActivityGraffiti::vf2 00564151: a StartEntityFreestyle / StartEntityFreeRide in the loaded subscenes (the level's Base and
           the area; the capture points 0x1040 may be absent) */
        const int base = sc && fmt_level_base_subscene(lv) ? fmt_scene_find_subscene(sc, fmt_level_base_subscene(lv)) : -1;
        int ok = 0;
        for (int k = 0; k < 2 && !ok; k++) {
            const int si = k ? sub : base;
            ok = scene_has_type(sc, si, "StartEntityFreestyleRSData") || scene_has_type(sc, si, "StartEntityFreeRideRSData");
        }
        if (!ok) snprintf(why, cap, "Graffiti needs a StartEntityFreestyle or StartEntityFreeRide entity in %s's Base or its subscene %s (no installed level has one)", spec->location, area);
    }
    fmt_level_free(lv);
    return why[0] ? why : NULL;
}

static int event_load_impl(struct rx_session* s, const rx_event_spec* spec) {
    if (!session_ok(s)) return 1;
    if (s->state == RX_SESSION_ERROR) return 1;
    if (!spec) { session_error(s, "no event spec"); return 1; }
    if (s->spec_loaded) { session_error(s, "an event is already loaded in this session"); return 1; }
    if (!s->install) { session_error(s, "no install"); return 1; }

    /* Accepted public lifecycle permits one event per session. Discard any
     * pre-event front-end terrain snapshot before a new event allocates objects. */
    terrain_dirty_invalidate(&s->terrain_cache);
    s->tracks_cache.valid = 0;
    {   char why[300];
        if (minigame_content_missing(s->install, spec, why, sizeof why)) { snprintf(s->error, sizeof s->error, "%s", why); return 3; }
    }
    s->spec = *spec;
    rx_event_set_spec(&s->spec);
    s->level = fmt_level_open(s->install, s->spec.location);
    if (!s->level) { session_error(s, "level %s: %s", s->spec.location, fmt_error()); return 1; }
    rx_event_set_level(s->level);

    const rx_addr ui = U32(0, RX_UI_GLOBAL);
    if (!s->game) { session_error(s, "no MXGame"); return 1; }
    if (U32(s->game, 0x18) != 2) SU32(s->game, 0x18, 2);

    /* StartEvent's path, as build_event_test.c: twice (the original's two EnsureLocalPlayers calls), SetupEvent, StartEvent. */
    rx_ensure_local_players(0, 0);
    rx_ensure_local_players(0, 0);
    rx_setup_event(ui);
    rx_start_event(ui);

    if (U32(s->game, 0x18) != 3) {
        session_error(s, "the level was not requested (MXGame state %u)", U32(s->game, 0x18));
        return 1;
    }
    s->spec_loaded = 1;
    s->state = RX_SESSION_LOADING;
    return 0;
}

/* MAP SCALE / BASE SCALE: the stderr line of the scale the track loads at and whether its records are kept (at the event's load) */
static void map_scale_log(void) {
    const float k = rx_ms_scale(), b = rx_ms_base();
    if (k == 1.0f && b == 1.0f) return;
    if (rx_ms_records_off())
        fprintf(stderr, "rxcore: MAP SCALE %.2f (the track's default %.2f): the track loads scaled; this event keeps no offline records, best laps or highlights\n", (double)k, (double)b);
    else fprintf(stderr, "rxcore: MAP SCALE %.2f: the track's default scale; this event keeps its records\n", (double)k);
}

/* MAP SCALE (port, game/map_scale.h, core/plan/MAP_SCALE.md): the next event's track scaled about the origin. Before event_load only, not
 * in a career session; 0.25 .. 4 (1 = the game's track, bit for bit). */
int rx_session_set_map_scale(struct rx_session* s, float scale) {
    if (!session_ok(s) || s->state == RX_SESSION_ERROR) return 1;
    if (s->spec_loaded || s->career_phase || s->level) { session_error(s, "the map scale is set before the event loads (exhibition only)"); return 2; }
    if (!(scale >= 0.25f && scale <= 4.0f)) { session_error(s, "map scale %g outside 0.25 .. 4", (double)scale); return 2; }
    rx_ms_set(scale);
    return 0;
}
float rx_session_map_scale(const struct rx_session* s) { return s && s->magic == RX_SESSION_MAGIC ? rx_ms_scale() : 1.0f; }

/* BASE SCALE (port, game/map_scale.h): the track's default scale, at which the event keeps its records. Before event_load only. */
int rx_session_set_map_scale_base(struct rx_session* s, float base) {
    if (!session_ok(s) || s->state == RX_SESSION_ERROR) return 1;
    if (s->spec_loaded || s->career_phase || s->level) {   /* refused, the session stays usable (as rx_session_set_practice) */
        snprintf(s->error, sizeof s->error, "the map scale base is set before the event loads (exhibition only)");
        return 2;
    }
    if (!(base >= 0.25f && base <= 4.0f)) { snprintf(s->error, sizeof s->error, "map scale base %g outside 0.25 .. 4", (double)base); return 2; }
    rx_ms_set_base(base);
    return 0;
}
float rx_session_map_scale_base(const struct rx_session* s) { return s && s->magic == RX_SESSION_MAGIC ? rx_ms_base() : 1.0f; }
int32_t rx_session_map_scale_records(const struct rx_session* s) { return s && s->magic == RX_SESSION_MAGIC && !rx_ms_records_off() ? 1 : 0; }

/* MODS/TRACKS (port, src/fmt_db.h fmt_install_mount_level): a custom track's files bound to its level name for this session only. */
int rx_session_mount_level(struct rx_session* s, const char* level, const char* folder) {
    if (!session_ok(s) || s->state == RX_SESSION_ERROR) return 1;
    if (s->spec_loaded || s->career_phase || s->level) { session_error(s, "a custom track is mounted before the event loads (exhibition only)"); return 2; }
    if (!s->install) { session_error(s, "no install"); return 2; }
    const int archive = fmt_rxar_sniff(folder) == 1;   /* a .rxtrack version 5 (RXARCHIVE.md): the level's own files, not a terrain package */
    const int mod = archive ? 0 : fmt_modtrack_open(folder);
    if (mod < 0) { session_error(s, "invalid experimental terrain.rxmod"); return 2; }
    if (mod) rx_session_set_free_ride_borders(s, RX_FREE_RIDE_BORDERS_OPEN);
    const int n = mod ? 0 : fmt_install_mount_level(s->install, level, folder);
    if (n < 0) { session_error(s, "%s", fmt_error()); return 2; }
    rx_ms_set_custom_track(1);   /* no offline records / highlight progress on a custom track (game/map_scale.h) */
    fprintf(stderr, "rxcore: MODS/TRACKS %s read from %s (%d files); this event keeps no offline records or highlights\n", level, folder, n);
    return 0;
}

/* FREER AIR (port, game/free_air.h): the in-air helpers left out for the local players' vehicles; any time between frames. */
int rx_session_set_free_air(struct rx_session* s, int32_t mode) {
    if (!session_ok(s)) return 1;
    if (mode & ~(int32_t)RX_FREE_AIR_ALL) { session_error(s, "free air mode %d: only bits 1 (no trick hold) and 2 (no auto-level)", (int)mode); return 2; }
    if ((uint32_t)mode != rx_fa_mask_value) fprintf(stderr, "rxcore: FREER AIR %s%s%s\n", mode ? "on:" : "off (the original's air helpers)",
                                                     mode & RX_FREE_AIR_NO_HOLD ? " no trick hold" : "", mode & RX_FREE_AIR_NO_SETTLE ? " no auto-level" : "");
    rx_fa_set((uint32_t)mode);
    return 0;
}
/* AUTO LEVEL / COUNTER-STEER SNAP strengths (port, game/free_air.h): 0 .. 1 each, 1 = the original's helper; any time between frames. */
int rx_session_set_air_helpers(struct rx_session* s, float auto_level, float snap) {
    if (!session_ok(s)) return 1;
    if (!(auto_level >= 0.0f && auto_level <= 1.0f) || !(snap >= 0.0f && snap <= 1.0f)) { session_error(s, "air helpers auto-level %g, snap %g: outside 0 .. 1", (double)auto_level, (double)snap); return 2; }
    if (auto_level != rx_fa_level_value || snap != rx_fa_snap_value)
        fprintf(stderr, "rxcore: AIR HELPERS auto-level %.0f %%, counter-steer snap %.0f %%\n", (double)(auto_level * 100.0f), (double)(snap * 100.0f));
    rx_fa_set_helpers(auto_level, snap);
    return 0;
}
float rx_session_air_auto_level(const struct rx_session* s) { return s && s->magic == RX_SESSION_MAGIC ? rx_fa_level_value : 1.0f; }
float rx_session_air_snap(const struct rx_session* s) { return s && s->magic == RX_SESSION_MAGIC ? rx_fa_snap_value : 1.0f; }
int32_t rx_session_free_air(const struct rx_session* s) { return s && s->magic == RX_SESSION_MAGIC ? (int32_t)rx_fa_mask_value : 0; }
/* AIR LOOSENESS (port, game/free_air.h): 0 .. 1, the local players' air damping / stick lag / take-off damping eased; any time. */
int rx_session_set_free_air_looseness(struct rx_session* s, float looseness) {
    if (!session_ok(s)) return 1;
    if (!(looseness >= 0.0f && looseness <= 1.0f)) { session_error(s, "air looseness %g outside 0 .. 1", (double)looseness); return 2; }
    if (looseness != rx_fa_looseness_value) fprintf(stderr, "rxcore: AIR LOOSENESS %.0f %%\n", (double)(looseness * 100.0f));
    rx_fa_set_looseness(looseness);
    return 0;
}
float rx_session_free_air_looseness(const struct rx_session* s) { return s && s->magic == RX_SESSION_MAGIC ? rx_fa_looseness_value : 0.0f; }
/* FREE RIDE WITHOUT BORDERS (port, game/free_ride_borders.h): no reset zones / reset triggers / edge resets for a human in an event
 * world's Free Ride (a fall net past the terrain's real edge stays); any time between frames. */
int rx_session_set_free_ride_borders(struct rx_session* s, int32_t mode) {
    if (!session_ok(s)) return 1;
    if (mode != RX_FREE_RIDE_BORDERS_ORIGINAL && mode != RX_FREE_RIDE_BORDERS_OPEN) { session_error(s, "free ride borders mode %d: 0 (original) or 1 (open)", (int)mode); return 2; }
    if (fmt_modtrack_active()) mode = RX_FREE_RIDE_BORDERS_OPEN; /* the preset's painted borders do not describe this track */
    if ((uint32_t)mode != rx_frb_mode_value) fprintf(stderr, "rxcore: FREE RIDE BORDERS %s\n", mode ? "open (no reset zones, no reset triggers but water, only the fall net past the terrain's edge)" : "original");
    rx_frb_set((uint32_t)mode);
    return 0;
}
int32_t rx_session_free_ride_borders(const struct rx_session* s) { return s && s->magic == RX_SESSION_MAGIC ? (int32_t)rx_frb_mode_value : 0; }
/* WEATHER GRIP (port, game/weather_grip.h): rain / snow, every tyre's static grip times the factor; any time between frames. */
int rx_session_set_weather_grip(struct rx_session* s, float factor) {
    if (!session_ok(s)) return 1;
    if (!(factor >= RX_WEATHER_GRIP_MIN && factor <= RX_WEATHER_GRIP_MAX)) { session_error(s, "weather grip %g outside %g .. %g", (double)factor, (double)RX_WEATHER_GRIP_MIN, (double)RX_WEATHER_GRIP_MAX); return 2; }
    const int was = (int)(g_rx_weather_grip * 100.0f + 0.5f), now = (int)(factor * 100.0f + 0.5f);
    if (was != now) fprintf(stderr, "rxcore: WEATHER GRIP %d %% of the tyres' grip%s\n", now, factor == 1.0f ? " (the original's)" : "");
    return rx_weather_grip_set(factor);
}
float rx_session_weather_grip(const struct rx_session* s) { return s && s->magic == RX_SESSION_MAGIC ? g_rx_weather_grip : 1.0f; }

struct event_call { struct rx_session* s; const rx_event_spec* spec; int rc; };
static void event_call_run(void* p) { struct event_call* c = (struct event_call*)p; c->rc = event_load_impl(c->s, c->spec); }

int rx_session_event_load(struct rx_session* s, const rx_event_spec* spec) {
    if (!session_ok(s)) return 1;
    if (spec && (spec->riders < 0 || spec->riders > RX_MAX_RIDERS)) { session_error(s, "EventRiders %d outside 0 .. %d", (int)spec->riders, RX_MAX_RIDERS); return 1; }
    if(s->career_phase){snprintf(s->error,sizeof s->error,"Exhibition load cannot replace configured Career");return 2;}
    rx_scenario_sounds_clear();   /* the previous event's unread scenario sounds (rx_session_scenario_sounds) */
    map_scale_log();              /* MAP SCALE / BASE SCALE: the scale the track loads at, and whether its records are kept */
    struct event_call call = {s, spec, 1};
    char captured[4096];
    const int sig = run_guarded(event_call_run, &call, captured, sizeof captured);
    if (sig) {
        char msg[512];
        describe_fault(msg, sizeof msg, "rx_session_event_load", sig, captured);
        session_error(s, "%s", msg);
        return 1;
    }
    return call.rc;
}

/* Career-only host entry. Do not route through exhibition event_load_impl. */
#ifndef RX_ORACLE
struct career_load_call {
 struct rx_session*s;rx_addr manager;rx_career_start_dependencies start;
 rx_career_route_dependencies routes;int prepared,rc;
};
/*The core permits one owning session/worker. This pointer exists only during
 * guarded synchronous manager start, and is cleared after both return/fault.*/
static struct career_load_call* career_load_active;
static int career_text(struct rx_session*s,rx_addr key,char*output,size_t capacity){
 RX_FRAME_ENTER(F,256);
 if(!rx_owned_property_get_string(key,F,256)){RX_FRAME_LEAVE(F,256);session_error(s,"Career required string property %08x missing",key);return 0;}
 size_t n=0;while(U8(F,(uint32_t)n))n++;
 if(n>=capacity){RX_FRAME_LEAVE(F,256);session_error(s,"Career property %08x exceeds session metadata field",key);return 0;}
 for(size_t i=0;i<=n;i++)output[i]=(char)U8(F,(uint32_t)i);
 RX_FRAME_LEAVE(F,256);return 1;
}
static int career_int(struct rx_session*s,rx_addr key,int32_t*out){
 RX_FRAME_ENTER(F,4);if(!rx_owned_property_get_int(key,F,0)){RX_FRAME_LEAVE(F,4);session_error(s,"Career required integer property %08x missing",key);return 0;}
 *out=I32(F,0);RX_FRAME_LEAVE(F,4);return 1;
}
static uint32_t career_asset_prepare(rx_addr requested){
 struct career_load_call*c=career_load_active;if(!c)return 0;struct rx_session*s=c->s;
 if(c->prepared||s->level){session_error(s,"Career attempted more than one level preparation");return 0;}
 rx_event_spec next=s->spec;
 if(!career_text(s,0x935e08,next.event_type,sizeof next.event_type)||
    !career_text(s,0x93e7c4,next.location,sizeof next.location)||
    !career_text(s,0x936e58,next.duration_type,sizeof next.duration_type)||
    !career_text(s,0x94be84,next.player_mode,sizeof next.player_mode))return 0;
 /*Location callback and actual owned property must identify the same asset.*/
 size_t i=0;for(;;i++){unsigned a=U8(requested,(uint32_t)i),b=(unsigned char)next.location[i];if(a!=b){session_error(s,"Career level request differs from actual EventLocation");return 0;}if(!a)break;}
 if(!next.location[0]){session_error(s,"Career EventLocation is empty");return 0;}
 if(!career_int(s,0x936e20,&next.location_id)||!career_int(s,0x936e40,&next.track_id)||
    !career_int(s,0x936e6c,&next.duration)||!career_int(s,0x94bfcc,&next.riders)||
    !career_int(s,0x936e48,&next.ai_difficulty)||!career_int(s,0x935df8,&next.vehicle_class))return 0;
 RX_FRAME_ENTER(F,64);for(i=0;i<=strlen(next.event_type);i++)SU8(F,(uint32_t)i,(uint8_t)next.event_type[i]);
 uint32_t kind=rx_event_type_of(F);RX_FRAME_LEAVE(F,64);
 if(kind>=0x3e){session_error(s,"Career EventType has no original enum entry");return 0;}
 next.event_type_id=(int32_t)rx_event_type_id_of(kind);
 if(next.event_type_id==-1){session_error(s,"Career EventType has no authored table row");return 0;}
 /*Activation is optional. Absence means absent, never exhibition FreeRide.*/
 next.event_activation[0]=0;
 RX_FRAME_ENTER(A,256);if(rx_owned_property_get_string(0x94c320,A,256)){
  size_t n=0;while(U8(A,(uint32_t)n))n++;
  if(n>=sizeof next.event_activation){RX_FRAME_LEAVE(A,256);session_error(s,"Career activation exceeds metadata field");return 0;}
  for(i=0;i<=n;i++)next.event_activation[i]=(char)U8(A,(uint32_t)i);
 }RX_FRAME_LEAVE(A,256);
 terrain_dirty_invalidate(&s->terrain_cache);s->tracks_cache.valid=0;
 fmt_level*level=fmt_level_open(s->install,next.location);
 if(!level){session_error(s,"Career level %s: %s",next.location,fmt_error());return 0;}
 s->level=level;s->spec=next;rx_event_set_spec(&s->spec);rx_event_set_level(level);c->prepared=1;return 1;
}
static void career_host_route(rx_addr out,uint32_t size,uint32_t kind,uint32_t mode,uint32_t players,rx_addr prefix){
 rx_career_build_route(out,size,kind,mode,players,prefix,&career_load_active->routes);
}
static uint32_t career_host_compare(rx_addr route){
 uint32_t accepted=rx_career_compare_route(U32(RX_UI_GLOBAL,0),route,&career_load_active->routes);
 if(!accepted&&U32(RX_UI_GLOBAL,0)==0){session_error(career_load_active->s,"Career owned presentation route not accepted");act_unreached(0x5a4350,"unsupported/unacknowledged owned Career route");}
 return accepted;
}
static void career_host_request(rx_addr ui,uint32_t changed){rx_career_request_level(ui,changed,&career_load_active->routes);}
static void career_load_run(void*opaque){
 struct career_load_call*c=opaque;struct rx_session*s=c->s;
 /* An initialized Tour series of a composed format (career_composition.c: the Single-class formats and Moto), the owned
  * properties active and the game in its front-end state 2. */
 const int initialized = rx_owned_properties_active() && c->manager == 0x9e2188 && U32(c->manager, 0) && U32(c->manager, 0x18c) &&
                         rx_career_composition_format_supported(c->manager) && U32(c->manager, 0x190) == 1 && U32(s->game, 0x18) == 2;
 if (!initialized) {
  snprintf(s->error, sizeof s->error, "Career requires an initialized Tour series of a composed format, active properties and game state 2");
  c->rc = 2;
  return;
 }
 rx_addr ui=U32(RX_UI_GLOBAL,0);
 if((!ui&&!c->routes.compare_owned_route)||(ui&&(!c->routes.acquire_route||!c->routes.release_route))){snprintf(s->error,sizeof s->error,"Career presentation route provider is incomplete");c->rc=2;return;}
 /*PlayerMode remains an older product spec consumer. Seed only this actual
  * owned value BEFORE native start; other metadata is captured after start
  * publishes its actual properties, at asset preparation. No player rows reset.*/
 if(!career_text(s,0x94be84,s->spec.player_mode,sizeof s->spec.player_mode)){c->rc=1;return;}
 /* The profile's display name (MXProfile_GetDisplayName's product answer, build_activity_setup.c) stays the one the local players
  * were made with (EnsureLocalPlayers at configure: the Player List's MXProfileName, the series' PlayerName): a career session has no
  * exhibition spec, so its zero name must not replace it (the Heat event's vf13 looks its local rider up by that name). */
 if(!s->spec.player_name[0])memcpy(s->spec.player_name,rx_event_spec_get()->player_name,sizeof s->spec.player_name);
 rx_event_set_spec(&s->spec);
 if(rx_game_player_mode()!=1){session_error(s,"Career host entry currently supports actual SinglePlayer only");c->rc=1;return;}
 c->routes.get_int=rx_career_property_int;c->routes.get_string=rx_career_property_string;c->routes.prepare_level=career_asset_prepare;
 c->start.ui_int=rx_owned_property_set_int;c->start.ui_string=rx_owned_property_set_string;c->start.ui_get_int=rx_career_property_int;
 c->start.build_route=career_host_route;c->start.compare_update_route=career_host_compare;c->start.request_configured_level=career_host_request;
 rx_career_start_event(c->manager,&c->start);
 if(!c->prepared||!s->level||U32(s->game,0x18)!=3){session_error(s,"Career did not issue a real prepared state3 level request");c->rc=1;return;}
 if(s->state==RX_SESSION_ERROR){c->rc=1;return;}
 s->spec_loaded=1;s->state=RX_SESSION_LOADING;c->rc=0;
}
int rx_career_session_start_owned(struct rx_session*s,rx_addr manager,const rx_career_start_dependencies*start,const rx_career_route_dependencies*routes){
 if(!session_ok(s)||s->state==RX_SESSION_ERROR)return 1;
 if(s->spec_loaded||s->records_finalized||s->level||!s->install||!s->game||career_load_active){snprintf(s->error,sizeof s->error,"Career requires fresh owning pre-event session");return 2;}
 if(!start||!start->event_call||!start->manager_refresh||!start->reset_player_requests||!routes||!routes->session_metadata){snprintf(s->error,sizeof s->error,"Career event/manager/package/session provider is incomplete");return 2;}
 struct career_load_call call={0};call.s=s;call.manager=manager;call.start=*start;call.routes=*routes;call.rc=1;
 career_load_active=&call;char captured[4096];int sig;
 /* Public career actions already own this exact session fault guard. Avoid
  * replacing its signal handlers/TLS jump target with a nested guard. The outer
  * dispatcher clears career_load_active on either return or caught fault. */
 if(s==g_career_live_session&&s->career_busy&&t_guard_env){career_load_run(&call);sig=0;}
 else sig=run_guarded(career_load_run,&call,captured,sizeof captured);
 career_load_active=NULL;
 if(sig){char msg[512];describe_fault(msg,sizeof msg,"rx_career_session_start_owned",sig,captured);if(s->state!=RX_SESSION_ERROR)session_error(s,"%s",msg);return 1;}
 return call.rc;
}
#endif

/* Profile writes share the session worker and the same fault boundary as event construction. */
struct profile_call { struct rx_session* s; const rx_profile_cell* cells; int count, mode, rc; int32_t* values; };
static void profile_call_run(void* p) {
    struct profile_call* c = p;
    if (c->mode == 0) c->rc = rx_ui_profile_import(c->cells, c->count, c->s->error, sizeof c->s->error);
    else if (c->mode == 1) c->rc = rx_ui_profile_tune(c->values, c->s->error, sizeof c->s->error);
    else c->rc = rx_ui_profile_tuning_read(c->values, c->s->error, sizeof c->s->error);
}
static int profile_guard(struct profile_call* c) {
    char captured[4096];
    int sig = run_guarded(profile_call_run, c, captured, sizeof captured);
    if (sig) { char msg[512]; describe_fault(msg, sizeof msg, "rx_session_profile", sig, captured); session_error(c->s, "%s", msg); return 1; }
    return c->rc == 0 ? 0 : 2;
}
int rx_session_profile_import(struct rx_session* s, const rx_profile_cell* cells, int32_t count) {
    if (!session_ok(s) || s->state == RX_SESSION_ERROR) return 1;
    if (s->spec_loaded||s->career_phase) { snprintf(s->error,sizeof s->error,"Profile selection must be applied before career configure/event load"); return 2; }
    struct profile_call c = {s,cells,count,0,-1,NULL}; return profile_guard(&c);
}
int rx_session_tuning_set(struct rx_session* s, const int32_t values[8]) {
    if (!session_ok(s) || s->state == RX_SESSION_ERROR) return 1;
    struct profile_call c = {s,NULL,0,1,-1,(int32_t*)values}; return profile_guard(&c);
}
int rx_session_tuning_get(struct rx_session* s, int32_t values[8]) {
    if (!session_ok(s) || s->state == RX_SESSION_ERROR) return 1;
    struct profile_call c = {s,NULL,0,2,-1,values}; return profile_guard(&c);
}

/* Full profile calls use the same single arena/fault boundary as event frames.
 * The input clone is host-owned outside run_guarded, so native longjmp cannot
 * leak it. No live arena pointer crosses the public API. */
struct player_snapshot_call { struct rx_session* s; void* data; uint32_t bytes; uint32_t* required; int write,rc; };
static void player_snapshot_run(void* ptr) {
    struct player_snapshot_call* c=ptr;
    c->rc=c->write?rx_player_snapshot_import(c->data,c->bytes,c->s->error,sizeof c->s->error)
        :rx_player_snapshot_export(c->data,c->bytes,c->required,c->s->error,sizeof c->s->error);
}
static int player_snapshot_guard(struct player_snapshot_call* c) {
    char captured[4096];int sig=run_guarded(player_snapshot_run,c,captured,sizeof captured);
    if(sig){char msg[512];describe_fault(msg,sizeof msg,"rx_session_player_snapshot",sig,captured);session_error(c->s,"%s",msg);return 1;}
    return c->rc?2:0;
}
int rx_session_player_export(struct rx_session* s,void* output,uint32_t capacity,uint32_t* required) {
    if(!session_ok(s)||s->state==RX_SESSION_ERROR)return 1;
    if(!required||(!output&&capacity)){snprintf(s->error,sizeof s->error,"Invalid Player export buffer");return 2;}
    struct player_snapshot_call c={s,output,capacity,required,0,-1};return player_snapshot_guard(&c);
}
int rx_session_player_import(struct rx_session* s,const void* input,uint32_t bytes) {
    if(!session_ok(s)||s->state==RX_SESSION_ERROR)return 1;
    if(s->spec_loaded||s->career_phase){snprintf(s->error,sizeof s->error,"Player snapshot import requires pre-configuration event state");return 2;}
    if(!input||bytes<sizeof(rx_player_snapshot_header)||bytes>RX_PLAYER_SNAPSHOT_MAX_BYTES){snprintf(s->error,sizeof s->error,"Invalid Player snapshot bytes");return 2;}
    void* copy=malloc(bytes);if(!copy){snprintf(s->error,sizeof s->error,"Player snapshot allocation failed before mutation");return 2;}
    memcpy(copy,input,bytes);struct player_snapshot_call c={s,copy,bytes,NULL,1,-1};int rc=player_snapshot_guard(&c);free(copy);return rc;
}

/* Explicit final-leave record settlement, not full activity destruction or a
 * synthesized finish. Mirror PlayerLeft0055c556..580 guards; canonical leave
 * handles the stored row, working-row deletion and original-derived highlights.
 * This API intentionally leaves the arena alive for a subsequent export. */
struct finalize_records_call { struct rx_session* s; int rc; };
static void finalize_records_run(void* ptr) {
    struct finalize_records_call* c=ptr;
    const rx_addr p=local_player(),a=p?U32(p,0x94):0,records=U32(0,0x00a946c0);
    c->rc=2;
    if(!a){snprintf(c->s->error,sizeof c->s->error,"Record settlement requires the loaded local activity");return;}
    if(U32(a,0x238)==4||!records){c->rc=0;return;}
    const int32_t count=I32(a,0x26c);const rx_addr list=U32(a,0x268);
    if(count<0||count>16||(count&&!list)){snprintf(c->s->error,sizeof c->s->error,"Invalid activity driver list for settlement");return;}
    /* Validate all eligible slots before the first mutation. Repeated slots are
     * harmless in original leave but would obscure ownership, so reject. */
    uint32_t slots[4],n=0,seen=0;
    for(int32_t i=0;i<count;i++){
        const rx_addr driver=U32(list,4u*(uint32_t)i),player=driver?U32(driver,0x74):0;
        if(!player||!U32(player,4)||U32(player,0x10)!=0)continue;
        const int32_t slot=I32(player,0x1f0);if(slot<0)continue;
        if(slot>=4||(seen&(1u<<(uint32_t)slot))){snprintf(c->s->error,sizeof c->s->error,"Invalid/duplicate local record slot");return;}
        seen|=1u<<(uint32_t)slot;
        if(U32(records,0x18u*(uint32_t)slot+4)!=0xffffffffu)slots[n++]=(uint32_t)slot;
    }
    for(uint32_t i=0;i<n;i++)rx_mxrecords_leave(records,slots[i]);
    c->rc=0;
}
int rx_session_finalize_records(struct rx_session* s) {
    if(!session_ok(s)||s->state==RX_SESSION_ERROR)return 1;
    if(s->records_finalized)return 0;
    if(s->replay_host.count)return RX_CAREER_BUSY;
    if(!s->spec_loaded||s->state==RX_SESSION_LOADING){snprintf(s->error,sizeof s->error,"Record settlement requires a loaded event; loading is not finalizable");return 2;}
    struct finalize_records_call call={s,2};char captured[4096];
    const int sig=run_guarded(finalize_records_run,&call,captured,sizeof captured);
    if(sig){char msg[512];describe_fault(msg,sizeof msg,"rx_session_finalize_records",sig,captured);session_error(s,"%s",msg);return 1;}
    if(call.rc==0)s->records_finalized=1;
    return call.rc;
}

/* Native controls: defaults never touch the single session arena. */
int rx_remap_defaults(rx_remap_table* out) { return rx_ui_remap_defaults(out); }
struct remap_call { struct rx_session* s; rx_remap_table* out; const int32_t* values; int write, rc; };
static int remap_rebuild_local(void);   /* fbhud_session.inc */
static void remap_call_run(void* p) {
    struct remap_call* c = p;
    c->rc = c->write ? rx_ui_remap_set(c->values, c->s->error, sizeof c->s->error)
                    : rx_ui_remap_get(c->out, c->s->error, sizeof c->s->error);
    if (c->write == 2 && c->rc == 0) (void)remap_rebuild_local();   /* rx_session_remap_apply in a loaded event (fbhud_session.inc) */
}
static int remap_guard(struct remap_call* c) {
    char captured[4096];
    int sig = run_guarded(remap_call_run, c, captured, sizeof captured);
    if (sig) { char msg[512]; describe_fault(msg, sizeof msg, "rx_session_remap", sig, captured); session_error(c->s, "%s", msg); return 1; }
    return c->rc == 0 ? 0 : 2;
}
int rx_session_remap_get(struct rx_session* s, rx_remap_table* out) {
    if (!out || !session_ok(s) || s->state == RX_SESSION_ERROR) return 1;
    struct remap_call c = {s, out, NULL, 0, -1}; return remap_guard(&c);
}
int rx_session_remap_set(struct rx_session* s, const int32_t values[38]) {
    if (!session_ok(s) || s->state == RX_SESSION_ERROR) return 1;
    if (s->spec_loaded) { snprintf(s->error, sizeof s->error, "Controls must be applied before event load"); return 2; }
    struct remap_call c = {s, NULL, values, 1, -1}; return remap_guard(&c);
}

/* ------------------------------------------------------------------------------------------------------------------- frame */

/* The activity state the session state mirrors: A+0x21c (Activity::vf24 0x558930 dispatches on it):
 * 0 Pre, 1 Countdown, 2 InProgress, 3 WindDown, 4 Post, 5 Inactive. */
static int32_t activity_state(rx_addr p) {
    const rx_addr a = U32(p, 0x94);
    return a ? I32(a, 0x21c) : -1;
}

static rx_addr race_driver_for(rx_addr p, rx_addr* activity) {
    const rx_addr a = U32(p, 0x94);
    if (activity) *activity = a;
    if (!a) return 0;
    const rx_addr list = U32(a, 0x268);
    const uint32_t n = U32(a, 0x26c);
    for (uint32_t i = 0; list && i < n && i < 64; i++) {
        const rx_addr d = U32(list, 4u * i);
        if (d && U32(d, 0x74) == p) return d;
    }
    return 0;
}

static int test_drive_spawning(const struct rx_session* s);   /* test_drive_session.inc */
static void lap_log_before_frame(void);                       /* practice_session.inc */
static void lap_log_after_frame(void);
static int tutorial_spawning(const struct rx_session* s);     /* tutorial_session.inc */
static void test_drive_loading_page_frame(struct rx_session* s);   /* test_drive_session.inc */
static void tutorial_loading_page_frame(struct rx_session* s);     /* tutorial_session.inc */
static void update_state(struct rx_session* s) {
    if (s->state == RX_SESSION_ERROR) return;
    if (!s->game) { s->state = RX_SESSION_LOADING; return; }
    if (U32(s->game, 0x18) != 2) { s->state = RX_SESSION_LOADING; return; }
    const rx_addr p = local_player();
    if (!p) { s->state = RX_SESSION_LOADING; return; }
    if (test_drive_spawning(s)) { s->state = RX_SESSION_LOADING; return; }   /* the Test Drive Loading Page: until the slot spawned */
    if (tutorial_spawning(s)) { s->state = RX_SESSION_LOADING; return; }     /* the Tutorial Load Page: until the slot spawned */
    const int32_t st = activity_state(p);
    rx_addr a = 0;
    const rx_addr d = race_driver_for(p, &a);
    if (d && U32(d, 0x70)) { s->state = RX_SESSION_FINISHED; return; }   /* the local driver's finish flag */
    switch (st) {
        case 0: case 1: s->state = RX_SESSION_COUNTDOWN; break;
        case 2: s->state = RX_SESSION_RACING; break;
        case 3: case 4: case 5: s->state = RX_SESSION_FINISHED; break;
        default: s->state = RX_SESSION_LOADING; break;
    }
}

/* The game's frame step: Engine::UpdateTiming's frame cap (Engine 0xd728d0 +0x14; FLT_MAX = uncapped -> 0). */
static float step_dt(void) {
    const float cap = F32(0, RX_ENGINE_GLOBAL + 0x14);
    return cap > 0.0f && cap < 3.0e38f ? cap : 0.0f;
}

/* the registered terrain of the active view (defined with the terrain getters below) */
static rx_addr active_terrain(void);

struct frame_call { struct rx_session* s; uint64_t ticks; };
/* What the session runs at the render's place of the original's frame (Engine::Frame 0084f260: after Engine::Update and the update
 * counter, before the Database's end-of-frame removal and the render counter; loop_engine.c rx_engine_frame). Unity draws later, from
 * the exported state; this is the game state the original's render pass and render thread produce, run by the game's own code. */
static void water_splats_capture(struct rx_session* s);   /* water_session.inc */
static void frame_render_run(struct frame_call* c) {
    /* Owned dashboard component update uses the actual original engine dt.
     * The managed tree consumes its copied state, never advances another timer. */
    rx_powercross_countdown_tick(F32(RX_ENGINE_GLOBAL,0x18));
    rx_race_hud_tick(local_player());   /* the HUD components' per-frame copies (game/race_hud.c) */
    /* RENDER BOUNDARY (documented): the original's render thread calls HFTerrainSurface::vf14 (007c4fa0) once per rendered
     * frame on the active terrain: GetRenderTargetData of the deformation target, the three TerrainTracks_FlushPending calls
     * and the level-reset clears. The core has no renderer, so the session runs the game's own vf14 here, after the frame's
     * update, exactly where the render pass would: the deformation grid the getters/handlers read and the real ribbon
     * vertices TerrainTracks_Draw submits are produced by the game's code, not by the API. */
    {
        const rx_addr terrain = active_terrain();
        /* Observe disappearance even if the renderer skipped its getter. The
         * original reset flag is consumed by sync, so invalidate before it clears.
         * This prevents a recycled pointer / same-buffer level reset using an old
         * export generation. Host cache only; no guest flag is written here. */
        if (!terrain || U32(terrain,0x44b8)!=0)
            terrain_dirty_invalidate(&c->s->terrain_cache);
        if (terrain) {
            rx_fx_terrain_sync(terrain);
            c->s->tracks_version++;
        }
    }
    /* RENDER BOUNDARY (documented, water_session.inc): the queue part of the original's Wake_Step 007ed300 (WakeSimRenderEvent) */
    water_splats_capture(c->s);
}
static void frame_render_hook(void* p) {
    struct frame_call* c = (struct frame_call*)p;
    frame_render_run(c);
    /* RENDER BOUNDARY: the character entities drawn this frame stamp the render counter (loop_engine.c rx_engine_render_stamps) */
    (void)rx_engine_render_stamps(RX_ENGINE_GLOBAL);
}
/* One frame of the original's loop (Engine::Frame 0084f260, loop_engine.c rx_engine_frame): the host's clock moves on by the real
 * time that passed, then Engine::Update (0084ed20): UpdateTiming reads the counter, the input manager polls the devices (the
 * keyboard, pads and mouse sums the host set), the key maps update, the scenes tick, MXGame::Update; the update counter (engine
 * +0x58); the render's place (frame_render_hook); the Database's end-of-frame removal of the handles queued for removal (vf33 with
 * the render counter +0x54) and the render counter. */
static void frame_call_run(void* p) {
    struct frame_call* c = (struct frame_call*)p;
    rx_replay_host_frame(&c->s->replay_host,c->s->frames+1);
    rx_host_clock_advance(c->ticks);
    /* Product sessions always run the complete original loop. The former environment-only
     * opt-in could be invisible to the Windows CRT after a managed OS environment update.
     * Keep the old full-frame override authoritative; partial frames are an explicit developer control. */
    const char* partial = getenv("RX_PARTIAL_FRAME");
    if (getenv("RX_FULL_FRAME") || !partial || strcmp(partial, "1") != 0) {
        if (g_rx_ps_armed) rx_ps_guard();   /* PHYSICS STUDIO (game/phys_studio.h): a vehicle the config made diverge gets the file's values back */
        rx_engine_set_render_hook(frame_render_hook, c);
        rx_engine_frame(RX_ENGINE_GLOBAL);
        rx_engine_set_render_hook(NULL, NULL);
    } else {
        /* Explicit developer-only partial loop: timing, input/key maps, then MXGame::Update.
         * This omits the scene tick and the frame's tail (counters, the end-of-frame removal): not a playable product session. */
        rx_engine_frame_head(RX_ENGINE_GLOBAL);
        rx_mxgame_update(c->s->game);
        frame_render_run(c);
    }
    /* the players' loading pages (the tutorial's / the test drive's MXUIPlayersLoadingPage): their exit once the local slot spawned */
    test_drive_loading_page_frame(c->s);
    tutorial_loading_page_frame(c->s);
}

int rx_session_advance_dt(struct rx_session* s, float dt) {
    if (!session_ok(s)) return 1;
    if (s->state == RX_SESSION_ERROR) return 1;
    if(s->replay_host.bound&&s->replay_host.count)return RX_CAREER_BUSY;
    if(s->replay_event&&!s->replay_host.bound){snprintf(s->error,sizeof s->error,"Exhibition replay host ended; further simulation is prohibited");return 2;}
    if(s->records_finalized){snprintf(s->error,sizeof s->error,"Session records finalized; further simulation is prohibited");return 2;}
    if (!s->spec_loaded) { session_error(s, "no event loaded"); return 1; }
    if (!s->game) { session_error(s, "no MXGame"); return 1; }

    if (!(dt > 0.0f) || dt > 3600.0f) dt = dt > 3600.0f ? 3600.0f : 0.0f;   /* NaN / negative: no time passed (the frame cap waits) */
    s->last_dt = dt;
    struct frame_call call = {s, (uint64_t)ceil((double)dt * (double)RX_HOST_QPF)};
    char captured[4096];
    lap_log_before_frame();   /* the LAP LOG (port addition, practice_session.inc): reads only */
    const int sig = run_guarded(frame_call_run, &call, captured, sizeof captured);
    if (sig) {
        char msg[512];
        describe_fault(msg, sizeof msg, "rx_session_advance", sig, captured);
        session_error(s, "%s (frame %llu)", msg, (unsigned long long)s->frames);
        return 1;
    }
    s->frames++;
    lap_log_after_frame();
    update_state(s);
    return s->state == RX_SESSION_ERROR ? 1 : 0;
}

/* No real time given: the frame runs when the game's frame cap says (UpdateTiming's busy wait ends at the cap). */
int rx_session_advance(struct rx_session* s) { return rx_session_advance_dt(s, 0.0f); }

int rx_session_set_input(struct rx_session* s, const uint8_t* keys, const rx_pad* pads, uint32_t pad_mask) {
    if (!session_ok(s)) return 1;
    /* the device state the game's own poll reads at the next frame (loop_hostinput.h): no game record is written here */
    _Static_assert(sizeof(rx_pad) == sizeof(rx_host_pad), "rx_pad is XINPUT_GAMEPAD");
    rx_host_input_set(keys, (const rx_host_pad*)(const void*)pads, pad_mask);
    return 0;
}

struct mouse_call { int32_t dx, dy, wheel; uint32_t flags; };
static void mouse_call_run(void* p) { struct mouse_call* c = (struct mouse_call*)p; rx_host_mouse_post(c->dx, c->dy, c->wheel, c->flags); }

int rx_session_set_mouse(struct rx_session* s, int32_t dx, int32_t dy, int32_t wheel, uint32_t button_flags) {
    if (!session_ok(s)) return 1;
    if (s->state == RX_SESSION_ERROR) return 1;
    if (!U32(0, 0x00a95844) || !U32(U32(0, 0x00a95844), 0x34)) return 1;   /* no input manager yet */
    struct mouse_call call = {dx, dy, wheel, button_flags};
    char captured[4096];
    const int sig = run_guarded(mouse_call_run, &call, captured, sizeof captured);
    if (sig) {
        char msg[512];
        describe_fault(msg, sizeof msg, "rx_session_set_mouse", sig, captured);
        session_error(s, "%s", msg);
        return 1;
    }
    return 0;
}

void rx_session_set_cursor(struct rx_session* s, int32_t x, int32_t y) {
    if (session_ok(s)) rx_host_input_cursor(x, y);
}

int rx_session_rumble(struct rx_session* s, uint32_t pad, uint16_t* left, uint16_t* right, uint32_t* calls) {
    if (!session_ok(s) || pad >= 4) return 1;
    const uint32_t n = rx_host_input_rumble(pad, left, right);
    if (calls) *calls = n;
    return n ? 0 : 1;
}

float rx_session_step_dt(const struct rx_session* s) { return session_ok(s) ? step_dt() : 0.0f; }

uint32_t rx_session_pace(struct rx_session* s, float real_dt, uint32_t max_steps) {
    if (!session_ok(s)) return 0;
    if(s->replay_host.bound&&s->replay_host.count)return 0;
    if (!(real_dt > 0.0f) || real_dt > 3600.0f) real_dt = real_dt > 3600.0f ? 3600.0f : 0.0f;
    const double step = (double)step_dt();
    if (step <= 0.0) { s->pace_acc = 0.0; return real_dt > 0.0f && max_steps ? 1u : 0u; }   /* uncapped: one frame per call */
    s->pace_acc += (double)real_dt;
    double due = floor(s->pace_acc / step);
    s->pace_acc -= due * step;
    if (s->pace_acc < 0.0) s->pace_acc = 0.0;
    return due > (double)max_steps ? max_steps : (uint32_t)due;   /* a backlog beyond max_steps is dropped (the fraction stays) */
}

float rx_session_alpha(const struct rx_session* s) {
    const double step = (double)step_dt();
    if (!session_ok(s) || step <= 0.0) return 0.0f;
    const double a = s->pace_acc / step;
    return (float)(a < 0.0 ? 0.0 : a > 1.0 ? 1.0 : a);
}

float rx_session_engine_dt(const struct rx_session* s) { return session_ok(s) ? F32(0, RX_ENGINE_GLOBAL + 0x18) : 0.0f; }

int32_t rx_session_sizeof(const char* name) {
    static const struct { const char* name; int32_t size; } k_sizes[] = {
        {"rx_event_spec", (int32_t)sizeof(rx_event_spec)},
        {"rx_pad", (int32_t)sizeof(rx_pad)},
        {"rx_bike_state", (int32_t)sizeof(rx_bike_state)},
        {"rx_onboard_state", (int32_t)sizeof(rx_onboard_state)},
        {"rx_camera_bindings", (int32_t)sizeof(rx_camera_bindings)},
        {"rx_remap_table", (int32_t)sizeof(rx_remap_table)},
        {"rx_camera", (int32_t)sizeof(rx_camera)},
        {"rx_race_state", (int32_t)sizeof(rx_race_state)},
        {"rx_event_ui_state", (int32_t)sizeof(rx_event_ui_state)},
        {"rx_driver_result", (int32_t)sizeof(rx_driver_result)},
        {"rx_slot", (int32_t)sizeof(rx_slot)},
        {"rx_appearance", (int32_t)sizeof(rx_appearance)},
        {"rx_profile_cell", (int32_t)sizeof(rx_profile_cell)},
        {"rx_vehicle_info", (int32_t)sizeof(rx_vehicle_info)},
        {"rx_hud_state", (int32_t)sizeof(rx_hud_state)},
        {"rx_race_hud_state", (int32_t)sizeof(rx_race_hud_state)},
        {"rx_mode_results", (int32_t)sizeof(rx_mode_results)},
        {"rx_stunt_hud_state", (int32_t)sizeof(rx_stunt_hud_state)},
        {"rx_reflex_hud_state", (int32_t)sizeof(rx_reflex_hud_state)},
        {"rx_vehicle_audio_state", (int32_t)sizeof(rx_vehicle_audio_state)},
        {"rx_sound_state", (int32_t)sizeof(rx_sound_state)},
        {"rx_rng_state", (int32_t)sizeof(rx_rng_state)},
        {"rx_results_page", (int32_t)sizeof(rx_results_page)},
        {"rx_results_row", (int32_t)sizeof(rx_results_row)},
        {"rx_waypoint_state", (int32_t)sizeof(rx_waypoint_state)},
        {"rx_terrain_info", (int32_t)sizeof(rx_terrain_info)},
        {"rx_terrain_rect", (int32_t)sizeof(rx_terrain_rect)},
        {"rx_particle", (int32_t)sizeof(rx_particle)},
        {"rx_particle_info", (int32_t)sizeof(rx_particle_info)},
        {"rx_ambient_info", (int32_t)sizeof(rx_ambient_info)},
        {"rx_ambient_entity", (int32_t)sizeof(rx_ambient_entity)},
        {"rx_scene_object", (int32_t)sizeof(rx_scene_object)},
        {"rx_prop", (int32_t)sizeof(rx_prop)},
        {"rx_mud_state", (int32_t)sizeof(rx_mud_state)},
        {"rx_track_info", (int32_t)sizeof(rx_track_info)},
        {"rx_track_ribbon", (int32_t)sizeof(rx_track_ribbon)},
        {"rx_podium_info", (int32_t)sizeof(rx_podium_info)},
        {"rx_podium_rider", (int32_t)sizeof(rx_podium_rider)},
        {"rx_podium_slot", (int32_t)sizeof(rx_podium_slot)},
        {"rx_scenario_model", (int32_t)sizeof(rx_scenario_model)},
        {"rx_scenario_sound", (int32_t)sizeof(rx_scenario_sound)},
        {"rx_crowd_flash", (int32_t)sizeof(rx_crowd_flash)},
        {"rx_start_gates", (int32_t)sizeof(rx_start_gates)},
        {"rx_lap_record", (int32_t)sizeof(rx_lap_record)},
    };
    if (!name) return -1;
    for (size_t i = 0; i < sizeof k_sizes / sizeof k_sizes[0]; i++)
        if (strcmp(k_sizes[i].name, name) == 0) return k_sizes[i].size;
    return -1;
}

rx_session_state rx_session_get_state(const struct rx_session* s) {
    return session_ok(s) ? s->state : RX_SESSION_ERROR;
}

const char* rx_session_last_error(const struct rx_session* s) {
    if (!s) return g_create_error;
    if (!session_ok(s)) return "invalid rx_session handle";
    return s->error;
}

uint64_t rx_session_frames(const struct rx_session* s) { return session_ok(s) ? s->frames : 0; }
float rx_session_frame_dt(const struct rx_session* s) { return session_ok(s) ? s->last_dt : 0.0f; }

float rx_session_game_time(const struct rx_session* s) {
    if (!session_ok(s)) return 0.0f;
    return F32(0, RX_ENGINE_GLOBAL + 0x3c);
}

/* ------------------------------------------------------------------------------------------------------------------- state */

int rx_session_bike(struct rx_session* s, rx_bike_state* out) {
    if (!session_ok(s) || !out) return 1;
    memset(out, 0, sizeof *out);
    const rx_addr p = local_player();
    if (!p) return 1;
    const rx_addr v = U32(p, 0x1ac);
    if (!v) return 1;
    const rx_addr sim = U32(v, 0x234);
    if (!sim) return 1;

    for (uint32_t i = 0; i < 4; i++) {
        /* Four bodies, each with one position vec4 and one orientation quaternion.
         * The bodies are 0xa0 bytes apart; adjacent words include velocities/forces,
         * not the next body's pose (same layout as reflex_world_bike). */
        const rx_addr b = sim + 0xf0 + 0xa0u * i;
        copy_vec4(out->position[i], b);
        copy_vec4(out->orientation[i], b + 0x640);
        copy_vec4(out->render_position[i], b + 0x90);
        copy_vec4(out->render_orientation[i], b + 0x6d0);
    }
    copy_vec4(out->velocity, sim + 0x100);
    out->speed = F32(sim, 0x94);
    const uint32_t vt = U32(sim, 0);
    uint32_t gear_off = 0;
    if (vt == 0x0093c8a4u) gear_off = 0x8ac0;        /* RavenSimulationMotocross */
    else if (vt == 0x0093d054u) gear_off = 0x96f0;   /* RavenSimulationATV */
    if (gear_off) out->gear = I32(sim, gear_off);
    for (uint32_t i = 0; i < 8; i++) out->inputs[i] = F32(sim, 0x1c + 4u * i);
    out->frames = (uint32_t)s->frames;
    return 0;
}

/* These bounded reads mirror Rdb's table/column storage (build_tables.h),
 * Controller_BuildKeyMaps00547f30's source rows, and ScanCodeToGameKey00a95848.
 * No input query, original call, allocation, cache initialization or write. */
static int camera_guest_name(rx_addr address, const char* name) {
    uint32_t i=0; for (; name[i]; ++i) if (U8(address,i)!=(uint8_t)name[i]) return 0;
    return U8(address,i)==0;
}
int rx_session_camera_bindings(struct rx_session* s, rx_camera_bindings* out) {
    if (!out) return 1;
    memset(out,0,sizeof *out); out->driving_camera=-1;
    if (!session_ok(s)) return 1;
    const rx_addr player=local_player();
    if (!player) return 0;
    static const uint32_t entities[4]={0,1,4,2};
    const uint32_t selected=U32(player,0x988);
    for(uint32_t i=0;i<4;i++)
        if (entities[i]==selected) { out->driving_camera=(int32_t)i; break; }
    rx_addr table=0;
    /* This is the default/controller remap table used by00547f30, also shared
     * with player0's database after UISettings initializes its two rows. */
    const rx_addr database=0x00ae0910u;
    const uint32_t tables=U32(database,0x1e8);
    if (tables>100) return 0;
    for(uint32_t i=0;i<tables;i++) {
        rx_addr candidate=U32(database,0x38+4*i);
        if(candidate&&camera_guest_name(candidate+0x3c,"ControllerRemapping")) { table=candidate;break; }
    }
    if(!table) return 0;
    const rx_addr begin=U32(table,0x10),end=U32(table,0x14);
    if(end<begin || (end-begin)/4>128) return 0;
    int found=0; uint32_t used_pad_inputs=0;
    for(rx_addr at=begin;at<end;at+=4) {
        const rx_addr column=U32(at,0); if(!column) continue;
        const rx_addr cells=U32(column,0x40); if(!cells) continue;
        const rx_addr cb=U32(cells,0xc),ce=U32(cells,0x10);
        if(ce<cb || ce-cb<8) continue;
        const int camera=camera_guest_name(column,"Camera");
        const int32_t pad=I32(cb,4);
        if(pad>0 && pad<32) used_pad_inputs|=1u<<(uint32_t)pad;
        if(camera) {
            const int32_t key=I32(cb,0);
            for(uint32_t scan=1;scan<256;scan++) if(U8(0x00a95848u,scan)!=0 && U8(0x00a95848u,scan)==key) { out->keyboard_scan=(int32_t)scan;break; }
            out->pad_input=pad;found=1;
        } else if(pad==3 || pad==4) out->right_stick_rides=1;
    }
    /* Source onboard look-back uses L3 when right-stick axes ride, else R3.
     * Do not steal that button from any authored native binding. */
    const uint32_t look_back=out->right_stick_rides?17u:18u;
    if(!(used_pad_inputs&(1u<<look_back))) out->look_back_pad_input=(int32_t)look_back;
    out->valid=found;
    return 0;
}

int rx_session_onboard_state(struct rx_session* s, rx_onboard_state* out) {
    if (!out) return 1;
    memset(out, 0, sizeof *out);
    if (!session_ok(s)) return 1;
    const rx_addr p = local_player();
    if (!p) return 0;
    const rx_addr v = U32(p, 0x1ac), ra = U32(p, 0x1b4);
    if (!v || !ra || U32(ra, 0) != 0x00958724u) return 0;
    const rx_addr sim = U32(v, 0x234);
    if (!sim) return 0;
    out->valid = 1;
    out->rider_active = U32(p, 4) != 0 && U32(ra, 0x5bb5c) != 0;
    /* 0054ea30 can recover/cancel a pending player+b34 loss of balance without a fall.
     * Stay onboard through that recoverable phase. Actual fall 0054ed10 sets vehicle+280
     * and sim+2374 (rider off); 0045f590 clears them when the rider returns. */
    out->wrecked = U32(v, 0x280) != 0 || U32(sim, 0x2374) != 0;
    /* StuntTracker at player+b58: +1c is the current trick (006c98c0 / 006c9df0).
     * 0061dd60 / 00621320 set/clear the independent air-trick animation flag. */
    out->trick_active = !out->wrecked && (U32(p, 0xb74) != 0 || U32(ra, 0x5b2dc) != 0);
    out->animation_mode = I32(ra, 0xaee0);
    out->stunt_state = I32(ra, 0xaf10);
    const uint32_t vt = U32(sim, 0);
    if (vt == 0x0093c8a4u) { /* 00447730, MX front/rear wheel contact flags */
        out->contacts_valid = 1;
        out->front_grounded = U32(sim, 0x89e0) != 0;
        out->rear_grounded = U32(sim, 0x8890) != 0;
    } else if (vt == 0x0093d054u) { /* 00486850, ATV left/right wheels grouped by axle */
        out->contacts_valid = 1;
        out->front_grounded = U32(sim, 0x9240) != 0 || U32(sim, 0x9390) != 0;
        out->rear_grounded = U32(sim, 0x94e0) != 0 || U32(sim, 0x9630) != 0;
    }
    return 0;
}

static int rider_pose_of(rx_addr p, float* out, uint32_t max_bones) {
    if (!p) return 0;
    const rx_addr r = U32(p, 0x1b4);
    if (!r || U32(r, 0) != 0x00958724u) return 0;   /* RiderAnimation */
    const rx_addr records = U32(r, 0x188);
    const uint32_t count = U32(r, 0x18c);
    const rx_addr world = U32(r, 0x19c);
    if (count != 69 || !records || !world) return 0;
    /* the physics wrote the driven bones; the renderer completes the rest with the game's own lazy evaluator */
    for (int32_t i = (int32_t)count - 1; i >= 0; i--) rx_world_node_evaluate_world(records + 16u * (uint32_t)i, 0);
    const uint32_t n = count < max_bones ? count : max_bones;
    copy_floats(out, world, n * 16u);
    return (int)n;
}

static int bike_pose_of(rx_addr p, float* out, uint32_t max_nodes) {
    if (!p) return 0;
    const rx_addr v = U32(p, 0x1ac);
    const rx_addr model = v ? U32(v, 0x1328) : 0;
    if (!model) return 0;
    const rx_addr hier = model + 0x10;
    const rx_addr records = U32(hier, 8);
    const uint32_t count = U32(hier, 0xc);
    const rx_addr world = U32(hier, 0x1c);
    if (!records || !world || count == 0 || count > 64) return 0;
    for (int32_t i = (int32_t)count - 1; i >= 0; i--) rx_world_node_evaluate_world(records + 16u * (uint32_t)i, 0);
    const uint32_t n = count < max_nodes ? count : max_nodes;
    copy_floats(out, world, n * 16u);
    return (int)n;
}

/* a slot's player record P = G + 0x7460 + slot * 0x12a0; 0 when the slot is out of range or has no vehicle */
static rx_addr slot_player(const struct rx_session* s, int32_t slot) {
    if (slot < 0 || slot >= (int32_t)RX_RC_MAX || !s->game) return 0;
    const rx_addr p = rx_seat(s->game + 0x7460, (uint32_t)slot);
    return U32(p, 0x1ac) ? p : 0;
}

int rx_session_rider_pose(struct rx_session* s, float* out, uint32_t max_bones) {
    if (!session_ok(s) || !out) return 0;
    return rider_pose_of(local_player(), out, max_bones);
}

int rx_session_bike_pose(struct rx_session* s, float* out, uint32_t max_nodes) {
    if (!session_ok(s) || !out) return 0;
    return bike_pose_of(local_player(), out, max_nodes);
}

int rx_session_rider_pose_slot(struct rx_session* s, int32_t slot, float* out, uint32_t max_bones) {
    if (!session_ok(s) || !out) return 0;
    return rider_pose_of(slot_player(s, slot), out, max_bones);
}

int rx_session_bike_pose_slot(struct rx_session* s, int32_t slot, float* out, uint32_t max_nodes) {
    if (!session_ok(s) || !out) return 0;
    return bike_pose_of(slot_player(s, slot), out, max_nodes);
}

/* The live camera the game's camera update writes (_re/notes/game-camera.md):
 *  - during the race / countdown: the local player's active camera entity P+0x9a8 (MXStartGateCamera on the grid, then
 *    the MXThirdPersonCamera), whose embedded Camera at +0x90 is what 0063dba0 copies into the scene's view camera;
 *  - during the intro fly-by (P+0x9a8 = 0): the world's MXAnimatedCamera at world+0x218f0 (the scenario's director),
 *    whose Camera at +0x90 the intro update 007473f0 copies into the view (seen through 0x7475a5);
 *  - the render camera copies as a last resort ([0xa7b1d8]; the static Camera 0xd6f620 that the null renderer's first
 *    view record points at, game-camera.md), for phases before the event world's scenario exists.
 * Camera fields: +0x10 position, +0x20 right, +0x30 up, +0x40 forward, +0x50 fov, +0x58 / +0x5c near / far. The camera
 * entity's Camera and the render copy differ only in +0x54 aspect (the view's display aspect), which rx_camera does not
 * export. Returns 2 when no camera exists yet. */
static int camera_read(rx_addr cam, rx_camera* out) {
    if (!cam || U32(cam, 0) != 0x0095a2d8u) return 0;
    /* a camera that was never posed / tuned has no lens: the original's fov / near / far are always positive */
    if (!(F32(cam, 0x50) > 0.0f) || !(F32(cam, 0x5c) > F32(cam, 0x58))) return 0;
    copy_vec3(out->position, cam + 0x10);
    copy_vec3(out->right, cam + 0x20);
    copy_vec3(out->up, cam + 0x30);
    copy_vec3(out->forward, cam + 0x40);
    out->fov = F32(cam, 0x50);
    out->near_plane = F32(cam, 0x58);
    out->far_plane = F32(cam, 0x5c);
    return 1;
}

static int podium_camera(rx_addr p, rx_camera* out);   /* podium_session.inc */

/* While a replay clip plays (the clip recorder's mode 0xa942b4 == 2: the finish replay of the podium scenario's step 7, the post-race
 * reel) and the world has a current TV director (+0x21dfc: one of the replay cameras the scene factory builds, loop_cam_replay.c), the view
 * is the Camera ReplayCameras_Update (007678b0) left in the scene's first view: the director's camera (or the selected vehicle's with
 * +0x21de4). The replay presenter reads the same Camera (rx_session_replay_view). */
static int replay_clip_camera(rx_addr p, rx_camera* out) {
    if (U32(0, 0x00a942b4) != 2) return 0;
    rx_addr act = p ? U32(p, 0x94) : 0;
    if (!act) act = U32(0, 0x00a942b8);
    const rx_addr world = act ? U32(act, 0x234) : 0;
    if (!world || !U32(world, 0x21dfc)) return 0;
    const rx_addr scene = U32(world, 0x20138);
    const rx_addr view = scene ? U32(scene, 0x19b8) : 0;
    return view && camera_read(U32(view, 0), out);
}

int rx_session_camera(struct rx_session* s, rx_camera* out) {
    if (!session_ok(s) || !out) return 1;
    memset(out, 0, sizeof *out);
    const rx_addr p = local_player();
    /* while a post-race scenario plays (the finish podium, the results), its animated camera is the view (podium_session.inc) */
    if (p && podium_camera(p, out)) return 0;
    if (replay_clip_camera(p, out)) return 0;
    if (p) {
        const rx_addr ent = U32(p, 0x9a8);
        if (ent && U32(ent, 0x2f4) == 1u && camera_read(ent + 0x90, out)) return 0;
        const rx_addr act = U32(p, 0x94);
        if (act) {
            /* the world's inline MXAnimatedCamera is only valid after its update set the pose (+0x2b0); the activity's world (the race
             * intro scenario's +0x14 activity at act + 0xff090 would be another class's field in the Freestyle / Waypoint activities) */
            const rx_addr w = U32(act, 0x234u);
            const rx_addr anim = w + 0x218f0u;
            if (w && U8(anim, 0x2b0u) == 1u && camera_read(anim + 0x90u, out)) return 0;
        }
    }
    /* the render copies (levels with a forest, game-camera.md): [0xa7b1d8], else the static Camera 0xd6f620 */
    rx_addr cam = U32(0, 0x00a7b1d8);
    if (!cam && U32(0, 0x00d6f620) == 0x0095a2d8u) cam = 0x00d6f620;
    if (cam && camera_read(cam, out)) return 0;
    return 2;
}

/* The view selector: the game's own Player_SetCamera 0054c9b0, the function the Camera key (C) calls. API mode 0 = the default
 * third-person GamePlay entity (1, [0x9c5410]'s shipped value and the session's default), 2 = the first-person entity (2); the
 * setter itself validates the player / activity state and performs the whole transition (P+0x988, the old entity's offsets, the
 * replay cameras, the per-controller index, the DrivingCamera setting, the "Camera Changed" event). Nothing changes unless this is
 * called. */
int rx_session_set_camera(struct rx_session* s, int32_t mode) {
    if (!session_ok(s)) return 1;
    if (s->state == RX_SESSION_ERROR) return 1;
    uint32_t entity;
    switch (mode) {
    case 0: entity = 1; break;    /* GamePlay third person (the game's default) */
    case 2: entity = 2; break;    /* first person */
    default: return 2;            /* the other views only through the key's cycle */
    }
    const rx_addr p = local_player();
    if (!p) return 2;
    return rx_player_set_camera(p, entity) ? 0 : 2;
}

int rx_session_set_driving_camera(struct rx_session* s, int32_t preset) {
    static const uint32_t entities[4] = {0, 1, 4, 2};
    if (!session_ok(s) || s->state == RX_SESSION_ERROR) return 1;
    if (preset < 0 || preset >= 4) return 2;
    const rx_addr p = local_player();
    if (!p) return 2;
    return rx_player_set_camera(p, entities[preset]) ? 0 : 2;
}

int rx_session_event_ui_state(struct rx_session* s, rx_event_ui_state* out) {
    if (!out) return 1;
    rx_event_ui_snapshot(0, out);
    if (!session_ok(s)) return 1;
    const rx_addr p = local_player();
    if (p) rx_event_ui_snapshot(U32(p, 0x94), out);
    return 0;
}

/* Actual constructor00516730 identifies this distinct driver layout. Avoid
 * interpreting an arbitrary activity-kind number as a driver memory schema. */
static int waypoint_driver_layout(rx_addr d) { return d && U32(d, 0) == 0x0093ffe4u; }

int rx_session_waypoint(struct rx_session* s, rx_waypoint_state* out) {
    if (!session_ok(s) || !out || out->size != sizeof *out || out->version != RX_WAYPOINT_API_VERSION) return 1;
    rx_waypoint_state result; memset(&result, 0, sizeof result);
    result.size = sizeof result; result.version = RX_WAYPOINT_API_VERSION;
    result.gate_index = -1;
    const rx_addr p = local_player();
    rx_addr a = 0, d = p ? race_driver_for(p, &a) : 0;
    if (a && waypoint_driver_layout(d)) {
        result.activity_type = I32(a, 0x23c); result.activity_mode = I32(a, 0x238);
        result.gate_count = I32(a, 0xff074);
        result.gate_index = I32(d, 0x10d4);
        result.laps_completed = I32(d, 0xf10); result.num_laps = I32(a, 0xff090);
        result.valid = result.gate_count > 0 && U32(a, 0xff078) && I32(d, 0x10d8) == result.gate_count;
        const float remaining = F32(d, 0x10b8);
        result.distance_valid = result.valid && I32(a, 0x21c) >= 2 && I32(a, 0x21c) <= 4 &&
            isfinite(remaining) && U32(d, 0x10b8) != U32(0, 0x00977494);
        if (result.distance_valid) result.remaining_distance = remaining;
    }
    *out = result; return 0;
}

int rx_session_race(struct rx_session* s, rx_race_state* out) {
    if (!session_ok(s) || !out) return 1;
    memset(out, 0, sizeof *out);
    out->countdown_step = -1;
    const rx_addr p = local_player();
    if (!p) return 1;
    const rx_addr a = U32(p, 0x94);
    out->reset_timer = F32(p, 0x116c);
    if (!a) return 1;
    const uint32_t n = U32(a, 0x26c);
    out->state = I32(a, 0x21c);
    out->riders = (int32_t)n;
    out->num_laps = I32(a, 0xff070);
    const rx_addr board = U32(a, 0x2cc);
    out->start_timer = board ? F32(board, 0x4f4) : 0.0f;
    if (out->state == 1 && !board) {   /* the activity's own countdown (Activity::vf26 00559190) as 005fb830 / 005fb880 show it */
        const int32_t count = I32(a, 0x2a0);
        out->countdown_step = count >= I32(a, 0x2b8) ? 3 : count < 0 ? 0 : count + 1;
    }
    const rx_addr d = race_driver_for(p, 0);
    if (!d) return 0;
    out->valid = 1;
    if (waypoint_driver_layout(d)) {
        out->num_laps = I32(a, 0xff090);
        out->laps_completed = I32(d, 0xf10);
        out->place = I32(d, 0x78) + 1;
        out->lap_time = F32(d, 0xf14);
        out->last_lap = F32(d, 0x10c4);
        out->best_lap = F32(d, 0x10b0);
        out->race_time = F32(d, 0x134);
        /* National node+length/to-line fields are not Waypoint progress.
         * Dedicated gate/distance export carries its genuine route values. */
        return 0;
    }
    out->laps_completed = I32(d, 0xf1c);
    out->place = I32(d, 0x78) + 1;
    out->lap_time = F32(d, 0xf20);
    out->last_lap = F32(d, 0x10e0);
    out->best_lap = F32(d, 0x10c4);
    out->race_time = F32(d, 0x134);
    /* RaceDriver0050ed80 has track BLOCK at+10, current SECTION at+14.
     * TDF00536ef0 tag5 stores global lap length at block+1dc; tag4 stores
     * start-segment length at+1e0. Node+4f4 is remaining distance, not a
     * global denominator (PX section0 first node is near the finish line).
     * Match only the positively identified original RaceDriver layout.
     * This is canonical00512690's copied-distance fallback normalized to
     * the current lap/start segment; never call its mutating track search. */
    if (U32(d, 0) == 0x0093f8dcu) {
        const rx_addr block = U32(d, 0x10);
        const int32_t laps = I32(d, 0xf1c);
        if (block && laps >= -1) {
            const float lap_length = F32(block, 0x1dc);
            const float start_length = F32(block, 0x1e0);
            const float remaining = F32(d, 0x10cc);
            if (isfinite(lap_length) && lap_length > 0.0f &&
                isfinite(start_length) && start_length >= 0.0f &&
                isfinite(lap_length + start_length) && isfinite(remaining)) {
                const float span = laps == -1 ? start_length : lap_length;
                /* A genuine no-start-segment course has no prestart fraction.
                 * Preserve the zero-initialized observation until crossing. */
                if (span > 0.0f) {
                    const float ratio = remaining / span;
                    if (isfinite(ratio)) {
                        float progress = 1.0f - ratio;
                        if (progress < 0.0f) progress = 0.0f;
                        if (progress > 1.0f) progress = 1.0f;
                        out->lap_progress = progress;
                    }
                }
            }
        }
        return 0;
    }
    /* Unknown non-Waypoint layouts retain their existing observation path. */
    const rx_addr track = U32(d, 0x14);
    const rx_addr node = track ? U32(track, 0) : 0;
    const float length = node ? F32(node, 0x13d * 4) : 0.0f;
    const float to_line = F32(d, 0x10cc);
    if (length > 0.0f) {
        float progress = 1.0f - to_line / length;
        if (progress < 0.0f) progress = 0.0f;
        if (progress > 1.0f) progress = 1.0f;
        out->lap_progress = progress;
    }
    return 0;
}

/* The original in-race HUD's other values (the old plugin's reflex_world_hud; _re/notes/ui.md). Reads only. */
int rx_session_hud(struct rx_session* s, rx_hud_state* out) {
    if (!session_ok(s) || !out) return 1;
    memset(out, 0, sizeof *out);
    const rx_addr p = local_player();
    if (!p) return 0;
    const rx_addr v = U32(p, 0x1ac);
    const rx_addr a = U32(p, 0x94);
    if (v) out->speed = F32(v, 0x150);
    if (!a) return 0;
    out->activity_state = I32(a, 0x238);
    const rx_addr d = race_driver_for(p, 0);
    if (!d) return 0;
    out->valid = 1;
    if (waypoint_driver_layout(d)) {
        /* +10b4 is a tuning margin, +10b8 route distance, +10dc a behavior
         * pointer. None is a National gap/hold timer. Unknown rules stay
         * unread; only positively identified canonical rule layouts qualify. */
        const rx_addr offtrack = U32(d, 0xac), backwards = U32(d, 0xa8);
        out->on_track = 1;
        if (offtrack && U32(offtrack, 0) == 0x0093fab0u) {
            out->on_track = U32(offtrack, 0x10) != 0;
            out->penalty = F32(offtrack, 0x20);
        }
        if (backwards && U32(backwards, 0) == 0x0093fb4cu)
            out->wrong_way = U32(backwards, 0xc) != 0;
        return 0;
    }
    out->time_behind = F32(d, 0x10b4);
    out->time_ahead = F32(d, 0x10b8);
    out->hold_clock = F32(d, 0x130);
    out->hold_until = F32(d, 0x10dc);
    const rx_addr offtrack = U32(d, 0xac), backwards = U32(d, 0xa8);
    out->on_track = offtrack ? (U32(offtrack, 0x10) != 0) : 1;
    out->penalty = offtrack ? F32(offtrack, 0x20) : 0.0f;
    out->wrong_way = backwards ? (U32(backwards, 0xc) != 0) : 0;
    return 0;
}

/* The race line ahead of the local player: the nodes' centres (their matrix translation) from D+0x1c on, following +0x64. */
int rx_session_race_line_ex(struct rx_session* s, float* out, uint32_t max_points, int32_t* closed) {
    if (closed) *closed = 0;
    if (!session_ok(s) || !out) return -1;
    const rx_addr p = local_player();
    if (!p) return 0;
    if (!U32(p, 0x94)) return 0;
    const rx_addr d = race_driver_for(p, 0);
    if (!d) return 0;
    /* Waypoint navigation uses behavior14+a4/a8; D+1c is not its route. */
    if (waypoint_driver_layout(d)) return 0;
    rx_addr node = U32(d, 0x1c), first = node;
    uint32_t count = 0;
    while (node && count < max_points) {
        copy_vec3(out + 3u * count, node);   /* the node centre: the first floats of the 0x500-byte node */
        count++;
        node = U32(node, 0x64);
        if (node == first) { if (closed) *closed = 1; break; }
    }
    return (int)count;
}

int rx_session_race_line(struct rx_session* s, float* out, uint32_t max_points) {
    return rx_session_race_line_ex(s, out, max_points, NULL);
}

int rx_session_standings(struct rx_session* s, rx_driver_result* out, uint32_t max) {
    if (!session_ok(s) || !out) return 0;
    const rx_addr p = local_player();
    if (!p) return 0;
    const rx_addr a = U32(p, 0x94);
    if (!a) return 0;
    const rx_addr list = U32(a, 0x268);
    const uint32_t n = U32(a, 0x26c);
    /* In the activity's standings order (+0x2d0[i], i < +0x26c, at most 12; 32 with rider_cap.h's side storage): the order the original's results page numbers (00613dd0's
     * row Index = i; PostEventResults.lua sorts by it and shows Index + 1). The place +0x78 is not that number: outside InProgress the
     * race's comparator (MXActivityRace::vf43 00565570) returns -1 or 0, never 1, so 00559eb0 moves nobody and gives a rider whose
     * adjusted time is not above the previous one's the previous one's place (places repeat). The driver list +0x268 when the standings
     * do not hold every driver once (an online race keeps only its local drivers there). */
    int ordered = list && n > 0 && n <= RX_RC_MAX;   /* (w-rc-core) 12 before: standings 12..31 in side storage */
    for (uint32_t i = 0; ordered && i < n; i++) {
        const rx_addr d = rx_stand(a, i);
        uint32_t j = 0;
        while (j < n && U32(list, 4u * j) != d) j++;
        if (!d || j == n) ordered = 0;
        for (uint32_t m = 0; ordered && m < i; m++) if (rx_stand(a, m) == d) ordered = 0;
    }
    uint32_t k = 0;
    for (uint32_t i = 0; list && i < n && i < 64 && k < max; i++) {
        const rx_addr d = ordered ? rx_stand(a, i) : U32(list, 4u * i);
        if (!d) continue;
        const rx_addr dp = U32(d, 0x74);
        rx_driver_result* r = &out[k++];
        memset(r, 0, sizeof *r);
        r->slot = rx_seat_of(s->game + 0x7460, dp);
        r->local = dp == p;
        r->place = I32(d, 0x78) + 1;
        r->finished = I32(d, 0x70);
        r->race_time = F32(d, 0x134);
        if (waypoint_driver_layout(d)) {
            r->laps_completed = I32(d, 0xf10);
            r->best_lap = F32(d, 0x10b0);
            r->last_lap = F32(d, 0x10c4);
        } else {
            r->laps_completed = I32(d, 0xf1c);
            r->best_lap = F32(d, 0x10c4);
            r->last_lap = F32(d, 0x10e0);
        }
    }
    return (int)k;
}

int rx_session_slots(struct rx_session* s, rx_slot* out, uint32_t max) {
    if (!session_ok(s) || !out) return 0;
    const rx_addr p_local = local_player();
    uint32_t n = 0;
    for (uint32_t slot = 0; slot < RX_RC_MAX && n < max; slot++) {   /* (w-rc-core) the seats 0..31 (16 before: past the 12-seat block) */
        const rx_addr p = rx_seat(s->game + 0x7460, slot);
        const rx_addr v = U32(p, 0x1ac);
        if (!v) continue;   /* the old plugin lists only slots with a vehicle */
        rx_slot* r = &out[n++];
        memset(r, 0, sizeof *r);
        r->slot = (int32_t)slot;
        r->type = I32(p, 0x10);
        r->local = p == p_local;
        r->rider_row = I32(p, 0x1b8);
        r->vehicle_row = I32(p, 0x1b0);
        r->vehicle = v;
        r->rider = U32(p, 0x1b4);
    }
    return (int)n;
}

/* Read-only construction metadata: build_vehicle.h, build_rider_init.c, and the original plate
 * readers 0054bec0/0054bf50. All pointers remain in the guest arena; only IDs/text cross the ABI. */
static void copy_guest_string(char* dst, uint32_t n, rx_addr a);
int rx_session_appearance(struct rx_session* s, int32_t slot, rx_appearance* out) {
    if (!session_ok(s) || !out) return 1;
    memset(out, 0, sizeof *out);
    const rx_addr p = slot_player(s, slot);
    if (!p) return 2;
    rx_addr vehicle = 0, rider = 0;
    const uint32_t vn = U32(s->game, 0x154dc), rn = U32(s->game, 0x1551c);
    if (vn > 64 || rn > 64) return 2;
    for (uint32_t i = 0; i < vn; i++) {
        rx_addr e = U32(U32(s->game, 0x154d8), 4*i);
        if (e && U32(e, 4) == U32(p, 0x1b0)) { vehicle = e; break; }
    }
    for (uint32_t i = 0; i < rn; i++) {
        rx_addr e = U32(U32(s->game, 0x15518), 4*i);
        if (e && U32(e, 4) == U32(p, 0x1b8)) { rider = e; break; }
    }
    if (!vehicle || !rider || !U32(vehicle, 8)) return 2;
    const rx_addr model = U32(vehicle, 8);
    out->vehicle_owner = I32(vehicle, 0x98);
    out->owner_vehicle_id = I32(vehicle, 0x9c);
    out->vehicle_model_id = I32(model, 0);
    out->rider_owner = I32(rider, 8);
    out->owner_rider_id = I32(rider, 0xc);
    out->gear_set_id = I32(rider, 0x10);
    for (uint32_t i = 0; i < 17; i++) {
        const rx_addr part = U32(vehicle, 0xc + 4*i);
        if (part) out->part_ids[out->part_count++] = I32(part, 0x40);
    }
    for (uint32_t i = 0; i < 8; i++) { /* +0x3c starts pending gear, not equipped gear */
        const rx_addr gear = U32(rider, 0x1c + 4*i);
        if (gear) out->gear_ids[out->gear_count++] = I32(gear, 0x40);
    }
    copy_guest_string(out->plate_name, sizeof out->plate_name, p + 0x200);
    copy_guest_string(out->plate_number, sizeof out->plate_number, p + 0x210);
    /* out->name stays empty: it is the display name (the roster fills it from the AIRider DisplayName / the profile); the plate text is
     * the jersey name (0053f3f0: PlayerRider Name / Number, AIRider Name / Number), a different string. */
    out->bike_dark = (U32(model, 0xc) != 0 && U32(model, 0xc) != 1) || U32(model, 0x10) != 1;
    out->on_jersey = out->invert = 1;
    rx_addr gear = U32(rider, 0x24), cp = U32(rider, 0x20);
    if (cp && I32(cp, 0x30) > 0) { out->on_jersey = 0; gear = cp; }
    if (gear) out->invert = U32(gear, 0x44) != 0;
    return 0;
}

/* The vehicle classes of a race slot (the old plugin's vehicles.h table): the factory index, the vehicle / simulation vtables,
 * the body count and the simulation's gear / engine-speed fields. MX and ATV are the ones the product ports; the rest are listed
 * so a slot of another class reports its type instead of -1. */
static const struct rx_vehicle_class {
    int32_t factory;
    uint32_t vehicle_vtable, sim_vtable, bodies, gear, engine;
} k_vehicle_classes[] = {
    {0, 0x0093cd84u, 0x0093c8a4u, 4, 0x8ac0, 0x8abc},   /* Motocross */
    {1, 0x0093d0ecu, 0x0093d054u, 6, 0x96f0, 0x96ec},   /* ATV */
    {3, 0x0093cee4u, 0x0093c93cu, 5, 0x8664, 0x865c},   /* TrophyTruck */
    {5, 0x0093d2e4u, 0x0093d24cu, 5, 0x8650, 0x864c},   /* Buggy */
    {7, 0x0093d4d4u, 0x0093d43cu, 5, 0x8660, 0x865c},   /* UTV */
};

static const struct rx_vehicle_class* vehicle_class(uint32_t vehicle_vtable, uint32_t sim_vtable, int by_sim) {
    for (size_t i = 0; i < sizeof k_vehicle_classes / sizeof k_vehicle_classes[0]; i++) {
        const struct rx_vehicle_class* c = &k_vehicle_classes[i];
        if (by_sim ? c->sim_vtable == sim_vtable : c->vehicle_vtable == vehicle_vtable) return c;
    }
    return NULL;
}

int rx_session_vehicle_info(struct rx_session* s, int32_t slot, rx_vehicle_info* out) {
    if (!session_ok(s) || !out) return 1;
    memset(out, 0, sizeof *out);
    out->type = -1;
    const rx_addr p = slot < 0 ? local_player() : slot_player(s, slot);
    out->slot = (slot >= 0 && p && s->game) ? rx_seat_of(s->game + 0x7460, p) : slot;
    const rx_addr v = p ? U32(p, 0x1ac) : 0;
    if (!v) return 1;
    const uint32_t vvt = U32(v, 0);
    const rx_addr sim = U32(v, 0x234);
    const uint32_t svt = sim ? U32(sim, 0) : 0;
    out->vehicle = v;
    out->sim = sim;
    const struct rx_vehicle_class* c = vehicle_class(vvt, svt, 0);
    out->type = c ? c->factory : -1;
    out->speed = F32(v, 0x150);
    const rx_addr model = U32(v, 0x1328);
    out->model_nodes = model ? I32(model, 0x10 + 0xc) : 0;
    if (!sim) return 0;
    const struct rx_vehicle_class* sc = vehicle_class(0, svt, 1);
    if (!sc || U32(sim, 0xec) != sc->bodies) return 0;
    out->bodies = (int32_t)U32(sim, 0xec);
    out->gear = I32(sim, sc->gear) + 1;
    out->rpm = F32(sim, sc->engine) * 9.55f;
    for (int i = 0; i < out->bodies && i < 6; i++) {
        const rx_addr b = sim + 0xf0 + 0xa0u * (uint32_t)i;   /* render pose: B+0x90 position, B+0x6d0 orientation */
        copy_vec4(&out->render_position[4 * i], b + 0x90);
        copy_vec4(&out->render_orientation[4 * i], b + 0x6d0);
    }
    return 0;
}

/* ----------------------------------------------------------------------------------------------------------------- terrain */

/* The registered terrain of the active view: the list 0x009e30d4 (inline entries, count +8, guard 0x009e30e0 bit 0), entry
 * whose +0x44a8 is the active scene. Same query as phys_rider.c's active_terrain, but read-only: while the guard is clear the
 * function-local static has no entries (the original's first-use initializer would only set the guard and count 0, which every
 * registrar - rx_hfterrain_register - does as its first step anyway), so a getter never has to write it. */
static rx_addr active_terrain(void) {
    if ((U32(0, 0x009e30e0) & 1) == 0) return 0;
    const rx_addr scene = U32(0, 0x00a95844);
    const uint32_t count = U32(0, 0x009e30dc);
    for (uint32_t k = 0; scene && k < count; k++) {
        const rx_addr t = U32(0, 0x009e30d4 + 4u * k);
        if (t && U32(scene, 0x1c + 4u * U32(scene, 4)) == U32(t, 0x44a8)) return t;
    }
    return 0;
}

/* HFTerrainSurface +0x230 is the DeformablePatch; +0x10 (+0x240) is the R32F SYSTEMMEM deformation surface. rx_d3d_bits gives
 * the product's CPU-visible grid (0 in the oracle build). */
static rx_addr terrain_deform_bits(rx_addr t, uint32_t* pitch) {
    const rx_addr surf = U32(t, 0x240);
    if (!surf) { if (pitch) *pitch = 0; return 0; }
    return rx_d3d_bits(surf, 0, pitch);
}

int rx_session_terrain(struct rx_session* s, rx_terrain_info* out) {
    if (!session_ok(s) || !out) return 1;
    memset(out, 0, sizeof *out);
    const rx_addr t = active_terrain();
    if (!t) { terrain_dirty_invalidate(&s->terrain_cache); return 2; }
    out->samples = U32(t, 0x44c0);
    out->stride = U32(t, 0x44bc);
    out->spacing = F32(t, 0x44e8);
    out->min_x = F32(t, 0x44d8);
    out->min_z = F32(t, 0x44dc);
    out->max_x = F32(t, 0x44e0);
    out->max_z = F32(t, 0x44e4);
    out->gain = F32(t, 0xbc);
    if (!out->samples || !out->stride) { terrain_dirty_invalidate(&s->terrain_cache); return 2; }

    uint32_t pitch = 0;
    const rx_addr bits = terrain_deform_bits(t, &pitch);
    const uint32_t n=out->samples;
    const rx_addr surface=U32(t,0x240);
    if(n>INT32_MAX || n>UINT32_MAX/4u || (bits && (!pitch || pitch<4u*n ||
       n>U32(surface,0xc) || n>U32(surface,0x10) ||
       (uint64_t)(n-1)*pitch+4ull*n>0x100000000ull-bits))) {
        terrain_dirty_invalidate(&s->terrain_cache);return 2;
    }
    struct terrain_dirty_key key={{t,U32(t,0x1c),U32(t,0x240),bits,pitch,n,out->stride,
        rx_fb(out->spacing),rx_fb(out->min_x),rx_fb(out->min_z),rx_fb(out->max_x),rx_fb(out->max_z),rx_fb(out->gain),U32(t,0x44a8),U32(t,0x44ac),0}};
    /* Product API boundary: rows are read-only views of the already CPU-visible
     * native arena. The pure helper owns only a packed host-side previous copy. */
    const void *rows=bits ? (const void*)(rx_base+bits) : NULL;
    /* the D3D layer's dirty rectangle of the surface since the last look (rx_d3d_track_take); taken only when this call scans */
    int32_t hint_rect[4]={0,0,0,0};const int32_t *hint=NULL;int bad=0;
    static int verify=-1;
    if(bits && terrain_dirty_pending(&s->terrain_cache,&key,s->frames) && rx_d3d_track_take(bits,hint_rect)) hint=hint_rect;
    if(verify<0){const char *v=getenv("RX_D3D_VERIFY");verify=v&&*v&&*v!='0';}
    if(!terrain_dirty_update_hint(&s->terrain_cache,&key,s->frames,n,rows,pitch,hint,verify,&bad)) {
        session_error(s,"terrain export cache allocation or dimensions invalid");return 1;
    }
    if(bad){fprintf(stderr,"rxcore: terrain export: the dirty-rectangle scan missed a change (hint %d,%d - %d,%d)\n",hint_rect[0],hint_rect[1],hint_rect[2],hint_rect[3]);abort();}
    out->dirty_x0=s->terrain_cache.x0;out->dirty_z0=s->terrain_cache.z0;
    out->dirty_x1=s->terrain_cache.x1;out->dirty_z1=s->terrain_cache.z1;
    out->deform_version=s->terrain_cache.version;
    return 0;
}

int rx_session_terrain_tiles(struct rx_session* s, rx_terrain_rect* out, int32_t max) {
    if (!session_ok(s)) return -1;
    rx_terrain_info info;
    const int rc = rx_session_terrain(s, &info);
    if (rc == 1) return -1;
    if (rc) return -2;
    struct terrain_dirty_rect r[TERRAIN_DIRTY_MAX_RECTS];
    int n = s->terrain_cache.nrects;
    if (n > TERRAIN_DIRTY_MAX_RECTS) n = TERRAIN_DIRTY_MAX_RECTS;
    for (int i = 0; i < n; i++) r[i] = s->terrain_cache.rects[i];
    if (!out || max <= 0) return n;
    if (n > max) terrain_dirty_reduce(r, &n, max);
    for (int i = 0; i < n; i++) { out[i].x0 = r[i].x0; out[i].z0 = r[i].z0; out[i].x1 = r[i].x1; out[i].z1 = r[i].z1; }
    return n;
}

/* x0..x0+w / z0..z0+h against the selected terrain; out is h rows of w floats. */
static int terrain_rect(struct rx_session* s, rx_addr* t_out, rx_terrain_info* info, int32_t x0, int32_t z0, int32_t w, int32_t h) {
    if (!session_ok(s)) return 1;
    const rx_addr t = active_terrain();
    if (!t) return 2;
    memset(info, 0, sizeof *info);
    info->samples = U32(t, 0x44c0);
    info->stride = U32(t, 0x44bc);
    info->spacing = F32(t, 0x44e8);
    info->gain = F32(t, 0xbc);
    if (!info->samples || !info->stride) return 2;
    if (x0 < 0 || z0 < 0 || w <= 0 || h <= 0 || (uint32_t)(x0 + w) > info->samples || (uint32_t)(z0 + h) > info->samples) return 3;
    *t_out = t;
    return 0;
}

int rx_session_terrain_heights(struct rx_session* s, float* out, int32_t x0, int32_t z0, int32_t w, int32_t h) {
    if (!out) return 1;
    rx_addr t = 0;
    rx_terrain_info ti;
    const int rc = terrain_rect(s, &t, &ti, x0, z0, w, h);
    if (rc) return rc;
    const rx_addr base = U32(t, 0x1c);
    if (!base) return 2;
    uint32_t pitch = 0;
    const rx_addr bits = terrain_deform_bits(t, &pitch);
    const uint32_t dstride = pitch ? pitch / 4u : ti.samples;
    for (int32_t r = 0; r < h; r++) {
        for (int32_t c = 0; c < w; c++) {
            const uint32_t idx = (uint32_t)(z0 + r) * ti.stride + (uint32_t)(x0 + c);
            const float raw = F32(base, 4u * idx);
            const float def = bits ? F32(bits, 4u * ((uint32_t)(z0 + r) * dstride + (uint32_t)(x0 + c))) : 0.5f;
            out[(size_t)r * (size_t)w + (size_t)c] = raw * 2048.0f + (def - 0.5f) * 2.0f * ti.gain;
        }
    }
    return 0;
}

int rx_session_terrain_deform(struct rx_session* s, float* out, int32_t x0, int32_t z0, int32_t w, int32_t h) {
    if (!out) return 1;
    rx_addr t = 0;
    rx_terrain_info ti;
    const int rc = terrain_rect(s, &t, &ti, x0, z0, w, h);
    if (rc) return rc;
    uint32_t pitch = 0;
    const rx_addr bits = terrain_deform_bits(t, &pitch);
    const uint32_t dstride = pitch ? pitch / 4u : ti.samples;
    for (int32_t r = 0; r < h; r++)
        for (int32_t c = 0; c < w; c++) {
            const uint32_t idx = (uint32_t)(z0 + r) * dstride + (uint32_t)(x0 + c);
            out[(size_t)r * (size_t)w + (size_t)c] = bits ? F32(bits, 4u * idx) : 0.5f;
        }
    return 0;
}

/* ---- tyre tracks (TerrainTracks) ------------------------------------------------------------------------------------ */

/* The original's three sets (HFTerrainSurface_InitDeformAndTracks 007c4890): ring capacities 30 / 10 / 5. */
static const uint32_t g_track_sets[3] = {0x00a95774u, 0x00a95778u, 0x00a9577cu};
static const char* const g_track_material[RX_TRACK_MATERIALS] = {"Generic", "MX", "ATV", "UTV", "ProLite", "Buggy"};

static void track_info_fill(struct rx_session* s, rx_track_info* info, rx_addr t) {
    memset(info, 0, sizeof *info);
    info->version = s->tracks_version;
    info->slots = 24;
    info->sets = 3;
    info->vertex_stride = RX_TRACK_VERTEX_FLOATS * (uint32_t)sizeof(float);
    info->vertex_format = RX_TRACK_VERTEX_XZ_SIDE_V_INTENSITY_TYPE;
    info->index_format = RX_TRACK_INDEX_TRIANGLE_LIST;
    if (t) {
        info->half_x = F32(t, 0x44d0) * 0.5f;
        info->half_z = F32(t, 0x44d4) * 0.5f;
        info->centre_x = (F32(t, 0x44d8) + F32(t, 0x44e0)) * 0.5f;
        info->centre_z = (F32(t, 0x44dc) + F32(t, 0x44e4)) * 0.5f;
    }
    info->material_count = RX_TRACK_MATERIALS;
    for (int i = 0; i < RX_TRACK_MATERIALS; i++) snprintf(info->material[i], sizeof info->material[i], "%s", g_track_material[i]);
}

/* One SHORT4N vertex (8 bytes: x, z, s, w) -> the 6 exported floats; the old World::tracks decode exactly
 * (world = n(x) * half extent, side from w, V = 32 |n(w)|, intensity |n(s)|, material type when s < 0). */
static void track_vertex(float* out, rx_addr v, float hx, float hz, int32_t type) {
    const int16_t x = (int16_t)U16(v, 0), z = (int16_t)U16(v, 2), s = (int16_t)U16(v, 4), w = (int16_t)U16(v, 6);
    const float nx = (float)x / 32767.0f, nz = (float)z / 32767.0f, ns = (float)s / 32767.0f, nw = (float)w / 32767.0f;
    out[0] = (nx < -1.0f ? -1.0f : nx) * hx;
    out[1] = (nz < -1.0f ? -1.0f : nz) * hz;
    out[2] = w >= 0 ? 1.0f : 0.0f;
    out[3] = 32.0f * (nw < 0.0f ? -nw : nw);
    out[4] = ns < 0.0f ? -ns : ns;
    out[5] = s < 0 ? (float)type : 0.0f;
}

/* Decode every live chunk the way TerrainTracks_Draw (007cc4f0) submits it: allocated slots, live chunk generations, the
 * chunk's index start over its vertex byte offset, 254-triangle strips, degenerate and never-written vertices rejected.
 * `out` may be NULL (count only); `ribbons` may be NULL. Returns the number of triangles written and always fills the totals
 * in *counts (triangles, writes, strips, chunks) and the descriptors. */
static int tracks_decode(rx_addr t, float* out, uint32_t max_triangles, rx_track_ribbon* ribbons, uint32_t max_ribbons,
                         int32_t* total_triangles, int32_t* strips, int32_t* chunks) {
    int32_t total = 0, written = 0, nstrips = 0, nchunks = 0;
    const float hx = F32(t, 0x44d0) * 0.5f, hz = F32(t, 0x44d4) * 0.5f;
    for (int g = 0; g < 3; g++) {
        const rx_addr tt = U32(0, g_track_sets[g]);
        if (!tt) continue;
        const rx_addr ib = U32(tt, 0x10), vb = U32(tt, 0x14);
        if (!ib || !vb) continue;
        if (U32(tt, 0x1c) != 8) continue;   /* vertex stride: SHORT4N */
        uint32_t ib_size = 0, vb_size = 0;
        const rx_addr ib_data = rx_d3d_bits(ib, 0, &ib_size), vb_data = rx_d3d_bits(vb, 0, &vb_size);
        if (!ib_data || !vb_data) continue;   /* the oracle's device stand-ins have no CPU view here */
        const uint32_t ring = U32(tt, 4), slots = U32(tt, 0xc), prims = U32(tt, 0x20), va = U32(tt, 0x24);
        if (!va || ring > 0x100 || slots > 64 || prims > 0x400) continue;
        for (uint32_t si = 0; si < slots; si++) {
            const rx_addr s = va + 0xc0u * (uint32_t)si;
            if (U32(s, 0x94) != 0) continue;   /* +0x94: free */
            const rx_addr tab = U32(s, 0), gens = U32(s, 0xc);
            if (!tab || !gens) continue;
            int32_t type = I32(s, 0xb4);
            if (type < 0 || type >= RX_TRACK_MATERIALS) type = 0;
            const int32_t slot_first = total * 3;
            int32_t slot_tris = 0;
            for (uint32_t c = 0; c < ring; c++) {
                if (U32(gens, 4u * (uint32_t)c) == 0) continue;   /* chunk generation: 0 = empty */
                const uint32_t voff = U32(tab, 12u * (uint32_t)c), start = U32(tab, 12u * (uint32_t)c + 4);
                if (voff >= vb_size) continue;
                int32_t chunk_tris = 0;
                for (uint32_t k = 0; k < prims; k++) {
                    const uint32_t ia = 2u * start + 2u * k;
                    if (ia + 6u > ib_size) break;
                    const uint16_t a = U16(ib_data, ia), b = U16(ib_data, ia + 2), cc = U16(ib_data, ia + 4);
                    if (a == b || b == cc || a == cc) continue;
                    if (a >= 0x100 || b >= 0x100 || cc >= 0x100) continue;
                    const rx_addr pa = vb_data + voff + 8u * a, pb = vb_data + voff + 8u * b, pc = vb_data + voff + 8u * cc;
                    if (voff + 8u * a + 8u > vb_size || voff + 8u * b + 8u > vb_size || voff + 8u * cc + 8u > vb_size) continue;
                    if (rx_rd64(pa) == 0 || rx_rd64(pb) == 0 || rx_rd64(pc) == 0) continue;   /* never written: all zero */
                    if (out && written < (int32_t)max_triangles) {
                        float* o = out + 18u * (size_t)written;
                        track_vertex(o, pa, hx, hz, type);
                        track_vertex(o + 6, pb, hx, hz, type);
                        track_vertex(o + 12, pc, hx, hz, type);
                        written++;
                    }
                    total++; slot_tris++; chunk_tris++;
                }
                if (chunk_tris > 0) nchunks++;
            }
            if (slot_tris > 0) {
                if (ribbons && nstrips < (int32_t)max_ribbons) {
                    rx_track_ribbon* r = &ribbons[nstrips];
                    memset(r, 0, sizeof *r);
                    r->set = g; r->slot = (int32_t)si; r->type = type;
                    r->first_vertex = slot_first; r->vertices = slot_tris * 3; r->triangles = slot_tris; r->flags = 3;
                }
                nstrips++;
            }
        }
    }
    *total_triangles = total;
    *strips = nstrips;
    *chunks = nchunks;
    return written;
}

/* The default size of the cached triangle list (Unity asks for 65,536). RX_TRACKS_NOCACHE=1 decodes every call afresh (A/B checks). */
#define RX_TRACKS_CACHE_DEFAULT 65536u
#define RX_TRACKS_RIBBONS       ((int32_t)(sizeof(((struct rx_session*)0)->tracks_cache.ribbons) / sizeof(rx_track_ribbon)))

static int tracks_cache_enabled(void) {
    static int on = -1;
    if (on < 0) { const char* v = getenv("RX_TRACKS_NOCACHE"); on = !(v && *v && *v != '0'); }
    return on;
}

static int tracks_cache_current(const struct rx_session* s, rx_addr t) {
    const struct rx_tracks_cache* c = &s->tracks_cache;
    return c->valid && c->version == s->tracks_version && c->frames == s->frames && c->terrain == t;
}

static void tracks_cache_note(struct rx_session* s, rx_addr t, int32_t total, int32_t strips, int32_t chunks, uint32_t have) {
    struct rx_tracks_cache* c = &s->tracks_cache;
    c->valid = 1; c->version = s->tracks_version; c->frames = s->frames; c->terrain = t;
    c->total = total; c->strips = strips; c->chunks = chunks; c->have = have;
    c->nribbons = strips < RX_TRACKS_RIBBONS ? strips : RX_TRACKS_RIBBONS;
}

/* One decode serves the totals, the strip descriptors and the triangle list of a frame (w-terrtiles). The decode of the tyre-track rings
 * walks every live chunk (1000 of them late in a race), and the three calls of a frame used to do it three times.
 *  - count only (out NULL): decodes into the cache's own list (65,536 triangles), so the full call that follows is a copy;
 *  - triangles (out): copies from the cache when it holds enough, else decodes straight into `out` (no copy) and keeps the totals;
 *  - strip descriptors: always from the totals of the frame's decode.
 * The cache is keyed by (tracks_version, frame, terrain) and dropped when an event loads. */
static int tracks_decode_cached(struct rx_session* s, rx_addr t, float* out, uint32_t max_triangles, int32_t* total, int32_t* strips, int32_t* chunks) {
    struct rx_tracks_cache* c = &s->tracks_cache;
    if (!tracks_cache_enabled()) {
        return tracks_decode(t, out, max_triangles, NULL, 0, total, strips, chunks);
    }
    const uint32_t want = out ? max_triangles : 0;
    if (!tracks_cache_current(s, t) || !(want <= c->have || c->have >= (uint32_t)c->total)) {
        int32_t tt = 0, ss = 0, cc = 0;
        memset(c->ribbons, 0, sizeof c->ribbons);
        if (out) {   /* the caller's buffer receives the decode; the cache keeps the totals and descriptors */
            const int w = tracks_decode(t, out, max_triangles, c->ribbons, (uint32_t)RX_TRACKS_RIBBONS, &tt, &ss, &cc);
            tracks_cache_note(s, t, tt, ss, cc, 0);
            *total = tt; *strips = ss; *chunks = cc;
            return w;
        }
        if (c->cap < RX_TRACKS_CACHE_DEFAULT) {
            float* n = (float*)realloc(c->tris, (size_t)RX_TRACKS_CACHE_DEFAULT * RX_TRACK_TRIANGLE_FLOATS * sizeof(float));
            if (n) { c->tris = n; c->cap = RX_TRACKS_CACHE_DEFAULT; }
        }
        const int w = tracks_decode(t, c->cap ? c->tris : NULL, c->cap, c->ribbons, (uint32_t)RX_TRACKS_RIBBONS, &tt, &ss, &cc);
        tracks_cache_note(s, t, tt, ss, cc, (uint32_t)w);
    }
    *total = c->total; *strips = c->strips; *chunks = c->chunks;
    if (!out) return 0;
    const uint32_t n = max_triangles < c->have ? max_triangles : c->have;
    memcpy(out, c->tris, (size_t)n * RX_TRACK_TRIANGLE_FLOATS * sizeof(float));
    return (int)n;
}

int rx_session_tracks(struct rx_session* s, float* out, uint32_t max_triangles, rx_track_info* info) {
    rx_track_info local;
    if (!info) info = &local;
    if (!session_ok(s)) { memset(info, 0, sizeof *info); return -1; }
    const rx_addr t = active_terrain();
    track_info_fill(s, info, t);
    if (!t) return 0;
    int32_t total = 0, strips = 0, chunks = 0;
    const int written = tracks_decode_cached(s, t, out, max_triangles, &total, &strips, &chunks);
    info->triangles = total;
    info->vertices = total * 3;
    info->strips = strips;
    info->chunks = chunks;
    return out ? written : 0;
}

int rx_session_track_ribbons(struct rx_session* s, rx_track_ribbon* out, uint32_t max, rx_track_info* info) {
    rx_track_info local;
    if (!info) info = &local;
    if (!session_ok(s)) { memset(info, 0, sizeof *info); return -1; }
    const rx_addr t = active_terrain();
    track_info_fill(s, info, t);
    if (!t) return 0;
    int32_t total = 0, strips = 0, chunks = 0;
    if (tracks_cache_enabled()) {
        if (!tracks_cache_current(s, t)) tracks_decode_cached(s, t, NULL, 0, &total, &strips, &chunks);   /* the frame's first call */
        const struct rx_tracks_cache* c = &s->tracks_cache;
        total = c->total; strips = c->strips; chunks = c->chunks;
        if (out) {
            const uint32_t n = max < (uint32_t)c->nribbons ? max : (uint32_t)c->nribbons;
            memcpy(out, c->ribbons, (size_t)n * sizeof *out);
        }
    } else {
        tracks_decode(t, NULL, 0, out, max, &total, &strips, &chunks);
    }
    info->triangles = total;
    info->vertices = total * 3;
    info->strips = strips;
    info->chunks = chunks;
    return out ? (strips < (int32_t)max ? strips : (int32_t)max) : 0;
}

/* ---------------------------------------------------------------------------------------------------------------- fx / mud */

/* IEEE half -> float, bit for bit (D3DXFLOAT16, as the frame table stores it). */
static float half_to_float(uint16_t h) {
    uint32_t s = (uint32_t)(h >> 15) << 31, e = (h >> 10) & 31u, m = h & 1023u, bits;
    if (e == 0) {
        if (!m) bits = s;
        else { e = 113; while (!(m & 1024u)) { m <<= 1; e--; } bits = s | (e << 23) | ((m & 1023u) << 13); }
    } else if (e == 31) bits = s | 0x7f800000u | (m << 13);
    else bits = s | ((e + 112u) << 23) | (m << 13);
    float f;
    memcpy(&f, &bits, sizeof f);
    return f;
}

/* One particle record (0x80 bytes) as ParticleManager2D_BuildInstances (00699a50 / 00699e70) reads it: the quad's position, half
 * extents, pivot, scale, rotation, the two colours and the atlas frame (the two half floats of the asset's frame table entry). */
static void read_particle(rx_addr arr, uint32_t i, uint32_t manager, rx_addr frame_table, uint32_t frame_count, rx_particle* p) {
    const rx_addr q = arr + 0x80u * i;
    memset(p, 0, sizeof *p);
    p->position[0] = F32(q, 0x00); p->position[1] = F32(q, 0x04); p->position[2] = F32(q, 0x08);
    p->size[0] = F32(q, 0x30); p->size[1] = F32(q, 0x34);
    p->pivot[0] = F32(q, 0x70); p->pivot[1] = F32(q, 0x74);
    p->scale = F32(q, 0x40);
    p->rotation = F32(q, 0x38);
    p->color = U32(q, 0x4c);
    p->color1 = U32(q, 0x50);
    /* frame: (int)(float) +0x54, out of range / NaN -> the last frame */
    const float fr = F32(q, 0x54);
    const int32_t fi = isfinite(fr) && fabsf(fr) < 2.1e9f ? (int32_t)fr : -1;
    if (frame_count && frame_table) {
        const uint32_t e = (fi >= 0 && (uint32_t)fi < frame_count) ? (uint32_t)fi : frame_count - 1;
        uint16_t h[4];
        for (int k = 0; k < 4; k++) h[k] = (uint16_t)U16(frame_table, 8u * e + 2u * (uint32_t)k);
        p->uv[0] = half_to_float(h[2]); p->uv[1] = half_to_float(h[3]);
        p->uv[2] = half_to_float(h[0]); p->uv[3] = half_to_float(h[1]);
    }
    p->flags = manager;   /* bit 0: the additive manager */
}

/* the order of one manager's particles: far to near view-depth buckets (int(min(max((p . fwd + vw - near) * 10000 / (far - near) + 1, 0),
 * 10001)); within a bucket the last index first), at most 3000 - as 00699a50 sorts them. */
struct particle_order { int32_t bucket; uint32_t index; };

static int particle_order_cmp(const void* a, const void* b) {
    const struct particle_order* x = (const struct particle_order*)a;
    const struct particle_order* y = (const struct particle_order*)b;
    if (x->bucket != y->bucket) return x->bucket > y->bucket ? -1 : 1;
    return x->index > y->index ? -1 : 1;
}

#define RX_PARTICLE_BUCKETS 10000
#define RX_PARTICLE_DRAW_CAP 3000

/* Fills *info for both managers and, when out != NULL, writes at most max particles (normal manager first). The info counts are
 * the managers' full exports (as the old plugin). Returns the number written. */
static int32_t particle_export(struct rx_session* s, rx_particle* out, uint32_t max, rx_particle_info* info) {
    (void)s;
    static const rx_addr managers[2] = {0x00c11bd0u, 0x00c11d80u};   /* type 2 normal, type 1 additive */
    memset(info, 0, sizeof *info);
    const rx_addr cam = U32(0, 0x00a7b1d8);
    float pos[3] = {0, 0, 0}, fwd[3] = {0, 0, 1}, near_plane = 0.1f, far_plane = 1000.0f;
    if (cam) {
        for (int i = 0; i < 3; i++) { pos[i] = F32(cam, 0x10 + 4u * (uint32_t)i); fwd[i] = F32(cam, 0x40 + 4u * (uint32_t)i); }
        near_plane = F32(cam, 0x58); far_plane = F32(cam, 0x5c);
    }
    if (!(far_plane > near_plane) || !isfinite(far_plane - near_plane)) { near_plane = 0.1f; far_plane = 1000.0f; }
    const float vw = -(pos[0] * fwd[0] + pos[1] * fwd[1] + pos[2] * fwd[2]);
    const float bucket_scale = 1.0f / (far_plane - near_plane) * (float)RX_PARTICLE_BUCKETS;
    uint32_t written = 0;
    for (int k = 0; k < 2; k++) {
        const rx_addr m = managers[k];
        const rx_addr handle = U32(m, 0x70);
        const rx_addr arr = U32(m, 0x14);
        uint32_t n = U32(m, 0xac);
        const uint32_t cap = U32(m, 0x1c) + 2u;
        if (!handle || !arr || !n) continue;
        const rx_addr ad = U32(handle, 0x44);
        if (!ad) continue;
        if (!info->texture[0]) {   /* the first manager with data: its atlas and luminance (00699423) */
            const rx_addr tex = U32(m, 0x8c);
            if (tex)
                for (int i = 0; i < 63; i++) {
                    const char c = (char)U8(tex, 4u + (uint32_t)i);
                    if (!c) break;
                    info->texture[i] = c;
                }
            for (int i = 0; i < 3; i++) {
                const float sl = F32(ad, 0x60 + 4u * (uint32_t)i), pl = F32(ad, 0x70 + 4u * (uint32_t)i);
                info->luminance[i] = pl * sl;
                info->luminance_delta[i] = pl * (1.0f - sl);
            }
            info->luminance[3] = 1.0f;
            info->luminance_delta[3] = 0.0f;
        }
        if (n > cap) n = cap;
        uint32_t drawable = n;
        const rx_addr frame_table = U32(ad, 0x18);
        uint32_t frame_count = U32(ad, 0x1c);
        if (frame_count > 4096u) frame_count = 0;   /* not a frame table (guards the host buffer below) */
        struct particle_order* order = (struct particle_order*)malloc((size_t)n * sizeof *order);
        uint32_t count = 0;
        if (order) {
            for (uint32_t i = 0; i < n; i++) {
                const rx_addr q = arr + 0x80u * i;
                const float px = F32(q, 0x00), py = F32(q, 0x04), pz = F32(q, 0x08);
                float d = (pz * fwd[2] + py * fwd[1] + px * fwd[0] + (vw - near_plane)) * bucket_scale + 1.0f;
                if (!(d < (float)(RX_PARTICLE_BUCKETS + 1))) d = (float)(RX_PARTICLE_BUCKETS + 1);
                if (d < 0.0f) d = 0.0f;
                const int32_t bucket = (int32_t)d;
                if (bucket >= 1 && bucket <= RX_PARTICLE_BUCKETS) { order[count].bucket = bucket; order[count].index = i; count++; }
            }
            qsort(order, count, sizeof *order, particle_order_cmp);
            if (count > RX_PARTICLE_DRAW_CAP) count = RX_PARTICLE_DRAW_CAP;
            drawable = count;
            if (out) {
                for (uint32_t i = 0; i < count && written < max; i++)
                    read_particle(arr, order[i].index, (uint32_t)k, frame_table, frame_count, &out[written++]);
            }
            free(order);
        }
        if (k == 0) info->normal = (int32_t)drawable;
        else info->additive = (int32_t)drawable;
        info->live += (int32_t)n;
    }
    info->soft = U32(0, 0x00c11d04u) != 0;
    /* g_ScreenAreaToFadeAndAlphaKill (c100) for the main view (00698890), as particles.cpp */
    {
        const float a = F32(0, 0x00c11d10u), b = F32(0, 0x00c11d14u);
        const float lo = a - b > 0.0f ? a - b : 0.0f;
        const float k = 1.0f / (a - lo);
        if (isfinite(k) && isfinite(a)) { info->fade[0] = k * -1.0f; info->fade[1] = k * a; }
        else { info->fade[0] = 0.0f; info->fade[1] = 1.0f; }
        info->fade[2] = F32(0, 0x009c475cu);
    }
    info->near_plane = near_plane;
    info->far_plane = far_plane;
    return (int32_t)written;
}

int rx_session_particles(struct rx_session* s, rx_particle_info* out) {
    if (!session_ok(s) || !out) return 1;
    particle_export(s, NULL, 0, out);
    return 0;
}

int rx_session_particles_array(struct rx_session* s, rx_particle* out, uint32_t max, rx_particle_info* info) {
    if (!session_ok(s) || !info) return -1;
    if (!out) return -2;
    return particle_export(s, out, max, info);
}

/* The ambient entity class table: the vtable of the object and of its IRenderable (+0x90), in the old plugin's kind order
 * (World::set_ambient_mask bits 0..9, then the always-simulated classes; native/drafts/world_runtime/ambient.cpp KINDS), then the
 * classes the port added after it (16 ThirtySecondBoard). */
enum { AK_ANIM_LOOP, AK_SCENARIO, AK_SPLINE_LOOP, AK_CREW_CHIEF, AK_INSTANCED, AK_ANIM_LOOP_PARTS, AK_SPLINE_LOOP_PARTS,
       AK_LAND_ANIMAL, AK_BILLBOARD, AK_SCORE_TOWER, AK_TRACKSIDE, AK_SPLINE_STOP, AK_SPLINE_STOP_PARTS, AK_PROXIMITY, AK_TRIGGER,
       AK_AIR_ANIMAL, AK_BOARD, AK_COUNT };
static const struct { uint32_t vt0, vt90; } k_ambient_kinds[AK_COUNT] = {
    {0x0095f674, 0x0095f6f4}, {0x0096cee4, 0x0096cf64}, {0x0095f83c, 0x0095f8bc},
    {0x0095f4e4, 0x0095f564}, {0x00939d24, 0x00939d9c}, {0x0095f73c, 0x0095f7bc},
    {0x0095f904, 0x0095f984}, {0x00960dec, 0x00960e6c}, {0x00960654, 0x009606cc},
    {0x00960094, 0x00960114}, {0x00960214, 0x00960294}, {0x0095f9cc, 0x0095fa4c},
    {0x0095fa94, 0x0095fb14}, {0x0095f5ac, 0x0095f62c}, {0x0095fb5c, 0x0095fbdc},
    {0x00960c6c, 0x00960cec},
    /* 16: ThirtySecondBoard (0x103a, a ScenarioAnimEntity: the National / Supercross start board carried by the "30 seconds" girl;
     * anim_ik_ent.c). Not in the old plugin's 16 kinds, so the renderer never drew it; the same skeleton layout as kind 1. */
    {0x0095f41c, 0x0095f49c},
};
#define RX_TEXTURE_HANDLE_VT 0x009719c8u   /* the TextureHandle class: name inline at +4, TextureAsset at +0x44 */
#define RX_TEXTURE_ASSET_VT  0x009462d0u
#define RX_MODEL_ASSET_VT    0x00972844u
#define RX_AMBIENT_FLAG_VISIBLE    0x1u
#define RX_AMBIENT_FLAG_SKINNED    0x2u
#define RX_AMBIENT_FLAG_INSTANCE   0x4u
#define RX_AMBIENT_FLAG_TEMPLATE   0x8u
#define RX_AMBIENT_FLAG_ANIMATED   0x10u
#define RX_AMBIENT_FLAG_COMPLETED  0x20u

/* guest string of at most n-1 bytes at a ("" for 0); always NUL terminated */
static void copy_guest_string(char* dst, uint32_t n, rx_addr a) {
    if (!dst || n == 0) return;
    uint32_t i = 0;
    if (a) for (; i + 1 < n; i++) { const uint8_t c = U8(a, i); dst[i] = (char)c; if (!c) return; }
    dst[i] = 0;
}

static int ambient_kind_of(uint32_t vt, uint32_t vt90) {
    for (int k = 0; k < AK_COUNT; k++) if (vt == k_ambient_kinds[k].vt0 && vt90 == k_ambient_kinds[k].vt90) return k;
    return -1;
}

/* The class-table scan (the old plugin's AmbientScene::scan): every heap block, the object's vtable at +0 and its IRenderable's
 * at +0x90, plus the texture handles the crew chiefs' shirt override needs (the old scan collects them in the same page pass).
 * Objects live as long as the world, so the result is cached; the scan itself is read-only. */
static int ambient_scan(struct rx_session* s) {
    s->ambient_objects_count = 0;
    s->ambient_textures_count = 0;
    const int blocks = rx_heap_blocks();
    for (int i = 0; i < blocks; i++) {
        rx_addr address = 0;
        uint32_t size = 0;
        int live = 0;
        rx_heap_block(i, &address, &size, &live);
        if (!live || size < 0x48) continue;
        for (uint32_t o = 0; o + 0x48 <= size; o += 16u) {   /* heap blocks are 16-aligned: objects start on 16 */
            const uint32_t vt = U32(address, o);
            if (vt == RX_TEXTURE_HANDLE_VT) {
                const uint32_t data = U32(address + o, 0x44);
                if (data) {
                    if (s->ambient_textures_count == s->ambient_textures_cap) {
                        const int32_t cap = s->ambient_textures_cap ? s->ambient_textures_cap * 2 : 256;
                        void* p = realloc(s->ambient_textures, (size_t)cap * sizeof *s->ambient_textures);
                        if (!p) return 0;
                        s->ambient_textures = (struct rx_ambient_texture*)p;
                        s->ambient_textures_cap = cap;
                    }
                    s->ambient_textures[s->ambient_textures_count].data = data;
                    s->ambient_textures[s->ambient_textures_count].handle = address + o;
                    s->ambient_textures_count++;
                }
                continue;
            }
            if (o + 0x94 > size) continue;   /* the ambient classes' IRenderable vtable at +0x90 */
            if (vt < 0x00939d24u || vt > 0x0096cee4u) continue;
            if (ambient_kind_of(vt, U32(address + o, 0x90)) < 0) continue;
            if (s->ambient_objects_count == s->ambient_objects_cap) {
                const int32_t cap = s->ambient_objects_cap ? s->ambient_objects_cap * 2 : 256;
                void* p = realloc(s->ambient_objects, (size_t)cap * sizeof *s->ambient_objects);
                if (!p) return 0;
                s->ambient_objects = (struct rx_ambient_object*)p;
                s->ambient_objects_cap = cap;
            }
            s->ambient_objects[s->ambient_objects_count].address = address + o;
            s->ambient_objects[s->ambient_objects_count].kind = ambient_kind_of(vt, U32(address + o, 0x90));
            s->ambient_objects_count++;
        }
    }
    s->ambient_scanned = 1;
    return 1;
}

/* The scan needs a world: before the load frames create the scene (state Loading) there is nothing to export and the result is
 * not cached (the old plugin only ran it on a live world). */
static void ambient_ensure(struct rx_session* s) {
    if (s->ambient_scanned) return;
    if (s->state == RX_SESSION_LOADING) return;
    ambient_scan(s);
}

/* The renderer's LOD divisor ([[0x00a95844 + 0x1c + 4 * [0x00a95844 + 4]] + 0x19d0]), 1 when there is none. */
static uint32_t ambient_lod_divisor(void) {
    const rx_addr renderer = U32(0, 0x00a95844u);
    if (!renderer) return 1;
    const rx_addr p = U32(renderer, 0x1c + 4u * U32(renderer, 4));
    const uint32_t d = p ? U32(p, 0x19d0) : 0;
    return d ? d : 1;
}

static rx_addr ambient_texture_handle(const struct rx_session* s, uint32_t data) {
    for (int32_t i = 0; i < s->ambient_textures_count; i++) if (s->ambient_textures[i].data == data) return s->ambient_textures[i].handle;
    return 0;
}

/* The crowd programs' emptyChairRatio (c66) of a BillboardCrowdEntity, as XenonBillboardCrowdRenderEvent 006c74a6 / 006c7d9b compute it:
 * ((tier - 1) * 0.133 + (4 - tier)) / 3 x the crowd density [0x009c3ed8], 100 when the density reaches 100 or the tier is 0. */
static float crowd_empty_chair_ratio(rx_addr a) {
    const int32_t tier = I32(a, 0x148);
    const float density = F32(0, 0x009c3ed8u);
    float ratio = (float)((double)(tier - 1) * 0.133 + (double)(4 - tier)) * F32(0, 0x0092e214u) * density;
    if (density >= F32(0, 0x0093005cu) || tier == 0) ratio = F32(0, 0x0093005cu);
    return ratio;
}

/* One skeleton entity (the old plugin's AmbientScene::collect's `character`): fills e from the object's live fields and appends
 * its node world matrices. Nodes the animation job left invalid are completed by the game's own evaluator (007db890), which
 * writes the world arrays and the valid bits exactly as the renderer's lazy evaluation would. */
static void ambient_character(struct rx_session* s, rx_ambient_entity* e, rx_addr a, int kind,
                              const rx_addr* templates, int ntemplates,
                              float* matrices, uint32_t max_matrices, uint32_t* matrix_count,
                              float* L, float* W, uint8_t* rec) {
    memset(e, 0, sizeof *e);
    e->object_id = a;
    e->entity_id = I32(a, 0x14);
    e->kind = kind;
    e->source = -1;
    const rx_addr handle = U32(a, 0x100);
    copy_guest_string(e->model, sizeof e->model, handle ? handle + 4 : 0);
    const uint32_t divisor = ambient_lod_divisor();
    const rx_addr md = handle ? U32(handle, 0x44) : 0;
    e->lod_distance = md ? (float)(U32(md, 0x120) / divisor) : 0.0f;
    if (U8(a, 0xa2)) e->flags |= RX_AMBIENT_FLAG_VISIBLE;
    for (int k = 0; k < ntemplates; k++) if (templates[k] == a) { e->flags |= RX_AMBIENT_FLAG_TEMPLATE; break; }
    copy_floats(e->matrix, a + 0x30, 16);
    if (kind == AK_CREW_CHIEF) {
        /* FUN_006bd760's override map at +0x38c (head) / +0x390 (size); the one node's value at +0x10 is a TextureAsset */
        const rx_addr head = U32(a, 0x38c), size = U32(a, 0x390);
        const rx_addr node = size ? U32(head, 4) : 0;
        const rx_addr value = node && node != head ? U32(node, 0x10) : 0;
        if (value && U32(value, 0) == RX_TEXTURE_ASSET_VT) {
            const rx_addr tex = ambient_texture_handle(s, value);
            if (tex) copy_guest_string(e->texture, sizeof e->texture, tex + 4);
            const rx_addr pkg = U32(value, 0xc);
            if (pkg) copy_guest_string(e->texture_package, sizeof e->texture_package, pkg);
        }
    }
    if (kind == AK_BILLBOARD) {
        /* XenonBillboardCrowdRenderEvent 006c74a6: c65 time +0x140, Is Non Animated +0x144, tier +0x148, the flash array +0x17c ..
         * +0x180 (entries of 16 bytes, byte 0 active) */
        const float ratio = crowd_empty_chair_ratio(a);
        e->param[0] = F32(a, 0x140);
        e->param[1] = ratio;
        e->param[2] = U32(a, 0x144) ? 1.0f : 0.0f;
        if (!U32(a, 0x144)) e->flags |= RX_AMBIENT_FLAG_ANIMATED;
        const rx_addr b = U32(a, 0x17c), en = U32(a, 0x180);
        int32_t active = 0;
        for (rx_addr f = b; b && f < en && f < b + 64u * 16u; f += 16u) active += U8(f, 0) != 0;
        e->param[3] = (float)active;
        return;
    }
    /* the skeleton: the node set of the instance's ModelAsset copy (+0x200; records +0x218, count +0x21c, local +0x228, world +0x22c) */
    const uint32_t n = U32(a, 0x21c), records = U32(a, 0x218), lw = U32(a, 0x228), ww = U32(a, 0x22c);
    if (U32(a, 0x200) != RX_MODEL_ASSET_VT || n == 0 || n > 255 || !records || !lw || !ww) return;
    for (uint32_t i = 0; i <= n; i++) {
        for (uint32_t k = 0; k < 16; k++) L[i * 16 + k] = F32(lw, 64u * i + 4u * k);
        for (uint32_t k = 0; k < 16; k++) W[i * 16 + k] = F32(ww, 64u * i + 4u * k);
        for (uint32_t k = 0; k < 16; k++) rec[i * 16 + k] = U8(records, 16u * i + k);
    }
    for (uint32_t i = 0; i < n; i++) {
        if (U8(records, 16u * i + 0xc) & 1u) continue;
        e->flags |= RX_AMBIENT_FLAG_COMPLETED;
        rx_world_node_evaluate_world(records + 16u * i, 0);   /* 007db890: this node and its invalid ancestors */
    }
    if (e->flags & RX_AMBIENT_FLAG_COMPLETED)
        for (uint32_t i = 0; i <= n; i++)
            for (uint32_t k = 0; k < 16; k++) W[i * 16 + k] = F32(ww, 64u * i + 4u * k);
    e->flags |= RX_AMBIENT_FLAG_SKINNED;
    e->bones = (int32_t)n;
    e->first_matrix = (int32_t)*matrix_count;
    for (uint32_t i = 0; i < n; i++) {
        if (matrices && *matrix_count + i < max_matrices)
            for (uint32_t k = 0; k < 16; k++) matrices[16u * (*matrix_count + i) + k] = W[i * 16 + k];
    }
    *matrix_count += n;
    for (uint32_t k = 0; k < 16; k++) e->matrix[k] = W[n * 16 + k];
}

/* The entity of one scanned object, or 0 when it was freed / reused (an instance's `source` is filled by the caller). */
static int ambient_entity_write(struct rx_session* s, const struct rx_ambient_object* o, rx_ambient_entity* out,
                                float* matrices, uint32_t max_matrices, uint32_t* matrix_count,
                                const rx_addr* templates, int ntemplates, float* L, float* W, uint8_t* rec) {
    const rx_addr a = o->address;
    if (U32(a, 0) != k_ambient_kinds[o->kind].vt0 || U32(a, 0x90) != k_ambient_kinds[o->kind].vt90) return 0;
    if (o->kind == AK_INSTANCED) {
        const rx_addr t = U32(a, 0x9f0) ? U32(a, 0x9f0) : U32(a, 0x9f4);
        int source = -1;
        for (int i = 0; i < ntemplates; i++) if (templates[i] == t) { source = i; break; }
        if (source < 0) return 0;
        memset(out, 0, sizeof *out);
        out->object_id = a;
        out->entity_id = I32(a, 0x14);
        out->kind = o->kind;
        out->source = source;
        out->flags = RX_AMBIENT_FLAG_INSTANCE;   /* the template's visible flag is added by the caller */
        copy_floats(out->matrix, a + 0xa00, 16);
        out->lod_distance = F32(a, 0xa64);
        out->param[0] = (float)U8(a, 0xa6c);
        return 1;
    }
    ambient_character(s, out, a, o->kind, templates, ntemplates, matrices, max_matrices, matrix_count, L, W, rec);
    return 1;
}

int rx_session_ambient(struct rx_session* s, rx_ambient_info* out) {
    if (!session_ok(s) || !out) return 1;
    memset(out, 0, sizeof *out);
    int32_t n = rx_session_ambient_entities(s, NULL, 0, NULL, 0, out);
    return n < 0 ? 1 : 0;
}

int rx_session_ambient_rescan(struct rx_session* s) {
    if (!session_ok(s)) return 1;
    s->ambient_scanned = 0;
    s->ambient_count = 0;
    s->ambient_objects_count = 0;
    s->ambient_textures_count = 0;
    return 0;
}

int rx_session_ambient_entities(struct rx_session* s, rx_ambient_entity* out, uint32_t max, float* matrices,
                                uint32_t max_matrices, rx_ambient_info* info) {
    if (!session_ok(s)) return -1;
    rx_ambient_info local;
    if (!info) info = &local;
    memset(info, 0, sizeof *info);
    ambient_ensure(s);
    info->fade_start = F32(0, 0x009c4b4cu);
    info->lod_divisor = (float)ambient_lod_divisor();
    info->objects = s->ambient_objects_count;
    const int32_t count = s->ambient_objects_count;
    if (count == 0) return 0;
    /* the crowd masters: an instance refers to its template by object address; templates are exported first so `source` is an
     * index into the same export (the old plugin's two passes) */
    rx_addr* templates = (rx_addr*)malloc((size_t)count * sizeof *templates);
    int32_t* template_out = (int32_t*)malloc((size_t)count * sizeof *template_out);
    float* L = (float*)malloc(256u * 16u * sizeof *L);
    float* W = (float*)malloc(256u * 16u * sizeof *W);
    uint8_t* rec = (uint8_t*)malloc(256u * 16u);
    if (!templates || !template_out || !L || !W || !rec) {
        free(templates); free(template_out); free(L); free(W); free(rec);
        return -1;
    }
    for (int32_t i = 0; i < count; i++) template_out[i] = -1;
    int ntemplates = 0;
    for (int32_t i = 0; i < count; i++) {
        const struct rx_ambient_object* o = &s->ambient_objects[i];
        if (o->kind != AK_INSTANCED) continue;
        const rx_addr a = o->address;
        if (U32(a, 0) != k_ambient_kinds[o->kind].vt0 || U32(a, 0x90) != k_ambient_kinds[o->kind].vt90) continue;
        const rx_addr t = U32(a, 0x9f0) ? U32(a, 0x9f0) : U32(a, 0x9f4);
        if (!t || t == a) continue;
        int known = 0;
        for (int k = 0; k < ntemplates; k++) if (templates[k] == t) { known = 1; break; }
        if (!known) templates[ntemplates++] = t;
    }
    uint32_t written = 0, matrix_count = 0;
    int pass;
    /* pass 0: templates (the masters), pass 1: everything else */
    for (pass = 0; pass < 2; pass++) {
        for (int32_t i = 0; i < count; i++) {
            const struct rx_ambient_object* o = &s->ambient_objects[i];
            const rx_addr a = o->address;
            if (U32(a, 0) != k_ambient_kinds[o->kind].vt0 || U32(a, 0x90) != k_ambient_kinds[o->kind].vt90) continue;
            int templ = 0;
            for (int k = 0; k < ntemplates; k++) if (templates[k] == a) { templ = 1; break; }
            if ((pass == 0) != templ) continue;
            rx_ambient_entity e;
            if (!ambient_entity_write(s, o, &e, matrices, max_matrices, &matrix_count, templates, ntemplates, L, W, rec)) continue;
            if (templ) {
                e.flags |= RX_AMBIENT_FLAG_TEMPLATE;
                for (int k = 0; k < ntemplates; k++) if (templates[k] == a) { template_out[k] = (int32_t)written; break; }
            }
            if (o->kind == AK_INSTANCED) {
                /* model and visibility of the template (the old plugin copies the template's model and visible flag), and its
                 * index in THIS export (`source` must index the caller's array, not the local template table) */
                const int k = e.source;
                if (k < 0 || k >= ntemplates || template_out[k] < 0) continue;   /* the template is not in this export */
                const rx_addr t = templates[k];
                const rx_addr th = U32(t, 0x100);
                copy_guest_string(e.model, sizeof e.model, th ? th + 4 : 0);
                e.flags = RX_AMBIENT_FLAG_INSTANCE | (U8(t, 0xa2) ? RX_AMBIENT_FLAG_VISIBLE : 0u);
                e.source = template_out[k];
            }
            if (written < max && out) out[written] = e;
            written++;
        }
    }
    info->entities = (int32_t)written;
    info->matrices = (int32_t)matrix_count;
    free(templates); free(L); free(W); free(rec);
    if (!out) return 0;
    return (int32_t)(written > max ? max : written);
}

/* The event scene: the slot the world's subscenes load into (rx_scene_request_subscene: slot 1; slot 0 is the front end's
 * scene), else the renderer's active slot. */
static rx_addr scene_manager(void) {
    const rx_addr renderer = U32(0, 0x00a95844u);
    if (!renderer) return 0;
    rx_addr s = U32(renderer, 0x1c + 4u * 1u);
    if (!s) s = U32(renderer, 0x1c + 4u * U32(renderer, 4));
    return s;
}

/* Every entity of the scene's lists, deduplicated (an entity with sort flags is in its kind list and a flag list): the unsorted
 * list (scene + 0x2c28: {data, count, ...}) and the four sorted vectors (+0x2c3c / +0x2c54 / +0x2c6c / +0x2c84: begin +0xc,
 * end +0x10). Returns the count written to out (at most max). */
static int32_t scene_collect_entities(rx_addr scene, rx_addr* out, int32_t max) {
    enum { HASH = 16384 };
    uint32_t seen[HASH];
    memset(seen, 0, sizeof seen);
    int32_t n = 0;
    if (!scene) return 0;
    #define SCENE_ADD(e) do { \
        const rx_addr _e = (e); \
        if (!_e || n >= HASH - 1) break; \
        uint32_t _h = ((_e >> 4) * 2654435761u) & (HASH - 1); \
        while (seen[_h] && seen[_h] != _e) _h = (_h + 1) & (HASH - 1); \
        if (seen[_h] == _e) break; \
        if (n >= max) break; \
        seen[_h] = _e; out[n++] = _e; \
    } while (0)
    const rx_addr list = scene + 0x2c28;
    const int32_t count = I32(list, 4);
    for (int32_t i = 0; i < count; i++) SCENE_ADD(U32(U32(list, 0), 4u * (uint32_t)i));
    static const uint32_t k_vectors[4] = {0x2c3c, 0x2c54, 0x2c6c, 0x2c84};
    for (int v = 0; v < 4; v++) {
        const rx_addr vec = scene + k_vectors[v];
        const rx_addr first = U32(vec, 0xc), last = U32(vec, 0x10);
        if (!first || last <= first) continue;
        const uint32_t cnt = (last - first) / 4u;
        for (uint32_t i = 0; i < cnt; i++) SCENE_ADD(U32(first, 4u * i));
    }
    #undef SCENE_ADD
    return n;
}

/* The scene's entity list in the session's buffer (grown as needed); 0 outside a world. */
static int32_t scene_entities(struct rx_session* s) {
    const rx_addr scene = scene_manager();
    if (!scene) return 0;
    for (;;) {
        if (!s->scene_entities || s->scene_entities_cap == 0) {
            const int32_t cap = 4096;
            rx_addr* p = (rx_addr*)realloc(s->scene_entities, (size_t)cap * sizeof *p);
            if (!p) return 0;
            s->scene_entities = p;
            s->scene_entities_cap = cap;
        }
        const int32_t n = scene_collect_entities(scene, s->scene_entities, s->scene_entities_cap);
        if (n < s->scene_entities_cap || s->scene_entities_cap >= 65536) { s->scene_entities_count = n; return n; }
        const int32_t cap = s->scene_entities_cap * 2;
        rx_addr* p = (rx_addr*)realloc(s->scene_entities, (size_t)cap * sizeof *p);
        if (!p) return 0;
        s->scene_entities = p;
        s->scene_entities_cap = cap;
    }
}

int rx_session_scene_objects(struct rx_session* s, rx_scene_object* out, uint32_t max) {
    if (!session_ok(s)) return -1;
    if (!out || max == 0) return 0;
    const int32_t n = scene_entities(s);
    uint32_t written = 0;
    for (int32_t i = 0; i < n && written < max; i++) {
        const rx_addr e = s->scene_entities[i];
        rx_scene_object* o = &out[written];
        memset(o, 0, sizeof *o);
        o->vtable = U32(e, 0);
        o->entity_id = I32(e, 0x14);
        o->flags = (I32(e, 0x1c) ? 1 : 0) | (I32(e, 0x7c) ? 2 : 0);
        copy_floats(o->matrix, e + 0x30, 16);
        copy_guest_string(o->name, sizeof o->name, U32(e, 0x18));
        const rx_addr mi = e + 0xa0;   /* ModelInstance at +0xa0 when the class has one; its handle at +0x60 */
        const rx_addr handle = U32(mi, 0x60);
        if (handle) copy_guest_string(o->model, sizeof o->model, handle + 4);
        written++;
    }
    return (int32_t)written;
}

/* Only scene-owned physics scenery: exact primary and IRenderable vtable pairs. */
static const uint32_t prop_vtables[4][2] = {
    {0x0093743c, 0x009374c4}, {0x0095ffcc, 0x00960054},
    {0x0095ff04, 0x0095ff8c}, {0x00939034, 0x009390bc}
};
int rx_session_props(struct rx_session* s, rx_prop* out, uint32_t max) {
    if (!session_ok(s)) return -1;
    if (s->state < RX_SESSION_COUNTDOWN || s->state == RX_SESSION_ERROR) return 0;
    const rx_addr scene = scene_manager();
    if (!scene) return 0;
    if (s->prop_scene != scene) {
        const int32_t n = scene_entities(s);
        struct rx_prop_object* props = (struct rx_prop_object*)calloc((size_t)n, sizeof *props);
        if (n && !props) return -1;
        free(s->prop_objects); s->prop_objects = props; s->prop_count = 0;
        for (int32_t i = 0; i < n; i++) {
            const rx_addr e = s->scene_entities[i];
            for (int32_t k = 0; k < 4; k++) {
                if (U32(e, 0) != prop_vtables[k][0] || U32(e, 0x90) != prop_vtables[k][1]) continue;
                struct rx_prop_object* p = &props[s->prop_count++];
                p->address = e; p->kind = k; copy_floats(p->first, e + 0x30, 16);
                break;
            }
        }
        s->prop_scene = scene; s->props_reported = 0;
    }
    uint32_t written = 0, total = 0;
    for (int32_t i = 0; i < s->prop_count; i++) {
        const struct rx_prop_object* p = &s->prop_objects[i];
        const rx_addr e = p->address;
        if (U32(e, 0) != prop_vtables[p->kind][0] || U32(e, 0x90) != prop_vtables[p->kind][1]) continue;
        total++;
        if (!out || written >= max) continue;
        rx_prop* o = &out[written++];
        o->vtable = U32(e, 0); o->entity_id = I32(e, 0x14); o->kind = p->kind;
        copy_floats(o->matrix, e + 0x30, 16);
        o->flags = (U8(e, 0xa2) ? 1u : 0u) | (memcmp(o->matrix, p->first, 64) ? 2u : 0u) | (s->props_reported ? 0u : 4u);
        const rx_addr handle = U32(e, 0x158);
        if (handle) {
            const rx_addr world = U32(handle, 4);
            const uint32_t index = U32(handle, 8);
            if (world && U32(world, 0x19a4) >= U32(world, 0x19a0) &&
                index < (U32(world, 0x19a4) - U32(world, 0x19a0)) / 0xe0u) {
                const rx_addr body = U32(world, 0x19a0) + 0xe0u * index;
                o->flags |= 8u | (U8(body, 0xb0) ? 16u : 0u);
            }
        }
    }
    if (out && max >= total) s->props_reported = 1;
    return (int32_t)total;
}

int rx_session_scene_object_matrix(struct rx_session* s, uint32_t vtable, int32_t entity_id, uint32_t offset, float* out16) {
    if (!session_ok(s) || !out16) return 1;
    if ((offset & 3u) || offset > 0x2000u) return 1;
    const int32_t n = scene_entities(s);
    for (int32_t i = 0; i < n; i++) {
        const rx_addr e = s->scene_entities[i];
        if (U32(e, 0) != vtable || I32(e, 0x14) != entity_id) continue;
        /* the object is a live scene entity, and the arena maps every address: only the caller's offset is bounded (a matrix
         * offset from the scene setup, e.g. DropGate +0x30) */
        copy_floats(out16, e + offset, 16);
        return 0;
    }
    return 1;
}

int rx_session_node_flags(struct rx_session* s, int32_t slot, uint8_t* out, uint32_t max) {
    if (!session_ok(s) || (!out && max)) return 0;
    rx_addr p = 0;
    if (slot < 0) p = local_player();
    else if (slot < (int32_t)RX_RC_MAX) p = rx_seat(s->game + 0x7460, (uint32_t)slot);
    if (!p) return 0;
    const rx_addr v = U32(p, 0x1ac);
    if (!v) return 0;
    const rx_addr model = U32(v, 0x1328);   /* the vehicle model: hierarchy at +0x10, records +0x18, count +0x1c */
    if (!model) return 0;
    const rx_addr records = U32(model, 0x18);
    const uint32_t count = U32(model, 0x1c);
    if (!records || count == 0 || count > 512) return 0;
    const uint32_t n = count > max ? max : count;
    for (uint32_t i = 0; i < n; i++) out[i] = U8(records, 16u * i + 0xc);
    return (int32_t)count;
}

/* The game's mud table: 0xa96d90, 12 records of 0x40 (amount +0x00, wetness +0x10, colour +0x20, owner V +0x30). */
#define RX_MUD_TABLE 0x00a96d90u
#define RX_MUD_RECORDS 12u
#define RX_MUD_STRIDE 0x40u

int rx_session_mud(struct rx_session* s, int32_t slot, rx_mud_state* out) {
    if (!session_ok(s) || !out) return 1;
    memset(out, 0, sizeof *out);
    out->slot = slot;
    out->record = -1;
    rx_addr p = 0;
    if (slot < 0) p = local_player();
    else if (slot < (int32_t)RX_RC_MAX) p = rx_seat(s->game + 0x7460, (uint32_t)slot);
    if (!p) return 1;
    const rx_addr v = U32(p, 0x1ac);
    if (!v) return 1;
    out->vehicle = v;
    out->rider = U32(v, 0x1a0);
    for (uint32_t k = 0; k < rx_mud_n(); k++) {   /* (w-rc-core) + the side records taken (rider_cap.h) */
        const rx_addr r = rx_mud_rec(k);
        if (U32(r, 0x30) != v) continue;
        out->record = (int32_t)k;
        copy_vec4(out->amount, r + 0x00);
        copy_vec4(out->wetness, r + 0x10);
        copy_vec4(out->color, r + 0x20);
        return 0;
    }
    return 1;
}

int rx_session_mud_count(struct rx_session* s) {
    if (!session_ok(s)) return 0;
    int n = 0;
    for (uint32_t k = 0; k < rx_mud_n(); k++)
        if (U32(rx_mud_rec(k), 0x30)) n++;
    return n;
}

/* Isolated candidate public career API; shared session guards stay canonical. */
#include "career_public_session.inc"
#include "replay_session.inc"
#include "podium_session.inc"
#include "scene_pages_session.inc"
#include "crowd_flash_session.inc"
#include "activation_session.inc"
#include "challenge_session.inc"
#include "compass_session.inc"
#include "spawn_marker_session.inc"
#include "fbhud_session.inc"     /* w-fb-hud: controls in a race, RESET TRACK */
#include "rtbump_session.inc"

#include "powercross_hud_session.inc"
#include "race_format_session.inc"
#include "race_hud_session.inc"
#include "mode_results_session.inc"
#include "stunt_hud_session.inc"
#include "reflex_hud_session.inc"
#include "vehicle_audio_session.inc"
#include "rng_session.inc"
#include "water_session.inc"
#include "results_session.inc"
#include "sound_session.inc"
#include "ui_settings_session.inc"
#include "test_drive_session.inc"
#include "tutorial_session.inc"
#include "phys_studio_session.inc"
#include "practice_session.inc"
#include "champ_weekend_session.inc"   /* RESTORED CONTENT: the championship weekend */
