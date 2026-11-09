/* race_snake_trail.h -- the Snake party mode's trails (race_snake_trail.c): each SnakeDriver's trail object (0x150 bytes at the driver's
 * +0xf14: its segment pools, its XenonSnakeVisibleMesh at +0x10 with the CPU vertex array) and the activity's spatial hash of the trail
 * segments (MXActivitySnake +0xff070) that the collision test (0069bba0) walks. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void     rx_snake_hash_init(rx_addr hash);                                   /* 006a21d0 (EAX = the hash: MXActivitySnake + 0xff070) */
void     rx_snake_hash_clear(rx_addr hash);                                  /* 006a26b0 (EDX = the hash) */
void     rx_snake_mesh_statics_load(void);                                   /* 006a0cd0: the trail meshes' vertex buffers and declaration */
void     rx_snake_mesh_shaders_load(void);                                   /* 006a0c70: the trail meshes' shaders ("SnakeShader") */
void     rx_snake_mesh_statics_release(void);                                /* 006a0d50 */
rx_addr  rx_snake_trail_construct(rx_addr obj, rx_addr scene, rx_addr color, uint32_t id, rx_addr hash);   /* 0069e3a0 (ECX = id, EAX = hash; stdcall obj, scene, colour; RET 0xc) */
void     rx_snake_trail_destroy(rx_addr trail);                              /* 0069e460 (EDI = trail) */
void     rx_snake_trail_reset(rx_addr trail);                                /* 0069ec90 (EAX = trail) */
void     rx_snake_trail_refresh(rx_addr trail);                              /* 0069e360 (EAX = trail) */
void     rx_snake_trail_update(rx_addr matrix, rx_addr trail, float back, float width, float height);   /* 0069e4c0 (EAX = the vehicle's matrix; stdcall trail, back offset, width scale, height; RET 0x10) */
rx_addr  rx_snake_hash_hit(rx_addr hash, rx_addr pos, float radius, uint32_t id, uint32_t age);   /* 0069bba0 (stdcall; RET 0x14) -> the collision record hit or 0 */

