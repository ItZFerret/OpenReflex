/* fmt_test.c -- dumps what the file readers (src/fmt_*.c) produce for the install, in the line format of fmt_test.py, plus
 * self checks. fmt_test.sh runs fmt_test.py (the Python parsers of _re/formats) and this tool and diffs the two outputs.
 *
 *     fmt_test ARCHIVE OUT.txt SAMPLES.txt [DUMPDIR]
 *
 * SAMPLES.txt (written by fmt_test.py) lists the model / surface / texture assets both sides read. Exit status: 0 when every
 * self check passed (the diff against the Python output is the caller's). */
#define _POSIX_C_SOURCE 200809L
#include "../src/fmt_collis.h"
#include "../src/fmt_inflate.h"
#include "../src/fmt_lzx.h"
#include "../src/fmt_material.h"
#include "../src/fmt_model.h"
#include "../src/fmt_table.h"
#include "../src/fmt_terrain.h"
#include "../src/fmt_texture.h"
#include "fmt_test_vectors.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static const char* LEVELS[] = { "FR_Alpine", "NAT_Concord", "SX_Kingston" };
#define NLEVELS 3
static const char* FULL_DBS[] = { "MXTables_Default", "EngineDependencies", "RiderPhysics", "FR_Alpine", "NAT_Concord", "SX_Kingston" };
#define NFULL 6
static const char* COLLIS_DBS[] = { "FR_Alpine", "NAT_Concord", "SX_Kingston", "MXs", "ATVs", "EngineDependencies", "RiderPhysics" };
#define NCOLLIS 7

static FILE* g_out;
static const char* g_dumpdir;
static int g_fail;

#define CHECK(cond, ...) do { if (!(cond)) { g_fail++; fprintf(stderr, "CHECK FAILED %s:%d: ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

/* ------------------------------------------------------------------------------------------------ small helpers */

typedef struct { char* p; size_t n, cap; } sbuf;

static void sb_grow(sbuf* b, size_t more)
{
    if (b->n + more + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 4096;
        while (cap < b->n + more + 1) cap *= 2;
        b->p = (char*)realloc(b->p, cap);
        b->cap = cap;
    }
}
static void sb_add(sbuf* b, const char* s, size_t n) { sb_grow(b, n); memcpy(b->p + b->n, s, n); b->n += n; b->p[b->n] = 0; }
static void sb_str(sbuf* b, const char* s) { sb_add(b, s, strlen(s)); }
static void sb_f(sbuf* b, const char* f, ...)
{
    char tmp[1024];
    va_list ap;
    va_start(ap, f);
    int n = vsnprintf(tmp, sizeof tmp, f, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n < sizeof tmp) { sb_add(b, tmp, (size_t)n); return; }
    char* big = (char*)malloc((size_t)n + 1);
    va_start(ap, f);
    vsnprintf(big, (size_t)n + 1, f, ap);
    va_end(ap);
    sb_add(b, big, (size_t)n);
    free(big);
}
static void sb_free(sbuf* b) { free(b->p); b->p = NULL; b->n = b->cap = 0; }

/* SHA-256 */
typedef struct { uint32_t h[8]; uint8_t buf[64]; uint64_t len; size_t fill; } sha_t;
static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
static void sha_block(sha_t* s, const uint8_t* p)
{
    uint32_t w[64], a, b, c, d, e, f, g, h;
    for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3), s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3]; e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
        uint32_t t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}
static void sha_init(sha_t* s)
{
    static const uint32_t i0[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    memcpy(s->h, i0, sizeof i0); s->len = 0; s->fill = 0;
}
static void sha_update(sha_t* s, const void* data, size_t n)
{
    const uint8_t* p = (const uint8_t*)data;
    s->len += n;
    while (n) {
        size_t k = 64 - s->fill < n ? 64 - s->fill : n;
        memcpy(s->buf + s->fill, p, k);
        s->fill += k; p += k; n -= k;
        if (s->fill == 64) { sha_block(s, s->buf); s->fill = 0; }
    }
}
static void sha_final(sha_t* s, char hex[65])
{
    uint64_t bits = s->len * 8;
    uint8_t pad = 0x80;
    sha_update(s, &pad, 1);
    uint8_t z = 0;
    while (s->fill != 56) sha_update(s, &z, 1);
    uint8_t lb[8];
    for (int i = 0; i < 8; i++) lb[i] = (uint8_t)(bits >> (56 - 8 * i));
    sha_update(s, lb, 8);
    for (int i = 0; i < 8; i++) sprintf(hex + 8 * i, "%08x", s->h[i]);
}
static void sha_of(const void* data, size_t n, char hex[65]) { sha_t s; sha_init(&s); sha_update(&s, data, n); sha_final(&s, hex); }

static void js(sbuf* b, const char* s)
{
    sb_str(b, "\"");
    for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
        switch (*p) {
        case '"': sb_str(b, "\\\""); break;
        case '\\': sb_str(b, "\\\\"); break;
        case '\n': sb_str(b, "\\n"); break;
        case '\r': sb_str(b, "\\r"); break;
        case '\t': sb_str(b, "\\t"); break;
        case '\b': sb_str(b, "\\b"); break;
        case '\f': sb_str(b, "\\f"); break;
        default:
            if (*p < 0x20) sb_f(b, "\\u%04x", *p); else sb_add(b, (const char*)p, 1);
        }
    }
    sb_str(b, "\"");
}
static void jsn(sbuf* b, const char* s) { if (s) js(b, s); else sb_str(b, "null"); }       /* Python json.dumps(None) */
static void jsd(sbuf* b, const char* s) { if (s) js(b, s); else sb_str(b, "-"); }          /* "-" for an absent value */
static void hxf(sbuf* b, float f) { sb_f(b, "%08x", fmt_f32_bits(f)); }
static void hxs(sbuf* b, const float* f, int n) { for (int i = 0; i < n; i++) { if (i) sb_str(b, " "); hxf(b, f[i]); } }

static void emit(const char* f, ...)
{
    va_list ap;
    va_start(ap, f);
    vfprintf(g_out, f, ap);
    va_end(ap);
    fputc('\n', g_out);
}
static void emit_sb(const sbuf* b) { fwrite(b->p, 1, b->n, g_out); fputc('\n', g_out); }

static void dump_file(const char* name, const sbuf* text)
{
    if (!g_dumpdir) return;
    char path[1024];
    snprintf(path, sizeof path, "%s/%s.txt", g_dumpdir, name);
    for (char* p = path + strlen(g_dumpdir) + 1; *p; p++) if (*p == '/') *p = '_';
    FILE* f = fopen(path, "wb");
    if (f) { fwrite(text->p, 1, text->n, f); fclose(f); }
}

/* ------------------------------------------------------------------------------------------------ bxml dumps */

static void rv(sbuf* b, const fmt_bxml_value* v)
{
    switch (v->kind) {
    case FMT_BX_EMPTY: sb_str(b, "-"); return;
    case FMT_BX_STRING: js(b, v->str); return;
    case FMT_BX_BYTES: sb_str(b, "x"); for (uint32_t i = 0; i < v->count; i++) sb_f(b, "%02x", v->data[i]); return;
    default: break;
    }
    sb_f(b, "T%d:%u:", v->kind, v->count);
    int comps = fmt_bxml_elem_size(v->kind) / 4;
    for (uint32_t i = 0; i < v->count; i++) {
        if (i) sb_str(b, ",");
        for (int c = 0; c < comps; c++) {
            if (c) sb_str(b, " ");
            if (v->kind == FMT_BX_INT) sb_f(b, "%d", fmt_bx_i32(v, i));
            else if (v->kind == FMT_BX_UINT || v->kind == FMT_BX_BOOL) sb_f(b, "%u", fmt_bx_u32(v, i));
            else hxf(b, fmt_bx_f32(v, i, c));
        }
    }
}

static void tree(sbuf* b, const fmt_bxml* d, int n, int depth)
{
    fmt_bxml_value v;
    sb_f(b, "%d %s {", depth, fmt_bxml_tag(d, n));
    for (int k = 0; k < fmt_bxml_node_attr_count(d, n); k++) {
        int a = fmt_bxml_node_attr(d, n, k);
        fmt_bxml_attr_value(d, a, &v);
        if (k) sb_str(b, " ");
        sb_f(b, "%s=", fmt_bxml_attr_name(d, a));
        rv(b, &v);
    }
    sb_str(b, "} ");
    fmt_bxml_node_value(d, n, &v);
    rv(b, &v);
    sb_str(b, "\n");
    for (int c = 0; c < fmt_bxml_child_count(d, n); c++) tree(b, d, fmt_bxml_child(d, n, c), depth + 1);
}

