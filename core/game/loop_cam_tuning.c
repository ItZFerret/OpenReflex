/* loop_cam_tuning.c -- the camera tuning XML parser (miscport): 0065c200 reads the <camera> file's presets into the tuning table
 * (0x1da80 bytes per slot, 0x404 per (camera type * 2 + first / side-scroll) entry: +0x400 the count, the entries' name pointers
 * from +0; a name points at a record {+0 the name, +0x40 the kind (0 .. 5), +0x44 the data}), called by the tuning loader at start-up
 * (0065bdb0) and by the developer's XML reload (00646a50, loop_cam_player.c). 0065c070 classifies the preset's camera name.
 * The reader is the game's XMLReader (guest object; its virtual slots as build_vehicle_sim.c's XR_ macros: 4 root, 0x20 first
 * child, 0x24 release, 0x28 next, 0xa0 name, 0xcc floats, 0xc0 vec4s, 0xe8 attribute, 0x13c attribute string). */
#include "loop_cam_tuning.h"
#include "crt.h"
#include "build_vehicle_int.h"
#include "rx_vcall.h"
#include "anim_extern.h"

/* the compiler's inlined strcmp (two bytes per step): 0 equal, else the sign of the first differing byte */
static int tune_strcmp(rx_addr a, rx_addr b) {
    for (uint32_t i = 0;; i++) {
        const uint8_t x = U8(a, i), y = U8(b, i);
        if (x != y) return x < y ? -1 : 1;
        if (x == 0) return 0;
    }
}

/* 0065c070 (ESI = the name; plain ret) -> EAX: FirstPerson 0, Visual 1, GamePlay 2 (0x941b28), QuickLeft 3, QuickRight 4, QuickBack 5
 * (the last match of the names tested in this order wins); "Wreck" (0x958414) returns 6 at once; no match -> 2. The names are
 * the game's strings 0x958980 / 0x9589b8 / 0x941b28 / 0x958a60 / 0x958a6c / 0x958a78. */
int32_t rx_cam_tuning_preset_type(rx_addr name) {
    int32_t r = -1;
    if (tune_strcmp(name, 0x00958980) == 0) r = 0;
    if (tune_strcmp(name, 0x009589b8) == 0) r = 1;
    if (tune_strcmp(name, 0x00941b28) == 0) r = 2;
    if (tune_strcmp(name, 0x00958a60) == 0) r = 3;
    if (tune_strcmp(name, 0x00958a6c) == 0) r = 4;
    if (tune_strcmp(name, 0x00958a78) == 0) r = 5;
    if (tune_strcmp(name, 0x00958414) == 0) return 6;
    return r == -1 ? 2 : r;
}
RX_FUNCTION(0x0065c070, rx_cam_tuning_preset_type)

/* the reader's virtual slots */
#define R_ROOT(R)              ((rx_addr)rx_vcall((R), 0x04, 0, NULL))
#define R_NAME(R, n, buf)      do { const uint32_t a_[3] = {(n), (buf), 0x800}; (void)rx_vcall((R), 0xa0, 3, a_); } while (0)
static rx_addr r_attr(rx_addr R, rx_addr node, rx_addr name) { const uint32_t a[2] = {node, name}; return rx_vcall(R, 0xe8, 2, a); }
static void r_attr_string(rx_addr R, rx_addr attr, rx_addr buf, uint32_t size) { const uint32_t a[3] = {attr, buf, size}; (void)rx_vcall(R, 0x13c, 3, a); }
static rx_addr r_first_child(rx_addr R, rx_addr node, rx_addr it) { const uint32_t a[3] = {node, 0, it}; return rx_vcall(R, 0x20, 3, a); }
static rx_addr r_next(rx_addr R, uint32_t it_value) { const uint32_t a[1] = {it_value}; return rx_vcall(R, 0x28, 1, a); }
static void r_floats(rx_addr R, rx_addr node, rx_addr dst, uint32_t k) { const uint32_t a[3] = {node, dst, k}; (void)rx_vcall(R, 0xcc, 3, a); }
static void r_vec4s(rx_addr R, rx_addr node, rx_addr dst, uint32_t k) { const uint32_t a[3] = {node, dst, k}; (void)rx_vcall(R, 0xc0, 3, a); }

