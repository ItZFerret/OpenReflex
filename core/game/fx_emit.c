/* fx_emit.c -- particle emission: ParticleEmitter::vf0 00695ab0 -> 00695780 (rate, the game's own LCG at 0x9cb860 / count
 * 0x9cb864, the "Emitting particles" sprintf, the spawn loops), the two template builders 006944c0 (16 rands up front) and
 * 00694ba0 (rands as needed, spherical start velocity), the emitter matrix lerp 006941a0, the player-distance gate 00695b00 /
 * 00695650, VehicleParticleEffectEmitter vf3 0069b060. notes/fx.md.
 *
 * Emitter state S (the effect's +0x10 block): +0x10 rate override, +0x14 asset holder (+0x44 asset data: +0x10 types, +0x1c
 * frames, +0x20 16 atlas indices; +0x4c busy), +0x18 type index, +0x1c active, +0x20 matrix pending, +0x30 matrix (4x4), +0x60
 * position, +0x70 the spawned-particle owner block, +0x7c running, +0x90 velocity source*, +0x94 user, +0x98 fractional spawn
 * count, +0x9c builder (0: 006944c0, else 00694ba0), +0xa0 acceleration offset*, +0xa4 4 distance bounds, +0xb4 4 range flags.
 * Template (0x80 bytes, the particle record of fx_particle.c): written by the builders except +0x0c / +0x1c / +0x2c and
 * +0x5c..+0x6c, +0x7c (+0x78 for 2-lane sizes): those words keep what lies on the stack (known-differences.md entry 1). */
#include "fx.h"
#include "fx_util.h"
#include "world_node.h"
#include <stdio.h>
#include <stdlib.h>

#define FX_K15 3.0517578125e-05   /* 0x977a90 (float) / 0x9776a0 (double): 2^-15 */

static inline uint32_t lcg(uint32_t s) { return s * 0x41c64e6du + 0x3039u; }
/* (s >> 16) & 0x7fff as a float times 2^-15 (cvtsi2ss / cvtss2sd / mulsd / cvtsd2ss: exact) */
static inline float rf(uint32_t s) { return (float)((double)(float)((s >> 16) & 0x7fffu) * FX_K15); }

/* a thiscall virtual without stack arguments, at the current ESP (return address `ret`) */
static uint32_t vcall0(rx_addr obj, uint32_t off, rx_addr ret) {
    const rx_addr fn = U32(U32(obj, 0), off);
    uint32_t (*f)(rx_addr) = RX_FN(fn, uint32_t (*)(rx_addr));
    if (!f) return rx_call_original(fn, RX_THISCALL, obj, 0, 0);
    const rx_addr s = fx_push(0, 0, ret);
    const uint32_t r = f(obj);
    fx_pop(s);
    return r;
}
/* ---- 00672fe0 (ESI = a, EDI = b; no stack arguments; xmm0 = the result): the angle between two vectors: both normalized when
 * their squared length is above 0 (a left as it is, unwritten, otherwise: the frame's words at E + 0x10 / 0x14 / 0x18 are read
 * as they lie), c = dot(a, b); acos by the rational approximation (the same inlined sequence as anim_math.h's
 * anim_acos_approx): |c| (Float_Abs_Thunk) at or above 0.7071 [0x9ddfc8] works on sqrt(|1 - c^2|) and takes the complement
 * from pi/2; c below 0 (or unordered) mirrors; the angle is pi/2 - that. Frame: push ebp; and esp, -16; sub esp, 0x20; E = A -
 * 0x20: +8 / +0xc the squared lengths / |c| and c, +0x10 the normalized a. The calls at ESP = E: SqrtF 0042c230 after `push
 * ecx; fstp [esp]` (arg, return address, SqrtF's push ebp), Float_Abs_Thunk 00401040 (return address, its push ecx, the call of
 * Float_Abs and its push ebp). ECX is the caller's (the model's). ---- */
static void angle_fabs_residue(rx_addr E, rx_addr ret) {
    SU32(E, -4u, ret); SU32(E, -8u, fx_reg(FX_ECX)); SU32(E, -0xcu, 0x00401046u); SU32(E, -0x10u, fx_reg(FX_EBP));
}
static float angle_sqrt(rx_addr E, uint32_t arg_bits, rx_addr ret) {
    SU32(E, -4u, arg_bits); SU32(E, -8u, ret); SU32(E, -0xcu, fx_reg(FX_EBP));
    return phys_sqrtf(rx_bf(arg_bits));
}
float rx_fx_vec_angle(rx_addr a, rx_addr b) {
    fx_frame f; fx_frame_enter0(&f, 0x20, 16);
    const rx_addr E = f.E;
    const float zero = F32(0, 0x0092e12c), thr = F32(0, 0x009ddfc8);
    {
        const float x = F32(a, 0), y = F32(a, 4), z = F32(a, 8);
        SF32(E, 0xc, x);
        const float l2 = rx_adds(rx_adds(rx_muls(x, x), rx_muls(y, y)), rx_muls(z, z));
        SF32(E, 8, l2);
        if (l2 > zero) {
            const float inv = rx_divs(1.0f /* 0x931f24 */, angle_sqrt(E, fx_x87_bits(rx_fb(l2)), 0x00673033));
            SF32(E, 0x10, rx_muls(x, inv)); SF32(E, 0x14, rx_muls(inv, y)); SF32(E, 0x18, rx_muls(inv, z));
        }
    }
    float nx, ny, nz;
    {
        const float x = F32(b, 0), y = F32(b, 4), z = F32(b, 8);
        SF32(E, 8, x);
        const float l2 = rx_adds(rx_adds(rx_muls(x, x), rx_muls(y, y)), rx_muls(z, z));
        SF32(E, 0xc, l2);
        if (l2 > zero) {
            const float inv = rx_divs(1.0f, angle_sqrt(E, fx_x87_bits(rx_fb(l2)), 0x006730c0));
            ny = rx_muls(inv, y); nx = rx_muls(x, inv); nz = rx_muls(inv, z);
        } else { nz = F32(E, 0x18); ny = F32(E, 0x14); nx = F32(E, 0x10); }
    }
    const float c = rx_adds(rx_adds(rx_muls(ny, F32(E, 0x14)), rx_muls(nx, F32(E, 0x10))), rx_muls(nz, F32(E, 0x18)));
    SF32(E, 0xc, c);
    angle_fabs_residue(E, 0x00673123);
    const float ad = rx_bf(rx_fb(c) & 0x7fffffffu);
    SF32(E, 8, ad);
    const int high = ad >= thr;
    float u = ad;
    if (high) {
        const float t = (float)((double)c * (double)c);
        angle_fabs_residue(E, 0x0067316d);
        const float w = rx_bf(rx_fb((float)(1.0 /* 0x977638 */ - (double)t)) & 0x7fffffffu);
        u = angle_sqrt(E, rx_fb(w), 0x00673178);
    }
    const float s = (float)((double)u * (double)u);
    const float q = (float)((double)s * (double)s);
    const float p1 = (float)(0.5648093223571777 /* 0x9777b0 */ - (double)s * 0.04992850869894028 /* 0x9777a8 */);
    const float p2 = (float)(1.0 - (double)s * 1.4430354833602905 /* 0x9777a0 */);
    const float r = (float)(1.0 / ((double)p1 * (double)q + (double)p2));
    const float p3 = (float)(0.3993034064769745 /* 0x977798 */ - (double)s * 0.0193784236907959 /* 0x977790 */);
    const float p4 = (float)(1.0 - (double)s * 1.2763687372207642 /* 0x977788 */);
    const float n = (float)(((double)p3 * (double)q + (double)p4) * (double)u);
    float t = (float)((double)r * (double)n);
    if (high) t = (float)(1.5707963705062866 /* 0x977510 */ - (double)t);
    if (!(c >= zero)) t = rx_subs(-0.0f /* 0x9774a8 */, t);
    const float res = (float)(1.5707963705062866 - (double)t);
    fx_frame_leave(&f);
    return res;
}
RX_FUNCTION(0x00672fe0, rx_fx_vec_angle)
/* the angle between a (ESI) and b (EDI) with ECX as the call site has it (the registers stay as the call leaves them) */
static float angle_between(rx_addr a, rx_addr b, uint32_t ecx) {
    fx_set_reg(FX_ESI, a); fx_set_reg(FX_EDI, b); fx_set_reg(FX_ECX, ecx);
    return rx_fx_vec_angle(a, b);
}

/* 0065ca30 (EAX = timing): timing +0x40 in xmm0 (inline; the call leaves its return address) */
static float timing_time(rx_addr timing, rx_addr ret) { SU32(rx_sp(), -4, ret); return F32(timing, 0x40); }
/* SqrtF 0042c230 of a value passed through fld / fstp into the pushed slot (cdecl; add esp, 4 after) */
static float sqrtf_call(float x, rx_addr ret) {
    const rx_addr sp = rx_sp();
    SU32(sp, -4, fx_x87_bits(rx_fb(x))); SU32(sp, -8, ret);
    return phys_sqrtf(rx_bf(fx_x87_bits(rx_fb(x))));
}

/* ---- 006941a0 (ESI = out matrix, EDI = b; stdcall a, t; RET 8): out = lerp(a, b, t) row by row, the three rotation rows
 * renormalized when their length is positive (w = 0), the translation row lerped (w = 1). Frame: push ebp; and esp, -16; sub
 * esp, 0x1c; push ebx: E = A - 0x20: +8 1 - t, +0xc |row|^2, +0x10 the row. */
