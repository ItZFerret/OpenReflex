/* build_asset.h -- construction unit "asset_system": the Database, its asset handles and the AssetIO dispatch the asset loaders
 * plug into (core/notes/construction.md section 3, unit 1; layouts and the reference event's load order: section 8).
 *
 * The original has ONE Database (vtable 0x0097695c, 0x83b4c bytes, created by the engine at 0084e4d0 and kept at engine + 0x18,
 * [0x00a95844] = the engine). Its parts the world keeps:
 *   +0x04  hash table tag -> AssetIO (the registered asset types; 8-byte tags such as "anim\0\0\0\0", "material")
 *   +0x24  hash table name -> database record (every database loaded: case-insensitive names)
 *   +0x44  hash table tag -> type entry (0x38 bytes: a CRITICAL_SECTION, then at +0x18 the type's handle table name -> handle)
 *   +0x64  two empty std::vectors (0x18 each), +0x94 the last loaded asset's name (0x40), +0xd4 its type tag (8 + NUL),
 *   +0xe8  the load serial (one per asset load, Database::vf30 / vf23), +0xec / +0xf0 / +0xf4 the request ring's read / snapshot /
 *          write indices (one per request: all equal once the loader is idle), +0x104 the ring (1000 x 0x194, cleared), CRITICAL_
 *          SECTIONs at +0x62b24 / 3c / 54 / 6c and +0x83b20, OS event / thread handles at +0xf8 / +0xfc / +0x100 / +0x83b38 / +0x83b48.
 * Hash tables (0x20 bytes): {+0 buckets n, +4 max load 0.5f, +8 count, +0xc bucket array, +0x10 0, +0x14 -1, +0x18 allocator (a 4-byte
 *   StdMemoryAllocator: plain _aligned_malloc blocks), +0x1c byte 1 (owns it)}. A node is inserted at the head of its bucket; before
 *   an insert that finds the load above 0.5 the table grows to (int)(n * 1.5f) buckets (chains re-linked bucket by bucket, each
 *   node to the head of its new bucket). Tag tables: node 0x18 {tag lo, tag hi, value, hash, next}, hash 0084a1f0 of the two words;
 *   name tables: node 0x10 {key (a pointer to the name, not a copy), value, hash, next}, hash String_HashNoCase 005c3400, _stricmp.
 * A database record (one _aligned_malloc block of 0x160 + 4 * handles + 0xa0 * packages + 0x1c * assets bytes): +0 name (0x40),
 *   +0x40 the directory (0x104), +0x144 flag p6, +0x148 packages, +0x14c their count, +0x150 handles, +0x154 their count, +0x158 0,
 *   +0x15c p5; then the handle pointers (the <Handles> list, each counted in the handle's +0x48), the package records (0xa0: +0 name,
 *   +0x40 "<file>.dx9.package", +0x80 asset records, +0x84 their count, +0x88 blockOffset, +0x8c heapOffset, +0x90 offset, +0x94
 *   loaded, +0x98 the requester's value, +0x9c the database record) and the asset records (0x1c: +0 handle, +4 block size, +8 block
 *   offset, +0xc heap size, +0x10 heap offset, +0x14 compressed, +0x18 codec 0 zlib / 1 LZX / 2 SPU zlib).
 * An asset handle (AssetHandle, 0x50 bytes; BXMLAssetHandle 0x58 with the tag at +0x50): +0 vtable (per type), +4 name (0x40),
 *   +0x44 the asset, +0x48 references (one per database record listing it), +0x4c loading. One handle per (type, name) for the
 *   whole game, created by the type's AssetIO::vf1 the first time a database lists it.
 * Every asset (IAsset) carries +4 its serial, +8 its reference count (+1 per package load of it), +0xc the package record that
 *   loaded it (-1: none, Database::vf23 or released) -- written by Database::vf30 after the type's AssetIO::vf3 built it.
 *
 * Bytes come from the fmt readers (src/fmt_db.h, src/fmt_bxml.h): the database XML through fmt_vfs + fmt_bxml, each asset's
 * decompressed bytes through fmt_install_read (the original's file streams, LZX / zlib decoders and XMLReader are replaced by them).
 * The asynchronous loader thread is replaced by direct calls in request order (the core builds on one thread). */
