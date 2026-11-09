/* fmt_collis.c -- see fmt_collis.h. Reference: alpine_collision_objects.py (parse_collis, entity_matrix, transform). */
#if defined(__clang__)
#  pragma STDC FP_CONTRACT OFF
#endif
#include "fmt_collis.h"

#include <math.h>
#include <stdlib.h>

void fmt_collis_free(fmt_collis* c)
{
    if (!c) return;
    for (uint32_t i = 0; c->chunks && i < c->count; i++) free(c->chunks[i].tris);
    free(c->chunks); free(c->shapes); free(c->raw);
    free(c);
}

fmt_collis* fmt_collis_parse(const uint8_t* d, size_t size)
{
    if (size < 24) { fmt_fail("collis: truncated header"); return NULL; }
    if (fmt_u32(d) != 1234 || memcmp(d + 4, "col\0", 4) != 0 || fmt_u32(d + 8) != 3 || fmt_u32(d + 16) != 0) { fmt_fail("collis: unsupported header"); return NULL; }
    fmt_collis* c = (fmt_collis*)fmt_alloc(sizeof *c);
    if (!c) return NULL;
    c->version = fmt_u32(d + 8); c->kind = fmt_u32(d + 12); c->count = fmt_u32(d + 20);
    c->raw = (uint8_t*)fmt_alloc(size + 1);
    if (!c->raw) { free(c); return NULL; }
    memcpy(c->raw, d, size);
    c->raw_size = size;
    if (c->kind == 22) {
        if (c->count > size / 16) { fmt_fail("collis: chunk count %u impossible for %llu bytes", c->count, (unsigned long long)size); fmt_collis_free(c); return NULL; }
        c->chunks = (fmt_collis_chunk*)fmt_alloc(((size_t)c->count + 1) * sizeof(fmt_collis_chunk));
        if (!c->chunks) { fmt_collis_free(c); return NULL; }
        size_t cur = 24;
        uint32_t i;
        for (i = 0; i < c->count; i++) {
            if (cur + 16 > size) { fmt_fail("collis: truncated chunk header"); break; }
            fmt_collis_chunk* ch = &c->chunks[i];
            for (int k = 0; k < 3; k++) ch->center[k] = fmt_f32(d + cur + 4 * (size_t)k);
            ch->count = fmt_u32(d + cur + 12);
            cur += 16;
            if (ch->count > (size - cur) / 36) { fmt_fail("collis: chunk %u triangles exceed the file", i); break; }
            ch->tris = (float*)fmt_alloc(((size_t)ch->count * 9 + 1) * sizeof(float));
            if (!ch->tris) break;
            for (size_t k = 0; k < (size_t)ch->count * 9; k++) ch->tris[k] = fmt_f32(d + cur + 4 * k);
            cur += (size_t)ch->count * 36;
        }
        if (i != c->count || cur != size) {
            if (i == c->count) fmt_fail("collis: %llu trailing bytes", (unsigned long long)(size - cur));
            c->count = i;                                         /* free only what was filled */
            fmt_collis_free(c);
            return NULL;
        }
    } else if (c->kind == 15) {
        if (size < 28) { fmt_fail("collis: truncated kind 15 header"); fmt_collis_free(c); return NULL; }
        c->pool_size = fmt_u32(d + 24);
        if (c->pool_size > size - 28) { fmt_fail("collis: name pool exceeds the file"); fmt_collis_free(c); return NULL; }
        c->pool = (const char*)c->raw + 28;
        if (c->pool_size && c->pool[c->pool_size - 1] != 0) { fmt_fail("collis: name pool is not terminated"); fmt_collis_free(c); return NULL; }
        if (c->count > size / 76) { fmt_fail("collis: shape count %u impossible for %llu bytes", c->count, (unsigned long long)size); fmt_collis_free(c); return NULL; }
        c->shapes = (fmt_collis_shape*)fmt_alloc(((size_t)c->count + 1) * sizeof(fmt_collis_shape));
        if (!c->shapes) { fmt_collis_free(c); return NULL; }
        size_t cur = 28 + (size_t)c->pool_size;
        for (uint32_t i = 0; i < c->count; i++) {
            if (cur + 76 > size) { fmt_fail("collis: truncated shape %u", i); fmt_collis_free(c); return NULL; }
            fmt_collis_shape* s = &c->shapes[i];
            s->type = fmt_u32(d + cur);
            size_t rec = s->type == 7 ? 76 : s->type == 8 ? 80 : s->type == 5 ? 88 : 0;
            if (!rec || cur + rec > size) { fmt_fail("collis: shape %u has unknown type %u", i, s->type); fmt_collis_free(c); return NULL; }
            for (int k = 0; k < 16; k++) s->matrix[k] = fmt_f32(d + cur + 4 + 4 * (size_t)k);
            s->link = fmt_i32(d + cur + 68);
            s->name = (s->link >= 0 && (uint32_t)s->link < c->pool_size) ? c->pool + s->link : NULL;
            for (int k = 0; k < (s->type == 7 ? 1 : s->type == 8 ? 2 : 3); k++) s->param[k] = fmt_f32(d + cur + 72 + 4 * (size_t)k);
            s->tail = s->type == 5 ? fmt_u32(d + cur + 84) : 0;
            cur += rec;
        }
        if (cur != size) { fmt_fail("collis: %llu trailing bytes after the shapes", (unsigned long long)(size - cur)); fmt_collis_free(c); return NULL; }
    }
    return c;
}

