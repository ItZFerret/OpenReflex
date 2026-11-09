/* build_rider_entry.c -- construction unit 7 (riders, core/notes/construction.md section 13): the rider entries and their gear
 * packages, the requesting code of the rider databases. FUN_0053cbd0 (the FactoryGearLoader: the "Rider" database and package, the
 * MXRider_Hierarchy model), FUN_0053e8c0 (a Rider List row's rider: 0053d680 / 0053d4c0 the entry, its parts from the gear tables
 * (0053d720 which tables, 0053d350 / 0053c830 a part, 004e74c0 its slot) and their package requests (0053cef0 / 0053d0d0 /
 * VehicleLoader_RequestPartPackages 0053cf30), FUN_0053efa0 (a gear change: the pending part) and the part release (0053d480,
 * 0053ca30, 0053d170, 0053d020). Hand transliterations of the disassembly.
 *
 * The package cache of the gear loader (0x00d92ef4 + 8) is unit 6's (build_vehicle.h): called directly. The Rdb readers are unit 6's / 12a's (build_vehicle.h, build_tables.h).
 *
 * The rider manager M (MXGame + 0x15504): +0 the parts (Rdb pointer array {data, count, capacity, growth}), +0x14 the entries (the
 * same), +0x28 the spawned riders' slots. A rider entry E (0x5c, 0053cb30): +4 rider id, +8 owner (0..3 a player's garage, 4 AI,
 * 5 / 6 static tables), +0xc owner's rider id, +0x10 gear set id, +0x14 high resolution, +0x18 the rider, +0x1c + 4k the current
 * parts (k = the slot of 004e74c0), +0x3c + 4k the pending ones. A part P (0x4c, 0053c830): +0 / +0x1c std::string the material /
 * model package, +0x38 HRAvailable, +0x3c slot, +0x40 RiderGearId, +0x44 InvertTextColors, +0x48 references. */
#include "build_rider.h"
#include "build_rider_util.h"
#include "build_vehicle.h"
#include "build_tables.h"
#include "world_misc.h"
#include "crt.h"

#define RR_HR_FORMAT 0x00940560u   /* "HR_%s" */
#define RR_EMPTY_STR 0x00935d6cu   /* "" */

static rx_addr str_chars(rx_addr s) { return U32(s, 0x18) >= 0x10 ? U32(s, 4) : s + 4; }
static void invalid_parameter(void) { rr_fatal("_invalid_parameter_noinfo (an index check failed)", 0); }
/* the column object of a name (Table_ColumnIndex, the index checked as the original's vector access) */
static rx_addr column_of(rx_addr table, rx_addr name) {
    uint32_t i = rx_tbl_column_index(table, name);
    if (i >= (U32(table, 0x14) - U32(table, 0x10)) >> 2) invalid_parameter();
    return U32(U32(table, 0x10), 4 * i);
}
/* the int cell (column `name`) of a row reference {+0 row, +8 store} */
static uint32_t ref_int(rx_addr ref, rx_addr name) {
    rx_addr store = U32(ref, 8);
    return U32(rx_tbl_cell_word(store, rx_tbl_column_index(store, name), U32(ref, 0)), 0);
}
/* the row reference of a key in a column's index map (00418ec0; the key passed by address) */
static rx_addr row_of(rx_addr column, uint32_t key) { return rx_tbl_row(column, (int32_t)key); }

/* ---- unit 6's package cache and Rdb reference reader, through the registry ---- */
static void cache_add(rx_addr pkg, rx_addr db) {   /* 00543830 (EDI = cache, s0 package, s1 database) */
    rx_pkgcache_add(U32(0, RR_GEARLOADER) + 8, pkg, db);
}
static void cache_request(rx_addr pkg, uint32_t flag) {   /* 00543b30 (EAX = cache, s0 package, s1 flag; RET 8) */
    rx_pkgcache_request(U32(0, RR_GEARLOADER) + 8, pkg, (int)flag);
}
static void cache_name_op(rx_addr fn, rx_addr name) {   /* 00543c30 / 00543990 (ECX = cache, EAX = package name) */
    rx_addr cache = U32(0, RR_GEARLOADER) + 8;
    if (fn == 0x00543c30) rx_pkgcache_release(cache, name); else rx_pkgcache_unmark(cache, name);
}
static void cache_construct(rx_addr cache) {   /* 005436d0 (ESI = cache) */
    rx_pkgcache_construct(cache);
}
static rx_addr ref_value(rx_addr ref, rx_addr name) {   /* 004ec210 (EAX = row reference, s0 column name; RET 4) -> &value */
    return rx_tbl_ref_value(ref, name);
}

