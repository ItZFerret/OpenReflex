// Terrain queries (0x007cxxxx) and their SSE helpers (0x0069bxxx): the height-field sampler, cell setup, triangles.
#include "collision_terrain.h"

using namespace collision;
using reflex_port::OUT_EAX;
using reflex_port::OUT_ECX;
using reflex_port::OUT_EDX;
using reflex_port::OUT_XMM0;

namespace collision {

// ---------------------------------------------------------------- 0069b2f0 / 0069b400

Floor4 floor4i(const Guest& g, const X& in) {
    // per lane: r = (x + c) - c with c = 2^23 carrying x's sign (round to nearest), minus 1.0 where r - x > (signed zero of x)
    // (r above x), then truncated. All scalar ops on lane 0 of registers whose other lanes are 0 (movss loads).
    Floor4 o;
    const X x1 = ld_ss(g, kNegZero), x2 = ld_ss(g, kTwo23), x6 = ld_ss(g, kOne);
    X x0, x3, x4, x5 = {{in.u[0], 0, 0, 0}};
    for (int i = 0; i < 3; i++) {
        x3 = andps(x1, x5);
        x4 = orps(x2, x3);
        x0 = subss(addss(x5, x4), x4);
        x4 = subss(x0, x5);
        x4 = andps(cmpnless(x4, x3), x6);
        x0 = subss(x0, x4);
        o.i.u[i] = uint32_t(x86::cvtt(x0.f(0)));
        if (i < 2) x5 = {{in.u[i + 1], 0, 0, 0}};
    }
    o.xmm4 = x4; o.xmm5 = x5;
    x3 = {{in.u[3], 0, 0, 0}};
    X y1 = andps(x1, x3), y2 = orps(x2, y1);
    x0 = subss(addss(x3, y2), y2);
    y2 = andps(cmpnless(subss(x0, x3), y1), x6);
    x0 = subss(x0, y2);
    o.i.u[3] = uint32_t(x86::cvtt(x0.f(0)));
    o.xmm1 = y1; o.xmm2 = y2; o.xmm3 = x3; o.xmm6 = x6;
    return o;
}

// ---------------------------------------------------------------- 007c5440 terrain sampler

// One bilinear sample of a 2x2 patch p (p0 p1 / p2 p3): rows lerped by fx, then the rows by fz, as the original's
// shufps 0x88 / 0xdd form.
static inline float bilerp(const float p[4], float fx, float fz) {
    float l0 = x86::add(x86::mul(x86::sub(p[1], p[0]), fx), p[0]);
    float l1 = x86::add(x86::mul(x86::sub(p[3], p[2]), fx), p[2]);
    return x86::add(x86::mul(x86::sub(l1, l0), fz), l0);
}

SampleRegs terrain_sample(Guest& g, uint32_t work, uint32_t out_center, uint32_t out_nbr, uint32_t grid, uint32_t scale) {
    // cell indices: floor(scale * (w34, w04, w0c, w3c) + w30), one row start per lane (four rows of the 4x4 window)
    const X v = {{g.u32(work + 0x34), g.u32(work + 0x04), g.u32(work + 0x0c), g.u32(work + 0x3c)}};
    const X idx = floor4i(g, addps(mulps(ld(g, scale), v), X::splat(g.u32(work + 0x30)))).i;
    float w[4][4];   // w[row][col]: rows A B C D
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) w[r][c] = g.f32(grid + idx.u[r] * 4 + 4 * c);
    const float fx = g.f32(work + 0x10), fz = g.f32(work + 0x14);
    const float up[4] = {w[0][1], w[0][2], w[1][1], w[1][2]}, left[4] = {w[1][0], w[1][1], w[2][0], w[2][1]};
    const float right[4] = {w[1][2], w[1][3], w[2][2], w[2][3]}, down[4] = {w[2][1], w[2][2], w[3][1], w[3][2]};
    const float center[4] = {w[1][1], w[1][2], w[2][1], w[2][2]};
    const float h_up = bilerp(up, fx, fz), h_left = bilerp(left, fx, fz), h_right = bilerp(right, fx, fz), h_down = bilerp(down, fx, fz);
    st(g, out_nbr, X{{x86::bits(h_left), x86::bits(h_right), x86::bits(h_up), x86::bits(h_down)}});
    // the center: the row lerp is stored first, read back, then the sample (fx / fz read again, as the original)
    const float fx2 = g.f32(work + 0x10);
    float l0 = x86::add(x86::mul(x86::sub(center[1], center[0]), fx2), center[0]);
    float l1 = x86::add(x86::mul(x86::sub(center[3], center[2]), fx2), center[2]);
    st(g, out_center, X{{x86::bits(l0), x86::bits(l1), x86::bits(l0), x86::bits(l1)}});
    l0 = g.f32(out_center); l1 = g.f32(out_center + 4);
    const float fz2 = g.f32(work + 0x14);
    st(g, out_center, X::fsplat(x86::add(x86::mul(x86::sub(l1, l0), fz2), l0)));
    return {x86::bits(h_down), x86::bits(h_up)};
}

