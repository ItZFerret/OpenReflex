/* crt_printf.c -- msvcr90's formatted output to strings: sprintf 78552b7e, _vsnprintf 7855471e, _vsnprintf_l 78554666 and the
 * engine they share, _output_l 78570f34 (with write_char 78572e8e / write_multi_char 78572ec6 / write_string 78572ef1 on a
 * string FILE), _LocaleUpdate 7855c094, _isleadbyte_l 7855c98f, and the %f chain _cfltcvt_l 7859924a -> _cftof_l 78599069 ->
 * _fltout2 78598788 (__dtold 785986c6, $I10_OUTPUT 78599ae6: MSVC's own 80-bit digit generation with 12-byte arithmetic) ->
 * _fptostr 78545fb0 -> _cftof2_l 78598f6d (_shift 78598963), _aulldvrm 7855bf50 for the integer digits.
 *
 * Effects. The original formats into its stack (dead) and writes the characters through the string FILE into the caller's
 * buffer; when the buffer runs out (_vsnprintf) _flsbuf sets errno = ERANGE in the per-thread data. _LocaleUpdate sets and
 * clears the per-thread data's "own locale" bit 2 (+0x70) around the call: no net change, not reproduced. This port formats
 * into a host buffer first and then stores the characters in order (the same final memory); it declines to the original
 * (crt_unported, before any store) for what the game does not use: conversions other than d i u o x X p c s % f, wide / %n /
 * %Z, floats with more than 163 digits of precision (a heap buffer), a lead byte in the format, a
 * %s source that overlaps the output, a thread without its per-thread data or with a stale locale (the update calls). */
#include "crt.h"
#include "crt_util.h"


/* _LocaleUpdate for a NULL locale: the thread's locinfo / mbcinfo, unless they are stale (not the global ones and the thread
 * does not own its locale: __updatetlocinfo / __updatetmbcinfo would run). Returns the locinfo, 0 when the original must run. */
static rx_addr crt_thread_locinfo(void) {
    const rx_addr ptd = crt_ptd();
    if (!ptd) return CRT_WHY("no per-thread data"), 0;
    const uint32_t own = U32(ptd, 0x70), global_status = U32(0, 0x785ba2b4);
    if (U32(ptd, 0x6c) != U32(0, 0x785ba398) && !(own & global_status)) return CRT_WHY("stale locinfo"), 0;   /* __ptlocinfo */
    if (U32(ptd, 0x68) != U32(0, 0x785b7b88) && !(own & global_status)) {   /* __ptmbcinfo: a new thread's first call */
        if (!crt_updatetmbcinfo()) return CRT_WHY("__updatetmbcinfo"), 0;
    }
    return U32(ptd, 0x6c);
}

/* ---- the output buffer: a string FILE {_ptr, _cnt, _base} ----------------------------------------------------------------- */
#define OUT_MAX 4096
typedef struct {
    int32_t cnt;         /* _cnt: room left */
    int counting;        /* _base == NULL: only counts (write_char's _IOSTRG test) */
    int overflow;        /* _flsbuf ran: errno = ERANGE */
    int32_t charsout;    /* the engine's count, -1 after an error */
    uint32_t n;          /* characters stored so far */
    uint8_t buf[OUT_MAX];
    int too_long;        /* more than OUT_MAX stored: decline */
} crt_out;
static void out_char(crt_out* o, uint8_t c) {                       /* write_char 78572e8e */
    if (o->counting) { o->charsout++; return; }
    if (--o->cnt >= 0) {
        if (o->n < OUT_MAX) o->buf[o->n] = c; else o->too_long = 1;
        o->n++; o->charsout++;
    } else { o->overflow = 1; o->charsout = -1; }               /* _flsbuf: _IOERR, errno ERANGE, EOF */
}
static void out_multi(crt_out* o, uint8_t c, int32_t n) {           /* write_multi_char 78572ec6 */
    while (n > 0) { n--; out_char(o, c); if (o->charsout == -1) break; }
}
static void out_host(crt_out* o, const uint8_t* s, int32_t n) {     /* write_string 78572ef1 (from a host buffer) */
    if (o->counting) { o->charsout += n; return; }
    while (n > 0) { n--; out_char(o, *s++); if (o->charsout == -1) break; }   /* errno is ERANGE here, not EILSEQ: break */
}
static void out_guest(crt_out* o, rx_addr s, int32_t n) {           /* write_string from game memory (%s) */
    if (o->counting) { o->charsout += n; return; }
    while (n > 0) { n--; out_char(o, U8(s, 0)); s++; if (o->charsout == -1) break; }
}

