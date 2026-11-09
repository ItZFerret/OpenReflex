/* fx_rtbump.c -- the RT_Bump cascade layout of a frame: where the three 768 x 512 slices of RT_Bump lie on the ground.
 *
 * HFTerrainTextureRenderEvent (007c1010) prepares a camera for the fit (fx_rtbump_camera_inputs): the frustum's corner directions
 * against an up vector square to the world's horizontal right, the vertical fov widened to cover them, the horizontal forward
 * (the forward's XZ; the up's when the camera looks nearly straight up or down) and right. RTBump_FitCascadeMatrices (007ba8d0)
 * then clips the frustum against the terrain (TerrainBaseHeightAt 007c5950, TerrainSegmentQuery 007c6cc0): the four edges from
 * the near plane's corners to 384 ahead, seven samples across the bottom plane, the top centre ray and a four-step bisection up
 * each side. The hits, in the camera's horizontal frame (x right, z forward, z at most 384), give one trapezoid: the near edge
 * 4 m before the nearest ground (at most 128), the far edge at the farthest (at least near + Lerp_Clamped(|forward.y|, 384, 128),
 * at most 384), the sides through the hits (square-root blends, at least 14 m apart at the near edge). It is cut at near + 16
 * and near + 64; each slab gets a top-down orthographic view-projection of its quad (view_out) and the trapezoid matrix of the
 * quad's projected corners (007ba260, trap_out). notes/rt-bump.md (_re/notes) section 3.
 *
 * Exactness: SSE single precision in the original's order; the fov's arcsine in double with float steps (as 007c1010); SqrtF
 * 0042c230, PowF 0042c1f0 (exponent 0.5), Lerp_Clamped 00430810, Float_Abs 00401000 as the original calls them. */
#include "fx_rtbump.h"
#include "phys_crt.h"
#include "phys_terrain.h"
#include "bike_curve.h"
#include <string.h>

static inline float K(uint32_t a) { return F32(0, a); }
static inline float fabs_(float x) { return rx_bf(rx_fb(x) & 0x7fffffffu); }      /* Float_Abs 00401000 */

#define FIT_FAR 384.0f          /* 0x9ddfe0 kRTBumpFarDistance */
#define STEP 0.125f         /* 0x9c4754 kRTBumpSearchStep */

/* tan(angle / 2) as 007ba8d0 (prologue) and 007c1010 (the widened fov) compute it: sin / cos polynomials (0x935d3c ..) of
 * pi - (|angle / 2| mod 2 pi), the sign put back on the sine */
static float tan_half(float angle) {
    const float h = angle * 0.5f;                                   /* 0x9322cc */
    const float sign = h >= 0.0f ? 1.0f : -1.0f;                    /* 0x931f24 / 0x9cd9e8 */
    const float a = sign * h;
    const int32_t n = rx_cvtt(a * K(0x9774a0));
    const float r = K(0x977498) - (a - (float)n * K(0x97749c));
    const float s = r * r, s2 = s * s;
    float num = s * K(0x935d68) + K(0x935d64);
    num = num * s2 + (s * K(0x935d60) + K(0x935d5c));
    num = num * s2 + (s * K(0x935d58) + K(0x935d54));
    num = num * r * sign;
    float den = s * K(0x935d50) + K(0x935d48);
    den = den * s2 + (s * K(0x935d44) + K(0x935d4c));
    den = den * s2 + (s * K(0x935d40) + K(0x935d3c));
    return num / (-0.0f - den);
}

