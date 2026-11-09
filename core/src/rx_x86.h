/* rx_x86.h -- the few places where plain C float math differs from what MXReflex.exe's SSE / x87 code computes. Everything else
 * (add / sub / mul / div / sqrt in single precision, compiled with -ffp-contract=off -fno-fast-math) is already exact. */
#pragma once
#include <math.h>
#include <stdint.h>
#include "rx_mem.h"

/* MINSS / MAXSS / MINPS / MAXPS: the second operand unless the comparison holds (a NaN in either gives the second). */
static inline float rx_minss(float a, float b) { return a < b ? a : b; }
static inline float rx_maxss(float a, float b) { return a > b ? a : b; }

/* CVTTSS2SI / CVTTSD2SI: truncate; NaN or out of range -> 0x80000000 ("integer indefinite"). */
static inline int32_t rx_cvtt(double x) { return (x != x || x >= 2147483648.0 || x < -2147483648.0) ? INT32_MIN : (int32_t)x; }
/* CVTSS2SI / CVTSD2SI with MXCSR round-to-nearest-even (the game never changes MXCSR rounding). */
static inline int32_t rx_cvt(double x) { double r = nearbyint(x); return (r != r || r >= 2147483648.0 || r < -2147483648.0) ? INT32_MIN : (int32_t)r; }

/* SSE arithmetic with the x86 NaN rule, for places where NaNs can arise and be stored (plain C differs on arm64: generated NaNs are
 * 0x7fc00000 instead of x86's "real indefinite" 0xffc00000; with two NaN operands x86 returns the FIRST, quieted; LLVM may fold
 * x * -1.0f into a negation that flips a NaN's sign). Non-NaN results are identical to plain C, so use plain C elsewhere. */
static inline float rx_nan_rule(float r, float a, float b) {
    if (r == r) return r;
    if (a != a) return rx_bf(rx_fb(a) | 0x400000u);
    if (b != b) return rx_bf(rx_fb(b) | 0x400000u);
    return rx_bf(0xffc00000u);
}
#if defined(__x86_64__) && defined(__SSE2__) && !defined(RX_X86_PORTABLE)
/* An x86-64 host (the Windows build) has these rules in hardware: ADDSS / SUBSS / MULSS / DIVSS / SQRTSS with the game's operand
 * order (the first operand is the destination) are the instructions the C below emulates, result and NaN alike. Inline asm pins
 * the order (C's a * b may be commuted, which changes the NaN picked when both are NaNs) and keeps the compiler from folding
 * x * -1.0f; it also drops the emulation's stack round trip and NaN tests (w-winbench). RX_X86_PORTABLE selects the C path. */
#define RX_X86_SSE_ASM 1
static inline float rx_adds(float a, float b) { __asm__("addss %1, %0" : "+x"(a) : "xm"(b)); return a; }
static inline float rx_subs(float a, float b) { __asm__("subss %1, %0" : "+x"(a) : "xm"(b)); return a; }
static inline float rx_muls(float a, float b) { __asm__("mulss %1, %0" : "+x"(a) : "xm"(b)); return a; }
static inline float rx_divs(float a, float b) { __asm__("divss %1, %0" : "+x"(a) : "xm"(b)); return a; }
static inline float rx_sqrts(float a) { float r = a; __asm__("sqrtss %1, %0" : "+x"(r) : "xm"(a)); return r; }
#else
#define RX_X86_SSE_ASM 0
static inline float rx_adds(float a, float b) { return rx_nan_rule(a + b, a, b); }
static inline float rx_subs(float a, float b) { return rx_nan_rule(a - b, a, b); }
static inline float rx_muls(float a, float b) { volatile float vb = b; return rx_nan_rule(a * vb, a, b); }
static inline float rx_divs(float a, float b) { return rx_nan_rule(a / b, a, b); }
static inline float rx_sqrts(float a) { float r = sqrtf(a); return r == r ? r : (a != a ? rx_bf(rx_fb(a) | 0x400000u) : rx_bf(0xffc00000u)); }
#endif

/* RCPSS / RSQRTSS: the hardware approximations, bit-exact (tables measured from the original's execution, as the emulator). */
uint32_t rx_rcp_bits(uint32_t x);
uint32_t rx_rsqrt_bits(uint32_t x);
static inline float rx_rcp(float x)   { return rx_bf(rx_rcp_bits(rx_fb(x))); }
static inline float rx_rsqrt(float x) { return rx_bf(rx_rsqrt_bits(rx_fb(x))); }

/* Packed helpers (lane-wise, in lane order). */
static inline rx_v4 rx_v4_add(rx_v4 a, rx_v4 b) { rx_v4 r; for (int i = 0; i < 4; i++) r.v[i] = a.v[i] + b.v[i]; return r; }
static inline rx_v4 rx_v4_sub(rx_v4 a, rx_v4 b) { rx_v4 r; for (int i = 0; i < 4; i++) r.v[i] = a.v[i] - b.v[i]; return r; }
static inline rx_v4 rx_v4_mul(rx_v4 a, rx_v4 b) { rx_v4 r; for (int i = 0; i < 4; i++) r.v[i] = a.v[i] * b.v[i]; return r; }
static inline rx_v4 rx_v4_div(rx_v4 a, rx_v4 b) { rx_v4 r; for (int i = 0; i < 4; i++) r.v[i] = a.v[i] / b.v[i]; return r; }
static inline rx_v4 rx_v4_splat(float x) { rx_v4 r = {{x, x, x, x}}; return r; }