/* ---- %f: the 12-byte arithmetic of $I10_OUTPUT --------------------------------------------------------------------------
 * A 12-byte long double: bytes 0-1 extension, 2-5 low mantissa, 6-9 high mantissa, 10-11 sign and exponent. */
typedef struct { uint8_t b[12]; } ld12;
static uint16_t w12(const ld12* x, int o) { return (uint16_t)(x->b[o] | (x->b[o + 1] << 8)); }
static uint32_t d12(const ld12* x, int o) { return (uint32_t)x->b[o] | ((uint32_t)x->b[o + 1] << 8) | ((uint32_t)x->b[o + 2] << 16) | ((uint32_t)x->b[o + 3] << 24); }
static void sw12(ld12* x, int o, uint16_t v) { x->b[o] = (uint8_t)v; x->b[o + 1] = (uint8_t)(v >> 8); }
static void sd12(ld12* x, int o, uint32_t v) { for (int i = 0; i < 4; i++) x->b[o + i] = (uint8_t)(v >> (8 * i)); }
static void shl12(ld12* x) {
    const uint32_t a = d12(x, 0), b = d12(x, 4), c = d12(x, 8);
    sd12(x, 0, a << 1); sd12(x, 4, (b << 1) | (a >> 31)); sd12(x, 8, (c << 1) | (b >> 31));
}
static void shr12(ld12* x) {
    const uint32_t a = d12(x, 0), b = d12(x, 4), c = d12(x, 8);
    sd12(x, 8, c >> 1); sd12(x, 4, (b >> 1) | (c << 31)); sd12(x, 0, (a >> 1) | (b << 31));
}
static int zero12(const ld12* x) { return (d12(x, 8) & 0x7fffffff) == 0 && d12(x, 4) == 0 && d12(x, 0) == 0; }
static void inf12(ld12* x, uint16_t sign) { sd12(x, 0, 0); sd12(x, 4, 0); sd12(x, 8, sign ? 0xffff8000u : 0x7fff8000u); }
static void zero12_set(ld12* x) { sd12(x, 0, 0); sd12(x, 4, 0); sd12(x, 8, 0); }

/* __ld12mul (inlined twice in $I10_OUTPUT, 78599cef / 78599f53): *x = *x * *y, 16-bit partial products into a 12-byte
 * accumulator (only the upper ones), normalized, rounded at bit 16 (half-up, ties to even on the 0x18000 pattern).
 * Returns 0 normally, 1 when x was zero (the first copy then keeps the zero with a cleared sign word and goes on). */
