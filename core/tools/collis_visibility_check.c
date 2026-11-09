/* collis_visibility_check.c (w-frnowalls) -- invisible collision: the static collision triangles of a level's Free Ride scene (the Base
 * subscene and "<level>FreeRide": every CollisionRSData placement, collis kind 22) that have NO drawn geometry near them. A rider is
 * stopped by a CollisionShapeTriangles wherever its triangles are, so collision with nothing drawn on it is an invisible wall.
 *   collis_visibility_check <install root> <level> [max distance ft (default 3)] [-v]
 * Drawn geometry = the surfaces of every scene entity of those subscenes with a ModelAsset (models placed with the scene Matrix, each
 * binding's surface through its node's world matrix), except models whose every binding / MaterialList entry is GL_Default or
 * Editor_Default (the collision proxies' placeholder material), plus the base heightfield (a sample within the distance of the terrain's
 * height counts as drawn). Each collision triangle is sampled at its vertices, edge midpoints, centroid and the four points halfway to
 * the centroid; a sample is covered when a drawn triangle (or the terrain) is within the distance. An uncovered sample counts only when it is
 * EXPOSED: higher (by the distance) than every drawn vertex within 8 ft around it and than the terrain (collision buried inside or under a
 * drawn rock / wall is never touched). One line per placement with exposed area: name, collision asset, exposed share, the exposed area
 * (ft^2), the bounding box of the exposed samples.
 * Placement (w-dfcollide): a chunk's triangles are chunk-local; the original rotates them by the placement's entity matrix and adds the
 * chunk's position (00819650 gives each chunk the entity matrix with the transformed centre as its translation; 00769550 / 0084fc20 keep
 * that position and the matrix's quaternion; 00844d30 and the triangle tests rotate the local vertices by it). Before w-dfcollide this tool
 * added the position only, which put every rotated placement in the wrong place: FMX_DoverB's JumboTron screen (COL_DO_MegaRamp_Screen,
 * turned 90 degrees) came out as a panel across the arena with nothing drawn on it.
 * Read-only. Build: clang -std=gnu11 -O2 -Icore/include -Icore/src -o collis_visibility_check core/tools/collis_visibility_check.c core/src/fmt_*.c -lz */