/* 0053cbd0 (EDI = loader): the FactoryGearLoader {vtable 0x00940798, +4 the MXRider_Hierarchy model handle, +8 the package cache} */
rx_addr rx_gear_loader_construct(rx_addr gl) {
    SU32(gl, 0, 0x00940798u);
    cache_construct(gl + 8);
    uint32_t a4[4] = {0x0093e434u, 0x009404d0u, 1, 0};   /* "Database\", "Rider" */
    rr_vcall(rr_database(), 0x10, 4, a4);                 /* vf4: the database */
    uint32_t a3[3] = {0x009404d0u, 0x009404d0u, 0};
    rr_vcall(rr_database(), 0x3c, 3, a3);                 /* vf15: its package */
    uint32_t a5[4] = {0x00940484u, 0x65646f6du, 0x6c, 1};   /* "MXRider_Hierarchy", "model" */
    SU32(gl, 4, rr_vcall(rr_database(), 0x6c, 4, a5));
    return gl;
}

/* 004dcf30 (cdecl name): the gear slot of a factory name (_stricmp against the 8 names at 0x009ccec8), 8 when none */
uint32_t rx_gear_slot_of(rx_addr name) {
    for (uint32_t i = 0; i < 8; i++) if (crt_stricmp(name, U32(0, 0x009ccec8u + 4 * i)) == 0) return i;
    return 8;
}
/* 004e74c0 (s0 = gear type id; RET 4): the slot of a RiderGearType row's FactoryName */
uint32_t rx_gear_type_slot(uint32_t type) {
    rx_addr t = rx_tbl_find(0x00ae0910u, 0x0093de18u);   /* "RiderGearType" */
    rx_addr ref = row_of(column_of(t, 0x0093e51cu), type);   /* "RiderGearTypeId" */
    rx_addr store = U32(ref, 8);
    rx_addr col = column_of(store, 0x0093e4a8u);         /* "FactoryName" */
    return rx_gear_slot_of(str_chars(rx_tbl_cell_string(col, U32(ref, 0))));
}

/* 0053cf30 VehicleLoader_RequestPartPackages (ECX = &database names[2], EDX = part, s0 = loader; RET 4): each of the part's two
 * packages added to the cache with its database (and the "HR_" twins when the part has them) */
