/* rxpack.c -- Reflex archive tool (core/plan/RXARCHIVE.md): packs game data into .rxdata, converts tracks into .rxtrack version 5,
 * lists, verifies and extracts archives. A developer / modder tool (macOS / Linux); the shipped client packs and reads through the
 * same core code (src/fmt_rxar.c, the rx_archive_* exports).
 *
 *   rxpack data   <game root> <out.rxdata> [--dir Assets]... [--skip-levels] [--level N] [--jobs N]
 *        every file below <game root>/<dir> (default: Assets, the import's extracted assets) as "<dir>/<path>"; --skip-levels leaves
 *        out the assets of every level the root has (Assets/<type>/<level>/...: they go into Tracks/<level>.rxtrack)
 *   rxpack track  <game root> <level> <out.rxtrack> [--level N] [--jobs N] [--name S] [--source base|alive|mod]
 *        one level of an install or imported library: Database/<level>.dx9.database / .level / .scene (and .package when the root
 *        has it), its extracted assets (Assets/<type>/<level>/...), an Alive level's sidecars (Database/alive-<level>-...,
 *        alive-import-<level>.json) and its alive-levels.json registry entry (metadata "alive")
 *   rxpack tracks <game root> <out dir> [--only L1,L2,...] [--level N] [--jobs N]
 *        `track` for every Database <level>.dx9.level of the root (base tracks, Alive tracks: --source from the registry)
 *   rxpack mod    <mod folder> <out.rxtrack> [--stem S] [--level N]
 *        an old Reflex track mod (Mods/Tracks folder or a Beta_*_Slot set: <stem>.dx9.database / .level / .package / .scene, byte for
 *        byte) plus its dress (mod.ini, mod.json, pictures, .txt) under Mod/
 *   rxpack list    <archive>                    the files: codec, raw and stored bytes
 *   rxpack verify  <archive> [<game root>]      decodes every file (CRC-32 and zstd checksums); with a root, compares each file byte for
 *                                               byte with <root>/<path>
 *   rxpack extract <archive> <out dir>          every file to <out dir>/<path>
 *   rxpack unpack  <game root> [--delete]       a packed library back to loose files (pool references resolved); the archives
 *                                               moved to <root>/archives-unpacked/ (or deleted)
 *   rxpack dupes   <game root>                  how much of a packed library is the same bytes more than once
 *   rxpack bench   <game root> <list file>      reads the listed paths through the core file system (fmt_vfs), loose then packed
 *                                               (REFLEX_NO_ARCHIVES), and prints the time of each pass
 * Build: C=$PWD/core O=<out>; source core/tools/product_link.sh; rx_product_objs && rx_link_driver <out>/rxpack core/tools/rxpack.c */
#define _DARWIN_C_SOURCE 1
#define _DEFAULT_SOURCE 1
#include "fmt_rxar.h"
#include "fmt_vfs.h"

#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* ------------------------------------------------------------------------------------------------ small helpers */

static int die(const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    fprintf(stderr, "rxpack: "); vfprintf(stderr, fmt, ap); fprintf(stderr, "\n");
    va_end(ap);
    return 1;
}
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

typedef struct { char** v; int n, cap; } strs;
static void strs_add(strs* s, const char* x)
{
    if (s->n == s->cap) { s->cap = s->cap ? s->cap * 2 : 256; s->v = realloc(s->v, (size_t)s->cap * sizeof *s->v); }
    s->v[s->n++] = strdup(x);
}
static int strs_cmp(const void* a, const void* b) { return strcmp(*(char* const*)a, *(char* const*)b); }

typedef struct { char* p; size_t n, cap; } sbuf;
static void sb_put(sbuf* b, const char* s, size_t n)
{
    if (b->n + n + 1 > b->cap) { b->cap = (b->n + n + 1) * 2; b->p = realloc(b->p, b->cap); }
    memcpy(b->p + b->n, s, n); b->n += n; b->p[b->n] = 0;
}
static void sb_str(sbuf* b, const char* s) { sb_put(b, s, strlen(s)); }
static void sb_fmt(sbuf* b, const char* fmt, ...)
{
    char tmp[4096]; va_list ap; va_start(ap, fmt); int n = vsnprintf(tmp, sizeof tmp, fmt, ap); va_end(ap);
    if (n > 0) sb_put(b, tmp, (size_t)n < sizeof tmp ? (size_t)n : sizeof tmp - 1);
}
static void sb_json(sbuf* b, const char* s)   /* a JSON string literal */
{
    sb_str(b, "\"");
    for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
        if (*p == '"' || *p == '\\') { char e[2] = { '\\', (char)*p }; sb_put(b, e, 2); }
        else if (*p < 0x20) sb_fmt(b, "\\u%04x", *p);
        else sb_put(b, (const char*)p, 1);
    }
    sb_str(b, "\"");
}

static int is_dir(const char* p) { struct stat st; return stat(p, &st) == 0 && S_ISDIR(st.st_mode); }
static int is_file(const char* p) { struct stat st; return stat(p, &st) == 0 && S_ISREG(st.st_mode); }

static int read_all(const char* path, uint8_t** out, size_t* n)
{
    *out = NULL; *n = 0;
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    if (fseeko(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    off_t len = ftello(f);
    if (len < 0 || fseeko(f, 0, SEEK_SET) != 0) { fclose(f); return -1; }
    uint8_t* d = malloc(len ? (size_t)len : 1);
    if (!d || (len && fread(d, 1, (size_t)len, f) != (size_t)len)) { free(d); fclose(f); return -1; }
    fclose(f);
    *out = d; *n = (size_t)len;
    return 0;
}

/* every regular file below base/rel (recursive), as rel paths with '/' */
static void walk(const char* base, const char* rel, strs* out)
{
    char dir[4096];
    snprintf(dir, sizeof dir, "%s%s%s", base, *rel ? "/" : "", rel);
    DIR* d = opendir(dir);
    if (!d) return;
    struct dirent* de;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;                     /* ., .., .DS_Store and other dot files */
        char sub[4096], full[8192];
        snprintf(sub, sizeof sub, "%s%s%s", rel, *rel ? "/" : "", de->d_name);
        snprintf(full, sizeof full, "%s/%s", base, sub);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) walk(base, sub, out);
        else if (S_ISREG(st.st_mode)) strs_add(out, sub);
    }
    closedir(d);
}

