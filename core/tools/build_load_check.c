/* build_load_check.c -- consumption test of the asset loaders (core/notes/construction.md section 9).
 *   build_load_check <install root> <type> [database name filter]
 * Runs the core's loader of <type> (rx_load_run) over EVERY asset of that type in every database of the install (or the databases whose
 * name contains the filter): each loader must consume its stream exactly (no short read, position at the end). Prints a summary and the
 * first failures; exit status 1 when any asset fails. Nothing is compared with the original here (that is build_test's assets unit). */
#include "../src/fmt_db.h"
#include "../src/rx_heap.h"
#include "../game/build_load.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: build_load_check <install> <type> [database filter]\n"); return 2; }
    if (!rx_mem_init()) { fprintf(stderr, "arena reservation failed\n"); return 1; }
    rx_load_test_scaffold();
    rx_load_runtime_constants();
    fmt_install* in = fmt_install_open(argv[1]);
    if (!in) { fprintf(stderr, "install: %s\n", fmt_error()); return 1; }
    char** names = NULL;
    int nd = fmt_install_database_names(in, argc > 3 ? argv[3] : NULL, &names);
    int built = 0, bad = 0, shown = 0, skipped = 0;
    for (int d = 0; d < nd; d++) {
        const fmt_database* db = fmt_install_db(in, names[d]);
        if (!db) { fprintf(stderr, "database %s: %s\n", names[d], fmt_error()); continue; }
        for (int p = 0; p < db->npackages; p++)
            for (int k = 0; k < db->packages[p].nassets; k++) {
                const fmt_asset* a = &db->packages[p].assets[k];
                if (strcmp(a->type, argv[2])) continue;
                fmt_buf b = {0};
                if (fmt_install_read(in, a, &b) != 0) { if (shown++ < 20) fprintf(stderr, "%s/%s: read: %s\n", names[d], a->name, fmt_error()); bad++; continue; }
                rx_stream s; rx_stream_init(&s, b.data, b.size);
                rx_addr asset = rx_load_run(argv[2], &s, 0);
                if (!asset && !s.pos) { skipped++; fmt_buf_free(&b); continue; }
                if (s.short_read || s.pos != s.size) {
                    bad++;
                    if (shown++ < 20) fprintf(stderr, "%s/%s %s: %zu of %zu bytes read%s\n", names[d], a->name, a->type, s.pos, s.size, s.short_read ? " (short read)" : "");
                }
                built++;
                fmt_buf_free(&b);
            }
    }
    printf("%s: %d assets built, %d failed, %d without a loader; heap blocks %d\n", argv[2], built, bad, skipped, rx_heap_blocks());
    free(names);
    return bad ? 1 : 0;
}
