/* crt_math.c -- msvcr90.dll's libm entry points the game calls: _copysign, _finite, floor, ceil, _CIatan2, _CIfmod, _CIpow
 * (with its SSE2 kernel) and __libm_sse2_pow. Hand transliterations of the DLL's code (9.0.30729.1), bit-exact including
 * the x87 values they leave and the memory they write.
 *
 * Conventions. The `_CI*` functions take their operands on the x87 stack and leave the result in st(0) (popping the operands);
 * here they take rx_f80 values and return the rx_f80 result. floor / ceil / _copysign / _finite are cdecl with double
 * arguments on the stack; floor / ceil also store their result into their own argument slot (crt_floor_args / crt_ceil_args
 * reproduce that for the adapters; core code calls crt_floor / crt_ceil). The x87 code of the dispatcher (__cintrindisp2)
 * switches to its own control word 0x133f (64-bit precision, round to nearest, all exceptions masked) for the computation and
 * restores the caller's afterwards, so the caller's precision does not enter atan2 / fmod; the SSE2 paths of floor / ceil /
 * pow need the caller's control word only for their dispatch test (cw & 0x7f == 0x7f: all x87 exceptions masked, the game's
 * case on every thread). */
#include "crt.h"
#include "libm_pow_zero.h"
#include "crt_util.h"

rx_addr crt_bypass;
/* crt_entry_sp / crt_caller_regs: the running guest thread's (rx_cpu in rx_mem.h, w-jobthreads) */
#ifdef RX_ORACLE
const char* crt_why;
unsigned crt_fallbacks;
#endif

rx_f80 crt_unported_x87(rx_addr fn, int n_in, const rx_f80* in) {
    rx_regs r, out;
    memset(&r, 0, sizeof r);
    r.fpu_in = n_in;
    for (int i = 0; i < n_in; i++) r.fpu[i] = in[i];
    crt_unported_ex(fn, &r, &out, 0, NULL);
    return out.st[0];
}

/* ---- 785a1254 _copysign(x, y): x with y's sign (the high dword is ((y ^ x) & 0x7fffffff) ^ y), loaded by fld qword. */
double crt_copysign(double x, double y) {
    const uint64_t ux = rx_db(x), uy = rx_db(y);
    const uint32_t hx = (uint32_t)(ux >> 32), hy = (uint32_t)(uy >> 32);
    const uint32_t hi = ((hy ^ hx) & 0x7fffffffu) ^ hy;
    return rx_bd(((uint64_t)hi << 32) | (uint32_t)ux);
}
RX_FUNCTION(0x785a1254, crt_copysign)

/* ---- 785a166d _finite(x): 1 unless the exponent field is all ones (inf / NaN). */
int32_t crt_finite(double x) {
    return ((uint16_t)(rx_db(x) >> 48) & 0x7ff0) != 0x7ff0;
}
RX_FUNCTION(0x785a166d, crt_finite)

/* ---- floor / ceil: the SSE2 path (785a2fd0 / 785a2de0) when __sse2_available and both the MXCSR and the x87 control word mask
 * every exception; the x87 path (785a9330 / 785a9256, _ctrlfp + _frnd) is not ported (the game never takes it).
 * The SSE2 code truncates by shifting the mantissa right and back left by 0x433 - exponent (psrlq / psllq: counts above 63 give
 * 0), then corrects by one where the truncation went the wrong way (cmpltpd / cmpnlepd masks of 1.0). Results that are stored
 * go to the argument slot ([esp+4]) and are loaded from there by fld qword. */
static int crt_sse2_libm(uint16_t cw, uint32_t mxcsr) {
    return U32(0, CRT_SSE2) != 0 && (mxcsr & 0x1f80) == 0x1f80 && (cw & 0x7f) == 0x7f;
}
static uint64_t crt_shr64(uint64_t v, uint64_t n) { return n > 63 ? 0 : v >> n; }
static uint64_t crt_shl64(uint64_t v, uint64_t n) { return n > 63 ? 0 : v << n; }
/* the 64-bit shift count: low dword 0x433 - exponent (psubd), high dword 0 */
static uint64_t crt_trunc_count(uint32_t e11) { return (uint32_t)(0x433u - e11); }

/* floor on the double at `args` (the cdecl argument slot); *stored: whether the result was written back to the slot. */
static double crt_floor_sse2(double x, int* stored) {
    const uint64_t u = rx_db(x);
    const uint32_t e = (uint32_t)(u >> 52);                 /* sign and exponent */
    const uint64_t n = crt_trunc_count(e & 0x7ff);
    const uint64_t t = crt_shl64(crt_shr64(u, n), n);       /* x truncated toward zero (when 0x3ff <= exponent <= 0x432) */
    *stored = 0;
    if (!(e & 0x800)) {
        if ((int32_t)e < 0x3ff) return 0.0;                 /* 0 <= x < 1: fldz */
        if ((int32_t)e > 0x432) return x;                   /* integral already, inf (NaN: crt_floor) */
        *stored = 1;
        return rx_bd(t);
    }
    if ((int32_t)e < 0xbff) {                               /* -1 < x <= -0: -1.0, or -0.0 for -0 (cmpltpd x, -0.0) */
        *stored = 1;
        return x < -0.0 ? -1.0 /* 0x785347b0 */ : -0.0;
    }
    if ((int32_t)e > 0xc32) return x;
    *stored = 1;
    return rx_bd(t) - (x < rx_bd(t) ? 1.0 /* 0x78534790 */ : 0.0);
}
static double crt_ceil_sse2(double x, int* stored) {
    const uint64_t u = rx_db(x);
    const uint32_t e = (uint32_t)(u >> 52);
    const uint64_t n = crt_trunc_count(e & 0x7ff);
    const uint64_t t = crt_shl64(crt_shr64(u, n), n);
    *stored = 0;
    if (e & 0x800) {
        if ((int32_t)e < 0xbff) return -0.0;                /* -1 < x < 0: fld qword [0x78534780] = -0.0 */
        if ((int32_t)e > 0xc32) return x;
        *stored = 1;
        return rx_bd(t);
    }
    if ((int32_t)e < 0x3ff) {                               /* 0 <= x < 1: 1.0 when x > 0 (cmpnlepd x, 0.0), else +0 */
        *stored = 1;
        return x > 0.0 ? 1.0 /* 0x78534740 */ : 0.0;
    }
    if ((int32_t)e > 0x432) return x;
    *stored = 1;
    return rx_bd(t) + (!(x <= rx_bd(t)) ? 1.0 /* 0x78534740 */ : 0.0);
}

/* ---- the NaN path of floor / ceil: 785a3023 / 785a2e33 (ucomisd x, x unordered) -> ___libm_error_support 785a8fa1(&x, &x, &x,
 * 0x3ed floor / 0x3ec ceil). Its jump-table case (785a91b1 / 785a91a8): name "floor" 0x7853f2d8 / "ceil" 0x7853f2e0, *arg3 = *arg1
 * (fld / fstp: an SNaN is quieted), then the _exception record {type 1 (_DOMAIN), name, arg1, arg2, retval} on its frame, the
 * _matherr handler called with it, errno = EDOM (0x21) when the handler returns 0, and *arg3 = retval. floor / ceil then load the
 * argument slot (fld qword [esp+4]): the result is the quieted NaN (payload kept) and the slot holds it. EAX = &errno.
 * The handler: [0x785bc644] != 0 (set by __setusermatherr, which the EXE's pre_c_init 009005c6 calls with its _matherr 009008e5:
 * xor eax, eax; ret) -> the decoded [0x785bd7ac]; else msvcr90's _matherr 78541c20, which returns 0 while [0x785b7000] != 0 (else
 * it would call the decoded [0x785bb3c4]). Both return 0 in the game. Decided before any store: a handler that is neither runs
 * the original (crt_unported; never the game's). The product does not run pre_c_init, so its flag is 0 and the msvcr90 default
 * answers (same result, same errno).
 * frame: the argument slot address (floor / ceil's [esp+4]) when the caller has the original's stack there, 0 for none (crt_floor /
 * crt_ceil). With a frame the dead stack the path leaves is written as the original leaves it (later code copies the stack under
 * PackedGrid_SampleUV uninitialized, notes/world.md): [A-0x14 .. A-0x8] the four arguments (over floor's fnstcw / stmxcsr slots),
 * [A-0x18] the return address, [A-0x1c] EBP, [A-0x44 .. A-0x1d] the _exception record and the zeroed bytes, [A-0x50 .. A-0x48]
 * EDI / ESI / EBX (crt_caller_regs: the caller's registers at the call), [A-0x54] / [A-0x58] the return addresses of the _errno
 * call (785a920d, over the record address pushed for _matherr) and of its _getptd_noexit call (78548558), [A-0x5c] / [A-0x60]
 * _getptd_noexit's saved ESI / EDI (both A: ___libm_error_support loaded its pointer arguments into them). Not reproduced: the
 * words below A-0x60 (the kernel32 calls' frames: GetLastError / FlsGetValue / SetLastError, the saved last-error value). The MXCSR / x87 IE flags
 * an SNaN raises are not modelled (nothing reads them: floor tests only the mask bits). */
#define CRT_ERRNO_NOMEM 0x785b7358u   /* _errno's static int when the thread has no data */
static int crt_matherr_returns_zero(void) {
    if (U32(0, 0x785bc644) != 0) {
#ifdef RX_ORACLE
        return crt_decode_ptr(U32(0, 0x785bd7ac)) == 0x009008e5u;   /* the EXE's _matherr */
#else
        return 1;   /* not set in the product (no pre_c_init); the only handler the game installs is 009008e5 (returns 0) */
#endif
    }
    return U32(0, 0x785b7000) != 0;                                   /* 78541c25: no further handler */
}
static int crt_libm_nan_error(double x, rx_addr frame, uint32_t code, uint32_t* eax) {
    if (!crt_matherr_returns_zero()) return CRT_WHY("_matherr handler"), 0;
    const uint64_t q = rx_db(x) | 0x0008000000000000ull;                /* the quieted NaN (fld / fstp) */
    const int is_floor = code == 0x3edu;
    /* 78548553 _errno: _getptd_noexit + 8 (its OS calls, oracle: made below the frame, where the original makes them) */
    const rx_addr sp = rx_sp();
    if (frame) rx_set_sp(frame - 0x60);
    const rx_addr ptd = crt_ptd();
    rx_set_sp(sp);
    if (frame) {
        const rx_addr A = frame;
        for (uint32_t o = 0x14; o >= 0x0c; o -= 4) SU32(A, -(int32_t)o, A);
        SU32(A, -0x08, code);
        SU32(A, -0x18, is_floor ? 0x785a304au : 0x785a2e5au);
        SU32(A, -0x1c, crt_caller_regs[5]);
        SU32(A, -0x20, 0); SU32(A, -0x24, 0);                         /* [ebp-8 .. ebp-1] = 0 */
        SU32(A, -0x2c, (uint32_t)q); SU32(A, -0x28, (uint32_t)(q >> 32));   /* retval */
        SU32(A, -0x34, (uint32_t)q); SU32(A, -0x30, (uint32_t)(q >> 32));   /* arg2 */
        SU32(A, -0x3c, (uint32_t)q); SU32(A, -0x38, (uint32_t)(q >> 32));   /* arg1 */
        SU32(A, -0x40, is_floor ? 0x7853f2d8u : 0x7853f2e0u);         /* name */
        SU32(A, -0x44, 1);                                             /* type _DOMAIN */
        SU32(A, -0x48, crt_caller_regs[3]); SU32(A, -0x4c, crt_caller_regs[6]); SU32(A, -0x50, crt_caller_regs[7]);
        SU32(A, -0x54, 0x785a920du);                                   /* the _errno call (over the record address pushed for _matherr) */
        SU32(A, -0x58, 0x78548558u);                                   /* _errno's _getptd_noexit call */
        SU32(A, -0x5c, A); SU32(A, -0x60, A);                          /* _getptd_noexit's push esi / edi (785a8fac..b4: both = A) */
        SU32(A, 0, (uint32_t)q); SU32(A, 4, (uint32_t)(q >> 32));      /* *arg3 = retval */
    }
    const rx_addr err = ptd ? ptd + 8 : CRT_ERRNO_NOMEM;
    SU32(err, 0, 0x21);                                                /* EDOM */
    if (eax) *eax = err;
    return 1;
}