void rx_fx_matrix_lerp(rx_addr a, float t, rx_addr out, rx_addr b) {
    fx_frame f; fx_frame_enter(&f, 0x1c, 16, FX_EBX);
    fx_set_reg(FX_EBX, a);
    const rx_addr E = f.E;
    const float u = 1.0f - t;
    for (int r = 0; r < 3; r++) {
        const uint32_t o = 0x10u * (uint32_t)r;
        float x = rx_adds(rx_muls(F32(a, o), u), rx_muls(F32(b, o), t));
        float y = rx_adds(rx_muls(F32(a, o + 4), u), rx_muls(F32(b, o + 4), t));
        float z = rx_adds(rx_muls(F32(a, o + 8), u), rx_muls(F32(b, o + 8), t));
        const float l2 = rx_adds(rx_adds(rx_muls(z, z), rx_muls(y, y)), rx_muls(x, x));
        if (r == 0) SF32(E, 8, u);
        SF32(E, 0x10, x); SF32(E, 0x14, y); SF32(E, 0x18, z); SF32(E, 0xc, l2);
        if (l2 > 0.0f) {
            const float inv = 1.0f / sqrtf_call(l2, r == 0 ? 0x0069424e : r == 1 ? 0x00694325 : 0x006943fd);
            x = rx_muls(inv, F32(E, 0x10)); y = rx_muls(inv, F32(E, 0x14)); z = rx_muls(inv, F32(E, 0x18));
        }
        SF32(out, o + 4, y); SF32(out, o + 8, z); SF32(out, o, x); SF32(out, o + 0xc, 0.0f);
    }
    for (uint32_t o = 0x30; o < 0x3c; o += 4) SF32(out, o, rx_adds(rx_muls(F32(a, o), u), rx_muls(F32(b, o), t)));
    SF32(out, 0x3c, 1.0f);
    fx_frame_leave(&f);
}
RX_FUNCTION(0x006941a0, rx_fx_matrix_lerp)

/* the colour of a new particle (both builders): u = r 2^-15 rounded to a float; channels trunc(gain u + base) (x87, 53 bits,
 * truncating fistp): 0xff000000 | c0 << 16 | c1 << 8 | c2 with c0 / c1 / c2 or-ed in full. cw_saved / cw are the frame's
 * fnstcw / fldcw words, q the fistp qword. The builders keep them at E + 0xc / E + 0x10 / E + 0x10; 0069a170 at E + 0x54 /
 * E + 0x50 / E + 0x60. */
static uint32_t spawn_colour_at(rx_addr cw_saved, rx_addr cw, rx_addr q, rx_addr T, rx_f80 u) {
    rx_f80 a = fx_add(fx_mul(fx_ld(T + 0x144), u), fx_ld(T + 0x140));
    SU16(cw_saved, 0, FX_CW);
    SU32(cw, 0, FX_CW | 0xc00u);
    rx_wr64(q, (uint64_t)rx_f80_to_int(a, 3));
    uint32_t c = (U32(q, 0) | 0xffffff00u) << 8;
    a = fx_mul(fx_ld(T + 0x14c), u);
    SU16(cw_saved, 0, FX_CW);
    SU32(cw, 0, FX_CW | 0xc00u);
    a = fx_add(a, fx_ld(T + 0x148));
    rx_wr64(q, (uint64_t)rx_f80_to_int(a, 3));
    c = (c | U32(q, 0)) << 8;
    a = fx_mul(u, fx_ld(T + 0x154));
    SU16(cw_saved, 0, FX_CW);
    a = fx_add(a, fx_ld(T + 0x150));
    SU32(cw, 0, FX_CW | 0xc00u);
    rx_wr64(q, (uint64_t)rx_f80_to_int(a, 3));
    return c | U32(q, 0);
}
static uint32_t spawn_colour(rx_addr E, rx_addr T, rx_f80 u) {
    return spawn_colour_at(E + 0xc, E + 0x10, E + 0x10, T, u);
}
/* the atlas index of a new particle (+0x50): the asset's entry `index` (< 16) when the type uses the atlas, else -1 */
static uint32_t spawn_atlas(rx_addr T, rx_addr data, uint32_t index) {
    if (U8(T, 0x161) == 0) return 0xffffffffu;
    return index > 0xf ? 0xffffffffu : U32(data, 0x20 + 4u * index);
}
/* the size (+0x70): 3 lanes when the manager's vf7 says 2, else 2 (x87 copies) */
static void spawn_size(rx_addr T, rx_addr p, int three) {
    if (three) { fx_x87_copy(p + 0x70, T + 0xa0); fx_x87_copy(p + 0x74, T + 0xa4); fx_x87_copy(p + 0x78, T + 0xa8); }
    else { fx_x87_copy(p + 0x70, T + 0x98); fx_x87_copy(p + 0x74, T + 0x9c); }
}

/* ---- 006944c0 (thiscall manager, EAX = matrix M; stdcall emitter state S, atlas index, template P; RET 0xc): the lifetime
 * (one LCG draw: life = r (+0xc8) + (+0xc4), none below 1/60 -> 0); then 16 draws r0..r15 (as floats at E + 0x30): colour
 * (r0), position (r1..3), velocity (r4..6 + base), acceleration (r7..9), rotation (r10, + the angle to (1, 1, 1) when the type
 * aligns), scale (r11 / r12), frame (r13, r14), spin (r15); velocity and position (with M's translation), acceleration through
 * M; the start time = the timing's +0x40. Returns 1. Frame: push ebp; and esp, -16; sub esp, 0x64; ebx / esi / edi: E = A - 0x70:
 * +0xc / +0x10 the x87 temporaries, +0x10 the (1, 1, 1) vector, +0x28 asset data, +0x2c vf7 == 2 / the rotation, +0x30 r0..r15. */
uint32_t rx_fx_spawn_build(rx_addr mgr, rx_addr S, uint32_t index, rx_addr P, rx_addr M) {
    fx_frame f; fx_frame_enter(&f, 0x64, 16, FX_EBX, FX_ESI, FX_EDI);
    fx_set_reg(FX_EDI, M);
    const rx_addr E = f.E;
    const int three = vcall0(mgr, 0x1c, 0x006944d5) == 2;
    SU32(E, 0x2c, (uint32_t)three);
    const rx_addr holder = U32(S, 0x14);
    SU32(holder, 0x4c, 1);
    const rx_addr data = U32(holder, 0x44);
    const rx_addr T = U32(S, 0x18) * 0x1f0u + U32(data, 0x10);
    fx_set_reg(FX_ESI, P); fx_set_reg(FX_EBX, T);
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
    SU32(E, 0x28, data);
    uint32_t s = lcg(U32(0, 0x009cb860));
    SU32(0, 0x009cb860, s);
    const float life = rx_adds(rx_muls(rf(s), F32(T, 0xc8)), F32(T, 0xc4));
    SF32(P, 0x48, life);
    if (rx_bf(0x3c888889u) /* 0x977954: 1/60 */ > life) { fx_frame_leave(&f); return 0; }
    s = U32(0, 0x009cb860);
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 0x10);
    s = lcg(s);
    for (int i = 0; i < 16; i++) {
        const uint32_t v = (s >> 16) & 0x7fffu;
        s = lcg(s);
        SU32(E, 0x10, v);
        SF32(E, 0x30 + 4u * (uint32_t)i, (float)v);
    }
    SU32(0, 0x009cb860, s);
    const float k = rx_bf(0x38000000u);
