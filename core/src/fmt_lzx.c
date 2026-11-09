/* fmt_lzx.c -- see fmt_lzx.h. */
#include "fmt_lzx.h"

#include <stdlib.h>

#define NUM_CHARS 256
#define MIN_MATCH 2
#define NUM_PRIMARY_LENGTHS 7
#define NUM_SECONDARY_LENGTHS 249
#define PRETREE_SYMS 20
#define ALIGNED_SYMS 8
#define MAX_MAIN (NUM_CHARS + 50 * 8)
#define FRAME_SIZE 32768
#define FAST 12                      /* bits of the first-level decode table */

enum { BLOCK_NONE = 0, BLOCK_VERBATIM = 1, BLOCK_ALIGNED = 2, BLOCK_UNCOMPRESSED = 3 };

/* ------------------------------------------------------------------------------------------------ Huffman */

typedef struct {
    int nsyms;
    uint16_t fast[1 << FAST];        /* (length << 10) | symbol, 0 = longer than FAST bits or invalid */
    uint16_t count[17];
    uint16_t symbol[MAX_MAIN];       /* symbols in canonical order */
    int empty;
} lzx_tree;

/* Canonical code assignment: shorter codes first, equal lengths in symbol order. 0 ok, -1 over-subscribed / incomplete. */
static int tree_build(lzx_tree* t, const uint8_t* len, int nsyms)
{
    int offs[18];
    memset(t->count, 0, sizeof t->count);
    t->nsyms = nsyms;
    for (int s = 0; s < nsyms; s++) t->count[len[s]]++;
    t->empty = t->count[0] == nsyms;
    memset(t->fast, 0, sizeof t->fast);
    if (t->empty) return 0;
    int left = 1;
    for (int l = 1; l <= 16; l++) { left <<= 1; left -= t->count[l]; if (left < 0) return -1; }
    if (left != 0) return -1;
    offs[1] = 0;
    for (int l = 1; l <= 16; l++) offs[l + 1] = offs[l] + t->count[l];
    for (int s = 0; s < nsyms; s++) if (len[s]) t->symbol[offs[len[s]]++] = (uint16_t)s;
    uint32_t code = 0;
    int idx = 0;
    for (int l = 1; l <= 16; l++) {
        for (int i = 0; i < t->count[l]; i++, idx++, code++) {
            if (l > FAST) continue;
            uint32_t first = code << (FAST - l), n = 1u << (FAST - l);
            uint16_t e = (uint16_t)(l << 10 | t->symbol[idx]);
            for (uint32_t k = 0; k < n; k++) t->fast[first + k] = e;
        }
        code <<= 1;
    }
    return 0;
}

/* ------------------------------------------------------------------------------------------------ state */

struct fmt_lzx {
    int window_bits;
    uint8_t* window;
    uint32_t window_size, window_posn;
    uint32_t R0, R1, R2;
    int main_elements;
    int header_read, block_type;
    uint32_t block_length, block_remaining;
    uint32_t frames_read;
    uint32_t overrun;                 /* bytes the last frame's final match wrote past its end: the start of the next frame */
    int32_t intel_filesize, intel_curpos;
    int intel_started;
    uint8_t* e8_buf;          /* one frame, the E8-translated copy */
    uint8_t pretree_len[PRETREE_SYMS], maintree_len[MAX_MAIN], length_len[NUM_SECONDARY_LENGTHS], aligned_len[ALIGNED_SYMS];
    lzx_tree pretree, maintree, length, aligned;
    /* input */
    const uint8_t* ip; const uint8_t* ie;
    uint32_t bitbuf; int bits_left;
    int padded;                       /* zero words read past the input: one is the format's final alignment; the community's track tools can cut the
                                       * last match's bits off a chunk (2019 Daytona's "green lights"): read as zeros */
    int bad;
};

static uint32_t g_position_base[51];
static uint8_t g_extra_bits[52];
static int g_tables_ready;