/* 785a2f90 floor (cdecl, the double at args = [esp+4]; result st(0); x87 control word cw, MXCSR mxcsr of the caller). *eax = the
 * EAX it leaves: (sign and exponent) on the SSE2 paths, &errno on the NaN path. The x87 path 785a9330 (no SSE2, or an exception
 * unmasked in the MXCSR or the x87 control word) is not ported: the game never takes it (every thread's word masks all exceptions;
 * the EXE's only control-word change is _setdefaultprecision 009008ba, the precision field; the MXCSR stays 0x1f80). */
double crt_floor_args_ex(rx_addr args, uint16_t cw, uint32_t mxcsr, uint32_t* eax) {
    const double x = F64(args, 0);
    const uint32_t a[2] = {U32(args, 0), U32(args, 4)};
    if (eax) *eax = a[1] >> 20;
    if (U32(0, CRT_SSE2) != 0) {                                 /* sub esp, 8; stmxcsr [esp+4]; fnstcw [esp] (dead stack) */
        SU32(args, -8, mxcsr);
        if ((mxcsr & 0x1f80) == 0x1f80) SU16(args, -0xc, cw);
    }
    if (crt_sse2_libm(cw, mxcsr) && crt_isnan64(rx_db(x)) && crt_libm_nan_error(x, args, 0x3ed, eax)) return F64(args, 0);
    if (!crt_sse2_libm(cw, mxcsr) || crt_isnan64(rx_db(x))) {   /* the x87 path (not reached) / a foreign _matherr */
        rx_regs r, out; memset(&r, 0, sizeof r);
        crt_unported_ex(0x785a2f90, &r, &out, 2, a);
        if (eax) *eax = out.r[0];
        return rx_f80_to_f64(out.st[0], 0);
    }
    int stored; const double r = crt_floor_sse2(x, &stored);
    if (stored) SF64(args, 0, r);
    return r;
}
double crt_floor_args(rx_addr args, uint16_t cw, uint32_t mxcsr) { return crt_floor_args_ex(args, cw, mxcsr, NULL); }
RX_FUNCTION(0x785a2f90, crt_floor_args)
/* floor for core callers without the original's stack: the NaN path's errno, no frame. */
double crt_floor(double x) {
    int stored; (void)stored;
    if (crt_isnan64(rx_db(x)) && crt_libm_nan_error(x, 0, 0x3ed, NULL)) return rx_bd(rx_db(x) | 0x0008000000000000ull);
    if (crt_isnan64(rx_db(x))) return rx_bd(rx_db(x) | 0x0008000000000000ull);   /* a foreign _matherr: not the game's */
    return crt_floor_sse2(x, &stored);
}

/* 785a2da0 ceil (as floor; NaN: code 0x3ec, the x87 path 785a9256 not ported). */
double crt_ceil_args_ex(rx_addr args, uint16_t cw, uint32_t mxcsr, uint32_t* eax) {
    const double x = F64(args, 0);
    const uint32_t a[2] = {U32(args, 0), U32(args, 4)};
    if (eax) *eax = a[1] >> 20;
    if (U32(0, CRT_SSE2) != 0) {                                 /* sub esp, 8; stmxcsr [esp+4]; fnstcw [esp] (dead stack) */
        SU32(args, -8, mxcsr);
        if ((mxcsr & 0x1f80) == 0x1f80) SU16(args, -0xc, cw);
    }
    if (crt_sse2_libm(cw, mxcsr) && crt_isnan64(rx_db(x)) && crt_libm_nan_error(x, args, 0x3ec, eax)) return F64(args, 0);
    if (!crt_sse2_libm(cw, mxcsr) || crt_isnan64(rx_db(x))) {
        rx_regs r, out; memset(&r, 0, sizeof r);
        crt_unported_ex(0x785a2da0, &r, &out, 2, a);
        if (eax) *eax = out.r[0];
        return rx_f80_to_f64(out.st[0], 0);
    }
    int stored; const double r = crt_ceil_sse2(x, &stored);
    if (stored) SF64(args, 0, r);
    return r;
}
double crt_ceil_args(rx_addr args, uint16_t cw, uint32_t mxcsr) { return crt_ceil_args_ex(args, cw, mxcsr, NULL); }
RX_FUNCTION(0x785a2da0, crt_ceil_args)
double crt_ceil(double x) {
    int stored; (void)stored;
    if (crt_isnan64(rx_db(x)) && crt_libm_nan_error(x, 0, 0x3ec, NULL)) return rx_bd(rx_db(x) | 0x0008000000000000ull);
    if (crt_isnan64(rx_db(x))) return rx_bd(rx_db(x) | 0x0008000000000000ull);
    return crt_ceil_sse2(x, &stored);
}

/* ---- _CIatan2 / _CIfmod: 7859aa6a / 7859ac8a load their descriptor (EDX = 0x785bb0a0 "atan2", 0x785ba8b0 "fmod") and jump to
 * the shared __cintrindisp2 785a4840: fnstcw (the caller's word), control word 0x133f (785a3ba7: the descriptor's byte +0xe is
 * not 5), both operands classified with fxam (st(0) and st(1): number / zero / NaN / infinity, the emulator has no denormals)
 * and a jump through the descriptor's 4 x 4 handler table; then 785a490a checks the handler's error byte, restores the caller's
 * control word and returns. The handlers that report an error (NaN operands, atan2(inf, inf) is not one: it loads the indefinite
 * with error byte 1) go to the CRT's error path (785a49ef: _87except, _matherr, errno), which is not ported; the 64-bit result
 * of atan2 checked as a double (785a496a) can underflow there too (|atan2| < 2^-1022), equally not ported.
 * 785bc614 (nonzero: no error checks at all) is read as the original does. */
static const rx_f80 kPi = {0, 0, 0xc90fdaa22168c235ull, 1};          /* fldpi (64 bits, round to nearest) */
#define CRT_HALF_PI crt_f80_raw(0xc90fdaa22168c235ull, 0x3fff)         /* fld tbyte [0x785bb07a] */

/* The dispatcher's tail 785a490a for a handler that left error byte `err` (0 none, 0xfe range-check the result as a double).
 * Returns 0 when the original would take its error path (the caller falls back to the original). */
static int crt_disp_ok(int err, rx_f80 r, uint16_t cw) {
    if (U32(0, 0x785bc614) != 0) return 1;
    if (err == 0xfe) {
        const uint16_t hi = (uint16_t)(rx_db(rx_f80_to_f64(r, 0)) >> 48) & 0x7ff0;   /* fst qword [ebp-0x2d0] */
        if (hi == 0 || hi == 0x7ff0) return 0;                                         /* underflow (4) / overflow (3) */
    } else if (err != 0) return 0;
    return (cw & 0x20) != 0;    /* precision exceptions masked: done; else the status word's PE decides (not ported) */
}

/* 785a3ba7: the computation's control word. The descriptor's byte +0xe is 5 for none of the two functions here (atan2 0x10,
 * fmod 0x16): 0x133f, 64-bit precision; with 5 the caller's word with 53-bit precision and exceptions masked. Returns the
 * precision in bits (the rounding is the caller's with 5, nearest otherwise: *rc). */
static int crt_disp_bits(rx_addr desc, uint16_t cw, int* rc) {
    if (U8(desc, 0xe) != 5) { *rc = 0; return 64; }
    *rc = (cw >> 10) & 3;
    return 53;
}

/* ---- the handlers that report an error, and the error path 785a49ef they lead to.
 * 785a3c7f (st(0) a NaN), 785a3c7d (st(1): fxch first), 785a3ca7 (both): error byte 7 when the NaN is quiet (the significand's bit 62,
 * read back through fstp / fld tbyte), 1 when signalling (only a tbyte load can put one on the stack: the game loads floats / doubles);
 * the result is faddp st(1) (the x87 NaN rules: an SNaN quieted, the larger significand of two NaNs). 785a3ce6: both popped, the
 * indefinite fld tbyte [0x785bb070] (-QNaN 0xc000000000000000), error byte 1 (_DOMAIN).
 * 785a490a with error byte 7 / 1: [ebp-0x8e] = the type, 785a49ef: the name (descriptor + 1) into the _exception record, the operands
 * not copied (785a4860 set the record's flag bit 0), the result stored as a double (fstp qword [ebp-0x76], the computation's word
 * 0x133f: nearest), _87except 785a951e(descriptor byte +0xe (0x10 atan2, 0x16 fmod), &record, &the caller's word): type 7 becomes 1
 * (_DOMAIN) without the exception handling; type 1 goes through _handle_exc 785a6d2e with the IEEE invalid flag 8, which with the
 * invalid exception masked (cw bit 0: every thread's word) only sets the status flag; _ctrlfp(cw, 0xffff); _matherr only when
 * [0x785b7000] == 0 (it is not: 0x2694, never written); _set_errno 785a6f15: _DOMAIN -> errno = EDOM (33). The result is the double
 * loaded back (fld qword). EAX = &errno (_set_errno's _errno), ECX = __security_cookie [0x785ba2a8] (_87except's cookie check:
 * [ebp-4] ^ ebp), EDX = the descriptor still (the dispatcher's EDX through _errno / _getptd_noexit: the OS calls there keep it in
 * the oracle's stand-ins; the real kernel32 may not, nothing reads it).
 * Declined (the original runs): the invalid exception unmasked (_raise_exc), [0x785b7000] == 0 (_matherr), and the range errors of
 * the 0xfe check (types 3 / 4: _handle_exc's overflow / underflow results), which need operands beyond the float range. The frames
 * of _87except / _handle_exc / _set_statfp below the dispatcher's are dead stack, not reproduced. */
static rx_f80 crt_nan_handler(rx_f80 st1, rx_f80 st0, int* err) {
    const int c0 = crt_f80_class(st0), c1 = crt_f80_class(st1);
    if (c0 == 2 && c1 == 2) {                                         /* 785a3ca7 */
        const int q0 = (st0.mant >> 62) & 1, q1 = (st1.mant >> 62) & 1;
        *err = q0 && q1 ? 7 : 1;
        return q0 ? rx_f80_add(st0, st1, 64, 0) : rx_f80_add(st1, st0, 64, 0);   /* after the fxch (QNaN st(0)) the operands swap */
    }
    if (c0 == 2) { *err = (st0.mant >> 62) & 1 ? 7 : 1; return rx_f80_add(st1, st0, 64, 0); }   /* 785a3c7f */
    *err = (st1.mant >> 62) & 1 ? 7 : 1; return rx_f80_add(st0, st1, 64, 0);                     /* 785a3c7d: fxch, 785a3c7f */
}
static rx_f80 crt_indefinite(void) { return crt_f80_raw(0xc000000000000000ull, 0xffff); }   /* fld tbyte [0x785bb070] */
/* 785a49ef .. _87except .. 785a4a49 for error byte 7 / 1: *ok = 0 when the original must run. */
static rx_f80 crt_disp_error(int err, rx_f80 r, uint16_t cw, uint32_t desc, uint32_t regs[3], int* ok) {
    *ok = 0;
    if (err != 7 && err != 1) return r;
    if (!(cw & 1)) return CRT_WHY("_87except: invalid unmasked"), r;
    if (U32(0, 0x785b7000) == 0) return CRT_WHY("_87except: _matherr"), r;
    const double ret = rx_f80_to_f64(r, 0);                           /* fstp qword [ebp-0x76] */
    const rx_addr ptd = crt_ptd();                                    /* _set_errno(_DOMAIN): _errno */
    const rx_addr e = ptd ? ptd + 8 : CRT_ERRNO_NOMEM;
    SU32(e, 0, 0x21);
    (void)U32(0, 0x785ba2a8);                                         /* the cookie (page residency) */
    if (regs) { regs[0] = e; regs[1] = U32(0, 0x785ba2a8); regs[2] = desc; }
    *ok = 1;
    return rx_f80_from_f64(ret);                                      /* fld qword [ebp-0x76] */
}
/* the normal tail's registers (785a493d .. ): EAX = the caller's word & 0x20, ECX = the operands' fxam sign bits as 785a3c24 leaves
 * them (st(0)'s in bit 2, st(1)'s in bit 10; the +-pi/2 handler 785a4828 copies st(1)'s into bit 2), EDX = the descriptor */