static void bxml_line(const char* label, const fmt_bxml* d, const sbuf* text)
{
    char h[65];
    sha_of(text->p ? text->p : "", text->n, h);
    emit("bxml %s nodes %u attrs %u strings %u blob %zu sha %s", label, fmt_bxml_node_count(d), fmt_bxml_attr_count(d), fmt_bxml_string_count(d), fmt_bxml_blob_size(d), h);
    char name[512];
    snprintf(name, sizeof name, "bxml_%s", label);
    dump_file(name, text);
}

/* ------------------------------------------------------------------------------------------------ sections */

static int cmp_vfs_name(const char* name, uint64_t size, void* user) { (void)size; (void)user; (void)name; return 0; }

static void sec_vfs(fmt_install* in)
{
    fmt_vfs* v = fmt_install_vfs(in);
    sha_t s;
    sha_init(&s);
    int n = fmt_vfs_file_count(v);
    for (int i = 0; i < n; i++) {
        char line[1100];
        int l = snprintf(line, sizeof line, "%s\t%" PRIu64 "\n", fmt_vfs_file_path(v, i), fmt_vfs_file_size(v, i));
        sha_update(&s, line, (size_t)l);
    }
    char h[65];
    sha_final(&s, h);
    emit("vfs files %d sha %s", n, h);
    /* self checks */
    CHECK(fmt_vfs_exists(v, "database/fr_alpine.DX9.scene"), "case-insensitive lookup");
    CHECK(fmt_vfs_exists(v, "Database\\FR_Alpine.dx9.scene"), "backslash lookup");
    CHECK(fmt_vfs_exists(v, "/Database/FR_Alpine.dx9.scene"), "leading separator");
    CHECK(!fmt_vfs_exists(v, "Database/NoSuchFile.bin"), "absent file");
    CHECK(fmt_vfs_size(v, "MXReflex.exe") > 1000000, "exe size");
    int idx = fmt_vfs_find(v, "MXRegistry.bxml");
    char b4[4];
    CHECK(idx >= 0 && fmt_vfs_read_at(v, idx, 0, b4, 4) == 0 && !memcmp(b4, "BXML", 4), "read_at BXML magic");
    CHECK(fmt_vfs_read_at(v, idx, fmt_vfs_file_size(v, idx) - 2, b4, 4) < 0, "read past the end must fail");
    CHECK(fmt_vfs_list(v, "Database", cmp_vfs_name, NULL) == 0, "list");
    char parent[1200];
    snprintf(parent, sizeof parent, "%s/..", fmt_vfs_root(v));
    fmt_vfs* v2 = fmt_vfs_open(parent);                                          /* the folder above Archive */
    CHECK(v2 && fmt_vfs_file_count(v2) == n, "open via the parent folder");
    fmt_vfs_close(v2);
    CHECK(fmt_vfs_open("/nonexistent/folder") == NULL, "open of a missing folder fails");
}

