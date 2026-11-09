/* fmt_common.h -- shared basics of the file readers (src/fmt_*.c): error text, byte buffers, little-endian readers, strings.
 *
 * The readers are plain C11 with their own data structures (they are NOT the game-memory model of rx_mem.h): they read the
 * player's Reflex install and hand out ordinary C structs. Conventions:
 *   - a function returning int returns 0 on success and -1 on failure; a function returning a pointer returns NULL on failure;
 *     the failure text is in fmt_error() (per thread, overwritten by the next failure on that thread).
 *   - fmt_xxx_open / _parse / _load allocate, fmt_xxx_free releases (NULL is fine). Strings and arrays reachable from a
 *     returned object belong to it and live until its free.
 *   - nothing is written to the install; nothing keeps global state except the (thread safe) error text.
 *   - the host is little endian, like the files; multi-byte fields are still read byte-wise (alignment safe). */
#ifndef FMT_COMMON_H
#define FMT_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- errors ---- */
const char* fmt_error(void);                          /* the last failure text of this thread ("" when none) */
int         fmt_fail(const char* format, ...);        /* sets the text, returns -1 (so `return fmt_fail("...")` works) */
void        fmt_clear_error(void);

/* ---- memory ---- */
typedef struct fmt_buf { uint8_t* data; size_t size; } fmt_buf;   /* an owned byte block */
void  fmt_buf_free(fmt_buf* b);                       /* frees data, zeroes the struct */
void* fmt_alloc(size_t bytes);                        /* zeroed; sets the error text and returns NULL on failure */
void* fmt_realloc(void* p, size_t bytes);             /* not zeroed */
char* fmt_strdup(const char* s);
char* fmt_strndup(const char* s, size_t n);

/* ---- strings ---- */
int      fmt_stricmp(const char* a, const char* b);   /* ASCII case-insensitive compare (<0, 0, >0) */
uint32_t fmt_hash_ci(const char* s);                  /* FNV-1a over the ASCII-lowercased bytes, '\\' folded to '/' */
uint32_t fmt_hash(const char* s);                     /* FNV-1a over the bytes */

/* ---- little-endian field readers (no bounds check; callers check lengths) ---- */
static inline uint16_t fmt_u16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
static inline uint32_t fmt_u32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static inline int32_t  fmt_i32(const uint8_t* p) { return (int32_t)fmt_u32(p); }
static inline float    fmt_f32(const uint8_t* p) { uint32_t u = fmt_u32(p); float f; memcpy(&f, &u, 4); return f; }
static inline uint32_t fmt_f32_bits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static inline float    fmt_f32_from_bits(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
float fmt_f16(uint16_t half);                         /* IEEE half -> float, exact (denormals, inf, NaN) */

#ifdef __cplusplus
}
#endif
#endif
