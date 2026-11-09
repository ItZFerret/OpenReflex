/* fmt_vfs.c -- see fmt_vfs.h. */
#if !defined(_WIN32)
#  define _POSIX_C_SOURCE 200809L
#  define _FILE_OFFSET_BITS 64
#  if defined(__APPLE__)
#    define _DARWIN_C_SOURCE 1
#  endif
#endif
#include "fmt_vfs.h"
#include "fmt_rxar.h"
#include "fmt_sys.h"

#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
#  include <wchar.h>
typedef HANDLE fmt_file;
#  define FMT_BAD_FILE INVALID_HANDLE_VALUE
#else
#  include <dirent.h>
#  include <fcntl.h>
#  include <sys/stat.h>
#  include <unistd.h>
typedef int fmt_file;
#  define FMT_BAD_FILE (-1)
#endif

#define FMT_VFS_HANDLES 32

/* real: a mounted file's own path (fmt_vfs_mount_dir), NULL below the root; arc >= 0: the file is entry arc_index of archive arc
 * (fmt_rxar: a .rxdata / .rxtrack of the root, or one mounted by fmt_vfs_mount_archive); mounted: mounted by a mount call */
typedef struct { char* path; uint64_t size; uint32_t hash; char* real; int arc, arc_index; int mounted; } vfs_entry;
typedef struct { int entry; fmt_file file; int users; uint64_t tick; } vfs_slot;

struct fmt_vfs {
    char* root;                  /* UTF-8, '/' separators on every platform */
    vfs_entry* files;            /* sorted by path (byte order) */
    int nfiles;
    int* table;                  /* open addressing over case-folded paths: index + 1, 0 = empty */
    uint32_t mask;
    fmt_mutex lock;
    vfs_slot slots[FMT_VFS_HANDLES];
    uint64_t tick;
    fmt_rxar** arcs;             /* the archives entries point into (owned) */
    int narcs;
};

/* ------------------------------------------------------------------------------------------------ platform bits */

