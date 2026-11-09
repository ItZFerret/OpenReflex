/* phys_terrain_ray.c -- the terrain segment query (_re/notes/terrain-ray-original-math.md): 007c6cc0 clips the segment to the
 * grid (grid units: world / spacing, z negated), 007c73e0 walks it through the 16 x 16 coarse cells (007c7350 rejects a cell
 * whose min / max heights the segment's height range cannot reach), 007c83b0 walks a coarse cell's unit cells and 007c7d10
 * intersects the segment with a cell's bilinear height patch (base heights * 2048), giving the hit point and normal in world
 * space. The walks count their steps on the x87 (cells = round(|distance| / size) through _CIfmod / _copysign / _ftol2_sse);
 * these functions run on the main thread (x87 control word 0x027f: 53-bit precision).
 *
 * Register results the callers read: each function reports the ECX / EDX the original leaves in regs[0] / regs[1] (the last
 * callee's, or the cell intersection's grid indices). */
#include "phys_terrain.h"
#include "phys_crt.h"
#include "crt.h"

#define RAY_X87_BITS 53                       /* the main thread's precision control (CRT_CW_MAIN) */
#define CALL_AT(cur, nstack, stmt) do { rx_set_sp((cur) - 4u * (nstack) - 4u); stmt; rx_set_sp(cur); } while (0)

/* Float_Abs 00401000: the sign bit cleared */
static float abs_bits(float x) { return rx_bf(rx_fb(x) & 0x7fffffffu); }

/* the exe's cell count: v = d - 0.49999999999 (SSE double, 0x977558), then _ftol2_sse(_copysign(0.5, v) + v) with the add on the
 * x87 -- round to nearest with halves away from zero, just below the half */
static int32_t round_cells_v(float d, double* v_out) {
    const double v = (double)d - 0.49999999999;
    if (v_out) *v_out = v;
    const rx_f80 s = rx_f80_add(rx_f80_from_f64(crt_copysign(0.5 /* 0x9774e8 */, v)), rx_f80_from_f64(v), RAY_X87_BITS, 0);
    return phys_ftol2_sse(s, RAY_X87_BITS);
}
static int32_t round_cells(float d) { return round_cells_v(d, 0); }

/* x - fmod(x, period) as the exe computes it: _CIfmod (st1 = x, st0 = period) stored as a float, then x - that (fsubr) */
static float cell_origin_r(float x, double period, float* r_out) {
    const float r = rx_f80_to_f32(crt_ci_fmod(rx_f80_from_f32(x), rx_f80_from_f64(period), CRT_CW_MAIN), 0);
    if (r_out) *r_out = r;
    return rx_f80_to_f32(rx_f80_sub(rx_f80_from_f32(x), rx_f80_from_f32(r), RAY_X87_BITS, 0), 0);
}
static float cell_origin(float x, double period) { return cell_origin_r(x, period, 0); }

/* ---- 007c7d10 TerrainSegment_CellIntersect (EAX = base grid; stack terrain*, hit out*, normal out*, segment*; RET 0x10): the
 * segment (x0, y0, z0) -> (x1, y1, z1) (+0 / +0x10, grid units) against the unit cell around its midpoint: the cell's corners
 * (x, z) .. (x + 1, z + 1) (clamped to the grid, heights base * 2048: h00, h10 (x + 1), h01 (z + 1), h11) give the bilinear
 * patch; the segment's height range must overlap the corners' [min, max]. The patch height along the segment minus the segment's
 * height is A t^2 + B t + C in the segment parameter t (local coordinates relative to the rounded cell corner fx, fz); the
 * smallest root in [0, 1] (A ~ 0: the linear root) is the hit: hit point (world space: x * spacing - size_x / 2, y, -(z *
 * spacing - size_z / 2)) and the patch normal (unit, z negated). Returns 1 on a hit. */
