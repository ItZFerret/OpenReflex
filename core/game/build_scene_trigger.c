/* build_scene_trigger.c -- MXTrigger (RSData ids 0x1028 / 0x1042 / 0x1043 / 0x107b / 0x1071 share the constructor 00409800): the trigger volumes. */
#include "build_scene_int.h"
#include "crt.h"

/* 4x4 inverse of the matrix at `m` as MXTrigger::vf16 00409b70 computes it (inlined cofactor expansion, the original's association of the
 * sums; generated from the disassembly 00409b97 .. 0040a409 by a line-by-line translation of the scalar SSE code): the cofactors go to
 * st[0x50 ..], 1 / determinant to st[0x3c]. Returns nothing; the callers transpose and scale. */
static void trigger_cofactors(rx_addr m, float* st) {
    float x0, x1, x2, x3, x4, x5, x6, x7;
    x6 = F32(m, 0x3c);
    x1 = F32(m, 0x28);
    x7 = F32(m, 0x38);
    x2 = F32(m, 0x2c);
    x5 = F32(m, 0x18);
    x0 = x1;
    x0 = x0 * x6;
    x3 = x2;
    x3 = x3 * x7;
    x0 = x0 - x3;
    st[28] = x0;
    x0 = F32(m, 0x1c);
    x4 = x5;
    x4 = x4 * x6;
    x5 = x5 * x2;
    x2 = F32(m, 0x8);
    x3 = x0;
    x0 = x0 * x1;
    x1 = F32(m, 0xc);
    x3 = x3 * x7;
    st[20] = x1;
    x1 = x2;
    x1 = x1 * x6;
    x6 = st[20];
    x6 = x6 * x7;
    x7 = st[20];
    x1 = x1 - x6;
    x6 = F32(m, 0x2c);
    st[48] = x2;
    x2 = x2 * x6;
    x6 = F32(m, 0x28);
    x7 = x7 * x6;
    x6 = F32(m, 0x1c);
    x2 = x2 - x7;
    x7 = st[48];
    x7 = x7 * x6;
    x6 = st[20];
    st[48] = x7;
    x7 = F32(m, 0x18);
    x6 = x6 * x7;
    x7 = st[48];
    x7 = x7 - x6;
    x6 = F32(m, 0x14);
    x4 = x4 - x3;
    x3 = F32(m, 0x0);
    st[48] = x7;
    x7 = x6;
    x7 = x7 * st[28];
    x6 = F32(m, 0x24);
    x6 = x6 * x4;
    x5 = x5 - x0;
    x0 = F32(m, 0x4);
    st[44] = x4;
    x4 = F32(m, 0x34);
    x7 = x7 - x6;
    x4 = x4 * x5;
    st[40] = x5;
    x7 = x7 + x4;
    x4 = F32(m, 0x24);
    x5 = x0;
    x5 = x5 * st[28];
    x6 = x1;
    x6 = x6 * x4;
    x4 = F32(m, 0x34);
    x5 = x5 - x6;
    x6 = x2;
    x7 = x7 * x3;
    x6 = x6 * x4;
    x5 = x5 + x6;
    x5 = x5 * F32(m, 0x10);
    x7 = x7 - x5;
    x5 = F32(m, 0x14);
    x6 = x0;
    x6 = x6 * st[44];
    x0 = x0 * st[40];
    x1 = x1 * x5;
    x6 = x6 - x1;
    x1 = st[48];
    x5 = x1;
    x5 = x5 * x4;
    x4 = F32(m, 0x14);
    x6 = x6 + x5;
    x6 = x6 * F32(m, 0x20);
    x7 = x7 + x6;
    x5 = F32(m, 0x3c);
    x6 = F32(m, 0x2c);
    x2 = x2 * x4;
    x0 = x0 - x2;
    x2 = F32(m, 0x24);
    x1 = x1 * x2;
    x0 = x0 + x1;
    x0 = x0 * F32(m, 0x30);
    x1 = F32(m, 0x28);
    x7 = x7 - x0;
    x0 = F32(0, 0x00931f24);
    x0 = x0 / x7;
    x7 = F32(m, 0x1c);
    x2 = x6;
    st[60] = x0;
    x0 = F32(m, 0x38);
    x7 = x7 * x0;
    x2 = x2 * x0;
    x0 = F32(m, 0x18);
    x0 = x0 * x6;
    x6 = F32(m, 0x1c);
    x1 = x1 * x5;
    x1 = x1 - x2;
    x2 = F32(m, 0x18);
    x2 = x2 * x5;
    x2 = x2 - x7;
    x7 = x6;
    x6 = F32(m, 0x28);
    x7 = x7 * x6;
    x6 = F32(m, 0x24);
    x0 = x0 - x7;
    x6 = x6 * x2;
    x7 = x4;
    x7 = x7 * x1;
    x7 = x7 - x6;
    x6 = F32(m, 0x34);
    x6 = x6 * x0;
    x7 = x7 + x6;
    x6 = F32(m, 0x10);
    x6 = x6 * x1;
    st[80] = x7;
    x7 = F32(m, 0x20);
    x7 = x7 * x2;
    x6 = x6 - x7;
    st[48] = x6;
    x6 = F32(m, 0x30);
    x6 = x6 * x0;
    st[56] = x0;
    x0 = st[48];
    x0 = x0 + x6;
    x6 = F32(0, 0x009774a8);
    x6 = x6 - x0;
    x0 = F32(m, 0x24);
    st[84] = x6;
    x6 = F32(m, 0x34);
    x7 = x6;
    x6 = F32(m, 0x2c);
    x7 = x7 * x6;
    x0 = x0 * x5;
    x6 = x4;
    x0 = x0 - x7;
    x6 = x6 * x5;
    x5 = F32(m, 0x34);
    st[44] = x0;
    x7 = x5;
    x5 = F32(m, 0x1c);
    x7 = x7 * x5;
    x6 = x6 - x7;
    st[32] = x6;
    x6 = F32(m, 0x2c);
    x7 = x4;
    x7 = x7 * x6;
    x6 = F32(m, 0x24);
    x6 = x6 * x5;
    x5 = F32(m, 0x30);
    x7 = x7 - x6;
    x7 = x7 * x5;
    x6 = st[32];
    x5 = x0;
    x0 = F32(m, 0x10);
    x5 = x5 * x0;
    x0 = F32(m, 0x20);
    x6 = x6 * x0;
    x5 = x5 - x6;
    x7 = x7 + x5;
    x5 = F32(m, 0x34);
    x6 = F32(m, 0x38);
    x0 = F32(m, 0x24);
    st[88] = x7;
    x0 = x0 * x6;
    x7 = x5;
    x5 = F32(m, 0x28);
    x7 = x7 * x5;
    x0 = x0 - x7;
    x5 = x4;
    x5 = x5 * x6;
    x6 = F32(m, 0x34);
    x7 = x6;
    x6 = F32(m, 0x18);
    x7 = x7 * x6;
    x5 = x5 - x7;
    st[40] = x5;
    x5 = F32(m, 0x28);
    x7 = x4;
    x7 = x7 * x5;
    x5 = F32(m, 0x24);
    x5 = x5 * x6;
    x6 = st[40];
    x7 = x7 - x5;
    x5 = F32(m, 0x30);
    x7 = x7 * x5;
    x5 = x0;
    st[52] = x0;
    x0 = F32(m, 0x10);
    x5 = x5 * x0;
    x0 = F32(m, 0x20);
    x6 = x6 * x0;
    x0 = F32(0, 0x009774a8);
    x5 = x5 - x6;
    x6 = F32(m, 0xc);
    x7 = x7 + x5;
    x5 = F32(m, 0x8);
    x0 = x0 - x7;
    x7 = F32(m, 0x3c);
    st[16] = x6;
    st[20] = x5;
    x5 = x5 * x7;
    x7 = x6;
    x6 = F32(m, 0x38);
    x7 = x7 * x6;
    x6 = st[20];
    x5 = x5 - x7;
    x7 = F32(m, 0x2c);
    x6 = x6 * x7;
    x7 = st[16];
    st[48] = x6;
    x6 = F32(m, 0x28);
    x7 = x7 * x6;
    x6 = st[48];
    x6 = x6 - x7;
    st[92] = x0;
    x0 = F32(m, 0x4);
    st[28] = x6;
    x6 = x0;
    st[24] = x5;
    x6 = x6 * x1;
    x1 = x1 * x3;
    x7 = x5;
    x5 = F32(m, 0x24);
    x7 = x7 * x5;
    x6 = x6 - x7;
    x5 = F32(m, 0x34);
    x7 = st[28];
    x7 = x7 * x5;
    x6 = x6 + x7;
    x5 = F32(0, 0x009774a8);
    x7 = x5;
    x7 = x7 - x6;
    x6 = F32(m, 0x20);
    st[96] = x7;
    x7 = st[24];
    x7 = x7 * x6;
    x1 = x1 - x7;
    x6 = F32(m, 0x30);
    x7 = st[28];
    x7 = x7 * x6;
    x1 = x1 + x7;
    x6 = F32(m, 0x3c);
    x7 = st[16];
    st[100] = x1;
    x1 = x0;
    x1 = x1 * x6;
    x6 = F32(m, 0x34);
    x7 = x7 * x6;
    x1 = x1 - x7;
    x7 = st[16];
    st[36] = x1;
    x1 = F32(m, 0x2c);
    x6 = x0;
    x6 = x6 * x1;
    x1 = F32(m, 0x24);
    x7 = x7 * x1;
    x1 = F32(m, 0x30);
    x6 = x6 - x7;
    x6 = x6 * x1;
    x1 = st[44];
    x7 = st[36];
    x1 = x1 * x3;
    st[48] = x1;
    x1 = F32(m, 0x20);
    x7 = x7 * x1;
    x1 = st[48];
    x1 = x1 - x7;
    x7 = st[20];
    x6 = x6 + x1;
    x1 = x5;
    x1 = x1 - x6;
    x6 = F32(m, 0x38);
    st[104] = x1;
    x1 = x0;
    x1 = x1 * x6;
    x6 = F32(m, 0x34);
    x7 = x7 * x6;
    x1 = x1 - x7;
    x7 = st[20];
    st[44] = x1;
    x1 = F32(m, 0x28);
    x6 = x0;
    x6 = x6 * x1;
    x1 = F32(m, 0x24);
    x7 = x7 * x1;
    x1 = F32(m, 0x30);
    x6 = x6 - x7;
    x7 = st[44];
    x6 = x6 * x1;
    x1 = st[52];
    x1 = x1 * x3;
    st[52] = x1;
    x1 = F32(m, 0x20);
    x7 = x7 * x1;
    x1 = st[52];
    x1 = x1 - x7;
    x6 = x6 + x1;
    x1 = st[20];
    st[108] = x6;
    x6 = F32(m, 0x1c);
    x1 = x1 * x6;
    x6 = F32(m, 0x18);
    x7 = st[16];
    x7 = x7 * x6;
    x1 = x1 - x7;
    x7 = st[24];
    x7 = x7 * x4;
    st[48] = x1;
    x6 = x0;
    x6 = x6 * x2;
    x6 = x6 - x7;
    x7 = x1;
    x1 = F32(m, 0x34);
    x7 = x7 * x1;
    x1 = st[24];
    x6 = x6 + x7;
    x7 = st[48];
    x2 = x2 * x3;
    st[112] = x6;
    x6 = F32(m, 0x10);
    x1 = x1 * x6;
    x2 = x2 - x1;
    x1 = F32(m, 0x30);
    x7 = x7 * x1;
    x2 = x2 + x7;
    x7 = x5;
    x7 = x7 - x2;
    x2 = F32(m, 0x1c);
    st[116] = x7;
    x7 = x0;
    x7 = x7 * x2;
    x2 = st[16];
    x2 = x2 * x4;
    x7 = x7 - x2;
    x2 = st[32];
    x7 = x7 * x1;
    x2 = x2 * x3;
    st[52] = x2;
    x2 = st[36];
    x2 = x2 * x6;
    x6 = st[52];
    x6 = x6 - x2;
    x2 = F32(m, 0x18);
    x7 = x7 + x6;
    x6 = F32(m, 0x10);
    st[120] = x7;
    x7 = x0;
    x0 = x0 * st[56];
    x7 = x7 * x2;
    x2 = st[20];
    x2 = x2 * x4;
    x7 = x7 - x2;
    x2 = st[44];
    x7 = x7 * x1;
    x1 = st[40];
    x1 = x1 * x3;
    x2 = x2 * x6;
    x6 = F32(m, 0x8);
    x1 = x1 - x2;
    x2 = F32(m, 0x24);
    x7 = x7 + x1;
    x1 = x5;
    x1 = x1 - x7;
    st[124] = x1;
    x1 = st[28];
    x1 = x1 * x4;
    x4 = F32(m, 0x28);
    x0 = x0 - x1;
    x1 = st[48];
    x1 = x1 * x2;
    x2 = F32(m, 0x1c);
    x0 = x0 + x1;
    x1 = x5;
    x1 = x1 - x0;
    x0 = F32(m, 0x18);
    st[128] = x1;
    x1 = F32(m, 0x2c);
    st[48] = x1;
    st[44] = x0;
    st[52] = x4;
    st[56] = x2;
    st[24] = x6;
    x6 = F32(m, 0x10);
    st[20] = x6;
    x6 = F32(m, 0x20);
    st[16] = x6;
    x7 = x2;
    x7 = x7 * x4;
    x6 = x0;
    x6 = x6 * x1;
    x6 = x6 - x7;
    x7 = st[24];
    x7 = x7 * x1;
    x1 = F32(m, 0xc);
    x6 = x6 * x3;
    x1 = x1 * x4;
    x4 = F32(m, 0xc);
    x7 = x7 - x1;
    x7 = x7 * st[20];
    x1 = st[24];
    x6 = x6 - x7;
    x1 = x1 * x2;
    x7 = x4;
    x7 = x7 * x0;
    x0 = F32(m, 0x24);
    x1 = x1 - x7;
    x1 = x1 * st[16];
    x6 = x6 + x1;
    x1 = F32(m, 0x14);
    st[132] = x6;
    st[28] = x0;
    x0 = F32(m, 0x4);
    x7 = st[28];
    x7 = x7 * x2;
    x6 = x1;
    x6 = x6 * st[48];
    x6 = x6 - x7;
    x7 = st[28];
    x7 = x7 * x4;
    x6 = x6 * x3;
    x2 = x0;
    x2 = x2 * st[48];
    x2 = x2 - x7;
    x2 = x2 * st[20];
    x6 = x6 - x2;
    x2 = x0;
    x2 = x2 * st[56];
    x7 = x1;
    x7 = x7 * x4;
    x4 = st[44];
    x2 = x2 - x7;
    x2 = x2 * st[16];
    x6 = x6 + x2;
    x2 = st[28];
    x5 = x5 - x6;
    st[136] = x5;
    x5 = st[52];
    x6 = x1;
    x6 = x6 * x5;
    x7 = x2;
    x7 = x7 * x4;
    x6 = x6 - x7;
    x6 = x6 * x3;
    x3 = st[24];
    x7 = x0;
    x7 = x7 * x5;
    x2 = x2 * x3;
    x0 = x0 * x4;
    x7 = x7 - x2;
    x7 = x7 * st[20];
    x1 = x1 * x3;
    x0 = x0 - x1;
    x0 = x0 * st[16];
    x6 = x6 - x7;
    x6 = x6 + x0;
    st[140] = x6;
    
    (void)x0; (void)x1; (void)x2; (void)x3; (void)x4; (void)x5; (void)x6; (void)x7;
}

