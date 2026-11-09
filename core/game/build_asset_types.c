/* build_asset_types.c -- the asset handles' type tags: AssetHandle::vf1 of each handle class (vtable slot 1; slot 2 is AssetHandle
 * 004dce30, the name). Each returns its 8-byte type tag in EDX:EAX ("texture\0" = 0x00657275:0x74786574); BXMLAssetHandle (vf1
 * 008221a0) returns the tag it keeps at +0x50 / +0x54. Found by scanning MXReflex.exe's .rdata for every vtable whose slot 2 is
 * 004dce30 (35 classes + BXMLAssetHandle; the 36th, vtable 0x00976a9c, is abstract: _purecall). Virtual signature: (rx_addr this)
 * returning EDX:EAX as uint64_t (EAX the low word). rx_asset_handle_type dispatches through the handle's vtable. */
#include "build_asset.h"
#include <stdio.h>
#include <stdlib.h>

#define TAG(hi, lo) (((uint64_t)(hi) << 32) | (uint32_t)(lo))

/* 0041be40 vtable 0x0095b6a4: "grass" */
static uint64_t handle_tag_grass(rx_addr self) { (void)self; return TAG(0x73u, 0x73617267u); }
RX_FUNCTION(0x0041be40, handle_tag_grass)
/* 0041be50 vtable 0x0096252c: "trncol" */
static uint64_t handle_tag_trncol(rx_addr self) { (void)self; return TAG(0x6c6fu, 0x636e7274u); }
RX_FUNCTION(0x0041be50, handle_tag_trncol)
/* 0041be90 vtable 0x009402a8: "tdf" */
static uint64_t handle_tag_tdf(rx_addr self) { (void)self; return TAG(0x0u, 0x666474u); }
RX_FUNCTION(0x0041be90, handle_tag_tdf)
/* 0041bea0 vtable 0x0095df0c: "partattr" */
static uint64_t handle_tag_partattr(rx_addr self) { (void)self; return TAG(0x72747461u, 0x74726170u); }
RX_FUNCTION(0x0041bea0, handle_tag_partattr)
/* 004dcf80 vtable 0x0093eb94: "tbldata" */
static uint64_t handle_tag_tbldata(rx_addr self) { (void)self; return TAG(0x617461u, 0x646c6274u); }
RX_FUNCTION(0x004dcf80, handle_tag_tbldata)
/* 005940d0 vtable 0x00946270: "bink" */
static uint64_t handle_tag_bink(rx_addr self) { (void)self; return TAG(0x0u, 0x6b6e6962u); }
RX_FUNCTION(0x005940d0, handle_tag_bink)
/* 00594fe0 vtable 0x009719c8: "texture" */
static uint64_t handle_tag_texture(rx_addr self) { (void)self; return TAG(0x657275u, 0x74786574u); }
RX_FUNCTION(0x00594fe0, handle_tag_texture)
/* 005c3680 vtable 0x009503dc: "localiz" */
static uint64_t handle_tag_localiz(rx_addr self) { (void)self; return TAG(0x7a696cu, 0x61636f6cu); }
RX_FUNCTION(0x005c3680, handle_tag_localiz)
/* 005d6100 vtable 0x00951864: "uicmpnt" */
static uint64_t handle_tag_uicmpnt(rx_addr self) { (void)self; return TAG(0x746e70u, 0x6d636975u); }
RX_FUNCTION(0x005d6100, handle_tag_uicmpnt)
/* 0065ca40 vtable 0x0095b52c: "tree" */
static uint64_t handle_tag_tree(rx_addr self) { (void)self; return TAG(0x0u, 0x65657274u); }
RX_FUNCTION(0x0065ca40, handle_tag_tree)
/* 0065ca50 vtable 0x0095b56c: "forest" */
static uint64_t handle_tag_forest(rx_addr self) { (void)self; return TAG(0x7473u, 0x65726f66u); }
RX_FUNCTION(0x0065ca50, handle_tag_forest)
/* 0065cba0 vtable 0x0095b66c: "frstmtl" */
static uint64_t handle_tag_frstmtl(rx_addr self) { (void)self; return TAG(0x6c746du, 0x74737266u); }
RX_FUNCTION(0x0065cba0, handle_tag_frstmtl)
/* 006ce780 vtable 0x00960bb4: "adv" */
static uint64_t handle_tag_adv(rx_addr self) { (void)self; return TAG(0x0u, 0x766461u); }
RX_FUNCTION(0x006ce780, handle_tag_adv)
/* 006d88f0 vtable 0x009624f4: "terrain" */
static uint64_t handle_tag_terrain(rx_addr self) { (void)self; return TAG(0x6e6961u, 0x72726574u); }
RX_FUNCTION(0x006d88f0, handle_tag_terrain)
/* 0070ce70 vtable 0x00965774: "script" */
static uint64_t handle_tag_script(rx_addr self) { (void)self; return TAG(0x7470u, 0x69726373u); }
RX_FUNCTION(0x0070ce70, handle_tag_script)
/* 00767ef0 vtable 0x0096dd60, 0x0096dd90: "collis" */
static uint64_t handle_tag_collis(rx_addr self) { (void)self; return TAG(0x7369u, 0x6c6c6f63u); }
RX_FUNCTION(0x00767ef0, handle_tag_collis)
/* 007939f0 vtable 0x00971a30: "morph" */
static uint64_t handle_tag_morph(rx_addr self) { (void)self; return TAG(0x68u, 0x70726f6du); }
RX_FUNCTION(0x007939f0, handle_tag_morph)
/* 00793a70 vtable 0x0095b4f4: "material" */
static uint64_t handle_tag_material(rx_addr self) { (void)self; return TAG(0x6c616972u, 0x6574616du); }
RX_FUNCTION(0x00793a70, handle_tag_material)
/* 00794710 vtable 0x00971aa4: "shader" */
static uint64_t handle_tag_shader(rx_addr self) { (void)self; return TAG(0x7265u, 0x64616873u); }
RX_FUNCTION(0x00794710, handle_tag_shader)
/* 00794f80 vtable 0x00971a60, 0x00971ee8: "surface" */
static uint64_t handle_tag_surface(rx_addr self) { (void)self; return TAG(0x656361u, 0x66727573u); }
RX_FUNCTION(0x00794f80, handle_tag_surface)
/* 007d13a0 vtable 0x00972870: "subscene" */
static uint64_t handle_tag_subscene(rx_addr self) { (void)self; return TAG(0x656e6563u, 0x73627573u); }
RX_FUNCTION(0x007d13a0, handle_tag_subscene)
/* 007d1440 vtable 0x00972898: "model" */
static uint64_t handle_tag_model(rx_addr self) { (void)self; return TAG(0x6cu, 0x65646f6du); }
RX_FUNCTION(0x007d1440, handle_tag_model)
/* 007d5520 vtable 0x009728f0: "banner" */
static uint64_t handle_tag_banner(rx_addr self) { (void)self; return TAG(0x7265u, 0x6e6e6162u); }
RX_FUNCTION(0x007d5520, handle_tag_banner)
/* 007e1810 vtable 0x00972948: "particle" */
static uint64_t handle_tag_particle(rx_addr self) { (void)self; return TAG(0x656c6369u, 0x74726170u); }
RX_FUNCTION(0x007e1810, handle_tag_particle)
/* 007e3eb0 vtable 0x00972a60: "water" */
static uint64_t handle_tag_water(rx_addr self) { (void)self; return TAG(0x72u, 0x65746177u); }
RX_FUNCTION(0x007e3eb0, handle_tag_water)
/* 007f84a0 vtable 0x00972aa4: "packdata" */
static uint64_t handle_tag_packdata(rx_addr self) { (void)self; return TAG(0x61746164u, 0x6b636170u); }
RX_FUNCTION(0x007f84a0, handle_tag_packdata)
/* 007fa700 vtable 0x00972cdc: "cell" */
static uint64_t handle_tag_cell(rx_addr self) { (void)self; return TAG(0x0u, 0x6c6c6563u); }
RX_FUNCTION(0x007fa700, handle_tag_cell)
/* 007fc190 vtable 0x00972d50: "texatlas" */
static uint64_t handle_tag_texatlas(rx_addr self) { (void)self; return TAG(0x73616c74u, 0x61786574u); }
RX_FUNCTION(0x007fc190, handle_tag_texatlas)
/* 007fcbc0 vtable 0x00972bf8: "icongeom" */
static uint64_t handle_tag_icongeom(rx_addr self) { (void)self; return TAG(0x6d6f6567u, 0x6e6f6369u); }
RX_FUNCTION(0x007fcbc0, handle_tag_icongeom)
/* 00803de0 vtable 0x00973a60: "soundbnk" */
static uint64_t handle_tag_soundbnk(rx_addr self) { (void)self; return TAG(0x6b6e6264u, 0x6e756f73u); }
RX_FUNCTION(0x00803de0, handle_tag_soundbnk)
/* 00808060 vtable 0x00973a28: "sound" */
static uint64_t handle_tag_sound(rx_addr self) { (void)self; return TAG(0x64u, 0x6e756f73u); }
RX_FUNCTION(0x00808060, handle_tag_sound)
/* 00808ae0 vtable 0x00973a00: "sounddat" */
static uint64_t handle_tag_sounddat(rx_addr self) { (void)self; return TAG(0x74616464u, 0x6e756f73u); }
RX_FUNCTION(0x00808ae0, handle_tag_sounddat)
/* 0082af50 vtable 0x00975db4: "action" */
static uint64_t handle_tag_action(rx_addr self) { (void)self; return TAG(0x6e6fu, 0x69746361u); }
RX_FUNCTION(0x0082af50, handle_tag_action)
/* 0082b920 vtable 0x00975e94: "anim" */
static uint64_t handle_tag_anim(rx_addr self) { (void)self; return TAG(0x0u, 0x6d696e61u); }
RX_FUNCTION(0x0082b920, handle_tag_anim)
/* 0083f5f0 vtable 0x00975ffc: "perfdat" */
static uint64_t handle_tag_perfdat(rx_addr self) { (void)self; return TAG(0x746164u, 0x66726570u); }
RX_FUNCTION(0x0083f5f0, handle_tag_perfdat)
/* 008221a0 BXMLAssetHandle::vf1 (vtable 0x0097567c): the tag kept in the handle */
static uint64_t handle_tag_bxml(rx_addr self) { return TAG(U32(self, 0x54), U32(self, 0x50)); }
RX_FUNCTION(0x008221a0, handle_tag_bxml)

uint64_t rx_asset_handle_type(rx_addr h) {
    rx_addr f = U32(U32(h, 0), 4);
    uint64_t (*vf1)(rx_addr) = RX_FN(f, uint64_t (*)(rx_addr));
    if (!vf1) { fprintf(stderr, "rxcore: asset handle vf1 %08x not ported (build_asset_types.c)\n", f); abort(); }
    return vf1(h);
}
