/* build_vehicle_spec.c -- construction unit 6 (vehicles, core/notes/construction.md section 15): the vehicle specs the activity
 * creates for the event's vehicles, their model / part records and the part packages' requests. Hand transliterations of the
 * disassembly:
 *   00542b20 VehicleList_CreateSpec (s0 list, s1 VehicleId, s2 high resolution; RET 0xc) -> 00540950 / 005409d0 the spec
 *   (00541220 the owner's tables, 0053ff30 the spec's init), 005407d0 / 0053faf0 / 0053fb80 the model record, 00540660 / 0053f7d0 the
 *   part record, 00540210 the part's packages added to the loader's cache, 005401b0 / 005403b0 the spec's packages requested,
 *   005400c0 (the factory / slot names copied to the frame), the enum lookups 004e7040 / 004e7160 / 004e7210 / 004e72c0 (004dcd50 ..
 *   004dcee0) and the Rdb row-reference readers 0041aaf0 / 004ec210 / 004eb920 / 004fa890 / 00419b20 / 0041a480.
 *
 * The vehicle list L (MXGame + 0x154b0): +0 the part records (Rdb pointer array {data, count, capacity, growth}), +0x14 the model
 * records (the same), +0x28 the specs, +0x3c the VehicleHierarchies table, +0x40 the made vehicles.
 * A model record M (100 bytes): +0 VehicleModelId, +4 VehicleTypeId, +8 VehicleHierarchyId, +0xc the factory (the type's
 * FactoryName: 0 MotocrossBike, 1 Atv, 2 MonsterTruck, 3 TrophyTruck, 4 SandRail, 5 Buggy, 6 GolfCart, 7 Rhino; 8 none), +0x10 the
 * engine class (VehicleEngineClass's FactoryName index of 4), +0x14 the engine size (VehicleEngineSize's of 11), +0x18 references,
 * +0x1c .. +0x30 DefaultEngine1 / 2, DefaultSuspension1 / 2, DefaultTire1 / 2, +0x34 = +0x38 = 50, +0x3c the model's parts (Rdb int
 * array {data, count, capacity 0x20, growth 8, max}), +0x50 its default parts (capacity 0x11).
 * A part record P (0x50): +0 std::string the material package, +0x1c std::string the model package, +0x38 HRAvailable, +0x3c the
 * slot (VehiclePartType's FactoryName index of 17), +0x40 VehiclePartId, +0x44 references, +0x48 / +0x4c DefaultTuneAttribute1 / 2.
 * A spec S (200 bytes): +0 0, +4 VehicleId, +8 M, +0xc + 4k the parts by slot, +0x50 + 4k the pending ones, +0x94 the crew chief's
 * shirt texture, +0x98 the owner (0..3 a player's garage, 4 AI, 5 / 6 / 7 / 8 static tables), +0x9c the owner's vehicle id, +0xa0 ..
 * +0xbc the tuning, +0xc0 high resolution, +0xc4 the vehicle. */
#include "build_vehicle.h"
#include "build_vehicle_util.h"
#include "build_rider.h"
#include "build_alloc.h"
#include "world_misc.h"
#include "crt.h"
#include <stdio.h>
#include <stdlib.h>

#define VS_DB       0x00ae0910u   /* the static table database */
#define VS_LOADER   0x00d92ef0u   /* the parts loader (+4 its package cache) */
static void vs_fatal(const char* what, uint32_t a) { fprintf(stderr, "rxcore: vehicle specs: %s (%08x)\n", what, a); abort(); }
static void invalid_parameter(void) { vs_fatal("_invalid_parameter_noinfo (an index check failed)", 0); }
static rx_addr chars(rx_addr s) { return U32(s, 0x18) >= 0x10 ? U32(s, 4) : s + 4; }

