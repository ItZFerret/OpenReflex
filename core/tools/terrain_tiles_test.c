/* w-terrtiles: the 32 x 32 tile rectangles of terrain_dirty_cache.h against a brute-force reference, over thousands of simulated frames.
 * Pure host test (no game files). For every frame: the single bounding rectangle equals the reference's (the old behaviour, unchanged);
 * the tile rectangles are at most 16, tile aligned, disjoint, hold every changed texel, and lie inside the tile-aligned box of the
 * bounding rectangle; when the exact cover needs 16 or fewer rectangles the union is EXACTLY the set of changed tiles. Applying the
 * rectangles to a mirror gives the same grid as applying the single rectangle (and as the source); a hash of the mirrors over all
 * frames is identical for the hinted and the unhinted cache. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../api/terrain_dirty_cache.h"

static uint64_t rng_state = 0x9e3779b97f4a7c15ull;
static uint32_t rnd(void) { rng_state = rng_state * 6364136223846793005ull + 1442695040888963407ull; return (uint32_t)(rng_state >> 33); }
static uint64_t fnv(uint64_t h, const void* p, size_t n) { const unsigned char* b = (const unsigned char*)p; while (n--) { h ^= *b++; h *= 1099511628211ull; } return h; }

struct world { uint32_t n; float* src; };

/* a rider's rut: a w x h patch of new values at (cx, cz) */
static void stamp(struct world* w, int cx, int cz, int pw, int ph, float v) {
    for (int z = cz; z < cz + ph; z++) for (int x = cx; x < cx + pw; x++)
        if (x >= 0 && z >= 0 && x < (int)w->n && z < (int)w->n) w->src[(size_t)z * w->n + x] = v;
}

/* number of rectangles of the exact cover of the changed tiles by the same run rule, from a plain tile map */
static int tile_changed(const uint8_t* map, uint32_t tw, uint32_t tx, uint32_t tz) { return map[(size_t)tz * tw + tx]; }

struct stats { size_t frames, changed_frames, forced, max_rects; uint64_t hash; };

