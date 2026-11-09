/* build_xml_stream.c -- see build_xml_stream.h. Hand transliteration of FUN_008252c0 (the XMLReader's loader from a guest
 * InputStream object), CompressedInputStream::vf1 0080fb80 and FUN_007105c0, from the disassembly.
 *
 * The BXML document (build_xml.h): a 0x20-byte header {magic "BXML", s16 version, u16 flags, strings, string bytes, data bytes,
 * values, nodes, 0}; flags 1 = everything after the header is one zlib stream (read through the reader's CompressedInputStream
 * wrapper +0x2c8), 2 = an external data section follows the document (read from the stream itself), 4 = Xbox big-endian records
 * and data (the header itself is little-endian). Then the string bytes, the data bytes, the value records (12 bytes each from
 * version 1002, 8 bytes before) and the node records (0x20 bytes).
 *
 * Frame (the original's): push ebp; ebp = esp; and esp, -8; sub esp, 0x74; push ebx / esi / edi. B = the frame bottom (ESP after
 * the pushes): B + 0x40 the header, B + 0x28 the value record read (B + 0x20 before version 1002), B + 0x60 the node record,
 * B + 0x14 the index whose address the string table insertion takes, B + 0x28 the external section header. Everything the
 * callees read by address lives there; the other locals are C locals. */
#include "build_xml_stream.h"
#include "build_alloc.h"
#include "fourwheel_xml_packet.h"
#include "guest_reader_reset.h"
#include "guest_string_hash.h"
#include "rx_vcall.h"
#include "../src/rx_os.h"
#include <stdio.h>
#include <stdlib.h>

/* ---- 007105c0: _aligned_free under the allocator lock -------------------------------------------------------------------------- */

void rx_aligned_free_locked(rx_addr p) {
    if (U8(0, 0x00da67d8) != 0) {
        const uint32_t cs = 0x00d61704;
        rx_os_import(0x0091600c, 1, &cs);   /* EnterCriticalSection */
    }
    rx_crt_aligned_free(p);                 /* [0x00916350] _aligned_free */
    if (U8(0, 0x00da67d8) != 0) {
        const uint32_t cs = 0x00d61704;
        rx_os_import(0x009160f8, 1, &cs);   /* LeaveCriticalSection */
    }
}
RX_FUNCTION(0x007105c0, rx_aligned_free_locked)

/* ---- 0080fb80 CompressedInputStream::vf1: close ------------------------------------------------------------------------------- */
/* Wrapper fields: +4 the source stream, +8 the decompressor (ZLibCompression), +0xc.. the chunk buffers, +0x10 "owns the chunks",
 * +0x14 the chunk count, +0x18 the chunk size, +0x1c the current chunk, +0x20 / +0x24 / +0x28 the feed state.
 * Frame: push ebx; push esi (and push ebp; push edi around the chunk loop). */
void rx_compressed_stream_close(rx_addr wrapper) {
    const rx_addr entry = rx_sp();
    const rx_addr frame = entry - 8;                       /* push ebx; push esi */
    if (U32(wrapper, 4) != 0) {
        /* the decompressor's vf4 (+0x10): ZLibCompression::vf4 ends the inflate state */
        rx_set_sp(frame);
        rx_vcall(U32(wrapper, 8), 0x10, 0, NULL);
    }
    if (U32(wrapper, 0x10) != 0 && I32(wrapper, 0x14) > 0) {
        const rx_addr loop_frame = frame - 8;              /* push ebp; push edi */
        for (int32_t i = 0; i < I32(wrapper, 0x14); i++) {
            const rx_addr chunk = U32(wrapper, 0xc + 4u * (uint32_t)i);
            rx_set_sp(loop_frame);
            if (U8(0, 0x00da67d8) != 0) {
                const uint32_t cs = 0x00d61704;
                rx_os_import(0x0091600c, 1, &cs);          /* EnterCriticalSection */
            }
            rx_crt_aligned_free(chunk);                    /* [0x00916350] _aligned_free */
            if (U8(0, 0x00da67d8) != 0) {
                const uint32_t cs = 0x00d61704;
                rx_os_import(0x009160f8, 1, &cs);          /* LeaveCriticalSection */
            }
        }
    }
    SU32(wrapper, 0x04, 0);
    SU32(wrapper, 0x08, 0);
    SU32(wrapper, 0x14, 0);
    SU32(wrapper, 0x18, 0);
    SU32(wrapper, 0x1c, 0);
    SU32(wrapper, 0x24, 0);
    SU32(wrapper, 0x28, 0);
    SU32(wrapper, 0x20, 0);
    rx_set_sp(entry);
}
RX_FUNCTION(0x0080fb80, rx_compressed_stream_close)

