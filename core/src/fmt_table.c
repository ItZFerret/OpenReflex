/* fmt_table.c -- see fmt_table.h. Reference: Rfx.Tables / ReflexEvents.Tables (C#), ui_export.py parse_localiz. */
#include "fmt_table.h"
#include "fmt_bxml.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct { const fmt_bxml* doc; int node; const char* origin; } row_t;

struct fmt_table {
    const char* name;
    int ncols;
    const char** col_name;
    const char** col_type;
    const char** col_default;
    int nrows, cap;
    row_t* rows;
};

struct fmt_tables {
    fmt_bxml** docs; int ndocs;
    char** origins;                    /* owned copies */
    fmt_table* tables; int ntables, cap;
};

void fmt_tables_free(fmt_tables* t)
{
    if (!t) return;
    for (int i = 0; i < t->ntables; i++) { free(t->tables[i].col_name); free(t->tables[i].col_type); free(t->tables[i].col_default); free(t->tables[i].rows); }
    for (int i = 0; i < t->ndocs; i++) { fmt_bxml_free(t->docs[i]); free(t->origins[i]); }
    free(t->docs); free(t->origins); free(t->tables);
    free(t);
}

static fmt_table* table_for(fmt_tables* ts, const char* name, const fmt_bxml* d, int columns)
{
    for (int i = 0; i < ts->ntables; i++) if (!strcmp(ts->tables[i].name, name)) return &ts->tables[i];
    if (ts->ntables == ts->cap) {
        int cap = ts->cap ? ts->cap * 2 : 128;
        fmt_table* n = (fmt_table*)fmt_realloc(ts->tables, (size_t)cap * sizeof *n);
        if (!n) return NULL;
        ts->tables = n; ts->cap = cap;
    }
    fmt_table* t = &ts->tables[ts->ntables];
    memset(t, 0, sizeof *t);
    t->name = name;
    int nc = columns >= 0 ? fmt_bxml_child_count(d, columns) : 0;
    t->col_name = (const char**)fmt_alloc(((size_t)nc + 1) * sizeof(char*));
    t->col_type = (const char**)fmt_alloc(((size_t)nc + 1) * sizeof(char*));
    t->col_default = (const char**)fmt_alloc(((size_t)nc + 1) * sizeof(char*));
    if (!t->col_name || !t->col_type || !t->col_default) { free(t->col_name); free(t->col_type); free(t->col_default); return NULL; }
    for (int c = 0; c < nc; c++) {
        int cn = fmt_bxml_child(d, columns, c);
        const char* nm = fmt_bxml_attr_str(d, cn, "name");
        if (!nm) continue;
        t->col_name[t->ncols] = nm;
        const char* ty = fmt_bxml_attr_str(d, cn, "type");
        t->col_type[t->ncols] = ty ? ty : "string";
        const char* df = fmt_bxml_attr_str(d, cn, "default");
        t->col_default[t->ncols] = df ? df : "";
        t->ncols++;
    }
    ts->ntables++;
    return t;
}

