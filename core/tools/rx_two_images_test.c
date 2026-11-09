/* rx_two_images_test.c -- two core images in one process (w-loadarena): the client's test-ride arena while an event loads.
 *   rx_two_images_test <librxcore copy A> <librxcore copy B> <install root> [level (NAT_Concord)] [riders (12)]
 *
 * Image A rides the garage's test drive in the UI world (rx_session_test_drive_start, then frames at the frame cap) on one thread;
 * image B, a second loaded copy of the library (its own arena, statics and session), creates a session and loads an event
 * (rx_session_event_load and its load frames) on another thread at the same time. Reports the ride's frame times while the event
 * loads (the ride must keep running: its worst frame), both load times and the process's peak resident memory. A guarded stop in
 * either image ends only that image's call, with its own error text (api/rx_session.c's fault guard).
 * Exit 0: both sessions ran side by side, A's frames kept coming, B loaded; 1 otherwise. Build: tools/rx_two_images_test.sh. */
#include "rx_api.h"
#include "rx_test_drive.h"

#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>

typedef struct rx_session* (*create_fn)(const char*);
typedef void (*event_default_fn)(rx_event_spec*);
typedef int (*event_load_fn)(struct rx_session*, const rx_event_spec*);
typedef int (*session_fn)(struct rx_session*);
typedef int (*advance_dt_fn)(struct rx_session*, float);
typedef float (*step_dt_fn)(const struct rx_session*);
typedef rx_session_state (*state_fn)(const struct rx_session*);
typedef const char* (*error_fn)(const struct rx_session*);
typedef void (*destroy_fn)(struct rx_session*);
typedef int (*td_start_fn)(struct rx_session*, const rx_event_spec*);
typedef int (*td_read_fn)(struct rx_session*, rx_test_drive_state*);

struct image {
    void* h; const char* path;
    create_fn create; event_default_fn event_default; event_load_fn event_load; session_fn advance; advance_dt_fn advance_dt;
    step_dt_fn step_dt; state_fn state; error_fn error; destroy_fn destroy; td_start_fn td_start; td_read_fn td_read;
};

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

static int bind(struct image* im, const char* path) {
    im->path = path;
    im->h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!im->h) { fprintf(stderr, "dlopen %s: %s\n", path, dlerror()); return 0; }
#define B(f, n) if (!(im->f = (void*)dlsym(im->h, n))) { fprintf(stderr, "%s: no %s\n", path, n); return 0; }
    B(create, "rx_session_create") B(event_default, "rx_session_event_default") B(event_load, "rx_session_event_load")
    B(advance, "rx_session_advance") B(advance_dt, "rx_session_advance_dt") B(step_dt, "rx_session_step_dt")
    B(state, "rx_session_get_state") B(error, "rx_session_last_error") B(destroy, "rx_session_destroy")
    B(td_start, "rx_session_test_drive_start") B(td_read, "rx_session_test_drive")
#undef B
    return 1;
}

static const char* g_install; static const char* g_level = "NAT_Concord"; static int g_riders = 12;
static struct image A, Bimg;
static volatile int g_event_done, g_event_started, g_stop; static double g_t0;

/* the arena: the test drive, frames at the frame cap until told to stop */
static double a_ready = -1, a_worst = 0, a_sum = 0, a_worst_during = 0; static int a_frames, a_frames_during, a_ok = 1;
static void* arena(void* unused) {
    (void)unused;
    double t0 = now();
    struct rx_session* s = A.create(g_install);
    if (!s) { fprintf(stderr, "A create: %s\n", A.error(NULL)); a_ok = 0; return NULL; }
    if (A.td_start(s, NULL) != 0) { fprintf(stderr, "A test drive: %s\n", A.error(s)); a_ok = 0; A.destroy(s); return NULL; }
    while (!g_stop) {
        double f0 = now();
        if (A.advance_dt(s, 1.0f / 60.0f) != 0) { fprintf(stderr, "A frame: %s\n", A.error(s)); a_ok = 0; break; }
        double dt = now() - f0;
        if (a_ready < 0 && A.state(s) != RX_SESSION_LOADING) { a_ready = now() - t0; printf("A: riding the UI world %.2f s after create (state %d)\n", a_ready, (int)A.state(s)); }
        a_frames++; a_sum += dt; if (dt > a_worst) a_worst = dt;
        if (!g_event_done) { a_frames_during++; if (dt > a_worst_during) a_worst_during = dt; }
        if (dt > 0.025) printf("A: a %.1f ms frame at %.2f s (%s)\n", dt * 1e3, now() - g_t0, g_event_done ? "B loaded" : g_event_started ? "B loading" : "B not started");
        double left = 1.0 / 60.0 - (now() - f0);
        if (left > 0) { struct timespec w = { 0, (long)(left * 1e9) }; nanosleep(&w, NULL); }
    }
    rx_test_drive_state td; memset(&td, 0, sizeof td); td.size = sizeof td; td.version = RX_TEST_DRIVE_API_VERSION;
    A.td_read(s, &td);
    printf("A: test drive active %d spawned %d vehicle type %d frames %u, state %d\n", td.active, td.spawned, td.vehicle_type, td.frames, (int)A.state(s));
    if (!td.spawned || A.state(s) == RX_SESSION_ERROR) a_ok = 0;
    A.destroy(s);
    return NULL;
}