static void init_tables(void)
{
    uint8_t eb[52];
    uint32_t pb[51];
    for (int i = 0, j = 0; i <= 50; i += 2) {
        eb[i] = eb[i + 1] = (uint8_t)j;
        if (i != 0 && j < 17) j++;
    }
    for (int i = 0, j = 0; i <= 50; i++) { pb[i] = (uint32_t)j; j += 1 << eb[i]; }
    memcpy(g_extra_bits, eb, sizeof eb);
    memcpy(g_position_base, pb, sizeof pb);
    g_tables_ready = 1;                                          /* idempotent: concurrent first calls write the same values */
}

fmt_lzx* fmt_lzx_new(int window_bits)
{
    if (window_bits < 15 || window_bits > 21) { fmt_fail("lzx: unsupported window size 2^%d", window_bits); return NULL; }
    if (!g_tables_ready) init_tables();
    fmt_lzx* z = (fmt_lzx*)fmt_alloc(sizeof *z);
    if (!z) return NULL;
    z->window_bits = window_bits;
    z->window_size = 1u << window_bits;
    z->window = (uint8_t*)fmt_alloc(z->window_size);
    if (!z->window) { free(z); return NULL; }
    int slots = window_bits == 20 ? 42 : window_bits == 21 ? 50 : window_bits * 2;
    z->main_elements = NUM_CHARS + slots * 8;
    fmt_lzx_reset(z);
    return z;
}

void fmt_lzx_reset(fmt_lzx* z)
{
    memset(z->window, 0xDC, z->window_size);
    z->window_posn = 0;
    z->R0 = z->R1 = z->R2 = 1;
    z->header_read = 0;
    z->block_type = BLOCK_NONE;
    z->block_length = z->block_remaining = 0;
    z->frames_read = 0;
    z->overrun = 0;
    z->intel_filesize = z->intel_curpos = 0;
    z->intel_started = 0;
    memset(z->maintree_len, 0, sizeof z->maintree_len);
    memset(z->length_len, 0, sizeof z->length_len);
    memset(z->pretree_len, 0, sizeof z->pretree_len);
    memset(z->aligned_len, 0, sizeof z->aligned_len);
}

void fmt_lzx_free(fmt_lzx* z)
{
    if (!z) return;
    free(z->window);
    free(z->e8_buf);
    free(z);
}

/* ------------------------------------------------------------------------------------------------ bit reader */

static inline void ensure(fmt_lzx* z, int n)
{
    while (z->bits_left < n) {
        uint32_t w = 0;
        if (z->ip + 2 <= z->ie) { w = z->ip[0] | (uint32_t)z->ip[1] << 8; z->ip += 2; }
        else if (z->padded < 4) { z->padded++; if (z->ip < z->ie) { w = z->ip[0]; z->ip = z->ie; } }
        else { z->bad = 1; }
        z->bitbuf |= w << (32 - 16 - z->bits_left);
        z->bits_left += 16;
    }
}

static inline void drop(fmt_lzx* z, int n) { z->bitbuf <<= n; z->bits_left -= n; }
static inline uint32_t bits(fmt_lzx* z, int n)
{
    if (!n) return 0;
    ensure(z, n);
    uint32_t v = z->bitbuf >> (32 - n);
    z->bitbuf <<= n; z->bits_left -= n;
    return v;
}

static int symbol(fmt_lzx* z, const lzx_tree* t)
{
    ensure(z, 16);
    uint32_t top = z->bitbuf >> (32 - FAST);
    unsigned e = t->fast[top];
    if (e) { drop(z, (int)(e >> 10)); return (int)(e & 1023); }
    if (t->empty) { z->bad = 1; return 0; }
    uint32_t code = 0, first = 0;
    int index = 0;
    uint32_t b16 = z->bitbuf >> 16;
    for (int l = 1; l <= 16; l++) {
        code = (code << 1) | ((b16 >> (16 - l)) & 1);
        uint32_t count = t->count[l];
        if (code - first < count) { drop(z, l); return t->symbol[index + (int)(code - first)]; }
        index += (int)count; first = (first + count) << 1;
    }
    z->bad = 1;
    return 0;
}