static void crt_disp_regs(uint32_t regs[3], rx_f80 st1, rx_f80 st0, int half_pi, uint16_t cw, uint32_t desc) {
    if (!regs) return;
    const uint32_t c1 = st1.sign ? 4u : 0u, c0 = half_pi ? c1 : (st0.sign ? 4u : 0u);
    regs[0] = cw & 0x20u; regs[1] = c0 | (c1 << 8); regs[2] = desc;
}

/* atan2(y, x) of st(1) = y, st(0) = x; regs = EAX / ECX / EDX after it (may be NULL). */
rx_f80 crt_ci_atan2_ex(rx_f80 y, rx_f80 x, uint16_t cw, uint32_t regs[3]) {
    const int cx = crt_f80_class(x), cy = crt_f80_class(y);
    int rc; const int bits = crt_disp_bits(0x785bb0a0u, cw, &rc);
    rx_f80 r; int err = 0, half = 0;
    if (cx == 2 || cy == 2 || (cx == 3 && cy == 3)) {                /* 785a3c7f / 785a3c7d / 785a3ca7 / 785a3ce6 */
        if (cx == 2 || cy == 2) r = crt_nan_handler(y, x, &err);
        else { r = crt_indefinite(); err = 1; }
        if (U32(0, 0x785bc614) != 0) { crt_disp_regs(regs, y, x, 0, cw, 0x785bb0a0u); return r; }
        int ok; const rx_f80 o = crt_disp_error(err, r, cw, 0x785bb0a0u, regs, &ok);
        if (ok) return o;
        goto original;
    }
    if (cx == 0 && cy == 0) {                                        /* 785a47ac: fpatan(|y|, |x|), pi - r for x < 0, -r for y < 0 */
        err = 0xfe;
        r = rx_f80_transcendental(RX_FPATAN, crt_f80_abs(x), crt_f80_abs(y), 0);
        if (x.sign) r = rx_f80_sub(kPi, r, bits, rc);
        if (y.sign) r = crt_f80_neg(r);
    } else if ((cx == 1 && cy == 0) || cy == 3) {                    /* 785a4828: x zero (y a number) or y infinite: +-pi/2 */
        r = CRT_HALF_PI; half = 1;
        if (y.sign) r = crt_f80_neg(r);
    } else {                                                         /* 785a4805: y zero, or x infinite: 0 or pi, y's sign */
        r = x.sign ? kPi : rx_f80_from_int(0);
        if (y.sign) r = crt_f80_neg(r);
    }
    if (crt_disp_ok(err, r, cw)) { crt_disp_regs(regs, y, x, half, cw, 0x785bb0a0u); return r; }
original:;
    const rx_f80 in[2] = {y, x};
    rx_regs ri, ro; memset(&ri, 0, sizeof ri);
    ri.fpu_in = 2; ri.fpu[0] = y; ri.fpu[1] = x;
    crt_unported_ex(0x7859aa6a, &ri, &ro, 0, NULL); (void)in;
    if (regs) { regs[0] = ro.r[0]; regs[1] = ro.r[1]; regs[2] = ro.r[2]; }
    return ro.st[0];
}
rx_f80 crt_ci_atan2(rx_f80 y, rx_f80 x, uint16_t cw) { return crt_ci_atan2_ex(y, x, cw, NULL); }
RX_FUNCTION(0x7859aa6a, crt_ci_atan2)

/* The exact remainder of fprem (truncating quotient; the emulator completes it in one step, so 7859ac94's loop runs once):
 * x - trunc(x / y) * y, x's sign, for finite x and nonzero finite y. */
static rx_f80 crt_fprem(rx_f80 x, rx_f80 y) {
    if (x.mant == 0) return x;
    const int64_t d = (int64_t)x.exp - y.exp;
    if (d < 0) return x;
    unsigned __int128 r = x.mant % y.mant;
    for (int64_t rem = d; rem > 0;) {
        const int k = rem > 60 ? 60 : (int)rem;
        r = (r << k) % y.mant;
        rem -= k;
    }
    rx_f80 out; out.kind = 0; out.sign = x.sign;
    if (r == 0) { out.mant = 0; out.exp = 0; return out; }
    int p = 127; while (!((r >> p) & 1)) p--;
    out.mant = p >= 63 ? (uint64_t)(r >> (p - 63)) : (uint64_t)(r << (63 - p));
    out.exp = (int32_t)(p + (int64_t)y.exp - 63);
    return out;
}

/* fmod(x, y) of st(1) = x, st(0) = y: handler 7859ac94 (fxch; fprem until C2 clear; fstp st(1)), 785a3c44 (0 % y: +0), 785a3ce6
 * (the indefinite, _DOMAIN: y zero or infinite, x infinite) and the NaN handlers (above). [0x785bc618] = 1 selects the Pentium
 * FDIV-bug workaround _adj_fprem (not ported: the flag is the CPU test's, 0 on every CPU the game runs on). regs as atan2's. */
rx_f80 crt_ci_fmod_ex(rx_f80 x, rx_f80 y, uint16_t cw, uint32_t regs[3]) {
    const int cx = crt_f80_class(x), cy = crt_f80_class(y);
    int rc; (void)crt_disp_bits(0x785ba8b0u, cw, &rc);                /* fprem is exact: the precision does not enter */
    if (cx == 0 && cy == 0 && U32(0, 0x785bc618) != 1) {
        const rx_f80 r = crt_fprem(x, y);
        if (crt_disp_ok(0, r, cw)) { crt_disp_regs(regs, x, y, 0, cw, 0x785ba8b0u); return r; }
    } else if (cx == 1 && cy == 0) {
        const rx_f80 r = rx_f80_from_int(0);
        if (crt_disp_ok(0, r, cw)) { crt_disp_regs(regs, x, y, 0, cw, 0x785ba8b0u); return r; }
    } else if (!(cx == 0 && cy == 0)) {
        int err; rx_f80 r;
        if (cx == 2 || cy == 2) r = crt_nan_handler(x, y, &err);
        else { r = crt_indefinite(); err = 1; }
        if (U32(0, 0x785bc614) != 0) { crt_disp_regs(regs, x, y, 0, cw, 0x785ba8b0u); return r; }
        int ok; const rx_f80 o = crt_disp_error(err, r, cw, 0x785ba8b0u, regs, &ok);
        if (ok) return o;
    }
    rx_regs ri, ro; memset(&ri, 0, sizeof ri);
    ri.fpu_in = 2; ri.fpu[0] = x; ri.fpu[1] = y;
    crt_unported_ex(0x7859ac8a, &ri, &ro, 0, NULL);
    if (regs) { regs[0] = ro.r[0]; regs[1] = ro.r[1]; regs[2] = ro.r[2]; }
    return ro.st[0];
}
rx_f80 crt_ci_fmod(rx_f80 x, rx_f80 y, uint16_t cw) { return crt_ci_fmod_ex(x, y, cw, NULL); }
RX_FUNCTION(0x7859ac8a, crt_ci_fmod)

/* ---- pow: _CIpow 7859afc0 -> (SSE2 test as floor's) 785a5780 (st(1) = x, st(0) = y stored as doubles) -> kernel 785a5799;
 * __libm_sse2_pow 7859e9d7 (xmm0 = x, xmm1 = y, result xmm0) is the same algorithm (Intel's libm, tables at -0xbf00) with a
 * different exponent rounding step and register results.
 *
 * The kernel: log2(x) = k + log2(m) with x = 2^k m, m reduced by three table reciprocals r1 r2 r3 (indices from the top 8, 8
 * and 9 bits of the successive products; logs of the reciprocals from the hi / lo tables, accumulated in xmm6's two lanes),
 * the reduced argument's log by a polynomial in hi / lo split arithmetic (the 0xfffffffff8000000 mask splits 27 bits off),
 * then y * log2(x) = n / 128 + f split into hi / lo parts, 2^(n / 128) from the 2^(j / 128) table (7853ca80) and the
 * exponent field, 2^f by a polynomial (7853d2c0 / 7853d2d0, ln 2).
 * Ported: positive normal x (high word 0x0010 .. 0x7fee) with finite y, and either |y log2 x| in the main range or so small
 * that the result is exactly 1.0 (y == 0 among them: the 785a624c test), plus the special path 785a5ae7 where the race frames
 * reach it: x = +-0 (the clutch / speed ratio is exactly zero on the first frame that reached this path, bike_force.c
 * force_pow_quarter), the y-sign / odd-integer sign rules there (785a5d07 / 785a5cd6), the positive subnormal scaling
 * (785a5c34: 2^64 x into the main path) and x < 0 with an integer y (785a5bd6). The 624c exact-1 result also carries the
 * sign (785a6280). w-crtnan2, variant 0 (_CIpow / 785a5799): x < 0 with a non-integer y (785a5e11, code 0x1c), x inf / NaN
 * (785a5d18 / 785a5dff), the y specials 785a5bf0 / 785a5e58 / 785a5f3d (y inf / NaN / +-0) and 0 ** negative (code 0x1b, which
 * the earlier port returned without its errno), with the error support's errno / slot (crt_pow_finish). Still the original: the
 * range exits 785a5fd0 / 785a624c (overflow / underflow, codes 0x18 / 0x19) and every special branch of variant 1
 * (__libm_sse2_pow's own special code, which has no error support) except its +-0 base (libm_pow_zero). */
#include "crt_tables.h"
#define PT(a) rx_bd(crt_pow_tab[((a) - 0x78539a00u) >> 3])   /* the table double at the kernel's address a */

/* err: the ___libm_error_support code the kernel calls with (0 none; the result slot value r goes in, the final result comes out) */
typedef struct { int ok; double r; uint64_t hi; uint32_t eax, ecx, edx; uint32_t err; } crt_pow_r;

/* a kernel exit through ___libm_error_support (785a5fa0: the result slot r, the code) */
static crt_pow_r crt_pow_err(uint64_t r, uint32_t code) {
    crt_pow_r o; memset(&o, 0, sizeof o);
    o.ok = 1; o.r = rx_bd(r); o.err = code;
    return o;
}
/* 785a621c (over = 1): code 0x18 (_OVERFLOW, errno ERANGE), the slot +-inf: [7853d320] ** 2, or [7853d318] * [7853d320] with the sign
 * flag (x < 0, y odd); 785a61f9 (over = 0): code 0x19 (_UNDERFLOW, no errno), the slot [7853d328] ** 2 = +0 with the sign flag << 45. */
static crt_pow_r crt_pow_range(int over, uint32_t eflag) {
    if (over) return crt_pow_err(eflag ? rx_db(rx_bd(0xffe0000000000000ull) * rx_bd(0x7fe0000000000000ull))
                                       : rx_db(rx_bd(0x7fe0000000000000ull) * rx_bd(0x7fe0000000000000ull)), 0x18);
    return crt_pow_err(rx_db(rx_bd(0x0010000000000000ull) * rx_bd(0x0010000000000000ull)) | ((uint64_t)eflag << 45), 0x19);
}

/* 785a5eb5 (and 785a5e58 for a positive normal x, main = 1): y is inf / NaN, x is not NaN. y NaN: y + y, code 0x3ee. y = +-inf:
 * x = -1 (main: x = 1) 1.0 with code 0x1c (_DOMAIN); else +inf / +0 by y's sign and |x| >= 1. (785a5e58 with x's low dword nonzero
 * goes to 785a5eb5 with the main path's ECX / EDX in the x = -1 test: they cannot make a positive x pass it.) */
static crt_pow_r crt_pow_yspecial(uint64_t ux, uint64_t uy, int main, uint32_t e6) {
    crt_pow_r o; memset(&o, 0, sizeof o);
    if (uy & 0x000fffffffffffffull) return crt_pow_err(rx_db(rx_bd(uy) + rx_bd(uy)), 0x3ee);   /* 785a5f2e */
    if (main ? ux == 0x3ff0000000000000ull : ux == 0xbff0000000000000ull) return crt_pow_err(0x3ff0000000000000ull, 0x1c);   /* 785a5f93 */
    const int big = ((uint32_t)(ux >> 48) & 0x7ff0u) >= 0x3ff0u;
    o.ok = 1;
    o.r = ((uy >> 63) ? !big : big) ? rx_bd(0x7ff0000000000000ull) /* 785a5f27 */ : 0.0 /* 785a5f10 / 785a5f24 */;
    /* EAX = pextrw x, 3 & 0x7ff0; ECX = hi32(x) ^ 0xbff00000 (785a5bf0's) or, from 785a5e58, the main path's 0 ^ 0xbff00000; EDX =
     * lo32(x) | ECX, or from 785a5e58 the main path's pextrw edx, xmm6, 3 (x's low dword nonzero) / hi32(x) (zero) | ECX */
    o.eax = (uint32_t)(ux >> 48) & 0x7ff0u;
    o.ecx = (main ? 0u : (uint32_t)(ux >> 32)) ^ 0xbff00000u;
    o.edx = (main ? ((uint32_t)ux != 0 ? (e6 & 0xffffu) : (uint32_t)(ux >> 32)) : (uint32_t)ux) | o.ecx;
    return o;
}

