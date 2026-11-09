/* build_tables_startup.c -- unit 12a: the startup functions that fill the table databases (build_tables.h):
 *   FUN_004e6280 (MXGame::Startup): the leaderboard / playlist collections from MX07Leaderboards.bxml / PlaylistData.bxml, the static
 *     tables registered in the two MXMemoryCollections, the playlist descriptions unescaped, the network / registry rider and stats
 *     tables rebuilt from temporary copies;
 *   MXRegistry_Load 004e6c30 (the game init thread): MXTables_Default into 0x00ae0910, the four player databases restored, the AI
 *     settings of MXRegistry.bxml.
 * The loose .bxml files are read from the install's root (the original's CRTFileInputStream in the reader, +0x340). */
#include "build_tables.h"
#include "build_alloc.h"
#include "build_asset.h"
#include "build_registry.h"
#include "build_vehicle.h"
#include "build_xml.h"
#include "crt.h"
#include "../src/fmt_db.h"
#include "../src/fmt_vfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t gstrlen(rx_addr p) { uint32_t n = 0; while (U8(p, n)) n++; return n; }
static void gets_game(rx_addr p, char* out, size_t n) { size_t i = 0; for (; i + 1 < n && U8(p, i); i++) out[i] = (char)U8(p, i); out[i] = 0; }

/* the reader's file load (FUN_00825260: FUN_00827e00, the stream's vf8 open(name), FUN_008252c0, vf1 close): 1 loaded */
static int reader_load_file(fmt_install* in, rx_addr reader, rx_addr name) {
    char path[256]; gets_game(name, path, sizeof path);
    fmt_buf b = {0};
    if (fmt_vfs_read_file(fmt_install_vfs(in), path, &b) != 0) return 0;
    rx_stream s; rx_stream_init(&s, b.data, b.size);
    int ok = rx_xml_reader_load(reader, &s);
    fmt_buf_free(&b);
    return ok;
}

/* FUN_004ef2b0 (ECX = file name): the file's <Tables> into [0x00a89cfc] (Database_LoadTables, rows cleared) */
int rx_tbl_load_tables_file(fmt_install* in, rx_addr name) {
    RX_FRAME_ENTER(F, 0x478);
    const rx_addr reader = F + 8;
    rx_xml_reader_construct(reader);
    int ok = reader_load_file(in, reader, name);
    if (ok) {
        SU32(reader, 0x3c, U32(reader, 0x38));
        rx_tbl_load_tables(U32(reader, 0x38), reader, 0);
    } else { fprintf(stderr, "rxcore: build_tables: %s: not read\n", "tables file"); abort(); }   /* 004ef2e1: not in the boot */
    rx_xml_reader_destroy(reader);
    RX_FRAME_LEAVE(F, 0x478);
    return ok;
}

/* a table registered in a collection: +0x38[count++] = t, map[t + 0x3c] = t */
static void collection_add(rx_addr db, rx_addr t, rx_addr keyslot) {
    SU32(db, 0x38 + 4 * U32(db, 0x1e8), t);
    SU32(db, 0x1e8, U32(db, 0x1e8) + 1);
    SU32(keyslot, 0, t + 0x3c);
    SU32(rx_tbl_name_map_index(db + 0x18, keyslot), 0, t);            /* 004f0540 */
}

/* FUN_0078feb0 (s; cdecl): "\n", "\r", "\t" (either case) unescaped in place; returns s */
static rx_addr unescape(rx_addr s) {
    uint32_t len = gstrlen(s);
    if (!len) { SU8(s, 0, 0); return s; }
    for (uint32_t i = 0; i < len; i++) {
        if (U8(s, i) != '\\' || i + 1 >= len) continue;
        uint8_t c = U8(s, i + 1), r;
        if (c == 'n' || c == 'N') r = 0xa;
        else if (c == 'r' || c == 'R') r = 0xd;
        else if (c == 't' || c == 'T') r = 9;
        else continue;
        SU8(s, i, r);
        for (uint32_t k = i + 1, cnt = len - (i + 1); cnt; cnt--, k++) if (k + 1 < len) SU8(s, k, U8(s, k + 1));
        len--;
        SU8(s, len, 0);
    }
    SU8(s, len, 0);
    return s;
}