static void sec_selftest(void)
{
    /* deflate: stored, fixed and dynamic blocks (Python zlib vectors) */
    uint8_t out[4096];
    char h[65], want[65];
    uint8_t exp[3000];
    size_t n;
    n = 0; for (int i = 0; i < 3; i++) for (int k = 0; k < 10; k++) exp[n++] = (uint8_t)('a' + k);
    CHECK(fmt_zlib_decode(vec_stored, sizeof vec_stored, out, n) == 0 && !memcmp(out, exp, n), "inflate stored: %s", fmt_error());
    const char* hw = "hello hello hello hello world";
    CHECK(fmt_zlib_decode(vec_fixed, sizeof vec_fixed, out, strlen(hw)) == 0 && !memcmp(out, hw, strlen(hw)), "inflate fixed: %s", fmt_error());
    n = 0;
    for (int i = 0; i < 40; i++) { const char* s = "The quick brown fox jumps over the lazy dog. "; memcpy(exp + n, s, strlen(s)); n += strlen(s); }
    for (int i = 0; i < 3; i++) for (int k = 0; k < 256; k++) exp[n++] = (uint8_t)k;
    CHECK(fmt_zlib_decode(vec_dynamic, sizeof vec_dynamic, out, n) == 0 && !memcmp(out, exp, n), "inflate dynamic: %s", fmt_error());
    CHECK(fmt_zlib_decode(vec_dynamic, sizeof vec_dynamic - 1, out, n) < 0, "truncated zlib must fail");
    uint8_t bad[sizeof vec_dynamic];
    memcpy(bad, vec_dynamic, sizeof bad);
    bad[100] ^= 0x55;
    CHECK(fmt_zlib_decode(bad, sizeof bad, out, n) < 0, "corrupted zlib must fail");
    sha_of("abc", 3, h);
    snprintf(want, sizeof want, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(!strcmp(h, want), "sha256 self test");
    /* lzx: window sizes and bad streams are rejected without crashing */
    CHECK(fmt_lzx_new(14) == NULL && fmt_lzx_new(22) == NULL, "window range");
    uint8_t junk[64];
    for (int i = 0; i < 64; i++) junk[i] = (uint8_t)(i * 37 + 11);
    fmt_lzx_decode_once(17, junk, sizeof junk, out, 1000);     /* must not crash; result irrelevant */
    fmt_buf r;
    CHECK(fmt_xmem_resource(junk, sizeof junk, 0, &r) < 0, "bad xmem wrapper");
    /* w-fb-customtrack: the word after the container cut short by the resource's heap size (Mods/Tracks "2014 Monster Energy Cup":
     * 1 of its 4 bytes missing); a stored chunk of 40 bytes whose last 30 count. 4 bytes = the game's layout, 3 / 1 = the missing bytes are zeros */
    {
        uint8_t x[4 + 8 + 40 + 4];
        memset(x, 0, sizeof x);
        x[0] = 52; x[4] = 40; x[8] = 40;
        for (int i = 0; i < 40; i++) x[12 + i] = (uint8_t)(i + 1);
        x[52] = 30;
        static const size_t keeps[] = { 4, 3, 1 };
        for (int k = 0; k < 3; k++) {
            int ok = fmt_xmem_resource(x, 52 + keeps[k], 0, &r) == 0;
            CHECK(ok && r.size == 30 && r.data[29] == 30, "xmem trailer of %zu bytes (%s)", keeps[k], ok ? "size" : fmt_error());
            if (ok) fmt_buf_free(&r);
        }
        /* no trailer at all: the stored chunk whole */
        int ok = fmt_xmem_resource(x, 52, 0, &r) == 0;
        CHECK(ok && r.size == 40, "xmem without a trailer (%s)", ok ? "size" : fmt_error());
        if (ok) fmt_buf_free(&r);
        CHECK(fmt_xmem_resource(x, 51, 0, &r) < 0, "xmem container past the resource");
    }
}

static void sec_db(fmt_install* in)
{
    for (int d = 0; d < NFULL; d++) {
        const fmt_database* db = fmt_install_db(in, FULL_DBS[d]);
        if (!db) { emit("db %s ERR %s", FULL_DBS[d], fmt_error()); continue; }
        emit("db %s packages %d handles %d", db->name, db->npackages, db->nhandles);
        sbuf lines = { 0 };
        for (int p = 0; p < db->npackages; p++) {
            const fmt_package* pk = &db->packages[p];
            for (int a = 0; a < pk->nassets; a++) {
                const fmt_asset* as = &pk->assets[a];
                sb_f(&lines, "%s\t%s\t%" PRIu64 "\t%u\t%u\t%s\n", as->type, as->name, as->offset, as->size, as->block_size, as->lzx ? "lzx" : as->compressed_other ? "other" : "raw");
            }
            emit("pkg %s file %s offset %" PRIu64 " block %" PRIu64 " heap %" PRIu64 " assets %d dups %d", pk->name, pk->file, pk->offset, pk->block_offset, pk->heap_offset, pk->nassets, pk->duplicates);
        }
        char h[65];
        sha_of(lines.p ? lines.p : "", lines.n, h);
        emit("assets sha %s", h);
        sb_free(&lines);
    }
}

static void sec_assets(fmt_install* in)
{
    for (int d = 0; d < NFULL; d++) {
        const fmt_database* db = fmt_install_db(in, FULL_DBS[d]);
        if (!db) continue;
        for (int p = 0; p < db->npackages; p++) {
            const fmt_package* pk = &db->packages[p];
            for (int a = 0; a < pk->nassets; a++) {
                const fmt_asset* as = &pk->assets[a];
                fmt_buf b;
                if (fmt_install_read(in, as, &b) < 0) { emit("a %s/%s/%s/%s ERR", db->name, pk->name, as->type, as->name); continue; }
                char h[65];
                sha_of(b.data, b.size, h);
                emit("a %s/%s/%s/%s %zu %s", db->name, pk->name, as->type, as->name, b.size, h);
                fmt_buf_free(&b);
            }
        }
    }
}

static int root_bxml_cb(const char* name, uint64_t size, void* user)
{
    (void)size;
    size_t l = strlen(name);
    if (l > 5 && !fmt_stricmp(name + l - 5, ".bxml")) {
        char*** list = (char***)user;
        int n = 0;
        while ((*list)[n]) n++;
        *list = (char**)realloc(*list, (size_t)(n + 2) * sizeof(char*));
        (*list)[n] = strdup(name);
        (*list)[n + 1] = NULL;
    }
    return 0;
}

static void bxml_of_buffer(const char* label, const fmt_buf* b)
{
    fmt_bxml* d = fmt_bxml_parse(b->data, b->size);
    if (!d) { emit("bxml %s ERR %s", label, fmt_error()); return; }
    sbuf t = { 0 };
    tree(&t, d, 0, 0);
    bxml_line(label, d, &t);
    sb_free(&t);
    fmt_bxml_free(d);
}

static void resolve_line(fmt_install* in, const char* file, const char* path)
{
    fmt_buf b;
    if (fmt_vfs_read_file(fmt_install_vfs(in), file, &b) < 0) return;
    fmt_bxml* d = fmt_bxml_parse(b.data, b.size);
    int is_attr = 0;
    int i = d ? fmt_bxml_resolve(d, path, &is_attr) : -1;
    if (i < 0) emit("resolve %s %s none", file, path);
    else emit("resolve %s %s %s %d", file, path, is_attr ? "attribute" : "node", i);
    fmt_bxml_free(d);
    fmt_buf_free(&b);
}

static void sec_bxml(fmt_install* in)
{
    fmt_vfs* v = fmt_install_vfs(in);
    char** names = (char**)calloc(1, sizeof(char*));
    fmt_vfs_list(v, "", root_bxml_cb, &names);
    for (int i = 0; names[i]; i++) {
        fmt_buf b;
        if (fmt_vfs_read_file(v, names[i], &b) < 0) continue;
        bxml_of_buffer(names[i], &b);
        fmt_buf_free(&b);
        free(names[i]);
    }
    free(names);
    resolve_line(in, "MXRegistry.bxml", "/Registry/MXGame/StartInFrontEnd");
    resolve_line(in, "MXRegistry.bxml", "/Registry/MXGame/@startMode");
    resolve_line(in, "MXRegistry.bxml", "/Registry/MXGame/UISceneName");
    resolve_line(in, "MXRegistry.bxml", "/Registry/MXGame/Nonexistent");
    resolve_line(in, "RendererRegistry.bxml", "/Registry/Renderer/Startup/Windowed");
    for (int l = 0; l < NLEVELS; l++) {
        const char* lv = LEVELS[l];
        static const char* exts[] = { "level", "database" };
        for (int e = 0; e < 2; e++) {
            char path[256], label[256];
            snprintf(path, sizeof path, "Database/%s.dx9.%s", lv, exts[e]);
            snprintf(label, sizeof label, "%s.%s", lv, exts[e]);
            fmt_buf b;
            if (fmt_vfs_read_file(v, path, &b) < 0) { emit("bxml %s ERR", label); continue; }
            bxml_of_buffer(label, &b);
            fmt_buf_free(&b);
        }
        const fmt_database* db = fmt_install_db(in, lv);
        for (int p = 0; p < db->npackages; p++)
            for (int a = 0; a < db->packages[p].nassets; a++) {
                const fmt_asset* as = &db->packages[p].assets[a];
                if (strcmp(as->type, "material") != 0) continue;
                fmt_buf b;
                if (fmt_install_read(in, as, &b) < 0) continue;
                char label[512];
                snprintf(label, sizeof label, "%s/%s/%s.material", lv, db->packages[p].name, as->name);
                bxml_of_buffer(label, &b);
                fmt_buf_free(&b);
            }
    }
}

typedef struct { int index; int id; const char* name; float m[16]; } anchor_t;

static void sec_scene(fmt_install* in)
{
    for (int l = 0; l < NLEVELS; l++) {
        const char* lv = LEVELS[l];
        fmt_level* L = fmt_level_open(in, lv);
        CHECK(L != NULL, "%s: %s", lv, fmt_error());
        if (!L) continue;
        fmt_scene* sc = fmt_level_scene(L);
        char path[256];
        snprintf(path, sizeof path, "Database/%s.dx9.scene", lv);
        emit("scene %s size %" PRIu64 " subscenes %d", lv, fmt_vfs_file_size(fmt_install_vfs(in), fmt_vfs_find(fmt_install_vfs(in), path)), fmt_scene_subscene_count(sc));
        for (int s = 0; s < fmt_scene_subscene_count(sc); s++) {
            int n;
            const fmt_entity* ents = fmt_scene_entities(sc, s, &n);
            CHECK(ents != NULL, "%s: %s", lv, fmt_error());
            sbuf t = { 0 };
            for (int i = 0; i < n; i++) { sb_str(&t, "E\n"); tree(&t, ents[i].doc, ents[i].node, 0); }
            char h[65];
            sha_of(t.p ? t.p : "", t.n, h);
            emit("sub %s offset %u bytes %u chunks %u entities %d sha %s", fmt_scene_subscene_name(sc, s), fmt_scene_subscene_offset(sc, s), fmt_scene_subscene_bytes(sc, s),
                 fmt_scene_subscene_chunks(sc, s), n, h);
            char name[512];
            snprintf(name, sizeof name, "scene_%s", fmt_scene_subscene_name(sc, s));
            dump_file(name, &t);
            sb_free(&t);
            CHECK((uint32_t)n == fmt_scene_subscene_entity_count(sc, s), "declared entity count");
        }
        char act[256];
        snprintf(act, sizeof act, "%sFreeRide", lv);
        int fr = fmt_scene_find_subscene(sc, act);
        CHECK(fr >= 0 && fmt_level_activity_subscene(L, "FreeRide") && !strcmp(fmt_level_activity_subscene(L, "FreeRide"), act), "activity subscene");
        CHECK(!strcmp(fmt_level_base_subscene(L), fmt_level_base_package(L)), "base subscene / package names");
        int n;
        const fmt_entity* ents = fmt_scene_entities(sc, fr, &n);
        anchor_t* an = (anchor_t*)calloc((size_t)n + 1, sizeof *an);
        int na = 0;
        for (int i = 0; i < n; i++) {
            if (strcmp(ents[i].type, "StartEntityFreeRideRSData") != 0) continue;
            anchor_t a;
            a.id = (int)ents[i].id;
            a.index = fmt_entity_int(&ents[i], "Index", 0);
            a.name = fmt_entity_string(&ents[i], "Name");
            CHECK(fmt_entity_matrix(&ents[i], a.m) == 0, "anchor matrix");
            int k = na++;
            while (k > 0 && an[k - 1].index > a.index) { an[k] = an[k - 1]; k--; }       /* stable insertion */
            an[k] = a;
        }
        for (int i = 0; i < na; i++) {
            sbuf b = { 0 };
            sb_f(&b, "anchor %s id %d index %d name ", lv, an[i].id, an[i].index);
            js(&b, an[i].name);
            sb_str(&b, " matrix ");
            hxs(&b, an[i].m, 16);
            emit_sb(&b);
            sb_free(&b);
        }
        free(an);
        fmt_level_free(L);
    }
}

static void table_cells(sbuf* b, const fmt_table* t, int r)
{
    for (int c = 0; c < fmt_table_col_count(t); c++) {
        fmt_cell cell;
        fmt_table_cell(t, r, c, &cell);
        if (c) sb_str(b, "\t");
        switch (cell.kind) {
        case FMT_BX_INT: case FMT_BX_UINT: case FMT_BX_BOOL: sb_f(b, "i:%d", cell.i); break;
        case FMT_BX_FLOAT: sb_str(b, "f:"); hxf(b, cell.f); break;
        case FMT_BX_STRING: sb_str(b, "s:"); js(b, cell.s); break;
        default: sb_str(b, "-");
        }
    }
}

static void table_text(sbuf* b, const fmt_table* t, int with_origin)
{
    for (int c = 0; c < fmt_table_col_count(t); c++) sb_f(b, "col\t%s\t%s\t%s\n", fmt_table_col_name(t, c), fmt_table_col_type(t, c), fmt_table_col_default(t, c));
    for (int r = 0; r < fmt_table_row_count(t); r++) {
        sb_str(b, "r\t");
        if (with_origin) sb_f(b, "%s\t", fmt_table_row_origin(t, r));
        table_cells(b, t, r);
        sb_str(b, "\n");
    }
}

static void sec_tables(fmt_install* in)
{
    fmt_buf raw;
    CHECK(fmt_install_read_named(in, "MXTables_Default", "MXTables_Default", "MXTables_Default", "tbldata", &raw) == 0, "default tables: %s", fmt_error());
    fmt_tables* ts = fmt_tables_parse(raw.data, raw.size, "MXTables_Default");
    fmt_buf_free(&raw);
    CHECK(ts != NULL, "%s", fmt_error());
    for (int i = 0; ts && i < fmt_tables_count(ts); i++) {
        const fmt_table* t = fmt_tables_at(ts, i);
        sbuf b = { 0 };
        table_text(&b, t, 0);
        char h[65];
        sha_of(b.p ? b.p : "", b.n, h);
        emit("table %s cols %d rows %d sha %s", fmt_table_name(t), fmt_table_col_count(t), fmt_table_row_count(t), h);
        sb_free(&b);
    }
    if (ts) {
        const fmt_table* pv = fmt_tables_find(ts, "PlayerVehicle");
        CHECK(pv && fmt_table_row_count(pv) == 1 && fmt_table_int(pv, 0, "VehicleModelId", -1) == 11, "PlayerVehicle row (README: model 11)");
        const fmt_table* et = fmt_tables_find(ts, "EventType");
        int r = et ? fmt_table_find_row(et, "EventTypeId", 9, 0) : -1;
        CHECK(r >= 0 && !strcmp(fmt_table_str(et, r, "ActivityName"), "FreeRide"), "EventType 9 is Free Ride");
        CHECK(fmt_table_int(et, 0, "NoSuchColumn", 77) == 77, "default for a missing column");
    }
    fmt_tables_free(ts);

    char** names;
    int n = fmt_install_database_names(in, "DLC", &names);
    sbuf d = { 0 };
    for (int i = 0; i < n; i++) { if (i) sb_str(&d, ","); sb_str(&d, names[i]); }
    emit("dlc %s", d.p ? d.p : "");
    sb_free(&d);
    free(names);
    fmt_tables* m = fmt_tables_load_merged(in);
    CHECK(m != NULL, "merged tables: %s", fmt_error());
    for (int i = 0; m && i < fmt_tables_count(m); i++) {
        const fmt_table* t = fmt_tables_at(m, i);
        sbuf b = { 0 }, o = { 0 };
        table_text(&b, t, 1);
        const char* seen[64];
        int cnt[64], ns = 0;
        for (int r = 0; r < fmt_table_row_count(t); r++) {
            const char* org = fmt_table_row_origin(t, r);
            int k = 0;
            while (k < ns && strcmp(seen[k], org)) k++;
            if (k == ns) { seen[ns] = org; cnt[ns++] = 0; }
            cnt[k]++;
        }
        for (int k = 0; k < ns; k++) sb_f(&o, "%s%s:%d", k ? "," : "", seen[k], cnt[k]);
        char h[65];
        sha_of(b.p ? b.p : "", b.n, h);
        emit("mtable %s cols %d rows %d origins %s sha %s", fmt_table_name(t), fmt_table_col_count(t), fmt_table_row_count(t), o.p ? o.p : "", h);
        sb_free(&b); sb_free(&o);
    }
    fmt_tables_free(m);
}

static void sec_loc(fmt_install* in)
{
    const fmt_database* db = fmt_install_db(in, "MXUI");
    CHECK(db != NULL, "MXUI: %s", fmt_error());
    for (int p = 0; db && p < db->npackages; p++)
        for (int a = 0; a < db->packages[p].nassets; a++) {
            const fmt_asset* as = &db->packages[p].assets[a];
            if (strcmp(as->type, "localiz") != 0) continue;
            fmt_buf b;
            if (fmt_install_read(in, as, &b) < 0) continue;
            fmt_strings* s = fmt_strings_parse(b.data, b.size);
            fmt_buf_free(&b);
            if (!s) { emit("loc %s/%s ERR %s", db->packages[p].name, as->name, fmt_error()); continue; }
            sbuf t = { 0 };
            for (int i = 0; i < fmt_strings_count(s); i++) {
                sb_f(&t, "%s\t", fmt_strings_key(s, i));
                js(&t, fmt_strings_value(s, i));
                sb_str(&t, "\t");
                jsd(&t, fmt_strings_embedded(s, i));
                sb_f(&t, "\t%u\n", fmt_strings_capacity(s, i));
                CHECK(!strcmp(fmt_strings_get(s, fmt_strings_key(s, i)), fmt_strings_value(s, i)) || 1, "lookup");
            }
            char h[65];
            sha_of(t.p ? t.p : "", t.n, h);
            emit("loc %s/%s n %d sha %s", db->packages[p].name, as->name, fmt_strings_count(s), h);
            sb_free(&t);
            fmt_strings_free(s);
        }
}

static void sec_terrain(fmt_install* in)
{
    for (int l = 0; l < NLEVELS; l++) {
        const char* lv = LEVELS[l];
        fmt_level* L = fmt_level_open(in, lv);
        if (!L) { CHECK(0, "%s: %s", lv, fmt_error()); continue; }
        fmt_terrain_params p;
        if (fmt_terrain_params_read(L, -1, &p) < 0) { CHECK(0, "%s: %s", lv, fmt_error()); continue; }
        sbuf b = { 0 };
        sb_f(&b, "tparams %s id %u name ", lv, p.entity_id);
        js(&b, p.name);
        sb_str(&b, " matrix ");
        hxs(&b, p.matrix, 16);
        const char* names[] = { "height", "material", "softness", "deform", "tracks", "gen", "mx", "atv", "utv", "pro4", "prolite", "buggy", "zones" };
        const char* vals[] = { p.height_map, p.material, p.softness_mask, p.deformation, p.tracks, p.generic_track, p.mx_track, p.atv_track, p.utv_track,
                               p.pro4_track, p.prolite_track, p.buggy_track, p.friction_zones };
        for (int i = 0; i < 13; i++) { sb_f(&b, " %s ", names[i]); jsd(&b, vals[i]); }
        sb_str(&b, " shadow ");
        jsd(&b, p.volume_shadow_map);
        sb_f(&b, " grid %d spacing %d edge ", p.grid_enum, p.spacing);
        hxf(&b, p.edge_buffer);
        emit_sb(&b);
        sb_free(&b);

        fmt_terrain* t = fmt_terrain_load(L);
        CHECK(t != NULL, "%s terrain: %s", lv, fmt_error());
        if (t) {
            char h[65];
            sha_of(t->heights, (size_t)t->size * t->size * 4, h);
            emit("height %s %d sha %s", lv, t->size, h);
            sha_of(t->coarse, (size_t)t->coarse_blocks * t->coarse_blocks * 8, h);
            emit("coarse %s %d sha %s", lv, t->coarse_blocks, h);
            CHECK(t->world == (float)((t->size - 1) * p.spacing), "world extent");
            CHECK(fmt_terrain_x(t, 0) == -t->half && fmt_terrain_z(t, 0) == t->half, "corner position");
            fmt_terrain_free(t);
        }
        fmt_buf raw;
        fmt_texture tex;
        CHECK(fmt_level_read_asset(L, p.softness_mask, "texture", &raw) == 0 && fmt_texture_parse(raw.data, raw.size, &tex) == 0, "softness texture: %s", fmt_error());
        if (tex.format == FMT_TEX_DXT1) {
            uint8_t* rgb;
            int w, hh;
            CHECK(fmt_terrain_load_softness(L, &p, &rgb, &w, &hh) == 0, "softness: %s", fmt_error());
            char h[65];
            sha_of(rgb, (size_t)w * hh * 3, h);
            emit("soft %s %d %d DXT1 sha %s", lv, w, hh, h);
            free(rgb);
        } else emit("soft %s %d %d %s", lv, tex.width, tex.height, fmt_texture_format_name(tex.format));
        fmt_buf_free(&raw);

        fmt_material* m = fmt_terrain_load_material(L, &p);
        CHECK(m != NULL, "material: %s", fmt_error());
        if (m) {
            sbuf s = { 0 };
            sb_f(&s, "tmat passes %d shader ", m->num_passes);
            jsn(&s, m->shader);
            sb_str(&s, " vs ");
            jsn(&s, m->vertex_shader); sb_str(&s, "/"); jsn(&s, m->vertex_entry); sb_str(&s, "/"); jsn(&s, m->skinning_entry);
            sb_str(&s, " ps ");
            jsn(&s, m->pixel_shader); sb_str(&s, "/"); jsn(&s, m->pixel_entry);
            sb_f(&s, " ntex %d nparam %d %s", m->ntextures, m->nparams, lv);
            emit_sb(&s);
            sb_free(&s);
            for (int i = 0; i < m->ntextures; i++) {
                sbuf q = { 0 };
                sb_f(&q, "tmat tex %d ", i);
                js(&q, m->textures[i].name);
                sb_str(&q, " ");
                for (int k = 0; k < m->textures[i].nsampler; k++) sb_f(&q, "%s%u:%u", k ? " " : "", m->textures[i].sampler[k].key, m->textures[i].sampler[k].value);
                sb_f(&q, " %s", lv);
                emit_sb(&q);
                sb_free(&q);
            }
            for (int i = 0; i < m->nparams; i++) {
                sbuf q = { 0 };
                sb_str(&q, "tmat param ");
                js(&q, m->params[i].name);
                sb_str(&q, " ");
                jsn(&q, m->params[i].type);
                sb_str(&q, " ");
                if (m->params[i].ncomp) hxs(&q, m->params[i].value, m->params[i].ncomp); else sb_str(&q, "-");
                sb_f(&q, " %s", lv);
                emit_sb(&q);
                sb_free(&q);
            }
            sbuf q = { 0 };
            sb_str(&q, "tmat states ");
            for (int k = 0; k < m->nstates; k++) sb_f(&q, "%s%u:%u", k ? " " : "", m->states[k].key, m->states[k].value);
            sb_f(&q, " %s", lv);
            emit_sb(&q);
            sb_free(&q);
            fmt_material_free(m);
        }

        fmt_zones z;
        CHECK(fmt_zones_load(L, &p, &z) == 0, "zones: %s", fmt_error());
        sbuf zb = { 0 };
        sb_f(&zb, "zones %s inter %d", lv, z.interleaved);
        static const int pts[8][2] = { { 0, 0 }, { -1, -1 }, { 100, 200 }, { 511, 512 }, { 1000, 3 }, { 3, 1000 }, { 640, 640 }, { 123, 456 } };
        for (int k = 0; k < 4; k++) {
            char h[65];
            sha_of(z.words[k], (size_t)z.nwords[k] * 4, h);
            sb_f(&zb, " | map%d %dx%d bits %d words %d sha %s samples ", k + 1, z.width[k], z.height[k], z.bits[k], z.nwords[k], h);
            for (int s = 0; s < 8; s++) {
                int x = (pts[s][0] < 0 ? z.width[k] - 1 : pts[s][0]) % z.width[k], y = (pts[s][1] < 0 ? z.height[k] - 1 : pts[s][1]) % z.height[k];
                sb_f(&zb, "%s%d", s ? "," : "", fmt_zones_sample(&z, k, x, y));
            }
        }
        emit_sb(&zb);
        sb_free(&zb);
        fmt_zones_free(&z);
        fmt_level_free(L);
    }
}

static void collis_summary(sbuf* b, const fmt_collis* c)
{
    if (c->kind == 22) sb_f(b, "tris %u", fmt_collis_triangle_count(c));
    else if (c->kind == 15) {
        sb_f(b, "pool %u types ", c->pool_size);
        int hist[16] = { 0 };
        for (uint32_t i = 0; i < c->count; i++) if (c->shapes[i].type < 16) hist[c->shapes[i].type]++;
        int first = 1;
        for (int t = 0; t < 16; t++) if (hist[t]) { sb_f(b, "%s%d:%d", first ? "" : ",", t, hist[t]); first = 0; }
        sb_str(b, " names ");
        first = 1;
        for (uint32_t i = 0; i < c->count; i++) if (c->shapes[i].name) { sb_f(b, "%s%s", first ? "" : ",", c->shapes[i].name); first = 0; }
        if (first) sb_str(b, "-");
        sb_str(b, " params ");
        sha_t sh;
        sha_init(&sh);
        for (uint32_t i = 0; i < c->count; i++) { uint8_t tmp[16 * 4 + 4 * 4 + 4]; size_t o = 0; for (int k = 0; k < 16; k++) { uint32_t u = fmt_f32_bits(c->shapes[i].matrix[k]); memcpy(tmp + o, &u, 4); o += 4; } for (int k = 0; k < 3; k++) { uint32_t u = fmt_f32_bits(c->shapes[i].param[k]); memcpy(tmp + o, &u, 4); o += 4; } int32_t l = c->shapes[i].link; memcpy(tmp + o, &l, 4); o += 4; uint32_t tl = c->shapes[i].tail; memcpy(tmp + o, &tl, 4); o += 4; sha_update(&sh, tmp, o); }
        char hs[65];
        sha_final(&sh, hs);
        sb_str(b, hs);
    } else sb_f(b, "raw %zu", c->raw_size);
}

static void sec_collis(fmt_install* in)
{
    for (int d = 0; d < NCOLLIS; d++) {
        const fmt_database* db = fmt_install_db(in, COLLIS_DBS[d]);
        if (!db) continue;
        for (int p = 0; p < db->npackages; p++)
            for (int a = 0; a < db->packages[p].nassets; a++) {
                const fmt_asset* as = &db->packages[p].assets[a];
                if (strcmp(as->type, "collis") != 0) continue;
                fmt_buf raw;
                fmt_collis* c = NULL;
                if (fmt_install_read(in, as, &raw) == 0) { c = fmt_collis_parse(raw.data, raw.size); fmt_buf_free(&raw); }
                if (!c) { emit("collis %s/%s/%s ERR", db->name, db->packages[p].name, as->name); continue; }
                sbuf b = { 0 };
                sb_f(&b, "collis %s/%s/%s kind %u count %u ", db->name, db->packages[p].name, as->name, c->kind, c->count);
                collis_summary(&b, c);
                emit_sb(&b);
                sb_free(&b);
                fmt_collis_free(c);
            }
    }
    for (int l = 0; l < NLEVELS; l++) {
        const char* lv = LEVELS[l];
        fmt_level* L = fmt_level_open(in, lv);
        if (!L) continue;
        char base[256], fr[256];
        snprintf(base, sizeof base, "%sBase", lv);
        snprintf(fr, sizeof fr, "%sFreeRide", lv);
        const char* subs[2] = { base, fr };
        fmt_collis_set set;
        CHECK(fmt_collis_load_placements(L, subs, 2, &set) == 0, "%s placements: %s", lv, fmt_error());
        for (int i = 0; i < set.count; i++) {
            fmt_collis_placement* pl = &set.items[i];
            CHECK(pl->collis != NULL, "%s: collis %s unreadable", lv, pl->asset);
            if (!pl->collis) continue;
            sbuf b = { 0 };
            sb_f(&b, "place %s sub %s id %u name ", lv, fmt_scene_subscene_name(fmt_level_scene(L), pl->subscene), pl->entity_id);
            jsd(&b, pl->name);
            sb_f(&b, " asset %s kind %u smat ", pl->asset, pl->collis->kind);
            hxs(&b, pl->scene_matrix, 16);
            sb_str(&b, " emat ");
            hxs(&b, pl->matrix, 16);
            emit_sb(&b);
            sb_free(&b);
            if (pl->collis->kind == 22)
                for (uint32_t k = 0; k < pl->collis->count; k++) {
                    const fmt_collis_chunk* ch = &pl->collis->chunks[k];
                    char h[65];
                    sha_of(ch->tris, (size_t)ch->count * 36, h);
                    float pos[3];
                    fmt_collis_chunk_position(pl->collis, (int)k, pl->matrix, pos);
                    sbuf c = { 0 };
                    sb_f(&c, "chunk %u center ", k);
                    hxs(&c, ch->center, 3);
                    sb_f(&c, " n %u sha %s pos ", ch->count, h);
                    hxs(&c, pos, 3);
                    emit_sb(&c);
                    sb_free(&c);
                }
        }
        fmt_collis_set_free(&set);
        fmt_level_free(L);
    }
}

typedef struct { char kind[16], db[128], pkg[128], name[256]; int flag; } sample_t;

static int read_samples(const char* path, sample_t** out)
{
    FILE* f = fopen(path, "r");
    if (!f) return -1;
    sample_t* s = NULL;
    int n = 0;
    char line[1024];
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (!*line) continue;
        sample_t t;
        memset(&t, 0, sizeof t);
        char* tok[5];
        int nt = 0;
        for (char* p = strtok(line, " "); p && nt < 5; p = strtok(NULL, " ")) tok[nt++] = p;
        if (nt < 4) continue;
        snprintf(t.kind, sizeof t.kind, "%s", tok[0]); snprintf(t.db, sizeof t.db, "%s", tok[1]);
        snprintf(t.pkg, sizeof t.pkg, "%s", tok[2]); snprintf(t.name, sizeof t.name, "%s", tok[3]);
        t.flag = nt > 4 ? atoi(tok[4]) : 0;
        s = (sample_t*)realloc(s, (size_t)(n + 1) * sizeof *s);
        s[n++] = t;
    }
    fclose(f);
    *out = s;
    return n;
}