static int crt_ld12mul(ld12* x, const ld12* y) {
    uint16_t ex = w12(x, 10), ey = w12(y, 10);
    const uint16_t sign = (uint16_t)((ex ^ ey) & 0x8000);
    ex &= 0x7fff; ey &= 0x7fff;
    uint16_t es = (uint16_t)(ex + ey);
    if (ex >= 0x7fff || ey >= 0x7fff || es > 0xbffd) { inf12(x, sign); return 0; }
    if (es <= 0x3fbf) { zero12_set(x); return 0; }
    if (ex == 0) { es++; if (zero12(x)) { sw12(x, 10, 0); return 1; } }
    if (ey == 0) { es++; if (zero12(y)) { zero12_set(x); return 0; } }
    ld12 t; memset(&t, 0, sizeof t);
    for (int i = 0; i < 5; i++) {
        int p = 2 * i, q = 8;
        for (int j = 5 - i; j > 0; j--, p += 2, q -= 2) {
            const uint32_t prod = (uint32_t)w12(x, p) * w12(y, q), r = d12(&t, 2 * i), sum = r + prod;
            sd12(&t, 2 * i, sum);
            if (sum < r || sum < prod) sw12(&t, 2 * i + 4, (uint16_t)(w12(&t, 2 * i + 4) + 1));
        }
    }
    es = (uint16_t)(es + 0xc002);                                        /* - 0x3ffe */
    if ((int16_t)es > 0) {
        while (!(d12(&t, 8) & 0x80000000u)) { shl12(&t); es = (uint16_t)(es - 1); if ((int16_t)es <= 0) break; }
    }
    if ((int16_t)es <= 0) {
        es = (uint16_t)(es - 1);
        if ((int16_t)es < 0) {
            uint16_t count = (uint16_t)(-(int16_t)es); int sticky = 0;
            es = (uint16_t)(es + count);
            while (count--) { if (t.b[0] & 1) sticky++; shr12(&t); }
            if (sticky) sw12(&t, 0, (uint16_t)(w12(&t, 0) | 1));
        }
    }
    if (w12(&t, 0) > 0x8000 || (d12(&t, 0) & 0x1ffff) == 0x18000) {
        if (d12(&t, 2) == 0xffffffffu) {
            sd12(&t, 2, 0);
            if (d12(&t, 6) == 0xffffffffu) {
                sd12(&t, 6, 0);
                if (w12(&t, 10) == 0xffff) { sw12(&t, 10, 0x8000); es++; }
                else sw12(&t, 10, (uint16_t)(w12(&t, 10) + 1));
            } else sd12(&t, 6, d12(&t, 6) + 1);
        } else sd12(&t, 2, d12(&t, 2) + 1);
    }
    if (es >= 0x7fff) { inf12(x, sign); return 0; }
    sw12(x, 0, w12(&t, 2)); sd12(x, 2, d12(&t, 4)); sd12(x, 6, d12(&t, 8)); sw12(x, 10, (uint16_t)(es | sign));
    return 0;
}

/* $I10_OUTPUT 78599ae6 for a finite nonzero-exponent value (the zero / inf / NaN cases are handled by the caller): the decimal
 * exponent and up to `ndigits` digits (fos: exp, sign, ManLen, man). man must hold 23 bytes. */
