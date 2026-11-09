/* rx_image.c -- see rx_image.h. */
#include "rx_image.h"
#include "rx_mem.h"
#include <stdlib.h>
#include <strings.h>
#include <zlib.h>

typedef struct rx_embedded_span { uint32_t image, addr, size; size_t packed_size; const unsigned char* packed; } rx_embedded_span;
typedef struct rx_embedded_binding { uint32_t slot, value; } rx_embedded_binding;
#include "../data/initial_data.inc"

static int embedded_copy(int crt) {
#ifndef RX_ORACLE
    if (!rx_base) return fmt_fail("rx_image: arena is not initialized");
    for (size_t i = 0; i < sizeof rx_initial_spans / sizeof *rx_initial_spans; ++i) {
        const rx_embedded_span* s = rx_initial_spans + i;
        if ((s->image != 0) != crt) continue;
        uLongf n = s->size;
        if (uncompress(rx_base + s->addr, &n, s->packed, (uLong)s->packed_size) != Z_OK || n != s->size)
            return fmt_fail("rx_image: invalid embedded data at %08x", s->addr);
    }
#else
    (void)crt;
#endif
    return 0;
}

static rx_image_section g_sections[16];
static int g_nsections;

int rx_image_sections(rx_image_section* out, int max) {
    int n = g_nsections < max ? g_nsections : max;
    for (int i = 0; i < n; i++) out[i] = g_sections[i];
    return g_nsections;
}

static int wanted(const char* name) {
    return !strcmp(name, ".rdata") || !strcmp(name, ".data") || !strcmp(name, ".tls");
}
/* (the DLLs have no .rdata: their read-only tables -- the printf state table __lookuptable at 0x78521bf0, the exception action table, the libm
 * constants -- sit in .text, which the core's CRT ports read as data; no code of it runs) */
static int wanted_dll(const char* name) { return !strcmp(name, ".data") || !strcmp(name, ".text"); }

/* maps the wanted sections of a PE image; base = the image base it must have (0: the exe's) */
static int pe_map(const uint8_t* p, size_t size, uint32_t want_base, int (*pick)(const char*), const char* what, rx_image_section* table, int* ntable) {
    if (size < 0x40 || p[0] != 'M' || p[1] != 'Z') return fmt_fail("%s: not a PE image", what);
    uint32_t pe = fmt_u32(p + 0x3c);
    if ((size_t)pe + 0x18 > size || memcmp(p + pe, "PE\0\0", 4) != 0) return fmt_fail("%s: no PE header", what);
    uint32_t nsec = fmt_u16(p + pe + 6), opt = fmt_u16(p + pe + 0x14);
    uint32_t base = fmt_u32(p + pe + 0x18 + 0x1c);   /* optional header: ImageBase */
    if (base != want_base) return fmt_fail("%s: image base %08x, expected %08x", what, base, want_base);
    if (nsec > 16 || (size_t)pe + 0x18 + opt + 0x28u * nsec > size) return fmt_fail("%s: section table out of range", what);
    *ntable = 0;
    for (uint32_t s = 0; s < nsec; s++) {
        const uint8_t* h = p + pe + 0x18 + opt + 0x28u * s;
        rx_image_section* e = &table[(*ntable)++];
        memcpy(e->name, h, 8); e->name[8] = 0;
        uint32_t vsize = fmt_u32(h + 8), va = fmt_u32(h + 12), raw = fmt_u32(h + 16), off = fmt_u32(h + 20);
        e->addr = base + va; e->raw_size = raw; e->virtual_size = vsize; e->loaded = 0;
        if (!pick(e->name)) continue;
        if ((uint64_t)off + raw > size) return fmt_fail("%s: section %s is truncated", what, e->name);
        uint32_t n = raw < vsize ? raw : vsize;   /* the file may pad the raw size beyond the virtual size */
        if (n > vsize) n = vsize;
#ifndef RX_ORACLE
        memcpy(rx_base + e->addr, p + off, n);
#endif
        e->loaded = 1;
    }
    return 0;
}

/* ---- import binding (what the Windows loader does): the import address table slots of the exe (and of msvcp90) that name a function of
 * msvcr90 / msvcp90 get that function's address in the loaded DLL. Slots of other DLLs (kernel32, d3d9, xinput, ...) keep the
 * file's hint / name RVA: the core reaches those services by slot address (rx_os.h, rx_d3d.h), never through the slot's value. ---- */
