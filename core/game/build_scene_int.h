/* build_scene_int.h -- internal to the scene unit (build_scene*.c): the RSData readers (the small helpers 0x0080e180 .. 0x0080f370 of
 * the original, all working on the reader global DAT_00a9583c = the entity's RSData), the Entity base, the class constructors the
 * factory calls, the virtual-call helper. */
#pragma once
#include "build_scene.h"
#include "build_alloc.h"
#include "build_terrain.h"
#include "build_world.h"
#include "build_collision.h"
#include "build_model.h"
#include "../src/fmt_scene.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* RSData value kinds: the tag table at 0x009b73e8 (FUN_0080e180's second argument) */
enum { RS_STRING = 1, RS_ENUM = 2, RS_ENUMVALUE = 3, RS_BOOL = 4, RS_INT = 5, RS_UINT = 6, RS_FLOAT = 7, RS_COLOR = 8, RS_MATRIX = 9,
       RS_VEC3 = 10, RS_VEC4 = 11, RS_NODEID = 12, RS_ASSET = 13 };

/* x87 fld / fstp dword copy: a signalling NaN comes out quiet */
static inline uint32_t scene_x87c(uint32_t b) {
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu) && !(b & 0x00400000u)) b |= 0x00400000u;
    return b;
}
rx_addr rx_trigger_create(void);
rx_addr rx_trigger_construct(rx_addr e);

/* ---- RSData readers (the entity being loaded is rx_scene_rs()) ---- */
int         rs_has(const char* field, int kind);           /* FUN_0080e180: the field has a value child of that kind */
int         rs_field(const char* field);                   /* FUN_0080e1f0: the Field node or -1 */
const char* rs_str(const char* field);                     /* FUN_0080e320: the String value, "" when absent */
uint32_t    rs_bool(const char* field);                    /* FUN_0080e540: the Bool (0 / 1), 0 when absent */
int32_t     rs_int(const char* field);                     /* FUN_0080e5b0: the Int, 0 when absent */
uint32_t    rs_uint(const char* field);                    /* FUN_0080e620: the UInt, 0 when absent */
int32_t     rs_enum(const char* field);                    /* FUN_0080e4d0: the EnumValue, 0 when absent */
const char* rs_enum_text(const char* field);               /* FUN_0080e400: the Enum text (NULL when absent) */
float       rs_float(const char* field);                   /* FUN_0080e850: the Float, 0 when absent */
int         rs_matrix(const char* field, float out[16]);   /* the Matrix value of a field (FUN_0080ea30 reads "Matrix"); 0 when absent */
int         rs_vec3(const char* field, float out[3]);      /* FUN_0080eab0: the Vector3, returns 1 when present */
int         rs_vec4(const char* field, float out[4]);      /* the Vector4 / Color value (4 floats), returns 1 when present */
rx_addr     rs_asset(const char* field);                   /* FUN_0080ec70: the handle of the Asset value (type by its "type" attribute) */

void     scene_rs_color_to(const char* field, rx_addr dest);   /* FUN_0080e940 (EDI = dest): a Color into 4 floats, version <= 1: x 1/255 */
void     scene_list_append(rx_addr list, uint32_t v);          /* the game's small growable array {data, count, capacity, growth, max} */
void     scene_rs_float_list(const char* field, rx_addr list);   /* FUN_0080f020 */
void     scene_rs_color_list(const char* field, rx_addr list);   /* FUN_0080f130 (field name is read from the argument; the original passes "LensFlareColorList") */
void     scene_rs_vec3_list(const char* field, rx_addr list);    /* FUN_0080f270 (EAX = list of 16-byte elements): the Vector3 children of the field's ListV3 */
void     scene_rs_asset_list(const char* field, rx_addr list); /* FUN_0080ed50: the Asset handles of the field's RSData children */

/* ---- raw access to the game objects ---- */
void     scene_copy_words(rx_addr dst, rx_addr src, uint32_t n);          /* n 32-bit words (rep movsd) */
void     scene_set_matrix_identity(rx_addr dst);                            /* the identity from .data 0x009cb510 */

/* ---- the Entity base ---- */
/* the constructor inlined in every class constructor (e.g. AnchorEntity 00403e90) up to the vtable store */
void     rx_entity_construct(rx_addr e, uint32_t vtable);
/* Entity::vf5 0080ded0 (ECX = e): name, matrix through vf16, ExportName */
void     rx_entity_vf5(rx_addr e);
/* a virtual call of slot `off` (byte offset) of the object's vtable: the core function when registered, else the original (oracle) */
uint32_t scene_vcall(rx_addr obj, uint32_t off, int nargs, const uint32_t* args);

