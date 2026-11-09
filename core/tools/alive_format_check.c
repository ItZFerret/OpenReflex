/* Validate converted MX vs ATV Alive level assets with the unmodified production readers (alive_validate.py).
 * Resources: tab-separated type and absolute path on stdin, one per line.
 * World: alive_format_check ROOT LEVEL ENTITIES STARTS PLACEMENTS (the expected Base-section counts). No game launch. */
#include "fmt_bxml.h"
#include "fmt_collis.h"
#include "fmt_model.h"
#include "fmt_scene.h"
#include "fmt_texture.h"
#include "fmt_terrain.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int check_world(const char* root, const char* name, int want_entities, int want_starts, int want_placements) {
    fmt_install* install = fmt_install_open(root);
    fmt_level* level = install ? fmt_level_open(install, name) : NULL;
    fmt_terrain* terrain = level ? fmt_terrain_load(level) : NULL;
    if (!terrain) { fprintf(stderr, "World terrain failed: %s\n", fmt_error()); return 1; }
    int count = 0, placements = 0, starts = 0;
    unsigned triangles = 0;
    const fmt_entity* entities = fmt_scene_entities(fmt_level_scene(level), 0, &count);
    if (!entities || count != want_entities || (terrain->size != 2048 && terrain->size != 1024)) {
        fprintf(stderr, "World entities %d (expected %d), terrain %d\n", count, want_entities, terrain->size); return 1;
    }
    float mn = INFINITY, mx = -INFINITY;
    for (int i = 0; i < terrain->size * terrain->size; i++) {
        float h = terrain->heights[i] * 2048.0f;
        if (!isfinite(h)) return 1;
        if (h < mn) mn = h;
        if (h > mx) mx = h;
    }
    uint8_t* softness = NULL; int width = 0, height = 0;
    if (fmt_terrain_load_softness(level, &terrain->params, &softness, &width, &height) < 0) {
        fprintf(stderr, "Softness failed: %s\n", fmt_error()); return 1;
    }
    free(softness);
    fmt_zones zones;
    if (fmt_zones_load(level, &terrain->params, &zones) < 0) {
        fprintf(stderr, "Terrain zones failed: %s\n", fmt_error()); return 1;
    }
    printf("Terrain physics maps: softness %dx%d; friction maps %dx%d, %dx%d, %dx%d, %dx%d\n",
           width, height, zones.width[0], zones.height[0], zones.width[1], zones.height[1],
           zones.width[2], zones.height[2], zones.width[3], zones.height[3]);
    fmt_zones_free(&zones);
    for (int i = 0; i < count; i++) {
        const fmt_entity* e = &entities[i];
        if (!strcmp(e->type, "StartEntityFreeRideRSData")) starts++;
        if (strcmp(e->type, "CollisionRSData")) continue;
        const char* asset = fmt_entity_asset(e, "FemaleCollisionAsset");
        fmt_buf b = {0};
        if (!asset || fmt_level_read_asset(level, asset, "collis", &b) < 0) {
            fprintf(stderr, "Collision %s: %s\n", asset ? asset : "(missing)", fmt_error()); return 1;
        }
        fmt_collis* c = fmt_collis_parse(b.data, b.size);
        if (!c) { fprintf(stderr, "Collision %s: %s\n", asset, fmt_error()); return 1; }
        triangles += fmt_collis_triangle_count(c); placements++;
        fmt_collis_free(c); fmt_buf_free(&b);
    }
    printf("World readers: %d entities, %d starts, %d collision placements (%u placed triangles), %dx%d height samples, height range %.3f..%.3f\n",
           count, starts, placements, triangles, terrain->size, terrain->size, mn, mx);
    int ok = starts == want_starts && placements == want_placements;
    fmt_terrain_free(terrain); fmt_level_free(level); fmt_install_close(install);
    return ok ? 0 : 1;
}

int main(int argc, char** argv) {
    if (argc == 6) return check_world(argv[1], argv[2], atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));
    if (argc != 1) return 2;
    char line[4096]; int checked = 0, failed = 0;
    while (fgets(line, sizeof line, stdin)) {
        char* path = strchr(line, '\t');
        if (!path) return 2;
        *path++ = 0; path[strcspn(path, "\r\n")] = 0;
        FILE* file = fopen(path, "rb");
        if (!file) { perror(path); return 2; }
        fseek(file, 0, SEEK_END); long length = ftell(file); rewind(file);
        if (length <= 0 || length > 128 * 1024 * 1024) return 2;
        uint8_t* data = malloc((size_t)length);
        if (!data || fread(data, 1, (size_t)length, file) != (size_t)length) return 2;
        fclose(file);
        int ok = 0;
        if (!strcmp(line, "model")) {
            fmt_model* m = fmt_model_parse(data, (size_t)length);
            ok = m != NULL; fmt_model_free(m);
        } else if (!strcmp(line, "surface")) {
            fmt_surfaces s;
            ok = fmt_surfaces_parse(data, (size_t)length, FMT_SURF_REPAIR_NONUNIT | FMT_SURF_FALLBACK_UP, &s) == 0;
            fmt_surfaces_free(&s);
        } else if (!strcmp(line, "collis")) {
            fmt_collis* c = fmt_collis_parse(data, (size_t)length);
            ok = c != NULL; fmt_collis_free(c);
        } else if (!strcmp(line, "texture")) {
            fmt_texture t;
            ok = fmt_texture_parse(data, (size_t)length, &t) == 0;
        } else if (!strcmp(line, "scene")) {
            fmt_scene* s = fmt_scene_parse(data, (size_t)length);
            ok = s != NULL;
            if (s) for (int i = 0; i < fmt_scene_subscene_count(s); ++i) {
                int count = 0;
                if (!fmt_scene_entities(s, i, &count) || count != (int)fmt_scene_subscene_entity_count(s, i)) ok = 0;
            }
            fmt_scene_free(s);
        } else {
            fmt_bxml* d = fmt_bxml_parse(data, (size_t)length);
            ok = d != NULL; fmt_bxml_free(d);
        }
        if (!ok) { fprintf(stderr, "FAIL %s: %s\n", path, fmt_error()); failed++; }
        checked++; free(data);
    }
    printf("Checked %d resources with current native readers; failed %d\n", checked, failed);
    return failed ? 1 : 0;
}