/* a file directly in dir, by name, any case: its real name (static buffer) or NULL */
static const char* find_ci(const char* dir, const char* name)
{
    static char found[1024];
    DIR* d = opendir(dir);
    if (!d) return NULL;
    struct dirent* de; const char* r = NULL;
    while ((de = readdir(d))) if (!strcasecmp(de->d_name, name)) { snprintf(found, sizeof found, "%s", de->d_name); r = found; break; }
    closedir(d);
    return r;
}

/* ------------------------------------------------------------------------------------------------ parallel packing */

typedef struct {
    fmt_rxar_writer* w;
    const char* base;                 /* the real folder the source paths are below */
    char** src; char** dst;           /* source path (below base), archive path */
    int n;
    int next, failed;
    pthread_mutex_t lock;
    double t0;
    uint64_t done_raw;
    int done;
} job;

static void* worker(void* arg)
{
    job* j = (job*)arg;
    for (;;) {
        pthread_mutex_lock(&j->lock);
        int i = j->failed ? j->n : j->next++;
        pthread_mutex_unlock(&j->lock);
        if (i >= j->n) return NULL;
        char full[8192];
        snprintf(full, sizeof full, "%s/%s", j->base, j->src[i]);
        uint8_t* d; size_t n;
        if (read_all(full, &d, &n) < 0) { fprintf(stderr, "rxpack: cannot read %s\n", full); pthread_mutex_lock(&j->lock); j->failed = 1; pthread_mutex_unlock(&j->lock); return NULL; }
        int rc = fmt_rxar_add(j->w, j->dst[i], d, n, 0);
        free(d);
        pthread_mutex_lock(&j->lock);
        if (rc < 0) { fprintf(stderr, "rxpack: %s: %s\n", j->dst[i], fmt_error()); j->failed = 1; }
        j->done++; j->done_raw += n;
        if (j->done % 2000 == 0 || j->done == j->n) {
            uint64_t raw = 0, st = fmt_rxar_written(j->w, &raw);
            double t = now() - j->t0;
            fprintf(stderr, "\r  %d / %d files, %.1f MB -> %.1f MB (%.3f), %.0f MB/s   ", j->done, j->n, raw / 1048576.0, st / 1048576.0,
                    raw ? (double)st / (double)raw : 0.0, t > 0 ? raw / 1048576.0 / t : 0.0);
        }
        pthread_mutex_unlock(&j->lock);
    }
}

static int run_jobs(fmt_rxar_writer* w, const char* base, strs* src, strs* dst, int jobs)
{
    job j; memset(&j, 0, sizeof j);
    j.w = w; j.base = base; j.src = src->v; j.dst = dst->v; j.n = src->n; j.t0 = now();
    pthread_mutex_init(&j.lock, NULL);
    if (jobs < 1) jobs = 1;
    if (jobs > 32) jobs = 32;
    pthread_t t[32];
    for (int i = 0; i < jobs; i++) pthread_create(&t[i], NULL, worker, &j);
    for (int i = 0; i < jobs; i++) pthread_join(t[i], NULL);
    if (j.n) fprintf(stderr, "\n");
    pthread_mutex_destroy(&j.lock);
    return j.failed ? -1 : 0;
}

/* ------------------------------------------------------------------------------------------------ options */

typedef struct { int level, jobs, skip_levels; const char* name; const char* source; const char* stem; const char* only; strs dirs; } opts;

static int parse_opts(int argc, char** argv, int first, opts* o, strs* pos)
{
    memset(o, 0, sizeof *o);
    o->level = 9; o->jobs = 4;
    for (int i = first; i < argc; i++) {
        const char* a = argv[i];
        const char* v = i + 1 < argc ? argv[i + 1] : NULL;
        if (!strcmp(a, "--level") && v) { o->level = atoi(v); i++; }
        else if (!strcmp(a, "--jobs") && v) { o->jobs = atoi(v); i++; }
        else if (!strcmp(a, "--name") && v) { o->name = v; i++; }
        else if (!strcmp(a, "--source") && v) { o->source = v; i++; }
        else if (!strcmp(a, "--stem") && v) { o->stem = v; i++; }
        else if (!strcmp(a, "--only") && v) { o->only = v; i++; }
        else if (!strcmp(a, "--dir") && v) { strs_add(&o->dirs, v); i++; }
        else if (!strcmp(a, "--skip-levels")) o->skip_levels = 1;
        else if (a[0] == '-' && a[1] == '-') return die("unknown option %s", a), -1;
        else strs_add(pos, a);
    }
    return 0;
}

static const char* utc_now(void)
{
    static char b[32]; time_t t = time(NULL); struct tm g; gmtime_r(&t, &g);
    strftime(b, sizeof b, "%Y-%m-%dT%H:%M:%SZ", &g);
    return b;
}

/* ------------------------------------------------------------------------------------------------ data */