#define R(i) F32(E, 0x30 + 4u * (i))
    if (U32(T, 0x158) != 0) {
        SF32(E, 0xc, fx_f32(fx_mul(fx_ld(E + 0x30), fx_f(k))));
        const rx_f80 u = fx_ld(E + 0xc);
        SU32(P, 0x4c, spawn_colour(E, T, u));
    }
    SU32(P, 0x50, spawn_atlas(T, U32(E, 0x28), index));
    SF32(P, 0, rx_adds(F32(T, 0x20), rx_muls(F32(T, 0x30), rx_muls(R(1), k))));
    SF32(P, 4, rx_adds(F32(T, 0x24), rx_muls(F32(T, 0x34), rx_muls(R(2), k))));
    SF32(P, 8, rx_adds(F32(T, 0x28), rx_muls(F32(T, 0x38), rx_muls(R(3), k))));
    const rx_addr V = P + 0x10;
    SF32(V, 0, rx_adds(F32(T, 0x50), rx_muls(F32(T, 0x60), rx_muls(R(4), k))));
    SF32(V, 4, rx_adds(F32(T, 0x54), rx_muls(F32(T, 0x64), rx_muls(R(5), k))));
    SF32(V, 8, rx_adds(F32(T, 0x58), rx_muls(F32(T, 0x68), rx_muls(R(6), k))));
    SF32(V, 0, rx_adds(F32(V, 0), F32(T, 0x40)));
    SF32(V, 4, rx_adds(F32(V, 4), F32(T, 0x44)));
    SF32(V, 8, rx_adds(F32(V, 8), F32(T, 0x48)));
    SF32(P, 0x20, rx_adds(rx_muls(F32(T, 0x80), rx_muls(R(7), k)), F32(T, 0x70)));
    SF32(P, 0x24, rx_adds(F32(T, 0x74), rx_muls(F32(T, 0x84), rx_muls(R(8), k))));
    SF32(P, 0x28, rx_adds(F32(T, 0x78), rx_muls(F32(T, 0x88), rx_muls(R(9), k))));
    {   /* velocity through M (no translation) */
        const float x = F32(V, 0), y = F32(V, 4), z = F32(V, 8);
        const float nx = rx_adds(rx_adds(rx_muls(y, F32(M, 0x10)), rx_muls(x, F32(M, 0))), rx_muls(z, F32(M, 0x20)));
        const float ny = rx_adds(rx_adds(rx_muls(x, F32(M, 4)), rx_muls(y, F32(M, 0x14))), rx_muls(z, F32(M, 0x24)));
        const float nz = rx_adds(rx_adds(rx_muls(x, F32(M, 8)), rx_muls(y, F32(M, 0x18))), rx_muls(z, F32(M, 0x28)));
        SF32(V, 0, nx); SF32(V, 4, ny); SF32(V, 8, nz);
    }
    const rx_addr src = U32(S, 0x90);
    if (src != 0) {
        const float g = F32(T, 0xe8);
        const float a0 = rx_muls(F32(src, 0), g), a1 = rx_muls(F32(src, 4), g), a2 = rx_muls(F32(src, 8), g);
        SF32(V, 0, rx_adds(F32(V, 0), a0)); SF32(V, 4, rx_adds(F32(V, 4), a1)); SF32(V, 8, rx_adds(F32(V, 8), a2));
    }
    {   /* position through M */
        const float x = F32(P, 0), y = F32(P, 4), z = F32(P, 8);
        const float nx = rx_adds(rx_adds(rx_adds(rx_muls(y, F32(M, 0x10)), rx_muls(F32(M, 0), x)), rx_muls(F32(M, 0x20), z)), F32(M, 0x30));
        const float ny = rx_adds(rx_adds(rx_adds(rx_muls(F32(M, 4), x), rx_muls(F32(M, 0x14), y)), rx_muls(z, F32(M, 0x24))), F32(M, 0x34));
        const float nz = rx_adds(rx_adds(rx_adds(rx_muls(x, F32(M, 8)), rx_muls(y, F32(M, 0x18))), rx_muls(F32(M, 0x28), z)), F32(M, 0x38));
        SF32(P, 0, nx); SF32(P, 4, ny); SF32(P, 8, nz);
    }
    {   /* acceleration through M */
        const float x = F32(P, 0x20), y = F32(P, 0x24), z = F32(P, 0x28);
        const float nx = rx_adds(rx_adds(rx_muls(F32(M, 0), x), rx_muls(F32(M, 0x10), y)), rx_muls(F32(M, 0x20), z));
        const float ny = rx_adds(rx_adds(rx_muls(F32(M, 4), x), rx_muls(F32(M, 0x14), y)), rx_muls(z, F32(M, 0x24)));
        const float nz = rx_adds(rx_adds(rx_muls(F32(M, 8), x), rx_muls(F32(M, 0x18), y)), rx_muls(F32(M, 0x28), z));
        SF32(P, 0x20, nx); SF32(P, 0x24, ny); SF32(P, 0x28, nz);
    }
    const rx_addr off = U32(S, 0xa0);
    if (off != 0) {
        SF32(P, 0x20, rx_adds(F32(off, 0), F32(P, 0x20)));
        SF32(P, 0x24, rx_adds(F32(P, 0x24), F32(off, 4)));
        SF32(P, 0x28, rx_adds(F32(P, 0x28), F32(off, 8)));
    }
    spawn_size(T, P, U32(E, 0x2c) != 0);
    const float rot = rx_adds(rx_muls(rx_muls(R(10), k), F32(T, 0xb4)), F32(T, 0xb0));
    SF32(P, 0x38, rot);
    SF32(E, 0x2c, rot);
    if (U8(T, 0xc0) != 0) {
        SF32(E, 0x10, 1.0f); SF32(E, 0x14, 1.0f); SF32(E, 0x18, 1.0f);
        const rx_addr sp = fx_push(0, 0, 0x00694aa0);
        const float ang = angle_between(E + 0x10, V, U32(S, 0xa0));
        fx_pop(sp);
        SF32(P, 0x38, rx_adds(ang, F32(E, 0x2c)));
    }
    SF32(P, 0x3c, rx_adds(rx_muls(rx_muls(R(15), k), F32(T, 0xbc)), F32(T, 0xb8)));
    SF32(P, 0x40, 0.0f);
    const float s11 = rx_muls(R(11), k);
    float s12 = rx_muls(R(12), k);
    if (U8(T, 0x128) != 0) s12 = s11;
    SF32(P, 0x30, rx_adds(F32(T, 0x118), rx_muls(F32(T, 0x120), s11)));
    SF32(P, 0x34, rx_adds(F32(T, 0x11c), rx_muls(F32(T, 0x124), s12)));
    SF32(P, 0x54, rx_adds(rx_muls(rx_muls(R(13), k), F32(T, 0xd8)), F32(T, 0xd4)));
    SF32(P, 0x58, rx_adds(rx_muls(rx_muls(R(14), k), F32(T, 0xe0)), F32(T, 0xdc)));
#undef R
    SF32(P, 0x44, timing_time(fx_timing(), 0x00694b85));
    fx_frame_leave(&f);
    return 1;
}
RX_FUNCTION(0x006944c0, rx_fx_spawn_build)

/* ---- 00694ba0 (thiscall manager; stdcall emitter state S, matrix M, atlas index, template P; RET 0x10): not while the game
 * is paused (timing +0xc, the level world's +0x20014). As 006944c0, drawing as it goes: life; colour (one draw); position (3
 * draws: x from the third); velocity = a random direction (3 draws - 0.5, normalized when > 0) + base, times a speed (one draw,
 * +0x100 / +0xfc), through M (+ the velocity source * +0xe8); acceleration (3 draws, + the offset, through M); rotation, spin,
 * scale (2 draws), frame (2 draws). Frame: push ebp; and esp, -16; sub esp, 0x24; ebx / esi / edi: E = A - 0x30: +0xc / +0x10
 * x87 temporaries / the direction's x and |d|^2, +0x10 the (1, 1, 1) vector, +0x2c vf7 == 2 / the rotation. */
