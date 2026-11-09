/* build_world.h -- construction unit "world" (core/notes/construction.md, unit 3): the physics world ("physics context", the
 * object the race frames call `world`: core/game/world.h, layout core/notes/world.md) and its broadphase grids, the global
 * registries the construction keeps (physics worlds, collision shapes), and the MSVC 9 containers the constructors use.
 *
 * The physics world (0xcce0 bytes, one _aligned_malloc block) is made by FUN_0075da90(scene) from FUN_00759f00 (the event's
 * world creation, MXWorld + 0x20134 = it); its constructor FUN_00768830(world, scene) builds:
 *   +0x1900 contact event count, +0x1910 byte: "not the front end" (the owning MXWorld's +0x20018 == 0),
 *   +0x1914 listeners {data (5 pointers), count, capacity 5, growth 5, max 0x7fffffff} (the game's small growable array),
 *   +0x1928 contact handlers (std::vector of pointers, its proxy block at +0x1928), +0x1940 simulations {data, n, 5, 5, max},
 *   four object vectors (std::vector: +0 proxy, +0xc begin, +0x10 end, +0x14 capacity end) each followed by a broadphase
 *   cursor (+0x18 the cursor block, +0x1c the world):
 *     +0x1954 generic dynamic objects (0xe0 bytes each; reserve 300 / front end 15)   cursor +0x196c (grid 30x30, 300, 250, 250)
 *     +0x1974 rider bodies (0x80 bytes; reserve 200 / 10)                              cursor +0x198c (grid 30x30, 200, 20, 20)
 *     +0x1994 keyed objects (0xe0 bytes; reserve 2000 / 150)                           cursor +0x19ac (grid 30x30, 2000, 40, 200)
 *     +0x19b4 static objects (0x40 bytes; reserve 20000 / 100)                         cursor +0x19cc (grid 30x30, 20000, 500, 200)
 *   +0x19d4 active kinematic bodies (count), +0x9a00 200 water states (0x20: 0, the runtime vector [0x00d616a0]),
 *   +0xb300 200 contact-sound timers (0x20: 1.0f), +0xcc00 the contact-handler list (std::vector, reserve 200) and +0xcc18 0,
 *   +0xcc1c four std::vectors of 12-byte records (the first three reserve 1000), +0xcc7c a proxy, +0xcc94 a std::map head node
 *   (0x18 bytes) and +0xcc98 its size, +0xcc9c the terrain (0 until a terrain entity registers), +0xcca0 0, +0xcca4 the scene,
 *   +0xcca8 0, +0xccac the physics property id of "Terrain", +0xccb0 the embedded CollisionShapeHFTerrain (0x24 bytes: base
 *   CollisionShape with type 0x200, flags 0x301 / 4, +0x20 = the terrain list 0x009e30d4), and registers itself in the global
 *   vector of physics worlds [0x00db460c] (FUN_00773cd0 / FUN_006b7590).
 * Broadphase cursor (0x40 bytes, FUN_0084a0d0): +0 0, +0x10 / +0x20 the runtime zero vector [0x00d61670], +0x30 the grid.
 * Broadphase grid (0x480 bytes, FUN_00882370; queries in core/game/phys_coll_grid.h): +0x400 0, +0x404 / +0x408 cells x / y,
 *   +0x40c capacity (objects), +0x410 cells, +0x414 per-cell capacity, +0x418 overflow capacity, +0x41c / +0x420 the boxes'
 *   min / max (capacity x 16 bytes), +0x430..+0x46f four zero vectors, +0x470 the cell lists (cells x per-cell x 4), +0x474 the
 *   cell counts (cells x 4, zeroed), +0x478 0, +0x47c the overflow list (overflow x 4). The index table +0..+0x3ff is not written.
 * Allocation order is the original's (every block in the same sequence; see core/notes/construction.md unit 3).
 * Not ported: the physics worker thread the constructor starts when [0x00a946c4] is set (never in the reference events). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ---- MSVC 9 containers over game memory (the game's std::vector: +0 proxy, +4 / +8 unused, +0xc begin, +0x10 end, +0x14 end
 * of capacity; the allocator is ReflexAlignedAlloc) ---- */
/* the container proxy (FUN_007ce6a0, EDI = vector): _aligned_malloc(4, 4) holding the container's address, stored at its +0 */
void     rx_vec_proxy_init(rx_addr vec);
/* the same proxy made by ReflexAlignedAlloc(4) (the constructors' inline form) */
void     rx_vec_proxy_init16(rx_addr vec);
/* push_back of a 4-byte element (FUN_00849660 / FUN_006b7590 = vector<T*>::push_back, growth by FUN_00418060: capacity * 1.5,
 * at least size + 1, ReflexAlignedAlloc(capacity * 4), old elements copied, old block freed) */
void     rx_vec_push_u32(rx_addr vec, uint32_t value);
void     rx_vec_reserve_empty(rx_addr vec, uint32_t n, uint32_t elem, int kind);   /* reserve(n) of an empty vector: kind 0 _aligned_malloc(16), 1 ReflexAlignedAlloc */
void     rx_vec_push_back_ptr(rx_addr vec, rx_addr value_ptr);   /* 00849660 itself (EDI = vector, s0 = &value) */
uint32_t rx_vec_size(rx_addr vec, uint32_t elem);   /* (end - begin) / elem */

