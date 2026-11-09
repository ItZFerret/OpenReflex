/* build_tables_test.c -- unit 12a test driver (core/notes/construction.md section 12): builds the table database from the install with
 * the core's code and writes the arena's canonical dump + image for core/tools/rx_canon_cmp.py / build_cmp.py.
 *   build_tables_test <install root> <out prefix>
 * Sequence (the reference boot's table operations, construction.md 8.4 / 12): the exe's .rdata / .data (STAND-IN for the globals
 * unit), the static initializers (rx_tbl_static_init), the static tables' schemas (rx_tbl_create_schemas), the asset system with the tbldata loader, [0x00a89cfc] = 0x00ae0910 and
 * MXRegistry_Load's table part (MXTables_Default database + package, the four player databases restored), then FUN_0053c410's
 * DLC table databases (MXTables_DLC003 .. 005: database + package each). Anchors: the static databases (same addresses). */
#include "../src/fmt_db.h"
#include "../src/fmt_vfs.h"
#include "../src/rx_heap.h"
#include "../game/build_asset.h"
#include "../game/build_load.h"
#include "../game/build_tables.h"
#include "../game/build_registry.h"
#include "rx_canon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* STAND-IN (globals unit): the exe's .rdata and initialized .data */
static int load_data(fmt_install* in) {
    fmt_buf b = {0};
    if (fmt_vfs_read_file(fmt_install_vfs(in), "MXReflex.exe", &b) != 0) { fprintf(stderr, "MXReflex.exe: %s\n", fmt_error()); return 0; }
    const uint8_t* p = b.data; uint32_t pe = (uint32_t)p[0x3c] | (uint32_t)p[0x3d] << 8;
    uint32_t nsec = (uint32_t)p[pe + 6] | (uint32_t)p[pe + 7] << 8, opt = (uint32_t)p[pe + 0x14] | (uint32_t)p[pe + 0x15] << 8;
    for (uint32_t s = 0; s < nsec; s++) {
        const uint8_t* h = p + pe + 0x18 + opt + 0x28 * s;
        if (memcmp(h, ".data", 5) && memcmp(h, ".rdata", 6)) continue;
        uint32_t va, raw, off; memcpy(&va, h + 12, 4); memcpy(&raw, h + 16, 4); memcpy(&off, h + 20, 4);
        for (uint32_t k = 0; k < raw; k++) rx_wr8(0x00400000 + va + k, p[off + k]);
    }
    fmt_buf_free(&b);
    return 1;
}


int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: build_tables_test <install> <out prefix>\n"); return 2; }
    if (!rx_mem_init()) { fprintf(stderr, "arena reservation failed\n"); return 1; }
    fmt_install* in = fmt_install_open(argv[1]);
    if (!in) { fprintf(stderr, "install: %s\n", fmt_error()); return 1; }
    if (!load_data(in)) return 1;
    rx_tbl_static_init();                                              /* the exe's static initializers */
    rx_tbl_create_schemas();                                           /* FUN_00756760's schema calls (MXGame construction) */
    rx_tbl_mxgame_startup_tables(in);                                  /* FUN_004e6280 (MXGame::Startup) */
    rx_addr db = rx_database_create();
    rx_database_register_all(db);
    rx_tbl_register_loader();
    rx_addr registry = rx_registry_startup(in);                        /* Engine::Startup's Registry (engine + 0xc) */
    rx_tbl_mxregistry_load(in, db, registry);                          /* MXRegistry_Load */
    if (!getenv("RX_TBL_NO_DLC")) {                                    /* FUN_0053c410: the finder's list of the reference install */
        static const char* const dlc[] = {"MXTables_DLC003", "MXTables_DLC004", "MXTables_DLC005", "MXUI_DLC001", "MXUI_DLC002",
                                          "MXUI_DLC003", "MXUI_DLC004", "MXUI_DLC005"};
        rx_tbl_load_dlc_tables(in, db, dlc, 8);
    }
    int ok = 1;
    /* RX_TBL_TEST_ROWS=1: the reference world's Vehicle / Rider List rows (activity's calls, values read from the reference world)
     * replayed through VehicleList_AddRow / RiderList_AddRow, to check the add-row path (test data, not the product's sequence) */
    if (getenv("RX_TBL_TEST_ROWS")) {
        static const uint32_t veh[11][2] = {{4,463},{4,287},{4,91},{4,378},{4,280},{4,357},{4,329},{4,126},{4,638},{4,119},{4,582}};
        static const uint32_t rid[11][3] = {{4,87,79},{4,58,50},{4,29,19},{4,72,64},{4,57,49},{4,69,61},{4,64,56},{4,34,25},{4,111,120},{4,33,24},{4,103,111}};
        SU32(0, 0x00ae063c, 0); SU32(0, 0x00ae1a24, 0);
        rx_tbl_vehicle_list_add_row(0, 0);
        for (int i = 0; i < 11; i++) rx_tbl_vehicle_list_add_row(veh[i][0], veh[i][1]);
        rx_tbl_rider_list_add_row(0, 0, 0xffffffffu);
        for (int i = 0; i < 11; i++) rx_tbl_rider_list_add_row(rid[i][0], rid[i][1], rid[i][2]);
    }
    char path[1024];
    snprintf(path, sizeof path, "%s-anchors.txt", argv[2]);
    FILE* anchors = fopen(path, "w");
    fprintf(anchors, "A:%08x=%08x\n", RX_TBL_DEFAULT_DB, RX_TBL_DEFAULT_DB);
    fprintf(anchors, "R:%08x\n", registry);                           /* the core's Registry (the original's: [[0x00a95844] + 0xc]) */
    for (int k = 0; k < 4; k++) fprintf(anchors, "A:%08x=%08x\n", RX_TBL_PLAYER_DB(k), RX_TBL_PLAYER_DB(k));
    fprintf(anchors, "A:107e6120=%08x\n", registry);                  /* the reference world's [[0x00a95844] + 0xc] */
    fclose(anchors);
    rx_canon_root* roots = (rx_canon_root*)malloc(2 * sizeof *roots);
    roots[0].addr = db; roots[0].key = 0;
    roots[1].addr = registry; roots[1].key = 1;                        /* the engine's Registry (the core has no engine object) */
    char info[256] = "";
    snprintf(path, sizeof path, "%s-canon.bin", argv[2]);
    if (!rx_canon_write(path, roots, 2, getenv("RX_CANON_CLASSES"), info, sizeof info)) return 1;
    snprintf(path, sizeof path, "%s-world.bin", argv[2]);
    if (!rx_canon_image(path)) return 1;
    printf("tables: default %u, player %u / %u / %u / %u; heap blocks %d; %s\n", U32(RX_TBL_DEFAULT_DB, 0x1e8),
           U32(RX_TBL_PLAYER_DB(0), 0x1e8), U32(RX_TBL_PLAYER_DB(1), 0x1e8), U32(RX_TBL_PLAYER_DB(2), 0x1e8),
           U32(RX_TBL_PLAYER_DB(3), 0x1e8), rx_heap_blocks(), info);
    return ok ? 0 : 1;
}
