// See deform.h. Every arithmetic step follows terrain_deform_reference.py: binary32 where the
// reference rounds with f32(), binary64 where it computes in Python floats.
#include "deform.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <set>

namespace reflex_vm {

static inline float f32(double x) { return float(x); }
static inline float sat(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

bool DeformModel::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), {});
    auto u32 = [&](size_t o) { uint32_t v; std::memcpy(&v, b.data() + o, 4); return v; };
    if (b.size() < 20 || std::memcmp(b.data(), "RXDF", 4) != 0 || u32(4) != 1) return false;
    mask_w = int(u32(8)); mask_h = int(u32(12)); uint32_t mips = u32(16);
    size_t at = 20; brush.clear();
    for (uint32_t i = 0; i < mips; i++) { brush.push_back({int(u32(at)), int(u32(at + 4)), {}}); at += 8; }
    size_t mask_bytes = size_t(mask_w) * size_t(mask_h) * 3;
    if (at + mask_bytes > b.size()) return false;
    mask.assign(b.begin() + long(at), b.begin() + long(at + mask_bytes)); at += mask_bytes;
    for (auto& m : brush) {
        size_t n = size_t(m.w) * size_t(m.h) * 3;
        if (at + n > b.size()) return false;
        m.rgb.assign(b.begin() + long(at), b.begin() + long(at + n)); at += n;
    }
    return true;
}

float ps_hft_deform(float D, float brush_r, float brush_g, float soft_r, float soft_g, float soft_b, float z) {
    float r0z = sat(D * 3.0f);
    r0z = r0z + -1.0f;
    float r1y = std::max(soft_b, r0z);
    r0z = (-z >= 0.0f) ? r1y : 1.0f;
    r1y = f32(double(f32(double(r0z) * double(f32(brush_g - brush_r)))) + brush_r);
    float a = r1y + -0.5f;
    float p = D + -0.5f;
    a = a * z;
    float kz = sat(p * -2.0f);
    float ky = sat(p + p);
    ky = sat(-ky + 1.25f);
    kz = sat(-kz + 1.25f);
    ky = ky * soft_r;
    kz = kz * soft_r;
    float k = (a >= 0.0f) ? kz : ky;
    float n = sat(f32(double(f32(double(k) * double(-a))) + D));
    n = n + -0.5f;
    n = n + n;
    n = std::max(n, -soft_g);
    n = std::min(soft_g, n);
    n = f32(double(f32(double(n) * 0.5)) + 0.5);
    return sat(n);
}

static inline float u8f(uint8_t c) { return f32(double(c) / 255.0); }

// D3D9 LINEAR filter on one mip, WRAP addressing (texel centres at (i+0.5)/n); binary64 then f32.
static void brush_sample(const DeformModel& m, int level, double u, double v, float out[3]) {
    const auto& mip = m.brush[size_t(level)];
    int w = mip.w, h = mip.h;
    double x = u * w - 0.5, y = v * h - 0.5;
    double x0 = std::floor(x), y0 = std::floor(y);
    double fx = x - x0, fy = y - y0;
    static const int frac_bits = [] { const char* e = std::getenv("REFLEX_FILTER_BITS"); return e ? std::atoi(e) : 0; }();
    if (frac_bits > 0) { double q = double(1 << frac_bits); fx = std::floor(fx * q) / q; fy = std::floor(fy * q) / q; }
    auto fetch = [&](long i, long j, int c) {
        long ii = ((i % w) + w) % w, jj = ((j % h) + h) % h;
        return u8f(mip.rgb[size_t(jj * w + ii) * 3 + size_t(c)]);
    };
    long ix = long(x0), iy = long(y0);
    for (int c = 0; c < 3; c++) {
        double t00 = fetch(ix, iy, c), t10 = fetch(ix + 1, iy, c), t01 = fetch(ix, iy + 1, c), t11 = fetch(ix + 1, iy + 1, c);
        out[c] = f32((t00 * (1 - fx) + t10 * fx) * (1 - fy) + (t01 * (1 - fx) + t11 * fx) * fy);
    }
}

