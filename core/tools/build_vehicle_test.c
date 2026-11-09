/* build_vehicle_test.c -- test driver of construction unit 6 (vehicles, core/notes/construction.md section 15): builds the unit's
 * objects from the install into the core's arena, then writes the canonical dump + image (rx_canon.h) and an anchor file for
 * core/tools/build_vehicle_cmp.py (snapshot compare).
 *   build_vehicle_test <install> <out prefix> <level> <units> [slots file] [asset script]
 *     units (comma separated, in order):
 *       lists          the reference event's rows of Vehicle List / Rider List (VehicleList_AddRow / RiderList_AddRow) and
 *                      Player List (TEST STAND-IN for the activity unit); every other table is built by unit 12a
 *                      (build_tables.h) from the install while the asset script loads
 *       world          the MXWorld (FUN_00765e20) and the physics world (FUN_0075da90) of unit 3
 *       sim            per slot of the slots file: RavenVehicleMotocross_Alloc + _Construct and a RavenSimulationMotocross
 *                      (_aligned_malloc(0x8fc0) + 0043ac40 with the slot's engine class)
 *     slots file: lines "<slot> <orig V> <orig S> <engine class>" (the original's objects, for the anchors)
 * Setup STAND-INs (other units' work, as tools/build_world_test.c): the exe's .rdata / .data, the runtime constants, XenonCore /
 * MXGame / scene, the Database with the asset script (every core loader; the table database of unit 12a). */
#include "../src/fmt_db.h"
#include "../src/rx_heap.h"
#include "../src/rx_d3d.h"
#include "../game/build_alloc.h"
#include "../game/build_asset.h"
#include "../game/build_load.h"
#include "../game/build_world.h"
#include "../game/build_mxworld.h"
#include "../game/build_vehicle.h"
#include "../game/build_terrain.h"
#include "../src/fmt_terrain.h"
#include "../src/fmt_scene.h"
#include "../game/build_tables.h"
#include "../game/build_globals.h"
#include "../src/rx_image.h"
#include "rx_canon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>

/* an abort prints the host backtrace (the "unported game function" stops name only the address) */
static void on_abort(int sig) { void* f[32]; int n = backtrace(f, 32); backtrace_symbols_fd(f, n, 2); signal(sig, SIG_DFL); raise(sig); }

static rx_canon_root roots[4096]; static int nroots;
static void root(rx_addr a) { roots[nroots].addr = a; roots[nroots].key = (uint32_t)nroots; nroots++; }
static FILE* anchors;

/* the exe's .rdata / .data / .tls (rx_image_load: the product's loader), the runtime constants */
static int load_data(fmt_install* in) {
    if (rx_image_load(fmt_install_vfs(in)) != 0) { fprintf(stderr, "image: %s\n", fmt_error()); return 0; }
    rx_load_runtime_constants();
    return 1;
}

/* STAND-IN unit 2g: a TextureAsset with an R32F texture holding raw floats (tools/build_world_test.c) */
static rx_addr texture_asset_r32f(rx_addr dev, const float* data, uint32_t w, uint32_t h) {
    rx_addr a = rx_reflex_aligned_alloc(0x24), holder = rx_reflex_aligned_alloc(4);
    SU32(a, 0, 0x009462d0); SU32(a, 0x10, holder);
    rx_d3d_create_texture(dev, w, h, 1, 0, RX_D3DFMT_R32F, RX_D3DPOOL_MANAGED, holder);
    uint32_t pitch; rx_addr bits = rx_d3d_bits(U32(holder, 0), 0, &pitch);
    for (uint32_t y = 0; y < h; y++) for (uint32_t x = 0; x < w; x++) SF32(bits + y * pitch, 4 * x, data[y * w + x]);
    return a;
}
/* STAND-IN unit 4: Entity::vf5's base fields the loaders read (scene, name) */
static void entity_base(rx_addr e, rx_addr scene, const char* name) {
    SU32(e, 0x10, scene);
    uint32_t n = (uint32_t)strlen(name);
    rx_addr s = rx_reflex_aligned_alloc(n + 1);
    for (uint32_t k = 0; k <= n; k++) SU8(s, k, (uint8_t)name[k]);
    SU32(e, 0x18, s);
}

