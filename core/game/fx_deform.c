#include "../src/fmt_modtrack.h"
/* fx_deform.c -- the terrain deformation pass as CPU code: what the original renders on the GPU after vf14's sync (007caf70
 * drawing queue[!q] with vs / ps_hft_deform into the R32F target) for the product build (the oracle's world runtime renders it
 * with its own copy of the same model, native/drafts/world_runtime/deform.cpp, at the first FlushPending). A transliteration of
 * deform.cpp (itself the port of _re/formats/terrain_deform_reference.py, validated against a live capture: footprint exact, 78
 * of 166 texel values bit-exact, the rest within 1.6e-5): per covered texel centre (top-left rule, 1/256 vertex snap) of every
 * queued triangle whose texel lies in one of this frame's vertex tiles (128 x 32), new = ps_hft_deform(previous, brush,
 * softness, z), reading the previous values (last triangle wins). _re/notes/terrain-deformation.md.
 *
 * The model: the SoftnessMask (P +0x28 -> its texture) and the brush (the "TerrainDeformation" material P +0x4264 -> the
 * actual serialized first-pass textureName binding -> its texture, all mips), DXT1 (the mask may be DXT5) decoded to 8-bit RGB with the
 * 'u8r' rounding (565 replicated, thirds (2a + b + 1) / 3), as the boot's model file. Decoded once per texture object (host
 * cache: derived data). The targets: both R32F render target textures (P +8 / +0xc) are kept equal (the stamps are written
 * into rt[!idx] from rt[idx]'s values, then the written rectangle copied back), so the readback of either gives the grid. */
#include "fx.h"
#include "fx_util.h"
#include "../src/rx_d3d.h"
#include <stdlib.h>
#include "build_load.h"
#include "fx_material_binding.h"
#include "../src/fmt_material.h"
#include <stdio.h>


#define DXT1 0x31545844u
#define DXT5 0x35545844u

/* ---- core texture objects (rx_d3d.h: +0xc width, +0x10 height, +0x14 levels, +0x18 format, +0x24 level table x 0x18) ---- */
static int is_texture(rx_addr v) { return v >= 0x10000 && U32(v, 0) == RX_D3D_VT_TEXTURE; }
static uint32_t tex_levels(rx_addr t) { return U32(t, 0x14); }
static uint32_t tex_format(rx_addr t) { return U32(t, 0x18); }
static uint32_t lvl_w(rx_addr t, uint32_t l) { return U32(U32(t, 0x24) + 0x18 * l, 0xc); }
static uint32_t lvl_h(rx_addr t, uint32_t l) { return U32(U32(t, 0x24) + 0x18 * l, 0x10); }

/* a game texture object -> its D3D texture: the pointer itself or up to three levels of fields (TextureAssetHandle -> +0x44
 * TextureAsset -> +0x10 platform data -> +0 the texture), the first that passes `ok` */
typedef int (*tex_ok)(rx_addr t);
static rx_addr find_texture(rx_addr v, int depth, tex_ok ok) {
    if (v < 0x10000 || v >= 0xfff00000u) return 0;
    if (is_texture(v) && ok(v)) return v;
    if (depth == 0) return 0;
    for (uint32_t off = 0; off < 0x80; off += 4) { const rx_addr r = find_texture(U32(v, off), depth - 1, ok); if (r) return r; }
    return 0;
}
static int mask_ok(rx_addr t) { return (tex_format(t) == DXT1 || tex_format(t) == DXT5) && U32(t, 0xc) >= 256; }
static int brush_ok(rx_addr t) { return tex_format(t) == DXT1 && tex_levels(t) > 1; }
/* CPU deformation requires only the actual first-pass sampler binding, not a
 * fabricated material effect object. Captured from the decoded loader stream. */
typedef struct brush_binding {
    rx_addr material;
    char* texture;
    struct brush_binding* next;
} brush_binding;

static brush_binding* bindings;
static unsigned binding_logs, model_logs;