/* the source / decompressor calls of the wrapper (at the ESP `esp`) */
static uint32_t source_read(rx_addr esp, rx_addr source, rx_addr dst, uint32_t n) {
    const uint32_t args[2] = {dst, n};
    rx_set_sp(esp);
    const uint32_t r = rx_vcall(source, 8, 2, args);                /* InputStream vf2 read */
    rx_set_sp(esp);
    return r;
}
static uint32_t vcall0_at(rx_addr esp, rx_addr obj, uint32_t off) {
    rx_set_sp(esp);
    const uint32_t r = rx_vcall(obj, off, 0, NULL);
    rx_set_sp(esp);
    return r;
}
static void vcall2_at(rx_addr esp, rx_addr obj, uint32_t off, uint32_t a, uint32_t b) {
    const uint32_t args[2] = {a, b};
    rx_set_sp(esp);
    rx_vcall(obj, off, 2, args);
    rx_set_sp(esp);
}
/* the next chunk slot: (current + 1) mod the chunk count (idiv; the count is 1) */
static uint32_t next_chunk(rx_addr s) {
    const int32_t count = I32(s, 0x14);
    if (count == 0) { fprintf(stderr, "rxcore: CompressedInputStream with no chunk (the original's idiv faults)\n"); abort(); }
    return (uint32_t)((I32(s, 0x1c) + 1) % count);
}

/* 0080f920 CompressedInputStream init (ESI = the wrapper, EAX = the source, ECX = the decompressor; stack the chunk size; RET 4):
 * the decompressor's vf3 (ZLibCompression: inflateInit), one chunk buffer (_aligned_malloc 16 under the lock), the compressed
 * length (4 bytes from the source into +0x24; -1 or a failed read: 0), the first chunk read (the whole stream when shorter, +0x20
 * "the source is exhausted") and handed to the decompressor (vf9 set input); a short read hands what the source read (its vf3). */
uint32_t rx_compressed_stream_init(rx_addr s, rx_addr source, rx_addr compression, uint32_t chunk_size) {
    const rx_addr entry = rx_sp();
    SU32(s, 4, source);
    SU32(s, 8, compression);
    if (vcall0_at(entry, compression, 0xc) == 0) {               /* vf3 */
        SU32(s, 4, 0);
        SU32(s, 8, 0);
        rx_set_sp(entry);
        return 0;
    }
    const rx_addr frame = entry - 12;                               /* push ebx; push ebp; push edi */
    SU32(s, 0x14, 1);
    SU32(s, 0x18, chunk_size);
    SU32(s, 0x1c, 0);
    int32_t i = 0;
    do {
        rx_set_sp(frame);
        if (U8(0, 0x00da67d8) != 0) {
            const uint32_t cs = 0x00d61704;
            rx_os_import(0x0091600c, 1, &cs);
        }
        const rx_addr chunk = rx_crt_aligned_malloc(U32(s, 0x18), 0x10);
        if (U8(0, 0x00da67d8) != 0) {
            const uint32_t cs = 0x00d61704;
            rx_os_import(0x009160f8, 1, &cs);
        }
        SU32(s, 0xc + 4u * (uint32_t)i, chunk);
        i++;
    } while (i < I32(s, 0x14));
    SU32(s, 0x10, 1);
    if (source_read(frame, U32(s, 4), s + 0x24, 4) == 0 || U32(s, 0x24) == 0xffffffffu) {
        SU32(s, 4, 0);
        SU32(s, 8, 0);
        rx_set_sp(entry);
        return 0;
    }
    const int32_t compressed = I32(s, 0x24);
    SU32(s, 0x1c, 0);
    uint32_t fed;
    int short_read = 0;
    if (compressed < I32(s, 0x18)) {
        SU32(s, 0x20, 1);
        if (source_read(frame, U32(s, 4), U32(s, 0xc), (uint32_t)compressed) != 0) fed = U32(s, 0x24);
        else short_read = 1;
    } else {
        if (source_read(frame, U32(s, 4), U32(s, 0xc), U32(s, 0x18)) != 0) fed = U32(s, 0x18);
        else { SU32(s, 0x20, 1); short_read = 1; }
    }
    if (short_read) {
        SU32(s, 0x28, vcall0_at(frame, U32(s, 4), 0xc));            /* the source's vf3: the bytes it read */
        fed = vcall0_at(frame, U32(s, 4), 0xc);
    } else {
        SU32(s, 0x28, fed);
    }
    vcall2_at(frame, U32(s, 8), 0x24, U32(s, 0xc + 4u * U32(s, 0x1c)), fed);   /* the decompressor's vf9: the input */
    rx_set_sp(entry);
    return 1;
}
RX_FUNCTION(0x0080f920, rx_compressed_stream_init)

