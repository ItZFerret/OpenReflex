/* fmt_model.c -- see fmt_model.h. Reference: bike_asset_export.py (parse_model, parse_surfaces, strip_triangles), RfxData.cs. */
#include "fmt_model.h"

#include <math.h>
#include <stdlib.h>

void fmt_model_free(fmt_model* m)
{
    if (!m) return;
    free(m->nodes); free(m->lods); free(m->bindings); free(m->storage);
    free(m);
}

static const char* pool_string(const uint8_t* pool, uint32_t size, uint32_t off)
{
    if (off >= size) return NULL;
    for (uint32_t i = off; i < size; i++) if (!pool[i]) return (const char*)pool + off;
    return NULL;
}

fmt_model* fmt_model_parse(const uint8_t* data, size_t size)
{
    if (size < 172 || size > ((size_t)16 << 20)) { fmt_fail("model: size %llu outside bounds", (unsigned long long)size); return NULL; }
    if (fmt_u32(data) != 0x88AABB00u || fmt_u32(data + 4) != 4) { fmt_fail("model: unsupported header"); return NULL; }
    uint32_t flags = fmt_u32(data + 8), count = fmt_u32(data + 12), names_size = fmt_u32(data + 16), lod_count = fmt_u32(data + 20),
             mat_size = fmt_u32(data + 24), bind_count = fmt_u32(data + 28), ref_count = fmt_u32(data + 32);
    if (flags > 1 || fmt_u32(data + 36) != 0) { fmt_fail("model: unsupported flags"); return NULL; }
    if (!(count > 0 && count <= 1024 && lod_count <= 16 && bind_count <= 4096 && ref_count <= 4096)) { fmt_fail("model: counts outside bounds"); return NULL; }
    uint64_t expected = 172 + (uint64_t)count * 108 + names_size + (uint64_t)lod_count * 12 + (uint64_t)bind_count * 20 + mat_size + (uint64_t)ref_count * 5;
    if (expected != size) { fmt_fail("model: record lengths (%llu) do not consume the resource (%llu)", (unsigned long long)expected, (unsigned long long)size); return NULL; }
    fmt_model* m = (fmt_model*)fmt_alloc(sizeof *m);
    if (!m) return NULL;
    m->storage = (uint8_t*)fmt_alloc(size + 1);
    m->nodes = (fmt_model_node*)fmt_alloc((size_t)count * sizeof(fmt_model_node));
    m->lods = (fmt_model_lod*)fmt_alloc(((size_t)lod_count + 1) * sizeof(fmt_model_lod));
    m->bindings = (fmt_model_binding*)fmt_alloc(((size_t)bind_count + 1) * sizeof(fmt_model_binding));
    if (!m->storage || !m->nodes || !m->lods || !m->bindings) { fmt_model_free(m); return NULL; }
    memcpy(m->storage, data, size);
    const uint8_t* b = m->storage;
    m->flags = flags; m->nnodes = (int)count; m->nlods = (int)lod_count; m->nbindings = (int)bind_count; m->surface_refs = (int)ref_count;
    memcpy(m->name, b + 40, 32); m->name[32] = 0;
    size_t records = 172 + (size_t)count * 64, names = records + (size_t)count * 44;
    for (uint32_t i = 0; i < count; i++) {
        fmt_model_node* n = &m->nodes[i];
        for (int k = 0; k < 16; k++) {
            n->local[k] = fmt_f32(b + 172 + (size_t)i * 64 + 4 * (size_t)k);
            if (!isfinite(n->local[k])) { fmt_fail("model: nonfinite matrix of node %u", i); fmt_model_free(m); return NULL; }
        }
        uint32_t name_off = fmt_u32(b + records + (size_t)i * 44), parent = fmt_u32(b + records + (size_t)i * 44 + 4);
        for (int k = 0; k < 9; k++) {
            n->extra[k] = fmt_f32(b + records + (size_t)i * 44 + 8 + 4 * (size_t)k);
            if (!isfinite(n->extra[k])) { fmt_fail("model: nonfinite record of node %u", i); fmt_model_free(m); return NULL; }
        }
        if (!(parent == 0xFFFFFFFFu || parent < i)) { fmt_fail("model: node %u parent %u must precede it", i, parent); fmt_model_free(m); return NULL; }
        n->parent = parent == 0xFFFFFFFFu ? -1 : (int)parent;
        n->name = pool_string(b + names, names_size, name_off);
        if (!n->name) { fmt_fail("model: node %u name outside the pool", i); fmt_model_free(m); return NULL; }
        if (n->parent < 0) { for (int k = 0; k < 16; k++) n->world64[k] = n->local[k]; }
        else {
            const double* pw = m->nodes[n->parent].world64;
            for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) {
                double s = 0;                                   /* sum(a[r*4+k] * b[k*4+c]) in double, k = 0..3, as the Python */
                for (int k = 0; k < 4; k++) s += (double)n->local[r * 4 + k] * pw[k * 4 + c];
                n->world64[r * 4 + c] = s;
            }
        }
        for (int k = 0; k < 16; k++) n->world[k] = (float)n->world64[k];
    }
    size_t p = names + names_size;
    for (uint32_t i = 0; i < lod_count; i++, p += 12) {
        m->lods[i].count = fmt_u32(b + p); m->lods[i].first = fmt_u32(b + p + 4); m->lods[i].distance = fmt_u32(b + p + 8);
        if ((uint64_t)m->lods[i].first + m->lods[i].count > bind_count) { fmt_fail("model: LOD %u exceeds the bindings", i); fmt_model_free(m); return NULL; }
    }
    size_t mats = p + (size_t)bind_count * 20;
    for (uint32_t i = 0; i < bind_count; i++, p += 20) {
        fmt_model_binding* bd = &m->bindings[i];
        uint32_t mo = fmt_u32(b + p);
        bd->surface = fmt_u32(b + p + 4); bd->node = fmt_u32(b + p + 8); bd->a = fmt_u32(b + p + 12); bd->b = fmt_u32(b + p + 16);
        bd->material = pool_string(b + mats, mat_size, mo);
        if (!bd->material || bd->surface >= ref_count || bd->node >= count) { fmt_fail("model: invalid binding %u", i); fmt_model_free(m); return NULL; }
    }
    return m;
}