/* ---- 007c1010 (the part before 007ba8d0) ------------------------------------------------------------------------------- */
void fx_rtbump_camera_inputs(rx_addr cam, fx_rtbump_inputs* o) {
    const float b0 = K(0x9cb4b0), b4 = K(0x9cb4b4), b8 = K(0x9cb4b8);       /* the world's up (0, 1, 0) */
    const float Fx = F32(cam, 0x40), Fy = F32(cam, 0x44), Fz = F32(cam, 0x48);
    const float Cx = F32(cam, 0x30), Cy = F32(cam, 0x34), Cz = F32(cam, 0x38);
    const float fov = F32(cam, 0x50), aspect = F32(cam, 0x54);

    /* the camera's right: forward x up */
    float rx = Fz * Cy - Cz * Fy, rz = Cx * Fy - Fx * Cy, ry = Fx * Cz - Cx * Fz;
    float l = ry * ry + rx * rx + rz * rz;
    if (0.0f < l) { const float i = 1.0f / phys_sqrtf(l); rx = rx * i; ry = ry * i; rz = rz * i; }

    /* a right square to the world's up: forward x up_w (the camera's up x up_w when the forward is within 0.99 of vertical) */
    float vx, vy, vz;
    if (Fy <= K(0x9d7894)) {
        if (-0.99f <= Fy) {                                                      /* 0x977d5c */
            vx = Fz * b4 - b8 * Fy; vy = Fx * b8 - Fz * b0; vz = b0 * Fy - Fx * b4;
        } else {
            vx = Cz * b4 - Cy * b8; vy = Cx * b8 - Cz * b0; vz = Cy * b0 - Cx * b4;
        }
    } else {
        vx = Cy * b8 - Cz * b4; vy = Cz * b0 - Cx * b8; vz = Cx * b4 - Cy * b0;
    }
    l = vy * vy + vx * vx + vz * vz;
    if (0.0f < l) { const float i = 1.0f / phys_sqrtf(l); vy = vy * i; vx = vx * i; vz = vz * i; }

    /* the up square to it: v x forward */
    float ux = vz * Fy - vy * Fz, uy = vx * Fz - vz * Fx, uz = vy * Fx - vx * Fy;
    l = uy * uy + ux * ux + uz * uz;
    if (0.0f < l) { const float i = 1.0f / phys_sqrtf(l); ux = ux * i; uy = uy * i; uz = uz * i; }

    /* the frustum's four corner directions (camera up +- t, right +- t aspect) against that up: the largest */
    const float t = tan_half(fov), ta = t * aspect;
    const float hx = rx * ta, hy = ry * ta, hz = rz * ta;
    const float e0 = (Cx * t + hx) * ux + (Cy * t + hy) * uy + (Cz * t + hz) * uz;
    const float e1 = (Cx * t - hx) * ux + (Cy * t - hy) * uy + (Cz * t - hz) * uz;
    const float nzt = (-0.0f - Cz) * t;
    const float e2 = ((-0.0f - Cx) * t + hx) * ux + ((-0.0f - Cy) * t + hy) * uy + (nzt + hz) * uz;
    const float e3 = ux * ((-0.0f - Cx) * t - hx) + ((-0.0f - Cy) * t - hy) * uy + ((-0.0f - Cz) * t - hz) * uz;
    float m = e3;
    if (m < e2) m = e2;
    if (m < e1) m = e1;
    const float top = e0 <= m ? m : e0;

    /* the widened fov: twice the angle between the forward and forward + up * top (the arcsine in double with float steps) */
    float dy = uy * top + Fy, dx = Fx + ux * top, dz = uz * top + Fz;
    l = dy * dy + dx * dx + dz * dz;
    if (0.0f < l) { const float i = 1.0f / phys_sqrtf(l); dx = dx * i; dy = dy * i; dz = dz * i; }
    const float c = dy * Fy + Fx * dx + dz * Fz;
    const float ac = fabs_(c);
    const double one = 1.0;                                                     /* 0x977638 */
    float y = ac;
    if (ac >= K(0x9ddfc8)) y = phys_sqrtf(fabs_((float)(one - (double)(float)((double)c * (double)c))));
    const float s = (float)((double)y * (double)y), s2 = (float)((double)s * (double)s);
    const float p1 = (float)(0.5648093223571777 - (double)s * 0.04992850869894028);      /* 0x9777b0, 0x9777a8 */
    const float p2 = (float)(one - (double)s * 1.4430354833602905);                       /* 0x9777a0 */
    const float inv = (float)(one / ((double)p1 * (double)s2 + (double)p2));
    const float p3 = (float)(0.3993034064769745 - (double)s * 0.0193784236907959);       /* 0x977798, 0x977790 */
    const float p4 = (float)(one - (double)s * 1.2763687372207642);                       /* 0x977788 */
    const float num = (float)(((double)p3 * (double)s2 + (double)p4) * (double)y);
    float r = (float)((double)inv * (double)num);
    const double half_pi = 1.5707963705062866;                                            /* 0x977510 */
    if (ac >= K(0x9ddfc8)) r = (float)(half_pi - (double)r);
    if (!(c >= 0.0f)) r = -0.0f - r;
    const float fov2 = (float)(half_pi - (double)r) * 2.0f;
    const float t2 = tan_half(fov2);

    o->fov = fov2;
    o->aspect = t2 * aspect / t2;
    o->up[0] = ux; o->up[1] = uy; o->up[2] = uz; o->up[3] = 0.0f;

    /* the horizontal forward: the forward's XZ (the up's, negated when looking up, within 0.95 of vertical) */
    float fx, fz;
    if (Fy <= K(0x9cd9ac)) {
        if (-0.95f <= Fy) { fx = Fx; fz = Fz; } else { fx = ux; fz = uz; }       /* 0x977d58 */
    } else { fx = -0.0f - ux; fz = -0.0f - uz; }
    float hfx = fx, hfy = 0.0f, hfz = fz;
    l = fx * fx + 0.0f * 0.0f + fz * fz;
    if (0.0f < l) { const float i = 1.0f / phys_sqrtf(l); hfx = fx * i; hfy = i * 0.0f; hfz = fz * i; }
    o->fwd_h[0] = hfx; o->fwd_h[1] = hfy; o->fwd_h[2] = hfz; o->fwd_h[3] = 0.0f;
    /* the horizontal right: fwd_h x up_w */
    o->right_h[0] = hfz * b4 - hfy * b8;
    o->right_h[1] = hfx * b8 - hfz * b0;
    o->right_h[2] = hfy * b0 - hfx * b4;
    o->right_h[3] = 0.0f;
}