/* MXTrigger (0x280 bytes): the constructor 00409800 (stdcall e): Entity fields, vtable 0x009376cc, +0x70 = 1 (sorted list), +0x90 list
 * {12 x 8 bytes, 0, 12, 5, max}, +0xa4 list {1 x 4 bytes, 0, 1, 1, max}, +0xb8 / +0xbc 0, +0xc0 byte 0, +0x100 / +0x104 / +0x108 0,
 * +0x20c 1, byte +0x210 1, byte +0x218 0, +0x214 0 */
rx_addr rx_trigger_construct(rx_addr e) {
    rx_entity_construct(e, 0x009376cc);
    for (uint32_t k = 0x90; k <= 0xb4; k += 4) SU32(e, k, 0);
    SU32(e, 0x20c, 1); SU8(e, 0x210, 1); SU32(e, 0xb8, 0); SU32(e, 0xbc, 0); SU8(e, 0x218, 0); SU8(e, 0xc0, 0); SU8(e, 0x108, 0);
    SU32(e, 0x104, 0); SU32(e, 0x100, 0); SU32(e, 0x70, 1);
    SU32(e, 0x90, rx_reflex_aligned_alloc(0xc * 8));
    SU32(e, 0x94, 0); SU32(e, 0x98, 0xc); SU32(e, 0x9c, 5); SU32(e, 0xa0, 0x7fffffff);
    SU32(e, 0xa4, rx_reflex_aligned_alloc(4));
    SU32(e, 0xb4, 0x7fffffff); SU32(e, 0xac, 1); SU32(e, 0xb0, 1); SU32(e, 0xa8, 0); SU32(e, 0x214, 0);
    return e;
}
rx_addr rx_trigger_create(void) { return rx_trigger_construct(rx_reflex_aligned_alloc(0x280)); }
/* the other trigger RSData (SectionRace 0x1042, WaypointGate 0x1043, TimedSection 0x107b: 0x290 bytes; Tutorial 0x1071: 0x280) share the
 * constructor 00409800 and the class */