static void part_add_packages(rx_addr dbnames, rx_addr p) {
    const rx_addr sp0 = rx_sp();
    const rx_addr F = ((sp0 - 4u) & ~7u) - 0xa8u;   /* push ebp; and esp, -8; sub esp, 0x9c; three pushes */
    rx_set_sp(F);
    for (uint32_t k = 0; k < 2; k++) {
        rx_addr s = p + 0x1c * k;
        if ((int32_t)U32(s, 0x14) <= 0) continue;
        rx_addr name = str_chars(s);
        rx_addr db = U32(dbnames, 4 * k);
        if (db) cache_add(name, db);
        if (!U32(p, 0x38)) continue;
        rx_gear_hr_name(F + 0x20, db);
        rx_gear_hr_name(F + 0x60, name);
        cache_add(F + 0x60, F + 0x20);
    }
    rx_set_sp(sp0);
}
/* 0053c830 (EAX = RiderGear row reference; s0 = part; RET 4): a part from its row (an absent row: empty names, slot 8, id -1) */
static void part_init(rx_addr ref, rx_addr p) {
    rx_addr store = U32(ref, 8);
    if (!store || U32(ref, 0) == 0xffffffffu) {
        crt_str_operator_assign(p + 0x1c, RR_EMPTY_STR);
        crt_str_operator_assign(p, RR_EMPTY_STR);
        SU32(p, 0x38, 0); SU32(p, 0x3c, 8); SU32(p, 0x40, 0xffffffffu); SU32(p, 0x44, 0); SU32(p, 0x48, 0);
        return;
    }
    SU32(p, 0x40, ref_int(ref, 0x0093e27cu));   /* "RiderGearId" */
    SU32(p, 0x44, ref_int(ref, 0x0094050cu));   /* "InvertTextColors" */
    SU32(p, 0x38, ref_int(ref, 0x00940520u));   /* "HRAvailable" */
    uint32_t type = ref_int(ref, 0x0093e51cu);  /* "RiderGearTypeId" */
    SU32(p, 0x3c, rx_gear_type_slot(type));
    SU32(p, 0x48, 0);
    RX_FRAME_ENTER(F, 8);
    SU32(F, 0, 0); SU32(F, 4, 0);   /* the two database names */
    static const rx_addr cols[2] = {0x0094052cu, 0x00940548u};   /* "MaterialPackageDatabaseId", "ModelPackageDatabaseId" */
    for (uint32_t k = 0; k < 2; k++) {
        rx_addr r = ref_value(ref, cols[k]);
        if (!U32(r, 8) || U32(r, 0) == 0xffffffffu) continue;
        rx_addr pkg = str_chars(rx_tbl_ref_string(ref_value(r, 0x00936fc0u), 0x00936fccu));   /* PackageId -> PackageName */
        rx_addr db = str_chars(rx_tbl_ref_string(ref_value(r, 0x00936fd8u), 0x00936fe4u));    /* DatabaseId -> DatabaseName */
        crt_str_operator_assign(p + 0x1c * k, pkg);
        SU32(F, 4 * k, db);
    }
    part_add_packages(F, p);
    RX_FRAME_LEAVE(F, 8);
}

/* 0053d350 (ESI = manager; s0 = RiderGearId; RET 4): the part of a gear id (shared: references + 1), made from its RiderGear row the
 * first time; 0 when the table has no such row */
static rx_addr part_get(rx_addr mgr, uint32_t id) {
    for (int32_t i = 0; i < (int32_t)U32(mgr, 4); i++) {
        rx_addr p = U32(U32(mgr, 0), 4 * (uint32_t)i);
        if (U32(p, 0x40) == id) { SU32(p, 0x48, U32(p, 0x48) + 1); return p; }
    }
    rx_addr t = rx_tbl_find(0x00ae0910u, 0x0093e7fcu);   /* "RiderGear" */
    rx_addr ref = row_of(column_of(t, 0x0093e27cu), id);
    if (!U32(ref, 8) || U32(ref, 0) == 0xffffffffu) return 0;
    rx_addr p = rr_alloc(0x4c);
    if (p) {
        crt_str_ctor(p); crt_str_ctor(p + 0x1c);
        part_init(ref, p);
    }
    SU32(p, 0x48, U32(p, 0x48) + 1);
    if ((int32_t)U32(mgr, 4) >= (int32_t)U32(mgr, 8)) {
        uint32_t cap = U32(mgr, 0xc) + U32(mgr, 8);
        rx_addr d = rr_alloc(rx_mul_sat(cap, 4));
        crt_memcpy(d, U32(mgr, 0), U32(mgr, 8) * 4);
        rx_reflex_aligned_free(U32(mgr, 0));
        SU32(mgr, 0, d); SU32(mgr, 8, cap);
    }
    SU32(U32(mgr, 0), 4 * U32(mgr, 4), p);
    SU32(mgr, 4, U32(mgr, 4) + 1);
    return p;
}

/* 0053d720 (EAX = owner; ESI = &gear table, EDI = &rider table; stdcall &rider column, &gear set column, &gear column; RET 0xc): the
 * tables of an owner: a player's PlayerRider / PlayerRiderGear (columns -1 -1 0), AI: AIRider / RiderGearSetGear (0 0 1), 5 / 6 the
 * static rider tables */
