/* fx_vector.c -- std::vector<T*> element helpers (4-byte elements) of the MSVC 9 library as the game instantiates them, and the
 * _Insert_n of the object registry's vector (006c8260). The helpers have register conventions (they were inlined into
 * several instantiations and stayed separate functions): 0042bf30 (ECX = element count: ReflexAlignedAlloc(4 n)), 00776d20
 * (std::_Umove: EAX = end, EDI = begin, EDX = destination: memmove_s, EAX = the destination's end), 007cfd80 (std::_Fill:
 * EDX = value*, ECX = end, stack: begin; cdecl; EAX = end). 006c8230 (std::_Uninitialized_fill_n) is fx_types.c's. Vector: +0 the
 * container proxy, +0xc begin, +0x10 end, +0x14 capacity end. A vector of 0x3fffffff elements throws in the original
 * (std::length_error / bad_alloc): unreachable, fatal here. The SEH record of 006c8260 is written to the frame; the TEB's
 * exception list head is restored at the end. notes/fx.md. */
#include <stdio.h>
#include <stdlib.h>
#include "fx.h"
#include "fx_util.h"
#include "build_alloc.h"

#define FX_FS0 rx_teb()    /* the TEB's exception list head (fs:[0]) */

static void fx_too_long(const char* what) { fprintf(stderr, "rxcore: %s (fx_vector.c)\n", what); abort(); }

/* ---- 0042bf30 (ECX = n; EAX): the block for n elements: ReflexAlignedAlloc(4 n); more than 0x3fffffff throws bad_alloc.
 * Frame: sub esp, 0x10. ---- */
rx_addr rx_fx_vector_alloc(uint32_t n) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 0x10);
    if (n > 0 && 0xffffffffu / n < 4) fx_too_long("vector too long (bad_alloc, 0042bf30)");
    const uint32_t a[1] = {n * 4u};
    const rx_addr s = fx_push(1, a, 0x0042bf46);
    const rx_addr r = rx_reflex_aligned_alloc(n * 4u);
    fx_pop(s);
    rx_set_sp(sp0);
    return r;
}
RX_FUNCTION(0x0042bf30, rx_fx_vector_alloc)

/* ---- 00776d20 (EAX = end, EDI = begin, EDX = destination; EAX = destination + bytes): memmove_s of [begin, end) when it is not
 * empty. Frame: push esi. ---- */
rx_addr rx_fx_vector_umove(rx_addr end, rx_addr begin, rx_addr dst) {
    const rx_addr sp0 = rx_sp();
    SU32(sp0, -4u, fx_reg(FX_ESI));
    const uint32_t n = (uint32_t)((int32_t)(end - begin) >> 2);
    const uint32_t bytes = n * 4u;
    if (n != 0) {
        rx_set_sp(sp0 - 4u);
        const uint32_t a[4] = {dst, bytes, begin, bytes};
        FX_CRT(4, a, 0x00776d3e, crt_memmove_s(dst, bytes, begin, bytes));
        rx_set_sp(sp0);
    }
    return dst + bytes;
}
RX_FUNCTION(0x00776d20, rx_fx_vector_umove)

/* ---- 007cfd80 (EDX = value*, ECX = end; stdcall-less begin; EAX = end): [begin, end) = *value. Frame (a non-empty range): push esi. ---- */
rx_addr rx_fx_vector_fill(rx_addr value, rx_addr end, rx_addr begin) {
    if (begin != end) {
        SU32(rx_sp(), -4u, fx_reg(FX_ESI));
        for (rx_addr p = begin; p != end; p += 4) SU32(p, 0, U32(value, 0));
    }
    return end;
}
RX_FUNCTION(0x007cfd80, rx_fx_vector_fill)

/* ---- 006c8260 (stdcall vector, count, value*, where proxy, where; RET 0x14): std::vector<T*>::_Insert_n(where, count, value):
 * nothing for 0 items; when the capacity is below size + count: a block of capacity * 1.5 (at least size + count), [begin,
 * where) moved, count copies of the value, [where, end) moved, the old block freed, the three pointers set; else in place: with
 * fewer than count items after where the tail moves up by count, the missing copies are appended at the old end and
 * [where, old end) is filled; else the last count items are appended, the middle moved up through memmove_s and [where, where +
 * count) filled. The value is copied into an argument slot first (the grow path keeps the capacity in [EBP + 8]: the slots are
 * written as the original does). Frame: push ebp; the SEH record (-1, handler 0x9024e0, fs:[0]); sub esp, 8; push ebx, esi, edi;
 * the cookie: E = sp0 - 0x28; EBP = sp0 - 4: [EBP - 4] the try level, [EBP - 0x14] the new block. ---- */
