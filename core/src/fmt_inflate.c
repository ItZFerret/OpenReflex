/* fmt_inflate.c -- deflate decoder: canonical Huffman codes with a 10-bit lookup table and a bit-serial fallback for the
 * longer codes (the method of zlib's puff.c, made table driven). */
#include "fmt_inflate.h"

#include <stdlib.h>

#define FAST_BITS 10

typedef struct {
    uint16_t count[16];            /* codes of each length */
    uint16_t symbol[288];          /* symbols ordered by (length, value) */
    uint16_t fast[1 << FAST_BITS]; /* (length << 12) | symbol for codes up to FAST_BITS long, indexed by the next bits LSB first; 0 = slow */
} huff;

typedef struct {
    const uint8_t* in; size_t pos, len;
    uint64_t bits; int nbits;
    int overrun;
} bitrd;

static void refill(bitrd* b)
{
    while (b->nbits <= 56) {
        if (b->pos < b->len) b->bits |= (uint64_t)b->in[b->pos++] << b->nbits;
        else if (b->overrun >= 16) return;                       /* ran dry: zero bytes (counted), the decode fails on them */
        else b->overrun++;
        b->nbits += 8;
    }
}

static uint32_t take(bitrd* b, int n)
{
    if (b->nbits < n) refill(b);
    uint32_t v = (uint32_t)(b->bits & ((1ull << n) - 1));
    b->bits >>= n; b->nbits -= n;
    return v;
}

static int build(huff* h, const uint8_t* length, int n)
{
    int offs[16];
    memset(h, 0, sizeof *h);
    for (int s = 0; s < n; s++) h->count[length[s]]++;
    if (h->count[0] == n) return 0;                              /* no codes: fine until a symbol is decoded */
    int left = 1;
    for (int len = 1; len < 16; len++) {
        left <<= 1; left -= h->count[len];
        if (left < 0) return -1;                                 /* over-subscribed */
    }
    offs[1] = 0;
    for (int len = 1; len < 15; len++) offs[len + 1] = offs[len] + h->count[len];
    for (int s = 0; s < n; s++) if (length[s]) h->symbol[offs[length[s]]++] = (uint16_t)s;
    /* fast table */
    int code = 0, idx = 0;
    for (int len = 1; len <= 15; len++) {
        for (int i = 0; i < h->count[len]; i++, idx++, code++) {
            if (len > FAST_BITS) continue;
            int rev = 0;
            for (int k = 0; k < len; k++) if (code >> k & 1) rev |= 1 << (len - 1 - k);
            for (int j = rev; j < (1 << FAST_BITS); j += 1 << len) h->fast[j] = (uint16_t)(len << 12 | h->symbol[idx]);
        }
        code <<= 1;
    }
    return left > 0 && h->count[0] != n ? (left) : 0;            /* >0: incomplete (allowed for single-code distance trees) */
}

static int decode(bitrd* b, const huff* h)
{
    if (b->nbits < 16) refill(b);
    unsigned e = h->fast[b->bits & ((1u << FAST_BITS) - 1)];
    if (e) {
        int len = (int)(e >> 12);
        b->bits >>= len; b->nbits -= len;
        return (int)(e & 0xfff);
    }
    int code = 0, first = 0, index = 0;
    uint64_t bits = b->bits;
    for (int len = 1; len <= 15; len++) {
        code |= (int)(bits & 1); bits >>= 1;
        int count = h->count[len];
        if (code - count < first) {
            b->bits >>= len; b->nbits -= len;
            return h->symbol[index + (code - first)];
        }
        index += count; first += count; first <<= 1; code <<= 1;
    }
    return -1;
}

static const uint16_t LBASE[29] = { 3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258 };
static const uint8_t  LEXT[29] = { 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0 };
static const uint16_t DBASE[30] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577 };
static const uint8_t  DEXT[30] = { 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13 };

static int codes(bitrd* b, uint8_t* out, size_t out_len, size_t* op, const huff* lit, const huff* dist)
{
    for (;;) {
        int sym = decode(b, lit);
        if (sym < 0 || b->overrun >= 16) return -1;
        if (sym < 256) {
            if (*op >= out_len) return -1;
            out[(*op)++] = (uint8_t)sym;
        } else if (sym == 256) return 0;
        else {
            sym -= 257;
            if (sym >= 29) return -1;
            size_t len = LBASE[sym] + take(b, LEXT[sym]);
            int ds = decode(b, dist);
            if (ds < 0 || ds >= 30) return -1;
            size_t d = DBASE[ds] + take(b, DEXT[ds]);
            if (d > *op || len > out_len - *op) return -1;
            uint8_t* dst = out + *op;
            const uint8_t* src = dst - d;
            for (size_t i = 0; i < len; i++) dst[i] = src[i];
            *op += len;
        }
    }
}

