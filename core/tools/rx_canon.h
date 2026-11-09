/* rx_canon.h -- verification of construction: the core's arena as a canonical heap dump (core/notes/construction.md).
 *
 * rx_canon_write writes the RXCN v1 format of the boot's Boot::write_canonical (native/drafts/world_runtime/boot.cpp), so
 * native/game/tools/world_compare.py compares a core-built world with the original's canonical dump of the same event, and
 * core/tools/rx_canon_cmp.py compares sub-worlds from anchors while the construction is partial. Nodes are the live blocks of the
 * core's heap (rx_heap.h: the core's HeapAlloc level; the game's _aligned_malloc header lives inside a block exactly as in the
 * original, build_alloc.h), named by their first breadth-first path from the roots: the executable's writable sections (.data
 * 0x009a6000..0x00dcf76c, .tls 0x00dd0000..0x00dd0811) in address order, then the extra roots the caller passes (objects the core
 * built that no global reaches yet). The original has more roots (the CRT DLLs' data, thread TLS slots, the live stacks): a node
 * named through one of those in the original gets another name here, which world_compare reports as only-A / only-B; the anchored
 * compare does not depend on names.
 *
 * Also writes <path>.addr: (name u64, address u32) per node, the address side of the core's dump.
 * rx_canon_image writes the arena's pages (.data / .tls and every page of a live heap block) as an RXWI world image (the boot's
 * write_world container, no thread table), the address side the anchored compare needs. Development tool: not in the product. */
#pragma once
#include "../src/rx_mem.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { rx_addr addr; uint32_t key; } rx_canon_root;   /* extra root: a pointer value; key names it (mix(0x52, key)) */
/* classes: optional path of _re/out/classes.tsv (RTTI class names by vtable, for display; NULL: none). info: a summary line. */
int rx_canon_write(const char* path, const rx_canon_root* extra, int nextra, const char* classes, char* info, size_t ninfo);
int rx_canon_image(const char* path);
#ifdef __cplusplus
}
#endif