uint32_t phys_terrain_cell_intersect(rx_addr base, rx_addr terrain, rx_addr hit, rx_addr normal, rx_addr seg, uint32_t regs[2]) {
    if (base == 0) return 0;
    const float x0 = F32(seg, 0), y0 = F32(seg, 4), z0 = F32(seg, 8);
    const float x1 = F32(seg, 0x10), y1 = F32(seg, 0x14), z1 = F32(seg, 0x18);
    const float mx = phys_mul(phys_add(x0, x1), 0.5f), mz = phys_mul(phys_add(z1, z0), 0.5f);
    const int32_t xi = rx_cvtt(mx), zi = rx_cvtt(mz);
    const float fx = (float)round_cells(mx), fz = (float)round_cells(mz);
    const int32_t stride = I32(terrain, 0x44bc), rows = I32(terrain, 0x44c0);
    const int32_t xn = xi + 1 < stride - 1 ? xi + 1 : stride - 1, zn = zi + 1 < rows - 1 ? zi + 1 : rows - 1;
    const int32_t i00 = stride * zi + xi, i10 = stride * zi + xn, i01 = stride * zn + xi, i11 = stride * zn + xn;
    const float h00 = phys_mul(F32(base, 4u * (uint32_t)i00), 2048.0f), h10 = phys_mul(F32(base, 4u * (uint32_t)i10), 2048.0f);
    const float h01 = phys_mul(F32(base, 4u * (uint32_t)i01), 2048.0f), h11 = phys_mul(F32(base, 4u * (uint32_t)i11), 2048.0f);
    regs[0] = (uint32_t)i10; regs[1] = (uint32_t)i00;
    float hmax = h00 > h10 ? h00 : h10, hmin = h10 > h00 ? h00 : h10;
    hmax = hmax > h01 ? hmax : h01; hmin = h01 > hmin ? hmin : h01;
    hmax = hmax > h11 ? hmax : h11; hmin = h11 > hmin ? hmin : h11;
    if (!(hmax >= y0) && !(hmax >= y1)) return 0;
    if (!(y0 >= hmin) && !(y1 >= hmin)) return 0;
    /* the quadratic's coefficients, in the exe's operation order */
    const float lx0 = phys_sub(x0, fx), lx1 = phys_sub(x1, fx), lz0 = phys_sub(z0, fz), lz1 = phys_sub(z1, fz);
    float a = phys_add(phys_sub(phys_sub(h00, h10), h01), h11);
    a = phys_mul(phys_mul(a, phys_sub(lx0, lx1)), phys_sub(lz0, lz1));
    const float A = phys_sub(-0.0f, a);
    const float x0h11 = phys_mul(lx0, h11), z0x2 = phys_mul(lz0, 2.0f), z0h01 = phys_mul(lz0, h01), d01 = phys_sub(h01, h11);
    const float z0m1 = phys_sub(lz0, 1.0f), x0m1 = phys_sub(lx0, 1.0f);
    float b = phys_sub(phys_sub(z0h01, y0), phys_mul(phys_sub(phys_mul(lx0, 2.0f), lx1), phys_mul(d01, lz0)));
    b = phys_add(b, y1);
    b = phys_add(b, phys_mul(phys_sub(phys_mul(x0m1, h01), x0h11), lz1));
    float t7 = phys_sub(lx1, phys_mul(phys_add(lx1, 1.0f), lz0));
    t7 = phys_add(t7, phys_mul(phys_sub(phys_sub(z0x2, 1.0f), lz1), lx0));
    t7 = phys_mul(phys_add(t7, lz1), h00);
    const float b00 = phys_add(b, t7);
    float b10 = phys_mul(phys_add(phys_sub(1.0f, z0x2), lz1), lx0);
    b10 = phys_mul(phys_add(b10, phys_mul(z0m1, lx1)), h10);
    const float B = phys_add(b00, b10);
    float c = phys_sub(phys_add(phys_mul(phys_mul(lx0, h10), z0m1), y0), z0h01);
    c = phys_add(c, phys_mul(phys_mul(lx0, h01), lz0));
    c = phys_sub(c, phys_mul(x0h11, lz0));
    const float C = phys_add(c, phys_mul(phys_sub(phys_add(x0m1, lz0), phys_mul(lz0, lx0)), h00));
    float t;
    if (abs_bits(A) > 9.99999975e-05f /* 0x9775a4 */) {
        const float two_a = phys_mul(A, 2.0f), neg_b = phys_sub(-0.0f, B);
        const float disc = phys_sub(phys_mul(neg_b, neg_b), phys_mul(phys_mul(two_a, 2.0f), C));
        float t1 = disc >= 0.0f ? phys_div(phys_add(phys_sqrt(disc), neg_b), two_a) : 2.0f;
        float t2 = disc >= 0.0f ? phys_div(phys_sub(neg_b, phys_sqrt(disc)), two_a) : 2.0f;
        if (!(t1 >= 0.0f)) t1 = 2.0f;
        if (!(t2 >= 0.0f)) t2 = 2.0f;
        t = t2 > t1 ? t1 : t2;
    } else {
        t = phys_sub(-0.0f, phys_div(C, B));
    }
    if (!(t >= 0.0f) || !(1.0f >= t)) return 0;
    /* the hit point (grid units) and the patch normal */
    const float hx = phys_add(x0, phys_mul(phys_sub(x1, x0), t));
    const float hy = phys_add(y0, phys_mul(phys_sub(y1, y0), t));
    const float hz = phys_add(z0, phys_mul(phys_sub(z1, z0), t));
    SF32(hit, 8, hz); SF32(hit, 0, hx); SF32(hit, 4, hy);
    const float v = phys_sub(hz, fz), u = phys_sub(hx, fx);
    const float nx = phys_add(phys_sub(phys_add(phys_mul(phys_sub(v, 1.0f), h10), h00), phys_mul(v, h00)), phys_mul(d01, v));
    const float nz = phys_add(phys_sub(phys_add(phys_mul(phys_sub(u, 1.0f), h01), h00), phys_mul(u, h00)),
                              phys_mul(phys_sub(h10, h11), u));
    const float len2 = phys_add(phys_add(phys_mul(nx, nx), 1.0f), phys_mul(nz, nz));
    if (len2 > 0.0f) {
        const float inv = phys_div(1.0f, phys_sqrt(len2));
        SF32(normal, 4, inv);
        SF32(normal, 0, phys_mul(inv, nx));
        SF32(normal, 8, phys_mul(inv, nz));
    }
    /* to world space */
    const float spacing = F32(terrain, 0x44e8);
    const float half_x = phys_mul(F32(terrain, 0x44d0), 0.5f), half_z = phys_mul(F32(terrain, 0x44d4), 0.5f);
    const float wx = phys_sub(phys_mul(F32(hit, 0), spacing), half_x);
    const float wz = phys_sub(phys_mul(F32(hit, 8), spacing), half_z);
    SF32(hit, 4, phys_sub(F32(hit, 4), 0.0f));
    SF32(hit, 0, wx);
    SF32(hit, 8, phys_sub(-0.0f, wz));
    SF32(normal, 8, phys_sub(-0.0f, F32(normal, 8)));
    regs[0] = terrain;
    return 1;
}
RX_FUNCTION(0x007c7d10, phys_terrain_cell_intersect)

