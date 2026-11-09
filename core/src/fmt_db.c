/* fmt_db.c -- see fmt_db.h. Reference: _re/formats/package_extract.py (extract), Rfx.Install in RfxData.cs. */
#include "fmt_db.h"
#include "fmt_rxar.h"
#include "fmt_bxml.h"
#include "fmt_lzx.h"
#include "fmt_sys.h"

#include <stdio.h>
#include <stdlib.h>

#define MAX_RESOURCE ((size_t)128 << 20)

static uint32_t asset_hash(const char* name, const char* type)
{
    uint32_t h = fmt_hash_ci(name);
    return (h ^ fmt_hash_ci(type)) * 16777619u;
}

static int package_index(fmt_package* p)
{
    uint32_t cap = 8;
    while (cap < (uint32_t)p->nassets * 2u) cap <<= 1;
    p->hash = (int*)fmt_alloc(cap * sizeof(int));
    if (!p->hash) return -1;
    p->hmask = cap - 1;
    for (int i = 0; i < p->nassets; i++) {
        const fmt_asset* a = &p->assets[i];
        uint32_t h = asset_hash(a->name, a->type) & p->hmask;
        for (;;) {
            int cur = p->hash[h];
            if (!cur) { p->hash[h] = i + 1; break; }
            const fmt_asset* b = &p->assets[cur - 1];
            if (!fmt_stricmp(b->name, a->name) && !fmt_stricmp(b->type, a->type)) { p->duplicates++; break; }
            h = (h + 1) & p->hmask;
        }
    }
    return 0;
}

void fmt_db_free(fmt_database* db)
{
    if (!db) return;
    for (int i = 0; i < db->npackages; i++) { free(db->packages[i].assets); free(db->packages[i].hash); }
    free(db->packages); free(db->handles); free(db->name);
    fmt_bxml_free((fmt_bxml*)db->bxml);
    free(db);
}

static uint32_t u32_attr(const fmt_bxml* d, int node, const char* name)
{
    return (uint32_t)fmt_bxml_attr_i32(d, node, name, 0);
}

