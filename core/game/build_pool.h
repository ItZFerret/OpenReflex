/* build_pool.h -- construction unit "alloc" (rest): the small-object pool at [0x00d94a38] and SimpleMemoryAllocator
 * (core/notes/construction.md section 2 / 5).
 *
 * The small-object pool is the memory of the game's Lua VM. FUN_00744180 (the script system's init, called once at startup) builds
 * it in the struct at 0x00d94938 ("lua system", `sys` below):
 *   +0x00 lua_State*, +0x08 a CRITICAL_SECTION, +0x20..+0x28 a list, +0x2c (0x00d94964) a heap object,
 *   +0x30 eight cell pools of 0x18 bytes (0x00d94968..0x00d94a28): {+0 cell size, +4 count, +8 0x10, +0xc cells, +0x10 free stack,
 *         +0x14 stack top}; cell sizes 0x10..0x80, counts 0x5dd 0x2711 0x1f41 0x5dd 0xbb9 0x1f5 0x191 0x191,
 *   +0xf0 (0x00d94a28) a first-fit heap of 0x20 bytes over one _aligned_malloc(0x300000, 16) block: {+0 size 0x300000, +4 usable end
 *         (offset), +8 alignment 0x10, +0xc chunk header size 0x10, +0x10 the block ([0x00d94a38]), +0x14 first chunk, +0x18 free
 *         list}. A chunk: {+0 payload size, +4 next free, +8 previous free, +0xc used byte, +0xd..+0xf the marker ab cd ef}, then the
 *         payload. The pools' cell arrays and free stacks are carved from this heap first (FUN_00823ab0), so every pool cell and every
 *         larger Lua block lives inside the 3 MB block.
 * The Lua state's frealloc is FUN_00743f00 with ud = sys (lua_newstate 0077bc10): sizes are rounded up to 16; 0x10..0x80 come from
 * the cell pool of that size while it has cells (pops; a free pushes back), everything else (and an exhausted pool) from the heap.
 * Its users are therefore the Lua VM's objects (strings, tables, closures, protos, the state and its stacks): the scripts of the
 * front end, the activity and the scenarios. Building the pool's contents exactly means running the same Lua code (a Lua unit);
 * these functions give it the original's exact allocator so that every block lands at the original's address.
 *
 * SimpleMemoryAllocator (vtable 0x009753c4, 0x14 bytes): a bump arena of 0x19000 bytes (_aligned_malloc(0x19000, 16)); created and
 * destroyed by FUN_007564f0 around one scene XML read (its only constructor call, 0x7565aa), so nothing of it persists into the
 * world. Layout: +4 the block, +8 the cursor, +0xc capacity, +0x10 used. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

#define RX_LUA_SYS 0x00d94938u   /* the script system struct; the pools at +0x30, the heap at +0xf0 */

/* the heap (sys + 0xf0) */
void    rx_pool_heap_init(rx_addr heap);                        /* 00823710 (EAX = heap): the 3 MB block, one free chunk */
rx_addr rx_pool_heap_alloc(rx_addr heap, uint32_t size);        /* 008237e0 (thiscall, ret 4): best fit, split; 0 when full */
rx_addr rx_pool_heap_realloc(rx_addr heap, rx_addr old, uint32_t size);   /* 00823900 (stdcall heap, size; EDI = old) */
void    rx_pool_heap_free(rx_addr heap, rx_addr p);             /* 00823960 (ECX = heap, EAX = p): to the free list head */
void    rx_pool_heap_coalesce(rx_addr heap);                    /* 008239a0 (ECX = heap): merge free neighbours */
void    rx_pool_heap_unlink(rx_addr heap, rx_addr chunk);       /* 00823a50 (EDX = heap, EAX = chunk) */
/* the cell pools (sys + 0x30 + 0x18 * i) */
rx_addr rx_pool_cells_construct(rx_addr pool);                  /* 00823a90 (ECX = pool): {0, 1, 0, 0, 0, -1}; returns pool */
void    rx_pool_cells_build(rx_addr pool, rx_addr sys);         /* 00823ab0 (ESI = pool, EDX = sys): cells + free stack */
int     rx_pool_cells_free(rx_addr pool, rx_addr p);            /* 00823b20 (EAX = pool, ESI = p): 1 when p is one of its cells */
/* the Lua frealloc */
rx_addr rx_lua_alloc(rx_addr ud, rx_addr ptr, uint32_t osize, uint32_t nsize);   /* 00743f00 (cdecl) */
/* the argument slots the last rx_lua_alloc call overwrote (caller-visible stack: the oracle adapter stores them) */
typedef struct { int ud_written; uint32_t ud; int nsize_written; uint32_t nsize; } rx_lua_alloc_args;
extern rx_lua_alloc_args rx_lua_alloc_slots;
/* FUN_00744180's memory part (up to 0x7442e5: the pools and the heap; the Lua state and libraries after it are not here) */
void    rx_lua_memory_init(rx_addr sys);

/* SimpleMemoryAllocator */
rx_addr rx_simple_allocator_construct(rx_addr self);            /* 00823600 (ESI = self) */
rx_addr rx_simple_allocator_alloc(rx_addr self, uint32_t size); /* vf1 008236d0 (thiscall, ret 4): (size + 0x13) & ~0xf bytes */
void    rx_simple_allocator_reset(rx_addr self);                /* vf3 00823700 */
void    rx_simple_allocator_destruct(rx_addr self);             /* 00823680 (ECX = self): frees the block */
rx_addr rx_simple_allocator_sdtor(rx_addr self, uint32_t flags);   /* vf0 00823660 */

#ifdef __cplusplus
}
#endif
