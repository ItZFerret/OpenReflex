/* build_xml.c -- see build_xml.h. Hand transliteration of XMLReader_Construct 00824e70, FUN_008252c0 (the loader), BXMLAssetIO::vf3
 * 008221f0 and FUN_00822100 from the disassembly. Added by the rider unit (construction.md section 13): the XMLReader virtuals with the exact cursor state
 * (rx_xml_vf*: the current node +0x3c, the cursor pool +0x184 / +0x1d4 / +0x2c4) that the animation tree / RiderPhysics readers call
 * through the vtable on the persistent BXMLAssets. The reader's own allocator (reader + 0x45c) is always 0 for the assets (plain
 * ReflexAlignedAlloc, FUN_00401f90); the big-endian swaps (flag 4) and the external data section (flag 2) are not in the PC install
 * and abort with a message if a file has them. */
#include "build_xml.h"
#include "build_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/fmt_inflate.h"
#include "../src/fmt_vfs.h"
#include "../src/rx_os.h"

/* 00824e70 */
rx_addr rx_xml_reader_construct(rx_addr s) {
    SU32(s, 0, 0x009753dc);
    SU32(s, 0x2c8, 0x00974b48);
    SU32(s, 0x2cc, 0); SU32(s, 0x2d0, 0); SU32(s, 0x2dc, 0); SU32(s, 0x2e0, 0); SU32(s, 0x2e4, 0); SU32(s, 0x2e8, 0);
    SU32(s, 0x2ec, 0); SU32(s, 0x2f0, 0); SU32(s, 0x2f4, 0);
    SU32(s, 0x2f8, 0x00975368);
    for (uint32_t k = 0; k < 0x38; k++) SU8(s, 0x300 + k, 0);          /* memset(+0x300, 0, 0x38) */
    SU32(s, 0x2fc, 0); SU32(s, 0x338, 0); SU32(s, 0x34c, 0); SU32(s, 0x350, 0); SU8(s, 0x358, 0); SU32(s, 0x344, 0);
    SU32(s, 0x340, 0x00974ca4);
    SU32(s, 4, 0); SU32(s, 8, 0); SU32(s, 0xc, 0); SU32(s, 0x10, 0); SU32(s, 0x14, 0); SU32(s, 0x18, 0); SU32(s, 0x1c, 0);
    SU32(s, 0x20, 0); SU32(s, 0x24, 0); SU32(s, 0x30, 0); SU32(s, 0x34, 0); SU32(s, 0x28, 0); SU32(s, 0x2c, 0); SU32(s, 0x38, 0);
    SU32(s, 0x3c, 0);
    for (uint32_t a = 0; a < 0x14; a++) { SU32(s, 0x40 + 4 * a, a); SU32(s, 0x90 + 12 * a, 0); }
    SU32(s, 0x180, 0x14);
    for (uint32_t a = 0; a < 0x14; a++) { SU32(s, 0x184 + 4 * a, a); SU32(s, 0x1d4 + 12 * a, 0); }
    SU32(0, 0x00a95950, U32(0, 0x00a95950) + 1);     /* the live XMLReader count */
    SU32(s, 0x33c, 0); SU32(s, 0x2c4, 0x14); SU32(s, 0x45c, 0);
    return s;
}
RX_FUNCTION(0x00824e70, rx_xml_reader_construct);

static int try_read(rx_stream* s, void* dst, uint32_t n) {
    if (s->size - s->pos < n) { s->short_read = 1; return 0; }
    rx_stream_read_host(s, dst, n);
    return 1;
}
static uint32_t le32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint32_t le16(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }

/* FUN_005c3380 (EDX = string) */
static uint32_t str_hash(rx_addr s) {
    uint32_t h = 0;
    for (uint8_t c; (c = U8(s, 0)); s++) {
        h = (h << 4) + (uint32_t)(int32_t)(int8_t)c;
        uint32_t t = h & 0xf0000000u;
        if (t) h ^= (t >> 24) ^ t;
    }
    return h;
}

/* FUN_0082ab20 + FUN_008434f0: the string table (key = the string pointer, value = its index) */
static rx_addr strings_table(rx_addr b) {
    uint32_t n = 2 * U32(b, 4);
    rx_addr t = rx_reflex_aligned_alloc(0x20);
    if (!t) return 0;
    rx_addr al = rx_reflex_aligned_alloc(4);
    if (al) SU32(al, 0, 0x009753b0);
    SU32(t, 0x18, al); SU8(t, 0x1c, 1);
    SU32(t, 0, n); SF32(t, 4, 0.75f /* 0x935aa8 */); SU32(t, 0x10, 0); SU32(t, 0x14, 0xffffffffu); SU32(t, 8, 0);
    rx_addr bk = rx_reflex_aligned_alloc(4 * n);
    SU32(t, 0xc, bk);
    for (uint32_t k = 0; k < 4 * n; k++) SU8(bk, k, 0);
    for (uint32_t i = 0; i < U32(b, 4); i++) {
        rx_addr key = U32(U32(b, 0xc), 4 * i);
        if ((float)U32(t, 8) / (float)U32(t, 0) > F32(t, 4)) { fprintf(stderr, "rxcore: XMLReader string table rehash not ported\n"); abort(); }
        uint32_t h = str_hash(key), bi = h % U32(t, 0);
        rx_addr e = U32(U32(t, 0xc), 4 * bi);
        for (; e; e = U32(e, 0xc))
            if (U32(e, 8) == h) {            /* equal hash: compare the two strings byte by byte */
                rx_addr p = U32(e, 0), q = key; int eq = 1;
                for (;; p++, q++) { if (U8(p, 0) != U8(q, 0)) { eq = 0; break; } if (!U8(p, 0)) break; }
                if (eq) break;
            }
        if (e) { SU32(e, 4, i); continue; }
        rx_addr nd = rx_reflex_aligned_alloc(0x10);
        SU32(nd, 0xc, 0);
        SU32(nd, 0, key); SU32(nd, 4, i); SU32(nd, 8, h);
        SU32(nd, 0xc, U32(U32(t, 0xc), 4 * bi));
        SU32(U32(t, 0xc), 4 * bi, nd);
        SU32(t, 8, U32(t, 8) + 1);
    }
    return t;
}

