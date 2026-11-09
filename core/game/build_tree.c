/* build_tree.c -- see build_tree.h. Transliteration of TreeAssetIO::vf3 00671ee0, XenonTreeAsset_Parse 00672030 and XenonTreeAsset_ParseLods
 * 00672540. */
#include "build_tree.h"
#include "build_alloc.h"
#include "build_render.h"
#include "build_xml.h"
#include "../src/rx_d3d.h"

static rx_addr lock_buffer(rx_addr res, rx_addr tmp) {
    uint32_t a[5] = {res, 0, 0, tmp, 0};
    rx_d3d_call(res, 11, 5, a);
    return U32(tmp, 0);
}
static void unlock_buffer(rx_addr res) { uint32_t a[1] = {res}; rx_d3d_call(res, 12, 1, a); }

/* XenonTreeAsset_ParseLods (EAX = asset, ESI = reader) */
static void parse_lods(rx_addr rd, rx_addr a) {
    rx_addr root = rx_xml_root(rd);
    rx_addr lods = rx_xml_find_child(rd, root, "Lods");
    rx_xml_attr_floats(rd, rx_xml_find_attr(rd, lods, "near"), a + 0x10, 1);
    rx_xml_attr_floats(rd, rx_xml_find_attr(rd, lods, "far"), a + 0x14, 1);
    rx_xml_attr_uints(rd, rx_xml_find_attr(rd, lods, "dropToBillboard"), a + 0x824, 1);
    static const struct { const char* name; uint32_t off; } fl[] = {
        {"targetAlphaValue", 0x828}, {"transitionFactor", 0x82c}, {"leafLodMethod", 0}, {"leafLodTransitionRadius", 0x834},
        {"leafLodCurveExponent", 0x838}, {"leafTransitionFactor", 0x83c}, {"crestWidthBB", 0x840}, {"crestWidth", 0x844},
        {"cycleLengthBB", 0x848}, {"cycleLength", 0x84c}, {"transitionWidth", 0x850}};
    for (size_t i = 0; i < sizeof fl / sizeof fl[0]; i++) {
        rx_addr e = rx_xml_find_attr(rd, lods, fl[i].name);
        if (!fl[i].off) rx_xml_attr_uints(rd, e, a + 0x830, 1);          /* leafLodMethod: a uint */
        else rx_xml_attr_floats(rd, e, a + fl[i].off, 1);
    }
    rx_addr e = rx_xml_find_attr(rd, lods, "branchFadeDistance");
    if (!e) SU32(a, 0x854, 0x466a6000u); else rx_xml_attr_floats(rd, e, a + 0x854, 1);
    e = rx_xml_find_attr(rd, lods, "frondFadeDistance");
    if (!e) SU32(a, 0x858, 0x466a6000u); else rx_xml_attr_floats(rd, e, a + 0x858, 1);
    e = rx_xml_find_attr(rd, lods, "transitionPercent");
    if (!e) SU32(a, 0x85c, 0); else rx_xml_attr_floats(rd, e, a + 0x85c, 1);
    rx_addr bb = rx_xml_find_child(rd, root, "BoundingBox");
    rx_xml_attr_vec3(rd, rx_xml_find_attr(rd, bb, "center"), a + 0x860, 1);
    rx_xml_attr_vec3(rd, rx_xml_find_attr(rd, bb, "half"), a + 0x870, 1);
    rx_addr sfp = rx_xml_find_child(rd, root, "ShadowFitPoints");
    rx_xml_attr_uints(rd, rx_xml_find_attr(rd, sfp, "numShadowFitPoints"), a + 0x820, 1);
    uint32_t n = U32(a, 0x820);
    n = n < 0x80 ? n : 0x80;
    SU32(a, 0x820, n);
    rx_xml_node_vec3(rd, sfp, a + 0x20, (int)n);
}

typedef struct { uint32_t g, decl, vb, stride, ib, starts, counts; int ib_mode; } blk_t;   /* ib_mode: 0 none, 1 when the block has indices, 2 always */

/* XenonTreeAsset_Parse (ECX = asset, stack stream) */
static void tree_parse(rx_addr a, rx_stream* s) {
    RX_FRAME_ENTER(F, 0x300 + 0x470);
    rx_addr H = F, rd = F + 0x300, tmp = F + 0x2f0;
    rx_xml_reader_construct(rd);
    rx_stream_read(s, H, 0x2ec);
    rx_xml_reader_load(rd, s);
    parse_lods(rd, a);
    rx_addr dev = rx_renderer_device();
    static const blk_t blk[4] = {
        {0x880, 0x884, 0x888, 0x88c, 0x890, 0x894, 0x8a8, 1}, {0x8bc, 0x8c0, 0x8c4, 0x8c8, 0x8cc, 0x8d0, 0x8e4, 2},
        {0x8f8, 0x8fc, 0x904, 0x900, 0, 0x908, 0x91c, 0}, {0x930, 0x934, 0x938, 0x93c, 0x940, 0x944, 0x958, 2}};
    for (int bi = 0; bi < 4; bi++) {
        const blk_t* b = &blk[bi];
        rx_addr HB = H + 0xc + 0xb8u * (uint32_t)bi;
        uint32_t idx = U32(HB, 0), vtx = U32(HB, 4), lodc = U16(HB, 0xe);
        SU32(a, b->g, lodc);
        if (lodc == 0 || vtx == 0) { SU32(a, b->g, 0); continue; }
        for (uint32_t k = 0; k < lodc; k++) { SU16(a, b->starts + 2 * k, U16(HB, 0x10 + 2 * k)); SU16(a, b->counts + 2 * k, U16(HB, 0x24 + 2 * k)); }
        SU32(a, b->decl, rx_vertex_decl_get(HB + 0x38, U32(U32(0, RX_RENDERER), 0x14)));
        SU32(a, b->stride, U16(HB, 0xc));
        rx_d3d_create_vb(dev, vtx, 8, 0, 1, a + b->vb);
        rx_addr vb = U32(a, b->vb);
        rx_stream_read(s, lock_buffer(vb, tmp), vtx);
        unlock_buffer(vb);
        if (b->ib_mode == 2 || (b->ib_mode == 1 && idx)) {
            rx_d3d_create_ib(dev, idx, 8, 0x65, 1, a + b->ib);
            rx_addr ib = U32(a, b->ib);
            rx_stream_read(s, lock_buffer(ib, tmp), idx);
            unlock_buffer(ib);
        } else if (b->ib_mode == 1) SU32(a, b->ib, 0);
    }
    rx_xml_reader_destroy(rd);
    RX_FRAME_LEAVE(F, 0x300 + 0x470);
}

/* TreeAssetIO::vf3 00671ee0 */
rx_addr rx_tree_asset_io_create(rx_stream* s) {
    rx_addr a = rx_reflex_aligned_alloc(0x970);
    if (a) { SU32(a, 8, 0); SU32(a, 0xc, 0); SU32(a, 0x884, 0); SU32(a, 0x8c0, 0); SU32(a, 0x8fc, 0); SU32(a, 0x934, 0); SU32(a, 0, 0x0095b51c); }
    tree_parse(a, s);
    return a;
}