/* Reads delta coded code lengths for lens[first..last) with the pre-tree. */
static int read_lengths(fmt_lzx* z, uint8_t* lens, int first, int last)
{
    for (int x = 0; x < PRETREE_SYMS; x++) z->pretree_len[x] = (uint8_t)bits(z, 4);
    if (tree_build(&z->pretree, z->pretree_len, PRETREE_SYMS) < 0) return fmt_fail("lzx: bad pre-tree");
    for (int x = first; x < last;) {
        int s = symbol(z, &z->pretree), run, v;
        if (z->bad) return fmt_fail("lzx: out of input in a length table");
        if (s == 17) { run = (int)bits(z, 4) + 4; if (run > last - x) run = last - x; while (run--) lens[x++] = 0; }
        else if (s == 18) { run = (int)bits(z, 5) + 20; if (run > last - x) run = last - x; while (run--) lens[x++] = 0; }
        else if (s == 19) {
            run = (int)bits(z, 1) + 4;
            s = symbol(z, &z->pretree);
            v = lens[x] - s; if (v < 0) v += 17;
            if (run > last - x) run = last - x;
            while (run--) lens[x++] = (uint8_t)v;
        } else { v = lens[x] - s; if (v < 0) v += 17; lens[x++] = (uint8_t)v; }
    }
    return 0;
}

/* ------------------------------------------------------------------------------------------------ decoder */