/* a static table rebuilt from a temporary (FUN_004e6280): the name cleared, the copy (004ec770), +0x74 from the temporary (the
 * rider tables), the name back */
static void rebuild_from(rx_addr dst, rx_addr tmp, rx_addr name, int copy74) {
    if (crt_stricmp(dst + 0x3c, 0x00935d6c)) { crt_strncpy(dst + 0x3c, 0x00935d6c, 0x29); SU8(dst, 0x65, 0); }
    rx_tbl_table_copy(dst, tmp);
    if (copy74) SU32(dst, 0x74, U32(tmp, 0x74));
    if (crt_stricmp(dst + 0x3c, name)) { crt_strncpy(dst + 0x3c, name, 0x29); SU8(dst, 0x65, 0); }
}

void rx_tbl_mxgame_startup_tables(fmt_install* in) {                 /* FUN_004e6280 */
    RX_FRAME_ENTER(F, 0x130);
    const rx_addr key = F + 0x10, str = F + 0x1c, t1 = F + 0x38, t2 = F + 0xb0;
    for (uint32_t k = 0x38; k < 0x128; k++) SU8(F, k, 0);            /* the two temporaries' frame bytes (the original's are old stack) */
    /* GetTickCount: not kept */
    SU32(0, RX_TBL_DATABASE_PTR, 0x00ae0268);
    rx_tbl_load_tables_file(in, 0x0093e3a8);                          /* "MX07Leaderboards.bxml" */
    SU32(0, RX_TBL_DATABASE_PTR, 0x00adfde8);
    rx_tbl_load_tables_file(in, 0x0093e3c0);                          /* "PlaylistData.bxml" */
    static const rx_addr mem1[10] = {0x00ae1b18, 0x00ae05c8, 0x00ae19b0, 0x00ae0e08, 0x00ae18c0, 0x00adffe0, 0x00ae07a8, 0x00ae0820, 0x00ae0640, 0x00ae06b8};
    for (int i = 0; i < 10; i++) {
        if (i == 0) {                                                 /* the database pointer is set between the first entry and its map */
            SU32(0, 0x00ae0bb8 + 4 * U32(0, 0x00ae0d68), mem1[0]); SU32(0, 0x00ae0d68, U32(0, 0x00ae0d68) + 1);
            SU32(0, RX_TBL_DATABASE_PTR, RX_TBL_DEFAULT_DB);
            SU32(key, 0, mem1[0] + 0x3c);
            SU32(rx_tbl_name_map_index(0x00ae0b80 + 0x18, key), 0, mem1[0]);
        } else collection_add(0x00ae0b80, mem1[i], key);
    }
    /* the playlist descriptions: every "Desc" cell unescaped (a copy of the string, FUN_0078feb0, set back through 004f5970) */
    rx_addr pd = rx_tbl_find(0x00adfde8, 0x0093e3d4);                 /* "PlaylistDesc" */
    for (uint32_t row = 0; ; row++) {
        uint32_t rows = U32(pd, 0x14) != U32(pd, 0x10) ? rx_tbl_column_rows(U32(U32(pd, 0x10), 0)) : 0;
        if (row >= rows) break;
        rx_addr col = U32(U32(pd, 0x10), 4 * rx_tbl_column_index(pd, 0x0093e3e4));   /* "Desc" */
        rx_addr cell = U32(U32(col, 0x40), 0xc) + 0x1c * row;
        rx_addr src = rx_tbl_str_cstr(cell);
        uint32_t n = gstrlen(src) + 1;
        rx_addr copy = rx_reflex_aligned_alloc(n);
        crt_strncpy(copy, src, n - 1); SU8(copy, n - 1, 0);
        rx_addr u = unescape(copy);
        rx_tbl_str_init(str);
        rx_tbl_str_assign_n(str, u, gstrlen(u));
        uint32_t ci = rx_tbl_column_index(pd, 0x0093e3e4);
        SU32(pd, 0x68, U32(pd, 0x68) + 1);
        rx_tbl_set_string(U32(U32(pd, 0x10), 4 * ci), row, str);
        rx_tbl_str_tidy(str);
        rx_reflex_aligned_free(copy);
    }
    /* the rider tables from a temporary NetworkRider (FUN_004e1820), the stats table from a temporary NetworkLBStats (FUN_004e0a40) */
    rx_tbl_table_construct_named(t1, 0x00935d6c); SU32(t1, 0, 0x0093ec34);
    rx_tbl_rider_schema(t1);
    rebuild_from(0x00ae0f10, t1, 0x0093e3ec, 1);                       /* "Network Rider List" */
    rebuild_from(0x00ae07a8, t1, 0x0093e400, 1);                       /* "Registry Rider List" */
    rx_tbl_table_construct_named(t2, 0x00935d6c); SU32(t2, 0, 0x0093ec10);
    rx_tbl_stats_schema(t2);
    rebuild_from(0x00ae04d8, t2, 0x0093e414, 0);                       /* "Network Stats" */
    static const rx_addr mem2[16] = {0x00ae0d78, 0x00ae0e98, 0x00ae0460, 0x00ae04d8, 0x00ae1ba8, 0x00ae0f88, 0x00ae0f10, 0x00ae1a28,
                                     0x00ae0898, 0x00ae0550, 0x00ae0b08, 0x00ae1aa0, 0x00ae1000, 0x00ae1078, 0x00ae1938, 0x00ae0730};
    for (int i = 0; i < 16; i++) collection_add(0x00ae0070, mem2[i], key);
    rx_tbl_table_destroy(t2);
    rx_tbl_table_destroy(t1);
    /* GetTickCount */
    if (!U32(0, 0x00d72a34)) SU32(0, 0x00d72a34, rx_reflex_aligned_alloc(1));
    RX_FRAME_LEAVE(F, 0x130);
}

