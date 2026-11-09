/* build_activity_util.h -- internal helpers of construction unit 8 (build_activity*.c): frames at the original's stack addresses,
 * calls of core callees at the original's depth, the UI boundary (act_ui_*), the Rdb helpers the event code uses. Not an API. */
#pragma once
#include "build_activity.h"
#include "build_tables.h"
#include "build_vehicle.h"
#include "build_alloc.h"
#include "crt.h"
#include <stdio.h>
#include <stdlib.h>

/* ---- frames (as build_tables_rdb.c's TF_*): F = the original's ESP after its prologue, from the entry ESP rx_sp() ---- */
#define ACT_FRAME(F, sub, pushes) const rx_addr F##_sp = rx_sp(); const rx_addr F = F##_sp - (sub) - 4u * (pushes); rx_set_sp(F)
#define ACT_FRAME_EBP(F, align, sub, pushes) const rx_addr F##_sp = rx_sp(); \
    const rx_addr F = ((F##_sp - 4) & ~((rx_addr)(align) - 1)) - (sub) - 4u * (pushes); rx_set_sp(F)
#define ACT_END(F) rx_set_sp(F##_sp)
/* a core callee with n stack arguments, entered at the original's ESP (current stack pointer = the caller's ESP before the pushes) */
#define ACT_CALL(nargs, call) do { const rx_addr sp_ = rx_sp(); rx_set_sp(sp_ - 4u * (nargs) - 4u); call; rx_set_sp(sp_); } while (0)
#define ACT_CALLR(var, nargs, call) do { const rx_addr sp_ = rx_sp(); rx_set_sp(sp_ - 4u * (nargs) - 4u); var = (call); rx_set_sp(sp_); } while (0)

static inline void act_unreached(uint32_t addr, const char* what) {
    fprintf(stderr, "rxcore: build_activity: %08x %s: path not in the reference boot (not ported)\n", addr, what);
    abort();
}

/* ---- the UI boundary ----
 * UI-only code (string tables, pages, widgets, the Lua) is not core. Where the original's event code calls it, the core calls
 * act_ui_call: the oracle build runs the original (so a Verify of the calling function sees the same memory), the product does
 * nothing. Each use names what the UI call does and that no gameplay code reads its result. */
#ifdef RX_ORACLE
static inline uint32_t act_ui_call(rx_addr fn, enum rx_conv conv, uint32_t ecx, int n, const uint32_t* a) { return rx_call_original(fn, conv, ecx, n, a); }
static inline void act_ui_call_ex(rx_addr fn, const rx_regs* in, rx_regs* out, int n, const uint32_t* a) { rx_call_original_ex(fn, in, out, n, a); }
#else
static inline uint32_t act_ui_call(rx_addr fn, enum rx_conv conv, uint32_t ecx, int n, const uint32_t* a) { (void)fn; (void)conv; (void)ecx; (void)n; (void)a; return 0; }
static inline void act_ui_call_ex(rx_addr fn, const rx_regs* in, rx_regs* out, int n, const uint32_t* a) { (void)fn; (void)n; (void)a; *out = *in; }
#endif

/* ---- tables ---- */
#define ACT_PLAYER_LIST  0x00ae0e08u   /* "Player List" (static PlayerTable) */
#define ACT_VEHICLE_LIST 0x00ae05c8u   /* "Vehicle List" */
#define ACT_RIDER_LIST   0x00ae19b0u   /* "Rider List" */
#define ACT_ACTIVITY_TBL 0x00ae1b18u   /* the activity table (ActivityType / ActivityArea / ActivityMode / ...) */
/* the column object of a table by name (Table_ColumnIndex + the column vector) */
static inline rx_addr act_column(rx_addr table, rx_addr name) {
    uint32_t c = rx_tbl_column_index(table, name);
    if (c >= (U32(table, 0x14) - U32(table, 0x10)) >> 2) act_unreached(0x00916224, "column index (_invalid_parameter_noinfo)");
    return U32(U32(table, 0x10), 4 * c);
}
/* an int cell by column name (Table_ColumnIndex + 0041aaf0) */
static inline uint32_t act_cell(rx_addr table, rx_addr name, uint32_t row) {
    return U32(rx_tbl_cell_word(table, rx_tbl_column_index(table, name), row), 0);
}
/* the first row of a std::set<RowRef> (begin = the head's left) */
#define LEFT_OF(set) U32(U32((set), 0x18), 0)
/* the inlined strcpy (byte loop, the terminator included) */
static inline void act_strcpy(rx_addr d, rx_addr s) { for (uint32_t i = 0;; i++) { uint8_t c = U8(s, i); SU8(d, i, c); if (!c) break; } }

/* ---- calls of functions by address (a virtual's or another unit's): the core's when registered (at the original's depth), else
 * the original (oracle only; in the product an unregistered address is fatal). thiscall: ecx = this; stdcall / cdecl: ecx unused ---- */
static inline uint32_t act_call_fn(rx_addr fn, enum rx_conv conv, uint32_t ecx, int n, const uint32_t* a) {
    void* f = RX_FN_INTS(fn, n + (conv == RX_THISCALL));
    if (!f) return rx_call_original(fn, conv, ecx, n, a);
    uint32_t r = 0;
    const int self = conv == RX_THISCALL;
    if (self) switch (n) {
        case 0: ACT_CALLR(r, 0, ((uint32_t (*)(uint32_t))f)(ecx)); break;
        case 1: ACT_CALLR(r, 1, ((uint32_t (*)(uint32_t, uint32_t))f)(ecx, a[0])); break;
        case 2: ACT_CALLR(r, 2, ((uint32_t (*)(uint32_t, uint32_t, uint32_t))f)(ecx, a[0], a[1])); break;
        case 3: ACT_CALLR(r, 3, ((uint32_t (*)(uint32_t, uint32_t, uint32_t, uint32_t))f)(ecx, a[0], a[1], a[2])); break;
        case 4: ACT_CALLR(r, 4, ((uint32_t (*)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t))f)(ecx, a[0], a[1], a[2], a[3])); break;
        case 5: ACT_CALLR(r, 5, ((uint32_t (*)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t))f)(ecx, a[0], a[1], a[2], a[3], a[4])); break;
        default: act_unreached(fn, "a call with more than 5 arguments");
    } else switch (n) {
        case 0: ACT_CALLR(r, 0, ((uint32_t (*)(void))f)()); break;
        case 1: ACT_CALLR(r, 1, ((uint32_t (*)(uint32_t))f)(a[0])); break;
        case 2: ACT_CALLR(r, 2, ((uint32_t (*)(uint32_t, uint32_t))f)(a[0], a[1])); break;
        case 3: ACT_CALLR(r, 3, ((uint32_t (*)(uint32_t, uint32_t, uint32_t))f)(a[0], a[1], a[2])); break;
        case 4: ACT_CALLR(r, 4, ((uint32_t (*)(uint32_t, uint32_t, uint32_t, uint32_t))f)(a[0], a[1], a[2], a[3])); break;
        case 5: ACT_CALLR(r, 5, ((uint32_t (*)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t))f)(a[0], a[1], a[2], a[3], a[4])); break;
        default: act_unreached(fn, "a call with more than 5 arguments");
    }
    return r;
}
/* a virtual of obj (slot = byte offset in the vtable), thiscall */
static inline uint32_t act_vcall(rx_addr obj, uint32_t slot, int n, const uint32_t* a) {
    return act_call_fn(U32(U32(obj, 0), slot), RX_THISCALL, obj, n, a);
}
/* the same for functions whose (last) argument is a float (the core's ports take it as a C float) */
static inline void act_vcall_f(rx_addr obj, uint32_t slot, float x) {
    const rx_addr fn = U32(U32(obj, 0), slot);
    void (*f)(rx_addr, float) = RX_FN(fn, void (*)(rx_addr, float));
    if (f) { ACT_CALL(1, f(obj, x)); return; }
    const uint32_t a = rx_fb(x);
    rx_call_original(fn, RX_THISCALL, obj, 1, &a);
}
static inline uint32_t act_call_f2(rx_addr fn, enum rx_conv conv, uint32_t a0, float x) {
    uint32_t (*f)(uint32_t, float) = RX_FN(fn, uint32_t (*)(uint32_t, float));
    if (f) { uint32_t r; ACT_CALLR(r, 2, f(a0, x)); return r; }
    const uint32_t a[2] = {a0, rx_fb(x)};
    return rx_call_original(fn, conv, 0, 2, a);
}