static void owner_tables(uint32_t owner, rx_addr* rider_t, rx_addr* gear_t, int32_t* rider_col, int32_t* set_col, int32_t* gear_col) {
    *rider_t = 0; *gear_t = 0; *rider_col = 0; *set_col = 0; *gear_col = 1;
    if (owner <= 3) {
        rx_addr db = 0x00ae10f0u + 0x1f4u * owner;
        *rider_t = rx_tbl_find(db, 0x0093e53cu);   /* "PlayerRider" */
        *gear_t = rx_tbl_find(db, 0x0094062cu);    /* "PlayerRiderGear" */
        *rider_col = -1; *set_col = -1; *gear_col = 0;
    } else if (owner == 4) {
        *rider_t = rx_tbl_find(0x00ae0910u, 0x0094063cu);   /* "AIRider" */
        *gear_t = rx_tbl_find(0x00ae0910u, 0x00940644u);    /* "RiderGearSetGear" */
    } else if (owner == 5) { *rider_t = 0x00ae0f10u; *gear_t = 0x00ae1a28u; }
    else if (owner == 6) { *rider_t = 0x00ae07a8u; *gear_t = 0x00ae0820u; }
}
/* 0053f3f0 (stdcall name buffer, number buffer, &found, rider id; RET 0x10): the display name and number of a Rider List row's
 * rider, the text of the number / name plates. The row (RiderId = id) names the owner and the owner's rider id (RiderOwner,
 * OwnerRiderId); 0053d720 gives the owner's rider table. An AI rider (a column: AIRider's AIRiderId) is the row of its id, found = 1;
 * a player's PlayerRider has the one row 0, found = NameStatus == 1. Name (15 characters, strncpy) and Number (3) of that row.
 * Hand transliteration of the disassembly (the product had no plate text before this: P+0x200 / P+0x210 stayed zero). Not a
 * registered function: the oracle's caller (build_activity_spawn.c player_rider_name) still runs the original. */
void rx_rider_name_number(rx_addr name_buf, rx_addr number_buf, rx_addr found, uint32_t rider_id) {
    const rx_addr rider_list = 0x00ae19b0u;   /* the static "Rider List" table */
    rx_addr ref = row_of(column_of(rider_list, 0x0093dec8u /* "RiderId" */), rider_id);
    const uint32_t owner = ref_int(ref, 0x0093dfa4u);   /* "RiderOwner" */
    const uint32_t owner_rider = ref_int(ref, 0x0093dfb0u);   /* "OwnerRiderId" */
    rx_addr rider_t, gear_t; int32_t rider_col, set_col, gear_col;
    owner_tables(owner, &rider_t, &gear_t, &rider_col, &set_col, &gear_col);
    uint32_t row = 0;
    if (rider_col >= 0) {   /* AI: the row of the owner's rider id */
        if ((uint32_t)rider_col >= (U32(rider_t, 0x14) - U32(rider_t, 0x10)) >> 2) invalid_parameter();
        row = U32(row_of(U32(U32(rider_t, 0x10), 4 * (uint32_t)rider_col), owner_rider), 0);
        SU32(found, 0, 1);
    } else {                /* a player: row 0, whether its name was set */
        uint32_t ci = rx_tbl_column_index(rider_t, 0x00940658u /* "NameStatus" */);
        SU32(found, 0, U32(rx_tbl_cell_word(rider_t, ci, 0), 0) == 1 ? 1u : 0u);
    }
    static const struct { rx_addr column; uint32_t n; } text[2] = { { 0x0093a3bcu /* "Name" */, 0xf }, { 0x0093e26cu /* "Number" */, 3 } };
    const rx_addr dst[2] = { name_buf, number_buf };
    for (int k = 0; k < 2; k++) {
        rx_addr src = str_chars(rx_tbl_cell_string(column_of(rider_t, text[k].column), row));
        uint32_t i = 0; int end = 0;
        for (; i < text[k].n; i++) {   /* strncpy: stops at the terminator and pads with zeros */
            uint8_t c = end ? 0 : U8(src, i);
            if (c == 0) end = 1;
            SU8(dst[k], i, c);
        }
        SU8(dst[k], text[k].n, 0);
    }
}

