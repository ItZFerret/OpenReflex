/* build_perf.c -- see build_perf.h. Transliteration of PerformanceAssetIO::vf3 0083ff90, FUN_0083f670 and FUN_0083fe30. The XMLReader of
 * FUN_0083f670 is a temporary on the game stack (destroyed at the end). */
#include "build_perf.h"
#include "build_alloc.h"
#include "build_xml.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static uint32_t x87_f32(uint32_t b) { if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu) && !(b & 0x00400000u)) b |= 0x00400000u; return b; }

#define TICK rx_bf(0x3d088889u)   /* 0x977470 (1/30 as the exe stores it; not the float nearest 0.0333333) */

/* the SSE round-to-nearest-even-then-floor sequence (add / subtract 2^23 with the sign, minus 1 where it rounded up) */
static float floor_sse(float x) {
    uint32_t sgn = rx_fb(x) & 0x80000000u;
    float c = rx_bf(rx_fb(8388608.0f /* 0x9774f0 */) | sgn);
    float r = (x + c) - c;
    float diff = r - x;
    float mask = (diff > rx_bf(sgn)) ? 1.0f : 0.0f;       /* cmpnless diff, +-0 ; andps with 1.0 */
    return r - mask;
}

static int32_t attr_int(rx_addr b, rx_addr node, const char* name) {
    RX_FRAME_ENTER(T, 0x10);
    SU32(T, 0, 0);
    rx_xml_attr_ints(b, rx_xml_find_attr(b, node, name), T, 1);
    int32_t v = I32(T, 0);
    RX_FRAME_LEAVE(T, 0x10);
    return v;
}
static float attr_float(rx_addr b, rx_addr node, const char* name) {
    RX_FRAME_ENTER(T, 0x10);
    SU32(T, 0, 0);
    rx_xml_attr_floats(b, rx_xml_find_attr(b, node, name), T, 1);
    float v = F32(T, 0);
    RX_FRAME_LEAVE(T, 0x10);
    return v;
}
static void attr_bool(rx_addr b, rx_addr node, const char* name, rx_addr dst) {
    rx_xml_attr_bools(b, rx_xml_find_attr(b, node, name), dst, 1);
}

/* FUN_0083fe30 (stack: asset): floor(max over blocks of (timeScale * duration + start) * 1000) * 0.001 */
static float performance_end(rx_addr a) {
    float m = 0.0f;
    for (uint32_t i = 0; i < U32(a, 0x14); i++) {
        rx_addr r = U32(a, 0x18) + 0x70 * i;
        float v = F32(r, 0x48) * F32(r, 0x44) + F32(r, 0x40);
        if (v > m) m = v;
    }
    return floor_sse(m * 1000.0f /* 0x9cf5b0 */) * 0.001f /* 0x97743c */;
}

