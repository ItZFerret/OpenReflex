/* fmt_scene.c -- see fmt_scene.h. Reference: _re/formats/scene_read.py, ReflexScene.cs, ReflexLevelContent.cs. */
#include "fmt_scene.h"
#include "fmt_modtrack.h"
#include "fmt_sys.h"

#include <stdio.h>
#include <stdlib.h>

/* ------------------------------------------------------------------------------------------------ .level */

static int count_children(const fmt_bxml* d, int node, const char* tag)
{
    int n = 0;
    for (int i = 0; i < fmt_bxml_child_count(d, node); i++) if (!strcmp(fmt_bxml_tag(d, fmt_bxml_child(d, node, i)), tag)) n++;
    return n;
}

static int fill_list(const fmt_bxml* d, int parent, const char* tag, char*** list, int* n, char** store)
{
    *n = 0;
    *list = NULL;
    if (parent < 0) return 0;
    int cnt = count_children(d, parent, tag);
    *list = (char**)fmt_alloc(((size_t)cnt + 1) * sizeof(char*));
    if (!*list) return -1;
    for (int i = 0; i < fmt_bxml_child_count(d, parent); i++) {
        int c = fmt_bxml_child(d, parent, i);
        if (strcmp(fmt_bxml_tag(d, c), tag) != 0) continue;
        const char* nm = fmt_bxml_attr_str(d, c, "name");
        if (!nm) continue;
        size_t l = strlen(nm) + 1;
        memcpy(*store, nm, l);
        (*list)[(*n)++] = *store;
        *store += l;
    }
    return 0;
}

fmt_level_info* fmt_level_info_parse(const uint8_t* data, size_t size)
{
    fmt_bxml* d = fmt_bxml_parse(data, size);
    if (!d) return NULL;
    fmt_level_info* li = (fmt_level_info*)fmt_alloc(sizeof *li);
    if (!li) { fmt_bxml_free(d); return NULL; }
    if (strcmp(fmt_bxml_tag(d, 0), "Level") != 0) { fmt_fail("level: root element is %s", fmt_bxml_tag(d, 0)); fmt_bxml_free(d); free(li); return NULL; }
    size_t total = 1;
    for (uint32_t i = 0; i < fmt_bxml_node_count(d); i++) { const char* nm = fmt_bxml_attr_str(d, (int)i, "name"); if (nm) total += strlen(nm) + 1; }
    li->storage = (char*)fmt_alloc(total);
    if (!li->storage) { fmt_bxml_free(d); free(li); return NULL; }
    char* store = li->storage;
    if (fill_list(d, fmt_bxml_find_child(d, 0, "Databases"), "Database", &li->databases, &li->ndatabases, &store) < 0 ||
        fill_list(d, fmt_bxml_find_child(d, 0, "Packages"), "Package", &li->packages, &li->npackages, &store) < 0 ||
        fill_list(d, fmt_bxml_find_child(d, 0, "Subscenes"), "Subscene", &li->subscenes, &li->nsubscenes, &store) < 0 ||
        /* the community's track tools write <SubScenes><SubScene> (Mods/Tracks); the game reads the scene file's own directory */
        (li->nsubscenes == 0 && fmt_bxml_find_child(d, 0, "Subscenes") < 0 &&
         fill_list(d, fmt_bxml_find_child(d, 0, "SubScenes"), "SubScene", &li->subscenes, &li->nsubscenes, &store) < 0)) {
        fmt_level_info_free(li); fmt_bxml_free(d);
        return NULL;
    }
    fmt_bxml_free(d);
    return li;
}

void fmt_level_info_free(fmt_level_info* li)
{
    if (!li) return;
    free(li->databases); free(li->packages); free(li->subscenes); free(li->storage);
    free(li);
}

/* ------------------------------------------------------------------------------------------------ entities */

static int ent_field(const fmt_entity* e, const char* name)
{
    if (!e || e->rsdata < 0) return -1;
    return fmt_bxml_find_child_attr(e->doc, e->rsdata, "name", name);
}

int fmt_entity_field(const fmt_entity* e, const char* name) { return ent_field(e, name); }

int fmt_entity_field_child(const fmt_entity* e, const char* name, int k)
{
    int f = ent_field(e, name);
    return f < 0 ? -1 : fmt_bxml_child(e->doc, f, k);
}