uint32_t rx_fx_spawn_build2(rx_addr mgr, rx_addr S, rx_addr M, uint32_t index, rx_addr P) {
    fx_frame f; fx_frame_enter(&f, 0x24, 16, FX_EBX, FX_ESI, FX_EDI);
    const rx_addr E = f.E;
    const rx_addr c = fx_ctx();
    uint32_t ok = 0;
    if (U32(U32(c, 0x10), 0xc) == 0) {
        rx_addr lw = 0;
        if (U32(U32(c, 0x20), 0x34) == 1) lw = U32(U32(0, FX_G_GAME), 0x5a54);
        else if (U32(U32(c, 0x1c), 0x34) == 1) lw = U32(U32(0, FX_G_GAME), 0x20c8);
        if (U32(lw, 0x20014) == 0) ok = 1;
    }
    if (!ok) { fx_frame_leave(&f); return 0; }
    const rx_addr holder = U32(S, 0x14);
    const rx_addr data = U32(holder, 0x44);
    SU32(holder, 0x4c, 1);
    const rx_addr T = U32(S, 0x18) * 0x1f0u + U32(data, 0x10);
    fx_set_reg(FX_ESI, 1); fx_set_reg(FX_EDI, data); fx_set_reg(FX_EBX, T);
    const int three = vcall0(mgr, 0x1c, 0x00694c1b) == 2;
    fx_set_reg(FX_ESI, P);
    uint32_t s = lcg(U32(0, 0x009cb860));
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
    SU32(0, 0x009cb860, s);
    const float life = rx_adds(rx_muls(rf(s), F32(T, 0xc8)), F32(T, 0xc4));
    SU32(E, 0x2c, (uint32_t)three);
    SF32(P, 0x48, life);
    if (rx_bf(0x3c888889u) /* 0x977954 */ > life) { fx_frame_leave(&f); return 0; }
    if (U32(T, 0x158) != 0) {
        s = lcg(U32(0, 0x009cb860));
        SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
        SU32(0, 0x009cb860, s);
        SU32(E, 0xc, (s >> 16) & 0x7fffu);
        SF32(E, 0xc, fx_f32(fx_mul(rx_f80_from_int((int32_t)U32(E, 0xc)), rx_f80_from_f64(FX_K15))));
        SU32(P, 0x4c, spawn_colour(E, T, fx_ld(E + 0xc)));
    }
    SU32(P, 0x50, spawn_atlas(T, data, index));
    uint32_t s1, s2, s3;
    /* position */
    s1 = lcg(U32(0, 0x009cb860)); s2 = lcg(s1); s3 = lcg(s2);
    SU32(0, 0x009cb860, s3);
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 3);
    SF32(P, 0, rx_adds(F32(T, 0x20), rx_muls(F32(T, 0x30), rf(s3))));
    SF32(P, 4, rx_adds(F32(T, 0x24), rx_muls(F32(T, 0x34), rf(s2))));
    SF32(P, 8, rx_adds(F32(T, 0x28), rx_muls(F32(T, 0x38), rf(s1))));
    /* direction */
    s1 = lcg(U32(0, 0x009cb860)); s2 = lcg(s1); s3 = lcg(s2);
    SU32(0, 0x009cb860, s3);
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 3);
    const rx_addr V = P + 0x10;
    {
        const float dz = rx_subs(rf(s1), 0.5f), dx = rx_subs(rf(s3), 0.5f), dy = rx_subs(rf(s2), 0.5f);
        SF32(V, 8, dz); SF32(V, 0, dx); SF32(V, 4, dy);
        const float z = F32(V, 8);
        SF32(E, 0xc, dx);
        const float l2 = rx_adds(rx_adds(rx_muls(dx, dx), rx_muls(dy, dy)), rx_muls(z, z));
        SF32(E, 0x10, l2);
        if (l2 > 0.0f) {
            rx_set_sp(E);
            const float inv = 1.0f / sqrtf_call(l2, 0x00694f5f);
            SF32(V, 0, rx_muls(F32(E, 0xc), inv));
            SF32(V, 4, rx_muls(F32(V, 4), inv));
            SF32(V, 8, rx_muls(F32(V, 8), inv));
        }
    }
    SF32(V, 0, rx_adds(F32(T, 0x40), F32(V, 0)));
    SF32(V, 4, rx_adds(F32(T, 0x44), F32(V, 4)));
    SF32(V, 8, rx_adds(F32(T, 0x48), F32(V, 8)));
    /* acceleration */
    s1 = lcg(U32(0, 0x009cb860)); s2 = lcg(s1); s3 = lcg(s2);
    SU32(0, 0x009cb860, s3);
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 3);
    {
        const float a0 = rx_adds(F32(T, 0x70), rx_muls(rf(s3), F32(T, 0x80)));
        SF32(P, 0x20, a0);
        SF32(P, 0x24, rx_adds(F32(T, 0x74), rx_muls(F32(T, 0x84), rf(s2))));
        SF32(P, 0x28, rx_adds(F32(T, 0x78), rx_muls(F32(T, 0x88), rf(s1))));
        const rx_addr off = U32(S, 0xa0);
        if (off != 0) {
            SF32(P, 0x20, rx_adds(a0, F32(off, 0)));
            SF32(P, 0x24, rx_adds(F32(P, 0x24), F32(off, 4)));
            SF32(P, 0x28, rx_adds(F32(P, 0x28), F32(off, 8)));
        }
    }
    /* speed, velocity through M */
    s = lcg(U32(0, 0x009cb860));
    SU32(0, 0x009cb860, s);
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
    {
        const float sp = rx_adds(rx_muls(rf(s), F32(T, 0x100)), F32(T, 0xfc));
        const rx_addr vs = U32(S, 0x90);
        SF32(V, 0, rx_muls(F32(V, 0), sp));
        SF32(V, 4, rx_muls(sp, F32(V, 4)));
        SF32(V, 8, rx_muls(sp, F32(V, 8)));
        const float x = F32(V, 0), y = F32(V, 4), z = F32(V, 8);
        const float nx = rx_adds(rx_adds(rx_muls(F32(M, 0), x), rx_muls(y, F32(M, 0x10))), rx_muls(z, F32(M, 0x20)));
        const float ny = rx_adds(rx_adds(rx_muls(F32(M, 4), x), rx_muls(F32(M, 0x14), y)), rx_muls(F32(M, 0x24), z));
        float nz = rx_adds(rx_adds(rx_muls(x, F32(M, 8)), rx_muls(y, F32(M, 0x18))), rx_muls(z, F32(M, 0x28)));
        SF32(V, 0, nx); SF32(V, 4, ny);
        if (vs != 0) {
            SF32(V, 8, nz);
            const float g = F32(T, 0xe8);
            const float a0 = rx_muls(g, F32(vs, 0)), a1 = rx_muls(F32(vs, 4), g), a2 = rx_muls(F32(vs, 8), g);
            SF32(V, 0, rx_adds(F32(V, 0), a0));
            SF32(V, 4, rx_adds(F32(V, 4), a1));
            nz = rx_adds(F32(V, 8), a2);
        }
        SF32(V, 8, nz);
    }
    {   /* position through M */
        const float x = F32(P, 0), y = F32(P, 4), z = F32(P, 8);
        const float nx = rx_adds(rx_adds(rx_adds(rx_muls(y, F32(M, 0x10)), rx_muls(x, F32(M, 0))), rx_muls(z, F32(M, 0x20))), F32(M, 0x30));
        const float ny = rx_adds(rx_adds(rx_adds(rx_muls(x, F32(M, 4)), rx_muls(y, F32(M, 0x14))), rx_muls(z, F32(M, 0x24))), F32(M, 0x34));
        const float nz = rx_adds(rx_adds(rx_adds(rx_muls(x, F32(M, 8)), rx_muls(y, F32(M, 0x18))), rx_muls(z, F32(M, 0x28))), F32(M, 0x38));
        SF32(P, 0, nx); SF32(P, 8, nz); SF32(P, 4, ny);
    }
    {   /* acceleration through M */
        const float x = F32(P, 0x20), y = F32(P, 0x24), z = F32(P, 0x28);
        const float nx = rx_adds(rx_adds(rx_muls(F32(M, 0), x), rx_muls(y, F32(M, 0x10))), rx_muls(z, F32(M, 0x20)));
        const float ny = rx_adds(rx_adds(rx_muls(x, F32(M, 4)), rx_muls(y, F32(M, 0x14))), rx_muls(F32(M, 0x24), z));
        const float nz = rx_adds(rx_adds(rx_muls(x, F32(M, 8)), rx_muls(y, F32(M, 0x18))), rx_muls(z, F32(M, 0x28)));
        SF32(P, 0x20, nx); SF32(P, 0x24, ny); SF32(P, 0x28, nz);
    }
    spawn_size(T, P, U32(E, 0x2c) != 0);
    s = lcg(U32(0, 0x009cb860));
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
    SU32(0, 0x009cb860, s);
    const float rot = rx_adds(rx_muls(rf(s), F32(T, 0xb4)), F32(T, 0xb0));
    SF32(P, 0x38, rot);
    SF32(E, 0x2c, rot);
    if (U8(T, 0xc0) != 0) {
        SF32(E, 0x10, 1.0f); SF32(E, 0x14, 1.0f); SF32(E, 0x18, 1.0f);
        const rx_addr sp = fx_push(0, 0, 0x00695482);
        const float ang = angle_between(E + 0x10, V, S);
        fx_pop(sp);
        SF32(P, 0x38, rx_adds(ang, F32(E, 0x2c)));
    }
    s = lcg(U32(0, 0x009cb860));
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
    SU32(0, 0x009cb860, s);
    SF32(P, 0x3c, rx_adds(rx_muls(rf(s), F32(T, 0xbc)), F32(T, 0xb8)));
    SF32(P, 0x40, 0.0f);
    s1 = lcg(U32(0, 0x009cb860)); s2 = lcg(s1);
    SU32(0, 0x009cb860, s2);
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 2);
    {
        float a = rf(s1);
        const float b = rf(s2);
        if (U8(T, 0x128) != 0) a = b;
        SF32(P, 0x30, rx_adds(rx_muls(F32(T, 0x120), b), F32(T, 0x118)));
        SF32(P, 0x34, rx_adds(F32(T, 0x11c), rx_muls(F32(T, 0x124), a)));
    }
    s = lcg(U32(0, 0x009cb860));
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
    SU32(0, 0x009cb860, s);
    SF32(P, 0x54, rx_adds(rx_muls(rf(s), F32(T, 0xd8)), F32(T, 0xd4)));
    s = lcg(U32(0, 0x009cb860));
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
    SU32(0, 0x009cb860, s);
    SF32(P, 0x58, rx_adds(rx_muls(rf(s), F32(T, 0xe0)), F32(T, 0xdc)));
    SF32(P, 0x44, timing_time(fx_timing(), 0x00695630));
    fx_frame_leave(&f);
    return 1;
}
RX_FUNCTION(0x00694ba0, rx_fx_spawn_build2)

static void spawn_call(rx_addr mgr, rx_addr owner, rx_addr P, rx_addr ret);   /* the manager's vf1 (defined below) */

/* the wrapped angle of the Y ring's direction (0069a170's two uses): pi - (|v| - trunc(|v| / 2pi) * 2pi) (Float_Abs 00401000
 * is the `andps` with 0x977570, inlined; the caller writes the call's frame words) */
static float yring_wrap(float v) {
    const float g = rx_bf(rx_fb(v) & 0x7fffffffu);
    const float t = rx_muls((float)rx_cvtt(rx_muls(g, rx_bf(0x3e22f983u) /* 0x9774a0: 1 / 2pi */)),
                            rx_bf(0x40c90fdbu) /* 0x97749c: 2pi */);
    return rx_subs(rx_bf(0x40490fdbu) /* 0x977498: pi */, rx_subs(g, t));
}