rx_addr rx_trigger_create_sized(uint32_t size) { return rx_trigger_construct(rx_reflex_aligned_alloc(size)); }

static int ieq(const char* a, const char* b) { while (*a && (*a | 0x20) == (*b | 0x20)) { a++; b++; } return (*a | 0x20) == (*b | 0x20); }

/* MXTrigger::vf16 00409b70 (stdcall m; RET 4): the matrix, its inverse (cofactors transposed (FUN_00401760) and scaled by 1 / det) at
 * +0x240, the translation row copy at +0x220, the length of the sum of the three axes at +0x230 */
uint32_t rx_trigger_vf16(rx_addr e, rx_addr src) {
    scene_copy_words(e + 0x30, src, 16);
    float st[0x100];
    memset(st, 0, sizeof st);
    rx_addr m = scene_vcall(e, 0x38, 0, 0);
    trigger_cofactors(m, st);
    float inv = st[0x3c];
    for (uint32_t i = 0; i < 4; i++) for (uint32_t k = 0; k < 4; k++)
        SF32(e, 0x240 + 4 * (4 * k + i), st[0x50 + 4 * (4 * i + k)] * inv);
    SU32(e, 0x220, scene_x87c(U32(src, 0x30))); SU32(e, 0x224, U32(src, 0x34)); SU32(e, 0x228, U32(src, 0x38));
    rx_addr a = scene_vcall(e, 0x38, 0, 0);
    float vx = F32(a, 0), vy = F32(a, 4), vz = F32(a, 8);
    a = scene_vcall(e, 0x38, 0, 0);
    vx = F32(a, 0x10) + vx; vy = F32(a, 0x14) + vy; vz = F32(a, 0x18) + vz;
    a = scene_vcall(e, 0x38, 0, 0);
    vx = F32(a, 0x20) + vx; vz = F32(a, 0x28) + vz; vy = F32(a, 0x24) + vy;
    float l2 = (vx * vx + vz * vz) + vy * vy;
    SF32(e, 0x230, rx_sqrts(l2));
    return 0;
}
RX_FUNCTION(0x00409b70, rx_trigger_vf16)

