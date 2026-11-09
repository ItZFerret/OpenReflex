/* build_activity_track.c -- construction unit 11: the AI racing line / track definition of an activity (core/notes/race.md,
 * construction.md section 16). FUN_00536ef0 loads the activity's .tdf07 track definition (FUN_00536d00 builds the path
 * "AI\\<level>-<suffix><area>.tdf07", FUN_00536dc0 the asset name "<level>-<suffix><area>"; suffix table 0x009c54cc: 0 nat, 1 sx,
 * 2 sm, 3 opx, 4 wp, 5 mm, 6 fmx, 7 fr, 8 cb, 9 ec, 10 trx, 11 utvm, 12 pwx, 13 chl, 14 pf) and parses it into the activity's
 * +0x890 block (and the world's +0x21e10, the same class): the sections (+0x130, count +0x1d8), the nodes (0x500 bytes each) the
 * AI / start grid / respawn read (rx_track_nearest_in_radius, rx_race_stub_respawn_point), the terrain heights and the track's
 * bounding box (+0x300 / +0x310).
 *
 * The file (recovered from the shipped assets; every .tdf07 of NAT_Concord / FR_Alpine / SX_Springfield parses to its exact
 * length): the header is four BIG-endian u32 -- n_nodes, n_sections, n_x, n_y; n_nodes * 0x500 + n_sections * 0x130 + n_x * 16 +
 * n_y * 16 + align16(4 * n_sections) bytes are allocated (the last two counts only size it). Then a tag / length / payload stream
 * (every tag and every u32 payload BIG endian):
 *     new section (0x0b): carves a 0x130-byte section: +0 first node, +4 last node, +8 / +0x10 next-section array / count,
 *       +0xc / +0x14 previous-section array / count (the entries are section indices, resolved after the parse);
 *     new node (0x14): carves a 0x500-byte node, linked (+0x60 previous, +0x64 next, +0x68 first node of the section);
 *     the node's fields (tags 0x15 .. 0x31: centre / left / right / two extra points, 9 grids of 8 x 4 dwords from +0x68 to
 *     +0x4e8, the dwords +0x4e8 .. +0x4f4), the block's (+0x100 .. +0x134, +0x1b4 .. +0x1ec), the section's (+0x18, +0x98,
 *     +0x118 .. +0x120); tags 8 / 9 / 10 / 0x23 / 0x2b read a 0x30-byte record and snap its centre onto the terrain; tags 0x1f
 *     reads the track type (+8) and sets the 8 class limits (+0x1b8: 90.0, or 53.66 for type 6), tag 0x24 an array at +0x1b8.
 *   Every node point is snapped to the terrain's base height (007c5950) after the parse and the bounding box of all node points
 *   goes to block+0x300 (min x / 5000 / min z) and block+0x310 (max x / 5000 / max z).
 *
 * The stream is the game's InputStream: the asset's MemoryInputStream (TDFAsset +0x10, build_tdf.c) when the database has the
 * .tdf07 (every shipped level does), else a loose file through CRTFileInputStream::vf8 008104d0. The product reads a loose file
 * through the install's fmt_vfs (file I/O is the only boundary; the oracle runs the original vf8 so its fopen and buffers are the
 * original's). */
#include "build_activity_int.h"
#include "tdf_owner.h"
#include "build_asset.h"
#include "build_scene.h"
#include "loop_platform.h"
#include "phys_terrain.h"
#include "map_scale.h"
#include "../src/fmt_db.h"
#include "../src/fmt_scene.h"
#include "../src/fmt_vfs.h"
#include "../src/fmt_modtrack.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* the original's .data / .rdata constants */
#define TRK_LOCK        0x00ae1cf0u   /* the track block's critical section (shared with 00536ca0) */
#define TRK_ALLOC_LOCK  0x00d61704u   /* the allocator lock the game's _aligned_malloc path takes */
#define TRK_COOKIE_SEED 0x009b45e4u

static const char* const k_suffix[15] = {"nat", "sx", "sm", "opx", "wp", "mm", "fmx", "fr", "cb", "ec", "trx", "utvm", "pwx", "chl", "pf"};

/* fld dword / fstp dword: the bits, a signalling NaN quieted */
static uint32_t x87_copy(uint32_t b) { return ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu)) ? (b | 0x400000u) : b; }

/* a game C string copied to a host buffer (four rotating buffers) */
static const char* track_cstr(rx_addr s) {
    static char b[4][512]; static int k;
    char* d = b[k++ & 3];
    uint32_t i = 0;
    for (; i < 511; i++) { d[i] = (char)U8(s, i); if (!d[i]) break; }
    d[i] = 0;
    return d;
}

