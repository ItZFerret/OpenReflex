/* rx_api_recreate_test.c -- create -> event load -> frames -> destroy, repeated in ONE process (core/notes/session-reset.md).
 *   rx_api_recreate_test <install root> [iterations (3)] [frames (10)] [level]
 *
 * Every iteration runs the same steps and records: a hash of the guest arena after each step (create, event load, frames), its
 * per-region split, the event-load result, the session state, the frame counts and the error text. Iterations 2.. must equal
 * iteration 1 in every field. The product still stops in an unported game function during the load frames; the API's fault guard
 * turns that into the error state, which is part of what must repeat (same text, same frame count, same arena).
 * After each destroy the arena must be unmapped and the heap table empty.
 * The results live in malloc memory: destroy restores the statics of this image, the test's own included (rx_reset.h).
 * Exit: 0 identical, 1 a mismatch or an API failure, 2 usage. Lines "RESULT iter=N ..." are machine-readable (the .sh compares
 * iteration 1 of this process with iteration 1 of a fresh process). */
#include "rx_api.h"
#include "rx_mem.h"
#include "rx_heap.h"
#include "rx_reset.h"
#include "rx_os.h"

#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/vm_page_size.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NREG 5
static const struct { uint32_t lo, hi; const char* name; } k_regions[NREG] = {
    {0x00000000u, 0x0fffffffu, "image"}, {0x10000000u, 0x6fffffffu, "heap"}, {0x70000000u, 0x7fdfffffu, "dll"},
    {0x7fe00000u, 0x7fffffffu, "stack"}, {0x80000000u, 0xffffffffu, "high"},
};

typedef struct { uint64_t all; uint64_t region[NREG]; uint64_t nonzero_pages; } arena_hash;

typedef struct {
    arena_hash created, loaded, final;
    int load_rc;
    int state;
    int load_frames, race_frames;
    unsigned long long frames;
    char error[512];
    long dirty_before_destroy;
} result;

static uint64_t mix(uint64_t h, uint64_t v) {
    h ^= v;
    h *= 0x100000001b3ull;
    h ^= h >> 29;
    return h;
}

/* Hash of the arena content: every touched non-zero page contributes (page address, words). Touched = resident or paged out
 * (mach_vm_page_range_query: reading an untouched page of the 4 GB reservation would fault it in, so the scan never does).
 * Zero pages (untouched or zeroed) do not contribute, so the result does not depend on which pages the OS kept resident. */
static arena_hash hash_arena(void) {
    arena_hash r;
    memset(&r, 0, sizeof r);
    r.all = 0xcbf29ce484222325ull;
    for (int i = 0; i < NREG; i++) r.region[i] = 0xcbf29ce484222325ull;
    if (!rx_base) return r;
    const uint64_t page = (uint64_t)vm_page_size;
    const uint64_t chunk = 256ull << 20;
    int* dispositions = (int*)malloc(sizeof(int) * ((256 << 20) / 4096));   /* heap: a static here would be part of the restored image */
    if (!dispositions) exit(1);
    for (uint64_t base = 0; base < 0x100000000ull; base += chunk) {
        mach_vm_size_t count = (mach_vm_size_t)(chunk / page);
        if (mach_vm_page_range_query(mach_task_self(), (mach_vm_address_t)(uintptr_t)(rx_base + base), chunk,
                                     (mach_vm_address_t)(uintptr_t)dispositions, &count) != KERN_SUCCESS) {
            fprintf(stderr, "rx_api_recreate_test: mach_vm_page_range_query failed\n");
            exit(1);
        }
        for (mach_vm_size_t pg = 0; pg < count; pg++) {
            if (!(dispositions[pg] & (VM_PAGE_QUERY_PAGE_PRESENT | VM_PAGE_QUERY_PAGE_PAGED_OUT))) continue;
            const uint64_t a = base + pg * page;
            const uint64_t* w = (const uint64_t*)(rx_base + a);
            uint64_t any = 0;
            for (uint64_t k = 0; k < page / 8; k++) any |= w[k];
            if (!any) continue;
            r.nonzero_pages++;
            int reg = 0;
            while (reg < NREG - 1 && a > k_regions[reg].hi) reg++;
            uint64_t h = mix(0xcbf29ce484222325ull, a);
            for (uint64_t k = 0; k < page / 8; k++) h = mix(h, w[k]);
            r.region[reg] = mix(r.region[reg], h);
            r.all = mix(r.all, h);
        }
    }
    free(dispositions);
    return r;
}

