/* phys_vec.h -- the vector helpers of the physics core (phys_*.c): 4-lane vectors as the original's SSE registers hold them
 * (rx_v4, lane 0 = x), with x86's exact results.
 *
 * Exactness: IEEE results equal the host's. Only NaN results differ by ISA: x86 returns the first operand if it is a NaN
 * (quieted), else the second, else the "real indefinite" 0xffc00000 (arm64: signaling-NaN priority and 0x7fc00000). The helpers
 * here apply x86's rule (src/rx_x86.h rx_adds ...), so vector code and the scalar helpers (phys_sqrt / phys_div / ...) are exact
 * for every input. Plain C float expressions are used where no NaN can be produced (finite inputs, no 0/0, inf-inf, sqrt(<0)).
 * Also: LLVM folds x * -1.0f into a negation (that flips a NaN's sign; MULSS keeps it): write phys_mul(x, -1.0f) there. */
#pragma once
#include "rx_game.h"

/* ---- scalars with x86's NaN rule: rx_adds / rx_subs / rx_muls / rx_divs / rx_sqrts (src/rx_x86.h); short names here */
static inline float phys_add(float a, float b) { return rx_adds(a, b); }
static inline float phys_sub(float a, float b) { return rx_subs(a, b); }
static inline float phys_mul(float a, float b) { return rx_muls(a, b); }
static inline float phys_div(float a, float b) { return rx_divs(a, b); }
static inline float phys_sqrt(float a) { return rx_sqrts(a); }
/* MINSS / MAXSS: a if the comparison holds, else b (NaNs and equal zeros give b) */
static inline float phys_min(float a, float b) { return a < b ? a : b; }
static inline float phys_max(float a, float b) { return a > b ? a : b; }
/* bitwise select: mask all ones -> a, zero -> b (what andps / andnps / orps compute) */
static inline float phys_selbits(uint32_t mask, float a, float b) { return rx_bf((mask & rx_fb(a)) | (~mask & rx_fb(b))); }

/* ---- 4-lane vectors */
static inline rx_v4 v4(float x, float y, float z, float w) { rx_v4 r = {{x, y, z, w}}; return r; }
static inline rx_v4 v4_bits(uint32_t x, uint32_t y, uint32_t z, uint32_t w) { return v4(rx_bf(x), rx_bf(y), rx_bf(z), rx_bf(w)); }
static inline uint32_t v4_lb(rx_v4 a, int i) { return rx_fb(a.v[i]); }
static inline rx_v4 v4_splat(float x) { return v4(x, x, x, x); }
static inline rx_v4 v4_zero(void) { return v4_bits(0, 0, 0, 0); }
static inline rx_v4 v4_lane(rx_v4 a, int i) { return v4_splat(a.v[i]); }                /* shufps a, a, i * 0x55 */
static inline rx_v4 v4_perm(rx_v4 a, int i0, int i1, int i2, int i3) { return v4(a.v[i0], a.v[i1], a.v[i2], a.v[i3]); }
static inline rx_v4 v4_shuf(rx_v4 a, rx_v4 b, int imm) {                                 /* shufps a, b, imm */
    return v4(a.v[imm & 3], a.v[(imm >> 2) & 3], b.v[(imm >> 4) & 3], b.v[(imm >> 6) & 3]);
}
static inline rx_v4 v4_movss(rx_v4 a, rx_v4 b) { a.v[0] = b.v[0]; return a; }             /* lane 0 from b */
#if RX_X86_SSE_ASM
/* x86-64 host: the packed instructions themselves (lane-wise the same as the scalar rule above; see src/rx_x86.h) */
#include <emmintrin.h>
#define PHYS_V4_PS(name, insn) static inline rx_v4 name(rx_v4 a, rx_v4 b) { __m128 x = _mm_loadu_ps(a.v); const __m128 y = _mm_loadu_ps(b.v); \
    __asm__(insn " %1, %0" : "+x"(x) : "x"(y)); _mm_storeu_ps(a.v, x); return a; }
