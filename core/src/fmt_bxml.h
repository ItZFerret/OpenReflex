/* fmt_bxml.h -- BXML, the engine's binary XML (databases, levels, scenes' entity chunks, materials, tables, packdata).
 *
 * File layout (little endian). Two headers exist:
 *     0x000103EA  36-byte header, payload is one zlib stream           (.database .level .tbldata .bxml, scene chunks)
 *     0x000003EA  32-byte header, payload stored raw                   (material and shader resources)
 *   u32 'BXML', u32 version, u32 string count, u32 string pool bytes, u32 value blob bytes, u32 attribute count,
 *   u32 node count, u32 0 [, u32 compressed payload bytes]
 *   payload: string pool (NUL-terminated strings), value blob, attributes (12 bytes: u32 name string, u32 value ref,
 *   u16 count, u16 type), nodes (32 bytes: u32 name string, u32 value ref, u32 count, u32 type, u32 first child,
 *   u32 child count, u32 first attribute, u32 attribute count). Node 0 is the root; children of a node are contiguous and
 *   come after their parent; every attribute belongs to exactly one node.
 * Value types (ref is a string index for strings, a byte offset into the blob otherwise; count = number of elements, for
 * type 2 the number of bytes): 0 empty (ref 0xFFFFFFFF), 1 string, 2 opaque bytes, 3 int32, 4 uint32, 5 float32, 7 vec4,
 * 8 matrix (16 floats, row-major D3D, row vectors: rows 0..2 the axes, row 3 the translation), 9 vec2, 10 vec3, 11 bool
 * (uint32 0 / 1). A count of 0 is an empty array; `count` 1 is a plain scalar.
 *
 * Node indices are the file's (document order of the table, parents before children). All functions taking a node or
 * attribute index return a neutral value (NULL / -1 / the default) for an index out of range. */
#ifndef FMT_BXML_H
#define FMT_BXML_H

#include "fmt_common.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    FMT_BX_EMPTY = 0, FMT_BX_STRING = 1, FMT_BX_BYTES = 2, FMT_BX_INT = 3, FMT_BX_UINT = 4, FMT_BX_FLOAT = 5,
    FMT_BX_VEC4 = 7, FMT_BX_MATRIX = 8, FMT_BX_VEC2 = 9, FMT_BX_VEC3 = 10, FMT_BX_BOOL = 11
};

typedef struct fmt_bxml fmt_bxml;

typedef struct fmt_bxml_value {
    int            kind;       /* FMT_BX_* */
    uint32_t       count;      /* elements (bytes for FMT_BX_BYTES); 0 for EMPTY and STRING */
    const char*    str;        /* FMT_BX_STRING */
    const uint8_t* data;       /* the elements, little endian, count * fmt_bxml_elem_size(kind) bytes (into the document) */
} fmt_bxml_value;

fmt_bxml*  fmt_bxml_parse(const uint8_t* data, size_t size);       /* copies what it keeps; either header */
void       fmt_bxml_free(fmt_bxml* d);

uint32_t   fmt_bxml_node_count(const fmt_bxml* d);
uint32_t   fmt_bxml_attr_count(const fmt_bxml* d);                 /* all attributes of the document */
uint32_t   fmt_bxml_string_count(const fmt_bxml* d);
size_t     fmt_bxml_blob_size(const fmt_bxml* d);                  /* bytes of the value blob */
const char* fmt_bxml_string(const fmt_bxml* d, uint32_t index);
int        fmt_bxml_elem_size(int kind);                           /* bytes per element, 0 for kinds without a payload */

/* nodes */
const char* fmt_bxml_tag(const fmt_bxml* d, int node);
int        fmt_bxml_child_count(const fmt_bxml* d, int node);
int        fmt_bxml_child(const fmt_bxml* d, int node, int k);     /* node index of the k-th child, -1 out of range */
int        fmt_bxml_first_child(const fmt_bxml* d, int node);      /* index of the first child (children are contiguous), or -1 */
int        fmt_bxml_find_child(const fmt_bxml* d, int node, const char* tag);             /* first child with that tag, or -1 */
int        fmt_bxml_find_child_n(const fmt_bxml* d, int node, const char* tag, int nth);  /* nth (0-based) such child */
int        fmt_bxml_find_child_attr(const fmt_bxml* d, int node, const char* attr, const char* value);   /* first child whose string attribute equals value */
int        fmt_bxml_node_value(const fmt_bxml* d, int node, fmt_bxml_value* out);         /* 0, or -1 for a bad index */

/* attributes (attribute indices are the document's; a node owns attr_count consecutive ones) */
int        fmt_bxml_node_attr_count(const fmt_bxml* d, int node);
int        fmt_bxml_node_attr(const fmt_bxml* d, int node, int k);                        /* attribute index, -1 out of range */
const char* fmt_bxml_attr_name(const fmt_bxml* d, int attr);
int        fmt_bxml_attr_value(const fmt_bxml* d, int attr, fmt_bxml_value* out);
int        fmt_bxml_find_attr(const fmt_bxml* d, int node, const char* name);             /* attribute index or -1 */

/* conveniences by attribute name (the default when absent or of another type) */
const char* fmt_bxml_attr_str(const fmt_bxml* d, int node, const char* name);             /* NULL unless a string */
int32_t    fmt_bxml_attr_i32(const fmt_bxml* d, int node, const char* name, int32_t def);  /* int / uint / bool / float (truncated) */
float      fmt_bxml_attr_f32(const fmt_bxml* d, int node, const char* name, float def);

/* path "/Root/Child[2]/@attr" as in _re/formats/bxml.py (1-based index, a repeated tag needs one). Returns the node index
 * (is_attr 0) or the attribute index (is_attr 1); -1 when absent or ambiguous. */
int        fmt_bxml_resolve(const fmt_bxml* d, const char* path, int* is_attr);

/* typed element access on a value (no bounds check beyond `i < count`, which the caller does) */
static inline int32_t  fmt_bx_i32(const fmt_bxml_value* v, uint32_t i) { return fmt_i32(v->data + 4 * (size_t)i); }
static inline uint32_t fmt_bx_u32(const fmt_bxml_value* v, uint32_t i) { return fmt_u32(v->data + 4 * (size_t)i); }
/* float component c of element i, for kinds FLOAT (1 component), VEC2/3/4 and MATRIX */
float      fmt_bx_f32(const fmt_bxml_value* v, uint32_t i, int c);
void       fmt_bx_floats(const fmt_bxml_value* v, uint32_t i, float* out);  /* all components of element i */

#ifdef __cplusplus
}
#endif
#endif