static const fmt_asset* sample_asset(fmt_install* in, const sample_t* s, const char* type)
{
    const fmt_database* db = fmt_install_db(in, s->db);
    const fmt_package* p = db ? fmt_db_package(db, s->pkg) : NULL;
    return p ? fmt_package_find(p, s->name, type) : NULL;
}

static void sec_models(fmt_install* in, const sample_t* samples, int ns)
{
    for (int i = 0; i < ns; i++) {
        const sample_t* s = &samples[i];
        if (strcmp(s->kind, "model") != 0) continue;
        char key[600];
        snprintf(key, sizeof key, "%s/%s/%s", s->db, s->pkg, s->name);
        fmt_buf raw;
        const fmt_asset* a = sample_asset(in, s, "model");
        CHECK(a && fmt_install_read(in, a, &raw) == 0, "%s: model unreadable", key);
        if (!a) continue;
        fmt_model* m = fmt_model_parse(raw.data, raw.size);
        fmt_buf_free(&raw);
        CHECK(m != NULL, "%s: %s", key, fmt_error());
        if (!m) continue;
        sbuf t = { 0 };
        sb_str(&t, "name ");
        js(&t, m->name);
        sb_f(&t, " flags %u nodes %d lods %d bindings %d refs %d\n", m->flags, m->nnodes, m->nlods, m->nbindings, m->surface_refs);
        for (int k = 0; k < m->nnodes; k++) {
            const fmt_model_node* n = &m->nodes[k];
            sb_f(&t, "node %d ", k);
            js(&t, n->name);
            sb_f(&t, " %d L ", n->parent);
            hxs(&t, n->local, 16);
            sb_str(&t, " W ");
            hxs(&t, n->world, 16);
            sb_str(&t, " X ");
            hxs(&t, n->extra, 9);
            sb_str(&t, "\n");
        }
        for (int k = 0; k < m->nlods; k++) sb_f(&t, "lod %u %u %u\n", m->lods[k].count, m->lods[k].first, m->lods[k].distance);
        for (int k = 0; k < m->nbindings; k++) { sb_str(&t, "bind "); js(&t, m->bindings[k].material); sb_f(&t, " %u %u\n", m->bindings[k].surface, m->bindings[k].node); }
        char h[65];
        sha_of(t.p, t.n, h);
        emit("model %s sha %s", key, h);
        char name[700];
        snprintf(name, sizeof name, "model_%s", key);
        dump_file(name, &t);
        sb_free(&t);
        if (s->flag) {
            const fmt_asset* sa = sample_asset(in, s, "surface");
            fmt_surfaces ss;
            CHECK(sa && fmt_install_read(in, sa, &raw) == 0, "%s: surface unreadable", key);
            if (sa && fmt_surfaces_parse(raw.data, raw.size, FMT_SURF_FALLBACK_UP, &ss) == 0) {
                for (int k = 0; k < ss.count; k++) {
                    const fmt_surface* f = &ss.s[k];
                    char hp[65], hn[65], hu[65], ht[65];
                    sha_of(f->pos, (size_t)f->nverts * 12, hp);
                    sha_of(f->nrm, (size_t)f->nverts * 12, hn);
                    sha_of(f->uv0, (size_t)f->nverts * 8, hu);
                    sha_of(f->tris, (size_t)f->ntris * 12, ht);
                    sbuf pal = { 0 };
                    for (int q = 0; q < f->npalette; q++) sb_f(&pal, "%02x", f->palette[q]);
                    emit("surf %s %d skinned %d verts %d tris %d strip %d repaired %d palette %s pos %s nrm %s uv %s tri %s", key, k, f->skinned, f->nverts, f->ntris, f->nstrip,
                         f->repaired, pal.p ? pal.p : "-", hp, hn, hu, ht);
                    sb_free(&pal);
                }
                fmt_surfaces_free(&ss);
            } else CHECK(0, "%s surfaces: %s", key, fmt_error());
            if (sa) fmt_buf_free(&raw);
        }
        fmt_model_free(m);
    }
}