#if defined(_WIN32)
static wchar_t* widen(const char* s)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wchar_t* w = n > 0 ? (wchar_t*)fmt_alloc((size_t)n * sizeof(wchar_t)) : NULL;
    if (w) MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    else if (n <= 0) fmt_fail("cannot convert path to UTF-16");
    for (wchar_t* p = w; p && *p; p++) if (*p == L'/') *p = L'\\';
    return w;
}
static char* narrow(const wchar_t* w)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    char* s = n > 0 ? (char*)fmt_alloc((size_t)n) : NULL;
    if (s) WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL);
    for (char* p = s; p && *p; p++) if (*p == '\\') *p = '/';
    return s;
}
static fmt_file open_file(const char* path)
{
    wchar_t* w = widen(path);
    if (!w) return FMT_BAD_FILE;
    HANDLE h = CreateFileW(w, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    free(w);
    return h;
}
static void close_file(fmt_file f) { CloseHandle(f); }
static int pread_all(fmt_file f, uint64_t off, void* dst, size_t n)
{
    uint8_t* p = (uint8_t*)dst;
    while (n) {
        DWORD want = n > 0x40000000u ? 0x40000000u : (DWORD)n, got = 0;
        OVERLAPPED ov;
        memset(&ov, 0, sizeof ov);
        ov.Offset = (DWORD)off;
        ov.OffsetHigh = (DWORD)(off >> 32);
        if (!ReadFile(f, p, want, &got, &ov) || got == 0) return -1;
        p += got; off += got; n -= got;
    }
    return 0;
}
#else
static fmt_file open_file(const char* path) { return open(path, O_RDONLY); }
static void close_file(fmt_file f) { close(f); }
static int pread_all(fmt_file f, uint64_t off, void* dst, size_t n)
{
    uint8_t* p = (uint8_t*)dst;
    while (n) {
        ssize_t got = pread(f, p, n > 0x40000000u ? 0x40000000u : n, (off_t)off);
        if (got <= 0) return -1;
        p += got; off += (uint64_t)got; n -= (size_t)got;
    }
    return 0;
}
#endif

/* ------------------------------------------------------------------------------------------------ indexing */

typedef struct { vfs_entry* a; int n, cap; } entry_list;

static int list_add(entry_list* l, const char* rel, uint64_t size)
{
    if (l->n == l->cap) {
        int cap = l->cap ? l->cap * 2 : 1024;
        vfs_entry* a = (vfs_entry*)fmt_realloc(l->a, (size_t)cap * sizeof *a);
        if (!a) return -1;
        l->a = a; l->cap = cap;
    }
    vfs_entry* e = &l->a[l->n];
    e->path = fmt_strdup(rel);
    if (!e->path) return -1;
    e->size = size;
    e->hash = fmt_hash_ci(rel);
    e->real = NULL;
    e->arc = -1; e->arc_index = -1; e->mounted = 0;
    l->n++;
    return 0;
}

#if defined(_WIN32)
static int scan(const char* root, const char* rel, entry_list* out, int depth)
{
    char pattern[4096];
    snprintf(pattern, sizeof pattern, "%s%s%s/*", root, *rel ? "/" : "", rel);
    wchar_t* wp = widen(pattern);
    if (!wp) return -1;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(wp, &fd);
    free(wp);
    if (h == INVALID_HANDLE_VALUE) return 0;
    int rc = 0;
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        char* name = narrow(fd.cFileName);
        if (!name) { rc = -1; break; }
        char sub[4096];
        snprintf(sub, sizeof sub, "%s%s%s", rel, *rel ? "/" : "", name);
        free(name);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { if (depth < 16 && scan(root, sub, out, depth + 1) < 0) { rc = -1; break; } }
        else if (list_add(out, sub, ((uint64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow) < 0) { rc = -1; break; }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return rc;
}
static int is_dir(const char* path)
{
    wchar_t* w = widen(path);
    if (!w) return 0;
    DWORD a = GetFileAttributesW(w);
    free(w);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}
#else
static int scan(const char* root, const char* rel, entry_list* out, int depth)
{
    char dir[4096];
    snprintf(dir, sizeof dir, "%s%s%s", root, *rel ? "/" : "", rel);
    DIR* d = opendir(dir);
    if (!d) return 0;
    int rc = 0;
    struct dirent* de;
    while ((de = readdir(d)) != NULL) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
        char sub[4096], full[8200];
        snprintf(sub, sizeof sub, "%s%s%s", rel, *rel ? "/" : "", de->d_name);
        snprintf(full, sizeof full, "%s/%s", root, sub);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { if (depth < 16 && scan(root, sub, out, depth + 1) < 0) { rc = -1; break; } }
        else if (S_ISREG(st.st_mode) && list_add(out, sub, (uint64_t)st.st_size) < 0) { rc = -1; break; }
    }
    closedir(d);
    return rc;
}
static int is_dir(const char* path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}
#endif

static int entry_cmp(const void* a, const void* b) { return strcmp(((const vfs_entry*)a)->path, ((const vfs_entry*)b)->path); }

static int has_database(const char* root)
{
    char p[4200];
    snprintf(p, sizeof p, "%s/Database", root);
    if (is_dir(p)) return 1;
    snprintf(p, sizeof p, "%s/database", root);
    return is_dir(p);
}

/* the lookup table over the sorted entries (open addressing on the case-folded paths) */
static int build_table(fmt_vfs* v)
{
    uint32_t cap = 16;
    while (cap < (uint32_t)v->nfiles * 2u) cap <<= 1;
    int* t = (int*)fmt_alloc(cap * sizeof *t);
    if (!t) return -1;
    free(v->table);
    v->table = t;
    v->mask = cap - 1;
    for (int i = 0; i < v->nfiles; i++) {
        uint32_t h = v->files[i].hash & v->mask;
        while (v->table[h]) {
            if (!fmt_stricmp(v->files[v->table[h] - 1].path, v->files[i].path)) break;   /* two spellings: the first (byte order) wins */
            h = (h + 1) & v->mask;
        }
        if (!v->table[h]) v->table[h] = i + 1;
    }
    return 0;
}

static int attach_archives(fmt_vfs* v);
static int adopt_packed(fmt_vfs* v, int index);

static fmt_vfs* open_utf8(const char* given)
{
    if (!given || !*given) { fmt_fail("no install folder"); return NULL; }
    size_t len = strlen(given);
    while (len > 1 && (given[len - 1] == '/' || given[len - 1] == '\\')) len--;
    /* the root of a drive (D: with a slash: a mounted disc / ISO, a USB stick) keeps its separator: a bare "D:" names the drive's current folder */
    int drive_root = len == 2 && given[1] == ':' && ((given[0] >= 'A' && given[0] <= 'Z') || (given[0] >= 'a' && given[0] <= 'z'));
    char* root = fmt_strndup(given, len);
    if (!root) return NULL;
    if (drive_root) {
        char* r = (char*)fmt_alloc(4);
        if (!r) { free(root); return NULL; }
        r[0] = root[0]; r[1] = ':'; r[2] = '/'; r[3] = 0;
        free(root); root = r;
    }
    for (char* p = root; *p; p++) if (*p == '\\') *p = '/';
    if (!is_dir(root)) { fmt_fail("install folder %s does not exist", root); free(root); return NULL; }
    if (!has_database(root)) {                                  /* the folder above "Archive" */
        char* sub = (char*)fmt_alloc(len + 16);
        if (!sub) { free(root); return NULL; }
        snprintf(sub, len + 16, "%s/Archive", root);
        if (is_dir(sub) && has_database(sub)) { free(root); root = sub; } else free(sub);
    }
    if (!has_database(root)) { fmt_fail("%s has no Database folder (not a Reflex install)", root); free(root); return NULL; }

    fmt_vfs* v = (fmt_vfs*)fmt_alloc(sizeof *v);
    if (!v) { free(root); return NULL; }
    v->root = root;
    fmt_mutex_init(&v->lock);
    for (int i = 0; i < FMT_VFS_HANDLES; i++) v->slots[i].entry = -1;
    entry_list l = { 0, 0, 0 };
    if (scan(root, "", &l, 0) < 0) {
        for (int i = 0; i < l.n; i++) free(l.a[i].path);
        free(l.a); fmt_vfs_close(v);
        return NULL;
    }
    qsort(l.a, (size_t)l.n, sizeof *l.a, entry_cmp);
    v->files = l.a; v->nfiles = l.n;
    if (build_table(v) < 0) { fmt_vfs_close(v); return NULL; }
    if (attach_archives(v) < 0) { fmt_vfs_close(v); return NULL; }
    return v;
}

fmt_vfs* fmt_vfs_open(const char* root_utf8) { fmt_clear_error(); return open_utf8(root_utf8); }

#if defined(_WIN32)
fmt_vfs* fmt_vfs_open_w(const wchar_t* root)
{
    fmt_clear_error();
    char* u = root ? narrow(root) : NULL;
    if (!u) { fmt_fail("no install folder"); return NULL; }
    fmt_vfs* v = open_utf8(u);
    free(u);
    return v;
}
#endif

void fmt_vfs_close(fmt_vfs* v)
{
    if (!v) return;
    for (int i = 0; i < FMT_VFS_HANDLES; i++) if (v->slots[i].entry >= 0) close_file(v->slots[i].file);
    fmt_mutex_destroy(&v->lock);
    for (int i = 0; i < v->nfiles; i++) { free(v->files[i].path); free(v->files[i].real); }
    for (int i = 0; i < v->narcs; i++) fmt_rxar_close(v->arcs[i]);
    free(v->arcs);
    free(v->files); free(v->table); free(v->root);
    free(v);
}

const char* fmt_vfs_root(const fmt_vfs* v) { return v ? v->root : ""; }
int fmt_vfs_file_count(const fmt_vfs* v) { return v ? v->nfiles : 0; }
const char* fmt_vfs_file_path(const fmt_vfs* v, int i) { return v && i >= 0 && i < v->nfiles ? v->files[i].path : NULL; }
uint64_t fmt_vfs_file_size(const fmt_vfs* v, int i) { return v && i >= 0 && i < v->nfiles ? v->files[i].size : 0; }

int fmt_vfs_find(const fmt_vfs* v, const char* path)
{
    if (!v || !path) return -1;
    while (*path == '/' || *path == '\\') path++;
    /* fold separators for the comparison: copy once */
    char tmp[1024];
    size_t n = strlen(path);
    if (n >= sizeof tmp) return -1;
    for (size_t i = 0; i <= n; i++) tmp[i] = path[i] == '\\' ? '/' : path[i];
    uint32_t h = fmt_hash_ci(tmp) & v->mask;
    for (;;) {
        int cur = v->table[h];
        if (!cur) return -1;
        if (!fmt_stricmp(v->files[cur - 1].path, tmp)) return cur - 1;
        h = (h + 1) & v->mask;
    }
}

int fmt_vfs_exists(const fmt_vfs* v, const char* path) { return fmt_vfs_find(v, path) >= 0; }
int64_t fmt_vfs_size(const fmt_vfs* v, const char* path) { int i = fmt_vfs_find(v, path); return i < 0 ? -1 : (int64_t)v->files[i].size; }

/* ------------------------------------------------------------------------------------------------ reading */

int fmt_vfs_read_at(const fmt_vfs* cv, int index, uint64_t offset, void* dst, size_t n)
{
    fmt_vfs* v = (fmt_vfs*)cv;                                  /* the handle cache is internal state */
    if (!v || index < 0 || index >= v->nfiles) return fmt_fail("bad file index %d", index);
    const vfs_entry* e = &v->files[index];
    if (offset > e->size || n > e->size - offset) return fmt_fail("%s: read of %llu bytes at %llu is past the end (%llu)", e->path,
                                                                  (unsigned long long)n, (unsigned long long)offset, (unsigned long long)e->size);
    if (n == 0) return 0;
    if (e->arc >= 0) return fmt_rxar_read_at(v->arcs[e->arc], e->arc_index, offset, dst, n);   /* packed (fmt_rxar: thread safe) */
    vfs_slot* slot = NULL;
    fmt_mutex_lock(&v->lock);
    for (int i = 0; i < FMT_VFS_HANDLES; i++) if (v->slots[i].entry == index) { slot = &v->slots[i]; break; }
    if (!slot) {
        vfs_slot* victim = NULL;
        for (int i = 0; i < FMT_VFS_HANDLES; i++) {
            vfs_slot* s = &v->slots[i];
            if (s->entry < 0) { victim = s; break; }
            if (s->users == 0 && (!victim || s->tick < victim->tick)) victim = s;
        }
        char* full = (char*)fmt_alloc(strlen(v->root) + strlen(e->path) + (e->real ? strlen(e->real) : 0) + 2);
        if (!full) { fmt_mutex_unlock(&v->lock); return -1; }
        if (e->real) strcpy(full, e->real);                     /* a mounted file (fmt_vfs_mount_dir) */
        else sprintf(full, "%s/%s", v->root, e->path);
        fmt_file f = open_file(full);
        free(full);
        if (f == FMT_BAD_FILE) {
            /* a loose file packed and removed since the file system was opened (PACK GAME FILES while a session lives): its archive */
            if (!e->real && adopt_packed(v, index) == 0) {
                fmt_rxar* ar = v->arcs[v->files[index].arc]; int ai = v->files[index].arc_index;
                fmt_mutex_unlock(&v->lock);
                return fmt_rxar_read_at(ar, ai, offset, dst, n);
            }
            fmt_mutex_unlock(&v->lock);
            return fmt_fail("cannot open %s", e->path);
        }
        if (victim) {
            if (victim->entry >= 0) close_file(victim->file);
            victim->entry = index; victim->file = f; victim->users = 0;
            slot = victim;
        } else {                                                /* every handle busy: read through a private one */
            fmt_mutex_unlock(&v->lock);
            int rc = pread_all(f, offset, dst, n);
            close_file(f);
            return rc < 0 ? fmt_fail("%s: read error", e->path) : 0;
        }
    }
    slot->users++;
    slot->tick = ++v->tick;
    fmt_file f = slot->file;
    fmt_mutex_unlock(&v->lock);
    int rc = pread_all(f, offset, dst, n);
    fmt_mutex_lock(&v->lock);
    slot->users--;
    fmt_mutex_unlock(&v->lock);
    return rc < 0 ? fmt_fail("%s: read error", e->path) : 0;
}

int fmt_vfs_read_file(const fmt_vfs* v, const char* path, fmt_buf* out)
{
    out->data = NULL; out->size = 0;
    int i = fmt_vfs_find(v, path);
    if (i < 0) return fmt_fail("not found: %s", path ? path : "(null)");
    uint64_t size = v->files[i].size;
    if (size > 0x7fffffffu) return fmt_fail("%s: file too large", path);
    uint8_t* d = (uint8_t*)fmt_alloc((size_t)size + 1);
    if (!d) return -1;
    if (fmt_vfs_read_at(v, i, 0, d, (size_t)size) < 0) { free(d); return -1; }
    out->data = d; out->size = (size_t)size;
    return 0;
}

static inline int lower_c(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

int fmt_vfs_list(const fmt_vfs* v, const char* dir, fmt_vfs_list_fn fn, void* user)
{
    if (!v) return 0;
    char prefix[512];
    size_t pl = 0;
    if (dir) {
        while (*dir == '/' || *dir == '\\') dir++;
        pl = strlen(dir);
        while (pl && (dir[pl - 1] == '/' || dir[pl - 1] == '\\')) pl--;
        if (pl + 2 > sizeof prefix) return 0;
        for (size_t i = 0; i < pl; i++) prefix[i] = dir[i] == '\\' ? '/' : dir[i];
        if (pl) prefix[pl++] = '/';
    }
    prefix[pl] = 0;
    for (int i = 0; i < v->nfiles; i++) {
        const char* p = v->files[i].path;
        size_t k = 0;
        while (k < pl && lower_c(p[k]) == lower_c(prefix[k])) k++;
        if (k < pl) continue;
        if (strchr(p + pl, '/')) continue;                      /* below a sub folder */
        int r = fn(p + pl, v->files[i].size, user);
        if (r) return r;
    }
    return 0;
}

/* ------------------------------------------------------------------------------------------------ mounts (Mods/Tracks) */

int fmt_vfs_mount_dir(fmt_vfs* v, const char* vdir, const char* real_dir, const char* stem)
{
    if (!v || !vdir || !real_dir || !*real_dir || !stem || !*stem) return fmt_fail("mount: no folder or name");
    size_t dl = strlen(real_dir);
    while (dl > 1 && (real_dir[dl - 1] == '/' || real_dir[dl - 1] == '\\')) dl--;
    char* dir = fmt_strndup(real_dir, dl);
    if (!dir) return -1;
    for (char* p = dir; *p; p++) if (*p == '\\') *p = '/';
    if (!is_dir(dir)) { fmt_fail("mount: folder %s does not exist", dir); free(dir); return -1; }
    entry_list l = { 0, 0, 0 };
    if (scan(dir, "", &l, 16) < 0) {                            /* depth 16: only the folder's own files, no sub folder */
        for (int i = 0; i < l.n; i++) free(l.a[i].path);
        free(l.a); free(dir);
        return -1;
    }
    /* the file indices change: every cached handle is closed first */
    fmt_mutex_lock(&v->lock);
    for (int i = 0; i < FMT_VFS_HANDLES; i++)
        if (v->slots[i].entry >= 0) { close_file(v->slots[i].file); v->slots[i].entry = -1; v->slots[i].users = 0; }
    fmt_mutex_unlock(&v->lock);
    const size_t sl = strlen(stem);
    int mounted = 0, rc = 0;
    for (int k = 0; k < l.n && rc == 0; k++) {
        const char* name = l.a[k].path;
        if (strchr(name, '/')) continue;
        size_t i = 0;
        while (i < sl && name[i] && lower_c((unsigned char)name[i]) == lower_c((unsigned char)stem[i])) i++;
        if (i < sl || name[sl] != '.') continue;
        char vpath[1024];
        const char* vd = vdir;
        while (*vd == '/' || *vd == '\\') vd++;
        if (snprintf(vpath, sizeof vpath, "%s%s%s", vd, *vd ? "/" : "", name) >= (int)sizeof vpath) { rc = fmt_fail("mount: path too long"); break; }
        for (char* p = vpath; *p; p++) if (*p == '\\') *p = '/';
        char* real = (char*)fmt_alloc(dl + strlen(name) + 2);
        if (!real) { rc = -1; break; }
        sprintf(real, "%s/%s", dir, name);
        const int at = fmt_vfs_find(v, vpath);
        if (at >= 0) { free(v->files[at].real); v->files[at].real = real; v->files[at].size = l.a[k].size; v->files[at].arc = -1; v->files[at].arc_index = -1; v->files[at].mounted = 1; }
        else {
            vfs_entry* a = (vfs_entry*)fmt_realloc(v->files, ((size_t)v->nfiles + 1) * sizeof *a);
            if (!a) { free(real); rc = -1; break; }
            v->files = a;
            vfs_entry* e = &v->files[v->nfiles];
            e->path = fmt_strdup(vpath);
            if (!e->path) { free(real); rc = -1; break; }
            e->size = l.a[k].size; e->hash = fmt_hash_ci(e->path); e->real = real; e->arc = -1; e->arc_index = -1; e->mounted = 1;
            v->nfiles++;
            qsort(v->files, (size_t)v->nfiles, sizeof *v->files, entry_cmp);
            if (build_table(v) < 0) { rc = -1; break; }
        }
        mounted++;
    }
    for (int i = 0; i < l.n; i++) free(l.a[i].path);
    free(l.a); free(dir);
    return rc < 0 ? -1 : mounted;
}

int fmt_vfs_mounted(const fmt_vfs* v, const char* path)
{
    const int i = fmt_vfs_find(v, path);
    return i >= 0 && (v->files[i].real != NULL || v->files[i].mounted);
}

int fmt_vfs_packed(const fmt_vfs* v, const char* path)
{
    const int i = fmt_vfs_find(v, path);
    return i >= 0 && v->files[i].arc >= 0;
}

int fmt_vfs_archive_count(const fmt_vfs* v) { return v ? v->narcs : 0; }
const char* fmt_vfs_archive_file(const fmt_vfs* v, int i) { return v && i >= 0 && i < v->narcs ? fmt_rxar_file(v->arcs[i]) : NULL; }

/* ------------------------------------------------------------------------------------------------ archives (core/plan/RXARCHIVE.md) */

#define FMT_VFS_MAX_ARCHIVES 4096
/* the archive table never moves (readers index it without the lock while adopt_packed may add to it) */
static int keep_archive(fmt_vfs* v, fmt_rxar* a)
{
    if (!v->arcs && !(v->arcs = (fmt_rxar**)fmt_alloc(FMT_VFS_MAX_ARCHIVES * sizeof *v->arcs))) return -1;
    if (v->narcs >= FMT_VFS_MAX_ARCHIVES) return fmt_fail("more than %d archives", FMT_VFS_MAX_ARCHIVES);
    v->arcs[v->narcs] = a;
    return v->narcs++;
}

static int fmt_stricmp_n(const char* a, const char* b, size_t n)
{
    for (size_t i = 0; i < n; i++) { int x = lower_c((unsigned char)a[i]), y = lower_c((unsigned char)b[i]); if (x != y || !x) return x - y; }
    return 0;
}

static int has_suffix_ci(const char* s, const char* suffix)
{
    size_t n = strlen(s), k = strlen(suffix);
    return n > k && !fmt_stricmp(s + n - k, suffix);
}

/* A pool reference (entry j of archive a): the archive and index that hold its bytes (Pool/<sha256> in an archive of the root). 0, or -1. */
static int pool_lookup(const fmt_vfs* v, const fmt_rxar* a, int j, int* arc, int* idx)
{
    uint8_t sha[32]; char pool[71];
    if (!fmt_rxar_sha(a, j, sha)) return -1;
    fmt_rxar_pool_path(sha, pool);
    for (int k = 0; k < v->narcs; k++) {
        if (fmt_rxar_kind(v->arcs[k]) != FMT_RXAR_DATA) continue;   /* the pool lives in the .rxdata (one find per reference, not one per archive) */
        int i = fmt_rxar_find(v->arcs[k], pool);
        if (i >= 0 && !fmt_rxar_external(v->arcs[k], i) && fmt_rxar_size(v->arcs[k], i) == fmt_rxar_size(a, j)) { *arc = k; *idx = i; return 0; }
    }
    return -1;
}

/* The root's own archives (the import's packed game data and packed tracks), below the loose files: a loose file of the same path
 * wins (an edit, a mod, a file written after the packing), then Tracks/<level>.rxtrack, then the .rxdata files in name order. The
 * environment's REFLEX_NO_ARCHIVES=1 leaves them all out (the loose-file comparison of tools/rxpack verify). */
static int attach_archives(fmt_vfs* v)
{
    const char* off = getenv("REFLEX_NO_ARCHIVES");
    if (off && *off && *off != '0') return 0;
    char** names = NULL; int nn = 0;
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < v->nfiles; i++) {
            const char* p = v->files[i].path;
            int track = pass == 0 && strlen(p) > 7 && lower_c((unsigned char)p[0]) == 't' && !fmt_stricmp_n(p, "tracks/", 7) && !strchr(p + 7, '/') && has_suffix_ci(p, ".rxtrack");
            int data = pass == 1 && !strchr(p, '/') && has_suffix_ci(p, ".rxdata");
            if (!track && !data) continue;
            char** a = (char**)fmt_realloc(names, ((size_t)nn + 1) * sizeof *a);
            if (!a) { free(names); return -1; }
            names = a; names[nn++] = v->files[i].path;
        }
    if (!nn) { free(names); return 0; }
    entry_list extra = { 0, 0, 0 };
    int* seen = NULL; uint32_t smask = 0;                       /* the accepted archive paths (open addressing over extra) */
    int rc = 0, packed = 0, hidden = 0, unresolved = 0;
    /* every archive open first: a track archive's pool references resolve into the .rxdata's Pool/ */
    for (int k = 0; k < nn; k++) {
        char full[4200];
        snprintf(full, sizeof full, "%s/%s", v->root, names[k]);
        if (fmt_rxar_sniff(full) != 1) continue;                 /* a legacy .rxtrack (gzip RXPK) or a damaged file: not an overlay */
        fmt_rxar* a = fmt_rxar_open(full);
        if (!a) { fprintf(stderr, "rxcore: archive %s skipped: %s\n", names[k], fmt_error()); fmt_clear_error(); continue; }
        if (keep_archive(v, a) < 0) { fmt_rxar_close(a); free(names); return -1; }
    }
    for (int id = 0; id < v->narcs && rc == 0; id++) {
        fmt_rxar* a = v->arcs[id];
        const int count = fmt_rxar_count(a);
        uint32_t need = 16;
        while (need < (uint32_t)(extra.n + count) * 2u) need <<= 1;
        if (need - 1 > smask) {                                  /* grow the set: re-insert what is accepted so far */
            int* t = (int*)fmt_alloc(need * sizeof *t);
            if (!t) { rc = -1; break; }
            free(seen); seen = t; smask = need - 1;
            for (int j = 0; j < extra.n; j++) { uint32_t h = extra.a[j].hash & smask; while (seen[h]) h = (h + 1) & smask; seen[h] = j + 1; }
        }
        int used = 0;
        for (int j = 0; j < count; j++) {
            const char* p = fmt_rxar_path(a, j);
            if (fmt_vfs_find(v, p) >= 0) continue;              /* a loose file wins */
            int ra = id, ri = j;
            if (fmt_rxar_external(a, j) && pool_lookup(v, a, j, &ra, &ri) < 0) { unresolved++; continue; }
            uint32_t hh = fmt_hash_ci(p), h = hh & smask; int dup = 0;
            while (seen[h]) { if (!fmt_stricmp(extra.a[seen[h] - 1].path, p)) { dup = 1; break; } h = (h + 1) & smask; }
            if (dup) continue;                                   /* an earlier archive has it */
            if (list_add(&extra, p, fmt_rxar_size(a, j)) < 0) { rc = -1; break; }
            extra.a[extra.n - 1].arc = ra; extra.a[extra.n - 1].arc_index = ri;
            seen[h] = extra.n;
            used++;
        }
        packed += used; hidden += count - used;
    }
    if (v->narcs) fprintf(stderr, "rxcore: %d archives below %s: %d packed files (%d under loose files or earlier archives)\n", v->narcs, v->root, packed, hidden);
    if (unresolved) fprintf(stderr, "rxcore: %d pool references of %s have no Pool/ file in its .rxdata (those files are missing)\n", unresolved, v->root);
    free(seen); free(names);
    if (rc == 0 && extra.n) {
        vfs_entry* all = (vfs_entry*)fmt_realloc(v->files, ((size_t)v->nfiles + (size_t)extra.n) * sizeof *all);
        if (!all) rc = -1;
        else {
            v->files = all;
            memcpy(v->files + v->nfiles, extra.a, (size_t)extra.n * sizeof *extra.a);
            v->nfiles += extra.n;
            free(extra.a); extra.a = NULL; extra.n = 0;
            qsort(v->files, (size_t)v->nfiles, sizeof *v->files, entry_cmp);
            rc = build_table(v);
        }
    }
    for (int i = 0; i < extra.n; i++) free(extra.a[i].path);
    free(extra.a);
    return rc;
}

/* (called with v->lock held) entry index, a loose file that is gone, found in an archive of the root that has it with the same size: the entry
 * reads from there from now on. Archives that appeared after the open are opened here. 0, or -1 when no archive has it. */
static int adopt_packed(fmt_vfs* v, int index)
{
    const char* off = getenv("REFLEX_NO_ARCHIVES");
    if (off && *off && *off != '0') return -1;
    vfs_entry* e = &v->files[index];
    for (int k = 0; k < v->narcs; k++) {                        /* the archives already open */
        int j = fmt_rxar_find(v->arcs[k], e->path), ra = k, ri = j;
        if (j >= 0 && fmt_rxar_size(v->arcs[k], j) == e->size && (!fmt_rxar_external(v->arcs[k], j) || pool_lookup(v, v->arcs[k], j, &ra, &ri) == 0)) { e->arc = ra; e->arc_index = ri; return 0; }
    }
    entry_list l = { 0, 0, 0 };
    if (scan(v->root, "", &l, 16) < 0 || scan(v->root, "Tracks", &l, 16) < 0) { for (int i = 0; i < l.n; i++) free(l.a[i].path); free(l.a); return -1; }
    int rc = -1;
    for (int i = 0; i < l.n && rc < 0; i++) {
        const char* p = l.a[i].path;
        if (!has_suffix_ci(p, ".rxdata") && !has_suffix_ci(p, ".rxtrack")) continue;
        char full[4200];
        snprintf(full, sizeof full, "%s/%s", v->root, p);
        int known = 0;
        for (int k = 0; k < v->narcs && !known; k++) known = !strcmp(fmt_rxar_file(v->arcs[k]), full);
        if (known || fmt_rxar_sniff(full) != 1) continue;
        fmt_rxar* a = fmt_rxar_open(full);
        if (!a) { fmt_clear_error(); continue; }
        int j = fmt_rxar_find(a, e->path);
        int id = keep_archive(v, a);
        if (id < 0) { fmt_rxar_close(a); break; }
        int ra = id, ri = j;
        if (j >= 0 && fmt_rxar_size(a, j) == e->size && (!fmt_rxar_external(a, j) || pool_lookup(v, a, j, &ra, &ri) == 0)) { e->arc = ra; e->arc_index = ri; rc = 0; }
    }
    for (int i = 0; i < l.n; i++) free(l.a[i].path);
    free(l.a);
    return rc;
}

int fmt_vfs_mount_archive(fmt_vfs* v, const char* vdir, const char* archive, const char* prefix, const char* stem)
{
    if (!v || !vdir || !archive || !*archive) return fmt_fail("mount: no archive");
    fmt_rxar* a = fmt_rxar_open(archive);
    if (!a) return -1;
    const size_t pl = prefix ? strlen(prefix) : 0, sl = stem ? strlen(stem) : 0;
    const char* vd = vdir;
    while (*vd == '/' || *vd == '\\') vd++;
    int id = -1, mounted = 0, rc = 0;
    fmt_mutex_lock(&v->lock);                                   /* the file indices change: every cached handle is closed first */
    for (int i = 0; i < FMT_VFS_HANDLES; i++)
        if (v->slots[i].entry >= 0) { close_file(v->slots[i].file); v->slots[i].entry = -1; v->slots[i].users = 0; }
    fmt_mutex_unlock(&v->lock);
    for (int j = 0; j < fmt_rxar_count(a) && rc == 0; j++) {
        const char* p = fmt_rxar_path(a, j);
        if (pl && (strlen(p) <= pl || fmt_stricmp(p, prefix) == 0)) continue;
        size_t k = 0;
        while (k < pl && lower_c((unsigned char)p[k]) == lower_c((unsigned char)prefix[k])) k++;
        if (k < pl) continue;
        const char* name = p + pl;
        if (strchr(name, '/')) continue;
        if (sl) {
            size_t i = 0;
            while (i < sl && name[i] && lower_c((unsigned char)name[i]) == lower_c((unsigned char)stem[i])) i++;
            if (i < sl || name[sl] != '.') continue;
        }
        if (id < 0 && (id = keep_archive(v, a)) < 0) { rc = -1; break; }
        char vpath[1024];
        if (snprintf(vpath, sizeof vpath, "%s%s%s", vd, *vd ? "/" : "", name) >= (int)sizeof vpath) { rc = fmt_fail("mount: path too long"); break; }
        for (char* q = vpath; *q; q++) if (*q == '\\') *q = '/';
        int ra = id, ri = j;
        if (fmt_rxar_external(a, j) && pool_lookup(v, a, j, &ra, &ri) < 0) { rc = fmt_fail("mount: %s of %s is in a pool this install does not have", p, archive); break; }
        const int at = fmt_vfs_find(v, vpath);
        if (at >= 0) {
            vfs_entry* e = &v->files[at];
            free(e->real); e->real = NULL; e->size = fmt_rxar_size(a, j); e->arc = ra; e->arc_index = ri; e->mounted = 1;
        } else {
            vfs_entry* arr = (vfs_entry*)fmt_realloc(v->files, ((size_t)v->nfiles + 1) * sizeof *arr);
            if (!arr) { rc = -1; break; }
            v->files = arr;
            vfs_entry* e = &v->files[v->nfiles];
            e->path = fmt_strdup(vpath);
            if (!e->path) { rc = -1; break; }
            e->size = fmt_rxar_size(a, j); e->hash = fmt_hash_ci(e->path); e->real = NULL; e->arc = ra; e->arc_index = ri; e->mounted = 1;
            v->nfiles++;
            qsort(v->files, (size_t)v->nfiles, sizeof *v->files, entry_cmp);
            if (build_table(v) < 0) { rc = -1; break; }
        }
        mounted++;
    }
    if (id < 0) fmt_rxar_close(a);                              /* nothing of it mounted (or failed before): not kept */
    return rc < 0 ? -1 : mounted;
}