int fmt_tables_append(fmt_tables* ts, const uint8_t* data, size_t size, const char* origin)
{
    fmt_bxml* d = fmt_bxml_parse(data, size);
    if (!d) return -1;
    if (ts->ndocs % 8 == 0) {
        fmt_bxml** nd = (fmt_bxml**)fmt_realloc(ts->docs, ((size_t)ts->ndocs + 8) * sizeof(*nd));
        if (!nd) { fmt_bxml_free(d); return -1; }
        ts->docs = nd;
        char** no = (char**)fmt_realloc(ts->origins, ((size_t)ts->ndocs + 8) * sizeof(*no));
        if (!no) { fmt_bxml_free(d); return -1; }
        ts->origins = no;
    }
    ts->docs[ts->ndocs] = d;
    ts->origins[ts->ndocs] = fmt_strdup(origin ? origin : "");
    if (!ts->origins[ts->ndocs]) { fmt_bxml_free(d); return -1; }
    const char* org = ts->origins[ts->ndocs];
    ts->ndocs++;
    int tables = fmt_bxml_find_child(d, 0, "Tables");
    if (tables < 0) return fmt_fail("tables: no <Tables> element");
    for (int i = 0; i < fmt_bxml_child_count(d, tables); i++) {
        int tn = fmt_bxml_child(d, tables, i);
        if (strcmp(fmt_bxml_tag(d, tn), "Table") != 0) continue;
        int nm = fmt_bxml_find_child(d, tn, "Name");
        fmt_bxml_value v;
        if (nm < 0 || fmt_bxml_node_value(d, nm, &v) < 0 || v.kind != FMT_BX_STRING) continue;
        fmt_table* t = table_for(ts, v.str, d, fmt_bxml_find_child(d, tn, "Columns"));
        if (!t) return -1;
        int rows = fmt_bxml_find_child(d, tn, "Rows");
        for (int r = 0; rows >= 0 && r < fmt_bxml_child_count(d, rows); r++) {
            int rn = fmt_bxml_child(d, rows, r);
            if (strcmp(fmt_bxml_tag(d, rn), "Row") != 0) continue;
            if (t->nrows == t->cap) {
                int cap = t->cap ? t->cap * 2 : 64;
                row_t* nr = (row_t*)fmt_realloc(t->rows, (size_t)cap * sizeof *nr);
                if (!nr) return -1;
                t->rows = nr; t->cap = cap;
            }
            t->rows[t->nrows].doc = d; t->rows[t->nrows].node = rn; t->rows[t->nrows].origin = org;
            t->nrows++;
        }
    }
    return 0;
}

fmt_tables* fmt_tables_parse(const uint8_t* data, size_t size, const char* origin)
{
    fmt_tables* t = (fmt_tables*)fmt_alloc(sizeof *t);
    if (!t) return NULL;
    if (fmt_tables_append(t, data, size, origin) < 0) { fmt_tables_free(t); return NULL; }
    return t;
}

fmt_tables* fmt_tables_load_merged(fmt_install* in)
{
    fmt_buf raw;
    if (fmt_install_read_named(in, "MXTables_Default", "MXTables_Default", "MXTables_Default", "tbldata", &raw) < 0) return NULL;
    fmt_tables* ts = fmt_tables_parse(raw.data, raw.size, "MXTables_Default");
    fmt_buf_free(&raw);
    if (!ts) return NULL;
    char** names;
    int n = fmt_install_database_names(in, "DLC", &names);
    if (n < 0) { fmt_tables_free(ts); return NULL; }
    for (int i = 0; i < n; i++) {
        const fmt_database* db = fmt_install_db(in, names[i]);
        if (!db) continue;                                           /* a damaged DLC database is skipped, as the game's loader does */
        int* order = (int*)fmt_alloc(((size_t)db->npackages + 1) * sizeof(int));
        if (!order) { free(names); fmt_tables_free(ts); return NULL; }
        for (int k = 0; k < db->npackages; k++) order[k] = k;
        for (int a = 1; a < db->npackages; a++) {                    /* by package name, ordinal (insertion sort: few packages) */
            int key = order[a], b = a - 1;
            while (b >= 0 && strcmp(db->packages[order[b]].name, db->packages[key].name) > 0) { order[b + 1] = order[b]; b--; }
            order[b + 1] = key;
        }
        for (int k = 0; k < db->npackages; k++) {
            const fmt_package* p = &db->packages[order[k]];
            for (int a = 0; a < p->nassets; a++) {
                if (strcmp(p->assets[a].type, "tbldata") != 0) continue;
                fmt_buf b;
                if (fmt_install_read(in, &p->assets[a], &b) < 0) continue;
                fmt_tables_append(ts, b.data, b.size, names[i]);
                fmt_buf_free(&b);
            }
        }
        free(order);
    }
    free(names);
    return ts;
}

