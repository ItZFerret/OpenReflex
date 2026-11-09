/* rx_heap.h -- the core's heap: allocations at game addresses inside the arena (product) for everything the core builds or the
 * game code allocates (operator new / malloc / the game's allocators all end here). Keeps a block table (address, size, live) so
 * a core-built world can be dumped in the canonical form (core/tools/rx_canon) and compared with the original's
 * (native/game/tools/world_compare.py). Heap region: 0x10000000 .. 0x70000000 (the original's heap range; addresses differ from
 * the original's, which is why worlds are compared canonically). Oracle build: allocations go to the emulated game's own heap is
 * NOT done here -- in the oracle, game allocation functions run as originals or ported code over the emulator. */
#pragma once
#include "rx_mem.h"
#ifdef __cplusplus
extern "C" {
#endif
rx_addr  rx_heap_alloc(uint32_t size, uint32_t align);   /* zero-filled; align: power of two >= 4 (0 = 16) */
rx_addr  rx_heap_alloc_fill(uint32_t size, uint32_t align, uint8_t fill);   /* like rx_heap_alloc, filled with `fill` */
/* Grow / shrink a live block in place when possible (it is the last block, or the new size fits its rounded size): 1 = done
 * (new bytes zero), 0 = not possible (the caller allocates, copies and frees, as msvcr90's realloc would move it). */
int      rx_heap_resize(rx_addr a, uint32_t new_size);
void     rx_heap_free(rx_addr a);
uint32_t rx_heap_size(rx_addr a);                         /* size of the live block starting at a, 0 if none */
/* Block table, address order: index -> (address, size, live). */
int      rx_heap_blocks(void);
void     rx_heap_block(int i, rx_addr* address, uint32_t* size, int* live);
void     rx_heap_reset(void);
/* RX_HEAP_REDZONE=<bytes> (development): the blocks whose redzone was written (the first `report` printed); -1 when off */
int      rx_heap_redzone_check(int report);
#ifdef __cplusplus
}
#endif
