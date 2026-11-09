/* rx_x87.h -- exact x87 extended-precision arithmetic for game code that the original computes on the x87 stack (where plain
 * float math is not exact: double constants, extended intermediates, transcendentals). Bit-for-bit equal to the emulator's
 * reflex_f80 (numbers verified against the x87 model _re/trace/x87.py; NaNs carry their significand in mant with the hardware's
 * propagation rules, f80.h).
 * bits = precision control: 24 (the game's control word 0x007f), 53 or 64. rc = rounding control: 0 nearest (the game's).
 * Typical: rx_f80 a = rx_f80_from_f32(x); a = rx_f80_mul(a, rx_f80_from_f64(k), 24, 0); float r = rx_f80_to_f32(a, 0); */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { uint8_t kind, sign; uint64_t mant; int32_t exp; } rx_f80;   /* kind 0 number, 1 inf, 2 nan */
enum rx_f80_op { RX_FSIN, RX_FCOS, RX_FPTAN, RX_FPATAN, RX_FYL2X, RX_FYL2XP1, RX_F2XM1 };
rx_f80 rx_f80_from_f32(float x);
rx_f80 rx_f80_from_f64(double x);
rx_f80 rx_f80_from_int(int64_t v);
float  rx_f80_to_f32(rx_f80 a, int rc);
double rx_f80_to_f64(rx_f80 a, int rc);
rx_f80 rx_f80_add(rx_f80 a, rx_f80 b, int bits, int rc);
rx_f80 rx_f80_sub(rx_f80 a, rx_f80 b, int bits, int rc);
rx_f80 rx_f80_mul(rx_f80 a, rx_f80 b, int bits, int rc);
rx_f80 rx_f80_div(rx_f80 a, rx_f80 b, int bits, int rc);
rx_f80 rx_f80_sqrt(rx_f80 a, int bits, int rc);
int    rx_f80_cmp(rx_f80 a, rx_f80 b);                       /* -1, 0, 1, or 2 unordered (fcom / fucomi) */
rx_f80 rx_f80_round_int(rx_f80 a, int rc);                   /* frndint */
rx_f80 rx_f80_rem(rx_f80 a, rx_f80 b, int nearest);           /* fprem (0) / fprem1 (1), completed: the exact remainder a - q b */
int64_t rx_f80_to_int(rx_f80 a, int rc);                     /* fistp (indefinite 0x8000000000000000 when out of range / NaN) */
/* a = st(0), b = st(1) as the instruction sees them: FPATAN gives atan(st1 / st0) = atan2(b, a), FYL2X b * log2(a); FSIN / FCOS / F2XM1
 * use a only; FPTAN returns tan (the 1.0 it pushes is the caller's). Results are at 64-bit precision (as the x87 delivers them). */
rx_f80 rx_f80_transcendental(enum rx_f80_op op, rx_f80 a, rx_f80 b, int rc);
/* Builds the library's lazily computed constants (pi, ln2, log2 e: function-local C++ statics with heap storage). rx_reset.c calls it
 * before it records the pristine statics, so a restore never separates such an object from its guard. */
void rx_x87_warm(void);
#ifdef __cplusplus
}
#endif