int fmt_tables_count(const fmt_tables* t) { return t ? t->ntables : 0; }
const fmt_table* fmt_tables_at(const fmt_tables* t, int i) { return t && i >= 0 && i < t->ntables ? &t->tables[i] : NULL; }
const fmt_table* fmt_tables_find(const fmt_tables* t, const char* name)
{
    if (t && name) for (int i = 0; i < t->ntables; i++) if (!strcmp(t->tables[i].name, name)) return &t->tables[i];
    return NULL;
}

const char* fmt_table_name(const fmt_table* t) { return t ? t->name : NULL; }
int fmt_table_col_count(const fmt_table* t) { return t ? t->ncols : 0; }
const char* fmt_table_col_name(const fmt_table* t, int c) { return t && c >= 0 && c < t->ncols ? t->col_name[c] : NULL; }
const char* fmt_table_col_type(const fmt_table* t, int c) { return t && c >= 0 && c < t->ncols ? t->col_type[c] : NULL; }
const char* fmt_table_col_default(const fmt_table* t, int c) { return t && c >= 0 && c < t->ncols ? t->col_default[c] : NULL; }
int fmt_table_col_find(const fmt_table* t, const char* name)
{
    if (t && name) for (int c = 0; c < t->ncols; c++) if (!strcmp(t->col_name[c], name)) return c;
    return -1;
}
int fmt_table_row_count(const fmt_table* t) { return t ? t->nrows : 0; }
const char* fmt_table_row_origin(const fmt_table* t, int r) { return t && r >= 0 && r < t->nrows ? t->rows[r].origin : NULL; }

int fmt_table_cell(const fmt_table* t, int row, int col, fmt_cell* out)
{
    memset(out, 0, sizeof *out);
    if (!t || row < 0 || row >= t->nrows || col < 0 || col >= t->ncols) return -1;
    const row_t* r = &t->rows[row];
    const fmt_bxml* d = r->doc;
    int a = -1;
    int na = fmt_bxml_node_attr_count(d, r->node);
    if (col < na) {                                              /* the usual case: one attribute per column, in column order */
        int cand = fmt_bxml_node_attr(d, r->node, col);
        if (!strcmp(fmt_bxml_attr_name(d, cand), t->col_name[col])) a = cand;
    }
    if (a < 0) a = fmt_bxml_find_attr(d, r->node, t->col_name[col]);
    if (a < 0) return 0;
    fmt_bxml_value v;
    fmt_bxml_attr_value(d, a, &v);
    out->kind = v.kind;
    switch (v.kind) {
    case FMT_BX_STRING: out->s = v.str; break;
    case FMT_BX_INT: case FMT_BX_UINT: case FMT_BX_BOOL: out->i = fmt_bx_i32(&v, 0); out->f = (float)out->i; break;
    case FMT_BX_FLOAT: out->f = fmt_bx_f32(&v, 0, 0); out->i = (int32_t)rintf(out->f); break;
    default: break;
    }
    return 0;
}

int32_t fmt_table_int(const fmt_table* t, int row, const char* col, int32_t def)
{
    fmt_cell c;
    int ci = fmt_table_col_find(t, col);
    if (ci < 0 || fmt_table_cell(t, row, ci, &c) < 0) return def;
    if (c.kind == FMT_BX_INT || c.kind == FMT_BX_UINT || c.kind == FMT_BX_BOOL || c.kind == FMT_BX_FLOAT) return c.i;
    if (c.kind == FMT_BX_STRING) {
        char* end;
        long v = strtol(c.s, &end, 10);
        return (*c.s && !*end) ? (int32_t)v : def;
    }
    return def;
}