/* ---- Rdb readers ---- */
/* a column object of a table by name (Table_ColumnIndex, the index checked as the vector access) */
static rx_addr column_of(rx_addr table, rx_addr name) {
    uint32_t i = rx_tbl_column_index(table, name);
    if (i >= (U32(table, 0x14) - U32(table, 0x10)) >> 2) invalid_parameter();
    return U32(U32(table, 0x10), 4 * i);
}
/* 0041aaf0 (EAX = store, EDI = column index; s0 = row; RET 4): &the int cell */
static rx_addr cell(rx_addr store, uint32_t col, uint32_t row) {
    if (col >= (U32(store, 0x14) - U32(store, 0x10)) >> 2) invalid_parameter();
    rx_addr v = U32(U32(U32(store, 0x10), 4 * col), 0x40);
    if (row >= (U32(v, 0x10) - U32(v, 0xc)) >> 2) invalid_parameter();
    return U32(v, 0xc) + 4 * row;
}
/* the int cell of a row reference {+0 row, +8 store} by column name */
static uint32_t ref_int(rx_addr ref, rx_addr name) {
    rx_addr store = U32(ref, 8);
    return U32(cell(store, rx_tbl_column_index(store, name), U32(ref, 0)), 0);
}
/* 004ec210 (EAX = row reference; s0 = column name; RET 4): the row the reference's foreign-key column names (004eb920: the parent
 * table +0x24 / its column +0x28; an int key (type 1) through the parent column's unique index, 00418ec0; other key types are not
 * reached) */
rx_addr rx_tbl_ref_value(rx_addr ref, rx_addr name) {
    rx_addr store = U32(ref, 8);
    uint32_t i = rx_tbl_column_index(store, name);
    if (i >= (U32(store, 0x14) - U32(store, 0x10)) >> 2) invalid_parameter();
    rx_addr col = U32(U32(store, 0x10), 4 * i);
    rx_addr parent = U32(col, 0x24);
    uint32_t pi = U32(col, 0x28);
    if (pi >= (U32(parent, 0x14) - U32(parent, 0x10)) >> 2) invalid_parameter();
    rx_addr pcol = U32(U32(parent, 0x10), 4 * pi);
    switch (U32(col, 0x2c)) {
    case 1: {
        rx_addr v = U32(col, 0x40);   /* 004fa890: &the int cell */
        if (U32(ref, 0) >= (U32(v, 0x10) - U32(v, 0xc)) >> 2) invalid_parameter();
        return rx_tbl_row(pcol, I32(U32(v, 0xc) + 4 * U32(ref, 0), 0));
    }
    case 0: case 2: case 4: case 5: case 6: vs_fatal("a foreign key of type 0 / 2 / 4 / 5 / 6 (004eb966 ..) not ported", col);
    default: return 0x009c9ff4u;
    }
}
RX_FUNCTION(0x004ec210, rx_tbl_ref_value);
static int ref_valid(rx_addr ref) { return U32(ref, 8) != 0 && I32(ref, 0) != -1; }

/* the enum lookups (004e7040 ..): the row of `table` whose `keycol` is the id (unique index), its FactoryName string, the index of
 * that name among n names (_stricmp; n when none) */
static uint32_t enum_of(rx_addr table_name, rx_addr keycol, uint32_t id, rx_addr names, uint32_t n) {
    rx_addr t = rx_tbl_find(VS_DB, table_name);
    rx_addr ref = rx_tbl_row(column_of(t, keycol), (int32_t)id);
    rx_addr store = U32(ref, 8);
    rx_addr s = rx_tbl_cell_string(column_of(store, 0x0093e4a8 /* "FactoryName" */), U32(ref, 0));
    rx_addr name = chars(s);
    for (uint32_t i = 0; i < n; i++) if (crt_stricmp(name, U32(names, 4 * i)) == 0) return i;
    return n;
}
static uint32_t vehicle_type_factory(uint32_t id) { return enum_of(0x00935dec, 0x00936ce4, id, 0x009cccd0, 8); }      /* 004e7040 / 004dcd50 */
static uint32_t engine_class_of(uint32_t id) { return enum_of(0x0093e4b4, 0x0093e4c8, id, 0x009cccf0, 4); }            /* 004e7160 / 004dcd90 */
static uint32_t engine_size_of(uint32_t id) { return enum_of(0x0093e4e0, 0x0093e4f4, id, 0x009ccd00, 0xb); }           /* 004e7210 / 004dcdd0 */
static uint32_t part_slot_of(uint32_t id) { return enum_of(0x0093ddfc, 0x0093e508, id, 0x009cce60, 0x11); }            /* 004e72c0 / 004dcee0 */

