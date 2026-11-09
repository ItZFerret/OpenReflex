/* build_tables.h -- construction unit 12a "tables": the game's table database (Rdb) and the startup registry, built from the install's
 * files by transliterated game code (core/notes/construction.md section 12).
 *
 * ---- Layouts (MSVC 9 STL instantiations of the game; every block _aligned_malloc(size, 16) under the allocator lock) ----
 * Rdb::TableCollection T (0x1f4, vtable 0x93eb64; MXPlayerDatabase 0x93ecac): the static default database 0x00ae0910 (the global
 *   table database pointer [0x00a89cfc] points to it while MXTables load) and the four player databases 0x00ae10f0 + 0x1f4 * k.
 *   +4 an array {+4 data = ReflexAlignedAlloc(10 * 4), +8 0, +0xc / +0x10 / +0x14 10}, +0x18 the table map (std::map<const char*,
 *   Table*, _stricmp>: proxy +0x18, head +0x30, size +0x34), +0x38 Table*[100] in load order, +0x1c8 a CRITICAL_SECTION, +0x1e0 0,
 *   +0x1e8 the table count, +0x1ec the version (1), +0x1f0 1.
 * Rdb::Table (0x74, vtable 0x93eb6c; the static subclasses PlayerTable / VehicleTable / ... are 0x78): +4 std::vector<Column*> (proxy
 *   +4, begin +0x10, end +0x14, capacity +0x18), +0x1c the column map std::map<const char*, int, _stricmp> (head +0x34, size +0x38),
 *   +0x3c the name (0x29 chars + NUL at +0x65), +0x68 the number of row loads, +0x6c the database version, +0x70 0.
 * Column C (0x50, 00506180): +0 the name (0x1f chars + NUL), +0x20 the table, +0x24 the parent table (0), +0x28 the parent column's
 *   index (-1), +0x2c the type (1 int, 2 float, 4 string; 0 / 5 / 6 are 16 / 24 / 8-byte types the files do not use), +0x30 the
 *   default (a Variant {+0 type, +8 value}: int / float inline, string a pointer to a 0x1c std::string), +0x40 the cells (a 0x18 block
 *   = std::vector<int | float | std::string>: proxy +0, begin +0xc, end +0x10, capacity +0x14), +0x44 the searchable index
 *   (std::map<key, std::set<RowRef>>, 0x20 block), +0x48 the unique index (std::map<key, RowRef>, 0x20 block), +0x4c an int (7th
 *   argument of the int column constructor).
 * A map / set object (0x20 when allocated): +0 the container proxy (a 4-byte block pointing back), +0x18 the head node, +0x1c size;
 *   nodes {+0 left, +4 parent, +8 right, +0xc key / value}: color / isnil bytes after the value (red 0, black 1).
 * RowRef (12 bytes): {+0 the row index, +4 the set holding it (0 in a unique index), +8 the table}. A std::set<RowRef> is 0x28:
 *   the tree at +0..+0x1c, +0x20 the sort column (0 here: rows ordered by index), +0x24 the table.
 * The game's std::string (0x1c): +4 the 16-byte buffer or the pointer (capacity +0x18 >= 0x10), +0x14 size, +0x18 capacity; its
 *   allocator is _aligned_malloc(n, 16) (FUN_004f5290 / 004f3320), not msvcp90's operator new.
 *
 * The critical sections (0x00ae1c44 around cell access, 0x00d61704 the allocator lock, the collections' +0x1c8) end each call as they
 * began: the core builds on one thread and takes none. The checked-iterator / _Xlen / _Xran failure paths of the STL code never fire
 * on these tables and abort. Paths the reference boot does not execute (boot coverage: rx_build/orig/boot-cov.bin) abort naming the
 * original address: column types 0 / 5 / 6, float-keyed indexes, sorted row sets. */