/* 785a5816 .. 785a5ae6: the main computation (variant 0) / __libm_sse2_pow's copy (variant 1). xb = the bits whose top field
 * (bits 44 .. 63) gives the exponent k: x itself; |2^64 x| for the subnormal path; x for x < 0, where off's 0x80000 cancels
 * the sign bit. m = the mantissa in [1, 2) (bits 51 .. 0 with 1.0); off = 0x3fe7f (x), 0xbfe7f (x < 0) or 0x43e7f (2^64 x);
 * eflag = 785a5bc4's 0x40000 (x < 0 and y an odd integer: the result is negative), else 0 (it enters the exponent field as
 * bit 18, 785a59ef). */
static crt_pow_r crt_pow_core(uint64_t xb, uint64_t uy, double m, uint32_t off, uint32_t eflag, int variant) {
    crt_pow_r o; memset(&o, 0, sizeof o);
    const uint64_t SPLIT = 0xfffffffff8000000ull;                                 /* 7853d2e8 */
    const double y = rx_bd(uy);
    /* reduction 1 (r1 = 78539a00[i1], logs 78539e10[i1]) */
    const uint32_t i1 = (((uint32_t)(xb >> 44) & 0xffu) + 1u) & 0x1feu;
    double t7 = m * PT(0x78539a00u + 4 * i1);
    double r = PT(0x78539a00u + 4 * i1);
    double l_lo = PT(0x78539e10u + 8 * i1), l_hi = PT(0x78539e10u + 8 * i1 + 8);   /* xmm6's lanes */
    const double k = (double)(int32_t)(uint32_t)(((xb >> 44) - (uint64_t)off) >> 8);  /* cvtdq2pd (785a5816) */
    /* reduction 2 (7853a620 / 7853aa30) */
    const uint32_t i2 = (((uint32_t)(rx_db(t7) >> 38) & 0xff) + 1) & 0x1fe;
    double t3 = t7 * PT(0x7853a620u + 4 * i2);
    r = r * PT(0x7853a620u + 4 * i2);
    l_lo = l_lo + PT(0x7853aa30u + 8 * i2); l_hi = l_hi + PT(0x7853aa30u + 8 * i2 + 8);
    l_lo = l_lo + k;
    const double mh = rx_bd(rx_db(m) & SPLIT), ml = m - mh;
    double t2 = t3;
    /* reduction 3 (7853b240 / 7853ba50) */
    const uint32_t i3 = (((uint32_t)(rx_db(t3) >> 31) & 0x1ff) + 1) & 0x3fe;
    double x7 = rx_bd(0xbff7154740000000ull);                                    /* 7853d2f0: -1.4426949024200439 */
    r = r * PT(0x7853b240u + 4 * i3);
    t2 = t2 * PT(0x7853b240u + 4 * i3);
    l_lo = l_lo + PT(0x7853ba50u + 8 * i3); l_hi = l_hi + PT(0x7853ba50u + 8 * i3 + 8);
    /* the reduced argument m * r - 1 in split arithmetic */
    const double rh = rx_bd(rx_db(r) & SPLIT), rl = r - rh;
    x7 = x7 + t2;
    double x0 = rh * mh;
    const double x1 = mh * rl;
    double x3 = rh * ml;
    t2 = t2 - x0;
    double x4 = ml * rl;
    x0 = l_lo;
    t2 = t2 - x1;
    double x6 = l_lo + x7;
    t2 = t2 - x3;
    x0 = x0 - x6;
    const uint32_t e6 = (uint32_t)(rx_db(x6) >> 48);
    t2 = t2 - x4;
    x4 = x6;
    x0 = x0 + x7;
    x7 = x7 - t2;
    x6 = x6 - t2;
    const double z = x7;                                                          /* unpcklpd xmm7, xmm7 */
    const uint32_t ey = (uint32_t)(uy >> 48) & 0x7ff0;
    if (ey >= 0x7ff0) {                                                           /* 785a5e58 / 7859f081: y inf / NaN */
        if (variant == 0 && off == 0x3fe7fu) return crt_pow_yspecial(xb, uy, 1, e6);
        o.err = 0xffffu; o.edx = e6 & 0xffffu;                                    /* variant 1: the lanes' caller finishes it */
        return o;
    }
    const uint32_t s = ey - 0x3ff0 + (e6 & 0x7ff0);
    if (((0x40a0u - s) | (s - 0x3c70u)) >= 0x80000000u) {                         /* 785a624c: |y log2 x| out of the main range */
        const uint32_t e = (uint32_t)(rx_db(y * x6) >> 48) & 0x7ff0;
        const uint32_t d = 0x40a0u - e, a = e - 0x3c70u;
        if ((d | a) < 0x80000000u) x4 = y * x6;                                   /* back into the main path (785a5968) with xmm4 = y * x6 */
        else if (a < 0x80000000u) {                                                    /* 785a62a0: overflow / underflow */
            /* the sign of y log2(x): hi16(y) ^ ((hi16(x) & 0x7ff0) - 0x3ff0), bit 15 (x's exponent: the subnormal path's scaled x has
             * the same sign of log2) */
            const uint32_t lg = (off == 0x43e7fu ? 0u : ((uint32_t)(xb >> 48) & 0x7ff0u)) - 0x3ff0u;   /* [esp+4]: a subnormal x's field is 0 */
            if (variant != 0) {                                                   /* 7859f41e: the lanes' caller finishes it */
                const int under = (((uint32_t)(uy >> 48) ^ lg) & 0x8000u) != 0;
                o.err = under ? 0xfffdu : 0xfffeu; o.ecx = eflag; o.edx = lg; o.eax = under ? 0x8000u : 0;
                o.hi = rx_db((double)(int32_t)(uint32_t)((((xb >> 44) - off) >> 8) >> 32));
                return o;
            }
            return crt_pow_range((((uint32_t)(uy >> 48) ^ lg) & 0x8000u) ? 0 : 1, eflag);
        }
        else {
            o.ok = 1; o.r = rx_bd((uint64_t)(eflag | 0x1ff80u) << 45); o.hi = 0;    /* 785a6280: (ecx | 0x1ff80) << 45 */
            o.eax = a; o.ecx = eflag | 0x1ff80u; o.edx = d | a;
            return o;
        }
    }
    /* y * log2(x) = (n + f) / 128 */
    x4 = x4 - x6;
    t2 = t2 - x4;
    const double yh = rx_bd(uy & SPLIT);
    const double lh = rx_bd(rx_db(x6) & SPLIT);
    x0 = x0 - t2;
    x3 = yh * lh;
    const double ll = x6 - lh;
    const double yl = y - yh;
    x3 = x3 * 128.0;
    double x5 = yh * ll;
    x4 = lh * yl;
    const int32_t n = rx_cvt(x3);
    x6 = ll * yl;
    x5 = x5 + x4;
    x5 = x5 + x6;
    if ((int32_t)((0x1ff7fu - (uint32_t)n) | ((uint32_t)n + 0x1e1ffu)) <= 0) {      /* 785a5fd0: overflow / underflow */
        const int far_over = n > 0 && (uint32_t)n >= 0x40000u, far_under = n <= (int32_t)0xfffc0200u;
        if (variant != 0 && (far_over || far_under)) {                            /* 7859f3ae / 7859f391: the lanes' caller */
            o.err = far_under ? 0xfffdu : 0xfffeu; o.ecx = eflag; o.eax = (uint32_t)n;
            o.edx = (0x1ff7fu - (uint32_t)n) | ((uint32_t)n + 0x1e1ffu);
            o.hi = rx_db((double)(int32_t)(uint32_t)((((xb >> 44) - off) >> 8) >> 32));
            return o;
        }
        if (far_over) return crt_pow_range(1, eflag);                               /* 785a621c */
        if (far_under) return crt_pow_range(0, eflag);                              /* 785a61f9 */
        /* 785a5fe0 (n > 0) / 785a6008 (n < 0): the result within a few binades of the range ends. The main tail with the scale split:
         * the table part scaled by sp << 45 (sp = (n - 0x80) & ~0x7f, or (n & ~0x7f) + 0x3fe80), the polynomial sum rounded to the
         * bits the final scale keeps (the masks ~0 << cnt, cnt from sp), then * (ecx << 45) with ecx = eflag + 0x3ff00 / + 0x80, and
         * the correction C6 * r (C6 = 1.0 on the overflow side, 0 on the underflow side); an infinite result: code 0x18, a zero /
         * subnormal one: 0x19 (the error support), else the double. Registers: EAX = its exponent word, ECX = ecx, EDX = 0x19. */
        const int over = n > 0;
        const uint32_t ec = eflag + (over ? 0x3ff00u : 0x80u);
        const uint32_t sp = over ? (((uint32_t)n - 0x80u) & 0xffffff80u) : (((uint32_t)n & 0xffffff80u) + 0x3fe80u);
        const int32_t si = (int32_t)(sp - 0x1ff80u);
        uint32_t cnt = (uint32_t)((-(int32_t)(sp - 0x1ff80u)) >> 7) + 2u;
        cnt += cnt & 0x20u;
        const uint32_t jj = (uint32_t)n & 0x7f;
        const double xx4 = l_hi + x0;
        const double rn = (x3 + 6755399441055744.0) - 6755399441055744.0;
        double ff0 = variant == 0 ? x3 - rn : (double)n - x3;                   /* 7859f236: cvtsi2sd n - x3 */
        double pl = 0.16015105075297303 * z, ph = 9.597935033233511e-08 * z;
        const double zz2 = z * z;
        pl = pl + -0.08325619496072671 * zz2; ph = ph + -0.3465736568077919 * zz2;
        double qq = pl * zz2; qq = qq + ph; qq = qq + xx4; qq = qq * y;
        const double xx5 = x5 + qq;
        const double sc = rx_bd((uint64_t)sp << 45);
        const double el = PT(0x7853ca80u + 16 * jj) * sc, eh = PT(0x7853ca80u + 16 * jj + 8) * sc;
        double f = ff0 * (variant == 0 ? 0.0078125 : -0.0078125);
        f = f + xx5;
        const double cl = 0.0013333558146428443 * f, ch = 0.055504108664821576 * f;
        const double lf = 0.6931471805599453 * f;
        const double ff = f * f;
        const double wl = (0.009618129107628477 + cl) * ff, wh = (0.2402265069591007 + ch) * ff;
        double res = ff * el; res = res * wl;
        const double v6 = wh * el, v4 = lf * el;
        const uint64_t M = cnt > 63 ? 0 : ~0ull << cnt;
        res = res + eh; res = res + v6;
        const double H = rx_bd(rx_db(el) & M);
        res = res + v4;
        const double L = el - H;
        const double t = rx_bd(rx_db(H + res) & M);
        res = res + (H - t);
        res = res + L;
        const double S2 = rx_bd((uint64_t)ec << 45), C6 = rx_bd((uint64_t)(over ? 0x3ff0u : 0u) << 48);
        if (si > 0) { res = res + t; res = res * S2; res = res + C6 * res; }          /* 785a61af */
        else { res = res * S2; res = res + t * S2; res = res + C6 * res; }          /* 785a6161 */
        if (variant != 0) { o.ok = 1; o.r = res; o.hi = rx_db(ff); o.eax = 0xbf80u; o.ecx = ec; o.edx = cnt; return o; }   /* 7859f37c / f363: no check */
        const uint32_t ex = (uint32_t)(rx_db(res) >> 48) & 0x7ff0u;
        if (ex == 0x7ff0u || ex == 0) { crt_pow_r e = crt_pow_err(rx_db(res), ex ? 0x18 : 0x19); return e; }
        o.ok = 1; o.r = res; o.hi = 0; o.eax = ex; o.ecx = ec; o.edx = 0x19;   /* the classification's mov edx, 0x19 */
        return o;
    }
    uint32_t ecx = (eflag + (uint32_t)n) & 0xffffff80u;                           /* 785a59ef: the sign bit (0x40000) rides along */
    const uint32_t scale = ecx + 0x1ff80u;                                        /* ecx: exponent field of 2^(n >> 7) << 7 */
    const uint32_t j = (uint32_t)n & 0x7f;
    x4 = l_hi + x0;
    double f;
    if (variant == 0) {                                                           /* 785a5a01: the 1.5 * 2^52 rounding */
        const double rn = (x3 + 6755399441055744.0 /* 7853d280 */) - 6755399441055744.0;
        f = (x3 - rn) * 0.0078125;                                                /* 2^-7 */
    } else {                                                                      /* 7859ec3a: cvtsi2sd */
        f = ((double)n - x3) * -0.0078125;
    }
    /* the polynomial of the log's lower part (7853ca60 / 7853ca70) */
    double p_lo = 0.16015105075297303 /* 7853ca60 */ * z, p_hi = 9.597935033233511e-08 /* 7853ca68 */ * z;
    const double zz = z * z;
    p_lo = p_lo + -0.08325619496072671 /* 7853ca70 */ * zz;
    p_hi = p_hi + -0.3465736568077919 /* 7853ca78 */ * zz;
    double q = p_lo * zz;
    q = q + p_hi;
    q = q + x4;
    q = q * y;
    x5 = x5 + q;
    /* 2^(n / 128) * 2^f */
    const double sc = rx_bd((uint64_t)scale << 45);
    const double e_lo = PT(0x7853ca80u + 16 * j) * sc, e_hi = PT(0x7853ca80u + 16 * j + 8) * sc;
    f = f + x5;
    const double c_lo = 0.0013333558146428443 /* 7853d2c0 */ * f, c_hi = 0.055504108664821576 /* 7853d2c8 */ * f;
    const double lf = 0.6931471805599453 /* 7853d2e0 */ * f;
    const double ff = f * f;
    const double w_lo = (0.009618129107628477 /* 7853d2d0 */ + c_lo) * ff, w_hi = (0.2402265069591007 /* 7853d2d8 */ + c_hi) * ff;
    double res = ff * e_lo;
    res = res * w_lo;
    const double v6 = w_hi * e_lo;
    const double v4 = lf * e_lo;
    res = res + e_hi;
    res = res + v6;
    res = res + v4;
    res = res + e_lo;
    o.ok = 1; o.r = res; o.hi = rx_db(ff);
    o.eax = j * 16; o.ecx = scale; o.edx = variant == 0 ? 0x3f80 : 0xbf80;
    return o;
}