/* an Rdb pointer / int array {+0 data, +4 count, +8 capacity, +0xc growth} push (the inline grow: capacity + growth) */
static void arr_push(rx_addr a, uint32_t v) {
    if ((int32_t)U32(a, 4) >= (int32_t)U32(a, 8)) {
        uint32_t cap = U32(a, 0xc) + U32(a, 8);
        rx_addr d = rx_reflex_aligned_alloc(rx_mul_sat(cap, 4));
        crt_memcpy(d, U32(a, 0), U32(a, 8) * 4);
        rx_reflex_aligned_free(U32(a, 0));
        SU32(a, 0, d);
        SU32(a, 8, cap);
    }
    SU32(U32(a, 0), 4 * U32(a, 4), v);
    SU32(a, 4, U32(a, 4) + 1);
}

/* ---- the model record ---- */
/* 0053fb80 (EAX = the VehicleModel row reference; s0 = the record; RET 4) */
static void model_fill(rx_addr ref, rx_addr M) {
    if (!ref_valid(ref)) {
        SU32(M, 0xc, 8); SU32(M, 0x10, 4); SU32(M, 0x14, 0xa);
        SU32(M, 0, 0xffffffffu); SU32(M, 4, 0xffffffffu); SU32(M, 8, 0xffffffffu);
        for (uint32_t o = 0x18; o <= 0x38; o += 4) SU32(M, o, 0);
        return;
    }
    rx_addr parts = rx_tbl_find(VS_DB, 0x0093e688 /* "VehicleModelParts" */);
    SU32(M, 0, ref_int(ref, 0x0093e170 /* VehicleModelId */));
    SU32(M, 4, ref_int(ref, 0x00936ce4 /* VehicleTypeId */));
    SU32(M, 0xc, vehicle_type_factory(U32(M, 4)));
    SU32(M, 8, ref_int(ref, 0x00940664 /* VehicleHierarchyId */));
    SU32(M, 0x10, engine_class_of(ref_int(ref, 0x0093e4c8 /* VehicleEngineClassId */)));
    SU32(M, 0x14, engine_size_of(ref_int(ref, 0x0093e4f4 /* VehicleEngineSizeId */)));
    SU32(M, 0x18, 0);
    static const rx_addr defs[6] = {0x00940678, 0x00940688, 0x00940698, 0x009406ac, 0x009406c0, 0x009406d0};   /* DefaultEngine1 .. DefaultTire2 */
    for (int k = 0; k < 6; k++) SU32(M, 0x1c + 4 * (uint32_t)k, ref_int(ref, defs[k]));
    SU32(M, 0x34, 0x32); SU32(M, 0x38, 0x32);
    /* the model's parts: the VehicleModelParts rows of this VehicleModelId (searchable index, rows in order) */
    rx_addr set = rx_tbl_rows(column_of(parts, 0x0093e170), I32(M, 0));
    for (rx_addr n = U32(U32(set, 0x18), 0); n != U32(set, 0x18);) {
        uint32_t row = U32(n, 0xc);
        uint32_t part = U32(cell(parts, rx_tbl_column_index(parts, 0x0093e29c /* VehiclePartId */), row), 0);
        uint32_t def = U32(cell(parts, rx_tbl_column_index(parts, 0x0093e6d0 /* Default */), row), 0);
        arr_push(M + 0x3c, part);
        if (def) {
            if ((int32_t)U32(M, 0x54) >= (int32_t)U32(M, 0x58)) rx_iarr_reserve(M + 0x50, U32(M, 0x5c) + U32(M, 0x58));   /* 007cff20 */
            SU32(U32(M, 0x50), 4 * U32(M, 0x54), part);
            SU32(M, 0x54, U32(M, 0x54) + 1);
        }
        /* ++it (00417df0: the row set's tree, isnil +0x19) */
        if (!U8(U32(n, 8), 0x19)) { n = U32(n, 8); while (!U8(U32(n, 0), 0x19)) n = U32(n, 0); }
        else { rx_addr p = U32(n, 4); while (!U8(p, 0x19) && n == U32(p, 8)) { n = p; p = U32(p, 4); } n = p; }
    }
}
/* 0053faf0 (ESI = the record; s0 = the row reference; RET 4): the two arrays, then 0053fb80 */
static rx_addr model_construct(rx_addr M, rx_addr ref) {
    SU32(M, 0x3c, rx_reflex_aligned_alloc(0x80));
    SU32(M, 0x40, 0); SU32(M, 0x44, 0x20); SU32(M, 0x48, 8); SU32(M, 0x4c, 0x7fffffff);
    SU32(M, 0x50, rx_reflex_aligned_alloc(0x44));
    SU32(M, 0x54, 0); SU32(M, 0x58, 0x11); SU32(M, 0x5c, 0); SU32(M, 0x60, 0x7fffffff);
    model_fill(ref, M);
    return M;
}
/* 005407d0 (EDI = the list; s0 = VehicleModelId; RET 4): the list's record of the model (counted), else a new one (0 when the
 * VehicleModel table has no such row) */