float fmt_table_float(const fmt_table* t, int row, const char* col, float def)
{
    fmt_cell c;
    int ci = fmt_table_col_find(t, col);
    if (ci < 0 || fmt_table_cell(t, row, ci, &c) < 0) return def;
    if (c.kind == FMT_BX_STRING) {
        char* end;
        float v = strtof(c.s, &end);
        return (*c.s && !*end) ? v : def;
    }
    return c.kind == FMT_BX_EMPTY ? def : c.f;
}

const char* fmt_table_str(const fmt_table* t, int row, const char* col)
{
    fmt_cell c;
    int ci = fmt_table_col_find(t, col);
    if (ci < 0 || fmt_table_cell(t, row, ci, &c) < 0 || c.kind != FMT_BX_STRING) return NULL;
    return c.s;
}

int fmt_table_find_row(const fmt_table* t, const char* col, int32_t value, int from)
{
    int ci = fmt_table_col_find(t, col);
    if (ci < 0) return -1;
    for (int r = from < 0 ? 0 : from; t && r < t->nrows; r++) {
        fmt_cell c;
        fmt_table_cell(t, r, ci, &c);
        if ((c.kind == FMT_BX_INT || c.kind == FMT_BX_UINT || c.kind == FMT_BX_BOOL) && c.i == value) return r;
    }
    return -1;
}

/* ------------------------------------------------------------------------------------------------ localiz */

struct fmt_strings {
    int n;
    char** key;
    char** value;
    char** embedded;
    uint32_t* capacity;
    int* sorted;                        /* indices ordered by key, for lookup */
    char* store;
};

void fmt_strings_free(fmt_strings* s)
{
    if (!s) return;
    for (int i = 0; i < s->n; i++) { free(s->key[i]); free(s->value[i]); free(s->embedded[i]); }
    free(s->key); free(s->value); free(s->embedded); free(s->capacity); free(s->sorted); free(s->store);
    free(s);
}

static char* utf16_to_utf8(const uint8_t* p, size_t units)
{
    char* out = (char*)fmt_alloc(units * 3 + 1);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < units; i++) {
        uint32_t c = fmt_u16(p + 2 * i);
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < units) {
            uint32_t lo = fmt_u16(p + 2 * (i + 1));
            if (lo >= 0xDC00 && lo < 0xE000) { c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00); i++; }
        }
        if (c < 0x80) out[o++] = (char)c;
        else if (c < 0x800) { out[o++] = (char)(0xC0 | c >> 6); out[o++] = (char)(0x80 | (c & 63)); }
        else if (c < 0x10000) { out[o++] = (char)(0xE0 | c >> 12); out[o++] = (char)(0x80 | ((c >> 6) & 63)); out[o++] = (char)(0x80 | (c & 63)); }
        else { out[o++] = (char)(0xF0 | c >> 18); out[o++] = (char)(0x80 | ((c >> 12) & 63)); out[o++] = (char)(0x80 | ((c >> 6) & 63)); out[o++] = (char)(0x80 | (c & 63)); }
    }
    out[o] = 0;
    return out;
}

typedef struct { const char* key; int index; } sort_item;
static int sort_cmp(const void* a, const void* b)
{
    const sort_item *x = (const sort_item*)a, *y = (const sort_item*)b;
    int c = strcmp(x->key, y->key);
    return c ? c : x->index - y->index;
}