/* 785a5cba .. 785a5d15: x is +-0 (the y specials were handled before). 785a5cc7 tests y's sign; 785a5cd6 builds
 * (0x7ff00000 | (hi32(x) & (ecx << 13))) << 32 (+-inf) and goes through the error support (err 0x1b, whose result slot value
 * is that double); 785a5d07 returns -0.0 (785a5db9, x < 0 and y an odd integer) or +0.0 (785a5d15 fldz). */
static crt_pow_r crt_pow_zero(uint64_t ux, uint64_t uy, uint32_t eflag) {
    crt_pow_r o; memset(&o, 0, sizeof o);
    const uint32_t xhi = (uint32_t)(ux >> 32);
    const uint32_t e = xhi & (eflag << 13);
    const int yneg = ((uint32_t)(uy >> 32) & 0x80000000u) != 0;
    o.ok = 1;
    o.eax = e;
    o.ecx = eflag << 13;
    o.edx = yneg ? 0x1b : 0;                     /* 785a5cba's edx = 0 on the zero branch; 785a5cfd sets 0x1b */
    o.r = yneg ? rx_bd((uint64_t)(0x7ff00000u | e) << 32)              /* 785a5ce2: +-inf */
               : (e != 0 ? -0.0 /* 785a5db9: 7853d310 */ : 0.0);      /* 785a5d15: fldz */
    if (yneg) o.err = 0x1b;                                            /* 785a5cfd: _SING (errno ERANGE) */
    return o;
}

/* 785a5ae7: x is not a positive normal (zero, subnormal, negative, inf / NaN) or y is special. Ported branches as above;
 * *ok = 0 asks the caller for the original. */
static crt_pow_r crt_pow_special(uint64_t ux, uint64_t uy, int variant) {
    crt_pow_r o; memset(&o, 0, sizeof o);
    if (variant != 0) {
        rx_libm_pow_zero_result zero;
        if (rx_libm_pow_zero_try(ux, uy, &zero)) {
            o.ok=zero.ok; o.r=zero.r; o.hi=zero.hi;
            o.eax=zero.eax; o.ecx=zero.ecx; o.edx=zero.edx;
            return o;
        }
        /* the others: crt_spow_special (crt_libm_sse2_pow_lanes) took the y / x specials; what reaches here is x < 0 with an integer
         * y (7859ee00) or the subnormal base (7859ee5e), both into the shared main path as in variant 0 */
    }
    const uint64_t MANT = 0x000fffffffffffffull, ONE = 0x3ff0000000000000ull, ABS = 0x7fffffffffffffffull;
    const uint16_t hx = (uint16_t)(ux >> 48);
    const int32_t edx0 = (int32_t)(0x7fefu - hx);
    const uint32_t ylo = (uint32_t)uy;
    const uint64_t yabs = uy & ABS;
    const uint32_t yhi = (uint32_t)(yabs >> 32);
    if (yhi >= 0x7ff00000u) {                         /* 785a5bf0: y inf / NaN */
        if (variant != 0) return o;
        const uint32_t ax = (uint32_t)(ux >> 32) & 0x7fffffffu;
        if (ax > 0x7ff00000u || (ax == 0x7ff00000u && (uint32_t)ux != 0)) return crt_pow_err(rx_db(rx_bd(ux) + rx_bd(ux)), 0x3ee);   /* 785a5dff: x + x */
        return crt_pow_yspecial(ux, uy, 0, 0);
    }
    if ((ylo | yhi) == 0) {                           /* 785a5f3d: y = +-0: 1.0; 0^0 code 0x1a, NaN^0 code 0x1d */
        if (variant != 0) return o;
        const uint32_t ax = (uint32_t)(ux >> 32) & 0x7fffffffu;
        if (((uint32_t)ux | ax) == 0) return crt_pow_err(ONE, 0x1a);
        if (ax > 0x7ff00000u || (ax == 0x7ff00000u && (uint32_t)ux != 0)) return crt_pow_err(ONE, 0x1d);
        o.ok = 1; o.r = 1.0;                          /* 785a5f82: fld via the stack */
        o.eax = ax; o.ecx = (uint32_t)ux; o.edx = 0x1d;
        return o;
    }
    uint32_t eflag = 0;                               /* ecx */
    if (edx0 < 0) {
        /* 785a5b2a: hi16(x) > 0x7fef: x's sign bit or its exponent is 0x7ff */
        const uint64_t y2 = uy | (ABS << 52);         /* 785a5b32: y | 0xfff0000000000000 */
        int32_t sh = (int32_t)(yhi >> 20) - 0x3f3;    /* 785a5b44: (hi32(|y|) >> 20) - 0x3f3, pmaxsw 0 */
        if (sh < 0) sh = 0;
        const uint64_t shifted = sh < 64 ? y2 << sh : 0;               /* psllq (counts above 63 give 0) */
        const uint32_t isint = shifted == 0 ? 0xffu : 0u;              /* 785a5b59: pcmpeqd 0 / pmovmskb */
        const uint32_t hxw = (uint32_t)hx;                             /* ecx */
        if ((hxw & 0x7fffu) >= 0x7ff0u) {                              /* 785a5d18: x inf / NaN (y finite, nonzero) */
            if (variant != 0) return o;
            if (ux & MANT) return crt_pow_err(rx_db(rx_bd(ux) + rx_bd(ux)), 0x3ee);   /* 785a5dff */
            const int yneg = (uy >> 63) != 0;
            o.eax = (uint32_t)(uy >> 48) & 0x8000u; o.edx = hxw & 0x7fffu;   /* EAX = pextrw y, 3 & 0x8000; EDX = hi16(x) & 0x7fff */
            if (!(ux >> 63)) { o.ok = 1; o.r = yneg ? 0.0 : rx_bd(0x7ff0000000000000ull); return o; }   /* 785a5de3: +inf ** y (ECX 0) */
            int odd = 0;                                                /* 785a5d56: -inf ** y: an odd integer y keeps the sign */
            o.ecx = 0x8000u;                                            /* pextrw x, 3 & 0x8000 */
            if (isint == 0xffu) {
                o.ecx = 0x3f4u;                                         /* 785a5d6e */
                const int32_t sh2 = (int32_t)(uint32_t)(yabs >> 52) - 0x3f4;
                const uint64_t v = (uint32_t)sh2 < 64 ? uy << sh2 : 0;
                odd = v != 0;
            }
            o.ok = 1;
            if (odd) o.r = yneg ? -0.0 /* 785a5db9 */ : rx_bd(0xfff0000000000000ull) /* 785a5dc0 */;
            else o.r = yneg ? 0.0 /* 785a5de0 */ : rx_bd(0x7ff0000000000000ull) /* 785a5f27 */;
            return o;
        }
        if (isint != 0xffu) {
            /* 785a5e11: x < 0 with a non-integer y is NaN, unless x is -0 (the zero path below): [7853d330] -QNaN, code 0x1c */
            if (((uint32_t)ux | ((uint32_t)(ux >> 32) & 0x7fffffffu)) != 0) {
                if (variant != 0) return o;
                return crt_pow_err(0xfff8000000000000ull, 0x1c);
            }
        } else {
            const int32_t sh2 = (int32_t)(uint32_t)(yabs >> 52) - 0x3f4;    /* 785a5b85 */
            const uint64_t v = sh2 < 64 ? uy << sh2 : 0;                   /* psllq */
            eflag = v == 0x8000000000000000ull ? 0x40000u : 0u;            /* 785a5bb3: y an odd integer */
            if ((hxw & 0x7fffu) >= 0x10u) {
                /* 785a5bd6: x < 0 with an integer y: |x| through the main path, the sign in eflag */
                const double m = rx_bd((ux & MANT) | ONE);
                return crt_pow_core(ux, uy, m, 0xbfe7fu, eflag, variant);
            }
        }
    }
    /* 785a5c34: 2^64 * x (the zero / subnormal path, 785a5cba / 785a5c68) */
    const double scaled = rx_bd(0x43f0000000000000ull) * rx_bd(ux);
    const uint32_t xlo = (uint32_t)ux, xhi = (uint32_t)(ux >> 32);
    if (xlo == 0 && (xhi & 0x7fffffffu) == 0) return crt_pow_zero(ux, uy, eflag);
    const uint64_t sb = rx_db(scaled);
    const double m = rx_bd((sb & MANT) | ONE);
    return crt_pow_core(sb & ABS, uy, m, 0x43e7fu, eflag, variant);        /* 785a5c68 -> 785a5816 */
}

/* 785a5799 (variant 0) / 7859e9d7 (variant 1). */
static crt_pow_r crt_pow_kernel(uint64_t ux, uint64_t uy, int variant) {
    crt_pow_r o; memset(&o, 0, sizeof o);
    const uint64_t MANT = 0x000fffffffffffffull, ONE = 0x3ff0000000000000ull;
    const uint32_t hx = (uint32_t)(ux >> 48);
    if (((hx - 0x10u) | (0x7fefu - hx)) >= 0x80000000u) return crt_pow_special(ux, uy, variant);   /* 785a5ae7 */
    const double m = rx_bd((ux & MANT) | ONE);                                     /* the mantissa in [1, 2) */
    return crt_pow_core(ux, uy, m, 0x3fe7fu, 0, variant);
}

/* 785a5fa0 -> ___libm_error_support 785a8fa1(&x, &y, &slot, code) for the pow codes (785a9101 / 785a8ff2 cases): 0x1a (0 ** 0) the
 * slot = 1.0 (fld1), nothing else; 0x1b _SING, errno ERANGE (34); 0x1c / 0x3ee (NaN operands, x < 0 ** non-integer, (+-1) ** inf)
 * _DOMAIN, errno EDOM (33); 0x1d (NaN ** 0) the slot = x, _DOMAIN, EDOM. The record goes to _matherr (both of the game's return 0:
 * crt_matherr_returns_zero); the slot is loaded back through the x87 (an SNaN quieted) and is the result (785a5fc8 fld qword).
 * The overflow / underflow codes 0x18 / 0x19 come from the kernel's range exits, not ported (declined before this). Returns 0 when
 * the original must run (a foreign handler). The error support's frame below the kernel's (dead stack) is not reproduced. */
