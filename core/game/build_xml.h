/* build_xml.h -- construction unit "asset_data" (2d): the game's XMLReader (a loaded BXML document) and BXMLAssetIO.
 *
 * XMLReader (the object is embedded: BXMLAsset is 0x478 bytes with the reader at +0, the IAsset part at +0x460; other loaders keep a
 * reader on their own frame or inside the asset): constructor XMLReader_Construct 00824e70, loader FUN_008252c0 (the binary BXML:
 * header 0x20 bytes {magic, u16 version 0x3ea, u16 flags (1 = the rest is one zlib stream, 2 = external data section, 4 = big endian
 * Xbox data), strings, string bytes, data bytes, values, nodes, 0}). Fields after loading:
 *   +4 number of strings, +8 the string bytes, +0xc the pointer table (one char* per string), +0x10 the string hash table (0x20 bytes:
 *   +0 buckets (2 * strings), +4 load 0.75f, +8 count, +0xc bucket array, +0x10 0, +0x14 -1, +0x18 allocator, +0x1c 1; nodes 0x10:
 *   key char*, string index, hash 005c3380, next), +0x14 / +0x18 the data bytes and their size, +0x24 the block of values and nodes,
 *   +0x28 node count, +0x2c the nodes (0x24 each: +0 name char*, +4 u16 value type, +6 u16 flags, +8 value pointer, +0xc value count,
 *   +0x10 parent, +0x14 first child, +0x18 child count, +0x1c first attribute, +0x20 attribute count), +0x30 value count, +0x34 the
 *   values (12 each: +0 u32 name index (into the string pointer table), +4 value / pointer into the data, +8 u16 count, +0xa u16 type),
 *   +0x38 / +0x3c the root node; the stream wrapper at +0x2c8 and the zlib state at +0x2f8 (compressed documents).
 * Value types of the files: 3 int, 4 uint, 5 float, 6 8 bytes, 7 vec4, 8 matrix, 9 vec2, 10 vec3, 11 bool, 12 vec4; 1 string. */
