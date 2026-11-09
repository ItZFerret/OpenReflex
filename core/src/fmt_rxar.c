/* fmt_rxar.c -- see fmt_rxar.h; the byte layout is core/plan/RXARCHIVE.md. */
#if !defined(_WIN32)
#  define _POSIX_C_SOURCE 200809L
#  define _FILE_OFFSET_BITS 64
#  if defined(__APPLE__)
#    define _DARWIN_C_SOURCE 1
#  endif
#endif
#include "fmt_rxar.h"
#include "fmt_sys.h"

#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>

#define ZSTDLIB_VISIBLE
#define ZSTDERRORLIB_VISIBLE
#include "zstd/zstd.h"

#if defined(_WIN32)
typedef HANDLE rx_file;
#  define RX_BAD_FILE INVALID_HANDLE_VALUE
#else
#  include <fcntl.h>
#  include <sys/stat.h>
#  include <unistd.h>
typedef int rx_file;
#  define RX_BAD_FILE (-1)
#endif

#define HEADER_BYTES   64u
#define INDEX_MAGIC    0x58495852u          /* "RXIX" */
#define MAX_ENTRIES    (4u << 20)
#define MAX_INDEX_RAW  (512ull << 20)
#define MAX_META       (16u << 20)
#define CHUNK_LOG2     20u                  /* 1 MiB chunks ... */
#define CHUNK_ABOVE    (4ull << 20)         /* ... for files above 4 MiB */
#define MAX_FILE       (1ull << 40)
#define CODEC_STORE    0
#define CODEC_ZSTD     1
#define CODEC_EXTERNAL 2                    /* the bytes are the root's pool file Pool/<sha256> (container version 2) */
#define FLAG_SHA       1u                   /* the entry carries the SHA-256 of its raw bytes (container version 2) */