/* ---- the global registries ---- */
#define RX_PHYS_WORLDS   0x00db460cu   /* std::vector of physics worlds (static guard bit 1 of [0x00dcbe54]) */
#define RX_SHAPE_LIST    0x00d6d8e0u   /* std::vector of every CollisionShape (FUN_00844110), lock 0x00d6d8f8 */
#define RX_TERRAIN_LIST  0x009e30d4u   /* the terrain list: two inline entries, +8 count (static guard bit 1 of [0x009e30e0]) */
void     rx_phys_worlds_init(void);                    /* 00773cd0: the vector of physics worlds (once, guard 0x00dcbe54) */
rx_addr  rx_phys_world_of_scene(rx_addr scene);        /* 007731f0: the physics world whose +0xcca4 is the scene, or 0 */

/* ---- CollisionShape base ---- */
/* 00844110 (ESI = shape, ECX = type): vtable CollisionShape 0x00976584, +4 0, +8 type, +0xc 1, +0x10 1, +0x14..+0x1c 0;
 * appended to the shape list [0x00d6d8e0] */
void     rx_collision_shape_construct(rx_addr shape, uint32_t type);

/* ---- broadphase ---- */
rx_addr  rx_bp_grid_construct(rx_addr grid, uint32_t nx, uint32_t ny, uint32_t capacity, uint32_t per_cell, uint32_t overflow);
                                                       /* 00882370 (ESI = grid, EAX = nx, ECX = ny, s0..s2; RET 0xc) */
rx_addr  rx_bp_cursor_construct(rx_addr cur, uint32_t nx, uint32_t ny, uint32_t capacity, uint32_t per_cell, uint32_t overflow);
                                                       /* 0084a0d0 (EDI = cursor; RET 0x14) -> EAX = cursor */
void     rx_bp_cursor_create(rx_addr owner, uint32_t nx, uint32_t ny, uint32_t capacity, uint32_t per_cell, uint32_t overflow);
                                                       /* 00773950 (ESI = owner vector; RET 0x14): [owner + 0x18] = new cursor */

/* ---- physics properties (the "PhysicsProperties" registry at 0x00da6ad0) ---- */
#define RX_PHYS_PROPS    0x00da6ad0u
/* FUN_00850bf0 (ECX = registry, EAX = &std::string): the id of a property name (the std::map<string, id> at registry + 0x30,
 * StringMap_Find 00702ad0 / its lower bound 008519a0); 0 when absent. The name is a C string here (the original builds a
 * transient std::string; the comparison is std::operator< of basic_string: bytes, then length). */
uint32_t rx_phys_props_find(rx_addr registry, const char* name);
/* the registry (0x00da6ad0: +0 proxy, +0xc materials (0xd0: name, density, linear / angular damping, reset time, disable time,
 * min disable speed^2, min disable angular rate^2, wake-up speed^2, max depenetration; each a 4-lane splat), +0x18 proxy, +0x24
 * contacts (12 bytes: restitution, friction, spring constant; triangular index rx_phys_props_contact_index), +0x30 the map name ->
 * id, +0x50 a second (empty) map), built once under the static guard bit 1 of [0x00dce7d4] (FUN_00850010 / 008501a0) from the
 * EngineDependencies BXML "PhysicsProperties" (read through fmt_bxml: rx_phys_props_set_install must have been called). */
struct fmt_install;
void     rx_phys_props_set_install(struct fmt_install* in);
struct fmt_install* rx_phys_props_install(void);                 /* the install set above (NULL before) */
rx_addr  rx_phys_props_get(void);
uint32_t rx_phys_props_contact_index(rx_addr registry, int32_t a, int32_t b);   /* 0084fe70 (ESI = registry; RET 8) */

/* ---- the physics world ---- */
rx_addr  rx_physics_world_construct(rx_addr world, rx_addr scene);   /* 00768830 (stdcall world, scene; RET 8) -> EAX = world */
rx_addr  rx_physics_world_create(rx_addr scene);                     /* 0075da90 (cdecl scene): _aligned_malloc(0xcce0, 16) + ctor */

/* ---- after the scene's objects are in (FUN_00769030 from 00556d65): the four grids' extents from the keyed and static objects'
 * boxes (FUN_007739c0 / 00773b50 over (1e6, -1e6) starts), then every grid rebuilt over them (FUN_00769370: 00882630 sets
 * +0x430 min, +0x440 max, +0x450 cell size, +0x460 its reciprocal from [0x00d61650]; 00882cf0 empties the cells; every object
 * re-inserted). The world's +0xcc9c (the scene's ForestSystem entity, type 0x100f, FUN_00772940 -> the forest's collision
 * FUN_00667cc0) belongs to the forest unit: `forest` is that entity or 0 (not set here when 0). ---- */
void     rx_bp_grid_clear(rx_addr grid);                                          /* 00882cf0 (EAX = grid) */
void     rx_bp_grid_set_extent(rx_addr grid, rx_addr mn, rx_addr mx);             /* 00882630 (stdcall grid, min, max; RET 0xc) */
void     rx_physics_world_finalize(rx_addr world, rx_addr forest);              /* 00769030 (stdcall world; RET 4) */

#ifdef __cplusplus
}
#endif
