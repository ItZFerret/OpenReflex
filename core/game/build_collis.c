/* build_collis.c -- see build_collis.h. Hand transliteration of CollisionAssetIO::vf3 00818ef0, the file factory FUN_0081b460 (header test
 * FUN_0081b3c0) and the geometry builders of the kinds the install uses, from the disassembly: SSE code is done lane by lane with the
 * original's operation order (sums as the shuffles associate them). Locals are C locals (they only receive stream reads); every
 * game-memory block, field and read is in the original's order. The allocator lock flag [0x00da67d8] is 0 in the game (one thread builds). */
#include "build_collis.h"
#include "build_alloc.h"
#include "build_load.h"
#include "map_scale.h"
#include <stdio.h>
#include <stdlib.h>

/* world_veh_math.c 00401740 Matrix3_ToQuaternionToEsi (cdecl M, ESI = out) */
void rx_world_matrix3_to_quaternion_to(rx_addr out, rx_addr M);

#define ZERO_VEC 0x00d61670u   /* the runtime zero vector the constructors copy */

/* vec3 stored as (x, y, z, z): the movss / shufps sequence of the loaders (lanes 2 and 3 both hold z) */
static void st_xyzz(rx_addr a, float x, float y, float z) { SF32(a, 0, x); SF32(a, 4, y); SF32(a, 8, z); SF32(a, 12, z); }
static void st_v4(rx_addr a, float x, float y, float z, float w) { SF32(a, 0, x); SF32(a, 4, y); SF32(a, 8, z); SF32(a, 12, w); }
static void st_splat(rx_addr a, float x) { st_v4(a, x, x, x, x); }
static float minps(float acc, float v) { return acc < v ? acc : v; }   /* minps dst = acc, src = v */
static float maxps(float acc, float v) { return acc > v ? acc : v; }

static void geom_header(rx_addr g, uint32_t kind, uint32_t vt) { SU32(g, 4, kind); SU8(g, 8, 0); SU32(g, 0, vt); }

/* FUN_00818aa0 (ESI = obj, stack n): a triangle group of n triangles */
static rx_addr triangles_new(uint32_t n) {
    rx_addr g = rx_reflex_aligned_alloc(0x18);
    if (!g) return 0;
    uint32_t bytes = rx_mul_sat(rx_mul_sat(n, 3), 0x10);
    SU32(g, 4, 0); SU8(g, 8, 0); SU32(g, 0, 0x00974f5c);
    SU32(g, 0xc, rx_crt_aligned_malloc(bytes, 16));
    SU32(g, 0x10, n); SF32(g, 0x14, 0.0f);
    return g;
}

/* FUN_0081b090 (ECX = stream): kind 22, groups of triangles with a center each */
static rx_addr kind22(rx_stream* s) {
    int32_t n = (int32_t)rx_stream_u32(s);
    rx_addr obj = rx_reflex_aligned_alloc(0x18);
    if (obj) {                                    /* FUN_00818d50 */
        SU32(obj, 4, 0x16); SU8(obj, 8, 0); SU32(obj, 0, 0x00974f7c);
        SU32(obj, 0xc, rx_crt_aligned_malloc(rx_mul_sat((uint32_t)n, 0x10), 16));
        SU32(obj, 0x10, rx_reflex_aligned_alloc(rx_mul_sat((uint32_t)n, 4)));
        SU32(obj, 0x14, (uint32_t)n);
    }
    for (int32_t g = 0; g < n; g++) {
        float c[3]; for (int k = 0; k < 3; k++) c[k] = rx_stream_f32(s);
        st_xyzz(U32(obj, 0xc) + 16u * (uint32_t)g, c[0], c[1], c[2]);
        int32_t m = (int32_t)rx_stream_u32(s);
        rx_addr grp = triangles_new((uint32_t)m);
        SU32(U32(obj, 0x10), 4 * (uint32_t)g, grp);
        float mn[4] = {rx_bf(0x7f7fffffu), rx_bf(0x7f7fffffu), rx_bf(0x7f7fffffu), rx_bf(0x7f7fffffu)};
        float mx[4] = {rx_bf(0xff7fffffu), rx_bf(0xff7fffffu), rx_bf(0xff7fffffu), rx_bf(0xff7fffffu)};
        for (int32_t t = 0; t < m; t++) {
            for (int v = 0; v < 3; v++) {
                float p[3]; for (int k = 0; k < 3; k++) p[k] = rx_stream_f32(s);
                rx_addr dst = U32(grp, 0xc) + 0x30u * (uint32_t)t + 0x10u * (uint32_t)v;
                st_xyzz(dst, p[0], p[1], p[2]);
                float l[4] = {p[0], p[1], p[2], p[2]};
                for (int k = 0; k < 4; k++) { mn[k] = minps(mn[k], l[k]); mx[k] = maxps(mx[k], l[k]); }
            }
        }
        float hx = (mx[0] - mn[0]) * 0.5f, hy = (mx[1] - mn[1]) * 0.5f, hz = (mx[2] - mn[2]) * 0.5f;
        float sum = rx_adds(rx_adds(hx * hx, hz * hz), hy * hy);   /* mulps, then addss lane0 + lane2, + lane1 */
        SF32(grp, 0x14, rx_sqrts(sum));
    }
    return obj;
}