/* timeGetTime is the one wall clock the core reads: a counter that advances 16 ms per read makes the runs reproducible. */
static uint32_t g_ms;
static uint32_t fake_time_ms(void) { return g_ms += 16; }

static void on_signal(int sig) {
    char buf[96];
    const int n = snprintf(buf, sizeof buf, "rx_api_recreate_test: signal %d\n", sig);
    if (n > 0) (void)write(2, buf, (size_t)n);
    _exit(128 + sig);
}

static int same_hash(const arena_hash* a, const arena_hash* b) {
    return a->all == b->all && a->nonzero_pages == b->nonzero_pages && memcmp(a->region, b->region, sizeof a->region) == 0;
}

static void print_hash(const char* what, const arena_hash* h) {
    printf("rx_api_recreate_test:   %-8s arena %016llx (%llu non-zero pages;", what, (unsigned long long)h->all,
           (unsigned long long)h->nonzero_pages);
    for (int i = 0; i < NREG; i++) printf(" %s %016llx", k_regions[i].name, (unsigned long long)h->region[i]);
    printf(")\n");
}

static int run_once(const char* root, int frames, const char* level, result* out, int* fatal) {
    memset(out, 0, sizeof *out);
    rx_session* s = rx_session_create(root);
    if (!s) {
        fprintf(stderr, "rx_api_recreate_test: rx_session_create failed: %s\n", rx_session_last_error(NULL));
        *fatal = 1;
        return 1;
    }
    out->created = hash_arena();
    g_ms = 0;
    if (!getenv("RX_RECREATE_REAL_CLOCK")) rx_os_set_time_source(fake_time_ms);

    rx_event_spec spec;
    rx_session_event_default(&spec);
    if (level) snprintf(spec.location, sizeof spec.location, "%s", level);
    out->load_rc = rx_session_event_load(s, &spec);
    out->loaded = hash_arena();

    if (out->load_rc == 0) {
        while (rx_session_get_state(s) == RX_SESSION_LOADING && out->load_frames < 10000) {
            if (rx_session_advance(s) != 0) break;
            out->load_frames++;
        }
        for (int i = 0; i < frames && rx_session_get_state(s) != RX_SESSION_ERROR; i++) {
            if (rx_session_advance(s) != 0) break;
            out->race_frames++;
        }
    }
    out->state = (int)rx_session_get_state(s);
    out->frames = (unsigned long long)rx_session_frames(s);
    snprintf(out->error, sizeof out->error, "%s", rx_session_last_error(s));
    out->final = hash_arena();
    out->dirty_before_destroy = rx_reset_dirty();

    rx_session_destroy(s);
    if (rx_base) { fprintf(stderr, "rx_api_recreate_test: the arena is still mapped after destroy\n"); *fatal = 1; }
    if (rx_heap_blocks() != 0) { fprintf(stderr, "rx_api_recreate_test: the heap table is not empty after destroy\n"); *fatal = 1; }
    return 0;
}