// ---------------------------------------------------------------- 007c51e0 / 007c52f0 cell setup

uint32_t terrain_cell_setup(Guest& g, uint32_t work, uint32_t dims, uint32_t pos, uint32_t cells, uint32_t* edx_out) {
    const X w = subps(ld(g, cells), ld(g, kCellsBias));
    st(g, work + 0x20, w);
    const X a = ld(g, dims), b = ld(g, pos);
    const X t = divps(mulps(addps(mulps(X::splat(g.u32(kHalf)), a), b), ld(g, work + 0x20)), a);
    st(g, work + 0x10, t);
    // (0, 0, 1, 1) through FUN_005452d0, then the floor of t + that (FUN_0042ccd0)
    Frame f(g, 0x10);
    vec_from_lanes(g, f.at(0), g.u32(kOne), g.u32(kOne), 0, 0);
    const X fl = call_floor4f(g, addps(ld(g, f.at(0)), ld(g, work + 0x10)));
    f.end();
    st(g, work, fl);
    st(g, work + 0x10, subps(ld(g, work + 0x10), fl));
    const X lo = ld(g, kZero4);
    const X fl2 = ld(g, work);
    const X m1 = cmpltps(lo, fl2);
    uint32_t s1 = m1.u[3] + m1.u[2] + m1.u[1] + m1.u[0];
    if (s1 == 0xfffffffcu) {
        const X m2 = cmpltps(fl2, ld(g, work + 0x20));
        uint32_t s2 = m2.u[3] + m2.u[2] + m2.u[1] + m2.u[0];
        if (s2 == s1) { *edx_out = s1; return s1 + 5; }
    }
    st(g, work, minps(ld(g, work + 0x20), maxps(lo, fl2)));
    *edx_out = s1;
    return 0;
}

void terrain_cell_offsets(Guest& g, uint32_t work) {
    Frame f(g, 0x10);
    vec_from_lanes(g, f.at(0), g.u32(kOne), g.u32(kOne), g.u32(kMinusOne), g.u32(kMinusOne));   // (-1, -1, 1, 1)
    st(g, work + 0x30, addps(ld(g, work), ld(g, f.at(0))));
    f.end();
    const X v = ld(g, work + 0x30), v2 = ld(g, work + 0x30);
    const X lo = ld(g, kZero4);
    const X over = maxps(lo, subps(shufps(v, v, 0xee), ld(g, work + 0x20)));
    const X under = minps(lo, shufps(v2, v2, 0x44));
    st(g, work + 0x30, subps(subps(ld(g, work + 0x30), over), under));
}

// ---------------------------------------------------------------- 007c5e80 height and normal at a point