int fmt_entity_field_tagged(const fmt_entity* e, const char* name, const char* tag)
{
    int f = ent_field(e, name);
    return f < 0 ? -1 : fmt_bxml_find_child(e->doc, f, tag);
}

const char* fmt_entity_string(const fmt_entity* e, const char* name)
{
    int c = fmt_entity_field_child(e, name, 0);
    fmt_bxml_value v;
    if (c < 0 || fmt_bxml_node_value(e->doc, c, &v) < 0 || v.kind != FMT_BX_STRING) return NULL;
    return v.str;
}

static int scalar(const fmt_entity* e, const char* name, fmt_bxml_value* v)
{
    int c = fmt_entity_field_child(e, name, 0);
    return c >= 0 && fmt_bxml_node_value(e->doc, c, v) == 0 && v->count >= 1 && v->kind != FMT_BX_STRING && v->kind != FMT_BX_BYTES;
}

float fmt_entity_float(const fmt_entity* e, const char* name, float def)
{
    fmt_bxml_value v;
    if (!scalar(e, name, &v)) return def;
    switch (v.kind) {
    case FMT_BX_FLOAT: return fmt_bx_f32(&v, 0, 0);
    case FMT_BX_INT: return (float)fmt_bx_i32(&v, 0);
    case FMT_BX_UINT: case FMT_BX_BOOL: return (float)fmt_bx_u32(&v, 0);
    default: return def;
    }
}

int32_t fmt_entity_int(const fmt_entity* e, const char* name, int32_t def)
{
    fmt_bxml_value v;
    if (!scalar(e, name, &v)) return def;
    switch (v.kind) {
    case FMT_BX_INT: case FMT_BX_UINT: case FMT_BX_BOOL: return fmt_bx_i32(&v, 0);
    case FMT_BX_FLOAT: return (int32_t)fmt_bx_f32(&v, 0, 0);
    default: return def;
    }
}

int fmt_entity_bool(const fmt_entity* e, const char* name, int def)
{
    fmt_bxml_value v;
    if (!scalar(e, name, &v)) return def;
    if (v.kind == FMT_BX_FLOAT) return fmt_bx_f32(&v, 0, 0) != 0.0f;
    return fmt_bx_u32(&v, 0) != 0;
}

int fmt_entity_enum(const fmt_entity* e, const char* name, int def)
{
    int c = fmt_entity_field_tagged(e, name, "EnumValue");
    fmt_bxml_value v;
    if (c < 0 || fmt_bxml_node_value(e->doc, c, &v) < 0 || v.count < 1 || (v.kind != FMT_BX_INT && v.kind != FMT_BX_UINT)) return def;
    return fmt_bx_i32(&v, 0);
}

int fmt_entity_floats(const fmt_entity* e, const char* name, float* out, int max)
{
    int c = fmt_entity_field_child(e, name, 0);
    fmt_bxml_value v;
    if (c < 0 || fmt_bxml_node_value(e->doc, c, &v) < 0 || v.count < 1) return 0;
    int comps = fmt_bxml_elem_size(v.kind) / 4;
    if (comps == 0 || v.kind == FMT_BX_STRING || v.kind == FMT_BX_BYTES) return 0;
    int n = 0;
    for (uint32_t i = 0; i < v.count && n < max; i++)
        for (int k = 0; k < comps && n < max; k++) out[n++] = (v.kind == FMT_BX_INT) ? (float)fmt_bx_i32(&v, i) : (v.kind == FMT_BX_UINT || v.kind == FMT_BX_BOOL) ? (float)fmt_bx_u32(&v, i) : fmt_bx_f32(&v, i, k);
    return n;
}

int fmt_entity_matrix(const fmt_entity* e, float out[16])
{
    int c = fmt_entity_field_child(e, "Matrix", 0);
    fmt_bxml_value v;
    if (c < 0 || fmt_bxml_node_value(e->doc, c, &v) < 0 || v.kind != FMT_BX_MATRIX || v.count != 1) return fmt_fail("entity %u: no matrix", e ? e->id : 0);
    fmt_bx_floats(&v, 0, out);
    if (fmt_modtrack_active() && strstr(e->type,"StartEntityFreeRide")) fmt_modtrack_start(out);
    return 0;
}

const char* fmt_entity_asset(const fmt_entity* e, const char* name)
{
    int c = fmt_entity_field_child(e, name, 0);
    return c < 0 ? NULL : fmt_bxml_attr_str(e->doc, c, "name");
}

