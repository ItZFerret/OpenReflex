/* crt_string.c -- msvcr90's memory and string functions the game calls. Their effect is the bytes they write and their return
 * value; the DLL's code (dword / SSE2 block copies, unrolled compares, jump tables) is reproduced by its result:
 *  - memcpy 7855aac0 copies backward when the destination overlaps the source from above (7855ac84), so it IS memmove; the
 *    forward paths (rep movsd, the SSE2 fastcopy_I 7854461e for 16-aligned pairs of 256+ bytes) load each block before storing
 *    it, so a forward overlap gives the memmove result too.
 *  - memcmp 78544792 compares dwords and the first differing dword byte by byte: the result is the SIGN of the first differing
 *    byte (-1 / 0 / 1, every tail case of its jump table 78545d92 normalizes), not the difference.
 *  - strcmp 7855b580: -1 / 0 / 1 (sbb); strncmp 78556789 and wcsncmp 78557165: the difference of the first differing units;
 *    _stricmp 78578d50 (the "C" locale path __ascii_stricmp 78578c38): the difference of the lowered bytes; _strnicmp 78579347
 *    (__ascii_strnicmp 7855b960): -1 / 0 / 1.
 * The *_s functions' invalid-argument paths (errno, _invalid_parameter) and the locale-aware paths (taken only after setlocale,
 * __locale_changed 0x785bc218 != 0) are not ported: they run the original (crt_unported). */
#include "crt.h"
#include "crt_util.h"

#define CRT_LOCALE_CHANGED 0x785bc218u   /* __locale_changed */

/* ---- memory ---------------------------------------------------------------------------------------------------------------- */
#define R_EBX crt_caller_regs[3]
#define R_EBP crt_caller_regs[5]
#define R_ESI crt_caller_regs[6]
#define R_EDI crt_caller_regs[7]
/* memcpy / memmove's frames (e = the return slot). Both start push ebp / push edi / push esi; the SSE2 path (n >= 256,
 * __sse2_available 0x785bd7b4, same 16-byte alignment) pushes edi / esi once more, pops everything and jumps to 785446aa
 * (ebp / ebx / esi / edi saved, the remainder n & 0x7f, for the remainder's rep movsb src / dst + n - rem) which calls
 * fastcopy_I 7854461e (n - rem, src, dst, the return 785446fc, its ebp, edi = 0, esi = n - rem). */
static void crt_move_frame(rx_addr d, rx_addr s, uint32_t n) {
    const rx_addr e = crt_frame_e(3);
    if (!e) return;
    SU32(e, -4, R_EBP); SU32(e, -8, R_EDI); SU32(e, -0xc, R_ESI);
    const int overlap = d > s && d < s + n;
    if (overlap || n < 0x100 || U32(0, 0x785bd7b4) == 0) return;
    SU32(e, -0x10, d); SU32(e, -0x14, s);                                   /* the alignment test's push edi / push esi */
    if ((d & 0xf) != (s & 0xf)) return;
    SU32(e, -4, R_EBP); SU32(e, -8, R_EBX); SU32(e, -0xc, R_ESI); SU32(e, -0x10, R_EDI);
    const uint32_t rem = n & 0x7f;
    if ((d & 0xf) != 0) return;   /* (785446aa's path for a common misalignment is not reproduced: the game does not take it) */
    SU32(e, -0x1c, rem);
    if (n != rem) {
        SU32(e, -0x24, n - rem); SU32(e, -0x28, s); SU32(e, -0x2c, d); SU32(e, -0x30, 0x785446fcu);
        SU32(e, -0x34, e - 4); SU32(e, -0x38, 0); SU32(e, -0x3c, n - rem);
    }
    if (rem) { SU32(e, -0x18, s + n - rem); SU32(e, -0x14, d + n - rem); }
}
/* the bytes: backward when the destination overlaps the source from above, else forward. Both have memmove's result while neither
 * range wraps the 32-bit space: then one bulk move (rx_move) */