#pragma once
#include "rx_game.h"
#include "build_stream.h"
#ifdef __cplusplus
extern "C" {
#endif

#define RX_TBL_DATABASE_PTR 0x00a89cfcu   /* the table database the loaders fill */
#define RX_TBL_DEFAULT_DB   0x00ae0910u   /* the default database (MXTables_Default + DLC) */
#define RX_TBL_PLAYER_DB(k) (0x00ae10f0u + 0x1f4u * (uint32_t)(k))   /* MXPlayerDatabase k = 0..3 */

/* ---- startup (static initializers of the exe, in _initterm order) ---- */
void    rx_tbl_alloc_init(void);                 /* 004dc0f0: the allocator singleton 0x00d92d58 (guard 0x00d92ed8 bit 0) */
void    rx_tbl_static_init(void);                /* 00903b20 / 00903b40 / 00904140 / 00904160 + the static tables' constructors */
rx_addr rx_tbl_collection_construct(rx_addr self);   /* 004ee8d0 (ESI = self) */

/* ---- the Rdb core (build_tables_rdb.c) ---- */
rx_addr rx_tbl_table_construct(rx_addr self);                      /* 004ec4f0 (EDI = self) */
rx_addr rx_tbl_table_construct_named(rx_addr self, rx_addr name);  /* 004ec560 (EDI = self; name; RET 4) */
uint32_t rx_tbl_row_count(rx_addr table);                          /* 004021d0 (EAX = table): rows of column 0 (0 without columns) */
uint32_t rx_tbl_column_rows(rx_addr col);                          /* 004eb830 (EAX = column) */
void    rx_tbl_column_reserve(rx_addr col, uint32_t n);            /* 004eb5c0 (ESI = column, EDI = n) */
void    rx_tbl_column_append_default(rx_addr col);                 /* 004eb4e0 (EAX = column) */
/* the column constructors: name / parent column are game C strings; def = the default's address (int / float), the std::string
 * object for a string column; flags bit 0 searchable, bit 1 unique; parent = a Table or 0 */
void    rx_tbl_add_int_column(rx_addr table, rx_addr name, rx_addr def, uint32_t flags, rx_addr parent, rx_addr pcol, uint32_t a7); /* 004f69d0 */
void    rx_tbl_add_string_column(rx_addr table, rx_addr name, rx_addr def, uint32_t flags, rx_addr parent, rx_addr pcol);         /* 004f6ae0 */
void    rx_tbl_add_float_column(rx_addr table, rx_addr name, rx_addr def, uint32_t flags, rx_addr parent, rx_addr pcol);          /* 004f6f90 */
void    rx_tbl_add_int64_column(rx_addr table, rx_addr name, rx_addr def);                                                     /* 004f7180 */
void    rx_tbl_set_int(rx_addr col, uint32_t row, rx_addr valp);     /* 004f5330 (col, row, &value; RET 0xc) */
void    rx_tbl_set_float(rx_addr col, uint32_t row, rx_addr valp);   /* 004f5650 */
void    rx_tbl_set_string(rx_addr col, uint32_t row, rx_addr str);   /* 004f5970 (EAX = row; col, std::string*; RET 8) */
void    rx_tbl_table_clear_rows(rx_addr table);                    /* 004ecdb0 (table; RET 4) */
rx_addr rx_tbl_rowset_copy_construct(rx_addr set, rx_addr src);    /* 004ebce0 (EDI = the new set; stdcall the source; RET 4) -> EAX = the new set: std::set<RowRef>'s copy constructor */
void    rx_tbl_erase_row(rx_addr table, uint32_t row);             /* 004ece50 (table, row; RET 8): every column's cell + index, +0x68 */
void    rx_tbl_set_cell_int(rx_addr table, rx_addr name, uint32_t row, rx_addr valp);   /* Table_SetCellInt 004f6e60 (EAX = name, ESI = table; row, &value; RET 8) */
void    rx_tbl_database_clear_rows(rx_addr db);                    /* 004eed40 Database_ClearRows */
rx_addr rx_tbl_table_copy(rx_addr t, rx_addr src);                 /* 004ec770 (copy, source; RET 8): columns, cells, indexes; name "" */
void    rx_tbl_table_destroy_columns(rx_addr t);                   /* 004ec600 (table; RET 4) */
void    rx_tbl_table_destroy(rx_addr t);                           /* the destructor of a table on the stack (FUN_004e6280's temporaries) */
rx_addr rx_tbl_name_map_index(rx_addr map, rx_addr key);           /* 004f0540 (EDI = map; &key; RET 4): &map[key] */
void    rx_tbl_vector_reserve_ptr(rx_addr vec, uint32_t n);        /* 004f0340 (ECX = vector; n; RET 4) */
void    rx_tbl_vector_push_ptr(rx_addr vec, rx_addr valp);         /* 004f0430 (EDI = vector; &value; RET 4) */

/* ---- rows from criteria (the row builder): a criterion is 0xb0 bytes (+0 name, +0x80 column index, +0x84 op, +0x88 Variant value,
 * +0x98 path vector); a criteria vector is a std::vector of them ---- */
void    rx_tbl_crit_vec_init(rx_addr v);
void    rx_tbl_crit_vec_destroy(rx_addr v);
void    rx_tbl_crit_vec_clear(rx_addr v);                          /* 004f2c40 erase(begin, end) */
void    rx_tbl_crit_init_int(rx_addr c, rx_addr name, uint32_t op, uint32_t value);   /* 00419a70 (an int value) */
void    rx_tbl_crit_destroy(rx_addr c);
void    rx_tbl_crit_push(rx_addr v, rx_addr c);                    /* 004175e0 push_back (a copy) */
rx_addr rx_tbl_add_row(rx_addr table, rx_addr out, rx_addr crits); /* 004ec950 (EAX = table; out RowRef, &criteria; RET 8) */
void    rx_tbl_crit_init_string(rx_addr c, rx_addr name, uint32_t op, rx_addr value);   /* 004f6c60 (a C string value) */
/* the rows matching a criteria vector (resolved: 004ed530) into a std::set<RowRef> (0x28 object: proxy, head +0x18, size +0x1c, sort
 * column +0x20, table +0x24) */
void    rx_tbl_query(rx_addr table, rx_addr crits, rx_addr out);   /* 004eddc0 (table, &criteria, &set; RET 0xc) */
uint32_t rx_tbl_delete_rows(rx_addr table, rx_addr crits);         /*004ecf00(table,&criteria;RET8): returns mutation count; emptycriteria reads afterclear.*/
void    rx_tbl_select(rx_addr table, rx_addr out, rx_addr crits);  /* 004ed2f0 (ECX = table, EAX = &set; &criteria (unresolved); RET 4) */
void    rx_tbl_find_first(rx_addr table, rx_addr crits, rx_addr out);   /* 004ed410 (table, &criteria, &out RowRef; RET 0xc) */
void    rx_tbl_rowset_erase(rx_addr set, uint32_t row);            /* 004f0800 erase(key) */
void    rx_tbl_rowset_copy(rx_addr set, rx_addr src);              /* 004ebce0 the copy constructor */
void    rx_tbl_rowset_init(rx_addr set, rx_addr table);           /* 004dc570 an empty set of a table */
void    rx_tbl_rowset_destroy(rx_addr set);                        /* the inlined destructor (table word cleared, tree, proxy) */
rx_addr rx_tbl_rowset_next(rx_addr node);                          /* 00417df0 ++it */
uint32_t rx_tbl_vehicle_list_add_row(uint32_t owner, uint32_t owner_vehicle_id);                 /* VehicleList_AddRow 004de810 */
uint32_t rx_tbl_rider_list_add_row(uint32_t owner, uint32_t owner_rider_id, uint32_t gear_set);  /* RiderList_AddRow 004dea90 */

/* the game's std::string over game memory */
void    rx_tbl_str_init(rx_addr s);                                /* the empty string (+0x18 0xf, +0x14 0, +4 0) */
rx_addr rx_tbl_str_cstr(rx_addr s);
void    rx_tbl_str_assign_sub(rx_addr s, rx_addr r, uint32_t pos, uint32_t n);   /* 004f1860 (EAX = s; r, pos, n; RET 0xc) */
void    rx_tbl_str_assign_n(rx_addr s, rx_addr p, uint32_t n);     /* 004f2ce0 (EAX = n, ESI = s; p; RET 4) */
void    rx_tbl_str_tidy(rx_addr s);                                /* free + empty */

/* ---- the static tables' columns (build_tables_schema.c) ---- */
/* the schema calls of FUN_00756760 (MXGame construction: 00756887 .. 00756936) in its order; the experience tables' rows (FUN_004e3c50 / FUN_004e5f70)
 * included */
void    rx_tbl_create_schemas(void);
void    rx_tbl_activity_results_schema(void);                     /* ActivityResults_CreateSchema 004df240 */

void    rx_tbl_rider_schema(rx_addr t);                            /* FUN_004e1820 (EDI = t) */
void    rx_tbl_stats_schema(rx_addr t);                            /* FUN_004e0a40 (EDI = t) */

/* ---- startup (build_tables_startup.c) ---- */
struct fmt_install;
/* FUN_004ef2b0 (ECX = file name): a loose .bxml of the install's root into [0x00a89cfc] */
int     rx_tbl_load_tables_file(struct fmt_install* in, rx_addr name);
/* FUN_004e6280 (MXGame::Startup, after FUN_00756760's schemas): leaderboards / playlists, the memory collections, the rebuilt tables */
void    rx_tbl_mxgame_startup_tables(struct fmt_install* in);
/* MXRegistry_Load 004e6c30: registry = the engine Registry ([[0x00a95844] + 0xc], build_registry.h: its MXGame/Tables value names the
 * table database); db = the asset Database */
void    rx_tbl_mxregistry_load(struct fmt_install* in, rx_addr db, rx_addr registry);

/* FUN_0053c410's table branch (names: the DLC finder's database list in order; those holding "Tables" are loaded) */
void    rx_tbl_load_dlc_tables(struct fmt_install* in, rx_addr db, const char* const* names, int n);

/* ---- loading (build_tables_load.c) ---- */
/* Database_LoadTables 004ef060 (ECX = the "Tables" node; reader, keep_rows; RET 8) into [0x00a89cfc] */
void    rx_tbl_load_tables(rx_addr tables_node, rx_addr reader, uint32_t keep_rows);
/* TableDataAssetIO::vf3 004f0170: the TableDataAsset (0x50) and its tables loaded (FUN_004eefa0) */
rx_addr rx_tbl_asset_io_create(rx_stream* s, rx_addr name);
void    rx_tbl_register_loader(void);            /* the tbldata loader with the asset system */
/* Database_RestorePlayerDefaults 004eea10 (db; RET 4) */
void    rx_tbl_restore_player_defaults(rx_addr db);

#ifdef __cplusplus
}
#endif

/* Host full Player snapshot: explicit values, shared canonical append kernels. */
void rx_tbl_append_values_row(rx_addr table,rx_addr values);
uint32_t rx_tbl_snapshot_unique_valid(rx_addr table);
