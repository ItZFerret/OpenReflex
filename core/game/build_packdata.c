/* build_packdata.c -- see build_packdata.h. Transliteration of PackedDataAssetIO::vf3, FUN_007f84d0, PackedDataAsset::vf3 007f8630 and the
 * two layouts FUN_007f8700 (interleaved) / FUN_007f88f0 (separate maps). */
#include "build_packdata.h"
#include "build_alloc.h"
#include "build_xml.h"
#include <string.h>

static uint32_t x87_u32_to_f32(uint32_t u) {      /* fild dword; fadd 2^32 when negative; fstp dword: the float nearest the unsigned value */
    double d = (double)(int32_t)u; if ((int32_t)u < 0) d += 4294967296.0;
    return rx_fb((float)d);
}

/* a child node's uint value into dst + the float of it */
static void read_uint_node(rx_addr rd, rx_addr root, const char* name, rx_addr dst, rx_addr fdst) {
    rx_addr n = rx_xml_find_child(rd, root, name);
    if (n) rx_xml_node_uints(rd, n, dst, 1);
    if (fdst) SU32(fdst, 0, x87_u32_to_f32(U32(dst, 0)));
}
static void read_data(rx_addr rd, rx_addr root, const char* name, rx_addr a, uint32_t block_off, uint32_t words) {
    rx_addr dst = rx_reflex_aligned_alloc(words * 4);
    SU32(a, block_off, dst);
    rx_addr n = rx_xml_find_child(rd, root, name);
    for (uint32_t k = 0; k < words * 4; k++) SU8(dst, k, U8(U32(n, 8), k));    /* memcpy from the node's value */
}

/* FUN_007f8700 (EDI = asset, ESI = reader, stack root) */
static void layout_interleaved(rx_addr rd, rx_addr a, rx_addr root) {
    read_uint_node(rd, root, "Width", a + 0x18, a + 0x38);
    read_uint_node(rd, root, "Height", a + 0x28, a + 0x48);
    read_uint_node(rd, root, "NumBits1", a + 0x68, 0);
    read_uint_node(rd, root, "NumBits2", a + 0x6c, 0);
    read_uint_node(rd, root, "NumBits3", a + 0x70, 0);
    read_uint_node(rd, root, "NumBits4", a + 0x74, 0);
    read_uint_node(rd, root, "NumWords", a + 0x58, 0);
    for (uint32_t k = 1; k <= 4; k++) {
        uint32_t slot = 0x98 + 4 * (k - 1);
        if (U32(a, slot - 0x30) == 0) {
            SU32(a, slot, 5);
            for (uint32_t j = k; j < 4; j++) SU32(a, 0x98 + 4 * j, U32(a, 0x98 + 4 * j) - 1);
        }
        uint32_t off = U32(a, 0x14);
        SU32(a, slot - 0x20, off);
        SU32(a, 0x14, off + U32(a, slot - 0x30));
    }
    uint32_t words = U32(a, 0x58);
    rx_addr dst = rx_crt_aligned_malloc(words * 4, 16);
    SU32(a, 0x88, dst);
    rx_addr n = rx_xml_find_child(rd, root, "Data");
    for (uint32_t k = 0; k < words * 4; k++) SU8(dst, k, U8(U32(n, 8), k));
}

/* FUN_007f88f0 */
static void layout_separate(rx_addr rd, rx_addr a, rx_addr root) {
    static const char* const w[4] = {"Width1", "Width2", "Width3", "Width4"}, * const h[4] = {"Height1", "Height2", "Height3", "Height4"};
    for (int k = 0; k < 4; k++) {
        read_uint_node(rd, root, w[k], a + 0x18 + 4 * (uint32_t)k, a + 0x38 + 4 * (uint32_t)k);
        read_uint_node(rd, root, h[k], a + 0x28 + 4 * (uint32_t)k, a + 0x48 + 4 * (uint32_t)k);
    }
    static const char* const nb[4] = {"NumBits1", "NumBits2", "NumBits3", "NumBits4"}, * const nw[4] = {"NumWords1", "NumWords2", "NumWords3", "NumWords4"};
    static const char* const dn[4] = {"Data1", "Data2", "Data3", "Data4"};
    for (int k = 0; k < 4; k++) read_uint_node(rd, root, nb[k], a + 0x68 + 4 * (uint32_t)k, 0);
    for (int k = 0; k < 4; k++) read_uint_node(rd, root, nw[k], a + 0x58 + 4 * (uint32_t)k, 0);   /* order of the reads: bits 1..4 first, then words per map */
    for (int k = 0; k < 4; k++)
        if (U32(a, 0x68 + 4 * (uint32_t)k)) read_data(rd, root, dn[k], a, 0x88 + 4 * (uint32_t)k, U32(a, 0x58 + 4 * (uint32_t)k));
    for (int k = 0; k < 4; k++) {
        uint32_t bits = U32(a, 0x68 + 4 * (uint32_t)k);
        if (!bits) SU32(a, 0x98 + 4 * (uint32_t)k, 5);
        SU32(a, 0x14, U32(a, 0x14) + bits);
    }
}

/* PackedDataAsset::vf3 007f8630 (ECX = asset, stack stream) */
static void packdata_load(rx_addr a, rx_stream* s) {
    RX_FRAME_ENTER(R, 0x470);
    rx_xml_reader_construct(R);
    rx_xml_reader_load(R, s);
    rx_addr root = rx_xml_root(R);
    if (root) {
        rx_xml_node_bools(R, rx_xml_find_child(R, root, "Interleaved"), a + 0x10, 1);
        if (U32(a, 0x10)) layout_interleaved(R, a, root); else layout_separate(R, a, root);
    }
    rx_xml_reader_destroy(R);
    RX_FRAME_LEAVE(R, 0x470);
}

/* FUN_007f84d0 (EAX = asset) */
static void packdata_construct(rx_addr a) {
    SU32(a, 0, 0x00972ba4);
    static const uint32_t zeros[] = {8, 0xc, 0x14, 0x18, 0x28, 0x58, 0x68, 0x88, 0x98, 0x1c, 0x2c, 0x5c, 0x6c, 0x8c, 0x78, 0x7c, 0x20, 0x30, 0x60, 0x70, 0x90, 0x80, 0x24, 0x34, 0x64, 0x74, 0x94, 0x84};
    for (size_t i = 0; i < sizeof zeros / sizeof zeros[0]; i++) SU32(a, zeros[i], 0);
    static const uint32_t fz[] = {0x38, 0x48, 0x3c, 0x4c, 0x40, 0x50, 0x44, 0x54};
    for (size_t i = 0; i < sizeof fz / sizeof fz[0]; i++) SF32(a, fz[i], 0.0f);
    SU32(a, 0x9c, 1); SU32(a, 0xa0, 2); SU32(a, 0xa4, 3);
}

/* PackedDataAssetIO::vf3 007f8f30 */
rx_addr rx_packdata_asset_io_create(rx_stream* s) {
    rx_addr a = rx_reflex_aligned_alloc(0xa8);
    if (a) packdata_construct(a);
    packdata_load(a, s);
    return a;
}