/* Decodes one output frame of `frame_size` bytes into the window at window_posn. */
static int decode_frame(fmt_lzx* z, uint32_t frame_size)
{
    uint8_t* window = z->window;
    /* the community's track tools (Mods/Tracks) let a match run past the 32 KB frame end (2019 Daytona's "green lights",
     * a track that plays in the original game): the bytes past the end are already the next frame's first ones */
    uint32_t carried = z->overrun < frame_size ? z->overrun : frame_size;
    uint32_t wpos = z->window_posn + carried, todo = frame_size - carried;
    z->overrun = 0;
    uint32_t window_size = z->window_size;

    while (todo > 0) {
        if (z->block_remaining == 0) {
            if (z->block_type == BLOCK_UNCOMPRESSED && (z->block_length & 1)) {
                if (z->ip < z->ie) z->ip++;                      /* the pad byte of an odd stored block */
            }
            z->block_type = (int)bits(z, 3);
            uint32_t hi = bits(z, 16), lo = bits(z, 8);
            z->block_remaining = z->block_length = hi << 8 | lo;
            switch (z->block_type) {
            case BLOCK_ALIGNED:
                for (int i = 0; i < ALIGNED_SYMS; i++) z->aligned_len[i] = (uint8_t)bits(z, 3);
                if (tree_build(&z->aligned, z->aligned_len, ALIGNED_SYMS) < 0) return fmt_fail("lzx: bad aligned tree");
                /* fall through: the rest of the header is the verbatim one */
            case BLOCK_VERBATIM:
                if (read_lengths(z, z->maintree_len, 0, 256) < 0) return -1;
                if (read_lengths(z, z->maintree_len, 256, z->main_elements) < 0) return -1;
                if (tree_build(&z->maintree, z->maintree_len, z->main_elements) < 0) return fmt_fail("lzx: bad main tree");
                if (z->maintree_len[0xE8] != 0) z->intel_started = 1;
                if (read_lengths(z, z->length_len, 0, NUM_SECONDARY_LENGTHS) < 0) return -1;
                if (tree_build(&z->length, z->length_len, NUM_SECONDARY_LENGTHS) < 0) return fmt_fail("lzx: bad length tree");
                break;
            case BLOCK_UNCOMPRESSED: {
                z->intel_started = 1;
                if (z->bits_left == 0) ensure(z, 16);
                z->bits_left = 0; z->bitbuf = 0;
                if (z->ie - z->ip < 12) return fmt_fail("lzx: truncated stored block header");
                z->R0 = fmt_u32(z->ip); z->R1 = fmt_u32(z->ip + 4); z->R2 = fmt_u32(z->ip + 8);
                z->ip += 12;
                break;
            }
            default:
                return fmt_fail("lzx: bad block type %d", z->block_type);
            }
            if (z->bad) return fmt_fail("lzx: out of input in a block header");
        }

        int32_t run = (int32_t)(z->block_remaining < todo ? z->block_remaining : todo);
        todo -= (uint32_t)run;
        z->block_remaining -= (uint32_t)run;

        if (z->block_type == BLOCK_UNCOMPRESSED) {
            if ((size_t)(z->ie - z->ip) < (size_t)run) return fmt_fail("lzx: truncated stored block");
            memcpy(window + wpos, z->ip, (size_t)run);
            z->ip += run; wpos += (uint32_t)run;
            continue;
        }

        while (run > 0) {
            int main = symbol(z, &z->maintree);
            if (z->bad) return fmt_fail("lzx: out of input or bad code in a block");
            if (main < NUM_CHARS) { window[wpos++] = (uint8_t)main; run--; continue; }
            main -= NUM_CHARS;
            int match_length = main & NUM_PRIMARY_LENGTHS;
            if (match_length == NUM_PRIMARY_LENGTHS) {
                if (z->length.empty) return fmt_fail("lzx: match length from an empty length tree");
                match_length += symbol(z, &z->length);
            }
            match_length += MIN_MATCH;
            uint32_t match_offset = (uint32_t)(main >> 3);
            switch (match_offset) {
            case 0: match_offset = z->R0; break;
            case 1: match_offset = z->R1; z->R1 = z->R0; z->R0 = match_offset; break;
            case 2: match_offset = z->R2; z->R2 = z->R0; z->R0 = match_offset; break;
            default: {
                int extra = match_offset >= 36 ? 17 : g_extra_bits[match_offset];
                if (z->block_type == BLOCK_VERBATIM) {
                    uint32_t v = bits(z, extra);
                    match_offset = g_position_base[match_offset] - 2 + v;
                } else {                                         /* aligned offset block */
                    uint32_t base = g_position_base[match_offset] - 2;
                    if (extra > 3) {
                        uint32_t v = bits(z, extra - 3);
                        base += v << 3;
                        base += (uint32_t)symbol(z, &z->aligned);
                    } else if (extra == 3) base += (uint32_t)symbol(z, &z->aligned);
                    else if (extra > 0) base += bits(z, extra);
                    else base = 1;
                    match_offset = base;
                }
                z->R2 = z->R1; z->R1 = z->R0; z->R0 = match_offset;
            }
            }
            if (z->bad) return fmt_fail("lzx: out of input in a match");
            if (wpos + (uint32_t)match_length > window_size) return fmt_fail("lzx: match runs over the window end");
            uint8_t* dst = window + wpos;
            int i = match_length;
            if (match_offset > wpos) {                           /* the source wraps to the end of the window */
                uint32_t j = match_offset - wpos;
                if (j > window_size) return fmt_fail("lzx: match offset %u outside the window", match_offset);
                const uint8_t* src = window + window_size - j;
                int k = (int)j < i ? (int)j : i;
                i -= k;
                while (k--) *dst++ = *src++;
                src = window;
                while (i-- > 0) *dst++ = *src++;
            } else {
                const uint8_t* src = dst - match_offset;
                while (i-- > 0) *dst++ = *src++;
            }
            wpos += (uint32_t)match_length;
            run -= match_length;
        }
        if (run < 0) {                                           /* a match ran past the block end: it eats into the next one */
            /* past the block itself only where the community's track tools end a chunk (2019 Daytona): a cut-off stream (padded, see the
             * field) or a last match that runs past the block and the frame together; the bytes past the chunk are dropped */
            if ((uint32_t)(-run) > z->block_remaining && !z->padded && todo) return fmt_fail("lzx: match overruns its block");
            z->block_remaining -= (uint32_t)(-run) < z->block_remaining ? (uint32_t)(-run) : z->block_remaining;
        }
    }
    if (wpos - z->window_posn < frame_size || wpos - z->window_posn > frame_size + 257)
        return fmt_fail("lzx: decode beyond the output frame (%u != %u)", wpos - z->window_posn, frame_size);
    z->overrun = wpos - z->window_posn - frame_size;
    z->window_posn += frame_size;
    return 0;
}