/* 0065c200 (stdcall table, reader; RET 8) -> EAX = 1 (the file was a <camera> tuning file, 0 when not), ECX = the security cookie
 * (0x9b45e4: __security_check_cookie's xor leaves it). The root's name attribute (0x93b470) must be "camera" (0x95a2cc); then,
 * walking the root's children: a child's name -> BUF_A, its preset attribute (0x94111c, 64 bytes -> BUF_P) -> the preset's camera
 * type (0065c070) and, from the name containing "FS_" (first) or "SS_" (side-scroll; default first), the table entry
 * table + 0x404 * (type * 2 + side) (+0x400 the count); every record of that entry whose name equals the child's (case-insensitive)
 * takes the child's values by its kind (+0x40): 0: one float (+0x44, vf51); 1: one vec4 (+0x50, vf48); 3: up to three floats at
 * +0x44 over the following children of the same name (the missing ones repeat the first); 4: up to three vec4s at +0x50 + 0x10 * i
 * (the missing ones repeat the first); 5: up to three 0x5c-byte curves (0048bd20) at +0x44 (the missing ones repeat the first);
 * 2: one curve at +0x44, scaled by 1.466667 (0x97771c) when it is one of the four speed tables of the table's slot (+0x126f4 /
 * +0x12fb4 / +0x183b4 / +0x18c74 + 0xa0 * entry index); the name buffer then takes the next child's name (strncpy 0x7ff) and the
 * search of the entry continues with it; without a match the next child is taken. The reader's iterator is released at the end.
 * Frame: push ebp; and esp, -16; alloca 0x1844; push ebx / esi / edi: S = A - 0x1850: +0x10 the "match seen" flag, +0x20 the node,
 * +0x28 the iterator, +0x40 the table argument, BUF_A (the name) +0x48, BUF_N (the next name) +0x848, BUF_P (the preset) +0x1048. */