typedef struct { int16_t exp; char sign; int8_t len; char man[24]; } crt_fos;
static void crt_i10_output(uint64_t mant, uint16_t se, int ndigits, crt_fos* fos) {
    const uint16_t sign = se & 0x8000, expn = se & 0x7fff;
    fos->sign = sign ? '-' : ' ';
    if (expn == 0x7fff) {                                               /* 78599ba0: inf / NaN, exp 1, strcpy_s of the name */
        const uint32_t mh = (uint32_t)(mant >> 32), ml = (uint32_t)mant;
        const char* name;
        if ((mh != 0x80000000u || ml != 0) && !(mh & 0x40000000u)) name = "1#SNAN";            /* 785247a4 */
        else if (sign && mh == 0xc0000000u && ml == 0) name = "1#IND";                         /* 7852479c: the indefinite */
        else if (mh == 0x80000000u && ml == 0) name = "1#INF";                                  /* 78524794 */
        else name = "1#QNAN";                                                                   /* 7852478c */
        fos->exp = 1; fos->len = (int8_t)strlen(name);
        memcpy(fos->man, name, strlen(name) + 1);
        return;
    }
    if (expn == 0 && mant == 0) {                                       /* 78599b79 */
        fos->exp = 0; fos->sign = sign == 0x8000 ? '-' : ' '; fos->len = 1; fos->man[0] = '0'; fos->man[1] = 0; return;
    }
    const uint32_t manhi = (uint32_t)(mant >> 32), manlo = (uint32_t)mant;
    /* nexp ~ log10(x): (expn * 0x4d10 + ((expn >> 8) + (manhi >> 24) * 2) * 0x4d - 0x134312f4) >> 16 */
    const int32_t est = (int32_t)(((expn >> 8) + (manhi >> 24) * 2) * 0x4d + expn * 0x4d10u - 0x134312f4u) >> 16;
    int16_t nexp = (int16_t)est;
    ld12 v; sw12(&v, 0, 0); sd12(&v, 2, manlo); sd12(&v, 6, manhi); sw12(&v, 10, expn);
    /* __multtenpow12(&v, -nexp, 1): the 12-byte power tables _pow10pos 785ba598 / _pow10neg 785ba6f8, 8 entries per group */
    int32_t pw = -(int32_t)nexp;
    rx_addr tab = 0x785ba598u - 0x60;
    if (pw < 0) { tab = 0x785ba6f8u - 0x60; pw = -pw; }
    while (pw) {
        tab += 0x54;
        const int last3 = pw & 7;
        pw >>= 3;
        if (!last3) continue;
        const rx_addr ya = tab + 12u * (uint32_t)last3;
        ld12 y; for (int i = 0; i < 12; i++) y.b[i] = U8(ya, i);
        if (w12(&y, 0) >= 0x8000) sd12(&y, 2, d12(&y, 2) - 1);   /* "unround" the adjacent byte */
        if (crt_ld12mul(&v, &y)) continue;
    }
    if (w12(&v, 10) >= 0x3fff) {                                        /* >= 1: one more tenth */
        nexp++;
        static const ld12 tenth = {{0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xfb, 0x3f}};
        crt_ld12mul(&v, &tenth);
    }
    fos->exp = nexp;
    int nd = ndigits;
    if (nd > 21) nd = 21;
    /* the mantissa as a fixed-point fraction: exponent word cleared, << 8, >> (0x3ffe - exponent) */
    int32_t sh = (int32_t)w12(&v, 10) - 0x3ffe;
    sw12(&v, 10, 0);
    for (int i = 0; i < 8; i++) shl12(&v);
    if (sh < 0) { int c = (-sh) & 0xff; while (c-- > 0) shr12(&v); }
    /* digits: v = v * 10 (mod 2^96), the top byte is the digit, then cleared */
    char* p = fos->man;
    for (int k = nd + 1; k > 0; k--) {
        unsigned __int128 x = (unsigned __int128)d12(&v, 0) | ((unsigned __int128)d12(&v, 4) << 32) | ((unsigned __int128)d12(&v, 8) << 64);
        const unsigned __int128 m96 = (((unsigned __int128)1) << 96) - 1;
        x = (((x << 2) & m96) + x) & m96;
        x = (x << 1) & m96;
        sd12(&v, 0, (uint32_t)x); sd12(&v, 4, (uint32_t)(x >> 32)); sd12(&v, 8, (uint32_t)(x >> 64));
        *p++ = (char)(v.b[11] + '0');
        v.b[11] = 0;
    }
    /* round on the extra digit */
    p--;
    const char last = *p; p--;
    if (last >= '5') {
        while (p >= fos->man && *p == '9') { *p = '0'; p--; }
        if (p < fos->man) { p++; fos->exp++; }
        (*p)++;
    } else {
        while (p >= fos->man && *p == '0') p--;
        if (p < fos->man) { fos->exp = 0; fos->sign = sign == 0x8000 ? '-' : ' '; fos->len = 1; fos->man[0] = '0'; fos->man[1] = 0; return; }
    }
    fos->len = (int8_t)(p - fos->man + 1);
    fos->man[fos->len] = 0;
}