/* FUN_008189a0 (ESI = obj, stack n): a list of n boxes (two vectors each) */
static rx_addr boxlist_new(uint32_t n) {
    rx_addr g = rx_reflex_aligned_alloc(0x18);
    if (!g) return 0;
    uint32_t bytes = rx_mul_sat(rx_mul_sat(n, 2), 0x10);
    SU32(g, 4, 2); SU8(g, 8, 0); SU32(g, 0, 0x00974f54);
    SU32(g, 0xc, rx_crt_aligned_malloc(bytes, 16));
    SU32(g, 0x10, n); SF32(g, 0x14, 0.0f);
    return g;
}

/* FUN_00819f90 (EDI = stream): kind 2 */
static rx_addr kind2(rx_stream* s) {
    uint8_t matrix[0x40]; rx_stream_read_host(s, matrix, 0x40);   /* read into the frame and not used */
    uint32_t x1 = rx_stream_u32(s);
    int32_t x2 = (int32_t)rx_stream_u32(s), x3 = (int32_t)rx_stream_u32(s);
    rx_addr obj = boxlist_new(x1);
    for (int32_t i = 0; i < x2; i++) {
        (void)rx_stream_u32(s);
        float b[3], c[3];
        for (int k = 0; k < 3; k++) b[k] = rx_stream_f32(s);
        for (int k = 0; k < 3; k++) c[k] = rx_stream_f32(s);
        if (i == 0) SF32(obj, 0x14, rx_sqrts(rx_adds(rx_adds(c[2] * c[2], c[1] * c[1]), c[0] * c[0])));
        uint8_t two[4]; rx_stream_read_host(s, two, 2); rx_stream_read_host(s, two, 2);
    }
    for (int32_t i = 0; i < x3; i++) {
        float a[3], b[3];
        for (int k = 0; k < 3; k++) a[k] = rx_stream_f32(s);
        st_xyzz(U32(obj, 0xc) + 0x20u * (uint32_t)i, a[0], a[1], a[2]);
        for (int k = 0; k < 3; k++) b[k] = rx_stream_f32(s);
        st_xyzz(U32(obj, 0xc) + 0x20u * (uint32_t)i + 0x10, b[0], b[1], b[2]);
    }
    return obj;
}

/* FUN_0081a790 (ECX = stream): kind 11, an oriented box: half extents sorted largest first and classified by their ratios */
static rx_addr kind11(rx_stream* s) {
    rx_addr b = rx_crt_aligned_malloc(0x60, 16);
    if (b) {
        SU32(b, 4, 0xb); SU8(b, 8, 0); SU32(b, 0, 0x00974f64);
        rx_v4 z = rx_ld4(ZERO_VEC);
        rx_st4(b + 0x10, z); rx_st4(b + 0x20, z); rx_st4(b + 0x30, z); rx_st4(b + 0x50, z);
    }
    float h[4]; for (int k = 0; k < 4; k++) h[k] = rx_stream_f32(s);
    if (b) for (int k = 0; k < 4; k++) SF32(b, 0x50 + 4 * k, h[k]);
    float a[3] = {h[0], h[1], h[2]};
    uint32_t idx[3] = {0, 1, 2};
    for (int i = 1; i < 3; i++)
        for (int j = i - 1; j >= 0; j--)
            if (a[i] > a[j]) { float t = a[i]; a[i] = a[j]; a[j] = t; uint32_t u = idx[i]; idx[i] = idx[j]; idx[j] = u; }
    float zero = rx_bf(rx_rd32(0x0092e12cu));
    float two = 2.0f /* 0x92e330 */;
    float a0 = a[0], a1 = a[1], a2 = a[2];
    /* comiss 2.0, ratio / jbe: the first branch (type 1) is taken when 2.0 > a2 / a0 */
    if (two > rx_divs(a2, a0)) {
        float sum = rx_adds(rx_adds(h[0] * h[0], h[2] * h[2]), h[1] * h[1]);
        SU32(b, 0x40, 1);
        st_splat(b + 0x30, rx_sqrts(sum));
        return b;
    }
    if (two > rx_divs(a2, a1)) {
        SU32(b, 0x40, 2);
        st_splat(b + 0x30, rx_sqrts(rx_adds(a2 * a2, a1 * a1)));
        if (idx[0] == 0) st_v4(b + 0x10, a0, zero, zero, zero);
        else if (idx[0] == 1) st_v4(b + 0x10, zero, a0, zero, zero);
        else st_v4(b + 0x10, zero, zero, a0, a0);
        return b;
    }
    SU32(b, 0x40, 3);
    st_splat(b + 0x30, a2);
    if (idx[2] == 0) { if (idx[0] == 1) st_v4(b + 0x10, zero, a0, a1, a1); return b; }
    if (idx[2] == 1) st_v4(b + 0x10, zero, a0, zero, zero);
    else st_v4(b + 0x10, zero, zero, a0, a0);
    return b;
}