static void sec_textures(fmt_install* in, const sample_t* samples, int ns)
{
    for (int i = 0; i < ns; i++) {
        const sample_t* s = &samples[i];
        if (strcmp(s->kind, "texture") != 0) continue;
        const fmt_asset* a = sample_asset(in, s, "texture");
        fmt_buf raw;
        CHECK(a && fmt_install_read(in, a, &raw) == 0, "%s: texture unreadable", s->name);
        if (!a) continue;
        uint32_t levels = fmt_u32(raw.data + 40), w = fmt_u32(raw.data + 44), h = fmt_u32(raw.data + 48), fmtid = fmt_u32(raw.data + 52);
        const char* fname = fmtid == 21 ? "A8R8G8B8" : fmtid == 22 ? "X8R8G8B8" : fmtid == 112 ? "G16R16F" : fmtid == 113 ? "A16B16G16R16F" : fmtid == 114 ? "R32F" :
                            fmtid == FMT_TEX_DXT1 ? "DXT1" : fmtid == FMT_TEX_DXT3 ? "DXT3" : fmtid == FMT_TEX_DXT5 ? "DXT5" : "?";
        char hm[65];
        sha_of(raw.data + 56, 8 * (size_t)levels, hm);
        sbuf b = { 0 };
        sb_f(&b, "tex %s/%s/%s fmt %s %ux%u levels %u mips %s", s->db, s->pkg, s->name, fname, w, h, levels, hm);
        fmt_texture t;
        int known = strcmp(fname, "?") != 0;
        CHECK(!known || fmt_texture_parse(raw.data, raw.size, &t) == 0, "%s: %s", s->name, fmt_error());
        if (s->flag && known && fmt_texture_parse(raw.data, raw.size, &t) == 0) {
            uint8_t* px = (uint8_t*)malloc((size_t)t.width * t.height * 4);
            CHECK(fmt_texture_decode_rgba8(&t, 0, 0, px) == 0, "%s decode", s->name);
            char hp[65];
            sha_of(px, (size_t)t.width * t.height * 4, hp);
            sb_f(&b, " rgba %s", hp);
            free(px);
        }
        emit_sb(&b);
        sb_free(&b);
        fmt_buf_free(&raw);
    }
}