typedef struct pe_import { uint32_t slot; uint8_t dll; uint8_t by_ordinal; uint16_t ordinal; char name[256]; } pe_import;
enum { DLL_MSVCR90, DLL_MSVCP90, DLL_OTHER };
static pe_import* g_imports; static int g_nimports, g_cap_imports;

/* Session end: the recorded import slots are host memory (the section tables are plain statics: rx_reset.h). */
void rx_image_reset(void) {
    free(g_imports);
    g_imports = NULL;
    g_nimports = g_cap_imports = 0;
    g_nsections = 0;
}

/* file offset of an RVA, or -1 */
static int64_t pe_rva(const uint8_t* p, size_t size, uint32_t rva) {
    uint32_t pe = fmt_u32(p + 0x3c), nsec = fmt_u16(p + pe + 6), opt = fmt_u16(p + pe + 0x14);
    for (uint32_t s = 0; s < nsec; s++) {
        const uint8_t* h = p + pe + 0x18 + opt + 0x28u * s;
        uint32_t vsize = fmt_u32(h + 8), va = fmt_u32(h + 12), raw = fmt_u32(h + 16), off = fmt_u32(h + 20);
        uint32_t span = raw > vsize ? raw : vsize;
        if (rva >= va && rva - va < span) { uint64_t o = (uint64_t)off + (rva - va); return o < size && rva - va < raw ? (int64_t)o : -1; }
    }
    return -1;
}
static int dll_id(const char* name) {
    if (!strcasecmp(name, "MSVCR90.dll")) return DLL_MSVCR90;
    if (!strcasecmp(name, "MSVCP90.dll")) return DLL_MSVCP90;
    return DLL_OTHER;
}
/* the import directory of an image: every slot of msvcr90 / msvcp90 recorded */
static int pe_record_imports(const uint8_t* p, size_t size, const char* what) {
    uint32_t pe = fmt_u32(p + 0x3c), base = fmt_u32(p + pe + 0x18 + 0x1c);
    uint32_t dir = fmt_u32(p + pe + 0x18 + 0x60 + 8);   /* data directory 1: imports */
    if (!dir) return 0;
    for (int64_t d = pe_rva(p, size, dir); d >= 0 && (size_t)d + 20 <= size; d += 20) {
        uint32_t ilt = fmt_u32(p + d), name = fmt_u32(p + d + 12), iat = fmt_u32(p + d + 16);
        if (!name && !iat) break;
        int64_t no = pe_rva(p, size, name);
        if (no < 0) return fmt_fail("%s: import name out of range", what);
        int id = dll_id((const char*)p + no);
        if (id == DLL_OTHER) continue;
        int64_t t = pe_rva(p, size, ilt ? ilt : iat);
        for (uint32_t k = 0; t >= 0 && (size_t)t + 4 * (k + 1) <= size; k++) {
            uint32_t e = fmt_u32(p + t + 4 * k);
            if (!e) break;
            if (g_nimports == g_cap_imports) {
                g_cap_imports = g_cap_imports ? 2 * g_cap_imports : 1024;
                g_imports = (pe_import*)realloc(g_imports, sizeof *g_imports * (size_t)g_cap_imports);
            }
            pe_import* im = &g_imports[g_nimports++];
            memset(im, 0, sizeof *im);
            im->slot = base + iat + 4 * k; im->dll = (uint8_t)id;
            if (e & 0x80000000u) { im->by_ordinal = 1; im->ordinal = (uint16_t)e; continue; }
            int64_t hn = pe_rva(p, size, e);
            if (hn < 0) return fmt_fail("%s: import hint/name out of range", what);
            size_t len = strnlen((const char*)p + hn + 2, size - (size_t)hn - 2);
            if (len >= sizeof im->name) return fmt_fail("%s: import name longer than %zu bytes", what, sizeof im->name - 1);
            memcpy(im->name, p + hn + 2, len);
        }
    }
    return 0;
}
/* the DLL's export address for a name or an ordinal (0: not exported / forwarded) */
static uint32_t pe_export(const uint8_t* p, size_t size, const pe_import* im) {
    uint32_t pe = fmt_u32(p + 0x3c), base = fmt_u32(p + pe + 0x18 + 0x1c);
    uint32_t dir = fmt_u32(p + pe + 0x18 + 0x60), dsize = fmt_u32(p + pe + 0x18 + 0x60 + 4);
    int64_t e = pe_rva(p, size, dir);
    if (e < 0) return 0;
    uint32_t obase = fmt_u32(p + e + 16), nfun = fmt_u32(p + e + 20), nname = fmt_u32(p + e + 24);
    int64_t fa = pe_rva(p, size, fmt_u32(p + e + 28)), na = pe_rva(p, size, fmt_u32(p + e + 32)), oa = pe_rva(p, size, fmt_u32(p + e + 36));
    if (fa < 0 || na < 0 || oa < 0) return 0;
    uint32_t idx = 0xffffffffu;
    if (im->by_ordinal) idx = im->ordinal - obase;
    else for (uint32_t i = 0; i < nname; i++) {
        int64_t s = pe_rva(p, size, fmt_u32(p + na + 4 * i));
        if (s >= 0 && !strcmp((const char*)p + s, im->name)) { idx = fmt_u16(p + oa + 2 * i); break; }
    }
    if (idx >= nfun) return 0;
    uint32_t rva = fmt_u32(p + fa + 4 * idx);
    if (rva >= dir && rva < dir + dsize) return 0;   /* forwarded */
    return base + rva;
}
/* writes the recorded slots of DLL `id` from its image; -1 if a slot names a function the DLL does not export */
static int pe_bind(int id, const uint8_t* p, size_t size, const char* what) {
    for (int i = 0; i < g_nimports; i++) {
        const pe_import* im = &g_imports[i];
        if (im->dll != id) continue;
        uint32_t a = pe_export(p, size, im);
        if (!a) return fmt_fail("%s: import %s (ordinal %u) of slot %08x not exported", what, im->name, im->ordinal, im->slot);
#ifndef RX_ORACLE
        rx_wr32(im->slot, a);
#endif
    }
    return 0;
}