fmt_database* fmt_db_open(const fmt_vfs* vfs, const char* name)
{
    char path[512];
    snprintf(path, sizeof path, "Database/%s.dx9.database", name);
    fmt_buf raw;
    if (fmt_vfs_read_file(vfs, path, &raw) < 0) return NULL;
    fmt_bxml* d = fmt_bxml_parse(raw.data, raw.size);
    fmt_buf_free(&raw);
    if (!d) return NULL;
    fmt_database* db = (fmt_database*)fmt_alloc(sizeof *db);
    if (!db) { fmt_bxml_free(d); return NULL; }
    db->bxml = d;
    db->name = fmt_strdup(name);
    if (!db->name) { fmt_db_free(db); return NULL; }
    int handles = fmt_bxml_find_child(d, 0, "Handles"), packages = fmt_bxml_find_child(d, 0, "Packages");
    if (fmt_stricmp(fmt_bxml_tag(d, 0), "Database") != 0 || packages < 0) { fmt_fail("%s: not a database document", path); fmt_db_free(db); return NULL; }
    if (handles >= 0) {
        int n = fmt_bxml_child_count(d, handles);
        db->handles = (fmt_handle*)fmt_alloc(((size_t)n + 1) * sizeof(fmt_handle));
        if (!db->handles) { fmt_db_free(db); return NULL; }
        for (int i = 0; i < n; i++) {
            int c = fmt_bxml_child(d, handles, i);
            const char *hn = fmt_bxml_attr_str(d, c, "name"), *ht = fmt_bxml_attr_str(d, c, "type");
            if (!hn || !ht) continue;
            db->handles[db->nhandles].name = hn; db->handles[db->nhandles].type = ht; db->nhandles++;
        }
    }
    int np = 0;
    for (int i = 0; i < fmt_bxml_child_count(d, packages); i++) if (!strcmp(fmt_bxml_tag(d, fmt_bxml_child(d, packages, i)), "Package")) np++;
    db->packages = (fmt_package*)fmt_alloc(((size_t)np + 1) * sizeof(fmt_package));
    if (!db->packages) { fmt_db_free(db); return NULL; }
    for (int i = 0; i < fmt_bxml_child_count(d, packages); i++) {
        int pn = fmt_bxml_child(d, packages, i);
        if (strcmp(fmt_bxml_tag(d, pn), "Package") != 0) continue;
        fmt_package* p = &db->packages[db->npackages++];
        p->database = db;
        p->name = fmt_bxml_attr_str(d, pn, "name");
        p->file = fmt_bxml_attr_str(d, pn, "file");
        if (!p->name || !p->file || !*p->file || strchr(p->file, '/') || strchr(p->file, '\\')) {
            fmt_fail("%s: bad package name or file name", path);
            fmt_db_free(db);
            return NULL;
        }
        p->offset = u32_attr(d, pn, "offset");
        p->block_offset = u32_attr(d, pn, "blockOffset");
        p->heap_offset = u32_attr(d, pn, "heapOffset");
        int na = 0;
        for (int k = 0; k < fmt_bxml_child_count(d, pn); k++) if (!strcmp(fmt_bxml_tag(d, fmt_bxml_child(d, pn, k)), "Asset")) na++;
        p->assets = (fmt_asset*)fmt_alloc(((size_t)na + 1) * sizeof(fmt_asset));
        if (!p->assets) { fmt_db_free(db); return NULL; }
        for (int k = 0; k < fmt_bxml_child_count(d, pn); k++) {
            int an = fmt_bxml_child(d, pn, k);
            if (strcmp(fmt_bxml_tag(d, an), "Asset") != 0) continue;
            int heap = fmt_bxml_find_child(d, an, "Heap"), block = fmt_bxml_find_child(d, an, "Block"), comp = fmt_bxml_find_child(d, an, "Compress");
            const char *an_name = fmt_bxml_attr_str(d, an, "name"), *an_type = fmt_bxml_attr_str(d, an, "type");
            if (heap < 0 || !an_name || !an_type) continue;      /* an asset without a heap region has no bytes */
            fmt_asset* a = &p->assets[p->nassets++];
            a->name = an_name; a->type = an_type;
            a->offset = p->offset + u32_attr(d, heap, "offset");
            a->size = u32_attr(d, heap, "size");
            a->block_size = block >= 0 ? u32_attr(d, block, "size") : 0;
            if (comp >= 0 && fmt_bxml_attr_i32(d, comp, "enabled", 0)) {
                const char* codec = fmt_bxml_attr_str(d, comp, "codec");
                if (codec && !strcmp(codec, "LZX")) a->lzx = 1; else a->compressed_other = 1;
            }
            a->package = p;
        }
        if (package_index(p) < 0) { fmt_db_free(db); return NULL; }
    }
    return db;
}

const fmt_package* fmt_db_package(const fmt_database* db, const char* name)
{
    if (db && name) for (int i = 0; i < db->npackages; i++) if (!fmt_stricmp(db->packages[i].name, name)) return &db->packages[i];
    return NULL;
}

const fmt_asset* fmt_package_find(const fmt_package* p, const char* name, const char* type)
{
    if (!p || !name || !type) return NULL;
    uint32_t h = asset_hash(name, type) & p->hmask;
    for (;;) {
        int cur = p->hash[h];
        if (!cur) return NULL;
        const fmt_asset* a = &p->assets[cur - 1];
        if (!fmt_stricmp(a->name, name) && !fmt_stricmp(a->type, type)) return a;
        h = (h + 1) & p->hmask;
    }
}

const fmt_asset* fmt_db_find(const fmt_database* db, const char* name, const char* type)
{
    if (db) for (int i = 0; i < db->npackages; i++) { const fmt_asset* a = fmt_package_find(&db->packages[i], name, type); if (a) return a; }
    return NULL;
}

/* Extracted client layout: each UTF-8 component is escaped byte-for-byte. Keep
 * this encoding identical to Rfx.ExtractedPath; original identifiers may contain
 * slashes, dots and Windows punctuation and must never become path traversal. */
