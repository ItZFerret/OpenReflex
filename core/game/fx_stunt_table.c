/* fx_stunt_table.c -- the stunt descriptor table and its loaders (game init thread 0075ed60, 2026-10-21, agent "profilestats").
 *
 * The trick system's records: a 100-record table of 0xc0-byte StuntTable rows (0x00d5aad4 pointer / 0x00d5aad8 count /
 * 0x00d5aadc capacity / 0x00d5aae0 growth / 0x00d5aae4 maximum), the four per-tier vectors of row pointers (0x00d5aa94,
 * 0x00d5aa80, 0x00d5aef8, 0x00d5af0c: {pointer, count, capacity, growth, maximum}), and the "StuntTable" BXML asset of the
 * StuntDatabase package. The original builds them in its init thread: 006c8480 (the vectors), 006c8b70 (the asset rows into the
 * table + the tier buckets), 006ca5a0 (the aerial / ground input mappings, ported in fx_stunt_input.c).
 *
 * 006c8670 reads one <Stunt> row: the row's name attribute (lowercased into +0x00 through the game's tolower table
 * [0x009b77c8]); <Anim value> (+0x20); <LocString value> (its reader string pointer, +0x40); <Tier value> (+0x70);
 * <Points value> (+0x6c); <Difficulty value> (+0x78); <Aerial value> (+0x64); <Physics value> (+0x68, also the return
 * value: 1 = a valid row); <AerialOrGroundEquivalent value> (+0x44); <SteeringDisabled> (+0x98); <IgnoreBrake> (+0x94);
 * <IgnoreThrottle> (+0x90). A missing child is skipped; a missing name/Anim leaves the zeroed buffer.
 *
 * The row is built on the caller's stack (memset 0xc0), then copied into the table by 00622050 (the constructor's fields only:
 * +0x00, +0x20, +0x40, +0x44, +0x64..+0x78, +0x80..+0xb0). The record's fields outside that copy (+0x7c, +0x8c, +0xb4, +0xb8,
 * +0xbc) are NOT written by the load: they keep the table allocation's residue in the original too (verified in the oracle
 * world image: values like 5 / 0x24a2 that no loader stores) and nothing reads them. The port leaves the allocation's
 * contents as the product heap has them; a byte compare of the *written* fields is the acceptance (see the notes).
 */
#include "fx.h"
#include "fx_stunt_table.h"
#include "build_alloc.h"
#include "build_asset.h"
#include "build_xml.h"
#include "crt.h"
#include "build_rider.h"   /* rx_iarr_reserve 007cff20 */

#define TBL_PTR   0x00d5aad4u
#define TBL_COUNT 0x00d5aad8u
#define TBL_CAP   0x00d5aadcu
#define TBL_GROW  0x00d5aae0u
#define TBL_MAX   0x00d5aae4u

/* the four tier vectors: {ptr, size, capacity, growth, maximum} */
static const rx_addr k_tier_vec[4] = {0x00d5aa94u, 0x00d5aa80u, 0x00d5aef8u, 0x00d5af0cu};
#define VEC_PTR 0u
#define VEC_N   4u
#define VEC_CAP 8u
#define VEC_GRW 0xcu
#define VEC_MAX 0x10u

/* the asset / XML names (the originals' .rdata) */
#define NAME_STUNT_TABLE 0x0092fbe8u   /* "StuntTable" */
#define NAME_STUNT       0x0095eae0u   /* "Stunt" */
#define ATTR_NAME        0x009362a4u   /* "name" */
#define ATTR_VALUE       0x0094fab4u   /* "value" */
#define FMT_S            0x009419e4u   /* "%s" */
#define CHILD_ANIM       0x0095eb94u   /* "Anim" */
#define CHILD_LOCSTRING  0x009607c4u   /* "LocString" */
#define CHILD_TIER       0x0093e274u   /* "Tier" */
#define CHILD_POINTS     0x0094d6b8u   /* "Points" */
#define CHILD_DIFFICULTY 0x0096082cu   /* "Difficulty" */
#define CHILD_AERIAL     0x009607d0u   /* "Aerial" */
#define CHILD_PHYSICS    0x009607d8u   /* "Physics" */
#define CHILD_AGE        0x009607e0u   /* "AerialOrGroundEquivalent" */
#define CHILD_STEER_DIS  0x009607fcu   /* "SteeringDisabled" */
#define CHILD_IGN_BRAKE  0x00960810u   /* "IgnoreBrake" */
#define CHILD_IGN_THR    0x0096081cu   /* "IgnoreThrottle" */
#define TOLOWER_TABLE    0x009b77c8u
#define TAG_BXML         0x6c6d7862u