static int cmd_data(int argc, char** argv)
{
    opts o; strs pos = { 0 };
    if (parse_opts(argc, argv, 2, &o, &pos) < 0 || pos.n != 2) return die("usage: rxpack data <game root> <out.rxdata> [--dir Assets]... [--level N] [--jobs N]");
    const char* root = pos.v[0]; const char* out = pos.v[1];
    if (!o.dirs.n) strs_add(&o.dirs, "Assets");
    strs src = { 0 };
    for (int d = 0; d < o.dirs.n; d++) {
        char base[4096]; snprintf(base, sizeof base, "%s/%s", root, o.dirs.v[d]);
        if (!is_dir(base)) return die("%s is not a folder", base);
        strs rel = { 0 };
        walk(base, "", &rel);
        for (int i = 0; i < rel.n; i++) { char p[8192]; snprintf(p, sizeof p, "%s/%s", o.dirs.v[d], rel.v[i]); strs_add(&src, p); free(rel.v[i]); }
        free(rel.v);
    }
    if (o.skip_levels) {                                         /* Assets/<type>/<level>/... of the root's levels: left to their track archives */
        strs levels = { 0 };
        char db[4096]; snprintf(db, sizeof db, "%s/Database", root);
        DIR* dd = opendir(db);
        struct dirent* de;
        while (dd && (de = readdir(dd))) { size_t n = strlen(de->d_name); if (n > 10 && !strcasecmp(de->d_name + n - 10, ".dx9.level")) { char l[512]; snprintf(l, sizeof l, "%.*s", (int)(n - 10), de->d_name); strs_add(&levels, l); } }
        if (dd) closedir(dd);
        int kept = 0, skipped = 0;
        for (int i = 0; i < src.n; i++) {
            const char* p = src.v[i];
            int skip = 0;
            const char* a = strchr(p, '/'); const char* b = a ? strchr(a + 1, '/') : NULL; const char* c = b ? strchr(b + 1, '/') : NULL;
            if (a && b && c && !strncmp(p, "Assets/", 7))
                for (int k = 0; k < levels.n && !skip; k++) if (strlen(levels.v[k]) == (size_t)(c - b - 1) && !strncasecmp(b + 1, levels.v[k], (size_t)(c - b - 1))) skip = 1;
            if (skip) { free(src.v[i]); skipped++; } else src.v[kept++] = src.v[i];
        }
        src.n = kept;
        fprintf(stderr, "rxpack data: %d files of %d levels left to their track archives\n", skipped, levels.n);
    }
    qsort(src.v, (size_t)src.n, sizeof *src.v, strs_cmp);
    fprintf(stderr, "rxpack data: %d files below %s, zstd level %d, %d jobs\n", src.n, root, o.level, o.jobs);
    fmt_rxar_writer* w = fmt_rxar_create(out, FMT_RXAR_DATA, o.level);
    if (!w) return die("%s", fmt_error());
    sbuf meta = { 0 };
    sb_str(&meta, "{\"format\":\"reflex-data-archive\",\"version\":1,\"created\":"); sb_json(&meta, utc_now());
    sb_str(&meta, ",\"dirs\":[");
    for (int d = 0; d < o.dirs.n; d++) { if (d) sb_str(&meta, ","); sb_json(&meta, o.dirs.v[d]); }
    sb_fmt(&meta, "],\"files\":%d}", src.n);
    fmt_rxar_set_meta(w, meta.p, meta.n);
    double t0 = now();
    if (run_jobs(w, root, &src, &src, o.jobs) < 0) { fmt_rxar_abort(w); return die("packing failed"); }
    uint64_t raw = 0, st = fmt_rxar_written(w, &raw);
    if (fmt_rxar_finish(w) < 0) return die("%s", fmt_error());
    printf("DATA %s: %d files, %.1f MB raw -> %.1f MB (%.3f) in %.1f s\n", out, src.n, raw / 1048576.0, st / 1048576.0, raw ? (double)st / raw : 0.0, now() - t0);
    return 0;
}

/* ------------------------------------------------------------------------------------------------ tracks */

/* the {...} of alive-levels.json's levels[] entry whose "level" is `level` (a small scanner: the registry is our own writer's JSON) */
static char* alive_entry(const char* json, const char* level)
{
    char key[512]; snprintf(key, sizeof key, "\"level\": \"%s\"", level);
    const char* hit = strstr(json, key);
    if (!hit) { snprintf(key, sizeof key, "\"level\":\"%s\"", level); hit = strstr(json, key); }
    if (!hit) return NULL;
    const char* s = hit;                                        /* back to the entry's '{' (no '{' inside the registry's strings) */
    while (s > json && *s != '{') s--;
    if (*s != '{') return NULL;
    int depth = 0, str = 0;
    for (const char* p = s; *p; p++) {
        if (str) { if (*p == '\\' && p[1]) p++; else if (*p == '"') str = 0; continue; }
        if (*p == '"') str = 1;
        else if (*p == '{' || *p == '[') depth++;
        else if (*p == '}' || *p == ']') { if (--depth == 0) { size_t n = (size_t)(p - s) + 1; char* r = malloc(n + 1); memcpy(r, s, n); r[n] = 0; return r; } }
    }
    return NULL;
}

/* the track's metadata files: name, bytes, SHA-256 of the four level files (Online/Core/OnlineTrackMod.cs identity) */
static void meta_files(sbuf* meta, const char* base, strs* src, strs* dst)
{
    sb_str(meta, ",\"files\":[");
    int first = 1;
    for (int i = 0; i < src->n; i++) {
        const char* p = dst->v[i];
        if (strncmp(p, "Database/", 9) || strchr(p + 9, '/') || !strstr(p, ".dx9.")) continue;
        char full[8192]; snprintf(full, sizeof full, "%s/%s", base, src->v[i]);
        uint8_t* d; size_t n;
        if (read_all(full, &d, &n) < 0) continue;
        fmt_sha256 s; char hex[65]; fmt_sha256_init(&s); fmt_sha256_add(&s, d, n); fmt_sha256_hex(&s, hex); free(d);
        if (!first) sb_str(meta, ",");
        first = 0;
        sb_str(meta, "{\"name\":"); sb_json(meta, p + 9); sb_fmt(meta, ",\"bytes\":%zu,\"sha256\":\"%s\"}", n, hex);
    }
    sb_str(meta, "]");
}