#include "fmt_db.h"
#include "fmt_scene.h"
#include "fmt_collis.h"
#include "fmt_model.h"
#include "fmt_terrain.h"
#include "fmt_common.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct { float a[3], b[3], c[3]; } tri;
static tri* T; static int nT, capT;
#define CELL 8.0f
#define HB (1 << 20)
typedef struct { int t; int next; } hnode;
static int* head; static hnode* hn; static int nhn, caphn;
static unsigned hcell(int x, int y, int z) { return ((unsigned)x * 73856093u ^ (unsigned)y * 19349663u ^ (unsigned)z * 83492791u) & (HB - 1); }
static void hins(int x, int y, int z, int t) {
    if (nhn == caphn) { caphn = caphn ? caphn * 2 : 1 << 16; hn = realloc(hn, (size_t)caphn * sizeof *hn); }
    unsigned h = hcell(x, y, z); hn[nhn].t = t; hn[nhn].next = head[h]; head[h] = nhn++;
}
static void add_tri(const float* a, const float* b, const float* c) {
    if (nT == capT) { capT = capT ? capT * 2 : 1 << 16; T = realloc(T, (size_t)capT * sizeof *T); }
    memcpy(T[nT].a, a, 12); memcpy(T[nT].b, b, 12); memcpy(T[nT].c, c, 12);
    float mn[3], mx[3];
    for (int k = 0; k < 3; k++) { mn[k] = fminf(a[k], fminf(b[k], c[k])); mx[k] = fmaxf(a[k], fmaxf(b[k], c[k])); }
    int x0 = (int)floorf(mn[0] / CELL), x1 = (int)floorf(mx[0] / CELL), y0 = (int)floorf(mn[1] / CELL), y1 = (int)floorf(mx[1] / CELL), z0 = (int)floorf(mn[2] / CELL), z1 = (int)floorf(mx[2] / CELL);
    if ((long)(x1 - x0 + 1) * (y1 - y0 + 1) * (z1 - z0 + 1) > 20000) { nT++; return; }   /* a huge triangle (sky dome): skipped */
    for (int x = x0; x <= x1; x++) for (int y = y0; y <= y1; y++) for (int z = z0; z <= z1; z++) hins(x, y, z, nT);
    nT++;
}
/* squared distance point - triangle (Ericson, Real-Time Collision Detection 5.1.5) */
static float d2_tri(const float* p, const tri* t) {
    float ab[3], ac[3], ap[3], r[3];
    for (int k = 0; k < 3; k++) { ab[k] = t->b[k] - t->a[k]; ac[k] = t->c[k] - t->a[k]; ap[k] = p[k] - t->a[k]; }
#define DOT(u, v) ((u)[0] * (v)[0] + (u)[1] * (v)[1] + (u)[2] * (v)[2])
    float d1 = DOT(ab, ap), d2 = DOT(ac, ap);
    if (d1 <= 0 && d2 <= 0) { memcpy(r, t->a, 12); goto out; }
    float bp[3]; for (int k = 0; k < 3; k++) bp[k] = p[k] - t->b[k];
    float d3 = DOT(ab, bp), d4 = DOT(ac, bp);
    if (d3 >= 0 && d4 <= d3) { memcpy(r, t->b, 12); goto out; }
    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0 && d1 >= 0 && d3 <= 0) { float v = d1 / (d1 - d3); for (int k = 0; k < 3; k++) r[k] = t->a[k] + v * ab[k]; goto out; }
    float cp[3]; for (int k = 0; k < 3; k++) cp[k] = p[k] - t->c[k];
    float d5 = DOT(ab, cp), d6 = DOT(ac, cp);
    if (d6 >= 0 && d5 <= d6) { memcpy(r, t->c, 12); goto out; }
    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0 && d2 >= 0 && d6 <= 0) { float w = d2 / (d2 - d6); for (int k = 0; k < 3; k++) r[k] = t->a[k] + w * ac[k]; goto out; }
    float va = d3 * d6 - d5 * d4;
    if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0) { float w = (d4 - d3) / ((d4 - d3) + (d5 - d6)); for (int k = 0; k < 3; k++) r[k] = t->b[k] + w * (t->c[k] - t->b[k]); goto out; }
    { float den = 1.0f / (va + vb + vc), v = vb * den, w = vc * den; for (int k = 0; k < 3; k++) r[k] = t->a[k] + ab[k] * v + ac[k] * w; }
