/* fmt_rxar.h -- Reflex archives: the indexed, compressed container of .rxdata (the imported game data) and .rxtrack version 5
 * (one track: a level's own files). Specification: core/plan/RXARCHIVE.md.
 *
 * An archive is a set of files by path ('/' separators, below the game root: "Assets/texture/NAT_Concord/...", "Database/X.dx9.level").
 * Each file is stored whole, or as one zstd frame, or as independent zstd frames of 2^chunk_log2 raw bytes (large files: a read of a
 * range decodes only the chunks it covers). A compressed index at the end lists the files; nothing has to be unpacked to read one.
 *
 * Reading is thread safe (positional reads on one handle, a pool of decoder contexts). Writing: fmt_rxar_add may be called from any
 * number of threads at once (each call compresses on its own thread, then appends); the archive is written to <path>.partial and only
 * becomes <path> at fmt_rxar_finish, so an interrupted write never leaves a file that looks complete. */
#ifndef FMT_RXAR_H
#define FMT_RXAR_H

#include "fmt_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FMT_RXAR_DATA 0x41544144u   /* "DATA": a .rxdata (game data) */
#define FMT_RXAR_TRAK 0x4B415254u   /* "TRAK": a .rxtrack version 5 (one track) */

#define FMT_RXAR_STORE    1u        /* fmt_rxar_add: keep the bytes as they are (already compressed data) */
#define FMT_RXAR_MAX_PATH 1023

typedef struct fmt_rxar fmt_rxar;
typedef struct fmt_rxar_writer fmt_rxar_writer;

typedef struct fmt_rxar_info {
    uint8_t  codec;                 /* 0 stored, 1 zstd */
    uint8_t  chunk_log2;            /* 0: one frame; else the raw size of each chunk is 1 << chunk_log2 */
    uint64_t raw, stored, offset;   /* raw size, stored bytes, file offset of the first stored byte */
    uint32_t crc;                   /* CRC-32 (zlib) of the raw bytes */
} fmt_rxar_info;

/* 1 when the file starts with an archive header, 0 when it does not, -1 when it cannot be read. */
int         fmt_rxar_sniff(const char* path_utf8);

fmt_rxar*   fmt_rxar_open(const char* path_utf8);
void        fmt_rxar_close(fmt_rxar* a);
uint32_t    fmt_rxar_kind(const fmt_rxar* a);
uint32_t    fmt_rxar_version(const fmt_rxar* a);                       /* 1, or 2 (SHA-256 per entry, dedupe, pool references) */
const char* fmt_rxar_file(const fmt_rxar* a);                    /* the archive's own path */
const char* fmt_rxar_meta(const fmt_rxar* a, size_t* length);    /* the UTF-8 JSON metadata ("" when none), NUL-terminated */
int         fmt_rxar_count(const fmt_rxar* a);
const char* fmt_rxar_path(const fmt_rxar* a, int index);         /* sorted by path (byte order) */
uint64_t    fmt_rxar_size(const fmt_rxar* a, int index);
int         fmt_rxar_info_of(const fmt_rxar* a, int index, fmt_rxar_info* out);
int         fmt_rxar_find(const fmt_rxar* a, const char* path);  /* index or -1; case and separator insensitive */
/* n bytes at offset of file index into dst: 0, or -1 (range outside the file, read or decode failure, checksum mismatch). */
int         fmt_rxar_read_at(const fmt_rxar* a, int index, uint64_t offset, void* dst, size_t n);
int         fmt_rxar_read(const fmt_rxar* a, int index, fmt_buf* out);       /* the whole file (malloc'ed), CRC checked */
/* Container version 2 (RXARCHIVE.md "Dedupe"): every entry written since carries the SHA-256 of its bytes; a file whose bytes another
 * entry of the same archive already has is stored once (two paths, one range); a POOL REFERENCE (codec 2) has no bytes here: they are the
 * file Pool/<sha256 hex> of an archive of the same game root (the library's Assets.rxdata), resolved by the file system (fmt_vfs). */
int         fmt_rxar_sha(const fmt_rxar* a, int index, uint8_t sha[32]);    /* 1 and the SHA-256 when the entry has one */
int         fmt_rxar_external(const fmt_rxar* a, int index);                /* 1: a pool reference (fmt_rxar_read_at refuses it) */
void        fmt_rxar_pool_path(const uint8_t sha[32], char out[71]);        /* "Pool/<64 hex>" */

/* level: zstd level 1..19 (0: 9). kind: FMT_RXAR_DATA / FMT_RXAR_TRAK. */
fmt_rxar_writer* fmt_rxar_create(const char* path_utf8, uint32_t kind, int level);
int         fmt_rxar_add(fmt_rxar_writer* w, const char* path, const void* data, size_t n, unsigned flags);   /* bytes already in: stored once */
int         fmt_rxar_add_ref(fmt_rxar_writer* w, const char* path, const void* data, size_t n);   /* a pool reference to these bytes */
int         fmt_rxar_add_external(fmt_rxar_writer* w, const char* path, uint64_t raw, uint32_t crc, const uint8_t sha[32]);   /* a pool reference */
uint64_t    fmt_rxar_deduped(const fmt_rxar_writer* w, uint64_t* external);   /* raw bytes stored once for several paths (and left to the pool) */
int         fmt_rxar_set_meta(fmt_rxar_writer* w, const char* json, size_t n);
uint64_t    fmt_rxar_written(const fmt_rxar_writer* w, uint64_t* raw);   /* stored bytes so far (and the raw bytes) */
int         fmt_rxar_finish(fmt_rxar_writer* w);     /* writes the index, renames <path>.partial to <path>; frees w either way */
void        fmt_rxar_abort(fmt_rxar_writer* w);      /* removes <path>.partial; frees w */

/* A path an archive may hold: relative, '/' separators, no empty / "." / ".." component, no '\\', ':' or control byte. */
int         fmt_rxar_path_ok(const char* path);

/* SHA-256 (the online track identity is the files' SHA-256, Online/Core/OnlineTrackMod.cs). */
typedef struct { uint32_t h[8]; uint64_t bytes; uint8_t block[64]; size_t used; } fmt_sha256;
void        fmt_sha256_init(fmt_sha256* s);
void        fmt_sha256_add(fmt_sha256* s, const void* data, size_t n);
void        fmt_sha256_hex(fmt_sha256* s, char out[65]);
void        fmt_sha256_bin(fmt_sha256* s, uint8_t out[32]);

#ifdef __cplusplus
}
#endif
#endif