static rx_addr model_get(rx_addr L, uint32_t id) {
    for (int32_t i = 0; i < I32(L, 0x18); i++) {
        rx_addr M = U32(U32(L, 0x14), 4 * (uint32_t)i);
        if (U32(M, 0) == id) { SU32(M, 0x18, U32(M, 0x18) + 1); return M; }
    }
    rx_addr t = rx_tbl_find(VS_DB, 0x0093e180 /* "VehicleModel" */);
    rx_addr ref = rx_tbl_row(column_of(t, 0x0093e170), (int32_t)id);
    if (!ref_valid(ref)) return 0;
    rx_addr M = rx_reflex_aligned_alloc(100);
    M = M ? model_construct(M, ref) : 0;
    SU32(M, 0x18, U32(M, 0x18) + 1);
    arr_push(L + 0x14, M);
    return M;
}

/* ---- the part record ---- */
/* 00540210 (ECX = &{material database, model database}, EDX = the part; s0 = the loader; RET 4): each package name with its database
 * added to the loader's cache (00543830), the "HR_" pair too when the part has a high-resolution version */
static void part_add_packages(rx_addr dbs, rx_addr P, rx_addr loader) {
    for (uint32_t k = 0; k < 2; k++) {
        rx_addr s = P + 0x1c * k;
        if ((int32_t)U32(s, 0x14) <= 0) continue;
        rx_addr name = chars(s), db = U32(dbs, 4 * k);
        if (db) rx_pkgcache_add(loader + 4, name, db);
        if (!U32(P, 0x38)) continue;
        RX_FRAME_ENTER(F, 0x80);
        rx_gear_hr_name(F, db);
        rx_gear_hr_name(F + 0x40, name);
        rx_pkgcache_add(loader + 4, F + 0x40, F);
        RX_FRAME_LEAVE(F, 0x80);
    }
}
/* 0053f7d0 (EAX = the VehiclePart row reference; s0 = the record; RET 4) */
static void part_fill(rx_addr ref, rx_addr P) {
    if (!ref_valid(ref)) {
        crt_str_operator_assign(P + 0x1c, 0x00935d6c /* "" */);
        crt_str_operator_assign(P, 0x00935d6c);
        SU32(P, 0x38, 0); SU32(P, 0x3c, 0x11); SU32(P, 0x40, 0xffffffffu); SU32(P, 0x44, 0); SU32(P, 0x48, 0); SU32(P, 0x4c, 0);
        return;
    }
    SU32(P, 0x40, ref_int(ref, 0x0093e29c /* VehiclePartId */));
    SU32(P, 0x3c, part_slot_of(ref_int(ref, 0x0093e508 /* VehiclePartTypeId */)));
    SU32(P, 0x48, ref_int(ref, 0x0093e6f0 /* DefaultTuneAttribute1 */));
    SU32(P, 0x4c, ref_int(ref, 0x0093e708 /* DefaultTuneAttribute2 */));
    SU32(P, 0x38, ref_int(ref, 0x00940520 /* HRAvailable */));
    SU32(P, 0x44, 0);
    RX_FRAME_ENTER(F, 8);
    SU32(F, 0, 0); SU32(F, 4, 0);   /* the two packages' database names */
    static const rx_addr pkgcols[2] = {0x0094052c /* MaterialPackageDatabaseId */, 0x00940548 /* ModelPackageDatabaseId */};
    for (uint32_t k = 0; k < 2; k++) {
        rx_addr apd = rx_tbl_ref_value(ref, pkgcols[k]);   /* the AssetPackageDatabase row */
        if (!ref_valid(apd)) continue;
        rx_addr pkg = chars(rx_tbl_ref_string(rx_tbl_ref_value(apd, 0x00936fc0 /* PackageId */), 0x00936fcc /* PackageName */));
        rx_addr db = chars(rx_tbl_ref_string(rx_tbl_ref_value(apd, 0x00936fd8 /* DatabaseId */), 0x00936fe4 /* DatabaseName */));
        crt_str_operator_assign(P + 0x1c * k, pkg);
        SU32(F, 4 * k, db);
    }
    part_add_packages(F, P, U32(0, VS_LOADER));
    RX_FRAME_LEAVE(F, 8);
}
/* 00540660 (ESI = the list; s0 = VehiclePartId; RET 4): the list's record of the part (counted), else a new one (0 when the
 * VehiclePart table has no such row) */