/* ---- 007ba260 RTBump_TrapezoidMatrix ---------------------------------------------------------------------------------- */
void fx_rtbump_trapezoid_matrix(rx_addr p0, rx_addr p1, rx_addr out, rx_addr p3, rx_addr p2) {
    const rx_v4 half = rx_ld4(0x00d615d0), one = rx_ld4(0x00d61650), ez = rx_ld4(0x00d61680);
    const float fz = K(0x00d61668);
    const float x0 = F32(p0, 0), y0 = F32(p0, 4), z0 = F32(p0, 8);
    const float x1 = F32(p1, 0), y1 = F32(p1, 4), z1 = F32(p1, 8);
    const float x3 = F32(p3, 0), y3 = F32(p3, 4), z3 = F32(p3, 8);
    const float x2 = F32(p2, 0), y2 = F32(p2, 4);
    float a = (x0 - x1) * half.v[0], b = (y0 - y1) * half.v[1], cz = (z0 - z1) * half.v[2];
    const float na = 0.0f - a;
    const float dx0 = x2 - x0, dy0 = y2 - y0, dx1 = x3 - x1, dy1 = y3 - y1;
    const float det = dy0 * dx1 - dx0 * dy1;
    const float c0 = x3 * y1 - y3 * x1, c1 = x2 * y0 - y2 * x0;
    const float g0 = (c0 * dx0 - c1 * dx1) * (one.v[0] / det);
    const float g1 = (c0 * dy0 - c1 * dy1) * (one.v[1] / det);
    const float g2 = (c0 * (F32(p2, 8) - z0) - c1 * (z3 - z1)) * (one.v[2] / det);
    float w = 0.0f - (g0 * a + g1 * b + g2 * (a * a + b * b + cz * cz));
    float v = 0.0f - (g0 * b + g1 * na + g2 * (b * a - b * a));
    const float k0 = x0 * a + y0 * b + z0 * w, k1 = x0 * b + y0 * na + z0 * v;
    const float k2 = x1 * a + y1 * b + z1 * w, k3 = x1 * b + y1 * na + z1 * v;
    const float q = (0.0f - (k2 + w + k0 + w)) / (k3 + v + k1 + v);
    const float ra = q * b + a, rb = q * na + b, rw = q * v + w;
    const float s0 = x0 * ra + y0 * rb + z0 * rw, s1 = x0 * b + y0 * na + z0 * v;
    const float i0 = one.v[0] / (rw + s0), i1 = one.v[1] / (rw + s0), i2 = one.v[2] / (rw + s0);
    const float j0 = one.v[0] / (s1 + v), j1 = one.v[1] / (s1 + v), j2 = one.v[2] / (s1 + v);
    const float pb = j0 * b, pna = j1 * na, pv = j2 * v;
    float u0 = pb + ez.v[0], u1 = pna + ez.v[1], u2 = pv + ez.v[2];
    const float m3 = x3 * u0 + y3 * u1 + z3 * u2;
    const float n3 = x3 * pb + y3 * pna + z3 * pv;
    const float m0 = x0 * u0 + y0 * u1 + z0 * u2;
    const float n0 = x0 * pb + y0 * pna + z0 * pv;
    const float h0 = (m0 + u2) * (one.v[0] - 0.5f) / (n0 + pv), h1 = (m0 + u2) * (one.v[1] - 0.5f) / (n0 + pv),
                h2 = (m0 + u2) * (one.v[2] - 0.5f) / (n0 + pv);
    const float o0 = (m3 + u2) * 0.5f / (n3 + pv), o1 = (m3 + u2) * 0.5f / (n3 + pv), o2 = (m3 + u2) * 0.5f / (n3 + pv);
    u0 = (0.0f - (h0 + o0)) * pb + u0; u1 = (0.0f - (h1 + o1)) * pna + u1; u2 = (0.0f - (h2 + o2)) * pv + u2;
    const float m0b = x0 * u0 + y0 * u1 + z0 * u2, n0b = x0 * pb + y0 * pna + z0 * pv;
    const float f0 = (0.0f - (n0b + pv)) / (m0b + u2), f1 = (0.0f - (n0b + pv)) / (m0b + u2), f2 = (0.0f - (n0b + pv)) / (m0b + u2);
    u0 = f0 * u0; u1 = f1 * u1; u2 = f2 * u2;
    const float m3b = x3 * u0 + y3 * u1 + z3 * u2, n3b = n3;
    const float r0 = (m3b + u2) / (n3b + pv), r1 = (m3b + u2) / (n3b + pv), r2 = (m3b + u2) / (n3b + pv);
    u0 = (one.v[0] - r0) * half.v[0] * pb + u0;
    u1 = (one.v[1] - r1) * half.v[1] * pna + u1;
    u2 = (one.v[2] - r2) * half.v[2] * pv + u2;
    const float m3c = x3 * u0 + y3 * u1 + z3 * u2, n3c = n3;
    const float z_0 = (0.0f - (n3c + pv)) / (u2 + m3c), z_1 = (0.0f - (n3c + pv)) / (u2 + m3c), z_2 = (0.0f - (n3c + pv)) / (u2 + m3c);
    SF32(out, 0x00, i0 * a);  SF32(out, 0x04, z_0 * u0); SF32(out, 0x08, fz); SF32(out, 0x0c, pb);
    SF32(out, 0x10, i1 * rb); SF32(out, 0x14, z_1 * u1); SF32(out, 0x18, fz); SF32(out, 0x1c, pna);
    SF32(out, 0x20, 0.0f);    SF32(out, 0x24, 0.0f);     SF32(out, 0x28, 1.0f); SF32(out, 0x2c, 0.0f);
    SF32(out, 0x30, i2 * rw); SF32(out, 0x34, z_2 * u2); SF32(out, 0x38, fz); SF32(out, 0x3c, pv);
    (void)rb;
}
RX_FUNCTION(0x007ba260, fx_rtbump_trapezoid_matrix)

/* ---- 007ba8d0 RTBump_FitCascadeMatrices ------------------------------------------------------------------------------- */

typedef struct { float x, y, z; } v3;
static inline v3 ld3(rx_addr a) { v3 r = {F32(a, 0), F32(a, 4), F32(a, 8)}; return r; }
static inline void st3(rx_addr a, v3 v) { SF32(a, 0, v.x); SF32(a, 4, v.y); SF32(a, 8, v.z); }

/* the frame's buffers the original passes by address (start / end / hit / normal of TerrainSegmentQuery, the point of
 * TerrainBaseHeightAt): one 0x80 block below the stack pointer */
typedef struct { rx_addr terrain, start, end, hit, normal, point; } qbuf;