/* 0053cb30 (EAX = memory): an empty entry */
static rx_addr entry_construct(rx_addr e) {
    SU32(e, 0x18, 0); SU32(e, 4, 0); SU32(e, 8, 7); SU32(e, 0xc, 0); SU32(e, 0x10, 0); SU32(e, 0x14, 0); SU32(e, 0, 0);
    rr_zero(e + 0x1c, 0x40);
    return e;
}
/* 00417df0 (ESI = iterator {owner, node}): std::set<row> iterator ++ (nodes: +0 left, +4 parent, +8 right, +0x19 isnil) */
static rx_addr set_next(rx_addr n) {
    if (U8(n, 0x19)) { invalid_parameter(); return n; }
    rx_addr r = U32(n, 8);
    if (!U8(r, 0x19)) { while (!U8(U32(r, 0), 0x19)) r = U32(r, 0); return r; }
    rx_addr p = U32(n, 4);
    while (!U8(p, 0x19) && n == U32(p, 8)) { n = p; p = U32(n, 4); }
    return p;
}
static void entry_put_part(rx_addr e, rx_addr p) { if (p) SU32(e, 0x1c + 4 * U32(p, 0x3c), p); }
/* 0053d4c0 (stdcall manager, rider id, owner, owner's rider id, gear set; RET 0x14): a new entry with its parts, added to the manager */
static rx_addr entry_create(rx_addr mgr, uint32_t id, uint32_t owner, uint32_t owner_id, uint32_t set) {
    rx_addr rider_t, gear_t; int32_t rider_col, set_col, gear_col;
    owner_tables(owner, &rider_t, &gear_t, &rider_col, &set_col, &gear_col);
    rx_addr mem = rr_alloc(0x5c);
    rx_addr e = mem ? entry_construct(mem) : 0;
    SU32(e, 4, id); SU32(e, 8, owner); SU32(e, 0xc, owner_id); SU32(e, 0x10, set);
    if (rider_col >= 0) {   /* AI: the gear set's rows of the gear table (the rider row is looked up and not used) */
        if ((uint32_t)rider_col >= (U32(rider_t, 0x14) - U32(rider_t, 0x10)) >> 2) invalid_parameter();
        row_of(U32(U32(rider_t, 0x10), 4 * (uint32_t)rider_col), owner_id);
        if ((uint32_t)set_col >= (U32(gear_t, 0x14) - U32(gear_t, 0x10)) >> 2) invalid_parameter();
        rx_addr s = rx_tbl_rows(U32(U32(gear_t, 0x10), 4 * (uint32_t)set_col), (int32_t)set);
        rx_addr head = U32(s, 0x18);
        for (rx_addr n = U32(head, 0); n != head; n = set_next(n)) {
            uint32_t gid = U32(rx_tbl_cell_word(gear_t, (uint32_t)gear_col, U32(n, 0xc)), 0);
            entry_put_part(e, part_get(mgr, gid));
        }
    } else {                /* a player: every row of the gear table */
        uint32_t n = rx_tbl_row_count(gear_t);
        for (uint32_t r = 0; r < n; r++) {
            uint32_t gid = U32(rx_tbl_cell_word(gear_t, (uint32_t)gear_col, r), 0);
            entry_put_part(e, part_get(mgr, gid));
        }
    }
    RX_FRAME_ENTER(F, 4);
    SU32(F, 0, e);
    rx_world_ptr_vector_push_unique(mgr + 0x14, F);
    RX_FRAME_LEAVE(F, 4);
    return e;
}
/* 0053d680 (EAX = Rider List row reference; s0 = manager; RET 4): the entry of a Rider List row (RiderId, RiderOwner, OwnerRiderId,
 * OwnerRiderGearSetId) */