out:
    { float d[3] = {p[0] - r[0], p[1] - r[1], p[2] - r[2]}; return DOT(d, d); }
}
static int* stamp; static int stamp_id;
static int covered(const float* p, float dist) {
    stamp_id++;
    const int r = (int)ceilf(dist / CELL);
    const int cx = (int)floorf(p[0] / CELL), cy = (int)floorf(p[1] / CELL), cz = (int)floorf(p[2] / CELL);
    for (int x = cx - r; x <= cx + r; x++) for (int y = cy - r; y <= cy + r; y++) for (int z = cz - r; z <= cz + r; z++)
        for (int i = head[hcell(x, y, z)]; i >= 0; i = hn[i].next) {
            const int t = hn[i].t;
            if (stamp[t] == stamp_id) continue;
            stamp[t] = stamp_id;
            if (d2_tri(p, &T[t]) <= dist * dist) return 1;
        }
    return 0;
}
/* the highest drawn point within `r` ft (xz) of p, from the triangles' vertices and centroids in the cells around it; -FLT_MAX if none */
static float drawn_top(const float* p, float r) {
    stamp_id++;
    float top = -FLT_MAX;
    const int rr = (int)ceilf(r / CELL);
    const int cx = (int)floorf(p[0] / CELL), cz = (int)floorf(p[2] / CELL), cy = (int)floorf(p[1] / CELL);
    for (int x = cx - rr; x <= cx + rr; x++) for (int z = cz - rr; z <= cz + rr; z++) for (int y = cy - 40; y <= cy + 40; y++)
        for (int i = head[hcell(x, y, z)]; i >= 0; i = hn[i].next) {
            const int t = hn[i].t;
            if (stamp[t] == stamp_id) continue;
            stamp[t] = stamp_id;
            const float* v[3] = {T[t].a, T[t].b, T[t].c};
            for (int k = 0; k < 3; k++) if (fabsf(v[k][0] - p[0]) <= r && fabsf(v[k][2] - p[2]) <= r && v[k][1] > top) top = v[k][1];
        }
    return top;
}
static void xf(const float* p, const float* m, float* o) {   /* row vector * row-major D3D matrix */
    for (int k = 0; k < 3; k++) o[k] = p[0] * m[k] + p[1] * m[4 + k] + p[2] * m[8 + k] + m[12 + k];
}
static void mul(const float* a, const float* b, float* o) {
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) { float s = 0; for (int k = 0; k < 4; k++) s += a[4 * i + k] * b[4 * k + j]; o[4 * i + j] = s; }
}
/* a collision vertex in the world: the chunk-local vertex l rotated by the placement's entity matrix, plus the chunk's position */
static void col_vertex(const fmt_collis_placement* pl, const float* pos, const float* l, float* w) {
    const float* m = pl->matrix;
    for (int k = 0; k < 3; k++) w[k] = l[0] * m[k] + l[1] * m[4 + k] + l[2] * m[8 + k] + pos[k];
}
/* the heightfield's bilinear height at (x, z); 0 outside the grid */
static int terrain_height(const fmt_terrain* t, float x, float z, float* h) {
    const float x0 = fmt_terrain_x(t, 0), dx = fmt_terrain_x(t, 1) - x0, z0 = fmt_terrain_z(t, 0), dz = fmt_terrain_z(t, 1) - z0;
    const float u = (x - x0) / dx, v = (z - z0) / dz;
    const int c = (int)floorf(u), r = (int)floorf(v);
    if (c < 0 || r < 0 || c + 1 >= t->size || r + 1 >= t->size) return 0;
    const float fu = u - (float)c, fv = v - (float)r;
    const float a = fmt_terrain_y(t, c, r), b = fmt_terrain_y(t, c + 1, r), cc = fmt_terrain_y(t, c, r + 1), d = fmt_terrain_y(t, c + 1, r + 1);
    *h = (a * (1 - fu) + b * fu) * (1 - fv) + (cc * (1 - fu) + d * fu) * fv;
    return 1;
}
static uint32_t proxy_id[4096]; static int nproxy;   /* entities whose own model draws only the placeholder material */
static int is_proxy(uint32_t id) { for (int i = 0; i < nproxy; i++) if (proxy_id[i] == id) return 1; return 0; }
static int placeholder(const char* m) { return m && (!strcasecmp(m, "GL_Default") || !strcasecmp(m, "Editor_Default")); }

/* the entity's MaterialList: 1 when every entry is a placeholder (and there is one) */
static int list_placeholder(const fmt_entity* e) {
    int f = fmt_entity_field(e, "MaterialList");
    if (f < 0) return 0;
    int n = fmt_bxml_child_count(e->doc, f), any = 0;
    for (int k = 0; k < n; k++) {
        int c = fmt_bxml_child(e->doc, f, k);
        int a = fmt_bxml_find_child(e->doc, c, "Asset");
        const char* nm = a >= 0 ? fmt_bxml_attr_str(e->doc, a, "name") : NULL;
        if (!nm) continue;
        any = 1;
        if (!placeholder(nm)) return 0;
    }
    return any;
}

/* CVC_VIEW=<entity id>:<prefix>: three orthographic views (<prefix>_top.ppm x-z, _front.ppm x-y, _side.ppm z-y) of the drawn geometry
 * around that placement (grey, lit, z-buffered) with its collision triangles drawn over it as red wireframe: to look at a candidate */