/* 00617e80 (__fastcall r): a fresh StuntTable record: the fields the constructor owns (see the header of the file) */
static void stunt_record_ctor(rx_addr r) {
    SU32(r, 0x80, 0); SU32(r, 0x84, 0); SU32(r, 0x88, 0);
    SU32(r, 0xa0, 0); SU32(r, 0xa4, 0); SU32(r, 0xa8, 0); SU32(r, 0xac, 0);
    SU32(r, 0x90, 0); SU32(r, 0x94, 0); SU32(r, 0x98, 0); SU32(r, 0x9c, 0);
    SU32(r, 0xb0, 0); SU8(r, 0, 0); SU8(r, 0x20, 0);
    SU32(r, 0x64, 0); SU32(r, 0x68, 0); SU32(r, 0x6c, 0); SU32(r, 0x70, 0);
    SF32(r, 0x78, 5.0f /* 0x40a00000 */); SU32(r, 0x40, 0);
}

/* 00622050 (ECX = source, EAX = destination): the row into the table (the constructed fields; +0x7c / +0x8c / +0xb4 / +0xb8 /
 * +0xbc are not copied - the same dead residue as the original) */
void rx_stunt_record_copy(rx_addr dst, rx_addr src) {
    for (uint32_t o = 0; o < 0x20; o += 4) { SU32(dst, o, U32(src, o)); SU32(dst, 0x20 + o, U32(src, 0x20 + o)); }
    SU32(dst, 0x40, U32(src, 0x40));
    for (uint32_t o = 0x44; o < 0x64; o += 4) SU32(dst, o, U32(src, o));
    for (uint32_t o = 0x64; o <= 0x78; o += 4) SU32(dst, o, U32(src, o));
    for (uint32_t o = 0x80; o <= 0x88; o += 4) SU32(dst, o, U32(src, o));
    for (uint32_t o = 0x90; o <= 0xb0; o += 4) SU32(dst, o, U32(src, o));
}
RX_FUNCTION(0x00622050, rx_stunt_record_copy)

/* 006cd6e0: the 100-record table allocation (the constructor iterator over 0xc0-byte records) */
void rx_stunt_table_alloc(void) {
    rx_addr p = rx_reflex_aligned_alloc(0x4b00);
    if (!p) p = 0;
    else for (uint32_t i = 0; i < 100; i++) stunt_record_ctor(p + 0xc0u * i);
    SU32(0, TBL_PTR, p);
    SU32(0, TBL_COUNT, 0);
    SU32(0, TBL_CAP, 100);
    SU32(0, TBL_GROW, 10);
    SU32(0, TBL_MAX, 100);
}
RX_FUNCTION(0x006cd6e0, rx_stunt_table_alloc)

/* 006ce1a0 (EDI = the new capacity): the descriptor table grows (newcap * 0xc0, every element constructed, the old
 * `capacity` records copied, the old block freed) */
void rx_stunt_descriptor_grow(uint32_t newcap) {
    const rx_addr old = U32(0, TBL_PTR);
    const uint32_t cap = U32(0, TBL_CAP);
    rx_addr d = rx_reflex_aligned_alloc(newcap * 0xc0u);
    if (!d) d = 0;
    else for (uint32_t i = 0; i < newcap; i++) stunt_record_ctor(d + 0xc0u * i);
    for (uint32_t o = 0; o < cap * 0xc0u; o += 4) SU32(d, o, U32(old, o));
    rx_reflex_aligned_free(old);
    SU32(0, TBL_PTR, d);
    SU32(0, TBL_CAP, newcap);
}
RX_FUNCTION(0x006ce1a0, rx_stunt_descriptor_grow)

