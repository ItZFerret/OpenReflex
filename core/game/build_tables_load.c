/* build_tables_load.c -- unit 12a: the table loaders (build_tables.h). Database_LoadTables 004ef060 with its column reader 004ef340
 * and row reader 004ef8f0, the tbldata asset (TableDataAssetIO::vf1 / vf3, FUN_004eefa0), Database_RestorePlayerDefaults 004eea10.
 * The XML reader is the game's XMLReader (build_xml.c), called through its vtable 0x009753dc like the original; the three virtuals
 * the reader unit did not port are here (vf4 00827fb0, vf5 00827f90, vf80 008290f0). Locals the original passes by address (name /
 * type buffers, cursors, values) live on the game stack. */
#include "build_tables.h"
#include "build_alloc.h"
#include "build_asset.h"
#include "build_vehicle.h"
#include "build_xml.h"
#include "crt.h"
#include <stdio.h>
#include <stdlib.h>

static void unreached(uint32_t addr, const char* what) {
    fprintf(stderr, "rxcore: build_tables: %08x %s: path not in the reference boot (not ported)\n", addr, what);
    abort();
}

/* ---- the XMLReader virtuals (this = the reader) ---- */
typedef rx_addr  (*xv_node_name)(rx_addr, rx_addr, rx_addr);              /* vf7 child, vf58 attr */
typedef rx_addr  (*xv_first)(rx_addr, rx_addr, rx_addr, rx_addr);         /* vf8 */
typedef uint32_t (*xv_one)(rx_addr, uint32_t);                            /* vf5, vf9, vf10 */
typedef uint32_t (*xv_values)(rx_addr, rx_addr, rx_addr, uint32_t);       /* vf54, vf76, vf77, vf79, vf80 */
typedef uint32_t (*xv_count)(rx_addr, rx_addr, rx_addr);                  /* vf4 */
static void* xv(rx_addr r, uint32_t off) {
    rx_addr a = U32(U32(r, 0), off);
    void* f = rx_fn(a);
    if (!f) { fprintf(stderr, "rxcore: build_tables: XMLReader virtual %08x (+%x) not ported\n", a, off); abort(); }
    return f;
}
static rx_addr  x_child(rx_addr r, rx_addr n, rx_addr name) { return ((xv_node_name)xv(r, 0x1c))(r, n, name); }
static rx_addr  x_first(rx_addr r, rx_addr n, rx_addr name, rx_addr out) { return ((xv_first)xv(r, 0x20))(r, n, name, out); }
static uint32_t x_release(rx_addr r, uint32_t c) { return ((xv_one)xv(r, 0x24))(r, c); }
static rx_addr  x_next(rx_addr r, uint32_t c) { return ((xv_one)xv(r, 0x28))(r, c); }
static uint32_t x_count(rx_addr r, rx_addr n, rx_addr name) { return ((xv_count)xv(r, 0x10))(r, n, name); }
static uint32_t x_children(rx_addr r, rx_addr n) { return ((xv_one)xv(r, 0x14))(r, n); }
static uint32_t x_node_string(rx_addr r, rx_addr n, rx_addr buf, uint32_t sz) { return ((xv_values)xv(r, 0xd8))(r, n, buf, sz); }
static rx_addr  x_attr(rx_addr r, rx_addr n, rx_addr name) { return ((xv_node_name)xv(r, 0xe8))(r, n, name); }
static uint32_t x_attr_floats(rx_addr r, rx_addr e, rx_addr d, uint32_t n) { return ((xv_values)xv(r, 0x130))(r, e, d, n); }
static uint32_t x_attr_ints(rx_addr r, rx_addr e, rx_addr d, uint32_t n) { return ((xv_values)xv(r, 0x134))(r, e, d, n); }
static uint32_t x_attr_string(rx_addr r, rx_addr e, rx_addr d, uint32_t n) { return ((xv_values)xv(r, 0x13c))(r, e, d, n); }
static uint32_t x_attr_bools(rx_addr r, rx_addr e, rx_addr d, uint32_t n) { return ((xv_values)xv(r, 0x140))(r, e, d, n); }