#define VW 640
static void view(const char* path, int ax, int ay, int depth_axis, int flip_depth, const float* lo, const float* hi, const fmt_collis_placement* pl) {
    static unsigned char img[VW * VW * 3]; static float zb[VW * VW];
    for (int i = 0; i < VW * VW; i++) { zb[i] = -FLT_MAX; img[3 * i] = 30; img[3 * i + 1] = 40; img[3 * i + 2] = 60; }
    const float sx = (float)(VW - 1) / (hi[ax] - lo[ax]), sy = (float)(VW - 1) / (hi[ay] - lo[ay]), sc = sx < sy ? sx : sy;
#define PX(v) ((v)[ax] - lo[ax]) * sc
#define PY(v) (float)(VW - 1) - ((v)[ay] - lo[ay]) * sc
    for (int t = 0; t < nT; t++) {
        const float* v[3] = {T[t].a, T[t].b, T[t].c};
        int inside = 1; for (int k = 0; k < 3 && inside; k++) for (int a = 0; a < 3; a++) if (v[k][a] < lo[a] - 50 || v[k][a] > hi[a] + 50) inside = 0;
        if (!inside) continue;
        float e1[3], e2[3], n[3]; for (int a = 0; a < 3; a++) { e1[a] = v[1][a] - v[0][a]; e2[a] = v[2][a] - v[0][a]; }
        n[0] = e1[1] * e2[2] - e1[2] * e2[1]; n[1] = e1[2] * e2[0] - e1[0] * e2[2]; n[2] = e1[0] * e2[1] - e1[1] * e2[0];
        float l = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]); if (l < 1e-6f) continue;
        const float shade = 0.35f + 0.65f * fabsf(0.3f * n[0] + 0.8f * n[1] + 0.5f * n[2]) / l;
        float X[3], Y[3], D[3]; for (int k = 0; k < 3; k++) { X[k] = PX(v[k]); Y[k] = PY(v[k]); D[k] = flip_depth ? -v[k][depth_axis] : v[k][depth_axis]; }
        int x0 = (int)fmaxf(0, floorf(fminf(X[0], fminf(X[1], X[2])))), x1 = (int)fminf(VW - 1, ceilf(fmaxf(X[0], fmaxf(X[1], X[2]))));
        int y0 = (int)fmaxf(0, floorf(fminf(Y[0], fminf(Y[1], Y[2])))), y1 = (int)fminf(VW - 1, ceilf(fmaxf(Y[0], fmaxf(Y[1], Y[2]))));
        const float den = (Y[1] - Y[2]) * (X[0] - X[2]) + (X[2] - X[1]) * (Y[0] - Y[2]);
        if (fabsf(den) < 1e-9f) continue;
        for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
            const float w0 = ((Y[1] - Y[2]) * (x - X[2]) + (X[2] - X[1]) * (y - Y[2])) / den, w1 = ((Y[2] - Y[0]) * (x - X[2]) + (X[0] - X[2]) * (y - Y[2])) / den, w2 = 1 - w0 - w1;
            if (w0 < 0 || w1 < 0 || w2 < 0) continue;
            const float d = w0 * D[0] + w1 * D[1] + w2 * D[2];
            if (d <= zb[y * VW + x]) continue;
            zb[y * VW + x] = d;
            const unsigned char c = (unsigned char)(255 * shade);
            img[3 * (y * VW + x)] = c; img[3 * (y * VW + x) + 1] = c; img[3 * (y * VW + x) + 2] = c;
        }
    }
    for (uint32_t c = 0; c < pl->collis->count; c++) {
        const fmt_collis_chunk* ch = &pl->collis->chunks[c]; float pos[3]; fmt_collis_chunk_position(pl->collis, (int)c, pl->matrix, pos);
        for (uint32_t t = 0; t < ch->count; t++) for (int k = 0; k < 3; k++) {
            float a[3], b[3]; col_vertex(pl, pos, &ch->tris[9 * t + 3 * k], a); col_vertex(pl, pos, &ch->tris[9 * t + 3 * ((k + 1) % 3)], b);
            for (int s = 0; s <= 200; s++) {
                float m[3]; for (int q = 0; q < 3; q++) m[q] = a[q] + (b[q] - a[q]) * (float)s / 200.f;
                const int x = (int)(PX(m)), y = (int)(PY(m));
                if (x >= 0 && y >= 0 && x < VW && y < VW) { img[3 * (y * VW + x)] = 255; img[3 * (y * VW + x) + 1] = 40; img[3 * (y * VW + x) + 2] = 40; }
            }
        }
    }
    FILE* f = fopen(path, "wb"); if (!f) return;
    fprintf(f, "P6 %d %d 255\n", VW, VW); fwrite(img, 1, sizeof img, f); fclose(f);