/* the "%s" copy into a record field (the original's 0078fd60(buf, 0x1f, "%s", string)): the string's bytes, NUL terminated
 * within the field; the original's format wrapper leaves the tail unchanged */
static void stunt_strcpy(rx_addr dst, rx_addr src) {
    const rx_addr s = rx_sp();
    rx_set_sp(s - 0x10);
    SU32(s, -4, src);
    rx_format_buffer(dst, 0x1f, FMT_S, s - 4);
    rx_set_sp(s);
}

/* 006c8670 (ECX = the <Stunt> node, stack the reader, the row): one row; returns the Physics read (1 = a valid row) */
uint32_t rx_stunt_row_read(rx_addr node, rx_addr reader, rx_addr rec) {
    RX_FRAME_ENTER(F, 0x10);
    rx_addr a;
    a = rx_xml_vf_attr(reader, node, ATTR_NAME);
    if (a) {
        stunt_strcpy(rec, rx_xml_vf_attr_cstr(reader, a));
        for (rx_addr p = rec; U8(p, 0); p++) SU8(p, 0, U8(0, TOLOWER_TABLE + U8(p, 0)));
    }
    a = rx_xml_vf_child(reader, node, CHILD_ANIM);
    if (a) { rx_addr e = rx_xml_vf_attr(reader, a, ATTR_VALUE); if (e) stunt_strcpy(rec + 0x20, rx_xml_vf_attr_cstr(reader, e)); }
    a = rx_xml_vf_child(reader, node, CHILD_LOCSTRING);
    if (a) { rx_addr e = rx_xml_vf_attr(reader, a, ATTR_VALUE); if (e) SU32(rec, 0x40, rx_xml_vf_attr_cstr(reader, e)); }
    a = rx_xml_vf_child(reader, node, CHILD_TIER);
    if (a) { rx_addr e = rx_xml_vf_attr(reader, a, ATTR_VALUE); if (e) rx_xml_attr_ints(reader, e, rec + 0x70, 1); }
    a = rx_xml_vf_child(reader, node, CHILD_POINTS);
    if (a) { rx_addr e = rx_xml_vf_attr(reader, a, ATTR_VALUE); if (e) rx_xml_attr_ints(reader, e, rec + 0x6c, 1); }
    a = rx_xml_vf_child(reader, node, CHILD_DIFFICULTY);
    if (a) { rx_addr e = rx_xml_vf_attr(reader, a, ATTR_VALUE); if (e) rx_xml_attr_floats(reader, e, rec + 0x78, 1); }
    a = rx_xml_vf_child(reader, node, CHILD_AERIAL);
    if (a) { rx_addr e = rx_xml_vf_attr(reader, a, ATTR_VALUE); if (e) rx_xml_attr_bools(reader, e, rec + 0x64, 1); }
    uint32_t valid = 0;
    a = rx_xml_vf_child(reader, node, CHILD_PHYSICS);
    if (a) {
        rx_addr e = rx_xml_vf_attr(reader, a, ATTR_VALUE);
        if (e) valid = rx_xml_attr_bools(reader, e, rec + 0x68, 1);
    }
    a = rx_xml_vf_child(reader, node, CHILD_AGE);
    if (a) { rx_addr e = rx_xml_vf_attr(reader, a, ATTR_VALUE); if (e) stunt_strcpy(rec + 0x44, rx_xml_vf_attr_cstr(reader, e)); }
    a = rx_xml_vf_child(reader, node, CHILD_STEER_DIS);
    if (a) { rx_addr e = rx_xml_vf_attr(reader, a, ATTR_VALUE); if (e) rx_xml_attr_bools(reader, e, rec + 0x98, 1); }
    a = rx_xml_vf_child(reader, node, CHILD_IGN_BRAKE);
    if (a) { rx_addr e = rx_xml_vf_attr(reader, a, ATTR_VALUE); if (e) rx_xml_attr_bools(reader, e, rec + 0x94, 1); }
    a = rx_xml_vf_child(reader, node, CHILD_IGN_THR);
    if (a) { rx_addr e = rx_xml_vf_attr(reader, a, ATTR_VALUE); if (e) rx_xml_attr_bools(reader, e, rec + 0x90, 1); }
    RX_FRAME_LEAVE(F, 0x10);
    return valid;
}
RX_FUNCTION(0x006c8670, rx_stunt_row_read)

