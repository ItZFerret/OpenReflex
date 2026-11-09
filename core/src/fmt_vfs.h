/* fmt_vfs.h -- the Reflex install as a read-only file system.
 *
 * The root is the install folder that holds MXReflex.exe and Database/ ("game files/Archive" in the repository). A path given
 * to open() may also be the folder above it (the root then is <path>/Archive). Every file below the root is indexed once at
 * open; lookups fold case and treat '/' and '\\' alike (the files were authored on Windows; the Mac / Linux file systems may be
 * case sensitive). Paths are UTF-8 on every platform; on Windows they are converted to wide characters for the file API
 * (fmt_vfs_open_w takes a wide root directly).
 *
 * Reads are positional (pread / overlapped ReadFile) on a small cache of open handles, so any number of threads may read at
 * once. Nothing is ever written. */
#ifndef FMT_VFS_H
#define FMT_VFS_H

#include "fmt_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fmt_vfs fmt_vfs;

fmt_vfs*    fmt_vfs_open(const char* root_utf8);
#if defined(_WIN32)
fmt_vfs*    fmt_vfs_open_w(const wchar_t* root);
#endif
void        fmt_vfs_close(fmt_vfs* v);
const char* fmt_vfs_root(const fmt_vfs* v);                     /* the resolved root, UTF-8, no trailing separator */

int         fmt_vfs_file_count(const fmt_vfs* v);
const char* fmt_vfs_file_path(const fmt_vfs* v, int index);     /* path below the root with '/', as it is on disk */
uint64_t    fmt_vfs_file_size(const fmt_vfs* v, int index);
int         fmt_vfs_find(const fmt_vfs* v, const char* path);   /* index or -1; case and separator insensitive */
int         fmt_vfs_exists(const fmt_vfs* v, const char* path);
int64_t     fmt_vfs_size(const fmt_vfs* v, const char* path);   /* -1 when absent */

/* Reads n bytes at offset into dst; 0, or -1 when the range is outside the file or a read fails. */
int         fmt_vfs_read_at(const fmt_vfs* v, int index, uint64_t offset, void* dst, size_t n);
int         fmt_vfs_read_file(const fmt_vfs* v, const char* path, fmt_buf* out);   /* the whole file (malloc'ed) */

/* MODS/TRACKS (port addition): a file of another folder seen at a path of the install, for this vfs only (the install on disk is never
 * touched). fmt_vfs_mount_dir mounts every file directly inside real_dir whose name starts with stem + "." (case-insensitive, e.g. stem
 * "Beta_Track_Slot_1": its .dx9.level / .database / .package / .scene) as vdir/<its name>; a mounted path replaces an install file of
 * the same path (case-insensitive). Returns the number of files mounted, -1 on failure (fmt_error). Not concurrent with reads: mount
 * before the files are read (the open file handles are closed, file indices change). fmt_vfs_mounted: 1 when path is a mounted file. */
int         fmt_vfs_mount_dir(fmt_vfs* v, const char* vdir, const char* real_dir, const char* stem);
int         fmt_vfs_mounted(const fmt_vfs* v, const char* path);

/* ARCHIVES (port addition, core/plan/RXARCHIVE.md): the root's own Reflex archives are part of the file system - every *.rxdata at the
 * root and every Tracks/<name>.rxtrack (version 5, an RXAR file) - BELOW the loose files: a loose file of the same path wins, then the
 * track archives, then the .rxdata files (name order). Their files read like any other (fmt_vfs_read_at decodes only what a read
 * covers). REFLEX_NO_ARCHIVES=1 in the environment leaves them out (comparisons against the loose files).
 * fmt_vfs_mount_archive mounts the files of an archive (a Mods/Tracks .rxtrack) the way fmt_vfs_mount_dir mounts a folder's: those
 * directly below `prefix` ("Database/") whose name starts with stem + "." (stem NULL / "": all of them) as vdir/<name>, above
 * everything (a mounted file). Not concurrent with reads. Returns the number of files mounted, -1 on failure.
 * fmt_vfs_packed: 1 when path is read from an archive. fmt_vfs_archive_count / _file: the archives in use (diagnostics). */
int         fmt_vfs_mount_archive(fmt_vfs* v, const char* vdir, const char* archive, const char* prefix, const char* stem);
int         fmt_vfs_packed(const fmt_vfs* v, const char* path);
int         fmt_vfs_archive_count(const fmt_vfs* v);
const char* fmt_vfs_archive_file(const fmt_vfs* v, int index);

/* Calls fn(name, size, user) for every file directly inside dir ("" or "Database"), in name order; stops (and returns the
 * value) when fn returns non-zero. Returns 0 after the last file. */
typedef int (*fmt_vfs_list_fn)(const char* name, uint64_t size, void* user);
int         fmt_vfs_list(const fmt_vfs* v, const char* dir, fmt_vfs_list_fn fn, void* user);

#ifdef __cplusplus
}
#endif
#endif