#ifndef RX_ORACLE
void rx_fx_material_binding_capture(rx_addr material, const void* bytes, size_t size) {
    /* Invalidate first, including malformed or incompatible reloads. */
    brush_binding** link = &bindings;
    while (*link) {
        if ((*link)->material == material) {
            brush_binding* old = *link;
            *link = old->next;
            free(old->texture);
            free(old);
            break;
        }
        link = &(*link)->next;
    }

    fmt_material* m = fmt_material_parse(bytes, size);
    if (!m) return;
    /* The implemented CPU shader has one sampler in its authored first pass.
     * Other shaders/effects do not acquire a guessed binding. The parser
     * normalizes a missing textureName to the empty string. */
    if (m->num_passes == 1 && m->pixel_entry &&
        !strcmp(m->pixel_entry, "ps_hft_deform") &&
        m->ntextures == 1 && m->textures[0].name[0]) {
        size_t n = strlen(m->textures[0].name);
        if (n < 64) {
            brush_binding* b = bindings;
            while (b && b->material != material) b = b->next;
            if (!b) {
                b = calloc(1, sizeof *b);
                if (b) {
                    b->material = material;
                    b->next = bindings;
                    bindings = b;
                }
            }
            if (b) {
                char* name = malloc(n + 1);
                if (name) {
                    memcpy(name, m->textures[0].name, n + 1);
                    free(b->texture);
                    b->texture = name;
                    if (binding_logs < 16) {
                        ++binding_logs;
                        fprintf(stderr, "[deform-binding] material=%08x texture=%s "
                                "pixel=%s passes=%d samplers=%d\n",
                                material, name, m->pixel_entry,
                                m->num_passes, m->ntextures);
                    }
                }
            }
        }
    }
    fmt_material_free(m);
}
#endif

static rx_addr find_brush(rx_addr material, int depth) {
    (void)depth;
#ifndef RX_ORACLE
    for (brush_binding* b = bindings; b; b = b->next) {
        if (b->material != material || !b->texture) continue;
        rx_addr h = rx_load_find_handle_c("texture", b->texture);
        if (!h) return 0;
        rx_addr a = U32(h, 0x44);
        if (!a || !U32(a, 0x14)) return 0;
        rx_addr array = U32(a, 0x10);
        if (!array) return 0;
        rx_addr texture = U32(array, 0);
        return is_texture(texture) && brush_ok(texture) ? texture : 0;
    }
#else
    /* Oracle terrain rendering is owned by its runtime provider. Never read
     * or populate this product-only host metadata during Verify. */
    (void)material;
#endif
    return 0;
}

/* DXT1 / DXT5 -> 8-bit RGB, 'u8r' (boot.cpp dxt1_rgb_u8r) */
static void decode_rgb(rx_addr bits, uint32_t pitch, uint32_t w, uint32_t h, int dxt5, uint8_t* out) {
    for (uint32_t by = 0; by < (h + 3) / 4; by++)
        for (uint32_t bx = 0; bx < (w + 3) / 4; bx++) {
            const rx_addr b = bits + by * pitch + (dxt5 ? bx * 16 + 8 : bx * 8);
            const uint32_t c0 = U16(b, 0), c1 = U16(b, 2), idx = U32(b, 4);
            uint32_t pal[4][3];
            const uint32_t cc[2] = {c0, c1};
            for (int k = 0; k < 2; k++) {
                const uint32_t r = cc[k] >> 11, g = (cc[k] >> 5) & 63, bl = cc[k] & 31;
                pal[k][0] = (r << 3) | (r >> 2); pal[k][1] = (g << 2) | (g >> 4); pal[k][2] = (bl << 3) | (bl >> 2);
            }
            for (int i = 0; i < 3; i++) {
                if (c0 > c1 || dxt5) { pal[2][i] = (2 * pal[0][i] + pal[1][i] + 1) / 3; pal[3][i] = (pal[0][i] + 2 * pal[1][i] + 1) / 3; }
                else { pal[2][i] = (pal[0][i] + pal[1][i] + 1) / 2; pal[3][i] = 0; }
            }
            for (uint32_t k = 0; k < 16; k++) {
                const uint32_t x = bx * 4 + (k & 3), y = by * 4 + (k >> 2);
                if (x >= w || y >= h) continue;
                const uint32_t* c = pal[(idx >> (2 * k)) & 3];
                uint8_t* o = out + ((size_t)y * w + x) * 3;
                o[0] = (uint8_t)c[0]; o[1] = (uint8_t)c[1]; o[2] = (uint8_t)c[2];
            }
        }
}