int fmt_model_find_node(const fmt_model* m, const char* name)
{
    if (m && name) for (int i = 0; i < m->nnodes; i++) if (!fmt_stricmp(m->nodes[i].name, name)) return i;
    return -1;
}

/* ------------------------------------------------------------------------------------------------ surfaces */

void fmt_surfaces_free(fmt_surfaces* s)
{
    if (!s) return;
    for (int i = 0; i < s->count; i++) {
        fmt_surface* f = &s->s[i];
        free(f->pos); free(f->nrm); free(f->uv0); free(f->uv1); free(f->color); free(f->tangent); free(f->bones); free(f->weights); free(f->strip); free(f->tris);
    }
    free(s->s);
    s->s = NULL; s->count = 0;
}

static int strip_to_triangles(fmt_surface* f)
{
    f->tris = (uint32_t*)fmt_alloc(((size_t)f->nstrip * 3 + 3) * sizeof(uint32_t));
    if (!f->tris) return -1;
    for (int i = 0; i + 2 < f->nstrip; i++) {
        uint32_t a = f->strip[i], b = f->strip[i + 1], c = f->strip[i + 2];
        if (i & 1) { uint32_t t = a; a = b; b = t; }
        if (a != b && b != c && a != c) { f->tris[3 * f->ntris] = a; f->tris[3 * f->ntris + 1] = b; f->tris[3 * f->ntris + 2] = c; f->ntris++; }
    }
    return 0;
}

static int bad_normal(const float* n, int nonunit)
{
    if (!(isfinite(n[0]) && isfinite(n[1]) && isfinite(n[2]))) return 1;
    if (nonunit) { float l = n[0] * n[0] + n[1] * n[1] + n[2] * n[2]; return fabsf(l - 1.0f) > 2e-3f; }
    return 0;
}

static int repair_normals(fmt_surface* f, int flags)
{
    for (int v = 0; v < f->nverts; v++) {
        if (!bad_normal(&f->nrm[3 * v], flags & FMT_SURF_REPAIR_NONUNIT)) continue;
        /* the adjacent faces' normals summed (double, triangle order), normalised; (0,1,0) if they cancel */
        double sum[3] = { 0, 0, 0 };
        for (int t = 0; t < f->ntris; t++) {
            uint32_t a = f->tris[3 * t], b = f->tris[3 * t + 1], c = f->tris[3 * t + 2];
            if ((uint32_t)v != a && (uint32_t)v != b && (uint32_t)v != c) continue;
            double u[3], w[3];
            for (int k = 0; k < 3; k++) { u[k] = (double)f->pos[3 * b + k] - (double)f->pos[3 * a + k]; w[k] = (double)f->pos[3 * c + k] - (double)f->pos[3 * a + k]; }
            double x = u[1] * w[2] - u[2] * w[1], y = u[2] * w[0] - u[0] * w[2], z = u[0] * w[1] - u[1] * w[0];
            sum[0] += x; sum[1] += y; sum[2] += z;
        }
        double len = sqrt(sum[0] * sum[0] + sum[1] * sum[1] + sum[2] * sum[2]);
        if (len <= 0) {
            if (!(flags & FMT_SURF_FALLBACK_UP)) return fmt_fail("surface: cannot repair a nonfinite normal from the adjacent geometry");
            f->nrm[3 * v] = 0; f->nrm[3 * v + 1] = 1; f->nrm[3 * v + 2] = 0;
        } else for (int k = 0; k < 3; k++) f->nrm[3 * v + k] = (float)(sum[k] / len);
        f->repaired++;
    }
    return 0;
}