const char* fmt_entity_asset_type(const fmt_entity* e, const char* name)
{
    int c = fmt_entity_field_child(e, name, 0);
    return c < 0 ? NULL : fmt_bxml_attr_str(e->doc, c, "type");
}

/* ------------------------------------------------------------------------------------------------ scene container */

typedef struct {
    char name[257];
    uint32_t offset, end;
    uint32_t chunks, entities;         /* declared */
    int loaded;
    fmt_bxml** docs;
    int ndocs;
    fmt_entity* ents;
    int nents;
} subscene_t;

struct fmt_scene {
    uint8_t* data;
    size_t size;
    subscene_t* sub;
    int nsub;
    fmt_mutex lock;
};

fmt_scene* fmt_scene_parse(const uint8_t* data, size_t size)
{
    if (!data || size < 8 || size > ((size_t)512 << 20)) { fmt_fail("scene: container outside bounds"); return NULL; }
    if (fmt_u32(data) != 0xC0FFFEEEu) { fmt_fail("scene: bad container magic"); return NULL; }
    uint32_t count = fmt_u32(data + 4);
    if (count == 0 || count > 128) { fmt_fail("scene: bad subscene count %u", count); return NULL; }
    size_t table_end = 8 + (size_t)count * 264;
    if (table_end > size) { fmt_fail("scene: truncated subscene table"); return NULL; }
    fmt_scene* s = (fmt_scene*)fmt_alloc(sizeof *s);
    if (!s) return NULL;
    fmt_mutex_init(&s->lock);
    s->data = (uint8_t*)fmt_alloc(size + 1);
    s->sub = (subscene_t*)fmt_alloc((size_t)count * sizeof(subscene_t));
    if (!s->data || !s->sub) { fmt_scene_free(s); return NULL; }
    memcpy(s->data, data, size);
    s->size = size;
    s->nsub = (int)count;
    uint32_t prev = 0;
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t* e = data + 8 + (size_t)i * 264;
        subscene_t* sc = &s->sub[i];
        size_t zero = 0;
        while (zero < 256 && e[zero]) zero++;
        if (zero == 0 || zero == 256) { fmt_fail("scene: subscene %u has an invalid name", i); fmt_scene_free(s); return NULL; }
        /* the bytes after the name's NUL are not read (the game takes the name as a C string): the original files pad with
         * zeros, the community's track tools leave stack garbage there (Mods/Tracks: Beta_Nat_Track_Slot_4 "Farm 14") */
        memcpy(sc->name, e, zero);
        /* +256 is a reserved word the game does not read: the original files hold 0, the community's track tools can leave stack
         * garbage there too (Mods/Tracks "Unadilla": 0x9ebe199a at its third subscene) */
        uint32_t off = fmt_u32(e + 260);
        if (off < table_end || off >= size || (i && off <= prev)) { fmt_fail("scene: invalid section pointer for subscene %u", i); fmt_scene_free(s); return NULL; }
        sc->offset = off;
        prev = off;
    }
    for (uint32_t i = 0; i < count; i++) {
        subscene_t* sc = &s->sub[i];
        sc->end = i + 1 < count ? s->sub[i + 1].offset : (uint32_t)size;
        if (sc->offset + 8 > sc->end) { fmt_fail("scene: truncated section header for %s", sc->name); fmt_scene_free(s); return NULL; }
        sc->chunks = fmt_u32(data + sc->offset);
        sc->entities = fmt_u32(data + sc->offset + 4);
        if (sc->chunks == 0 || sc->chunks > 4096 || sc->entities > 100000) { fmt_fail("scene: section counts of %s exceed bounds", sc->name); fmt_scene_free(s); return NULL; }
    }
    return s;
}

void fmt_scene_free(fmt_scene* s)
{
    if (!s) return;
    for (int i = 0; i < s->nsub; i++) {
        for (int k = 0; k < s->sub[i].ndocs; k++) fmt_bxml_free(s->sub[i].docs[k]);
        free(s->sub[i].docs); free(s->sub[i].ents);
    }
    free(s->sub); free(s->data);
    fmt_mutex_destroy(&s->lock);
    free(s);
}