/* ---- the model (host cache) ---- */
#define MAX_MIPS 16
typedef struct { int w, h; uint8_t* rgb; } mip;
static struct { rx_addr mask_tex, brush_tex; int mask_w, mask_h; uint8_t* mask; int mask_mips; mip softness[MAX_MIPS]; int mips; mip brush[MAX_MIPS]; } g_model;

/* Session end (rx_session_destroy): the cache is malloc'd host memory keyed by guest texture addresses; release it. */
void rx_fx_deform_release(void) {
    while (bindings) {
        brush_binding* b = bindings;
        bindings = b->next;
        free(b->texture);
        free(b);
    }
    binding_logs = 0;
    model_logs = 0;
    for (int i = 0; i < g_model.mask_mips; i++) free(g_model.softness[i].rgb);
    for (int i = 0; i < g_model.mips; i++) free(g_model.brush[i].rgb);
    memset(&g_model, 0, sizeof g_model);
}

static int model_load(rx_addr P) {
    const rx_addr mt = find_texture(U32(P, 0x28), 3, mask_ok), bt = find_brush(U32(P, 0x4264), 3);
    if (!mt || !bt) return 0;
    if (mt == g_model.mask_tex && bt == g_model.brush_tex && g_model.mask) return 1;
    for (int i = 0; i < g_model.mask_mips; i++) free(g_model.softness[i].rgb); for (int i = 0; i < g_model.mips; i++) free(g_model.brush[i].rgb);
    memset(&g_model, 0, sizeof g_model);
    uint32_t pitch;
    const uint32_t mask_levels = tex_levels(mt) < MAX_MIPS ? tex_levels(mt) : MAX_MIPS;
    if (!mask_levels) return 0;
    for (uint32_t l = 0; l < mask_levels; l++) {
        const rx_addr bits = rx_d3d_bits(mt, l, &pitch);
        mip* m = &g_model.softness[l];
        m->w = (int)lvl_w(mt, l);
        m->h = (int)lvl_h(mt, l);
        if (!bits || m->w <= 0 || m->h <= 0) return 0;
        m->rgb = malloc((size_t)m->w * (size_t)m->h * 3);
        if (!m->rgb) return 0;
        g_model.mask_mips = (int)l + 1;
        decode_rgb(bits, pitch, (uint32_t)m->w, (uint32_t)m->h,
                   tex_format(mt) == DXT5, m->rgb);
    }
    g_model.mask_w = g_model.softness[0].w;
    g_model.mask_h = g_model.softness[0].h;
    g_model.mask = g_model.softness[0].rgb; /* non-owning equal-size fast-path alias */
    const uint32_t n = tex_levels(bt) < MAX_MIPS ? tex_levels(bt) : MAX_MIPS;
    for (uint32_t l = 0; l < n; l++) {
        const rx_addr bb = rx_d3d_bits(bt, l, &pitch);
        mip* m = &g_model.brush[l];
        m->w = (int)lvl_w(bt, l); m->h = (int)lvl_h(bt, l);
        m->rgb = malloc((size_t)m->w * (size_t)m->h * 3);
        if (!bb || !m->rgb) return 0;
        decode_rgb(bb, pitch, (uint32_t)m->w, (uint32_t)m->h, 0, m->rgb);
        g_model.mips = (int)l + 1;
    }
    g_model.mask_tex = mt; g_model.brush_tex = bt;
    if (model_logs < 16) {
        ++model_logs;
        fprintf(stderr, "[deform-model-resolved] patch=%08x maskRoot=%08x brushRoot=%08x "
                "mask=%08x maskW=%d maskH=%d maskFormat=%u maskMips=%u "
                "brush=%08x brushW=%u brushH=%u brushFormat=%u brushMips=%u\n",
                P, U32(P, 0x28), U32(P, 0x4264), mt,
                g_model.mask_w, g_model.mask_h, tex_format(mt), tex_levels(mt),
                bt, lvl_w(bt, 0), lvl_h(bt, 0), tex_format(bt), tex_levels(bt));
    }
    return 1;
}