/* _cftof_l + _fptostr + _cftof2_l for 'f' with ndec digits: the text into buf (host, >= 512 bytes). */
static void crt_cftof(double value, int ndec, char decimal, char* buf) {
    /* __dtold 785986c6: the double as an 80-bit value */
    const uint64_t u = rx_db(value);
    const uint16_t sign = (uint16_t)((u >> 48) & 0x8000);
    uint16_t e = (uint16_t)((u >> 52) & 0x7ff);
    uint32_t hi = (uint32_t)(u >> 32) & 0xfffff, lo = (uint32_t)u;
    uint32_t top = 0x80000000u; uint16_t ex;
    if (e == 0) {
        if (hi == 0 && lo == 0) ex = 0;
        else { ex = (uint16_t)(e + 0x3c01); top = 0; }
    } else if (e == 0x7ff) ex = 0x7fff;                                 /* 7859870f: inf / NaN */
    else ex = (uint16_t)(e + 0x3c00);
    uint32_t mh = (lo >> 21) | (hi << 11) | top, ml = lo << 11;
    if (!(e == 0 && hi == 0 && lo == 0)) while (!(mh & 0x80000000u)) { mh = (mh << 1) | (ml >> 31); ml <<= 1; ex = (uint16_t)(ex - 1); }
    else { mh = 0; ml = 0; }
    /* _fltout2 78598788: $I10_OUTPUT(ld, 17, 0) */
    crt_fos fos; crt_i10_output(((uint64_t)mh << 32) | ml, (uint16_t)(sign | ex), 17, &fos);
    int decpt = fos.exp; const int neg = fos.sign == '-';
    /* _fptostr 78545fb0 into buf + neg: '0' then ndec + decpt digits (zeros past the mantissa), rounded on the next one */
    char* b = buf + neg;
    const char* mp = fos.man;
    int digits = ndec + decpt;
    b[0] = '0';
    char* q = b + 1;
    while (digits > 0) { *q++ = *mp ? *mp++ : '0'; digits--; }
    *q = 0;
    if (digits >= 0 && *mp >= '5') {
        q--;
        while (*q == '9') *q-- = '0';
        (*q)++;
    }
    if (b[0] == '1') decpt++;
    else memmove(b, b + 1, strlen(b + 1) + 1);
    /* _cftof2_l 78598f6d (not %g): sign, integer part (or '0'), decimal point, leading zeros of the fraction */
    char* p = buf;
    if (neg) *p++ = '-';
    if (decpt <= 0) { memmove(p + 1, p, strlen(p) + 1); *p++ = '0'; }
    else p += decpt;
    if (ndec > 0) {
        memmove(p + 1, p, strlen(p) + 1);
        *p++ = decimal;
        if (decpt < 0) {
            const int z = ndec < -decpt ? ndec : -decpt;
            memmove(p + z, p, strlen(p) + 1);
            memset(p, '0', (size_t)z);
        }
    }
}

/* ---- _output_l 78570f34 on a string FILE ------------------------------------------------------------------------------------
 * Returns 0 when the original must run (nothing stored yet), else 1 with o filled. locinfo: the _LocaleUpdate's locinfo. */
#define FL_SIGN 0x1
#define FL_SIGNSP 0x2
#define FL_LEFT 0x4
#define FL_LEADZERO 0x8
#define FL_LONG 0x10
#define FL_SHORT 0x20
#define FL_SIGNED 0x40
#define FL_ALTERNATE 0x80
#define FL_NEGATIVE 0x100
#define FL_FORCEOCTAL 0x200
#define FL_WIDECHAR 0x800
#define FL_LONGLONG 0x1000
#define FL_I64 0x8000

