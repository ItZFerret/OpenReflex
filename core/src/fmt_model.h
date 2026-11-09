/* fmt_model.h -- model (type "model", magic 0x88AABB00 v4) and surface (type "surface", magic 0x99BBCC00 v4) resources, as
 * far as the core needs them: node hierarchies and bind matrices of models, vertex / index data of surfaces (physics
 * and collision use; drawing stays with Unity's readers).
 *
 * Model (little endian, matrices row-major D3D with row vectors, world = local * parentWorld):
 *     u32 0x88AABB00, u32 4, u32 flags (0|1), u32 node count, u32 name pool bytes, u32 LOD count, u32 material pool bytes,
 *     u32 binding count, u32 surface reference count, u32 0; char name[32] at 40; ... header ends at 172;
 *     node count * 64   local matrices
 *     node count * 44   { u32 name offset, u32 parent (0xFFFFFFFF = root, else an earlier node), f32 extra[9] }
 *     names pool, LODs { u32 binding count, u32 first binding, u32 distance }, bindings { u32 material name offset,
 *     u32 surface, u32 node, u32 a, u32 b } (a, b are runtime pointers in some models), material name pool,
 *     surface reference flags (bytes, 0) and the identity reference map (u32 each).
 *   The model and its surface resource share a name: binding.surface indexes the surface array.
 * Surface: u32 0x99BBCC00, u32 4, u32 count, count * 352 byte headers { 0, 0, 0xFFFFFFFF, flags (0 static, 0x40003 skinned),
 *   primitive count, vertex count, index bytes, ... 20 vertex declaration elements at +28 { u16 stream, u16 offset, u8 type,
 *   u8 method, u8 usage, u8 index } ended by stream 255, vertex bytes at +192, bone palette bytes at +256 }, u32 0, then per
 *   surface: palette bytes, 16-bit triangle strip, vertex buffer. FLOAT16 positions / UVs, FLOAT3 normals. */
#ifndef FMT_MODEL_H
#define FMT_MODEL_H

#include "fmt_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fmt_model_node {
    const char* name;
    int         parent;           /* -1 root */
    float       local[16];
    float       world[16];        /* local * parent world (world64 rounded to float) */
    double      world64[16];      /* the same accumulated in double down the hierarchy (what the Python reference computes) */
    float       extra[9];         /* the record's nine unexplained floats, as stored */
} fmt_model_node;

typedef struct fmt_model_lod { uint32_t count, first, distance; } fmt_model_lod;
typedef struct fmt_model_binding { const char* material; uint32_t surface, node, a, b; } fmt_model_binding;

typedef struct fmt_model {
    char name[33];
    uint32_t flags;
    fmt_model_node* nodes; int nnodes;
    fmt_model_lod* lods; int nlods;
    fmt_model_binding* bindings; int nbindings;
    int surface_refs;
    uint8_t* storage;             /* internal: the resource copy that owns the strings */
} fmt_model;

fmt_model* fmt_model_parse(const uint8_t* data, size_t size);
void       fmt_model_free(fmt_model* m);
int        fmt_model_find_node(const fmt_model* m, const char* name);     /* case-insensitive, -1 if absent */

enum { FMT_SURF_FALLBACK_UP = 1,      /* an unrepairable non-finite normal becomes (0, 1, 0) instead of an error */
       FMT_SURF_REPAIR_NONUNIT = 2 }; /* normals that are not unit length (|n.n - 1| > 2e-3) are repaired like non-finite ones */

typedef struct fmt_surface {
    int      skinned;
    uint32_t flags;
    int      nverts;
    float*   pos;                 /* 3 per vertex */
    float*   nrm;                 /* 3 per vertex (repaired) */
    float*   uv0; float* uv1;     /* 2 per vertex, uv1 zero when not declared */
    uint8_t* color;               /* 4 per vertex RGBA (from D3DCOLOR), or NULL */
    float*   tangent;             /* 3 per vertex or NULL */
    uint8_t  palette[256]; int npalette;
    int*     bones;               /* 4 per vertex: model node through the palette, -1 for a zero weight; skinned only */
    float*   weights;             /* 4 per vertex (byte / 255) */
    int      nstrip; uint16_t* strip;
    int      ntris; uint32_t* tris;   /* 3 per triangle: the strip as D3D order triangles, degenerates dropped */
    int      repaired;            /* normals repaired */
} fmt_surface;

typedef struct fmt_surfaces { fmt_surface* s; int count; } fmt_surfaces;

int  fmt_surfaces_parse(const uint8_t* data, size_t size, int flags, fmt_surfaces* out);   /* 0 / -1 */
void fmt_surfaces_free(fmt_surfaces* s);

#ifdef __cplusplus
}
#endif
#endif
