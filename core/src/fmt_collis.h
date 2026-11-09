/* fmt_collis.h -- collision files (type "collis", *.collis) and their placement in a level.
 *
 * Header (24 bytes): u32 1234, 'col\0', u32 version 3, u32 kind, u32 0, u32 count.
 *   kind 22 (0x16)  static triangle chunks: count * { f32 center[3], u32 n, n * 9 f32 (three xyz vertices, chunk-local) }.
 *                   Used by CollisionRSData entities (rocks, hovercraft, markers, vehicle collision hulls). Each chunk
 *                   becomes one CollisionShapeTriangles in the original (_re/notes/alpine-terrain-world-live.md).
 *   kind 15 (0x0F)  primitive shapes (trees: *_H / *_S files; signs, gates, bales, the vehicle parts): u32 pool size at 0x18, the
 *                   name pool (NUL-terminated strings, "Tail\0Handles\0..."; empty for most files), then `count` records
 *                   { u32 type, f32 matrix[16] (row-vector D3D, w column 0 0 0 1), i32 name (byte offset into the pool, -1 none),
 *                   parameters }: type 7 one f32 (76 bytes), type 8 two f32 (80), type 5 three f32 and a u32 (88).
 *                   Probable meaning from the named vehicle files (Ron_BikeCollisionExperiment: Tail 0.5/0.4/2.4, FrontTire 1.0/1.0):
 *                   5 = box (half extents + flags), 8 = wheel / cylinder (radius, half width), 7 = one-radius shape (tree
 *                   trunks / canopies); NOT verified against the original's shape builder. Trees are placed by the forest
 *                   system, not by scene entities. Records are exposed as read.
 *   other kinds     kept raw: kind 2 (vehicle bodies, Editor_Default; the word at 0x14 is a float 1.0), 11 (36 bytes), 16 (24
 *                   bytes) ... `count` is then just the word at 0x14 and means nothing.
 * Entity placement (CollisionEntity::vf28 0x819650, verified bit exact on the 10 Alpine entities): the entity matrix is the
 * scene Matrix with every rotation row scaled by f32(1 / sqrt((x*x + y*y) + z*z)) in float32; a chunk's world position is
 * its center transformed by that matrix as ((x*m0 + y*m4) + z*m8) + t in float32; the triangles stay chunk-local. The
 * orientation quaternion is the original routine 0x401210 (not reproduced here: see fmt_collis_quat_approx). */
#ifndef FMT_COLLIS_H
#define FMT_COLLIS_H

#include "fmt_scene.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fmt_collis_chunk {
    float    center[3];
    uint32_t count;
    float*   tris;                 /* count * 9 floats */
} fmt_collis_chunk;

typedef struct fmt_collis_shape {   /* kind 15 */
    uint32_t type;
    float    matrix[16];
    int32_t  link;                 /* name offset into the pool, -1 none */
    const char* name;              /* the pool string at `link`, or NULL */
    float    param[3];             /* type 7: 1 used, type 8: 2, type 5: 3 (unused ones 0) */
    uint32_t tail;                 /* type 5 only */
} fmt_collis_shape;

typedef struct fmt_collis {
    uint32_t version, kind, count;
    uint32_t pool_size;            /* kind 15: the name pool bytes (the word at 0x18) */
    const char* pool;              /* kind 15: the pool (into raw) */
    fmt_collis_chunk* chunks;      /* kind 22: count entries */
    fmt_collis_shape* shapes;      /* kind 15: count entries */
    uint8_t* raw; size_t raw_size; /* a copy of the file (always) */
} fmt_collis;

fmt_collis* fmt_collis_parse(const uint8_t* data, size_t size);
void        fmt_collis_free(fmt_collis* c);
uint32_t    fmt_collis_triangle_count(const fmt_collis* c);

/* Entity matrix from the scene matrix (float32 exact, see above). */
void        fmt_collis_entity_matrix(const float scene[16], float out[16]);
/* p * M with the original's float32 operation order. */
void        fmt_collis_transform_point(const float p[3], const float m[16], float out[3]);
/* Rotation rows of m as a quaternion (x, y, z, w), Shepperd's method in double: within an ulp of the original 0x401210, NOT bit exact. */
void        fmt_collis_quat_approx(const float m[16], float q[4]);
/* The element the original builds for chunk k of an entity: position (x, y, z) = transformed center. */
void        fmt_collis_chunk_position(const fmt_collis* c, int k, const float entity_matrix[16], float pos[3]);
/* A triangle widened the way the original stores it: three vec4 (x, y, z, w = z). */
void        fmt_collis_widen_triangle(const float t[9], float out[12]);

/* ---- CollisionRSData placements of a level ---- */
typedef struct fmt_collis_placement {
    int         subscene;
    uint32_t    entity_id;
    const char* name;              /* the entity's Name field */
    const char* asset;             /* FemaleCollisionAsset */
    float       scene_matrix[16];
    float       matrix[16];        /* the entity matrix */
    fmt_collis* collis;            /* owned by the set; NULL if the asset could not be read (see error) */
} fmt_collis_placement;

typedef struct fmt_collis_set {
    fmt_collis_placement* items; int count;
} fmt_collis_set;

/* Every CollisionRSData entity of the named subscenes (in the order given, entities in file order) with its collis loaded. */
int         fmt_collis_load_placements(fmt_level* lv, const char* const* subscenes, int nsub, fmt_collis_set* out);
void        fmt_collis_set_free(fmt_collis_set* s);

#ifdef __cplusplus
}
#endif
#endif