typedef struct { rx_addr lo, hi; } crt_range;
static int crt_output(crt_out* o, rx_addr format, rx_addr locinfo, rx_addr args, crt_range* srcs, int* nsrc) {
    int state = 0;
    int32_t flags = 0, width = 0, precision = -1, prefixlen = 0, capexp = 0, textlen = 0, hexadd = 0, radix = 0;
    char prefix[2] = {0, 0};
    uint8_t text[600]; const uint8_t* textp = text; rx_addr gtext = 0; int text_guest = 0;
    rx_addr f = format;
    (void)capexp;
    for (;;) {
        uint8_t ch = U8(f, 0);
        if (ch == 0 || o->charsout < 0) break;
        f++;
        const int cls = (uint8_t)(ch - 0x20) <= 0x58 ? (int8_t)U8(0x78521bf0u, ch) & 0xf : 0;          /* __lookuptable */
        state = (int8_t)U8(0x78521c10u, (uint32_t)(cls * 8 + state)) >> 4;
        if ((unsigned)state > 7) continue;                                              /* ja: no state */
        switch (state) {
        case 0:                                                                          /* ST_NORMAL */
        normal:
            if (U16(U32(locinfo, 0xc8), 2u * ch) & 0x8000) return CRT_WHY("a lead byte (_isleadbyte_l)"), 0;                    /* a lead byte (_isleadbyte_l) */
            out_char(o, ch);
            break;
        case 1:                                                                          /* ST_PERCENT */
            precision = -1; capexp = 0; width = 0; prefixlen = 0; flags = 0;
            break;
        case 2:                                                                          /* ST_FLAG */
            switch (ch) {
            case ' ': flags |= FL_SIGNSP; break; case '#': flags |= FL_ALTERNATE; break; case '+': flags |= FL_SIGN; break;
            case '-': flags |= FL_LEFT; break; case '0': flags |= FL_LEADZERO; break;
            }
            break;
        case 3:                                                                          /* ST_WIDTH */
            if (ch == '*') { width = (int32_t)U32(args, 0); args += 4; if (width < 0) { flags |= FL_LEFT; width = -width; } }
            else width = width * 10 + (ch - '0');
            break;
        case 4: precision = 0; break;                                                    /* ST_DOT */
        case 5:                                                                          /* ST_PRECIS */
            if (ch == '*') { precision = (int32_t)U32(args, 0); args += 4; if (precision < 0) precision = -1; }
            else precision = precision * 10 + (ch - '0');
            break;
        case 6:                                                                          /* ST_SIZE */
            if (ch == 'I') {
                const uint8_t c1 = U8(f, 0), c2 = U8(f, 1);
                if (c1 == '6' && c2 == '4') { f += 2; flags |= FL_I64; }
                else if (c1 == '3' && c2 == '2') { f += 2; flags &= ~FL_I64; }
                else if (c1 == 'd' || c1 == 'i' || c1 == 'o' || c1 == 'u' || c1 == 'x' || c1 == 'X') {}
                else { state = 0; goto normal; }
            } else if (ch == 'h') flags |= FL_SHORT;
            else if (ch == 'l') { if (U8(f, 0) == 'l') { f++; flags |= FL_LONGLONG; } else flags |= FL_LONG; }
            else if (ch == 'w') flags |= FL_WIDECHAR;
            break;
        case 7: {                                                                        /* ST_TYPE */
            text_guest = 0;
            switch (ch) {
            case 'c':
                if (flags & (FL_LONG | FL_WIDECHAR)) return CRT_WHY("wctomb"), 0;                          /* wctomb */
                text[0] = (uint8_t)U32(args, 0); args += 4; textlen = 1; textp = text;
                break;
            case 's': {
                if (flags & (FL_LONG | FL_WIDECHAR)) return CRT_WHY("a wide string"), 0;                          /* a wide string */
                int32_t i = precision == -1 ? 0x7fffffff : precision;
                gtext = U32(args, 0); args += 4;
                if (gtext == 0) gtext = U32(0, 0x785b7f3c);                             /* __nullstring "(null)" */
                rx_addr p = gtext;
                while (i != 0) { i--; if (!U8(p, 0)) break; p++; }
                textlen = (int32_t)(p - gtext); text_guest = 1;
                if (*nsrc < 64) { srcs[*nsrc].lo = gtext; srcs[*nsrc].hi = gtext + (uint32_t)textlen; (*nsrc)++; } else return CRT_WHY("more than 64 %s"), 0;
                break;
            }
            case 'f': {
                flags |= FL_SIGNED;
                if (precision < 0) precision = 6;
                if (precision > 0x200) precision = 0x200;
                if (precision > 0xa3) return CRT_WHY("the heap buffer"), 0;                                          /* the heap buffer */
                const double d = rx_bd((uint64_t)U32(args, 0) | ((uint64_t)U32(args, 4) << 32)); args += 8;
                /* inf / NaN: $I10_OUTPUT's "1#INF" / "1#QNAN" / "1#SNAN" / "1#IND" with exponent 1 go through _fptostr's digits and
                 * rounding like any mantissa ("%.6f" "1.#INF00", "%.1f" of inf "1.$", "%.2f" "1.#J", "%.0f" "1") */
                const char decimal = (char)U8(U32(U32(locinfo, 0xbc), 0), 0);           /* lconv->decimal_point[0] */
                char buf[600]; memset(buf, 0, sizeof buf);
                crt_cftof(d, precision, decimal, buf);
                if ((flags & FL_ALTERNATE) && precision == 0) {
                    /* 7859881d _forcdecpt_l (only for precision 0, 7857168d): past the first character (not 'e') and the digits (the C
                     * locale's ctype table, C1_DIGIT), past "x" when there, the locale's decimal point inserted, the rest moved up one */
                    char* q = buf;
                    if ((*q | 0x20) != 'e') { q++; while (*q >= '0' && *q <= '9') q++; }
                    if ((*q | 0x20) == 'x') q += 2;
                    memmove(q + 1, q, strlen(q) + 1);
                    *q = decimal;
                }
                char* t = buf;
                if (*t == '-') { flags |= FL_NEGATIVE; t++; }
                textlen = (int32_t)strlen(t);
                memcpy(text, t, (size_t)textlen + 1); textp = text;
                break;
            }
            case 'd': case 'i': flags |= FL_SIGNED; radix = 10; goto common_int;
            case 'u': radix = 10; goto common_int;
            case 'o': radix = 8; if (flags & FL_ALTERNATE) flags |= FL_FORCEOCTAL; goto common_int;
            case 'p': precision = 8; hexadd = 7; goto common_hex;
            case 'X': hexadd = 7; goto common_hex;
            case 'x': hexadd = 0x27;
            common_hex:
                radix = 16;
                if (flags & FL_ALTERNATE) { prefix[0] = '0'; prefix[1] = (char)(hexadd + 0x51); prefixlen = 2; }
            common_int: {
                uint64_t number;
                if (flags & (FL_I64 | FL_LONGLONG)) { number = (uint64_t)U32(args, 0) | ((uint64_t)U32(args, 4) << 32); args += 8; }
                else {
                    const uint32_t a = U32(args, 0); args += 4;
                    if (flags & FL_SHORT) number = (flags & FL_SIGNED) ? (uint64_t)(int64_t)(int16_t)a : (uint64_t)(uint16_t)a;
                    else number = (flags & FL_SIGNED) ? (uint64_t)(int64_t)(int32_t)a : (uint64_t)a;
                }
                if ((flags & FL_SIGNED) && (int64_t)number < 0) { number = (uint64_t)0 - number; flags |= FL_NEGATIVE; }
                if (!(flags & (FL_I64 | FL_LONGLONG))) number &= 0xffffffffu;
                if (precision < 0) precision = 1;
                else { flags &= ~FL_LEADZERO; if (precision > 0x200) precision = 0x200; }
                if (number == 0) prefixlen = 0;
                int pos = 511;                                                           /* buffer.sz[BUFFERSIZE - 1] */
                while (precision-- > 0 || number != 0) {
                    int digit = (int)(number % (uint64_t)(int64_t)radix) + '0';          /* _aulldvrm */
                    number /= (uint64_t)(int64_t)radix;
                    if (digit > '9') digit += hexadd;
                    text[pos--] = (uint8_t)digit;
                }
                textlen = 511 - pos; pos++;
                if ((flags & FL_FORCEOCTAL) && (textlen == 0 || text[pos] != '0')) { text[--pos] = '0'; textlen++; }
                textp = text + pos;
                break;
            }
            default:
#ifdef RX_ORACLE
                { static char why[32]; why[0] = '%'; why[1] = (char)ch; why[2] = 0; CRT_WHY(why); }
#endif
                return 0;            /* e E g G a A C S Z n and the unknown types (which re-print the previous text). No ported caller formats %e / %g: the
                                    * EXE's only one is Lua's "%.14g" (0096eabc, 00784ed0 tostring), which the core does not run */
            }
            /* the common tail 785718be: sign prefix, padding, text */
            if (flags & FL_SIGNED) {
                if (flags & FL_NEGATIVE) { prefix[0] = '-'; prefixlen = 1; }
                else if (flags & FL_SIGN) { prefix[0] = '+'; prefixlen = 1; }
                else if (flags & FL_SIGNSP) { prefix[0] = ' '; prefixlen = 1; }
            }
            const int32_t pad = width - textlen - prefixlen;
            if (!(flags & (FL_LEFT | FL_LEADZERO))) out_multi(o, ' ', pad);
            out_host(o, (const uint8_t*)prefix, prefixlen);
            if ((flags & FL_LEADZERO) && !(flags & FL_LEFT)) out_multi(o, '0', pad);
            if (text_guest) out_guest(o, gtext, textlen); else out_host(o, textp, textlen);
            if (o->charsout >= 0 && (flags & FL_LEFT)) out_multi(o, ' ', pad);
            break;
        }
        }
    }
    return 1;
}