/* 0080fc10 CompressedInputStream::vf2 read (ECX = the wrapper; stack dst, count; RET 8): the decompressor's output set to dst / count
 * (vf7); while it has output to give (vf8): its process step vf11 (four in / out words: two locals and the caller's two argument
 * slots), a failed step ends the read (+0x20 = 1; the count less what is left, 1 when complete); when its input is used up (vf10
 * zero) and the source is not exhausted the next chunk is read (the rest of the compressed length when shorter: exhausted) and fed
 * (vf9), a short read feeding what was read. +0x2c = the count. 1 when read. Frame: sub esp, 8; push ebp / esi (/ ebx / edi). */
uint32_t rx_compressed_stream_read(rx_addr s, rx_addr dst, uint32_t count) {
    const rx_addr entry = rx_sp();
    if (count == 0) return 1;
    const rx_addr F = entry - 0x18;                                 /* the locals at entry - 8 / entry - 4 */
    const rx_addr compression = U32(s, 8);
    vcall2_at(F, compression, 0x1c, dst, count);                    /* vf7: the output */
    uint32_t result = 1;
    int more = vcall0_at(F, U32(s, 8), 0x20) != 0;                  /* vf8 */
    while (more) {
        {
            const uint32_t args[4] = {entry - 4, entry - 8, entry + 4, entry + 8};
            rx_set_sp(F);
            const uint32_t ok = rx_vcall(U32(s, 8), 0x2c, 4, args);   /* vf11: process */
            rx_set_sp(F);
            if (ok == 0) {
                SU32(s, 0x20, 1);
                const uint32_t left = vcall0_at(F, U32(s, 8), 0x20);
                const uint32_t done = count - left;
                SU32(s, 0x2c, done);
                rx_set_sp(entry);
                return done == count;
            }
        }
        if (vcall0_at(F, U32(s, 8), 0x28) == 0 && U32(s, 0x20) == 0) {   /* vf10: no input left, the source not exhausted */
            const int32_t remaining = I32(s, 0x24) - I32(s, 0x28);
            uint32_t fed;
            int short_read = 0;
            if (remaining < I32(s, 0x18)) {
                SU32(s, 0x20, 1);
                if (source_read(F, U32(s, 4), U32(s, 0xc + 4u * U32(s, 0x1c)), (uint32_t)remaining) != 0) fed = (uint32_t)remaining;
                else short_read = 1;
            } else {
                if (source_read(F, U32(s, 4), U32(s, 0xc + 4u * U32(s, 0x1c)), U32(s, 0x18)) != 0) fed = U32(s, 0x18);
                else { short_read = 2; }
            }
            if (short_read) {
                SU32(s, 0x28, U32(s, 0x28) + vcall0_at(F, U32(s, 4), 0xc));
                const uint32_t next = next_chunk(s);
                if (short_read == 2) SU32(s, 0x20, 1);
                SU32(s, 0x1c, next);
                fed = vcall0_at(F, U32(s, 4), 0xc);
            } else {
                SU32(s, 0x28, U32(s, 0x28) + fed);
                SU32(s, 0x1c, next_chunk(s));
            }
            vcall2_at(F, U32(s, 8), 0x24, U32(s, 0xc + 4u * U32(s, 0x1c)), fed);   /* vf9 */
        }
        more = vcall0_at(F, U32(s, 8), 0x20) != 0;
    }
    SU32(s, 0x2c, count);
    rx_set_sp(entry);
    return result;
}
RX_FUNCTION(0x0080fc10, rx_compressed_stream_read)

/* ---- the loader's calls (each at the original's ESP = B before its pushes) ---------------------------------------------------- */

/* the stream's vf2 read(dst, n): 1 when all n bytes were read */
static uint32_t stream_read(rx_addr B, rx_addr stream, rx_addr dst, uint32_t n) {
    const uint32_t args[2] = {dst, n};
    rx_set_sp(B);
    const uint32_t ok = rx_vcall(stream, 8, 2, args);
    rx_set_sp(B);
    return ok;
}

/* the stack pointer of a call with n pushed arguments (written to the dead stack as the pushes do): the callee's entry ESP */
static rx_addr push_args(rx_addr esp, int n, const uint32_t* args) {
    const rx_addr sp = esp - 4u * (uint32_t)n - 4u;
    for (int i = 0; i < n; i++) SU32(sp, 4u + 4u * (uint32_t)i, args[i]);
    rx_set_sp(sp);
    return sp;
}

/* a block for the document: the reader's allocator vf1 (+4) when it has one, else 00401f90 (_aligned_malloc 16 under the lock;
 * cdecl: `pending` = the bytes of earlier arguments the caller has not popped yet) */
static rx_addr reader_alloc(rx_addr B, rx_addr reader, uint32_t size, uint32_t pending) {
    const rx_addr allocator = U32(reader, 0x45c);
    rx_addr p;
    rx_set_sp(B);
    if (allocator != 0) {
        p = rx_vcall(allocator, 4, 1, &size);
    } else {
        push_args(B - pending, 1, &size);
        p = rx_tuning_buffer_alloc(size);
    }
    rx_set_sp(B);
    return p;
}