/* e: _CIpow's entry ESP (its return slot; 0 unknown): the error support's record lies at ((e - 0x14) & ~15) - 0x50 (785a5780's push
 * ebp / sub 0x10 / and -16, the kernel's call, 785a5fa0's sub 0x1c, the call, push ebp; the record at ebp - 0x28). Registers after
 * the error support: EAX = &errno (0x1a: the handler address it loaded, 78541c20 or the decoded EXE handler), ECX = the record
 * address (pop ecx after the handler call; 0x1a: code - 0x1a = 0), EDX = 0xa6 (785a8fed; 0x18: 0, the switch on codes below 0x1a
 * counts EDX down). */
static int crt_pow_finish(crt_pow_r* o, uint64_t ux, rx_addr e) {
    if (!o->ok || o->err == 0) return 1;
    uint64_t r = rx_db(o->r);
    o->edx = 0xa6;
    if (o->err == 0x1a) {
        o->r = 1.0; o->ecx = 0;
#ifdef RX_ORACLE
        o->eax = U32(0, 0x785bc644) ? crt_decode_ptr(U32(0, 0x785bd7ac)) : 0x78541c20u;
#else
        o->eax = 0x78541c20u;
#endif
        return 1;
    }
    if (!crt_matherr_returns_zero()) { CRT_WHY("pow: _matherr handler"); o->ok = 0; return 0; }
    if (o->err == 0x1d) r = ux;
    if (crt_isnan64(r)) r |= 0x0008000000000000ull;
    o->ecx = e ? ((e - 0x14) & ~15u) - 0x50 : 0;
    if (o->err == 0x18) o->edx = 0;                                        /* the code's switch runs in EDX (785a9011 ..) down to 0 */
    if (o->err == 0x19) { o->eax = 0; o->r = rx_bd(r); return 1; }          /* _UNDERFLOW: no errno; EAX = _matherr's 0 */
    const rx_addr ptd = crt_ptd();
    const rx_addr err = ptd ? ptd + 8 : CRT_ERRNO_NOMEM;
    SU32(err, 0, o->err == 0x1b || o->err == 0x18 ? 0x22u : 0x21u);
    o->eax = err;
    o->r = rx_bd(r);
    return 1;
}

/* 785a5799 (cdecl: x at [esp+4], y at [esp+0xc] as doubles; result fld qword) */
double crt_pow_sse2(double x, double y) {
    crt_pow_r o = crt_pow_kernel(rx_db(x), rx_db(y), 0);
    if (o.ok && crt_pow_finish(&o, rx_db(x), 0)) return o.r;
    const uint32_t a[4] = {(uint32_t)rx_db(x), (uint32_t)(rx_db(x) >> 32), (uint32_t)rx_db(y), (uint32_t)(rx_db(y) >> 32)};
    rx_regs in, out; memset(&in, 0, sizeof in);
    crt_unported_ex(0x785a5799, &in, &out, 4, a);
    return rx_f80_to_f64(out.st[0], 0);
}

/* The kernel's own result (the oracle's pow probe): *ok = 0 when the port declines (the original then runs). regs = EAX /
 * ECX / EDX as the SSE2 code leaves them. */
double crt_pow_kernel_raw(uint64_t x, uint64_t y, uint32_t regs[3], int* ok) {
    crt_pow_r o = crt_pow_kernel(x, y, 0);
    crt_pow_finish(&o, x, 0);
    if (regs) { regs[0] = o.eax; regs[1] = o.ecx; regs[2] = o.edx; }
    *ok = o.ok;
    return o.ok ? o.r : 0.0;
}

/* 7859afc0 _CIpow (st(1) = x, st(0) = y): the SSE2 path when __sse2_available, MXCSR masks all and the caller's x87 control
 * word masks all exceptions (785a5780 stores x and y with fstp qword: the caller's rounding control), else the x87 path 7859b01d
 * (not ported). regs: EAX / ECX / EDX as the kernel leaves them (may be NULL). */
rx_f80 crt_ci_pow_ex(rx_f80 x, rx_f80 y, uint16_t cw, uint32_t mxcsr, uint32_t regs[3]) {
    if (crt_sse2_libm(cw, mxcsr)) {
        const int rc = (cw >> 10) & 3;
        const uint64_t ux = rx_db(rx_f80_to_f64(x, rc));
        crt_pow_r o = crt_pow_kernel(ux, rx_db(rx_f80_to_f64(y, rc)), 0);
        if (o.ok && crt_pow_finish(&o, ux, crt_frame_e(0))) {
            if (regs) { regs[0] = o.eax; regs[1] = o.ecx; regs[2] = o.edx; }
            return rx_f80_from_f64(o.r);
        }
    }
    rx_regs in, out; memset(&in, 0, sizeof in);
    in.fpu_in = 2; in.fpu[0] = x; in.fpu[1] = y;
    crt_unported_ex(0x7859afc0, &in, &out, 0, NULL);
    if (regs) { regs[0] = out.r[0]; regs[1] = out.r[1]; regs[2] = out.r[2]; }
    return out.st[0];
}
rx_f80 crt_ci_pow(rx_f80 x, rx_f80 y, uint16_t cw) { return crt_ci_pow_ex(x, y, cw, 0x1f80, NULL); }
RX_FUNCTION(0x7859afc0, crt_ci_pow_ex)

/* 7859e9d7 __libm_sse2_pow (xmm0 = x, xmm1 = y as doubles; result xmm0). out: xmm0's two lanes and EAX / ECX / EDX. */
/* __libm_sse2_pow's own special code 7859ed11 (x not a positive normal; no error support: the values only), w-crtnan3. Returns 0 for
 * the branches the kernel port takes (the +-0 / subnormal base 7859ee5e, x < 0 with an integer y into the main path). H = the caller's
 * xmm high lanes (H[0] xmm0, H[2] xmm2, H[3] xmm3, H[4] xmm4): the exits that load xmm0's low lane keep its high lane (x's high lane
 * >> 44 after 7859e9fe), x + x / y + y return xmm4 / xmm2 whole. Not here: the main path's y inf / NaN exit 7859f081 (its xmm0 high lane
 * is the main path's), which still runs the original. */
static int crt_spow_special(uint64_t ux, uint64_t uy, const uint64_t H[8], crt_pow_r* o) {
    const uint64_t MANT = 0x000fffffffffffffull, ABS = 0x7fffffffffffffffull, INF = 0x7ff0000000000000ull;
    memset(o, 0, sizeof *o);
    const uint64_t h0 = H[0] >> 44;                                     /* psrlq xmm0, 0x2c */
    const uint32_t xlo = (uint32_t)ux, xhi = (uint32_t)(ux >> 32), xa = xhi & 0x7fffffffu;
    const uint32_t ylo = (uint32_t)uy, yah = (uint32_t)((uy & ABS) >> 32);
    const uint32_t ysign = (uint32_t)(uy >> 48) & 0x8000u;
#define SPOW_RET(V_, H_, A_, C_, D_) do { o->ok = 1; o->r = rx_bd(V_); o->hi = (H_); o->eax = (A_); o->ecx = (C_); o->edx = (D_); return 1; } while (0)
    if (yah >= 0x7ff00000u) {                                           /* 7859ee1a: y inf / NaN */
        if (xa > 0x7ff00000u || (xa == 0x7ff00000u && xlo != 0))         /* 7859f03c: x + x (xmm4: its high lane the caller's) */
            SPOW_RET(rx_db(rx_bd(ux) + rx_bd(ux)), H[4], xa, xhi, xlo);
        if (uy & MANT) {                                                /* 7859f1aa: y + y (xmm2) */
            const uint32_t m = ((((uint32_t)uy & 0xffffffffu) & 0xffffffffu) == 0 ? 0x0fu : 0u) | (((uint32_t)(uy >> 32) & 0xfffffu) == 0 ? 0xf0u : 0u);
            SPOW_RET(rx_db(rx_bd(uy) + rx_bd(uy)), H[2], m, xhi, xlo);
        }
        const uint32_t c = xhi ^ 0xbff00000u, d = xlo | c;
        if (d == 0) SPOW_RET(0x3ff0000000000000ull, h0, ysign, c, 0);   /* 7859f1d9: (-1) ** inf = 1 */
        const uint32_t ex = (uint32_t)(ux >> 48) & 0x7ff0u;
        if (!ysign) {                                                   /* 7859f185 */
            if (ex >= 0x3ff0u) SPOW_RET(INF, h0, ex, c, d);
            SPOW_RET(0, 0, ex, c, d);
        }
        if (ex >= 0x3ff0u) SPOW_RET(0, 0, ex, c, d);                     /* 7859f17d */
        const uint32_t z = xa | xlo;                                    /* 7859f141 */
        if (z != 0) SPOW_RET(INF, h0, xlo, c, z);
        SPOW_RET(INF, h0, 0x3ff0u, c, 0);                               /* 7859f15b: 1 / 0 (the flag), +inf */
    }
    if ((ylo | yah) == 0)                                               /* 7859f1b6: y = +-0: 1.0 (no error support here) */
        SPOW_RET(0x3ff0000000000000ull, h0, xlo | xa, xlo, xa);
    const uint16_t hx = (uint16_t)(ux >> 48);
    if ((int32_t)(0x7fefu - hx) >= 0) return 0;                          /* 7859ee59: the +-0 / subnormal base (kernel port) */
    /* 7859ed54: the integer test of y (pmovmskb over both lanes: the high lane from the caller's xmm2 / xmm3) */
    const uint64_t y2 = uy | (ABS << 52), y2h = H[2] | (H[3] << 52);
    int32_t sh = (int32_t)(yah >> 20) - 0x3f3; if (sh < 0) sh = 0;
    const uint64_t s0 = sh < 64 ? y2 << sh : 0, s1 = sh < 64 ? y2h << sh : 0;
    const uint32_t isint = ((uint32_t)s0 == 0 ? 0x000fu : 0u) | ((uint32_t)(s0 >> 32) == 0 ? 0x00f0u : 0u)
                         | ((uint32_t)s1 == 0 ? 0x0f00u : 0u) | ((uint32_t)(s1 >> 32) == 0 ? 0xf000u : 0u);
    const uint32_t hx7 = hx & 0x7fffu;
    if (hx7 >= 0x7ff0u) {                                               /* 7859ef41: x inf / NaN */
        const uint32_t mz = ((uint32_t)ux == 0 ? 0x0fu : 0u) | ((xhi & 0xfffffu) == 0 ? 0xf0u : 0u);
        if (mz != 0xffu) SPOW_RET(rx_db(rx_bd(ux) + rx_bd(ux)), H[4], isint, mz, hx7);   /* 7859f03c */
        if (!(ux >> 63)) {                                              /* 7859f01b: +inf */
            if (!ysign) SPOW_RET(INF, h0, ysign, 0, hx7);
            SPOW_RET(0, 0, ysign, 0, hx7);
        }
        uint32_t cc = 0x8000u; int odd = 0;
        if ((isint & 0xffu) == 0xffu) {                                 /* 7859ef8b: odd or even */
            cc = 0x3f4u;
            const int32_t sh2 = (int32_t)(uint32_t)((uy & ABS) >> 52) - 0x3f4;
            odd = ((uint32_t)sh2 < 64 ? uy << sh2 : 0) != 0;
        }
        if (odd) SPOW_RET(ysign ? 0x8000000000000000ull : 0xfff0000000000000ull, h0, ysign, cc, hx7);   /* 7859efe2 / 7859efee */
        if (!ysign) SPOW_RET(INF, h0, ysign, cc, hx7);                  /* 7859effa -> 7859f19e */
        SPOW_RET(0, 0, ysign, cc, hx7);
    }
    if ((isint & 0xffu) != 0xffu) {                                     /* 7859f048: x < 0 ** non-integer */
        const uint32_t a = xlo | xa;
        if (a == 0) return 0;                                           /* -0: the zero path */
        SPOW_RET(rx_db(rx_bd(0x7ff0000000000001ull) * rx_bd(0x7ff0000000000001ull)), h0, a, 0, xa);   /* 7859f071: the NaN */
    }
    return 0;                                                           /* x < 0 ** integer: the main path */
#undef SPOW_RET
}
/* 7859f081: __libm_sse2_pow's main path (x a positive normal) with y inf / NaN. xmm0's high lane there: cvtdq2pd's (double)(int32) of
 * the high dword of ((x >> 44) - 0x3fe7f) >> 8 (0, or 16777215.0 when x < 1), and'ed with the caller's xmm5 high lane (andpd at
 * 7859eb10); xmm2 keeps the caller's high lane. Registers: ECX = 0 (7859ea53), EDX = the main path's pextrw xmm6, 3 (e6) or hi32(x). */