/* The three entry points: format into o, then store the characters at buf (and the terminator where the original writes it). */
typedef enum { CRT_SPRINTF, CRT_VSNPRINTF } crt_kind;
static int crt_format(rx_addr buf, uint32_t count, rx_addr format, rx_addr locale, rx_addr args, crt_kind kind, int32_t* result) {
    if (format == 0) return CRT_WHY("no format"), 0;
    rx_addr locinfo;
    if (locale) locinfo = U32(locale, 0);                                               /* _LocaleUpdate with a _locale_t */
    else { locinfo = crt_thread_locinfo(); if (!locinfo) return 0; }
    static crt_out o;
    memset(&o, 0, sizeof o);
    o.cnt = kind == CRT_SPRINTF ? 0x7fffffff : (int32_t)(count > 0x7fffffffu ? 0x7fffffffu : count);
    o.counting = buf == 0;
    crt_range srcs[64]; int nsrc = 0;
    if (!crt_output(&o, format, locinfo, args, srcs, &nsrc)) return 0;
    if (o.too_long) return CRT_WHY("output longer than 4096"), 0;
    /* the terminator: --cnt >= 0 ? *ptr = 0 : _flsbuf (errno ERANGE) -- _vsnprintf only when it has a buffer */
    int term = 0;
    if (kind == CRT_SPRINTF || buf != 0) { if (--o.cnt >= 0) term = 1; else o.overflow = 1; }
    /* a %s source inside the output: the original reads characters it already wrote */
    const uint32_t end = buf + o.n + (uint32_t)term;
    for (int i = 0; i < nsrc; i++) if (buf != 0 && srcs[i].lo < end && srcs[i].hi > buf) return CRT_WHY("a %s source overlaps the output"), 0;
    rx_addr ptd = 0;
    if (o.overflow) { ptd = crt_ptd(); if (!ptd) return CRT_WHY("no per-thread data for errno"), 0; }
    for (uint32_t i = 0; i < o.n; i++) SU8(buf, i, o.buf[i]);
    if (term) SU8(buf, o.n, 0);
    if (o.overflow) SU32(ptd, 8, 34);                                                   /* errno = ERANGE */
    *result = o.charsout;
    return 1;
}