/* a block freed: the reader's allocator vf2 (+8), else 007105c0 (the inline copies of it in the later failure paths are the same) */
static void reader_free(rx_addr B, rx_addr reader, rx_addr p) {
    const rx_addr allocator = U32(reader, 0x45c);
    rx_set_sp(B);
    if (allocator != 0) {
        rx_vcall(allocator, 8, 1, &p);
    } else {
        push_args(B, 1, &p);
        rx_aligned_free_locked(p);
    }
    rx_set_sp(B);
}

/* the default allocator's _aligned_malloc(size, 16) under the lock, inline in the external section (00827712) */
static rx_addr locked_aligned_malloc(rx_addr B, uint32_t size) {
    rx_set_sp(B);
    if (U8(0, 0x00da67d8) != 0) {
        const uint32_t cs = 0x00d61704;
        rx_os_import(0x0091600c, 1, &cs);
    }
    const rx_addr p = rx_crt_aligned_malloc(size, 0x10);   /* [0x00916358] */
    if (U8(0, 0x00da67d8) != 0) {
        const uint32_t cs = 0x00d61704;
        rx_os_import(0x009160f8, 1, &cs);
    }
    return p;
}

/* the wrapper's vf1 (+4): CompressedInputStream::vf1 0080fb80 (also for an uncompressed document: it only clears the state then) */
static void close_wrapper(rx_addr B, rx_addr reader) {
    rx_set_sp(B);
    rx_vcall(reader + 0x2c8, 4, 0, NULL);
    rx_set_sp(B);
}

/* 0080f920 through its register convention (ESI = the wrapper +0x2c8, EAX = the source stream, ECX = the decompressor +0x2f8; stack
 * the chunk size 0x40000), called at B */
static uint32_t compressed_stream_init(rx_addr B, rx_addr reader, rx_addr source) {
    const uint32_t chunk_size = 0x40000;
    rx_set_sp(B - 8);                                  /* push 0x40000; call */
    SU32(B - 4, 0, chunk_size);
    const uint32_t ok = rx_compressed_stream_init(reader + 0x2c8, source, reader + 0x2f8, chunk_size);
    rx_set_sp(B);
    return ok;
}

/* ---- big-endian (Xbox) data: in-place byte swaps, as the original computes them --------------------------------------------- */

static uint32_t bswap32(uint32_t x) {
    return (x >> 24) | ((x >> 8) & 0x0000ff00u) | ((x << 8) & 0x00ff0000u) | (x << 24);
}
static uint16_t bswap16(uint16_t x) {
    return (uint16_t)((x >> 8) | (x << 8));
}
static void swap32_at(rx_addr p) {
    SU32(p, 0, bswap32(U32(p, 0)));
}
/* The 8-byte swap (00825a4b / 00826a2b): the compiler's sequence of 64-bit shifts, two of them the SIGNED __allshr. Exact
 * reversal while the high word is below 0x80000000; with its top bit set the sign fills the upper bytes, as in the original. */
static void swap64_at(rx_addr p) {
    const uint32_t lo = U32(p, 0);
    const uint32_t hi = U32(p, 4);
    uint64_t x = ((uint64_t)hi << 32) | lo;                /* the low word's bytes go to the high word, reversed */
    x = (x << 16) | (lo & 0x0000ff00u);
    x = (x << 16) | (lo & 0x00ff0000u);
    x = (x << 16) | (lo & 0xff000000u);
    x <<= 8;
    const uint32_t sign = (uint32_t)((int32_t)hi >> 31);
    int64_t y = (int64_t)(((uint64_t)(sign | (hi & 0x0000ff00u)) << 32) | hi);
    y >>= 16;                                              /* __allshr(y, 16) */
    y = (int64_t)((uint64_t)y | ((uint64_t)(hi & 0xffu) << 32));
    y >>= 8;                                               /* __allshr(y, 8) */
    const uint32_t new_lo = (uint32_t)x | (uint32_t)(uint64_t)y | ((uint32_t)((int32_t)hi >> 8) & 0x0000ff00u);
    const uint32_t new_hi = (uint32_t)(x >> 32) | (uint32_t)((uint64_t)y >> 32);
    SU32(p, 4, new_hi);
    SU32(p, 0, new_lo);
}

/* the swap of `count` elements of a value of `type` at `data` (the jump tables 0x827d78 / 0x827da0, types 3 .. 12): 4-byte words
 * for 3 int / 4 uint / 5 float / 11 bool, 2 words for 9 (vec2), 3 for 10 (vec3), 4 for 7 / 12 (vec4), 16 for 8 (matrix), one
 * 8-byte swap for 6. Types 1 (string), 2 and others are left. */