/* STAND-IN unit 1: the asset loads of the reference boot (the recorded script; unit 1's test oracle) */
static void load_assets(fmt_install* in, rx_addr db, const char* script) {
    rx_asset_register_core_loaders();   /* includes unit 12a's tbldata loader */
    /* unit 12a: the static tables, their schemas (FUN_00756760), the loaders' table database (MXRegistry_Load) */
    rx_tbl_static_init();
    rx_tbl_create_schemas();
    SU32(0, RX_TBL_DATABASE_PTR, RX_TBL_DEFAULT_DB);
    rx_tbl_database_clear_rows(RX_TBL_DEFAULT_DB);
    rx_database_register_all(db);
    FILE* sc = fopen(script, "r"); char line[1024];
    if (!sc) { fprintf(stderr, "script %s\n", script); exit(1); }
    while (fgets(line, sizeof line, sc)) {
        char a[256], b[256], c[64], d[64];
        if (line[0] == '#' || line[0] == '\n' || !strncmp(line, "register", 8)) continue;
        if (!strncmp(line, "cacheon", 7)) rx_database_cache_enable(db);
        else if (sscanf(line, "cacheadd %255s %255s", a, b) == 2) rx_database_cache_add(db, a, b);
        else if (sscanf(line, "db %255s %255s %63s %63s", a, b, c, d) == 4) rx_database_load(db, in, a, b, (uint32_t)strtoul(c, NULL, 16), (uint32_t)strtoul(d, NULL, 16));
        else if (sscanf(line, "pkg %255s %255s %63s %63s", a, b, c, d) == 4) {
            rx_database_load_package(db, in, a, b, 0, (uint32_t)strtoul(d, NULL, 16));
            if (!strcmp(a, "MXTables_Default"))   /* MXRegistry_Load 004e6c30: the four player databases restored (unit 12a) */
                for (int k = 0; k < 4; k++) {
                    rx_addr pdb = RX_TBL_PLAYER_DB(k);
                    for (int32_t i = 0; i < I32(pdb, 0x1e8); i++) rx_tbl_table_clear_rows(U32(pdb, 0x38 + 4 * (uint32_t)i));
                    rx_tbl_restore_player_defaults(pdb);
                }
        }
        else if (sscanf(line, "new %255s %255s", a, b) == 2) rx_database_add_asset(db, b, 0, rx_asset_tag_lo(a), rx_asset_tag_hi(a));
        else if (sscanf(line, "unload %255s %255s", a, b) == 2) rx_database_unload_package(db, a, b);
        else if (sscanf(line, "requests %63s", c) == 1) for (unsigned long k = strtoul(c, NULL, 10); k; k--) rx_database_request(db);
    }
    fclose(sc);
}

/* ---- the list tables of the reference event (National NAT_Concord, 12 riders, AI 95) ----
 * Vehicle List / Rider List: the activity's VehicleList_AddRow / RiderList_AddRow calls (unit 12a's ports), with the reference boot's
 * values (owner 0 = the local player's garage, 4 = the AI table; the owner ids are the AI vehicles / riders the event drew).
 * Player List: TEST STAND-IN for the activity unit (EnsureLocalPlayers 0059e030 + the PlayerList_* setters): one row per rider
 * through the Rdb add-row API with the reference values of every int column and the names. */
static const uint32_t k_owner_vehicle[12] = {0, 463, 287, 91, 378, 280, 357, 329, 126, 638, 119, 582};
static const uint32_t k_owner_rider[12] = {0, 87, 58, 29, 72, 57, 69, 64, 34, 111, 33, 103};
static const int32_t k_gear_set[12] = {-1, 79, 50, 19, 64, 49, 61, 56, 25, 120, 24, 111};
static const char* k_player_names[12] = {"Player 1", "Steve Snow", "Jessica Peters", "Ali Bouda", "Miguel Marquez", "Jered McFerron",
                                         "Melissa Saito", "Dusty Weasle", "Brian Coonce", "Nick Wlodyka", "Brad Bussell", "Tristan Gragg"};
