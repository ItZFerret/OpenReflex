/* build_registry.h -- unit 12a: the engine Registry (Engine::Startup) and the XMLBuilder DOMs it holds (core/notes/construction.md 12).
 *
 * XMLBuilder (0x2aac, vtable 0x976edc; IXMLBuilder 0x976cc4): an editable XML document. +4 the interned-string table (0x20: +0 buckets,
 * +4 load 0.75, +8 count, +0xc bucket array, +0x10 0, +0x14 -1, +0x18 the allocator object (vtable 0x9753b0: _aligned_malloc 16 / free),
 * +0x1c 1; nodes 0x10 {string, reference count, hash, next}), +0x24 / +0x74 the attribute cursor pool (20 indices, 20 cursors of 0x108,
 * top +0x1514), +0x1518 / +0x1568 the node cursor pool (cursor {+0 node, +4 index, +8 name[0x100]}, top +0x2a08), +0x2a0c the root,
 * +0x2a10 the current node, +0x2a14 the zlib state (vtable 0x975368), +0x2a58 a stream (0x974b60), +0x2a7c the compressed stream
 * wrapper (0x974b48). A node (0x24): +0 name (interned), +4 value (an interned string, or an _aligned_realloc'ed array), +8 u16 value
 * type (1 string, 3 int, 5 float, 0xa vec3, 0xb bool), +0xa u16 flags, +0xc value count, +0x10 parent, +0x14 child array, +0x18 child
 * count, +0x1c attribute array (0xc each: name, value, u16 type 0, u16 1 = string), +0x20 attribute count.
 *
 * Registry (0x4c, vtable 0x97580c): +4 the documents (an array {data, count, capacity 5, growth 5, 0x7fffffff} of entries), +0x18 a
 * hash table node pointer -> record (Thomas Wang's integer hash, 100 buckets, grown x1.5 past 0.75), +0x38 the changed records
 * {data, count, 0x19, 0x19, 0x7fffffff}. An entry (0x10): +0 a critical-section block (0x1c; +0x18 the document), +4 an array of
 * record blocks, +8 records used, +0xc records per block. A record (0xc): {+0 the entry's critical-section block, +4 the value node,
 * +8 changed}. Registry::vf1(path, key) finds the record of <path>\<key> in the documents in load order. */
#pragma once
#include "rx_game.h"
#include "build_stream.h"
#ifdef __cplusplus
extern "C" {
#endif
struct fmt_install;

rx_addr  rx_xmlb_construct(rx_addr self);                     /* XMLBuilder_Construct 00874300 (ECX = self) */
int      rx_xmlb_load(rx_addr self, rx_stream* s);            /* 00874530 (ECX = self; stream; RET 4): the BXML as a DOM */
rx_addr  rx_xmlb_add_node(rx_addr self, rx_addr parent, rx_addr name);   /* vf81 00877360 (parent 0: the root) */

rx_addr  rx_registry_construct(rx_addr self);                 /* 00823bd0 (EDI = self) */
void     rx_registry_load_file(struct fmt_install* in, rx_addr reg, rx_addr name);   /* 00823e90 (EDI = registry; name; RET 4) */
/* Engine::Startup's registry part (0084e36e .. 0084e416): ReflexAlignedAlloc(0x4c) + 00823bd0, then every <Registry> of Engine.bxml
 * loaded (00823e90). Returns the registry (the caller stores it as the engine's, Engine vf20: [engine + 0xc]). */
rx_addr  rx_registry_startup(struct fmt_install* in);

/* Registry virtuals (this = the registry; rec = a record of vf1) */
uint32_t rx_registry_hash_find(rx_addr t, rx_addr keyp, rx_addr out);    /* 0082ac10 (ESI = table, EAX = &key; &out; RET 4) */
void     rx_registry_hash_insert(rx_addr t, rx_addr keyp, rx_addr valp);  /* 0082ac80 (EDI = table; &key, &value; RET 8) */
rx_addr  rx_registry_vf1_find(rx_addr reg, rx_addr path, rx_addr key, uint32_t a3);   /* 008241f0 */
uint32_t rx_registry_vf20_string(rx_addr reg, rx_addr rec, rx_addr buf, uint32_t n);  /* 00824540 */
uint32_t rx_registry_vf17_floats(rx_addr reg, rx_addr rec, rx_addr dst, uint32_t n);  /* 00824640 */
void     rx_registry_vf3_clear_changed(rx_addr reg);                                    /* 00824390 */

#ifdef __cplusplus
}
#endif