/* ---- 0069a170 (stdcall emitter state S, manager, matrix, atlas index; RET 0x10): the YRingParticleEmitter's spawn builder.
 * S's asset holder (+0x14) gives the type list D (+0x10) and the 0x1f0 record T (S +0x18); the holder is marked busy (+0x4c). One
 * LCG draw gives the lifetime (T +0xc4 / +0xc8); below 1/60 nothing spawns. Then the colour (T +0x158: one draw, the three
 * truncating channel fistp's), the atlas index (T +0x161, D +0x20), the position (3 draws, T +0x20 / +0x30), the ring's start
 * angle (one draw, T +0xec / +0xf0): two wrapped angles a0 + pi/2 and a0 through the cosine polynomial 0x935d3c.. give the
 * direction (-cos a0, 0, cos (a0 + pi/2)), normalized when its squared length is above 0; the speed (one draw, T +0xfc / +0x100)
 * and the Y velocity range (one draw, T +0xf4 / +0xf8) plus the base velocity (T +0x40), the acceleration (3 draws, T +0x70 /
 * +0x80; the offset S +0xa0), through the matrix (velocity without the translation, then the source S +0x90 * T +0xe8; position
 * with it; acceleration); the player-range gate 00695b00 on the position. In range: the two sizes (T +0x98 / +0x9c), the rotation
 * (one draw, + the angle to the velocity when T +0xc0), the spin (one draw), the two scale draws (T +0x118 / +0x120 / +0x11c /
 * +0x124, mirrored when T +0x128), the frame and frame2 draws (T +0xcc / +0xd0 / +0xdc / +0xe0) and the manager's spawn (vf1) of
 * the template at E + 0x80 with the owner block S + 0x70; the holder's busy flag cleared. Frame: push ebp; and esp, -128; sub
 * esp, 0xf4; ebx / esi / edi: E = A - 0x100: +0x50..+0x68 x87 scratch / the (1, 1, 1) angle vector, +0x70..+0x88 position,
 * +0x90.. velocity, +0xa0.. acceleration, +0xb0 scale, +0xb8 rotation, +0xbc spin, +0xc4 time, +0xc8 lifetime, +0xcc colour,
 * +0xd0 atlas, +0xd4 / +0xd8 frames, +0xf0 sizes (the template = E + 0x80, 0x80 bytes). */
void rx_yring_spawn(rx_addr S, rx_addr mgr, rx_addr mat, uint32_t index) {
    fx_frame f; fx_frame_enter(&f, 0xf4, 128, FX_EBX, FX_ESI, FX_EDI);
    const rx_addr E = f.E;
    const rx_addr H = U32(S, 0x14);
    const rx_addr D = U32(H, 0x44);
    SU32(H, 0x4c, 1);
    const rx_addr T = U32(S, 0x18) * 0x1f0u + U32(D, 0x10);
    fx_set_reg(FX_EBX, T);
    uint32_t s = lcg(U32(0, 0x009cb860));
    uint32_t cnt = U32(0, 0x009cb864) + 1;
    SU32(0, 0x009cb864, cnt); SU32(0, 0x009cb860, s);
    const float life = rx_adds(rx_muls(rf(s), F32(T, 0xc8)), F32(T, 0xc4));
    SF32(E, 0xc8, life);
    fx_set_reg(FX_ESI, cnt);
    if (rx_bf(0x3c888889u) /* 0x977954: 1/60 */ > life) { fx_frame_leave(&f); return; }
    if (U32(T, 0x158) != 0) {
        s = lcg(s);
        cnt++;
        SU32(0, 0x009cb860, s); SU32(0, 0x009cb864, cnt);
        SU32(E, 0x50, (s >> 16) & 0x7fffu);                   /* fild's integer source */
        SF32(E, 0x50, rf(s));                                 /* fstp: u rounded to a float */
        SU32(E, 0xcc, spawn_colour_at(E + 0x54, E + 0x50, E + 0x60, T, fx_ld(E + 0x50)));
    }
    SU32(E, 0xd0, spawn_atlas(T, D, index));
    /* position: three draws (x from the third, y from the second, z from the first) */
    const uint32_t sA = lcg(s), sB = lcg(sA), sC = lcg(sB), sD = lcg(sC);
    SU32(0, 0x009cb860, sC);
    cnt += 3; SU32(0, 0x009cb864, cnt);
    SF32(E, 0x80, rx_adds(F32(T, 0x20), rx_muls(F32(T, 0x30), rf(sC))));
    SF32(E, 0x84, rx_adds(F32(T, 0x24), rx_muls(F32(T, 0x34), rf(sB))));
    SF32(E, 0x88, rx_adds(F32(T, 0x28), rx_muls(F32(T, 0x38), rf(sA))));
    cnt++; SU32(0, 0x009cb864, cnt); SU32(0, 0x009cb860, sD);
    /* the start angle: one draw; two wrapped values, each through the cosine polynomial */
    const float a0 = rx_adds(rx_muls(rf(sD), F32(T, 0xf0)), F32(T, 0xec));
    SF32(E, 0x50, a0);
    const float c0 = rx_bf(0x37cb076bu) /* 0x935d48 */, c1 = rx_bf(0xb46beed0u) /* 0x935d50 */;
    const float c2 = rx_bf(0xbab5a662u) /* 0x935d44 */, c3 = rx_bf(0x3d2aa372u) /* 0x935d4c */;
    const float c4 = rx_bf(0xbeffff3du) /* 0x935d40 */, c5 = rx_bf(0x3f7ffff3u) /* 0x935d3c */;
    SU32(E, -4, 0x0069a44bu); SU32(E, -8, fx_reg(FX_EBP));  /* Float_Abs 00401000: push ebp */
    const float f1 = yring_wrap(rx_adds(a0, rx_bf(0x3fc90fdbu) /* 0x977508: pi/2 */));
    const float y1 = rx_muls(f1, f1), y1sq = rx_muls(y1, y1);
    SF32(E, 0x58, y1); SF32(E, 0x5c, y1sq);
    SU32(E, -4, 0x0069a498u); SU32(E, -8, fx_reg(FX_EBP));
    const float f2 = yring_wrap(a0);
    const float z = rx_muls(f2, f2), z2 = rx_muls(z, z);
    const float s1 = rx_adds(rx_muls(rx_adds(rx_muls(rx_adds(rx_muls(z, c1), c0), z2), rx_adds(rx_muls(z, c2), c3)), z2),
                             rx_adds(rx_muls(z, c4), c5));
    const float s2 = rx_adds(rx_muls(rx_adds(rx_muls(rx_adds(rx_muls(y1, c1), c0), y1sq), rx_adds(rx_muls(y1, c2), c3)), y1sq),
                             rx_adds(rx_muls(y1, c4), c5));
    const float neg1 = rx_subs(rx_bf(0x80000000u) /* 0x9774a8: -0.0 */, s1);
    const float len2 = rx_adds(rx_adds(rx_muls(neg1, neg1), rx_muls(0.0f, 0.0f)), rx_muls(s2, s2));
    float dx = neg1, dz = s2;
    if (len2 > 0.0f) {
        const float len = sqrtf_call(len2, 0x0069a5e3u);
        const float inv = rx_divs(1.0f /* 0x931f24 */, len);
        dx = rx_muls(neg1, inv);
        dz = rx_muls(s2, inv);
    }
    /* velocity: the speed draw, the Y range draw, the base velocity */
    const uint32_t sE = lcg(sD); cnt++;
    SU32(0, 0x009cb860, sE); SU32(0, 0x009cb864, cnt);
    const float speed = rx_adds(rx_muls(rf(sE), F32(T, 0x100)), F32(T, 0xfc));
    const uint32_t sF = lcg(sE);
    SU32(0, 0x009cb860, sF);
    const float a2 = rx_adds(rx_muls(rf(sF), F32(T, 0xf8)), F32(T, 0xf4));
    const uint32_t sG = lcg(sF), sH = lcg(sG), sI = lcg(sH);
    cnt += 4; SU32(0, 0x009cb864, cnt); SU32(0, 0x009cb860, sI);
    float vx = rx_adds(rx_muls(dx, speed), F32(T, 0x40));
    float vy = rx_adds(F32(T, 0x44), a2);
    float vz = rx_adds(F32(T, 0x48), rx_muls(dz, speed));
    /* acceleration: three draws (x from the last), plus the offset */
    float ax = rx_adds(F32(T, 0x70), rx_muls(rf(sI), F32(T, 0x80)));
    float ay = rx_adds(F32(T, 0x74), rx_muls(F32(T, 0x84), rf(sH)));
    float az = rx_adds(F32(T, 0x78), rx_muls(F32(T, 0x88), rf(sG)));
    const rx_addr off = U32(S, 0xa0);
    if (off != 0) {
        ax = rx_adds(ax, F32(off, 0));
        ay = rx_adds(F32(off, 4), ay);
        az = rx_adds(F32(off, 8), az);
    }
    /* through the matrix: velocity (no translation) and its source, position (translation), acceleration */
    {
        const float nx = rx_adds(rx_adds(rx_muls(F32(mat, 0), vx), rx_muls(F32(mat, 0x10), vy)), rx_muls(F32(mat, 0x20), vz));
        const float ny = rx_adds(rx_adds(rx_muls(F32(mat, 0x14), vy), rx_muls(F32(mat, 4), vx)), rx_muls(F32(mat, 0x24), vz));
        const float nz = rx_adds(rx_adds(rx_muls(F32(mat, 8), vx), rx_muls(F32(mat, 0x18), vy)), rx_muls(F32(mat, 0x28), vz));
        vx = nx; vy = ny; vz = nz;
    }
    const rx_addr src = U32(S, 0x90);
    if (src != 0) {
        const float g = F32(T, 0xe8);
        const float g0 = rx_muls(F32(src, 0), g), g1 = rx_muls(F32(src, 4), g), g2 = rx_muls(F32(src, 8), g);
        vx = rx_adds(vx, g0); vy = rx_adds(g1, vy); vz = rx_adds(g2, vz);
    }
    {
        const float px = F32(E, 0x80), py = F32(E, 0x84), pz = F32(E, 0x88);
        const float nx = rx_adds(rx_adds(rx_adds(rx_muls(F32(mat, 0), px), rx_muls(F32(mat, 0x10), py)), rx_muls(F32(mat, 0x20), pz)), F32(mat, 0x30));
        const float ny = rx_adds(rx_adds(rx_adds(rx_muls(F32(mat, 4), px), rx_muls(F32(mat, 0x14), py)), rx_muls(F32(mat, 0x24), pz)), F32(mat, 0x34));
        const float nz = rx_adds(rx_adds(rx_adds(rx_muls(F32(mat, 8), px), rx_muls(F32(mat, 0x18), py)), rx_muls(F32(mat, 0x28), pz)), F32(mat, 0x38));
        SF32(E, 0x80, nx); SF32(E, 0x84, ny); SF32(E, 0x88, nz);
    }
    {
        const float nx = rx_adds(rx_adds(rx_muls(F32(mat, 0), ax), rx_muls(F32(mat, 0x10), ay)), rx_muls(F32(mat, 0x20), az));
        const float ny = rx_adds(rx_adds(rx_muls(F32(mat, 4), ax), rx_muls(F32(mat, 0x14), ay)), rx_muls(F32(mat, 0x24), az));
        const float nz = rx_adds(rx_adds(rx_muls(F32(mat, 8), ax), rx_muls(F32(mat, 0x18), ay)), rx_muls(F32(mat, 0x28), az));
        SF32(E, 0xa0, nx); SF32(E, 0xa4, ny); SF32(E, 0xa8, nz);
    }
    SF32(E, 0x90, vx); SF32(E, 0x94, vy); SF32(E, 0x98, vz);
    fx_set_reg(FX_EDI, sI); fx_set_reg(FX_ESI, cnt); fx_set_reg(FX_ECX, S);
    {
        const uint32_t a[2] = {S, E + 0x80};
        const rx_addr sp = fx_push(2, a, 0x0069aab1u);
        const uint32_t hit = rx_fx_emitter_in_range(S, E + 0x80);
        fx_pop(sp);
        if (hit == 0) { fx_frame_leave(&f); return; }
    }
    SF32(E, 0xf0, F32(T, 0x98)); SF32(E, 0xf4, F32(T, 0x9c));
    /* rotation (one draw, plus the angle to the velocity when T +0xc0), spin (one draw), scale (two), frame and frame2 (two) */
    const uint32_t sJ = lcg(sI); cnt++;
    SU32(0, 0x009cb860, sJ); SU32(0, 0x009cb864, cnt);
    SF32(E, 0xb8, rx_adds(rx_muls(rf(sJ), F32(T, 0xb4)), F32(T, 0xb0)));
    if (U8(T, 0xc0) != 0) {
        SF32(E, 0x60, 1.0f); SF32(E, 0x64, 1.0f); SF32(E, 0x68, 1.0f);
        fx_set_reg(FX_ESI, E + 0x60); fx_set_reg(FX_EDI, E + 0x90);
        const rx_addr sp = fx_push(0, 0, 0x0069ab61u);
        const float ang = rx_fx_vec_angle(E + 0x60, E + 0x90);
        fx_pop(sp);
        SF32(E, 0xb8, rx_adds(ang, F32(E, 0xb8)));
    }
    const uint32_t sK = lcg(sJ); cnt++;
    SU32(0, 0x009cb860, sK); SU32(0, 0x009cb864, cnt);
    SF32(E, 0xbc, rx_adds(rx_muls(rf(sK), F32(T, 0xbc)), F32(T, 0xb8)));
    SF32(E, 0xc0, 0.0f);
    const uint32_t k2 = lcg(sK), k3 = lcg(k2);
    cnt += 2; SU32(0, 0x009cb864, cnt);
    const float rk2 = rf(k2), rk3 = rf(k3);
    SF32(E, 0xb0, rx_adds(rx_muls(F32(T, 0x120), rk3), F32(T, 0x118)));
    SF32(E, 0xb4, rx_adds(F32(T, 0x11c), rx_muls(F32(T, 0x124), U8(T, 0x128) != 0 ? rk3 : rk2)));
    const uint32_t k4 = lcg(k3); cnt++;
    SU32(0, 0x009cb860, k4); SU32(0, 0x009cb864, cnt);
    SF32(E, 0xd4, rx_adds(rx_muls(rf(k4), F32(T, 0xd0)), F32(T, 0xcc)));
    const uint32_t k5 = lcg(k4); cnt++;
    SU32(0, 0x009cb860, k5); SU32(0, 0x009cb864, cnt);
    SF32(E, 0xd8, rx_adds(rx_muls(rf(k5), F32(T, 0xe0)), F32(T, 0xdc)));
    SF32(E, 0xc4, timing_time(U32(fx_ctx(), 0x10), 0x0069ad22u));
    fx_set_reg(FX_ESI, S); fx_set_reg(FX_EDI, sK);
    spawn_call(mgr, S + 0x70, E + 0x80, 0x0069ad44u);
    SU32(H, 0x4c, 0);
    fx_frame_leave(&f);
}
RX_FUNCTION(0x0069a170, rx_yring_spawn)