static int compare(int iter, const result* a, const result* b) {
    int bad = 0;
#define CHECK(cond, what) do { if (!(cond)) { fprintf(stderr, "rx_api_recreate_test: iteration %d differs from iteration 1: %s\n", iter, what); bad++; } } while (0)
    CHECK(same_hash(&a->created, &b->created), "arena hash after create (the startup state)");
    CHECK(same_hash(&a->loaded, &b->loaded), "arena hash after event load");
    CHECK(same_hash(&a->final, &b->final), "arena hash after the frames");
    CHECK(a->load_rc == b->load_rc, "event load result");
    CHECK(a->state == b->state, "session state");
    CHECK(a->load_frames == b->load_frames && a->race_frames == b->race_frames, "frame counts");
    CHECK(a->frames == b->frames, "session frame count");
    CHECK(strcmp(a->error, b->error) == 0, "error text");
#undef CHECK
    return bad;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: rx_api_recreate_test <install root> [iterations (3)] [frames (10)] [level]\n");
        return 2;
    }
    const int iterations = argc > 2 ? atoi(argv[2]) : 3;
    const int frames = argc > 3 ? atoi(argv[3]) : 10;
    const char* level = argc > 4 ? argv[4] : NULL;
    if (iterations < 1) return 2;

    signal(SIGABRT, on_signal);
    signal(SIGSEGV, on_signal);
    signal(SIGBUS, on_signal);
    signal(SIGALRM, on_signal);
    {
        const char* w = getenv("RX_API_TEST_WATCHDOG");
        alarm(w && atoi(w) > 0 ? (unsigned)atoi(w) : 900u);
    }
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("rx_api_recreate_test: api version %u, statics restore %s, %d iterations, %d race frames\n", rx_session_api_version(),
           rx_reset_available() ? "available" : "NOT available (second create is refused)", iterations, frames);

    {   /* the refusals leave the process clean: a failed create, and a second create while a session is open */
        rx_session* bad_root = rx_session_create("/nonexistent/install/root");
        if (bad_root) { fprintf(stderr, "rx_api_recreate_test: create from a missing install succeeded\n"); return 1; }
        printf("rx_api_recreate_test: bad install -> NULL (%s)\n", rx_session_last_error(NULL));
        rx_session* first = rx_session_create(argv[1]);
        if (!first) { fprintf(stderr, "rx_api_recreate_test: create after a failed create: %s\n", rx_session_last_error(NULL)); return 1; }
        rx_session* second = rx_session_create(argv[1]);
        if (second) { fprintf(stderr, "rx_api_recreate_test: a second session opened while the first is live\n"); return 1; }
        printf("rx_api_recreate_test: second create while open -> NULL (%s)\n", rx_session_last_error(NULL));
        rx_session_destroy(first);
    }

    result* res = (result*)calloc((size_t)iterations, sizeof *res);
    if (!res) return 1;
    int bad = 0, fatal = 0;
    for (int i = 0; i < iterations && !fatal; i++) {
        printf("rx_api_recreate_test: iteration %d\n", i + 1);
        if (run_once(argv[1], frames, level, &res[i], &fatal) != 0) { bad++; break; }
        const result* r = &res[i];
        print_hash("created", &r->created);
        print_hash("loaded", &r->loaded);
        print_hash("final", &r->final);
        printf("rx_api_recreate_test:   event load rc %d, %d load frames + %d race frames, session frames %llu, state %d, "
               "%ld static bytes dirtied (informational: host pointers vary)\n", r->load_rc, r->load_frames, r->race_frames, r->frames, r->state, r->dirty_before_destroy);
        printf("rx_api_recreate_test:   error: %s\n", r->error[0] ? r->error : "(none)");
        printf("RESULT iter=%d created=%016llx loaded=%016llx final=%016llx load_rc=%d state=%d frames=%llu load_frames=%d race_frames=%d error=\"%s\"\n",
               i + 1, (unsigned long long)r->created.all, (unsigned long long)r->loaded.all, (unsigned long long)r->final.all,
               r->load_rc, r->state, r->frames, r->load_frames, r->race_frames, r->error);
        if (i > 0) bad += compare(i + 1, &res[0], r);
    }
    if (fatal) bad++;
    printf("rx_api_recreate_test: %s (%d iterations)\n", bad ? "MISMATCH" : "IDENTICAL", iterations);
    free(res);
    return bad ? 1 : 0;
}