static int pack_track(const char* root, const char* level, const char* out, const opts* o, char* alive_json)
{
    char db[4096]; snprintf(db, sizeof db, "%s/Database", root);
    strs src = { 0 }, dst = { 0 };
    static const char* const ext[] = { ".dx9.database", ".dx9.level", ".dx9.scene", ".dx9.package" };
    int have[4] = { 0 };
    char want[512];
    for (int k = 0; k < 4; k++) {
        snprintf(want, sizeof want, "%s%s", level, ext[k]);
        const char* real = find_ci(db, want);
        if (!real) continue;
        char s[1024]; snprintf(s, sizeof s, "Database/%s", real);
        strs_add(&src, s); strs_add(&dst, s); have[k] = 1;
    }
    if (!have[0] || !have[1] || !have[2]) return die("%s: Database/%s.dx9.database / .level / .scene missing", root, level);
    /* the extracted assets of the level's database (an imported library) */
    int assets = 0;
    char abase[4096]; snprintf(abase, sizeof abase, "%s/Assets", root);
    DIR* ad = opendir(abase);
    if (ad) {
        struct dirent* te;
        while ((te = readdir(ad))) {
            if (te->d_name[0] == '.') continue;
            char tdir[8192]; snprintf(tdir, sizeof tdir, "%s/%s", abase, te->d_name);
            if (!is_dir(tdir)) continue;
            const char* ldir = find_ci(tdir, level);
            if (!ldir) continue;
            char lrel[1024]; snprintf(lrel, sizeof lrel, "Assets/%s/%s", te->d_name, ldir);
            char lfull[8192]; snprintf(lfull, sizeof lfull, "%s/%s", root, lrel);
            strs rel = { 0 };
            walk(lfull, "", &rel);
            for (int i = 0; i < rel.n; i++) { char p[8192]; snprintf(p, sizeof p, "%s/%s", lrel, rel.v[i]); strs_add(&src, p); strs_add(&dst, p); free(rel.v[i]); assets++; }
            free(rel.v);
        }
        closedir(ad);
    }
    if (!have[3] && !assets) {                                  /* an extracted library's level with no assets (a test scene: Ctrl_Lighting) */
        char marker[4200]; snprintf(marker, sizeof marker, "%s/reflex-extracted-v1.txt", root);
        if (!is_file(marker)) return die("%s: %s has neither a package (Database/%s.dx9.package) nor extracted assets", root, level, level);
        fprintf(stderr, "rxpack: %s has no extracted assets (its database lists none)\n", level);
    }
    /* an Alive level's sidecars */
    DIR* dd = opendir(db);
    char pre[600], imp[600];
    snprintf(pre, sizeof pre, "alive-%s-", level); snprintf(imp, sizeof imp, "alive-import-%s.json", level);
    int sidecars = 0;
    if (dd) {
        struct dirent* de;
        while ((de = readdir(dd))) {
            if (strncasecmp(de->d_name, pre, strlen(pre)) && strcasecmp(de->d_name, imp)) continue;
            char s[1024]; snprintf(s, sizeof s, "Database/%s", de->d_name);
            strs_add(&src, s); strs_add(&dst, s); sidecars++;
        }
        closedir(dd);
    }
    char* entry = alive_json ? alive_entry(alive_json, level) : NULL;
    const char* source = o->source ? o->source : entry ? "alive" : "base";
    fmt_rxar_writer* w = fmt_rxar_create(out, FMT_RXAR_TRAK, o->level);
    if (!w) { free(entry); return die("%s", fmt_error()); }
    sbuf meta = { 0 };
    sb_str(&meta, "{\"format\":\"reflex-track-archive\",\"version\":1,\"level\":"); sb_json(&meta, level);
    sb_str(&meta, ",\"name\":"); sb_json(&meta, o->name ? o->name : level);
    sb_str(&meta, ",\"source\":"); sb_json(&meta, source);
    sb_str(&meta, ",\"layout\":"); sb_json(&meta, have[3] ? (assets ? "packed+extracted" : "packed") : "extracted");
    sb_str(&meta, ",\"created\":"); sb_json(&meta, utc_now());
    meta_files(&meta, root, &src, &dst);
    if (entry) { sb_str(&meta, ",\"alive\":"); sb_str(&meta, entry); }
    sb_str(&meta, "}");
    free(entry);
    fmt_rxar_set_meta(w, meta.p, meta.n);
    free(meta.p);
    double t0 = now();
    if (run_jobs(w, root, &src, &dst, o->jobs) < 0) { fmt_rxar_abort(w); return die("packing %s failed", level); }
    uint64_t raw = 0, st = fmt_rxar_written(w, &raw);
    if (fmt_rxar_finish(w) < 0) return die("%s", fmt_error());
    printf("TRACK %s (%s): %d files (%d assets, %d sidecars), %.1f MB raw -> %.1f MB (%.3f) in %.1f s\n", level, source, src.n, assets, sidecars,
           raw / 1048576.0, st / 1048576.0, raw ? (double)st / raw : 0.0, now() - t0);
    for (int i = 0; i < src.n; i++) { free(src.v[i]); free(dst.v[i]); }
    free(src.v); free(dst.v);
    return 0;
}

static char* load_alive_registry(const char* root)
{
    char p[4096]; snprintf(p, sizeof p, "%s/Database/alive-levels.json", root);
    uint8_t* d; size_t n;
    if (read_all(p, &d, &n) < 0) return NULL;
    char* s = realloc(d, n + 1); s[n] = 0;
    return s;
}

static int cmd_track(int argc, char** argv)
{
    opts o; strs pos = { 0 };
    if (parse_opts(argc, argv, 2, &o, &pos) < 0 || pos.n != 3) return die("usage: rxpack track <game root> <level> <out.rxtrack> [--level N] [--jobs N] [--name S] [--source base|alive|mod]");
    char* reg = load_alive_registry(pos.v[0]);
    int rc = pack_track(pos.v[0], pos.v[1], pos.v[2], &o, reg);
    free(reg);
    return rc;
}

static int cmd_tracks(int argc, char** argv)
{
    opts o; strs pos = { 0 };
    if (parse_opts(argc, argv, 2, &o, &pos) < 0 || pos.n != 2) return die("usage: rxpack tracks <game root> <out dir> [--only L1,L2] [--level N] [--jobs N]");
    const char* root = pos.v[0]; const char* outdir = pos.v[1];
    char db[4096]; snprintf(db, sizeof db, "%s/Database", root);
    DIR* d = opendir(db);
    if (!d) return die("%s has no Database folder", root);
    strs levels = { 0 };
    struct dirent* de;
    while ((de = readdir(d))) {
        size_t n = strlen(de->d_name);
        if (n > 10 && !strcasecmp(de->d_name + n - 10, ".dx9.level")) { char l[512]; snprintf(l, sizeof l, "%.*s", (int)(n - 10), de->d_name); strs_add(&levels, l); }
    }
    closedir(d);
    qsort(levels.v, (size_t)levels.n, sizeof *levels.v, strs_cmp);
    mkdir(outdir, 0755);
    char* reg = load_alive_registry(root);
    int fails = 0, made = 0;
    for (int i = 0; i < levels.n; i++) {
        if (o.only) {
            char key[600]; snprintf(key, sizeof key, ",%s,", levels.v[i]);
            char list[8192]; snprintf(list, sizeof list, ",%s,", o.only);
            if (!strstr(list, key)) continue;
        }
        char out[8192]; snprintf(out, sizeof out, "%s/%s.rxtrack", outdir, levels.v[i]);
        if (pack_track(root, levels.v[i], out, &o, reg) != 0) fails++; else made++;
    }
    free(reg);
    printf("TRACKS %d written, %d failed\n", made, fails);
    return fails ? 1 : 0;
}