/* the segment record the walks pass on: (ax, ay, az) at +0, (bx, by, bz) at +0x10 */
static void put_segment(rx_addr seg, float ax, float ay, float az, float bx, float by, float bz) {
    SF32(seg, 0, ax); SF32(seg, 4, ay); SF32(seg, 8, az);
    SF32(seg, 0x10, bx); SF32(seg, 0x14, by); SF32(seg, 0x18, bz);
}
/* the segment's height at a coordinate c along x or z: ((c - origin) * dir.y) / dir.c + origin.y */
static float height_along(float c, float origin_c, float origin_y, float dir_y, float dir_c) {
    return phys_add(phys_div(phys_mul(phys_sub(c, origin_c), dir_y), dir_c), origin_y);
}

/* a walk's state: its frame, the piece record it passes on and the query's outputs */
struct walk {
    rx_addr L, seg, base, terrain, hit, normal, dir3, dir2;
    uint32_t* regs;
    uint32_t ebp;                             /* the walking function's frame pointer (its entry ESP - 4) */
};
/* the four stack arguments a walk pushes for its piece call (the last first: a4 at L - 4) and the call's return address at L - 0x14 */
static void walk_pushes(rx_addr L, uint32_t ret, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    SU32(L - 4u, 0, a4); SU32(L - 8u, 0, a3); SU32(L - 0xcu, 0, a2); SU32(L - 0x10u, 0, a1); SU32(L - 0x14u, 0, ret);
}
/* one unit-cell piece of the fine walk: TerrainSegment_CellIntersect, called from the call site whose return address is `ret`
 * with its pushes (terrain, hit, normal, the segment record) */
static uint32_t cell_piece(const struct walk* w, uint32_t ret, float ax, float ay, float az, float bx, float by, float bz) {
    uint32_t r;
    put_segment(w->seg, ax, ay, az, bx, by, bz);
    walk_pushes(w->L, ret, w->terrain, w->hit, w->normal, w->seg);
    CALL_AT(w->L, 4, r = phys_terrain_cell_intersect(w->base, w->terrain, w->hit, w->normal, w->seg, w->regs));
    return r;
}
/* one coarse-cell piece: the coarse height reject, then the fine walk (piece start +0, end +0x10) from the call site whose return
 * address is `ret` (its pushes: terrain = the caller's EBX, hit, normal, the unit (x, z) direction; the fine walk's saved EBX /
 * EBP are this frame's) */
