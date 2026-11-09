/* fx_stunt_input.c -- the stunt input mappings and the gesture matcher (2026-10-21, agent "stuntgesture").
 *
 * The trick input machine (fx_stunt.c, 006c9510) turns the right stick's direction changes into a short gesture string (K + 0x30,
 * up to 3 characters '0'..'3', classified by 006cd2c0) and matches it against the player's stunt mappings. A mapping record is
 * 0x22 bytes: +0x00 the stunt's name (the StuntTable key, 0x18 bytes, as the <StuntMapping stunt="..."> attribute) and +0x18 the
 * gesture (10 bytes, the <StuntMapping input="..."> attribute). The matcher (006c9aa0) returns the record whose gesture equals
 * the input; the caller then looks the name up in the StuntTable (006c8570) to get the trick.
 *
 * The two tables are built in the game init thread (0075ed60 at 0075f103): 006c8480 (the StuntTable), 006c8b70 (its rows), then
 * 006ca5a0: it loads "AerialStuntInputMappings.bxml" into [0x00a94310] and "GroundStuntInputMappings.bxml" into [0x00a94314]
 * through 006ca3f0, which reads the file's <StuntMapping> nodes ("StuntMapping" root-level children) into a (count + 1) * 0x22
 * block whose last record is the all-zero terminator. In the shipped install the aerial file has 36 mappings (all 3-directions
 * over the classifier's four values '0'..'3') and the ground file none (its rows are commented out), so [0xa94314] is a
 * one-record terminator table; the product used to leave both pointers 0, where the matcher read address 0 (the arena's zero
 * page) and never matched.
 *
 * The inputs of the machine are here too: 00548810 (the look / gesture stick's pair from the pad's steer / lean KeyMaps, input 7)
 * and 006cd2c0 (the direction classifier: '0' up / '1' right / '2' down / '3' left / 'N' in the dead zone, the 3-character
 * buffer and its count / finished flag). The trick tracker itself (006c9510 / 006c9770) is fx_stunt.c. In the original the
 * gesture only starts while the "StuntModifier" composite is down (00548660, keyboard Space / pad LB by the remap defaults) and
 * the activity is InProgress; a mapped gesture then reaches the trick start 006c98c0 / the trick-mode handlers, which are not
 * ported yet (see core/notes/fx.md).
 *
 * Exactness: the frames are the originals' (006ca3f0: alloca 0x11c8, reader at sp - 0x11c0, the 100 x 0x22 record scratch at
 * sp - 0xd60, the "StuntMapping" name at sp - 0x14, the cursor at sp - 0x11c4); the loader's 100-record cap, its
 * (count + 1) x 0x22 ReflexAlignedAlloc with the constructor iterator 006c8e50 (a 0x22-byte zero fill), the count x 0x22 memcpy
 * and the explicit terminator clear; 006ca2f0's attribute order ("stunt" then "input"), its "%s" formatting with sizes 0x18 / 0xa
 * (0078fd60), and its return (both attributes present). The matcher compares two bytes per step and stops at the first NUL
 * (006ca9xx), returns the record base, and walks 0x22-byte records while the record's stunt-name byte is non-zero.
 *
 * The original's 006ca3f0 also calls FUN_00827e00 (the reader reset: its five _aligned_free(NULL) and zeroed fields) after the
 * node walk and FUN_00824fe0 (the destructor: the same frees for a reset reader, the live-reader count decrement) after the copy.
 * The port calls rx_xml_reader_destroy once, between the walk and the allocation: the same frees in the same order and the same
 * count decrement (the two original calls free nothing the second time), and the reader object is below the final ESP.
 */
#include "fx.h"
#include "fx_util.h"
#include "fx_stunt_input.h"
#include "build_alloc.h"
#include "build_xml.h"
#include "crt.h"
#include "world_misc.h"
#include "../src/fmt_vfs.h"
#include <math.h>
#include <string.h>

#define AIR_TABLE    0x00a94310u
#define GROUND_TABLE 0x00a94314u
#define NAME_AERIAL  0x00960914u   /* "AerialStuntInputMappings.bxml" (the 006ca5a0 stack copies' .rdata) */
#define NAME_GROUND  0x00960934u   /* "GroundStuntInputMappings.bxml" */
#define NODE_STUNT   0x00960904u   /* "StuntMapping" (the 006ca3f0 frame's name) */
#define ATTR_STUNT   0x009608f4u   /* "stunt" */
#define ATTR_INPUT   0x009608fcu   /* "input" */
#define FMT_S        0x009419e4u   /* "%s" */
#define REC_SIZE     0x22u
#define REC_MAX      100u
#define CTOR_006c8e50 0x006c8e50u

