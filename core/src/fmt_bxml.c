/* fmt_bxml.c -- see fmt_bxml.h. Reference: _re/formats/bxml.py (strict decoder), scene_read.py (extended types) and
 * Unity/Reflex/Assets/Scripts/Content/Bxml.cs. */
#include "fmt_bxml.h"
#include "fmt_inflate.h"

#include <stdlib.h>

#define MAX_DECODED ((size_t)256 << 20)
#define BXML_NONE 0xFFFFFFFFu

typedef struct { uint32_t name, ref; uint32_t count, kind; } attr_t;
typedef struct { uint32_t name, ref, count, kind, child_start, child_count, attr_start, attr_count; } node_t;

struct fmt_bxml {
    uint8_t* payload;           /* pool + blob (the tables are decoded into the arrays below) */
    uint32_t nstr;
    const char** str;
    const uint8_t* blob;
    size_t blob_size;
    uint32_t nattr, nnode;
    attr_t* attrs;
    node_t* nodes;
};

int fmt_bxml_elem_size(int kind)
{
    switch (kind) {
    case FMT_BX_BYTES: return 1;
    case FMT_BX_INT: case FMT_BX_UINT: case FMT_BX_FLOAT: case FMT_BX_BOOL: return 4;
    case FMT_BX_VEC2: return 8;
    case FMT_BX_VEC3: return 12;
    case FMT_BX_VEC4: return 16;
    case FMT_BX_MATRIX: return 64;
    default: return 0;
    }
}

void fmt_bxml_free(fmt_bxml* d)
{
    if (!d) return;
    free(d->payload); free(d->str); free(d->attrs); free(d->nodes);
    free(d);
}

static int check_value(const fmt_bxml* d, uint32_t ref, uint32_t count, uint32_t kind, int is_node, const char* what, uint32_t index)
{
    if (kind == FMT_BX_EMPTY) {
        if (!is_node || ref != BXML_NONE || count != 0) return fmt_fail("bxml: invalid empty value at %s %u", what, index);
        return 0;
    }
    if (kind == FMT_BX_STRING) {
        if (count != 0 || ref >= d->nstr) return fmt_fail("bxml: invalid string value at %s %u", what, index);
        return 0;
    }
    int es = fmt_bxml_elem_size((int)kind);
    if (!es) return fmt_fail("bxml: unknown value type %u at %s %u", kind, what, index);
    if ((uint64_t)ref + (uint64_t)count * (uint64_t)es > d->blob_size) return fmt_fail("bxml: value of %s %u exceeds the blob", what, index);
    return 0;
}