/* an old Reflex track mod: the folder's <stem>.dx9.* set + its dress */
static int cmd_mod(int argc, char** argv)
{
    opts o; strs pos = { 0 };
    if (parse_opts(argc, argv, 2, &o, &pos) < 0 || pos.n != 2) return die("usage: rxpack mod <mod folder> <out.rxtrack> [--stem S] [--name S] [--level N]");
    const char* folder = pos.v[0]; const char* out = pos.v[1];
    DIR* d = opendir(folder);
    if (!d) return die("cannot open %s", folder);
    strs names = { 0 };
    struct dirent* de;
    while ((de = readdir(d))) if (de->d_name[0] != '.') { char f[8192]; snprintf(f, sizeof f, "%s/%s", folder, de->d_name); if (is_file(f)) strs_add(&names, de->d_name); }
    closedir(d);
    qsort(names.v, (size_t)names.n, sizeof *names.v, strs_cmp);
    char stem[512] = "";
    if (o.stem) snprintf(stem, sizeof stem, "%s", o.stem);
    else for (int i = 0; i < names.n; i++) {
        size_t n = strlen(names.v[i]);
        if (n > 10 && !strcasecmp(names.v[i] + n - 10, ".dx9.level")) {
            if (*stem) return die("%s holds several tracks: name one with --stem", folder);
            snprintf(stem, sizeof stem, "%.*s", (int)(n - 10), names.v[i]);
        }
    }
    if (!*stem) return die("%s has no <stem>.dx9.level", folder);
    static const char* const ext[] = { ".dx9.database", ".dx9.level", ".dx9.package", ".dx9.scene" };
    strs src = { 0 }, dst = { 0 };
    for (int k = 0; k < 4; k++) {
        char want[600]; snprintf(want, sizeof want, "%s%s", stem, ext[k]);
        const char* real = find_ci(folder, want);
        if (!real) return die("%s: %s is missing (a track is its .dx9.database, .level, .package and .scene)", folder, want);
        char dpath[1024]; snprintf(dpath, sizeof dpath, "Database/%s%s", stem, ext[k]);   /* the stem as the set names it, the extension lower case */
        strs_add(&src, real); strs_add(&dst, dpath);
    }
    for (int i = 0; i < names.n; i++) {
        const char* n = names.v[i];
        const char* dot = strrchr(n, '.');
        int dress = !strcasecmp(n, "mod.ini") || !strcasecmp(n, "mod.json") || (dot && (!strcasecmp(dot, ".png") || !strcasecmp(dot, ".jpg") ||
                    !strcasecmp(dot, ".jpeg") || !strcasecmp(dot, ".txt")));
        if (!dress) continue;
        char dpath[1024]; snprintf(dpath, sizeof dpath, "Mod/%s", n);
        if (!fmt_rxar_path_ok(dpath)) continue;
        strs_add(&src, n); strs_add(&dst, dpath);
    }
    fmt_rxar_writer* w = fmt_rxar_create(out, FMT_RXAR_TRAK, o.level);
    if (!w) return die("%s", fmt_error());
    const char* base = strrchr(folder, '/');
    sbuf meta = { 0 };
    sb_str(&meta, "{\"format\":\"reflex-track-archive\",\"version\":1,\"level\":"); sb_json(&meta, stem);
    sb_str(&meta, ",\"name\":"); sb_json(&meta, o.name ? o.name : base ? base + 1 : folder);
    sb_str(&meta, ",\"source\":\"mod\",\"layout\":\"packed\",\"created\":"); sb_json(&meta, utc_now());
    meta_files(&meta, folder, &src, &dst);
    sb_str(&meta, "}");
    fmt_rxar_set_meta(w, meta.p, meta.n);
    free(meta.p);
    double t0 = now();
    if (run_jobs(w, folder, &src, &dst, o.jobs) < 0) { fmt_rxar_abort(w); return die("packing failed"); }
    uint64_t raw = 0, st = fmt_rxar_written(w, &raw);
    if (fmt_rxar_finish(w) < 0) return die("%s", fmt_error());
    printf("MOD %s -> %s: %d files, %.1f MB raw -> %.1f MB (%.3f) in %.1f s\n", stem, out, src.n, raw / 1048576.0, st / 1048576.0, raw ? (double)st / raw : 0.0, now() - t0);
    return 0;
}

/* ------------------------------------------------------------------------------------------------ list / verify / extract */

static int cmd_list(int argc, char** argv)
{
    if (argc != 3) return die("usage: rxpack list <archive>");
    fmt_rxar* a = fmt_rxar_open(argv[2]);
    if (!a) return die("%s", fmt_error());
    size_t ml; const char* m = fmt_rxar_meta(a, &ml);
    uint32_t k = fmt_rxar_kind(a);
    printf("%s: %.4s, %d files\nmeta %s\n", argv[2], (const char*)&k, fmt_rxar_count(a), m);
    uint64_t raw = 0, st = 0;
    for (int i = 0; i < fmt_rxar_count(a); i++) {
        fmt_rxar_info in; fmt_rxar_info_of(a, i, &in);
        raw += in.raw; st += in.stored;
        printf("%-5s %2u %12llu %12llu  %s\n", in.codec ? "zstd" : "store", in.chunk_log2, (unsigned long long)in.raw, (unsigned long long)in.stored, fmt_rxar_path(a, i));
    }
    printf("total %llu raw, %llu stored (%.3f)\n", (unsigned long long)raw, (unsigned long long)st, raw ? (double)st / raw : 0.0);
    fmt_rxar_close(a);
    return 0;
}

typedef struct { fmt_rxar* a; const char* root; fmt_vfs* v; int next, bad, n, refs; pthread_mutex_t lock; uint64_t bytes; } vjob;