static double b_load = -1; static int b_ok = 0;
static void* event(void* unused) {
    (void)unused;
    double t0 = now(); g_event_started = 1;
    printf("B: starts at %.2f s\n", t0 - g_t0);
    struct rx_session* s = Bimg.create(g_install);
    if (!s) { fprintf(stderr, "B create: %s\n", Bimg.error(NULL)); g_event_done = 1; return NULL; }
    double t1 = now();
    rx_event_spec spec; Bimg.event_default(&spec);
    snprintf(spec.location, sizeof spec.location, "%s", g_level);
    spec.riders = g_riders;
    if (Bimg.event_load(s, &spec) != 0) { fprintf(stderr, "B event load: %s\n", Bimg.error(s)); Bimg.destroy(s); g_event_done = 1; return NULL; }
    double t2 = now(); int frames = 0;
    while (Bimg.state(s) == RX_SESSION_LOADING && frames < 10000) { if (Bimg.advance(s) != 0) break; frames++; }
    b_load = now() - t0;
    b_ok = Bimg.state(s) != RX_SESSION_ERROR && Bimg.state(s) != RX_SESSION_LOADING;
    printf("B: %s, %d riders: create %.2f s, event load %.2f s, %d load frames %.2f s: %.2f s in all, state %d%s%s\n", g_level, g_riders,
           t1 - t0, t2 - t1, frames, now() - t2, b_load, (int)Bimg.state(s), b_ok ? "" : ", error: ", b_ok ? "" : Bimg.error(s));
    g_event_done = 1;
    Bimg.destroy(s);
    return NULL;
}

int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "usage: rx_two_images_test <lib A> <lib B> <install root> [level] [riders]\n"); return 2; }
    g_install = argv[3]; if (argc > 4) g_level = argv[4]; if (argc > 5) g_riders = atoi(argv[5]);
    if (!bind(&A, argv[1]) || !bind(&Bimg, argv[2])) return 1;
    if (A.h == Bimg.h || A.create == Bimg.create) { fprintf(stderr, "the two paths loaded ONE image (same handle / symbols): copy the library to two files\n"); return 1; }
    pthread_t ta, tb; g_t0 = now();
    pthread_create(&ta, NULL, arena, NULL);
    while (a_ready < 0 && a_ok) { struct timespec w = { 0, 10000000 }; nanosleep(&w, NULL); }   /* riding first, then the event loads */
    pthread_create(&tb, NULL, event, NULL);
    pthread_join(tb, NULL);
    struct timespec w = { 2, 0 }; nanosleep(&w, NULL);   /* two more seconds of riding after the load */
    g_stop = 1;
    pthread_join(ta, NULL);
    struct rusage ru; getrusage(RUSAGE_SELF, &ru);
    printf("A: %d frames, mean %.2f ms, worst %.2f ms; while B loaded: %d frames, worst %.2f ms\n", a_frames, a_frames ? a_sum / a_frames * 1e3 : 0,
           a_worst * 1e3, a_frames_during, a_worst_during * 1e3);
    printf("peak resident %.0f MB\n", ru.ru_maxrss / (1024.0 * 1024.0));
    int ok = a_ok && b_ok && a_frames_during > 0;
    printf("RESULT %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