int fmt_scene_subscene_count(const fmt_scene* s) { return s ? s->nsub : 0; }
const char* fmt_scene_subscene_name(const fmt_scene* s, int i) { return s && i >= 0 && i < s->nsub ? s->sub[i].name : NULL; }
int fmt_scene_find_subscene(const fmt_scene* s, const char* name)
{
    if (s && name) for (int i = 0; i < s->nsub; i++) if (!strcmp(s->sub[i].name, name)) return i;
    return -1;
}
uint32_t fmt_scene_subscene_offset(const fmt_scene* s, int i) { return s && i >= 0 && i < s->nsub ? s->sub[i].offset : 0; }
uint32_t fmt_scene_subscene_bytes(const fmt_scene* s, int i) { return s && i >= 0 && i < s->nsub ? s->sub[i].end - s->sub[i].offset : 0; }
uint32_t fmt_scene_subscene_chunks(const fmt_scene* s, int i) { return s && i >= 0 && i < s->nsub ? s->sub[i].chunks : 0; }
uint32_t fmt_scene_subscene_entity_count(const fmt_scene* s, int i) { return s && i >= 0 && i < s->nsub ? s->sub[i].entities : 0; }

static int load_subscene(fmt_scene* s, int index)
{
    subscene_t* sc = &s->sub[index];
    sc->docs = (fmt_bxml**)fmt_alloc((size_t)sc->chunks * sizeof(fmt_bxml*));
    if (!sc->docs) return -1;
    size_t cursor = sc->offset + 8;
    int ent_cap = (int)sc->entities + 4;
    sc->ents = (fmt_entity*)fmt_alloc((size_t)ent_cap * sizeof(fmt_entity));
    if (!sc->ents) return -1;
    for (uint32_t k = 0; k < sc->chunks; k++) {
        if (cursor + 36 > sc->end) return fmt_fail("scene: truncated embedded BXML header in %s", sc->name);
        const uint8_t* h = s->data + cursor;
        if (memcmp(h, "BXML", 4) || fmt_u32(h + 4) != 0x103EA) return fmt_fail("scene: unsupported embedded BXML in %s", sc->name);
        size_t sz = 36 + (size_t)fmt_u32(h + 32);
        if (sz <= 36 || cursor + sz > sc->end) return fmt_fail("scene: embedded BXML crosses the section boundary in %s", sc->name);
        fmt_bxml* d = fmt_bxml_parse(h, sz);
        if (!d) return -1;
        sc->docs[sc->ndocs++] = d;
        for (uint32_t n = 0; n < fmt_bxml_node_count(d); n++) {
            if (strcmp(fmt_bxml_tag(d, (int)n), "Entity") != 0) continue;
            if (sc->nents == ent_cap) {
                ent_cap *= 2;
                fmt_entity* ne = (fmt_entity*)fmt_realloc(sc->ents, (size_t)ent_cap * sizeof(fmt_entity));
                if (!ne) return -1;
                sc->ents = ne;
            }
            fmt_entity* e = &sc->ents[sc->nents++];
            memset(e, 0, sizeof *e);
            e->doc = d; e->node = (int)n; e->subscene = index;
            e->id = (uint32_t)fmt_bxml_attr_i32(d, (int)n, "entityId", 0);
            e->rsdata = -1;
            if (fmt_bxml_child_count(d, (int)n) == 1 && !strcmp(fmt_bxml_tag(d, fmt_bxml_child(d, (int)n, 0)), "RSData")) e->rsdata = fmt_bxml_child(d, (int)n, 0);
            e->ns = e->rsdata >= 0 ? fmt_bxml_attr_str(d, e->rsdata, "namespace") : NULL;
            if (!e->ns) e->ns = "";
            const char* us = strrchr(e->ns, '_');
            e->type = us ? us + 1 : e->ns;
        }
        cursor += sz;
    }
    if (cursor != sc->end) return fmt_fail("scene: %s has %llu bytes past its last chunk", sc->name, (unsigned long long)(sc->end - cursor));
    if ((uint32_t)sc->nents != sc->entities) return fmt_fail("scene: %s declares %u entities, holds %d", sc->name, sc->entities, sc->nents);
    return 0;
}

