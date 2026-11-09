/* phys_crt.c -- the exe's own runtime helpers the physics frame runs (statically linked CRT pieces at 0x008fxxxx) and the thin
 * libm wrappers at 0x0042c1d0..0x0042c23e. The x87 ones take the value on the x87 stack (rx_f80) and the precision control of
 * the calling thread (24 / 53 / 64 bits: the main thread runs with control word 0x027f = 53 bits). */
#include "phys_crt.h"
#include "crt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define X87_RC   0    /* round to nearest (every thread of the game) */

/* 008fa106 _ftol2: x truncated toward zero, as a 64-bit integer (EDX:EAX; x popped). The CRT's method: n = fistp (nearest),
 * then the sign of the single-precision difference x - n moves n one step toward zero: the "+ 0x7fffffff" carry is set exactly
 * when the difference has the wrong sign and is not zero. n = 0 and n = INT64_MIN (the indefinite) are returned as they are. */
int64_t phys_ftol2(rx_f80 x, int precision) {
    const uint32_t x_bits = rx_fb(rx_f80_to_f32(x, X87_RC));   /* fst dword: only its sign is used */
    const int64_t n = rx_f80_to_int(x, X87_RC);
    if ((uint32_t)n == 0 && ((uint32_t)((uint64_t)n >> 32) & 0x7fffffffu) == 0) return n;
    const rx_f80 diff = rx_f80_sub(x, rx_f80_from_int(n), precision, X87_RC);
    const uint32_t d = rx_fb(rx_f80_to_f32(diff, X87_RC));
    if ((int32_t)x_bits < 0)                                    /* x < 0: n + 1 when x - n > 0 */
        return n + ((uint64_t)(d ^ 0x80000000u) + 0x7fffffffu > 0xffffffffu ? 1 : 0);
    return n - ((uint64_t)d + 0x7fffffffu > 0xffffffffu ? 1 : 0);   /* x >= 0: n - 1 when x - n < 0 */
}
RX_FUNCTION(0x008fa106, phys_ftol2)

/* 008fa0d0 _ftol2_sse: with SSE2 (the CRT's flag at 0x00dcf760) x is stored as a double and truncated by CVTTSD2SI (EAX);
 * without it, _ftol2. */
int32_t phys_ftol2_sse(rx_f80 x, int precision) {
    if (U32(0, 0x00dcf760) == 0) return (int32_t)phys_ftol2(x, precision);
    return rx_cvtt(rx_f80_to_f64(x, X87_RC));
}
RX_FUNCTION(0x008fa0d0, phys_ftol2_sse)

/* 008f9efe __security_check_cookie (ECX = the frame's cookie): returns when it equals the global cookie at 0x009b45e4, else
 * jumps to ___report_gsfailure (008fa43c, which terminates the process). */
void phys_security_check_cookie(uint32_t cookie) {
    if (cookie != U32(0, 0x009b45e4)) {
        const rx_addr sp = rx_sp();
        rx_set_sp(sp - 4u);                       /* the jump to ___report_gsfailure keeps the caller's return address on top */
        SU32(rx_sp(), 0, 0);
        rx_report_gsfailure();
        rx_set_sp(sp);
    }
}
RX_FUNCTION(0x008f9efe, phys_security_check_cookie)

/* ---- the /GS failure report ------------------------------------------------------------------------------------------------- */

/* The register file ___report_gsfailure records. The general registers are the core's model of the caller's (fx_reg: the emulated CPU in
 * the oracle, a register file of its own in the product). The segment selectors are those of a 32-bit Windows process (WoW64): CS 0x23,
 * SS / DS / ES / GS 0x2b, FS 0x53. */
uint32_t fx_reg(int r);
enum { GS_EAX, GS_ECX, GS_EDX, GS_EBX, GS_ESP, GS_EBP, GS_ESI, GS_EDI };
#define GS_CS 0x23u
#define GS_SS 0x2bu
#define GS_DS 0x2bu
#define GS_ES 0x2bu
#define GS_FS 0x53u
#define GS_GS 0x2bu

/* EFLAGS after `sub esp, 0x328` (the last flag-setting instruction before the pushfd): reserved bit 1 and IF set (a user-mode thread), DF /
 * TF clear, CF / PF / AF / ZF / SF / OF of the subtraction esp - 0x328. */
static uint32_t gs_eflags_after_sub(uint32_t a, uint32_t b) {
    const uint32_t r = a - b;
    uint32_t f = 0x202u;
    if (a < b) f |= 0x1u;                                              /* CF */
    if ((__builtin_popcount(r & 0xffu) & 1) == 0) f |= 0x4u;           /* PF */
    if (((a ^ b ^ r) & 0x10u) != 0) f |= 0x10u;                        /* AF */
    if (r == 0) f |= 0x40u;                                            /* ZF */
    if (r & 0x80000000u) f |= 0x80u;                                   /* SF */
    if (((a ^ b) & (a ^ r) & 0x80000000u) != 0) f |= 0x800u;           /* OF */
    return f;
}

/* 008fa43c ___report_gsfailure (cdecl, no arguments; entered by `jmp` from __security_check_cookie, or called when a /GS check fails): fills the
 * EXCEPTION_RECORD (0x009e1a88: code 0xc0000409 STATUS_STACK_BUFFER_OVERRUN, flags 1 non-continuable, address = the failing function's
 * return address 0x009e1a94) and CONTEXT (0x009e1ae0: ContextFlags 0x10001, the registers and selectors) the CRT hands to the unhandled
 * exception filter, asks IsDebuggerPresent (0x009e1ad8), and ends the process: SetUnhandledExceptionFilter(0), UnhandledExceptionFilter(
 * {0x009e1a88, 0x009e1ae0}), TerminateProcess(GetCurrentProcess(), 0xc0000409). The report here is the same record written to the game
 * memory, then a fatal diagnostic naming the address and the process ends (abort): the core runs inside the host process and the host
 * is the one that must not continue after a corrupted stack. Frame: mov edi, edi; push ebp; mov ebp, esp; sub esp, 0x328 (the cookie
 * and its complement at ebp - 0x328 / - 0x324). */
