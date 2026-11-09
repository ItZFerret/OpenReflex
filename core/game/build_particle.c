/* build_particle.c -- the particle asset: ParticleAssetIO::vf3 007e3a10, ParticleAsset's constructor 007e1820 (which builds the 12 x 11
 * curve LUT at 0xa817b0 from the basis functions 007e3510 .. 007e3940) and ParticleAsset::Parse 007e1a40, plus the frame rect builder
 * 007e3460 and the destructor 007e18f0. Hand transliteration from the disassembly; notes/fx.md "The particle asset".
 *
 * Readable port: the XMLReader of the original is the core's (build_xml.c). The parser reads the same fields in the same order; the
 * 0x1f0-byte emitter records are filled at the original's offsets. The x87 computations use src/rx_x87.h at the loader thread's
 * precision (53 bits, the process default) and the SSE polynomials use plain float in the original's order. */
#include "build_particle.h"
#include "build_alloc.h"
#include "build_xml.h"
#include "build_asset.h"
#include "loop_platform.h"
#include "build_render_asset.h"
#include "../src/rx_d3d.h"
#include "../src/rx_x87.h"
#include "../src/rx_x86.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PA_VT 0x00972934u
#define CURVE_LUT 0x00a817b0u

/* ---------------------------------------------------------------------------------------------------------------------------
 * the curve basis functions (007e3510 .. 007e3940): each takes one float and returns st(0) (x87, 53-bit precision as the loader
 * thread runs them). The SSE parts are plain float in the original's order. */

static rx_f80 ff(float x) { return rx_f80_from_f32(x); }
static rx_f80 fmul(rx_f80 a, float b) { return rx_f80_mul(a, rx_f80_from_f32(b), 53, 0); }
static rx_f80 fadd(rx_f80 a, float b) { return rx_f80_add(a, rx_f80_from_f32(b), 53, 0); }
static rx_f80 fsub(rx_f80 a, float b) { return rx_f80_sub(a, rx_f80_from_f32(b), 53, 0); }
static float ftop(rx_f80 a) { return rx_f80_to_f32(a, 0); }

static void rx_particle_frame_rect(rx_addr dst, float arg1, float arg2, float arg3, float arg4);

/* 007e3510 */
static float curve_0(float x) {
    const float r = rx_rcp(((x * rx_bf(0x3ef0b092u) /* 0.47009712 */ - rx_bf(0x3f1824efu) /* 0.59431356 */) * x +
                            rx_bf(0x3edec5d3u) /* 0.43510303 */) * x + rx_bf(0x3ce863e6u) /* 0.028367948 */);
    rx_f80 a = fmul(ff(x), 0.66761327f /* 0x977618 */);
    a = fsub(a, 1.3874407f /* 0x97760c */);
    a = fmul(a, x);
    a = fadd(a, 0.7179731f /* 0x977600 */);
    a = fmul(a, x);
    a = fadd(a, 0.0019466943f /* 0x9775fc */);
    return ftop(fmul(a, r));
}
/* 007e3580 */
static float curve_1(float x) {
    const float r = rx_rcp(x * rx_bf(0x3f30ac64u) /* 0.69013047 */ + rx_bf(0x3ed0ba32u) /* 0.40767056 */);
    rx_f80 a = fmul(ff(x), -0.40734333f /* 0x9775f8 */);
    a = fadd(a, 0.4075132f /* 0x9775ec */);
    return ftop(fmul(a, r));
}
/* 007e35d0 */
static float curve_2(float x) {
    const float r = rx_rcp(x * rx_bf(0x3f30ac64u) + rx_bf(0x3ed0ba32u));
    rx_f80 a = fmul(ff(x), -0.40734333f);
    a = fadd(a, 0.4075132f);
    a = fmul(a, r);
    return ftop(rx_f80_sub(rx_f80_from_f32(1.0f /* 0x931f24 */), a, 53, 0));
}
/* 007e3620 */
static float curve_3(float x) {
    const float u = 1.0f - x;
    const float r = rx_rcp(u * rx_bf(0x3f30ac64u) + rx_bf(0x3ed0ba32u));
    rx_f80 a = fmul(ff(u), -0.40734333f);
    a = fadd(a, 0.4075132f);
    a = fmul(a, r);
    return ftop(rx_f80_sub(rx_f80_from_f32(1.0f), a, 53, 0));
}
/* 007e3680: ((x * -0.7872178 + 0.33504853) * x - 0.19239031) rcps; ((x * -0.20063592 + 0.39173824) * x - 0.19233876) * rcp,
 * clamped: >= 1 -> 1, <= 0 -> 0 (comiss order: NaN keeps the value) */