int rx_image_load_bytes(const uint8_t* p, size_t size) {
    int r = pe_map(p, size, RX_IMAGE_BASE, wanted, RX_IMAGE_EXE, g_sections, &g_nsections);
    if (r != 0) return r;
    g_nimports = 0;
    return pe_record_imports(p, size, RX_IMAGE_EXE);
}

int rx_image_load(const fmt_vfs* v) {
    (void)v; /* The product owns its immutable startup data; install assets remain in the VFS. */
    if (embedded_copy(0) != 0) return -1;
    g_nsections = (int)(sizeof rx_initial_sections / sizeof *rx_initial_sections);
    memcpy(g_sections, rx_initial_sections, sizeof rx_initial_sections);
    g_nimports = 0;
    return 0;
}

/* ---- the runtime DLLs from the install's vcredist_x86.exe (IExpress: a CAB, in it vc_red.cab, in it the DLLs; all LZX) ---- */
#include "fmt_lzx.h"

/* The member `want` (exact name) of the cabinet that starts at b[o]; 0 and *out filled, or -1. Only the folder holding the member is
 * decoded. CFHEADER: signature, cbCabinet, coffFiles +16, cFolders +26, cFiles +28, flags +30, reserve sizes when flags & 4. */
static int cab_member(const uint8_t* b, size_t n, size_t o, const char* want, fmt_buf* out) {
    if (o + 36 > n || memcmp(b + o, "MSCF", 4) != 0) return -1;
    uint32_t files_off = fmt_u32(b + o + 16);
    uint32_t folders = fmt_u16(b + o + 26), files = fmt_u16(b + o + 28), flags = fmt_u16(b + o + 30);
    size_t p = o + 36;
    uint32_t folder_res = 0, data_res = 0;
    if (flags & 4) {
        if (p + 4 > n) return -1;
        uint32_t h = fmt_u16(b + p); folder_res = b[p + 2]; data_res = b[p + 3]; p += 4 + h;
    }
    /* the file table first: the member's folder and range */
    size_t q = o + files_off;
    int want_folder = -1; uint32_t want_off = 0, want_size = 0;
    for (uint32_t i = 0; i < files; i++) {
        if (q + 16 > n) return -1;
        uint32_t size = fmt_u32(b + q), off = fmt_u32(b + q + 4), folder = fmt_u16(b + q + 8);
        q += 16;
        size_t e = q; while (e < n && b[e]) e++;
        if (e >= n) return -1;
        const char* name = (const char*)b + q;
        while (*name == '.' || *name == '\\') name++;
        if (!strcmp(name, want)) { want_folder = (int)folder; want_off = off; want_size = size; }
        q = e + 1;
    }
    if (want_folder < 0 || (uint32_t)want_folder >= folders) return -1;
    p += (size_t)(8 + folder_res) * (uint32_t)want_folder;
    if (p + 8 > n) return -1;
    uint32_t first = fmt_u32(b + p), blocks = fmt_u16(b + p + 4), type = fmt_u16(b + p + 6);
    if ((type & 0xf) != 3 || blocks == 0 || blocks > 40000) return -1;      /* not LZX */
    uint8_t* in = (uint8_t*)malloc((size_t)blocks * 33000u);
    if (!in) return -1;
    size_t in_len = 0, total = 0, d = o + first;
    for (uint32_t k = 0; k < blocks; k++) {
        if (d + 8 + data_res > n) { free(in); return -1; }
        uint32_t packed = fmt_u16(b + d + 4), size = fmt_u16(b + d + 6);
        d += 8 + data_res;
        if (d + packed > n || in_len + packed > (size_t)blocks * 33000u) { free(in); return -1; }
        memcpy(in + in_len, b + d, packed); in_len += packed; d += packed; total += size;
    }
    uint8_t* dec = (uint8_t*)malloc(total ? total : 1);
    int r = dec ? fmt_lzx_decode_once((int)((type >> 8) & 0x1f), in, in_len, dec, total) : -1;
    free(in);
    if (r != 0 || (uint64_t)want_off + want_size > total) { free(dec); return -1; }
    out->data = (uint8_t*)malloc(want_size ? want_size : 1);
    if (!out->data) { free(dec); return -1; }
    memcpy(out->data, dec + want_off, want_size); out->size = want_size;
    free(dec);
    return 0;
}