static void crt_spow_mainy(uint64_t ux, uint64_t uy, uint32_t e6, const uint64_t H[8], crt_pow_r* o) {
    memset(o, 0, sizeof *o);
    const uint64_t cvt = ((ux >> 44) - 0x3fe7fu) >> 8;
    const uint64_t xh = rx_db((double)(int32_t)(uint32_t)(cvt >> 32)) & H[5];
    const uint32_t xlo = (uint32_t)ux, xhi = (uint32_t)(ux >> 32);
    o->ok = 1;
    if (xlo == 0 && xhi == 0x3ff00000u) { o->r = 1.0; o->hi = xh; o->eax = 0; o->ecx = 0; o->edx = xhi; return; }   /* 7859f1d9 */
    const uint32_t edx = xlo != 0 ? e6 : xhi;
    if (uy & 0x000fffffffffffffull) {                                   /* 7859f1aa: y + y */
        const uint32_t m = ((uint32_t)uy == 0 ? 0x0fu : 0u) | (((uint32_t)(uy >> 32) & 0xfffffu) == 0 ? 0xf0u : 0u);
        o->r = rx_bd(uy) + rx_bd(uy); o->hi = H[2]; o->eax = m; o->ecx = 0; o->edx = edx; return;
    }
    const uint32_t ex = (uint32_t)(ux >> 48) & 0x7ff0u, c = 0xbff00000u, d = edx | c;
    o->ecx = c;
    if (!(uy >> 63)) { o->eax = ex; o->edx = d; if (ex >= 0x3ff0u) { o->r = rx_bd(0x7ff0000000000000ull); o->hi = xh; } else { o->r = 0.0; o->hi = 0; } return; }
    if (ex >= 0x3ff0u) { o->eax = ex; o->edx = d; o->r = 0.0; o->hi = 0; return; }   /* 7859f17d */
    o->eax = xlo; o->edx = (xhi & 0x7fffffffu) | xlo; o->r = rx_bd(0x7ff0000000000000ull); o->hi = xh;   /* 7859f141 -> 7859f19e */
}
/* __libm_sse2_pow with the caller's xmm high lanes (H may be NULL: zeros) */
double crt_libm_sse2_pow_lanes(double x, double y, const uint64_t H[8], uint64_t xmm0_out[2], uint32_t regs[3]) {
    static const uint64_t Z[8] = {0};
    const uint64_t ux = rx_db(x);
    const uint32_t hx = (uint32_t)(ux >> 48);
    crt_pow_r so;
    if (((hx - 0x10u) | (0x7fefu - hx)) >= 0x80000000u && crt_spow_special(ux, rx_db(y), H ? H : Z, &so)) {
        if (xmm0_out) { xmm0_out[0] = rx_db(so.r); xmm0_out[1] = so.hi; }
        if (regs) { regs[0] = so.eax; regs[1] = so.ecx; regs[2] = so.edx; }
        return so.r;
    }
    {   /* the main path (a positive normal x, or the special path's x < 0 with an integer y / subnormal x) and its markers */
        const crt_pow_r m = crt_pow_kernel(ux, rx_db(y), 1);
        if (!m.ok && (m.err == 0xfffeu || m.err == 0xfffdu)) {
            /* 7859f3ae (far overflow): xmm1 = 0xffef << 48, xmm0's low lane = it (its high lane kept: the main path's cvtdq2pd value &
             * the caller's xmm5 high lane, 7859eb10), with the sign flag psllq / psrlq 1 on both lanes, then * xmm1; EAX = 0xffef.
             * 7859f391 (far underflow): [78531418] ** 2 | ecx << 45 into the low lane. */
            const uint64_t prev = m.hi & (H ? H : Z)[5];
            memset(&so, 0, sizeof so);
            so.ecx = m.ecx; so.edx = m.edx;
            if (m.err == 0xfffeu) {
                uint64_t lo = 0xffef000000000000ull, hi = prev;
                if (m.ecx) { lo = (lo << 1) >> 1; hi = (hi << 1) >> 1; }
                so.r = rx_bd(lo) * rx_bd(0xffef000000000000ull); so.hi = hi; so.eax = 0xffefu;
            } else {
                so.r = rx_bd(rx_db(rx_bd(0x0010000000000000ull) * rx_bd(0x0010000000000000ull)) | ((uint64_t)m.ecx << 45)); so.hi = prev; so.eax = m.eax;
            }
            if (xmm0_out) { xmm0_out[0] = rx_db(so.r); xmm0_out[1] = so.hi; }
            if (regs) { regs[0] = so.eax; regs[1] = so.ecx; regs[2] = so.edx; }
            return so.r;
        }
        if (!m.ok && m.err == 0xffffu) {
            crt_spow_mainy(ux, rx_db(y), m.edx, H ? H : Z, &so);
            if (xmm0_out) { xmm0_out[0] = rx_db(so.r); xmm0_out[1] = so.hi; }
            if (regs) { regs[0] = so.eax; regs[1] = so.ecx; regs[2] = so.edx; }
            return so.r;
        }
    }
    return crt_libm_sse2_pow_ex(x, y, xmm0_out, regs);
}
double crt_libm_sse2_pow_ex(double x, double y, uint64_t xmm0_out[2], uint32_t regs[3]) {
    const crt_pow_r o = crt_pow_kernel(rx_db(x), rx_db(y), 1);
    if (o.ok) {
        if (xmm0_out) { xmm0_out[0] = rx_db(o.r); xmm0_out[1] = o.hi; }
        if (regs) { regs[0] = o.eax; regs[1] = o.ecx; regs[2] = o.edx; }
        return o.r;
    }
    rx_regs in, out; memset(&in, 0, sizeof in);
    const uint64_t ux = rx_db(x), uy = rx_db(y);
    memcpy(in.xmm[0].v, &ux, 8); memcpy(in.xmm[1].v, &uy, 8);
    crt_unported_ex(0x7859e9d7, &in, &out, 0, NULL);
    uint64_t lanes[2]; memcpy(lanes, out.xmm[0].v, 16);
    if (xmm0_out) { xmm0_out[0] = lanes[0]; xmm0_out[1] = lanes[1]; }
    if (regs) { regs[0] = out.r[0]; regs[1] = out.r[1]; regs[2] = out.r[2]; }
    return rx_bd(lanes[0]);
}
double crt_libm_sse2_pow(double x, double y) { return crt_libm_sse2_pow_lanes(x, y, NULL, NULL, NULL); }
RX_FUNCTION(0x7859e9d7, crt_libm_sse2_pow_ex)

/* ---- 7859fb0c __libm_sse2_sin (xmm0 = x -> xmm0): x = n * pi/32 + r (n from x * 32/pi rounded, pi/32 in three parts
 * 7853 2230 / 2220 / 2238), sin(x) = sin(j pi/32) cos(r) + cos(j pi/32) sin(r) with j = n mod 64 from the 64-entry table
 * 785319e0 (each: two hi / lo pairs) and odd / even polynomials in r (785321e0 .. 78532218), in split arithmetic.
 * Tiny |x| (7859fc5b): x * (1 - 2^-53) at one exponent, else x itself. Huge |x| (the x87 sin through the CRT's `sin`
 * 7859b210) and inf / NaN (x * -0.0) run the original (*ok = 0). In the main path the output's lane 1 is the caller's xmm4
 * lane 1 (the result is built in xmm4 with scalar operations; the adapter sets it). */
#define ST(i) rx_bd(crt_sin_tab[i])
double crt_libm_sse2_sin_ex(double x, uint32_t regs[3], int* ok) {
    const uint16_t ax = (uint16_t)(((uint16_t)(rx_db(x) >> 48) & 0x7fff) - 0x3030);
    *ok = 1;
    if (ax > 0x10c5) {
        if ((int16_t)ax > 0x10c5) { *ok = 0; return 0.0; }                 /* 7859fc87: huge, inf, NaN */
        if ((uint16_t)(ax >> 4) == 0xcfd) return x * 0.9999999999999999 /* 78532260 */;
        return x;   /* 7859fc6e computes (x * 2^55 - x) * 2^-55 in xmm3 (for the flags) and returns x itself */
    }
    double x1 = 10.185916357881302 /* 78532240: 32/pi */ * x;
    const int32_t n = rx_cvt(x1);
    x1 = (x1 + 6755399441055744.0) - 6755399441055744.0;                  /* 78532248 */
    double x3 = 0.09817477042088285 /* 78532230: pi/32 hi */ * x1;
    const uint32_t off = ((uint32_t)(n + 0x1c7600) & 0x3f) << 5;
    const int t = (int)(off >> 3);                                         /* the entry: 4 doubles */
    const double c2 = rx_bd(0x3d90b4611a600000ull);                        /* 78532220 / 78532228: pi/32 mid */
    double p2lo = c2 * x1, p2hi = c2 * x1;
    double x0 = x - x3;
    double x1lo = x1 * 1.2639164054974691e-22 /* 78532238 */;
    double x4 = x - x3;
    double x7 = ST(t + 1);
    double x0lo = x0, x0hi = x0;
    x3 = x4;
    x4 = x4 - p2lo;
    double x5lo = 2.7557319223985893e-06 /* 78532210 */ * x0lo, x5hi = 2.48015873015873e-05 /* 78532218 */ * x0hi;
    x0lo = x0lo - p2lo; x0hi = x0hi - p2hi;
    double x6lo = 0.008333333333333333 /* 785321f0 */, x6hi = 0.041666666666666664 /* 785321f8 */;
    x7 = x7 * x4;
    x3 = x3 - x4;
    x5lo = x5lo * x0lo; x5hi = x5hi * x0hi;
    x0lo = x0lo * x0lo; x0hi = x0hi * x0hi;
    x3 = x3 - p2lo;
    double x2lo = ST(t), x2hi = ST(t + 1);
    x1lo = x1lo - x3;
    x3 = ST(t + 3);
    x2lo = x2lo + x3;
    x7 = x7 - x2lo;
    x2lo = x2lo * x4;
    x6lo = x6lo * x0lo; x6hi = x6hi * x0hi;
    x3 = x3 * x4;
    x2lo = x2lo * x0lo; x2hi = x2hi * x0hi;
    x0lo = x0lo * x0lo; x0hi = x0hi * x0hi;
    x5lo = x5lo + -0.0001984126984126984 /* 78532200 */; x5hi = x5hi + -0.001388888888888889 /* 78532208 */;
    x4 = x4 * ST(t);
    x6lo = x6lo + -0.16666666666666666 /* 785321e0 */; x6hi = x6hi + -0.5 /* 785321e8 */;
    x5lo = x5lo * x0lo; x5hi = x5hi * x0hi;
    x0lo = x3;
    x3 = x3 + ST(t + 1);
    x1lo = x1lo * x7;
    x7 = x4;
    x4 = x4 + x3;
    x6lo = x6lo + x5lo; x6hi = x6hi + x5hi;
    x5lo = ST(t + 1);
    x5lo = x5lo - x3;
    x3 = x3 - x4;
    x1lo = x1lo + ST(t + 2);
    x6lo = x6lo * x2lo; x6hi = x6hi * x2hi;
    x5lo = x5lo + x0lo;
    x3 = x3 + x7;
    x1lo = x1lo + x5lo;
    x1lo = x1lo + x3;
    x1lo = x1lo + x6lo;
    x1lo = x1lo + x6hi;
    x4 = x4 + x1lo;
    if (regs) { regs[0] = 0x785319e0u + off; regs[2] = off; }
    return x4;
}
/* 7859fc87: abs(x) above the main range. inf / NaN (exponent 0x7ff): x * -0.0 (78532268; mulsd: a NaN quieted, inf -> the default
 * NaN 0xfff8000000000000), EAX = 0x7ff0, xmm0's lane 1 kept. Finite: the x87 sin: an aligned frame F = (E - 0x20) & ~15 (E = the entry
 * ESP: [F] = x, [F+8] = E), 7859b210 (msvcr90's sin: the SSE2 test's stmxcsr / fnstcw at [F-8] / [F-0xc], 785a64b8's range test sends
 * it on to the x87 code 7859b29f), fld x (785a3db5), fsin under the caller's word (0x027f: no fldcw), the error-check flag
 * [0x785bc614] (0: ECX = 0x785ba930, EDX = 0x1e, then 785a3e1b with the caller's word 0x027f: no check), fstp qword [F], movq xmm0
 * (lane 1 = 0). EAX = (hi(x) & 0x7ff00000) | cw (785a3db5's EAX, then mov ax, [esp]), EDX = [F-8] = (F & 0xffff0000) | cw (the
 * pushed EDX = F with fnstcw over its low word). abs(x) >= 2^63: fsin sets C2 and leaves x (the hardware; the emulator models it since
 * w-crtnan4), 7859b2e2 reduces it with fprem1 by the tbyte pi 2^62 at 7853608a (completed: the exact remainder) and fsin runs again. A word
 * other than 0x027f declines (no thread calls this function with another word).
 * e: the entry ESP when the caller has the original's stack (the dead frame is written), 0 for none. regs: EAX / ECX / EDX in / out. */
