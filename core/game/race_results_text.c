/* race_results_text.c -- the UI text helpers the results table's fillers call (w-wpfill): App_FormatCredits 005dc430 (a number with
 * thousands separators, the Section Race's SECTIONS cell) with its digit writer 005dc3c0 and the in-place UTF-16 reverse 007900c0, and
 * the localized ordinal 005dc7a0 / 005dc720 (the Eliminator's ELIMINATED cell, 00614950 column 9: "1ST", "2ND", ...).
 *
 * The ordinal's suffix is a UI string: 005dc720 looks it up in the UI's language registry (005dfc90 / 005e0270: STR_FIRST, STR_SECOND,
 * STR_THIRD, every other place STR_FOURTH; without an entry the literal L"st" / L"nd" / L"rd" / L"th" next to the keys) and formats
 * "%i%ls" with msvcr90's _vswprintf_c_l (00790040). The registry is filled by the UI system only, which the product does not run (its
 * index check would terminate the product, race_activity_misc.c 005dfc90), so this is a UI boundary: the oracle build runs the
 * language lookup and the CRT as the original does; the product writes the text the original shows with the shipped English language,
 * recorded in the oracle (w-wpfill: the Eliminator series 5 boot, the table's cells "1ST" / "2ND" / "3RD"; the install's string tables
 * InGame / Shared MXRaven_Default_Strings: STR_FIRST "ST", STR_SECOND "ND", STR_THIRD "RD", STR_FOURTH "TH"), and the table's sort
 * value carries the place, so the presenter localizes the suffix itself with the original's rule (1 / 2 / 3 / any
 * other place: STR_FIRST / STR_SECOND / STR_THIRD / STR_FOURTH; Unity OwnModes). See core/plan/QUEUE.md "w-wpfill". */
#include "race_results_text.h"
#include "race_activity_misc.h"
#include "race_activity_ui.h"
#include "build_activity_util.h"
#include "rx_os.h"
#include <string.h>

#define WCMB_SLOT 0x009160a8u   /* kernel32 WideCharToMultiByte */

/* WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, size, 0, 0) through its import, entered at the current stack pointer (also the
 * party modes' fillers in race_results_table.c) */
uint32_t rx_results_wcmb(rx_addr src, rx_addr dst, uint32_t size) {
    const uint32_t w[8] = {0xfde9 /* CP_UTF8 */, 0, src, 0xffffffffu, dst, size, 0, 0};
    return rx_os_import(WCMB_SLOT, 8, w);
}

/* ---- 007900c0 (EAX = a UTF-16 string; plain RET) -> EAX unchanged: the string reversed in place (its length by the terminator,
 * then the ends swapped towards the middle). Frame: push esi / edi. */
rx_addr rx_wstr_reverse(rx_addr s) {
    ACT_FRAME(L, 0, 2);
    rx_addr p = s;
    while (U16(p, 0) != 0) p += 2;
    int32_t hi = (int32_t)((p + 2) - (s + 2)) >> 1;
    hi -= 1;
    for (int32_t lo = 0; hi > 0 && lo < hi; lo++, hi--) {
        const uint16_t a = U16(s, 2u * (uint32_t)lo), b = U16(s, 2u * (uint32_t)hi);
        SU16(s, 2u * (uint32_t)lo, b);
        SU16(s, 2u * (uint32_t)hi, a);
    }
    ACT_END(L);
    return s;
}
RX_FUNCTION(0x007900c0, rx_wstr_reverse)

/* ---- 005dc3c0 (ECX = value, EDI = a UTF-16 buffer; plain RET) -> EAX: the unsigned value in decimal with a ',' after every third
 * digit from the right, written backwards (units first) and then reversed by 007900c0 (a tail jump: EAX = the buffer). 0 is "0"
 * (EAX 0x30). Frame: push ebx / esi (popped before the tail jump). */
rx_addr rx_format_thousands_w(uint32_t value, rx_addr w) {
    if (value == 0) {
        SU16(w, 0, 0x30);
        SU16(w, 2, 0);
        return 0x30;
    }
    {
        ACT_FRAME(L, 0, 2);
        uint32_t n = 0, group = 0;
        while (value != 0) {
            const uint32_t g = group++;
            if (g == 3) { SU16(w, 2u * n, 0x2c); n++; group = 1; }
            const uint32_t q = value / 10u;
            SU16(w, 2u * n, (uint16_t)(value - q * 10u + 0x30u));
            value = q;
            n++;
        }
        SU16(w, 2u * n, 0);
        ACT_END(L);
    }
    return rx_wstr_reverse(w);
}
RX_FUNCTION(0x005dc3c0, rx_format_thousands_w)

/* ---- App_FormatCredits 005dc430 (ECX = value, EDX = buffer size; cdecl buffer) -> EAX (WideCharToMultiByte's result): the value as
 * 005dc3c0 writes it ("1,250"; -1 is the empty string) into a UTF-16 buffer of 0x400 characters, converted to UTF-8 into the buffer
 * (size clamped to 0x400), whose last byte is then the terminator. Frame: sub 0x804; push ebx / esi (the UTF-16 buffer at +8), edi
 * around 005dc3c0. */