uint32_t terrain_height_normal(Guest& g, uint32_t normal, uint32_t terrain, uint32_t point) {
    st(g, normal, ld(g, kUpNormal));
    if (g.u32(terrain + 0x1c) == 0) return 0;
    // frame: 0x00 lock rect, 0x10 scaled position, 0x20 extents, 0x30 cells, 0x40 base neighbours, 0x50 base center, 0x60 deform
    // neighbours, 0x70 deform center, 0x80 stride, 0x90 cell work (0x40), 0xd0 signs / normal scale
    Frame f(g, 0x100);
    const uint32_t F = f.at(0);
    const X p = ld(g, point);
    vec_from_lanes(g, F + 0xd0, g.u32(kMinusOne), g.u32(kOne), g.u32(kMinusOne), g.u32(kOne));
    const uint32_t base = g.u32(terrain + 0x1c);
    const int32_t stride = g.i32(terrain + 0x44bc);
    st(g, F + 0x10, mulps(shufps(p, p, 0x88), ld(g, F + 0xd0)));
    const uint32_t res = g.u32(terrain + 0x240);
    g.call(g.u32(g.u32(res) + 0x34), {res, F, 0, 0x810});   // LockRect(rect, NULL, 0x810)
    const uint32_t bits = g.u32(F + 4);
    if (bits == 0) { f.end(); return 0; }
    vec_from_lanes(g, F + 0x20, g.u32(terrain + 0x44d4), g.u32(terrain + 0x44d0), g.u32(terrain + 0x44d4), g.u32(terrain + 0x44d0));
    vec_from_lanes(g, F + 0x30, g.u32(terrain + 0x44cc), g.u32(terrain + 0x44c8), g.u32(terrain + 0x44cc), g.u32(terrain + 0x44c8));
    st(g, F + 0x80, X::fsplat(float(stride)));
    uint32_t edx;
    if (!terrain_cell_setup(g, F + 0x90, F + 0x20, F + 0x10, F + 0x30, &edx)) {
        g.call(g.u32(g.u32(res) + 0x38), {res});   // UnlockRect
        f.end();
        return 0;
    }
    terrain_cell_offsets(g, F + 0x90);
    terrain_sample(g, F + 0x90, F + 0x50, F + 0x40, base, F + 0x80);
    const X k = X::splat(g.u32(kHeightScale));
    st(g, F + 0x40, mulps(k, ld(g, F + 0x40)));
    st(g, F + 0x50, mulps(k, ld(g, F + 0x50)));
    terrain_sample(g, F + 0x90, F + 0x70, F + 0x60, bits, F + 0x80);
    // height: ((deform - 0.5) * 2 * gain) + base * 2048, into the point's y
    const uint32_t two = g.u32(kDeformScale);
    X h = subps(ld(g, F + 0x70), X::splat(g.u32(kHalf)));
    h = mulps(h, X::splat(two));
    h = mulps(h, X::splat(g.u32(terrain + 0xbc)));
    h = addps(h, ld(g, F + 0x50));
    const X pt = ld(g, point);
    st(g, point, movss(shufps(h, pt, 0xe5), pt));
    // normal: (left - right, normal scale, up - down) of the summed base and deform samples, normalized
    const uint32_t ny = fld_fstp(g.u32(terrain + 0x44ec));
    const float dk = x86::mul(g.f32(terrain + 0xbc), x86::flt(two));
    st(g, F + 0x60, mulps(X::fsplat(dk), ld(g, F + 0x60)));
    g.call(kSplatHelper, {F + 0xd0, ny});   // FUN_0041c6c0: [F + 0xd0] = splat(ny)
    auto f32 = [&](uint32_t o) { return g.f32(F + o); };
    const float nz = x86::add(x86::sub(f32(0x6c), f32(0x68)), x86::sub(f32(0x4c), f32(0x48)));
    X n = shufps(ld(g, F + 0xd0), X::fsplat(nz), 0);
    const float nx = x86::add(x86::sub(f32(0x60), f32(0x64)), x86::sub(f32(0x40), f32(0x44)));
    n = movss(n, X::fsplat(nx));
    X sq = mulps(n, n);
    X s = shufps(sq, sq, 0x99);
    sq = addss(sq, s);
    sq = addss(sq, shuf(s, 0x55));
    st(g, normal, divps(n, sqrtps(shuf(sq, 0))));
    g.call(g.u32(g.u32(res) + 0x38), {res});   // UnlockRect
    f.end();
    return 1;
}


// ---------------------------------------------------------------- 007c6160 terrain triangles

