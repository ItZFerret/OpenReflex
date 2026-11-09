/* build_anim.c -- see build_anim.h. Hand transliteration of AnimAssetIO::vf3 0082cc70, the AnimAsset constructor 0082bb90 and the
 * stream loaders 0082bf10 / 0082bda0 from the disassembly (the decompile drops most of 0082bf10's version branches as unreachable).
 * Locals of the original's frame are C locals (they only receive stream reads); every game-memory write, allocation and read is
 * in the original's order. Signed / unsigned compares as the original's jl / jle / jbe. */
#include "build_anim.h"
#include "build_alloc.h"
/* rx_x86.h comes with rx_game.h */

static uint32_t pad16(uint32_t n) { return (n & 0xf) ? 0x10 - (n & 0xf) : 0; }

/* x87 fld dword / fstp dword: the bits, a signalling NaN quieted (invalid is masked) */
static uint32_t x87_f32_copy(uint32_t b) {
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu) && !(b & 0x00400000u)) b |= 0x00400000u;
    return b;
}

/* 0082bb90 (EAX = self): vtable and zeroed fields; +4 and +0x30 are left as allocated */
rx_addr rx_anim_asset_construct(rx_addr a) {
    SU32(a, 0x08, 0); SU32(a, 0x0c, 0);
    SU32(a, 0x00, 0x00975ebc);   /* AnimAsset::vftable */
    SU32(a, 0x10, 0); SU32(a, 0x14, 0); SU32(a, 0x18, 0); SU32(a, 0x1c, 0); SU32(a, 0x20, 0); SU32(a, 0x24, 0); SU32(a, 0x28, 0);
    SU32(a, 0x3c, 0); SU32(a, 0x2c, 0); SU32(a, 0x34, 0); SU32(a, 0x38, 0); SU32(a, 0x40, 0); SU32(a, 0x44, 0); SU32(a, 0x48, 0);
    SF32(a, 0x4c, 0.0f); SF32(a, 0x50, 0.0f);
    SU32(a, 0x54, 0); SU32(a, 0x58, 0); SU32(a, 0x5c, 0); SU32(a, 0x60, 0); SU32(a, 0x64, 0);
    return a;
}
RX_FUNCTION(0x0082bb90, rx_anim_asset_construct);

/* the names array: new char[n][0x20] (no count: trivial destructor), each element's first byte cleared (Fn_0082b910), then for each
 * name a length word and that many bytes into its 0x20-byte slot */
static void read_names(rx_addr a, rx_stream* s) {
    uint32_t n = U32(a, 0x10);
    rx_addr names = rx_reflex_aligned_alloc(rx_mul_sat(n, 0x20));
    if (names) for (int32_t k = (int32_t)n; --k >= 0;) SU8(names + 0x20u * (uint32_t)((int32_t)n - 1 - k), 0, 0);
    SU32(a, 0x14, names);
    for (int32_t i = 0; i < I32(a, 0x10); i++) {
        uint32_t len = rx_stream_u32(s);
        rx_stream_read(s, U32(a, 0x14) + 0x20u * (uint32_t)i, len);
    }
}

/* new T[n] of 0x14-byte key tracks: (ptr 0, 1, 0, 0, 0) -- the inline constructor loops of 0x82c5f8 / 0x82c785 / 0x82c94e */
static rx_addr new_tracks_14(uint32_t n) {
    rx_addr p = rx_reflex_alloc_array(n, 0x14);
    if (p) for (int32_t k = (int32_t)n - 1, i = 0; k >= 0; k--, i++) {
        rx_addr e = p + 0x14u * (uint32_t)i;
        SU32(e, 0x00, 0); SU32(e, 0x04, 1); SU32(e, 0x08, 0); SU32(e, 0x0c, 0); SU32(e, 0x10, 0);
    }
    return p;
}

/* key tracks of (time, value) pairs: per track a count into +8, the pairs array (count * 8) reallocated into +0x10, then the pairs as
 * (t, v) reads stored v at +4, t at +0 -- 0x82c7c5 (+0x2c), 0x82c990 (+0x34), 0x82caf0 (+0x38) */