fmt_bxml* fmt_bxml_parse(const uint8_t* raw, size_t size)
{
    if (!raw || size < 32) { fmt_fail("bxml: truncated header"); return NULL; }
    if (memcmp(raw, "BXML", 4) != 0) { fmt_fail("bxml: bad magic"); return NULL; }
    uint32_t version = fmt_u32(raw + 4), ns = fmt_u32(raw + 8), ss = fmt_u32(raw + 12), bs = fmt_u32(raw + 16),
             na = fmt_u32(raw + 20), nn = fmt_u32(raw + 24), reserved = fmt_u32(raw + 28);
    if (reserved != 0) { fmt_fail("bxml: nonzero header word at byte 28"); return NULL; }
    uint64_t total64 = (uint64_t)ss + bs + (uint64_t)na * 12 + (uint64_t)nn * 32;
    if (total64 == 0 || total64 > MAX_DECODED || nn == 0) { fmt_fail("bxml: decoded size %llu outside limits", (unsigned long long)total64); return NULL; }
    size_t total = (size_t)total64;
    fmt_bxml* d = (fmt_bxml*)fmt_alloc(sizeof *d);
    if (!d) return NULL;
    uint8_t* pl = (uint8_t*)fmt_alloc(total);
    if (!pl) { free(d); return NULL; }
    d->payload = pl;
    if (version == 0x103EA) {
        if (size < 36 || fmt_u32(raw + 32) != size - 36) { fmt_fail("bxml: compressed length mismatch"); goto fail; }
        if (fmt_zlib_decode(raw + 36, size - 36, pl, total) < 0) goto fail;
    } else if (version == 0x3EA) {
        if (size - 32 != total) { fmt_fail("bxml: raw table sizes differ from the resource size"); goto fail; }
        memcpy(pl, raw + 32, total);
    } else { fmt_fail("bxml: unsupported version 0x%08x", version); goto fail; }

    if (ss == 0 || pl[ss - 1] != 0) { fmt_fail("bxml: string pool lacks a terminating NUL"); goto fail; }
    d->nstr = ns;
    d->str = (const char**)fmt_alloc(((size_t)ns + 1) * sizeof(char*));
    if (!d->str) goto fail;
    {
        uint32_t n = 0, start = 0;
        for (uint32_t i = 0; i < ss; i++) {
            if (pl[i]) continue;
            if (n >= ns) { fmt_fail("bxml: more strings than declared"); goto fail; }
            d->str[n++] = (const char*)pl + start;
            start = i + 1;
        }
        if (n != ns) { fmt_fail("bxml: string count mismatch (%u != %u)", n, ns); goto fail; }
    }
    d->blob = pl + ss;
    d->blob_size = bs;
    d->nattr = na; d->nnode = nn;
    d->attrs = (attr_t*)fmt_alloc((size_t)na * sizeof(attr_t));
    d->nodes = (node_t*)fmt_alloc((size_t)nn * sizeof(node_t));
    if (!d->attrs || !d->nodes) goto fail;
    {
        const uint8_t* p = pl + ss + bs;
        for (uint32_t i = 0; i < na; i++, p += 12) {
            d->attrs[i].name = fmt_u32(p); d->attrs[i].ref = fmt_u32(p + 4);
            d->attrs[i].count = fmt_u16(p + 8); d->attrs[i].kind = fmt_u16(p + 10);
        }
        for (uint32_t i = 0; i < nn; i++, p += 32) {
            node_t* n = &d->nodes[i];
            n->name = fmt_u32(p); n->ref = fmt_u32(p + 4); n->count = fmt_u32(p + 8); n->kind = fmt_u32(p + 12);
            n->child_start = fmt_u32(p + 16); n->child_count = fmt_u32(p + 20); n->attr_start = fmt_u32(p + 24); n->attr_count = fmt_u32(p + 28);
        }
    }
    /* structure: parents before children, every node and attribute owned once */
    {
        uint8_t* owner = (uint8_t*)fmt_alloc((size_t)nn + (size_t)na);
        if (!owner) goto fail;
        uint8_t* aown = owner + nn;
        int ok = 1;
        for (uint32_t i = 0; i < nn && ok; i++) {
            const node_t* n = &d->nodes[i];
            if (n->name >= ns) { fmt_fail("bxml: invalid node name at %u", i); ok = 0; break; }
            if (check_value(d, n->ref, n->count, n->kind, 1, "node", i) < 0) { ok = 0; break; }
            if ((uint64_t)n->child_start > nn || (uint64_t)n->child_start + n->child_count > nn) { fmt_fail("bxml: invalid child range at node %u", i); ok = 0; break; }
            if ((uint64_t)n->attr_start > na || (uint64_t)n->attr_start + n->attr_count > na) { fmt_fail("bxml: invalid attribute range at node %u", i); ok = 0; break; }
            for (uint32_t c = n->child_start; c < n->child_start + n->child_count; c++) {
                if (c <= i) { fmt_fail("bxml: child reference is backward or cyclic at node %u", i); ok = 0; break; }
                if (owner[c]++) { fmt_fail("bxml: node %u has two parents", c); ok = 0; break; }
            }
            for (uint32_t a = n->attr_start; ok && a < n->attr_start + n->attr_count; a++) {
                if (aown[a]++) { fmt_fail("bxml: attribute %u is shared", a); ok = 0; break; }
                const attr_t* at = &d->attrs[a];
                if (at->name >= ns) { fmt_fail("bxml: invalid attribute name at %u", a); ok = 0; break; }
                if (check_value(d, at->ref, at->count, at->kind, 0, "attribute", a) < 0) { ok = 0; break; }
            }
        }
        if (ok) {
            if (owner[0]) { fmt_fail("bxml: the root has a parent"); ok = 0; }
            for (uint32_t i = 1; ok && i < nn; i++) if (owner[i] != 1) { fmt_fail("bxml: node %u is an orphan", i); ok = 0; }
            for (uint32_t a = 0; ok && a < na; a++) if (aown[a] != 1) { fmt_fail("bxml: attribute %u has no owner", a); ok = 0; }
        }
        free(owner);
        if (!ok) goto fail;
    }
    return d;
fail:
    fmt_bxml_free(d);
    return NULL;
}