int fmt_lzx_decode(fmt_lzx* z, const uint8_t* in, size_t in_len, uint8_t* out, size_t out_len)
{
    z->ip = in; z->ie = in + in_len;
    z->bitbuf = 0; z->bits_left = 0; z->padded = 0; z->bad = 0;
    if (!z->header_read) {
        uint32_t i = 0, j = 0;
        if (bits(z, 1)) { i = bits(z, 16); j = bits(z, 16); }
        z->intel_filesize = (int32_t)(i << 16 | j);
        z->header_read = 1;
    }
    size_t done = 0;
    while (done < out_len) {
        uint32_t frame = out_len - done < FRAME_SIZE ? (uint32_t)(out_len - done) : FRAME_SIZE;
        uint32_t start = z->window_posn;
        if (decode_frame(z, frame) < 0) return -1;
        /* re-align the bit stream to the 16-bit boundary the frame ends on */
        if (z->bits_left > 0) ensure(z, 16);
        if (z->bits_left & 15) drop(z, z->bits_left & 15);
        if (z->bad) return fmt_fail("lzx: out of input at the end of a frame");
        uint8_t* data = z->window + start;
        if (z->intel_started && z->intel_filesize && z->frames_read <= 32768 && frame > 10) {
            /* Intel call translation (E8): absolute call targets back to relative ones. It is applied to a COPY of the frame (libmspack's
             * e8_buf): the window keeps the untranslated bytes, which later matches copy from. */
            if (!z->e8_buf) z->e8_buf = (uint8_t*)malloc(FRAME_SIZE);
            if (!z->e8_buf) return fmt_fail("lzx: out of memory");
            memcpy(z->e8_buf, data, frame);
            data = z->e8_buf;
            uint8_t* p = data;
            uint8_t* end = data + frame - 10;
            int32_t curpos = z->intel_curpos, filesize = z->intel_filesize;
            z->intel_curpos += (int32_t)frame;
            while (p < end) {
                if (*p++ != 0xE8) { curpos++; continue; }
                int32_t abs_off = (int32_t)fmt_u32(p), rel_off;
                if (abs_off >= -curpos && abs_off < filesize) {
                    rel_off = abs_off >= 0 ? abs_off - curpos : abs_off + filesize;
                    p[0] = (uint8_t)rel_off; p[1] = (uint8_t)(rel_off >> 8); p[2] = (uint8_t)(rel_off >> 16); p[3] = (uint8_t)(rel_off >> 24);
                }
                p += 4; curpos += 5;
            }
        } else if (z->intel_started && z->intel_filesize) z->intel_curpos += (int32_t)frame;
        memcpy(out + done, data, frame);
        done += frame;
        z->frames_read++;
        if (z->window_posn >= z->window_size) z->window_posn = 0;
    }
    return 0;
}

int fmt_lzx_decode_once(int window_bits, const uint8_t* in, size_t in_len, uint8_t* out, size_t out_len)
{
    fmt_lzx* z = fmt_lzx_new(window_bits);
    if (!z) return -1;
    int rc = fmt_lzx_decode(z, in, in_len, out, out_len);
    fmt_lzx_free(z);
    return rc;
}

/* ------------------------------------------------------------------------------------------------ XMem resource */