/* ---- 00695b00 (stdcall emitter state S, position*; RET 8): 1 when some player of the active scene (scene +0x19b8, count +0x19d0;
 * [[entry]] +0x10 its position) lies, squared distance, within one of the four enabled ranges (+0xb4 flags; bounds: the lower
 * of range i is bound i - 1 (range 0: 0), the upper bound i, all from +0xa4), lanes 0..2 of the compare masks (cmpleps).
 * Frame: push ebp; and esp, -16; sub esp, 0x94 (/GS cookie at +0x90); ebx / esi / edi: E = A - 0xa0: +0x18 cursor, +0x1c
 * count, +0x20 / +0x30 the masks, +0x40 the five bound splats. */
uint32_t rx_fx_emitter_in_range(rx_addr S, rx_addr pos) {
    fx_frame f; fx_frame_enter(&f, 0x94, 16, FX_EBX, FX_ESI, FX_EDI);
    const rx_addr E = f.E, esp0 = f.A - 0x94;
    SU32(esp0, 0x90, U32(0, 0x009b45e4) ^ esp0);
    const rx_v4 p = rx_ld4(pos);
    uint32_t hit = 0;
    rx_st4(E + 0x40, (rx_v4){{0.0f, 0.0f, 0.0f, 0.0f}});   /* 0x92e12c splat */
    for (uint32_t i = 0; i < 4; i++) { const float b = F32(S, 0xa4 + 4 * i); rx_st4(E + 0x50 + 0x10 * i, (rx_v4){{b, b, b, b}}); }
    const rx_addr scene = fx_active_scene();
    if (scene != 0 && U32(scene, 0x34) != 0) {
        int32_t n = I32(scene, 0x19d0);
        if (n > 0) {
            rx_addr cur = scene + 0x19b8;
            SU32(E, 0x18, cur); SU32(E, 0x1c, (uint32_t)n);
            for (;;) {
                const rx_addr pl = U32(U32(cur, 0), 0);
                float d[4];
                for (int l = 0; l < 4; l++) { const float v = rx_subs(F32(pl, 0x10 + 4u * (uint32_t)l), p.v[l]); d[l] = rx_muls(v, v); }
                const float l2 = rx_adds(rx_adds(d[0], d[1]), d[2]);
                for (uint32_t i = 0; i < 4; i++) {
                    if (U8(S, 0xb4 + i) == 0) continue;
                    const rx_addr lo = E + 0x40 + 0x10 * i, hi = E + 0x50 + 0x10 * i;
                    for (int l = 0; l < 4; l++) SU32(E, 0x30 + 4u * (uint32_t)l, F32(lo, 4u * (uint32_t)l) <= l2 ? 0xffffffffu : 0);
                    for (int l = 0; l < 4; l++) SU32(E, 0x20 + 4u * (uint32_t)l, l2 <= F32(hi, 4u * (uint32_t)l) ? 0xffffffffu : 0);
                    const uint32_t a = (U32(E, 0x28) + U32(E, 0x24) + U32(E, 0x20)) == 0xfffffffdu;
                    const uint32_t b = (U32(E, 0x38) + U32(E, 0x34) + U32(E, 0x30)) == 0xfffffffdu;
                    hit |= a & b;
                }
                cur += 4;
                SU32(E, 0x1c, U32(E, 0x1c) - 1);
                SU32(E, 0x18, cur);
                if (U32(E, 0x1c) == 0) break;
            }
        }
    }
    SU32(esp0, -4, 0x00695c59);                             /* __security_check_cookie's return address (then RET 8) */
    phys_security_check_cookie(U32(esp0, 0x90) ^ esp0);
    fx_frame_leave(&f);
    return hit;
}
RX_FUNCTION(0x00695b00, rx_fx_emitter_in_range)

/* ---- 00695650 (EAX = emitter state S, EDI = manager): the player-distance gate of an emitter: in range (00695b00 at the
 * position +0x60) and active: start it (manager vf4 (user +0x94, trunc(rate * (gain 0.5 + base) * 0.75), owner block +0x70))
 * unless running; out of range or inactive: stop it (vf5 (owner block)) when running. Returns what vf4 / vf5 return (EAX).
 * Frame: push ebp; and esp, -16; sub esp, 0x1c; esi: E = A - 0x20: +0xc / +0x10 the x87 operands and temporaries, +0x10
 * position. */