static uint32_t coarse_piece(const struct walk* w, uint32_t ret, float ax, float ay, float az, float bx, float by, float bz) {
    uint32_t r;
    put_segment(w->seg, ax, ay, az, bx, by, bz);
    w->regs[0] = w->seg; w->regs[1] = U32(w->terrain, 0xd4);
    if (!phys_terrain_segment_coarse_reject(w->seg, w->terrain)) return 0;
    walk_pushes(w->L, ret, w->terrain, w->hit, w->normal, w->dir2);
    CALL_AT(w->L, 4, r = phys_terrain_fine_traversal(w->seg, w->seg + 0x10, w->dir3, w->terrain, w->hit, w->normal, w->dir2, w->regs));
    phys_terrain_fine_traversal_saved_regs(w->L - 0x14u, w->terrain, w->ebp);
    return r;
}

/* ---- 007c83b0 TerrainSegment_FineTraversal (ESI = P, EAX = Q: the part of the segment inside one coarse cell; EDI = the unit
 * direction (x, y, z); stack terrain*, hit*, normal*, the unit (x, z) direction*; RET 0x10): walks the unit cells from P to Q --
 * along x one cell boundary at a time, and within each x step along z -- and intersects each piece with its cell
 * (TerrainSegment_CellIntersect on the base grid terrain+0x1c). Returns 1 at the first hit. Frame: push ebp; and esp, -16; sub
 * esp, 0x9c; push ebx (segment record at +0x40). The prefetch loop over the cells' rows has no effect.
 * The frame is left as the original leaves it (w-parity4): later code reads this stack uninitialized (a CylinderVsCylinder's
 * 0081c590 took its Newton step from L + 0x20 after the water pass's terrain query ran at that depth: core/notes/phys.md "The
 * frame of the fine terrain walk"). Slots (L = the frame base after push ebx): +0x08 the fmod remainders, then the x / z counts
 * left (floats); +0x0c min z cell (int), then the z cell boundary; +0x10 / +0x14 the x / z cell boundaries; +0x18 / +0x1c P.x /
 * P.z, then +0x18 the z count left; +0x20 the x count's double (|Q.x - cx| - 0.49999999999), then the z pieces' end (x, z) at
 * +0x20 / +0x24; +0x2c min x cell (int), then x per z; +0x30 / +0x34 Q - P (x, z), then +0x34 the z boundary; +0x38 / +0x3c
 * the steps; +0x40 the segment record; +0x60 / +0x64 the x piece's end, +0x68 / +0x6c its start; +0x78 / +0x7c Q.x / Q.z;
 * +0x80 the z counts' double; +0x8c z per x; +0x90 / +0x94 the z walk's start. The piece calls' pushes and return addresses
 * (007c8753, 007c8822, 007c899d, 007c8a74) lie below L; the CRT calls' frames (_CIfmod, _copysign, _ftol2_sse) under the last
 * piece call's are not modelled. The caller writes the saved EBX / EBP (phys_terrain_fine_traversal_saved_regs). */
uint32_t phys_terrain_fine_traversal(rx_addr P, rx_addr Q, rx_addr dir3, rx_addr terrain, rx_addr hit, rx_addr normal,
                                     rx_addr dir2, uint32_t regs[2]) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x9c - 4, seg = L + 0x40;
    rx_set_sp(L);
    const struct walk w = {L, seg, U32(terrain, 0x1c), terrain, hit, normal, 0, 0, regs, sp0 - 4};
    uint32_t r = 0;
