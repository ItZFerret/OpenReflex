/* build_collision.h -- construction unit "collision" (core/notes/construction.md, unit 5): the static collision of the scene's
 * CollisionRSData entities: CollisionEntity::vf5's collision fields 00819130, CollisionEntity::vf28 00819650 (the entity's static
 * objects in the physics world), the shapes built from the CollisionAsset geometries (unit 2c, build_collis.h), the physics world's
 * static object vector and broadphase grid (build_world.h).
 *
 * CollisionEntity (vtables 0x00974f84 / 0x00974ffc at +0x90, 0x1a0 bytes): Entity / ModelEntity base (+0x10 scene, +0x30 matrix,
 * +0x74 flags (8 = AIAvoid), +0xa0 the model instance), +0xc0 the scene matrix (copied before the rows of +0x30 are normalised),
 * +0x140 MaleCollisionAsset handle, +0x144 FemaleCollisionAsset handle (the collision), +0x148 0, +0x14c CollisionAudioMaterial /
 * hardness, +0x150 audio hardness (5; 0 for soft collision; 10 max), +0x154 0, +0x158 sound zone / drive-on material (0..7), +0x15c
 * StaticObjectHandle (vtable 0x0096ddb8; +0x160 its shared record {refcount, physics world, static index}), +0x164 std::vector of
 * StaticObjectHandles (8 bytes: vtable, record) for the triangle groups, +0x17c std::vector of their matrices (0x40), +0x194 byte 0,
 * +0x198 the PhysicsType property id, +0x19c byte DriveOn.
 * Static object (0x40 bytes, physics world + 0x19c0 vector): +0 position (x, y, z, z), +0x10 orientation quaternion (00401210 of the
 * matrix), +0x20 the physics property id, +0x24 CollisionShapeHandle (vtable 0x009765a8, +0x28 the shape), +0x2c the handle record,
 * +0x30 the CollisionEntity, +0x34 0, +0x38 byte 1.
 * Shapes (base 00844110: +4 references, +8 type, +0xc flags, +0x10 flags (|= 2 static, = 4 drive-on), +0x18 / +0x1c the collis
 * handle): Triangles (0x10, vtable 0x00976608: +0x20 count, +0x24 triangles (3 vec4 each), +0x30 radius splat), StaticBox (4,
 * 0x009766bc: +0x20 half extents, +0x30 .., +0x40..+0x6f the box's axes), Sphere (2, 0x00976674: +0x20 radius splat), StaticCylinder
 * (8, 0x00976728: +0x20 radius, +0x24 half height, +0x30 class 1 / 2 / 3, +0x40.. the class's vectors), StaticComposite (0x8000,
 * 0x009767a0, 0xb0: +0x20 children vector, +0x38 rotations, +0x50 offsets, +0x68 offsets again, +0x80 names, +0x98 ids), Unknown
 * (1, 0x009765c0: +0x20 a radius splat, 1.0 or 2.0). Shapes are shared between entities of the same collis asset (8445e0's search of
 * the shape list by +0x1c). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
struct fmt_entity;

void rx_modtrack_add_collision(rx_addr world);

/* shapes */
rx_addr rx_shape_triangles_construct(rx_addr shape, rx_addr tris);         /* 008449d0 (ECX = shape, EDI = triangle group) */
rx_addr rx_shape_unknown_construct(rx_addr shape, float radius);           /* 00844930 (ECX = shape; RET 4) */
rx_addr rx_shape_static_box_construct(rx_addr shape, rx_addr geo);         /* 00846320 (ECX = shape, EDI = kind 11 geometry) */
rx_addr rx_shape_sphere_construct(rx_addr shape, rx_addr geo);             /* 008453f0 (ECX = shape, EDI = kind 16 geometry or 0) */
rx_addr rx_shape_static_cylinder_construct(rx_addr shape, rx_addr geo);    /* 00847790 (EAX = shape, EDI = kind 17 geometry) */
rx_addr rx_shape_static_composite_construct(rx_addr shape, rx_addr geo);   /* 00848cd0 (stdcall shape, kind 15 geometry; RET 8) */
rx_addr rx_shape_from_handle(rx_addr handle);                              /* 008446e0 (ESI = collis handle) */
rx_addr rx_shape_handle_get(rx_addr out, rx_addr handle, int share);       /* 008445e0 (cdecl): CollisionShapeHandle {vt, shape} */
/* bounds (thiscall q*, pos*, min*, max*; RET 0x10) */
void    rx_shape_triangles_bounds(rx_addr self, rx_addr q, rx_addr pos, rx_addr mn, rx_addr mx);          /* 00844d30 */

/* static objects of a physics world */
void    rx_static_record_init(rx_addr rec, uint32_t material, rx_addr matrix);   /* 0084fc20 (EDI = record; RET 8) */
void    rx_static_vector_push(rx_addr vec, rx_addr rec);            /* 00774a90 (EAX = world + 0x19b4; RET 4) via 00777310 */
void    rx_static_broadphase_insert(rx_addr vec, uint32_t index);   /* 00774850 (ECX = world + 0x19b4, EDI = index) */
rx_addr rx_static_add_shape(rx_addr world, rx_addr out, rx_addr matrix, rx_addr shape_handle, int drive_on, uint32_t material);
                                                                    /* 00769550 (ECX = material; RET 0x14) -> out */
rx_addr rx_static_add_asset(rx_addr world, rx_addr out, rx_addr matrix, int drive_on, int share, rx_addr collis_handle,
                            uint32_t material);                    /* 007693c0 (ECX = handle, EDX = material; RET 0x14) */

/* the CollisionEntity */
void    rx_collision_entity_load(rx_addr entity, const struct fmt_entity* rs);   /* 00819130 after ModelInstance_Init / Entity::vf5 */
void    rx_collision_entity_add_to_world(rx_addr entity, rx_addr world);         /* vf28 00819650 (thiscall world; RET 4) */
void    rx_collision_entity_set_drive_on(rx_addr entity);                        /* 00819510 (stdcall entity) */
/* TEMP until scene unit: the CollisionEntity constructor 00818f60 with the ModelEntity base 00401db0 (its model instance at +0xa0,
 * FUN_007f11f0, is not built) */
rx_addr rx_collision_entity_construct_temp(rx_addr entity);

#ifdef __cplusplus
}
#endif