/* MXTrigger::vf5 00409a00 */
/* FUN_0078fcf0 (EAX = haystack, EDI = needle): the first place of the needle in the haystack, case-insensitive (the lower-case table
 * 0x009b77c8), or 0 */
static rx_addr ci_strstr(rx_addr hay, rx_addr needle) {
    const uint8_t first = U8(0, 0x009b77c8 + U8(needle, 0));
    for (rx_addr p = hay; U8(p, 0); p++) {
        if (U8(0, 0x009b77c8 + U8(p, 0)) != first) continue;
        rx_addr n = needle + 1, h = p + 1;
        for (;;) {
            uint8_t c = U8(n, 0);
            if (!c) return p;
            if (U8(0, 0x009b77c8 + U8(h, 0)) != U8(0, 0x009b77c8 + c)) break;
            n++; h++;
        }
    }
    return 0;
}
static rx_addr c_strstr(rx_addr hay, const char* needle) {   /* msvcr90 strstr */
    uint32_t nl = (uint32_t)strlen(needle);
    for (rx_addr p = hay; ; p++) {
        uint32_t k = 0;
        while (k < nl && U8(p, k) == (uint8_t)needle[k]) k++;
        if (k == nl) return p;
        if (!U8(p, 0)) return 0;
    }
}
static uint32_t guest_strlen(rx_addr s) { uint32_t n = 0; while (U8(s, n)) n++; return n; }
/* FUN_00409560 (stdcall config, text; RET 8): the LevelConfig of a trigger (config = trigger + 0x100: +0 "is a challenge", +4 the
 * number / variant letter, +8 the name): "Exhibition<Name><n>" -> the name cut after the matching entry of the table 0x009cb9c8 (31
 * entries) and the number after it; "Challenge<Name><L>" -> flag 1, the name of the matching entry of the table 0x009cbac0 (11 entries)
 * and the letter after it (L - 'A') */