static void read_pair_tracks(rx_addr a, uint32_t base_off, uint32_t count_off, rx_stream* s) {
    for (int32_t i = 0; i < I32(a, count_off); i++) {
        uint32_t o = 0x14u * (uint32_t)i;
        rx_stream_read(s, U32(a, base_off) + o + 8, 4);
        rx_addr e = U32(a, base_off) + o;
        rx_addr old = U32(e, 0x10); uint32_t cnt = U32(e, 8);
        SU32(e, 0x0c, cnt);
        rx_addr p = rx_crt_aligned_realloc(old, cnt * 8, 16);
        SU32(e, 0x10, p);
        for (int32_t k = 0; k < I32(U32(a, base_off) + o, 8); k++) {
            uint32_t t = rx_stream_u32(s), v = rx_stream_u32(s);
            rx_addr e2 = U32(a, base_off) + o;
            SU32(U32(e2, 0x10) + 8u * (uint32_t)k, 4, v);
            SU32(U32(e2, 0x10) + 8u * (uint32_t)k, 0, t);
        }
    }
}

/* 0082bda0 (EDI = self, ESI = stream): the packed format (after the version word and the 0x12345678 marker) */
void rx_anim_asset_load_packed(rx_addr a, rx_stream* s) {
    (void)rx_stream_u32(s);                              /* a word the loader ignores */
    rx_stream_read(s, a + 0x10, 4);
    read_names(a, s);
    rx_addr h = rx_reflex_aligned_alloc(0x40);
    if (h) {
        SU32(h, 0x08, 0); SU32(h, 0x0c, 0); SU32(h, 0x24, 0); SU32(h, 0x28, 0); SU32(h, 0x3c, 0); SU32(h, 0x30, 0); SU32(h, 0x2c, 0);
        SU32(h, 0x1c, 0); SU32(h, 0x04, 0);
        SF32(h, 0x14, 0.0f); SF32(h, 0x10, 0.0f);
        SU32(h, 0x38, 0xffffffffu); SU32(h, 0x34, 0xffffffffu);
    }
    SU32(a, 0x64, h);
    rx_stream_read(s, h, 0x40);
    rx_addr blk = rx_crt_aligned_malloc(U32(U32(a, 0x64), 0), 16);
    SU32(U32(a, 0x64), 0x3c, blk);
    rx_stream_read(s, U32(U32(a, 0x64), 0x3c), U32(U32(a, 0x64), 0));
    h = U32(a, 0x64);
    SU32(a, 0x4c, x87_f32_copy(U32(h, 0x10)));
    SU32(a, 0x20, U32(h, 0x24));
    SU32(a, 0x18, U32(h, 0x28));
}