uint32_t rx_fx_emitter_gate(rx_addr S, rx_addr mgr) {
    fx_frame f; fx_frame_enter(&f, 0x1c, 16, FX_ESI);
    fx_set_reg(FX_ESI, S);
    const rx_addr E = f.E;
    SF32(E, 0x10, F32(S, 0x60));
    SU32(E - 4, 0, E + 0x10);
    SF32(E, 0x14, F32(S, 0x64));
    SU32(E - 8, 0, S);
    SF32(E, 0x18, F32(S, 0x68));
    rx_set_sp(E - 8);
    const rx_addr sp = fx_push(0, 0, 0x00695688);
    const uint32_t in = rx_fx_emitter_in_range(S, E + 0x10);
    fx_pop(sp);
    rx_set_sp(E);                                             /* 00695b00 is stdcall (RET 8) */
    uint32_t r = in;
    if (U32(S, 0x1c) == 0 || in == 0) {
        if (U32(S, 0x7c) != 0) {
            const rx_addr fn = U32(U32(mgr, 0), 0x14);
            uint32_t (*g)(rx_addr, rx_addr) = RX_FN(fn, uint32_t (*)(rx_addr, rx_addr));
            const uint32_t a[1] = {S + 0x70};
            if (g) { const rx_addr s2 = fx_push(1, a, 0x0069576e); r = g(mgr, S + 0x70); fx_pop(s2); }
            else r = rx_call_original(fn, RX_THISCALL, mgr, 1, a);
        }
    } else if (U32(S, 0x7c) == 0) {
        const rx_addr holder = U32(S, 0x14);
        SU32(holder, 0x4c, 1);
        const rx_addr T = U32(S, 0x18) * 0x1f0u + U32(U32(holder, 0x44), 0x10);
        const float half = 0.5f /* 0x9322cc */;
        SF32(E, 0x10, rx_adds(rx_muls(F32(T, 0x108), half), F32(T, 0x104)));
        float rate = F32(S, 0x10);
        if (!(rate >= rx_bf(0x3727c5acu) /* 0x9774a4: 1e-5 */)) rate = rx_adds(rx_muls(F32(T, 0xc8), half), F32(T, 0xc4));
        const rx_addr fn = U32(U32(mgr, 0), 0x10);
        SF32(E, 0xc, rate);
        rx_f80 x = fx_mul(fx_ld(E + 0xc), fx_ld(E + 0x10));
        SU32(E - 4, 0, S + 0x70);
        SU16(E, 0xc, FX_CW);
        x = fx_mul(x, fx_f(0.75f /* 0x935aa8 */));
        SU32(E, 0x10, FX_CW | 0xc00u);
        const uint32_t user = U32(S, 0x94);
        rx_wr64(E + 0x10, (uint64_t)rx_f80_to_int(x, 3));
        const uint32_t n = U32(E, 0x10);
        uint32_t (*g)(rx_addr, uint32_t, uint32_t, rx_addr) = RX_FN(fn, uint32_t (*)(rx_addr, uint32_t, uint32_t, rx_addr));
        const uint32_t a[3] = {user, n, S + 0x70};
        if (g) { const rx_addr s2 = fx_push(3, a, 0x0069574c); r = g(mgr, user, n, S + 0x70); fx_pop(s2); }
        else r = rx_call_original(fn, RX_THISCALL, mgr, 3, a);
        SU32(U32(S, 0x14), 0x4c, 0);
    }
    fx_frame_leave(&f);
    return r;
}
RX_FUNCTION(0x00695650, rx_fx_emitter_gate)

/* a builder call: 006944c0 (thiscall manager, EAX = matrix; S, index, template) or 00694ba0 (S, matrix, index, template),
 * at the current ESP with the call site's return address */
static uint32_t build1(rx_addr mgr, rx_addr S, uint32_t index, rx_addr P, rx_addr M, rx_addr ret) {
    const uint32_t a[3] = {S, index, P};
    const rx_addr s = fx_push(3, a, ret);
    const uint32_t r = rx_fx_spawn_build(mgr, S, index, P, M);
    fx_pop(s);
    return r;
}
static uint32_t build2(rx_addr mgr, rx_addr S, rx_addr M, uint32_t index, rx_addr P, rx_addr ret) {
    const uint32_t a[4] = {S, M, index, P};
    const rx_addr s = fx_push(4, a, ret);
    const uint32_t r = rx_fx_spawn_build2(mgr, S, M, index, P);
    fx_pop(s);
    return r;
}
/* the manager's SpawnParticle (vf1: owner block, template) */
static void spawn_call(rx_addr mgr, rx_addr owner, rx_addr P, rx_addr ret) {
    const rx_addr fn = U32(U32(mgr, 0), 4);
    void (*g)(rx_addr, rx_addr, rx_addr) = RX_FN(fn, void (*)(rx_addr, rx_addr, rx_addr));
    const uint32_t a[2] = {owner, P};
    if (!g) { rx_call_original(fn, RX_THISCALL, mgr, 2, a); return; }
    const rx_addr s = fx_push(2, a, ret);
    g(mgr, owner, P);
    fx_pop(s);
}

/* ---- 00695780 (cdecl record {+0 dt, +4 manager, +8 matrix, +0xc interpolate, +0x10 atlas index, +0x14 emitter state S}):
 * unless the global pause [0xc11bc4]: dt clamped to [0.001, 0.0333]; a pending matrix is taken (+0x20 -> the record's matrix at
 * S +0x30, +0x70 = +0x94); with a manager, an asset and the game running: the distance gate (00695650), and when active: n =
 * trunc(rate draw * [0x9c46b8] * dt + carry) / players (+0x98 keeps the fraction), the debug sprintf, then n particles built
 * and spawned (interpolating: the matrix lerped from S +0x30 to the record's per particle, (j + 1) / n); S +0x30 = the record's
 * matrix at the end. Frame: push ebp; and esp, -0x80; sub esp, 0x1f4 (/GS cookie at +0x1f0); ebx / esi / edi: E = A - 0x200:
 * +0x24 dt / j, +0x28 atlas index, +0x2c matrix, +0x30 manager, +0x34 interpolate, +0x38 n (float), +0x3c n, +0x40 the lerped
 * matrix, +0x80 the template, +0x178 the sprintf buffer. */
void rx_fx_emitter_emit(rx_addr rec) {
    fx_frame f; fx_frame_enter(&f, 0x1f4, 128, FX_EBX, FX_ESI, FX_EDI);
    const rx_addr E = f.E, esp0 = E + 12;
    SU32(esp0, 0x1f0, U32(0, 0x009b45e4) ^ esp0);
    const uint32_t interp = U32(rec, 0xc);
    const rx_addr mgr = U32(rec, 4);
    const float dt0 = F32(rec, 0);
    const rx_addr S = U32(rec, 0x14);
    const rx_addr mat = U32(rec, 8);
    SU32(E, 0x34, interp);
    const uint32_t index = U32(rec, 0x10);
    SU32(E, 0x30, mgr); SU32(E, 0x2c, mat); SU32(E, 0x28, index);
    uint32_t esi = mat;
    fx_set_reg(FX_EBX, S); fx_set_reg(FX_ESI, mat);
    if (U32(0, 0x00c11bc4) == 0) {
        float dt = dt0;
        const float hi = rx_bf(0x3d086595u) /* 0x977bb4: 0.0333 */, lo = rx_bf(0x3a83126fu) /* 0x97743c: 0.001 */;
        if (dt >= hi) dt = hi;
        else if (!(lo < dt) && dt == dt) dt = lo;
        SF32(E, 0x24, dt);
        if (U32(S, 0x20) != 0) {
            const uint32_t user = U32(S, 0x94);
            for (uint32_t k = 0; k < 0x40; k += 4) SU32(S + 0x30, k, U32(mat, k));
            esi = mat + 0x40;
            fx_set_reg(FX_ESI, esi); fx_set_reg(FX_EDI, S + 0x70);
            SU32(S, 0x20, 0);
            SU32(S, 0x70, user);
        }
        int go = mgr != 0 && U32(S, 0x14) != 0 && U32(fx_timing(), 0xc) == 0;
        if (go) go = U32(fx_level_world(U32(0, FX_G_GAME), 0x0069583a, esi), 0x20014) == 0;
        if (go) {
            fx_set_reg(FX_EDI, U32(E, 0x30));
            const rx_addr sp = fx_push(0, 0, 0x00695852);
            rx_fx_emitter_gate(S, U32(E, 0x30));
            fx_pop(sp);
            go = U32(S, 0x1c) != 0;
        }
        if (go) {
            const rx_addr holder = U32(S, 0x14);
            SU32(holder, 0x4c, 1);
            const rx_addr T = U32(S, 0x18) * 0x1f0u + U32(U32(holder, 0x44), 0x10);
            const uint32_t s = lcg(U32(0, 0x009cb860));
            SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
            const float g = rx_muls(F32(0, 0x009c46b8), F32(E, 0x24));
            SU32(0, 0x009cb860, s);
            float x = rx_adds(rx_muls(rf(s), F32(T, 0x108)), F32(T, 0x104));
            const rx_addr scene = fx_active_scene();
            x = rx_adds(rx_muls(x, g), F32(S, 0x98));
            const int32_t total = rx_cvtt(x), players = I32(scene, 0x19d0);
            const int32_t n = total / players;
            fx_set_reg(FX_ESI, (uint32_t)n);
            SU32(0, 0x00a94270, U32(0, 0x00a94270) + (uint32_t)n);
            const float fn = (float)n;
            rx_set_sp(E - 4); SU32(E - 4, 0, (uint32_t)n);
            rx_set_sp(E - 8); SU32(E - 8, 0, 0x0095dc24);
            rx_set_sp(E - 0xc); SU32(E - 0xc, 0, E + 0x178);
            SU32(E - 0xc, 0x48, (uint32_t)n);
            SF32(E - 0xc, 0x44, fn);
            SF32(S, 0x98, rx_subs(x, fn));
            {   /* sprintf(buffer, "Emitting particles: %d\n", n) */
                const rx_addr s2 = fx_push(0, 0, 0x00695924);
                crt_sprintf(E + 0x178, 0x0095dc24, E - 4);
                fx_pop(s2);
            }
            rx_set_sp(E);
            if (U32(E, 0x34) != 0) {
                SU32(E, 0x24, 0);
                if (n > 0) {
                    for (;;) {
                        const float t = rx_divs(rx_adds((float)I32(E, 0x24), 1.0f), F32(E, 0x38));
                        fx_set_reg(FX_EDI, U32(E, 0x2c)); fx_set_reg(FX_ESI, E + 0x40);
                        {   /* push ecx (the t slot); push S + 0x30; ESI = E + 0x40, EDI = the record's matrix */
                            const uint32_t a[2] = {S + 0x30, rx_fb(t)};
                            const rx_addr s2 = fx_push(2, a, 0x0069596d);
                            rx_fx_matrix_lerp(S + 0x30, t, E + 0x40, U32(E, 0x2c));
                            fx_pop(s2);
                        }
                        uint32_t r = 0;
                        rx_addr m2 = U32(E, 0x30);
                        if (U32(S, 0x9c) != 0) {
                            if (U32(S, 0x14) != 0) { fx_set_reg(FX_ESI, U32(E, 0x30)); r = build2(U32(E, 0x30), S, E + 0x40, U32(E, 0x28), E + 0x80, 0x0069599b); }
                            else goto next;
                        } else {
                            if (U32(S, 0x14) != 0) { r = build1(U32(E, 0x30), S, U32(E, 0x28), E + 0x80, E + 0x40, 0x006959da); fx_set_reg(FX_ESI, U32(E, 0x30)); }
                            else goto next;
                        }
                        if (r != 0) spawn_call(m2, S + 0x70, E + 0x80, 0x006959f7);
                    next:
                        SU32(E, 0x24, U32(E, 0x24) + 1);
                        if (!(I32(E, 0x24) < I32(E, 0x3c))) break;
                    }
                }
            } else {
                const rx_addr m2 = U32(E, 0x30);
                for (int32_t j = n; j > 0; j--) {
                    fx_set_reg(FX_ESI, (uint32_t)j);
                    uint32_t r = 0;
                    if (U32(S, 0x9c) != 0) {
                        if (U32(S, 0x14) == 0) continue;
                        r = build2(m2, S, U32(E, 0x2c), U32(E, 0x28), E + 0x80, 0x00695a3c);
                    } else {
                        if (U32(S, 0x14) == 0) continue;
                        r = build1(m2, S, U32(E, 0x28), E + 0x80, U32(E, 0x2c), 0x00695a5d);
                    }
                    if (r != 0) spawn_call(m2, S + 0x70, E + 0x80, 0x00695a76);
                }
            }
            const rx_addr h2 = U32(S, 0x14);
            for (uint32_t k = 0; k < 0x40; k += 4) SU32(S + 0x30, k, U32(U32(E, 0x2c), k));
            SU32(h2, 0x4c, 0);
        }
    }
    SU32(esp0, -4, 0x00695aa4);
    phys_security_check_cookie(U32(esp0, 0x1f0) ^ esp0);
    fx_frame_leave(&f);
}
RX_FUNCTION(0x00695780, rx_fx_emitter_emit)