const fmt_entity* fmt_scene_entities(fmt_scene* s, int subscene, int* n)
{
    if (n) *n = 0;
    if (!s || subscene < 0 || subscene >= s->nsub) { fmt_fail("scene: bad subscene index %d", subscene); return NULL; }
    subscene_t* sc = &s->sub[subscene];
    fmt_mutex_lock(&s->lock);
    if (!sc->loaded) {
        if (load_subscene(s, subscene) < 0) {
            for (int k = 0; k < sc->ndocs; k++) fmt_bxml_free(sc->docs[k]);
            free(sc->docs); free(sc->ents);
            sc->docs = NULL; sc->ents = NULL; sc->ndocs = sc->nents = 0;
            fmt_mutex_unlock(&s->lock);
            return NULL;
        }
        if (fmt_modtrack_active()) {
            int count=0;
            for(int i=0;i<sc->nents;i++) {
                const char* t=sc->ents[i].type;
                if (!strcmp(t,"HFTerrainRSData") || strstr(t,"StartEntityFreeRide") || !strcmp(t,"AmbientDirectionalLightRSData"))
                    sc->ents[count++]=sc->ents[i];
            }
            sc->nents=count;
        }
        sc->loaded = 1;
    }
    fmt_mutex_unlock(&s->lock);
    if (n) *n = sc->nents;
    return sc->ents;
}

const fmt_bxml* fmt_scene_chunk(const fmt_scene* s, int subscene, int k)
{
    if (!s || subscene < 0 || subscene >= s->nsub || !s->sub[subscene].loaded || k < 0 || k >= s->sub[subscene].ndocs) return NULL;
    return s->sub[subscene].docs[k];
}

const fmt_entity* fmt_scene_find_type(const fmt_scene* s, int subscene, const char* type, int nth)
{
    if (!s || !type) return NULL;
    for (int i = 0; i < s->nsub; i++) {
        if (subscene >= 0 && i != subscene) continue;
        const subscene_t* sc = &s->sub[i];
        for (int k = 0; k < sc->nents; k++) if (!strcmp(sc->ents[k].type, type) && nth-- == 0) return &sc->ents[k];
    }
    return NULL;
}

/* ------------------------------------------------------------------------------------------------ level */

struct fmt_level {
    fmt_install* in;
    char* name;
    fmt_level_info* info;
    const fmt_database* db;
    const fmt_database* engine;      /* EngineDependencies, may be NULL */
    fmt_scene* scene;
    char* base_package;
    char* base_subscene;
    const fmt_package** order;       /* lookup order */
    int norder;
    char activity_buf[256];
};

static int ends_with(const char* s, const char* suffix)
{
    size_t a = strlen(s), b = strlen(suffix);
    return a >= b && !strcmp(s + a - b, suffix);
}

void fmt_level_free(fmt_level* lv)
{
    if (!lv) return;
    fmt_level_info_free(lv->info); fmt_scene_free(lv->scene);
    free(lv->name); free(lv->base_package); free(lv->base_subscene); free(lv->order);
    free(lv);
}