/* msvcr90 sprintf into game memory (00536d00 / 00536dc0 format the "<level>-<suffix><area>" names) */
static void track_format(rx_addr dst, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
static void track_format(rx_addr dst, const char* fmt, ...) {
    char buf[512];
    va_list ap; va_start(ap, fmt); int n = vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    if (n < 0) n = 0;
    if ((size_t)n >= sizeof buf) n = (int)sizeof buf - 1;
    for (int i = 0; i <= n; i++) SU8(dst, (uint32_t)i, (uint8_t)(i == n ? 0 : buf[i]));
}

/* the suffix of a kind (0x009c54cc); the reference events use 0 .. 0xc */
static const char* track_suffix(uint32_t kind) {
    if (kind > 14) act_unreached(0x00536d55, "a .tdf07 suffix beyond the table (0x009c54cc)");
    return k_suffix[kind];
}

/* "<suffix><area>" (or just the suffix) into a frame buffer */
static void track_suffix_area(rx_addr dst, uint32_t kind, uint32_t area) {
    if (area) { track_format(dst, "%s%d", track_suffix(kind), (int)area); return; }
    const char* s = track_suffix(kind);
    uint32_t i = 0;
    do SU8(dst, i, (uint8_t)s[i]); while (s[i++]);
}

/* ======================================================================================================================== */
/* the file names (00536dc0 / 00536d00 / 00536e80)                                                                           */

/* 00536dc0 (ESI = level, ECX = kind, EDI = out; stdcall area; RET 4): "<level>-<suffix><area>" (no level: "AI\\temp") */
void rx_track_asset_name(rx_addr level, uint32_t kind, rx_addr out, uint32_t area) {
    ACT_FRAME(L, 0x108, 1);   /* sub esp,0x108; the /GS cookie at L+0x104 */
    if (U8(level, 0) == 0) { track_format(out, "%stemp", "AI\\"); ACT_END(L); return; }
    track_suffix_area(L + 4, kind, area);
    track_format(out, "%s-%s", track_cstr(level), track_cstr(L + 4));
    ACT_END(L);
}
RX_FUNCTION(0x00536dc0, rx_track_asset_name)

/* 00536d00 (ESI = level, ECX = kind, EDI = out; stdcall area; RET 4): "AI\\<asset name>.tdf07" */
void rx_track_file_path(rx_addr level, uint32_t kind, rx_addr out, uint32_t area) {
    ACT_FRAME(L, 0x108, 1);
    if (U8(level, 0) == 0) { track_format(out, "%stemp.tdf07", "AI\\"); ACT_END(L); return; }
    track_suffix_area(L + 4, kind, area);
    track_format(out, "%s%s-%s.tdf07", "AI\\", track_cstr(level), track_cstr(L + 4));
    ACT_END(L);
}
RX_FUNCTION(0x00536d00, rx_track_file_path)

/* 00536e80 (EDX = level, ECX = kind; stdcall area; RET 4) -> EAX: the Database (engine + 8) has the .tdf07 asset (vf27 != 0) */
uint32_t rx_track_has_asset(uint32_t kind, rx_addr level, uint32_t area) {
    if(fmt_modtrack_racing() && (kind==0 || kind==1))return 1;
    ACT_FRAME(L, 0x110, 2);
    const rx_addr name = L + 0xc;
    ACT_CALL(1, rx_track_asset_name(level, kind, name, area));
    const rx_addr db = U32(U32(0, 0x00a95844), 8);
    rx_addr handle;
    ACT_CALLR(handle, 4, rx_database_vf_handle(db, name, 0x666474u /* 'tdf' */, 0, 0));
    ACT_END(L);
    return handle != 0;
}
RX_FUNCTION(0x00536e80, rx_track_has_asset)

/* ======================================================================================================================== */
/* the InputStream virtuals of the loader (vtable 0x00974d1c: MemoryInputStream)                                             */

/* 00810600 (EAX = s): BufferedFileInputStream's constructor (vtable 0x00974cd0, the fields, 0) */
void rx_buffered_stream_construct(rx_addr s) {
    SU32(s, 0xc, 0); SU32(s, 0x10, 0); SU8(s, 0x18, 0); SU32(s, 0, 0x00974cd0);
    SU32(s, 4, 0); SU32(s, 8, 0);
    SU32(s, 0x11c, 0); SU32(s, 0x120, 0); SU32(s, 0x124, 0); SU32(s, 0x12c, 0);
    SU32(s, 0x170, 0); SU32(s, 0x174, 0); SU32(s, 0x128, 0); SU32(s, 0x14c, 0);
    SU32(s, 0x16c, 0); SU32(s, 0x130, 0); SU32(s, 0x150, 0);
}
RX_FUNCTION(0x00810600, rx_buffered_stream_construct)

/* 00810680 (ECX = s): BufferedFileInputStream's destructor: the buffer freed (vtables 0x00974cd0 then 0x00974c78, 0x0094dab4) */
void rx_buffered_stream_destroy(rx_addr s) {
    SU32(s, 0, 0x00974cd0);
    if (U32(s, 4) != 0 && U32(s, 8) != 0) {
        SU8(s, 0x18, 0); SU32(s, 0x10, 0);
        crt_free(U32(s, 4));
        SU32(s, 4, 0);
    }
    SU32(s, 0, 0x00974c78);
    if (U32(s, 4) != 0 && U32(s, 8) != 0) { SU8(s, 0x18, 0); crt_free(U32(s, 4)); }
    SU32(s, 0, 0x0094dab4);
}
RX_FUNCTION(0x00810680, rx_buffered_stream_destroy)

/* 008109b0 (ECX = s; stdcall dst, count; RET 8) -> EAX: count bytes from the memory stream's position (s +4 data, +8 size,
 * +0xc position, +0x18 the last count); 1 for a full read, else the rest copied and 0 */
uint32_t rx_memory_stream_read(rx_addr s, rx_addr dst, uint32_t n) {
    const uint32_t pos = U32(s, 0xc), avail = U32(s, 8) - pos;
    if ((int32_t)n <= (int32_t)avail) {
        crt_memcpy(dst, U32(s, 4) + pos, n);
        SU32(s, 0x18, n);
        SU32(s, 0xc, pos + n);
        return 1;
    }
    SU32(s, 0x18, avail);
    crt_memcpy(dst, U32(s, 4) + pos, avail);
    SU32(s, 0xc, pos + avail);
    return 0;
}
RX_FUNCTION(0x008109b0, rx_memory_stream_read)

/* 00810a10 (ECX = s; stdcall n; RET 4): the position += n (returns 1) */
void rx_memory_stream_skip(rx_addr s, uint32_t n) { SU32(s, 0xc, U32(s, 0xc) + n); }
RX_FUNCTION(0x00810a10, rx_memory_stream_skip)

/* 00810960 (ECX = s): the stream's buffer released when it owns it (+0x14), +4 / +8 cleared */
void rx_memory_stream_close(rx_addr s) {
    rx_owned_memory_stream_close(s);
}
RX_FUNCTION(0x00810960, rx_memory_stream_close)

/* 008104d0 (ECX = s; stdcall path, unused, unused; RET 0xc) -> EAX: opens the file ("rb"; 00810440 prepends the exe directory
 * when the path has no drive letter) into the stream (+4 the FILE*, +8 1). The product has no CRT file streams: the same file
 * comes from the install's fmt_vfs into a MemoryInputStream (the file I/O boundary). */
uint32_t rx_file_stream_open(rx_addr s, rx_addr path, uint32_t a2, uint32_t a3) {
    ACT_FRAME(L, 0x108, 3);
    SU32(s, 0x14, 0);
#ifdef RX_ORACLE
    uint32_t args[3] = {path, a2, a3};
    const uint32_t r = rx_call_original(0x008104d0, RX_THISCALL, s, 3, args);
    ACT_END(L);
    return r;
#else
    (void)a2; (void)a3;
    struct fmt_level* lv = rx_event_level();
    fmt_install* in = lv ? fmt_level_install(lv) : NULL;
    fmt_buf b;
    if (!in || fmt_vfs_read_file(fmt_install_vfs(in), track_cstr(path), &b) != 0) { ACT_END(L); return 0; }
    const uint32_t size = (uint32_t)b.size;
    rx_addr data = rx_crt_aligned_malloc(size ? size : 1, 0x10);
    if (data) for (uint32_t i = 0; i < size; i++) SU8(data, i, b.data[i]);
    fmt_buf_free(&b);
    if (!data) { ACT_END(L); return 0; }
    SU32(s, 0, 0x00974d1c);   /* MemoryInputStream: +4 data, +8 size, +0xc position, +0x10 loaded, +0x14 owns */
    SU32(s, 4, data); SU32(s, 8, size); SU32(s, 0xc, 0); SU32(s, 0x10, 1); SU32(s, 0x14, 1); SU32(s, 0x18, 0);
    ACT_END(L);
    return 1;
#endif
}
RX_FUNCTION(0x008104d0, rx_file_stream_open)

/* the stream virtuals at the original's depth (the core's when the vtable entry is registered, else the original) */
static uint32_t stream_read(rx_addr s, rx_addr dst, uint32_t n) {
    const rx_addr fn = U32(U32(s, 0), 8);
    uint32_t (*f)(rx_addr, rx_addr, uint32_t) = RX_FN(fn, uint32_t (*)(rx_addr, rx_addr, uint32_t));
    if (f) { uint32_t r; ACT_CALLR(r, 2, f(s, dst, n)); return r; }
    const uint32_t a[2] = {dst, n};
    return rx_call_original(fn, RX_THISCALL, s, 2, a);
}
static void stream_skip(rx_addr s, uint32_t n) {
    const rx_addr fn = U32(U32(s, 0), 0x10);
    void (*f)(rx_addr, uint32_t) = RX_FN(fn, void (*)(rx_addr, uint32_t));
    if (f) { ACT_CALL(1, f(s, n)); return; }
    rx_call_original(fn, RX_THISCALL, s, 1, &n);
}
static void stream_close(rx_addr s) {
    const rx_addr fn = U32(U32(s, 0), 4);
    void (*f)(rx_addr) = RX_FN(fn, void (*)(rx_addr));
    if (f) { ACT_CALL(0, f(s)); return; }
    rx_call_original(fn, RX_THISCALL, s, 0, 0);
}

/* ======================================================================================================================== */
/* the parser leaves (00539550 .. 00539820)                                                                                  */

/* 00539550 (EAX = stream, EDI = &len; stdcall &tag; RET 4) -> EAX: the next (tag, length) pair, both byte swapped; 0 at the end
 * (the length read's result, as the original returns) */
uint32_t rx_track_read_tag(rx_addr stream, rx_addr tag_out, rx_addr len_out) {
    ACT_FRAME(L, 0, 2);
    stream_read(stream, tag_out, 4);
    const uint32_t r = stream_read(stream, len_out, 4);
    SU32(tag_out, 0, __builtin_bswap32(U32(tag_out, 0)));
    SU32(len_out, 0, __builtin_bswap32(U32(len_out, 0)));
    ACT_END(L);
    return r;
}
RX_FUNCTION(0x00539550, rx_track_read_tag)

/* 005395d0 (EDI = stream, ESI = dst): three dwords into dst, each byte swapped (RET 4) */
void rx_track_read_vec3(rx_addr stream, rx_addr dst) {
    ACT_FRAME(L, 0, 2);
    for (uint32_t k = 0; k < 3; k++) {
        stream_read(stream, dst + 4 * k, 4);
        SU32(dst, 4 * k, __builtin_bswap32(U32(dst, 4 * k)));
    }
    ACT_END(L);
}
RX_FUNCTION(0x005395d0, rx_track_read_vec3)

/* 00539680 (EDI = stream; stdcall dst; RET 4): a u32 count, then count byte-swapped dwords into dst */
void rx_track_read_u32_array(rx_addr stream, rx_addr dst) {
    ACT_FRAME(L, 8, 0);   /* push ecx (the count slot at L+4), push ebx */
    stream_read(stream, L + 4, 4);
    const uint32_t n = __builtin_bswap32(U32(L, 4));
    if (n > 0) {
        ACT_FRAME(R, 0, 1);                                            /* push esi (005396c5) for the element reads */
        for (uint32_t i = 0; i < n; i++) {
            stream_read(stream, dst + 4 * i, 4);
            SU32(dst, 4 * i, __builtin_bswap32(U32(dst, 4 * i)));
        }
        ACT_END(R);
    }
    ACT_END(L);
}
RX_FUNCTION(0x00539680, rx_track_read_u32_array)

/* 00539720 (EDI = stream; stdcall dst; RET 4): a grid: u32 rows, u32 columns, then rows x columns byte-swapped dwords, one
 * 0x10-byte row each (the grids at node +0x68 .. +0x4e8 are 8 x 4) */
void rx_track_read_grid(rx_addr stream, rx_addr dst) {
    ACT_FRAME(L, 8, 2);
    stream_read(stream, L + 0xc, 4);
    stream_read(stream, L + 8, 4);
    const uint32_t rows = __builtin_bswap32(U32(L, 0xc)), cols = __builtin_bswap32(U32(L, 8));
    ACT_FRAME(R, 0, 1);                                                /* push ebx (005397aa) before the element reads */
    for (uint32_t r = 0; r < rows; r++) {
        for (uint32_t c = 0; c < cols; c++) {
            const rx_addr e = dst + 0x10 * r + 4 * c;
            stream_read(stream, e, 4);
            SU32(e, 0, __builtin_bswap32(U32(e, 0)));
        }
    }
    ACT_END(R);
    ACT_END(L);
}
RX_FUNCTION(0x00539720, rx_track_read_grid)

/* 0042d960 (EDX = the terrain list 0x009e30d4 {entries, count +8}, EDI = scene) -> EAX: the terrain object whose +0x44a8 is the
 * scene, else 0 (its static initializer: guard bit 0 of 0x009e30e0) */
static rx_addr terrain_for_scene(rx_addr scene) {
    if (!(U8(0, 0x009e30e0) & 1)) { SU8(0, 0x009e30e0, U8(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
    const uint32_t n = U32(0, 0x009e30dc);
    for (uint32_t i = 0; i < n; i++) {
        const rx_addr e = U32(0, 0x009e30d4 + 4 * i);
        if (e && U32(e, 0x44a8) == scene) return e;
    }
    return 0;
}

/* 00539820 (ECX = stream; stdcall dst, world holder; RET 4): a 0x30-byte record (the dwords +0, +4, +8, +0x10, +0x14, +0x18,
 * +0x20, +0x28, +0x2c byte swapped), its centre snapped onto the terrain of the holder's world (holder +0 = the MXWorld, +0x20138
 * its scene) */
void rx_track_read_record30(rx_addr stream, rx_addr dst, rx_addr holder) {
    ACT_FRAME(L, 0, 2);
    stream_read(stream, dst, 0x30);
    static const uint8_t sw[9] = {0x00, 0x04, 0x08, 0x10, 0x14, 0x18, 0x20, 0x28, 0x2c};
    for (int k = 0; k < 9; k++) SU32(dst, sw[k], __builtin_bswap32(U32(dst, sw[k])));
    {   /* MAP SCALE (port, map_scale.h): the record's place (x, z; y is snapped below) and its width, before the snap (nothing at 1) */
        const float ms = rx_ms_load();
        if (ms != 1.0f) { rx_ms_f(dst, 0, ms); rx_ms_f(dst, 8, ms); rx_ms_f(dst, 0x20, ms); }
    }
    const rx_addr world = U32(holder, 0);
    if (world != 0) {
        const rx_addr terrain = terrain_for_scene(U32(world, 0x20138));
        uint32_t edx = 0;
        ACT_CALL(1, phys_terrain_base_height_at(terrain, dst, &edx));
    }
    ACT_END(L);
}
RX_FUNCTION(0x00539820, rx_track_read_record30)

/* 00538090 (EAX = block): the track block's buffer (+0x320) freed under the allocator lock and +0x10 .. +0x1ef cleared */
void rx_track_block_reset(rx_addr block) {
    const rx_addr p = U32(block, 0x320);
    if (p) {
        if (U8(0, 0x00da67d8) != 0) loop_enter_cs(TRK_ALLOC_LOCK);
        crt_aligned_free(p);
        if (U8(0, 0x00da67d8) != 0) loop_leave_cs(TRK_ALLOC_LOCK);
        SU32(block, 0x320, 0);
        crt_memset(block + 0x10, 0, 0x1e0);
    }
}
RX_FUNCTION(0x00538090, rx_track_block_reset)

/* MAP SCALE (port, map_scale.h): the parsed track scaled about the origin -- every node's five points (x, z: y is the 5000
 * placeholder the terrain snap replaces), its segment / cumulative lengths (+0x4f0 / +0x4f4), each section's length (+0x120), the
 * block's grid box (+0x100 .. +0x12c) and its two track lengths (+0x1dc / +0x1e0). Speeds, directions, flags stay. */
static void track_map_scale(rx_addr block) {
    const float k = rx_ms_load();
    if (k == 1.0f || !U32(block, 0x130)) return;
    rx_ms_v3(block, 0x100, k); rx_ms_v3(block, 0x110, k); rx_ms_v3(block, 0x120, k);
    rx_ms_f(block, 0x1dc, k); rx_ms_f(block, 0x1e0, k);
    const rx_addr secs = U32(block, 0x130);
    const int32_t ns = (int32_t)U32(block, 0x1d8);
    for (int32_t i = 0; i < ns; i++) {
        const rx_addr sec = U32(secs, 4 * (uint32_t)i);
        rx_ms_f(sec, 0x120, k);
        for (rx_addr n = U32(sec, 0); n; n = U32(n, 0x64)) {
            for (uint32_t p = 0; p < 0x50; p += 0x10) { rx_ms_f(n, p, k); rx_ms_f(n, p + 8, k); }
            rx_ms_f(n, 0x4f0, k); rx_ms_f(n, 0x4f4, k);
        }
    }
}

/* ======================================================================================================================== */
/* FUN_00536ef0: the track definition                                                                                       */

uint32_t rx_track_load(rx_addr level, rx_addr block, uint32_t kind, uint32_t area) {
    const rx_addr sp0 = rx_sp();
    const rx_addr L = ((sp0 - 4) & ~15u) - 0x2d4 - 12;        /* push ebp; and esp,-16; sub 0x2d4; push ebx / esi / edi */
    rx_set_sp(L);
    const rx_addr name = L + 0x1d0, stream = L + 0x58;
    uint32_t file_opened = 0;

    ACT_CALL(1, rx_track_asset_name(level, kind, name, area));
    act_strcpy(block + 0x1f0, name);                          /* the block keeps the asset name */
    const int ms_was = rx_ms_level_begin(track_cstr(level));  /* MAP SCALE (port): the event level's track loads scaled (map_scale.h) */

    loop_enter_cs(TRK_LOCK);
    const rx_addr db = U32(U32(0, 0x00a95844), 8);
    rx_addr handle;
    ACT_CALLR(handle, 4, rx_database_vf_handle(db, name, 0x666474u /* 'tdf' */, 0, 0));
    rx_buffered_stream_construct(stream);                     /* 00810600 */
    rx_addr src;
    if (fmt_modtrack_racing() && (kind==0 || kind==1)) {
        uint32_t size;const uint8_t* data=fmt_modtrack_race_stream(&size);
        rx_addr mem=rx_crt_aligned_malloc(size,0x10);
        for(uint32_t i=0;i<size;i++)SU8(mem,i,data[i]);
        SU32(stream,0,0x00974d1c);SU32(stream,4,mem);SU32(stream,8,size);
        SU32(stream,0xc,0);SU32(stream,0x10,1);SU32(stream,0x14,1);SU32(stream,0x18,0);
        file_opened=1;src=stream;
    } else if (handle == 0) {
        ACT_CALL(1, rx_track_file_path(level, kind, name, area));
        uint32_t opened;
        ACT_CALLR(opened, 3, rx_file_stream_open(stream, name, 0, 0));
        if (!opened) {
            loop_leave_cs(TRK_LOCK);
            rx_buffered_stream_destroy(stream);
            rx_ms_level_end(ms_was);
            rx_set_sp(sp0);   /* the adapter's m.ret(0xc) pops the return address here */
            return 0;
        }
        file_opened = 1;
        src = stream;
    } else {
        SU32(handle, 0x4c, 1);                                /* the handle is loading while its TDFAsset is parsed */
        const rx_addr asset = U32(handle, 0x44);
        const rx_addr ms = U32(asset, 0x10);
        SU32(ms, 0xc, 0);
        src = ms;
    }

    rx_track_block_reset(block);

    /* the header: n_nodes, n_sections, n_x, n_y (all byte swapped) */
    const uint16_t ho[4] = {0x34, 0x28, 0x38, 0x44};
    for (int k = 0; k < 4; k++) {
        stream_read(src, L + ho[k], 4);
        SU32(L, ho[k], __builtin_bswap32(U32(L, ho[k])));
    }
    const uint32_t n_nodes = U32(L, 0x34), n_sections = U32(L, 0x28), n_x = U32(L, 0x38), n_y = U32(L, 0x44);
    const uint32_t sec_bytes = (n_sections * 4u + 15u) & ~15u;
    uint32_t sec_off = 0, bump = 0, cur_node = 0, cur_sec = 0, next_link = 0;

    {
        uint32_t size = n_nodes * 0x500u;
        size += n_sections * 0x130u;
        size += n_x * 16u;
        size += n_y * 16u;
        size += sec_bytes;
        if (U8(0, 0x00da67d8) != 0) loop_enter_cs(TRK_ALLOC_LOCK);
        const rx_addr mem = rx_crt_aligned_malloc(size, 0x10);
        if (U8(0, 0x00da67d8) != 0) loop_leave_cs(TRK_ALLOC_LOCK);
        SU32(block, 0x320, mem);
        crt_memset(mem, 0, size);
        crt_memset(block + 0x10, 0, 0x1e0);
        SU32(block, 0x1d8, n_sections);
        SU32(block, 0x130, mem);
        bump = mem + sec_bytes;
        SU32(L, 0x14, bump);
    }
    SU32(L, 0x40, 0); SU32(L, 0x10, 0); SU32(L, 0x2c, 0); SU32(L, 0x48, 0);

    /* the tag stream: tag / length / payload; the length read's result 0 ends it */
    for (;;) {
        uint32_t ok;
        ACT_CALLR(ok, 1, rx_track_read_tag(src, L + 0x4c, L + 0x50));
        if (!ok) break;                     /* the length read failed: end of the stream */
        const uint32_t tag = U32(L, 0x4c);
        const uint32_t len = U32(L, 0x50);
        switch (tag) {
        case 0x01: rx_track_read_vec3(src, block + 0x120); break;
        case 0x02: rx_track_read_vec3(src, block + 0x110); break;
        case 0x03: rx_track_read_vec3(src, block + 0x100); break;
        case 0x04: case 0x05: case 0x07: case 0x0d: case 0x11: case 0x18: case 0x19: case 0x1b:
        case 0x26: case 0x27: case 0x29: case 0x2a: {
            rx_addr dst;
            switch (tag) {
            case 0x04: dst = block + 0x1e0; break;
            case 0x05: dst = block + 0x1dc; break;
            case 0x07: dst = block + 0x1b4; break;
            case 0x0d: dst = cur_sec + 0x120; break;
            case 0x11: dst = cur_sec + 0x118; break;
            case 0x18: dst = cur_node + 0x4f0; break;
            case 0x19: dst = cur_node + 0x4f4; break;
            case 0x1b: dst = cur_node + 0x4e8; break;
            case 0x26: dst = block + 0x1e4; break;
            case 0x27: dst = block + 0x1e8; break;
            case 0x29: dst = block + 0x1ec; break;
            default:   dst = cur_sec + 0x11c; break;
            }
            stream_read(src, dst, 4);
            SU32(dst, 0, __builtin_bswap32(U32(dst, 0)));
            break;
        }
        case 0x06: rx_track_read_grid(src, block + 0x134); break;
        case 0x08: rx_track_read_record30(src, block + 0x10, block); break;
        case 0x09: rx_track_read_record30(src, block + 0x70, block); break;
        case 0x0a: rx_track_read_record30(src, block + 0x40, block); break;
        case 0x0b: {   /* a new section */
            const rx_addr sec = bump;
            bump += 0x130;
            SU32(L, 0x14, bump);
            SU32(U32(block, 0x130), sec_off, sec);
            sec_off += 4;
            cur_sec = sec;
            next_link = sec;
            SU32(L, 0x2c, cur_sec); SU32(L, 0x40, next_link); SU32(L, 0x48, sec_off);
            break;
        }
        case 0x0c: case 0x0f: {   /* a section's next / previous-section list: count + array */
            const int prev = tag == 0x0f;
            const rx_addr cnt_p = cur_sec + (prev ? 0x14 : 0x10), arr_p = cur_sec + (prev ? 0xc : 8);
            stream_read(src, cnt_p, 4);
            SU32(cnt_p, 0, __builtin_bswap32(U32(cnt_p, 0)));
            const uint32_t n = U32(cnt_p, 0);
            const rx_addr arr = bump;
            bump += (n * 4u + 15u) & ~15u;
            SU32(L, 0x14, bump);
            SU32(arr_p, 0, arr);
            break;
        }
        case 0x0e: case 0x10: {   /* an entry of the next / previous-section list */
            const int prev = tag == 0x10;
            const rx_addr arr = U32(cur_sec, prev ? 0xc : 8);
            stream_read(src, L + 0x3c, 4);
            stream_read(src, L + 0x54, 4);
            const uint32_t idx = __builtin_bswap32(U32(L, 0x3c)), val = __builtin_bswap32(U32(L, 0x54));
            SU32(arr, idx * 4, val);
            break;
        }
        case 0x12: rx_track_read_grid(src, cur_sec + 0x98); break;
        case 0x13: rx_track_read_grid(src, cur_sec + 0x18); break;
        case 0x14: {   /* a new node */
            const rx_addr node = bump;
            bump += 0x500;
            SU32(L, 0x14, bump);
            SU32(next_link, 0, node);
            SU32(cur_sec, 4, node);
            cur_node = node;
            next_link = node + 0x64;
            SU32(L, 0x10, cur_node); SU32(L, 0x40, next_link);
            break;
        }
        case 0x15: rx_track_read_vec3(src, cur_node + 0x00); break;
        case 0x16:
            rx_track_read_vec3(src, cur_node + 0x10);
            for (uint32_t k = 0; k < 3; k++) SU32(cur_node, 0x30 + 4 * k, x87_copy(U32(cur_node, 0x10 + 4 * k)));
            break;
        case 0x17:
            rx_track_read_vec3(src, cur_node + 0x20);
            for (uint32_t k = 0; k < 3; k++) SU32(cur_node, 0x40 + 4 * k, x87_copy(U32(cur_node, 0x20 + 4 * k)));
            break;
        case 0x1a: rx_track_read_vec3(src, cur_node + 0x50); break;
        case 0x1c: rx_track_read_grid(src, cur_node + 0x68); break;
        case 0x1d: rx_track_read_grid(src, cur_node + 0xe8); break;
        case 0x1f: {   /* the track type (+8) and the 8 class limits (+0x1b8) */
            stream_read(src, block + 8, 4);
            SU32(block, 8, __builtin_bswap32(U32(block, 8)));
            const uint32_t v = U32(block, 8) == 6 ? 0x426aa3d7u : 0x42b40000u;
            for (uint32_t k = 0; k < 8; k++) SU32(block, 0x1b8 + 4 * k, v);
            break;
        }
        case 0x21: rx_track_read_grid(src, cur_node + 0x168); break;
        case 0x22: rx_track_read_grid(src, cur_node + 0x1e8); break;
        case 0x23: rx_track_read_record30(src, block + 0xa0, block); break;
        case 0x24: rx_track_read_u32_array(src, block + 0x1b8); break;
        case 0x28: rx_track_read_grid(src, cur_node + 0x268); break;
        case 0x2b: rx_track_read_record30(src, block + 0xd0, block); break;
        case 0x2c: case 0x2d: {   /* a point; a NaN / non-finite x or z copies the previous point's */
            const uint32_t base = tag == 0x2c ? 0x30 : 0x40, prev = tag == 0x2c ? 0x10 : 0x20;
            rx_track_read_vec3(src, cur_node + base);
            const float a = F32(cur_node, base), b = F32(cur_node, base + 8);
            const int bad = isnan(a) || isnan(b) || !crt_finite((double)a) || !crt_finite((double)b);
            if (bad) for (uint32_t k = 0; k < 3; k++) SU32(cur_node, base + 4 * k, x87_copy(U32(cur_node, prev + 4 * k)));
            break;
        }
        case 0x2e: rx_track_read_grid(src, cur_node + 0x468); break;
        case 0x2f: rx_track_read_grid(src, cur_node + 0x2e8); break;
        case 0x30: rx_track_read_grid(src, cur_node + 0x368); break;
        case 0x31: rx_track_read_grid(src, cur_node + 0x3e8); break;
        default: stream_skip(src, len); break;
        }
    }

    /* resolve the section arrays and the stored (section, node) index pairs */
    {
        const rx_addr secs = U32(block, 0x130);
        const int32_t ns = (int32_t)U32(block, 0x1d8);
        for (int32_t s = 0; s < ns; s++) {
            const rx_addr sec = U32(secs, 4 * (uint32_t)s);
            rx_addr n = U32(sec, 0);
            while (U32(n, 0x64) != 0) { const rx_addr nx = U32(n, 0x64); SU32(nx, 0x60, n); n = nx; }
            const int32_t nn = (int32_t)U32(sec, 0x10);
            for (int32_t i = 0; i < nn; i++) {
                const rx_addr e = U32(sec, 8) + 4 * (uint32_t)i;
                SU32(e, 0, U32(secs, 4 * U32(e, 0)));
            }
            const int32_t pn = (int32_t)U32(sec, 0x14);
            for (int32_t i = 0; i < pn; i++) {
                const rx_addr e = U32(sec, 0xc) + 4 * (uint32_t)i;
                SU32(e, 0, U32(secs, 4 * U32(e, 0)));
            }
        }
        /* the five stored markers: (section index, node index) -> pointers; the last four only when their byte is set */
        const uint16_t mk[5][3] = {{0x98, 0x9c, 0}, {0x68, 0x6c, 1}, {0x38, 0x3c, 1}, {0xc8, 0xcc, 1}, {0xf8, 0xfc, 1}};
        for (int k = 0; k < 5; k++) {
            if (mk[k][2] && U8(block, mk[k][0] - 4) == 0) continue;
            const rx_addr sec = U32(secs, 4 * U32(block, mk[k][0]));
            int32_t cnt = (int32_t)U32(block, mk[k][1]);
            rx_addr n = U32(sec, 0);
            SU32(block, mk[k][0], sec);
            while (cnt > 0) { n = U32(n, 0x64); cnt--; }
            SU32(block, mk[k][1], n);
        }
    }

    /* close the loose stream, or release the loading handle */
    if (file_opened) stream_close(stream);
    else SU32(handle, 0x4c, 0);

    track_map_scale(block);   /* MAP SCALE (port): the node points and lengths, before they are snapped and bounded (nothing at 1) */
    rx_ms_level_end(ms_was);

    /* the track's bounding box */
    SU32(block, 0x300, 0x7f7fffffu); SF32(block, 0x304, 5000.0f /* 0x977398 */); SU32(block, 0x308, 0x7f7fffffu);
    SF32(block, 0x310, -3.4028234663852886e+38f /* 0xff7fffff */); SF32(block, 0x314, 5000.0f);
    SF32(block, 0x318, -3.4028234663852886e+38f);
    if (U32(block, 0) == 0) { rx_set_sp(sp0); return 0; }

    const rx_addr terrain = terrain_for_scene(U32(U32(block, 0), 0x20138));
    uint32_t edx = 0;
    {
        static const uint8_t off[5] = {0x00, 0x10, 0x20, 0x30, 0x40};
        const rx_addr secs = U32(block, 0x130);
        const int32_t ns = (int32_t)U32(block, 0x1d8);
        for (int32_t s = 0; s < ns; s++) {
            rx_addr n = U32(U32(secs, 4 * (uint32_t)s), 0);
            for (;;) {
                for (int k = 0; k < 5; k++) {
                    float source_y=F32(n,off[k]+4)*rx_ms_scale();
                    ACT_CALL(1, phys_terrain_base_height_at(terrain, n + off[k], &edx));
                    /* MTX contains riding decks above the heightfield. Keep the authored
                     * route on that collision layer, including AI recovery/respawn. */
                    if(fmt_modtrack_racing() && (kind==0 || kind==1))SF32(n,off[k]+4,fmt_modtrack_ride_height(F32(n,off[k]),F32(n,off[k]+8),source_y,F32(n,off[k]+4),rx_ms_scale()));
                }
                if (n == 0) break;
                float mxx = F32(block, 0x300), mnz = F32(block, 0x308);
                float mxx2 = F32(block, 0x310), mnz2 = F32(block, 0x318);
                for (int k = 0; k < 5; k++) {
                    const float x = F32(n, off[k]), z = F32(n, off[k] + 8);
                    if (mxx > x) mxx = x;
                    if (mnz > z) mnz = z;
                    if (x > mxx2) mxx2 = x;
                    if (z > mnz2) mnz2 = z;
                }
                SF32(block, 0x300, mxx); SF32(block, 0x308, mnz);
                SF32(block, 0x310, mxx2); SF32(block, 0x318, mnz2);
                if (U32(n, 0x64) == 0) break;
                n = U32(n, 0x64);
            }
        }
    }
    ACT_CALL(1, phys_terrain_base_height_at(terrain, block + 0x300, &edx));
    ACT_CALL(1, phys_terrain_base_height_at(terrain, block + 0x310, &edx));
    rx_set_sp(sp0);   /* the adapter's m.ret(0xc) pops the return address here */
    return 1;
}
RX_FUNCTION(0x00536ef0, rx_track_load)