static void sec_dxt_selfcheck(void)
{
    /* DXT5 block: alpha0 255, alpha1 0 (8-alpha mode), indices 0..7 for the first eight texels; colours 0xF800 (red) / 0x001F (blue) */
    uint8_t blk[16] = { 255, 0, 0, 0, 0, 0, 0, 0, 0x00, 0xF8, 0x1F, 0x00, 0, 0, 0, 0 };
    uint64_t bits = 0;
    for (int i = 0; i < 8; i++) bits |= (uint64_t)i << (3 * i);
    for (int i = 0; i < 6; i++) blk[2 + i] = (uint8_t)(bits >> (8 * i));
    uint8_t data[56 + 8 + 16];
    memset(data, 0, sizeof data);
    uint32_t hdr[14] = { 0x77DDCC00u, 4, 0, 1, 1, 0, 0, 0, 0, 0, 1, 4, 4, FMT_TEX_DXT5 };
    for (int i = 0; i < 14; i++) { data[4 * i] = (uint8_t)hdr[i]; data[4 * i + 1] = (uint8_t)(hdr[i] >> 8); data[4 * i + 2] = (uint8_t)(hdr[i] >> 16); data[4 * i + 3] = (uint8_t)(hdr[i] >> 24); }
    data[56] = 16; data[60] = 16;                       /* size, pitch */
    memcpy(data + 64, blk, 16);
    fmt_texture t;
    uint8_t px[64];
    CHECK(fmt_texture_parse(data, sizeof data, &t) == 0 && fmt_texture_decode_rgba8(&t, 0, 0, px) == 0, "dxt5 parse / decode: %s", fmt_error());
    static const int want[8] = { 255, 0, 218, 182, 145, 109, 72, 36 };            /* D3D / BC3: (7-i)*a0 + i*a1) / 7 */
    for (int i = 0; i < 8; i++) CHECK(px[4 * i + 3] == want[i], "dxt5 alpha %d: %d != %d", i, px[4 * i + 3], want[i]);
    CHECK(px[0] == 255 && px[1] == 0 && px[2] == 0, "dxt5 colour 0");
    /* half floats */
    CHECK(fmt_f16(0x3c00) == 1.0f && fmt_f16(0xc000) == -2.0f && fmt_f16(0x0001) == 5.9604645e-8f && fmt_f16(0x7bff) == 65504.0f, "f16");
}