fmt_level* fmt_level_open(fmt_install* in, const char* name)
{
    char path[512];
    snprintf(path, sizeof path, "Database/%s.dx9.level", name);
    fmt_buf raw;
    if (fmt_vfs_read_file(fmt_install_vfs(in), path, &raw) < 0) return NULL;
    fmt_level_info* info = fmt_level_info_parse(raw.data, raw.size);
    fmt_buf_free(&raw);
    if (!info) return NULL;
    fmt_level* lv = (fmt_level*)fmt_alloc(sizeof *lv);
    if (!lv) { fmt_level_info_free(info); return NULL; }
    lv->in = in; lv->info = info;
    lv->name = fmt_strdup(name);
    lv->db = fmt_install_db(in, name);
    if (!lv->db) { fmt_level_free(lv); return NULL; }
    fmt_clear_error();
    lv->engine = fmt_install_has_db(in, "EngineDependencies") ? fmt_install_db(in, "EngineDependencies") : NULL;
    snprintf(path, sizeof path, "Database/%s.dx9.scene", name);
    if (fmt_vfs_exists(fmt_install_vfs(in), path)) {
        if (fmt_vfs_read_file(fmt_install_vfs(in), path, &raw) < 0) { fmt_level_free(lv); return NULL; }
        lv->scene = fmt_scene_parse(raw.data, raw.size);
        fmt_buf_free(&raw);
        if (!lv->scene) { fmt_level_free(lv); return NULL; }
    }
    /* package lookup order: Base, the others as the .level lists them, then the rest of the database, then EngineDependencies */
    const fmt_package* base = NULL;
    for (int i = 0; i < info->npackages && !base; i++) {
        const fmt_package* p = fmt_db_package(lv->db, info->packages[i]);
        if (p && ends_with(p->name, "Base")) base = p;
    }
    for (int i = 0; i < lv->db->npackages && !base; i++) if (ends_with(lv->db->packages[i].name, "Base")) base = &lv->db->packages[i];
    int cap = lv->db->npackages + (lv->engine ? lv->engine->npackages : 0) + 1;
    lv->order = (const fmt_package**)fmt_alloc((size_t)cap * sizeof(fmt_package*));
    if (!lv->order) { fmt_level_free(lv); return NULL; }
    if (base) { lv->order[lv->norder++] = base; lv->base_package = fmt_strdup(base->name); }
    for (int i = 0; i < info->npackages; i++) {
        const fmt_package* p = fmt_db_package(lv->db, info->packages[i]);
        int dup = 0;
        for (int k = 0; k < lv->norder; k++) if (lv->order[k] == p) dup = 1;
        if (p && !dup) lv->order[lv->norder++] = p;
    }
    for (int i = 0; i < lv->db->npackages; i++) {
        const fmt_package* p = &lv->db->packages[i];
        int dup = 0;
        for (int k = 0; k < lv->norder; k++) if (lv->order[k] == p) dup = 1;
        if (!dup) lv->order[lv->norder++] = p;
    }
    if (lv->engine) for (int i = 0; i < lv->engine->npackages; i++) lv->order[lv->norder++] = &lv->engine->packages[i];
    /* base subscene */
    if (lv->scene) {
        int bs = base ? fmt_scene_find_subscene(lv->scene, base->name) : -1;
        for (int i = 0; bs < 0 && i < fmt_scene_subscene_count(lv->scene); i++) if (ends_with(fmt_scene_subscene_name(lv->scene, i), "Base")) bs = i;
        if (bs < 0) bs = 0;
        lv->base_subscene = fmt_strdup(fmt_scene_subscene_name(lv->scene, bs));
    }
    return lv;
}

const char* fmt_level_name(const fmt_level* lv) { return lv ? lv->name : NULL; }
const fmt_level_info* fmt_level_get_info(const fmt_level* lv) { return lv ? lv->info : NULL; }
const fmt_database* fmt_level_database(const fmt_level* lv) { return lv ? lv->db : NULL; }
fmt_scene* fmt_level_scene(fmt_level* lv) { return lv ? lv->scene : NULL; }
fmt_install* fmt_level_install(const fmt_level* lv) { return lv ? lv->in : NULL; }
const char* fmt_level_base_package(const fmt_level* lv) { return lv ? lv->base_package : NULL; }
const char* fmt_level_base_subscene(const fmt_level* lv) { return lv ? lv->base_subscene : NULL; }

const char* fmt_level_activity_subscene(const fmt_level* lv, const char* activity)
{
    if (!lv || !lv->scene || !lv->base_subscene || !activity) return NULL;
    fmt_level* m = (fmt_level*)lv;
    size_t bl = strlen(lv->base_subscene);
    if (ends_with(lv->base_subscene, "Base")) bl -= 4;
    snprintf(m->activity_buf, sizeof m->activity_buf, "%.*s%s", (int)bl, lv->base_subscene, activity);
    int i = fmt_scene_find_subscene(lv->scene, m->activity_buf);
    if (i >= 0) return fmt_scene_subscene_name(lv->scene, i);
    for (i = 0; i < fmt_scene_subscene_count(lv->scene); i++) {
        const char* n = fmt_scene_subscene_name(lv->scene, i);
        if (strcmp(n, lv->base_subscene) && ends_with(n, activity)) return n;
    }
    return NULL;
}

const fmt_asset* fmt_level_find_asset(fmt_level* lv, const char* name, const char* type)
{
    if (!lv || !name || !*name) return NULL;
    for (int i = 0; i < lv->norder; i++) { const fmt_asset* a = fmt_package_find(lv->order[i], name, type); if (a) return a; }
    return NULL;
}

int fmt_level_read_asset(fmt_level* lv, const char* name, const char* type, fmt_buf* out)
{
    out->data = NULL; out->size = 0;
    const fmt_asset* a = fmt_level_find_asset(lv, name, type);
    if (!a) return fmt_fail("%s: %s.%s missing", lv->name, name, type);
    return fmt_install_read(lv->in, a, out);
}