/* the product's install (006ca3f0's 00825260 opens the file at the install's root in the original; the oracle's reader path
 * ignores it, build_xml.c) */
static const struct fmt_vfs* g_stunt_vfs;

/* 006c8e50 (ECX = record): a fresh 0x22-byte mapping record (the vector-constructor iterator's element constructor) */
static rx_addr mapping_record_ctor(rx_addr r) {
    for (uint32_t o = 0; o < 0x20; o += 8) { SU32(r, o, 0); SU32(r, o + 4, 0); }
    SU16(r, 0x20, 0);
    return r;
}
RX_FUNCTION(0x006c8e50, mapping_record_ctor)

/* a game string into a host buffer (the file name the original hands to 00825260 as a char*) */
static void mapping_path(rx_addr name, char* out, uint32_t n) {
    uint32_t i = 0;
    for (; i + 1 < n && U8(name, i); i++) out[i] = (char)U8(name, i);
    out[i] = 0;
}

/* the 0078fd60 call of 006ca2f0: format "%s" into `dst` with the string at `src` (EDI = dst, ESI = size in the original; the
 * variadic argument in the game stack slot below the current SP, crt_format.c) */
static void mapping_strcpy(rx_addr dst, uint32_t size, rx_addr src) {
    const rx_addr s = rx_sp();
    rx_set_sp(s - 0x10);
    SU32(s, -4, src);
    rx_format_buffer(dst, size, FMT_S, s - 4);
    rx_set_sp(s);
}

/* ---- 006ca2f0 (ECX = record; stack the reader, the node; caller-cleaned): one <StuntMapping> node. The "stunt" attribute is
 * formatted into +0 (0x18 bytes), the "input" one into +0x18 (10 bytes); a missing "stunt" leaves +0 as the caller's memset put
 * it; the return value is 1 only when both exist (the original keeps it in the frame slot set to 1 before the first vf58 and
 * cleared on its failure). Frame: sub esp, 0x18, /GS at +0x14; ebx / ebp / esi / edi. */
uint32_t rx_fx_stunt_mapping_record(rx_addr rec, rx_addr reader, rx_addr node) {
    uint32_t both = 0;
    rx_addr a = rx_xml_vf_attr(reader, node, ATTR_STUNT);
    if (a) {
        mapping_strcpy(rec, 0x18, rx_xml_vf_attr_cstr(reader, a));
        both = 1;
    }
    a = rx_xml_vf_attr(reader, node, ATTR_INPUT);
    if (!a) return 0;
    mapping_strcpy(rec + 0x18, 0xa, rx_xml_vf_attr_cstr(reader, a));
    return both;
}
RX_FUNCTION(0x006ca2f0, rx_fx_stunt_mapping_record)

/* ---- 006ca3f0 (cdecl name; EAX = table or 0): the "StuntMapping" nodes of the file `name` (006ca5a0's stack file names) into
 * a fresh (count + 1) * 0x22 block; the last record is the zero terminator. Frame: alloca 0x11c8 (cookie at sp - 4; ebx / ebp /
 * esi / edi pushed); the reader at E0 - 0x11c0, the record scratch at E0 - 0xd60, the name at E0 - 0x14. */
