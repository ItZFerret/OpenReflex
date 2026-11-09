/* fmt_db.h -- databases (.dx9.database), packages (.dx9.package) and the install that ties the readers together.
 *
 * Database/<name>.dx9.database is BXML:  <Database><Handles><Asset name type/>...</Handles><Packages><Package name file offset
 * blockOffset heapOffset><Asset name type><Heap offset size/><Block offset size/><Compress enabled codec/></Asset>...</Package>
 * ...</Packages></Database>. The bytes of an asset are in Database/<file>.dx9.package at (package offset + heap offset), `size`
 * bytes; with Compress enabled (codec LZX) they are an XMem resource (fmt_lzx.h), otherwise stored. Asset types met:
 * texture, material, shader, model, surface, anim, collis, forest, tdf, packdata, bxml, script, tbldata, localiz, sound,
 * soundbnk ... A name is unique per (package, type); the level databases have a Shared and a <level>Base package.
 *
 * fmt_install is the handle most callers want: the virtual file system plus a cache of opened databases; safe to use from
 * several threads (databases are immutable once opened). */
#ifndef FMT_DB_H
#define FMT_DB_H

#include "fmt_common.h"
#include "fmt_vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fmt_database fmt_database;
typedef struct fmt_package fmt_package;

typedef struct fmt_asset {
    const char*  name;
    const char*  type;
    uint64_t     offset;          /* in the package file: package offset + heap offset */
    uint32_t     size;            /* packed bytes (heap size) */
    uint32_t     block_size;      /* nonzero: a block resource (not supported by the readers) */
    uint8_t      lzx;             /* Compress enabled with codec LZX */
    uint8_t      compressed_other;/* Compress enabled with another codec (not supported) */
    fmt_package* package;
} fmt_asset;

struct fmt_package {
    const char*   name;
    const char*   file;           /* base name of the package file; the file is Database/<file>.dx9.package */
    uint64_t      offset, block_offset, heap_offset;
    fmt_asset*    assets;
    int           nassets;
    int           duplicates;     /* assets repeating an earlier (name, type): lookups return the first */
    fmt_database* database;
    int*          hash;           /* internal */
    uint32_t      hmask;
};

typedef struct fmt_handle { const char* name; const char* type; } fmt_handle;

struct fmt_database {
    char*         name;
    fmt_package*  packages;
    int           npackages;
    fmt_handle*   handles;        /* the <Handles> list: every asset the database refers to (also in other databases) */
    int           nhandles;
    void*         bxml;           /* internal: owns the strings above */
};

/* ---- reading one database ---- */
fmt_database*     fmt_db_open(const fmt_vfs* vfs, const char* name);              /* name without ".dx9.database" */
void              fmt_db_free(fmt_database* db);
const fmt_package* fmt_db_package(const fmt_database* db, const char* name);       /* case-insensitive; NULL if absent */
const fmt_asset*  fmt_package_find(const fmt_package* p, const char* name, const char* type);   /* case-insensitive; NULL if absent */
const fmt_asset*  fmt_db_find(const fmt_database* db, const char* name, const char* type);      /* first package that has it */
/* Reads an asset's bytes, decompressed. max_out 0 = 256 MiB. */
int               fmt_db_read(const fmt_vfs* vfs, const fmt_asset* a, fmt_buf* out);

/* ---- the install ---- */
typedef struct fmt_install fmt_install;

fmt_install*      fmt_install_open(const char* root_utf8);                        /* see fmt_vfs_open */
void              fmt_install_close(fmt_install* in);
fmt_vfs*          fmt_install_vfs(fmt_install* in);
const fmt_database* fmt_install_db(fmt_install* in, const char* name);            /* cached; NULL (with an error) when absent */
int               fmt_install_has_db(fmt_install* in, const char* name);
int               fmt_install_read(fmt_install* in, const fmt_asset* a, fmt_buf* out);
/* MODS/TRACKS (port addition): the level `level` read from `folder` (a custom track: its <level>.dx9.level / .database / .package / .scene,
 * any case) instead of the install, for this install handle only (fmt_vfs_mount_dir; nothing on disk changes). Before the level's
 * database is opened; all four files are required. The number of files mounted, or -1 (fmt_error). */
int               fmt_install_mount_level(fmt_install* in, const char* level, const char* folder);
/* database + package + name + type; package NULL = any package of the database */
int               fmt_install_read_named(fmt_install* in, const char* db, const char* package, const char* name, const char* type, fmt_buf* out);
/* Names (without ".dx9.database") of the databases whose file name contains `contains` (case-insensitive; NULL = all), in
 * ordinal name order. The array and its strings are one malloc block: free() it. Returns the count or -1. */
int               fmt_install_database_names(fmt_install* in, const char* contains, char*** names);

#ifdef __cplusplus
}
#endif
#endif
