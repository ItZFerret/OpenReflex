/* build_scene.h -- construction unit "scene" (core/notes/construction.md, unit 4): the entity factory FUN_004046a0 and the entity
 * classes a level's scene creates from its RSData records, and the attachment of the entities to the scene manager.
 *
 * The original reads a subscene through an XMLReader (a BXML chunk in game memory); the core reads the same chunks with the fmt readers
 * (core/src/fmt_scene.h): a `struct fmt_entity` is the Entity node with its RSData. The reader global of the original (DAT_00a9583c: the
 * XMLReader positioned on the RSData being loaded) is `rx_scene_rs()`; every entity vf5 reads its fields from it.
 *
 * Flow (FUN_007d1dc0 .. FUN_007d1ef0 .. FUN_007d1fb0 .. FUN_0080df90):
 *   1. a subscene record of the scene manager is taken (name at +0, entity array +0x104, count +0x108, used flag +0x10c);
 *   2. per Entity node in file order: type id of the RSData namespace (FUN_0080de70) -> factory (FUN_004046a0) -> +0x7c isUiNode,
 *      +0x10 the scene manager, +0x14 the entityId -> vf5 (Entity::vf5 and the class' own fields) -> stored in the array;
 *   3. vf7 of every entity (post-load hook), then per entity FUN_007d2f70 (SceneManager_AddEntity: link into the scene's lists, name
 *      registry, lights, tick list, vf1).
 * Entity layout (Entity base, 0x80 bytes; classes add to it): +0 vtable, +0x10 scene manager, +0x14 entityId, +0x18 name (a
 * ReflexAlignedAlloc'd C string), +0x1c ExportName (bool), +0x20 / +0x24 floats 0, +0x28 / +0x2c flags (sorted lists), +0x30 matrix (16
 * floats, row vectors, translation row at +0x60), +0x70 sort kind (0 = unsorted list), +0x74 flags (8 = AIAvoid), +0x78 sort key
 * (float), +0x7c isUiNode. Classes with an IRenderable base (ModelEntity and its subclasses, InstancedAnimationEntity, ...) have a second
 * vtable at +0x90 (+0x94 0xf0) and, for models, the ModelInstance at +0xa0.
 * Not built (nothing in the race frames reads it; each class documents what): render-only objects, FMOD handles, UI. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
struct fmt_entity;
struct fmt_scene;

/* ---- the reader state (DAT_00a9583c) ---- */
const struct fmt_entity* rx_scene_rs(void);
void                     rx_scene_set_rs(const struct fmt_entity* rs);

/* ---- the factory ---- */
uint32_t rx_scene_type_id(const char* rsdata_namespace);            /* FUN_0080de70: 0 when unknown */
rx_addr  rx_scene_create_entity(uint32_t type_id);                  /* FUN_004046a0: the allocated, constructed entity, or 0 */
/* FUN_0080df90: the entity of one Entity node: factory, +0x7c / +0x10 / +0x14, the class' vf5 (or vf4 for a UI node). 0 when the
 * namespace is unknown (the original stores 0 in the array). */
rx_addr  rx_scene_entity_from_rsdata(rx_addr scene, const struct fmt_entity* rs, int is_ui_node);

/* The factory for classes outside the scene unit: the camera, FX, audio and rider units register their creators (allocation + constructor,
 * returning the entity) for their type ids; the scene unit's own classes and the classes outside the core (render lights and fx, audio,
 * UI, cameras not registered) are handled by the factory itself: the latter leave an empty entity slot. */
typedef rx_addr (*rx_scene_create_fn)(void);
void     rx_scene_register_class(uint32_t id, rx_scene_create_fn fn);

/* ---- constructors other units call ---- */
/* 00401db0 ModelEntity constructor (stack arg: the allocated entity; RET 4; returns the entity): the inlined base of AnimationEntity
 * 0082d040 / CharacterEntity 00839a40 and of the model-carrying entity classes. */
rx_addr  rx_model_entity_construct(rx_addr entity);
/* 007f11f0 ModelInstance constructor (stack arg: the instance, entity + 0xa0; RET 4) */
uint32_t rx_model_instance_construct(rx_addr instance);
/* Entity::vf5 0080ded0 (this = ECX, stack arg: the reader, ignored; RET 4): name, matrix (vf16), ExportName of the current RSData */
uint32_t rx_entity_base_vf5(rx_addr entity, uint32_t reader);

/* 00834de0 CharacterAnim_Init (EAX = node set, stack C; RET 4): the character on a node set (0xe0 bytes of animation state per node, the
 * motion slots); registered here, the riders (0053eab0 -> RiderCharacter_BindMud 00624990) call it */
void     rx_character_anim_init(rx_addr set, rx_addr c);

/* FUN_00772940 (EDI = physics world): the world's forest (the scene's first ForestSystem, type 0x100f) and its trunk collision shapes
 * (FUN_00667cc0); the first step of FUN_00769030 (rx_physics_world_finalize). Call it before the finalize until that does. */
void     rx_physics_world_install_forest(rx_addr world);

/* ---- the scene manager ---- */
/* FUN_007d2f70 (EDI = entity, stack scene): link, name registry, lights, tick list, vf1 */
void     rx_scene_add_entity(rx_addr scene, rx_addr entity);
/* FUN_007d1dc0 (EAX = name, stack scene, reader): loads one subscene of an opened fmt_scene into the scene manager: the entity array of
 * its record, every entity constructed (file order), vf7 of each, then each added to the scene. Returns the record's entity array. */
rx_addr  rx_scene_load_subscene(rx_addr scene, struct fmt_scene* fs, int subscene, const char* name);

#ifdef __cplusplus
}
#endif