int fmt_inflate(const uint8_t* in, size_t in_len, uint8_t* out, size_t out_len, size_t* consumed)
{
    bitrd b = { in, 0, in_len, 0, 0, 0 };
    size_t op = 0;
    huff *lit = (huff*)fmt_alloc(sizeof(huff) * 2), *dist = lit ? lit + 1 : NULL;
    if (!lit) return -1;
    int last, rc = 0;
    do {
        last = (int)take(&b, 1);
        int type = (int)take(&b, 2);
        if (type == 0) {
            int drop = b.nbits & 7;
            b.bits >>= drop; b.nbits -= drop;
            uint32_t len = take(&b, 16), nlen = take(&b, 16);
            if ((len ^ 0xffff) != nlen) { rc = fmt_fail("deflate: stored block length check"); break; }
            while (len && b.nbits >= 8) { if (op >= out_len) { rc = fmt_fail("deflate: output overflow"); break; } out[op++] = (uint8_t)take(&b, 8); len--; }
            if (rc) break;
            if (len) {
                size_t at = b.pos;                               /* the bit buffer is empty now: continue from the byte stream */
                if (len > in_len - at || len > out_len - op) { rc = fmt_fail("deflate: stored block overrun"); break; }
                memcpy(out + op, in + at, len); op += len; b.pos += len;
            }
        } else if (type == 1) {
            uint8_t l[288 + 32];
            int i = 0;
            for (; i < 144; i++) l[i] = 8;
            for (; i < 256; i++) l[i] = 9;
            for (; i < 280; i++) l[i] = 7;
            for (; i < 288; i++) l[i] = 8;
            build(lit, l, 288);
            for (i = 0; i < 30; i++) l[i] = 5;
            build(dist, l, 30);
            if (codes(&b, out, out_len, &op, lit, dist) < 0) { rc = fmt_fail("deflate: bad data in a fixed block"); break; }
        } else if (type == 2) {
            static const uint8_t order[19] = { 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 };
            uint8_t l[320];
            int nlen = (int)take(&b, 5) + 257, ndist = (int)take(&b, 5) + 1, ncode = (int)take(&b, 4) + 4;
            if (nlen > 286 || ndist > 30) { rc = fmt_fail("deflate: too many codes"); break; }
            int i;
            for (i = 0; i < 19; i++) l[i] = 0;
            for (i = 0; i < ncode; i++) l[order[i]] = (uint8_t)take(&b, 3);
            if (build(lit, l, 19) != 0) { rc = fmt_fail("deflate: bad code length code"); break; }
            for (i = 0; i < nlen + ndist;) {
                int sym = decode(&b, lit);
                if (sym < 0) { rc = fmt_fail("deflate: bad code length"); break; }
                if (sym < 16) l[i++] = (uint8_t)sym;
                else {
                    int rep, val = 0;
                    if (sym == 16) { if (i == 0) { rc = fmt_fail("deflate: repeat without a length"); break; } val = l[i - 1]; rep = 3 + (int)take(&b, 2); }
                    else if (sym == 17) rep = 3 + (int)take(&b, 3);
                    else rep = 11 + (int)take(&b, 7);
                    if (i + rep > nlen + ndist) { rc = fmt_fail("deflate: too many lengths"); break; }
                    while (rep--) l[i++] = (uint8_t)val;
                }
            }
            if (rc) break;
            if (l[256] == 0) { rc = fmt_fail("deflate: no end-of-block code"); break; }
            int r = build(lit, l, nlen);
            if (r < 0 || (r > 0 && nlen - lit->count[0] != 1)) { rc = fmt_fail("deflate: bad literal/length code"); break; }
            r = build(dist, l + nlen, ndist);
            if (r < 0 || (r > 0 && ndist - dist->count[0] != 1)) { rc = fmt_fail("deflate: bad distance code"); break; }
            if (codes(&b, out, out_len, &op, lit, dist) < 0) { rc = fmt_fail("deflate: bad data in a dynamic block"); break; }
        } else { rc = fmt_fail("deflate: bad block type"); break; }
        if (b.nbits < 8 * b.overrun) { rc = fmt_fail("deflate: truncated input"); break; }
    } while (!last);
    free(lit);
    if (rc) return rc;
    if (op != out_len) return fmt_fail("deflate: produced %llu bytes, expected %llu", (unsigned long long)op, (unsigned long long)out_len);
    if (consumed) {
        /* bytes still unread in the bit buffer go back */
        size_t unread = (size_t)(b.nbits / 8) - (size_t)b.overrun;     /* whole real bytes still in the bit buffer */
        *consumed = b.pos - unread;
    }
    return 0;
}

uint32_t fmt_adler32(const uint8_t* d, size_t n)
{
    uint32_t a = 1, b = 0;
    while (n) {
        size_t k = n > 5552 ? 5552 : n;
        n -= k;
        while (k--) { a += *d++; b += a; }
        a %= 65521; b %= 65521;
    }
    return b << 16 | a;
}

int fmt_zlib_decode(const uint8_t* in, size_t in_len, uint8_t* out, size_t out_len)
{
    if (in_len < 6) return fmt_fail("zlib: stream too short");
    if ((in[0] & 15) != 8 || ((in[0] << 8 | in[1]) % 31) != 0 || (in[1] & 0x20)) return fmt_fail("zlib: bad header");
    size_t used = 0;
    if (fmt_inflate(in + 2, in_len - 2, out, out_len, &used) < 0) return -1;
    if (used + 2 + 4 != in_len) return fmt_fail("zlib: %llu bytes of trailing data", (unsigned long long)(in_len - used - 6));
    uint32_t want = (uint32_t)in[in_len - 4] << 24 | (uint32_t)in[in_len - 3] << 16 | (uint32_t)in[in_len - 2] << 8 | in[in_len - 1];
    if (fmt_adler32(out, out_len) != want) return fmt_fail("zlib: Adler-32 mismatch");
    return 0;
}