/* _aligned_malloc / _aligned_free under the allocator lock (critical section 0x00d61704 when [0x00da67d8]) */
static rx_addr rx_crt_aligned_malloc_locked(uint32_t size, uint32_t align) {
    if (U8(0, 0x00da67d8)) { uint32_t a = 0x00d61704; rx_os_import(0x0091600c, 1, &a); }
    const rx_addr p = rx_crt_aligned_malloc(size, align);
    if (U8(0, 0x00da67d8)) { uint32_t a = 0x00d61704; rx_os_import(0x009160f8, 1, &a); }
    return p;
}
static void rx_crt_aligned_free_locked(rx_addr p) {
    if (U8(0, 0x00da67d8)) { uint32_t a = 0x00d61704; rx_os_import(0x0091600c, 1, &a); }
    rx_crt_aligned_free(p);
    if (U8(0, 0x00da67d8)) { uint32_t a = 0x00d61704; rx_os_import(0x009160f8, 1, &a); }
}

/* zlib's 32 KB window holds the decompressed stream: `total` < 32 KB at offset 0 (the first updatewindow copy copies the output
 * as produced); the rest of the block keeps its old content. Host zlib does not keep the window; this restores the original's. */
static void fill_window(rx_addr w, const uint8_t* data, uint64_t total) {
    if (!w || !data) return;
    const uint32_t n = total < 0x8000 ? (uint32_t)total : 0x8000;
    const uint8_t* p = data + (total - n);
    for (uint32_t i = 0; i < n; i++) SU8(w, i, p[i]);
}