PHYS_V4_PS(v4_add, "addps")
PHYS_V4_PS(v4_sub, "subps")
PHYS_V4_PS(v4_mul, "mulps")
PHYS_V4_PS(v4_div, "divps")
#undef PHYS_V4_PS
static inline rx_v4 v4_sqrt(rx_v4 a) { __m128 x = _mm_loadu_ps(a.v); __asm__("sqrtps %0, %0" : "+x"(x)); _mm_storeu_ps(a.v, x); return a; }
#else
static inline rx_v4 v4_add(rx_v4 a, rx_v4 b) { for (int i = 0; i < 4; i++) a.v[i] = phys_add(a.v[i], b.v[i]); return a; }
static inline rx_v4 v4_sub(rx_v4 a, rx_v4 b) { for (int i = 0; i < 4; i++) a.v[i] = phys_sub(a.v[i], b.v[i]); return a; }
static inline rx_v4 v4_mul(rx_v4 a, rx_v4 b) { for (int i = 0; i < 4; i++) a.v[i] = phys_mul(a.v[i], b.v[i]); return a; }
static inline rx_v4 v4_div(rx_v4 a, rx_v4 b) { for (int i = 0; i < 4; i++) a.v[i] = phys_div(a.v[i], b.v[i]); return a; }
static inline rx_v4 v4_sqrt(rx_v4 a) { for (int i = 0; i < 4; i++) a.v[i] = phys_sqrt(a.v[i]); return a; }
#endif
static inline rx_v4 v4_min(rx_v4 a, rx_v4 b) { for (int i = 0; i < 4; i++) a.v[i] = phys_min(a.v[i], b.v[i]); return a; }
static inline rx_v4 v4_max(rx_v4 a, rx_v4 b) { for (int i = 0; i < 4; i++) a.v[i] = phys_max(a.v[i], b.v[i]); return a; }
static inline rx_v4 v4_scale(rx_v4 a, float s) { return v4_mul(a, v4_splat(s)); }       /* a * s (a first) */
static inline rx_v4 v4_and(rx_v4 a, rx_v4 b) { for (int i = 0; i < 4; i++) a.v[i] = rx_bf(v4_lb(a, i) & v4_lb(b, i)); return a; }
static inline rx_v4 v4_andn(rx_v4 a, rx_v4 b) { for (int i = 0; i < 4; i++) a.v[i] = rx_bf(~v4_lb(a, i) & v4_lb(b, i)); return a; }
static inline rx_v4 v4_or(rx_v4 a, rx_v4 b) { for (int i = 0; i < 4; i++) a.v[i] = rx_bf(v4_lb(a, i) | v4_lb(b, i)); return a; }
static inline rx_v4 v4_xor(rx_v4 a, rx_v4 b) { for (int i = 0; i < 4; i++) a.v[i] = rx_bf(v4_lb(a, i) ^ v4_lb(b, i)); return a; }
/* masks (all ones / zero per lane) as cmpps computes them: unordered is false for lt / le / eq, true for neq / nlt / nle */
static inline rx_v4 v4_mask(int m0, int m1, int m2, int m3) { return v4_bits(m0 ? ~0u : 0u, m1 ? ~0u : 0u, m2 ? ~0u : 0u, m3 ? ~0u : 0u); }
static inline rx_v4 v4_cmplt(rx_v4 a, rx_v4 b) { return v4_mask(a.v[0] < b.v[0], a.v[1] < b.v[1], a.v[2] < b.v[2], a.v[3] < b.v[3]); }
static inline rx_v4 v4_cmple(rx_v4 a, rx_v4 b) { return v4_mask(a.v[0] <= b.v[0], a.v[1] <= b.v[1], a.v[2] <= b.v[2], a.v[3] <= b.v[3]); }
static inline rx_v4 v4_cmpeq(rx_v4 a, rx_v4 b) { return v4_mask(a.v[0] == b.v[0], a.v[1] == b.v[1], a.v[2] == b.v[2], a.v[3] == b.v[3]); }
static inline rx_v4 v4_sel(rx_v4 mask, rx_v4 a, rx_v4 b) { return v4_or(v4_and(mask, a), v4_andn(mask, b)); }
/* the xyz mask (0092db80): w cleared */
static inline rx_v4 v4_xyz(rx_v4 a) { a.v[3] = rx_bf(0); return a; }

/* The exe's horizontal sums of a product P = a * b:
 *  dot3: (P0 + P1) + P2                      (shufps 0x99 / 0x55 + addss)
 *  dot4: (P1 + P3) + (P0 + P2)               (shufps 0x40 / 0x30 + addps, lane 2) */
static inline float phys_hsum3(rx_v4 p) { return phys_add(phys_add(p.v[0], p.v[1]), p.v[2]); }
static inline float phys_dot3(rx_v4 a, rx_v4 b) { return phys_hsum3(v4_mul(a, b)); }
static inline float phys_hsum4(rx_v4 p) { return phys_add(phys_add(p.v[1], p.v[3]), phys_add(p.v[0], p.v[2])); }
static inline float phys_dot4(rx_v4 a, rx_v4 b) { return phys_hsum4(v4_mul(a, b)); }

/* game-memory vec4s */
static inline rx_v4 v4_ld(rx_addr a) { return rx_ld4(a); }
static inline void v4_st(rx_addr a, rx_v4 x) { rx_st4(a, x); }
/* movss xmm, m32: lane 0 loaded, lanes 1..3 zero */
static inline rx_v4 v4_ld_ss(rx_addr a) { return v4(F32(a, 0), rx_bf(0), rx_bf(0), rx_bf(0)); }