static int segment(const qbuf* Q, v3 s, v3 e, v3* hit, v3* normal) {
    st3(Q->start, s); st3(Q->end, e);
    uint32_t regs[2] = {Q->start, Q->hit};
    const uint32_t r = phys_terrain_segment_query(Q->terrain, Q->start, Q->end, Q->hit, Q->normal, regs);
    *hit = ld3(Q->hit); *normal = ld3(Q->normal);
    return r != 0;
}
static int base_height(const qbuf* Q, v3* p) {
    st3(Q->point, *p);
    uint32_t edx = 0;
    const uint32_t r = phys_terrain_base_height_at(Q->terrain, Q->point, &edx);
    *p = ld3(Q->point);
    return r != 0;
}
/* the segment's direction (end - start, normalised when not zero; left as it was otherwise) */
static void seg_dir(v3 s, v3 e, v3* dir) {
    const float x = e.x - s.x, y = e.y - s.y, z = e.z - s.z;
    const float l = x * x + y * y + z * z;
    if (0.0f < l) { const float i = 1.0f / phys_sqrtf(l); dir->x = x * i; dir->y = i * y; dir->z = i * z; }
}
static inline float dot3(v3 a, v3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

/* a point in the camera's horizontal frame (x along right_h, z along fwd_h, z at most 384) */
typedef struct { float x, z; } hz;

/* The four square-root side blends of 007ba8d0 (one helper, two orientations): the side line through (ex, ez) (the edge's
 * near point, lower / mid hit) pulled towards the hit (hx, hz). left: the near x is lowered / the far x raised by the blend. */
static void side_blend(float* nx, float* fx, float* fz, float near_z, float* hzp, float hx, int left) {
    float h = *hzp;
    if (fabs_(near_z - h) < 0.1f) h = near_z + 0.1f;                     /* 0x931d94 */
    *hzp = h;
    if (fabs_(*fz - h) < 1.0f) *fz = *fz + 1.0f;
    const float dzf = near_z - *fz, dzh = near_z - h, x0 = *nx;
    const float dxf = *nx - *fx;
    const float a = phys_sqrtf((*nx - hx) * (*nx - hx) + 0.0f + dzh * dzh);
    const float b = phys_sqrtf(dxf * dxf + 0.0f * 0.0f + dzf * dzf);
    float ratio = a / b;
    if (ratio < 0.0f) ratio = 0.0f;
    const float p = phys_powf(ratio, 0.5f);
    const float nnx = ((*fx - hx) * dzf / (*fz - h) + *fx) * (1.0f - p) + p * *nx;
    if (left) { if (nnx <= *nx) *nx = nnx; } else { if (*nx <= nnx) *nx = nnx; }
    const float nfx = (*fx - p * *fx) + ((hx - x0) * dzf / dzh + x0) * p;
    if (left) { if (nfx <= *fx) *fx = nfx; } else { if (*fx <= nfx) *fx = nfx; }
}

void fx_rtbump_fit_cascades(rx_addr fwd_h, rx_addr right_h, rx_addr event, rx_addr camera, rx_addr position, rx_addr forward,
                            rx_addr up, float aspect, rx_addr view_out, rx_addr trap_out, float fov, fx_rtbump_quad* quad) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x4d0;
    rx_set_sp(L);
    const qbuf Q = {U32(event, 4), L + 0x380, L + 0x3c0, L + 0xd0, L + 0x120, L + 0x70};
    const float n = F32(camera, 0x58);
    const v3 P = ld3(position), F = ld3(forward), U = ld3(up), H = ld3(fwd_h), R = ld3(right_h);

    /* the camera's right (up x forward, normalised) and the near plane's / the 384 plane's corners */
    const float t = tan_half(fov), ta = t * aspect;
    float rx = F.z * U.y - F.y * U.z, ry = F.x * U.z - U.x * F.z, rz = U.x * F.y - F.x * U.y;
    {
        const float l = ry * ry + rx * rx + rz * rz;
        if (0.0f < l) { const float i = 1.0f / phys_sqrtf(l); rx = rx * i; ry = ry * i; rz = rz * i; }
    }
    const float rnx = rx * n, rny = ry * n, rnz = rz * n;
    const float fnx = F.x * n, fny = F.y * n, fnz = F.z * n, unx = U.x * n, uny = U.y * n, unz = U.z * n;
    v3 c[4], f[4];
    c[0] = (v3){((P.x + fnx) - unx * t) - rnx * ta, ((P.y + fny) - uny * t) - rny * ta, ((P.z + fnz) - unz * t) - rnz * ta};
    c[1] = (v3){((P.x + fnx) - unx * t) + rnx * ta, ((P.y + fny) - uny * t) + rny * ta, ((P.z + fnz) - unz * t) + rnz * ta};
    c[2] = (v3){(P.x + fnx + unx * t) - rnx * ta, (P.y + fny + uny * t) - rny * ta, (P.z + fnz + unz * t) - rnz * ta};
    c[3] = (v3){P.x + fnx + unx * t + rnx * ta, P.y + fny + uny * t + rny * ta, P.z + fnz + unz * t + rnz * ta};
    const float rfx = rx * FIT_FAR, rfy = ry * FIT_FAR, rfz = rz * FIT_FAR;
    const float ffx = F.x * FIT_FAR, ffy = F.y * FIT_FAR, ffz = F.z * FIT_FAR, ufx = U.x * FIT_FAR, ufy = U.y * FIT_FAR, ufz = U.z * FIT_FAR;
    f[0] = (v3){((P.x + ffx) - ufx * t) - rfx * ta, ((P.y + ffy) - ufy * t) - rfy * ta, ((P.z + ffz) - ufz * t) - rfz * ta};
    f[1] = (v3){((P.x + ffx) - ufx * t) + rfx * ta, ((ffy + P.y) - ufy * t) + rfy * ta, ((ffz + P.z) - ufz * t) + rfz * ta};
    const v3 e = {P.x + ffx + ufx * t, ffy + P.y + ufy * t, ffz + P.z + ufz * t};
    f[2] = (v3){e.x - rfx * ta, e.y - rfy * ta, e.z - rfz * ta};
    f[3] = (v3){e.x + rfx * ta, e.y + rfy * ta, e.z + rfz * ta};

    /* the near corners onto the ground (kept, flagged, when below it) */
    v3 A = c[0], B = c[1];
    int hitA = 0, hitB = 0;
    if (base_height(&Q, &A) && c[0].y < A.y) { A = c[0]; hitA = 1; }
    if (base_height(&Q, &B) && c[1].y < B.y) { B = c[1]; hitB = 1; }
    /* the far corners: 384 ahead along the horizontal forward, at the camera's height, the frustum's half width */
    const float lx = R.x * FIT_FAR * ta, ly = R.y * FIT_FAR * ta, lz = R.z * FIT_FAR * ta;
    v3 C = {(P.x + H.x * FIT_FAR) - lx, (H.y * FIT_FAR + P.y) - ly, (H.z * FIT_FAR + P.z) - lz};
    v3 D = {P.x + H.x * FIT_FAR + lx, H.y * FIT_FAR + P.y + ly, H.z * FIT_FAR + P.z + lz};
    int hitC = 0, hitD = 0;

    /* the four frustum edges */
    v3 dir = {0, 0, 0}, hit, nrm;
    if (!hitA && segment(&Q, c[0], f[0], &hit, &nrm)) { seg_dir(c[0], f[0], &dir); if (dot3(nrm, dir) <= 0.25f) { A = hit; hitA = 1; } }
    if (!hitB && segment(&Q, c[1], f[1], &hit, &nrm)) { seg_dir(c[1], f[1], &dir); if (dot3(nrm, dir) <= 0.25f) { B = hit; hitB = 1; } }
    if (segment(&Q, c[2], f[2], &hit, &nrm)) { seg_dir(c[2], f[2], &dir); if (dot3(nrm, dir) <= 0.25f) { C = hit; hitC = 1; } }
    if (segment(&Q, c[3], f[3], &hit, &nrm)) { seg_dir(c[3], f[3], &dir); if (dot3(nrm, dir) <= 0.25f) { D = hit; hitD = 1; } }

    /* seven samples across the bottom plane: the nearest hit */
    v3 best = f[0];
    int hitBottom = 0;
    v3 ua = {ufx * t, ufy * t, ufz * t}, ub = e;      /* (the original's locals hold these before; used only for zero lengths) */
    {
        v3 sa = {c[1].x - c[0].x, c[1].y - c[0].y, c[1].z - c[0].z}, sb = {f[1].x - f[0].x, f[1].y - f[0].y, f[1].z - f[0].z};
        float l = sa.x * sa.x + sa.y * sa.y + sa.z * sa.z;
        if (0.0f < l) { const float i = 1.0f / phys_sqrtf(l); ua = (v3){sa.x * i, i * sa.y, i * sa.z}; }
        l = sb.x * sb.x + sb.y * sb.y + sb.z * sb.z;
        if (0.0f < l) { const float i = 1.0f / phys_sqrtf(l); ub = (v3){sb.x * i, i * sb.y, i * sb.z}; }
        const float la = phys_sqrtf(sa.x * sa.x + sa.y * sa.y + sa.z * sa.z);
        const float lb = phys_sqrtf(sb.x * sb.x + sb.y * sb.y + sb.z * sb.z);
        const v3 da = {ua.x * la, ua.y * la, ua.z * la}, db = {ub.x * lb, ub.y * lb, ub.z * lb};
        float s = STEP;
        for (int i = 0; i < 7; i++, s = s + STEP) {
            const v3 st = {da.x * s + c[0].x, da.y * s + c[0].y, da.z * s + c[0].z};
            const v3 en = {f[0].x + s * db.x, db.y * s + f[0].y, db.z * s + f[0].z};
            if (!segment(&Q, st, en, &hit, &nrm)) continue;
            seg_dir(st, en, &dir);
            if (!(dot3(nrm, dir) <= 0.25f)) continue;
            const float dh = phys_sqrtf((hit.y - st.y) * (hit.y - st.y) + (hit.x - st.x) * (hit.x - st.x) + (hit.z - st.z) * (hit.z - st.z));
            const float bx = best.x - st.x, by = best.y - st.y, bz = best.z - st.z;
            const float db_ = phys_sqrtf(by * by + bx * bx + bz * bz);
            if (dh < db_) { best = hit; hitBottom = 1; }
        }
    }

    /* the top centre ray; the side mid points */
    const v3 nt = {(c[3].x + c[2].x) * 0.5f, (c[3].y + c[2].y) * 0.5f, (c[3].z + c[2].z) * 0.5f};
    const v3 ft = {(f[3].x + f[2].x) * 0.5f, (f[3].y + f[2].y) * 0.5f, (f[3].z + f[2].z) * 0.5f};
    v3 topHit = ft;
    v3 leftMid = {(f[2].x + f[0].x) * 0.5f, (f[2].y + f[0].y) * 0.5f, (f[2].z + f[0].z) * 0.5f};
    v3 rightMid = {(f[3].x + f[1].x) * 0.5f, (f[3].y + f[1].y) * 0.5f, (f[3].z + f[1].z) * 0.5f};
    if (segment(&Q, nt, ft, &hit, &nrm)) { seg_dir(nt, ft, &dir); if (dot3(nrm, dir) <= 0.25f) topHit = hit; }

    /* the bisections up each side (4 steps from a third): the first hit gives the low point, later ones the mid point. The
     * directions keep their last value when a length is zero (ua / ub from the sweep, as the original's locals). */
    v3 leftLow, rightLow;
    int midL = 0, midR = 0, firstL = 0, firstR = 0;
    for (int side = 0; side < 2; side++) {
        const v3 cn = side ? c[1] : c[0], cf = side ? c[3] : c[2], fn = side ? f[1] : f[0], ff = side ? f[3] : f[2];
        const float qx = cn.x - cf.x, qy = cn.y - cf.y, qz = cn.z - cf.z;
        float l = qx * qx + qy * qy + qz * qz;
        if (0.0f < l) { const float i = 1.0f / phys_sqrtf(l); ua = (v3){qx * i, qy * i, qz * i}; }
        const float gx = fn.x - ff.x, gy = fn.y - ff.y, gz = fn.z - ff.z;
        l = gx * gx + gy * gy + gz * gz;
        if (0.0f < l) { const float i = 1.0f / phys_sqrtf(l); ub = (v3){gx * i, gy * i, gz * i}; }
        const float lc = phys_sqrtf((cf.x - cn.x) * (cf.x - cn.x) + (cf.y - cn.y) * (cf.y - cn.y) + (cf.z - cn.z) * (cf.z - cn.z));
        const float lf = phys_sqrtf((ff.x - fn.x) * (ff.x - fn.x) + (ff.y - fn.y) * (ff.y - fn.y) + (ff.z - fn.z) * (ff.z - fn.z));
        const v3 dc = {ua.x * lc, ua.y * lc, ua.z * lc}, df = {ub.x * lf, ub.y * lf, ub.z * lf};
        v3 low = {fn.x - df.x * 0.3333f, fn.y - df.y * 0.3333f, fn.z - df.z * 0.3333f};            /* 0x977b98 */
        v3* mid = side ? &rightMid : &leftMid;
        int* midHit = side ? &midR : &midL;
        float hi = 1.0f, lo = 0.0f, s = 0.3333f;
        int first = 0;
        for (int i = 0; i < 4; i++) {
            const v3 st = {cn.x - s * dc.x, cn.y - dc.y * s, cn.z - dc.z * s};
            const v3 en = {fn.x - s * df.x, fn.y - df.y * s, fn.z - df.z * s};
            float next;
            int ok = segment(&Q, st, en, &hit, &nrm);
            if (ok) { seg_dir(st, en, &dir); ok = !(0.25f < dot3(nrm, dir)); }
            if (ok) {
                next = hi;
                if (!first) { low = hit; first = 1; } else { *mid = hit; *midHit = 1; }
                lo = s;
            } else { hi = s; next = lo; }
            s = (next - s) * 0.5f + s;
        }
        if (side) { rightLow = low; firstR = first; } else { leftLow = low; firstL = first; }
    }

    /* everything into the horizontal frame */
    hz qa, qb, qc, qd;
    {
        float dx = A.x - P.x, dy = A.y - P.y, dz = A.z - P.z;
        float z = dx * H.x + dy * H.y + dz * H.z;
        qa.x = dx * R.x + dy * R.y + dz * R.z; qa.z = FIT_FAR <= z ? FIT_FAR : z;
        dx = B.x - P.x; dy = B.y - P.y; dz = B.z - P.z;
        z = dx * H.x + dy * H.y + dz * H.z;
        qb.x = dx * R.x + dy * R.y + dz * R.z; qb.z = FIT_FAR <= z ? FIT_FAR : z;
        dx = C.x - P.x; dy = C.y - P.y; dz = C.z - P.z;
        z = dy * H.y + dx * H.x + dz * H.z;
        qc.x = dy * R.y + dx * R.x + dz * R.z; qc.z = FIT_FAR <= z ? FIT_FAR : z;
        dx = D.x - P.x; dy = D.y - P.y; dz = D.z - P.z;
        z = dy * H.y + dx * H.x + dz * H.z;
        qd.x = dy * R.y + dx * R.x + dz * R.z; qd.z = FIT_FAR <= z ? FIT_FAR : z;
    }
    float bestZ = (best.y - P.y) * H.y + (best.x - P.x) * H.x + (best.z - P.z) * H.z;
    if (FIT_FAR <= bestZ) bestZ = FIT_FAR;
    float topZ = (topHit.x - P.x) * H.x + (topHit.y - P.y) * H.y + (topHit.z - P.z) * H.z;
    if (FIT_FAR <= topZ) topZ = FIT_FAR;
    hz lm, rm, ll, rl;
    {
        float dx = leftMid.x - P.x, dy = leftMid.y - P.y, dz = leftMid.z - P.z;
        float z = dx * H.x + dy * H.y + dz * H.z;
        lm.x = dy * R.y + dx * R.x + dz * R.z; lm.z = FIT_FAR <= z ? FIT_FAR : z;
        dx = rightMid.x - P.x; dy = rightMid.y - P.y; dz = rightMid.z - P.z;
        z = dx * H.x + dy * H.y + dz * H.z;
        rm.x = dx * R.x + dy * R.y + dz * R.z; rm.z = FIT_FAR <= z ? FIT_FAR : z;
        dx = leftLow.x - P.x; dy = leftLow.y - P.y; dz = leftLow.z - P.z;
        z = dx * H.x + dy * H.y + dz * H.z;
        ll.x = dx * R.x + dy * R.y + dz * R.z; ll.z = FIT_FAR <= z ? FIT_FAR : z;
        dx = rightLow.x - P.x; dy = rightLow.y - P.y; dz = rightLow.z - P.z;
        z = dx * H.x + dy * H.y + dz * H.z;
        rl.x = dx * R.x + dy * R.y + dz * R.z; rl.z = FIT_FAR <= z ? FIT_FAR : z;
    }
    hz k[8];
    for (int i = 0; i < 8; i++) {
        const v3 p = i < 4 ? c[i] : f[i - 4];
        const float dx = p.x - P.x, dy = p.y - P.y, dz = p.z - P.z;
        const float z = dx * H.x + dy * H.y + dz * H.z;
        k[i].x = dy * R.y + dx * R.x + dz * R.z; k[i].z = FIT_FAR <= z ? FIT_FAR : z;
    }

    /* no near corner on the ground: the near edge at the bottom plane's nearest hit, on the frustum's bottom edges */
    if (!hitA && !hitB && hitBottom) {
        float dx = k[4].x - k[0].x, dz = k[4].z - k[0].z;
        float l = dx * dx + 0.0f + dz * dz;
        qa.z = bestZ;
        if (0.0f < l) { const float s = phys_sqrtf(l); dx = dx * (1.0f / s); dz = dz * (1.0f / s); }
        qa.x = (dz / dx) * bestZ;
        dx = k[5].x - k[1].x; dz = k[5].z - k[1].z;
        l = dx * dx + 0.0f + dz * dz;
        qb.z = bestZ;
        if (0.0f < l) { const float s = phys_sqrtf(l); dx = dx * (1.0f / s); dz = dz * (1.0f / s); }
        qb.x = (dz / dx) * bestZ;
    }
    /* side hits further out than the far corners take their place (and the far corners theirs) */
    if (qc.z < lm.z) { const hz o = qc; qc = lm; lm = o; }
    if (qd.z < rm.z) { const hz o = qd; qd = rm; rm = o; }
    if (qc.z < ll.z) { const hz o = qc; qc = ll; ll = o; }
    if (qd.z < rl.z) { const hz o = qd; qd = rl; rl = o; }

    /* the near edge level: the nearer corner's, the other side blended to it */
    if (qb.z <= qa.z) {
        const float nx = -0.0f - qb.x;
        const float d = fabs_(qb.z - qa.z);
        if (1.0f < d) {
            if (d < 0.1f) qa.z = qb.z + 0.1f;
            if (fabs_(qc.z - qa.z) < 1.0f) qc.z = qc.z + 1.0f;
            const float x0 = qa.x, dzf = qb.z - qc.z, dzn = qb.z - qa.z;
            const float a = phys_sqrtf((nx - qa.x) * (nx - qa.x) + 0.0f + dzn * dzn);
            const float b = phys_sqrtf((nx - qc.x) * (nx - qc.x) + 0.0f * 0.0f + dzf * dzf);
            float ratio = a / b;
            if (ratio < 0.0f) ratio = 0.0f;
            const float p = phys_powf(ratio, 0.5f);
            qa.x = ((qc.x - qa.x) * dzf / (qc.z - qa.z) + qc.x) * (1.0f - p) + p * nx;
            if (nx < qa.x) qa.x = nx;
            const float g = (qc.x - p * qc.x) + ((x0 - nx) * dzf / dzn + nx) * p;
            if (g <= qc.x) qc.x = g;
        } else qa.x = nx;
    } else {
        const float nx = -0.0f - qa.x;
        const float d = fabs_(qa.z - qb.z);
        if (d <= 1.0f) { qb.x = nx; qb.z = qa.z; }
        else {
            if (d < 0.1f) qb.z = qa.z + 0.1f;
            if (fabs_(qd.z - qb.z) < 1.0f) qd.z = qd.z + 1.0f;
            const float x0 = qb.x, dzf = qa.z - qd.z, dzn = qa.z - qb.z;
            const float a = phys_sqrtf((nx - qb.x) * (nx - qb.x) + 0.0f + dzn * dzn);
            const float b = phys_sqrtf(0.0f * 0.0f + (nx - qd.x) * (nx - qd.x) + dzf * dzf);
            float ratio = a / b;
            if (ratio < 0.0f) ratio = 0.0f;
            const float p = phys_powf(ratio, 0.5f);
            qb.x = ((qd.x - qb.x) * dzf / (qd.z - qb.z) + qd.x) * (1.0f - p) + p * nx;
            if (qb.x < nx) qb.x = nx;
            const float g = (qd.x - p * qd.x) + ((x0 - nx) * dzf / dzn + nx) * p;
            if (qd.x <= g) qd.x = g;
            qb.z = qa.z;
        }
    }
    qa.z = qb.z;

    /* far corners without a hit: the frustum's widest */
    if (!hitC) { const float a = fabs_(k[7].x); float b = fabs_(k[6].x); if (b <= a) b = a; qc.x = -0.0f - b; }
    if (!hitD) { const float a = fabs_(k[7].x), b = fabs_(k[6].x); qd.x = a; if (a < b) qd.x = b; }
    /* the far edge level: the farther of C / D, the other side scaled to it; the wider half width */
    {
        const float nl = -0.0f - qc.x;
        const float w = nl <= qd.x ? qd.x : nl;
        const float z = qc.z <= qd.z ? qd.z : qc.z;
        if (nl <= qd.x) {
            qd.x = (qd.x / qd.z) * z;
            qc.z = qc.z / qc.x;
            qc.x = -0.0f - w;
            qc.z = (-0.0f - qc.z) * w;
            qd.z = z;
        } else {
            qc.x = (qc.x / qc.z) * z;
            qd.z = (qd.z / qd.x) * w;
            qc.z = z;
            qd.x = w;
        }
    }
    {
        float g = qd.z - qb.z;
        if (qc.z - qa.z < qd.z - qb.z) g = qc.z - qa.z;
        const float off = 1.0f <= g ? 0.0f : 1.0f;
        qa.z = qa.z - off;
        qb.z = qb.z - off;
    }
    /* the side hits pull the side lines (square-root blends): left mid, left low, right mid, right low */
    if (midL) side_blend(&qa.x, &qc.x, &qc.z, qa.z, &lm.z, lm.x, 1);
    if (firstL) side_blend(&qa.x, &qc.x, &qc.z, qa.z, &ll.z, ll.x, 1);
    if (midR) side_blend(&qb.x, &qd.x, &qd.z, qb.z, &rm.z, rm.x, 0);
    if (firstR) side_blend(&qb.x, &qd.x, &qd.z, qb.z, &rl.z, rl.x, 0);

    /* the far edge: the farthest ground seen, at least near + the camera-pitch reach, at most 384 */
    const float reach = bike_lerp_clamped(fabs_(F.y), FIT_FAR, 128.0f);
    float farz = bestZ;
    if (bestZ < topZ) farz = topZ;
    {
        float cd = qd.z;
        if (qd.z < qc.z) cd = qc.z;
        if (farz < cd) farz = cd;
    }
    if (farz < reach + qa.z) farz = reach + qa.z;
    if (FIT_FAR <= farz) farz = FIT_FAR;
    const float wfar = farz - qb.z;
    qc.x = ((farz - qa.z) / (qc.z - qa.z)) * (qc.x - qa.x) + qa.x;
    const float dfar = (wfar / (qd.z - qb.z)) * (qd.x - qb.x) + qb.x;
    /* the near edge: 4 m before the nearest ground (at most 128), the sides at least 14 m apart there */
    float nmin = qb.z;
    if (qa.z < qb.z) nmin = qa.z;
    float nearz = bestZ;
    if (nmin < bestZ) nearz = nmin;
    nearz = nearz - 4.0f;
    if (128.0f <= nearz) nearz = 128.0f;
    qa.x = ((nearz - qa.z) / (farz - qa.z)) * (qc.x - qa.x) + qa.x;
    const float bn = nearz - qb.z;
    qb.x = (bn / wfar) * (dfar - qb.x) + qb.x;
    float widen = 14.0f - (qb.x - qa.x);
    if (widen < 0.0f) widen = 0.0f;
    qb.x = qb.x + widen * 0.5f;
    qa.x = qa.x - widen * 0.5f;
    float fw = ((qb.x - qa.x) - (dfar - qc.x)) + 1.0f;
    if (fw < 0.0f) fw = 0.0f;
    const float depth = farz - nearz;
    qc.x = qc.x - fw * 0.5f;
    qd.x = dfar + fw * 0.5f;
    if (quad) { quad->near_z = nearz; quad->far_z = farz; quad->ax = qa.x; quad->bx = qb.x; quad->cx = qc.x; quad->dx = qd.x; }

    /* the side directions and the slab corners (near, near + 16, near + 64, far along each side line) */
    v3 dl = {0, 0, 0}, dr = {0, 0, 0};
    {
        const float x = qc.x - qa.x;
        const float l = x * x + 0.0f + depth * depth;
        if (0.0f < l) { const float i = 1.0f / phys_sqrtf(l); dl = (v3){x * i, i * 0.0f, i * depth}; }
        const float y = qd.x - qb.x;
        const float m = y * y + 0.0f * 0.0f + depth * depth;
        if (0.0f < m) { const float i = 1.0f / phys_sqrtf(m); dr = (v3){y * i, i * 0.0f, i * depth}; }
    }
    const float b0 = K(0x9cb4c0), b4 = K(0x9cb4c4), b8 = K(0x9cb4c8);
    const float denL = dl.x * b0 + dl.y * b4 + dl.z * b8, denR = dr.x * b0 + dr.y * b4 + dr.z * b8;
    hz s[12];
    s[0] = (hz){qa.x, nearz};
    s[1] = (hz){qb.x, nearz};
    s[2] = (hz){(dl.x * 16.0f) / denL + qa.x, (dl.z * 16.0f) / denL + nearz};                    /* 0x9ddfdc */
    s[3] = (hz){(dr.x * 16.0f) / denR + qb.x, (dr.z * 16.0f) / denR + nearz};
    s[4] = s[2]; s[5] = s[3];
    s[6] = (hz){(dl.x * 64.0f) / denL + qa.x, (dl.z * 64.0f) / denL + nearz};
    s[7] = (hz){(dr.x * 64.0f) / denR + qb.x, (dr.z * 64.0f) / denR + nearz};
    s[8] = s[6]; s[9] = s[7];
    s[10] = (hz){qc.x, farz};
    s[11] = (hz){qd.x, farz};
    /* to the world (the height is the camera's: the frame is horizontal) */
    float wx[12], wz[12];
    for (int i = 0; i < 12; i++) {
        wx[i] = P.x + (s[i].x * R.x + s[i].z * H.x);
        wz[i] = P.z + (s[i].x * R.z + s[i].z * H.z);
    }

    /* per slab: a top-down camera 2 above the quad's centre, an orthographic box around it, their product into view_out; the
     * trapezoid matrix of the projected corners (y = 0) into trap_out */
    const rx_addr corners = L + 0x400;              /* the four projected corners (x, y, 0, 0) and the trapezoid's result */
    for (int sl = 0; sl < 3; sl++) {
        const float* x = wx + 4 * sl;
        const float* z = wz + 4 * sl;
        const rx_addr V = view_out + 0x40u * (uint32_t)sl, T = trap_out + 0x40u * (uint32_t)sl;
        const float cx = (x[3] + x[2] + x[1] + x[0]) * 0.25f;
        const float cz = (z[1] + z[0] + z[2] + z[3]) * 0.25f;
        const float r1 = phys_sqrtf((x[3] - cx) * (x[3] - cx) + 4.0f + (z[3] - cz) * (z[3] - cz));
        const float r2 = phys_sqrtf(-2.0f * -2.0f + (x[2] - cx) * (x[2] - cx) + (z[2] - cz) * (z[2] - cz));
        const float r = r1 < r2 ? r2 : r1;
        const float nr = -0.0f - r;
        float O[16];
        memset(O, 0, sizeof O);
        O[0] = 2.0f / (r - nr); O[5] = 2.0f / (r - nr);
        O[10] = K(0x977b90); O[14] = K(0x977b8c); O[15] = 1.0f;
        O[12] = -0.0f - (r + nr) / (r - nr); O[13] = -0.0f - (r + nr) / (r - nr);
        for (int i = 0; i < 16; i++) SF32(T, 4u * (uint32_t)i, O[i]);
        /* the view: right (1, 0, 0), up (0, 0, 1), forward (0, -1, 0), the eye 2 above (cx, cz) (0x9cb4a0 .., 0x9cb4b0 ..) */
        const float a0 = K(0x9cb4a0), a4 = K(0x9cb4a4), a8 = K(0x9cb4a8), u0 = K(0x9cb4b0), u4 = K(0x9cb4b4), u8 = K(0x9cb4b8);
        const float z84 = a8 * 0.0f, z240 = a4 * 0.0f, z224 = b4 * 0.0f, z23c = b8 * 0.0f;
        const float m400 = -0.0f - u0, m3f8 = -0.0f - u8, m4 = -0.0f - u4, ncx = -0.0f - cx, ncz = -0.0f - cz;
        const float z238 = m3f8 * 0.0f, z234 = m4 * 0.0f, z228 = m400 * 0.0f;
        float W[16];
        W[0] = z240 + a0 + z84;           W[1] = z224 + b0 + z23c;          W[2] = z234 + m400 + z238;    W[3] = 0.0f;
        W[4] = a4 + a0 * 0.0f + z84;      W[5] = b4 + b0 * 0.0f + z23c;     W[6] = m4 + z228 + z238;      W[7] = 0.0f;
        W[8] = a0 * 0.0f + z240 + a8;     W[9] = b0 * 0.0f + z224 + b8;     W[10] = z228 + z234 + m3f8;   W[11] = 0.0f;
        W[12] = (ncx * a0 - a4 * 2.0f) + ncz * a8 + 0.0f;
        W[13] = (ncx * b0 - b4 * 2.0f) + ncz * b8 + 0.0f;
        W[14] = (ncx * m400 - m4 * 2.0f) + ncz * m3f8 + 0.0f;
        W[15] = 1.0f;
        for (int i = 0; i < 16; i++) SF32(V, 4u * (uint32_t)i, W[i]);
        float M[16];
        for (int row = 0; row < 4; row++) {
            const float w0 = W[4 * row], w1 = W[4 * row + 1], w2 = W[4 * row + 2], w3 = W[4 * row + 3];
            M[4 * row + 0] = O[0] * w0 + O[4] * w1 + w2 * O[8] + O[12] * w3;
            M[4 * row + 1] = O[5] * w1 + O[1] * w0 + w2 * O[9] + O[13] * w3;
            M[4 * row + 2] = w0 * O[2] + O[6] * w1 + w2 * O[10] + O[14] * w3;
            M[4 * row + 3] = O[3] * w0 + O[7] * w1 + w2 * O[11] + O[15] * w3;
        }
        const float y4 = M[4] * 0.0f, y5 = M[5] * 0.0f, y7 = M[7] * 0.0f;
        const float w0 = x[0] * M[3] + y7 + z[0] * M[11] + M[15];
        const float w1 = x[1] * M[3] + y7 + z[1] * M[11] + M[15];
        const float w2 = x[2] * M[3] + y7 + z[2] * M[11] + M[15];
        const float w3 = x[3] * M[3] + y7 + z[3] * M[11] + M[15];
        const float py0 = (x[0] * M[1] + y5 + z[0] * M[9] + M[13]) / w0;
        const float py3 = (x[3] * M[1] + y5 + z[3] * M[9] + M[13]) / w3;
        const float py1 = (x[1] * M[1] + y5 + z[1] * M[9] + M[13]) / w1;
        const float py2 = (x[2] * M[1] + y5 + z[2] * M[9] + M[13]) / w2;
        for (int i = 0; i < 16; i++) SF32(V, 4u * (uint32_t)i, M[i]);
        const float px3 = (x[3] * M[0] + y4 + z[3] * M[8] + M[12]) / w3;
        const float px2 = (x[2] * M[0] + y4 + z[2] * M[8] + M[12]) / w2;
        const float px0 = (x[0] * M[0] + y4 + z[0] * M[8] + M[12]) / w0;
        const float px1 = (x[1] * M[0] + y4 + z[1] * M[8] + M[12]) / w1;
        const rx_addr p0 = corners, p1 = corners + 0x10, p2 = corners + 0x20, p3 = corners + 0x30, out = corners + 0x40;
        const float pts[4][2] = {{px0, py0}, {px1, py1}, {px2, py2}, {px3, py3}};
        for (int i = 0; i < 4; i++) {
            SF32(corners + 0x10u * (uint32_t)i, 0, pts[i][0]); SF32(corners + 0x10u * (uint32_t)i, 4, pts[i][1]);
            SF32(corners + 0x10u * (uint32_t)i, 8, 0.0f); SF32(corners + 0x10u * (uint32_t)i, 12, 0.0f);
        }
        fx_rtbump_trapezoid_matrix(p0, p1, out, p3, p2);
        for (int i = 0; i < 16; i++) SU32(T, 4u * (uint32_t)i, U32(out, 4u * (uint32_t)i));
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x007ba8d0, fx_rtbump_fit_cascades)