/* ---- 00695ab0 ParticleEmitter::vf0 (thiscall S; stdcall dt, manager, matrix, interpolate, atlas index; RET 0x14): the record
 * {dt, manager, matrix, interpolate, index, S} at the 8-aligned frame base - 0x40, then 00695780. */
void rx_fx_emitter_vf0(rx_addr S, float dt, rx_addr mgr, rx_addr mat, uint32_t interp, uint32_t index) {
    fx_frame f; fx_frame_enter0(&f, 0x40, 8);
    const rx_addr rec = f.E;
    SU32(rec, 4, mgr); SU32(rec, 0xc, interp); SU32(rec, 8, mat);
    SF32(rec, 0, dt); SU32(rec, 0x10, index); SU32(rec, 0x14, S);
    const uint32_t a[1] = {rec};
    const rx_addr s = fx_push(1, a, 0x00695aed);
    rx_fx_emitter_emit(rec);
    fx_pop(s);
    fx_frame_leave(&f);
}
RX_FUNCTION(0x00695ab0, rx_fx_emitter_vf0)

/* ---- 0063c040 AttachableParticleEffect_ParticleEmitter::vf3 (thiscall self; stdcall dt; RET 4): the rider's lens-swipe
 * effect (rider + 0x5b6dc, vtable 0x00958908), the idle gesture's attach (0061eac0). Unbound (+4 == 0): the active scene's
 * particle entity (0xa94264 [scene +0x2cc4]) gives the type list ([[ent + 0xb0] + 0x44] + 0x10, record type index
 * ([self + 0xc]) * 0x1f0) and the manager (ent + 0xc4 when the record's +0x160 byte says additive, else ent + 0xc0) with the
 * holder's busy mark ([[ent + 0xb0] + 0x4c]) raised around the read. Then the attached flag: a first attach (+0x2c == 0)
 * sets +0x30, +0x2c = 1; the effect's node (+0x100) is evaluated (007db890) and the emitter at +0x10 (vtable slot 0) gets
 * (dt, the manager, the node's world matrix, 1, -1). Frame: push ebx / esi / edi. */
void rx_fx_attachable_emitter_vf3(rx_addr self, float dt) {
    fx_frame f; fx_frame_flat(&f, 0, FX_EBX, FX_ESI, FX_EDI);
    fx_set_reg(FX_ESI, self);
    if (U32(self, 4) == 0) {
        const rx_addr eng = U32(0, FX_G_ENGINE);
        const rx_addr ent = U32(0, 0x00a94264 + 4u * U32(U32(eng, 0x1c + 4u * U32(eng, 4)), 0x2cc4));
        const rx_addr holder = U32(ent, 0xb0);
        const rx_addr types = U32(holder, 0x44);
        SU32(holder, 0x4c, 1);
        const rx_addr rec = U32(types, 0x10) + U32(self, 0xc) * 0x1f0u;
        const uint8_t additive = U8(rec, 0x160);
        SU32(holder, 0x4c, 0);
        SU32(self, 4, additive != 0 ? U32(ent, 0xc4) : U32(ent, 0xc0));
    }
    if (U32(self, 0x2c) == 0) SU32(self, 0x30, 1);
    SU32(self, 0x2c, 1);
    const rx_addr em = self + 0x10;
    fx_set_reg(FX_EDI, em);                     /* lea edi, [esi + 0x10] */
    const rx_addr node = U32(self, 0x100);
    fx_set_reg(FX_EBX, node);
    {   /* Node_EvaluateWorld(node, 0); ret 4 */
        const uint32_t a[1] = {0};
        const rx_addr s = fx_push(1, a, 0x0063c0ddu);
        rx_world_node_evaluate_world(node, 0);
        fx_pop(s);
    }
    const float d = rx_bf(fx_x87_bits(rx_fb(dt)));
    const rx_addr mat = U32(U32(node, 0), 0x1c) + 64u * U8(node, 0xb);
    const rx_addr fn = U32(U32(em, 0), 0);
    const uint32_t a[5] = {rx_fb(d), U32(self, 4), mat, 1, 0xffffffffu};
    void (*g)(rx_addr, float, rx_addr, rx_addr, uint32_t, uint32_t) =
        RX_FN(fn, void (*)(rx_addr, float, rx_addr, rx_addr, uint32_t, uint32_t));
    if (g) { const rx_addr s = fx_push(5, a, 0x0063c102); g(em, d, a[1], a[2], 1, 0xffffffffu); fx_pop(s); }
    else rx_call_original(fn, RX_THISCALL, em, 5, a);
    fx_frame_leave(&f);
}
RX_FUNCTION(0x0063c040, rx_fx_attachable_emitter_vf3)

/* ---- 0069b060 VehicleParticleEffectEmitter vf3 (thiscall effect, dt; RET 4): the emitter at +0x10's vf0 (dt through fld /
 * fstp, manager +4, matrix +0xf0, interpolate 1, index -1). Frame: push esi, then the five arguments. */
void rx_fx_vehicle_emitter_vf3(rx_addr self, float dt) {
    const rx_addr sp0 = rx_sp();
    const fx_saved in = fx_save();
    const rx_addr em = self + 0x10, fn = U32(U32(em, 0), 0);
    const float d = rx_bf(fx_x87_bits(rx_fb(dt)));
    const uint32_t a[5] = {rx_fb(d), U32(self, 4), U32(self, 0xf0), 1, 0xffffffffu};
    SU32(sp0 - 4, 0, in.esi);                                  /* push esi */
    fx_set_reg(FX_ESI, U32(self, 0xf0));
    rx_set_sp(sp0 - 4);
    void (*g)(rx_addr, float, rx_addr, rx_addr, uint32_t, uint32_t) =
        RX_FN(fn, void (*)(rx_addr, float, rx_addr, rx_addr, uint32_t, uint32_t));
    if (g) { const rx_addr s = fx_push(5, a, 0x0069b084); g(em, d, a[1], a[2], 1, 0xffffffffu); fx_pop(s); }
    else rx_call_original(fn, RX_THISCALL, em, 5, a);
    rx_set_sp(sp0);
    fx_restore(in);
}
RX_FUNCTION(0x0069b060, rx_fx_vehicle_emitter_vf3)