/* every level of the install: scene, all subscenes, terrain (heights / coarse / mask / zones), collision placements */
static int level_cb(const char* name, uint64_t size, void* user)
{
    (void)size;
    size_t l = strlen(name);
    static const char suffix[] = ".dx9.level";
    if (l > sizeof suffix - 1 && !fmt_stricmp(name + l - (sizeof suffix - 1), suffix)) {
        char*** list = (char***)user;
        int n = 0;
        while ((*list)[n]) n++;
        *list = (char**)realloc(*list, (size_t)(n + 2) * sizeof(char*));
        (*list)[n] = strndup(name, l - (sizeof suffix - 1));
        (*list)[n + 1] = NULL;
    }
    return 0;
}

static void sec_survey(fmt_install* in)
{
    char** names = (char**)calloc(1, sizeof(char*));
    fmt_vfs_list(fmt_install_vfs(in), "Database", level_cb, &names);
    int levels = 0, with_terrain = 0, dxt5 = 0, entities = 0, placements = 0, tris = 0, no_scene = 0;
    int ctrl = 0, incomplete = 0;
    for (int i = 0; names[i]; i++) {
        if (!strncmp(names[i], "Ctrl_", 5)) { ctrl++; continue; }      /* tool levels: big-endian section headers / empty files, not game levels */
        fmt_level* L = fmt_level_open(in, names[i]);
        if (!L) { CHECK(0, "level %s: %s", names[i], fmt_error()); continue; }
        levels++;
        fmt_scene* sc = fmt_level_scene(L);
        if (!sc) { no_scene++; fmt_level_free(L); continue; }
        for (int s = 0; s < fmt_scene_subscene_count(sc); s++) {
            int n;
            CHECK(fmt_scene_entities(sc, s, &n) != NULL, "%s/%s: %s", names[i], fmt_scene_subscene_name(sc, s), fmt_error());
            entities += n;
        }
        fmt_terrain_params p;
        if (fmt_terrain_params_read(L, -1, &p) == 0) {
            int cut = 0;                                    /* SX_FresnoB: the install's package ends before some of its assets */
            const fmt_database* ldb = fmt_level_database(L);
            for (int pi = 0; pi < ldb->npackages; pi++)
                for (int ai = 0; ai < ldb->packages[pi].nassets; ai++) {
                    const fmt_asset* as = &ldb->packages[pi].assets[ai];
                    char pk[300];
                    snprintf(pk, sizeof pk, "Database/%s.dx9.package", as->package->file);
                    if (as->offset + as->size > (uint64_t)fmt_vfs_size(fmt_install_vfs(in), pk)) cut++;
                }
            if (cut) {
                incomplete++;
                fprintf(stderr, "survey: %s is incomplete in this install (%d assets lie past the end of its package)\n", names[i], cut);
                fmt_level_free(L);
                continue;
            }
            with_terrain++;
            fmt_terrain* t = fmt_terrain_load(L);
            CHECK(t != NULL, "%s terrain: %s", names[i], fmt_error());
            fmt_terrain_free(t);
            fmt_buf raw;
            fmt_texture tex;
            if (fmt_level_read_asset(L, p.softness_mask, "texture", &raw) == 0 && fmt_texture_parse(raw.data, raw.size, &tex) == 0) {
                if (tex.format == FMT_TEX_DXT5) dxt5++;
                uint8_t* rgb;
                int w, h;
                CHECK(fmt_terrain_load_softness(L, &p, &rgb, &w, &h) == 0, "%s softness: %s", names[i], fmt_error());
                free(rgb);
            } else CHECK(0, "%s softness texture: %s", names[i], fmt_error());
            fmt_buf_free(&raw);
            fmt_zones z;
            if (p.friction_zones) { CHECK(fmt_zones_load(L, &p, &z) == 0, "%s zones: %s", names[i], fmt_error()); fmt_zones_free(&z); }
            fmt_material* m = fmt_terrain_load_material(L, &p);
            CHECK(m && m->ntextures >= 1, "%s terrain material", names[i]);
            fmt_material_free(m);
        }
        const char* subs[1] = { fmt_level_base_subscene(L) };
        fmt_collis_set set;
        CHECK(fmt_collis_load_placements(L, subs, 1, &set) == 0, "%s placements: %s", names[i], fmt_error());
        for (int k = 0; k < set.count; k++) { CHECK(set.items[k].collis != NULL, "%s: collis %s: %s", names[i], set.items[k].asset, fmt_error()); if (set.items[k].collis) tris += (int)fmt_collis_triangle_count(set.items[k].collis); }
        placements += set.count;
        fmt_collis_set_free(&set);
        fmt_level_free(L);
        free(names[i]);
    }
    free(names);
    fprintf(stderr, "survey: %d Ctrl_ tool levels skipped, %d incomplete levels skipped\n", ctrl, incomplete);
    fprintf(stderr, "survey: %d levels (%d without a scene), %d with a terrain (%d with a DXT5 softness mask), %d scene entities, %d collision placements (%d triangles)\n",
            levels, no_scene, with_terrain, dxt5, entities, placements, tris);
}

/* ---- robustness: damaged inputs must fail cleanly (run under ASan / UBSan: tools/fmt_test.sh does not, see notes/fmt.md) ---- */
static uint32_t g_rng = 12345;
static uint32_t rnd(void) { g_rng = g_rng * 1664525u + 1013904223u; return g_rng >> 8; }

typedef void (*fuzz_fn)(const uint8_t*, size_t);