void rx_trigger_level_config(rx_addr cfg, rx_addr text) {
    rx_addr ex = c_strstr(text, "Exhibition");
    if (ex) {
        rx_addr src = ex + 10;
        for (uint32_t k = 0; ; k++) { SU8(cfg, 8 + k, U8(src, k)); if (!U8(src, k)) break; }
        rx_addr needle = 0, found = 0;
        for (uint32_t k = 0; k < 0x7c; k += 4) {
            needle = U32(0, 0x009cb9c8 + k);
            found = ci_strstr(cfg + 8, needle);
            if (found) break;
        }
        uint32_t nl = guest_strlen(needle);
        if (!found) { fprintf(stderr, "rxcore: scene: LevelConfig '%s' matches no exhibition level\n", (const char*)0); abort(); }
        uint32_t v = (uint32_t)crt_atoi(found + nl);
        SU32(cfg, 4, v);
        SU8(cfg, 8 + nl, 0);
        return;
    }
    if (!c_strstr(text, "Challenge")) return;
    rx_addr needle = 0, found = 0;
    for (uint32_t k = 0; k < 0x2c; k += 4) {
        needle = U32(0, 0x009cbac0 + k);
        found = ci_strstr(text, needle);
        if (found) { SU32(cfg, 0, 1); break; }
    }
    for (uint32_t k = 0; ; k++) { SU8(cfg, 8 + k, U8(needle, k)); if (!U8(needle, k)) break; }
    uint32_t nl = guest_strlen(needle);
    if (!found) { SU32(cfg, 4, 0); SU8(cfg, 8 + nl, 0); return; }
    if (guest_strlen(found) > nl) { SU32(cfg, 4, (uint32_t)((int32_t)(int8_t)U8(found, nl) - 0x41)); SU8(cfg, 8 + nl, 0); return; }
    SU32(cfg, 4, 0);
    SU8(cfg, 8 + nl, 0);
}
RX_FUNCTION(0x00409560, rx_trigger_level_config)