double crt_libm_sse2_sin_big(double x, rx_addr e, uint16_t cw, uint32_t regs[3], int* lane1_zero, int* ok) {
    const uint64_t u = rx_db(x);
    *ok = 1; *lane1_zero = 0;
    if (((uint32_t)(u >> 48) & 0x7ff0u) == 0x7ff0u) {
        if (regs) regs[0] = 0x7ff0;
        return rx_bd(crt_isnan64(u) ? u | 0x0008000000000000ull : 0xfff8000000000000ull);
    }
    if (cw != 0x027f || U32(0, CRT_SSE2) == 0) { *ok = 0; return CRT_WHY("sin: control word"), 0.0; }
    rx_f80 a = rx_f80_from_f64(x);
    if (((u >> 52) & 0x7ff) >= 0x43e)   /* abs(x) >= 2^63: fsin sets C2 and leaves x; 7859b2e2: fld tbyte pi 2^62 (7853608a), fxch, fprem1 until C2 clears, fstp st(1) */
        a = rx_f80_rem(a, crt_f80_raw(0xc90fdaa22168c235ull, 0x403e), 1);   /* pi * 2^62 */
    const double r = rx_f80_to_f64(rx_f80_transcendental(RX_FSIN, a, rx_f80_from_int(0), 0), 0);
    const rx_addr F = (e - 0x20) & ~15u;
    if (e) {
        SU32(F, 0, (uint32_t)rx_db(r)); SU32(F, 4, (uint32_t)(rx_db(r) >> 32)); SU32(F, 8, e);
        SU32(F, -4, 0x7859fcae);
        SU32(F, -8, (F & 0xffff0000u) | cw);
        SU16(F, -0xc, cw);
    }
    if (regs) {
        regs[0] = ((uint32_t)(u >> 32) & 0x7ff00000u) | cw;
        if (U32(0, 0x785bc614) == 0) regs[1] = 0x785ba930u;
        regs[2] = (F & 0xffff0000u) | cw;
    }
    *lane1_zero = 1;
    return r;
}
double crt_libm_sse2_sin(double x) {
    int ok; const double r = crt_libm_sse2_sin_ex(x, NULL, &ok);
    if (ok) return r;
    if (((uint16_t)(((uint16_t)(rx_db(x) >> 48) & 0x7fff) - 0x3030)) > 0x10c5 && (int16_t)(((uint16_t)(rx_db(x) >> 48) & 0x7fff) - 0x3030) > 0x10c5) {
        int z, ok2; const double b = crt_libm_sse2_sin_big(x, crt_frame_e(0), CRT_CW_MAIN, NULL, &z, &ok2);
        if (ok2) return b;
    }
    rx_regs in, out; memset(&in, 0, sizeof in);
    const uint64_t ux = rx_db(x); memcpy(in.xmm[0].v, &ux, 8);
    crt_unported_ex(0x7859fb0c, &in, &out, 0, NULL);
    uint64_t lo; memcpy(&lo, out.xmm[0].v, 8);
    return rx_bd(lo);
}
RX_FUNCTION(0x7859fb0c, crt_libm_sse2_sin_ex)

/* __libm_sse2_exp 785a4dae (_CIexp 7859ac2c with SSE2 enabled: fstp the x87 operand to xmm0, call it, fld the result). Main path
 * (|x| in [2^-55, ~1000) and a result within the normal range): n = round(x * 64 / ln 2) (the 1.5 * 2^52 trick, 7853 8340 / 8320),
 * r = x - n * ln2/64 (two constants, 8350 / 8360), exp(r) - 1 from two polynomial pairs (8370 / 8380) and the table entry 2^(j/64)
 * (j = n mod 64, 8390 ..) scaled by 2^(n div 64) (the exponent bits built in the integer unit): result = q * S + S with S the scale
 * and q the polynomial sum. Tiny |x| (785a501e) returns x + 1.0. Everything else (huge or non-finite x, a denormal or overflowing
 * result: the x87 scaling tail 785a4eb3, the error handler) sets *ok = 0 and the caller runs the original. */
typedef struct { int32_t k; uint32_t j; double q; } crt_exp_state;   /* the scale decline's state for the x87 tail */
static double crt_exp_core(double x, int* ok, crt_exp_state* st) {
    *ok = 1;
    const uint64_t xb = rx_db(x);
    uint32_t e = (uint32_t)(xb >> 48) & 0x7fffu;
    const uint32_t chk = (0x408fu - e) | (e - 0x3c90u);
    if (chk >= 0x80000000u) {
        if ((uint32_t)(xb >> 32 & 0x7fffffffu) >= 0x40900000u) { *ok = 0; return 0.0; }
        return x + 1.0 /* 78538790 */;
    }
    const double t = (rx_bd(0x40571547652b82feull) * x) + rx_bd(0x4338000000000000ull);
    const double n = t - rx_bd(0x4338000000000000ull);
    const double hi = rx_bd(0x3f862e42fefa0000ull) * n, lo = rx_bd(0x3d1cf79abc9e3b3aull) * n;
    const double r = (x - hi) - lo;
    const uint32_t tl = (uint32_t)rx_db(t);
    const uint32_t j = tl & 0x3fu;
    const int32_t k = (int32_t)tl >> 6;
    const double p0 = rx_bd(0x3fa5555726eced80ull) + rx_bd(0x3f811074b1d108e5ull) * r;
    const double p1 = rx_bd(0x3fdfffffffffe17bull) + rx_bd(0x3fc555555566a45aull) * r;
    const double r2 = r * r;
    const double r4 = r2 * r2;
    double s1 = r + rx_bd(crt_exp_tab[2 * j]);
    const double q0 = r4 * p0, q1 = r2 * p1;
    s1 = s1 + q0;
    const double q = q1 + s1;
    if ((uint32_t)(k + 0x37e) > 0x77cu) { *ok = 0; st->k = k; st->j = j; st->q = q; return 0.0; }
    const uint64_t scale = ((uint64_t)(tl & 0xffffffc0u) + 0xffc0u) << 46;
    const double S = rx_bd(crt_exp_tab[2 * j + 1] | scale);
    return q * S + S;
}
double crt_exp_sse2(double x, int* ok) { crt_exp_state st; return crt_exp_core(x, ok, &st); }

/* _CIexp (7859ac2c) for the game's callers: the main path above; anything else (|x| >= 1000 or tiny results, NaN, inf) is the
 * original's in the oracle and the host's exp elsewhere (the spring's argument is -k * dt with a finite k and dt <= 0.1: not reached). */
/* 785a501e -> 785a4fb3: abs(x) >= 1024 (hi32 >= 0x40900000). NaN: code 0x3ea (___libm_error_support: the slot = x through the x87, an
 * SNaN quieted; _DOMAIN, errno EDOM); +inf: +inf (785387b0), -inf: +0 (785387b8), both without an error; finite x > 0: 785387c0 squared
 * (+inf) with code 0xe (_OVERFLOW, errno ERANGE), x < 0: 785387c8 squared (+0) with code 0xf (_UNDERFLOW, no errno). The record goes
 * to _matherr (crt_matherr_returns_zero). *ok = 0 outside these cases (the x87 scaling tail 785a4eb3 for results near the range ends,
 * not ported) or with a foreign handler. */
static double crt_exp_special(double x, int* ok) {
    const uint64_t u = rx_db(x);
    const uint32_t hi = (uint32_t)(u >> 32), ahi = hi & 0x7fffffffu;
    *ok = 0;
    if (ahi < 0x40900000u) return 0.0;
    uint32_t code; double slot;
    if (ahi >= 0x7ff00000u) {
        if (ahi == 0x7ff00000u && (uint32_t)u == 0) { *ok = 1; return hi == 0x7ff00000u ? rx_bd(0x7ff0000000000000ull) : 0.0; }
        code = 0x3ea; slot = rx_bd(u | 0x0008000000000000ull);
    } else if (hi < 0x80000000u) { code = 0xe; slot = rx_bd(0x7ff0000000000000ull); }      /* [785387c0] ** 2 */
    else { code = 0xf; slot = 0.0; }                                                        /* [785387c8] ** 2 */
    if (!crt_matherr_returns_zero()) return CRT_WHY("exp: _matherr handler"), 0.0;
    if (code != 0xf) { const rx_addr ptd = crt_ptd(); SU32(ptd ? ptd + 8 : CRT_ERRNO_NOMEM, 0, code == 0xe ? 0x22u : 0x21u); }
    *ok = 1;
    return slot;
}
/* 785a4eb3: the scale 2^k out of the double's normal range (k + 0x37e > 0x77c): the x87 under the caller's word with 64-bit precision
 * (or 0x300), k split as a = k >> 1 (sar), b = k - a: ((q * T1) + T1) * 2^b with T1 = the table's 2^(j/64) mantissa (pandn the
 * exponent mask 785382f0) | (a + 0x3ff) << 52, rounded to a double by fstp qword (the caller's rounding); an infinite result goes to the
 * error support with code 0xe (_OVERFLOW, errno ERANGE), a zero / subnormal one with 0xf (_UNDERFLOW, no errno), the slot unchanged. */
static double crt_exp_tail(const crt_exp_state* st, uint16_t cw, int* ok) {
    const int32_t k = st->k, a = k >> 1, b = k - a;
    const int rc = (cw >> 10) & 3;
    const double t1 = rx_bd((crt_exp_tab[2 * st->j + 1] & 0x000fffffffffffffull) | ((uint64_t)(uint32_t)(a + 0x3ff) << 52));
    const double t2 = rx_bd((uint64_t)(uint32_t)(b + 0x3ff) << 52);
    rx_f80 v = rx_f80_mul(rx_f80_from_f64(st->q), rx_f80_from_f64(t1), 64, rc);
    v = rx_f80_add(v, rx_f80_from_f64(t1), 64, rc);
    v = rx_f80_mul(v, rx_f80_from_f64(t2), 64, rc);
    const double r = rx_f80_to_f64(v, rc);
    const uint32_t ex = (uint32_t)(rx_db(r) >> 48) & 0x7ff0u;
    *ok = 1;
    if (ex != 0x7ff0u && ex != 0) return r;
    if (!crt_matherr_returns_zero()) { *ok = 0; return CRT_WHY("exp: _matherr handler"), 0.0; }
    if (ex == 0x7ff0u) { const rx_addr ptd = crt_ptd(); SU32(ptd ? ptd + 8 : CRT_ERRNO_NOMEM, 0, 0x22); }
    return r;
}
double crt_exp_x87(double x) {
    int ok; crt_exp_state st = {0, 0, 0.0};
    const double r = crt_exp_core(x, &ok, &st);
    if (ok) return r;
    const double sp = crt_exp_special(x, &ok);
    if (ok) return sp;
    if (((uint32_t)(rx_db(x) >> 32) & 0x7fffffffu) < 0x40900000u) {   /* the decline was the scale test: the x87 tail */
        const double t = crt_exp_tail(&st, CRT_CW_MAIN, &ok);
        if (ok) return t;
    }
#ifdef RX_ORACLE
    return rx_f80_to_f64(crt_unported_x87(0x7859ac2c, 1, (rx_f80[]){rx_f80_from_f64(x)}), 0);
#else
    return exp(x);
#endif
}