static void* vworker(void* arg)
{
    vjob* j = (vjob*)arg;
    for (;;) {
        pthread_mutex_lock(&j->lock); int i = j->next++; pthread_mutex_unlock(&j->lock);
        if (i >= j->n) return NULL;
        fmt_buf b;
        int bad = 0;
        if (fmt_rxar_external(j->a, i)) {                       /* a pool reference: its Pool/ file in the root, the same SHA-256 */
            uint8_t sha[32], got[32]; char pool[71];
            fmt_rxar_sha(j->a, i, sha); fmt_rxar_pool_path(sha, pool);
            pthread_mutex_lock(&j->lock); j->refs++; pthread_mutex_unlock(&j->lock);
            if (!j->v) continue;
            if (fmt_vfs_read_file(j->v, pool, &b) < 0) { fprintf(stderr, "POOL MISSING %s (%s)\n", fmt_rxar_path(j->a, i), pool); bad = 1; }
            else {
                fmt_sha256 s; fmt_sha256_init(&s); fmt_sha256_add(&s, b.data, b.size); fmt_sha256_bin(&s, got);
                if (b.size != fmt_rxar_size(j->a, i) || memcmp(got, sha, 32)) { fprintf(stderr, "POOL DIFFERENT %s\n", fmt_rxar_path(j->a, i)); bad = 1; }
                fmt_buf_free(&b);
            }
            if (bad) { pthread_mutex_lock(&j->lock); j->bad++; pthread_mutex_unlock(&j->lock); }
            continue;
        }
        if (fmt_rxar_read(j->a, i, &b) < 0) { fprintf(stderr, "BAD %s: %s\n", fmt_rxar_path(j->a, i), fmt_error()); bad = 1; }
        else {
            uint8_t sha[32];
            if (fmt_rxar_sha(j->a, i, sha)) {                    /* version 2: the stored SHA-256 too */
                uint8_t got[32]; fmt_sha256 s; fmt_sha256_init(&s); fmt_sha256_add(&s, b.data, b.size); fmt_sha256_bin(&s, got);
                if (memcmp(got, sha, 32)) { fprintf(stderr, "SHA %s\n", fmt_rxar_path(j->a, i)); bad = 1; }
            }
            if (j->root) {                                       /* the loose file of the same path, when the root has one */
                char full[8192]; snprintf(full, sizeof full, "%s/%s", j->root, fmt_rxar_path(j->a, i));
                uint8_t* d; size_t n;
                if (is_file(full) && read_all(full, &d, &n) == 0) { if (n != b.size || memcmp(d, b.data, n)) { fprintf(stderr, "DIFFERENT %s\n", fmt_rxar_path(j->a, i)); bad = 1; } free(d); }
            }
            /* a ranged read that crosses chunks, against the whole */
            if (!bad && b.size > 3) {
                size_t off = b.size / 3, n = b.size - off - b.size / 7;
                uint8_t* r = malloc(n ? n : 1);
                if (fmt_rxar_read_at(j->a, i, off, r, n) < 0 || memcmp(r, b.data + off, n)) { fprintf(stderr, "RANGE %s\n", fmt_rxar_path(j->a, i)); bad = 1; }
                free(r);
            }
            pthread_mutex_lock(&j->lock); j->bytes += b.size; pthread_mutex_unlock(&j->lock);
            fmt_buf_free(&b);
        }
        if (bad) { pthread_mutex_lock(&j->lock); j->bad++; pthread_mutex_unlock(&j->lock); }
    }
}

static int cmd_verify(int argc, char** argv)
{
    if (argc != 3 && argc != 4) return die("usage: rxpack verify <archive> [<game root>]");
    double t0 = now();
    fmt_rxar* a = fmt_rxar_open(argv[2]);
    if (!a) return die("%s", fmt_error());
    double topen = now() - t0;
    vjob j; memset(&j, 0, sizeof j);
    j.a = a; j.root = argc == 4 ? argv[3] : NULL; j.n = fmt_rxar_count(a);
    if (j.root && !(j.v = fmt_vfs_open(j.root))) { fprintf(stderr, "rxpack: %s: %s (pool references not checked)\n", j.root, fmt_error()); fmt_clear_error(); }
    pthread_mutex_init(&j.lock, NULL);
    pthread_t t[8];
    for (int i = 0; i < 8; i++) pthread_create(&t[i], NULL, vworker, &j);
    for (int i = 0; i < 8; i++) pthread_join(t[i], NULL);
    printf("VERIFY %s: %d files (%d pool references%s), %.1f MB, %d bad%s (open %.1f ms, %.1f s)\n", argv[2], j.n, j.refs, j.refs && !j.v ? " not checked: give the root" : "", j.bytes / 1048576.0, j.bad,
           j.root ? " (compared with the loose files there are)" : "", topen * 1000.0, now() - t0);
    fmt_vfs_close(j.v);
    fmt_rxar_close(a);
    return j.bad ? 1 : 0;
}

static int mkdirs(char* path)   /* the folders of a file path */
{
    for (char* p = path + 1; *p; p++) if (*p == '/') { *p = 0; if (mkdir(path, 0755) != 0 && errno != EEXIST) { *p = '/'; return -1; } *p = '/'; }
    return 0;
}

static int cmd_extract(int argc, char** argv)
{
    if (argc != 4) return die("usage: rxpack extract <archive> <out dir>");
    fmt_rxar* a = fmt_rxar_open(argv[2]);
    if (!a) return die("%s", fmt_error());
    for (int i = 0; i < fmt_rxar_count(a); i++) {
        fmt_buf b;
        if (fmt_rxar_read(a, i, &b) < 0) { fmt_rxar_close(a); return die("%s", fmt_error()); }
        char out[8192]; snprintf(out, sizeof out, "%s/%s", argv[3], fmt_rxar_path(a, i));
        if (mkdirs(out) < 0) { fmt_buf_free(&b); fmt_rxar_close(a); return die("cannot create folders for %s", out); }
        FILE* f = fopen(out, "wb");
        if (!f || fwrite(b.data, 1, b.size, f) != b.size) { if (f) fclose(f); fmt_buf_free(&b); fmt_rxar_close(a); return die("cannot write %s", out); }
        fclose(f);
        fmt_buf_free(&b);
    }
    printf("EXTRACT %s: %d files to %s\n", argv[2], fmt_rxar_count(a), argv[3]);
    fmt_rxar_close(a);
    return 0;
}

