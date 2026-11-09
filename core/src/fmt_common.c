/* fmt_common.c -- see fmt_common.h. */
#include "fmt_common.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(_MSC_VER)
#  define FMT_TLS __declspec(thread)
#else
#  define FMT_TLS _Thread_local
#endif

static FMT_TLS char g_error[512];

const char* fmt_error(void) { return g_error; }
void fmt_clear_error(void) { g_error[0] = 0; }

int fmt_fail(const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    vsnprintf(g_error, sizeof g_error, format, ap);
    va_end(ap);
    return -1;
}

void fmt_buf_free(fmt_buf* b)
{
    if (!b) return;
    free(b->data);
    b->data = NULL;
    b->size = 0;
}

void* fmt_alloc(size_t bytes)
{
    void* p = calloc(1, bytes ? bytes : 1);
    if (!p) fmt_fail("out of memory (%llu bytes)", (unsigned long long)bytes);
    return p;
}

void* fmt_realloc(void* p, size_t bytes)
{
    void* q = realloc(p, bytes ? bytes : 1);
    if (!q) fmt_fail("out of memory (%llu bytes)", (unsigned long long)bytes);
    return q;
}

char* fmt_strndup(const char* s, size_t n)
{
    char* d = (char*)fmt_alloc(n + 1);
    if (d) { memcpy(d, s, n); d[n] = 0; }
    return d;
}

char* fmt_strdup(const char* s) { return fmt_strndup(s ? s : "", s ? strlen(s) : 0); }

static inline int lower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

int fmt_stricmp(const char* a, const char* b)
{
    for (;; a++, b++) {
        int x = lower((unsigned char)*a), y = lower((unsigned char)*b);
        if (x != y) return x - y;
        if (!x) return 0;
    }
}

uint32_t fmt_hash_ci(const char* s)
{
    uint32_t h = 2166136261u;
    for (; *s; s++) {
        int c = lower((unsigned char)*s);
        if (c == '\\') c = '/';
        h = (h ^ (uint32_t)c) * 16777619u;
    }
    return h;
}

uint32_t fmt_hash(const char* s)
{
    uint32_t h = 2166136261u;
    for (; *s; s++) h = (h ^ (uint8_t)*s) * 16777619u;
    return h;
}

float fmt_f16(uint16_t h)
{
    uint32_t sign = (uint32_t)(h & 0x8000) << 16, e = (h >> 10) & 31, m = h & 1023, u;
    if (e == 0) {
        if (m == 0) u = sign;                                   /* +-0 */
        else {                                                  /* denormal: normalise */
            int shift = 0;
            while (!(m & 1024)) { m <<= 1; shift++; }
            m &= 1023;
            u = sign | (uint32_t)(113 - shift) << 23 | m << 13;
        }
    } else if (e == 31) u = sign | 0x7f800000u | m << 13;       /* inf / NaN (payload kept) */
    else u = sign | (e + 112) << 23 | m << 13;
    return fmt_f32_from_bits(u);
}