static rx_addr entry_of_row(rx_addr ref, rx_addr mgr) {
    uint32_t id = ref_int(ref, 0x0093dec8u);      /* "RiderId" */
    uint32_t owner = ref_int(ref, 0x0093dfa4u);   /* "RiderOwner" */
    uint32_t oid = ref_int(ref, 0x0093dfb0u);     /* "OwnerRiderId" */
    uint32_t set = ref_int(ref, 0x0093dfc0u);     /* "OwnerRiderGearSetId" */
    return entry_create(mgr, id, owner, oid, set);
}

/* 0053d0d0 (ECX = part; stdcall loader, high resolution; RET 8): the part's packages requested from the cache (00543b30, flag 1) */
static void part_request(rx_addr p, uint32_t hr) {
    const rx_addr sp0 = rx_sp();
    const rx_addr F = ((sp0 - 4u) & ~7u) - 0x60u;   /* push ebp; and esp, -8; sub esp, 0x54; three pushes */
    rx_set_sp(F);
    for (uint32_t k = 0; k < 2; k++) {
        rx_addr s = p + 0x1c * k;
        if ((int32_t)U32(s, 0x14) <= 0) continue;
        rx_addr name = str_chars(s);
        if (hr && U32(p, 0x38)) { rx_gear_hr_name(F + 0x18, name); name = F + 0x18; }
        cache_request(name, 1);
    }
    rx_set_sp(sp0);
}
/* 0053d170 (ECX = part; stdcall loader, high resolution; RET 8): the part's packages released (00543c30) */
static void part_release_packages(rx_addr p, uint32_t hr) {
    const rx_addr sp0 = rx_sp();
    const rx_addr F = ((sp0 - 4u) & ~7u) - 0x60u;
    rx_set_sp(F);
    for (uint32_t k = 0; k < 2; k++) {
        rx_addr s = p + 0x1c * k;
        if ((int32_t)U32(s, 0x14) <= 0) continue;
        rx_addr name = str_chars(s);
        if (hr && U32(p, 0x38)) { rx_gear_hr_name(F + 0x18, name); name = F + 0x18; }
        cache_name_op(0x00543c30, name);
    }
    rx_set_sp(sp0);
}
/* 0053d020 (ECX = part; s0 loader; RET 4): the part's packages dropped from the cache (00543990), the "HR_" twins too */
static void part_drop_packages(rx_addr p) {
    const rx_addr sp0 = rx_sp();
    const rx_addr F = ((sp0 - 4u) & ~7u) - 0x68u;   /* sub esp, 0x5c; three pushes */
    rx_set_sp(F);
    for (uint32_t k = 0; k < 2; k++) {
        rx_addr s = p + 0x1c * k;
        if ((int32_t)U32(s, 0x14) <= 0) continue;
        rx_addr name = str_chars(s);
        cache_name_op(0x00543990, name);
        if (!U32(p, 0x38)) continue;
        rx_gear_hr_name(F + 0x20, name);
        cache_name_op(0x00543990, F + 0x20);
    }
    rx_set_sp(sp0);
}
/* 0053ca30 (EAX = part): its slot's packages dropped (0053d020) unless its id is -1, the two strings destroyed */
void rx_rider_part_destroy(rx_addr p) {
    if (U32(p, 0x40) != 0xffffffffu) part_drop_packages(p);
    crt_str_dtor(p + 0x1c);
    crt_str_dtor(p);
}
/* 00544c80 (ECX = pointer array {data, count}, EDX = &value): removed (the last one moved into its place) */
void rx_rider_ptr_array_remove(rx_addr a, rx_addr value) {
    int32_t n = (int32_t)U32(a, 4);
    for (int32_t i = 0; i < n; i++) {
        if (U32(U32(a, 0), 4 * (uint32_t)i) != U32(value, 0)) continue;
        SU32(U32(a, 0), 4 * (uint32_t)i, U32(U32(a, 0), 4 * (uint32_t)n - 4));
        SU32(a, 4, U32(a, 4) - 1);
        return;
    }
}
/* 0053d480 (ECX = manager; s0 = part; RET 4): one reference dropped; at 0 the part leaves the manager and is destroyed */
void rx_rider_part_release(rx_addr mgr, rx_addr p) {
    if (!p) return;
    SU32(p, 0x48, U32(p, 0x48) - 1);
    if (U32(p, 0x48)) return;
    RX_FRAME_ENTER(F, 4);
    SU32(F, 0, p);
    rx_rider_ptr_array_remove(mgr, F);
    RX_FRAME_LEAVE(F, 4);
    rx_rider_part_destroy(p);
    rx_reflex_aligned_free(p);
}
/* the release of a pending part's packages (0053d170), for build_rider_gear.c */
void rx_rider_part_release_packages(rx_addr p, uint32_t hr) { part_release_packages(p, hr); }