/* FUN_0083f670 (ECX = stream, stack: asset) */
static void performance_load(rx_addr a, rx_stream* s) {
    RX_FRAME_ENTER(R, 0x470);
    rx_addr rd = R;
    rx_xml_reader_construct(rd);
    rx_xml_reader_load(rd, s);
    rx_addr root = rx_xml_root(rd);
    rx_addr blocks = rx_xml_find_child(rd, root, "AnimBlocks");
    rx_xml_attr_ints(rd, rx_xml_find_attr(rd, blocks, "num"), a + 0x14, 1);
    uint32_t n = U32(a, 0x14);
    rx_addr recs = rx_reflex_aligned_alloc(rx_mul_sat(n, 0x70));          /* new Record[n] (Fn_0082ccf0 each) */
    if (recs) for (uint32_t i = 0; i < n; i++) {
        rx_addr r = recs + 0x70 * i;
        for (uint32_t k = 0; k < 0x40; k++) SU8(r, k, 0);
        SF32(r, 0x40, 0.0f); SF32(r, 0x44, 0.0f); SF32(r, 0x48, 0.0f); SF32(r, 0x4c, 0.0f); SF32(r, 0x50, 0.0f); SF32(r, 0x54, 0.0f);
        SF32(r, 0x58, 1.0f); SF32(r, 0x5c, 0.0f); SF32(r, 0x60, 1.0f); SU32(r, 0x64, 0); SU32(r, 0x68, 0); SU32(r, 0x6c, 0);
    }
    SU32(a, 0x18, recs);
    uint32_t off = 0;
    for (rx_addr blk = rx_xml_find_child(rd, blocks, "AnimBlock"); blk; blk = rx_xml_next_child(rd, blk), off += 0x70) {
        rx_addr r = U32(a, 0x18) + off;
        rx_addr nm = rx_xml_find_attr(rd, blk, "name");
        {   char tmp[0x100]; rx_xml_attr_string(rd, nm, tmp, sizeof tmp);
            uint32_t k = 0; do { SU8(r, k, (uint8_t)tmp[k]); } while (tmp[k++]); }
        SF32(r, 0x40, (float)attr_int(rd, blk, "start") * TICK);
        SF32(r, 0x44, (float)attr_int(rd, blk, "duration") * TICK);
        SF32(r, 0x48, attr_float(rd, blk, "timeScale"));
        attr_bool(rd, blk, "additive", r + 0x64);
        attr_bool(rd, blk, "looping", r + 0x6c);
        attr_bool(rd, blk, "override", r + 0x68);
        SF32(r, 0x4c, (float)attr_int(rd, blk, "cropFront") * TICK);
        SF32(r, 0x50, (float)attr_int(rd, blk, "cropBack") * TICK);
        SF32(r, 0x58, attr_float(rd, blk, "addMax"));
        SF32(r, 0x5c, attr_float(rd, blk, "addIn"));
        SF32(r, 0x60, attr_float(rd, blk, "addOut"));
        float t = F32(r, 0x48) * F32(r, 0x44) + F32(r, 0x40);
        SF32(r, 0x54, t);
        SF32(r, 0x54, F32(r, 0x54) * 1000.0f);
        SF32(r, 0x54, floor_sse(F32(r, 0x54)));
        SF32(r, 0x54, F32(r, 0x54) * 0.001f /* 0x97743c */);
    }
    rx_addr marks = rx_xml_find_child(rd, root, "HashMarks");
    rx_xml_attr_ints(rd, rx_xml_find_attr(rd, marks, "num"), a + 0x1c, 1);
    uint32_t m = U32(a, 0x1c);
    if (m) {
        rx_addr hm = rx_reflex_aligned_alloc(rx_mul_sat(m, 0xc));
        if (hm) for (uint32_t i = 0; i < m; i++) { SF32(hm + 0xc * i, 4, 0.0f); SU32(hm + 0xc * i, 8, 0xffffffffu); }
        SU32(a, 0x20, hm);
    } else SU32(a, 0x20, 0);
    off = 0;
    for (rx_addr h = rx_xml_find_child(rd, marks, "HashMark"); h; h = rx_xml_next_child(rd, h), off += 0xc) {
        rx_addr r = U32(a, 0x20) + off;
        SF32(r, 4, (float)attr_int(rd, h, "tick") * TICK);
        rx_xml_attr_ints(rd, rx_xml_find_attr(rd, h, "type"), r + 8, 1);
        rx_addr d = rx_xml_find_attr(rd, h, "duration");
        if (d) SF32(r, 0, attr_float(rd, h, "duration") * TICK);
        else SF32(r, 0, 0.25f /* 0x92ff84 */);
    }
    rx_addr attrs = rx_xml_find_child(rd, root, "Attributes");
    rx_xml_attr_bools(rd, rx_xml_find_attr(rd, attrs, "Mirrored"), a + 0x10, 1);
    float end = performance_end(a);
    SF32(a, 0x24, end); SF32(a, 0x2c, 0.0f); SF32(a, 0x28, end); SF32(a, 0x30, end);
    for (uint32_t i = 0; i < U32(a, 0x1c); i++) {
        rx_addr r = U32(a, 0x20) + 0xc * i;
        switch (U32(r, 8)) {
        case 10: SU32(a, 0x2c, x87_f32(U32(r, 4))); break;
        case 11: SU32(a, 0x28, x87_f32(U32(r, 4))); break;
        case 12: SU32(a, 0x30, x87_f32(U32(r, 4))); break;
        default: break;
        }
    }
    rx_xml_reader_destroy(rd);
    RX_FRAME_LEAVE(R, 0x470);
}

/* PerformanceAssetIO::vf3 0083ff90 */
rx_addr rx_perf_asset_io_create(rx_stream* s) {
    rx_addr a = rx_reflex_aligned_alloc(0x34);
    if (a) { SU32(a, 8, 0); SU32(a, 0xc, 0); SU32(a, 0x10, 0); SU32(a, 0, 0x00975fec); }
    performance_load(a, s);
    return a;
}