/* XMLReader::vf4 00827fb0 (node, name; RET 8): the number of children of that name (a cursor run over them) */
uint32_t rx_tbl_xml_vf_count(rx_addr r, rx_addr node, rx_addr name) {
    if (!node || !U32(node, 0x18)) return 0;
    if (!name) return 0;
    uint32_t n = 0;
    RX_FRAME_ENTER(F, 4);
    if (x_first(r, node, name, F)) { do n++; while (x_next(r, U32(F, 0))); }
    x_release(r, U32(F, 0));
    RX_FRAME_LEAVE(F, 4);
    return n;
}
RX_FUNCTION(0x00827fb0, rx_tbl_xml_vf_count)
/* XMLReader::vf5 00827f90 (node; RET 4): the node's child count */
uint32_t rx_tbl_xml_vf_children(rx_addr r, rx_addr node) {
    (void)r;
    if (!node) unreached(0x00827f98, "child count of no node");
    return U32(node, 0x18);
}
RX_FUNCTION(0x00827f90, rx_tbl_xml_vf_children)
/* XMLReader::vf80 008290f0 (attr, dst, n; RET 0xc): bools (type 0xb: memcpy n * 4) or a comma list (ParseCommaBoolList 008294e0) */
uint32_t rx_tbl_xml_vf_attr_bools(rx_addr r, rx_addr e, rx_addr dst, uint32_t n) {
    uint32_t t = U16(e, 0xa);
    if (t != 0xb && t != 1) return 0;
    rx_xml_attr_bools(r, e, dst, (int)n);
    return 1;
}
RX_FUNCTION(0x008290f0, rx_tbl_xml_vf_attr_bools)

/* the .rdata literals the loaders pass by address */
#define S_TABLE        0x0093dcd4u   /* "Table" */
#define S_TYPE_UC      0x0093eae0u   /* "Type" */
#define S_NAME_UC      0x0093a3bcu   /* "Name" */
#define S_COLUMNS      0x0093eae8u   /* "Columns" */
#define S_COLUMN       0x0093eaf0u   /* "Column" */
#define S_NAME         0x009362a4u   /* "name" */
#define S_TYPE         0x0093b470u   /* "type" */
#define S_SEARCHABLE   0x0093eaf8u   /* "searchable" */
#define S_UNIQUE       0x0093eb04u   /* "unique" */
#define S_PARENTTABLE  0x0093eb0cu   /* "parentTable" */
#define S_PARENTCOLUMN 0x0093eb18u   /* "parentColumn" */
#define S_DEFAULT      0x0093eb28u   /* "default" */
#define S_INT          0x0093eb30u   /* "int" */
#define S_FLOAT        0x0093eb34u   /* "float" */
#define S_STRING       0x0093eb3cu   /* "string" */
#define S_ROWS         0x0093eb44u   /* "Rows" */
#define S_ROW          0x0093eb4cu   /* "Row" */
#define S_TABLES       0x0093e424u   /* "Tables" */
#define S_EMPTY        0x00935d6cu   /* "" */

static void zero(rx_addr p, uint32_t n) { for (uint32_t i = 0; i < n; i++) SU8(p, i, 0); }
static uint32_t gstrlen(rx_addr p) { uint32_t n = 0; while (U8(p, n)) n++; return n; }

/* 004ef340 (ECX = reader; table, table node): the columns of a new table from its <Columns> (or, without one, those of the table
 * the database already has of that name). Frame: sub 0x53c + 4 pushes. */
static void load_columns(rx_addr reader, rx_addr t, rx_addr tnode) {
    RX_FRAME_ENTER(F, 0x54c);
    rx_addr cols = x_child(reader, tnode, S_COLUMNS);
    if (!cols) unreached(0x004ef743, "a table without <Columns> (the columns of the database's table of that name)");
    rx_tbl_vector_reserve_ptr(t + 4, x_count(reader, cols, S_COLUMN));       /* vf4, 004f0340 */
    const rx_addr cur = F + 0x1c, ival = F + 0x14, fval = F + 0x20, str = F + 0x24;
    const rx_addr bname = F + 0x40, btype = F + 0x140, bpcol = F + 0x240, bdef = F + 0x340, bptab = F + 0x440;
    SU32(cur, 0, 0);
    for (rx_addr col = x_first(reader, cols, S_COLUMN, cur); col; col = x_next(reader, U32(cur, 0))) {
        zero(bname, 0x100); zero(btype, 0x100); zero(bptab, 0x100); zero(bpcol, 0x100);
        uint32_t flags = 0;
        rx_addr parent = 0, e;
        if ((e = x_attr(reader, col, S_NAME))) x_attr_string(reader, e, bname, 0x100);
        if ((e = x_attr(reader, col, S_TYPE))) x_attr_string(reader, e, btype, 0x100);
        if ((e = x_attr(reader, col, S_SEARCHABLE))) { SU32(ival, 0, 0); if (x_attr_bools(reader, e, ival, 1) && U32(ival, 0) == 1) flags = 1; }
        if ((e = x_attr(reader, col, S_UNIQUE))) { SU32(ival, 0, 0); if (x_attr_bools(reader, e, ival, 1) && U32(ival, 0) == 1) flags |= 2; }
        if ((e = x_attr(reader, col, S_PARENTTABLE))) {
            x_attr_string(reader, e, bptab, 0x100);
            parent = rx_tbl_find(U32(0, RX_TBL_DATABASE_PTR), bptab);
        }
        if ((e = x_attr(reader, col, S_PARENTCOLUMN))) x_attr_string(reader, e, bpcol, 0x100);
        rx_addr def = x_attr(reader, col, S_DEFAULT);
        if (crt_stricmp(btype, S_INT) == 0) {
            SU32(ival, 0, 0);
            if (def) x_attr_ints(reader, def, ival, 1);
            rx_tbl_add_int_column(t, bname, ival, flags, parent, bpcol, 0);
        } else if (crt_stricmp(btype, S_FLOAT) == 0) {
            SU32(fval, 0, 0);
            if (def) x_attr_floats(reader, def, fval, 1);
            rx_tbl_add_float_column(t, bname, fval, flags, parent, bpcol);
        } else if (crt_stricmp(btype, S_STRING) == 0) {
            zero(bdef, 0x100);
            if (def) x_attr_string(reader, def, bdef, 0x100);
            rx_tbl_str_init(str);
            rx_tbl_str_assign_n(str, bdef, gstrlen(bdef));            /* 004f2ce0 */
            rx_tbl_add_string_column(t, bname, str, flags, parent, bpcol);
            rx_tbl_str_tidy(str);                                     /* 004f3320 when the buffer is on the heap */
        }
    }
    x_release(reader, U32(cur, 0));
    RX_FRAME_LEAVE(F, 0x54c);
}

