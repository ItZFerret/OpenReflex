/* build_load.h -- construction units 2b-2g "asset loaders" (core/notes/construction.md section 9): shared helpers of the per-type
 * AssetIO::vf3 loaders (build_collis.c, build_model.c, build_data.c, ...) and their registration.
 *
 * A loader is the core's AssetIO::vf3 (build_asset.h rx_asset_loader): it gets the asset's stream, name and tag and returns the new
 * asset (an _aligned_malloc block at the original's exact layout); the Database writes asset +4 / +8 / +0xc and the handle's +0x44
 * itself, so loaders never do. Loaders that look other assets up (a model finds its surfaces and materials) do it through the engine's
 * Database (original: [[0x00a95844] + 8]; Database::vf26 / vf27 are find-by-name) -- rx_load_database(). */
#pragma once
#include "rx_game.h"
#include "build_stream.h"
#ifdef __cplusplus
extern "C" {
#endif

/* registers every loader of this unit with the asset system (rx_asset_register_core_loaders calls it) */
void    rx_load_register(void);
/* the runtime vector constants of the exe's static initializers at 0x00d615c0..0x00d616c0 (identity matrix 0x00d61600, zero vector 0x00d61670,
 * 1-splat 0x00d61690 ...), written when unset: unit 12 (globals) owns them, the loaders' tests need them */
void    rx_load_runtime_constants(void);

/* runs the loader of a type tag on a stream (tests): the asset, 0 when this unit has no loader for the type */
rx_addr rx_load_run(const char* type, rx_stream* s, rx_addr name);

/* TEST SCAFFOLDING (tools only): the renderer stand-in [0x00a95960] the GPU-resource loaders need (+0x14 an empty vertex declaration cache, +0x1ec
 * a core/src/rx_d3d.h device) when no renderer exists yet; the product's engine startup owns the real one */
void    rx_load_test_scaffold(void);

/* the Database the loaders look handles up in: set by whoever creates it (build_asset.c / the engine: [[0xa95844] + 8]) */
void    rx_load_set_database(rx_addr db);
rx_addr rx_load_database(void);
/* Database::vf27(name, tag, ...) of the original: the handle of (type, name) or 0 (the databases listing it have created it) */
rx_addr rx_load_find_handle(const char* type, rx_addr name);
rx_addr rx_load_find_handle_c(const char* type, const char* name);

/* a C string of game memory into a host buffer (at most n-1 bytes) / the length of a game string */
void     rx_ld_gets(rx_addr s, char* out, uint32_t n);
uint32_t rx_ld_strlen(rx_addr s);

/* ---- MSVC 9 std::vector over game memory (the game's layout: +0 container proxy, +0xc begin, +0x10 end, +0x14 end of capacity) ---- */
/* the proxy block (ReflexAlignedAlloc(4) holding the vector's address; the vector's +0 points to it), begin / end / capacity 0 */
void    rx_ld_vec_init(rx_addr v);
/* the same with the CRT's _aligned_malloc(4, 4) proxy (FUN_007ce6a0) */
void    rx_ld_vec_init_crt(rx_addr v);
/* resize of an EMPTY vector to n elements (zero filled) as _Insert_n does it: capacity exactly n, one block of n * elem bytes
 * (aligned 0: allocator<char>::allocate = operator new = malloc(n * elem), a raw block; 1: ReflexAlignedAlloc; nothing for n 0) */
void    rx_ld_vec_resize_empty(rx_addr v, uint32_t n, uint32_t elem, int aligned);
/* the prefilled constructors vector(n, value) of FUN_0081fa90 / 0081fb80 / 0081fc40 / 0081fcf0: one _aligned_malloc(n * elem, 16)
 * block, begin / end / capacity set; the elements are zero here (the callers overwrite them all) */
void    rx_ld_vec_alloc(rx_addr v, uint32_t n, uint32_t elem);
static inline uint32_t rx_ld_vec_size(rx_addr v, uint32_t elem) { return (U32(v, 0x10) - U32(v, 0xc)) / elem; }

#ifdef __cplusplus
}
#endif