static int mip_level_point(const DeformModel& m, double lod) {
    if (!(lod > 0.0)) return 0;
    int level = int(std::floor(lod + 0.5));
    return std::min(int(m.brush.size()) - 1, level);
}

static inline double snap(double v) { return std::nearbyint(v * 256.0) / 256.0; }   // half-even (default rounding mode)

DeformRect apply_stamps(std::vector<float>& grid, int W, const std::vector<StampVertex>& verts, const DeformModel& model) {
    DeformRect rect;
    if (verts.size() < 3 || !model.loaded()) return rect;
    // Tiles (one per vertex, 128x32 texels), as FUN_007caf70 builds them.
    std::set<std::pair<int, int>> tiles;
    float half = f32(double(float(W)) * 0.5), cs = f32(double(half) * 0.0078125), rs = f32(double(half) * 0.03125);
    int cmax = W / 128 - 1, rmax = W / 32 - 1;
    for (const auto& v : verts) {
        int row = int(f32(double(f32(1.0 - double(v.y))) * double(rs))) & 0xff;
        int col = int(f32(double(f32(double(v.x) + 1.0)) * double(cs))) & 0xff;
        tiles.insert({std::min(col, cmax), std::min(row, rmax)});
    }
    // The pre-stamp values (every texel reads the source, not earlier triangles' output): only the batch's bounding box of texels
    // is copied (the whole 2048^2 grid was 16 MB per call); bounds as the rasterizer below computes them per triangle.
    int sx0 = W, sx1 = -1, sy0 = W, sy1 = -1;
    for (const auto& v : verts) {
        double px = snap((double(v.x) + 1.0) * W * 0.5), py = snap((1.0 - double(v.y)) * W * 0.5);
        sx0 = std::min(sx0, std::max(0, int(std::floor(px - 0.5)))); sx1 = std::max(sx1, std::min(W - 1, int(std::ceil(px - 0.5))));
        sy0 = std::min(sy0, std::max(0, int(std::floor(py - 0.5)))); sy1 = std::max(sy1, std::min(W - 1, int(std::ceil(py - 0.5))));
    }
    if (sx1 < sx0 || sy1 < sy0) return rect;
    const int sw = sx1 - sx0 + 1;
    std::vector<float> src(size_t(sw) * size_t(sy1 - sy0 + 1));
    for (int y = sy0; y <= sy1; y++) std::memcpy(&src[size_t(y - sy0) * sw], &grid[size_t(y) * size_t(W) + size_t(sx0)], sizeof(float) * size_t(sw));
    int bw = model.brush[0].w, bh = model.brush[0].h;
    for (size_t t = 0; t + 2 < verts.size(); t += 3) {
        const StampVertex* tri[3] = {&verts[t], &verts[t + 1], &verts[t + 2]};
        double P[3][2];
        for (int k = 0; k < 3; k++) { P[k][0] = snap((double(tri[k]->x) + 1.0) * W * 0.5); P[k][1] = snap((1.0 - double(tri[k]->y)) * W * 0.5); }
        double area = (P[1][0] - P[0][0]) * (P[2][1] - P[0][1]) - (P[2][0] - P[0][0]) * (P[1][1] - P[0][1]);
        if (area == 0.0) continue;
        int order[3] = {0, 1, 2};
        if (area < 0) { order[1] = 2; order[2] = 1; }
        double Q[3][2]; for (int k = 0; k < 3; k++) { Q[k][0] = P[order[k]][0]; Q[k][1] = P[order[k]][1]; }
        area = std::fabs(area);
        struct Edge { double ax, ay, dx, dy; bool tl; } edges[3];
        for (int k = 0; k < 3; k++) {
            double ax = Q[(k + 1) % 3][0], ay = Q[(k + 1) % 3][1], bx = Q[(k + 2) % 3][0], by = Q[(k + 2) % 3][1];
            double dx = bx - ax, dy = by - ay;
            edges[k] = {ax, ay, dx, dy, (dy == 0.0 && dx > 0.0) || dy < 0.0};
        }
        double minx = std::min({Q[0][0], Q[1][0], Q[2][0]}), maxx = std::max({Q[0][0], Q[1][0], Q[2][0]});
        double miny = std::min({Q[0][1], Q[1][1], Q[2][1]}), maxy = std::max({Q[0][1], Q[1][1], Q[2][1]});
        int ix0 = std::max(0, int(std::floor(minx - 0.5))), ix1 = std::min(W - 1, int(std::ceil(maxx - 0.5)));
        int iy0 = std::max(0, int(std::floor(miny - 0.5))), iy1 = std::min(W - 1, int(std::ceil(maxy - 0.5)));
        double grads[3][2];
        for (int k = 0; k < 3; k++) { grads[k][0] = -edges[k].dy / area; grads[k][1] = edges[k].dx / area; }
        int inv[3]; for (int pos = 0; pos < 3; pos++) inv[order[pos]] = pos;
        // brush derivatives are constant per triangle
        double dudx = 0, dvdx = 0, dudy = 0, dvdy = 0;
        for (int i = 0; i < 3; i++) {
            const double* g = grads[inv[i]];
            dudx += g[0] * tri[i]->u; dvdx += g[0] * tri[i]->v; dudy += g[1] * tri[i]->u; dvdy += g[1] * tri[i]->v;
        }
        dudx *= bw; dvdx *= bh; dudy *= bw; dvdy *= bh;
        double rho = std::max(std::hypot(dudx, dvdx), std::hypot(dudy, dvdy));
        double lod = rho > 0 ? std::log2(rho) : -1.0;
        int level = mip_level_point(model, lod);
        for (int iy = iy0; iy <= iy1; iy++) {
            double py = iy + 0.5;
            for (int ix = ix0; ix <= ix1; ix++) {
                double px = ix + 0.5, lam[3]; bool ok = true;
                for (int k = 0; k < 3 && ok; k++) {
                    double e = edges[k].dx * (py - edges[k].ay) - edges[k].dy * (px - edges[k].ax);
                    if (e < 0.0 || (e == 0.0 && !edges[k].tl)) ok = false; else lam[k] = e / area;
                }
                if (!ok) continue;
                if (!tiles.count({ix / 128, iy / 32})) continue;
                double l0 = lam[inv[0]], l1 = lam[inv[1]], l2 = lam[inv[2]];
                float z = f32(l0 * tri[0]->z + l1 * tri[1]->z + l2 * tri[2]->z);
                double u = l0 * tri[0]->u + l1 * tri[1]->u + l2 * tri[2]->u;
                double v = l0 * tri[0]->v + l1 * tri[1]->v + l2 * tri[2]->v;
                float br[3]; brush_sample(model, level, u, v, br);
                float so[3] = {1.0f, 1.0f, 0.0f};
                if (model.mask_w == W && model.mask_h == W) {
                    const uint8_t* s = &model.mask[(size_t(iy) * size_t(W) + size_t(ix)) * 3];
                    so[0] = u8f(s[0]); so[1] = u8f(s[1]); so[2] = u8f(s[2]);
                }
                size_t i = size_t(iy) * size_t(W) + size_t(ix);
                grid[i] = ps_hft_deform(src[size_t(iy - sy0) * sw + size_t(ix - sx0)], br[0], br[1], so[0], so[1], so[2], z);
                rect.x0 = std::min(rect.x0, ix); rect.x1 = std::max(rect.x1, ix + 1);
                rect.z0 = std::min(rect.z0, iy); rect.z1 = std::max(rect.z1, iy + 1);
            }
        }
    }
    return rect;
}

}  // namespace reflex_vm
