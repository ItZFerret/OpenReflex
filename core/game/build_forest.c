/* build_forest.c -- see build_forest.h. Hand transliteration of ForestAssetIO::vf3 00661f10, FUN_0065cbb0, XenonForestAsset_Parse 0065cd90,
 * ForestAsset_ParseTreeTypes 0065dd30 and ForestTreeType_BuildLodRecords 0065eb00 from the disassembly (and the decompile), checked against
 * Unity/Reflex/Assets/Scripts/Content/ReflexForest.cs (verified float for float against the running game). SSE single precision in the
 * original's operation order; the x87 parts (the LOD thresholds 1 / n, the first leaf transition) are computed in double (the 53-bit
 * precision control of the game's threads) and stored as float; x87 fld / fstp dword copies quiet a signalling NaN. */
#include "build_forest.h"
#include "build_alloc.h"
#include "build_load.h"
#include "build_render.h"
#include "build_xml.h"
#include "map_scale.h"
#include "../src/rx_d3d.h"
#include "../src/rx_x86.h"
#include "crt.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    FA_CLUSTERS = 0x10, FA_NCLUSTERS = 0x1c210, FA_SHADOW_VS = 0x1d028, FA_SHADOW_PS = 0x1d02c, FA_TYPES = 0x1d030, FA_BBROWS = 0x246f0,
    FA_NTYPES = 0x24ad0, FA_BB_VS = 0x24ad4, FA_BB_PS = 0x24ad8, FA_INSTANCE_VB = 0x24adc, FA_ATLAS_DIFFUSE = 0x24ae0, FA_ATLAS_NORMAL = 0x24ae4,
    FA_TREES = 0x24ae8, FA_NTREES = 0x24aec, FA_BBHALF = 0x24af0, FA_BBCENTER = 0x24b00, FA_SIZE = 0x24b10
};
#define TYPE_SIZE 0x5f0u
#define MAX_TYPES 20u
#define MAX_CLUSTERS 900u
#define G_DECL_A    0x00a94194u
#define G_VB_A      0x00a94198u
#define G_VB_B      0x00a9419cu
#define G_DECL_B    0x00a941a0u
#define G_VB_CORNER 0x00a941a4u
#define G_IB_QUAD   0x00a941a8u

static void fatal(const char* what) { fprintf(stderr, "rxcore: forest asset: %s\n", what); abort(); }
static uint32_t x87_copy(uint32_t b) {
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu) && !(b & 0x00400000u)) b |= 0x00400000u;
    return b;
}
static void copy_f32(rx_addr dst, rx_addr src) { SU32(dst, 0, x87_copy(U32(src, 0))); }
static const float C329 = 0.329411775f;   /* 0x3ea8a8a9 (0x00977664) */
static const float C671 = 0.670588255f;   /* 0x3f2babac (0x00977710) */

/* ---- the XMLReader queries (the stateless forms of vf7 / vf8 / vf10 / vf58 / vf68 / vf76 / vf77 / vf79); `sc` is a game-memory scratch ---- */
typedef struct { rx_addr rd, a, sc; } fctx;
static rx_addr attr_cstr(const fctx* c, rx_addr e) { return U32(U32(c->rd, 0xc), 4 * U32(e, 4)); }          /* vf68 */
static rx_addr first_any_child(rx_addr node) { return U32(node, 0x18) ? U32(node, 0x14) : 0; }              /* vf8 with name 0 */
static rx_addr next_any_child(rx_addr prev) {                                                                  /* vf10 of that cursor */
    rx_addr parent = U32(prev, 0x10);
    rx_addr last = U32(parent, 0x14) + 0x24 * U32(parent, 0x18), n = prev + 0x24;
    return n < last ? n : 0;
}
static uint32_t read_int(const fctx* c, rx_addr e) {      /* vf77 into a local: 0 when the attribute is absent */
    if (!e) return 0;
    SU32(c->sc, 0, 0);
    rx_xml_attr_ints(c->rd, e, c->sc, 1);
    return U32(c->sc, 0);
}
static rx_addr str_handle(const char* type, rx_addr name) { return U8(name, 0) ? rx_load_find_handle(type, name) : 0; }