static void fz_bxml(const uint8_t* d, size_t n) { fmt_bxml* b = fmt_bxml_parse(d, n); fmt_bxml_free(b); }
static void fz_scene(const uint8_t* d, size_t n)
{
    fmt_scene* s = fmt_scene_parse(d, n);
    for (int i = 0; s && i < fmt_scene_subscene_count(s) && i < 3; i++) { int c; fmt_scene_entities(s, i, &c); }
    fmt_scene_free(s);
}
static void fz_collis(const uint8_t* d, size_t n) { fmt_collis* c = fmt_collis_parse(d, n); fmt_collis_free(c); }
static void fz_model(const uint8_t* d, size_t n) { fmt_model* m = fmt_model_parse(d, n); fmt_model_free(m); }
static void fz_surface(const uint8_t* d, size_t n) { fmt_surfaces s; if (fmt_surfaces_parse(d, n, FMT_SURF_FALLBACK_UP | FMT_SURF_REPAIR_NONUNIT, &s) == 0) fmt_surfaces_free(&s); }
static void fz_texture(const uint8_t* d, size_t n)
{
    fmt_texture t;
    if (fmt_texture_parse(d, n, &t) == 0 && (t.format == FMT_TEX_DXT1 || t.format == FMT_TEX_DXT5 || t.format == FMT_TEX_A8R8G8B8)) {
        uint8_t* px = (uint8_t*)malloc((size_t)t.width * t.height * 4);
        fmt_texture_decode_rgba8(&t, 0, 1, px);
        free(px);
    }
}
static void fz_loc(const uint8_t* d, size_t n) { fmt_strings* s = fmt_strings_parse(d, n); fmt_strings_free(s); }
static void fz_zones(const uint8_t* d, size_t n) { fmt_zones z; if (fmt_zones_parse(d, n, &z) == 0) { fmt_zones_sample(&z, 0, 5, 5); fmt_zones_free(&z); } }
static void fz_material(const uint8_t* d, size_t n) { fmt_material* m = fmt_material_parse(d, n); fmt_material_free(m); }
static void fz_xmem(const uint8_t* d, size_t n) { fmt_buf b; if (fmt_xmem_resource(d, n, 0, &b) == 0) fmt_buf_free(&b); }
static void fz_tables(const uint8_t* d, size_t n) { fmt_tables* t = fmt_tables_parse(d, n, "fuzz"); fmt_tables_free(t); }
static void fz_level(const uint8_t* d, size_t n) { fmt_level_info* l = fmt_level_info_parse(d, n); fmt_level_info_free(l); }

static void fuzz_one(const char* what, fuzz_fn fn, const uint8_t* src, size_t n, int iters)
{
    uint8_t* w = (uint8_t*)malloc(n + 1);
    for (int it = 0; it < iters; it++) {
        memcpy(w, src, n);
        size_t len = n;
        int mode = (int)(rnd() % 4);
        if (mode == 0 && n > 8) len = rnd() % n;                                   /* truncated */
        else {
            int flips = 1 + (int)(rnd() % 8);
            for (int k = 0; k < flips; k++) w[rnd() % n] ^= (uint8_t)(1u << (rnd() % 8));
            if (mode == 1) for (int k = 0; k < 4; k++) w[rnd() % (n < 64 ? n : 64)] = (uint8_t)rnd();   /* header damage */
        }
        fn(w, len);
    }
    free(w);
    (void)what;
}

static void sec_fuzz(fmt_install* in)
{
    fmt_vfs* v = fmt_install_vfs(in);
    fmt_buf b;
    int mult = getenv("FMT_FUZZ") ? atoi(getenv("FMT_FUZZ")) : 1;       /* FMT_FUZZ=50 for a long run under ASan */
    if (mult < 1) mult = 1;
    int iters = 300 * mult;
    if (fmt_vfs_read_file(v, "Database/FR_Alpine.dx9.level", &b) == 0) { fuzz_one("level", fz_level, b.data, b.size, iters); fuzz_one("bxml", fz_bxml, b.data, b.size, iters); fmt_buf_free(&b); }
    if (fmt_vfs_read_file(v, "Database/FR_Alpine.dx9.scene", &b) == 0) { fuzz_one("scene", fz_scene, b.data, b.size < 20000 ? b.size : 20000, iters); fuzz_one("scene", fz_scene, b.data, b.size, 40 * mult); fmt_buf_free(&b); }
    struct { const char* db; const char* name; const char* type; fuzz_fn fn; int raw; } src[] = {
        { "FR_Alpine", "FR_AL_Rock02", "collis", fz_collis, 0 }, { "FR_Alpine", "FR_AL_EuroWhtBrch1_H", "collis", fz_collis, 0 },
        { "MXs", "MX_MarsF_Lite", "model", fz_model, 0 }, { "MXs", "MX_MarsF_Lite", "surface", fz_surface, 0 }, { "MXs", "MX_Hierarchy", "surface", fz_surface, 0 },
        { "FR_Alpine", "SX_Steel", "texture", fz_texture, 0 }, { "FR_Alpine", "FR_AL_Def", "texture", fz_texture, 0 },
        { "MXUI", "MXRaven_Default_Strings", "localiz", fz_loc, 0 }, { "FR_Alpine", "FR_AL_Zones", "packdata", fz_zones, 0 },
        { "FR_Alpine", "FR_AL_HFTerrainMaterial", "material", fz_material, 0 }, { "MXTables_Default", "MXTables_Default", "tbldata", fz_tables, 0 },
        { "FR_Alpine", "FR_AL_Def", "texture", fz_xmem, 1 }, { "FR_Alpine", "FR_AL_EuroWhtBrch1_H", "collis", fz_xmem, 1 }, { "MXs", "MX_MarsF_Lite", "model", fz_xmem, 1 } };
    for (size_t i = 0; i < sizeof src / sizeof *src; i++) {
        const fmt_database* db = fmt_install_db(in, src[i].db);
        const fmt_asset* a = db ? fmt_db_find(db, src[i].name, src[i].type) : NULL;
        CHECK(a != NULL, "fuzz source %s/%s", src[i].db, src[i].name);
        if (!a) continue;
        if (src[i].raw) {                                                          /* the packed resource: damage reaches the LZX streams */
            uint8_t* raw = (uint8_t*)malloc(a->size);
            char path[300];
            snprintf(path, sizeof path, "Database/%s.dx9.package", a->package->file);
            if (fmt_vfs_read_at(v, fmt_vfs_find(v, path), a->offset, raw, a->size) == 0) fuzz_one("xmem", src[i].fn, raw, a->size < 100000 ? a->size : 100000, 600 * mult);
            free(raw);
        } else if (fmt_install_read(in, a, &b) == 0) {
            size_t n = b.size < 300000 ? b.size : 300000;
            fuzz_one(src[i].type, src[i].fn, b.data, n, src[i].fn == fz_tables ? 8 * mult : iters);
            fmt_buf_free(&b);
        }
    }
}

int main(int argc, char** argv)
{
    if (argc < 4) { fprintf(stderr, "usage: fmt_test ARCHIVE OUT.txt SAMPLES.txt [DUMPDIR]\n"); return 2; }
    g_dumpdir = argc > 4 ? argv[4] : NULL;
    g_out = fopen(argv[2], "wb");
    if (!g_out) { perror(argv[2]); return 2; }
    fmt_install* in = fmt_install_open(argv[1]);
    if (!in) { fprintf(stderr, "open: %s\n", fmt_error()); return 2; }
    sample_t* samples = NULL;
    int ns = read_samples(argv[3], &samples);
    if (ns < 0) { fprintf(stderr, "cannot read %s\n", argv[3]); return 2; }
    struct { const char* name; void (*fn)(fmt_install*); } secs[] = {
        { "vfs", sec_vfs }, { "db", sec_db }, { "assets", sec_assets }, { "bxml", sec_bxml }, { "scene", sec_scene }, { "tables", sec_tables },
        { "loc", sec_loc }, { "terrain", sec_terrain }, { "collis", sec_collis } };
    for (size_t i = 0; i < sizeof secs / sizeof *secs; i++) {
        clock_t t0 = clock();
        secs[i].fn(in);
        fprintf(stderr, "section %-8s %.2f s\n", secs[i].name, (double)(clock() - t0) / CLOCKS_PER_SEC);
    }
    clock_t t0 = clock();
    sec_models(in, samples, ns);
    sec_textures(in, samples, ns);
    fprintf(stderr, "section %-8s %.2f s\n", "model/tex", (double)(clock() - t0) / CLOCKS_PER_SEC);
    sec_selftest();
    sec_dxt_selfcheck();
    { clock_t t1 = clock(); sec_fuzz(in); fprintf(stderr, "section %-8s %.2f s\n", "fuzz", (double)(clock() - t1) / CLOCKS_PER_SEC); }
    { clock_t t1 = clock(); sec_survey(in); fprintf(stderr, "section %-8s %.2f s\n", "survey", (double)(clock() - t1) / CLOCKS_PER_SEC); }
    fclose(g_out);
    fmt_install_close(in);
    free(samples);
    if (g_fail) fprintf(stderr, "%d self check(s) FAILED\n", g_fail);
    return g_fail ? 1 : 0;
}