void rx_fx_vector_insert_n(rx_addr vec, uint32_t count, rx_addr value_p, rx_addr proxy, rx_addr where) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    const rx_addr ebp = sp0 - 4, E = sp0 - 0x28;
    const rx_addr s8 = sp0 + 4, sc = sp0 + 8, s10 = sp0 + 0xc;      /* [EBP + 8] / [EBP + 0xc] / [EBP + 0x10] */
    (void)proxy;
    SU32(ebp, 0, sv.ebp);
    fx_set_reg(FX_EBP, ebp);
    SU32(sp0, -8u, 0xffffffffu); SU32(sp0, -0xcu, 0x009024e0u);
    const uint32_t seh_prev = U32(FX_FS0, 0);
    SU32(sp0, -0x10u, seh_prev);
    SU32(sp0, -0x1cu, sv.ebx); SU32(sp0, -0x20u, sv.esi); SU32(sp0, -0x24u, sv.edi);
    SU32(sp0, -0x28u, U32(0, 0x009b45e4) ^ ebp);
    SU32(FX_FS0, 0, ebp - 0xc);
    SU32(ebp, -0x10u, E);
    rx_set_sp(E);
    fx_set_reg(FX_EBX, vec);
    const uint32_t begin = U32(vec, 0xc);
    uint32_t cap = begin == 0 ? 0 : (uint32_t)((int32_t)(U32(vec, 0x14) - begin) >> 2);
    if (count != 0) {
        const uint32_t end = U32(vec, 0x10);
        const uint32_t size = (uint32_t)((int32_t)(end - begin) >> 2);
        fx_set_reg(FX_ESI, end); fx_set_reg(FX_EDI, cap);
        if (0x3fffffffu - size < count) fx_too_long("vector too long (std::length_error, 00851560)");
        const uint32_t need = size + count;
        if (cap < need) {
            const uint32_t half = cap >> 1;
            if (0x3fffffffu - half < cap) { SU32(s8, 0, 0); cap = 0; } else { cap += half; SU32(s8, 0, cap); }
            if (cap < need) { SU32(s8, 0, need); cap = need; }
            fx_set_reg(FX_EDI, cap);
            rx_addr p;
            { fx_set_reg(FX_ECX, cap); const rx_addr s = fx_push(0, 0, 0x006c8303); p = rx_fx_vector_alloc(cap); fx_pop(s); }
            SU32(ebp, -0x14u, p);
            SU32(ebp, -4u, 0);
            rx_addr e1;
            { fx_set_reg(FX_EDI, U32(vec, 0xc)); const rx_addr s = fx_push(0, 0, 0x006c831a); e1 = rx_fx_vector_umove(where, U32(vec, 0xc), p); fx_pop(s); }
            fx_set_reg(FX_ESI, count); fx_set_reg(FX_EDI, e1);
            rx_addr e2;
            { const rx_addr s = fx_push(0, 0, 0x006c8327); e2 = rx_fx_fill_dwords(count, e1, value_p); fx_pop(s); }
            fx_set_reg(FX_EDI, where);
            { const rx_addr s = fx_push(0, 0, 0x006c8334); rx_fx_vector_umove(U32(vec, 0x10), where, e2); fx_pop(s); }
            SU32(ebp, -4u, 0xffffffffu);
            const rx_addr old = U32(vec, 0xc);
            const uint32_t total = count + (uint32_t)((int32_t)(U32(vec, 0x10) - old) >> 2);
            fx_set_reg(FX_ESI, total);
            if (old != 0) { const uint32_t a[1] = {old}; const rx_addr s = fx_push(1, a, 0x006c8352); rx_reflex_aligned_free(old); fx_pop(s); }
            SU32(vec, 0x14, p + U32(s8, 0) * 4u);
            SU32(vec, 0x10, p + total * 4u);
            SU32(vec, 0xc, p);
        } else {
            const uint32_t after = (uint32_t)((int32_t)(end - where) >> 2);
            fx_set_reg(FX_EDI, where);
            if (after < count) {
                SU32(s8, 0, U32(value_p, 0));
                const uint32_t bytes = count * 4u;
                SU32(s10, 0, bytes);
                { const rx_addr s = fx_push(0, 0, 0x006c83bd); rx_fx_vector_umove(end, where, where + bytes); fx_pop(s); }
                SU32(ebp, -4u, 2);
                const uint32_t e = U32(vec, 0x10);
                const uint32_t fillc = U32(sc, 0) - (uint32_t)((int32_t)(e - where) >> 2);
                fx_set_reg(FX_EDI, e); fx_set_reg(FX_ESI, fillc);
                { const rx_addr s = fx_push(0, 0, 0x006c83dc); rx_fx_fill_dwords(fillc, e, s8); fx_pop(s); }
                SU32(ebp, -4u, 0xffffffffu);
                SU32(vec, 0x10, U32(vec, 0x10) + U32(s10, 0));
                const uint32_t old_end = U32(vec, 0x10) - U32(s10, 0);
                { const uint32_t a[1] = {where}; const rx_addr s = fx_push(1, a, 0x006c844b); rx_fx_vector_fill(s8, old_end, where); fx_pop(s); }
            } else {
                SU32(sc, 0, U32(value_p, 0));
                const uint32_t bytes = count * 4u;
                SU32(s10, 0, bytes);
                uint32_t edi = end - bytes;
                { fx_set_reg(FX_EDI, edi); const rx_addr s = fx_push(0, 0, 0x006c8415); SU32(vec, 0x10, rx_fx_vector_umove(end, edi, end)); fx_pop(s); }
                edi = (uint32_t)((int32_t)(edi - where) >> 2);
                if ((int32_t)edi > 0) {
                    const uint32_t ecx = edi * 4u;
                    fx_set_reg(FX_EDI, edi); fx_set_reg(FX_ESI, end - ecx);
                    const uint32_t a[4] = {end - ecx, ecx, where, ecx};
                    FX_CRT(4, a, 0x006c8437, crt_memmove_s(end - ecx, ecx, where, ecx));
                }
                { const uint32_t a[1] = {where}; const rx_addr s = fx_push(1, a, 0x006c844b); rx_fx_vector_fill(sc, where + U32(s10, 0), where); fx_pop(s); }
            }
        }
    }
    SU32(FX_FS0, 0, seh_prev);
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006c8260, rx_fx_vector_insert_n)