/* FUN_008252c0 (stack: reader, stream) */
int rx_xml_reader_load(rx_addr b, rx_stream* src) {
    rx_stream sub;
    rx_stream* st = src;
    uint8_t H[0x20];
    uint64_t ztotal = 0;
    if (!try_read(src, H, 0x20)) return 0;
    uint32_t version = le16(H + 4), flags = le16(H + 6);
    uint32_t nstr = le32(H + 8), sbytes = le32(H + 0xc), dbytes = le32(H + 0x10), nvals = le32(H + 0x14), nnodes = le32(H + 0x18);
    uint8_t* inflated = NULL;
    rx_addr zstate = 0, zchunk = 0, zwindow = 0;
    if (flags & 4) { fprintf(stderr, "rxcore: BXML big-endian data not ported\n"); abort(); }
    if (flags & 1) {                                  /* FUN_0080f920: the compressed stream wrapper at +0x2c8 */
        /* FUN_0080f920 (the wrapper +0x2c8 on the source, the decompressor ZLibCompression +0x2f8): the decompressor's vf3 = zlib 1.2.1
         * inflateInit2 (the exe's debug-built zlib: its state through zcalloc 008ef290 = msvcr90 malloc(0x1ba8)), then the one chunk
         * buffer _aligned_malloc(0x40000, 16) under the allocator lock, the compressed length and the compressed bytes read into the
         * chunk (the whole stream when shorter than the chunk, else chunk by chunk as inflate consumes them: the chunk ends with the
         * last part over the tail of the one before). zlib's 32 KB window is malloc'ed by the first inflate that produces output
         * (updatewindow), and everything is freed at the wrapper's close (0080fb80: inflateEnd frees the window, then the state; then
         * the chunk). The decoding itself is host zlib (identical output): the state / window CONTENTS are not reproduced (freed
         * blocks: known-differences.md). */
        zstate = rx_crt_malloc(0x1ba8);
        zchunk = rx_crt_aligned_malloc_locked(0x40000, 16);
        uint32_t clen = 0;   /* (src->data is a host buffer in both builds: the caller's file bytes) */
        if (!try_read(src, &clen, 4) || clen == 0xffffffffu) return 0;
        clen = le32((uint8_t*)&clen);
        uint64_t total = (uint64_t)sbytes + dbytes + (uint64_t)nvals * 12 + (uint64_t)nnodes * 32;
        ztotal = total;
        if (src->size - src->pos < clen || total > 0x40000000u) return 0;
        {
            const uint8_t* cd = src->data + src->pos;
            const uint32_t C = 0x40000u, full = clen / C, part = clen % C;
            if (clen < C) { for (uint32_t i = 0; i < clen; i++) SU8(zchunk, i, cd[i]); }
            else {
                /* The final partial read overwrites the start of the preceding complete chunk (0080fc00).
                 * Starting at full*C here would copy C bytes from the partial tail and overread the host input. */
                const uint32_t lastfull = (full - 1) * C;
                for (uint32_t i = 0; i < C; i++) SU8(zchunk, i, cd[lastfull + i]);
                if (part) for (uint32_t i = 0; i < part; i++) SU8(zchunk, i, cd[full * C + i]);
            }
        }
        inflated = (uint8_t*)malloc(total ? total : 1);
        if (fmt_zlib_decode(src->data + src->pos, clen, inflated, (size_t)total) < 0) { free(inflated); return 0; }
        src->pos += clen;
        rx_stream_init(&sub, inflated, (size_t)total);
        st = &sub;
        SU32(b, 0x2d4, zchunk); SU32(b, 0x2d8, 1);   /* the wrapper's chunk pointer (+0xc) and count (+0x10) stay after its close */
    }
    SU32(b, 0x30, 0);
    /* strings */
    rx_addr str = rx_reflex_aligned_alloc(sbytes);
    SU32(b, 8, str);
    rx_addr tab = rx_reflex_aligned_alloc(nstr * 4);
    SU32(b, 0xc, tab);
    if (!str || sbytes > st->size - st->pos) goto fail;
    if (zstate && sbytes) { zwindow = rx_crt_malloc(0x8000); fill_window(zwindow, inflated, ztotal); }       /* the first inflate's updatewindow */
    rx_stream_read(st, str, sbytes);
    SU32(tab, 0, str);
    SU32(b, 4, 1);
    for (uint32_t i = 0; (int32_t)U32(b, 4) < (int32_t)nstr; i++) {
        rx_addr p = U32(b, 8) + i;
        if (U8(p, 0) == 0) { SU32(U32(b, 0xc), 4 * U32(b, 4), p + 1); SU32(b, 4, U32(b, 4) + 1); }
    }
    if (dbytes) {
        SU32(b, 0x18, dbytes);
        SU32(b, 0x14, rx_reflex_aligned_alloc(dbytes));
        if (dbytes > st->size - st->pos) goto fail;
        if (zstate && !zwindow) { zwindow = rx_crt_malloc(0x8000); fill_window(zwindow, inflated, ztotal); }
        rx_stream_read(st, U32(b, 0x14), dbytes);
    }
    SU32(b, 0x30, nvals); SU32(b, 0x28, nnodes);
    rx_addr blk = rx_reflex_aligned_alloc(12 * (nvals + 3 * nnodes));
    SU32(b, 0x24, blk); SU32(b, 0x34, blk); SU32(b, 0x2c, blk + nvals * 12);
    if (nvals > 0) {
        if (version < 0x3ea) { fprintf(stderr, "rxcore: BXML version %x not ported\n", version); abort(); }
        for (uint32_t i = 0; i < nvals; i++) {
            uint8_t r[12];
            if (!try_read(st, r, 12)) goto fail;
            uint32_t w0 = le32(r), w1 = le32(r + 4), h2 = le16(r + 8), h3 = le16(r + 10);
            rx_addr e = U32(b, 0x34) + 12 * i;
            SU32(e, 0, w0); SU16(e, 8, h2); SU16(e, 0xa, h3);
            if (h3 == 1) SU32(e, 4, w1);
            else if (h3 == 0) { SU32(e, 4, 0); SU16(e, 8, 0); }
            else SU32(e, 4, U32(b, 0x14) + w1);
        }
    }
    for (uint32_t i = 0; i < nnodes; i++) {
        uint8_t r[0x20];
        if (!try_read(st, r, 0x20)) goto fail;
        uint32_t w0 = le32(r), w1 = le32(r + 4), w2 = le32(r + 8), d = le16(r + 12), g = le32(r + 0x10), cc = le32(r + 0x14),
                 a0 = le32(r + 0x18), ac = le32(r + 0x1c);
        rx_addr base = U32(b, 0x2c), n = base + 0x24 * i;
        SU32(n, 0, U32(U32(b, 0xc), 4 * w0));
        SU8(n, 6, r[14]); SU8(n, 7, r[15]);
        if (r[14] & 1) { SU16(n, 4, d); SU32(n, 0xc, w2); SU32(n, 8, w1); }
        if (w2 == 0) {
            SU32(n, 0xc, 0); SU32(n, 8, 0);
            if (d == 1) { SU16(n, 4, 1); SU32(n, 8, U32(U32(b, 0xc), 4 * w1)); }
            else { SU16(n, 4, 0); SU32(n, 8, 0); }
        } else {
            SU16(n, 4, d); SU32(n, 0xc, w2); SU32(n, 8, U32(b, 0x14) + w1);
        }
        SU32(n, 0x18, cc);
        SU32(n, 0x14, base + g * 0x24);
        for (uint32_t k = 0; k < cc; k++) SU32(U32(n, 0x14) + 0x10 + 0x24 * k, 0, n);
        SU32(n, 0x20, ac);
        SU32(n, 0x1c, U32(b, 0x34) + a0 * 0xc);
    }
    SU32(b, 0x38, U32(b, 0x2c)); SU32(b, 0x3c, U32(b, 0x2c));
    SU32(b, 0x10, strings_table(b));
    /* CompressedInputStream::vf1 (the wrapper closes) */
    if (flags & 2) { fprintf(stderr, "rxcore: BXML external data section (flag 2) not ported\n"); abort(); }
    if (flags & 1) {            /* CompressedInputStream::vf2 (0080fc10) leaves the size of its last read at wrapper + 0x2c */
        uint32_t last = nnodes ? 0x20 : nvals ? 12 : dbytes ? dbytes : sbytes;
        if (last) SU32(b, 0x2f4, last);
    }
    if (zstate) {                                                /* 0080fb80: inflateEnd (window, state), then the chunk */
        if (zwindow) rx_crt_free(zwindow);
        rx_crt_free(zstate);
        rx_crt_aligned_free_locked(zchunk);
    }
    SU32(b, 0x33c, 0);
    free(inflated);
    return 1;
fail:
    fprintf(stderr, "rxcore: BXML load failed (truncated)\n");
    free(inflated);
    return 0;
}

/* FUN_00825260 (ECX = reader; name, flags; RET 8): a BXML file of the install into the reader: FUN_00827e00 (the reader reset: five
 * _aligned_free(NULL), no heap effect), the reader's CRTFileInputStream (+0x340) vf8 = fopen(name, "rb") (008104d0), FUN_008252c0,
 * vf1 = fclose. The stdio stream's buffer is msvcr90's: _getbuf's _malloc_crt(0x1000) at the first fread (the 0x20-byte header),
 * filled from the file, freed by fclose after the load (its contents are dead heap). 1 when loaded.
 *
 * Oracle build: the file is read through the original CRT's fopen / fread / fclose (the exe's IAT slots; the boot maps C:\Reflex
 * to the install), and the FILE stays open while the core parses, so the CRT's 0x1000 _getbuf block sits in the heap exactly where
 * the original's does. The bytes are held in a host buffer (no game allocation). Product: the session install's vfs; the 0x1000
 * stdio buffer is allocated by the core for the same heap shape. (The zlib state / window contents of the compressed wrapper are
 * the documented host-zlib boundary; the window is filled below to match the original.) */