void terrain_triangles(Guest& g, uint32_t q, uint32_t T, uint32_t out, uint32_t countp, int32_t cap, uint32_t extp) {
    g.u32(countp, 0);
    const uint32_t base = g.u32(T + 0x1c);
    if (!base) return;
    const uint32_t one = g.u32(kOne), m1 = g.u32(kMinusOne);
    const X Q = ld(g, q);
    const X Qm = {{Q.u[0], Q.u[1], Q.u[2], g.u32(kZero4 + 12)}};
    const int32_t stride = g.i32(T + 0x44bc);
    const X S = mulps(shufps(Qm, Qm, 0x88), X{{one, m1, one, m1}});
    const uint32_t res = g.u32(T + 0x240);
    Frame f(g, 0x10);
    g.call(g.u32(g.u32(res) + 0x34), {res, f.at(0), 0, 0x810});   // LockRect(rect, NULL, 0x810)
    const uint32_t bits = g.u32(f.at(4));
    f.end();
    if (!bits) return;
    auto unlock = [&] { g.call(g.u32(g.u32(res) + 0x38), {res}); };
    const uint32_t d0 = g.u32(T + 0x44d0), d4 = g.u32(T + 0x44d4), c8 = g.u32(T + 0x44c8), cc = g.u32(T + 0x44cc);
    const X dims = {{d0, d4, d0, d4}}, cells = {{c8, cc, c8, cc}};
    const X ext = ld(g, extp);
    const X W = subps(cells, ld(g, kCellsBias));
    const X nx_ = subps(X::zero(), X{{ext.u[0], ext.u[1], ext.u[2], g.u32(kZero4 + 12)}});
    const X EX = {{nx_.u[0], nx_.u[2], ext.u[0], ext.u[2]}};
    const X half4 = X::splat(g.u32(kHalf));
    X t = divps(mulps(addps(mulps(dims, half4), S), W), dims);
    t = addps(addps(t, EX), X{{0, 0, one, one}});
    const X fl = call_floor4f(g, t);
    const X lo = ld(g, kZero4);
    X mk = cmpltps(fl, lo);
    if (mk.u[3] + mk.u[2] + mk.u[1] + mk.u[0] == 0xfffffffcu) { unlock(); return; }
    mk = cmpltps(W, fl);
    if (mk.u[3] + mk.u[2] + mk.u[1] + mk.u[0] == 0xfffffffcu) { unlock(); return; }
    const X k2 = X::splat(g.u32(kDeformScale));
    const X cl = minps(maxps(fl, lo), subps(cells, k2));
    const X I = floor4i(g, cl).i;
    const uint32_t nz = I.u[3] - I.u[1], nx = I.u[2] - I.u[0];
    const uint32_t n = nz * nx * 2;
    if (int32_t(n) > cap) { unlock(); return; }
    g.u32(countp, n);
    const float hf = g.f32(kHalf);
    const uint32_t K = g.u32(kZeroF);
    const X HALF = {{x86::bits(x86::mul(g.f32(T + 0x44d0), hf)), K, x86::bits(x86::mul(g.f32(T + 0x44d4), hf)), 0}};
    const uint32_t sp = g.u32(T + 0x44e8);
    const X HALFv = {{HALF.u[0], K, HALF.u[2], HALF.u[2]}};
    const X SCALE = {{sp, one, sp, sp}};
    const uint32_t nsp = x86::bits(x86::sub(g.f32(kNegZero), g.f32(T + 0x44e8)));
    const X OFF2 = {{K, K, nsp, nsp}}, OFF1 = {{sp, K, K, K}};
    X row = {{cl.u[0], cl.u[1], cl.u[1], cl.u[1]}};
    if (int32_t(nz) <= 0) { unlock(); return; }
    uint32_t rowbase = I.u[1] * uint32_t(stride), tri = 0;
    for (uint32_t rows = nz; rows != 0; rows--) {
        if (int32_t(nx) > 0) {
            const uint32_t next = rowbase + uint32_t(stride);
            const X SIGNS = {{one, one, g.u32(kMinusOne), g.u32(kMinusOne)}};
            const X k2048 = X::splat(g.u32(kHeightScale));
            uint32_t p = tri * 48 + out, c = I.u[0];
            X P = row;
            for (uint32_t k = nx; k != 0; k--, c++) {
                auto D = [&](uint32_t i) { return g.u32(bits + i * 4); };
                auto B = [&](uint32_t i) { return g.u32(base + i * 4); };
                const X Dv = {{D(rowbase + c), D(rowbase + c + 1), D(next + c), D(next + c + 1)}};
                const X Bv = {{B(rowbase + c), B(rowbase + c + 1), B(next + c), B(next + c + 1)}};
                const X gain = X::splat(g.u32(T + 0xbc));
                const X H = addps(mulps(mulps(subps(Dv, half4), gain), k2), mulps(Bv, k2048));
                const X V00 = mulps(subps(mulps(X{{P.u[0], H.u[0], P.u[2], P.u[3]}}, SCALE), HALFv), SIGNS);
                const X V01 = addps(X{{V00.u[0], H.u[1], V00.u[2], V00.u[3]}}, OFF1);
                const X V10 = addps(X{{V00.u[0], H.u[2], V00.u[2], V00.u[3]}}, OFF2);
                const X V11 = addps(X{{V10.u[0], H.u[3], V10.u[2], V10.u[3]}}, OFF1);
                st(g, p, V10); st(g, p + 0x10, V00); st(g, p + 0x20, V01);
                st(g, p + 0x30, V10); st(g, p + 0x40, V01); st(g, p + 0x50, V11);
                p += 0x60; tri += 2;
                P = addps(P, ld(g, kColStep));
            }
        }
        rowbase += uint32_t(stride);
        row = addps(row, ld(g, kRowStep));
    }
    unlock();
}

}  // namespace collision

