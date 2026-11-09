/* fmt_scene.h -- level files: <level>.dx9.level, the <level>.dx9.scene container and its entities, and the level as a whole.
 *
 * <level>.dx9.level (BXML): <Level><Databases><Database name/>..</Databases><Packages><Package name/>..</Packages>
 *   <Subscenes><Subscene name/>..</Subscenes></Level>. A race level lists its database, the packages "Shared" and
 *   "<level>Base", and the subscenes "<level>Base" (the static world) plus one per activity ("<level>FreeRide",
 *   "<level>WaypointRace1", "<level>ChallengeHillClimbA" ...).
 *
 * <level>.dx9.scene (little endian):
 *     u32 0xC0FFFEEE, u32 subscene count
 *     per subscene { char name[256] NUL padded, u32 0, u32 absolute section offset }   (264 bytes, offsets increasing)
 *     sections: u32 chunk count, u32 entity count, then the chunks, back to back up to the next section / the file end:
 *       a chunk is a complete embedded BXML document (0x103EA; its size = 36 + the last header word).
 *   Every <Entity entityId=..> node of a chunk has one <RSData namespace="RSToolsNS_RSDataNS_<Type>RSData"> child whose
 *   children are the named fields: <Field name="Matrix"> with the typed value nodes below it (<Matrix>, <String>,
 *   <UInt>, <Float>, <Bool>, <Asset name=".." type="..">, <Enum>+<EnumValue> ...). The scene Matrix is row-major D3D with row
 *   vectors: rows 0..2 the local axes, row 3 the translation (floats 12..14).
 *
 * Entities are views over the chunk documents the scene owns; they are valid until fmt_scene_free. */
#ifndef FMT_SCENE_H
#define FMT_SCENE_H

#include "fmt_bxml.h"
#include "fmt_db.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- .dx9.level ---- */
typedef struct fmt_level_info {
    char** databases; int ndatabases;
    char** packages;  int npackages;
    char** subscenes; int nsubscenes;
    char*  storage;                       /* internal */
} fmt_level_info;

fmt_level_info* fmt_level_info_parse(const uint8_t* data, size_t size);
void            fmt_level_info_free(fmt_level_info* li);

/* ---- entities ---- */
typedef struct fmt_entity {
    uint32_t    id;                       /* the Entity's entityId attribute */
    const char* ns;                       /* RSData namespace, "RSToolsNS_RSDataNS_ModelRSData" */
    const char* type;                     /* its short form after the last '_': "ModelRSData" */
    int         subscene;                 /* index into the scene's subscene table */
    fmt_bxml*   doc;                      /* the chunk document the entity lives in */
    int         node;                     /* the Entity node */
    int         rsdata;                   /* its RSData node, -1 if the Entity has another shape */
} fmt_entity;

int         fmt_entity_field(const fmt_entity* e, const char* name);                   /* node of <.. name="name">, or -1 */
int         fmt_entity_field_child(const fmt_entity* e, const char* name, int k);      /* k-th child (the typed value), or -1 */
int         fmt_entity_field_tagged(const fmt_entity* e, const char* name, const char* tag);   /* child of the field with that tag, or -1 */
const char* fmt_entity_string(const fmt_entity* e, const char* name);                  /* value of a <String> field, else NULL */
float       fmt_entity_float(const fmt_entity* e, const char* name, float def);        /* Float / Int / UInt / Bool field */
int32_t     fmt_entity_int(const fmt_entity* e, const char* name, int32_t def);        /* Int / UInt / Bool / Float(truncated) */
int         fmt_entity_bool(const fmt_entity* e, const char* name, int def);
int         fmt_entity_enum(const fmt_entity* e, const char* name, int def);           /* the <EnumValue> under the field */
int         fmt_entity_floats(const fmt_entity* e, const char* name, float* out, int max);   /* components of the first value; count */
int         fmt_entity_matrix(const fmt_entity* e, float out[16]);                     /* field "Matrix"; 0 or -1 */
const char* fmt_entity_asset(const fmt_entity* e, const char* name);                   /* <Asset name=..> of a field */
const char* fmt_entity_asset_type(const fmt_entity* e, const char* name);

/* ---- the scene container ---- */
typedef struct fmt_scene fmt_scene;

fmt_scene*  fmt_scene_parse(const uint8_t* data, size_t size);     /* copies data; reads only the subscene table */
void        fmt_scene_free(fmt_scene* s);
int         fmt_scene_subscene_count(const fmt_scene* s);
const char* fmt_scene_subscene_name(const fmt_scene* s, int i);
int         fmt_scene_find_subscene(const fmt_scene* s, const char* name);
uint32_t    fmt_scene_subscene_offset(const fmt_scene* s, int i);
uint32_t    fmt_scene_subscene_bytes(const fmt_scene* s, int i);
uint32_t    fmt_scene_subscene_chunks(const fmt_scene* s, int i);          /* declared; known without loading */
uint32_t    fmt_scene_subscene_entity_count(const fmt_scene* s, int i);    /* declared */
/* Decodes the chunks of a subscene (once; thread safe) and returns its entities in file order. The count goes to *n. */
const fmt_entity* fmt_scene_entities(fmt_scene* s, int subscene, int* n);
const fmt_bxml*   fmt_scene_chunk(const fmt_scene* s, int subscene, int k);    /* the chunk documents (loaded subscenes) */
/* The nth entity of a short type ("HFTerrainRSData") in one subscene, or in every loaded subscene when subscene < 0. */
const fmt_entity* fmt_scene_find_type(const fmt_scene* s, int subscene, const char* type, int nth);

/* ---- a level: info + database + scene, and the asset lookup order the game uses ---- */
typedef struct fmt_level fmt_level;

fmt_level*  fmt_level_open(fmt_install* in, const char* name);     /* "FR_Alpine": reads .level, opens the database and the scene */
void        fmt_level_free(fmt_level* lv);
const char* fmt_level_name(const fmt_level* lv);
const fmt_level_info* fmt_level_get_info(const fmt_level* lv);
const fmt_database*   fmt_level_database(const fmt_level* lv);
fmt_scene*  fmt_level_scene(fmt_level* lv);                        /* NULL when the level has no scene file */
fmt_install* fmt_level_install(const fmt_level* lv);
const char* fmt_level_base_package(const fmt_level* lv);           /* the package ending in "Base" (the level's static assets) */
const char* fmt_level_base_subscene(const fmt_level* lv);          /* the subscene named like the Base package, else the first ending in "Base" */
/* The subscene the game loads with Base for an activity ("FreeRide", "WaypointRace1" ...): "<stem><activity>" with stem =
 * the Base name without "Base", else the first subscene ending in the activity. NULL if the level has none. */
const char* fmt_level_activity_subscene(const fmt_level* lv, const char* activity);
/* Asset lookup as the level loader does: the Base package first, then the other packages of the level in .level order, then
 * the packages of EngineDependencies. */
const fmt_asset* fmt_level_find_asset(fmt_level* lv, const char* name, const char* type);
int         fmt_level_read_asset(fmt_level* lv, const char* name, const char* type, fmt_buf* out);

#ifdef __cplusplus
}
#endif
#endif