static void crt_move(rx_addr d, rx_addr s, uint32_t n) {
    if (d + n >= d && s + n >= s) { rx_move(d, s, n); return; }
    if (d > s && d < s + n) { for (uint32_t i = n; i-- > 0;) SU8(d, i, U8(s, i)); return; }
    for (uint32_t i = 0; i < n; i++) SU8(d, i, U8(s, i));
}
rx_addr crt_memcpy(rx_addr d, rx_addr s, uint32_t n) { crt_move_frame(d, s, n); crt_move(d, s, n); return d; }   /* 7855aac0 */
RX_FUNCTION(0x7855aac0, crt_memcpy)
rx_addr crt_memmove(rx_addr d, rx_addr s, uint32_t n) { crt_move_frame(d, s, n); crt_move(d, s, n); return d; }  /* 7855afb0 */
RX_FUNCTION(0x7855afb0, crt_memmove)
/* memset's frames: the byte path pushes edi; the SSE2 path (c == 0, n >= 256) jumps to _VEC_memzero 78545e73 (ebp, edi,
 * the remainder, the misaligned head's length; calls itself after the head, and fastzero_I 78545e17 for the 128-byte
 * blocks). */
static void crt_memzero_frame(rx_addr e, rx_addr d, uint32_t n, uint32_t edi, uint32_t ebp) {
    SU32(e, -4, ebp); SU32(e, -8, edi);
    const uint32_t mis = d & 0xf;
    if (mis == 0) {
        const uint32_t rem = n & 0x7f;
        SU32(e, -0x10, rem);
        if (n != rem) {
            SU32(e, -0x18, n - rem); SU32(e, -0x1c, d); SU32(e, -0x20, 0x78545ea9u); SU32(e, -0x24, e - 4); SU32(e, -0x28, 0);
        }
        if (rem) SU32(e, -0xc, d + n - rem);
        return;
    }
    const uint32_t head = 16 - mis;
    SU32(e, -0x14, head);
    SU32(e, -0x18, n - head); SU32(e, -0x1c, 0); SU32(e, -0x20, d + head); SU32(e, -0x24, 0x78545ef5u);
    crt_memzero_frame(e - 0x24, d + head, n - head, d + head, e - 4);
}
rx_addr crt_memset(rx_addr d, int32_t c, uint32_t n) {                                                /* 7855b320 */
    const rx_addr e = n ? crt_frame_e(3) : 0;
    if (e) {
        if ((uint8_t)c == 0 && n >= 0x100 && U32(0, 0x785bd7b4) != 0) crt_memzero_frame(e, d, n, R_EDI, R_EBP);
        else SU32(e, -4, R_EDI);
    }
    for (uint32_t i = 0; i < n; i++) SU8(d, i, (uint8_t)c);
    return d;
}
RX_FUNCTION(0x7855b320, crt_memset)
int32_t crt_memcmp(rx_addr a, rx_addr b, uint32_t n) {                                                /* 78544792 */
    for (uint32_t i = 0; i < n; i++) {
        const uint8_t x = U8(a, i), y = U8(b, i);
        if (x != y) return x > y ? 1 : -1;
    }
    return 0;
}
RX_FUNCTION(0x78544792, crt_memcmp)
rx_addr crt_memchr(rx_addr p, int32_t c, uint32_t n) {                                                /* 7855ae30 */
    for (uint32_t i = 0; i < n; i++) if (U8(p, i) == (uint8_t)c) return p + i;
    return 0;
}
RX_FUNCTION(0x7855ae30, crt_memchr)
/* 7855664b memcpy_s(dst, size, src, count): 0 when count is 0 or it fits; the error paths (null pointers, too small: zeroes dst,
 * errno, _invalid_parameter) run the original. */
int32_t crt_memcpy_s(rx_addr d, uint32_t size, rx_addr s, uint32_t n) {
    if (n == 0) return 0;
    if (d == 0 || s == 0 || size < n) { const uint32_t a[4] = {d, size, s, n}; return (int32_t)crt_unported(0x7855664b, 4, a); }
    crt_memcpy(d, s, n);
    return 0;
}
RX_FUNCTION(0x7855664b, crt_memcpy_s)
int32_t crt_memmove_s(rx_addr d, uint32_t size, rx_addr s, uint32_t n) {                             /* 785566cd */
    if (n == 0) return 0;
    if (d == 0 || s == 0 || size < n) { const uint32_t a[4] = {d, size, s, n}; return (int32_t)crt_unported(0x785566cd, 4, a); }
    crt_memmove(d, s, n);
    return 0;
}
RX_FUNCTION(0x785566cd, crt_memmove_s)