/* ------------------------------------------------------------------------------------------------ files */

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
static rx_file open_read(const char* path)
{
    wchar_t* w = widen(path);
    if (!w) return RX_BAD_FILE;
    HANDLE h = CreateFileW(w, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    free(w);
    return h;
}
static rx_file open_write(const char* path)
{
    wchar_t* w = widen(path);
    if (!w) return RX_BAD_FILE;
    HANDLE h = CreateFileW(w, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    free(w);
    return h;
}
static void close_file(rx_file f) { CloseHandle(f); }
static int file_size(rx_file f, uint64_t* size) { LARGE_INTEGER s; if (!GetFileSizeEx(f, &s)) return -1; *size = (uint64_t)s.QuadPart; return 0; }
static int pread_all(rx_file f, uint64_t off, void* dst, size_t n)
{
    uint8_t* p = (uint8_t*)dst;
    while (n) {
        DWORD want = n > 0x40000000u ? 0x40000000u : (DWORD)n, got = 0;
        OVERLAPPED ov; memset(&ov, 0, sizeof ov);
        ov.Offset = (DWORD)off; ov.OffsetHigh = (DWORD)(off >> 32);
        if (!ReadFile(f, p, want, &got, &ov) || got == 0) return -1;
        p += got; off += got; n -= got;
    }
    return 0;
}
static int pwrite_all(rx_file f, uint64_t off, const void* src, size_t n)
{
    const uint8_t* p = (const uint8_t*)src;
    while (n) {
        DWORD want = n > 0x40000000u ? 0x40000000u : (DWORD)n, put = 0;
        OVERLAPPED ov; memset(&ov, 0, sizeof ov);
        ov.Offset = (DWORD)off; ov.OffsetHigh = (DWORD)(off >> 32);
        if (!WriteFile(f, p, want, &put, &ov) || put == 0) return -1;
        p += put; off += put; n -= put;
    }
    return 0;
}
static int rename_over(const char* from, const char* to)
{
    wchar_t* a = widen(from); wchar_t* b = widen(to);
    int ok = a && b && MoveFileExW(a, b, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    free(a); free(b);
    return ok ? 0 : -1;
}
static void remove_file(const char* path) { wchar_t* w = widen(path); if (w) { DeleteFileW(w); free(w); } }
#else
static rx_file open_read(const char* path) { return open(path, O_RDONLY | O_CLOEXEC); }
static rx_file open_write(const char* path) { return open(path, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0644); }
static void close_file(rx_file f) { close(f); }
static int file_size(rx_file f, uint64_t* size) { struct stat st; if (fstat(f, &st) != 0) return -1; *size = (uint64_t)st.st_size; return 0; }
static int pread_all(rx_file f, uint64_t off, void* dst, size_t n)
{
    uint8_t* p = (uint8_t*)dst;
    while (n) {
        ssize_t got = pread(f, p, n > 0x40000000u ? 0x40000000u : n, (off_t)off);
        if (got <= 0) return -1;
        p += got; off += (uint64_t)got; n -= (size_t)got;
    }
    return 0;
}
static int pwrite_all(rx_file f, uint64_t off, const void* src, size_t n)
{
    const uint8_t* p = (const uint8_t*)src;
    while (n) {
        ssize_t put = pwrite(f, p, n > 0x40000000u ? 0x40000000u : n, (off_t)off);
        if (put <= 0) return -1;
        p += put; off += (uint64_t)put; n -= (size_t)put;
    }
    return 0;
}
static int rename_over(const char* from, const char* to) { return rename(from, to) == 0 ? 0 : -1; }
static void remove_file(const char* path) { unlink(path); }
#endif

static void put16(uint8_t* p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t* p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i)); }
static void put64(uint8_t* p, uint64_t v) { for (int i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (8 * i)); }
static uint64_t get64(const uint8_t* p) { return (uint64_t)fmt_u32(p) | (uint64_t)fmt_u32(p + 4) << 32; }

static uint32_t crc_of(const void* data, size_t n)
{
    uLong c = crc32(0L, Z_NULL, 0);
    const Bytef* p = (const Bytef*)data;
    while (n) { uInt k = n > 0x40000000u ? 0x40000000u : (uInt)n; c = crc32(c, p, k); p += k; n -= k; }
    return (uint32_t)c;
}

int fmt_rxar_path_ok(const char* path)
{
    if (!path || !*path) return 0;
    size_t n = strlen(path);
    if (n > FMT_RXAR_MAX_PATH || path[0] == '/' || path[n - 1] == '/') return 0;
    const char* c = path;
    for (const char* p = path;; p++) {
        if (*p == '/' || *p == 0) {
            size_t len = (size_t)(p - c);
            if (len == 0 || (len == 1 && c[0] == '.') || (len == 2 && c[0] == '.' && c[1] == '.')) return 0;
            if (!*p) break;
            c = p + 1;
            continue;
        }
        unsigned char u = (unsigned char)*p;
        if (u < 0x20 || u == 0x7f || u == '\\' || u == ':') return 0;
    }
    return 1;
}

/* ------------------------------------------------------------------------------------------------ zstd context pools */

typedef struct { void** items; int n, cap; fmt_mutex lock; } ctx_pool;

static void* pool_take(ctx_pool* p) { void* c = NULL; fmt_mutex_lock(&p->lock); if (p->n) c = p->items[--p->n]; fmt_mutex_unlock(&p->lock); return c; }
static void pool_give(ctx_pool* p, void* c, int decoder)
{
    fmt_mutex_lock(&p->lock);
    if (p->n == p->cap && p->cap < 64) {
        int cap = p->cap ? p->cap * 2 : 8;
        void** a = (void**)realloc(p->items, (size_t)cap * sizeof *a);
        if (a) { p->items = a; p->cap = cap; }
    }
    if (p->n < p->cap) { p->items[p->n++] = c; c = NULL; }
    fmt_mutex_unlock(&p->lock);
    if (c) { if (decoder) ZSTD_freeDCtx((ZSTD_DCtx*)c); else ZSTD_freeCCtx((ZSTD_CCtx*)c); }
}
static void pool_free(ctx_pool* p, int decoder)
{
    for (int i = 0; i < p->n; i++) { if (decoder) ZSTD_freeDCtx((ZSTD_DCtx*)p->items[i]); else ZSTD_freeCCtx((ZSTD_CCtx*)p->items[i]); }
    free(p->items);
    fmt_mutex_destroy(&p->lock);
}

/* ------------------------------------------------------------------------------------------------ reading */

typedef struct {
    char* path;
    uint8_t codec, chunk_log2;
    uint64_t raw, stored, offset;
    uint32_t crc, nchunks;
    uint64_t* chunk_at;            /* nchunks + 1 stored offsets relative to offset (chunked entries only) */
    uint8_t has_sha, sha[32];
} rx_entry;

struct fmt_rxar {
    char* file;
    rx_file f;
    uint32_t kind, version;
    char* meta; size_t meta_len;
    rx_entry* e; int n;
    int* table; uint32_t mask;
    ctx_pool dctx;
};

int fmt_rxar_sniff(const char* path)
{
    if (!path) return -1;
    rx_file f = open_read(path);
    if (f == RX_BAD_FILE) return -1;
    uint8_t b[4];
    int rc = pread_all(f, 0, b, 4) < 0 ? 0 : !memcmp(b, "RXAR", 4);
    close_file(f);
    return rc;
}

static void free_entries(rx_entry* e, int n) { for (int i = 0; i < n; i++) { free(e[i].path); free(e[i].chunk_at); } free(e); }

void fmt_rxar_close(fmt_rxar* a)
{
    if (!a) return;
    if (a->f != RX_BAD_FILE) close_file(a->f);
    free_entries(a->e, a->n);
    free(a->table); free(a->meta); free(a->file);
    pool_free(&a->dctx, 1);
    free(a);
}

static int build_table(fmt_rxar* a)
{
    uint32_t cap = 16;
    while (cap < (uint32_t)a->n * 2u) cap <<= 1;
    a->table = (int*)fmt_alloc(cap * sizeof *a->table);
    if (!a->table) return -1;
    a->mask = cap - 1;
    for (int i = 0; i < a->n; i++) {
        uint32_t h = fmt_hash_ci(a->e[i].path) & a->mask;
        while (a->table[h]) {
            if (!fmt_stricmp(a->e[a->table[h] - 1].path, a->e[i].path)) return fmt_fail("%s: two files named %s", a->file, a->e[i].path);
            h = (h + 1) & a->mask;
        }
        a->table[h] = i + 1;
    }
    return 0;
}

/* the raw index (RXARCHIVE.md "Index") into the entries; bounds-checked against the data region [64, data_end) */
static int parse_index(fmt_rxar* a, const uint8_t* p, size_t n, uint32_t count, uint64_t data_end)
{
    size_t at = 0;
#define NEED(k) do { if ((k) > n - at) return fmt_fail("%s: index truncated", a->file); } while (0)
    NEED(12);
    if (fmt_u32(p) != INDEX_MAGIC) return fmt_fail("%s: bad index magic", a->file);
    if (fmt_u32(p + 4) != count) return fmt_fail("%s: index count disagrees with the header", a->file);
    uint32_t ml = fmt_u32(p + 8); at = 12;
    if (ml > MAX_META) return fmt_fail("%s: metadata too large", a->file);
    NEED(ml);
    a->meta = (char*)fmt_alloc((size_t)ml + 1);
    if (!a->meta) return -1;
    memcpy(a->meta, p + at, ml); a->meta_len = ml; at += ml;
    a->e = (rx_entry*)fmt_alloc((size_t)(count ? count : 1) * sizeof *a->e);
    if (!a->e) return -1;
    for (uint32_t i = 0; i < count; i++) {
        rx_entry* e = &a->e[i];
        NEED(2);
        uint32_t pl = fmt_u16(p + at); at += 2;
        NEED((size_t)pl + 32);
        e->path = fmt_strndup((const char*)p + at, pl); at += pl;
        a->n = (int)i + 1;
        if (!e->path) return -1;
        if (strlen(e->path) != pl || !fmt_rxar_path_ok(e->path)) return fmt_fail("%s: bad file path in the index", a->file);
        if (i && strcmp(a->e[i - 1].path, e->path) >= 0) return fmt_fail("%s: index not sorted at %s", a->file, e->path);
        e->codec = p[at]; e->chunk_log2 = p[at + 1];
        uint32_t flags = fmt_u16(p + at + 2);
        e->raw = get64(p + at + 4); e->offset = get64(p + at + 12); e->stored = get64(p + at + 20); e->crc = fmt_u32(p + at + 28);
        at += 32;
        if (flags & ~FLAG_SHA) return fmt_fail("%s: %s has unknown flags %u", a->file, e->path, flags);
        if (flags & FLAG_SHA) { NEED(32); memcpy(e->sha, p + at, 32); e->has_sha = 1; at += 32; }
        if (e->codec > CODEC_EXTERNAL) return fmt_fail("%s: %s has unknown codec %u", a->file, e->path, e->codec);
        if (e->codec == CODEC_EXTERNAL) {               /* no bytes here: the pool's (resolved by the file system) */
            if (!e->has_sha || e->chunk_log2 || e->stored || e->offset || e->raw > MAX_FILE) return fmt_fail("%s: %s: bad pool reference", a->file, e->path);
            continue;
        }
        if (e->raw > MAX_FILE || e->stored > MAX_FILE || e->offset < HEADER_BYTES || e->offset > data_end || e->stored > data_end - e->offset)
            return fmt_fail("%s: %s lies outside the data", a->file, e->path);
        if (e->codec == CODEC_STORE && (e->chunk_log2 || e->stored != e->raw)) return fmt_fail("%s: %s: bad stored entry", a->file, e->path);
        if (e->chunk_log2) {
            if (e->chunk_log2 < 12 || e->chunk_log2 > 30) return fmt_fail("%s: %s: bad chunk size", a->file, e->path);
            uint64_t want = (e->raw + (1ull << e->chunk_log2) - 1) >> e->chunk_log2;
            NEED(4);
            e->nchunks = fmt_u32(p + at); at += 4;
            if (e->nchunks != want || e->nchunks == 0) return fmt_fail("%s: %s: chunk count %u, expected %llu", a->file, e->path, e->nchunks, (unsigned long long)want);
            NEED((size_t)e->nchunks * 4);
            e->chunk_at = (uint64_t*)fmt_alloc(((size_t)e->nchunks + 1) * sizeof *e->chunk_at);
            if (!e->chunk_at) return -1;
            uint64_t sum = 0;
            for (uint32_t c = 0; c < e->nchunks; c++) { e->chunk_at[c] = sum; uint32_t s = fmt_u32(p + at); at += 4; if (!s) return fmt_fail("%s: %s: empty chunk", a->file, e->path); sum += s; }
            e->chunk_at[e->nchunks] = sum;
            if (sum != e->stored) return fmt_fail("%s: %s: chunk sizes do not add up", a->file, e->path);
        } else if (e->codec == CODEC_ZSTD && e->stored == 0) return fmt_fail("%s: %s: empty frame", a->file, e->path);
    }
    if (at != n) return fmt_fail("%s: %llu bytes after the index", a->file, (unsigned long long)(n - at));
#undef NEED
    return 0;
}

/* the stored bytes [at, at + n) of the data region */
static int read_stored(const fmt_rxar* a, uint64_t at, void* dst, size_t n)
{
    return pread_all(a->f, at, dst, n) < 0 ? -1 : 0;
}

fmt_rxar* fmt_rxar_open(const char* path)
{
    fmt_clear_error();
    if (!path || !*path) { fmt_fail("no archive"); return NULL; }
    fmt_rxar* a = (fmt_rxar*)fmt_alloc(sizeof *a);
    if (!a) return NULL;
    a->f = RX_BAD_FILE;
    fmt_mutex_init(&a->dctx.lock);
    a->file = fmt_strdup(path);
    uint8_t h[HEADER_BYTES];
    uint64_t size = 0;
    uint8_t* raw = NULL; uint8_t* packed = NULL;
    if (!a->file) goto fail;
    a->f = open_read(path);
    if (a->f == RX_BAD_FILE) { fmt_fail("cannot open %s", path); goto fail; }
    if (file_size(a->f, &size) < 0 || size < HEADER_BYTES || pread_all(a->f, 0, h, HEADER_BYTES) < 0) { fmt_fail("%s: not an archive (too short)", path); goto fail; }
    if (memcmp(h, "RXAR", 4)) { fmt_fail("%s: not a Reflex archive", path); goto fail; }
    if (fmt_u32(h + 4) != 1 && fmt_u32(h + 4) != 2) { fmt_fail("%s: archive version %u is newer than this game reads", path, fmt_u32(h + 4)); goto fail; }
    if (crc_of(h, 60) != fmt_u32(h + 60)) { fmt_fail("%s: damaged archive header", path); goto fail; }
    a->kind = fmt_u32(h + 8); a->version = fmt_u32(h + 4);
    uint64_t io = get64(h + 16), is = get64(h + 24), ir = get64(h + 32);
    uint32_t count = fmt_u32(h + 40), icrc = fmt_u32(h + 44);
    uint32_t aflags = fmt_u32(h + 12);
    if (aflags != 0) { fmt_fail("%s: unknown archive flags (this build reads open archives only)", path); goto fail; }
    if (count > MAX_ENTRIES || ir > MAX_INDEX_RAW || is == 0 || io < HEADER_BYTES || io > size || is > size - io || ir < 12) { fmt_fail("%s: damaged archive index location", path); goto fail; }
    packed = (uint8_t*)fmt_alloc((size_t)is);
    raw = (uint8_t*)fmt_alloc((size_t)ir);
    if (!packed || !raw) goto fail;
    if (pread_all(a->f, io, packed, (size_t)is) < 0) { fmt_fail("%s: cannot read the index", path); goto fail; }
    size_t got = ZSTD_decompress(raw, (size_t)ir, packed, (size_t)is);
    if (ZSTD_isError(got) || got != ir) { fmt_fail("%s: damaged index (%s)", path, ZSTD_isError(got) ? ZSTD_getErrorName(got) : "size"); goto fail; }
    if (crc_of(raw, (size_t)ir) != icrc) { fmt_fail("%s: index checksum mismatch", path); goto fail; }
    if (parse_index(a, raw, (size_t)ir, count, io) < 0) goto fail;
    if (build_table(a) < 0) goto fail;
    free(raw); free(packed);
    return a;
fail:
    free(raw); free(packed);
    fmt_rxar_close(a);
    return NULL;
}

uint32_t fmt_rxar_kind(const fmt_rxar* a) { return a ? a->kind : 0; }
uint32_t fmt_rxar_version(const fmt_rxar* a) { return a ? a->version : 0; }
int fmt_rxar_add_ref(fmt_rxar_writer* w, const char* path, const void* data, size_t n)
{
    if (n && !data) return fmt_fail("%s: no data", path ? path : "(null)");
    uint8_t sha[32];
    fmt_sha256 hs; fmt_sha256_init(&hs); fmt_sha256_add(&hs, data, n); fmt_sha256_bin(&hs, sha);
    return fmt_rxar_add_external(w, path, n, crc_of(data, n), sha);
}
const char* fmt_rxar_file(const fmt_rxar* a) { return a ? a->file : ""; }
const char* fmt_rxar_meta(const fmt_rxar* a, size_t* len) { if (len) *len = a ? a->meta_len : 0; return a && a->meta ? a->meta : ""; }
int fmt_rxar_count(const fmt_rxar* a) { return a ? a->n : 0; }
const char* fmt_rxar_path(const fmt_rxar* a, int i) { return a && i >= 0 && i < a->n ? a->e[i].path : NULL; }
uint64_t fmt_rxar_size(const fmt_rxar* a, int i) { return a && i >= 0 && i < a->n ? a->e[i].raw : 0; }
int fmt_rxar_info_of(const fmt_rxar* a, int i, fmt_rxar_info* o)
{
    if (!a || i < 0 || i >= a->n || !o) return fmt_fail("bad archive entry %d", i);
    const rx_entry* e = &a->e[i];
    o->codec = e->codec; o->chunk_log2 = e->chunk_log2; o->raw = e->raw; o->stored = e->stored; o->offset = e->offset; o->crc = e->crc;
    return 0;
}

int fmt_rxar_sha(const fmt_rxar* a, int i, uint8_t sha[32])
{
    if (!a || i < 0 || i >= a->n || !a->e[i].has_sha) return 0;
    if (sha) memcpy(sha, a->e[i].sha, 32);
    return 1;
}
int fmt_rxar_external(const fmt_rxar* a, int i) { return a && i >= 0 && i < a->n && a->e[i].codec == CODEC_EXTERNAL; }
void fmt_rxar_pool_path(const uint8_t sha[32], char out[71])
{
    static const char hx[] = "0123456789abcdef";
    memcpy(out, "Pool/", 5);
    for (int i = 0; i < 32; i++) { out[5 + 2 * i] = hx[sha[i] >> 4]; out[6 + 2 * i] = hx[sha[i] & 15]; }
    out[69] = 0;
}

int fmt_rxar_find(const fmt_rxar* a, const char* path)
{
    if (!a || !path || !a->n) return -1;
    while (*path == '/' || *path == '\\') path++;
    char tmp[FMT_RXAR_MAX_PATH + 1];
    size_t n = strlen(path);
    if (n > FMT_RXAR_MAX_PATH) return -1;
    for (size_t i = 0; i <= n; i++) tmp[i] = path[i] == '\\' ? '/' : path[i];
    uint32_t h = fmt_hash_ci(tmp) & a->mask;
    for (;;) {
        int cur = a->table[h];
        if (!cur) return -1;
        if (!fmt_stricmp(a->e[cur - 1].path, tmp)) return cur - 1;
        h = (h + 1) & a->mask;
    }
}

/* decodes one zstd frame of `stored` bytes at file offset `at` into dst (exactly `raw` bytes) */
static int decode_frame(const fmt_rxar* ca, uint64_t at, uint64_t stored, void* dst, size_t raw, const char* what)
{
    fmt_rxar* a = (fmt_rxar*)ca;
    uint8_t* src = (uint8_t*)malloc(stored ? (size_t)stored : 1);
    if (!src) return fmt_fail("out of memory (%llu bytes)", (unsigned long long)stored);
    if (read_stored(a, at, src, (size_t)stored) < 0) { free(src); return fmt_fail("%s: read error in %s", what, a->file); }
    ZSTD_DCtx* d = (ZSTD_DCtx*)pool_take(&a->dctx);
    if (!d) d = ZSTD_createDCtx();
    if (!d) { free(src); return fmt_fail("out of memory (zstd decoder)"); }
    size_t got = ZSTD_decompressDCtx(d, dst, raw, src, (size_t)stored);
    pool_give(&a->dctx, d, 1);
    free(src);
    if (ZSTD_isError(got)) return fmt_fail("%s: damaged data in %s (%s)", what, a->file, ZSTD_getErrorName(got));
    if (got != raw) return fmt_fail("%s: decoded %llu of %llu bytes in %s", what, (unsigned long long)got, (unsigned long long)raw, a->file);
    return 0;
}

int fmt_rxar_read_at(const fmt_rxar* a, int i, uint64_t off, void* dst, size_t n)
{
    if (!a || i < 0 || i >= a->n) return fmt_fail("bad archive entry %d", i);
    const rx_entry* e = &a->e[i];
    if (off > e->raw || n > e->raw - off) return fmt_fail("%s: read of %llu bytes at %llu is past the end (%llu)", e->path,
                                                         (unsigned long long)n, (unsigned long long)off, (unsigned long long)e->raw);
    if (n == 0) return 0;
    if (e->codec == CODEC_EXTERNAL) return fmt_fail("%s: its bytes are in the library's shared pool (Assets.rxdata Pool/), not in %s", e->path, a->file);
    if (e->codec == CODEC_STORE) return read_stored(a, e->offset + off, dst, n) < 0 ? fmt_fail("%s: read error in %s", e->path, a->file) : 0;
    if (!e->chunk_log2) {
        if (off == 0 && n == e->raw) return decode_frame(a, e->offset, e->stored, dst, n, e->path);
        uint8_t* tmp = (uint8_t*)malloc((size_t)e->raw);
        if (!tmp) return fmt_fail("out of memory (%llu bytes)", (unsigned long long)e->raw);
        int rc = decode_frame(a, e->offset, e->stored, tmp, (size_t)e->raw, e->path);
        if (rc == 0) memcpy(dst, tmp + off, n);
        free(tmp);
        return rc;
    }
    const uint64_t cs = 1ull << e->chunk_log2;
    uint8_t* out = (uint8_t*)dst;
    uint8_t* tmp = NULL;
    for (uint64_t c = off >> e->chunk_log2; n; c++) {
        uint64_t c0 = c * cs, craw = e->raw - c0 < cs ? e->raw - c0 : cs;
        uint64_t from = off - c0, take = craw - from < n ? craw - from : n;
        if (from == 0 && take == craw) {
            if (decode_frame(a, e->offset + e->chunk_at[c], e->chunk_at[c + 1] - e->chunk_at[c], out, (size_t)craw, e->path) < 0) { free(tmp); return -1; }
        } else {
            if (!tmp && !(tmp = (uint8_t*)malloc((size_t)cs))) return fmt_fail("out of memory (%llu bytes)", (unsigned long long)cs);
            if (decode_frame(a, e->offset + e->chunk_at[c], e->chunk_at[c + 1] - e->chunk_at[c], tmp, (size_t)craw, e->path) < 0) { free(tmp); return -1; }
            memcpy(out, tmp + from, (size_t)take);
        }
        out += take; off += take; n -= (size_t)take;
    }
    free(tmp);
    return 0;
}

int fmt_rxar_read(const fmt_rxar* a, int i, fmt_buf* out)
{
    out->data = NULL; out->size = 0;
    if (!a || i < 0 || i >= a->n) return fmt_fail("bad archive entry %d", i);
    const rx_entry* e = &a->e[i];
    if (e->raw > 0x7fffffffu) return fmt_fail("%s: file too large", e->path);
    uint8_t* d = (uint8_t*)fmt_alloc((size_t)e->raw + 1);
    if (!d) return -1;
    if (fmt_rxar_read_at(a, i, 0, d, (size_t)e->raw) < 0) { free(d); return -1; }
    if (crc_of(d, (size_t)e->raw) != e->crc) { free(d); return fmt_fail("%s: checksum mismatch in %s", e->path, a->file); }
    out->data = d; out->size = (size_t)e->raw;
    return 0;
}

/* ------------------------------------------------------------------------------------------------ writing */

typedef struct {
    char* path;
    uint8_t codec, chunk_log2;
    uint64_t raw, stored, offset;
    uint32_t crc, nchunks;
    uint32_t* chunk;               /* stored size of each chunk */
    uint8_t has_sha, sha[32];
    uint8_t alias;                 /* the stored bytes of an earlier entry (dedupe) */
} wr_entry;

struct fmt_rxar_writer {
    char* path; char* partial;
    rx_file f;
    uint32_t kind;
    int level;
    fmt_mutex lock;
    uint64_t end, raw_total;
    wr_entry* e; int n, cap;
    char* meta; size_t meta_len;
    int failed;
    ctx_pool cctx;
    int* dedupe; uint32_t dmask;   /* SHA-256 -> entry index + 1 of the stored entries (open addressing; under lock) */
    uint64_t deduped, external;    /* raw bytes stored once for several paths / left to the pool */
};

fmt_rxar_writer* fmt_rxar_create(const char* path, uint32_t kind, int level)
{
    fmt_clear_error();
    if (!path || !*path) { fmt_fail("no archive path"); return NULL; }
    if (kind != FMT_RXAR_DATA && kind != FMT_RXAR_TRAK) { fmt_fail("unknown archive kind"); return NULL; }
    if (level <= 0) level = 9;
    if (level > 19) level = 19;
    fmt_rxar_writer* w = (fmt_rxar_writer*)fmt_alloc(sizeof *w);
    if (!w) return NULL;
    fmt_mutex_init(&w->lock);
    fmt_mutex_init(&w->cctx.lock);
    w->kind = kind; w->level = level; w->end = HEADER_BYTES;
    w->path = fmt_strdup(path);
    w->partial = (char*)fmt_alloc(strlen(path) + 9);
    if (!w->path || !w->partial) { fmt_rxar_abort(w); return NULL; }
    sprintf(w->partial, "%s.partial", path);
    w->f = open_write(w->partial);
    if (w->f == RX_BAD_FILE) { fmt_fail("cannot create %s", w->partial); w->f = RX_BAD_FILE; fmt_rxar_abort(w); return NULL; }
    uint8_t zero[HEADER_BYTES] = { 0 };                         /* no magic until finish: an unfinished file never reads as an archive */
    if (pwrite_all(w->f, 0, zero, sizeof zero) < 0) { fmt_fail("cannot write %s", w->partial); fmt_rxar_abort(w); return NULL; }
    return w;
}

static ZSTD_CCtx* take_cctx(fmt_rxar_writer* w)
{
    ZSTD_CCtx* c = (ZSTD_CCtx*)pool_take(&w->cctx);
    if (!c) c = ZSTD_createCCtx();
    if (c) {
        ZSTD_CCtx_reset(c, ZSTD_reset_session_and_parameters);
        ZSTD_CCtx_setParameter(c, ZSTD_c_compressionLevel, w->level);
        ZSTD_CCtx_setParameter(c, ZSTD_c_checksumFlag, 1);   /* every frame carries its own XXH64 check, verified when it is decoded */
        ZSTD_CCtx_setParameter(c, ZSTD_c_contentSizeFlag, 1);
    }
    return c;
}

/* compresses src into a new buffer; *out NULL (and 0 returned) when it does not shrink by 3 % */
static int pack(fmt_rxar_writer* w, ZSTD_CCtx* c, const uint8_t* src, size_t n, uint8_t** out, size_t* outn)
{
    *out = NULL; *outn = 0;
    size_t cap = ZSTD_compressBound(n);
    uint8_t* d = (uint8_t*)malloc(cap);
    if (!d) return fmt_fail("out of memory (%llu bytes)", (unsigned long long)cap);
    ZSTD_CCtx_reset(c, ZSTD_reset_session_only);
    ZSTD_CCtx_setParameter(c, ZSTD_c_compressionLevel, w->level);
    size_t got = ZSTD_compress2(c, d, cap, src, n);
    if (ZSTD_isError(got)) { free(d); return fmt_fail("zstd: %s", ZSTD_getErrorName(got)); }
    if ((double)got > (double)n * 0.97) { free(d); return 0; }
    *out = d; *outn = got;
    return 0;
}

/* data that does not compress (Bink video, already-compressed sound banks): the first 128 KiB at level 1 tell */
static int incompressible(ZSTD_CCtx* c, const uint8_t* src, size_t n)
{
    if (n < (64u << 10)) return 0;
    size_t probe = n < (128u << 10) ? n : (128u << 10), cap = ZSTD_compressBound(probe);
    uint8_t* d = (uint8_t*)malloc(cap);
    if (!d) return 0;
    ZSTD_CCtx_reset(c, ZSTD_reset_session_only);
    ZSTD_CCtx_setParameter(c, ZSTD_c_compressionLevel, 1);
    size_t got = ZSTD_compress2(c, d, cap, src, probe);
    free(d);
    return !ZSTD_isError(got) && (double)got > (double)probe * 0.97;
}

/* (under w->lock) the stored entry with these bytes, -1 when none */
static int dedupe_find(const fmt_rxar_writer* w, const uint8_t sha[32])
{
    if (!w->dedupe) return -1;
    uint32_t h = (uint32_t)fmt_u32(sha) & w->dmask;
    for (int cur; (cur = w->dedupe[h]) != 0; h = (h + 1) & w->dmask)
        if (!memcmp(w->e[cur - 1].sha, sha, 32)) return cur - 1;
    return -1;
}
static int dedupe_put(fmt_rxar_writer* w, int index)
{
    if (!w->dedupe || (uint32_t)w->n * 2u > w->dmask) {
        uint32_t cap = w->dedupe ? (w->dmask + 1) * 2 : 4096;
        int* t = (int*)fmt_alloc(cap * sizeof *t);
        if (!t) return -1;
        free(w->dedupe); w->dedupe = t; w->dmask = cap - 1;
        for (int i = 0; i < w->n; i++) if (i != index && w->e[i].codec != CODEC_EXTERNAL && !w->e[i].alias) {
            uint32_t h = (uint32_t)fmt_u32(w->e[i].sha) & w->dmask; while (t[h]) h = (h + 1) & w->dmask; t[h] = i + 1;
        }
    }
    uint32_t h = (uint32_t)fmt_u32(w->e[index].sha) & w->dmask;
    while (w->dedupe[h]) h = (h + 1) & w->dmask;
    w->dedupe[h] = index + 1;
    return 0;
}
static int grow_entries(fmt_rxar_writer* w)
{
    if (w->n < w->cap) return 0;
    int cap = w->cap ? w->cap * 2 : 1024;
    wr_entry* a = (wr_entry*)fmt_realloc(w->e, (size_t)cap * sizeof *a);
    if (!a) return -1;
    w->e = a; w->cap = cap;
    return 0;
}
/* (under w->lock) a new entry at path with the stored bytes of entry `of` (the same content: stored once) */
static int add_alias(fmt_rxar_writer* w, const char* path, int of)
{
    if (grow_entries(w) < 0) return -1;
    wr_entry e = w->e[of];
    e.path = fmt_strdup(path);
    e.chunk = NULL; e.alias = 1;
    if (!e.path) return -1;
    if (w->e[of].chunk) {
        e.chunk = (uint32_t*)fmt_alloc(e.nchunks * sizeof *e.chunk);
        if (!e.chunk) { free(e.path); return -1; }
        memcpy(e.chunk, w->e[of].chunk, e.nchunks * sizeof *e.chunk);
    }
    w->e[w->n++] = e; w->raw_total += e.raw; w->deduped += e.raw;
    return 0;
}

int fmt_rxar_add_external(fmt_rxar_writer* w, const char* path, uint64_t raw, uint32_t crc, const uint8_t sha[32])
{
    if (!w) return fmt_fail("no archive writer");
    if (!fmt_rxar_path_ok(path)) return fmt_fail("%s is not a path an archive may hold", path ? path : "(null)");
    wr_entry e; memset(&e, 0, sizeof e);
    e.codec = CODEC_EXTERNAL; e.raw = raw; e.crc = crc; e.has_sha = 1; memcpy(e.sha, sha, 32);
    if (!(e.path = fmt_strdup(path))) return -1;
    fmt_mutex_lock(&w->lock);
    int rc = w->failed ? fmt_fail("the archive already failed") : grow_entries(w);
    if (rc == 0) { w->e[w->n++] = e; w->raw_total += raw; w->external += raw; }
    fmt_mutex_unlock(&w->lock);
    if (rc < 0) free(e.path);
    return rc;
}

int fmt_rxar_add(fmt_rxar_writer* w, const char* path, const void* data, size_t n, unsigned flags)
{
    if (!w) return fmt_fail("no archive writer");
    if (!fmt_rxar_path_ok(path)) return fmt_fail("%s is not a path an archive may hold", path ? path : "(null)");
    if (n && !data) return fmt_fail("%s: no data", path);
    const uint8_t* src = (const uint8_t*)data;
    wr_entry e; memset(&e, 0, sizeof e);
    e.raw = n; e.crc = crc_of(src, n);
    { fmt_sha256 hs; fmt_sha256_init(&hs); fmt_sha256_add(&hs, src, n); fmt_sha256_bin(&hs, e.sha); e.has_sha = 1; }
    /* the same bytes already stored in this archive: a second path to them (gear kits share textures; level variants share most assets) */
    fmt_mutex_lock(&w->lock);
    int same = w->failed ? -2 : dedupe_find(w, e.sha);
    int done = same >= 0 ? add_alias(w, path, same) : 1;
    fmt_mutex_unlock(&w->lock);
    if (same == -2) return fmt_fail("the archive already failed");
    if (done <= 0) return done;
    uint8_t* whole = NULL; size_t wholen = 0;
    uint8_t** parts = NULL; size_t* partn = NULL; uint32_t nparts = 0;
    ZSTD_CCtx* c = NULL;
    int rc = 0;
    if (n && !(flags & FMT_RXAR_STORE)) {
        c = take_cctx(w);
        if (!c) return fmt_fail("out of memory (zstd encoder)");
        if (incompressible(c, src, n)) { /* stored */ }
        else if (n <= CHUNK_ABOVE) {
            if (pack(w, c, src, n, &whole, &wholen) < 0) rc = -1;
            else if (whole) { e.codec = CODEC_ZSTD; e.stored = wholen; }
        } else {
            const uint64_t cs = 1ull << CHUNK_LOG2;
            e.nchunks = (uint32_t)((n + cs - 1) >> CHUNK_LOG2);
            nparts = e.nchunks;
            parts = (uint8_t**)fmt_alloc(e.nchunks * sizeof *parts);
            partn = (size_t*)fmt_alloc(e.nchunks * sizeof *partn);
            e.chunk = (uint32_t*)fmt_alloc(e.nchunks * sizeof *e.chunk);
            if (!parts || !partn || !e.chunk) rc = -1;
            uint64_t total = 0;
            int all = 1;
            for (uint32_t k = 0; rc == 0 && k < e.nchunks; k++) {
                uint64_t at = (uint64_t)k * cs, len = n - at < cs ? n - at : cs;
                if (pack(w, c, src + at, (size_t)len, &parts[k], &partn[k]) < 0) { rc = -1; break; }
                if (!parts[k]) {                                /* a chunk that does not shrink: whole-file test below decides */
                    size_t cap = ZSTD_compressBound((size_t)len);
                    parts[k] = (uint8_t*)malloc(cap);
                    if (!parts[k]) { rc = fmt_fail("out of memory"); break; }
                    ZSTD_CCtx_reset(c, ZSTD_reset_session_only);
                    size_t got = ZSTD_compress2(c, parts[k], cap, src + at, (size_t)len);
                    if (ZSTD_isError(got)) { rc = fmt_fail("zstd: %s", ZSTD_getErrorName(got)); break; }
                    partn[k] = got;
                }
                if (partn[k] > 0xffffffffu) all = 0;
                e.chunk[k] = (uint32_t)partn[k];
                total += partn[k];
            }
            if (rc == 0 && all && (double)total <= (double)n * 0.97) { e.codec = CODEC_ZSTD; e.chunk_log2 = CHUNK_LOG2; e.stored = total; }
        }
    }
    if (rc == 0 && e.codec == CODEC_STORE) { e.stored = n; e.chunk_log2 = 0; e.nchunks = 0; free(e.chunk); e.chunk = NULL; }
    if (c) pool_give(&w->cctx, c, 0);
    if (rc == 0) {
        e.path = fmt_strdup(path);
        if (!e.path) rc = -1;
    }
    /* reserve the range and the table slot under the lock; write outside it (the table owns e.path / e.chunk once inserted) */
    int inserted = 0;
    if (rc == 0) {
        fmt_mutex_lock(&w->lock);
        int again = -1;
        if (w->failed) rc = fmt_fail("the archive already failed");
        else if ((again = dedupe_find(w, e.sha)) >= 0) {   /* the same bytes finished on another thread meanwhile */
            rc = add_alias(w, path, again);
            if (rc == 0) rc = 1;
        }
        else if (grow_entries(w) < 0) rc = -1;
        else {
            e.offset = w->end; w->end += e.stored; w->raw_total += e.raw; w->e[w->n++] = e; inserted = 1;
            if (dedupe_put(w, w->n - 1) < 0) rc = -1;
        }
        fmt_mutex_unlock(&w->lock);
        if (rc == 1) { free(e.path); free(e.chunk); free(whole); if (parts) for (uint32_t k = 0; k < nparts; k++) free(parts[k]); free(parts); free(partn); return 0; }
    }
    if (!inserted) { free(e.path); free(e.chunk); }
    if (rc == 0) {
        int wr = 0;
        if (e.codec == CODEC_STORE) wr = n ? pwrite_all(w->f, e.offset, src, n) : 0;
        else if (!e.chunk_log2) wr = pwrite_all(w->f, e.offset, whole, wholen);
        else { uint64_t at = e.offset; for (uint32_t k = 0; k < e.nchunks && wr == 0; k++) { wr = pwrite_all(w->f, at, parts[k], partn[k]); at += partn[k]; } }
        if (wr < 0) { fmt_mutex_lock(&w->lock); w->failed = 1; fmt_mutex_unlock(&w->lock); rc = fmt_fail("cannot write %s (disk full?)", w->partial); }
    }
    free(whole);
    if (parts) for (uint32_t k = 0; k < nparts; k++) free(parts[k]);
    free(parts); free(partn);
    return rc;
}

int fmt_rxar_set_meta(fmt_rxar_writer* w, const char* json, size_t n)
{
    if (!w) return fmt_fail("no archive writer");
    if (n > MAX_META) return fmt_fail("metadata too large");
    char* m = (char*)fmt_alloc(n + 1);
    if (!m) return -1;
    if (n) memcpy(m, json, n);
    fmt_mutex_lock(&w->lock);
    free(w->meta); w->meta = m; w->meta_len = n;
    fmt_mutex_unlock(&w->lock);
    return 0;
}

uint64_t fmt_rxar_deduped(const fmt_rxar_writer* cw, uint64_t* external)
{
    fmt_rxar_writer* w = (fmt_rxar_writer*)cw;
    if (!w) { if (external) *external = 0; return 0; }
    fmt_mutex_lock(&w->lock);
    uint64_t d = w->deduped, x = w->external;
    fmt_mutex_unlock(&w->lock);
    if (external) *external = x;
    return d;
}

uint64_t fmt_rxar_written(const fmt_rxar_writer* cw, uint64_t* raw)
{
    fmt_rxar_writer* w = (fmt_rxar_writer*)cw;
    if (!w) { if (raw) *raw = 0; return 0; }
    fmt_mutex_lock(&w->lock);
    uint64_t s = w->end - HEADER_BYTES, r = w->raw_total;
    fmt_mutex_unlock(&w->lock);
    if (raw) *raw = r;
    return s;
}

static void free_writer(fmt_rxar_writer* w)
{
    if (w->f != RX_BAD_FILE) close_file(w->f);
    for (int i = 0; i < w->n; i++) { free(w->e[i].path); free(w->e[i].chunk); }
    free(w->e); free(w->meta); free(w->path); free(w->partial); free(w->dedupe);
    pool_free(&w->cctx, 0);
    fmt_mutex_destroy(&w->lock);
    free(w);
}

void fmt_rxar_abort(fmt_rxar_writer* w)
{
    if (!w) return;
    if (w->f != RX_BAD_FILE) { close_file(w->f); w->f = RX_BAD_FILE; }
    if (w->partial) remove_file(w->partial);
    free_writer(w);
}

static int wr_cmp(const void* a, const void* b) { return strcmp(((const wr_entry*)a)->path, ((const wr_entry*)b)->path); }

int fmt_rxar_finish(fmt_rxar_writer* w)
{
    if (!w) return fmt_fail("no archive writer");
    if (w->failed) { fmt_fail("cannot finish %s: a write failed", w->path); fmt_rxar_abort(w); return -1; }
    qsort(w->e, (size_t)w->n, sizeof *w->e, wr_cmp);
    for (int i = 1; i < w->n; i++)
        if (!fmt_stricmp(w->e[i - 1].path, w->e[i].path)) { fmt_fail("%s: two files named %s", w->path, w->e[i].path); fmt_rxar_abort(w); return -1; }
    size_t size = 12 + w->meta_len;
    for (int i = 0; i < w->n; i++) size += 2 + strlen(w->e[i].path) + 32 + (w->e[i].has_sha ? 32 : 0) + (w->e[i].chunk_log2 ? 4 + 4 * (size_t)w->e[i].nchunks : 0);
    uint8_t* raw = (uint8_t*)fmt_alloc(size);
    if (!raw) { fmt_rxar_abort(w); return -1; }
    put32(raw, INDEX_MAGIC); put32(raw + 4, (uint32_t)w->n); put32(raw + 8, (uint32_t)w->meta_len);
    size_t at = 12;
    if (w->meta_len) memcpy(raw + at, w->meta, w->meta_len);
    at += w->meta_len;
    for (int i = 0; i < w->n; i++) {
        const wr_entry* e = &w->e[i];
        size_t pl = strlen(e->path);
        put16(raw + at, (uint32_t)pl); memcpy(raw + at + 2, e->path, pl); at += 2 + pl;
        raw[at] = e->codec; raw[at + 1] = e->chunk_log2; put16(raw + at + 2, e->has_sha ? FLAG_SHA : 0);
        put64(raw + at + 4, e->raw); put64(raw + at + 12, e->offset); put64(raw + at + 20, e->stored); put32(raw + at + 28, e->crc);
        at += 32;
        if (e->has_sha) { memcpy(raw + at, e->sha, 32); at += 32; }
        if (e->chunk_log2) { put32(raw + at, e->nchunks); at += 4; for (uint32_t k = 0; k < e->nchunks; k++) { put32(raw + at, e->chunk[k]); at += 4; } }
    }
    size_t cap = ZSTD_compressBound(size);
    uint8_t* packed = (uint8_t*)malloc(cap);
    if (!packed) { free(raw); fmt_fail("out of memory"); fmt_rxar_abort(w); return -1; }
    size_t pn = ZSTD_compress(packed, cap, raw, size, 19);
    if (ZSTD_isError(pn)) { free(raw); free(packed); fmt_fail("zstd: %s", ZSTD_getErrorName(pn)); fmt_rxar_abort(w); return -1; }
    uint8_t h[HEADER_BYTES]; memset(h, 0, sizeof h);
    memcpy(h, "RXAR", 4); put32(h + 4, 2); put32(h + 8, w->kind); put32(h + 12, 0);
    put64(h + 16, w->end); put64(h + 24, pn); put64(h + 32, size); put32(h + 40, (uint32_t)w->n); put32(h + 44, crc_of(raw, size));
    put32(h + 60, crc_of(h, 60));
    int ok = pwrite_all(w->f, w->end, packed, pn) == 0 && pwrite_all(w->f, 0, h, sizeof h) == 0;
    free(raw); free(packed);
#if defined(_WIN32)
    if (ok) ok = FlushFileBuffers(w->f) != 0;
#else
    if (ok) ok = fsync(w->f) == 0;
#endif
    close_file(w->f); w->f = RX_BAD_FILE;
    if (!ok) { fmt_fail("cannot write %s (disk full?)", w->partial); fmt_rxar_abort(w); return -1; }
    if (rename_over(w->partial, w->path) < 0) { fmt_fail("cannot rename %s to %s", w->partial, w->path); fmt_rxar_abort(w); return -1; }
    free_writer(w);
    return 0;
}

/* ------------------------------------------------------------------------------------------------ SHA-256 (FIPS 180-4) */

static const uint32_t K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha_block(fmt_sha256* s, const uint8_t* b)
{
    uint32_t w[64];
    for (int i = 0; i < 16; i++) w[i] = (uint32_t)b[4 * i] << 24 | (uint32_t)b[4 * i + 1] << 16 | (uint32_t)b[4 * i + 2] << 8 | b[4 * i + 3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3), s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = s->h[0], b2 = s->h[1], c = s->h[2], d = s->h[3], e = s->h[4], f = s->h[5], g = s->h[6], h = s->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K256[i] + w[i];
        uint32_t t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b2) ^ (a & c) ^ (b2 & c));
        h = g; g = f; f = e; e = d + t1; d = c; c = b2; b2 = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b2; s->h[2] += c; s->h[3] += d; s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}