#pragma once
#include "rx_game.h"
#include "build_stream.h"
#ifdef __cplusplus
extern "C" {
#endif
rx_addr rx_xml_reader_construct(rx_addr self);                 /* 00824e70 (ESI = self, stack allocator 0): returns self */
int     rx_xml_reader_load(rx_addr self, rx_stream* s);        /* FUN_008252c0: 1 loaded, 0 failed */
struct fmt_vfs;
int     rx_xml_reader_load_file(const struct fmt_vfs* vfs, rx_addr self, const char* name);   /* FUN_00825260: an install file, stdio buffer included */
/* FUN_00824fe0 (ESI = reader): the temporary reader's blocks freed (strings, string pointers, data, external data, values / nodes, the string table and its nodes) */
void    rx_xml_reader_destroy(rx_addr self);
/* BXMLAssetIO::vf3 008221f0: ReflexAlignedAlloc(0x478), FUN_00822100 (the reader, the asset vtable 0x97566c at +0x460, the tag at
 * +0x470 / +0x474), the load; returns the asset = object + 0x460 */
rx_addr rx_bxml_asset_io_create(rx_stream* s, uint32_t tag_lo, uint32_t tag_hi);

/* ---- queries over a loaded reader (the XMLReader virtuals the asset loaders call; stateless: the cursor pool and the "current node" at
 * +0x3c of the original's calls are not kept -- loaders use temporary readers) ---- */
rx_addr rx_xml_root(rx_addr b);                                      /* [reader + 0x38] */
rx_addr rx_xml_find_child(rx_addr b, rx_addr node, const char* name); /* vf7 00828020: the first child named `name` (the first child when name is NULL), or 0 */
rx_addr rx_xml_next_child(rx_addr b, rx_addr prev);                  /* vf8 / vf10: the next sibling with the same name, or 0 */
rx_addr rx_xml_find_attr(rx_addr b, rx_addr node, const char* name); /* vf58 00828ae0: the value entry (12 bytes) of the attribute, or 0 */
/* attribute value entries (12 bytes: +0 name (a string INDEX), +4 value / string index, +8 count, +0xa type) */
int     rx_xml_attr_ints(rx_addr b, rx_addr e, rx_addr dst, int n);   /* vf77 00829080: int (3) memcpy or comma list (1, strtol base 0, T/F letters) */
int     rx_xml_attr_uints(rx_addr b, rx_addr e, rx_addr dst, int n);  /* vf78 00829010: uint (4) or strtoul list */
int     rx_xml_attr_floats(rx_addr b, rx_addr e, rx_addr dst, int n); /* vf76 00829160: float (5) or atof list */
int     rx_xml_attr_bools(rx_addr b, rx_addr e, rx_addr dst, int n);  /* vf80 008290f0: bool (0xb) or list */
/* the same on a node's own value (vf51 .. vf57: type at node + 4, pointer at node + 8) */
int     rx_xml_node_ints(rx_addr b, rx_addr node, rx_addr dst, int n);   /* vf52 00828760 */
int     rx_xml_node_uints(rx_addr b, rx_addr node, rx_addr dst, int n);  /* vf53 00828700 */
int     rx_xml_node_floats(rx_addr b, rx_addr node, rx_addr dst, int n); /* vf51 008287c0 */
int     rx_xml_node_bools(rx_addr b, rx_addr node, rx_addr dst, int n);  /* vf57 008286a0 */
int     rx_xml_attr_vec3(rx_addr b, rx_addr e, rx_addr dst, int n);   /* vf73 008292b0: n vec3 (type 0xa, 12 bytes each) or "x,y,z" lists, written with a 16-byte stride (lane 3 untouched) */
int     rx_xml_node_vec3(rx_addr b, rx_addr node, rx_addr dst, int n);   /* vf48 008288e0: the same from a node's own value */
const char* rx_xml_attr_string(rx_addr b, rx_addr e, char* out, size_t n); /* the string of a string-typed (1) attribute copied to out; NULL otherwise */

/* ---- the XMLReader virtuals themselves (registered: vtable calls land here; thiscall r = the reader, stack arguments, callee cleanup),
 * exact including the cursor state the original keeps (+0x3c the current node, the cursor pool +0x184 / +0x1d4 / top +0x2c4). Callers
 * that need the original's cursor state call these (through the vtable), not the stateless queries above. ---- */
rx_addr  rx_xml_vf_root(rx_addr r);                                              /* vf1  00827f70 (RET 0) */
rx_addr  rx_xml_vf_child(rx_addr r, rx_addr node, rx_addr name);                 /* vf7  00828020 (RET 8) */
rx_addr  rx_xml_vf_first(rx_addr r, rx_addr node, rx_addr name, rx_addr out);    /* vf8  008280b0 (RET 0xc): first child + a cursor */
uint32_t rx_xml_vf_release(rx_addr r, uint32_t c);                               /* vf9  00828190 (RET 4) */
rx_addr  rx_xml_vf_next(rx_addr r, uint32_t c);                                  /* vf10 008281c0 (RET 4) */
uint32_t rx_xml_vf_node_name(rx_addr r, rx_addr node, rx_addr buf, uint32_t n);  /* vf40 008284f0 (RET 0xc) */
uint32_t rx_xml_vf_node_vec3(rx_addr r, rx_addr node, rx_addr dst, uint32_t n);  /* vf48 008288e0 (RET 0xc) */
uint32_t rx_xml_vf_node_floats(rx_addr r, rx_addr node, rx_addr dst, uint32_t n);/* vf51 008287c0 (RET 0xc) */
uint32_t rx_xml_vf_node_ints(rx_addr r, rx_addr node, rx_addr dst, uint32_t n);  /* vf52 00828760 (RET 0xc) */
uint32_t rx_xml_vf_node_string(rx_addr r, rx_addr node, rx_addr dst, uint32_t n);/* vf54 00828640 (RET 0xc) */
uint32_t rx_xml_vf_node_bools(rx_addr r, rx_addr node, rx_addr dst, uint32_t n); /* vf57 008286a0 (RET 0xc) */
rx_addr  rx_xml_vf_attr(rx_addr r, rx_addr node, rx_addr name);                  /* vf58 00828ae0 (RET 8) */
rx_addr  rx_xml_vf_node_text(rx_addr r, rx_addr node);                          /* vf56 00828630 (RET 4) */
rx_addr  rx_xml_vf_attr_cstr(rx_addr r, rx_addr e);                              /* vf68 00828f90 (RET 4) */
uint32_t rx_xml_vf_attr_string(rx_addr r, rx_addr e, rx_addr buf, uint32_t n);   /* vf79 00828fd0 (RET 0xc) */
uint32_t rx_xml_vf_attr_vec4b(rx_addr r, rx_addr e, rx_addr dst, uint32_t n);    /* vf70 00829400 (RET 0xc) */
uint32_t rx_xml_vf_attr_mat4(rx_addr r, rx_addr e, rx_addr dst, uint32_t n);     /* vf69 00829460 (RET 0xc) */
uint32_t rx_xml_vf_attr_vec4a(rx_addr r, rx_addr e, rx_addr dst, uint32_t n);    /* vf71 008293a0 (RET 0xc) */
uint32_t rx_xml_vf_attr_vec3(rx_addr r, rx_addr e, rx_addr dst, uint32_t n);     /* vf73 008292b0 (RET 0xc) */
uint32_t rx_xml_vf_attr_floats(rx_addr r, rx_addr e, rx_addr dst, uint32_t n);   /* vf76 00829160 (RET 0xc) */
uint32_t rx_xml_vf_attr_ints(rx_addr r, rx_addr e, rx_addr dst, uint32_t n);     /* vf77 00829080 (RET 0xc) */

#ifdef __cplusplus
}
#endif