/* ---- strings --------------------------------------------------------------------------------------------------------------- */
uint32_t crt_strlen(rx_addr s) { uint32_t n = 0; while (U8(s, n)) n++; return n; }                   /* 7855b660 */
RX_FUNCTION(0x7855b660, crt_strlen)
/* 785569b2 strcpy_s(dst, size, src): copies up to size bytes; 0 when the terminator fit (else the original's error path). */
int32_t crt_strcpy_s(rx_addr d, uint32_t size, rx_addr s) {
    if (d == 0 || size == 0 || s == 0) goto error;
    {
        uint32_t left = size, i = 0;
        for (;;) {
            const uint8_t c = U8(s, i);
            SU8(d, i, c); i++;
            if (c == 0) break;
            if (--left == 0) goto error;   /* not terminated within size: the original zeroes dst[0], errno ERANGE (its path) */
        }
        return 0;
    }
error:;
    const uint32_t a[3] = {d, size, s};
    return (int32_t)crt_unported(0x785569b2, 3, a);
}
RX_FUNCTION(0x785569b2, crt_strcpy_s)
rx_addr crt_strncpy(rx_addr d, rx_addr s, uint32_t n) {                                               /* 7855b830 */
    uint32_t i = 0;
    for (; i < n; i++) { const uint8_t c = U8(s, i); SU8(d, i, c); if (!c) { i++; break; } }
    for (; i < n; i++) SU8(d, i, 0);
    return d;
}
RX_FUNCTION(0x7855b830, crt_strncpy)
rx_addr crt_wcsncpy(rx_addr d, rx_addr s, uint32_t n) {                                               /* 785571c9 */
    uint32_t i = 0;
    for (; i < n; i++) { const uint16_t c = U16(s, 2 * i); SU16(d, 2 * i, c); if (!c) { i++; break; } }
    for (; i < n; i++) SU16(d, 2 * i, 0);
    return d;
}
RX_FUNCTION(0x785571c9, crt_wcsncpy)
rx_addr crt_strchr(rx_addr s, int32_t c) {                                                            /* 7855b4b0 */
    for (uint32_t i = 0;; i++) {
        const uint8_t x = U8(s, i);
        if (x == (uint8_t)c) return s + i;
        if (!x) return 0;
    }
}
RX_FUNCTION(0x7855b4b0, crt_strchr)
rx_addr crt_strrchr(rx_addr s, int32_t c) {                                                           /* 7855ba50 */
    rx_addr r = 0;
    for (uint32_t i = 0;; i++) {
        const uint8_t x = U8(s, i);
        if (x == (uint8_t)c) r = s + i;
        if (!x) return r;
    }
}
RX_FUNCTION(0x7855ba50, crt_strrchr)
rx_addr crt_strstr(rx_addr h, rx_addr n) {                                                            /* 7855bb50 */
    if (!U8(n, 0)) return h;
    for (uint32_t i = 0; U8(h, i); i++) {
        uint32_t j = 0;
        while (U8(n, j) && U8(h, i + j) == U8(n, j)) j++;
        if (!U8(n, j)) return h + i;
    }
    return 0;
}
RX_FUNCTION(0x7855bb50, crt_strstr)
rx_addr crt_strpbrk(rx_addr s, rx_addr set) {                                                         /* 7855ba00 */
    for (uint32_t i = 0; U8(s, i); i++)
        for (uint32_t j = 0; U8(set, j); j++) if (U8(set, j) == U8(s, i)) return s + i;
    return 0;
}
RX_FUNCTION(0x7855ba00, crt_strpbrk)
/* 78556bd6 strtok: the delimiter set as a 32-byte bit map (its terminator included), the cursor in the thread's data (_getptd 785438bd:
 * _tiddata +0x18 _token): leading delimiters skipped, the token ended by a 0 written over the next delimiter, the cursor after it */