int fmt_xmem_resource(const uint8_t* data, size_t size, size_t max_out, fmt_buf* out)
{
    out->data = NULL; out->size = 0;
    if (!max_out) max_out = (size_t)256 << 20;
    if (size < 12) return fmt_fail("xmem: resource of %llu bytes", (unsigned long long)size);
    size_t limit = fmt_u32(data);
    /* a few resources carry bytes past their container (MXUI FrontEndIntro.ENG PublisherVideo.lyr): <= */
    if (limit < 4 || limit > size) return fmt_fail("xmem: container end %llu outside %llu bytes", (unsigned long long)limit, (unsigned long long)size);
    /* the word after the container: the game's files always carry all four bytes; some of the community's track tools (Mods/Tracks
     * "2014 Monster Energy Cup" 30SecondGirl_Torso_Color2.texture, GL_PortoPotty.surface) give the resource a heap size that cuts the
     * word's zero high byte off: the missing bytes read as zeros. No word at all (limit == size): the last chunk is what its frames hold */
    const size_t tail = size - limit < 4 ? size - limit : 4;
    uint32_t last = 0;
    for (size_t i = 0; i < tail; i++) last |= (uint32_t)data[limit + i] << (8 * i);
    const int no_last = tail == 0;
    size_t cap = 0, len = 0;
    uint8_t* res = NULL;
    fmt_lzx* z = NULL;
    uint8_t* stream = NULL;
    size_t stream_cap = 0;
    size_t p = 4;
    while (p < limit) {
        if (p + 8 > limit) { fmt_fail("xmem: truncated chunk header"); goto fail; }
        uint32_t capacity = fmt_u32(data + p), packed = fmt_u32(data + p + 4);
        p += 8;
        if (!(capacity > 0 && capacity <= 65536 && packed > 0 && packed <= limit - p)) { fmt_fail("xmem: bad chunk (allocation %u, packed %u)", capacity, packed); goto fail; }
        size_t end = p + packed;
        size_t want = (end == limit && last > 0) ? last : capacity;
        /* the word after the container is the last chunk's size in the game's files; the community's track tools (Mods/Tracks) write the
         * resource's whole size there: a size no chunk can hold is the total, the last chunk then holds the rest */
        if (end == limit && last > capacity && last > len && last - len <= capacity) want = last - len;
        if (len + want > max_out) { fmt_fail("xmem: output exceeds %llu bytes", (unsigned long long)max_out); goto fail; }
        if (len + want > cap) {
            size_t ncap = cap ? cap * 2 : 65536;
            while (ncap < len + want) ncap *= 2;
            uint8_t* n = (uint8_t*)fmt_realloc(res, ncap);
            if (!n) goto fail;
            res = n; cap = ncap;
        }
        if (packed == capacity) {                                // a stored chunk
            memcpy(res + len, data + p, want < packed ? want : packed);
            len += want < packed ? want : packed;
            p = end;
            continue;
        }
        /* frames: strip their headers, one LZX stream per chunk */
        if (stream_cap < packed) {
            uint8_t* n = (uint8_t*)fmt_realloc(stream, packed);
            if (!n) goto fail;
            stream = n; stream_cap = packed;
        }
        size_t slen = 0, decoded = 0, q = p;
        for (;;) {
            size_t rest = end - q;
            int more = rest > 5;
            if (!more) {                                         // only a terminator (or less) may remain
                for (size_t i = 0; i < rest; i++) if (data[q + i]) { more = 1; break; }
            }
            if (!more) break;
            uint32_t frame_out = 32768, frame_in;
            if (data[q] == 0xFF) {
                if (q + 5 > end) { fmt_fail("xmem: truncated long frame header"); goto fail; }
                frame_out = (uint32_t)data[q + 1] << 8 | data[q + 2];
                frame_in = (uint32_t)data[q + 3] << 8 | data[q + 4];
                q += 5;
            } else {
                if (q + 2 > end) { fmt_fail("xmem: truncated frame header"); goto fail; }
                frame_in = (uint32_t)data[q] << 8 | data[q + 1];
                q += 2;
            }
            if (!(frame_out > 0 && frame_out <= 32768 && frame_in > 0 && frame_in <= end - q)) { fmt_fail("xmem: bad frame (%u -> %u)", frame_in, frame_out); goto fail; }
            memcpy(stream + slen, data + q, frame_in);
            slen += frame_in; q += frame_in; decoded += frame_out;
        }
        if (no_last && end == limit && decoded < want) want = decoded;
        if (decoded < want) { fmt_fail("xmem: frames hold %llu bytes, the chunk needs %llu", (unsigned long long)decoded, (unsigned long long)want); goto fail; }
        if (!z) { z = fmt_lzx_new(17); if (!z) goto fail; } else fmt_lzx_reset(z);
        if (fmt_lzx_decode(z, stream, slen, res + len, want) < 0) goto fail;
        len += want;
        p = end;
    }
    if (p != limit || !res) { fmt_fail("xmem: chunk boundary mismatch"); goto fail; }
    fmt_lzx_free(z); free(stream);
    out->data = res; out->size = len;
    return 0;
fail:
    fmt_lzx_free(z); free(stream); free(res);
    return -1;
}