static int extracted_component(char* path, size_t cap, size_t* at, const char* value)
{
    static const char hex[] = "0123456789ABCDEF";
    if (!value || !*value) return fmt_fail("empty extracted asset identifier");
    size_t length = strlen(value);
    char upper[5] = {0};
    if (length <= 4) for (size_t i = 0; i < length; i++) {
        unsigned char c = (unsigned char)value[i];
        upper[i] = (char)(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
    }
    int device = length <= 4 && (!strcmp(upper, "CON") || !strcmp(upper, "PRN") ||
        !strcmp(upper, "AUX") || !strcmp(upper, "NUL") ||
        (length == 4 && (!memcmp(upper, "COM", 3) || !memcmp(upper, "LPT", 3)) && upper[3] >= '1' && upper[3] <= '9'));
    for (const unsigned char* p = (const unsigned char*)value; *p; p++) {
        unsigned char c = *p;
        int plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (device && p == (const unsigned char*)value) plain = 0;
        if (*at + (plain ? 1u : 3u) >= cap) return fmt_fail("extracted asset path too long");
        if (plain) path[(*at)++] = (char)c;
        else { path[(*at)++] = '%'; path[(*at)++] = hex[c >> 4]; path[(*at)++] = hex[c & 15]; }
    }
    path[*at] = 0;
    return 0;
}

static int read_extracted(const fmt_vfs* vfs, const fmt_asset* a, fmt_buf* out)
{
    char path[4096] = "Assets/";
    size_t at = 7;
    const char* parts[] = { a->type, a->package->database->name, a->package->name, a->name, a->type };
    for (int i = 0; i < 5; i++) {
        if (extracted_component(path, sizeof path - 1, &at, parts[i]) < 0) return -1;
        if (i < 4) { path[at++] = i == 3 ? '.' : '/'; path[at] = 0; }
    }
    int64_t size = fmt_vfs_size(vfs, path);
    if (size <= 0 || (uint64_t)size > MAX_RESOURCE)
        return fmt_fail("extracted asset missing or invalid: %s", path);
    return fmt_vfs_read_file(vfs, path, out);
}

int fmt_db_read(const fmt_vfs* vfs, const fmt_asset* a, fmt_buf* out)
{
    out->data = NULL; out->size = 0;
    if (!a) return fmt_fail("no asset");
    {   /* RX_TRACE_READS=1 (diagnostics): every asset the core reads, "rxread <type> <database> <package> <name>" on stderr */
        static int trace = -1;
        if (trace < 0) { const char* t = getenv("RX_TRACE_READS"); trace = t && *t && *t != '0'; }
        if (trace) fprintf(stderr, "rxread %s %s %s %s\n", a->type, a->package->database->name, a->package->name, a->name);
    }
    if (a->block_size) return fmt_fail("%s.%s: block resources are not supported", a->name, a->type);
    if (a->compressed_other) return fmt_fail("%s.%s: unsupported compression codec", a->name, a->type);
    /* The marker commits this install to unpacked assets. Never silently fall
     * back to an original package when an extracted file is damaged/missing. */
    char path[512];
    snprintf(path, sizeof path, "Database/%s.dx9.package", a->package->file);
    /* a mounted package (Mods/Tracks: fmt_vfs_mount_dir / _mount_archive) is a mod's own packed file, also in an extracted install; so
     * is a package a track archive of the root carries (Tracks/<level>.rxtrack converted from an original install: RXARCHIVE.md) */
    if (fmt_vfs_exists(vfs, "reflex-extracted-v1.txt") && !fmt_vfs_mounted(vfs, path) && !fmt_vfs_packed(vfs, path)) return read_extracted(vfs, a, out);
    if (a->size == 0 || a->size > MAX_RESOURCE) return fmt_fail("%s.%s: bad heap size %u", a->name, a->type, a->size);
    int fi = fmt_vfs_find(vfs, path);
    /* an original install with a track archive of the extracted layout (a level packed from an imported library): its assets */
    if (fi < 0 && fmt_vfs_archive_count(vfs) > 0) return read_extracted(vfs, a, out);
    if (fi < 0) return fmt_fail("%s.%s: package file %s is missing", a->name, a->type, path);
    if (a->offset + a->size > fmt_vfs_file_size(vfs, fi)) return fmt_fail("%s.%s: heap region exceeds %s", a->name, a->type, path);
    uint8_t* raw = (uint8_t*)fmt_alloc(a->size);
    if (!raw) return -1;
    if (fmt_vfs_read_at(vfs, fi, a->offset, raw, a->size) < 0) { free(raw); return -1; }
    if (!a->lzx) { out->data = raw; out->size = a->size; return 0; }
    int rc = fmt_xmem_resource(raw, a->size, MAX_RESOURCE, out);
    free(raw);
    return rc;
}

/* ------------------------------------------------------------------------------------------------ install */

typedef struct { char* name; fmt_database* db; } cached_db;

struct fmt_install {
    fmt_vfs* vfs;
    fmt_mutex lock;
    cached_db* dbs;
    int ndbs, cap;
};

fmt_install* fmt_install_open(const char* root)
{
    fmt_vfs* v = fmt_vfs_open(root);
    if (!v) return NULL;
    fmt_install* in = (fmt_install*)fmt_alloc(sizeof *in);
    if (!in) { fmt_vfs_close(v); return NULL; }
    in->vfs = v;
    fmt_mutex_init(&in->lock);
    return in;
}

void fmt_install_close(fmt_install* in)
{
    if (!in) return;
    for (int i = 0; i < in->ndbs; i++) { fmt_db_free(in->dbs[i].db); free(in->dbs[i].name); }
    free(in->dbs);
    fmt_mutex_destroy(&in->lock);
    fmt_vfs_close(in->vfs);
    free(in);
}

fmt_vfs* fmt_install_vfs(fmt_install* in) { return in ? in->vfs : NULL; }

int fmt_install_has_db(fmt_install* in, const char* name)
{
    char path[512];
    snprintf(path, sizeof path, "Database/%s.dx9.database", name);
    return in && fmt_vfs_exists(in->vfs, path);
}

const fmt_database* fmt_install_db(fmt_install* in, const char* name)
{
    if (!in || !name) { fmt_fail("no install or database name"); return NULL; }
    fmt_mutex_lock(&in->lock);
    for (int i = 0; i < in->ndbs; i++)
        if (!fmt_stricmp(in->dbs[i].name, name)) {
            fmt_database* db = in->dbs[i].db;
            fmt_mutex_unlock(&in->lock);
            return db;
        }
    if (!fmt_install_has_db(in, name)) { fmt_mutex_unlock(&in->lock); fmt_fail("database %s not found", name); return NULL; }
    fmt_database* db = fmt_db_open(in->vfs, name);                 /* a damaged one is not cached: the error stays */
    if (!db) { fmt_mutex_unlock(&in->lock); return NULL; }
    if (in->ndbs == in->cap) {
        int cap = in->cap ? in->cap * 2 : 16;
        cached_db* n = (cached_db*)fmt_realloc(in->dbs, (size_t)cap * sizeof *n);
        if (!n) { fmt_db_free(db); fmt_mutex_unlock(&in->lock); return NULL; }
        in->dbs = n; in->cap = cap;
    }
    in->dbs[in->ndbs].name = fmt_strdup(name);
    in->dbs[in->ndbs].db = db;
    in->ndbs++;
    fmt_mutex_unlock(&in->lock);
    return db;
}

int fmt_install_read(fmt_install* in, const fmt_asset* a, fmt_buf* out) { return fmt_db_read(in->vfs, a, out); }

int fmt_install_mount_level(fmt_install* in, const char* level, const char* folder)
{
    if (!in || !level || !*level || !folder || !*folder) return fmt_fail("mount: no level or folder");
    for (const char* p = level; *p; p++)
        if (*p == '/' || *p == '\\' || *p == ':' || (p == level && *p == '.')) return fmt_fail("mount: %s is not a level name", level);
    fmt_mutex_lock(&in->lock);
    for (int i = 0; i < in->ndbs; i++)
        if (!fmt_stricmp(in->dbs[i].name, level)) { fmt_mutex_unlock(&in->lock); return fmt_fail("mount: the database %s is already open", level); }
    fmt_mutex_unlock(&in->lock);
    /* a .rxtrack version 5 (a Reflex archive: RXARCHIVE.md) holds the level's files below Database/ */
    const int n = fmt_rxar_sniff(folder) == 1 ? fmt_vfs_mount_archive(in->vfs, "Database", folder, "Database/", level)
                                              : fmt_vfs_mount_dir(in->vfs, "Database", folder, level);
    if (n < 0) return -1;
    static const char* const need[] = { "level", "database", "package", "scene" };
    char path[512], missing[128] = "";
    for (int k = 0; k < 4; k++) {
        snprintf(path, sizeof path, "Database/%s.dx9.%s", level, need[k]);
        if (!fmt_vfs_mounted(in->vfs, path)) { size_t m = strlen(missing); snprintf(missing + m, sizeof missing - m, "%s.dx9.%s", m ? ", " : "", need[k]); }
    }
    if (*missing) return fmt_fail("mount: %s has no %s of %s", folder, missing, level);
    return n;
}

int fmt_install_read_named(fmt_install* in, const char* dbname, const char* package, const char* name, const char* type, fmt_buf* out)
{
    out->data = NULL; out->size = 0;
    const fmt_database* db = fmt_install_db(in, dbname);
    if (!db) return -1;
    const fmt_asset* a;
    if (package) {
        const fmt_package* p = fmt_db_package(db, package);
        if (!p) return fmt_fail("%s: package %s missing", dbname, package);
        a = fmt_package_find(p, name, type);
    } else a = fmt_db_find(db, name, type);
    if (!a) return fmt_fail("%s/%s: %s.%s missing", dbname, package ? package : "*", name, type);
    return fmt_db_read(in->vfs, a, out);
}

typedef struct { char** names; int n, cap; const char* contains; } name_acc;

static int collect(const char* file, uint64_t size, void* user)
{
    (void)size;
    name_acc* acc = (name_acc*)user;
    static const char suffix[] = ".dx9.database";
    size_t l = strlen(file), sl = sizeof suffix - 1;
    if (l <= sl || fmt_stricmp(file + l - sl, suffix)) return 0;
    char base[256];
    if (l - sl >= sizeof base) return 0;
    memcpy(base, file, l - sl); base[l - sl] = 0;
    if (acc->contains && *acc->contains) {
        int found = 0;
        size_t cl = strlen(acc->contains);
        for (size_t i = 0; base[i] && !found; i++) {
            size_t k = 0;
            while (k < cl && base[i + k] && ((base[i + k] | 32) == (acc->contains[k] | 32))) k++;
            if (k == cl) found = 1;
        }
        if (!found) return 0;
    }
    if (acc->n == acc->cap) {
        int cap = acc->cap ? acc->cap * 2 : 64;
        char** nn = (char**)fmt_realloc(acc->names, (size_t)cap * sizeof(char*));
        if (!nn) return -1;
        acc->names = nn; acc->cap = cap;
    }
    acc->names[acc->n] = fmt_strdup(base);
    if (!acc->names[acc->n]) return -1;
    acc->n++;
    return 0;
}

static int name_cmp(const void* a, const void* b) { return strcmp(*(char* const*)a, *(char* const*)b); }

int fmt_install_database_names(fmt_install* in, const char* contains, char*** names)
{
    *names = NULL;
    name_acc acc = { NULL, 0, 0, contains };
    if (fmt_vfs_list(in->vfs, "Database", collect, &acc) != 0) {
        for (int i = 0; i < acc.n; i++) free(acc.names[i]);
        free(acc.names);
        return -1;
    }
    qsort(acc.names, (size_t)acc.n, sizeof(char*), name_cmp);       /* ordinal order */
    size_t total = ((size_t)acc.n + 1) * sizeof(char*);
    for (int i = 0; i < acc.n; i++) total += strlen(acc.names[i]) + 1;
    char** block = (char**)fmt_alloc(total);
    if (!block) { for (int i = 0; i < acc.n; i++) free(acc.names[i]); free(acc.names); return -1; }
    char* s = (char*)(block + acc.n + 1);
    for (int i = 0; i < acc.n; i++) {
        size_t l = strlen(acc.names[i]) + 1;
        memcpy(s, acc.names[i], l);
        block[i] = s; s += l;
        free(acc.names[i]);
    }
    free(acc.names);
    *names = block;
    return acc.n;
}
