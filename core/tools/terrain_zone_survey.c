/* terrain_zone_survey.c (w-surfaces) -- every level of an install: the friction-zone maps of its HFTerrain (the FrictionZones packdata,
 * fmt_zones: layer 0 = the grip material TerrainMaterialAt 007c5120 returns to 00461680, layer 1 = the drag factor of 00461760), as
 * value histograms, and the heightfield / SoftnessMask sizes (the CPU deformation pass's equal-size fast path vs its sampler).
 * The grip of material k at a wheel (00461680): 0 -> 0.1, 1 -> 0.3, 3 -> [0x9d78a0] 0.65, 4 -> [0x9cc8a8] 0.8, 5 -> [0x9cd970] 0.9,
 * else 0.5, plus the vehicle's +0x224 (the tyre tuning); on water 0.5 flat. Layer 1: 0 -> 0, 2 -> 0.75, 3 -> 1, else 0.25.
 * Read-only. Build: clang -std=gnu11 -O2 -Icore/include -Icore/src -o terrain_zone_survey core/tools/terrain_zone_survey.c core/src/fmt_*.c -lz
 * usage: terrain_zone_survey <game files/Archive> */
#include "fmt_db.h"
#include "fmt_scene.h"
#include "fmt_terrain.h"
#include "fmt_texture.h"
#include "fmt_vfs.h"
#include "fmt_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
static int level_cb(const char* name, uint64_t size, void* user) {
    (void)size; size_t l = strlen(name); static const char sfx[] = ".dx9.level";
    if (l > sizeof sfx - 1 && !strcasecmp(name + l - (sizeof sfx - 1), sfx)) {
        char*** list = (char***)user; int n = 0; while ((*list)[n]) n++;
        *list = realloc(*list, (size_t)(n + 2) * sizeof(char*)); (*list)[n] = strndup(name, l - (sizeof sfx - 1)); (*list)[n + 1] = NULL;
    }
    return 0;
}
int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <game files/Archive>\n", argv[0]); return 2; }
    fmt_install* in = fmt_install_open(argv[1]);
    if (!in) { fprintf(stderr, "open: %s\n", fmt_error()); return 1; }
    char** names = calloc(1, sizeof(char*));
    fmt_vfs_list(fmt_install_vfs(in), "Database", level_cb, &names);
    for (int i = 0; names[i]; i++) {
        if (!strncmp(names[i], "Ctrl_", 5)) continue;
        fmt_level* L = fmt_level_open(in, names[i]);
        if (!L) continue;
        fmt_terrain_params p;
        if (fmt_terrain_params_read(L, -1, &p) != 0) { printf("%s: no terrain\n", names[i]); fmt_level_free(L); continue; }
        fmt_buf raw; fmt_texture tex; int hw = -1, sw = -1, sh = -1;
        if (p.height_map && fmt_level_read_asset(L, p.height_map, "texture", &raw) == 0) { if (fmt_texture_parse(raw.data, raw.size, &tex) == 0) hw = (int)tex.width; fmt_buf_free(&raw); }
        if (p.softness_mask && fmt_level_read_asset(L, p.softness_mask, "texture", &raw) == 0) { if (fmt_texture_parse(raw.data, raw.size, &tex) == 0) { sw = (int)tex.width; sh = (int)tex.height; } fmt_buf_free(&raw); }
        printf("%s heightmap %d spacing %d softness %dx%d%s", names[i], hw, p.spacing, sw, sh, hw > 0 && sw > 0 && hw != sw ? " UNEQUAL" : "");
        fmt_zones z;
        if (!p.friction_zones || fmt_zones_load(L, &p, &z) != 0) { printf(" | zones: %s\n", p.friction_zones ? fmt_error() : "none"); fmt_level_free(L); continue; }
        printf(" | zones %s interleaved %d", p.friction_zones, z.interleaved);
        for (int m = 0; m < 4; m++) {
            long h[256] = {0}, tot = 0;
            if (!z.interleaved && z.bits[m] > 0)
                for (int y = 0; y < z.height[m]; y++) for (int x = 0; x < z.width[m]; x++) { const int v = fmt_zones_sample(&z, m, x, y); if (v >= 0) { h[v]++; tot++; } }
            printf(" | L%d %dx%d b%d:", m, z.width[m], z.height[m], z.bits[m]);
            for (int v = 0; v < 256; v++) if (h[v]) printf(" %d=%.1f%%", v, 100.0 * (double)h[v] / (double)tot);
        }
        printf("\n");
        fmt_zones_free(&z); fmt_level_free(L);
    }
    return 0;
}