/* the handle of the asset named by attribute `attr` of `node` (0: no attribute or an empty name) */
static rx_addr attr_handle(const fctx* c, rx_addr node, const char* attr, const char* type) {
    rx_addr e = rx_xml_find_attr(c->rd, node, attr);
    return e ? str_handle(type, attr_cstr(c, e)) : 0;
}
/* the same for the attributes read with vf79 into a 0x40 byte buffer */
static rx_addr attr_handle_buf(const fctx* c, rx_addr node, const char* attr, const char* type) {
    rx_addr e = rx_xml_find_attr(c->rd, node, attr);
    if (!e) return 0;
    rx_xml_vf_attr_string(c->rd, e, c->sc, 0x40);
    return U8(c->sc, 0) ? rx_load_find_handle(type, c->sc) : 0;
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * ForestTreeType_BuildLodRecords 0065eb00 (EDX = the type; leafLodMethod 1 only) */

static void build_lod_records(rx_addr T) {
    rx_addr th = U32(T, 0);
    SU32(th, 0x4c, 1);
    rx_addr t = U32(th, 0x44);                       /* the TreeAsset */
    uint32_t nb = U32(t, 0x880), nf = U32(t, 0x8bc), nl = U32(t, 0x8f8), n3 = U32(t, 0x930);
    float near_ = F32(t, 0x10), far_ = F32(t, 0x14);
    if (U32(t, 0x830) != 1) fatal("leafLodMethod other than 1 (FUN_0065eb00's second path) is not ported");
    if (nb > 10 || nf > 10 || nl > 5 || n3 > 10) fatal("tree LOD counts");
    uint32_t amax = nb ? nb - 1 : 0, bmax = nf ? nf - 1 : 0, cmax = 0, dmax = n3 ? n3 - 1 : 0;
    float P[10][8], Q[10][8], C[12], Ab[12] = {0}, Bf[12] = {0}, Dl[12] = {0};
    uint32_t F[12] = {0};
    for (int i = 0; i < 10; i++) {
        float d[8] = {1.0f, 0.0f, 1.0f, 0.0f, C329, C329, 0.0f, 1.0f};
        memcpy(P[i], d, sizeof d); memcpy(Q[i], d, sizeof d);
    }
    for (int i = 0; i < 12; i++) C[i] = 0.0f;
    /* the thresholds i / n of the branch / frond / 3D leaf LODs: x87 1.0 / n, stored as float, then summed in SSE */
    uint32_t cnt[3] = {nb, nf, n3};
    float* arr[3] = {Ab, Bf, Dl};
    for (int k = 0; k < 3; k++) {
        uint32_t n = cnt[k], i = 0;
        if (n) {
            float inv = (float)(1.0 / (double)n), x = inv;
            for (; i + 1 < n; i++) { arr[k][i] = x; x = x + inv; }
        }
        arr[k][i] = 1.0f;
    }
    if (nl) {
        float crest = F32(t, 0x840), cl = F32(t, 0x848), tw = F32(t, 0x850), tf = F32(t, 0x82c), expo = F32(t, 0x838);
        float ltf = F32(t, 0x83c), radius = F32(t, 0x834);
        float a = (1.0f - crest) * cl, bb = cl - a;
        float tt = (float)(((1.0 - (double)tw) - (double)nl * (double)cl) + (double)a);     /* x87 */
        float R = far_ - near_, clR = cl * R, twc = tw / cl, omt = 1.0f - tf, tfo = tf / omt, inv = 1.0f / (omt * tw);
        uint32_t r = 0;
        for (uint32_t i = 0; i < nl; i++) {
            F[r] = 0;
            P[r][2] = 1.0f; P[r][3] = 0.0f; P[r][5] = 0.0f; P[r][6] = 0.0f; P[r][7] = 1.0f;
            Q[r][0] = 1.0f; Q[r][1] = 0.0f; Q[r][2] = 1.0f; Q[r][3] = 0.0f; Q[r][5] = 0.0f; Q[r][6] = 0.0f; Q[r][7] = 1.0f;
            float t1 = bb + tt, d0 = R * t1 + near_, d1 = clR + d0;
            C[r] = t1; F[r + 1] = 1;
            float g = 1.0f / (d1 - d0), h = g * d1 - twc;
            P[r][1] = 0.0f; Q[r][4] = C329; P[r][0] = 1.0f; P[r][4] = C329;
            Q[r + 1][0] = -0.0f - g; Q[r + 1][1] = h;
            P[r + 1][0] = -0.0f - g; P[r + 1][1] = h; P[r + 1][2] = -0.0f - inv;
            float ci = crest * inv;
            P[r + 1][3] = ci + 1.0f; Q[r + 1][2] = inv; Q[r + 1][3] = (-0.0f - ci) - tfo;
            P[r + 1][5] = C671; P[r + 1][6] = 0.0f; P[r + 1][7] = expo; P[r + 1][4] = C329;
            for (int j = 4; j < 8; j++) Q[r + 1][j] = P[r + 1][j];
            C[r + 1] = a + t1; tt = C[r + 1];
            r += 2;
        }
        uint32_t last = r - 1;
        float u = 1.0f / (((1.0f - ltf) * radius * 2.0f) * (far_ - near_));
        P[last][7] = expo; P[last][1] = far_ * u; P[last][2] = -1.0f; P[last][0] = -0.0f - u; P[last][3] = 1.0f;
        P[last][4] = C329; P[last][5] = C671; P[last][6] = 0.0f;
        cmax = last; C[last] = 1.0f;
    } else C[0] = 1.0f;
    const float eps = 0.0001f;   /* 0x009775a4 */
    uint32_t ia = 0, ib = 0, ic = 0, id = 0;
    while (eps > Ab[ia]) ia++;
    while (eps > Bf[ib]) ib++;
    while (eps > C[ic]) ic++;
    while (eps > Dl[id]) id++;
    SU32(T, 0x4c0, 0);
    for (;;) {
        float m = Ab[ia];
        if (!(Bf[ib] > m)) m = Bf[ib];
        if (!(C[ic] > m)) m = C[ic];
        if (!(Dl[id] > m)) m = Dl[id];
        uint32_t k = U32(T, 0x4c0);
        rx_addr rec = T + 0x60 + 0x70 * k;
        SF32(rec, 0x14, (far_ - near_) * m + near_);
        SU32(rec, 0x00, ia); SU32(rec, 0x04, ib); SU32(rec, 0x08, ic >> 1); SU32(rec, 0x0c, id);
        for (int j = 0; j < 8; j++) { SU32(rec, 0x20 + 4u * (uint32_t)j, x87_copy(rx_fb(P[ic][j]))); SU32(rec, 0x40 + 4u * (uint32_t)j, x87_copy(rx_fb(Q[ic][j]))); }
        SU32(T, 0x70 * (k + 1), F[ic]);     /* this record's transition flag at +0x10 = the next record's +0 */
        if (ia >= amax && ib >= bmax && ic >= cmax && id >= dmax) { SU32(T, 0x4c0, k + 1); break; }
        m = m + eps;
        if (m >= Ab[ia] && ia < amax) ia++;
        if (m >= Bf[ib] && ib < bmax) ib++;
        if (m >= C[ic] && ic < cmax) ic++;
        if (m >= Dl[id] && id < dmax) id++;
        SU32(T, 0x4c0, k + 1);
        if (k + 1 >= 10) break;
    }
    SU32(th, 0x4c, 0);
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * ForestAsset_ParseTreeTypes 0065dd30 (ECX = the TreeTypes node, ESI = the reader, stack: the asset) */

static void parse_tree_types(const fctx* c, rx_addr types) {
    rx_addr rd = c->rd, a = c->a;
    rx_addr en = rx_xml_find_attr(rd, types, "numTypes");
    SU32(a, FA_NTYPES, 0);
    if (en) rx_xml_attr_ints(rd, en, a + FA_NTYPES, 1);
    uint32_t ntypes = U32(a, FA_NTYPES);
    if ((int32_t)ntypes > (int32_t)MAX_TYPES) fatal("more than 20 tree types");
    rx_addr node = rx_xml_find_child(rd, types, "Type");
    for (uint32_t t = 0; (int32_t)t < (int32_t)ntypes; t++) {
        rx_addr T = a + FA_TYPES + TYPE_SIZE * t, row = a + FA_BBROWS + 0x30 * t;
        /* the asset handles */
        SU32(T, 0, attr_handle(c, node, "TreeAsset", "tree"));
        SU32(T, 4, attr_handle(c, node, "BranchMaterial", "material"));
        SU32(T, 8, attr_handle(c, node, "FrondMaterial", "material"));
        SU32(T, 0xc, attr_handle(c, node, "LeafMaterial", "material"));
        SU32(T, 0x10, attr_handle(c, node, "3DLeafMaterial", "material"));
        /* Shadow {texture, invProj} */
        rx_addr sh = rx_xml_find_child(rd, node, "Shadow"), inv = 0;
        int identity = 1;
        if (!sh) SU32(T, 0x4f4, 0);
        else {
            SU32(T, 0x4f4, attr_handle(c, sh, "texture", "texture"));
            inv = rx_xml_find_attr(rd, sh, "invProj");
            if (inv) {                                           /* vf69: a matrix (type 8) or a comma list (type 1) */
                uint32_t ty = U16(inv, 0xa);
                if (ty == 8) { for (uint32_t k = 0; k < 64; k += 4) SU32(T, 0x500 + k, U32(U32(inv, 4), k)); identity = 0; }
                else if (ty == 1) { rx_xml_attr_floats(rd, inv, T + 0x500, 16); identity = 0; }
            }
        }
        if (identity) {
            for (uint32_t k = 0; k < 16; k++) SF32(T, 0x500 + 4 * k, (k % 5 == 0) ? 1.0f : 0.0f);
        }
        SU32(T, 0x540, attr_handle_buf(c, node, "PhysicsBxmlAsset", "bxml"));
        SU32(T, 0x544, attr_handle_buf(c, node, "CollisionMAsset", "collis"));
        SU32(T, 0x548, attr_handle_buf(c, node, "CollisionFAsset", "collis"));
        SU32(T, 0x554, attr_handle_buf(c, node, "CrownCollisionAsset", "collis"));
        static const struct { const char* name; uint32_t off; } ints[] = {
            {"audioMaterial", 0x54c}, {"audioSubclass", 0x550}, {"CrownAudioMaterial", 0x558}, {"CrownAudioSubclass", 0x55c}};
        for (size_t i = 0; i < sizeof ints / sizeof ints[0]; i++) {
            rx_addr e = rx_xml_find_attr(rd, node, ints[i].name);
            SU32(T, ints[i].off, 0);
            if (e) rx_xml_attr_ints(rd, e, T + ints[i].off, 1);
        }
        SU8(T, 0x560, 0);
        rx_addr e = rx_xml_find_attr(rd, node, "CrownParticleEmitter");
        if (e) crt_strncpy(T + 0x560, attr_cstr(c, e), 0x40);
        SU8(T, 0x5a1, 0);
        e = rx_xml_find_attr(rd, node, "TrunkParticleEmitter");
        if (e) crt_strncpy(T + 0x5a1, attr_cstr(c, e), 0x40);
        SF32(T, 0x5e4, 0.0f);
        e = rx_xml_find_attr(rd, node, "TrunkParticleVelocityThreshold");
        if (e) rx_xml_attr_floats(rd, e, T + 0x5e4, 1);
        if (U32(T, 0)) build_lod_records(T);
        /* the per-type constants and the billboard row */
        SF32(row, 0x8, 0.0f);   /* +0x246f8 (the tree-less case keeps 0) */
        rx_addr tree = U32(T, 0), hx, cx;                /* hx / cx: the half extents / centre the row's size is made from */
        if (!tree) {
            SF32(row, 0x20, 1.0f); SF32(row, 0x24, 0.0f);      /* +0x24710 / +0x24714 */
            /* +0x14 = a copy of the float at T + 4 + 0x70 * (record count): the last record's distance (the count is unset without a tree) */
            uint32_t cnt = U32(T, 0x4c0);
            copy_f32(T + 0x14, T + 4 + 0x70 * (cnt > 10 ? 0 : cnt));
            hx = T + 0x4e0; cx = T + 0x4d0;
        } else {
            rx_addr ta = U32(tree, 0x44);
            SU32(tree, 0x4c, 1);
            for (uint32_t k = 0; k < 3; k++) SU32(T, 0x4d0 + 4 * k, x87_copy(U32(ta, 0x860 + 4 * k)));
            for (uint32_t k = 0; k < 3; k++) SU32(T, 0x4e0 + 4 * k, x87_copy(U32(ta, 0x870 + 4 * k)));
            float near_ = F32(ta, 0x10), far_ = F32(ta, 0x14);
            float f = ((1.0f - F32(ta, 0x83c)) * F32(ta, 0x834)) * 2.0f;
            SF32(T, 0x14, far_ - (far_ - near_) * f);
            float s = 1.5f / ((far_ - near_) * f);
            SF32(row, 0x20, s);
            SF32(row, 0x24, 1.5f - far_ * s);
            for (int w = 0; w < 2; w++) {                       /* branch (0x854) then frond (0x858) fade constants */
                uint32_t o = w ? 0x40 : 0x20;
                float fd = F32(ta, w ? 0x858 : 0x854);
                if (fd > 0.0f) {
                    float g = 1.0f / ((far_ - near_) * fd);
                    SF32(T, o + 0x00, g * -0.6666667f /* 0x00977718 */);
                    SF32(T, o + 0x04, far_ * g * 0.6666667f /* 0x00977714 */);
                    float h = 1.0f / ((((1.0f - F32(ta, 0x82c)) * (far_ - near_)) * F32(ta, 0x834)) * 2.0f);
                    SF32(T, o + 0x08, h);
                    SF32(T, o + 0x0c, 1.0f - far_ * h);
                    SF32(T, o + 0x10, -0.6666667f / fd);
                    copy_f32(T + o + 0x14, ta + 0x838);
                    SF32(T, o + 0x18, C671);
                } else {
                    SF32(T, o + 0x00, 0.0f); SF32(T, o + 0x04, 0.6666667f); SF32(T, o + 0x08, 0.0f); SF32(T, o + 0x0c, 1.0f);
                    SF32(T, o + 0x10, 0.0f); SF32(T, o + 0x14, 0.0f); SF32(T, o + 0x18, 0.0f);
                }
                SF32(T, o + 0x1c, C329);
            }
            copy_f32(row + 8, ta + 0x85c);                       /* +0x246f8 */
            SU32(tree, 0x4c, 0);
            hx = ta + 0x870; cx = ta + 0x860;
        }
        float sx = F32(hx, 0) + F32(cx, 0), sz = F32(hx, 8) + F32(cx, 8), sy = F32(hx, 4) + F32(cx, 4);
        SF32(row, 0x0, sqrtf(sz * sz + sx * sx) * 2.0f);
        SF32(row, 0x4, sy);
        SF32(row, 0x10, 0.0f);
        SF32(row, 0x14, (float)(int32_t)t / (float)(int32_t)ntypes);
        SF32(row, 0x18, 0.125f /* 0x009c4754 */);
        SF32(row, 0x1c, 1.0f / (float)(int32_t)ntypes);
        rx_addr bb = rx_xml_find_child(rd, node, "Billboard");
        if (bb) {
            static const struct { const char* name; uint32_t off; } fl[] = {
                {"width", 0x0}, {"height", 0x4}, {"uStartOffset", 0x10}, {"vStartOffset", 0x14}, {"uScale", 0x18}, {"vScale", 0x1c}};
            for (size_t i = 0; i < sizeof fl / sizeof fl[0]; i++) {
                rx_addr fe = rx_xml_find_attr(rd, bb, fl[i].name);
                if (fe) rx_xml_attr_floats(rd, fe, row + fl[i].off, 1);
            }
            rx_addr su = rx_xml_find_attr(rd, bb, "uStep");
            if (su) rx_xml_attr_floats(rd, su, row + 0xc, 1); else copy_f32(row + 0xc, row + 0x18);
            SU32(c->sc, 0, 8);
            rx_addr ce = rx_xml_find_attr(rd, bb, "count");
            if (ce) rx_xml_attr_ints(rd, ce, c->sc, 1);
            SF32(row, 0x28, (float)(int32_t)U32(c->sc, 0));
            SF32(row, 0x2c, 1.0f / (float)(int32_t)U32(c->sc, 0));
        }
        node = rx_xml_next_child(rd, node);
    }
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * the tree records: XenonForestAsset_Parse 0065daa4..0065dc20 */

/* rotation quantised to 45 degrees: x = rot * 4 / pi (0x00977da4), + 8 while negative, rounded to nearest with the 2^23 trick, one
 * subtracted when the rounding went up, * pi / 4 (0x009776e0) */
static float quantise_rotation(float rot) {
    float x = rot * 1.2732395f;
    while (0.0f > x) x = x + 8.0f;
    uint32_t sign = rx_fb(x) & 0x80000000u;
    float c = rx_bf(sign | 0x4b000000u), signf = rx_bf(sign);
    float m = (x + c) - c, t = m - x;
    if (!(t <= signf)) m = m - 1.0f;
    return m * 0.7853982f;
}
/* the game's own sin / cos polynomials (0065db11..0065dc20), single precision in this operation order; the coefficients are the exe's
 * float constants 0x00935d3c .. 0x00935d68 (bit patterns) */
#define BF(bits) rx_bf(bits)
static void tree_sincos(float r, float* sn, float* cs) {
    float sg = r >= 0.0f ? 1.0f : -1.0f;
    r = r * sg;
    float q = (float)rx_cvtt(r * 0.15915494f);
    float a = 3.1415927f - (r - q * 6.2831855f);
    float x = a * a, x2 = x * x;
    float cc = x * BF(0xb46beed0u) + BF(0x37cb076bu);
    cc = cc * x2;
    cc = cc + (x * BF(0xbab5a662u) + BF(0x3d2aa372u));
    cc = cc * x2;
    cc = cc + (x * BF(0xbeffff3du) + BF(0x3f7ffff3u));
    *cs = -0.0f - cc;
    float s = x * BF(0xb2aee90au) + BF(0x36352cc0u);
    s = s * x2;
    float t = x * BF(0xb94fb563u) + BF(0x3c0884a2u), x1 = x * BF(0xbe2aaa5cu) + BF(0x3f7ffff9u);
    s = s + t; s = s * x2; s = s + x1; s = s * a; s = s * sg;
    *sn = s;
}

/* FUN_0065cbb0 (EAX = the block) */
static void forest_asset_construct(rx_addr a) {
    SF32(a, 0x24ab0, 0.0f); SF32(a, 0x24ab4, 0.0f); SF32(a, 0x24ab8, 0.0f); SF32(a, 0x24abc, 0.0f);
    SF32(a, 0x24ac4, 0.0f); SF32(a, 0x24ac8, 0.0f);
    SU32(a, 8, 0); SU32(a, 0xc, 0); SU32(a, 0, 0x0095b554);
    SU32(a, FA_NTYPES, 0); SU32(a, FA_NTREES, 0); SU32(a, FA_TREES, 0); SU32(a, 0x1d024, 0); SU32(a, FA_BB_VS, 0); SU32(a, FA_BB_PS, 0);
    SU32(a, FA_SHADOW_VS, 0); SU32(a, FA_SHADOW_PS, 0); SU32(a, FA_INSTANCE_VB, 0);
    SF32(a, 0x24ac0, C329); SF32(a, 0x24acc, 1.0f);
}

static void addref(rx_addr o) { uint32_t g[1] = {o}; rx_d3d_call(o, 1, 1, g); }
static rx_addr lock_res(rx_addr res, rx_addr tmp) {
    uint32_t a[5] = {res, 0, 0, tmp, 0};
    rx_d3d_call(res, 11, 5, a);                      /* Lock(0, 0, &data, 0) */
    return U32(tmp, 0);
}
static void unlock_res(rx_addr res) { uint32_t a[1] = {res}; rx_d3d_call(res, 12, 1, a); }
static void set_elem(rx_addr e, uint32_t i, uint32_t stream, uint32_t off, uint32_t type, uint32_t method, uint32_t usage, uint32_t index) {
    SU16(e, 8 * i, stream); SU16(e, 8 * i + 2, off); SU8(e, 8 * i + 4, type); SU8(e, 8 * i + 5, method); SU8(e, 8 * i + 6, usage); SU8(e, 8 * i + 7, index);
}

/* the shared render objects of the forest (created by the first asset, AddRef'ed by every one): 0065d383..0065d773 */
static void render_objects(rx_addr dev, rx_addr elems, rx_addr tmp) {
    rx_addr cache = U32(U32(0, RX_RENDERER), 0x14);
    if (!U32(0, G_DECL_A)) {
        set_elem(elems, 0, 0, 0, 3, 0, 0, 0); set_elem(elems, 1, 1, 0, 0, 0, 0, 1); set_elem(elems, 2, 0xff, 0, 0x11, 0, 0, 0);
        SU32(0, G_DECL_A, rx_vertex_decl_get(elems, cache));
    }
    addref(U32(0, G_DECL_A));
    if (!U32(0, G_VB_A)) {
        rx_d3d_create_vb(dev, 0x60, 8, 0, 1, G_VB_A);
        rx_addr vb = U32(0, G_VB_A), p = lock_res(vb, tmp);
        static const float v[24] = {-1.0f, 1.0f, 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, -1.0f, 0.125f, 1.0f,
                                    -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 0.125f, 1.0f, 1.0f, 1.0f, 0.125f, 0.0f};
        for (uint32_t k = 0; k < 24; k++) SF32(p, 4 * k, v[k]);
        unlock_res(vb);
    }
    addref(U32(0, G_VB_A));
    if (!U32(0, G_DECL_B)) {
        set_elem(elems, 0, 1, 0, 3, 0, 0, 0); set_elem(elems, 1, 1, 0x10, 3, 0, 5, 0); set_elem(elems, 2, 1, 0x20, 3, 0, 5, 2);
        set_elem(elems, 3, 0, 0, 3, 0, 5, 1); set_elem(elems, 4, 0xff, 0, 0x11, 0, 0, 0);
        SU32(0, G_DECL_B, rx_vertex_decl_get(elems, cache));
    }
    addref(U32(0, G_DECL_B));
    if (!U32(0, G_VB_CORNER)) {
        rx_d3d_create_vb(dev, 0x40, 8, 0, 1, G_VB_CORNER);
        rx_addr vb = U32(0, G_VB_CORNER), p = lock_res(vb, tmp);
        static const float v[16] = {0.5f, 1.0f, 1.0f, 0.0f, 0.5f, 0.0f, 1.0f, 1.0f, -0.5f, 0.0f, 0.0f, 1.0f, -0.5f, 1.0f, 0.0f, 0.0f};
        for (uint32_t k = 0; k < 16; k++) SF32(p, 4 * k, v[k]);
        unlock_res(vb);
    }
    addref(U32(0, G_VB_CORNER));
    if (!U32(0, G_IB_QUAD)) {
        rx_d3d_create_ib(dev, 0xc, 8, 0x65, 1, G_IB_QUAD);
        rx_addr ib = U32(0, G_IB_QUAD), p = lock_res(ib, tmp);
        static const uint16_t idx[6] = {0, 1, 2, 2, 3, 0};
        for (uint32_t k = 0; k < 6; k++) SU16(p, 2 * k, idx[k]);
        unlock_res(ib);
    }
    addref(U32(0, G_IB_QUAD));
}

/* XenonForestAsset_Parse 0065cd90 (ECX = the asset, stack: the stream) */
static void forest_parse(rx_addr a, rx_stream* s) {
    RX_FRAME_ENTER(F, 0x800);
    fctx c = {F + 0x300, a, F + 0x20};
    rx_addr rd = c.rd, elems = F + 0x40, tmp = F + 0x10, dev;
    rx_xml_reader_construct(rd);
    rx_xml_reader_load(rd, s);
    rx_addr root = rx_xml_root(rd);
    /* the clusters' header */
    rx_addr clusters = rx_xml_find_child(rd, root, "Clusters");
    rx_xml_attr_ints(rd, rx_xml_find_attr(rd, clusters, "numClusters"), a + FA_NCLUSTERS, 1);
    rx_xml_attr_ints(rd, rx_xml_find_attr(rd, clusters, "totalNumTrees"), a + FA_NTREES, 1);
    rx_xml_attr_vec3(rd, rx_xml_find_attr(rd, clusters, "totalBBCenter"), a + FA_BBCENTER, 1);
    rx_xml_attr_vec3(rd, rx_xml_find_attr(rd, clusters, "totalBBHalf"), a + FA_BBHALF, 1);
    uint32_t ncl = U32(a, FA_NCLUSTERS);
    if (ncl > MAX_CLUSTERS) fatal("more than 900 clusters");
    /* the trees of each type, counted over all clusters */
    for (uint32_t k = 0; k < MAX_TYPES; k++) SU32(a, 0x1d4f4 + TYPE_SIZE * k, 0);
    for (rx_addr cl = first_any_child(clusters); cl; cl = next_any_child(cl)) {
        if (!read_int(&c, rx_xml_find_attr(rd, cl, "numTrees"))) continue;
        uint32_t k = 0;
        for (rx_addr to = rx_xml_find_child(rd, cl, "TreesOfType"); to; to = rx_xml_next_child(rd, to), k++) {
            if (k >= MAX_TYPES) fatal("TreesOfType beyond 20 types");
            SU32(a, 0x1d4f4 + TYPE_SIZE * k, U32(a, 0x1d4f4 + TYPE_SIZE * k) + read_int(&c, rx_xml_find_attr(rd, to, "numTrees")));
        }
    }
    rx_addr types = rx_xml_find_child(rd, root, "TreeTypes");
    parse_tree_types(&c, types);
    uint32_t ntypes = U32(a, FA_NTYPES), ntrees = U32(a, FA_NTREES);
    rx_addr trees = rx_crt_aligned_malloc(ntrees * 0x30u, 16);
    SU32(a, FA_TREES, trees);
    dev = rx_renderer_device();
    /* the billboard atlas textures */
    rx_addr atlas = rx_xml_find_child(rd, root, "BillboardAtlas");
    if (!atlas) { SU32(a, FA_ATLAS_DIFFUSE, 0); SU32(a, FA_ATLAS_NORMAL, 0); }
    else {
        SU32(a, FA_ATLAS_DIFFUSE, rx_load_find_handle("texture", attr_cstr(&c, rx_xml_find_attr(rd, atlas, "diffuse"))));
        SU32(a, FA_ATLAS_NORMAL, rx_load_find_handle("texture", attr_cstr(&c, rx_xml_find_attr(rd, atlas, "normal"))));
    }
    /* the TreeShader asset: ShadowBBVertexShader / ShadowBBPixelShader / BBVertexShader / BBPixelShader (AddRef'ed) */
    rx_addr sh = rx_load_find_handle_c("shader", "TreeShader");
    if (!sh || !U32(sh, 0x44)) fatal("the TreeShader shader asset is not loaded");
    rx_addr sa = U32(sh, 0x44);
    SU32(sh, 0x4c, 1);
    rx_shader_find_vs(sa, "ShadowBBVertexShader", a + FA_SHADOW_VS, 0);
    rx_shader_find_ps(sa, "ShadowBBPixelShader", a + FA_SHADOW_PS, 0);
    rx_shader_find_vs(sa, "BBVertexShader", a + FA_BB_VS, 0);
    rx_shader_find_ps(sa, "BBPixelShader", a + FA_BB_PS, 0);
    SU32(sh, 0x4c, 0);
    /* the render objects shared by all forests, the instance buffer */
    render_objects(dev, elems, tmp);
    rx_gpu_create_vb(ntrees * 0x30u, 0x208, a + FA_INSTANCE_VB, 0, 0, 0x0095ae88, 0x1c8);
    if (!U32(0, G_VB_B)) {
        rx_d3d_create_vb(dev, 0x960, 8, 0, 1, G_VB_B);
        rx_addr vb = U32(0, G_VB_B), p = lock_res(vb, tmp);
        for (int32_t i = 99; i >= 0; i--) {
            float f = (float)i + 0.1f /* 0x00931d94 */;
            for (uint32_t k = 0; k < 6; k++) SF32(p, 24u * (uint32_t)i + 4 * k, f);
        }
        unlock_res(vb);
    }
    addref(U32(0, G_VB_B));
    /* the clusters and their trees */
    rx_addr treecur = trees;
    uint32_t vboff = 0;
    rx_addr cl = first_any_child(rx_xml_find_child(rd, root, "Clusters"));
    for (uint32_t ci = 0; ci < ncl; ci++, cl = cl ? next_any_child(cl) : 0) {
        rx_addr rec = a + FA_CLUSTERS + 0x80 * ci;
        uint32_t n = read_int(&c, rx_xml_find_attr(rd, cl, "numTrees"));
        SU32(rec, 8, n);
        if (!n) { SU32(rec, 0, 0); continue; }
        uint32_t bytes = n * 0x30u;
        SU32(rec, 0, treecur); treecur += bytes;
        SU32(rec, 4, vboff); vboff += bytes;
        rx_xml_attr_vec3(rd, rx_xml_find_attr(rd, cl, "boundingBoxCenter"), rec + 0x60, 1);
        rx_xml_attr_vec3(rd, rx_xml_find_attr(rd, cl, "boundingBoxHalf"), rec + 0x70, 1);
        rx_addr to = rx_xml_find_child(rd, cl, "TreesOfType");
        uint32_t cum = 0;
        for (uint32_t t = 0; t < ntypes; t++) {
            SU32(rec, 0xc + 4 * t, cum);
            uint32_t nt = to ? read_int(&c, rx_xml_find_attr(rd, to, "numTrees")) : 0;
            if (nt) {
                rx_addr pos = U32(rx_xml_find_child(rd, to, "Positions"), 8), rot = U32(rx_xml_find_child(rd, to, "Rotations"), 8);
                rx_addr scl = U32(rx_xml_find_child(rd, to, "Scales"), 8);
                for (uint32_t i = 0; i < nt; i++) {
                    rx_addr tr = U32(rec, 0) + 0x30 * (cum + i);
                    SU32(tr, 0, x87_copy(U32(pos, 12 * i))); SU32(tr, 4, U32(pos, 12 * i + 4)); SU32(tr, 8, U32(pos, 12 * i + 8));
                    SF32(tr, 0xc, (float)t + 0.5f);
                    SU32(tr, 0x10, x87_copy(U32(scl, 8 * i + 4)));
                    SU32(tr, 0x20, 0); SU32(tr, 0x24, 0); SU32(tr, 0x28, 0); SU32(tr, 0x2c, 0);
                    float q = quantise_rotation(F32(rot, 4 * i)), sn, cs;
                    SF32(tr, 0x14, q);
                    tree_sincos(q, &sn, &cs);
                    SF32(tr, 0x1c, cs); SF32(tr, 0x18, sn);
                }
            }
            cum += nt;
            if (to) to = rx_xml_next_child(rd, to);
        }
        SU32(rec, 0xc + 4 * ntypes, cum);
    }
    {   /* MAP SCALE (port, map_scale.h): the trees stand where the scaled map puts them; the clusters' and the forest's boxes follow.
         * A tree keeps its own scale (+0x10): its trunk collision (a level collis asset) is scaled already (nothing at 1) */
        const float k = rx_ms_load();
        if (k != 1.0f) {
            rx_ms_v3(a, FA_BBCENTER, k); rx_ms_v3(a, FA_BBHALF, k);
            for (uint32_t ci = 0; ci < ncl; ci++) {
                rx_addr rec = a + FA_CLUSTERS + 0x80 * ci;
                rx_ms_v3(rec, 0x60, k); rx_ms_v3(rec, 0x70, k);
                for (uint32_t i = 0; i < U32(rec, 8); i++) rx_ms_v3(U32(rec, 0) + 0x30 * i, 0, k);
            }
        }
    }
    rx_xml_reader_destroy(rd);
    RX_FRAME_LEAVE(F, 0x800);
}

/* ForestAssetIO::vf3 00661f10 */
rx_addr rx_forest_asset_io_create(rx_stream* s) {
    rx_addr a = rx_reflex_aligned_alloc(FA_SIZE);
    if (a) forest_asset_construct(a);
    forest_parse(a, s);
    return a;
}