rx_addr rx_fx_stunt_mapping_load(rx_addr name) {
    const rx_addr sp0 = rx_sp();
    const rx_addr F = sp0 - 0x11c8;
    const rx_addr reader = F + 8, records = F + 0x468, nodename = F + 0x11b4, cursor = F + 4;
    rx_set_sp(F);
    rx_xml_reader_construct(reader);
    char path[0x104];
    mapping_path(name, path, sizeof path);
    rx_addr table = 0;
    if (rx_xml_reader_load_file(g_stunt_vfs, reader, path)) {
        crt_memset(records, 0, REC_MAX * REC_SIZE);
        static const char node_name[] = "StuntMapping";
        for (uint32_t i = 0; i < sizeof node_name; i++) SU8(nodename, i, (uint8_t)node_name[i]);
        SU32(cursor, 0, 0xffffffffu);
        uint32_t count = 0;
        rx_addr node = rx_xml_vf_first(reader, U32(reader, 0x38), nodename, cursor);
        while (node) {
            rx_fx_stunt_mapping_record(records + REC_SIZE * count, reader, node);
            count++;
            if (count == REC_MAX) break;
            node = rx_xml_vf_next(reader, U32(cursor, 0));
        }
        rx_xml_reader_destroy(reader);                       /* FUN_00827e00 (the reset); FUN_00824fe0 below */
        const uint64_t sz = (uint64_t)(count + 1u) * REC_SIZE;
        const uint32_t want = (sz >> 32) != 0 ? 0xffffffffu : (uint32_t)sz;   /* the checked mul */
        table = rx_reflex_aligned_alloc(want);
        if (table) {
            rx_fx_vector_ctor_iterator(table, REC_SIZE, (int32_t)(count + 1u), CTOR_006c8e50);
            if (count) crt_memcpy(table, records, count * REC_SIZE);
            for (uint32_t o = 0; o < 0x20; o += 8) {
                SU32(table, count * REC_SIZE + o, 0); SU32(table, count * REC_SIZE + o + 4, 0);
            }
            SU16(table, count * REC_SIZE + 0x20, 0);
        }
    } else {
        rx_xml_reader_destroy(reader);                      /* FUN_00824fe0 */
    }
    rx_set_sp(sp0);
    return table;
}
RX_FUNCTION(0x006ca3f0, rx_fx_stunt_mapping_load)

/* ---- 006ca5a0: the init thread's stunt setup: the two mapping files, in order (aerial then ground), into the globals; returns
 * 1 when both pointers are non-zero. Frame: sub esp, 0x48 (cookie at +0x40); esi; the two file names on the frame at +0 and
 * +0x20. (The original also copies its 32-byte strings from .rdata 0x00960914 / 0x00960934.) */
uint32_t rx_fx_stunt_mappings_init(const struct fmt_vfs* vfs) {
    static const char aerial[] = "AerialStuntInputMappings.bxml";
    static const char ground[] = "GroundStuntInputMappings.bxml";
    const rx_addr sp0 = rx_sp();
    const rx_addr F = sp0 - 0x48;
    rx_set_sp(sp0 - 0x4c);                                  /* push esi */
    g_stunt_vfs = vfs;
    for (uint32_t i = 0; i < sizeof aerial; i++) SU8(F, i, (uint8_t)aerial[i]);
    for (uint32_t i = 0; i < sizeof ground; i++) SU8(F, 0x20 + i, (uint8_t)ground[i]);
    SU32(0, AIR_TABLE, rx_fx_stunt_mapping_load(F));
    SU32(0, GROUND_TABLE, rx_fx_stunt_mapping_load(F + 0x20));
    rx_set_sp(sp0);
    return U32(0, AIR_TABLE) != 0 && U32(0, GROUND_TABLE) != 0;
}
RX_FUNCTION(0x006ca5a0, rx_fx_stunt_mappings_init)

/* the original's two-byte-stride string equality (006c9ac0..006c9ae0): stops at the first NUL of either string */
static int gesture_eq(rx_addr a, rx_addr b) {
    for (;;) {
        const uint8_t x = U8(a, 0), y0 = U8(b, 0);
        if (x != y0) return 0;
        if (x == 0) return 1;
        const uint8_t y = U8(a, 1), z = U8(b, 1);
        if (y != z) return 0;
        a += 2; b += 2;
        if (y == 0) return 1;
    }
}

/* ---- 006c9aa0 (stdcall airborne; RET 4; EDI = the gesture): the table [0xa94310] when airborne, else [0xa94314]; walk its
 * 0x22-byte records while the record's first byte (its stunt name) is non-zero, compare the gesture with +0x18 two bytes at a
 * time, and return the first record that is equal (or 0). Frame: push ebx; push esi. */
rx_addr rx_fx_stunt_mapping_match(uint32_t airborne, rx_addr gesture) {
    rx_addr rec = U32(0, airborne != 0 ? AIR_TABLE : GROUND_TABLE);
    for (;;) {
        if (U8(rec, 0) == 0) return 0;
        if (gesture_eq(gesture, rec + 0x18)) return rec;
        rec += REC_SIZE;
    }
}
RX_FUNCTION(0x006c9aa0, rx_fx_stunt_mapping_match)