static uint32_t cache_has(rx_addr name) {   /* 00543cd0 (EAX = cache, s0 package name; RET 4): the package is loaded */
    return rx_pkgcache_loaded(U32(0, RR_GEARLOADER) + 8, name);
}
/* 0053ca70 VehicleLoader_PartPackagesReady (ECX = part; s0 high resolution; RET 4): 1 when both packages (the "HR_" ones for a
 * high-resolution entry and part) are loaded */
uint32_t rx_rider_part_packages_ready(rx_addr p, uint32_t hr) {
    const rx_addr sp0 = rx_sp();
    const rx_addr F = sp0 - 0x60u;   /* sub esp, 0x50; four pushes */
    rx_set_sp(F);
    uint32_t ok = 1;
    for (uint32_t k = 0; k < 2 && ok; k++) {
        rx_addr s = p + 0x1c * k;
        if (!U32(s, 0x14)) continue;
        rx_addr name = str_chars(s);
        if (hr && U32(p, 0x38)) { rx_gear_hr_name(F + 0x18, name); name = F + 0x18; }
        if (!name || !U8(name, 0) || !cache_has(name)) ok = 0;
    }
    rx_set_sp(sp0);
    return ok;
}

/* 0053cef0 (stdcall loader, entry; RET 8): every current part's packages requested */
static void entry_request(rx_addr e) {
    for (uint32_t k = 0; k < 8; k++) if (U32(e, 0x1c + 4 * k)) part_request(U32(e, 0x1c + 4 * k), U32(e, 0x14));
}

/* 0053e8c0 (stdcall manager, rider id, high resolution; RET 0xc): the entry of the Rider List row of a rider id, its packages
 * requested */
void rx_rider_entry_create(rx_addr mgr, uint32_t id, uint32_t hr) {
    rx_addr col = column_of(0x00ae19b0u, 0x0093dec8u);   /* Rider List "RiderId" */
    rx_addr e = entry_of_row(row_of(col, id), mgr);
    SU32(e, 0x14, hr);
    entry_request(e);
}
RX_FUNCTION(0x0053e8c0, rx_rider_entry_create)

/* 0053efa0 (ECX = &out old id (or 0), ESI = entry; stdcall manager, part; RET 8): a gear change: the part becomes the pending one of
 * its slot (its packages requested); 0 when it already is the pending or (with none pending) the current one */
uint32_t rx_rider_entry_set_pending(rx_addr out, rx_addr e, rx_addr mgr, rx_addr p) {
    uint32_t slot = U32(p, 0x3c);
    if (out) {
        if (U32(e, 0x3c + 4 * slot)) SU32(out, 0, U32(U32(e, 0x3c + 4 * slot), 0x40));
        else if (U32(e, 0x1c + 4 * slot)) SU32(out, 0, U32(U32(e, 0x1c + 4 * slot), 0x40));
    }
    rx_addr pend = U32(e, 0x3c + 4 * slot);
    if (pend == p) return 0;
    if (pend) {
        part_release_packages(pend, U32(e, 0x14));
        rx_rider_part_release(mgr, pend);
        SU32(e, 0x3c + 4 * slot, 0);
    } else if (U32(e, 0x1c + 4 * slot) == p) return 0;
    part_request(p, U32(e, 0x14));
    SU32(e, 0x3c + 4 * slot, p);
    return 1;
}