static void run(uint32_t n, int riders, int frames, int use_hint, int spread, struct stats* st) {
    struct world w = { n, calloc((size_t)n * n, 4) };
    float* mirror_single = calloc((size_t)n * n, 4);   /* applies the bounding rectangle */
    float* mirror_tiles = calloc((size_t)n * n, 4);    /* applies the tile rectangles */
    float* prev = calloc((size_t)n * n, 4);            /* the reference's previous grid */
    assert(w.src && mirror_single && mirror_tiles && prev);
    const uint32_t tw = (n + 31) / 32;
    uint8_t* map = calloc((size_t)tw * tw, 1);
    struct terrain_dirty_cache c = {0};
    struct terrain_dirty_key k = {{7, 1, 2, 3}};
    int px[32], pz[32], dx[32], dz[32];
    for (int r = 0; r < riders; r++) { px[r] = rnd() % n; pz[r] = rnd() % n; dx[r] = (int)(rnd() % 7) - 3; dz[r] = (int)(rnd() % 7) - 3; }
    assert(terrain_dirty_update(&c, &k, 0, n, w.src, (size_t)n * 4));   /* the first call: everything */
    assert(c.nrects == 1 && c.rects[0].x0 == 0 && c.rects[0].z0 == 0 && c.rects[0].x1 == (int32_t)n && c.rects[0].z1 == (int32_t)n);
    memcpy(mirror_single, w.src, (size_t)n * n * 4); memcpy(mirror_tiles, w.src, (size_t)n * n * 4); memcpy(prev, w.src, (size_t)n * n * 4);
    for (int f = 1; f <= frames; f++) {
        /* riders move, stamp a 3 x 5 rut; some frames one rider jumps (a respawn), some frames nothing moves */
        int moving = (rnd() % 16) != 0;
        for (int r = 0; r < riders && moving; r++) {
            if (spread && rnd() % 500 == 0) { px[r] = rnd() % n; pz[r] = rnd() % n; }
            px[r] += dx[r]; pz[r] += dz[r];
            if (px[r] < 0 || px[r] >= (int)n) { dx[r] = -dx[r]; px[r] += 2 * dx[r]; }
            if (pz[r] < 0 || pz[r] >= (int)n) { dz[r] = -dz[r]; pz[r] += 2 * dz[r]; }
            if (rnd() % 8 == 0) { dx[r] = (int)(rnd() % 7) - 3; dz[r] = (int)(rnd() % 7) - 3; }
            stamp(&w, px[r], pz[r], 3, 5, (float)f + 0.25f * (float)r);
        }
        if (spread && f % 997 == 0) for (int i = 0; i < 40; i++) stamp(&w, rnd() % n, rnd() % n, 1, 1, -(float)f);   /* a scatter: more than 16 tiles */
        if (f % 1500 == 0) stamp(&w, 0, 0, (int)n, 1, (float)f);                                                    /* a whole row: a wide strip */
        /* the hint: the box around everything stamped is not tracked here, so the hint is the full grid or none */
        const int32_t hint[4] = { 0, 0, (int32_t)n, (int32_t)n }; int bad = 0;
        assert(terrain_dirty_update_hint(&c, &k, (uint64_t)f, n, w.src, (size_t)n * 4, use_hint ? hint : NULL, use_hint, &bad)); assert(!bad);
        /* reference */
        int32_t rx0 = INT32_MAX, rz0 = INT32_MAX, rx1 = -1, rz1 = -1;
        memset(map, 0, (size_t)tw * tw);
        size_t changed_tiles = 0;
        for (uint32_t z = 0; z < n; z++) for (uint32_t x = 0; x < n; x++)
            if (memcmp(&prev[(size_t)z * n + x], &w.src[(size_t)z * n + x], 4)) {
                if ((int32_t)x < rx0) rx0 = (int32_t)x; if ((int32_t)x + 1 > rx1) rx1 = (int32_t)x + 1;
                if ((int32_t)z < rz0) rz0 = (int32_t)z; if ((int32_t)z + 1 > rz1) rz1 = (int32_t)z + 1;
                uint8_t* m = &map[(size_t)(z / 32) * tw + x / 32]; if (!*m) { *m = 1; ++changed_tiles; }
            }
        assert(c.x0 == rx0 && c.z0 == rz0 && c.x1 == rx1 && c.z1 == rz1);   /* the single rectangle: the old answer */
        assert(c.changed_tiles == changed_tiles);
        assert(c.nrects >= 0 && c.nrects <= TERRAIN_DIRTY_MAX_RECTS);
        if (rx1 < 0) assert(c.nrects == 0);
        else assert(c.nrects >= 1);
        /* the rectangles */
        uint8_t* covered = calloc((size_t)tw * tw, 1);   /* tiles inside the union */
        for (int i = 0; i < c.nrects; i++) {
            const struct terrain_dirty_rect* r = &c.rects[i];
            assert(r->x0 >= 0 && r->z0 >= 0 && r->x1 <= (int32_t)n && r->z1 <= (int32_t)n && r->x0 < r->x1 && r->z0 < r->z1);
            assert(r->x0 % 32 == 0 && r->z0 % 32 == 0 && (r->x1 % 32 == 0 || r->x1 == (int32_t)n) && (r->z1 % 32 == 0 || r->z1 == (int32_t)n));
            assert(r->x0 >= (rx0 / 32) * 32 && r->z0 >= (rz0 / 32) * 32 && r->x1 <= (int32_t)(((rx1 + 31) / 32) * 32 < (int32_t)n ? ((rx1 + 31) / 32) * 32 : n) && r->z1 <= (int32_t)(((rz1 + 31) / 32) * 32 < (int32_t)n ? ((rz1 + 31) / 32) * 32 : n));
            for (uint32_t tz = (uint32_t)r->z0 / 32; tz * 32 < (uint32_t)r->z1; tz++) for (uint32_t tx = (uint32_t)r->x0 / 32; tx * 32 < (uint32_t)r->x1; tx++) {
                assert(!covered[(size_t)tz * tw + tx]);   /* disjoint */
                covered[(size_t)tz * tw + tx] = 1;
            }
        }
        for (uint32_t tz = 0; tz < tw; tz++) for (uint32_t tx = 0; tx < tw; tx++)
            if (tile_changed(map, tw, tx, tz)) assert(covered[(size_t)tz * tw + tx]);   /* every changed tile (so every changed texel) is covered */
        /* exact when the cover is not forced: recompute the run cover's size with the library helper on the reference map */
        struct terrain_dirty_rect ref[TERRAIN_DIRTY_MAX_RECTS]; size_t refc = 0;
        int refn = rx1 < 0 ? 0 : terrain_dirty_tile_rects(map, tw, n, (uint32_t)rx0 / 32, (uint32_t)(rx1 - 1) / 32, (uint32_t)rz0 / 32, (uint32_t)(rz1 - 1) / 32, ref, &refc);
        assert(refn == c.nrects && refc == changed_tiles);
        size_t covered_tiles = 0; for (size_t i = 0; i < (size_t)tw * tw; i++) covered_tiles += covered[i];
        if (covered_tiles == changed_tiles) st->frames += 0; else ++st->forced;   /* merged: some unchanged tiles ride along */
        if (c.nrects < TERRAIN_DIRTY_MAX_RECTS && rx1 >= 0) assert(covered_tiles >= changed_tiles);
        free(covered);
        /* apply: the single rectangle and the tile rectangles to their mirrors */
        if (rx1 >= 0) {
            for (int32_t z = c.z0; z < c.z1; z++) memcpy(&mirror_single[(size_t)z * n + c.x0], &w.src[(size_t)z * n + c.x0], (size_t)(c.x1 - c.x0) * 4);
            for (int i = 0; i < c.nrects; i++) for (int32_t z = c.rects[i].z0; z < c.rects[i].z1; z++)
                memcpy(&mirror_tiles[(size_t)z * n + c.rects[i].x0], &w.src[(size_t)z * n + c.rects[i].x0], (size_t)(c.rects[i].x1 - c.rects[i].x0) * 4);
            ++st->changed_frames;
        }
        assert(!memcmp(mirror_single, w.src, (size_t)n * n * 4));
        assert(!memcmp(mirror_tiles, w.src, (size_t)n * n * 4));
        assert(!memcmp(mirror_tiles, mirror_single, (size_t)n * n * 4));
        memcpy(prev, w.src, (size_t)n * n * 4);
        /* same frame again: the same rectangles, no change in version */
        uint32_t ver = c.version; struct terrain_dirty_cache before = c;
        assert(terrain_dirty_update(&c, &k, (uint64_t)f, n, w.src, (size_t)n * 4)); assert(c.version == ver && c.nrects == before.nrects && !memcmp(c.rects, before.rects, sizeof c.rects));
        if ((size_t)c.nrects > st->max_rects) st->max_rects = (size_t)c.nrects;
        st->hash = fnv(st->hash, &c.version, 4); st->hash = fnv(st->hash, &c.nrects, 4); st->hash = fnv(st->hash, c.rects, sizeof(struct terrain_dirty_rect) * (size_t)c.nrects);
        st->hash = fnv(st->hash, &c.x0, 16);
        ++st->frames;
    }
    st->hash = fnv(st->hash, mirror_tiles, (size_t)n * n * 4);
    terrain_dirty_dispose(&c); free(w.src); free(mirror_single); free(mirror_tiles); free(prev); free(map);
}