static rx_addr game_cstr(const char* s) {
    size_t n = strlen(s); rx_addr p = rx_reflex_aligned_alloc((uint32_t)n + 1);
    for (size_t k = 0; k <= n; k++) rx_wr8(p + (uint32_t)k, (uint8_t)s[k]);
    return p;
}
static void lists_build(void) {
    /* the ids of the reference event start at 1: the id counters 0x00ae063c / 0x00ae1a24 (-1 from the schemas) hold 0 when the
     * activity adds the event's rows (the frontend's earlier rows and their removal are the activity unit's): set here */
    SU32(0, 0x00ae063c, 0); SU32(0, 0x00ae1a24, 0);
    for (int i = 0; i < 12; i++) rx_tbl_vehicle_list_add_row(i ? 4 : 0, k_owner_vehicle[i]);
    for (int i = 0; i < 12; i++) rx_tbl_rider_list_add_row(i ? 4 : 0, k_owner_rider[i], (uint32_t)k_gear_set[i]);
    static const char* cols[] = {"PlayerType", "PlayerLocal", "ActivityType", "ActivityArea", "VehicleId", "RiderId", "Controller",
                                 "Viewport", "Disconnected", "Active", "StartZone", "UniqueId", "Ready", "Loaded", "Team", "Spectator",
                                 "AIDifficulty", "RequestedActivityType", "RequestedActivityArea", "RequestedVehicleId", "RequestedRiderId"};
    enum { NC = sizeof cols / sizeof cols[0] };
    rx_addr names[NC]; for (int c = 0; c < NC; c++) names[c] = game_cstr(cols[c]);
    const rx_addr t = 0x00ae0e08, F = rx_reflex_aligned_alloc(0x100), vec = F, crit = F + 0x20, out = F + 0xd0, str = F + 0xe0;
    for (int i = 0; i < 12; i++) {
        const int32_t v[NC] = {i ? 1 : 0, 1, 0, 0, i + 1, i + 1, i ? -1 : 0, i ? -1 : 0, 0, 1, -1, -1, 0, 0, i & 1, 0,
                               i == 0 ? 0 : i < 9 ? 2 : 3, 61, -1, -1, -1};
        rx_tbl_crit_vec_init(vec);
        for (int c = 0; c < NC; c++) { rx_tbl_crit_init_int(crit, names[c], 0, (uint32_t)v[c]); rx_tbl_crit_push(vec, crit); rx_tbl_crit_destroy(crit); }
        rx_tbl_add_row(t, out, vec);
        rx_tbl_crit_vec_destroy(vec);
        const uint32_t row = U32(out, 0);
        static const char* scols[3] = {"PlayerName", "DisplayName", "MXProfileName"};
        for (int c = 0; c < 3; c++) {
            const char* sv = c < 2 || i == 0 ? k_player_names[i] : "";
            rx_addr p = game_cstr(sv);
            rx_tbl_str_init(str); rx_tbl_str_assign_n(str, p, (uint32_t)strlen(sv));
            uint32_t ci = rx_tbl_column_index(t, game_cstr(scols[c]));
            rx_tbl_set_string(U32(U32(t, 0x10), 4 * ci), row, str);
            rx_tbl_str_tidy(str);
        }
    }
    printf("lists: Vehicle List %u, Rider List %u, Player List %u rows\n", rx_tbl_row_count(0x00ae05c8), rx_tbl_row_count(0x00ae19b0), rx_tbl_row_count(t));
}

typedef struct { int slot; uint32_t ov, os, cls; } slot_t;