fmt_strings* fmt_strings_parse(const uint8_t* b, size_t size)
{
    if (size < 12 || fmt_u32(b) != 2) { fmt_fail("localiz: bad version"); return NULL; }
    uint32_t n = fmt_u32(b + 4), pool = fmt_u32(b + 8);
    if (n >= 200000 || (size_t)12 + pool > size) { fmt_fail("localiz: bad counts"); return NULL; }
    fmt_strings* s = (fmt_strings*)fmt_alloc(sizeof *s);
    if (!s) return NULL;
    s->key = (char**)fmt_alloc(((size_t)n + 1) * sizeof(char*));
    s->value = (char**)fmt_alloc(((size_t)n + 1) * sizeof(char*));
    s->embedded = (char**)fmt_alloc(((size_t)n + 1) * sizeof(char*));
    s->capacity = (uint32_t*)fmt_alloc(((size_t)n + 1) * sizeof(uint32_t));
    s->sorted = (int*)fmt_alloc(((size_t)n + 1) * sizeof(int));
    if (!s->key || !s->value || !s->embedded || !s->capacity || !s->sorted) { fmt_strings_free(s); return NULL; }
    size_t p = 12, end = 12 + (size_t)pool;
    for (uint32_t i = 0; i < n; i++) {
        size_t e = p;
        while (e < end && b[e]) e++;
        if (e >= end) { fmt_fail("localiz: unterminated key"); s->n = (int)i; fmt_strings_free(s); return NULL; }
        s->key[i] = fmt_strndup((const char*)b + p, e - p);
        s->n = (int)i + 1;
        if (!s->key[i]) { fmt_strings_free(s); return NULL; }
        p = e + 1;
    }
    if (p != end) { fmt_fail("localiz: key pool size mismatch"); fmt_strings_free(s); return NULL; }
    if (p + 4 * (size_t)n + 4 > size) { fmt_fail("localiz: truncated capacity table"); fmt_strings_free(s); return NULL; }
    for (uint32_t i = 0; i < n; i++) s->capacity[i] = fmt_u32(b + p + 4 * (size_t)i);
    p += 4 * (size_t)n + 4;
    for (uint32_t i = 0; i < n; i++) {
        for (int pass = 0; pass < 2; pass++) {
            size_t st = p;
            while (p + 1 < size && (b[p] || b[p + 1])) p += 2;
            if (p + 1 >= size) { fmt_fail("localiz: truncated string text"); fmt_strings_free(s); return NULL; }
            char* u = utf16_to_utf8(b + st, (p - st) / 2);
            if (!u) { fmt_strings_free(s); return NULL; }
            p += 2;
            if (pass == 0) {
                s->value[i] = u;
                /* EMB_ keys whose string is only '?' characters are followed by their embedded-format string */
                int only_q = 1;
                for (const char* c = u; *c; c++) if (*c != '?') { only_q = 0; break; }
                if (!(!strncmp(s->key[i], "EMB_", 4) && only_q)) break;
            } else s->embedded[i] = u;
        }
    }
    sort_item* items = (sort_item*)fmt_alloc(((size_t)n + 1) * sizeof *items);
    if (!items) { fmt_strings_free(s); return NULL; }
    for (uint32_t i = 0; i < n; i++) { items[i].key = s->key[i]; items[i].index = (int)i; }
    qsort(items, n, sizeof *items, sort_cmp);
    for (uint32_t i = 0; i < n; i++) s->sorted[i] = items[i].index;
    free(items);
    return s;
}

int fmt_strings_count(const fmt_strings* s) { return s ? s->n : 0; }
const char* fmt_strings_key(const fmt_strings* s, int i) { return s && i >= 0 && i < s->n ? s->key[i] : NULL; }
const char* fmt_strings_value(const fmt_strings* s, int i) { return s && i >= 0 && i < s->n ? s->value[i] : NULL; }
const char* fmt_strings_embedded(const fmt_strings* s, int i) { return s && i >= 0 && i < s->n ? s->embedded[i] : NULL; }
uint32_t fmt_strings_capacity(const fmt_strings* s, int i) { return s && i >= 0 && i < s->n ? s->capacity[i] : 0; }

const char* fmt_strings_get(const fmt_strings* s, const char* key)
{
    if (!s || !key) return NULL;
    int lo = 0, hi = s->n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2, c = strcmp(s->key[s->sorted[mid]], key);
        if (c == 0) {                                               /* a repeated key: the last one wins, as in the loaders */
            while (mid + 1 < s->n && !strcmp(s->key[s->sorted[mid + 1]], key)) mid++;
            return s->value[s->sorted[mid]];
        }
        if (c < 0) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}