int rx_xml_reader_load_file(const fmt_vfs* vfs, rx_addr reader, const char* name) {
#ifdef RX_ORACLE
    /* the original's CRT file path (fopen / fread / fclose through the exe's IAT) */
    (void)vfs;
    RX_FRAME_ENTER(F, 0x1080);
    const uint32_t nlen = (uint32_t)strlen(name);
    for (uint32_t i = 0; i <= nlen && i < 0x40; i++) SU8(F + 0x1000, i, (uint8_t)(i < nlen ? name[i] : 0));
    SU8(F, 0x1040, 'r'); SU8(F, 0x1041, 'b'); SU8(F, 0x1042, 0);
    const uint32_t fa[2] = {F + 0x1000, F + 0x1040};
    const rx_addr f = rx_call_original(U32(0, 0x009162dc), RX_CDECL, 0, 2, fa);   /* fopen */
    if (!f) { RX_FRAME_LEAVE(F, 0x1080); return 0; }
    size_t cap = 0x2000, n = 0;
    uint8_t* buf = (uint8_t*)malloc(cap);
    int failed = buf == NULL;
    for (;;) {
        const uint32_t ra[4] = {F, 1, 0x100, f};   /* < the stdio buffer (0x1000): keeps the CRT's _getbuf allocation, as the original's 0x20-byte header read does */
        const uint32_t got = rx_call_original(U32(0, 0x009162c8), RX_CDECL, 0, 4, ra);   /* fread */
        if (failed) break;
        if (n + got > cap) {
            uint8_t* nb = (uint8_t*)realloc(buf, cap * 2 + got);
            if (!nb) { failed = 1; break; }
            buf = nb; cap = cap * 2 + got;
        }
        for (uint32_t i = 0; i < got; i++) buf[n + i] = U8(F, i);
        n += got;
        if (got < 0x100) break;
    }
    RX_FRAME_LEAVE(F, 0x1080);
    int ok = 0;
    if (!failed) {
        rx_stream s; rx_stream_init(&s, buf, n);
        ok = rx_xml_reader_load(reader, &s);           /* the FILE is still open: its stdio buffer stays in the heap */
    }
    { const uint32_t ca[1] = {f}; rx_call_original(U32(0, 0x009162c4), RX_CDECL, 0, 1, ca); }   /* fclose */
    free(buf);
    return ok;
#else
    fmt_buf fb = {0};
    if (fmt_vfs_read_file(vfs, name, &fb) != 0) return 0;      /* fopen fails: 0 (nothing allocated) */
    const rx_addr iobuf = rx_crt_malloc(0x1000);
    if (iobuf) { const size_t n = fb.size < 0x1000 ? fb.size : 0x1000; for (size_t i = 0; i < n; i++) SU8(iobuf, (uint32_t)i, fb.data[i]); }
    rx_stream s; rx_stream_init(&s, fb.data, fb.size);
    const int ok = rx_xml_reader_load(reader, &s);
    if (iobuf) rx_crt_free(iobuf);                             /* fclose: _freebuf */
    fmt_buf_free(&fb);
    return ok;
#endif
}

/* FUN_00822100 (EAX = object, stack tag lo / hi) */
static rx_addr bxml_construct(rx_addr o, uint32_t lo, uint32_t hi) {
    rx_xml_reader_construct(o);
    SU32(o, 0x460, 0x0093eb78);
    SU32(o, 0x468, 0); SU32(o, 0x46c, 0);
    SU32(o, 0x470, lo);
    SU32(o, 0, 0x00975524);
    SU32(o, 0x460, 0x0097566c);
    SU32(o, 0x474, hi);
    return o;
}

/* BXMLAssetIO::vf3 008221f0 */
rx_addr rx_bxml_asset_io_create(rx_stream* s, uint32_t lo, uint32_t hi) {
    rx_addr o = rx_reflex_aligned_alloc(0x478);
    if (o) bxml_construct(o, lo, hi);
    if (!o) return 0;
    rx_xml_reader_load(o, s);
    return o + 0x460;
}

/* ---- queries ---- */

rx_addr rx_xml_root(rx_addr b) { return U32(b, 0x38); }

/* FUN_007fa030: the index of a string in the table (-1 absent) */
static int str_index(rx_addr b, const char* name) {
    rx_addr t = U32(b, 0x10);
    if (!t || !U32(t, 0)) return -1;
    uint32_t h = 0;
    for (const unsigned char* c = (const unsigned char*)name; *c; c++) { h = (h << 4) + (uint32_t)(int32_t)(int8_t)*c; uint32_t x = h & 0xf0000000u; if (x) h ^= (x >> 24) ^ x; }
    for (rx_addr e = U32(U32(t, 0xc), 4 * (h % U32(t, 0))); e; e = U32(e, 0xc)) {
        if (U32(e, 8) != h) continue;
        rx_addr p = U32(e, 0); const char* q = name; int eq = 1;
        for (;; p++, q++) { if (U8(p, 0) != (uint8_t)*q) { eq = 0; break; } if (!*q) break; }
        if (eq) return (int)U32(e, 4);
    }
    return -1;
}
static rx_addr str_ptr(rx_addr b, uint32_t idx) { return U32(U32(b, 0xc), 4 * idx); }

rx_addr rx_xml_find_child(rx_addr b, rx_addr node, const char* name) {
    if (!node || !U32(node, 0x18)) return 0;
    if (!name) return U32(node, 0x14);
    int idx = str_index(b, name);
    if (idx < 0) return 0;
    rx_addr want = str_ptr(b, (uint32_t)idx);
    for (uint32_t i = 0; i < U32(node, 0x18); i++) { rx_addr c = U32(node, 0x14) + 0x24 * i; if (U32(c, 0) == want) return c; }
    return 0;
}
rx_addr rx_xml_next_child(rx_addr b, rx_addr prev) {
    (void)b;
    rx_addr parent = U32(prev, 0x10);
    if (!parent) return 0;
    uint32_t last = U32(parent, 0x14) + 0x24 * U32(parent, 0x18);
    for (rx_addr c = prev + 0x24; c < last; c += 0x24) if (U32(c, 0) == U32(prev, 0)) return c;
    return 0;
}
rx_addr rx_xml_find_attr(rx_addr b, rx_addr node, const char* name) {
    if (!node || !U32(node, 0x20)) return 0;
    if (!name) return U32(node, 0x1c);
    int idx = str_index(b, name);
    if (idx < 0) return 0;
    for (uint32_t i = 0; i < U32(node, 0x20); i++) { rx_addr e = U32(node, 0x1c) + 12 * i; if (U32(e, 0) == (uint32_t)idx) return e; }   /* attribute names are string indices */
    return 0;
}