/* a KeyMap's value getter (vtable +0x10, input 7, the pad; x87 st0): the input system owns the classes (KeyMap 00811710,
 * Compound 00811b50, Cumulative 00811db0) and they are registered; the original runs only if a class is not. */
static float keymap_value(rx_addr km, uint32_t pad) {
    const rx_addr fn = U32(U32(km, 0), 0x10);
    float (*f)(rx_addr, uint32_t, uint32_t) = RX_FN(fn, float (*)(rx_addr, uint32_t, uint32_t));
    if (f) return f(km, 7, pad);
    rx_regs in; memset(&in, 0, sizeof in); in.r[FX_ECX] = km;
    rx_regs out; memset(&out, 0, sizeof out);
    const uint32_t a[2] = {7, pad};
    rx_call_original_ex(fn, &in, &out, 2, a);
    return out.st0;
}

/* ---- 00548810 (EAX = pad; stack &dir; plain ret): the look / gesture stick's x / y from the pad's steer map
 * (00548fc0 -> the KeyMap at +0x1f20 / +0x17d8 / ...) and its lean map (00549050), the value of input 7. Frame: push ebx / ebp /
 * esi / edi. */
void rx_fx_stunt_gesture_dir(uint32_t pad, rx_addr out) {
    const rx_addr steer = rx_world_pad_wreck_steer_map(pad);
    const rx_addr lean = rx_world_pad_wreck_lean_map(pad);
    SF32(out, 0, keymap_value(steer, pad));
    SF32(out, 4, keymap_value(lean, pad));
}
RX_FUNCTION(0x00548810, rx_fx_stunt_gesture_dir)

/* ---- 006cd2c0 (ESI = the input buffer K + 0x30; stack dt, dir.x, dir.y, airborne; RET 0x10): one gesture sample: the direction
 * char ('0' up, '1' right, '2' down, '3' left; 'N' inside [0x9caeb8]'s dead zone) into +4 (the previous one into +5), the two
 * clocks +0x10 += dt / +0x14 -= dt (airborne only, clamped to 0 by clearing the buffer / count), the direction into the buffer
 * when it changed (up to 3 characters, +0x1c = "finished" = count >= 3), or the whole gesture restarted when the buffer's +0xc
 * flag is set. Frame: push ecx. The float compares are the original's comiss / jbe + ja (NaN: 'N' out of the dead zone; the
 * clamp skips a NaN). */
void rx_fx_stunt_gesture_classify(rx_addr buf, float dt, float dx, float dy, uint32_t airborne) {
    const float dz = F32(0, 0x009caeb8);
    uint8_t c = 'N';
    SU8(buf, 5, U8(buf, 4));
    if (rx_adds(rx_muls(dx, dx), rx_muls(dy, dy)) > rx_muls(dz, dz)) {
        const float ax = fabsf(dx), ay = fabsf(dy);
        if (ay > ax) c = dy > 0.0f ? '0' : '2';
        else c = dx > 0.0f ? '1' : '3';
    }
    SF32(buf, 0x10, rx_adds(F32(buf, 0x10), dt));
    SU8(buf, 4, c);
    if (airborne != 0) SF32(buf, 0x14, rx_subs(F32(buf, 0x14), dt));
    if (F32(buf, 0x14) < 0.0f) {
        SF32(buf, 0x14, 0.0f);
        SU32(buf, 8, 0);
        SU32(buf, 0, 0);
    }
    SF32(buf, 0x20, dx);
    SF32(buf, 0x24, dy);
    if (U32(buf, 0xc) == 0) {
        if (c != U8(buf, 5) && c != 'N') {
            if (U32(buf, 8) < 3) {
                SU8(buf, U32(buf, 8), c);
                SU32(buf, 8, U32(buf, 8) + 1);
                SU32(buf, 0x1c, 0);
            } else SU32(buf, 0x1c, 1);
            SU32(buf, 0x10, 0);
        }
        SU32(buf, 0x1c, U32(buf, 8) >= 3);
    } else if (c != 'N') {
        SU32(buf, 8, 0);
        SU8(buf, 0, c);
        SU32(buf, 8, 1);
        SU32(buf, 0xc, 0);
        SU32(buf, 0x10, 0);
    }
}
RX_FUNCTION(0x006cd2c0, rx_fx_stunt_gesture_classify)