int main(int argc, char** argv) {
    signal(SIGABRT, on_abort);
    if (argc < 5) { fprintf(stderr, "usage: build_vehicle_test <install> <out prefix> <level> <units> [slots file] [asset script]\n"); return 2; }
    if (!rx_mem_init()) { fprintf(stderr, "arena reservation failed\n"); return 1; }
    slot_t slots[16]; int nslots = 0;
    if (argc > 5) {
        FILE* f = fopen(argv[5], "r");
        if (f) { while (nslots < 16 && fscanf(f, "%d %x %x %u", &slots[nslots].slot, &slots[nslots].ov, &slots[nslots].os, &slots[nslots].cls) == 4) nslots++; fclose(f); }
    }
    fmt_install* in = fmt_install_open(argv[1]);
    if (!in) { fprintf(stderr, "install: %s\n", fmt_error()); return 1; }
    if (!load_data(in)) return 1;
    /* the runtime DLLs' data (unit 12b: rx_image_load_crt, the DLL's startup rx_crt_startup): the CRT ports' locale / per-thread data */
    if (rx_image_load_crt(fmt_install_vfs(in)) != 0) { fprintf(stderr, "crt image: %s\n", fmt_error()); return 1; }
    rx_crt_startup();
    rx_phys_props_set_install(in);
    rx_database_set_install(in);   /* Database vf4 / vf15 (the package requests) served at once */
    /* the engine context: XenonCore (unit 12b, FUN_00843950: the renderer settings' defaults, texture quality +0x64 = 2) at
     * [0x00a95844] / [0x00a95960] */
    rx_addr core = rx_engine_context_create(), game = rx_reflex_aligned_alloc(0x16000);
    rx_addr scene = rx_reflex_aligned_alloc(0x2d20), decls = rx_reflex_aligned_alloc(0x7f0);
    SU32(0, 0x00a946b8, game);
    SU32(core, 0x20, scene);
    rx_vec_proxy_init16(decls + 0x18); SU32(decls, 0x24, 0); SU32(decls, 0x28, 0); SU32(decls, 0x2c, 0);
    SU32(core, 0x14, decls);
    const rx_addr dev = rx_d3d_device_create();
    SU32(core, 0x1ec, dev);
    /* the scene's light-receiver list and the 0x2a68 array the terrain / entity loaders append to (as tools/build_world_test.c) */
    SU32(scene, 0x1a18 + 0x10c8, rx_reflex_aligned_alloc(0x40 * 0x20)); SU32(scene, 0x1a18 + 0x10cc, 0);
    SU32(scene, 0x1a18 + 0x10d0, 0x40); SU32(scene, 0x1a18 + 0x10d4, 0x40);
    SU32(scene, 0x2a68, rx_reflex_aligned_alloc(0x100 * 4)); SU32(scene, 0x2a6c, 0); SU32(scene, 0x2a70, 0x100); SU32(scene, 0x2a74, 0x100);
    fmt_level* lv = fmt_level_open(in, argv[3]);
    if (!lv) { fprintf(stderr, "level %s: %s\n", argv[3], fmt_error()); return 1; }
    /* STAND-IN (ReflexWinMain 008814a0, not ported: the GraphicsConfig keys via GraphicsConfig_FindKey 00883820 overwrite the
     * XenonCore defaults; the reference config gives the display 1280 x 720 (+0x44 / +0x48) and TextureQuality 2 (+0x64, 008816e2 ..
     * 00881701, the ctor's default too)) */
    SU32(core, 0x3c + 8, 0x500); SU32(core, 0x3c + 0xc, 0x2d0);
    rx_addr db = rx_database_create();
    SU32(core, 8, db);
    if (argc > 6) load_assets(in, db, argv[6]);
    char path[1024];
    snprintf(path, sizeof path, "%s-anchors.txt", argv[2]);
    anchors = fopen(path, "w");
    rx_addr mxw = 0, pw = 0;
    char units[1024]; snprintf(units, sizeof units, "%s", argv[4]);
    for (char* u = strtok(units, ","); u; u = strtok(NULL, ",")) {
        if (!strcmp(u, "lists")) lists_build();
        else if (!strcmp(u, "world")) {
            if (!U32(0, 0x00d6925c)) rx_command_registry_init();
            mxw = rx_reflex_aligned_alloc(0x22150);
            rx_mxworld_construct(mxw, game + 0x39c0);
            SU32(game, 0x5a54, mxw);
            pw = rx_physics_world_create(scene);
            SU32(mxw, 0x20134, pw);
            root(mxw); root(pw);
            printf("MXWorld %08x physics world %08x\n", mxw, pw);
        } else if (!strcmp(u, "terrain")) {
            /* the level's HFTerrain entity (HFTerrainRSData): the surface with its deformation patch, the base grid, the registry
             * entry the height queries find (tools/build_world_test.c "terrain") */
            fmt_scene* sc = fmt_level_scene(lv);
            int sub = fmt_scene_find_subscene(sc, fmt_level_base_subscene(lv)), ne = 0;
            fmt_scene_entities(sc, sub, &ne);
            const fmt_entity* rs = fmt_scene_find_type(sc, sub, "HFTerrainRSData", 0);
            if (!rs) { fprintf(stderr, "no HFTerrainRSData\n"); return 1; }
            fmt_terrain* t = fmt_terrain_load(lv);
            if (!t) { fprintf(stderr, "terrain: %s\n", fmt_error()); return 1; }
            rx_addr hm = rx_rs_asset_handle(rs, "HeightMapAsset");
            if (!U32(hm, 0x44)) SU32(hm, 0x44, texture_asset_r32f(dev, t->heights, (uint32_t)t->size, (uint32_t)t->size));
            rx_addr e = rx_reflex_aligned_alloc(0x45a0);
            rx_hfterrain_entity_construct_temp(e);
            entity_base(e, scene, t->params.name ? t->params.name : "");
            rx_hfterrain_entity_load(e, rs);
            fmt_terrain_free(t);
            root(e);
            printf("terrain entity %08x (surface %08x)\n", e, e + 0x90);
        } else if (!strcmp(u, "sim")) {
            for (int k = 0; k < nslots; k++) {
                rx_addr v = rx_vehicle_motocross_alloc();
                rx_vehicle_motocross_construct(v);
                rx_addr s = rx_crt_aligned_malloc(0x8fc0, 16);
                rx_sim_moto_construct(s, v + 0x16e0, slots[k].cls);
                SU32(v, 0x16d4, s); SU32(v, 0x234, s);
                root(v); root(s);
                fprintf(anchors, "A:%08x=%08x\nA:%08x=%08x\n", slots[k].ov, v, slots[k].os, s);
                printf("slot %d: V %08x S %08x\n", slots[k].slot, v, s);
            }
        } else if (!strcmp(u, "vehicles")) {
            /* the vehicle list (MXGame + 0x154b0, 005404f0), the parts loader (00540610), then per slot of the slots file (VehicleId
             * = slot + 1) the activity's spec (00542b20, not high resolution) and VehicleFactory_CreateVehicle (00542b90) */
            if (!mxw) { fprintf(stderr, "vehicles: the world unit first\n"); return 2; }
            const rx_addr list = game + 0x154b0;
            rx_vehicle_list_construct(list);
            rx_vehicle_list_init_loader(list);
            for (int k = 0; k < nslots; k++) {
                uint32_t id = (uint32_t)slots[k].slot + 1;
                rx_addr spec = rx_vehicle_list_create_spec(list, id, 0);
                rx_addr v = rx_vehicle_factory_create(mxw, list, id);
                root(spec); root(v);
                fprintf(anchors, "A:%08x=%08x\nA:%08x=%08x\n", slots[k].ov, v, slots[k].os, U32(v, 0x234));
                printf("slot %d: spec %08x V %08x S %08x\n", slots[k].slot, spec, v, U32(v, 0x234));
            }
        } else { fprintf(stderr, "unknown unit %s\n", u); return 2; }
    }
    root(U32(0, 0x00a95844));
    fclose(anchors);
    char info[256] = "";
    snprintf(path, sizeof path, "%s-canon.bin", argv[2]);
    if (!rx_canon_write(path, roots, nroots, getenv("RX_CANON_CLASSES"), info, sizeof info)) return 1;
    snprintf(path, sizeof path, "%s-world.bin", argv[2]);
    if (!rx_canon_image(path)) return 1;
    printf("heap blocks %d; %s\n", rx_heap_blocks(), info);
    return 0;
}