/* the same reads through the core's file system: loose (REFLEX_NO_ARCHIVES=1) and packed */
static int cmd_bench(int argc, char** argv)
{
    if (argc != 4) return die("usage: rxpack bench <game root> <list file of paths>");
    uint8_t* lst; size_t ln;
    if (read_all(argv[3], &lst, &ln) < 0) return die("cannot read %s", argv[3]);
    char* s = realloc(lst, ln + 1); s[ln] = 0;
    strs paths = { 0 };
    for (char* line = strtok(s, "\n"); line; line = strtok(NULL, "\n")) if (*line) strs_add(&paths, line);
    for (int pass = 0; pass < 2; pass++) {
        setenv("REFLEX_NO_ARCHIVES", pass == 0 ? "1" : "0", 1);
        double t0 = now();
        fmt_vfs* v = fmt_vfs_open(argv[2]);
        if (!v) return die("%s", fmt_error());
        double topen = now() - t0;
        uint64_t bytes = 0; int missing = 0;
        double t1 = now();
        for (int i = 0; i < paths.n; i++) {
            fmt_buf b;
            if (fmt_vfs_read_file(v, paths.v[i], &b) < 0) { missing++; continue; }
            bytes += b.size; fmt_buf_free(&b);
        }
        double tread = now() - t1;
        printf("BENCH %s: open %.1f ms (%d files, %d archives), read %d files %.1f MB in %.3f s (%.0f MB/s), %d missing\n", pass == 0 ? "loose " : "packed",
               topen * 1000.0, fmt_vfs_file_count(v), fmt_vfs_archive_count(v), paths.n - missing, bytes / 1048576.0, tread, tread > 0 ? bytes / 1048576.0 / tread : 0.0, missing);
        fmt_vfs_close(v);
    }
    return 0;
}


/* ------------------------------------------------------------------------------------------------ dupes */

typedef struct { int arc, idx; uint64_t raw, stored, offset; uint8_t sha[32]; } dent;
typedef struct { fmt_rxar** arcs; dent* e; int n, next, bad; pthread_mutex_t lock; } djob;

static void hex2bin(const char* h, uint8_t* o) { for (int i = 0; i < 32; i++) { unsigned v; sscanf(h + 2 * i, "%2x", &v); o[i] = (uint8_t)v; } }
static void* dworker(void* arg)
{
    djob* j = (djob*)arg;
    for (;;) {
        pthread_mutex_lock(&j->lock); int i = j->next++; pthread_mutex_unlock(&j->lock);
        if (i >= j->n) return NULL;
        dent* d = &j->e[i];
        fmt_buf b;
        if (fmt_rxar_read(j->arcs[d->arc], d->idx, &b) < 0) { pthread_mutex_lock(&j->lock); j->bad++; pthread_mutex_unlock(&j->lock); continue; }
        fmt_sha256 s; char hex[65]; fmt_sha256_init(&s); fmt_sha256_add(&s, b.data, b.size); fmt_sha256_hex(&s, hex); hex2bin(hex, d->sha);
        fmt_buf_free(&b);
    }
}
static int dcmp(const void* a, const void* b) { const dent* x = a; const dent* y = b; int c = memcmp(x->sha, y->sha, 32); return c ? c : (x->arc - y->arc ? x->arc - y->arc : x->idx - y->idx); }
static const char* type_of(const char* p) { static char t[64]; const char* a = strchr(p, '/'); if (!a || strncmp(p, "Assets/", 7)) { snprintf(t, sizeof t, "(%.*s)", a ? (int)(a - p) : 20, p); return t; } const char* b = strchr(a + 1, '/'); snprintf(t, sizeof t, "%.*s", b ? (int)(b - a - 1) : 20, a + 1); return t; }

/* how much of a root's packed content is the same bytes more than once (within an archive, between level archives, a level and the shared data) */
static int cmd_dupes(int argc, char** argv)
{
    if (argc != 3) return die("usage: rxpack dupes <game root with Assets.rxdata / Tracks/*.rxtrack>");
    strs files = { 0 };
    char tdir[4096]; snprintf(tdir, sizeof tdir, "%s/Tracks", argv[2]);
    DIR* d = opendir(tdir); struct dirent* de;
    while (d && (de = readdir(d))) if (strstr(de->d_name, ".rxtrack")) { char f[8192]; snprintf(f, sizeof f, "%s/%s", tdir, de->d_name); strs_add(&files, f); }
    if (d) closedir(d);
    d = opendir(argv[2]);
    while (d && (de = readdir(d))) if (strstr(de->d_name, ".rxdata")) { char f[8192]; snprintf(f, sizeof f, "%s/%s", argv[2], de->d_name); strs_add(&files, f); }
    if (d) closedir(d);
    djob j; memset(&j, 0, sizeof j); pthread_mutex_init(&j.lock, NULL);
    j.arcs = calloc((size_t)files.n, sizeof *j.arcs);
    int cap = 0;
    for (int k = 0; k < files.n; k++) {
        if (!(j.arcs[k] = fmt_rxar_open(files.v[k]))) return die("%s", fmt_error());
        for (int i = 0; i < fmt_rxar_count(j.arcs[k]); i++) {
            if (j.n == cap) { cap = cap ? cap * 2 : 65536; j.e = realloc(j.e, (size_t)cap * sizeof *j.e); }
            if (fmt_rxar_external(j.arcs[k], i)) continue;      /* a pool reference: no bytes here */
            fmt_rxar_info in; fmt_rxar_info_of(j.arcs[k], i, &in);
            j.e[j.n++] = (dent){ k, i, in.raw, in.stored, in.offset, { 0 } };
        }
    }
    double t0 = now();
    pthread_t t[8];
    for (int i = 0; i < 8; i++) pthread_create(&t[i], NULL, dworker, &j);
    for (int i = 0; i < 8; i++) pthread_join(t[i], NULL);
    qsort(j.e, (size_t)j.n, sizeof *j.e, dcmp);
    uint64_t raw = 0, stored = 0, draw = 0, dstored = 0, same_raw = 0, lvl_raw = 0, shared_raw = 0, same_st = 0, lvl_st = 0, shared_st = 0;
    int groups = 0, dfiles = 0;
    typedef struct { char t[64]; uint64_t raw, st; int n; } tstat; tstat ts[128]; int nts = 0;
    for (int a = 0; a < j.n;) {
        int b = a + 1;
        while (b < j.n && !memcmp(j.e[a].sha, j.e[b].sha, 32)) b++;
        for (int i = a; i < b; i++) { raw += j.e[i].raw; stored += j.e[i].stored; }
        if (b - a > 1) {
            groups++;
            int data_first = 0;   /* is one copy in the shared data (.rxdata)? */
            for (int i = a; i < b; i++) if (fmt_rxar_kind(j.arcs[j.e[i].arc]) == FMT_RXAR_DATA) data_first = 1;
            for (int i = a + 1; i < b; i++) {
                const dent* e = &j.e[i];
                int alias = 0; for (int k = a; k < i && !alias; k++) alias = j.e[k].arc == e->arc && j.e[k].offset == e->offset;
                if (alias) { raw -= e->raw; stored -= e->stored; continue; }   /* stored once already (version 2 dedupe): not a copy */
                dfiles++; draw += e->raw; dstored += e->stored;
                int same = 0; for (int k = a; k < i; k++) if (j.e[k].arc == e->arc) same = 1;
                if (same) { same_raw += e->raw; same_st += e->stored; }
                else if (data_first) { shared_raw += e->raw; shared_st += e->stored; }
                else { lvl_raw += e->raw; lvl_st += e->stored; }
                const char* ty = type_of(fmt_rxar_path(j.arcs[e->arc], e->idx));
                int k = 0; while (k < nts && strcmp(ts[k].t, ty)) k++;
                if (k == nts && nts < 128) { snprintf(ts[nts].t, 64, "%s", ty); ts[nts].raw = ts[nts].st = 0; ts[nts].n = 0; nts++; }
                if (k < 128) { ts[k].raw += e->raw; ts[k].st += e->stored; ts[k].n++; }
            }
        }
        a = b;
    }
    printf("DUPES %s: %d archives, %d files, %.1f MB raw / %.1f MB stored (hashed in %.1f s, %d unreadable)\n", argv[2], files.n, j.n, raw / 1048576.0, stored / 1048576.0, now() - t0, j.bad);
    printf("  duplicate copies: %d files in %d groups: %.1f MB raw, %.1f MB stored (%.1f %% of the stored bytes)\n", dfiles, groups, draw / 1048576.0, dstored / 1048576.0, stored ? 100.0 * dstored / stored : 0.0);
    printf("    within one archive:            %.1f MB raw, %.1f MB stored\n", same_raw / 1048576.0, same_st / 1048576.0);
    printf("    between level archives:        %.1f MB raw, %.1f MB stored\n", lvl_raw / 1048576.0, lvl_st / 1048576.0);
    printf("    a level and the shared data:   %.1f MB raw, %.1f MB stored\n", shared_raw / 1048576.0, shared_st / 1048576.0);
    for (int a = 0; a < nts; a++) for (int b = a + 1; b < nts; b++) if (ts[b].st > ts[a].st) { tstat x = ts[a]; ts[a] = ts[b]; ts[b] = x; }
    for (int k = 0; k < nts && k < 14; k++) printf("    %-12s %6d copies %9.1f MB raw %9.1f MB stored\n", ts[k].t, ts[k].n, ts[k].raw / 1048576.0, ts[k].st / 1048576.0);
    return 0;
}