#pragma once
#include "rx_game.h"
#include "build_stream.h"
#include "crt.h"
#ifdef __cplusplus
extern "C" {
#endif

struct fmt_install;

#define RX_DATABASE_SIZE 0x83b4cu

/* ---- hash tables ---- */
void     rx_htab_init_50(rx_addr t);                     /* 0084ddc0 (ESI = t): 50 buckets (the Database's tag tables) */
void     rx_htab_init_5(rx_addr t);                      /* 0084dcc0: 5 buckets (the database table) */
void     rx_htab_init_500(rx_addr t);                    /* 0084da40: 500 buckets (a type's handle table) */
uint32_t rx_tag_hash(uint32_t lo, uint32_t hi);          /* 0084a1f0 (cdecl) */
uint32_t rx_string_hash_nocase(rx_addr s);               /* 005c3400 String_HashNoCase (EAX = s) */
int      rx_tagtab_find(rx_addr t, uint32_t lo, uint32_t hi, uint32_t* value);   /* 0084dea0 (= 0084db00): 1 found */
void     rx_tagtab_insert(rx_addr t, uint32_t lo, uint32_t hi, uint32_t value);  /* 0084db60 (ESI = t) */
void     rx_tagtab_rehash(rx_addr t, uint32_t n);        /* 0084dfe0 */
int      rx_tagtab_remove(rx_addr t, uint32_t lo, uint32_t hi);                 /* 0084dc30 */
int      rx_strtab_find(rx_addr t, rx_addr key, uint32_t* value);               /* 007fa250 (EAX = t, EDI = &key) */
void     rx_strtab_insert(rx_addr t, rx_addr key, uint32_t value);              /* 007cfaf0 (EDI = t): the key pointer is kept */
/* The name tables' calls with the original's frames (e = the call's return slot, r = the caller's EBX / EBP / ESI / EDI; dead
 * stack that later code copies: build_vehicle_parts.c, anim_scene.c): _stricmp through the import (`push b; push a; call`),
 * String_HashNoCase 005c3400 (EAX = string), the lookup 007fa250 (EAX = map, EDI = &key; s0 = value out) and the insert 007cfaf0
 * (EDI = t, ECX = the caller's; s0 = &key, s1 = &value). */
int32_t  rx_stricmp_call_at(rx_addr ret_slot, uint32_t ret, crt_regs4 r, rx_addr a, rx_addr b);
uint32_t rx_hash_nocase_at(rx_addr e, crt_regs4 r, rx_addr str);
uint32_t rx_name_hash_find_at(rx_addr e, crt_regs4 r, rx_addr map, rx_addr keyp, rx_addr out);
void     rx_strtab_insert_at(rx_addr e, crt_regs4 r, uint32_t ecx, rx_addr t, rx_addr keyp, rx_addr valp);
void     rx_strtab_rehash(rx_addr t, uint32_t n);        /* 0084df50 */
int      rx_strtab_remove(rx_addr t, rx_addr key_slot);  /* 0084dd30 (the key slot's address) */

/* ---- the Database ---- */
rx_addr  rx_database_construct(rx_addr db);              /* 0084a2d0 (EDI = db): the parts the world keeps (no thread / events) */
rx_addr  rx_database_create(void);                       /* the engine's 0084e4d0: ReflexAlignedAlloc(0x83b4c) + 0084a2d0 */
void     rx_database_register_io(rx_addr db, rx_addr io, uint32_t tag_lo, uint32_t tag_hi);   /* vf24 0084ab30 */
/* the startup registration of every AssetIO (AssetIORegistration::vf1 0084d140 + MXRavenAssetIORegistration::vf1 0041bed0): the
 * static AssetIO objects' vtables and init guards in .data, XenonMaterialAssetIO's heap singleton (FUN_00793a20), then vf24 per type
 * in the original's order */
void     rx_database_register_all(rx_addr db);
void     rx_database_cache_enable(rx_addr db);                                      /* vf34 0084cf10 */
void     rx_database_cache_add(rx_addr db, const char* dir, const char* pattern);   /* vf38 0084cf50: a background cache entry */
void     rx_database_cache_enable(rx_addr db);                                      /* vf34 0084cf10 */
uint32_t rx_database_reset_event(rx_addr db);                                        /* vf36 0084cf20 (thiscall: ResetEvent) */
void     rx_database_queue_remove(rx_addr db, rx_addr handle, uint32_t frame);         /* vf22 0084ce00 (thiscall db, handle, frame; RET 8) */
/* the end-of-frame removal (build_asset_frame.c): Engine::Frame 0084f260 calls vf33 after the render with the render counter */
void     rx_database_drain_removals(rx_addr db, uint32_t frame);        /* vf33 0084cc80 (thiscall db, frame; RET 4) */
void     rx_database_remove_handle(rx_addr db, rx_addr handle);         /* vf32 0084af90 (thiscall db, handle; RET 4) */
void     rx_ptr_vector_clear(rx_addr v);                                /* 0084d9c0 (EDI = the vector) */
uint32_t rx_asset_handle_refs(rx_addr h);                               /* AssetHandle::vf3 0082a1f0 */
void     rx_asset_handle_unref(rx_addr h);                              /* AssetHandle::vf8 004dce60 */
void     rx_asset_handle_base_dtor(rx_addr h);                          /* 0084a2c0 (thiscall) */
rx_addr  rx_asset_handle_sdtor(rx_addr h, uint32_t flags);              /* IAssetHandle::vf0 00668b20 (RET 4) */
rx_addr  rx_icon_geometry_handle_sdtor(rx_addr h, uint32_t flags);      /* IconGeometryAssetHandle::sdtor 005e15c0 (RET 4) */
rx_addr  rx_split_triangle_handle_sdtor(rx_addr h, uint32_t flags);     /* SplitTriangleCollisionAssetHandle::sdtor 007726b0 (RET 4) */
void     rx_database_cache_add(rx_addr db, const char* dir, const char* pattern);   /* vf38 0084cf50: a background cache entry */
rx_addr  rx_database_find_db(rx_addr db, const char* name);                         /* the record of a loaded database, or 0 */
rx_addr  rx_database_find_package(rx_addr db, const char* dbname, const char* pkg);  /* vf8 0084a890: the package record or 0 */
/* one request served (the ring's three indices advance): every vf4 / vf5 / vf6 / vf14 / vf15 / vf16 / vf17 call of the original */
void     rx_database_request(rx_addr db);
/* the Database virtuals the requesting code calls (registered; thiscall db, stack arguments): vf8 / vf26 / vf27 as the original, vf4 /
 * vf15 served at once in the product (the install of rx_database_set_install), the original in the oracle build */
void     rx_database_set_install(struct fmt_install* in);
rx_addr  rx_database_vf_find_package(rx_addr db, rx_addr dbname, rx_addr pkg);                    /* vf8  0084a890 (RET 8) */
uint32_t rx_database_vf_has_handle(rx_addr db, rx_addr name, uint32_t lo, uint32_t hi);           /* vf26 0084ac20 (RET 0xc) */
rx_addr  rx_database_vf_handle(rx_addr db, rx_addr name, uint32_t lo, uint32_t hi, uint32_t flag); /* vf27 0084ac70 (RET 0x10) */
uint32_t rx_database_is_owner_thread(rx_addr db);                                              /* vf1 0084a7a0 (thiscall) -> EAX */
void     rx_database_vf_load_db(rx_addr db, rx_addr dir, rx_addr name, uint32_t p3, uint32_t p4);  /* vf4  0084b020 (RET 0x10) */
void     rx_database_vf_load_package(rx_addr db, rx_addr dbname, rx_addr pkg, uint32_t p98);       /* vf15 0084a7f0 (RET 0xc) */
/* the asynchronous requests and the ring's queries (construction unit 6; construction.md 8.8) */
void     rx_database_vf_load_db_async(rx_addr db, rx_addr dir, rx_addr name, uint32_t p3, uint32_t p4);           /* vf5  0084b070 (RET 0x10) */
void     rx_database_vf_load_package_async(rx_addr db, rx_addr dbname, rx_addr pkg, uint32_t p2, uint32_t requester);   /* vf16 0084a7c0 (RET 0x10) */
void     rx_database_vf_unload_package(rx_addr db, rx_addr dbname, rx_addr pkg, uint32_t frame);                  /* vf17 0084cde0 (RET 0xc) */
uint32_t rx_database_vf_has_db(rx_addr db, rx_addr name);                                                         /* vf7  0084bc20 (RET 4) */
uint32_t rx_database_vf_package_loaded(rx_addr db, rx_addr dbname, rx_addr pkg);                                  /* vf10 0084a860 (RET 8) */
uint32_t rx_database_vf_idle(rx_addr db);                                                                    /* vf11 0084c090 (build_db_query.c) */
uint32_t rx_database_vf_package_known(rx_addr db, rx_addr dbname, rx_addr pkg);                                   /* vf9 0084a840 (RET 8; build_db_query.c) */
uint32_t rx_database_vf_package_pending(rx_addr db, rx_addr dbname, rx_addr pkg);                                 /* vf12 0084c3a0 (RET 8) */
uint32_t rx_database_vf_db_pending(rx_addr db, rx_addr name);                                                     /* vf13 0084c490 (RET 4) */
/* vf14 0084c0f0 (thiscall db, name, fn, arg, progress; RET 0x10): queue the loader job {kind 5, progress, fn at +0x18c, arg at +0x190}
 * in the first free ring slot (FUN_0084c5b0) and signal the loader's event; the loader thread's case 5 calls fn(arg, progress).
 * The name argument is not read by the original. */
uint32_t rx_database_vf_submit_job(rx_addr db, rx_addr name, rx_addr fn, uint32_t arg, rx_addr progress);
uint32_t rx_database_ring_free(rx_addr db);                                                                       /* FUN_0084c5b0 */
/* the product's loader thread: one queue turn (Database_LoaderThreadProc's +0xf0 snapshot, its case-5 calls, the clear and +0xec) */
void     rx_database_loader_step(rx_addr db);

/* Fn_0084b0a0 (loader request 3): reads <dir><name>.dx9.database, builds the record, creates / counts the handles, registers the
 * record. Returns the record (0: file missing). Already loaded: the loader skips the request (returns the existing record). */
rx_addr  rx_database_load(rx_addr db, struct fmt_install* in, const char* name, const char* dir, uint32_t p5, uint32_t p6);
/* loader request 1 (Database_LoaderThreadProc case 1 + Fn_0084bc70): every asset of the package through vf30 in record order;
 * progress: a float the original updates (0: none); p98 the requester's value kept at the package's +0x98. A loaded package is
 * skipped. Returns the number of assets loaded, -1 when the package is unknown. */
int      rx_database_load_package(rx_addr db, struct fmt_install* in, const char* dbname, const char* pkg, rx_addr progress, uint32_t p98);
/* loader request 2 (FUN_0084c550): vf31 for every asset record, then the package's loaded flag cleared */
void     rx_database_unload_package(rx_addr db, const char* dbname, const char* pkg);
/* Database::vf30 0084ad10: one asset from its stream (pkg: the package record, or -1) */
void     rx_database_load_asset(rx_addr db, rx_addr handle, rx_stream* s, rx_addr pkg);
/* Database::vf23 0084aa10: an asset made by other code gets a handle (created on first use), a reference and a serial. The core may
 * not build the asset itself yet (asset 0): the handle and the serial are made all the same; tag = the asset's type (its vf1). */
rx_addr  rx_database_add_asset(rx_addr db, const char* name, rx_addr asset, uint32_t tag_lo, uint32_t tag_hi);
/* Database::vf31 0084af30: one reference of the handle's asset dropped (destroyed at 0); pkg: the releasing package */
void     rx_database_release_asset(rx_addr db, rx_addr handle, rx_addr pkg);
void     rx_asset_io_destroy(rx_addr io, rx_addr asset);   /* AssetIO::vf4 007d3ee0: delete the asset through its destructor (vtable slot 0, flag 1) */
/* a handle of a loaded type by name (the type's handle table), or 0 */
rx_addr  rx_database_find_handle(rx_addr db, const char* type, const char* name);

/* ---- AssetIO dispatch: the core's loaders by AssetIO vtable ----
 * AssetIO::vf3(stream, name, tag lo, tag hi, &out) of the original: a loader builds the asset from the stream (exactly the original's
 * reads and allocations) and returns it; 0 = the core does not build this type yet (the handle stays empty, the serial is used). */
typedef rx_addr (*rx_asset_loader)(rx_stream* s, rx_addr name, uint32_t tag_lo, uint32_t tag_hi);
void     rx_asset_set_loader(rx_addr io_vtable, rx_asset_loader fn);
/* sets the loaders the core has (anim: build_anim.c); each loader unit adds its type here */
void     rx_asset_register_core_loaders(void);
/* the AssetIO of a type tag ("anim"), its vtable and handle class (from the core's table of the 36 types) */
uint64_t rx_asset_handle_type(rx_addr handle);   /* build_asset_types.c: the handle's vf1 (type tag, EDX:EAX) */
uint32_t rx_asset_tag_lo(const char* type);
uint32_t rx_asset_tag_hi(const char* type);
/* AssetIO::vf1 of every type (0082ccc0 AnimAssetIO::vf1, 00822280 BXMLAssetIO::vf1 ...): a new handle of the type's class */
rx_addr  rx_asset_io_new_handle(rx_addr io, uint32_t tag_lo, uint32_t tag_hi);

/* Optional trace (development): every vf30 / vf23 as one line "ASSET <name> <tag> serial=<n> built=<0|1>" */
void     rx_asset_trace(void (*fn)(const char* line));
/* Test driver hook: called before each asset of a package load (where the original's main thread may run Database::vf23 while the
 * loader thread loads a package: build_test replays those at their serial) */
void     rx_asset_between_loads(void (*fn)(rx_addr db));

#ifdef __cplusplus
}
#endif