static float curve_4(float x) {
    const float r = rx_rcp((x * -0.7872178f /* 0x9775e8 */ + 0.33504853f /* 0x9775e4 */) * x - 0.19239031f /* 0x9775e0 */);
    float v = (x * -0.20063592f /* 0x9775dc */ + 0.39173824f /* 0x9775d8 */) * x - 0.19233876f /* 0x9775d4 */;
    v = v * r;
    if (!(v < 1.0f)) v = 1.0f;
    else if (!(v > 0.0f)) v = 0.0f;
    return v;
}
/* 007e3720: 1 - (2x - 1)^2 */
static float curve_5(float x) {
    rx_f80 a = rx_f80_add(ff(x), ff(x), 53, 0);
    a = rx_f80_sub(a, rx_f80_from_f32(1.0f), 53, 0);
    a = rx_f80_mul(a, a, 53, 0);
    return ftop(rx_f80_sub(rx_f80_from_f32(1.0f), a, 53, 0));
}
/* 007e3740 */
static float curve_6(float x) {
    const float r = rx_rcp((x * -0.65338516f /* 0x9775cc */ + 0.6490748f /* 0x9775c8 */) * x - 0.21425326f /* 0x9775c0 */);
    rx_f80 a = fmul(ff(x), 0.21219122f /* 0x9775d0 */);
    a = fsub(a, 0.21213108f /* 0x9775c4 */);
    a = fmul(a, x);
    return ftop(fmul(a, r));
}
/* 007e37a0 */
static float curve_7(float x) {
    const float u = 1.0f - x;
    const float r = rx_rcp(u * rx_bf(0x3f30ac64u) + rx_bf(0x3ed0ba32u));
    rx_f80 a = fmul(ff(u), -0.40734333f);
    a = fadd(a, 0.4075132f);
    return ftop(fmul(a, r));
}
/* 007e3800 */
static float curve_8(float x) { return ftop(rx_f80_sub(rx_f80_from_f32(1.0f), ff(x), 53, 0)); }
/* 007e3810 */
static float curve_9(float x) { return x; }
/* the shared polynomial of 007e3890 / 007e3940 / 007e3820 */
static float curve_edge(float f) {
    const float r = rx_rcp((f * -0.2688322f /* 0x9775bc */ + 0.567458f /* 0x9775b8 */) * f - 0.33775666f /* 0x9775b4 */);
    return ((f * -0.23391953f /* 0x9775b0 */ + 0.5693428f /* 0x9775ac */) * f - 0.33581486f /* 0x9775a8 */) * r;
}
/* 007e3890 */
static float curve_10(float x) {
    if (!(x < 0.875f /* 0x9776cc */)) return curve_edge((x - 0.875f) * 8.0f /* 0x9322b8 */);
    return 1.0f;
}
/* 007e3940: below 0.125 (0x9c4754) the mirrored edge; at or above 0.875 the edge; between: 1 */
static float curve_11(float x) {
    const float lo = F32(0, 0x009c4754);
    if (!(x < lo)) {
        if (!(x < 0.875f)) return curve_edge((x - 0.875f) * 8.0f);
        return 1.0f;
    }
    return curve_edge((lo - x) * 8.0f);
}

/* 007e1820 (ECX = the asset): the vtable and the zero fields, then the 12 x 11 curve LUT at 0xa817b0: row r = basis r evaluated at
 * i / 9 (i = 0..10), clamped to [0, 1] as the ctor does (>= 1 -> 1, <= 0 -> 0, NaN -> 0). */