rx_addr crt_strtok(rx_addr s, rx_addr delims) {
    uint8_t map[32];
    memset(map, 0, sizeof map);
    for (uint32_t k = 0;; k++) { const uint8_t c = U8(delims, k); map[c >> 3] |= (uint8_t)(1u << (c & 7)); if (!c) break; }
    const rx_addr ptd = crt_ptd();
    rx_addr p = s ? s : U32(ptd, 0x18);
    for (;;) { const uint8_t c = U8(p, 0); if (!((map[c >> 3] >> (c & 7)) & 1)) break; if (!c) break; p++; }
    const rx_addr tok = p;
    for (; U8(p, 0); p++) {
        const uint8_t c = U8(p, 0);
        if ((map[c >> 3] >> (c & 7)) & 1) { SU8(p, 0, 0); p++; break; }
    }
    SU32(ptd, 0x18, p);
    return tok == p ? 0 : tok;
}
RX_FUNCTION(0x78556bd6, crt_strtok)
int32_t crt_strcmp(rx_addr a, rx_addr b) {                                                            /* 7855b580 */
    for (uint32_t i = 0;; i++) {
        const uint8_t x = U8(a, i), y = U8(b, i);
        if (x != y) return x < y ? -1 : 1;
        if (!x) return 0;
    }
}
RX_FUNCTION(0x7855b580, crt_strcmp)
int32_t crt_strncmp(rx_addr a, rx_addr b, uint32_t n) {                                               /* 78556789 */
    for (uint32_t i = 0; i < n; i++) {
        const uint8_t x = U8(a, i), y = U8(b, i);
        if (!x || x != y) return (int32_t)x - (int32_t)y;
    }
    return 0;
}
RX_FUNCTION(0x78556789, crt_strncmp)
/* 78557165 wcsncmp: it counts down its count argument in place (dec [ebp+0x10]): *n_left = the final count (the slot's value
 * for the adapter; unchanged when the count is 0). */
int32_t crt_wcsncmp_ex(rx_addr a, rx_addr b, uint32_t n, uint32_t* n_left) {
    *n_left = n;
    if (n == 0) return 0;
    uint32_t i = 0;
    while (--n && U16(a, 2 * i) && U16(a, 2 * i) == U16(b, 2 * i)) i++;
    *n_left = n;
    return (int32_t)U16(a, 2 * i) - (int32_t)U16(b, 2 * i);
}
int32_t crt_wcsncmp(rx_addr a, rx_addr b, uint32_t n) { uint32_t left; return crt_wcsncmp_ex(a, b, n, &left); }
RX_FUNCTION(0x78557165, crt_wcsncmp_ex)

/* ---- case (the "C" locale) -------------------------------------------------------------------------------------------------- */
int32_t crt_tolower(int32_t c) {                                                                      /* 7855e8c1 */
    if (U32(0, CRT_LOCALE_CHANGED) != 0) { const uint32_t a[1] = {(uint32_t)c}; return (int32_t)crt_unported(0x7855e8c1, 1, a); }
    return (uint32_t)(c - 0x41) <= 0x19 ? c + 0x20 : c;
}
RX_FUNCTION(0x7855e8c1, crt_tolower)
static uint32_t crt_lower(uint32_t c) { return c - 0x41 <= 0x19 ? c + 0x20 : c; }
int32_t crt_stricmp(rx_addr a, rx_addr b) {                                                           /* 78578d50 */
    if (U32(0, CRT_LOCALE_CHANGED) != 0 || a == 0 || b == 0) {
        const uint32_t args[2] = {a, b}; return (int32_t)crt_unported(0x78578d50, 2, args);
    }
    uint32_t x, y;
    for (uint32_t i = 0;; i++) {
        x = crt_lower(U8(a, i)); y = crt_lower(U8(b, i));
        if (x == 0 || x != y) break;
    }
    return (int32_t)(x - y);
}
RX_FUNCTION(0x78578d50, crt_stricmp)
int32_t crt_strnicmp(rx_addr a, rx_addr b, uint32_t n) {                                              /* 78579347 */
    if (U32(0, CRT_LOCALE_CHANGED) != 0 || a == 0 || b == 0 || n > 0x7fffffffu) {
        const uint32_t args[3] = {a, b, n}; return (int32_t)crt_unported(0x78579347, 3, args);
    }
    if (n == 0) return 0;
    uint8_t x = 0, y = 0;
    for (uint32_t i = 0; i < n; i++) {
        x = U8(a, i); y = U8(b, i);
        if (!x || !y) break;
        if (x >= 0x41 && x <= 0x5a) x = (uint8_t)(x + 0x20);
        if (y >= 0x41 && y <= 0x5a) y = (uint8_t)(y + 0x20);
        if (x != y) return x < y ? -1 : 1;
    }
    return x == y ? 0 : x < y ? -1 : 1;
}
RX_FUNCTION(0x78579347, crt_strnicmp)

/* ---- strtol 7855e3af -> strtoxl 7855e17b (atol 78546074 = strtol(s, 0, 10), atoi 785460ad = atol) -------------------------
 * With __locale_changed == 0 strtol passes the initial locale (__initiallocalestructinfo 0x785ba3a0): the "C" ctype table
 * (locinfo + 0xc8; 8 space, 4 digit, 0x103 letter). The other locale paths, multibyte locales (+0xac > 1), invalid
 * arguments and the ERANGE store without per-thread data run the original. */
