/* build_vehicle_tables.c -- the generic Rdb table readers the construction code calls (build_vehicle.h; owner: unit 6). Hand
 * transliteration of Table_ColumnIndex 00402160, Database_FindTable 00402220, the column map search 00416e40 / 00417da0, the key
 * map search 00418ec0 / 00763270, the cell accessors 0041aaf0 / 0041a480 and the by-name helpers 00419b20 / 004ec210. The
 * _invalid_parameter_noinfo checks of the checked iterators never fire on valid tables and are left out; the critical section
 * 0x00ae1c44 around the cell vectors is the core's single thread (no lock). */
#include "build_vehicle.h"
#include "crt.h"

/* 00417da0 (EAX = map object, s0 = &key char*): lower bound by _stricmp over the tree (head [map + 0x18]; node: +0 left, +4 parent,
 * +8 right, +0xc key, +0x15 isnil) */
static rx_addr strmap_lower_bound(rx_addr map, rx_addr key) {
    rx_addr head = U32(map, 0x18), n = U32(head, 4), res = head;
    while (!U8(n, 0x15)) {
        if (crt_stricmp(U32(n, 0xc), key) < 0) n = U32(n, 8);
        else { res = n; n = U32(n, 0); }
    }
    return res;
}
/* 00416e40 (ESI = map object, s0 = out, s1 = &key): find = the lower bound unless it is the end or key < its key */
static rx_addr strmap_find(rx_addr map, rx_addr key) {
    rx_addr lb = strmap_lower_bound(map, key), head = U32(map, 0x18);
    if (lb == head || crt_stricmp(key, U32(lb, 0xc)) < 0) return head;
    return lb;
}

uint32_t rx_tbl_column_index(rx_addr table, rx_addr name) {
    rx_addr n = strmap_find(table + 0x1c, name);
    if (n == U32(table + 0x1c, 0x18)) return 0xffffffffu;
    (void)U32(U32(table + 0x1c, 0), 0);   /* the checked iterator's dereference reads its proxy's container (0040219e..004021a8) */
    return U32(n, 0x10);
}
RX_FUNCTION(0x00402160, rx_tbl_column_index);

rx_addr rx_tbl_find(rx_addr database, rx_addr name) {
    rx_addr n = strmap_find(database + 0x18, name);
    return n == U32(database + 0x18, 0x18) ? 0 : U32(n, 0x10);
}
RX_FUNCTION(0x00402220, rx_tbl_find);

/* 00763270 (EDI = map object, s0 = out, s1 = &key): std::map<int, ...>::find (node +0xc key, +0x1d isnil) */
static rx_addr intmap_find(rx_addr map, int32_t key) {
    rx_addr head = U32(map, 0x18), res = head;
    if (!U8(U32(head, 4), 0x1d))
        for (rx_addr n = U32(head, 4); !U8(n, 0x1d);) {
            if (I32(n, 0xc) < key) n = U32(n, 8);
            else { res = n; n = U32(n, 0); }
        }
    if (res == head || key < I32(res, 0xc)) return head;
    return res;
}

rx_addr rx_tbl_row(rx_addr column, int32_t key) {
    rx_addr map = U32(column, 0x48), n = intmap_find(map, key);
    return n == U32(map, 0x18) ? 0x009c9ff4u : n + 0x10;
}
RX_FUNCTION(0x00418ec0, rx_tbl_row);

rx_addr rx_tbl_cell_word(rx_addr store, uint32_t col, uint32_t row) {
    rx_addr column = U32(U32(store, 0x10), 4 * col);
    return U32(U32(column, 0x40), 0xc) + 4 * row;
}
RX_FUNCTION(0x0041aaf0, rx_tbl_cell_word);
rx_addr rx_tbl_cell_string(rx_addr column, uint32_t row) { return U32(U32(column, 0x40), 0xc) + 0x1c * row; }
RX_FUNCTION(0x0041a480, rx_tbl_cell_string);

rx_addr rx_tbl_ref_string(rx_addr ref, rx_addr col_name) {
    rx_addr store = U32(ref, 8);
    uint32_t c = rx_tbl_column_index(store, col_name);
    return rx_tbl_cell_string(U32(U32(store, 0x10), 4 * c), U32(ref, 0));
}
RX_FUNCTION(0x00419b20, rx_tbl_ref_string);

/* 00419ea0 (EDI = map object, s0 = out, s1 = &key): find in a std::map<int, std::set> (node +0xc key, +0x39 isnil) */
static rx_addr intsetmap_find(rx_addr map, int32_t key) {
    rx_addr head = U32(map, 0x18), res = head;
    for (rx_addr n = U32(head, 4); !U8(n, 0x39);) {
        if (I32(n, 0xc) < key) n = U32(n, 8);
        else { res = n; n = U32(n, 0); }
    }
    if (res == head || key < I32(res, 0xc)) return head;
    return res;
}
/* 00418e50 (EAX = &key, s0 = column; RET 4): the rows of a non-unique index (column +0x44: std::map<int, std::set<row>>) -> &the
 * std::set (+0x18 head, +0x1c size; node +0xc the row), or the static empty set 0x00ae1c1c */
rx_addr rx_tbl_rows(rx_addr column, int32_t key) {
    rx_addr map = U32(column, 0x44), n = intsetmap_find(map, key);
    return n == U32(map, 0x18) ? 0x00ae1c1cu : n + 0x10;
}
RX_FUNCTION(0x00418e50, rx_tbl_rows);