void fmt_sha256_init(fmt_sha256* s)
{
    static const uint32_t iv[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    memcpy(s->h, iv, sizeof iv); s->bytes = 0; s->used = 0;
}
void fmt_sha256_add(fmt_sha256* s, const void* data, size_t n)
{
    const uint8_t* p = (const uint8_t*)data;
    s->bytes += n;
    if (s->used) {
        size_t k = 64 - s->used < n ? 64 - s->used : n;
        memcpy(s->block + s->used, p, k); s->used += k; p += k; n -= k;
        if (s->used == 64) { sha_block(s, s->block); s->used = 0; }
    }
    while (n >= 64) { sha_block(s, p); p += 64; n -= 64; }
    if (n) { memcpy(s->block, p, n); s->used = n; }
}
void fmt_sha256_bin(fmt_sha256* s, uint8_t out[32])
{
    uint64_t bits = s->bytes * 8;
    uint8_t pad = 0x80, zero = 0, len[8];
    fmt_sha256_add(s, &pad, 1);
    while (s->used != 56) fmt_sha256_add(s, &zero, 1);
    for (int i = 0; i < 8; i++) len[i] = (uint8_t)(bits >> (56 - 8 * i));
    fmt_sha256_add(s, len, 8);
    for (int i = 0; i < 32; i++) out[i] = (uint8_t)(s->h[i / 4] >> (24 - 8 * (i % 4)));
}
void fmt_sha256_hex(fmt_sha256* s, char out[65])
{
    uint64_t bits = s->bytes * 8;
    uint8_t pad = 0x80, zero = 0, len[8];
    fmt_sha256_add(s, &pad, 1);
    while (s->used != 56) fmt_sha256_add(s, &zero, 1);
    for (int i = 0; i < 8; i++) len[i] = (uint8_t)(bits >> (56 - 8 * i));
    fmt_sha256_add(s, len, 8);
    static const char hx[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) { uint8_t v = (uint8_t)(s->h[i / 4] >> (24 - 8 * (i % 4))); out[2 * i] = hx[v >> 4]; out[2 * i + 1] = hx[v & 15]; }
    out[64] = 0;
}