/* 78552b7e sprintf(buffer, format, ...): args = the address of the first variadic argument. */
int32_t crt_sprintf(rx_addr buf, rx_addr format, rx_addr args) {
    int32_t r;
    if (buf != 0 && crt_format(buf, 0, format, 0, args, CRT_SPRINTF, &r)) return r;
    /* NULL arguments (errno, _invalid_parameter) and the cases above: the original, with a copy of the first 32 dwords of
     * the variadic block (it reads only those its format names) */
    uint32_t a[2 + 32] = {buf, format};
    for (int i = 0; i < 32; i++) a[2 + i] = U32(args, 4u * (uint32_t)i);
    return (int32_t)crt_unported(0x78552b7e, 2 + 32, a);
}
RX_FUNCTION(0x78552b7e, crt_sprintf)
/* 78554666 _vsnprintf_l(buffer, count, format, locale, va_list) */
int32_t crt_vsnprintf_l(rx_addr buf, uint32_t count, rx_addr format, rx_addr locale, rx_addr args) {
    int32_t r;
    if (!(buf == 0 && count != 0) && crt_format(buf, count, format, locale, args, CRT_VSNPRINTF, &r)) return r;
    const uint32_t a[5] = {buf, count, format, locale, args};
    return (int32_t)crt_unported(0x78554666, 5, a);
}
RX_FUNCTION(0x78554666, crt_vsnprintf_l)
/* 7855471e _vsnprintf(buffer, count, format, va_list) = _vsnprintf_l(..., NULL, ...) */
int32_t crt_vsnprintf(rx_addr buf, uint32_t count, rx_addr format, rx_addr args) {
    int32_t r;
    if (!(buf == 0 && count != 0) && crt_format(buf, count, format, 0, args, CRT_VSNPRINTF, &r)) return r;
    const uint32_t a[4] = {buf, count, format, args};
    return (int32_t)crt_unported(0x7855471e, 4, a);
}
RX_FUNCTION(0x7855471e, crt_vsnprintf)