static rx_addr part_get(rx_addr L, uint32_t id) {
    for (int32_t i = 0; i < I32(L, 4); i++) {
        rx_addr P = U32(U32(L, 0), 4 * (uint32_t)i);
        if (U32(P, 0x40) == id) { SU32(P, 0x44, U32(P, 0x44) + 1); return P; }
    }
    rx_addr t = rx_tbl_find(VS_DB, 0x0093e69c /* "VehiclePart" */);
    rx_addr ref = rx_tbl_row(column_of(t, 0x0093e29c), (int32_t)id);
    if (!ref_valid(ref)) return 0;
    rx_addr P = rx_reflex_aligned_alloc(0x50);
    if (P) {
        crt_str_ctor(P); crt_str_ctor(P + 0x1c);
        part_fill(ref, P);
    }
    SU32(P, 0x44, U32(P, 0x44) + 1);
    arr_push(L, P);
    return P;
}

/* ---- the spec ---- */
/* 0053ff30 (ESI = the spec) */
static rx_addr spec_init(rx_addr S) {
    SU32(S, 0xc0, 0); SU32(S, 4, 0); SU32(S, 0x98, 9); SU32(S, 0x9c, 0); SU32(S, 0xc4, 0); SU32(S, 0x94, 0); SU32(S, 0, 0);
    crt_memset(S + 0xc, 0, 0x44);
    crt_memset(S + 0x50, 0, 0x44);
    for (uint32_t o = 0xa0; o <= 0xbc; o += 4) SU32(S, o, 0);
    return S;
}
/* 00541220 (ECX = the owner, ESI = &parts table, EDI = &vehicle table; s0 = &the vehicle table's model column; RET 4) */
static void owner_tables(uint32_t owner, rx_addr* vehicles, rx_addr* parts, uint32_t* model_col) {
    switch (owner) {
    case 0: case 1: case 2: case 3: {
        rx_addr db = 0x00ae10f0 + 0x1f4 * owner;   /* the controller's database */
        *vehicles = rx_tbl_find(db, 0x0093e568 /* "PlayerVehicle" */);
        *parts = rx_tbl_find(db, 0x0093e6a8 /* "PlayerVehicleParts" */);
        break;
    }
    case 4: *vehicles = rx_tbl_find(VS_DB, 0x00940718 /* "AIVehicle" */); *parts = rx_tbl_find(VS_DB, 0x00940724 /* "AIVehicleParts" */); break;
    case 5: *vehicles = 0x00ae0898; *parts = 0x00ae0550; break;
    case 6: *model_col = 0; *vehicles = rx_tbl_find(VS_DB, 0x0093e180 /* "VehicleModel" */); *parts = 0; break;
    case 7: *vehicles = 0x00ae0640; *parts = 0x00ae06b8; break;
    case 8: *vehicles = rx_tbl_find(VS_DB, 0x00940734 /* "PresetVehicle" */); *parts = rx_tbl_find(VS_DB, 0x00940744 /* "PresetVehicleParts" */); break;
    }
}