#define CELL(ret, ax, ay, az, bx, by, bz) (r = cell_piece(&w, ret, ax, ay, az, bx, by, bz))
    const float px0 = F32(P, 0), py0 = F32(P, 4), pz0 = F32(P, 8), qx = F32(Q, 0), qz = F32(Q, 8);
    const float dx3 = F32(dir3, 0), dy3 = F32(dir3, 4), dz3 = F32(dir3, 8);
    SU32(L, 0x2c, (uint32_t)rx_cvtt(qx > px0 ? px0 : qx));     /* the prefetch loop's first cell */
    SU32(L, 0x0c, (uint32_t)rx_cvtt(qz > pz0 ? pz0 : qz));
    const float z_per_x = phys_div(F32(dir2, 4), F32(dir2, 0)), x_per_z = phys_div(F32(dir2, 0), F32(dir2, 4));
    const float ddx = phys_sub(qx, px0), ddz = phys_sub(qz, pz0);
    SF32(L, 0x2c, x_per_z); SF32(L, 0x18, px0); SF32(L, 0x30, ddx); SF32(L, 0x1c, pz0);
    SF32(L, 0x78, qx); SF32(L, 0x7c, qz); SF32(L, 0x8c, z_per_x); SF32(L, 0x34, ddz);
    const float minus1 = F32(0, 0x009cd9e8);
    float rem;
    const float origin_x = cell_origin_r(px0, 1.0, &rem);
    SF32(L, 0x08, rem);
    float cx = phys_add(ddx >= 0.0f ? 0.0f : 1.0f, origin_x);
    const float step_x = ddx >= 0.0f ? 1.0f : minus1;
    SF32(L, 0x10, cx); SF32(L, 0x38, step_x);
    const float origin_z = cell_origin_r(pz0, 1.0, &rem);
    SF32(L, 0x08, rem);
    float cz = phys_add(ddz >= 0.0f ? 0.0f : 1.0f, origin_z);
    const float step_z = ddz >= 0.0f ? 1.0f : minus1;
    SF32(L, 0x14, cz); SF32(L, 0x3c, step_z);
    SF32(L, 0x68, px0); SF32(L, 0x6c, pz0);
    double v;
    float nx = (float)round_cells_v(abs_bits(phys_sub(qx, cx)), &v);
    SF64(L, 0x20, v);
    SF32(L, 0x34, cz); SF32(L, 0x08, nx);
    float px = px0, pz = pz0;
    while (nx > 0.0f) {
        cx = phys_add(cx, step_x);
        const float dxs = phys_sub(cx, px);
        const float bz = phys_add(phys_mul(dxs, z_per_x), pz), bx = phys_add(px, dxs);
        SF32(L, 0x10, cx); SF32(L, 0x64, bz); SF32(L, 0x60, bx); SF32(L, 0x90, px); SF32(L, 0x94, pz);
        float sx = px, sz = pz;
        float nz = (float)round_cells_v(abs_bits(phys_sub(bz, cz)), &v);
        SF64(L, 0x80, v);
        SF32(L, 0x18, nz);
        while (nz > 0.0f) {
            cz = phys_add(cz, step_z);
            const float dzs = phys_sub(cz, sz);
            const float ez = phys_add(sz, dzs), ex = phys_add(phys_mul(dzs, x_per_z), sx);
            SF32(L, 0x0c, cz); SF32(L, 0x24, ez); SF32(L, 0x20, ex);
            if (CELL(0x007c8753u, sx, height_along(sz, pz0, py0, dy3, dz3), sz, ex, height_along(ez, pz0, py0, dy3, dz3), ez)) goto done;
            nz = phys_sub(nz, 1.0f);
            SF32(L, 0x18, nz); SF32(L, 0x34, cz);
            sx = ex; sz = ez;
        }
        if (CELL(0x007c8822u, sx, height_along(sx, px0, py0, dy3, dx3), sz, bx, height_along(bx, px0, py0, dy3, dx3), bz)) goto done;
        nx = phys_sub(nx, 1.0f);
        SF32(L, 0x08, nx); SF32(L, 0x68, bx); SF32(L, 0x6c, bz);
        px = bx; pz = bz;
    }
    {
        float sx = px, sz = pz;
        SF32(L, 0x90, px); SF32(L, 0x94, pz);
        float nz = (float)round_cells_v(abs_bits(phys_sub(qz, cz)), &v);
        SF64(L, 0x80, v);
        SF32(L, 0x08, nz);
        while (nz > 0.0f) {
            cz = phys_add(cz, step_z);
            const float dzs = phys_sub(cz, sz);
            const float ex = phys_add(phys_mul(dzs, x_per_z), sx), ez = phys_add(dzs, sz);
            SF32(L, 0x0c, cz); SF32(L, 0x24, ez); SF32(L, 0x20, ex);
            if (CELL(0x007c899du, sx, height_along(sz, pz0, py0, dy3, dz3), sz, ex, height_along(ez, pz0, py0, dy3, dz3), ez)) goto done;
            nz = phys_sub(nz, 1.0f);
            SF32(L, 0x08, nz);
            sx = ex; sz = ez;
        }
        CELL(0x007c8a74u, sx, height_along(sx, px0, py0, dy3, dx3), sz, qx, height_along(qx, px0, py0, dy3, dx3), qz);
    }
#undef CELL
done:
    rx_set_sp(sp0);
    return r != 0;
}
RX_FUNCTION(0x007c83b0, phys_terrain_fine_traversal)
/* 007c83b0's prologue words: the caller's EBP at entry ESP - 4 and its EBX at the frame base (sp0 = the entry ESP); written by the
 * caller (TerrainSegment_CoarseTraversal: EBX = its terrain argument, EBP its frame; the oracle adapter: the machine's) */
void phys_terrain_fine_traversal_saved_regs(rx_addr sp0, uint32_t ebx, uint32_t ebp) {
    SU32(sp0 - 4u, 0, ebp);
    SU32(((sp0 - 4) & ~15u) - 0x9c - 4, 0, ebx);
}