/* a packed library back to loose files: every archive file (not the pool's own Pool/ entries) written where it belongs, read through the
 * root's file system (loose first, pool references resolved), checked, then the archives moved to <root>/archives-unpacked/ (--delete: removed) */
static int cmd_unpack(int argc, char** argv)
{
    if (argc < 3) return die("usage: rxpack unpack <game root> [--delete]");
    const char* root = argv[2];
    int del = argc > 3 && !strcmp(argv[3], "--delete");
    fmt_vfs* v = fmt_vfs_open(root);
    if (!v) return die("%s", fmt_error());
    int written = 0, kept = 0, bad = 0; uint64_t bytes = 0;
    for (int k = 0; k < fmt_vfs_archive_count(v) && !bad; k++) {
        fmt_rxar* a = fmt_rxar_open(fmt_vfs_archive_file(v, k));
        if (!a) return die("%s", fmt_error());
        for (int i = 0; i < fmt_rxar_count(a); i++) {
            const char* p = fmt_rxar_path(a, i);
            if (!strncmp(p, "Pool/", 5)) continue;
            char out[8192]; snprintf(out, sizeof out, "%s/%s", root, p);
            if (is_file(out)) { kept++; continue; }               /* a loose file is already there (it wins) */
            fmt_buf b;
            if (fmt_vfs_read_file(v, p, &b) < 0) { fprintf(stderr, "rxpack: %s: %s\n", p, fmt_error()); bad++; break; }
            FILE* f = mkdirs(out) == 0 ? fopen(out, "wb") : NULL;
            if (!f || fwrite(b.data, 1, b.size, f) != b.size || fclose(f) != 0) { fprintf(stderr, "rxpack: cannot write %s\n", out); bad++; fmt_buf_free(&b); break; }
            bytes += b.size; written++;
            fmt_buf_free(&b);
        }
        fmt_rxar_close(a);
    }
    char* files[4096]; int nf = 0;
    for (int k = 0; k < fmt_vfs_archive_count(v) && nf < 4096; k++) files[nf++] = strdup(fmt_vfs_archive_file(v, k));
    fmt_vfs_close(v);
    if (bad) return die("unpack stopped: the archives were left in place (the files written so far are loose copies of theirs)");
    char dest[8192]; snprintf(dest, sizeof dest, "%s/archives-unpacked", root);
    if (!del) mkdir(dest, 0755);
    for (int k = 0; k < nf; k++) {
        if (del) unlink(files[k]);
        else { char to[8192]; const char* b = strrchr(files[k], '/'); snprintf(to, sizeof to, "%s/%s", dest, b ? b + 1 : files[k]); rename(files[k], to); }
        free(files[k]);
    }
    printf("UNPACK %s: %d files written (%.1f MB), %d were loose already; %d archives %s\n", root, written, bytes / 1048576.0, kept, nf, del ? "deleted" : "moved to archives-unpacked/");
    return 0;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: rxpack data|track|tracks|mod|list|verify|extract|bench ...  (see core/tools/rxpack.c)\n");
        return 2;
    }
    const char* c = argv[1];
    if (!strcmp(c, "data")) return cmd_data(argc, argv);
    if (!strcmp(c, "track")) return cmd_track(argc, argv);
    if (!strcmp(c, "tracks")) return cmd_tracks(argc, argv);
    if (!strcmp(c, "mod")) return cmd_mod(argc, argv);
    if (!strcmp(c, "list")) return cmd_list(argc, argv);
    if (!strcmp(c, "verify")) return cmd_verify(argc, argv);
    if (!strcmp(c, "extract")) return cmd_extract(argc, argv);
    if (!strcmp(c, "bench")) return cmd_bench(argc, argv);
    if (!strcmp(c, "dupes")) return cmd_dupes(argc, argv);
    if (!strcmp(c, "unpack")) return cmd_unpack(argc, argv);
    return die("unknown command %s", c);
}