uint32_t fmt_bxml_node_count(const fmt_bxml* d) { return d ? d->nnode : 0; }
uint32_t fmt_bxml_attr_count(const fmt_bxml* d) { return d ? d->nattr : 0; }
uint32_t fmt_bxml_string_count(const fmt_bxml* d) { return d ? d->nstr : 0; }
size_t fmt_bxml_blob_size(const fmt_bxml* d) { return d ? d->blob_size : 0; }
const char* fmt_bxml_string(const fmt_bxml* d, uint32_t i) { return d && i < d->nstr ? d->str[i] : NULL; }

static inline const node_t* nd(const fmt_bxml* d, int n) { return d && n >= 0 && (uint32_t)n < d->nnode ? &d->nodes[n] : NULL; }
static inline const attr_t* at(const fmt_bxml* d, int a) { return d && a >= 0 && (uint32_t)a < d->nattr ? &d->attrs[a] : NULL; }

const char* fmt_bxml_tag(const fmt_bxml* d, int node) { const node_t* n = nd(d, node); return n ? d->str[n->name] : NULL; }
int fmt_bxml_child_count(const fmt_bxml* d, int node) { const node_t* n = nd(d, node); return n ? (int)n->child_count : 0; }
int fmt_bxml_first_child(const fmt_bxml* d, int node) { const node_t* n = nd(d, node); return n && n->child_count ? (int)n->child_start : -1; }
int fmt_bxml_child(const fmt_bxml* d, int node, int k)
{
    const node_t* n = nd(d, node);
    return n && k >= 0 && (uint32_t)k < n->child_count ? (int)n->child_start + k : -1;
}

int fmt_bxml_find_child(const fmt_bxml* d, int node, const char* tag) { return fmt_bxml_find_child_n(d, node, tag, 0); }

int fmt_bxml_find_child_n(const fmt_bxml* d, int node, const char* tag, int nth)
{
    const node_t* n = nd(d, node);
    if (!n || !tag) return -1;
    for (uint32_t c = n->child_start; c < n->child_start + n->child_count; c++)
        if (!strcmp(d->str[d->nodes[c].name], tag) && nth-- == 0) return (int)c;
    return -1;
}

int fmt_bxml_find_child_attr(const fmt_bxml* d, int node, const char* attr, const char* value)
{
    const node_t* n = nd(d, node);
    if (!n) return -1;
    for (uint32_t c = n->child_start; c < n->child_start + n->child_count; c++) {
        const char* s = fmt_bxml_attr_str(d, (int)c, attr);
        if (s && !strcmp(s, value)) return (int)c;
    }
    return -1;
}

static void make_value(const fmt_bxml* d, uint32_t ref, uint32_t count, uint32_t kind, fmt_bxml_value* v)
{
    v->kind = (int)kind; v->count = 0; v->str = NULL; v->data = NULL;
    if (kind == FMT_BX_STRING) v->str = d->str[ref];
    else if (kind != FMT_BX_EMPTY) { v->count = count; v->data = d->blob + ref; }
}

int fmt_bxml_node_value(const fmt_bxml* d, int node, fmt_bxml_value* v)
{
    const node_t* n = nd(d, node);
    if (!n) { memset(v, 0, sizeof *v); return -1; }
    make_value(d, n->ref, n->count, n->kind, v);
    return 0;
}

int fmt_bxml_node_attr_count(const fmt_bxml* d, int node) { const node_t* n = nd(d, node); return n ? (int)n->attr_count : 0; }
int fmt_bxml_node_attr(const fmt_bxml* d, int node, int k)
{
    const node_t* n = nd(d, node);
    return n && k >= 0 && (uint32_t)k < n->attr_count ? (int)n->attr_start + k : -1;
}
const char* fmt_bxml_attr_name(const fmt_bxml* d, int attr) { const attr_t* a = at(d, attr); return a ? d->str[a->name] : NULL; }
int fmt_bxml_attr_value(const fmt_bxml* d, int attr, fmt_bxml_value* v)
{
    const attr_t* a = at(d, attr);
    if (!a) { memset(v, 0, sizeof *v); return -1; }
    make_value(d, a->ref, a->count, a->kind, v);
    return 0;
}