/* FUN_0081a740 (EDI = stream): kind 16, a sphere: the radius at +0xc */
static rx_addr kind16(rx_stream* s) {
    rx_addr g = rx_reflex_aligned_alloc(0x10);
    if (g) geom_header(g, 0x10, 0x0096dd58);
    float r = rx_stream_f32(s);
    SF32(g, 0xc, r);
    return g;
}

/* FUN_0081aaf0 (EDI = stream): kind 17, a cylinder: radius (+0x44) and half height (+0x48) with a shape class at +0x40 */
static rx_addr kind17(rx_stream* s) {
    rx_addr g = rx_crt_aligned_malloc(0x50, 16);
    if (g) geom_header(g, 0x11, 0x00974f6c);
    float a = rx_stream_f32(s), b = rx_stream_f32(s);
    SF32(g, 0x44, a); SF32(g, 0x48, b);
    float r = rx_divs(b, a), half = 0.5f /* 0x9322cc */, zero = rx_bf(rx_rd32(0x0092e12cu));
    if (half > r) {                  /* ratio < 0.5 (comiss: not taken when unordered) */
        SU32(g, 0x40, 3);
        float p = rx_muls(a, rx_bf(0x3fb51eb8u) /* 0x9776b0 */);
        st_v4(g + 0x10, p, zero, zero, zero);
        st_v4(g + 0x20, zero, zero, p, p);
        st_splat(g + 0x30, rx_muls(b, half));
        return g;
    }
    if (r > 2.0f /* 0x92e330 */) {
        SU32(g, 0x40, 2);
        float h = rx_muls(b, half);
        st_v4(g + 0x10, zero, h, zero, zero);
        st_splat(g + 0x30, a);
        return g;
    }
    SU32(g, 0x40, 1);
    st_splat(g + 0x30, rx_sqrts(rx_adds(rx_muls(b, b), rx_muls(a, a))));
    return g;
}

/* FUN_00819d20 (stack: obj, n): the composite's vectors */
static rx_addr composite_new(uint32_t n) {
    rx_addr g = rx_reflex_aligned_alloc(0x9c);
    if (!g) return 0;
    SU32(g, 4, 0xf); SU8(g, 8, 0); SU32(g, 0, 0x00974f74);
    rx_ld_vec_init(g + 0xc);           /* FUN_00819de0: vector A (names) at +0xc and vector B (ids) at +0x24 */
    rx_ld_vec_init(g + 0x24);
    rx_ld_vec_init(g + 0x3c);          /* C: n shape pointers (FUN_0081fa90) */
    rx_ld_vec_alloc(g + 0x3c, n, 4);
    rx_ld_vec_init_crt(g + 0x54);      /* D: n matrices (FUN_0081fb80) */
    rx_ld_vec_alloc(g + 0x54, n, 0x40);
    rx_ld_vec_init_crt(g + 0x6c);      /* E: n quaternions (FUN_0081fc40) */
    rx_ld_vec_alloc(g + 0x6c, n, 0x10);
    rx_ld_vec_init_crt(g + 0x84);      /* F: n translations (FUN_0081fcf0) */
    rx_ld_vec_alloc(g + 0x84, n, 0x10);
    return g;
}

