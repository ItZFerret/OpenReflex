/* fx_text.c -- the low helpers of the 0x401xxx..0x402xxx range the effects and the HUD share: the 4x4 matrix product
 * (00401830), the text element's string setters (004029a0 wide, 00402970 wide zero-terminated, 004027e0 narrow / UTF-8) and
 * the per-thread table lookup (00402420). notes/fx.md.
 *
 * Text element T (ESI): +0xfa the dirty flags (0xc3 after a change), +0x104 the length, +0x114 the line count (1 + the
 * '\n's), +0x11c wide (the buffer holds UTF-16), +0x120 the buffer. */
#include "fx.h"
#include "fx_util.h"
#include "loop_player.h"

/* ---- 00401830 (EAX = B, ECX = A; cdecl out*): out = A x B (row vectors: out[r][c] = sum_k A[r][k] B[k][c], summed k = 0..3
 * left to right in single precision, x86's NaN rule), built in a local and copied (rep movsd: ECX 0). Frame: push ebp; and esp, -16; sub esp,
 * 0x48; push esi; push edi: the local at E + 0x10. */
void rx_fx_matrix4_mul(rx_addr a, rx_addr b, rx_addr out) {
    const rx_addr sp0 = rx_sp(), A16 = (sp0 - 4) & ~15u, E = A16 - 0x50;
    SU32(sp0 - 4, 0, fx_reg(FX_EBP));
    SU32(A16 - 0x4c, 0, fx_reg(FX_ESI)); SU32(A16 - 0x50, 0, fx_reg(FX_EDI));
    for (uint32_t r = 0; r < 4; r++)
        for (uint32_t c = 0; c < 4; c++) {
            float s = rx_muls(F32(a, 16 * r), F32(b, 4 * c));
            for (uint32_t k = 1; k < 4; k++) s = rx_adds(s, rx_muls(F32(a, 16 * r + 4 * k), F32(b, 16 * k + 4 * c)));
            SF32(E, 0x10 + 16 * r + 4 * c, s);
        }
    for (uint32_t i = 0; i < 16; i++) SU32(out, 4 * i, U32(E, 0x10 + 4 * i));
}
RX_FUNCTION(0x00401830, rx_fx_matrix4_mul)

/* the line count: 1 + the '\n's of the first n characters (16-bit or 8-bit) at s */
static void count_lines(rx_addr T, rx_addr s, int32_t n, int wide) {
    SF32(T, 0x114, 1.0f);
    for (int32_t i = 0; i < n; i++)
        if ((wide ? U16(s, 2 * (uint32_t)i) : U8(s, (uint32_t)i)) == 0xa) SF32(T, 0x114, F32(T, 0x114) + 1.0f);
}

/* ---- 004029a0 (ESI = text; stdcall str*, len; RET 8): a wide string: copied (wcsncpy, IAT 0x916340) and terminated when the
 * buffer is wide, the lines counted in the source; the element marked dirty. Frame: push ebx; push ebp; (wide) push edi. */
void rx_fx_text_set_wide(rx_addr T, rx_addr str, int32_t len) {
    const rx_addr sp0 = rx_sp();
    SU32(sp0, -4, fx_reg(FX_EBX)); SU32(sp0, -8, fx_reg(FX_EBP));
    if (str == 0) return;
    if (U32(T, 0x11c) != 0) {
        SU32(sp0, -0xc, fx_reg(FX_EDI));
        const rx_addr buf = U32(T, 0x120);
        SU32(T, 0x104, (uint32_t)len);
        rx_set_sp(sp0 - 0xc);
        { const uint32_t a[3] = {buf, str, (uint32_t)len}; const rx_addr s = fx_push(3, a, 0x004029d7); crt_wcsncpy(buf, str, (uint32_t)len); fx_pop(s); }
        rx_set_sp(sp0);
        SU16(buf, 2 * (uint32_t)len, 0);
        SU16(U32(T, 0x120), 2 * U32(T, 0x104), 0);
        count_lines(T, str, I32(T, 0x104), 1);
    }
    SU8(T, 0xfa, 0xc3);
}
RX_FUNCTION(0x004029a0, rx_fx_text_set_wide)

/* ---- 00402970 (EDX = str*; stdcall text; RET 4): a zero-terminated wide string (004029a0 with its length). Frame: push
 * esi. */
void rx_fx_text_set_wide_z(rx_addr T, rx_addr str) {
    if (str == 0) return;
    const rx_addr sp0 = rx_sp();
    SU32(sp0, -4, fx_reg(FX_ESI));
    uint32_t n = 0;
    while (U16(str, 2 * n) != 0) n++;
    rx_set_sp(sp0 - 4);
    const fx_saved sv = fx_save();
    fx_set_reg(FX_ESI, T);
    { const uint32_t a[2] = {str, n}; const rx_addr s = fx_push(2, a, 0x0040299a); rx_fx_text_set_wide(T, str, (int32_t)n); fx_pop(s); }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00402970, rx_fx_text_set_wide_z)

/* MultiByteToWideChar(CP_UTF8, 0, s, -1, d, cap) (IAT 0x9160a4): the UTF-8 string with its terminator as UTF-16, at most cap
 * units (invalid bytes become U+FFFD) -> the units written, 0 when it did not fit */