int fmt_bxml_find_attr(const fmt_bxml* d, int node, const char* name)
{
    const node_t* n = nd(d, node);
    if (!n || !name) return -1;
    for (uint32_t a = n->attr_start; a < n->attr_start + n->attr_count; a++)
        if (!strcmp(d->str[d->attrs[a].name], name)) return (int)a;
    return -1;
}

const char* fmt_bxml_attr_str(const fmt_bxml* d, int node, const char* name)
{
    fmt_bxml_value v;
    int a = fmt_bxml_find_attr(d, node, name);
    if (a < 0 || fmt_bxml_attr_value(d, a, &v) < 0 || v.kind != FMT_BX_STRING) return NULL;
    return v.str;
}

int32_t fmt_bxml_attr_i32(const fmt_bxml* d, int node, const char* name, int32_t def)
{
    fmt_bxml_value v;
    int a = fmt_bxml_find_attr(d, node, name);
    if (a < 0 || fmt_bxml_attr_value(d, a, &v) < 0 || v.count < 1) return def;
    switch (v.kind) {
    case FMT_BX_INT: case FMT_BX_UINT: case FMT_BX_BOOL: return fmt_bx_i32(&v, 0);
    case FMT_BX_FLOAT: return (int32_t)fmt_bx_f32(&v, 0, 0);
    default: return def;
    }
}

float fmt_bxml_attr_f32(const fmt_bxml* d, int node, const char* name, float def)
{
    fmt_bxml_value v;
    int a = fmt_bxml_find_attr(d, node, name);
    if (a < 0 || fmt_bxml_attr_value(d, a, &v) < 0 || v.count < 1) return def;
    switch (v.kind) {
    case FMT_BX_FLOAT: return fmt_bx_f32(&v, 0, 0);
    case FMT_BX_INT: return (float)fmt_bx_i32(&v, 0);
    case FMT_BX_UINT: case FMT_BX_BOOL: return (float)fmt_bx_u32(&v, 0);
    default: return def;
    }
}

float fmt_bx_f32(const fmt_bxml_value* v, uint32_t i, int c)
{
    int es = fmt_bxml_elem_size(v->kind);
    return fmt_f32(v->data + (size_t)es * i + 4 * (size_t)c);
}

void fmt_bx_floats(const fmt_bxml_value* v, uint32_t i, float* out)
{
    int n = fmt_bxml_elem_size(v->kind) / 4;
    for (int c = 0; c < n; c++) out[c] = fmt_bx_f32(v, i, c);
}

int fmt_bxml_resolve(const fmt_bxml* d, const char* path, int* is_attr)
{
    if (is_attr) *is_attr = 0;
    if (!d || !path || path[0] != '/') return -1;
    const char* p = path + 1;
    char part[256];
    int node = 0, first = 1;
    while (*p) {
        const char* e = strchr(p, '/');
        size_t len = e ? (size_t)(e - p) : strlen(p);
        if (!len || len >= sizeof part) return -1;
        memcpy(part, p, len); part[len] = 0;
        p += len; if (*p == '/') p++;
        if (first) {                                             /* the root element */
            if (strcmp(part, fmt_bxml_tag(d, 0))) return -1;
            first = 0;
            continue;
        }
        if (part[0] == '@') {
            if (*p) return -1;
            int a = fmt_bxml_find_attr(d, node, part + 1);
            if (a < 0) return -1;
            for (int b = fmt_bxml_find_attr(d, node, part + 1), k = 0; k < fmt_bxml_node_attr_count(d, node); k++)   /* duplicate names are ambiguous */
                if (fmt_bxml_node_attr(d, node, k) != b && !strcmp(fmt_bxml_attr_name(d, fmt_bxml_node_attr(d, node, k)), part + 1)) return -1;
            if (is_attr) *is_attr = 1;
            return a;
        }
        int occurrence = 0;
        char* br = strchr(part, '[');
        if (br) {
            char* close = strchr(br, ']');
            if (!close || close[1] || br[1] < '1' || br[1] > '9') return -1;
            occurrence = atoi(br + 1);
            *br = 0;
        }
        int matches = 0, found = -1;
        for (int k = 0; k < fmt_bxml_child_count(d, node); k++) {
            int c = fmt_bxml_child(d, node, k);
            if (!strcmp(fmt_bxml_tag(d, c), part)) { matches++; if (matches == (occurrence ? occurrence : 1)) found = c; }
        }
        if (occurrence ? occurrence > matches : matches != 1) return -1;
        node = found;
    }
    return node;
}
