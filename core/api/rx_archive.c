/* rx_archive.c -- the Reflex archive exports of core/include/rx_api.h (port addition): thin wrappers over src/fmt_rxar.c. */
#include "rx_api.h"
#include "../src/fmt_rxar.h"
#include <string.h>

RX_API void* rx_archive_open(const char* path) { return fmt_rxar_open(path); }
RX_API void rx_archive_close(void* a) { fmt_rxar_close((fmt_rxar*)a); }
RX_API uint32_t rx_archive_kind(void* a) { return fmt_rxar_kind((const fmt_rxar*)a); }
RX_API const char* rx_archive_meta(void* a, int64_t* length)
{
    size_t n = 0;
    const char* m = fmt_rxar_meta((const fmt_rxar*)a, &n);
    if (length) *length = (int64_t)n;
    return m;
}
RX_API int32_t rx_archive_count(void* a) { return fmt_rxar_count((const fmt_rxar*)a); }
RX_API const char* rx_archive_path(void* a, int32_t i) { return fmt_rxar_path((const fmt_rxar*)a, i); }
RX_API int64_t rx_archive_size(void* a, int32_t i)
{
    const fmt_rxar* r = (const fmt_rxar*)a;
    return r && i >= 0 && i < fmt_rxar_count(r) ? (int64_t)fmt_rxar_size(r, i) : -1;
}
RX_API int32_t rx_archive_find(void* a, const char* path) { return fmt_rxar_find((const fmt_rxar*)a, path); }
RX_API int32_t rx_archive_read(void* a, int32_t i, uint64_t offset, void* dst, uint64_t n)
{
    if ((uint64_t)(size_t)n != n) return fmt_fail("read too large");
    return fmt_rxar_read_at((const fmt_rxar*)a, i, offset, dst, (size_t)n);
}
RX_API int64_t rx_archive_list(void* a, char* dst, int64_t cap, int64_t* sizes, int32_t nsizes, uint8_t* kinds)
{
    const fmt_rxar* r = (const fmt_rxar*)a;
    if (!r) return -1;
    const int n = fmt_rxar_count(r);
    int64_t need = 0;
    for (int i = 0; i < n; i++) need += (int64_t)strlen(fmt_rxar_path(r, i)) + (i ? 1 : 0);
    if (dst && cap >= need) {
        char* o = dst;
        for (int i = 0; i < n; i++) { if (i) *o++ = '\n'; size_t l = strlen(fmt_rxar_path(r, i)); memcpy(o, fmt_rxar_path(r, i), l); o += l; }
    }
    for (int i = 0; sizes && i < n && i < nsizes; i++) sizes[i] = (int64_t)fmt_rxar_size(r, i);
    for (int i = 0; kinds && i < n && i < nsizes; i++) kinds[i] = (uint8_t)fmt_rxar_external(r, i);
    return need;
}
RX_API int32_t rx_archive_entry_sha(void* a, int32_t i, char* hex)
{
    const fmt_rxar* r = (const fmt_rxar*)a;
    uint8_t sha[32];
    if (!fmt_rxar_sha(r, i, sha)) return 0;
    if (hex) { char p[71]; fmt_rxar_pool_path(sha, p); memcpy(hex, p + 5, 65); }
    return fmt_rxar_external(r, i) ? 2 : 1;
}
RX_API int32_t rx_archive_version(void* a) { return (int32_t)fmt_rxar_version((const fmt_rxar*)a); }
RX_API int32_t rx_archive_add_ref(void* w, const char* path, const void* data, uint64_t n)
{
    if ((uint64_t)(size_t)n != n) return fmt_fail("file too large");
    return fmt_rxar_add_ref((fmt_rxar_writer*)w, path, data, (size_t)n);
}
RX_API int64_t rx_archive_deduped(void* w, int64_t* pooled)
{
    uint64_t x = 0, d = fmt_rxar_deduped((const fmt_rxar_writer*)w, &x);
    if (pooled) *pooled = (int64_t)x;
    return (int64_t)d;
}
RX_API void* rx_archive_create(const char* path, uint32_t kind, int32_t level) { return fmt_rxar_create(path, kind, level); }
RX_API int32_t rx_archive_add(void* w, const char* path, const void* data, uint64_t n, uint32_t flags)
{
    if ((uint64_t)(size_t)n != n) return fmt_fail("file too large");
    return fmt_rxar_add((fmt_rxar_writer*)w, path, data, (size_t)n, flags);
}
RX_API int32_t rx_archive_set_meta(void* w, const char* json, int64_t length)
{
    if (length < 0) return fmt_fail("bad metadata length");
    return fmt_rxar_set_meta((fmt_rxar_writer*)w, json, (size_t)length);
}
RX_API int64_t rx_archive_written(void* w, int64_t* raw)
{
    uint64_t r = 0, s = fmt_rxar_written((const fmt_rxar_writer*)w, &r);
    if (raw) *raw = (int64_t)r;
    return (int64_t)s;
}
RX_API int32_t rx_archive_finish(void* w) { return fmt_rxar_finish((fmt_rxar_writer*)w); }
RX_API void rx_archive_abort(void* w) { fmt_rxar_abort((fmt_rxar_writer*)w); }
RX_API const char* rx_archive_error(void) { return fmt_error(); }