static rx_image_section g_dll_sections[2][16];
static int g_dll_n[2];

int rx_image_load_crt_bytes(const uint8_t* r, size_t n1, const uint8_t* pp, size_t n2) {
    if (pe_map(r, n1, RX_MSVCR90_BASE, wanted_dll, "msvcr90.dll", g_dll_sections[0], &g_dll_n[0]) != 0) return -1;
    if (pe_map(pp, n2, RX_MSVCP90_BASE, wanted_dll, "msvcp90.dll", g_dll_sections[1], &g_dll_n[1]) != 0) return -1;
    if (!g_nsections) return fmt_fail("rx_image_load_crt: load the exe first (its imports are bound here)");
    if (pe_record_imports(pp, n2, "msvcp90.dll") != 0) return -1;   /* msvcp90's own msvcr90 imports (its IAT is in its .text) */
    if (pe_bind(DLL_MSVCR90, r, n1, "msvcr90.dll") != 0) return -1;
    return pe_bind(DLL_MSVCP90, pp, n2, "msvcp90.dll");
}

int rx_image_extract_crt(const fmt_vfs* v, fmt_buf* msvcr90, fmt_buf* msvcp90) {
    fmt_buf exe = {0}, red = {0};
    int rc = -1;
    if (fmt_vfs_read_file(v, "Installers/vcredist_x86.exe", &exe) != 0) return -1;
    for (size_t at = 0; at + 4 <= exe.size; at++) {      /* the IExpress stub: find the cabinet that holds vc_red.cab */
        if (memcmp(exe.data + at, "MSCF", 4) != 0) continue;
        if (cab_member(exe.data, exe.size, at, "vc_red.cab", &red) == 0) break;
    }
    if (!red.data) { fmt_fail("vcredist_x86.exe: no vc_red.cab"); goto done; }
    if (cab_member(red.data, red.size, 0, "msvcr90.dll.30729.01.Microsoft_VC90_CRT_x86.SP", msvcr90) != 0 ||
        cab_member(red.data, red.size, 0, "msvcp90.dll.30729.01.Microsoft_VC90_CRT_x86.SP", msvcp90) != 0) {
        fmt_buf_free(msvcr90); fmt_buf_free(msvcp90);
        fmt_fail("vc_red.cab: msvcr90.dll / msvcp90.dll 9.0.30729.1 not found"); goto done;
    }
    rc = 0;
done:
    fmt_buf_free(&exe); fmt_buf_free(&red);
    return rc;
}

int rx_image_load_crt(const fmt_vfs* v) {
    (void)v;
    if (!g_nsections) return fmt_fail("rx_image_load_crt: load initial game data first");
    if (embedded_copy(1) != 0) return -1;
#ifndef RX_ORACLE
    for (size_t i = 0; i < sizeof rx_initial_bindings / sizeof *rx_initial_bindings; ++i)
        rx_wr32(rx_initial_bindings[i].slot, rx_initial_bindings[i].value);
#endif
    return 0;
}