/* ---- 007c73e0 TerrainSegment_CoarseTraversal (EAX = E; stack terrain*, hit*, normal*, S*; RET 0x10): the segment S -> E (grid
 * units, clipped) walked through the 16 x 16 coarse cells as the fine walk does through unit cells (cell size 16, the counts
 * scaled by 1/16); each piece whose height range passes TerrainSegment_CoarseReject goes to the fine walk with the segment's
 * unit direction (x, y, z) and its unit (x, z) direction (each normalized only when its length is not zero). Returns 1 at the
 * first hit. Frame: push ebp; and esp, -16; sub esp, 0xd4; push ebx / esi / edi (unit (x, z) direction +0x48, unit direction
 * +0x50, piece +0x70 / +0x80). The cell size 16 is read from the writable float 0x9ddfdc. */
uint32_t phys_terrain_coarse_traversal(rx_addr E, rx_addr terrain, rx_addr hit, rx_addr normal, rx_addr S, uint32_t regs[2]) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0xd4 - 12;
    rx_set_sp(L);
    const rx_addr dir2 = L + 0x48, dir3 = L + 0x50, box = L + 0x70;
    const float sx0 = F32(S, 0), sy0 = F32(S, 4), sz0 = F32(S, 8), ex0 = F32(E, 0), ez0 = F32(E, 8);
    const float dx = phys_sub(ex0, sx0), dz = phys_sub(ez0, sz0), dy = phys_sub(F32(E, 4), sy0);
    /* the directions */
    const float len2_xz = phys_add(phys_mul(dx, dx), phys_mul(dz, dz));
    float ux = dx, uz = dz;
    if (!(len2_xz == 0.0f)) {
        const float inv = phys_div(1.0f, phys_sqrt(len2_xz));
        ux = phys_mul(dx, inv); uz = phys_mul(dz, inv);
    }
    SF32(dir2, 0, ux); SF32(dir2, 4, uz);
    const float dz3 = phys_sub(F32(E, 8), F32(S, 8));
    const float len2 = phys_add(phys_add(phys_mul(dx, dx), phys_mul(dy, dy)), phys_mul(dz3, dz3));
    float nx3 = dx, ny3 = dy, nz3 = dz3;
    if (len2 > 0.0f) {
        const float inv = phys_div(1.0f, phys_sqrt(len2));
        nx3 = phys_mul(dx, inv); ny3 = phys_mul(dy, inv); nz3 = phys_mul(dz3, inv);
    }
    SF32(dir3, 0, nx3); SF32(dir3, 4, ny3); SF32(dir3, 8, nz3);
    const float z_per_x = phys_div(uz, ux), x_per_z = phys_div(ux, uz);
    const float size = F32(0, 0x009ddfdc);
    float cx = phys_add(dx >= 0.0f ? 0.0f : size, cell_origin(sx0, 16.0 /* 0x977a58 */));
    const float step_x = dx >= 0.0f ? size : -16.0f /* 0x977a50 */;
    float cz = phys_add(dz >= 0.0f ? 0.0f : size, cell_origin(sz0, 16.0));
    const float step_z = dz >= 0.0f ? F32(0, 0x009ddfdc) : -16.0f;
    uint32_t r = 0;
    const struct walk w = {L, box, 0, terrain, hit, normal, dir3, dir2, regs, sp0 - 4};
#define PIECE(ret, ax, ay, az, bx, by, bz) (r = coarse_piece(&w, ret, ax, ay, az, bx, by, bz))
    float px = sx0, pz = sz0;
    for (float nx = (float)round_cells(phys_mul(abs_bits(phys_sub(ex0, cx)), 0.0625f)); nx > 0.0f; nx = phys_sub(nx, 1.0f)) {
        cx = phys_add(cx, step_x);
        const float dxs = phys_sub(cx, px);
        const float bz = phys_add(phys_mul(dxs, z_per_x), pz), bx = phys_add(px, dxs);
        float qx = px, qz = pz;
        for (float nz = (float)round_cells(phys_mul(abs_bits(phys_sub(bz, cz)), 0.0625f)); nz > 0.0f; nz = phys_sub(nz, 1.0f)) {
            cz = phys_add(cz, step_z);
            const float dzs = phys_sub(cz, qz);
            const float ex = phys_add(phys_mul(dzs, x_per_z), qx), ez = phys_add(qz, dzs);
            if (PIECE(0x007c7911u, qx, height_along(qz, sz0, sy0, ny3, nz3), qz, ex, height_along(ez, sz0, sy0, ny3, nz3), ez)) goto done;
            qx = ex; qz = ez;
        }
        if (PIECE(0x007c7a14u, qx, height_along(qx, sx0, sy0, ny3, nx3), qz, bx, height_along(bx, sx0, sy0, ny3, nx3), bz)) goto done;
        px = bx; pz = bz;
    }
    {
        float qx = px, qz = pz;
        for (float nz = (float)round_cells(phys_mul(abs_bits(phys_sub(ez0, cz)), 0.0625f)); nz > 0.0f; nz = phys_sub(nz, 1.0f)) {
            cz = phys_add(cz, step_z);
            const float dzs = phys_sub(cz, qz);
            const float ex = phys_add(phys_mul(dzs, x_per_z), qx), ez = phys_add(dzs, qz);
            if (PIECE(0x007c7be1u, qx, height_along(qz, sz0, sy0, ny3, nz3), qz, ex, height_along(ez, sz0, sy0, ny3, nz3), ez)) goto done;
            qx = ex; qz = ez;
        }
        PIECE(0x007c7ceeu, qx, height_along(qx, sx0, sy0, ny3, nx3), qz, ex0, height_along(ex0, sx0, sy0, ny3, nx3), ez0);
    }