static uint32_t utf8_to_utf16(rx_addr s, rx_addr d, uint32_t cap) {
    uint32_t o = 0, i = 0;
    for (;;) {
        uint32_t c = U8(s, i), need = 0, cp;
        if (c < 0x80) { cp = c; }
        else if ((c & 0xe0) == 0xc0) { cp = c & 0x1f; need = 1; }
        else if ((c & 0xf0) == 0xe0) { cp = c & 0x0f; need = 2; }
        else if ((c & 0xf8) == 0xf0) { cp = c & 0x07; need = 3; }
        else { cp = 0xfffd; }
        i++;
        for (uint32_t k = 0; k < need; k++) {
            const uint32_t t = U8(s, i);
            if ((t & 0xc0) != 0x80) { cp = 0xfffd; need = 0; break; }
            cp = (cp << 6) | (t & 0x3f); i++;
        }
        if (need == 1 && cp < 0x80) cp = 0xfffd;
        if (need == 2 && (cp < 0x800 || (cp >= 0xd800 && cp < 0xe000))) cp = 0xfffd;
        if (need == 3 && (cp < 0x10000 || cp > 0x10ffff)) cp = 0xfffd;
        if (cp >= 0x10000) {
            if (o + 2 > cap) return 0;
            SU16(d, 2 * o, (uint16_t)(0xd800 + ((cp - 0x10000) >> 10))); SU16(d, 2 * o + 2, (uint16_t)(0xdc00 + (cp & 0x3ff)));
            o += 2;
        } else {
            if (o + 1 > cap) return 0;
            SU16(d, 2 * o, (uint16_t)cp); o++;
        }
        if (cp == 0) return o;
    }
}

/* ---- 004027e0 (ESI = text, EAX = len; stdcall str*; RET 4): a narrow string: a wide buffer gets it converted from UTF-8 (the
 * lines counted in the source), a narrow one copied (strncpy, IAT 0x916260; the lines counted in the buffer); terminated, the
 * element marked dirty. Frame: push ebx; push edi; (str) push ebp. */
void rx_fx_text_set(rx_addr T, uint32_t len, rx_addr str) {
    const rx_addr sp0 = rx_sp();
    SU32(sp0, -4, fx_reg(FX_EBX)); SU32(sp0, -8, fx_reg(FX_EDI));
    if (str == 0) return;
    SU32(sp0, -0xc, fx_reg(FX_EBP));
    const rx_addr buf = U32(T, 0x120);
    SU32(T, 0x104, len);
    rx_set_sp(sp0 - 0xc);
    if (U32(T, 0x11c) != 0) {
        const uint32_t a[6] = {0xfde9, 0, str, 0xffffffffu, buf, len + 1};
        fx_import_residue(0x0040281d, 6, a);
        utf8_to_utf16(str, buf, len + 1);
        SU16(buf, 2 * (len + 1) - 2, 0);
        SU16(U32(T, 0x120), 2 * U32(T, 0x104), 0);
        count_lines(T, str, I32(T, 0x104), 0);
    } else {
        { const uint32_t a[3] = {buf, str, len}; const rx_addr s = fx_push(3, a, 0x00402906); crt_strncpy(buf, str, len); fx_pop(s); }
        SU8(buf, len, 0);
        SU8(U32(T, 0x120), U32(T, 0x104), 0);
        count_lines(T, U32(T, 0x120), I32(T, 0x104), 0);
    }
    SU8(T, 0xfa, 0xc3);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x004027e0, rx_fx_text_set)

/* ---- 00402420 (ECX = the per-thread tables; stdcall key, thread; RET 8): thread 0: the table's own entry (008841d0 of ECX);
 * else entry key of the thread's table (+0x30: a vector of tables at +0xc..+0x10), -1 after noting 0xe0000003 (00884390)
 * for a thread out of range. Frame: push edi; (thread) push esi. */
uint32_t rx_fx_thread_table_get(rx_addr self, uint32_t key, uint32_t thread) {
    const rx_addr sp0 = rx_sp();
    SU32(sp0, -4, fx_reg(FX_EDI));
    uint32_t r;
    if (thread == 0) {
        rx_set_sp(sp0 - 4);
        { const uint32_t a[1] = {key}; const rx_addr s = fx_push(1, a, 0x00402433); r = rx_table11_get(self, key); fx_pop(s); }
    } else {
        SU32(sp0, -8, fx_reg(FX_ESI));
        rx_set_sp(sp0 - 8);
        const rx_addr v = U32(self, 0x30);
        if (thread >= (U32(v, 0x10) - U32(v, 0xc)) >> 2) {
            const uint32_t a[4] = {0, 0, 0xe0000003u, 0};
            const rx_addr s = fx_push(4, a, 0x00402458); rx_thread_note(0, 0, 0xe0000003u, 0); fx_pop(s);
            r = 0xffffffffu;
        } else {
            const rx_addr t = U32(U32(v, 0xc), 4 * thread);
            const uint32_t a[1] = {key}; const rx_addr s = fx_push(1, a, 0x00402488); r = rx_table11_get(t, key); fx_pop(s);
        }
    }
    rx_set_sp(sp0);
    return r;
}
RX_FUNCTION(0x00402420, rx_fx_thread_table_get)