// 0069b2f0 (custom: xmm0 in / out): xmm0 = the four lanes floored to int32 (round-to-nearest trick and correction, then
// cvttss2si); returns the lanes also in eax (lane 3) / ecx (lane 1) / edx (lane 2); xmm1..6 as the original leaves them.
REFLEX_PORT_FN(0x0069b2f0, Vec4FloorToInt, "collision",
               OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM0 | OUT_XMM0 << 1 | OUT_XMM0 << 2 | OUT_XMM0 << 3 | OUT_XMM0 << 4 | OUT_XMM0 << 5 | OUT_XMM0 << 6) {
    Guest g{m};
    Floor4 o = floor4i(g, xr(g, 0));
    xw(g, 0, o.i); xw(g, 1, o.xmm1); xw(g, 2, o.xmm2); xw(g, 3, o.xmm3); xw(g, 4, o.xmm4); xw(g, 5, o.xmm5); xw(g, 6, o.xmm6);
    m.r[0] = o.i.u[3]; m.r[1] = o.i.u[1]; m.r[2] = o.i.u[2];
    return m.ret(0);
}

// 0069b400 (custom: xmm0, cdecl destination*): stores xmm0 at the (16-aligned) destination unless it is null; xmm0 keeps
// only its low half (the store goes through two movq). ecx: the security cookie (the check's argument).
REFLEX_PORT_FN(0x0069b400, Vec4StoreAligned, "collision", OUT_EAX | OUT_ECX | OUT_XMM0) {
    Guest g{m};
    uint32_t dst = m.arg(0);
    if (dst) {
        X v = xr(g, 0);
        dst &= ~15u;
        g.u32(dst, v.u[0]); g.u32(dst + 4, v.u[1]); g.u32(dst + 8, v.u[2]); g.u32(dst + 12, v.u[3]);
        xw(g, 0, X{{v.u[2], v.u[3], 0, 0}});
    }
    m.r[0] = dst; m.r[1] = g.u32(kCookie);
    return m.ret(0);
}

// 007c5440 (custom: ESI cell work, EDI center out; stack: neighbours out*, grid*, scale*; cdecl): the 4x4 window of the
// height grid around the cell, bilinear samples at the cell (EDI, splat) and its four neighbours (left, right, up, down).
REFLEX_PORT_FN(0x007c5440, TerrainSampleWindow, "collision", OUT_EAX | OUT_ECX | OUT_EDX) {
    Guest g{m};
    SampleRegs r = terrain_sample(g, m.r[6], m.r[7], m.arg(0), m.arg(1), m.arg(2));
    m.r[0] = r.eax; m.r[1] = g.u32(kCookie); m.r[2] = r.edx;
    return m.ret(0);
}

// 007c51e0 (custom: ESI cell work, ECX extents*, EDX position*; stack: cell counts*; cdecl): grid coordinates of the
// position, their floor and fraction; 1 if inside the grid, else 0 with the cell clamped.
REFLEX_PORT_FN(0x007c51e0, TerrainCellSetup, "collision", OUT_EAX | OUT_EDX) {
    Guest g{m};
    uint32_t edx;
    uint32_t r = terrain_cell_setup(g, m.r[6], m.r[1], m.r[2], m.arg(0), &edx);
    m.r[0] = r; m.r[1] = g.u32(kCookie); m.r[2] = edx;
    return m.ret(0);
}

// 007c52f0 (custom: ESI cell work; cdecl): the window origin (cell + (1, -1, 1, -1)) clamped to the grid.
REFLEX_PORT_FN(0x007c52f0, TerrainCellOffsets, "collision", 0) {
    Guest g{m};
    terrain_cell_offsets(g, m.r[6]);
    m.r[1] = g.u32(kCookie);
    return m.ret(0);
}