rx_addr rx_particle_asset_construct(rx_addr a) {
    SU32(a, 8, 0); SU32(a, 0xc, 0); SU32(a, 0, PA_VT); SU32(a, 0x10, 0); SU32(a, 0x14, 0); SU32(a, 0x18, 0); SU32(a, 0x1c, 0);
    float (*const curves[12])(float) = {curve_0, curve_1, curve_2, curve_3, curve_4, curve_5,
                                       curve_6, curve_7, curve_8, curve_9, curve_10, curve_11};
    for (int r = 0; r < 12; r++) {
        for (int i = 0; i < 11; i++) {
            const float x = (float)i * rx_bf(0x3de38e39u) /* 0x92e210: 1/9 */;
            float v = curves[r](x);
            if (!(v < 1.0f)) v = 1.0f;
            else if (!(v > 0.0f)) v = 0.0f;
            SF32(CURVE_LUT, 0x2c * (uint32_t)r + 4u * (uint32_t)i, v);
        }
    }
    return a;
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * the XML value readers (the XMLReader virtuals' semantics on nodes / attributes; see build_xml.h) */

static int node_type(rx_addr n) { return n ? (int)U16(n, 4) : -1; }

/* vf8 (node, NULL, &cursor) / vf10 (cursor): any next sibling (the cursor's name is 0 = any) */
static rx_addr next_sibling_any(rx_addr c) {
    const rx_addr p = U32(c, 0x10);
    if (!p) return 0;
    const rx_addr first = U32(p, 0x14), n = U32(p, 0x18);
    for (uint32_t i = 0; i < n; i++) if (first + 0x24u * i == c) return i + 1 < n ? first + 0x24u * (i + 1) : 0;
    return 0;
}

/* host buffer -> game memory (the code never takes a host pointer into the arena) */
static void game_puts(rx_addr d, const char* s) { uint32_t i = 0; for (; s[i]; i++) SU8(d, i, (uint8_t)s[i]); SU8(d, i, 0); }
static void game_memset(rx_addr d, uint8_t v, uint32_t n) { for (uint32_t i = 0; i < n; i++) SU8(d, i, v); }

/* 00829640 ParseCommaFloatList / 008296e0 ParseCommaFloat2List / 00829560 ParseCommaIntList / 008294e0 ParseCommaBoolList: work on
 * the game string; the core parses the same text into a host buffer (atof / strtol base 0) */
static void host_str(rx_addr s, char* out, size_t n) {
    size_t i = 0;
    for (; i + 1 < n && U8(s, (uint32_t)i); i++) out[i] = (char)U8(s, (uint32_t)i);
    out[i] = 0;
}
static void comma_floats(rx_addr s, float* out, int n) {
    char b[0x400]; host_str(s, b, sizeof b);
    const char* p = b;
    for (int i = 0; i < n; i++) {
        out[i] = (float)strtod(p, NULL);
        const char* c = strchr(p, ',');
        if (!c) { if (i != n - 1) return; p = p + strlen(p); }
        else p = c + 1;
    }
}
static void comma_floats2(rx_addr s, float* out, int n) {
    char b[0x400]; host_str(s, b, sizeof b);
    const char* p = b;
    for (int i = 0; i < n; i++) {
        out[2 * i] = (float)strtod(p, NULL);
        const char* c = strchr(p, ',');
        if (!c) return;
        out[2 * i + 1] = (float)strtod(c + 1, NULL);
        c = strchr(c + 1, ',');
        if (!c) { if (i != n - 1) return; p = p + strlen(p); }
        else p = c + 1;
    }
}
static void comma_ints(rx_addr s, int32_t* out, int n) {
    char b[0x400]; host_str(s, b, sizeof b);
    const char* p = b;
    for (int i = 0; i < n; i++) {
        if (*p == 'T' || *p == 't') out[i] = 1;
        else if (*p == 'F' || *p == 'f') out[i] = 0;
        else out[i] = (int32_t)strtol(p, NULL, 0);
        const char* c = strchr(p, ',');
        if (!c) { if (i != n - 1) return; p = p + strlen(p); }
        else p = c + 1;
    }
}
static void comma_bools(rx_addr s, int32_t* out, int n) {
    char b[0x400]; host_str(s, b, sizeof b);
    const char* p = b;
    for (int i = 0; i < n; i++) {
        if (*p == 0) return;
        out[i] = (*p == 'F' || *p == 'f' || *p == '0') ? 0 : 1;
        const char* c = strchr(p, ',');
        if (!c) { if (i != n - 1) return; p = p + strlen(p); }
        else {
            p = c + 1;
            while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
        }
    }
}
/* a node's float pair (type 9 raw / type 1 "a,b") */
static void node_f2(rx_addr n, float* out) {
    if (node_type(n) == 9) { for (int k = 0; k < 2; k++) out[k] = F32(U32(n, 8), 4u * (uint32_t)k); }
    else if (node_type(n) == 1) comma_floats2(U32(n, 8), out, 1);
}
/* a node's single float (type 5 raw / type 1 list) */
static void node_f1(rx_addr n, float* out) {
    if (node_type(n) == 5) *out = F32(U32(n, 8), 0);
    else if (node_type(n) == 1) comma_floats(U32(n, 8), out, 1);
}
/* a node's bool (type 0xb raw / type 1 list) -> 1 when the value is exactly 1 */
static int node_bool(rx_addr n, int def) {
    int32_t v = 0;
    if (node_type(n) == 0xb) v = I32(U32(n, 8), 0);
    else if (node_type(n) == 1) comma_bools(U32(n, 8), &v, 1);
    else return def;
    return v == 1;
}
/* a node's int (type 3 raw / type 1 list) */
static int32_t node_i32(rx_addr n) {
    int32_t v = 0;
    if (node_type(n) == 3) v = I32(U32(n, 8), 0);
    else if (node_type(n) == 1) comma_ints(U32(n, 8), &v, 1);
    return v;
}
/* a node's vec3 (type 0xa / type 1 "x,y,z") into dst (3 floats; the lane 3 is untouched) */
static void node_v3(rx_addr rd, rx_addr n, rx_addr dst) { rx_xml_vf_node_vec3(rd, n, dst, 1); }

/* the arrow-style env-gated tag helper: the field node of a name (-1 = 0) */
static rx_addr fld(rx_addr rd, rx_addr node, const char* name) { return rx_xml_find_child(rd, node, name); }

/* ---------------------------------------------------------------------------------------------------------------------------
 * ParticleAsset::Parse (007e1a40) */
void rx_particle_asset_parse(rx_addr a, rx_stream* s) {
    RX_FRAME_ENTER(R, 0x470);                         /* the XMLReader (0x460) + locals */
    const rx_addr rd = R;
    rx_xml_reader_construct(rd);
    rx_xml_reader_load(rd, s);
    const rx_addr root = rx_xml_root(rd);
    if (root == 0) { rx_xml_reader_destroy(rd); RX_FRAME_LEAVE(R, 0x470); return; }
    uint32_t version_bits = 0;
    rx_addr ver = rx_xml_find_attr(rd, root, "version");
    if (ver != 0) { RX_FRAME_ENTER(V, 0x10); rx_xml_attr_ints(rd, ver, V, 1); version_bits = U32(V, 0); RX_FRAME_LEAVE(V, 0x10); }
    const float version = rx_bf(version_bits);   /* vf77 stores the int; the original compares its bits as a float */
    const rx_addr list = (ver != 0 && version != 0.0f) ? rx_xml_find_child(rd, root, "ParticleEmitters") : root;

    /* count the children (vf8 with a NULL name, then vf10 while the same name) */
    SU32(a, 0x14, 0);
    for (rx_addr c = rx_xml_find_child(rd, list, NULL); c; c = next_sibling_any(c)) SU32(a, 0x14, U32(a, 0x14) + 1);
    const uint32_t n = U32(a, 0x14);
    rx_addr recs = rx_crt_aligned_malloc(rx_mul_sat(n, 0x1f0), 16);
    SU32(a, 0x10, recs);
    if (recs) game_memset(recs, 0, n * 0x1f0);

    uint32_t idx = 0;
    for (rx_addr node = rx_xml_find_child(rd, list, NULL); node; node = next_sibling_any(node)) {
        const rx_addr rec = U32(a, 0x10) + idx * 0x1f0u;
        idx++;
        /* the record's name = the emitter node's name (strncpy into a 0x103 buffer, then a CRT aligned copy) */
        char name[0x103];
        host_str(U32(node, 0), name, sizeof name);
        const size_t nl = strlen(name);
        rx_addr nm = rx_crt_aligned_malloc((uint32_t)nl + 1, 16);
        SU32(rec, 0, nm);
        if (nm) game_puts(nm, name);

        /* Use3D: a surface asset handle by name (cut at the extension); the render fields from the surface's index buffer */
        rx_addr use3d = fld(rd, node, "Use3D");
        if (use3d == 0) {
            SU32(rec, 8, 0); SU32(rec, 0xc, 0); SU32(rec, 0x10, 0); SU32(rec, 0x14, 0); SU32(rec, 0x18, 0); SU32(rec, 0x1c, 0);
            SU32(rec, 0x164, 0); SU32(rec, 4, 0);
        } else {
            SU32(rec, 0x164, 1);
            char surf[0x103]; surf[0] = 0;
            const int t = node_type(use3d);
            if (t == 0) surf[0] = 0;
            else if (t == 1) host_str(U32(use3d, 8), surf, sizeof surf);
            char* dot = strrchr(surf, '.');
            if (dot) *dot = 0;
            const rx_addr db = U32(U32(0, 0x00a95844), 8);
            RX_FRAME_ENTER(N, 0x110);
            game_puts(N, surf);
            rx_addr h = rx_database_vf_handle(db, N, 0x66727573u /* "surf" */, 0x656361u /* "ace" */, 1);
            RX_FRAME_LEAVE(N, 0x110);
            SU32(rec, 4, h);
            if (h) {
                SU32(h, 0x4c, 1);
                const rx_addr asset = U32(h, 0x44);
                if (asset) {
                    const rx_addr fn = U32(U32(asset, 0), 0x10);
                    rx_addr (*get)(rx_addr, uint32_t) = RX_FN(fn, rx_addr (*)(rx_addr, uint32_t));
                    const rx_addr surf_rec = get ? get(asset, 0) : 0;
                    rx_addr ib = surf_rec ? U32(surf_rec, 0x18) : 0;
                    int ok = 0;
                    if (ib) {
                        RX_FRAME_ENTER(D, 0x100);
                        uint32_t a2[2] = {ib, D + 0x8c};
                        rx_d3d_call(ib, 13, 2, a2);                       /* GetDesc(this, out): Format at out+0 */
                        uint32_t a5[5] = {ib, 0, 0, D + 0x80, 0};
                        rx_d3d_call(ib, 11, 5, a5);                       /* Lock(this, 0, 0, &data, 0) */
                        ok = U32(D, 0x8c) == 0x65;                        /* D3DFMT_INDEX16 */
                        if (ok) {
                            SU32(rec, 0x1c, U32(surf_rec, 8));
                            SU32(rec, 0x10, U32(surf_rec, 0x14));
                            SU32(rec, 0x14, U32(surf_rec, 0xc));
                            SU32(rec, 0x18, U32(surf_rec, 0x64));
                            SU32(rec, 8, U32(surf_rec, 0x24));
                            SU32(rec, 0xc, U32(surf_rec, 0x18));
                            if (U32(rec, 8)) { uint32_t a1[1] = {U32(rec, 8)}; rx_d3d_call(U32(rec, 8), 1, 1, a1); }      /* AddRef */
                            if (U32(rec, 0xc)) { uint32_t a1[1] = {U32(rec, 0xc)}; rx_d3d_call(U32(rec, 0xc), 1, 1, a1); }
                        } else {
                            SU32(rec, 8, 0); SU32(rec, 0xc, 0); SU32(rec, 0x10, 0); SU32(rec, 0x14, 0); SU32(rec, 0x18, 0); SU32(rec, 0x1c, 0);
                            SU32(rec, 0x164, 0); SU32(rec, 4, 0);
                        }
                        uint32_t a1[1] = {ib};
                        rx_d3d_call(ib, 12, 1, a1);                       /* Unlock(this) */
                        RX_FRAME_LEAVE(D, 0x100);
                    } else {
                        SU32(rec, 8, 0); SU32(rec, 0xc, 0); SU32(rec, 0x10, 0); SU32(rec, 0x14, 0); SU32(rec, 0x18, 0); SU32(rec, 0x1c, 0);
                        SU32(rec, 0x164, 0); SU32(rec, 4, 0);
                    }
                }
                SU32(h, 0x4c, 0);
            }
        }

        /* the two noise boxes: min at +0x20 / +0x50 / +0x70, extent at +0x30 / +0x60 / +0x80 */
        {
            rx_addr mn = fld(rd, node, "PositionNoiseBoxMin"), mx = fld(rd, node, "PositionNoiseBoxMax");
            if (mn && mx) {
                RX_FRAME_ENTER(T, 0x20);
                node_v3(rd, mn, T); node_v3(rd, mx, T + 0x10);
                SU32(rec, 0x20, U32(T, 0)); SU32(rec, 0x24, U32(T, 4)); SU32(rec, 0x28, U32(T, 8));
                SU32(rec, 0x30, rx_fb(rx_subs(F32(T + 0x10, 0), F32(T, 0))));
                SU32(rec, 0x34, rx_fb(rx_subs(F32(T + 0x10, 4), F32(T, 4))));
                SU32(rec, 0x38, rx_fb(rx_subs(F32(T + 0x10, 8), F32(T, 8))));
                RX_FRAME_LEAVE(T, 0x20);
            }
            mn = fld(rd, node, "VelocityNoiseBoxMin"); mx = fld(rd, node, "VelocityNoiseBoxMax");
            if (mn && mx) {
                RX_FRAME_ENTER(T, 0x20);
                node_v3(rd, mn, T); node_v3(rd, mx, T + 0x10);
                SU32(rec, 0x50, U32(T, 0)); SU32(rec, 0x54, U32(T, 4)); SU32(rec, 0x58, U32(T, 8));
                SU32(rec, 0x60, rx_fb(rx_subs(F32(T + 0x10, 0), F32(T, 0))));
                SU32(rec, 0x64, rx_fb(rx_subs(F32(T + 0x10, 4), F32(T, 4))));
                SU32(rec, 0x68, rx_fb(rx_subs(F32(T + 0x10, 8), F32(T, 8))));
                RX_FRAME_LEAVE(T, 0x20);
            }
            mn = fld(rd, node, "AccelerationNoiseBoxMin"); mx = fld(rd, node, "AccelerationNoiseBoxMax");
            if (mn && mx) {
                RX_FRAME_ENTER(T, 0x20);
                node_v3(rd, mn, T); node_v3(rd, mx, T + 0x10);
                SU32(rec, 0x70, U32(T, 0)); SU32(rec, 0x74, U32(T, 4)); SU32(rec, 0x78, U32(T, 8));
                SU32(rec, 0x80, rx_fb(rx_subs(F32(T + 0x10, 0), F32(T, 0))));
                SU32(rec, 0x84, rx_fb(rx_subs(F32(T + 0x10, 4), F32(T, 4))));
                SU32(rec, 0x88, rx_fb(rx_subs(F32(T + 0x10, 8), F32(T, 8))));
                RX_FRAME_LEAVE(T, 0x20);
            }
        }
        {
            float v[2];
            rx_addr f;
            if ((f = fld(rd, node, "BaseVelocity"))) node_v3(rd, f, rec + 0x40);
            v[0] = v[1] = 0.0f;
            if ((f = fld(rd, node, "AngleOfEmit"))) { node_f2(f, v); v[0] = v[0] * 0.01745329f /* 0x977678 */; v[1] = v[1] * 0.01745329f; SF32(rec, 0xec, v[0]); SF32(rec, 0xf0, rx_subs(v[1], v[0])); }
            v[0] = v[1] = 0.0f;
            if ((f = fld(rd, node, "YSpeed"))) { node_f2(f, v); SF32(rec, 0xf4, v[0]); SF32(rec, 0xf8, rx_subs(v[1], v[0])); }
            v[0] = v[1] = 0.0f;
            if ((f = fld(rd, node, "Speed"))) { node_f2(f, v); SF32(rec, 0xfc, v[0]); SF32(rec, 0x100, rx_subs(v[1], v[0])); }
            v[0] = v[1] = 0.0f;
            if ((f = fld(rd, node, "RotationPivot"))) { node_f2(f, v); SF32(rec, 0x98, v[0]); SF32(rec, 0x9c, rx_subs(v[1], v[0])); }
            f = fld(rd, node, "RotationAxis");
            SF32(rec, 0xa0, 0.0f); SF32(rec, 0xa4, 0.0f); SF32(rec, 0xa8, 0.0f);
            if (f) node_v3(rd, f, rec + 0xa0);
            v[0] = v[1] = 0.0f;
            if ((f = fld(rd, node, "Rotation"))) { node_f2(f, v); v[0] *= 0.01745329f; v[1] *= 0.01745329f; SF32(rec, 0xb0, v[0]); SF32(rec, 0xb4, rx_subs(v[1], v[0])); }
            v[0] = v[1] = 0.0f;
            if ((f = fld(rd, node, "RotationRate"))) { node_f2(f, v); v[0] *= 0.01745329f; v[1] *= 0.01745329f; SF32(rec, 0xb8, v[0]); SF32(rec, 0xbc, rx_subs(v[1], v[0])); }
            SU8(rec, 0xc0, 0);
            if ((f = fld(rd, node, "TrajectoryLock"))) SU8(rec, 0xc0, (uint8_t)node_bool(f, 0));
            v[0] = v[1] = 0.0f;
            if ((f = fld(rd, node, "Life"))) { node_f2(f, v); SF32(rec, 0xc4, v[0]); SF32(rec, 0xc8, rx_subs(v[1], v[0])); }
            v[0] = v[1] = 0.0f;
            if ((f = fld(rd, node, "FrameRange"))) { node_f2(f, v); SF32(rec, 0xcc, v[0]); SF32(rec, 0xd0, rx_subs(v[1], v[0])); }
            v[0] = v[1] = 0.0f;
            if ((f = fld(rd, node, "FrameStart"))) { node_f2(f, v); SF32(rec, 0xd4, v[0]); SF32(rec, 0xd8, rx_subs(v[1], v[0])); }
            v[0] = v[1] = 0.0f;
            if ((f = fld(rd, node, "FrameRate"))) { node_f2(f, v); SF32(rec, 0xdc, v[0]); SF32(rec, 0xe0, rx_subs(v[1], v[0])); }
            SF32(rec, 0xe4, 1.0f);
            if ((f = fld(rd, node, "FrameLooping"))) SF32(rec, 0xe4, node_bool(f, 0) ? 1.0f : 0.0f);
            v[0] = v[1] = 0.0f;
            if ((f = fld(rd, node, "EmitRate"))) { node_f2(f, v); SF32(rec, 0x104, v[0]); SF32(rec, 0x108, rx_subs(v[1], v[0])); }
            if ((f = fld(rd, node, "InheritVelocity"))) { float x = 0.0f; node_f1(f, &x); SF32(rec, 0xe8, x); }
            if ((f = fld(rd, node, "Gravity"))) { float x = 0.0f; node_f1(f, &x); SF32(rec, 0x74, rx_adds(F32(rec, 0x74), x)); }
        }
        {
            rx_addr cs = fld(rd, node, "ColorStart"), ce = fld(rd, node, "ColorEnd");
            if (cs && ce) {
                RX_FRAME_ENTER(T, 0x20);
                node_v3(rd, cs, T); node_v3(rd, ce, T + 0x10);
                SF32(rec, 0x140, F32(T, 0)); SF32(rec, 0x144, rx_subs(F32(T + 0x10, 0), F32(T, 0)));
                SF32(rec, 0x148, F32(T, 4)); SF32(rec, 0x14c, rx_subs(F32(T + 0x10, 4), F32(T, 4)));
                SF32(rec, 0x150, F32(T, 8)); SF32(rec, 0x154, rx_subs(F32(T + 0x10, 8), F32(T, 8)));
                RX_FRAME_LEAVE(T, 0x20);
            }
        }
        {
            float v[2]; rx_addr f;
            /* 007e277e: dec / neg / sbb / neg / dec = (v != 1) - 1: all ones when the value is 1 (w-lcgdraws: this was inverted, so
             * the colour draw of 00694ba0 / 0069a170 (T + 0x158 != 0) went to the emitters without ColorRandom) */
            if ((f = fld(rd, node, "ColorRandom"))) { SU32(rec, 0x158, node_bool(f, 0) ? 0xffffffffu : 0u); }
            else SU32(rec, 0x158, 0);
            v[0] = v[1] = 0.0f;
            if ((f = fld(rd, node, "ScaleRange"))) { node_f2(f, v); SF32(rec, 0x12c, v[0]); SF32(rec, 0x130, rx_subs(v[1], v[0])); }
            uint32_t sc = 0;
            if ((f = fld(rd, node, "ScaleCurve"))) { int32_t q = node_i32(f); sc = ((uint32_t)q < 0xc) ? (uint32_t)q : 0; }
            SU32(rec, 0x10c, sc * 0xb);
            float init = 1.0f;
            if ((f = fld(rd, node, "InitialScalar"))) { float x = 0.0f; node_f1(f, &x); if (x > 0.0f) init = x; }
            SF32(rec, 0x13c, init);
            float ratio[2] = {1.0f, 1.0f};
            if ((f = fld(rd, node, "ScaleRatio"))) node_f2(f, ratio);
            float var[2] = {0.0f, 0.0f};
            if ((f = fld(rd, node, "ScaleVariation"))) node_f2(f, var);
            const float rx = ratio[0] * var[0], ry = ratio[1] * var[1];
            SF32(rec, 0x118, rx_subs(ratio[0], rx));
            SF32(rec, 0x11c, rx_subs(ratio[1], ry));
            SF32(rec, 0x120, rx_adds(rx, rx));
            SF32(rec, 0x124, rx_adds(ry, ry));
            SU8(rec, 0x128, 0);
            if ((f = fld(rd, node, "ScaleRatioLocked"))) SU8(rec, 0x128, (uint8_t)node_bool(f, 0));
            v[0] = v[1] = 0.0f;
            if ((f = fld(rd, node, "AlphaRange"))) { node_f2(f, v); SF32(rec, 0x134, v[0]); SF32(rec, 0x138, rx_subs(v[1], v[0])); }
            uint32_t ac = 0;
            if ((f = fld(rd, node, "AlphaCurve"))) { int32_t q = node_i32(f); ac = ((uint32_t)q < 0xc) ? (uint32_t)q : 0; }
            SU32(rec, 0x110, ac * 0xb);
            SF32(rec, 0x15c, 0.0f);
            if ((f = fld(rd, node, "DragForceCoefficient"))) { float x = 0.0f; node_f1(f, &x); SF32(rec, 0x15c, x); }
            SU8(rec, 0x160, 0);
            if ((f = fld(rd, node, "Additive"))) SU8(rec, 0x160, (uint8_t)node_bool(f, 0));
            SU8(rec, 0x161, 0);
            if ((f = fld(rd, node, "UseColorMap"))) SU8(rec, 0x161, (uint8_t)node_bool(f, 0));
            /* the derived min / max pairs */
            SU32(rec, 0x168, U32(rec, 0x104));
            SF32(rec, 0x16c, rx_adds(F32(rec, 0x108), F32(rec, 0x104)));
            SU32(rec, 0x170, U32(rec, 0x20)); SU32(rec, 0x174, U32(rec, 0x24)); SU32(rec, 0x178, U32(rec, 0x28));
            SF32(rec, 0x184, rx_adds(F32(rec, 0x34), F32(rec, 0x24)));
            SF32(rec, 0x180, rx_adds(F32(rec, 0x20), F32(rec, 0x30)));
            SF32(rec, 0x188, rx_adds(F32(rec, 0x38), F32(rec, 0x28)));
            SU32(rec, 0x190, U32(rec, 0x40)); SU32(rec, 0x194, U32(rec, 0x44)); SU32(rec, 0x198, U32(rec, 0x48));
            SU32(rec, 0x1a0, U32(rec, 0x50)); SU32(rec, 0x1a4, U32(rec, 0x54)); SU32(rec, 0x1a8, U32(rec, 0x58));
            SF32(rec, 0x1b4, rx_adds(F32(rec, 0x64), F32(rec, 0x54)));
            SF32(rec, 0x1b0, rx_adds(F32(rec, 0x50), F32(rec, 0x60)));
            SF32(rec, 0x1b8, rx_adds(F32(rec, 0x68), F32(rec, 0x58)));
            SU32(rec, 0x1c0, U32(rec, 0xec)); SF32(rec, 0x1c4, rx_adds(F32(rec, 0xf0), F32(rec, 0xec)));
            SU32(rec, 0x1c8, U32(rec, 0xfc)); SF32(rec, 0x1cc, rx_adds(F32(rec, 0x100), F32(rec, 0xfc)));
            SU32(rec, 0x1d0, U32(rec, 0xf4)); SF32(rec, 0x1d4, rx_adds(F32(rec, 0xf8), F32(rec, 0xf4)));
            SU32(rec, 0x1d8, U32(rec, 0x118)); SU32(rec, 0x1dc, U32(rec, 0x11c));
            SF32(rec, 0x1e0, rx_adds(F32(rec, 0x120), F32(rec, 0x118)));
            SF32(rec, 0x1e4, rx_adds(F32(rec, 0x124), F32(rec, 0x11c)));
        }
    }

    /* the frame table: version 0 builds the default 8 x 8 grid; else ParticleFrames/Frame (Top, Bottom, Left, Right) */
    if (ver != 0 && version == 0.0f) {
        SU32(a, 0x1c, 0x40);
        rx_addr fr = rx_crt_aligned_malloc(0x40 * 8, 16);
        SU32(a, 0x18, fr);
        if (fr) game_memset(fr, 0, 0x40 * 8);
        const float step = F32(0, 0x009c4754);
        for (float v0 = 0.0f; ; ) {
            const float v1 = v0 + step;
            for (float u0 = 0.0f; ; ) {
                const float u1 = u0 + step;
                rx_particle_frame_rect(fr, u0, u1, v0, v1);
                fr += 8;
                u0 = u1;
                if (!(u1 < 1.0f)) break;
            }
            v0 = v1;
            if (!(v1 < 1.0f)) break;
        }
    } else {
        const rx_addr frames = rx_xml_find_child(rd, root, "ParticleFrames");
        SU32(a, 0x1c, 0);
        for (rx_addr f = rx_xml_find_child(rd, frames, "Frame"); f; f = rx_xml_next_child(rd, f)) SU32(a, 0x1c, U32(a, 0x1c) + 1);
        const uint32_t nf = U32(a, 0x1c);
        rx_addr fr = rx_crt_aligned_malloc(rx_mul_sat(nf, 8), 16);
        SU32(a, 0x18, fr);
        if (fr) game_memset(fr, 0, nf * 8);
        for (rx_addr f = rx_xml_find_child(rd, frames, "Frame"); f; f = rx_xml_next_child(rd, f)) {
            float top = 0.0f, bottom = 0.0f, left = 0.0f, right = 0.0f;
            rx_addr e = rx_xml_find_attr(rd, f, "Top");
            if (e) { if (U16(e, 0xa) == 5) top = F32(U32(e, 4), 0); else if (U16(e, 0xa) == 1) comma_floats(U32(U32(rd, 0xc), 4u * U32(e, 4)), &top, 1); }
            e = rx_xml_find_attr(rd, f, "Bottom");
            if (e) { if (U16(e, 0xa) == 5) bottom = F32(U32(e, 4), 0); else if (U16(e, 0xa) == 1) comma_floats(U32(U32(rd, 0xc), 4u * U32(e, 4)), &bottom, 1); }
            e = rx_xml_find_attr(rd, f, "Left");
            if (e) { if (U16(e, 0xa) == 5) left = F32(U32(e, 4), 0); else if (U16(e, 0xa) == 1) comma_floats(U32(U32(rd, 0xc), 4u * U32(e, 4)), &left, 1); }
            e = rx_xml_find_attr(rd, f, "Right");
            if (e) { if (U16(e, 0xa) == 5) right = F32(U32(e, 4), 0); else if (U16(e, 0xa) == 1) comma_floats(U32(U32(rd, 0xc), 4u * U32(e, 4)), &right, 1); }
            rx_particle_frame_rect(fr, top, bottom, left, right);   /* the call at 007e30f6 pushes (Top, Bottom, Left, Right): the table is {Left, Top, Right - Left, Bottom - Top} = (u offset, v offset, u scale, v scale) as 00699e70 copies it into the instance and UncompressedNoLightVS reads it (uv = corner * v1.xy + v1.zw) */
            fr += 8;
        }
    }

    /* the colour palette (16 words, default 0xffffffff) and the luminances */
    for (int i = 0; i < 16; i++) SU32(a, 0x20 + 4 * i, 0xffffffffu);
    {
        const rx_addr pal = rx_xml_find_child(rd, root, "ColorMapPalette");
        rx_addr w = a + 0x20;
        for (rx_addr c = rx_xml_find_child(rd, pal, "Color"); c; c = rx_xml_next_child(rd, c)) {
            float col[4] = {0, 0, 0, 0};
            if (node_type(c) == 1) {
                char b[0x400]; host_str(U32(c, 8), b, sizeof b);
                const char* p = b;
                for (int k = 0; k < 4; k++) {
                    col[k] = (float)strtod(p, NULL);
                    const char* q = strchr(p, ',');
                    if (!q) break;
                    p = q + 1;
                }
            } else if (node_type(c) == 7) {
                for (int k = 0; k < 4; k++) col[k] = F32(U32(c, 8), 4u * (uint32_t)k);
            }
            const uint32_t v = ((uint32_t)(int32_t)col[3] << 24) | ((uint32_t)(int32_t)col[0] << 16) |
                               ((uint32_t)(int32_t)col[1] << 8) | (uint32_t)(int32_t)col[2];
            SU32(w, 0, v);
            w += 4;
        }
    }
    for (int i = 0; i < 4; i++) SF32(a, 0x60 + 4 * i, 0.2f /* 0x3e4ccccd */);
    {
        rx_addr e = rx_xml_find_child(rd, root, "ShadowLuminance");
        if (e) {
            if (node_type(e) == 5) { const float v = F32(U32(e, 8), 0); for (int i = 0; i < 4; i++) SF32(a, 0x60 + 4 * i, v); }
            else if (node_type(e) == 7) { for (int i = 0; i < 4; i++) SU32(a, 0x60 + 4 * i, U32(U32(e, 8), 4u * (uint32_t)i)); }
        }
    }
    for (int i = 0; i < 4; i++) SF32(a, 0x70 + 4 * i, 1.0f /* 0x931f24 */);
    {
        rx_addr e = rx_xml_find_child(rd, root, "ParticleLuminance");
        if (e) {
            float v = 1.0f;
            if (node_type(e) == 1) comma_floats(U32(e, 8), &v, 1);
            else if (node_type(e) == 5) v = F32(U32(e, 8), 0);
            for (int i = 0; i < 4; i++) SF32(a, 0x70 + 4 * i, v);
        }
    }
    rx_xml_reader_destroy(rd);
    RX_FRAME_LEAVE(R, 0x470);
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * 007e3460 (stack arg1, arg2, arg3, arg4; ESI = the 8-byte destination): four half floats {arg3, arg1, arg4 - arg3, arg2 - arg1}
 * (D3DXFloat32To16Array's round-to-nearest-even conversion). The entry is {u offset, v offset, u scale, v scale} (00699e70 copies it into
 * the instance's TEXCOORD0 as (scale, offset) and UncompressedNoLightVS draws uv = corner * v1.xy + v1.zw), so arg3 / arg4 are the U range
 * and arg1 / arg2 the V range. The XML call (007e30f6) pushes (Top, Bottom, Left, Right); the default grid (007e2e6e) pushes (v0, v1, u0, u1)
 * of its loops below. */
static uint16_t f32_to_f16(float v) {
    const uint32_t b = rx_fb(v), s = (b >> 16) & 0x8000u;
    const int32_t e = (int32_t)((b >> 23) & 0xffu) - 127 + 15;
    const uint32_t m = b & 0x7fffffu;
    if (((b >> 23) & 0xffu) == 0xffu) return (uint16_t)(s | 0x7c00u | (m ? 0x200u | (m >> 13) : 0));
    if (e >= 0x1f) return (uint16_t)(s | 0x7c00u);
    if (e <= 0) {
        if (e < -10) return (uint16_t)s;
        uint32_t mm = m | 0x800000u;
        const int shift = 14 - e;
        uint32_t r = mm >> shift;
        const uint32_t rem = mm & ((1u << shift) - 1u);
        const uint32_t half = 1u << (shift - 1);
        if (rem > half || (rem == half && (r & 1u))) r++;
        return (uint16_t)(s | r);
    }
    uint32_t r = (uint32_t)e << 10 | (m >> 13);
    const uint32_t rem = m & 0x1fffu;
    if (rem > 0x1000u || (rem == 0x1000u && (r & 1u))) r++;
    return (uint16_t)(s | r);
}
static void rx_particle_frame_rect(rx_addr dst, float arg1, float arg2, float arg3, float arg4) {
    SU16(dst, 0, f32_to_f16(arg3));
    SU16(dst, 2, f32_to_f16(arg1));
    SU16(dst, 4, f32_to_f16(rx_subs(arg4, arg3)));
    SU16(dst, 6, f32_to_f16(rx_subs(arg2, arg1)));
}

/* 007e18f0 (ECX = the asset): release the emitter records (name, Use3D surface buffers), the frame table; then the asset itself */
void rx_particle_asset_destroy(rx_addr a) {
    SU32(a, 0, PA_VT);
    if (U32(a, 0x10)) {
        if (I32(a, 0x14) > 0) {
            for (uint32_t i = 0; i < U32(a, 0x14); i++) {
                const rx_addr r = U32(a, 0x10) + 0x1f0u * i;
                if (U32(r, 8)) { const rx_addr b = U32(r, 8); const rx_addr fn = U32(U32(b, 0), 8); uint32_t (*rel)(rx_addr, uint32_t) = RX_FN(fn, uint32_t (*)(rx_addr, uint32_t)); if (rel) rel(b, 1); }
                if (U32(r, 0xc)) { const rx_addr b = U32(r, 0xc); const rx_addr fn = U32(U32(b, 0), 8); uint32_t (*rel)(rx_addr, uint32_t) = RX_FN(fn, uint32_t (*)(rx_addr, uint32_t)); if (rel) rel(b, 1); }
                if (U32(r, 0)) rx_crt_aligned_free(U32(r, 0));
            }
            rx_crt_aligned_free(U32(a, 0x10));
        }
    }
    if (U32(a, 0x18)) rx_crt_aligned_free(U32(a, 0x18));
}

/* ParticleAsset vf1 007e1810 (the tag) and the AssetIO vf1 007e3a50 (the handle) are in build_asset_types.c's table. */
/* ParticleAssetIO::vf3 007e3a10: ReflexAlignedAlloc(0x80), the constructor, Parse(stream) */
rx_addr rx_particle_asset_create(rx_stream* s) {
    rx_addr a = rx_reflex_aligned_alloc(0x80);
    if (!a) return 0;
    rx_particle_asset_construct(a);
    rx_particle_asset_parse(a, s);
    return a;
}