/* the internals (race_snake_trail.c; registered for the oracle) */
rx_addr  rx_snake_hash_pool_init(rx_addr pool);                              /* 006a2650 (ESI) */
rx_addr  rx_snake_segment_pool_init(rx_addr pool);                           /* 006a24f0 (ESI) */
rx_addr  rx_snake_chain_pool_init(rx_addr pool);                             /* 006a25a0 (ESI) */
rx_addr  rx_snake_chain_take(rx_addr pool);                                  /* 006a2600 (EDX) */
rx_addr  rx_snake_segment_take(rx_addr pool);                                /* 006a2550 (EDX) */
uint32_t rx_snake_hash_key(rx_addr key);                                     /* 0069b8e0 (ECX) */
rx_addr  rx_snake_hash_find(rx_addr key, rx_addr hash);                      /* 006a24a0 (ESI = key, EDI = hash) */
rx_addr  rx_snake_hash_insert(rx_addr key, rx_addr hash);                    /* 006a2280 (EAX = key, ESI = hash) */
void     rx_snake_hash_remove(rx_addr key, rx_addr hash);                    /* 006a23b0 (EAX = key, ESI = hash) */
rx_addr  rx_snake_cell(rx_addr pos, rx_addr out);                            /* 0069b9b0 (EDI = pos, ESI = out) */
void     rx_snake_hash_add(rx_addr hash, rx_addr rec);                       /* 0069bab0 (stdcall; RET 8) */
void     rx_snake_hash_del(rx_addr hash, rx_addr rec);                       /* 0069bb20 (stdcall; RET 8) */
void     rx_snake_shape_side(rx_addr buf, float v);                          /* 0069bcf0 (cdecl) */
void     rx_snake_shape_cap(rx_addr buf, float v);                           /* 0069bf00 (cdecl) */
void     rx_snake_shape_outline(rx_addr buf, float v);                       /* 0069c000 (cdecl) */
uint32_t rx_snake_mesh_cap_front(rx_addr mesh, rx_addr shape, rx_addr pts, float w);   /* 006a0e70 (ESI = mesh, EDI = shape; stdcall pts, w; RET 8) -> AL */
uint32_t rx_snake_mesh_cap_end(rx_addr mesh, rx_addr shape, rx_addr pts, float w);     /* 006a1030 (same) -> AL */
uint32_t rx_snake_mesh_cap_tail(rx_addr mesh, rx_addr shape, rx_addr pts, float w);    /* 006a1510 (same) -> AL */
uint32_t rx_snake_mesh_sides(rx_addr mesh, rx_addr p0, rx_addr shape, float w0, rx_addr p1, float w1);   /* 006a11f0 (ESI = mesh, EDI = p0; stdcall shape, w0, p1, w1; RET 0x10) -> AL */
void     rx_snake_mesh_uncap_head(rx_addr mesh, rx_addr shape);              /* 006a14e0 (ECX = mesh, EDX = shape) */
void     rx_snake_mesh_drop_tail(rx_addr mesh, rx_addr shape);               /* 006a16c0 (EAX = mesh, ECX = shape) */
void     rx_snake_mesh_drop_sides(rx_addr mesh, rx_addr shape);              /* 006a1700 (EAX = mesh, ECX = shape) */
void     rx_snake_quad_planes(rx_addr c, rx_addr d, rx_addr q, rx_addr p, rx_addr out_b, rx_addr out_a);   /* 0069b530 (ECX = c, EDX = d, EDI = q, ESI = out A; stdcall p, out B; RET 8) */
rx_addr  rx_snake_record_build(rx_addr plane_new, rx_addr plane_prev, rx_addr out, uint32_t id, uint32_t counter, rx_addr mid, rx_addr shape, rx_addr outline_prev, rx_addr outline_new);   /* 0069b450 (ECX, EDX; stdcall 7; RET 0x1c) */
void     rx_snake_record_copy(rx_addr src, rx_addr dst);                     /* 0069fb00 (ECX = src, EAX = dst) */
void     rx_snake_trail_free_oldest(rx_addr trail);                          /* 006a0470 (ECX) */
void     rx_snake_trail_close(rx_addr trail);                                /* 0069fcd0 (stdcall; RET 4) */
void     rx_snake_trail_append(rx_addr up, rx_addr trail, rx_addr pos, rx_addr side, float width, float height);   /* 0069ed90 (ECX = up; stdcall; RET 0x14) */
void     rx_snake_trail_new_chain(rx_addr trail);                            /* 0069ed10 (ESI) */
rx_addr  rx_snake_mesh_construct(rx_addr mesh, uint32_t id, rx_addr color); /* 006a0b40 (EAX = colour; stdcall mesh, id; RET 8) */
void     rx_snake_mesh_destroy(rx_addr mesh);                                /* 006a0db0 (ESI) */
rx_addr  rx_snake_mesh_sdtor(rx_addr mesh, uint32_t flags);                  /* 006a0c50 slot 0 (RET 4) */
rx_addr  rx_snake_mesh_sdtor_o90(rx_addr r, uint32_t flags);                 /* 006a2770 IRenderable slot 0 (ECX = mesh + 0x90) */
void     rx_gpu_release_record(rx_addr out);                                 /* 007b0ee0 (cdecl) */

/* Port addition (no original counterpart): the trail's segments, oldest chain first and each chain oldest segment first, for the
 * host's renderer, read only. Writes min(count, max) records of RX_SNAKE_TRAIL_SEG_FLOATS floats to out (out may be NULL) and returns
 * the count. Record: [0..2] the position (segment +0x10), [3..5] the side axis (+0x20),
 * [6..8] the up axis (+0x30), [9] the width scale (+0x40), [10] the height (+0x44), [11] the distance laid at it (+0x48: the texture's
 * v), [12] the chain index (0 = the oldest; consecutive records of one chain are joined by wall, a new index starts a new piece),
 * [13..15] 0. The original's wall cross section (shape 0x9308cc) spans side * [-0.1, 0.1] * width scale
 * and up * [0, 1] * height (a wall 0.2 * scale thick). */
#define RX_SNAKE_TRAIL_SEG_FLOATS 16u
uint32_t rx_snake_trail_segments(rx_addr trail, float* out, uint32_t max);
#ifdef __cplusplus
}
#endif