/* Full00541040, career needs model identity before vehicle specs are made.
 * One canonical owner_tables kernel remains shared with spec construction. */
uint32_t rx_career_vehicle_model_id(rx_addr list,uint32_t vehicle_id) {
    for (int32_t i=0;i<I32(list,0x2c);++i) {
        const rx_addr spec=U32(U32(list,0x28),4u*(uint32_t)i);
        if(spec && U32(spec,4)==vehicle_id) return U32(U32(spec,8),0);
    }
    const rx_addr reference=rx_tbl_row(column_of(0x00ae05c8,0x00936cb4),(int32_t)vehicle_id);
    if(!U32(reference,8)||U32(reference,0)==UINT32_MAX) return UINT32_MAX;
    const uint32_t owner=ref_int(reference,0x0093df84);
    const uint32_t owner_vehicle=ref_int(reference,0x0093df94);
    rx_addr vehicles=0,parts=0;uint32_t model_column=1;
    if(owner<=8) owner_tables(owner,&vehicles,&parts,&model_column);
    (void)parts;
    /*No default owner table: invalid owner follows the original fatal index path.*/
    if(((U32(vehicles,0x14)-U32(vehicles,0x10))>>2)==0) invalid_parameter();
    const rx_addr found=rx_tbl_row(U32(U32(vehicles,0x10),0),(int32_t)owner_vehicle);
    return U32(rx_tbl_cell_word(U32(found,8),model_column,U32(found,0)),0);
}
RX_FUNCTION(0x00541040,rx_career_vehicle_model_id)

/* the part record released (00540c52 .. / 00540cb0 ..: the list entry removed, 0053f9f0 / 00540300 the packages, freed): not reached
 * by the reference boot */
static void part_release(rx_addr L, rx_addr P) {
    (void)L;
    SU32(P, 0x44, U32(P, 0x44) - 1);
    if (U32(P, 0x44) == 0) vs_fatal("a spec's part record released to 0 (00540c5e / 00540cb6: 0053f9f0 / 00540300) not ported", P);
}
/* 005409d0 (s0 = the list, s1 = VehicleId, s2 = the owner, s3 = the owner's vehicle id; RET 0x10): the spec (its model from the owner's
 * vehicle table, its parts from the owner's part rows, else the model's defaults), listed in L + 0x28 */