static int crt_strtoxl(rx_addr locale, rx_addr nptr, rx_addr endptr, int32_t ibase, uint32_t flags, uint32_t* out) {
    if (locale == 0) return CRT_WHY("strtoxl: no locale"), 0;
    const rx_addr locinfo = U32(locale, 0);
    if (I32(locinfo, 0xac) > 1) return CRT_WHY("strtoxl: multibyte locale"), 0;
    if (nptr == 0 || (ibase != 0 && (ibase < 2 || ibase > 36))) return CRT_WHY("strtoxl: arguments"), 0;
    const rx_addr ctype = U32(locinfo, 0xc8);
    rx_addr p = nptr;
    uint8_t c = U8(p, 0); p++;
    while (U16(ctype, 2u * c) & 8) { c = U8(p, 0); p++; }
    if (c == '-') { flags |= 2; c = U8(p, 0); p++; }
    else if (c == '+') { c = U8(p, 0); p++; }
    uint32_t base = (uint32_t)ibase;
    if (ibase == 0) {
        if (c != '0') base = 10;
        else if (U8(p, 0) == 'x' || U8(p, 0) == 'X') { base = 16; p++; c = U8(p, 0); p++; }
        else base = 8;
    } else if (ibase == 16 && c == '0' && (U8(p, 0) == 'x' || U8(p, 0) == 'X')) { p++; c = U8(p, 0); p++; }
    const uint32_t maxval = 0xffffffffu / base, maxrem = 0xffffffffu % base;
    uint32_t number = 0;
    for (;;) {
        const uint16_t t = U16(ctype, 2u * c);
        uint32_t d;
        if (t & 4) d = (uint32_t)((int8_t)c - '0');
        else if (t & 0x103) d = (uint32_t)((int32_t)(int8_t)c - ((uint8_t)(c - 'a') <= 0x19 ? 0x20 : 0) - 0x37);
        else break;
        if (d >= base) break;
        flags |= 8;
        if (number < maxval || (number == maxval && d <= maxrem)) number = number * base + d;
        else { flags |= 4; if (endptr == 0) break; }
        c = U8(p, 0); p++;
    }
    p--;
    if (!(flags & 8)) { if (endptr) p = nptr; number = 0; }
    else if ((flags & 4) || (!(flags & 1) && (((flags & 2) && number > 0x80000000u) || (!(flags & 2) && number > 0x7fffffffu)))) {
        const rx_addr ptd = crt_ptd();
        if (!ptd) return CRT_WHY("strtoxl: errno without per-thread data"), 0;
        SU32(ptd, 8, 34);                                                            /* errno = ERANGE */
        if (flags & 1) number = 0xffffffffu;
        else number = 0x7fffffffu + ((flags & 2) ? 1u : 0u);
    }
    if (endptr) SU32(endptr, 0, p);
    if (flags & 2) number = 0u - number;
    *out = number;
    return 1;
}
int32_t crt_strtol(rx_addr nptr, rx_addr endptr, int32_t base) {
    uint32_t r;
    const rx_addr locale = U32(0, CRT_LOCALE_CHANGED) == 0 ? 0x785ba3a0u : 0;
    if (crt_strtoxl(locale, nptr, endptr, base, 0, &r)) return (int32_t)r;
    const uint32_t a[3] = {nptr, endptr, (uint32_t)base};
    return (int32_t)crt_unported(0x7855e3af, 3, a);
}
RX_FUNCTION(0x7855e3af, crt_strtol)
int32_t crt_atol(rx_addr s) {
    uint32_t r;
    const rx_addr locale = U32(0, CRT_LOCALE_CHANGED) == 0 ? 0x785ba3a0u : 0;
    if (crt_strtoxl(locale, s, 0, 10, 0, &r)) return (int32_t)r;
    const uint32_t a[1] = {s};
    return (int32_t)crt_unported(0x78546074, 1, a);
}
RX_FUNCTION(0x78546074, crt_atol)
int32_t crt_atoi(rx_addr s) {
    uint32_t r;
    const rx_addr locale = U32(0, CRT_LOCALE_CHANGED) == 0 ? 0x785ba3a0u : 0;
    if (crt_strtoxl(locale, s, 0, 10, 0, &r)) return (int32_t)r;
    const uint32_t a[1] = {s};
    return (int32_t)crt_unported(0x785460ad, 1, a);
}
RX_FUNCTION(0x785460ad, crt_atoi)