/* ---- deform.cpp ---- */
static inline float f32(double x) { return (float)x; }
static inline float sat(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
static inline float fmaxs(float a, float b) { return a < b ? b : a; }   /* std::max(a, b) */
static inline float fmins(float a, float b) { return b < a ? b : a; }   /* std::min(a, b) */

/* ps_hft_deform for one texel (binary32 per instruction) */
float rx_fx_ps_hft_deform(float D, float brush_r, float brush_g, float soft_r, float soft_g, float soft_b, float z) {
    float r0z = sat(D * 3.0f);
    r0z = r0z + -1.0f;
    float r1y = fmaxs(soft_b, r0z);
    r0z = (-z >= 0.0f) ? r1y : 1.0f;
    r1y = f32((double)f32((double)r0z * (double)f32(brush_g - brush_r)) + brush_r);
    float a = r1y + -0.5f;
    const float p = D + -0.5f;
    a = a * z;
    float kz = sat(p * -2.0f);
    float ky = sat(p + p);
    ky = sat(-ky + 1.25f);
    kz = sat(-kz + 1.25f);
    ky = ky * soft_r;
    kz = kz * soft_r;
    const float k = (a >= 0.0f) ? kz : ky;
    float n = sat(f32((double)f32((double)k * (double)(-a)) + D));
    n = n + -0.5f;
    n = n + n;
    n = fmaxs(n, -soft_g);
    n = fmins(soft_g, n);
    n = f32((double)f32((double)n * 0.5) + 0.5);
    return sat(n);
}

static inline float u8f(uint8_t c) { return f32((double)c / 255.0); }

/* D3D9 LINEAR filter on one mip, WRAP addressing (texel centres at (i + 0.5) / n); binary64 then f32 */
static void brush_sample(int level, double u, double v, float out[3]) {
    const mip* m = &g_model.brush[level];
    const int w = m->w, h = m->h;
    const double x = u * w - 0.5, y = v * h - 0.5;
    const double x0 = floor(x), y0 = floor(y);
    const double fx = x - x0, fy = y - y0;
    const long ix = (long)x0, iy = (long)y0;
#define FETCH(i, j, c) u8f(m->rgb[(size_t)((((j) % h) + h) % h * w + (((i) % w) + w) % w) * 3 + (size_t)(c)])
    for (int c = 0; c < 3; c++) {
        const double t00 = FETCH(ix, iy, c), t10 = FETCH(ix + 1, iy, c), t01 = FETCH(ix, iy + 1, c), t11 = FETCH(ix + 1, iy + 1, c);
        out[c] = f32((t00 * (1 - fx) + t10 * fx) * (1 - fy) + (t01 * (1 - fx) + t11 * fx) * fy);
    }
#undef FETCH
}
static int mip_level_point(double lod) {
    if (!(lod > 0.0)) return 0;
    const int level = (int)floor(lod + 0.5);
    return level < g_model.mips - 1 ? level : g_model.mips - 1;
}
/* s2 at full-target normalized texel centres: MIN/MAG LINEAR, MIP POINT,
 * U/V CLAMP. The render setup explicitly authors these states (7cb2d3..327).
 * Keep the equal-size mip0 direct texel path bit-identical. */
static void softness_sample(int target_width, int ix, int iy, float out[3]) {
    if(fmt_modtrack_active()) { fmt_modtrack_softness((uint32_t)ix*2048/target_width,(uint32_t)iy*2048/target_width,out); return; }
    if (g_model.mask_w == target_width && g_model.mask_h == target_width) {
        const uint8_t* s = &g_model.mask[((size_t)iy * (size_t)target_width + (size_t)ix) * 3];
        out[0] = u8f(s[0]); out[1] = u8f(s[1]); out[2] = u8f(s[2]);
        return;
    }
    const int dimension = g_model.mask_w > g_model.mask_h ? g_model.mask_w : g_model.mask_h;
    const double lod = log2((double)dimension / (double)target_width);
    int level = lod > 0.0 ? (int)floor(lod + 0.5) : 0;
    if (level >= g_model.mask_mips) level = g_model.mask_mips - 1;
    const mip* m = &g_model.softness[level];
    const double u = ((double)ix + 0.5) / (double)target_width;
    const double v = ((double)iy + 0.5) / (double)target_width;
    const double x = u * m->w - 0.5, y = v * m->h - 0.5;
    const double x0 = floor(x), y0 = floor(y), fx = x - x0, fy = y - y0;
    int ax = (int)x0, ay = (int)y0, bx = ax + 1, by = ay + 1;
    if (ax < 0) ax = 0; if (bx < 0) bx = 0;
    if (ay < 0) ay = 0; if (by < 0) by = 0;
    if (ax >= m->w) ax = m->w - 1; if (bx >= m->w) bx = m->w - 1;
    if (ay >= m->h) ay = m->h - 1; if (by >= m->h) by = m->h - 1;
    for (int c = 0; c < 3; c++) {
        const double a = u8f(m->rgb[((size_t)ay * m->w + ax) * 3 + c]);
        const double b = u8f(m->rgb[((size_t)ay * m->w + bx) * 3 + c]);
        const double d = u8f(m->rgb[((size_t)by * m->w + ax) * 3 + c]);
        const double e = u8f(m->rgb[((size_t)by * m->w + bx) * 3 + c]);
        out[c] = f32((a * (1 - fx) + b * fx) * (1 - fy) + (d * (1 - fx) + e * fx) * fy);
    }
}
static inline double snap(double v) { return nearbyint(v * 256.0) / 256.0; }   /* half-even (the default rounding mode) */

typedef struct { float x, y, z, u, v; } stamp_vertex;
typedef struct { int x0, z0, x1, z1; } deform_rect;

/* one frame's queued quads (6 vertices each) into dst (width W, R32F rows of `pitch` bytes), reading src; the written rect */
static deform_rect apply_stamps(rx_addr dst, rx_addr src, uint32_t pitch, int W, const stamp_vertex* verts, int nv) {
    deform_rect rect = {0x7fffffff, 0x7fffffff, -1, -1};
    if (nv < 3) return rect;
    /* the tiles of this frame's vertices (128 x 32 texels), as FUN_007caf70 builds them */
    static uint8_t tiles[256][256];
    memset(tiles, 0, sizeof tiles);
    const float half = f32((double)(float)W * 0.5), cs = f32((double)half * 0.0078125), rs = f32((double)half * 0.03125);
    const int cmax = W / 128 - 1, rmax = W / 32 - 1;
    for (int i = 0; i < nv; i++) {
        const int row = (int)f32((double)f32(1.0 - (double)verts[i].y) * (double)rs) & 0xff;
        const int col = (int)f32((double)f32((double)verts[i].x + 1.0) * (double)cs) & 0xff;
        tiles[col < cmax ? col : cmax][row < rmax ? row : rmax] = 1;
    }
    const int bw = g_model.brush[0].w, bh = g_model.brush[0].h;
    for (int t = 0; t + 2 < nv; t += 3) {
        const stamp_vertex* tri[3] = {&verts[t], &verts[t + 1], &verts[t + 2]};
        double Pt[3][2];
        for (int k = 0; k < 3; k++) { Pt[k][0] = snap(((double)tri[k]->x + 1.0) * W * 0.5); Pt[k][1] = snap((1.0 - (double)tri[k]->y) * W * 0.5); }
        double area = (Pt[1][0] - Pt[0][0]) * (Pt[2][1] - Pt[0][1]) - (Pt[2][0] - Pt[0][0]) * (Pt[1][1] - Pt[0][1]);
        if (area == 0.0) continue;
        int order[3] = {0, 1, 2};
        if (area < 0) { order[1] = 2; order[2] = 1; }
        double Q[3][2]; for (int k = 0; k < 3; k++) { Q[k][0] = Pt[order[k]][0]; Q[k][1] = Pt[order[k]][1]; }
        area = fabs(area);
        struct { double ax, ay, dx, dy; int tl; } edges[3];
        for (int k = 0; k < 3; k++) {
            const double ax = Q[(k + 1) % 3][0], ay = Q[(k + 1) % 3][1], bx = Q[(k + 2) % 3][0], by = Q[(k + 2) % 3][1];
            const double dx = bx - ax, dy = by - ay;
            edges[k].ax = ax; edges[k].ay = ay; edges[k].dx = dx; edges[k].dy = dy; edges[k].tl = (dy == 0.0 && dx > 0.0) || dy < 0.0;
        }
        const double minx = fmin(fmin(Q[0][0], Q[1][0]), Q[2][0]), maxx = fmax(fmax(Q[0][0], Q[1][0]), Q[2][0]);
        const double miny = fmin(fmin(Q[0][1], Q[1][1]), Q[2][1]), maxy = fmax(fmax(Q[0][1], Q[1][1]), Q[2][1]);
        int ix0 = (int)floor(minx - 0.5), ix1 = (int)ceil(maxx - 0.5), iy0 = (int)floor(miny - 0.5), iy1 = (int)ceil(maxy - 0.5);
        if (ix0 < 0) ix0 = 0;
        if (iy0 < 0) iy0 = 0;
        if (ix1 > W - 1) ix1 = W - 1;
        if (iy1 > W - 1) iy1 = W - 1;
        double grads[3][2];
        for (int k = 0; k < 3; k++) { grads[k][0] = -edges[k].dy / area; grads[k][1] = edges[k].dx / area; }
        int inv[3]; for (int pos = 0; pos < 3; pos++) inv[order[pos]] = pos;
        double dudx = 0, dvdx = 0, dudy = 0, dvdy = 0;
        for (int i = 0; i < 3; i++) {
            const double* g = grads[inv[i]];
            dudx += g[0] * tri[i]->u; dvdx += g[0] * tri[i]->v; dudy += g[1] * tri[i]->u; dvdy += g[1] * tri[i]->v;
        }
        dudx *= bw; dvdx *= bh; dudy *= bw; dvdy *= bh;
        const double rho = fmax(hypot(dudx, dvdx), hypot(dudy, dvdy));
        const double lod = rho > 0 ? log2(rho) : -1.0;
        const int level = mip_level_point(lod);
        for (int iy = iy0; iy <= iy1; iy++) {
            const double py = iy + 0.5;
            for (int ix = ix0; ix <= ix1; ix++) {
                const double px = ix + 0.5;
                double lam[3]; int ok = 1;
                for (int k = 0; k < 3 && ok; k++) {
                    const double e = edges[k].dx * (py - edges[k].ay) - edges[k].dy * (px - edges[k].ax);
                    if (e < 0.0 || (e == 0.0 && !edges[k].tl)) ok = 0; else lam[k] = e / area;
                }
                if (!ok || !tiles[ix / 128][iy / 32]) continue;
                const double l0 = lam[inv[0]], l1 = lam[inv[1]], l2 = lam[inv[2]];
                const float z = f32(l0 * tri[0]->z + l1 * tri[1]->z + l2 * tri[2]->z);
                const double u = l0 * tri[0]->u + l1 * tri[1]->u + l2 * tri[2]->u;
                const double v = l0 * tri[0]->v + l1 * tri[1]->v + l2 * tri[2]->v;
                float br[3]; brush_sample(level, u, v, br);
                float so[3];
                softness_sample(W, ix, iy, so);
                const uint32_t o = (uint32_t)iy * pitch + 4u * (uint32_t)ix;
                SF32(dst, o, rx_fx_ps_hft_deform(F32(src, o), br[0], br[1], so[0], so[1], so[2], z));
                if (ix < rect.x0) rect.x0 = ix;
                if (ix + 1 > rect.x1) rect.x1 = ix + 1;
                if (iy < rect.z0) rect.z0 = iy;
                if (iy + 1 > rect.z1) rect.z1 = iy + 1;
            }
        }
    }
    return rect;
}

/* ---- the pass: queue[!q] with its offsets (x += off.x / (extent / 2), y += off.z / (extent / 2): the runtime's stamp_queue) */
void rx_fx_deform_render(rx_addr P) {
    const rx_addr surf = P - 0x230;
    if (!model_load(P)) return;
    const rx_addr rt_src = U32(P, 8 + 4 * (U32(P, 0x14) != 0)), rt_dst = U32(P, 8 + 4 * (U32(P, 0x14) == 0));
    uint32_t pitch, pitch2;
    const rx_addr src = rx_d3d_bits(rt_src, 0, &pitch), dst = rx_d3d_bits(rt_dst, 0, &pitch2);
    if (!src || !dst || pitch != pitch2) return;
    const int W = (int)lvl_w(rt_src, 0);
    const uint32_t qi = U32(P, 0x18) ? 0 : 1;
    uint32_t quads = U32(P, 0x230 + qi * 0x208), tris = U32(P, 0x440 + qi * 4);
    if (quads > tris / 2) quads = tris / 2;
    if (quads > 64) quads = 64;
    const float extent = F32(surf, 0x44d0);
    const float rcp = 1.0f / (extent * 0.5f);
    stamp_vertex verts[64 * 6];
    int nv = 0;
    for (uint32_t q = 0; q < quads; q++) {
        const rx_addr off = P + 0x30 + qi * 0x208 + 8 * q;
        for (uint32_t k = 0; k < 6; k++) {
            const rx_addr v = P + 0x448 + qi * 0x1e04 + (q * 6 + k) * 20;
            verts[nv].x = F32(off, 0) * rcp + F32(v, 0); verts[nv].y = F32(off, 4) * rcp + F32(v, 4);
            verts[nv].z = F32(v, 8); verts[nv].u = F32(v, 0xc); verts[nv].v = F32(v, 0x10);
            nv++;
        }
    }
    const deform_rect r = apply_stamps(dst, src, pitch, W, verts, nv);
    /* keep both targets equal: the written rectangle's rows copied (the same words the per-word loop moved; with 12 riders spread over
     * the track the rectangle is 50 - 250 thousand texels, ~0.2 ms as a word loop) */
    if (r.x1 > r.x0 && r.z1 > r.z0) {
        for (int z = r.z0; z < r.z1; z++) {
#ifdef RX_ORACLE   /* no flat arena in the oracle build: the same words, one at a time */
            for (int x = r.x0; x < r.x1; x++) SU32(src + (uint32_t)z * pitch + 4u * (uint32_t)x, 0, U32(dst + (uint32_t)z * pitch + 4u * (uint32_t)x, 0));
#else
            memmove(rx_base + src + (uint32_t)z * pitch + 4u * (uint32_t)r.x0, rx_base + dst + (uint32_t)z * pitch + 4u * (uint32_t)r.x0, 4u * (uint32_t)(r.x1 - r.x0));
#endif
        }
        rx_d3d_track_note(U32(surf, 0x240), r.x0, r.z0, r.x1, r.z1);   /* the physics surface the next readback fills: the dirty rectangle */
    }
}