static void gets_game(rx_addr s, char* out, size_t n) {
    size_t i = 0; for (; i + 1 < n && U8(s, (uint32_t)i); i++) out[i] = (char)U8(s, (uint32_t)i); out[i] = 0;
}
/* the comma list loops of 00829560 / 008295e0 / 00829640 / 008294e0: element i parsed at the cursor, the cursor moved after the next
 * comma; fails when a comma is missing before the last element */
typedef enum { L_INT, L_UINT, L_FLOAT, L_BOOL } list_kind;
static int parse_list(rx_addr str, rx_addr dst, int n, list_kind k) {
    char buf[0x400]; gets_game(str, buf, sizeof buf);
    const char* p = buf;
    for (int i = 0; i < n; i++) {
        uint32_t val = 0;
        if (k == L_INT) {
            char c = *p;
            if (c == 'T' || c == 't') val = 1;
            else if (c == 'F' || c == 'f') val = 0;
            else { long v = strtol(p, NULL, 0); val = (uint32_t)(int32_t)(v > INT32_MAX ? INT32_MAX : v < INT32_MIN ? INT32_MIN : v); }
        } else if (k == L_UINT) { unsigned long v = strtoul(p, NULL, 0); val = v > 0xffffffffUL ? 0xffffffffu : (uint32_t)v; }
        else if (k == L_FLOAT) { float f = (float)atof(p); val = rx_fb(f); }
        else {
            if (!*p) return 0;
            val = (*p == 'F' || *p == 'f' || *p == '0') ? 0 : 1;
        }
        SU32(dst, 4 * (uint32_t)i, val);
        const char* c = strchr(p, ',');
        if (!c) { if (i != n - 1) return 0; break; }
        p = c + 1;
        if (k == L_BOOL) while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
    }
    return 1;
}
static int attr_values(rx_addr b, rx_addr e, rx_addr dst, int n, int direct_type, list_kind k) {
    uint32_t t = U16(e, 0xa);
    if (t == (uint32_t)direct_type) { for (int i = 0; i < n; i++) SU32(dst, 4 * (uint32_t)i, U32(U32(e, 4), 4 * (uint32_t)i)); return 1; }
    if (t == 1) return parse_list(str_ptr(b, U32(e, 4)), dst, n, k);
    return 0;
}
int rx_xml_attr_ints(rx_addr b, rx_addr e, rx_addr dst, int n) { return attr_values(b, e, dst, n, 3, L_INT); }
int rx_xml_attr_uints(rx_addr b, rx_addr e, rx_addr dst, int n) { return attr_values(b, e, dst, n, 4, L_UINT); }
int rx_xml_attr_floats(rx_addr b, rx_addr e, rx_addr dst, int n) { return attr_values(b, e, dst, n, 5, L_FLOAT); }
int rx_xml_attr_bools(rx_addr b, rx_addr e, rx_addr dst, int n) { return attr_values(b, e, dst, n, 0xb, L_BOOL); }
const char* rx_xml_attr_string(rx_addr b, rx_addr e, char* out, size_t n) {
    if (!e || U16(e, 0xa) != 1) return NULL;
    gets_game(str_ptr(b, U32(e, 4)), out, n);
    return out;
}
static int node_values(rx_addr node, rx_addr dst, int n, int direct_type, list_kind k) {
    uint32_t t = U16(node, 4);
    if (t == (uint32_t)direct_type) { for (int i = 0; i < n; i++) SU32(dst, 4 * (uint32_t)i, U32(U32(node, 8), 4 * (uint32_t)i)); return 1; }
    if (t == 1) return parse_list(U32(node, 8), dst, n, k);
    return 0;
}
int rx_xml_node_ints(rx_addr b, rx_addr node, rx_addr dst, int n) { (void)b; return node_values(node, dst, n, 3, L_INT); }
int rx_xml_node_uints(rx_addr b, rx_addr node, rx_addr dst, int n) { (void)b; return node_values(node, dst, n, 4, L_UINT); }
int rx_xml_node_floats(rx_addr b, rx_addr node, rx_addr dst, int n) { (void)b; return node_values(node, dst, n, 5, L_FLOAT); }
int rx_xml_node_bools(rx_addr b, rx_addr node, rx_addr dst, int n) { (void)b; return node_values(node, dst, n, 0xb, L_BOOL); }

/* FUN_00824fe0 (the allocator-less path) */
void rx_xml_reader_destroy(rx_addr b) {
    SU32(0, 0x00a95950, U32(0, 0x00a95950) - 1);
    rx_reflex_aligned_free(U32(b, 8)); rx_reflex_aligned_free(U32(b, 0xc)); rx_reflex_aligned_free(U32(b, 0x14));
    rx_reflex_aligned_free(U32(b, 0x1c)); rx_reflex_aligned_free(U32(b, 0x24));
    rx_addr t = U32(b, 0x10);
    if (t) {                                                  /* FUN_007fa1e0 */
        for (uint32_t k = 0; k < U32(t, 0); k++)
            for (rx_addr e = U32(U32(t, 0xc), 4 * k), nx; e; e = nx) { nx = U32(e, 0xc); rx_reflex_aligned_free(e); }
        rx_reflex_aligned_free(U32(t, 0xc));
        if (U8(t, 0x1c) && U32(t, 0x18)) rx_reflex_aligned_free(U32(t, 0x18));
        rx_reflex_aligned_free(t);
    }
}