uint32_t fmt_collis_triangle_count(const fmt_collis* c)
{
    uint32_t n = 0;
    if (c && c->kind == 22) for (uint32_t i = 0; i < c->count; i++) n += c->chunks[i].count;
    return n;
}

void fmt_collis_entity_matrix(const float m[16], float out[16])
{
    for (int i = 0; i < 16; i++) out[i] = m[i];
    for (int r = 0; r < 3; r++) {
        float x = m[4 * r], y = m[4 * r + 1], z = m[4 * r + 2];
        float xx = x * x, yy = y * y, zz = z * z;
        float s1 = xx + yy;
        float dot = s1 + zz;
        float root = sqrtf(dot);
        float scale = 1.0f / root;
        float ox = x * scale, oy = y * scale, oz = z * scale;
        out[4 * r] = ox; out[4 * r + 1] = oy; out[4 * r + 2] = oz;
    }
}

void fmt_collis_transform_point(const float p[3], const float m[16], float out[3])
{
    for (int c = 0; c < 3; c++) {
        float a = p[0] * m[c];
        float b = p[1] * m[4 + c];
        float s1 = a + b;
        float e = p[2] * m[8 + c];
        float s2 = s1 + e;
        out[c] = s2 + m[12 + c];
    }
}

void fmt_collis_quat_approx(const float m[16], float q[4])
{
    double r[3][3];
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) r[i][j] = m[4 * i + j];
    double trace = r[0][0] + r[1][1] + r[2][2];
    if (trace > 0) {
        double s = sqrt(trace + 1) * 2;
        q[0] = (float)((r[1][2] - r[2][1]) / s); q[1] = (float)((r[2][0] - r[0][2]) / s); q[2] = (float)((r[0][1] - r[1][0]) / s); q[3] = (float)(s / 4);
        return;
    }
    int i = 0;
    if (r[1][1] > r[i][i]) i = 1;
    if (r[2][2] > r[i][i]) i = 2;
    int j = (i + 1) % 3, k = (i + 2) % 3;
    double s = sqrt(1 + r[i][i] - r[j][j] - r[k][k]) * 2, t[4];
    t[i] = s / 4; t[j] = (r[i][j] + r[j][i]) / s; t[k] = (r[i][k] + r[k][i]) / s; t[3] = (r[j][k] - r[k][j]) / s;
    for (int n = 0; n < 4; n++) q[n] = (float)t[n];
}

void fmt_collis_chunk_position(const fmt_collis* c, int k, const float em[16], float pos[3])
{
    fmt_collis_transform_point(c->chunks[k].center, em, pos);
}

void fmt_collis_widen_triangle(const float t[9], float o[12])
{
    for (int v = 0; v < 3; v++) { o[4 * v] = t[3 * v]; o[4 * v + 1] = t[3 * v + 1]; o[4 * v + 2] = t[3 * v + 2]; o[4 * v + 3] = t[3 * v + 2]; }
}

void fmt_collis_set_free(fmt_collis_set* s)
{
    if (!s) return;
    for (int i = 0; i < s->count; i++) fmt_collis_free(s->items[i].collis);
    free(s->items);
    s->items = NULL; s->count = 0;
}

int fmt_collis_load_placements(fmt_level* lv, const char* const* subscenes, int nsub, fmt_collis_set* out)
{
    out->items = NULL; out->count = 0;
    fmt_scene* sc = fmt_level_scene(lv);
    if (!sc) return fmt_fail("%s has no scene", fmt_level_name(lv));
    int cap = 0;
    for (int s = 0; s < nsub; s++) {
        int si = fmt_scene_find_subscene(sc, subscenes[s]);
        if (si < 0) { fmt_collis_set_free(out); return fmt_fail("%s: no subscene %s", fmt_level_name(lv), subscenes[s]); }
        int n;
        const fmt_entity* ents = fmt_scene_entities(sc, si, &n);
        if (!ents) { fmt_collis_set_free(out); return -1; }
        for (int i = 0; i < n; i++) {
            if (strcmp(ents[i].type, "CollisionRSData") != 0) continue;
            const char* asset = fmt_entity_asset(&ents[i], "FemaleCollisionAsset");
            if (!asset) continue;
            if (out->count == cap) {
                cap = cap ? cap * 2 : 16;
                fmt_collis_placement* ni = (fmt_collis_placement*)fmt_realloc(out->items, (size_t)cap * sizeof *ni);
                if (!ni) { fmt_collis_set_free(out); return -1; }
                out->items = ni;
            }
            fmt_collis_placement* p = &out->items[out->count];
            memset(p, 0, sizeof *p);
            p->subscene = si; p->entity_id = ents[i].id; p->asset = asset;
            p->name = fmt_entity_string(&ents[i], "Name");
            if (fmt_entity_matrix(&ents[i], p->scene_matrix) < 0) { fmt_collis_set_free(out); return -1; }
            fmt_collis_entity_matrix(p->scene_matrix, p->matrix);
            out->count++;
            fmt_buf raw;
            if (fmt_level_read_asset(lv, asset, "collis", &raw) < 0) continue;     /* collis stays NULL, the error text stays */
            p->collis = fmt_collis_parse(raw.data, raw.size);
            fmt_buf_free(&raw);
        }
    }
    return 0;
}