int fmt_surfaces_parse(const uint8_t* b, size_t size, int flags, fmt_surfaces* out)
{
    out->s = NULL; out->count = 0;
    if (size < 16 || size > ((size_t)64 << 20)) return fmt_fail("surface: size outside bounds");
    if (fmt_u32(b) != 0x99BBCC00u || fmt_u32(b + 4) != 4) return fmt_fail("surface: unsupported header");
    uint32_t count = fmt_u32(b + 8);
    if (count == 0 || count > 4096) return fmt_fail("surface: bad count %u", count);
    size_t p = 12 + (size_t)count * 352;
    if (p + 4 > size || fmt_u32(b + p) != 0) return fmt_fail("surface: invalid data start");
    p += 4;
    out->s = (fmt_surface*)fmt_alloc((size_t)count * sizeof(fmt_surface));
    if (!out->s) return -1;
    out->count = (int)count;
    for (uint32_t s = 0; s < count; s++) {
        const uint8_t* h = b + 12 + (size_t)s * 352;
        fmt_surface* f = &out->s[s];
        uint32_t sflags = fmt_u32(h + 12), prims = fmt_u32(h + 16), nv = fmt_u32(h + 20), ib = fmt_u32(h + 24), vb = fmt_u32(h + 192), pal = fmt_u32(h + 256);
        if (fmt_u32(h) != 0 || fmt_u32(h + 4) != 0 || fmt_u32(h + 8) != 0xFFFFFFFFu || !(sflags == 0 || sflags == 0x40003)) { fmt_fail("surface %u: unsupported flags", s); goto fail; }
        if (!(nv > 0 && nv <= 65535 && ib == (prims + 2) * 2 && vb > 0 && vb % nv == 0 && pal <= 256)) { fmt_fail("surface %u: invalid buffer lengths", s); goto fail; }
        uint32_t stride = vb / nv;
        if ((uint64_t)p + pal + ib + vb > size) { fmt_fail("surface %u: truncated payload", s); goto fail; }
        f->flags = sflags; f->skinned = sflags == 0x40003; f->nverts = (int)nv;
        f->npalette = (int)pal;
        memcpy(f->palette, b + p, pal); p += pal;
        f->nstrip = (int)(ib / 2);
        f->strip = (uint16_t*)fmt_alloc((size_t)f->nstrip * 2 + 2);
        if (!f->strip) goto fail;
        for (int i = 0; i < f->nstrip; i++) {
            f->strip[i] = fmt_u16(b + p + 2 * (size_t)i);
            if (f->strip[i] >= nv) { fmt_fail("surface %u: strip index outside the vertex buffer", s); goto fail; }
        }
        p += ib;
        f->pos = (float*)fmt_alloc((size_t)nv * 12); f->nrm = (float*)fmt_alloc((size_t)nv * 12);
        f->uv0 = (float*)fmt_alloc((size_t)nv * 8); f->uv1 = (float*)fmt_alloc((size_t)nv * 8);
        if (!f->pos || !f->nrm || !f->uv0 || !f->uv1) goto fail;
        int have_pos = 0, have_nrm = 0, bone_at = -1, weight_at = -1;
        for (int d = 0; d < 20; d++) {
            const uint8_t* e = h + 28 + (size_t)d * 8;
            unsigned stream = fmt_u16(e), offset = fmt_u16(e + 2), type = e[4], usage = e[6], index = e[7];
            if (stream == 255) break;
            if (stream != 0 || offset >= stride) { fmt_fail("surface %u: bad vertex declaration", s); goto fail; }
            const uint8_t* vbuf = b + p;
            if (usage == 0) {
                if (type != 16 && type != 2) { fmt_fail("surface %u: unsupported position type %u", s, type); goto fail; }
                have_pos = 1;
                for (uint32_t v = 0; v < nv; v++) {
                    const uint8_t* at = vbuf + (size_t)v * stride + offset;
                    for (int k = 0; k < 3; k++) f->pos[3 * v + k] = type == 2 ? fmt_f32(at + 4 * k) : fmt_f16(fmt_u16(at + 2 * k));
                    if (type == 16 && fmt_f16(fmt_u16(at + 6)) != 1.0f) { fmt_fail("surface %u: vertex w is not 1", s); goto fail; }
                    if (!isfinite(f->pos[3 * v]) || !isfinite(f->pos[3 * v + 1]) || !isfinite(f->pos[3 * v + 2])) { fmt_fail("surface %u: nonfinite position", s); goto fail; }
                }
            } else if (usage == 3) {
                if (type != 2) { fmt_fail("surface %u: unsupported normal type %u", s, type); goto fail; }
                have_nrm = 1;
                for (uint32_t v = 0; v < nv; v++) for (int k = 0; k < 3; k++) f->nrm[3 * v + k] = fmt_f32(vbuf + (size_t)v * stride + offset + 4 * (size_t)k);
            } else if (usage == 5 && index <= 1) {
                if (type != 15 && type != 1) { fmt_fail("surface %u: unsupported UV type %u", s, type); goto fail; }
                float* uv = index == 0 ? f->uv0 : f->uv1;
                for (uint32_t v = 0; v < nv; v++) {
                    const uint8_t* at = vbuf + (size_t)v * stride + offset;
                    for (int k = 0; k < 2; k++) uv[2 * v + k] = type == 1 ? fmt_f32(at + 4 * k) : fmt_f16(fmt_u16(at + 2 * k));
                }
            } else if (usage == 10 && index == 0 && type == 4) {
                f->color = (uint8_t*)fmt_alloc((size_t)nv * 4);
                if (!f->color) goto fail;
                for (uint32_t v = 0; v < nv; v++) {
                    const uint8_t* at = vbuf + (size_t)v * stride + offset;         /* D3DCOLOR: little-endian ARGB word */
                    f->color[4 * v] = at[2]; f->color[4 * v + 1] = at[1]; f->color[4 * v + 2] = at[0]; f->color[4 * v + 3] = at[3];
                }
            } else if (usage == 6 && index == 0 && type == 2) {
                f->tangent = (float*)fmt_alloc((size_t)nv * 12);
                if (!f->tangent) goto fail;
                for (uint32_t v = 0; v < nv; v++) for (int k = 0; k < 3; k++) f->tangent[3 * v + k] = fmt_f32(vbuf + (size_t)v * stride + offset + 4 * (size_t)k);
            } else if (usage == 2) { if (type != 5) { fmt_fail("surface %u: unsupported blend index type", s); goto fail; } bone_at = (int)offset; }
            else if (usage == 1) { if (type != 8) { fmt_fail("surface %u: unsupported blend weight type", s); goto fail; } weight_at = (int)offset; }
        }
        if (!have_pos || !have_nrm) { fmt_fail("surface %u: positions and normals are required", s); goto fail; }
        if (f->skinned) {
            if (bone_at < 0 || weight_at < 0 || pal == 0) { fmt_fail("surface %u: incomplete skinned vertex layout", s); goto fail; }
            f->bones = (int*)fmt_alloc((size_t)nv * 16); f->weights = (float*)fmt_alloc((size_t)nv * 16);
            if (!f->bones || !f->weights) goto fail;
            for (uint32_t v = 0; v < nv; v++) for (int k = 0; k < 4; k++) {
                int bi = b[p + (size_t)v * stride + (size_t)bone_at + k];
                float w = b[p + (size_t)v * stride + (size_t)weight_at + k] / 255.0f;
                if (w > 0) {
                    if ((uint32_t)bi >= pal) { fmt_fail("surface %u: blend index outside the bone palette", s); goto fail; }
                    f->bones[4 * v + k] = f->palette[bi]; f->weights[4 * v + k] = w;
                } else f->bones[4 * v + k] = -1;
            }
        }
        if (strip_to_triangles(f) < 0) goto fail;
        if (repair_normals(f, flags) < 0) goto fail;
        p += vb;
    }
    if (p != size) { fmt_fail("surface: %llu trailing bytes", (unsigned long long)(size - p)); goto fail; }
    return 0;
fail:
    fmt_surfaces_free(out);
    return -1;
}