uint32_t rx_trigger_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    rx_entity_vf5(e);
    const char* tt = rs_enum_text("TriggerType");
    if (!tt) tt = "";
    if (ieq(tt, "Box")) SU32(e, 0xb8, 0);
    else if (ieq(tt, "Sphere")) SU32(e, 0xb8, 1);
    else if (ieq(tt, "Cylinder")) SU32(e, 0xb8, 2);
    const char* te = rs_enum_text("TriggerEntityType");
    if (!te) te = "";
    if (ieq(te, "Vehicle")) SU32(e, 0xbc, 0);
    else if (ieq(te, "PhysicsObject")) SU32(e, 0xbc, 1);
    const char* id = rs_str("TriggerID");
    for (uint32_t k = 0;; k++) { SU8(e, 0xc0 + k, (uint8_t)id[k]); if (!id[k]) break; }
    if (rs_has("LevelConfig", RS_ENUM)) {
        const char* lc = rs_enum_text("LevelConfig");
        if (lc && lc[0]) {
            RX_FRAME_ENTER(F, 0x40);   /* the original's 0x40-byte local copy of the text */
            uint32_t n = (uint32_t)strlen(lc);
            for (uint32_t k = 0; k <= n; k++) SU8(F, k, (uint8_t)lc[k]);
            rx_trigger_level_config(e + 0x100, F);
            RX_FRAME_LEAVE(F, 0x40);
        }
    }
    uint32_t f = rs_uint("InverseTickFreq");
    SU32(e, 0x20c, f);
    if (f < 1) SU32(e, 0x20c, 1);
    return 0;
}
RX_FUNCTION(0x00409a00, rx_trigger_vf5)