#undef PX
#undef PY
}

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: collis_visibility_check <install> <level> [dist] [-v]\n"); return 2; }
    const float dist = argc > 3 && argv[3][0] != '-' ? strtof(argv[3], NULL) : 3.0f;
    int verbose = 0; for (int i = 3; i < argc; i++) if (!strcmp(argv[i], "-v")) verbose = 1;
    fmt_install* in = fmt_install_open(argv[1]);
    fmt_level* lv = in ? fmt_level_open(in, argv[2]) : NULL;
    if (!lv) { fprintf(stderr, "open %s: %s\n", argv[2], fmt_error()); return 1; }
    const char* base = fmt_level_base_subscene(lv), *fr = fmt_level_activity_subscene(lv, "FreeRide");
    const char* subs[2] = {base, fr};
    const int ns = fr ? 2 : 1;
    head = malloc(HB * sizeof(int)); memset(head, 0xff, HB * sizeof(int));
    fmt_scene* sc = fmt_level_scene(lv);
    int models = 0, skipped_ph = 0, missing = 0;
    for (int si = 0; si < ns; si++) {
        int n = 0; const fmt_entity* e = fmt_scene_entities(sc, fmt_scene_find_subscene(sc, subs[si]), &n);
        for (int i = 0; i < n; i++) {
            const char* mod = fmt_entity_asset(&e[i], "ModelAsset");
            if (!mod) continue;
            if (placeholder(mod)) { if (nproxy < 4096) proxy_id[nproxy++] = e[i].id; continue; }
            const char* sur = fmt_entity_asset(&e[i], "SurfaceAsset");
            float M[16]; if (fmt_entity_matrix(&e[i], M) != 0) continue;
            fmt_buf mb = {0}, sb = {0};
            if (fmt_level_read_asset(lv, mod, "model", &mb) != 0) { missing++; continue; }
            fmt_model* m = fmt_model_parse(mb.data, mb.size);
            fmt_buf_free(&mb);
            if (!m) { missing++; continue; }
            int all_ph = m->nbindings > 0;
            for (int b = 0; b < m->nbindings; b++) if (!placeholder(m->bindings[b].material)) all_ph = 0;
            if (all_ph || list_placeholder(&e[i])) { skipped_ph++; if (nproxy < 4096) proxy_id[nproxy++] = e[i].id; fmt_model_free(m); continue; }
            fmt_surfaces ss; memset(&ss, 0, sizeof ss);
            if (!sur || fmt_level_read_asset(lv, sur, "surface", &sb) != 0 || fmt_surfaces_parse(sb.data, sb.size, FMT_SURF_FALLBACK_UP, &ss) != 0) {
                missing++; fmt_buf_free(&sb); fmt_model_free(m); continue;
            }
            fmt_buf_free(&sb);
            models++;
            const char* want = getenv("CVC_ENTITY"); const char* en = fmt_entity_string(&e[i], "Name");
            const int report = want && en && !strcmp(want, en); const int t_first = nT;
            /* only the first LOD's bindings when the model lists LODs */
            int b0 = 0, b1 = m->nbindings;
            if (m->nlods > 0) { b0 = (int)m->lods[0].first; b1 = b0 + (int)m->lods[0].count; if (b1 > m->nbindings) b1 = m->nbindings; }
            for (int b = b0; b < b1; b++) {
                const fmt_model_binding* bd = &m->bindings[b];
                if ((int)bd->surface >= ss.count) continue;
                const fmt_surface* f = &ss.s[bd->surface];
                float W[16];
                if ((int)bd->node < m->nnodes && !f->skinned) mul(m->nodes[bd->node].world, M, W); else memcpy(W, M, sizeof W);
                for (int t = 0; t < f->ntris; t++) {
                    float p[3][3];
                    for (int k = 0; k < 3; k++) xf(&f->pos[3 * f->tris[3 * t + k]], W, p[k]);
                    add_tri(p[0], p[1], p[2]);
                }
            }
            if (report) {
                float mn[3] = {FLT_MAX, FLT_MAX, FLT_MAX}, mx[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
                for (int t = t_first; t < nT; t++) for (int a = 0; a < 3; a++) {
                    mn[a] = fminf(mn[a], fminf(T[t].a[a], fminf(T[t].b[a], T[t].c[a]))); mx[a] = fmaxf(mx[a], fmaxf(T[t].a[a], fmaxf(T[t].b[a], T[t].c[a])));
                }
                printf("DRAWN %s (%s) %d triangles box x %.0f..%.0f y %.0f..%.0f z %.0f..%.0f lods %d bindings %d\n", en, e[i].type, nT - t_first, mn[0], mx[0], mn[1], mx[1], mn[2], mx[2], m->nlods, m->nbindings);
            }
            fmt_surfaces_free(&ss);
            fmt_model_free(m);
        }
    }
    stamp = calloc((size_t)nT + 1, sizeof(int));
    /* the base heightfield as drawn geometry: sampled through the terrain reader */
    fmt_terrain* ter = fmt_terrain_load(lv);
    fmt_collis_set set; memset(&set, 0, sizeof set);
    fmt_collis_load_placements(lv, subs, ns, &set);
    printf("LEVEL %s drawn models %d (placeholder %d, unreadable %d) triangles %d terrain %s collision placements %d\n", argv[2], models, skipped_ph, missing, nT,
           ter ? "yes" : "no", set.count);
    if (getenv("CVC_VIEW")) {
        unsigned id = 0; char pre[400] = "";
        if (sscanf(getenv("CVC_VIEW"), "%u:%399s", &id, pre) == 2)
            for (int i = 0; i < set.count; i++) {
                const fmt_collis_placement* pl = &set.items[i];
                if (pl->entity_id != id || !pl->collis || pl->collis->kind != 22) continue;
                float lo[3] = {FLT_MAX, FLT_MAX, FLT_MAX}, hi[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
                for (uint32_t c = 0; c < pl->collis->count; c++) {
                    const fmt_collis_chunk* ch = &pl->collis->chunks[c]; float pos[3]; fmt_collis_chunk_position(pl->collis, (int)c, pl->matrix, pos);
                    for (uint32_t t = 0; t < ch->count * 3; t++) { float w[3]; col_vertex(pl, pos, &ch->tris[3 * t], w); for (int a = 0; a < 3; a++) { lo[a] = fminf(lo[a], w[a]); hi[a] = fmaxf(hi[a], w[a]); } }
                }
                for (int a = 0; a < 3; a++) { const float m = fmaxf(30.f, 0.25f * (hi[a] - lo[a])); lo[a] -= m; hi[a] += m; }
                char path[512];
                snprintf(path, sizeof path, "%s_top.ppm", pre);   view(path, 0, 2, 1, 0, lo, hi, pl);
                snprintf(path, sizeof path, "%s_front.ppm", pre); view(path, 0, 1, 2, 0, lo, hi, pl);
                snprintf(path, sizeof path, "%s_side.ppm", pre);  view(path, 2, 1, 0, 0, lo, hi, pl);
                printf("VIEW %u written: %s_{top,front,side}.ppm, box x %.0f..%.0f y %.0f..%.0f z %.0f..%.0f\n", id, pre, lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]);
            }
    }
    double total_area = 0, total_un = 0;
    for (int i = 0; i < set.count; i++) {
        fmt_collis_placement* pl = &set.items[i];
        if (!pl->collis || pl->collis->kind != 22) continue;
        double area = 0, un_area = 0; int samples = 0, un = 0;
        float mn[3] = {FLT_MAX, FLT_MAX, FLT_MAX}, mx[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
        for (uint32_t c = 0; c < pl->collis->count; c++) {
            const fmt_collis_chunk* ch = &pl->collis->chunks[c]; float pos[3]; fmt_collis_chunk_position(pl->collis, (int)c, pl->matrix, pos);
            for (uint32_t t = 0; t < ch->count; t++) {
                float v[3][3]; for (int k = 0; k < 3; k++) col_vertex(pl, pos, &ch->tris[9 * t + 3 * k], v[k]);
                float e1[3], e2[3], cr[3]; for (int a = 0; a < 3; a++) { e1[a] = v[1][a] - v[0][a]; e2[a] = v[2][a] - v[0][a]; }
                cr[0] = e1[1] * e2[2] - e1[2] * e2[1]; cr[1] = e1[2] * e2[0] - e1[0] * e2[2]; cr[2] = e1[0] * e2[1] - e1[1] * e2[0];
                const double ar = 0.5 * sqrt((double)cr[0] * cr[0] + (double)cr[1] * cr[1] + (double)cr[2] * cr[2]);
                float g[3]; for (int a = 0; a < 3; a++) g[a] = (v[0][a] + v[1][a] + v[2][a]) / 3;
                float S[11][3]; int k = 0;
                for (int q = 0; q < 3; q++) { memcpy(S[k++], v[q], 12); }
                for (int q = 0; q < 3; q++) for (int a = 0; a < 3; a++) S[k + q][a] = 0.5f * (v[q][a] + v[(q + 1) % 3][a]);
                k += 3;
                memcpy(S[k++], g, 12);
                for (int q = 0; q < 3; q++) for (int a = 0; a < 3; a++) S[k + q][a] = 0.5f * (v[q][a] + g[a]);
                k += 3;
                int tu = 0;
                for (int q = 0; q < k; q++) {
                    int ok = covered(S[q], dist);
                    if (!ok && ter) { float h; if (terrain_height(ter, S[q][0], S[q][2], &h) && fabsf(h - S[q][1]) <= dist) ok = 1; }
                    if (!ok) {
                        /* exposed: above everything drawn within 8 ft around it (and above the terrain): a rider can touch it */
                        float top = drawn_top(S[q], 8.0f), h;
                        if (ter && terrain_height(ter, S[q][0], S[q][2], &h) && h > top) top = h;
                        if (S[q][1] > top + dist) {
                            tu++;
                            for (int a = 0; a < 3; a++) { if (S[q][a] < mn[a]) mn[a] = S[q][a]; if (S[q][a] > mx[a]) mx[a] = S[q][a]; }
                        }
                    }
                }
                samples += k; un += tu; area += ar; un_area += ar * tu / k;
            }
        }
        total_area += area; total_un += un_area;
        if (un_area >= 20.0 || verbose)
            printf("UNDRAWN%s id %-8u sub %d %-34s col %-34s uncovered %5.1f %% %8.0f ft^2 of %8.0f  box x %.0f..%.0f y %.0f..%.0f z %.0f..%.0f\n", is_proxy(pl->entity_id) ? " PROXY" : "", pl->entity_id, pl->subscene, pl->name ? pl->name : "?", pl->asset ? pl->asset : "?",
                   samples ? 100.0 * un / samples : 0.0, un_area, area, mn[0], mx[0], mn[1], mx[1], mn[2], mx[2]);
    }
    printf("TOTAL %s collision area %.0f ft^2, undrawn %.0f ft^2 (%.2f %%)\n", argv[2], total_area, total_un, total_area > 0 ? 100.0 * total_un / total_area : 0.0);
    return 0;
}