static rx_addr spec_create(rx_addr L, uint32_t vid, uint32_t owner, uint32_t ovid) {
    rx_tbl_find(VS_DB, 0x0093e180);   /* "VehicleModel" (unused) */
    rx_addr vehicles = 0, parts = 0;
    uint32_t model_col = 1;
    if (owner > 8) vs_fatal("a vehicle owner above 8", owner);
    owner_tables(owner, &vehicles, &parts, &model_col);
    if (!((U32(vehicles, 0x14) - U32(vehicles, 0x10)) >> 2)) invalid_parameter();
    rx_addr ref = rx_tbl_row(U32(U32(vehicles, 0x10), 0), (int32_t)ovid);   /* the first column's unique index */
    rx_addr S = rx_reflex_aligned_alloc(200);
    S = S ? spec_init(S) : 0;
    rx_addr M = model_get(L, U32(cell(U32(ref, 8), model_col, U32(ref, 0)), 0));
    SU32(S, 4, vid); SU32(S, 8, M); SU32(S, 0x98, owner); SU32(S, 0x9c, ovid);
    rx_vehicle_spec_apply_tuning(S);
    if (!parts) {
        for (int32_t i = 0; i < I32(U32(S, 8), 0x54); i++) {
            rx_addr P = part_get(L, U32(U32(U32(S, 8), 0x50), 4 * (uint32_t)i));
            SU32(S, 0xc + 4 * U32(P, 0x3c), P);
        }
    } else {
        if (!((U32(parts, 0x14) - U32(parts, 0x10)) >> 2)) invalid_parameter();
        rx_addr set = rx_tbl_rows(U32(U32(parts, 0x10), 0), (int32_t)ovid);   /* the owner's part rows */
        for (rx_addr n = U32(U32(set, 0x18), 0); n != U32(set, 0x18);) {
            uint32_t pid = U32(cell(parts, 1, U32(n, 0xc)), 0);
            M = U32(S, 8);
            int done = 0;
            for (int32_t j = 0; j < I32(M, 0x40) && !done; j++) {   /* one of the model's parts */
                if (U32(U32(M, 0x3c), 4 * (uint32_t)j) != pid) continue;
                rx_addr P = part_get(L, pid);
                if (!U32(S, 0xc + 4 * U32(P, 0x3c))) { SU32(S, 0xc + 4 * U32(P, 0x3c), P); done = 1; }
                else part_release(L, P);
                M = U32(S, 8);
            }
            if (!done) {   /* else the model's default part of the same slot (00540b5e) */
                rx_addr vp = rx_tbl_find(VS_DB, 0x0093e69c /* "VehiclePart" */);
                rx_addr pr = rx_tbl_row(column_of(vp, 0x0093e29c), (int32_t)pid);
                uint32_t slot = part_slot_of(U32(cell(U32(pr, 8), rx_tbl_column_index(U32(pr, 8), 0x0093e508), U32(pr, 0)), 0));
                for (int32_t i = 0; i < I32(U32(S, 8), 0x54); i++) {
                    rx_addr P = part_get(L, U32(U32(U32(S, 8), 0x50), 4 * (uint32_t)i));
                    if (!U32(S, 0xc + 4 * U32(P, 0x3c)) && U32(P, 0x3c) == slot) { SU32(S, 0xc + 4 * slot, P); break; }
                    part_release(L, P);
                }
            }
            /* ++it (00417df0) */
            if (!U8(U32(n, 8), 0x19)) { n = U32(n, 8); while (!U8(U32(n, 0), 0x19)) n = U32(n, 0); }
            else { rx_addr p = U32(n, 4); while (!U8(p, 0x19) && n == U32(p, 8)) { n = p; p = U32(p, 4); } n = p; }
        }
    }
    RX_FRAME_ENTER(F, 4);
    SU32(F, 0, S);
    rx_world_ptr_vector_push_unique(L + 0x28, F);
    S = U32(F, 0);
    RX_FRAME_LEAVE(F, 4);
    return S;
}

/* 005400c0 (cdecl the factory, the slot): the two names copied into a frame buffer (debug; no state) */
void rx_names_to_frame(uint32_t factory, uint32_t slot) {
    RX_FRAME_ENTER(F, 0x84);
    crt_strncpy(F, U32(0, 0x009ce220 + 4 * factory), 0x3f);
    SU8(F, 0x3f, 0);
    crt_strncpy(F + 0x40, U32(0, 0x009ce240 + 4 * slot), 0x3f);
    RX_FRAME_LEAVE(F, 0x84);
}
/* 005403b0 (ECX = the part; s0 = the loader, s1 = high resolution; RET 8): the part's two packages requested (async; "HR_" names
 * when the spec is high resolution and the part has them) */
void rx_part_request_packages(rx_addr P, rx_addr loader, uint32_t hr) {
    for (uint32_t k = 0; k < 2; k++) {
        rx_addr s = P + 0x1c * k;
        if ((int32_t)U32(s, 0x14) <= 0) continue;
        rx_addr name = chars(s);
        RX_FRAME_ENTER(F, 0x40);
        if (hr && U32(P, 0x38)) { rx_gear_hr_name(F, name); name = F; }
        rx_pkgcache_request(loader + 4, name, 1);
        RX_FRAME_LEAVE(F, 0x40);
    }
}
RX_FUNCTION(0x005403b0, rx_part_request_packages);
/* 005401b0 (s0 = the loader, s1 = the spec; RET 8): every part's packages requested */
void rx_spec_request_packages(rx_addr loader, rx_addr S) {
    for (uint32_t k = 0; k < 0x11; k++) {
        rx_addr P = U32(S, 0xc + 4 * k);
        if (!P) continue;
        rx_names_to_frame(U32(U32(S, 8), 0xc), k);
        rx_part_request_packages(P, loader, U32(S, 0xc0));
        rx_names_to_frame(U32(U32(S, 8), 0xc), k);
    }
}
RX_FUNCTION(0x005401b0, rx_spec_request_packages);