uint32_t rx_app_format_credits(int32_t value, int32_t size, rx_addr buf) {
    ACT_FRAME(L, 0x804, 2);
    const rx_addr W = L + 8;
    if (size >= 0x400) size = 0x400;
    if (value == -1) SU16(W, 0, 0);
    else {
        const rx_addr sp = rx_sp();
        rx_set_sp(sp - 4);                                         /* push edi */
        ACT_CALL(0, (void)rx_format_thousands_w((uint32_t)value, W));
        rx_set_sp(sp);
    }
    const uint32_t r = rx_results_wcmb(W, buf, (uint32_t)size);
    SU8(buf, (uint32_t)(size - 1), 0);
    ACT_END(L);
    return r;
}
RX_FUNCTION(0x005dc430, rx_app_format_credits)

/* ---- 005dc720 (cdecl UTF-16 buffer, size in characters, place) -> EAX: "%i%ls" of the place and its ordinal suffix (above): the
 * language (005dfc90(0): the current one) and its entry for the key of the place (005e0270: +0x10 the UTF-16 text), else the
 * literal next to the key; then 00790040 (EDI = buffer, ESI = size; cdecl format L"%i%ls" 0x0094fdf4, ...: _vswprintf_c_l with no
 * locale and the terminator at buffer[size - 1]). Frame: push ebx / esi / edi. UI boundary in the product (the file's comment). */
uint32_t rx_ui_ordinal_w(rx_addr w, uint32_t size, int32_t place) {
    ACT_FRAME(L, 0, 3);
    rx_addr key, fallback;
    switch (place) {
    case 1:  key = 0x0094fe08 /* "STR_FIRST" */;  fallback = 0x0094fe00 /* L"st" */; break;
    case 2:  key = 0x0094fe1c /* "STR_SECOND" */; fallback = 0x0094fe14 /* L"nd" */; break;
    case 3:  key = 0x0094fe30 /* "STR_THIRD" */;  fallback = 0x0094fe28 /* L"rd" */; break;
    default: key = 0x0094fe44 /* "STR_FOURTH" */; fallback = 0x0094fe3c /* L"th" */; break;
    }
#ifdef RX_ORACLE
    rx_addr lang;
    ACT_CALLR(lang, 1, rx_ui_language_find(0));                    /* 005dfc90 (push 0) */
    rx_addr entry;
    ACT_CALLR(entry, 0, rx_ui_language_lookup(key, lang));         /* 005e0270 (EAX = key, EDI = the language) */
    const rx_addr suffix = entry ? U32(entry, 0x10) : fallback;
    /* 00790040: the format's arguments as the caller pushes them (format, place, suffix), then _vswprintf_c_l(buffer, size, format,
     * 0, &place) through msvcr90's import (cdecl), then buffer[size - 1] = 0 */
    SU32(L, -12, 0x0094fdf4 /* L"%i%ls" */); SU32(L, -8, (uint32_t)place); SU32(L, -4, suffix);
    const rx_addr F = L - 16;                                      /* 00790040's entry: its return address at F */
    const uint32_t a[5] = {w, size, 0x0094fdf4, 0, F + 8};
    rx_set_sp(F);
    const uint32_t r = rx_call_original(U32(0, 0x00916344) /* _vswprintf_c_l */, RX_CDECL, 0, 5, a);
    rx_set_sp(L);
    SU16(w, 2u * size - 2u, 0);
#else
    /* UI boundary: no language registry in the product; the English language's entry of the key as the oracle recorded it (the
     * file's comment), in the format's "%i%ls" */
    (void)key; (void)fallback;
    static const char* const suffixes[4] = {"ST", "ND", "RD", "TH"};   /* STR_FIRST / STR_SECOND / STR_THIRD / STR_FOURTH (ENG) */
    const char* sfx = suffixes[place >= 1 && place <= 3 ? place - 1 : 3];
    char text[24];
    snprintf(text, sizeof text, "%d%s", (int)place, sfx);
    uint32_t n = 0;
    for (; text[n] && n + 1 < size; n++) SU16(w, 2u * n, (uint8_t)text[n]);
    SU16(w, 2u * n, 0);
    SU16(w, 2u * size - 2u, 0);
    const uint32_t r = n;
#endif
    ACT_END(L);
    return r;
}
RX_FUNCTION(0x005dc720, rx_ui_ordinal_w)

/* ---- 005dc7a0 (ECX = buffer size, EDI = buffer; cdecl place; plain RET) -> EAX (WideCharToMultiByte's result): the place's ordinal
 * text (005dc720 into a UTF-16 buffer of 0x400 characters, size clamped to 0x400) converted to UTF-8 into the buffer, whose last
 * byte is then the terminator. Frame: sub 0x804; push esi (the UTF-16 buffer at +4). */
uint32_t rx_ui_ordinal_text(rx_addr buf, int32_t size, int32_t place) {
    ACT_FRAME(L, 0x804, 1);
    const rx_addr W = L + 4;
    if (size >= 0x400) size = 0x400;
    ACT_CALL(3, (void)rx_ui_ordinal_w(W, (uint32_t)size, place));
    const uint32_t r = rx_results_wcmb(W, buf, (uint32_t)size);
    SU8(buf, (uint32_t)(size - 1), 0);
    ACT_END(L);
    return r;
}
RX_FUNCTION(0x005dc7a0, rx_ui_ordinal_text)