/* FUN_0081ac90 (ECX = stream, stack: version >= 3): kind 15 */
static rx_addr kind15(rx_stream* s, int v3) {
    int32_t n = (int32_t)rx_stream_u32(s);
    rx_addr g = composite_new((uint32_t)n);
    if (v3) {
        uint32_t cnt = 0;
        /* FUN_0081f9e0 (EAX = n): vector B resized to n, then FUN_00819cd0 (EDI = +0xc): the names */
        rx_ld_vec_resize_empty(g + 0x24, (uint32_t)n, 4, 1);
        cnt = rx_stream_u32(s);
        if ((int32_t)cnt > 0) {
            rx_ld_vec_resize_empty(g + 0xc, cnt, 1, 0);
            rx_stream_read(s, U32(g, 0x18), cnt);
        }
    }
    for (int32_t i = 0; i < n; i++) {
        uint32_t type = rx_stream_u32(s);
        float m[16]; for (int k = 0; k < 16; k++) m[k] = rx_stream_f32(s);
        if (v3) rx_stream_read(s, U32(g, 0x30) + 4u * (uint32_t)i, 4);
        float len[3];
        for (int r = 0; r < 3; r++) {
            float x = m[4 * r], y = m[4 * r + 1], z = m[4 * r + 2], w = m[4 * r + 3];
            float xx = x * x, yy = y * y, zz = z * z; (void)w;
            len[r] = rx_sqrts(rx_adds(rx_adds(xx, zz), yy));
        }
        for (int r = 0; r < 3; r++) for (int k = 0; k < 4; k++) m[4 * r + k] = rx_divs(m[4 * r + k], len[r]);
        float l0 = len[0], l1 = len[1], l2 = len[2];
        rx_addr shape = 0;
        if (type == 5) {
            shape = kind11(s);
            float h0 = F32(shape, 0x50), h1 = F32(shape, 0x54), h2 = F32(shape, 0x58), h3 = F32(shape, 0x5c);
            st_v4(shape + 0x50, rx_muls(l0, h0), rx_muls(l1, h1), rx_muls(l2, h2), rx_muls(l2, h3));
        } else if (type == 7) {
            shape = kind16(s);
            float r0 = F32(shape, 0xc);
            float avg = rx_divs(rx_adds(rx_adds(l1, l0), l2), 3.0f /* 0x92fe48 */);
            SF32(shape, 0xc, rx_muls(r0, avg));
        } else if (type == 8) {
            shape = kind17(s);
            float h = F32(shape, 0x48);
            SF32(shape, 0x48, rx_muls(h, l1));
            float rr = F32(shape, 0x44);
            float f = rx_muls(rx_adds(l2, l0), 0.5f /* 0x9322cc */);
            SF32(shape, 0x44, rx_muls(rr, f));
        }
        SU32(U32(g, 0x48), 4u * (uint32_t)i, shape);
        rx_addr dm = U32(g, 0x60) + 0x40u * (uint32_t)i;
        for (int k = 0; k < 16; k++) SF32(dm, 4 * k, m[k]);
        st_xyzz(U32(g, 0x90) + 0x10u * (uint32_t)i, m[12], m[13], m[14]);
        /* Matrix3_ToQuaternionToEsi 00401740 on the frame copy of the rows */
        RX_FRAME_ENTER(F, 0x40);
        for (int k = 0; k < 16; k++) SF32(F, 4 * k, m[k]);
        rx_world_matrix3_to_quaternion_to(U32(g, 0x78) + 0x10u * (uint32_t)i, F);
        RX_FRAME_LEAVE(F, 0x40);
    }
    return g;
}

/* FUN_0081b3c0 (ESI = stream): the header test; sets *version */
static int header_ok(rx_stream* s, uint32_t* version) {
    uint32_t magic = rx_stream_u32(s);
    char tag[4]; rx_stream_read_host(s, tag, 4);
    *version = rx_stream_u32(s);
    return magic == 0x4d2 && !strcmp(tag, "col") && (*version == 1 || *version == 2 || *version == 3);
}

/* FUN_0081b460 (EAX = stream) */
rx_addr rx_collision_geometry_load(rx_stream* s) {
    uint32_t version = 1;
    if (!header_ok(s, &version)) return 0;
    uint32_t kind = rx_stream_u32(s);
    uint8_t flag = 0;
    rx_addr g;
    switch (kind) {
    case 0: case 2: case 11: case 15: case 16: case 17: case 22: break;
    default: return 0;
    }
    if (version >= 2) flag = (uint8_t)rx_stream_u32(s);
    switch (kind) {
    case 2: g = kind2(s); break;
    case 11: g = kind11(s); break;
    case 15: g = kind15(s, version >= 3); break;
    case 16: g = kind16(s); break;
    case 17: g = kind17(s); break;
    case 22: g = kind22(s); break;
    default: fprintf(stderr, "rxcore: collision geometry kind %u is not ported (FUN_0081a1e0)\n", kind); abort();
    }
    SU8(g, 8, flag);
    return g;
}

/* CollisionAssetIO::vf3 00818ef0 */
rx_addr rx_collision_asset_io_create(rx_stream* s) {
    rx_addr a = rx_reflex_aligned_alloc(0x14);
    if (a) { SU32(a, 8, 0); SU32(a, 0xc, 0); SU32(a, 0x10, 0); SU32(a, 0, 0x00974f28); }
    rx_addr g = rx_collision_geometry_load(s);
    rx_ms_collision_geometry(g, rx_ms_load());   /* MAP SCALE (port): a level asset's geometry scaled; nothing at 1 */
    SU32(a, 0x10, g);
    return a;
}