static void swap_value_data(int32_t type, rx_addr data, int32_t count) {
    uint32_t words;
    switch (type) {
    case 3: case 4: case 5: case 11: words = 1; break;
    case 9: words = 2; break;
    case 10: words = 3; break;
    case 7: case 12: words = 4; break;
    case 8: words = 16; break;
    case 6:
        for (int32_t k = 0; k < count; k++) swap64_at(data + 8u * (uint32_t)k);
        return;
    default:
        return;
    }
    for (int32_t k = 0; k < count; k++) {
        for (uint32_t w = 0; w < words; w++) swap32_at(data + 4u * (words * (uint32_t)k + w));
    }
}

/* ---- the failure exits ------------------------------------------------------------------------------------------------------- */

/* the strings, string pointers, the data block (when the document has data) and the value / node block freed and cleared */
static void free_document(rx_addr B, rx_addr reader, uint32_t data_bytes) {
    reader_free(B, reader, U32(reader, 8));
    reader_free(B, reader, U32(reader, 0xc));
    if (data_bytes != 0) reader_free(B, reader, U32(reader, 0x14));
    reader_free(B, reader, U32(reader, 0x24));
    SU32(reader, 0x08, 0);
    SU32(reader, 0x0c, 0);
    SU32(reader, 0x14, 0);
    SU32(reader, 0x24, 0);
}

/* ---- 008252c0 ------------------------------------------------------------------------------------------------------------------ */