/* ---- class constructors (the factory's allocation + constructor; build_scene_simple.c) ---- */
rx_addr rx_anchor_entity_create(void);
rx_addr rx_general_storage_create(void);
rx_addr rx_distance_fog_create(void);
rx_addr rx_shadow_map_create(void);
rx_addr rx_hdr_settings_create(void);
rx_addr rx_overlay_color_create(void);
rx_addr rx_mud_manager_create(void);
rx_addr rx_tdf_entity_create(void);
rx_addr rx_level_info_create(void);
rx_addr rx_music_track_list_create(void);
rx_addr rx_model_entity_create(void);
rx_addr rx_massive_ad_create(void);
rx_addr rx_sky_create(void);
rx_addr rx_reflection_map_gen_create(void);
rx_addr rx_lens_flare_create(void);
rx_addr rx_hfbump_create(void);
rx_addr rx_collision_entity_create(void);
rx_addr rx_anim_single_loop_create(void);
rx_addr rx_anim_single_loop_particles_create(void); /*0x1058:006ba2e0,0xa60*/
rx_addr rx_instanced_anim_create(void);
uint32_t scene_light_list_add(rx_addr list, rx_addr rec);   /* 007f9e70 (EAX = list, ESI = record; 1 = added, 0 = already in the list) */
/* 007f9e70 with the original's frames: e = its return slot, the caller's EBX / EBP / EDI */
uint32_t scene_light_list_add_fr(rx_addr e, uint32_t ebx, uint32_t ebp, uint32_t edi, rx_addr list, rx_addr rec);
rx_addr rx_anim_spline_loop_create(void);
rx_addr rx_anim_spline_particles_create(void);
rx_addr rx_land_animal_create(void);                            /* 0x1057: 006cfbe0, 0x890 bytes */
rx_addr rx_scenario_anim_create(void);
rx_addr scene_character_set_animation(rx_addr c, rx_addr handle, uint32_t loops);   /* 00834eb0 */
void    rx_model_instance_init(rx_addr instance);   /* 007f12c0 */
void    scene_model_bounds(rx_addr asset, rx_addr instance, rx_addr ctr, rx_addr ext, rx_addr rad);   /* 007f7f40 */
/* the lights container of a scene manager (scene + 0x1a18): Lights_Add 007dfdd0 */
void    rx_scene_lights_add(rx_addr lights, rx_addr entity);

/* ---- props (build_scene_props.c) ---- */
rx_addr  rx_physics_entity_create(void);                         /* 0x1029 / 0x106f: 004036d0 */
rx_addr  rx_popup_bale_create(void);                              /* 0x1031: 006c0dc0 */
rx_addr  rx_popdown_bale_create(void);                            /* 0x1032: 006c05a0 */
rx_addr  rx_drop_gate_create(void);                               /* 0x1033: 006bd910 */
rx_addr  rx_collision_entity_construct(rx_addr e);                /* 00818f60 */
uint32_t rx_collision_entity_vf5(rx_addr e, uint32_t reader);     /* 00819130 */
uint32_t rx_physics_entity_vf5(rx_addr e, uint32_t reader);       /* 0041c8a0 */
rx_addr  rx_hfterrain_entity_create(void);                        /* 0x10: 007c8cf0 */
void     scene_character_load_model(rx_addr e, rx_addr handle);   /* the vf5 prologue of the character props: ModelAsset cloned into +0x200, CharacterAnim_Init */
rx_addr  rx_scenario_anim_construct(rx_addr e);                    /* 0074a810 */
rx_addr  rx_crew_chief_create(void);                               /* 0x103b */
rx_addr  rx_trackside_flag_holder_create(void);                    /* 0x1038 */
rx_addr  rx_scoretower_flag_holder_create(void);                   /* 0x1039 */
rx_addr  rx_thirty_second_board_create(void);                      /* 0x103a */
void     rx_event_register_first_human(rx_addr entity, uint32_t cb, uint32_t arg);   /* 006c3860 */
void     rx_event_register_global(rx_addr name, rx_addr entity, uint32_t cb, uint32_t arg);   /* 006c4970 */
void     rx_event_register_global_b(rx_addr name, rx_addr entity, uint32_t cb, uint32_t arg);   /* 006c44b0 */
int      scene_list_push_unique(rx_addr list, uint32_t value);     /* 00544cc0 */
rx_addr  rx_forest_system_create(void);                            /* 0x100f: 00662420 */
rx_addr  rx_camera_path_create(void);                              /* 0x104b: 0063e490 */
rx_addr  rx_camera_path_construct_named(rx_addr t, const char* label, const char* clip);   /* 0063e590 (stdcall; label and clip name as C strings) */
void     rx_trigger_level_config(rx_addr cfg, rx_addr text);       /* 00409560 */
rx_addr  rx_start_entity_create(uint32_t id);                     /* the StartEntity family (build_scene_start.c) */
rx_addr  rx_challenge_specs_create(void);                         /* 0x1019 */
rx_addr  rx_collectable_create(uint32_t id);                        /* 0x106c / 0x106d / 0x106e / 0x1044 */
rx_addr  rx_trigger_create_sized(uint32_t size);
rx_addr  rx_scrolling_texture_create(void);                      /* 0x1078 */
rx_addr  rx_camera_facing_create(void);                            /* 0x1079 */
rx_addr  rx_water_entity_create(void);                             /* 0xf */
rx_addr  rx_billboard_crowd_create(void);                          /* 0x1041: 006c5f30, 0x190 bytes (build_scene_crowd.c) */
rx_addr  rx_ai_avoid_sphere_create(void);                         /* 0x1073 */
rx_addr  rx_stepup_upright_create(void);                          /* 0x1070 */