/* 00540950 (EAX = the Vehicle List row reference; s0 = the list; RET 4) */
static rx_addr spec_of_row(rx_addr ref, rx_addr L) {
    uint32_t vid = ref_int(ref, 0x00936cb4 /* VehicleId */);
    uint32_t owner = ref_int(ref, 0x0093df84 /* VehicleOwner */);
    uint32_t ovid = ref_int(ref, 0x0093df94 /* OwnerVehicleId */);
    return spec_create(L, vid, owner, ovid);
}
/* 00542b20 VehicleList_CreateSpec (s0 = the list, s1 = VehicleId, s2 = high resolution; RET 0xc): the spec of the Vehicle List row,
 * its packages requested */
rx_addr rx_vehicle_list_create_spec(rx_addr L, uint32_t vehicle_id, uint32_t hr) {
    rx_addr ref = rx_tbl_row(column_of(0x00ae05c8, 0x00936cb4), (int32_t)vehicle_id);
    rx_addr S = spec_of_row(ref, L);
    SU32(S, 0xc0, hr);
    rx_spec_request_packages(U32(0, VS_LOADER), S);
    return S;
}
RX_FUNCTION(0x00542b20, rx_vehicle_list_create_spec);

/* 005404f0 (ESI = the vehicle list, MXGame + 0x154b0): the four arrays (part records {cap 0x80, growth 0x20}, model records {0x14,
 * 5}, specs {0x18, 0, max 0x18}, made vehicles {0xc, 0, max 0xc}), the hierarchies table 0 */
rx_addr rx_vehicle_list_construct(rx_addr L) {
    SU32(L, 0, rx_reflex_aligned_alloc(0x200));
    SU32(L, 4, 0); SU32(L, 8, 0x80); SU32(L, 0xc, 0x20); SU32(L, 0x10, 0x7fffffff);
    rx_addr d = rx_reflex_aligned_alloc(0x50);
    SU32(L, 0x24, 0x7fffffff); SU32(L, 0x14, d); SU32(L, 0x18, 0); SU32(L, 0x1c, 0x14); SU32(L, 0x20, 5);
    d = rx_reflex_aligned_alloc(0x60);
    SU32(L, 0x30, 0x18); SU32(L, 0x38, 0x18); SU32(L, 0x28, d); SU32(L, 0x2c, 0); SU32(L, 0x34, 0);
    d = rx_reflex_aligned_alloc(0x30);
    SU32(L, 0x40, d); SU32(L, 0x44, 0); SU32(L, 0x48, 0xc); SU32(L, 0x4c, 0); SU32(L, 0x50, 0xc);
    SU32(L, 0x3c, 0);
    return L;
}
RX_FUNCTION(0x005404f0, rx_vehicle_list_construct);

/* the registry: the transliterations above that are whole original functions (their conventions in the comments) */
RX_FUNCTION(0x0053fb80, model_fill);
RX_FUNCTION(0x0053faf0, model_construct);
RX_FUNCTION(0x0053f7d0, part_fill);
RX_FUNCTION(0x0053ff30, spec_init);
RX_FUNCTION(0x005400c0, rx_names_to_frame);
RX_FUNCTION(0x00540950, spec_of_row);
RX_FUNCTION(0x005409d0, spec_create);
RX_FUNCTION(0x005407d0, model_get);
RX_FUNCTION(0x00540660, part_get);
RX_FUNCTION(0x00540210, part_add_packages);
RX_FUNCTION(0x004e7040, vehicle_type_factory);
RX_FUNCTION(0x004e7160, engine_class_of);
RX_FUNCTION(0x004e7210, engine_size_of);
RX_FUNCTION(0x004e72c0, part_slot_of);