// 007c5e80 (thiscall: ECX normal out*; terrain*, point* (in / out); RET 8): the terrain height (base * 2048 plus the locked
// deformation surface) put into point.y, and the surface normal; 0 without base grid, lock bits or outside the grid.
REFLEX_PORT_FN(0x007c5e80, TerrainHeightNormal, "collision", OUT_EAX) {
    Guest g{m};
    const uint32_t normal = m.r[1], terrain = m.arg(0), point = m.arg(1);
    uint32_t r = terrain_height_normal(g, normal, terrain, point);
    m.r[0] = r;
    return m.ret(8);
}

// 007c6160 (custom: ECX query point*; stack: terrain*, triangles out*, count out*, capacity, extent*; RET 0x14): the terrain
// triangles (two per grid cell, base * 2048 plus the locked deformation surface) of the cells around the query point.
REFLEX_PORT_FN(0x007c6160, TerrainTriangles, "collision", 0) {
    Guest g{m};
    terrain_triangles(g, m.r[1], m.arg(0), m.arg(1), m.arg(2), int32_t(m.arg(3)), m.arg(4));
    return m.ret(0x14);
}

// 007cda90 (custom: EAX float list, xmm0 value): appends xmm0's lane 0 (count at +0x1e00).
REFLEX_PORT_FN(0x007cda90, FloatList_Push, "collision", OUT_ECX) {
    Guest g{m};
    const uint32_t list = m.r[0], n = g.u32(list + 0x1e00);
    g.u32(list + n * 4, xr(g, 0).u[0]);
    g.u32(list + 0x1e00, g.u32(list + 0x1e00) + 1);
    m.r[1] = n;
    return m.ret(0);
}

// 007cda50 (custom: EAX pair list; stack: two floats; RET 8): appends the pair (count at +0x200).
REFLEX_PORT_FN(0x007cda50, FloatPairList_Push, "collision", OUT_ECX | OUT_XMM0) {
    Guest g{m};
    const uint32_t list = m.r[0], n = g.u32(list + 0x200), at = list + n * 8;
    g.u32(at, m.arg(0));
    g.u32(at + 4, m.arg(1));
    g.u32(list + 0x200, g.u32(list + 0x200) + 1);
    xw(g, 0, X{{m.arg(1), 0, 0, 0}});
    m.r[1] = at;
    return m.ret(8);
}

// 007c5380 TerrainSampleCell (custom: ESI grid, EDI cell work, ECX scale*; stack: out*; cdecl): the bilinear sample of the
// cell's 2x2 corners (indices floor((w.y, w.y, w.w, w.w) * scale + (w.x, w.z, w.x, w.z))), splat into out.
REFLEX_PORT_FN(0x007c5380, TerrainSampleCell, "collision", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM0 | OUT_XMM0 << 1) {
    Guest g{m};
    const uint32_t grid = m.r[6], work = m.r[7], out = m.arg(0);
    const X w = ld(g, work);
    const Floor4 f = floor4i(g, addps(mulps(shufps(w, w, 0xf5), ld(g, m.r[1])), shufps(w, w, 0x88)));
    const X& I = f.i;
    auto at = [&](uint32_t i) { return g.u32(grid + i * 4); };
    const uint32_t c3 = at(I.u[3]), c2 = at(I.u[2]), c1 = at(I.u[1]), c0 = at(I.u[0]);
    const float p[4] = {x86::flt(c0), x86::flt(c1), x86::flt(c2), x86::flt(c3)};
    const float fx = g.f32(work + 0x10);
    const float l0 = x86::add(x86::mul(x86::sub(p[1], p[0]), fx), p[0]);
    const float l1 = x86::add(x86::mul(x86::sub(p[3], p[2]), fx), p[2]);
    const float fz = g.f32(work + 0x14);
    const X h = X::fsplat(x86::add(x86::mul(x86::sub(l1, l0), fz), l0));
    st(g, out, h);
    xw(g, 0, h); xw(g, 1, X::fsplat(l0)); xw(g, 2, X::fsplat(fz)); xw(g, 3, X{{c1, c3, 0, 0}});
    xw(g, 4, f.xmm4); xw(g, 5, f.xmm5); xw(g, 6, f.xmm6);
    m.r[0] = out; m.r[1] = I.u[1]; m.r[2] = I.u[0];
    return m.ret(0);
}