/* 004ef8f0 (ECX = table; reader, table node; RET 8 by the caller): every <Row> of <Rows>: a default cell appended to each column,
 * then the row's attribute of the column's name, when present, stored. Frame: sub 0x84c + 4 pushes. */
static void load_rows(rx_addr t, rx_addr reader, rx_addr tnode) {
    RX_FRAME_ENTER(F, 0x85c);
    const rx_addr ival = F + 0x24, cur = F + 0x28, fval = F + 0x30, str = F + 0x34, buf = F + 0x50;
    rx_addr rows = x_child(reader, tnode, S_ROWS);
    uint32_t n = x_children(reader, rows);                            /* vf5 */
    uint32_t ncols = (U32(t, 0x14) - U32(t, 0x10)) >> 2;
    uint32_t row = rx_tbl_row_count(t);                               /* 004021d0 */
    SU32(cur, 0, 0);
    for (uint32_t i = 0; i < ncols; i++) rx_tbl_column_reserve(U32(U32(t, 0x10), 4 * i), n);   /* 004eb5c0 */
    for (rx_addr r = x_first(reader, rows, S_ROW, cur); r; r = x_next(reader, U32(cur, 0)), row++) {
        for (uint32_t i = 0; i < ncols; i++) {
            rx_addr col = U32(U32(t, 0x10), 4 * i);
            rx_tbl_column_append_default(col);                        /* 004eb4e0 */
            rx_addr e = x_attr(reader, r, col);                       /* the attribute named like the column */
            if (!e) continue;
            switch (U32(col, 0x2c)) {
            case 1:
                SU32(ival, 0, 0);
                if (x_attr_ints(reader, e, ival, 1)) rx_tbl_set_int(col, row, ival);
                break;
            case 2:
                SU32(fval, 0, 0);
                if (x_attr_floats(reader, e, fval, 1)) rx_tbl_set_float(col, row, fval);
                break;
            case 4:
                zero(buf, 0x800);
                if (x_attr_string(reader, e, buf, 0x800)) {
                    rx_tbl_str_init(str);
                    rx_tbl_str_assign_n(str, buf, gstrlen(buf));
                    rx_tbl_set_string(col, row, str);
                    rx_tbl_str_tidy(str);
                }
                break;
            default: break;
            }
        }
    }
    x_release(reader, U32(cur, 0));
    SU32(t, 0x68, U32(t, 0x68) + 1);
    RX_FRAME_LEAVE(F, 0x85c);
}

/* Database_LoadTables 004ef060: every <Table> of the node into [0x00a89cfc] (keep_rows 0: Database_ClearRows first). A table the
 * database does not have yet is created (0x74, 004ec4f0), named, listed (+0x38[count++]), mapped by name (+0x18) and given its
 * columns; then its rows are appended, Table::vf1 (0x007e3be0: ret) runs and its version becomes the database's. Frame: sub 0x214
 * + 4 pushes. */