/* 0082bf10 (ECX = self, EAX = stream) */
void rx_anim_asset_load(rx_addr a, rx_stream* s) {
    int32_t version = -(int32_t)rx_stream_u32(s);
    if (version >= 5 && rx_stream_u32(s) == 0x12345678u) { rx_anim_asset_load_packed(a, s); return; }
    rx_stream_read(s, a + 0x48, 4);
    rx_stream_read(s, a + 0x10, 4);
    read_names(a, s);
    rx_stream_read(s, a + 0x4c, 4);
    (void)rx_stream_u32(s); (void)rx_stream_u32(s);      /* two words into locals the loader does not use */
    rx_stream_read(s, a + 0x18, 4);
    rx_stream_read(s, a + 0x20, 4);
    if (version >= 6) rx_stream_read(s, a + 0x28, 4); else SU32(a, 0x28, 0);
    int32_t key_bones = version >= 9 ? (int32_t)rx_stream_u32(s) : 0;
    if (version >= 10) { uint32_t f = rx_stream_u32(s); SU32(a, 0x30, f ? U32(a, 0x28) : 0); }
    else SU32(a, 0x30, 0);
    if (version <= 7) (void)rx_stream_u32(s);
    if (version > 10) {
        /* one block for both track arrays and their keys: tracks (0x14 each) and positions (0x18 each), 16-aligned, then the keys */
        (void)rx_stream_u32(s); (void)rx_stream_u32(s);
        uint32_t keys = rx_stream_u32(s);
        uint32_t ta = U32(a, 0x18) * 0x14u; ta += pad16(ta);
        uint32_t tb = U32(a, 0x20) * 0x18u; tb += pad16(tb);
        uint32_t total = tb + ta + keys;
        rx_addr blk = rx_crt_aligned_malloc(total, 16);
        SU32(a, 0x58, blk); SU32(a, 0x60, total);
        if (total > U32(0, 0x00a95954)) SU32(0, 0x00a95954, total);   /* the largest anim block so far (a global statistic) */
        if (I32(a, 0x18) > 0) {
            uint32_t f = U32(a, 0x5c);
            SU32(a, 0x1c, blk + f); SU32(a, 0x5c, f + ta);
            for (int32_t i = 0; i < I32(a, 0x18); i++) {
                uint32_t o = 0x14u * (uint32_t)i;
                rx_addr e = U32(a, 0x1c) + o;
                SU32(e, 0x00, 0); SU32(e, 0x04, 0); SU32(e, 0x0c, 0); SU32(e, 0x10, 0); SU32(e, 0x08, 1);
                rx_stream_read(s, U32(a, 0x1c) + o + 0xc, 4);
                rx_stream_read(s, U32(a, 0x1c) + o + 4, 4);
                SU32(U32(a, 0x1c) + o, 0, U32(a, 0x58) + U32(a, 0x5c));
                e = U32(a, 0x1c) + o;
                uint32_t sz = U32(e, 0xc) << 4;
                SU32(a, 0x5c, U32(a, 0x5c) + pad16(sz) + sz);
                rx_stream_read(s, U32(e, 0), U32(e, 0xc) << 4);
            }
        }
        if (I32(a, 0x20) > 0) {
            uint32_t f = U32(a, 0x5c);
            SU32(a, 0x24, U32(a, 0x58) + f); SU32(a, 0x5c, f + tb);
            for (int32_t i = 0; i < I32(a, 0x20); i++) {
                uint32_t o = 0x18u * (uint32_t)i;
                rx_addr e = U32(a, 0x24) + o;
                SU32(e, 0x08, 0); SU32(e, 0x0c, 1); SU32(e, 0x10, 0); SU32(e, 0x14, 0); SU32(e, 0x00, 0); SU32(e, 0x04, 1);
                rx_stream_read(s, U32(a, 0x24) + o + 0x10, 4);
                rx_stream_read(s, U32(a, 0x24) + o + 8, 4);
                SU32(U32(a, 0x24) + o, 0, U32(a, 0x58) + U32(a, 0x5c));
                e = U32(a, 0x24) + o;
                uint32_t sz = U32(e, 0x10) * 12u;
                SU32(a, 0x5c, U32(a, 0x5c) + pad16(sz) + sz);
                rx_stream_read(s, U32(e, 0), U32(e, 0x10) * 12u);
            }
        }
    } else {
        /* separate arrays: new T[n] with a count, each track's keys reallocated (the element pointers are 0: a plain allocation) */
        int32_t n1 = I32(a, 0x18);
        if (n1 > 0) {
            rx_addr p = rx_reflex_alloc_array((uint32_t)n1, 0x14);
            if (p) for (int32_t k = 0; k < n1; k++) {   /* `vector constructor iterator` with 00839ea0 */
                rx_addr e = p + 0x14u * (uint32_t)k;
                SU32(e, 0x00, 0); SU32(e, 0x04, 0); SU32(e, 0x08, 1); SU32(e, 0x0c, 0); SU32(e, 0x10, 0);
            }
            SU32(a, 0x1c, p);
            for (int32_t i = 0; i < I32(a, 0x18); i++) {
                uint32_t o = 0x14u * (uint32_t)i;
                rx_stream_read(s, U32(a, 0x1c) + o + 0xc, 4);
                rx_stream_read(s, U32(a, 0x1c) + o + 4, 4);
                rx_addr e = U32(a, 0x1c) + o;
                uint32_t cnt = U32(e, 0xc); rx_addr old = U32(e, 0);
                SU32(e, 0x10, cnt);
                SU32(e, 0, rx_crt_aligned_realloc(old, cnt << 4, 16));
                rx_stream_read(s, U32(U32(a, 0x1c) + o, 0), U32(U32(a, 0x1c) + o, 0xc) << 4);
            }
        }
        int32_t n2 = I32(a, 0x20);
        if (n2 > 0) {
            rx_addr p = rx_reflex_alloc_array((uint32_t)n2, 0x18);
            if (p) for (int32_t k = n2 - 1, i = 0; k >= 0; k--, i++) {
                rx_addr e = p + 0x18u * (uint32_t)i;
                SU32(e, 0x08, 0); SU32(e, 0x0c, 1); SU32(e, 0x10, 0); SU32(e, 0x14, 0); SU32(e, 0x00, 0); SU32(e, 0x04, 1);
            }
            SU32(a, 0x24, p);
            for (int32_t i = 0; i < I32(a, 0x20); i++) {
                uint32_t o = 0x18u * (uint32_t)i;
                rx_stream_read(s, U32(a, 0x24) + o + 0x10, 4);
                rx_stream_read(s, U32(a, 0x24) + o + 8, 4);
                rx_addr e = U32(a, 0x24) + o;
                uint32_t cnt = U32(e, 0x10); rx_addr old = U32(e, 0);
                SU32(e, 0x14, cnt);
                SU32(e, 0, rx_crt_aligned_realloc(old, cnt * 12u, 16));
                rx_stream_read(s, U32(U32(a, 0x24) + o, 0), U32(U32(a, 0x24) + o, 0x10) * 12u);
            }
        }
    }
    /* 0x82c59f: per-bone key tracks (version >= 9): K key times, per time one value per bone (scaled by 0.01) */
    if (key_bones > 0) {
        uint32_t nk = rx_stream_u32(s);
        rx_stream_read(s, a + 0x3c, 4);
        SU32(a, 0x40, new_tracks_14(U32(a, 0x3c)));
        for (int32_t i = 0; i < I32(a, 0x3c); i++) {
            uint32_t o = 0x14u * (uint32_t)i;
            SU32(U32(a, 0x40) + o, 8, nk);
            rx_addr e = U32(a, 0x40) + o;
            rx_addr old = U32(e, 0x10); uint32_t cnt = U32(e, 8);
            SU32(e, 0x0c, cnt);
            SU32(e, 0x10, rx_crt_aligned_realloc(old, cnt * 8u, 16));
        }
        for (int32_t j = 0; j < (int32_t)nk; j++) {
            uint32_t t = rx_stream_u32(s);
            for (int32_t b = 0; b < I32(a, 0x3c); b++) {
                float x = rx_muls(rx_stream_f32(s), 0.00999999977648258209f /* 0x009322ac */);
                rx_addr e = U32(a, 0x40) + 0x14u * (uint32_t)b;
                SF32(U32(e, 0x10) + 8u * (uint32_t)j, 4, x);
                SU32(U32(e, 0x10) + 8u * (uint32_t)j, 0, t);
            }
        }
    }
    /* 0x82c74d: float-key tracks at +0x2c (count +0x28); the names are freed after them */
    if (I32(a, 0x28) > 0) {
        SU32(a, 0x2c, new_tracks_14(U32(a, 0x28)));
        read_pair_tracks(a, 0x2c, 0x28, s);
        rx_crt_aligned_free(U32(a, 0x14));
        SU32(a, 0x14, 0);
    }
    /* 0x82c918: two float-key track sets at +0x34 / +0x38 (count +0x30) */
    if (I32(a, 0x30) > 0) {
        SU32(a, 0x34, new_tracks_14(U32(a, 0x30)));
        read_pair_tracks(a, 0x34, 0x30, s);
        SU32(a, 0x38, new_tracks_14(U32(a, 0x30)));
        read_pair_tracks(a, 0x38, 0x30, s);
        rx_crt_aligned_free(U32(a, 0x14));
        SU32(a, 0x14, 0);
    }
    /* 0x82cc35 */
    float blend = rx_muls(F32(a, 0x4c), 0.25f /* 0x0092ff84 */);
    SF32(a, 0x50, blend);
    if (blend > 0.5f /* 0x009322cc */) SF32(a, 0x50, 0.5f);
}

/* AnimAssetIO::vf3 0082cc70 (stdcall, 5 arguments: the stream first, the out pointer last) */
rx_addr rx_anim_asset_io_create(rx_stream* s) {
    rx_addr a = rx_reflex_aligned_alloc(0x68);
    if (!a) { rx_anim_asset_load(0, s); return 0; }
    a = rx_anim_asset_construct(a);
    rx_anim_asset_load(a, s);
    return a;
}