void rx_report_gsfailure(void) {
    const rx_addr entry = rx_sp(), ebp = entry - 4u, esp = ebp - 0x328u;
    SU32(entry, -4, fx_reg(GS_EBP));                                   /* push ebp */
    SU32(0, 0x009e1b90, fx_reg(GS_EAX));
    SU32(0, 0x009e1b8c, fx_reg(GS_ECX));
    SU32(0, 0x009e1b88, fx_reg(GS_EDX));
    SU32(0, 0x009e1b84, fx_reg(GS_EBX));
    SU32(0, 0x009e1b80, fx_reg(GS_ESI));
    SU32(0, 0x009e1b7c, fx_reg(GS_EDI));
    SU16(0, 0x009e1ba8, GS_SS);
    SU16(0, 0x009e1b9c, GS_CS);
    SU16(0, 0x009e1b78, GS_DS);
    SU16(0, 0x009e1b74, GS_ES);
    SU16(0, 0x009e1b70, GS_FS);
    SU16(0, 0x009e1b6c, GS_GS);
    SU32(0, 0x009e1ba0, gs_eflags_after_sub(ebp, 0x328u));             /* pushfd; pop [..] */
    SU32(0, 0x009e1b94, U32(ebp, 0));                                  /* the caller's EBP */
    SU32(0, 0x009e1b98, U32(ebp, 4));                                  /* the return address */
    SU32(0, 0x009e1ba4, ebp + 8u);                                     /* the caller's ESP */
    SU32(0, 0x009e1ae0, 0x10001u);
    SU32(0, 0x009e1a94, U32(0, 0x009e1b98));
    SU32(0, 0x009e1a88, 0xc0000409u);
    SU32(0, 0x009e1a8c, 1);
    SU32(esp, 0, U32(0, 0x009b45e4));                                  /* the local copies of the cookie and its complement */
    SU32(esp, 4, U32(0, 0x009b45e8));
    SU32(0, 0x009e1ad8, 0);                                            /* IsDebuggerPresent: no debugger */
    fprintf(stderr, "rxcore: fatal: stack security cookie check failed (___report_gsfailure 008fa43c), return address %08x, status c0000409\n",
            U32(0, 0x009e1b98));
    abort();
}
RX_FUNCTION(0x008fa43c, rx_report_gsfailure)

/* 008faa92 _RTC_StackFailure (cdecl return address, variable name; the run-time stack check of /RTCs builds): when the RTC error mode
 * (0x009b45f8, initially 1) is not -1, the message "Stack around the variable '<name>' was corrupted." is built in a 1024-byte buffer (a
 * name that is empty or too long for it gives "Stack corrupted near unknown variable") and handed to failwithmessage (008fa85e, kind 2):
 * with no debugger attached and no RTC error callback installed (008fa7a0 / 008fa79a) that ends in DebugBreak, which kills the process.
 * Here: the same message, reported as a fatal diagnostic, then the process ends (abort). With the mode -1 the check is disabled and the
 * function returns after its own cookie check. */
void rx_rtc_stack_failure(rx_addr return_address, rx_addr name) {
    if (U32(0, 0x009b45f8) == 0xffffffffu) return;
    char message[1100];
    const char* unknown = "Stack corrupted near unknown variable";
    int composed = 0;
    if (name != 0 && U8(name, 0) != 0) {
        uint32_t len = 0;
        while (U8(name, len) != 0) len++;
        if (len + 1u + 0x2cu <= 0x400u) {                              /* strlen + 1 + 0x2c > 0x400 gives the unknown-variable text */
            snprintf(message, sizeof message, "Stack around the variable '");
            size_t at = strlen(message);
            for (uint32_t i = 0; i < len; i++) message[at++] = (char)U8(name, i);
            message[at] = 0;
            strcat(message, "' was corrupted.");
            composed = 1;
        }
    }
    fprintf(stderr, "rxcore: fatal: Run-Time Check Failure #2 - %s (_RTC_StackFailure 008faa92, return address %08x)\n",
            composed ? message : unknown, return_address);
    abort();
}
RX_FUNCTION(0x008faa92, rx_rtc_stack_failure)

/* 0042c230 SqrtF (cdecl float): SQRTSS into xmm0 lane 0. */
float phys_sqrtf(float x) { return phys_sqrt(x); }
RX_FUNCTION(0x0042c230, phys_sqrtf)

/* 0042c1d0 CeilF (xmm0 lane 0): MSVCR90 ceil of x widened to a double (crt_ceil); the result is on the x87 stack (exactly a
 * float: the ceiling of a float is one). */
float phys_ceilf(float x) { return (float)crt_ceil((double)x); }
RX_FUNCTION(0x0042c1d0, phys_ceilf)

/* 0042c1f0 PowF (xmm0 = base, xmm1 = exponent): both widened to doubles and passed to MSVCR90's __libm_sse2_pow
 * (crt_libm_sse2_pow), the result narrowed back. */
float phys_powf(float base, float exponent) { return (float)crt_libm_sse2_pow((double)base, (double)exponent); }
RX_FUNCTION(0x0042c1f0, phys_powf)