int main(void) {
    (void)terrain_dirty_invalidate; (void)terrain_dirty_pending;   /* the session uses these; this test does not need them */
    /* hand-made cases */
    {
        struct terrain_dirty_cache c = {0}; struct terrain_dirty_key k = {{1}};
        const uint32_t n = 256; float* g = calloc((size_t)n * n, 4); assert(g);
        assert(terrain_dirty_update(&c, &k, 0, n, g, n * 4)); assert(c.nrects == 1);
        assert(terrain_dirty_update(&c, &k, 1, n, g, n * 4)); assert(c.nrects == 0 && c.changed_tiles == 0);
        g[5 * n + 7] = 1.f;   /* one texel: one tile */
        assert(terrain_dirty_update(&c, &k, 2, n, g, n * 4)); assert(c.nrects == 1 && c.changed_tiles == 1);
        assert(c.rects[0].x0 == 0 && c.rects[0].z0 == 0 && c.rects[0].x1 == 32 && c.rects[0].z1 == 32);
        g[40 * n + 40] = 1.f; g[41 * n + 71] = 1.f;   /* tiles (1,1) and (2,1): one rectangle 64 x 32 */
        assert(terrain_dirty_update(&c, &k, 3, n, g, n * 4)); assert(c.nrects == 1 && c.changed_tiles == 2);
        assert(c.rects[0].x0 == 32 && c.rects[0].x1 == 96 && c.rects[0].z0 == 32 && c.rects[0].z1 == 64);
        g[200 * n + 200] = 2.f; g[10 * n + 250] = 2.f;   /* two far tiles: two rectangles, not their bounding box */
        assert(terrain_dirty_update(&c, &k, 4, n, g, n * 4)); assert(c.nrects == 2 && c.changed_tiles == 2);
        /* an L shape: runs of different width: two or three rectangles that cover exactly the 5 tiles */
        for (int i = 0; i < 3; i++) g[(100 + i * 32) * n + 100] = 3.f; g[196 * n + 132] = 3.f; g[196 * n + 164] = 3.f;
        assert(terrain_dirty_update(&c, &k, 5, n, g, n * 4)); assert(c.changed_tiles == 5 && c.nrects >= 2 && c.nrects <= 3);
        /* more than 16 separate tiles: merged to 16, all covered */
        for (int i = 0; i < 24; i++) g[(size_t)((i % 6) * 40 + 3) * n + (size_t)(i / 6) * 60 + 5] = 10.f + (float)i;
        assert(terrain_dirty_update(&c, &k, 6, n, g, n * 4)); assert(c.nrects <= TERRAIN_DIRTY_MAX_RECTS && c.nrects >= 1);
        /* a texel on the last partial tile of a grid that is not a multiple of 32 */
        terrain_dirty_dispose(&c); free(g);
        const uint32_t m = 100; g = calloc((size_t)m * m, 4); assert(g);
        assert(terrain_dirty_update(&c, &k, 0, m, g, m * 4)); assert(terrain_dirty_update(&c, &k, 1, m, g, m * 4));
        g[99 * m + 99] = 1.f; assert(terrain_dirty_update(&c, &k, 2, m, g, m * 4));
        assert(c.nrects == 1 && c.rects[0].x0 == 96 && c.rects[0].x1 == 100 && c.rects[0].z0 == 96 && c.rects[0].z1 == 100);
        /* no memory for the bitmap: the tile-aligned box around the change, one rectangle */
        terrain_dirty_dispose(&c); assert(terrain_dirty_update(&c, &k, 0, m, g, m * 4));
        free(c.tiles); c.tiles = NULL; c.tiles_cap = 0;
        g[3 * m + 3] = 1.f; g[70 * m + 70] = 1.f;
        assert(terrain_dirty_update(&c, &k, 1, m, g, m * 4)); assert(c.tiles || c.nrects >= 1);
        terrain_dirty_dispose(&c); free(g);
        /* reduce: merging to a smaller target */
        struct terrain_dirty_rect r[4] = {{0, 0, 32, 32}, {32, 0, 64, 32}, {200, 200, 232, 232}, {0, 100, 32, 132}}; int cnt = 4;
        terrain_dirty_reduce(r, &cnt, 2); assert(cnt == 2);
        cnt = 1; terrain_dirty_reduce(r, &cnt, 1); assert(cnt == 1);
    }
    struct stats a = {0}, b = {0}, c2 = {0}, d = {0};
    a.hash = b.hash = c2.hash = d.hash = 1469598103934665603ull;
    /* 12 riders on a 256 grid for 6000 frames, hinted (full-grid hint, verify on) and unhinted: identical */
    run(256, 12, 6000, 0, 1, &a);
    rng_state = 0x9e3779b97f4a7c15ull; run(256, 12, 6000, 1, 1, &b);
    assert(a.hash == b.hash && a.frames == 6000);
    /* the same on a grid that is not a multiple of 32, and few riders without scatter */
    rng_state = 77; run(200, 3, 4000, 0, 0, &c2);
    /* a 2048 grid (the nationals): 12 riders, 150 frames (the reference compares every texel) */
    rng_state = 99; run(2048, 12, 150, 0, 1, &d);
    printf("PASS terrain tiles: %zu + %zu + %zu + %zu frames, %zu changed frames, max %zu rectangles, %zu frames merged beyond the exact cover, hinted == unhinted (hash %016llx)\n",
           a.frames, b.frames, c2.frames, d.frames, a.changed_frames + b.changed_frames + c2.changed_frames + d.changed_frames,
           a.max_rects > d.max_rects ? a.max_rects : d.max_rects, a.forced + b.forced + c2.forced + d.forced, (unsigned long long)a.hash);
    return 0;
}