/* FUN_00829750 (EAX = string, ECX = dst, stack n): n "x,y,z" triples into 16-byte slots */
static int parse_vec3_list(rx_addr str, rx_addr dst, int n) {
    char buf[0x400]; gets_game(str, buf, sizeof buf);
    const char* p = buf;
    for (int i = 0; i < n; i++) {
        for (int k = 0; k < 3; k++) {
            SU32(dst, 16 * (uint32_t)i + 4 * (uint32_t)k, rx_fb((float)atof(p)));
            const char* c = strchr(p, ',');
            if (!c) { if (k == 2 && i == n - 1) return 1; return 0; }
            p = c + 1;
        }
    }
    return 1;
}
static int vec3_values(rx_addr src, int n, rx_addr dst) {
    for (int i = n; i > 0; i--) { for (int k = 0; k < 3; k++) SU32(dst, 16 * (uint32_t)(i - 1) + 4 * (uint32_t)k, U32(src, 12 * (uint32_t)(i - 1) + 4 * (uint32_t)k)); }
    return 1;
}
int rx_xml_attr_vec3(rx_addr b, rx_addr e, rx_addr dst, int n) {
    uint32_t t = U16(e, 0xa);
    if (t == 0xa) return n > 0 ? vec3_values(U32(e, 4), n, dst) : 1;
    if (t == 1 && n > 0) parse_vec3_list(str_ptr(b, U32(e, 4)), dst, n);
    return 1;
}
int rx_xml_node_vec3(rx_addr b, rx_addr node, rx_addr dst, int n) {
    (void)b;
    int32_t t = (int16_t)U16(node, 4);
    if (t == 1) { parse_vec3_list(U32(node, 8), dst, n); return 1; }
    if (t == 0xa) return n > 0 ? vec3_values(U32(node, 8), n, dst) : 1;
    return 0;
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * the XMLReader virtuals (vtable 0x009753dc .., this = the reader), exact including the cursor state the original keeps:
 * +0x3c the current node, the cursor pool: +0x184 + 4 * k the free cursor indices (top +0x2c4), cursor c at +0x1d4 + 0xc * c
 * {parent node, child index, name (a string-table string pointer, 0 = any)} (rider unit, construction.md section 13) */
#include "crt.h"
#include "world_misc.h"

/* a virtual call of the reader with one argument (the core's when registered) */
static rx_addr rr_xml_vcall1(rx_addr r, uint32_t off, uint32_t a) {
    rx_addr t = U32(U32(r, 0), off);
    rx_addr (*f)(rx_addr, uint32_t) = RX_FN(t, rx_addr (*)(rx_addr, uint32_t));
    return f ? f(r, a) : rx_call_original(t, RX_THISCALL, r, 1, &a);
}
/* the string-table index of a name (007fa030 over the reader's hash table +0x10): 1 found; the original passes the name and the out
 * slot by address (its own argument slots): a small game-stack frame here */
static int vf_string_index(rx_addr r, rx_addr name, uint32_t* idx) {
    RX_FRAME_ENTER(F, 8);
    SU32(F, 0, name); SU32(F, 4, 0);
    uint32_t found = rx_world_string_table_find(U32(r, 0x10), F, F + 4);
    *idx = U32(F, 4);
    RX_FRAME_LEAVE(F, 8);
    return found != 0;
}
/* vf1 00827f70: the root node (+0x3c = +0x38) */
rx_addr rx_xml_vf_root(rx_addr r) { SU32(r, 0x3c, U32(r, 0x38)); return U32(r, 0x38); }
RX_FUNCTION(0x00827f70, rx_xml_vf_root)
/* vf7 00828020 (node, name; RET 8): the first child of that name (the first child for name 0), also +0x3c */
rx_addr rx_xml_vf_child(rx_addr r, rx_addr node, rx_addr name) {
    if (node && U32(node, 0x18)) {
        int32_t n = (int32_t)U32(node, 0x18);
        if (!name) { SU32(r, 0x3c, U32(node, 0x14)); return U32(node, 0x14); }
        uint32_t idx;
        if (vf_string_index(r, name, &idx)) {
            uint32_t key = U32(U32(r, 0xc), 4 * idx);
            for (int32_t k = 0; k < n; k++) {
                rx_addr c = U32(node, 0x14) + 0x24u * (uint32_t)k;
                if (U32(c, 0) == key) { SU32(r, 0x3c, c); return c; }
            }
        }
    }
    SU32(r, 0x3c, 0);
    return 0;
}
RX_FUNCTION(0x00828020, rx_xml_vf_child)
/* vf10 008281c0 (cursor; RET 4): the cursor's next child (of its name), also +0x3c */
rx_addr rx_xml_vf_next(rx_addr r, uint32_t c) {
    rx_addr cur = r + 0xc * c;   /* +0x1d4 node, +0x1d8 index, +0x1dc name */
    rx_addr node = U32(cur, 0x1d4);
    int32_t n = (int32_t)U32(node, 0x18), i = (int32_t)U32(cur, 0x1d8);
    if (i >= n - 1) { SU32(cur, 0x1d8, (uint32_t)n); SU32(r, 0x3c, 0); return 0; }
    i++;
    uint32_t name = U32(cur, 0x1dc);
    if (!name) {
        SU32(cur, 0x1d8, (uint32_t)i);
        rx_addr e = U32(node, 0x14) + 0x24u * (uint32_t)i;
        SU32(r, 0x3c, e); return e;
    }
    for (; i < n; i++) {
        rx_addr e = U32(node, 0x14) + 0x24u * (uint32_t)i;
        if (name == U32(e, 0)) { SU32(cur, 0x1d8, (uint32_t)i); SU32(r, 0x3c, e); return e; }
    }
    SU32(cur, 0x1d8, (uint32_t)i);
    SU32(r, 0x3c, 0);
    return 0;
}
RX_FUNCTION(0x008281c0, rx_xml_vf_next)
/* vf8 008280b0 (node, name, out cursor*; RET 0xc): a cursor from the pool over the node's children (of that name), its first match */
rx_addr rx_xml_vf_first(rx_addr r, rx_addr node, rx_addr name, rx_addr out) {
    uint32_t c = U32(r, 0x180 + 4 * U32(r, 0x2c4));
    rx_addr cur = r + 0xc * c;
    SU32(cur, 0x1d4, node);
    SU32(cur, 0x1d8, 0xffffffffu);
    if (name) {
        uint32_t idx;
        if (!vf_string_index(r, name, &idx)) {
            SU32(cur, 0x1d4, 0); SU32(out, 0, 0xffffffffu); SU32(r, 0x3c, 0); return 0;
        }
        SU32(cur, 0x1dc, U32(U32(r, 0xc), 4 * idx));
    } else SU32(cur, 0x1dc, 0);
    uint32_t a = c;
    rx_addr e = rr_xml_vcall1(r, 0x28, a);
    if (!e) { SU32(cur, 0x1d4, 0); SU32(out, 0, 0xffffffffu); SU32(r, 0x3c, 0); return 0; }
    SU32(r, 0x2c4, U32(r, 0x2c4) - 1);
    SU32(out, 0, c);
    SU32(r, 0x3c, e);
    return e;
}
RX_FUNCTION(0x008280b0, rx_xml_vf_first)
/* vf9 00828190 (cursor; RET 4): the cursor back to the pool */
uint32_t rx_xml_vf_release(rx_addr r, uint32_t c) {
    if (c != 0xffffffffu) {
        SU32(r + 0xc * c, 0x1d4, 0);
        SU32(r, 0x184 + 4 * U32(r, 0x2c4), c);
        SU32(r, 0x2c4, U32(r, 0x2c4) + 1);
    }
    return 0;
}
RX_FUNCTION(0x00828190, rx_xml_vf_release)
/* vf40 008284f0 (node, buf, n; RET 0xc): the node's name into buf (strncpy n - 1, terminated) */
uint32_t rx_xml_vf_node_name(rx_addr r, rx_addr node, rx_addr buf, uint32_t n) {
    (void)r; crt_strncpy(buf, U32(node, 0), n - 1); SU8(buf, n - 1, 0); return buf;
}
RX_FUNCTION(0x008284f0, rx_xml_vf_node_name)
/* vf58 00828ae0 (node, name; RET 8): the attribute entry of that name (the first for name 0), or 0 */
rx_addr rx_xml_vf_attr(rx_addr r, rx_addr node, rx_addr name) {
    if (!node || !U32(node, 0x20)) return 0;
    int32_t n = (int32_t)U32(node, 0x20);
    if (!name) return U32(node, 0x1c);
    uint32_t idx;
    if (!vf_string_index(r, name, &idx)) return 0;
    for (int32_t k = 0; k < n; k++) if (U32(U32(node, 0x1c) + 12u * (uint32_t)k, 0) == idx) return U32(node, 0x1c) + 12u * (uint32_t)k;
    return 0;
}
RX_FUNCTION(0x00828ae0, rx_xml_vf_attr)
/* vf56 00828630 (node; RET 4): the node's text (+8) (w-tutorials: TutorialAudioMappings' voice paths) */
rx_addr rx_xml_vf_node_text(rx_addr r, rx_addr node) { (void)r; return U32(node, 8); }
RX_FUNCTION(0x00828630, rx_xml_vf_node_text)
/* vf68 00828f90 (attr; RET 4): the string of a string attribute */
rx_addr rx_xml_vf_attr_cstr(rx_addr r, rx_addr e) { return U32(U32(r, 0xc), 4 * U32(e, 4)); }
RX_FUNCTION(0x00828f90, rx_xml_vf_attr_cstr)
/* vf79 00828fd0 (attr, buf, n; RET 0xc): the attribute's string into buf; 1 */
uint32_t rx_xml_vf_attr_string(rx_addr r, rx_addr e, rx_addr buf, uint32_t n) {
    crt_strncpy(buf, U32(U32(r, 0xc), 4 * U32(e, 4)), n - 1); SU8(buf, n - 1, 0); return 1;
}
RX_FUNCTION(0x00828fd0, rx_xml_vf_attr_string)
/* the node value forms (this unused): vf51 008287c0 floats (5), vf52 00828760 ints (3), vf57 008286a0 bools (0xb), vf54 00828640 a
 * string (type 1) or "" (type 0) */
static uint32_t node_form(rx_addr r, rx_addr node, rx_addr dst, uint32_t n, int32_t type) {
    int32_t t = (int16_t)U16(node, 4);
    if (t == 1) {
        if (type == 5) rx_xml_node_floats(r, node, dst, (int)n);
        else if (type == 3) rx_xml_node_ints(r, node, dst, (int)n);
        else rx_xml_node_bools(r, node, dst, (int)n);
        return 1;
    }
    if (t == type) { for (uint32_t k = 0; k < n; k++) SU32(dst, 4 * k, U32(U32(node, 8), 4 * k)); return 1; }
    return 0;
}
uint32_t rx_xml_vf_node_floats(rx_addr r, rx_addr node, rx_addr dst, uint32_t n) { return node_form(r, node, dst, n, 5); }
RX_FUNCTION(0x008287c0, rx_xml_vf_node_floats)
uint32_t rx_xml_vf_node_ints(rx_addr r, rx_addr node, rx_addr dst, uint32_t n) { return node_form(r, node, dst, n, 3); }
RX_FUNCTION(0x00828760, rx_xml_vf_node_ints)
uint32_t rx_xml_vf_node_bools(rx_addr r, rx_addr node, rx_addr dst, uint32_t n) { return node_form(r, node, dst, n, 0xb); }
RX_FUNCTION(0x008286a0, rx_xml_vf_node_bools)
uint32_t rx_xml_vf_node_string(rx_addr r, rx_addr node, rx_addr dst, uint32_t n) {
    (void)r;
    uint32_t t = U16(node, 4);
    if (t == 0) { SU8(dst, 0, 0); return 1; }
    if (t != 1) return 0;
    crt_strncpy(dst, U32(node, 8), n - 1); SU8(dst, n - 1, 0);
    return 1;
}
RX_FUNCTION(0x00828640, rx_xml_vf_node_string)
/* the attribute value forms: vf76 00829160 floats (5), vf77 00829080 ints (3), vf73 008292b0 vec3 (0xa; 16-byte stride), vf71 008293a0
 * vec4 (7) / vf70 00829400 vec4 (0xc) (or "x,y,z,w" lists, 00829870) */
static uint32_t attr_form(rx_addr r, rx_addr e, rx_addr dst, uint32_t n, uint32_t type) {
    uint32_t t = U16(e, 0xa);
    if (t != type && t != 1) return 0;
    if (type == 5) rx_xml_attr_floats(r, e, dst, (int)n);
    else if (type == 3) rx_xml_attr_ints(r, e, dst, (int)n);
    else rx_xml_attr_vec3(r, e, dst, (int)n);
    return 1;
}
uint32_t rx_xml_vf_attr_floats(rx_addr r, rx_addr e, rx_addr dst, uint32_t n) { return attr_form(r, e, dst, n, 5); }
RX_FUNCTION(0x00829160, rx_xml_vf_attr_floats)
uint32_t rx_xml_vf_attr_ints(rx_addr r, rx_addr e, rx_addr dst, uint32_t n) { return attr_form(r, e, dst, n, 3); }
RX_FUNCTION(0x00829080, rx_xml_vf_attr_ints)
uint32_t rx_xml_vf_attr_vec3(rx_addr r, rx_addr e, rx_addr dst, uint32_t n) { return attr_form(r, e, dst, n, 0xa); }
RX_FUNCTION(0x008292b0, rx_xml_vf_attr_vec3)
/* vf48 008288e0 (node, dst, n; RET 0xc): a node's own value as n vec3 (type 0xa, 16-byte stride) or an "x,y,z" list (type 1): 1;
 * other types 0 */
uint32_t rx_xml_vf_node_vec3(rx_addr r, rx_addr node, rx_addr dst, uint32_t n) { return (uint32_t)rx_xml_node_vec3(r, node, dst, (int)n); }
RX_FUNCTION(0x008288e0, rx_xml_vf_node_vec3)
/* 00829870 (EAX = string, ECX = dst, s0 n): n "x,y,z,w" vectors (atof, 16 bytes each); 0 when a comma is missing */
static int parse_vec4_list(rx_addr str, rx_addr dst, int n) {
    char buf[0x400]; gets_game(str, buf, sizeof buf);
    const char* p = buf;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < 4; j++) {
            SF32(dst + 16u * (uint32_t)i, 4u * (uint32_t)j, (float)atof(p));
            const char* c = strchr(p, ',');
            if (!c) { if (j == 3 && i == n - 1) return 1; return 0; }
            p = c + 1;
        }
    }
    return 1;
}
static uint32_t attr_vec4(rx_addr r, rx_addr e, rx_addr dst, uint32_t n, uint32_t type) {
    uint32_t t = U16(e, 0xa);
    if (t == type) { for (uint32_t k = 0; k < 4 * n; k++) SU32(dst, 4 * k, U32(U32(e, 4), 4 * k)); return 1; }
    if (t == 1) { parse_vec4_list(str_ptr(r, U32(e, 4)), dst, (int)n); return 1; }
    return 0;
}
uint32_t rx_xml_vf_attr_vec4a(rx_addr r, rx_addr e, rx_addr dst, uint32_t n) { return attr_vec4(r, e, dst, n, 7); }
RX_FUNCTION(0x008293a0, rx_xml_vf_attr_vec4a)
uint32_t rx_xml_vf_attr_vec4b(rx_addr r, rx_addr e, rx_addr dst, uint32_t n) { return attr_vec4(r, e, dst, n, 0xc); }
RX_FUNCTION(0x00829400, rx_xml_vf_attr_vec4b)
/* 00829920 (EAX = dst, stack: n, string; RET 8): n "x,y,z,..." groups of 16 floats (a 4x4 matrix, 64 bytes each); after every
 * float a comma is required, except after the last float of the last matrix: 1, else 0 (the prefix stays written). atof is the
 * host's (with the rest of the reader's text forms: plan/QUEUE.md "Sweep (from build12a)"). */