/* the XMLReader virtuals MXRegistry_Load calls (vf7 child 0x1c, vf52 node ints 0xd0, vf57 node bools 0xe4) */
static void* xv(rx_addr r, uint32_t off) {
    void* f = rx_fn(U32(U32(r, 0), off));
    if (!f) { fprintf(stderr, "rxcore: build_tables: XMLReader virtual +%x not ported\n", off); abort(); }
    return f;
}
static rx_addr x_child(rx_addr r, rx_addr n, rx_addr name) { return ((rx_addr (*)(rx_addr, rx_addr, rx_addr))xv(r, 0x1c))(r, n, name); }
static void x_values(rx_addr r, uint32_t off, rx_addr n, rx_addr dst) { ((uint32_t (*)(rx_addr, rx_addr, rx_addr, uint32_t))xv(r, off))(r, n, dst, 1); }

/* MXRegistry_Load 004e6c30 (registry = the engine's, [[0x00a95844] + 0xc]; db = the asset Database) */
void rx_tbl_mxregistry_load(fmt_install* in, rx_addr db, rx_addr registry) {
    /* GetTickCount: not kept */
    RX_FRAME_ENTER(N, 0x40);
    for (uint32_t k = 0; k < 0x40; k++) SU8(N, k, 0);                 /* the name buffer (memset 0x3f after a 0 byte) */
    SU32(0, RX_TBL_DATABASE_PTR, RX_TBL_DEFAULT_DB);
    rx_addr rec = rx_registry_vf1_find(registry, 0x0093e42c, 0x0093e424, 1);   /* Registry::vf1("MXGame", "Tables", 1) */
    if (rec) rx_registry_vf20_string(registry, rec, N, 0x40);         /* vf20 */
    char tables[0x40]; for (uint32_t k = 0; k < 0x40; k++) tables[k] = (char)U8(N, k);
    tables[0x3f] = 0;
    RX_FRAME_LEAVE(N, 0x40);
    rx_tbl_database_clear_rows(RX_TBL_DEFAULT_DB);
    if (!rx_database_load(db, in, tables, "Database\\", 1, 0)) { fprintf(stderr, "rxcore: build_tables: database %s not loaded\n", tables); abort(); }   /* vf4 */
    rx_database_load_package(db, in, tables, tables, 0, 0);          /* vf15 */
    for (int k = 0; k < 4; k++) {
        rx_addr pdb = RX_TBL_PLAYER_DB(k);
        for (int32_t i = 0; i < I32(pdb, 0x1e8); i++) rx_tbl_table_clear_rows(U32(pdb, 0x38 + 4 * (uint32_t)i));   /* 004ecdb0 */
        rx_tbl_restore_player_defaults(pdb);                          /* 004eea10 */
    }
    RX_FRAME_ENTER(F, 0x4c0);
    const rx_addr reader = F + 0x10;
    rx_xml_reader_construct(reader);
    if (reader_load_file(in, reader, 0x0093e448)) {                   /* "MXRegistry.bxml" */
        rx_addr root = U32(reader, 0x38), e;
        rx_addr g = x_child(reader, root, 0x0093e42c);                /* "MXGame" */
        SU32(0, 0x00ae1b8c, 0);
        if ((e = x_child(reader, g, 0x0093e458))) x_values(reader, 0xe4, e, 0x00ae1b8c);   /* "AmbientFreerideAI" */
        SU32(0, 0x00ae1b90, 0);
        if ((e = x_child(reader, g, 0x0093e46c))) x_values(reader, 0xe4, e, 0x00ae1b90);   /* "TuneAI" */
        SU32(0, 0x00ae1b94, 0);
        if ((e = x_child(reader, g, 0x0093e474))) x_values(reader, 0xd0, e, 0x00ae1b94);   /* "TuneAICount" */
        SU32(0, 0x00ae1ba0, 3);
        if ((e = x_child(reader, g, 0x00936e48))) x_values(reader, 0xd0, e, 0x00ae1ba0);   /* "AIDifficulty" */
        SU32(0, 0x00ae1ba4, 3);
        if ((e = x_child(reader, g, 0x0093e480))) x_values(reader, 0xd0, e, 0x00ae1ba4);   /* "AITier" */
        SU32(0, 0x00ae1ba4, U32(0, 0x00ae1ba4) - 1);
        SU32(0, 0x00ae1b9c, 1);
        if ((e = x_child(reader, g, 0x0093e488))) x_values(reader, 0xe4, e, 0x00ae1b9c);   /* "AILapPacing" */
        SU32(0, 0x00ae1b98, 0);
        if ((e = x_child(reader, g, 0x0093e494))) x_values(reader, 0xe4, e, 0x00ae1b98);   /* "RunAITerrainTests" */
    } else { fprintf(stderr, "rxcore: build_tables: MXRegistry.bxml not read\n"); abort(); }   /* 004e6d7f with no file: not in the boot */
    rx_xml_reader_destroy(reader);
    RX_FRAME_LEAVE(F, 0x4c0);
}

/* FUN_0053c410's table branch: every DLC database the finder lists (its vf9 glob, vf2 the directory ".\Database\") whose name holds
 * "Tables" loaded with its package of the same name (vf4 + vf15) into [0x00a89cfc]; names = the finder's list in order. The "MXUI"
 * databases of the same loop (+ FUN_005dbcb0's localized package) are the UI's. */
void rx_tbl_load_dlc_tables(fmt_install* in, rx_addr db, const char* const* names, int n) {
    for (int i = 0; i < n; i++) {
        if (!strstr(names[i], "Tables")) continue;
        if (!rx_database_load(db, in, names[i], ".\\Database\\", 1, 0)) { fprintf(stderr, "rxcore: build_tables: %s not loaded\n", names[i]); abort(); }
        rx_database_load_package(db, in, names[i], names[i], 0, 0);
    }
}