#undef PIECE
done:
    rx_set_sp(sp0);
    return r != 0;
}
RX_FUNCTION(0x007c73e0, phys_terrain_coarse_traversal)

/* comiss-style flags of the clipping test: 1.0 / 0.0 */
static float flag_below(float a, float b) { return a >= b ? 0.0f : 1.0f; }          /* a < b or unordered */
static float flag_at_or_above(float a, float b) { return a >= b ? 1.0f : 0.0f; }
static float flag_both(float sum) { return phys_sub(sum, 1.5f) >= 0.0f ? 1.0f : 0.0f; }   /* both endpoints out (0x9322a4) */
static float clip_zero(float v) { return v > 0.0f ? 0.0f : v; }                      /* the overshoot below 0 */

/* ---- 007c6cc0 TerrainSegmentQuery (ESI = terrain, ECX = start*, EAX = end*; stack hit out*, normal out*; RET 8): the segment
 * against the height field. Needs terrain+0x14 (validity) and the coarse bounds +0xd4. The endpoints go to grid units
 * (x = (size_x / 2 + x) / spacing, z = (size_z / 2 - z) / spacing); a segment with both endpoints beyond the same grid edge
 * misses; the end is nudged 0.01 further along x and z; a segment with an endpoint outside the grid is clipped to it (x
 * edges, then z edges, moving the other coordinates along the segment's direction) and misses when it shrinks below 0.001
 * in x and z, then clamped to [0, cells - 1]. Returns TerrainSegment_CoarseTraversal's hit (1 / 0). Frame: push ebp; and
 * esp, -16; sub esp, 0x80 (clipped start +0x60, end +0x70). */