static int parse_mat4_list(rx_addr str, rx_addr dst, int n) {
    char buf[0x400]; gets_game(str, buf, sizeof buf);
    const char* p = buf;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < 16; j++) {
            SF32(dst + 64u * (uint32_t)i, 4u * (uint32_t)j, (float)atof(p));
            const char* c = strchr(p, ',');
            if (!c) return (i == n - 1 && j == 15) ? 1 : 0;
            p = c + 1;
        }
    }
    return 1;
}
/* vf69 00829460 (element, dst, n; RET 0xc): n 4x4 matrices: type 8 copies n * 64 bytes from the element's value (+4, a pointer);
 * type 1 reads n comma-separated 16-float groups (00829920); 1 in both cases, 0 for another type. */
uint32_t rx_xml_vf_attr_mat4(rx_addr r, rx_addr e, rx_addr dst, uint32_t n) {
    const uint32_t t = U16(e, 0xa);
    if (t == 8) {
        const rx_addr src = U32(e, 4);
        for (uint32_t k = 0; k < 16u * n; k++) SU32(dst, 4u * k, U32(src, 4u * k));
        return 1;
    }
    if (t == 1) { (void)parse_mat4_list(str_ptr(r, U32(e, 4)), dst, (int)n); return 1; }
    return 0;
}
RX_FUNCTION(0x00829460, rx_xml_vf_attr_mat4)