/* one vector insert: grow when full, store the pointer, count++ */
static void tier_vec_push(rx_addr vec, rx_addr rec) {
    const uint32_t n = U32(vec, VEC_N);
    if (n >= U32(vec, VEC_CAP)) rx_iarr_reserve(vec, U32(vec, VEC_CAP) + U32(vec, VEC_GRW));
    SU32(U32(vec, VEC_PTR), 4u * n, rec);
    SU32(vec, VEC_N, n + 1);
}

/* 006c8b70: the "StuntTable" asset rows into the table and the four tier buckets (the table must be allocated) */
uint32_t rx_stunt_table_load(void) {
    const rx_addr db = U32(U32(0, 0x00a95844), 8);
    if (!db) return 0;
    if (!rx_database_vf_has_handle(db, NAME_STUNT_TABLE, TAG_BXML, 0)) return 0;
    const rx_addr h = rx_database_vf_handle(db, NAME_STUNT_TABLE, TAG_BXML, 0, 1);
    if (!h) return 0;
    SU32(h, 0x4c, 1);
    const rx_addr reader = U32(h, 0x44) ? U32(h, 0x44) - 0x460 : 0;
    uint32_t added = 0;
    if (reader) {
        const rx_addr sp0 = rx_sp(), F = sp0 - 0xd0;   /* the row (0xc0) + the cursor / scratch */
        rx_set_sp(F);
        const rx_addr rec = F + 0x10, cur = F + 0;
        const rx_addr root = rx_xml_vf_root(reader);
        rx_addr node = rx_xml_vf_first(reader, root, NAME_STUNT, cur);
        while (node) {
            for (uint32_t o = 0; o < 0xc0; o += 4) SU32(rec, o, 0);
            if (rx_stunt_row_read(node, reader, rec)) {
                const uint32_t n = U32(0, TBL_COUNT);
                if (n >= U32(0, TBL_CAP)) rx_stunt_descriptor_grow(U32(0, TBL_CAP) + U32(0, TBL_GROW));
                SU32(rec, 0x74, added);                 /* the row index */
                rx_stunt_record_copy(U32(0, TBL_PTR) + (uint32_t)n * 0xc0u, rec);
                SU32(0, TBL_COUNT, n + 1);
                if (U32(rec, 0x64)) SU32(0, 0x00a942fc, U32(0, 0x00a942fc) + 1);   /* aerial rows */
                else SU32(0, 0x00a94300, U32(0, 0x00a94300) + 1);                   /* ground rows */
                added++;
            }
            node = rx_xml_vf_next(reader, U32(cur, 0));
        }
        rx_set_sp(sp0);
        SU32(h, 0x4c, 0);
        const uint32_t n = U32(0, TBL_COUNT);
        for (uint32_t i = 0; i < n; i++) {
            const rx_addr r = U32(0, TBL_PTR) + 0xc0u * i;
            const uint32_t tier = U32(r, 0x70);
            if (tier >= 1 && tier <= 4) tier_vec_push(k_tier_vec[tier - 1], r);
        }
    } else SU32(h, 0x4c, 0);
    return added;
}
RX_FUNCTION(0x006c8b70, rx_stunt_table_load)

/* 006c8480: the init thread's stunt setup: the descriptor table and the four (100-slot) per-tier pointer vectors */
void rx_stunt_tables_init(void) {
    rx_stunt_table_alloc();
    for (uint32_t v = 0; v < 4; v++) {
        const rx_addr vec = k_tier_vec[v];
        SU32(vec, VEC_PTR, rx_reflex_aligned_alloc(400));
        SU32(vec, VEC_N, 0);
        SU32(vec, VEC_CAP, 100);
        SU32(vec, VEC_GRW, 10);
        SU32(vec, VEC_MAX, 100);
    }
}
RX_FUNCTION(0x006c8480, rx_stunt_tables_init)