uint32_t rx_cam_tuning_parse(rx_addr table, rx_addr R) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4) & ~15u;
    const rx_addr S = A - 0x1844 - 12;
    const rx_addr BUF_A = S + 0x48, BUF_N = S + 0x848, BUF_P = S + 0x1048, IT = S + 0x28;
    rx_set_sp(S);
    SU32(A, -4, U32(0, 0x009b45e4) ^ (A - 0x1844));            /* the cookie at [esp + 0x1840] of the frame after the alloca */
    SU32(S, 0x40, table);
    uint32_t ret = 0;
    rx_addr edi;
    if (R_ROOT(R) == 0) goto done;
    R_NAME(R, R_ROOT(R), BUF_A);
    r_attr_string(R, r_attr(R, R_ROOT(R), 0x0093b470), BUF_A, 0x40);
    if (tune_strcmp(BUF_A, 0x0095a2cc /* "camera" */) != 0) goto done;
    edi = r_first_child(R, R_ROOT(R), IT);
    SU32(S, 0x20, edi);
    if (edi == 0) goto l93d;
    for (;;) {                                                  /* 0065c310: one child node (edi) */
        R_NAME(R, edi, BUF_A);
        r_attr_string(R, r_attr(R, edi, 0x0094111c), BUF_P, 0x40);
        const int32_t ptype = rx_cam_tuning_preset_type(BUF_P);
        int32_t side = -1;
        if (crt_strstr(BUF_A, 0x0095a2c4 /* "FS_" */) != 0) side = 0;
        if (crt_strstr(BUF_A, 0x0095a2c8 /* "SS_" */) != 0) side = 1; else if (side == -1) side = 0;
        const uint32_t index = (uint32_t)side + (uint32_t)ptype * 2u;
        const rx_addr entry = table + index * 0x404u;
        uint32_t k = 0;
        SU32(S, 0x10, 0);
        if (U32(entry, 0x400) == 0) goto l91d;
        {
            rx_addr p = entry;
            do {                                                /* 0065c3d8: the entry's records */
                const rx_addr rec = U32(p, 0);
                if (crt_stricmp(BUF_A, rec) == 0) {
                    uint32_t cnt = 0;
                    const uint32_t kind = U32(rec, 0x40);
                    if (kind == 0 || kind == 1) {               /* one float / one vec4, the next child's name follows */
                        if (kind == 0) r_floats(R, edi, rec + 0x44, 1); else r_vec4s(R, edi, rec + 0x50, 1);
                        edi = r_next(R, U32(IT, 0)); SU32(S, 0x20, edi);
                        if (edi == 0) goto l912;
                        R_NAME(R, edi, BUF_N);
                    } else if (kind == 3) {                     /* up to three floats over the children of this name */
                        rx_addr dst = rec + 0x44;
                        for (;;) {
                            if ((int32_t)cnt < 3) r_floats(R, edi, dst, 1);
                            edi = r_next(R, U32(IT, 0));
                            dst += 4; cnt++;
                            if (edi == 0) { SU32(S, 0x20, 0); break; }
                            R_NAME(R, edi, BUF_N);
                            if (tune_strcmp(BUF_A, BUF_N) != 0) { SU32(S, 0x20, edi); break; }
                        }
                        if ((int32_t)(3 - cnt) >= 4) {          /* (cnt >= 1: not reached) four copies of the first value, the count advanced by 4 */
                            cnt += 4;
                            for (uint32_t q = 0; q < 4; q++) anim_x87_copy_f32(rec + 0x34 + 4 * cnt + 4 * q, rec + 0x44);
                        }
                        if (cnt < 3) for (uint32_t q = 0; q < 3 - cnt; q++) anim_x87_copy_f32(rec + 0x44 + 4 * (cnt + q), rec + 0x44);
                    } else if (kind == 4) {                     /* up to three vec4s over the children of this name */
                        rx_addr dst = rec + 0x50;
                        for (;;) {
                            if ((int32_t)cnt < 3) r_vec4s(R, edi, dst, 1);
                            edi = r_next(R, U32(IT, 0));
                            cnt++; dst += 0x10;
                            if (edi == 0) { SU32(S, 0x20, 0); break; }
                            R_NAME(R, edi, BUF_N);
                            if (tune_strcmp(BUF_A, BUF_N) != 0) { SU32(S, 0x20, edi); break; }
                        }
                        if (cnt < 3) for (uint32_t q = 0; q < 3 - cnt; q++) rx_st4(rec + (cnt + q + 5) * 0x10, rx_ld4(rec + 0x50));
                    } else if (kind == 2) {                     /* one curve, scaled when it is a speed table of the slot */
                        const rx_addr curve = rec + 0x44;
                        rx_set_sp(S - 8);
                        rx_veh_read_curve(R, U32(S, 0x20), curve);          /* 0048bd20 (EDI = curve, ESI = the reader; stack node) */
                        rx_set_sp(S);
                        const rx_addr base = index * 0xa0u + table;
                        if (curve == base + 0x126f4 || curve == base + 0x12fb4 || curve == base + 0x183b4 || curve == base + 0x18c74) {
                            const int32_t n = I32(curve, 0);
                            const float scale = F32(0, 0x0097771c);
                            for (int32_t i = 0; i < n; i++) SF32(curve, 4u + 4u * (uint32_t)i, F32(curve, 4u + 4u * (uint32_t)i) * scale);
                        }
                        const rx_addr nx = r_next(R, U32(IT, 0));
                        SU32(S, 0x20, nx);
                        if (nx == 0) goto l912;
                        R_NAME(R, nx, BUF_N);
                        crt_strncpy(BUF_A, BUF_N, 0x7ff);
                        edi = rec;                              /* mov edi, [esp + 0x2c] */
                        goto l8e1;
                    } else if (kind == 5) {                     /* up to three 0x5c-byte curves */
                        rx_addr dst = rec + 0x44;
                        for (;;) {
                            if ((int32_t)cnt < 3) { rx_set_sp(S - 8); rx_veh_read_curve(R, U32(S, 0x20), dst); rx_set_sp(S); }
                            edi = r_next(R, U32(IT, 0));
                            cnt++; dst += 0x5c;
                            SU32(S, 0x20, edi);
                            if (edi == 0) break;
                            R_NAME(R, edi, BUF_N);
                            if (tune_strcmp(BUF_A, BUF_N) != 0) break;
                        }
                        if (cnt < 3) {
                            for (uint32_t q = 0; q < 3 - cnt; q++)
                                for (uint32_t w = 0; w < 0x17; w++) SU32(rec + 0x44 + 0x5c * (cnt + q), 4 * w, U32(rec + 0x44, 4 * w));
                            edi = U32(S, 0x20);
                        }
                    } else goto next_record;
                    crt_strncpy(BUF_A, BUF_N, 0x7ff);           /* 0065c8c9 */
                l8e1:
                    SU32(S, 0x10, 1);
                    SU8(S, 0x853, 0);
                }
            next_record:
                p += 4; k++;
            } while (k < U32(entry, 0x400));
        }
    l912:
        edi = U32(S, 0x20);
        if (U32(S, 0x10) != 0) goto l935;
    l91d:
        if (edi == 0) goto l93d;
        edi = r_next(R, U32(IT, 0)); SU32(S, 0x20, edi);
    l935:
        if (edi == 0) goto l93d;
    }
l93d:
    { const uint32_t a[1] = {U32(IT, 0)}; (void)rx_vcall(R, 0x24, 1, a); }
    ret = 1;
done:
    rx_set_sp(sp0);
    return ret;
}
RX_FUNCTION(0x0065c200, rx_cam_tuning_parse)