uint32_t phys_terrain_segment_query(rx_addr terrain, rx_addr start, rx_addr end, rx_addr hit, rx_addr normal, uint32_t regs[2]) {
    if (U32(terrain, 0x14) == 0 || U32(terrain, 0xd4) == 0) return 0;
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x80;
    rx_set_sp(L);
    const rx_addr seg0 = L + 0x60, seg1 = L + 0x70;
    const float half_x = phys_mul(F32(terrain, 0x44d0), 0.5f), half_z = phys_mul(F32(terrain, 0x44d4), 0.5f);
    const float max_x = phys_sub(F32(terrain, 0x44c8), 1.0f), max_z = phys_sub(F32(terrain, 0x44cc), 1.0f);
    const float spacing = F32(terrain, 0x44e8), sy = F32(start, 4), ey = F32(end, 4);
    const float gx0 = phys_div(phys_add(half_x, F32(start, 0)), spacing);
    float gz1 = phys_div(phys_add(half_z, phys_sub(-0.0f, F32(end, 8))), spacing);
    const float gz0 = phys_div(phys_add(half_z, phys_sub(-0.0f, F32(start, 8))), spacing);
    float gx1 = phys_div(phys_add(half_x, F32(end, 0)), spacing);
    /* which endpoints lie beyond each grid edge (1.0 / 0.0), and how many edges have both beyond them */
    const float a = phys_add(flag_below(gx1, 0.0f), flag_below(gx0, 0.0f));
    const float b = phys_add(flag_below(gz1, 0.0f), flag_below(gz0, 0.0f));
    const float c = phys_add(flag_at_or_above(gx1, max_x), flag_at_or_above(gx0, max_x));
    const float d = phys_add(flag_at_or_above(gz1, max_z), flag_at_or_above(gz0, max_z));
    const float outside = phys_add(phys_add(phys_add(a, b), c), d);
    float both = phys_add(flag_both(b), flag_both(a));
    both = phys_add(flag_both(c), both);
    both = phys_add(flag_both(d), both);
    gx1 = phys_sub(gx1, gx0) >= 0.0f ? phys_add(gx1, 0.01f) : phys_sub(gx1, 0.01f);      /* 0x9322ac */
    gz1 = phys_sub(gz1, gz0) >= 0.0f ? phys_add(gz1, 0.01f) : phys_sub(gz1, 0.01f);
    uint32_t result = 0;
    if (both > 0.0f) goto out;
    float x0 = gx0, y0 = sy, z0 = gz0, x1 = gx1, y1 = ey, z1 = gz1;
    if (!(outside == 0.0f)) {
        float dx = phys_sub(gx1, gx0), dy = phys_sub(ey, sy), dz = phys_sub(gz1, gz0);
        const float len2 = phys_add(phys_add(phys_mul(dz, dz), phys_mul(dy, dy)), phys_mul(dx, dx));
        if (len2 > 0.0f) {
            const float inv = phys_div(1.0f, phys_sqrt(len2));
            dx = phys_mul(dx, inv); dy = phys_mul(dy, inv); dz = phys_mul(dz, inv);
        }
        const float inv_dz = phys_div(1.0f, dz), inv_dx = phys_div(1.0f, dx);
        const float x_per_z = phys_mul(inv_dz, dx), z_per_x = phys_mul(inv_dx, dz);
        const float y_per_z = phys_mul(inv_dz, dy), y_per_x = phys_mul(inv_dx, dy);
        /* the start against x = 0 and x = max_x */
        float p = phys_sub(-0.0f, clip_zero(gx0));
        float x = phys_add(gx0, p), y = phys_add(phys_mul(p, y_per_x), sy), z = phys_add(phys_mul(p, z_per_x), gz0);
        float r = clip_zero(phys_sub(max_x, x));
        x0 = phys_add(r, x); y0 = phys_add(phys_mul(r, y_per_x), y); z0 = phys_add(phys_mul(r, z_per_x), z);
        /* the end against x */
        p = phys_sub(-0.0f, clip_zero(gx1));
        x = phys_add(p, gx1); y = phys_add(phys_mul(p, y_per_x), ey); z = phys_add(phys_mul(p, z_per_x), gz1);
        r = clip_zero(phys_sub(max_x, x));
        x1 = phys_add(r, x); y1 = phys_add(phys_mul(r, y_per_x), y); z1 = phys_add(phys_mul(r, z_per_x), z);
        /* the start against z = 0 and z = max_z */
        p = phys_sub(-0.0f, clip_zero(z0));
        z = phys_add(p, z0); y = phys_add(phys_mul(p, y_per_z), y0); x = phys_add(phys_mul(p, x_per_z), x0);
        r = clip_zero(phys_sub(max_z, z));
        z0 = phys_add(r, z); y0 = phys_add(phys_mul(r, y_per_z), y); x0 = phys_add(phys_mul(r, x_per_z), x);
        /* the end against z */
        p = phys_sub(-0.0f, clip_zero(z1));
        z = phys_add(p, z1); y = phys_add(phys_mul(p, y_per_z), y1); x = phys_add(phys_mul(p, x_per_z), x1);
        r = clip_zero(phys_sub(max_z, z));
        y1 = phys_add(phys_mul(r, y_per_z), y); x1 = phys_add(phys_mul(r, x_per_z), x); z1 = phys_add(r, z);
        if (0.001f > abs_bits(phys_sub(x0, x1)) && 0.001f > abs_bits(phys_sub(z0, z1))) goto out;   /* 0x97743c */
        /* clamped to the grid */
        x0 = x0 >= max_x ? max_x : x0; x0 = 0.0f >= x0 ? 0.0f : x0;
        z0 = z0 >= max_z ? max_z : z0; z0 = 0.0f >= z0 ? 0.0f : z0;
        x1 = x1 >= max_x ? max_x : x1; x1 = 0.0f >= x1 ? 0.0f : x1;
        z1 = z1 >= max_z ? max_z : z1; z1 = 0.0f >= z1 ? 0.0f : z1;
    }
    SF32(seg0, 0, x0); SF32(seg0, 4, y0); SF32(seg0, 8, z0);
    SF32(seg1, 0, x1); SF32(seg1, 4, y1); SF32(seg1, 8, z1);
    CALL_AT(L, 4, result = phys_terrain_coarse_traversal(seg1, terrain, hit, normal, seg0, regs));
out:
    rx_set_sp(sp0);
    return result != 0;
}
RX_FUNCTION(0x007c6cc0, phys_terrain_segment_query)
