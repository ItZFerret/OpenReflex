/* build_alloc.h -- construction unit "alloc": the game's allocators over the core's heap (core/notes/construction.md).
 *
 * The original's objects live in msvcr90 heap blocks: malloc(n) = HeapAlloc(_crtheap, 0, n) (one block of n bytes, the boot's heap
 * stand-in returns 16-aligned blocks), and nearly every game allocation is _aligned_malloc(size, 16), which takes malloc(size + 0x13)
 * and returns block + 0x10 with the block's address stored at [result - 4] (VS2008 align.c, _aligned_offset_malloc_base). The
 * canonical dump (core/tools/rx_canon.h) sees the blocks, so the core must make the same blocks: rx_heap_alloc is the HeapAlloc
 * level, these functions are the CRT's and the game's levels above it.
 *   ReflexAlignedAlloc 0078fbe0, FUN_00401f90, StdMemoryAllocator::vf1 004eef20: _aligned_malloc(size, 16) under the allocator
 *   lock (critical section 0x00d61704 when the byte [0x00da67d8] is set); the core builds on one thread: no lock.
 *   StdMemoryAllocator::vf2 00775540: _aligned_free.
 * The arenas that pack many objects: build_pool.h (the small-object pool = the Lua heap, SimpleMemoryAllocator). */
#pragma once
#include "rx_game.h"
#include "crt.h"
#ifdef __cplusplus
extern "C" {
#endif
rx_addr rx_crt_malloc(uint32_t n);                                    /* msvcr90 malloc: one heap block of n bytes (n 0 -> 1) */
void    rx_crt_free(rx_addr p);
rx_addr rx_crt_aligned_malloc(uint32_t size, uint32_t align);         /* msvcr90 _aligned_malloc */
rx_addr rx_crt_aligned_realloc(rx_addr p, uint32_t size, uint32_t align);   /* msvcr90 _aligned_realloc */
void    rx_crt_aligned_free(rx_addr p);                               /* msvcr90 _aligned_free */
rx_addr rx_reflex_aligned_alloc(uint32_t size);                       /* 0078fbe0 ReflexAlignedAlloc (= 00401f90, StdMemoryAllocator::vf1) */
void    rx_reflex_aligned_free(rx_addr p);                            /* 0078fc50 ReflexAlignedFree (= StdMemoryAllocator::vf2 00775540) */
/* the same with the original's frame (build_alloc.c): e = the call's return slot, r = the caller's EBX / EBP / ESI / EDI */
void    rx_reflex_aligned_free_at(rx_addr e, crt_regs4 r, rx_addr p);
rx_addr rx_reflex_aligned_alloc_at(rx_addr e, crt_regs4 r, uint32_t size);   /* 0078fbe0 with its frame */
rx_addr rx_memory_allocator_dtor(rx_addr self, uint32_t flag);        /* 008220e0 IMemoryAllocator::vf0 (ECX; s0; RET 4) */
/* `operator new[]` of an MSVC array with a destructor: n * elem + 4 bytes (saturating like the original: a product that overflows,
 * or + 4 that carries, asks for 0xffffffff), the count at [result], the elements from result + 4. Returns the elements' address
 * (0 when the allocation fails). The caller constructs the elements. */
rx_addr rx_reflex_alloc_array(uint32_t n, uint32_t elem);
/* the size the original computes for n * elem (saturating to 0xffffffff on overflow), for new[] without a count */
static inline uint32_t rx_mul_sat(uint32_t n, uint32_t elem) { uint64_t p = (uint64_t)n * elem; return p >> 32 ? 0xffffffffu : (uint32_t)p; }
#ifdef __cplusplus
}
#endif