uint32_t rx_xml_reader_load_stream(rx_addr reader, rx_addr stream) {
    const rx_addr entry = rx_sp();
    const rx_addr B = ((entry - 4) & ~7u) - 0x80;          /* push ebp; and esp, -8; sub esp, 0x74; push ebx / esi / edi */
    const rx_addr header = B + 0x40;
    rx_set_sp(B);

    /* 008252d4: 00827e00 releases what the reader held (ESI = reader) */
    push_args(B, 0, NULL);
    rx_guest_reader_reset(reader);
    rx_set_sp(B);

    /* 008252e7: the 0x20-byte header from the stream itself */
    if (stream_read(B, stream, header, 0x20) == 0) {
        rx_set_sp(entry);
        return 0;
    }
    const int16_t version = (int16_t)U16(header, 4);
    const uint16_t flags = U16(header, 6);
    const int32_t string_count = I32(header, 8);
    const uint32_t string_bytes = U32(header, 0xc);
    const uint32_t data_bytes = U32(header, 0x10);
    const int32_t value_count = I32(header, 0x14);
    const int32_t node_count = I32(header, 0x18);
    const int big_endian = (flags & 4) != 0;

    /* 008252ed: a compressed document is read through the wrapper (0080f920 binds it to the stream) */
    rx_addr source;
    if ((flags & 1) != 0) {
        source = reader + 0x2c8;
        SU32(B, 0x18, source);
        if (compressed_stream_init(B, reader, stream) == 0) {
            rx_set_sp(entry);
            return 0;
        }
    } else {
        source = stream;
        SU32(B, 0x18, source);
    }

    /* 00825323: the string bytes and the string pointer table */
    SU32(reader, 8, reader_alloc(B, reader, string_bytes, 0));
    SU32(reader, 0xc, reader_alloc(B, reader, (uint32_t)string_count * 4u, 4));   /* (default: the first push not popped yet) */
    if (stream_read(B, source, U32(reader, 8), string_bytes) == 0) {
        reader_free(B, reader, U32(reader, 8));
        reader_free(B, reader, U32(reader, 0xc));
        SU32(reader, 8, 0);
        SU32(reader, 0xc, 0);
        close_wrapper(B, reader);
        rx_set_sp(entry);
        return 0;
    }
    /* 008253f5: string 0 starts the bytes, each later one follows a terminating zero */
    SU32(U32(reader, 0xc), 0, U32(reader, 8));
    SU32(reader, 4, 1);
    if (string_count > 1) {
        uint32_t i = 0;
        do {
            const rx_addr p = U32(reader, 8) + i;
            if (U8(p, 0) == 0) {
                SU32(U32(reader, 0xc), 4u * U32(reader, 4), p + 1);
                SU32(reader, 4, U32(reader, 4) + 1);
            }
            i++;
        } while (I32(reader, 4) < string_count);
    }

    /* 0082543b: the data bytes */
    if (data_bytes != 0) {
        SU32(reader, 0x18, data_bytes);
        SU32(reader, 0x14, reader_alloc(B, reader, data_bytes, 0));
        if (stream_read(B, source, U32(reader, 0x14), data_bytes) == 0) {
            reader_free(B, reader, U32(reader, 8));
            reader_free(B, reader, U32(reader, 0xc));
            reader_free(B, reader, U32(reader, 0x14));
            SU32(reader, 0x14, 0);
            SU32(reader, 0xc, 0);
            SU32(reader, 8, 0);
            close_wrapper(B, reader);
            rx_set_sp(entry);
            return 0;
        }
    }

    /* 008254fd: one block for the values (12 bytes) and the nodes (0x24 bytes = 3 * 12) */
    SU32(reader, 0x30, (uint32_t)value_count);
    SU32(reader, 0x28, (uint32_t)node_count);
    {
        const uint32_t block = reader_alloc(B, reader, ((uint32_t)value_count + (uint32_t)node_count * 3u) * 12u, 0);
        SU32(reader, 0x24, block);
        SU32(reader, 0x34, block);
        SU32(reader, 0x2c, block + U32(reader, 0x30) * 12u);
    }

    /* 0082554d: the value records */
    if (I32(reader, 0x30) > 0) {
        if (version < 0x3ea) {
            /* 0082577b: before version 1002 a value is 8 bytes {name, string index}: every value a string */
            for (int32_t i = 0; i < value_count; i++) {
                if (stream_read(B, source, B + 0x20, 8) == 0) {
                    free_document(B, reader, data_bytes);
                    close_wrapper(B, reader);
                    rx_set_sp(entry);
                    return 0;
                }
                const rx_addr e = U32(reader, 0x34) + 12u * (uint32_t)i;
                SU32(e, 0, U32(B, 0x20));
                SU32(e, 4, U32(B, 0x24));
                SU16(e, 8, 0);
                SU16(e, 0xa, 1);
            }
        } else {
            /* 00825570: {u32 name, u32 value / string index / data offset, u16 count, u16 type} */
            for (int32_t i = 0; i < value_count; i++) {
                if (stream_read(B, source, B + 0x28, 12) == 0) {
                    reader_free(B, reader, U32(reader, 8));
                    reader_free(B, reader, U32(reader, 0xc));
                    if (data_bytes != 0) reader_free(B, reader, U32(reader, 0x14));
                    reader_free(B, reader, U32(reader, 0x24));
                    SU32(reader, 0x24, 0);
                    SU32(reader, 0x14, 0);
                    SU32(reader, 0xc, 0);
                    SU32(reader, 8, 0);
                    close_wrapper(B, reader);
                    rx_set_sp(entry);
                    return 0;
                }
                uint32_t name = U32(B, 0x28);
                if (big_endian) {
                    name = bswap32(name);
                    SU32(B, 0x28, name);
                    SU32(B, 0x2c, bswap32(U32(B, 0x2c)));
                    SU16(B, 0x30, bswap16(U16(B, 0x30)));
                    SU16(B, 0x32, bswap16(U16(B, 0x32)));
                }
                const rx_addr e = U32(reader, 0x34) + 12u * (uint32_t)i;
                const uint16_t type = U16(B, 0x32);
                SU32(e, 0, name);
                SU16(e, 8, U16(B, 0x30));
                SU16(e, 0xa, type);
                if (type == 1) {
                    SU32(e, 4, U32(B, 0x2c));                          /* a string index */
                } else if (type == 0) {
                    SU32(e, 4, 0);
                    SU16(e, 8, 0);
                } else {
                    SU32(e, 4, U32(reader, 0x14) + U32(B, 0x2c));      /* into the data bytes */
                }
            }
        }
    }

    /* 008256b2: big-endian value data swapped in place (the loop runs over the values either way) */
    for (int32_t i = 0; i < value_count; i++) {
        const rx_addr e = U32(reader, 0x34) + 12u * (uint32_t)i;
        if (big_endian) swap_value_data((int16_t)U16(e, 0xa), U32(e, 4), (int32_t)U16(e, 8));
    }

    /* 008263fa: the node records {name, value / string index / data offset, count, u16 type, u16 flags, first child, children,
     * first attribute, attributes} */
    for (int32_t i = 0; i < node_count; i++) {
        const rx_addr rec = B + 0x60;
        if (stream_read(B, source, rec, 0x20) == 0) {
            free_document(B, reader, data_bytes);
            close_wrapper(B, reader);
            rx_set_sp(entry);
            return 0;
        }
        if (big_endian) {
            SU32(rec, 0x00, bswap32(U32(rec, 0x00)));
            SU32(rec, 0x04, bswap32(U32(rec, 0x04)));
            SU32(rec, 0x08, bswap32(U32(rec, 0x08)));
            SU16(rec, 0x0c, bswap16(U16(rec, 0x0c)));
            SU16(rec, 0x0e, bswap16(U16(rec, 0x0e)));
            SU32(rec, 0x10, bswap32(U32(rec, 0x10)));
            SU32(rec, 0x14, bswap32(U32(rec, 0x14)));
            SU32(rec, 0x18, bswap32(U32(rec, 0x18)));
            SU32(rec, 0x1c, bswap32(U32(rec, 0x1c)));
        }
        const rx_addr n = U32(reader, 0x2c) + 0x24u * (uint32_t)i;
        const uint32_t value = U32(rec, 0x04);
        const uint32_t count = U32(rec, 0x08);
        const uint16_t type = U16(rec, 0x0c);
        const uint16_t node_flags = U16(rec, 0x0e);
        SU32(n, 0, U32(U32(reader, 0xc), 4u * U32(rec, 0x00)));        /* the name's string */
        SU16(n, 6, node_flags);
        if ((node_flags & 1) != 0) {                                     /* external data: raw until the section is read */
            SU16(n, 4, type);
            SU32(n, 0xc, count);
            SU32(n, 8, value);
        }
        if (count == 0) {
            SU32(n, 0xc, 0);
            SU32(n, 8, 0);
            if (type == 1) {
                SU16(n, 4, 1);
                SU32(n, 8, U32(U32(reader, 0xc), 4u * value));         /* the node's own string */
            } else {
                SU16(n, 4, 0);
                SU32(n, 8, 0);
            }
        } else {
            SU16(n, 4, type);
            SU32(n, 0xc, count);
            SU32(n, 8, U32(reader, 0x14) + value);
        }
        SU32(n, 0x18, U32(rec, 0x14));
        SU32(n, 0x14, U32(reader, 0x2c) + 0x24u * U32(rec, 0x10));
        for (int32_t k = 0; k < I32(n, 0x18); k++) {
            SU32(U32(n, 0x14), 0x10 + 0x24u * (uint32_t)k, n);         /* the children's parent */
        }
        SU32(n, 0x20, U32(rec, 0x1c));
        SU32(n, 0x1c, U32(reader, 0x34) + 12u * U32(rec, 0x18));
    }
    SU32(reader, 0x38, U32(reader, 0x2c));
    SU32(reader, 0x3c, U32(reader, 0x2c));

    /* 0082670a: big-endian node data swapped in place */
    for (int32_t i = 0; i < node_count; i++) {
        const rx_addr n = U32(reader, 0x2c) + 0x24u * (uint32_t)i;
        if (big_endian) swap_value_data((int16_t)U16(n, 4), U32(n, 8), I32(n, 0xc));
    }

    /* 008273c4: the string hash table (0x20 bytes: 2 * strings buckets, load 0.75) */
    {
        rx_addr table = 0;
        const rx_addr allocator = U32(reader, 0x45c);
        if (allocator != 0) {
            uint32_t size = 0x20;
            rx_set_sp(B);
            const rx_addr t = rx_vcall(allocator, 4, 1, &size);
            if (t != 0) {
                const uint32_t args[2] = {0x3f400000u /* 0.75f, fld / fstp of 0x935aa8 */, 0};
                push_args(B, 2, args);
                table = rx_guest_string_hash_construct(t, allocator, 2u * U32(reader, 4), args[0], args[1]);
            }
        } else {
            const uint32_t table_size = 0x20, allocator_size = 4;
            push_args(B, 1, &table_size);
            const rx_addr t = rx_reflex_aligned_alloc(table_size);          /* 0078fbe0 */
            rx_set_sp(B);
            if (t != 0) {
                push_args(B, 1, &allocator_size);
                rx_addr own_allocator = rx_reflex_aligned_alloc(allocator_size);
                rx_set_sp(B);
                if (own_allocator != 0) SU32(own_allocator, 0, 0x009753b0);   /* a StdMemoryAllocator the table owns */
                const uint32_t args[2] = {0x3f400000u /* 0.75f */, 1};
                push_args(B, 2, args);
                table = rx_guest_string_hash_construct(t, own_allocator, 2u * U32(reader, 4), args[0], args[1]);
            }
        }
        rx_set_sp(B);
        SU32(reader, 0x10, table);
    }
    /* 0082743b: every string -> its index (the index is passed by address: B + 0x14) */
    SU32(B, 0x14, 0);
    for (int32_t i = 0; i < I32(reader, 4); i++) {
        const uint32_t args[2] = {U32(reader, 0xc) + 4u * (uint32_t)i, B + 0x14};   /* &strings[i], &index */
        push_args(B, 2, args);
        rx_guest_string_hash_insert(U32(reader, 0x10), args[0], args[1]);       /* 008434f0 (EDI = the table) */
        rx_set_sp(B);
        SU32(B, 0x14, (uint32_t)i + 1);
    }

    /* 0082746e: the wrapper closed */
    close_wrapper(B, reader);

    /* 0082747f: the external data section (from the stream itself) */
    if ((flags & 2) != 0) {
        const rx_addr ext = B + 0x28;
        if (stream_read(B, stream, ext, 0x14) == 0) {
            free_document(B, reader, data_bytes);
            rx_set_sp(entry);
            return 0;
        }
        if (big_endian) {
            SU32(ext, 0x00, bswap32(U32(ext, 0x00)));
            SU32(ext, 0x04, bswap32(U32(ext, 0x04)));
            SU32(ext, 0x08, bswap32(U32(ext, 0x08)));
            SU32(ext, 0x0c, bswap32(U32(ext, 0x0c)));
            SU16(ext, 0x10, bswap16(U16(ext, 0x10)));
        }
        const uint32_t ext_size = U32(ext, 0x10);                        /* the whole word (only its low half swapped) */
        const int32_t table_count = I32(ext, 0x08);
        SU32(reader, 0x20, ext_size);
        rx_addr offsets;                                                 /* table_count words, read, swapped and freed */
        {
            const rx_addr allocator = U32(reader, 0x45c);
            if (allocator != 0) {
                uint32_t size = ext_size;
                rx_set_sp(B);
                SU32(reader, 0x1c, rx_vcall(allocator, 4, 1, &size));
                size = (uint32_t)table_count * 4u;
                rx_set_sp(B);
                offsets = rx_vcall(allocator, 4, 1, &size);
            } else {
                SU32(reader, 0x1c, locked_aligned_malloc(B, ext_size));
                offsets = locked_aligned_malloc(B, (uint32_t)table_count * 4u);
            }
        }
        SU32(B, 0x1c, offsets);
        uint32_t ok = stream_read(B, stream, offsets, (uint32_t)table_count * 4u);
        SU32(B, 0x18, ok);
        if (ok == 0) {
            reader_free(B, reader, U32(reader, 8));
            reader_free(B, reader, U32(reader, 0xc));
            if (data_bytes != 0) reader_free(B, reader, U32(reader, 0x14));
            reader_free(B, reader, U32(reader, 0x24));
            reader_free(B, reader, U32(reader, 0x1c));
            reader_free(B, reader, offsets);
            SU32(reader, 0x08, 0);
            SU32(reader, 0x0c, 0);
            SU32(reader, 0x14, 0);
            SU32(reader, 0x24, 0);
            rx_set_sp(entry);
            return 0;
        }
        for (int32_t k = 0; k < table_count; k++) {
            if (big_endian) swap32_at(offsets + 4u * (uint32_t)k);
        }
        /* 008279cf: each node with external data (flags bit 0) reads it into the section, in node order; bits 1..3 = that node's
         * data is compressed (the wrapper bound to the stream for it) */
        uint32_t offset = 0;
        for (int32_t i = 0; i < node_count; i++) {
            const rx_addr n = U32(reader, 0x2c) + 0x24u * (uint32_t)i;
            const uint16_t node_flags = U16(n, 6);
            if ((node_flags & 1) == 0) continue;
            rx_addr from = stream;
            if ((node_flags & 0xe) != 0) {
                compressed_stream_init(B, reader, stream);              /* the result is not checked */
                from = reader + 0x2c8;
            }
            SU32(n, 8, U32(reader, 0x1c) + offset);
            SU16(n, 6, (uint16_t)(U16(n, 6) & 0xfffe));
            uint32_t element = 0;
            switch ((int16_t)U16(n, 4)) {
            case 1: case 2: element = 1; break;
            case 3: case 4: case 5: case 11: element = 4; break;
            case 6: case 9: element = 8; break;
            case 7: case 12: element = 16; break;
            case 8: element = 64; break;
            case 10: element = 12; break;
            default: element = 0; break;                                  /* no read: the last result stands */
            }
            if (element != 0) {
                ok = stream_read(B, from, U32(n, 8), U32(n, 0xc) * element);
                SU32(B, 0x18, ok);
                offset += U32(n, 0xc) * element;
            }
            close_wrapper(B, reader);
            if (U32(B, 0x18) == 0) {
                reader_free(B, reader, U32(reader, 8));
                reader_free(B, reader, U32(reader, 0xc));
                if (data_bytes != 0) reader_free(B, reader, U32(reader, 0x14));
                reader_free(B, reader, U32(reader, 0x24));
                reader_free(B, reader, U32(reader, 0x1c));
                reader_free(B, reader, U32(B, 0x1c));
                SU32(reader, 0x08, 0);
                SU32(reader, 0x0c, 0);
                SU32(reader, 0x14, 0);
                SU32(reader, 0x24, 0);
                rx_set_sp(entry);
                return 0;
            }
        }
        reader_free(B, reader, offsets);
    }

    /* 00827d60 */
    SU32(reader, 0x33c, 0);
    rx_set_sp(entry);
    return 1;
}
RX_FUNCTION(0x008252c0, rx_xml_reader_load_stream)