/* register block for rx_call_original_ex, all zero */
static inline rx_regs phys_regs(void) { rx_regs r; memset(&r, 0, sizeof r); return r; }
/* A direct call of a core function as if the original had pushed `arg_bytes` of stack arguments and its return address: the
 * callee's frame (and its alignment) lands where the original's does. */
#define PHYS_CALL_SP(arg_bytes, call) do { rx_addr sp_ = rx_sp(); rx_set_sp(sp_ - (arg_bytes) - 4); call; rx_set_sp(sp_); } while (0)

/* ---- quaternions (x, y, z, w lanes) and 3-vectors as the exe's SSE code computes them */
/* a * b (Hamilton product): with rev = b.wzyx, r2 = rev.zwxy, r3 = r2.wzyx:
 *   ((r3 * a.z) * (-1, 1, 1, -1) + (a.y * r2) * (1, 1, -1, -1)) + ((a.w * b) + (a.x * rev) * (1, -1, 1, -1))
 * (sign vectors 0093a210 / 0093a200 / 0093a1f0). z_splat_first: a few call sites multiply a.z * r3 (NaN operand order). */
/* Optional endpoint observation of the same kernel's intermediates. The
 * observer never recomputes a quaternion product or changes its operands. */
typedef struct { rx_v4 raw_x, raw_y, r2, zy, value; } phys_quat_observation;
static inline rx_v4 phys_quat_mul_observed(rx_v4 a, rx_v4 b, int z_splat_first, phys_quat_observation* observation) {
    const rx_v4 rev = v4_perm(b, 3, 2, 1, 0), r2 = v4_perm(rev, 1, 0, 3, 2), r3 = v4_perm(r2, 3, 2, 1, 0);
    const rx_v4 raw_x = v4_mul(v4_lane(a, 0), rev), raw_y = v4_mul(v4_lane(a, 1), r2);
    const rx_v4 tx = v4_mul(raw_x, v4(1.0f, -1.0f, 1.0f, -1.0f));
    const rx_v4 ty = v4_mul(raw_y, v4(1.0f, 1.0f, -1.0f, -1.0f));
    const rx_v4 tz = v4_mul(z_splat_first ? v4_mul(v4_lane(a, 2), r3) : v4_mul(r3, v4_lane(a, 2)), v4(-1.0f, 1.0f, 1.0f, -1.0f));
    const rx_v4 tw = v4_mul(v4_lane(a, 3), b);
    const rx_v4 zy = v4_add(tz, ty), value = v4_add(zy, v4_add(tw, tx));
    if (observation) { observation->raw_x = raw_x; observation->raw_y = raw_y; observation->r2 = r2; observation->zy = zy; observation->value = value; }
    return value;
}
static inline rx_v4 phys_quat_mul_ex(rx_v4 a, rx_v4 b, int z_splat_first) { return phys_quat_mul_observed(a, b, z_splat_first, NULL); }
static inline rx_v4 phys_quat_mul(rx_v4 a, rx_v4 b) { return phys_quat_mul_ex(a, b, 0); }
/* conjugate: q * (-1, -1, -1, 1) (0093a220) */
static inline rx_v4 phys_quat_conj(rx_v4 q) { return v4_mul(q, v4(-1.0f, -1.0f, -1.0f, 1.0f)); }
/* the exe's cross product (shufps 0xd2 / 0xc9 pairs): (a.z b.y - a.y b.z, a.x b.z - a.z b.x, a.y b.x - a.x b.y), w = 0
 * (masked with 0092db80) -- that is b x a */
static inline rx_v4 phys_cross_ba(rx_v4 a, rx_v4 b) {
    const rx_v4 a1 = v4_perm(a, 2, 0, 1, 3), a2 = v4_perm(a1, 2, 0, 1, 3);
    const rx_v4 b1 = v4_perm(b, 1, 2, 0, 3), b2 = v4_perm(b1, 1, 2, 0, 3);
    return v4_xyz(v4_sub(v4_mul(a1, b1), v4_mul(a2, b2)));
}
/* the exe's guarded normalization of a 3-vector (w lane included in the division): n = sqrt(dot3(v, v)); v / n where n != 0,
 * 0 where n == 0, and the NaN vec4 (0092dc30) when |v|^2 is infinite (0092dc20) */
static inline rx_v4 phys_normalize3(rx_v4 v) {
    const float n2 = phys_dot3(v, v), n = phys_sqrt(n2);
    const int finite = n2 != rx_bf(0x7f800000u), nonzero = n != 0.0f;
    rx_v4 r;
    for (int i = 0; i < 4; i++)
        r.v[i] = finite ? (nonzero ? phys_div(v.v[i], n) : rx_bf(0)) : rx_bf(0x7fc00000u);
    return r;
}