void rx_tbl_load_tables(rx_addr tables, rx_addr reader, uint32_t keep_rows) {
    RX_FRAME_ENTER(F, 0x224);
    if (!keep_rows) rx_tbl_database_clear_rows(U32(0, RX_TBL_DATABASE_PTR));
    const rx_addr cur = F + 0x10, keyp = F + 0x14, bname = F + 0x18, btype = F + 0x118;
    SU32(cur, 0, 0);
    for (rx_addr tn = x_first(reader, tables, S_TABLE, cur); tn; tn = x_next(reader, U32(cur, 0))) {
        crt_strncpy(btype, S_TABLE, 6);                                /* "Table" + memset(0xf9) */
        zero(btype + 6, 0xfa);
        rx_addr e;
        if ((e = x_child(reader, tn, S_TYPE_UC))) x_node_string(reader, e, btype, 0x100);
        zero(bname, 0x100);
        if ((e = x_child(reader, tn, S_NAME_UC))) x_node_string(reader, e, bname, 0x100);
        rx_addr t = rx_tbl_find(U32(0, RX_TBL_DATABASE_PTR), bname);  /* Database_FindTable 00402220 */
        if (!t) {
            rx_tbl_alloc_init();
            rx_addr p = rx_crt_aligned_malloc(0x74, 0x10);
            t = p ? rx_tbl_table_construct(p) : 0;
            if (crt_stricmp(t + 0x3c, bname) != 0) { crt_strncpy(t + 0x3c, bname, 0x29); SU8(t, 0x65, 0); }
            rx_addr db = U32(0, RX_TBL_DATABASE_PTR);
            SU32(keyp, 0, t + 0x3c);
            SU32(db, 0x38 + 4 * U32(db, 0x1e8), t);
            SU32(db, 0x1e8, U32(db, 0x1e8) + 1);
            SU32(rx_tbl_name_map_index(db + 0x18, keyp), 0, t);       /* 004f0540 */
            load_columns(reader, t, tn);                              /* 004ef340 */
        }
        load_rows(t, reader, tn);                                     /* 004ef8f0 */
        /* Table::vf1 (0x007e3be0) is a bare ret */
        SU32(t, 0x6c, U32(U32(0, RX_TBL_DATABASE_PTR), 0x1ec));
    }
    x_release(reader, U32(cur, 0));
    RX_FRAME_LEAVE(F, 0x224);
}

/* 00544cc0 (ESI = array {+0 data, +4 count, +8 capacity, +0xc growth}; &value; RET 4): append unless present */
static void array_add_unique(rx_addr a, rx_addr valp) {
    uint32_t v = U32(valp, 0), n = U32(a, 4);
    for (uint32_t i = 0; i < n; i++) if (U32(U32(a, 0), 4 * i) == v) return;
    if ((int32_t)n >= (int32_t)U32(a, 8)) {
        uint32_t cap = U32(a, 0xc) + U32(a, 8);
        rx_addr p = rx_reflex_aligned_alloc(rx_mul_sat(cap, 4));
        crt_memcpy(p, U32(a, 0), 4 * U32(a, 8));
        rx_reflex_aligned_free(U32(a, 0));
        SU32(a, 0, p); SU32(a, 8, cap);
    }
    SU32(U32(a, 0), 4 * U32(a, 4), v);
    SU32(a, 4, U32(a, 4) + 1);
}

/* TableDataAssetIO::vf3 004f0170: the TableDataAsset {vtable 0x93eb54, +8 0, +0xc 0, +0x10 the name} and FUN_004eefa0 (EDI = asset;
 * stream, name): an XMLReader on the stack loads the stream; the asset joins the database's asset array (+4) and the document's
 * <Tables> go into [0x00a89cfc] keeping the rows already there. Frame: and esp, -8; sub 0x470 + 2 pushes. */
rx_addr rx_tbl_asset_io_create(rx_stream* s, rx_addr name) {
    rx_addr a = rx_reflex_aligned_alloc(0x50);
    if (a) { SU32(a, 8, 0); SU32(a, 0xc, 0); SU32(a, 0, 0x0093eb54); }
    RX_FRAME_ENTER(F, 0x480);
    const rx_addr reader = F + 0x18, slot = F + 0x14;
    rx_xml_reader_construct(reader);
    if (rx_xml_reader_load(reader, s)) {
        crt_strncpy(a + 0x10, name, 0x3f); SU8(a, 0x4f, 0);
        rx_addr tables = x_child(reader, U32(reader, 0x38), S_TABLES);   /* XMLReader::vf7 00828020 */
        SU32(slot, 0, a);
        array_add_unique(U32(0, RX_TBL_DATABASE_PTR) + 4, slot);
        rx_tbl_load_tables(tables, reader, 1);
    }
    rx_xml_reader_destroy(reader);                                    /* 00824fe0 */
    RX_FRAME_LEAVE(F, 0x480);
    return a;
}
static rx_addr tbl_vf3(rx_stream* s, rx_addr name, uint32_t lo, uint32_t hi) { (void)lo; (void)hi; return rx_tbl_asset_io_create(s, name); }
void rx_tbl_register_loader(void) { rx_asset_set_loader(0x0093a184, tbl_vf3); }   /* TableDataAssetIO */
