/* level_entities_dump.c -- the entities of a level's scene file by class and name (w-ridefx: which of UI_Compound's entities are the MotoSkills
 * tutorials' furniture, and of which RSData class: only a CollisionRSData / physics class can be touched by a rider).
 *   level_entities_dump <install root> <level> [name substring ...]
 * With no substring: the entity count per subscene and class; with substrings: every entity whose Name (or model asset) contains one of them.
 * Build: clang -std=c11 -O2 -Icore/src -Icore/include core/tools/level_entities_dump.c core/src/fmt_*.c -lz -o <out> */
#include "fmt_scene.h"
#include "fmt_vfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: level_entities_dump <install root> <level> [substring ...]\n"); return 2; }
    fmt_vfs* v = fmt_vfs_open(argv[1]);
    if (!v) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
    char path[512]; snprintf(path, sizeof path, "Database/%s.dx9.scene", argv[2]);
    fmt_buf b; memset(&b, 0, sizeof b);
    if (fmt_vfs_read_file(v, path, &b) < 0) { fprintf(stderr, "cannot read %s\n", path); return 1; }
    fmt_scene* sc = fmt_scene_parse(b.data, b.size);
    if (!sc) { fprintf(stderr, "cannot parse %s\n", path); return 1; }
    for (int si = 0; si < fmt_scene_subscene_count(sc); si++) {
        int n = 0; const fmt_entity* ents = fmt_scene_entities(sc, si, &n);
        printf("subscene %d '%s': %d entities\n", si, fmt_scene_subscene_name(sc, si), n);
        if (argc == 3) {
            for (int i = 0; i < n; i++) {
                int seen = 0; for (int j = 0; j < i; j++) if (!strcmp(ents[j].type, ents[i].type)) { seen = 1; break; }
                if (seen) continue;
                int c = 0; for (int j = 0; j < n; j++) if (!strcmp(ents[j].type, ents[i].type)) c++;
                printf("  %-40s %d\n", ents[i].type, c);
            }
            continue;
        }
        for (int i = 0; i < n; i++) {
            const char* name = fmt_entity_string(&ents[i], "Name");
            const char* model = fmt_entity_asset(&ents[i], "Model");
            int hit = 0;
            for (int k = 3; k < argc && !hit; k++) hit = (name && strstr(name, argv[k])) || (model && strstr(model, argv[k]));
            if (hit) printf("  id %-6u %-34s name '%s' model '%s'\n", ents[i].id, ents[i].type, name ? name : "", model ? model : "");
        }
    }
    fmt_scene_free(sc); fmt_buf_free(&b); fmt_vfs_close(v);
    return 0;
}
