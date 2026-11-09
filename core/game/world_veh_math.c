/* world_veh_math.c -- the small math helpers of the vehicle code (0x00401xxx, 0x0041cxxx, 0x0041fxxx): |x|, fmod, 3-vector
 * cross / difference, 4-vector splat / compare, 3x3 (3x4 rows of 16 bytes) matrix transpose / inverse / product, the 4x4
 * transpose and the rotation-matrix -> quaternion conversion. Most take their pointers in registers (LTCG); the convention is
 * stated above each function. core/notes/world.md (veh). */
#include "phys_rigbody.h"
#include "phys_ragdoll.h"
#include "phys_rider.h"
#include "world_veh.h"
#include "world_veh_util.h"
#include "phys_crt.h"

/* 00401000 Float_Abs (xmm0 lane 0 in; xmm0 out): andps with (0x7fffffff, 0, 0, 0) (0x977570, movss): |x|, lanes 1..3 cleared. */
float rx_world_float_abs(float x) { return rx_bf(rx_fb(x) & 0x7fffffffu /* 0x977570 */); }
RX_FUNCTION(0x00401000, rx_world_float_abs)

/* 00401040 Float_Abs_Thunk: push ecx; call Float_Abs; pop ecx. */
float rx_world_float_abs_thunk(float x) { return rx_world_float_abs(x); }
RX_FUNCTION(0x00401040, rx_world_float_abs_thunk)

/* 00401020 Float_Fmod (cdecl a, b; result st(0)): fld a; fld b; _CIfmod (the CRT port; the remainder of two floats is exact
 * and a float). */
float rx_world_float_fmod(float a, float b) {
    return rx_f80_to_f32(crt_ci_fmod(rx_f80_from_f32(a), rx_f80_from_f32(b), CRT_CW_MAIN), 0);
}
RX_FUNCTION(0x00401020, rx_world_float_fmod)

/* 00401630 Vec3_Cross (EAX = a, ECX = c, stack out; cdecl): out = c x a, three floats, products in the original's operand order
 * (a.z * c.y - a.y * c.z, a.x * c.z - c.x * a.z, c.x * a.y - a.x * c.y). */
void rx_world_vec3_cross(rx_addr out, rx_addr c, rx_addr a) {
    const float az = F32(a, 8), cz = F32(c, 8), ay = F32(a, 4), cy = F32(c, 4), ax = F32(a, 0), cx = F32(c, 0);
    const float x = phys_sub(phys_mul(az, cy), phys_mul(ay, cz));
    const float y = phys_sub(phys_mul(ax, cz), phys_mul(cx, az));
    const float z = phys_sub(phys_mul(cx, ay), phys_mul(ax, cy));
    SF32(out, 0, x); SF32(out, 4, y); SF32(out, 8, z);
}
RX_FUNCTION(0x00401630, rx_world_vec3_cross)

/* 00401700 Vec3_Sub (EDX = a, ECX = b, stack out; cdecl; EAX = out): out = a - b, three floats (each written before the next
 * is read, as the original). */
void rx_world_vec3_sub(rx_addr out, rx_addr a, rx_addr b) {
    SF32(out, 0, phys_sub(F32(a, 0), F32(b, 0)));
    SF32(out, 4, phys_sub(F32(a, 4), F32(b, 4)));
    SF32(out, 8, phys_sub(F32(a, 8), F32(b, 8)));
}
/* (00401700 itself is registered by rx_phys: phys_vec3_sub; this is the share-local copy) */

/* 0041c6c0 Vec4_Splat (stdcall out, float s; RET 8; EAX = out): out = (s, s, s, s). */
void rx_world_vec4_splat(rx_addr out, float s) { v4_st(out, v4_splat(s)); }
RX_FUNCTION(0x0041c6c0, rx_world_vec4_splat)

/* 0041c640 Vec4_CmpLe_CR6 (xmm0 = a, xmm1 = b, cdecl flags word; xmm0 = mask): the AltiVec record compare vcmpgefp. as the Xenon
 * code used it: mask = b <= a per lane; the flags word loses bits 4..7, gets 0x80 when every lane holds, 0x20 when none does. */
rx_v4 rx_world_vec4_cmple_cr6(rx_v4 a, rx_v4 b, rx_addr flags) {
    const rx_v4 mask = v4_cmple(b, a);
    int all = 1, none = 1;
    for (int i = 0; i < 4; i++) { if (v4_lb(mask, i) == 0) all = 0; else none = 0; }
    SU32(flags, 0, U32(flags, 0) & 0xffffff0fu);
    if (all) SU32(flags, 0, U32(flags, 0) | 0x80u);
    if (none) SU32(flags, 0, U32(flags, 0) | 0x20u);
    return mask;
}
RX_FUNCTION(0x0041c640, rx_world_vec4_cmple_cr6)

/* 0041f5c0 Matrix3_MulVec3 (ECX = out, EDX = v, cdecl M; EAX = M): out = (M0 . v, M1 . v, M2 . v, M2 . v), each dot the product
 * row * v summed (x + y) + z. */
void rx_world_matrix3_mul_vec3(rx_addr out, rx_addr v, rx_addr M) {
    const rx_v4 x = v4_ld(v);
    const float d0 = phys_dot3(v4_ld(M), x), d1 = phys_dot3(v4_ld(M + 0x10), x), d2 = phys_dot3(v4_ld(M + 0x20), x);
    v4_st(out, v4(d0, d1, d2, d2));
}
RX_FUNCTION(0x0041f5c0, rx_world_matrix3_mul_vec3)

/* 0041f650 Matrix3_Transpose (ECX = src, cdecl dst; EAX = dst): dst = src's three rows, then the transpose built in place in
 * dst; rows (a0 b0 c0 c0), (a1 b1 c1 c1), (a2 b2 c2 c3). src is re-read after dst is written, as the original does (so an
 * in-place call, src == dst, gives the original's result). */
void rx_world_matrix3_transpose(rx_addr dst, rx_addr src) {
    v4_st(dst, v4_ld(src)); v4_st(dst + 0x10, v4_ld(src + 0x10)); v4_st(dst + 0x20, v4_ld(src + 0x20));
    rx_v4 r = v4_ld(dst), s = v4_ld(src + 0x10);
    r = v4(r.v[0], s.v[0], r.v[2], r.v[3]);                 /* (a0 b0 a2 a3) */
    v4_st(dst, r);
    s = v4_ld(src + 0x20);
    v4_st(dst, v4(r.v[0], r.v[1], s.v[0], s.v[0]));          /* (a0 b0 c0 c0) */
    r = v4_ld(dst + 0x10); s = v4_ld(src);
    v4_st(dst + 0x10, v4(s.v[1], r.v[1], r.v[2], r.v[3]));   /* (a1 b1 b2 b3) */
    s = v4_ld(src + 0x20); r = v4_ld(dst + 0x10);
    v4_st(dst + 0x10, v4(r.v[0], r.v[1], s.v[1], s.v[1]));   /* (a1 b1 c1 c1) */
    r = v4_ld(dst + 0x20); s = v4_ld(src);
    v4_st(dst + 0x20, v4(s.v[2], r.v[1], r.v[2], r.v[3]));   /* (a2 c1 c2 c3) */
    r = v4_ld(dst + 0x20); s = v4_ld(src + 0x10);
    v4_st(dst + 0x20, v4(r.v[0], s.v[2], r.v[2], r.v[3]));   /* (a2 b2 c2 c3) */
}
RX_FUNCTION(0x0041f650, rx_world_matrix3_transpose)

/* The inlined 3-vector cross product of the matrix code: (a.yzx * b.zxy) - (a.zxy * b.yzx) with the products' operands in the
 * given order (callers pass the shuffled vectors in the original's order). */
static rx_v4 cross_parts(rx_v4 p, rx_v4 q, rx_v4 r, rx_v4 s) { return v4_sub(v4_mul(p, q), v4_mul(r, s)); }

/* 0041f6e0 Matrix3_Inverse (ECX = dst, cdecl src; EAX = a stack address after): rows a, b, c of src; the cofactor rows b x c,
 * c x a, a x b (w cleared by the xyz mask 0x92db80), det = a . (b x c) summed (x + y) + z, inv = [0xd61650] / det (4 lanes, the
 * writable "one" vector), dst = the rows scaled by inv, then dst = transpose(dst) through a stack buffer (0041f650). */
void rx_world_matrix3_inverse(rx_addr dst, rx_addr src) {
    world_frame f = world_frame_enter(0x50, 0, 1);   /* push ebp; and esp, -16; sub esp, 0x50 */
    const rx_addr tmp = f.esp + 0x10;
    const rx_v4 a = v4_ld(src), b = v4_ld(src + 0x10), c = v4_ld(src + 0x20);
    const rx_v4 c_201 = v4_perm(c, 2, 0, 1, 3), c_120 = v4_perm(c, 1, 2, 0, 3);
    const rx_v4 b_120 = v4_perm(b, 1, 2, 0, 3), b_201 = v4_perm(b, 2, 0, 1, 3);
    const rx_v4 a_201 = v4_perm(a, 2, 0, 1, 3), a_120 = v4_perm(a, 1, 2, 0, 3);
    const rx_v4 bc = v4_xyz(cross_parts(c_201, b_120, c_120, b_201));     /* (c2 b1 - c1 b2, ...) = b x c */
    const float det = phys_dot3(a, bc);
    const rx_v4 inv = v4_div(v4_ld(0x00d61650), v4_splat(det));
    v4_st(dst, v4_mul(inv, bc));
    const rx_v4 ca = v4_xyz(cross_parts(a_201, c_120, a_120, v4_perm(c, 2, 0, 1, 3)));   /* c x a */
    v4_st(dst + 0x10, v4_mul(ca, inv));
    const rx_v4 ab = v4_xyz(cross_parts(b_201, a_120, b_120, a_201));     /* a x b */
    v4_st(dst + 0x20, v4_mul(ab, inv));
    WORLD_CALL_SP(4, rx_world_matrix3_transpose(tmp, dst));
    v4_st(dst, v4_ld(tmp)); v4_st(dst + 0x10, v4_ld(tmp + 0x10)); v4_st(dst + 0x20, v4_ld(tmp + 0x20));
    world_frame_leave(&f);
}
RX_FUNCTION(0x0041f6e0, rx_world_matrix3_inverse)

/* 00401760 (EAX = src, EDX = dst; ECX = 0 after): the 4x4 transpose of 16 floats through a stack copy (rep movsd). */
void rx_world_matrix4_transpose(rx_addr dst, rx_addr src) {
    uint32_t t[16];
    for (int col = 0; col < 4; col++)
        for (int row = 0; row < 4; row++) t[4 * col + row] = U32(src, 16u * (uint32_t)row + 4u * (uint32_t)col);
    for (int i = 0; i < 16; i++) SU32(dst, 4u * (uint32_t)i, t[i]);
}
RX_FUNCTION(0x00401760, rx_world_matrix4_transpose)

/* 0041ad80 Cursor_Advance4 (ECX = cursor; EAX = old value, EDX = new): the va_arg-style advance, *cursor += 4. */
uint32_t rx_world_cursor_advance4(rx_addr cursor) {
    const uint32_t old = U32(cursor, 0);
    SU32(cursor, 0, old + 4);
    return old;
}
RX_FUNCTION(0x0041ad80, rx_world_cursor_advance4)

/* ---- 00401210 Matrix3_ToQuaternion (cdecl M; xmm0 = q): the branch-free Xenon conversion. The four candidate traces
 *   T = (m00 - m11 - m22, m11 - m00 - m22, m22 - m00 - m11, m00 + m11 + m22) (each lane ((m11 * s1) + (m00 * s0)) + m22 * s2),
 * r = 1 / sqrt(1 + T), s = r * (1 + T) (= sqrt; 1 + T itself where r is a NaN), the component with the largest trace k chosen by
 * compares (lane w when T.w >= 2^-23, else the largest of x, y, z with ties to the earlier), and the other three components
 * from W = (V * sign_k + U) * (0.5 r_k) with U = (m01, m12, m20, m03), V = (m10, m21, m02, m03).
 * The original selects with compare masks and the AltiVec byte permutes emulated by byte loops through its frame (vperm with
 * xor 3 for the byte order). Its 0xc0-byte frame is dead after the call, but later code reads it back as never-written lanes
 * (006b0530's goal w and the rider record w lane, core/notes/anim.md; the rider twist stage 00636990 calls it three times on the
 * animation worker's stack: rx_loop's Engine::Update residue, known-differences 1), so the function is transliterated as the
 * original executes it: every vector and byte it stores in its frame at the original's addresses, the result read back from the
 * last permute (it equals the selection table of the decompile). Entered at its return slot (rx_sp()); the saved EBP at
 * entry - 4 is the caller's (rx_world_matrix3_to_quaternion_to writes it). */
static rx_v4 q_c(uint32_t a) { return v4_ld(a); }                                  /* a .rdata constant */
static rx_v4 q_eqd(rx_v4 a, rx_v4 b) {                                             /* pcmpeqd */
    return v4_mask(v4_lb(a, 0) == v4_lb(b, 0), v4_lb(a, 1) == v4_lb(b, 1), v4_lb(a, 2) == v4_lb(b, 2), v4_lb(a, 3) == v4_lb(b, 3));
}
static void q_perm(rx_addr S, uint32_t dst, uint32_t src, uint32_t ctl) {         /* the vperm loop: dst[i] = src[(ctl[i] ^ 3) & 0x1f] */
    for (uint32_t i = 0; i < 16; i++) SU8(S + dst, i, U8(S + src, (U8(S + ctl, i) ^ 3u) & 0x1fu));
}
rx_v4 rx_world_matrix3_to_quaternion(rx_addr M) {
    const rx_addr A = (rx_sp() - 4u) & ~15u, S = A - 0xc0;   /* push ebp; mov ebp, esp; and esp, -16; sub esp, 0xc0 */
    SU32(S, 0xbc, U32(0, 0x009b45e4) ^ S);                    /* the stack cookie */
    rx_v4 x0 = v4_ld(M + 0x10), x1 = v4_ld(M), x3 = q_c(0x0092dc90), x2 = v4_ld(M + 0x20), x4 = q_c(0x0092dc40), x5, x6, x7;
    v4_st(S, x1); v4_st(S + 0x20, x0);
    x0 = v4_mul(v4_shuf(x0, x0, 0x55), x3);
    x3 = v4_mul(v4_shuf(x1, x1, 0), q_c(0x0092dc80));
    x1 = v4_shuf(x2, x2, 0xaa);
    v4_st(S + 0x30, x2);
    x1 = v4_mul(x1, q_c(0x0092dca0));
    x0 = v4_add(v4_add(x0, x3), x1);                         /* T */
    x3 = q_c(0x0092dba0);                                     /* 1 */
    x2 = v4_add(x3, x0);                                      /* 1 + T */
    x1 = v4_sqrt(x2);
    x3 = v4_div(x3, x1);                                      /* r */
    x1 = q_eqd(v4_and(x3, x4), q_c(0x0092dbb0));
    x4 = q_c(0x0092dc20);
    x5 = q_eqd(v4_and(x3, x4), x4);
    x1 = v4_andn(x1, x5);                                     /* r is a NaN */
    x4 = x1; x1 = v4_and(x1, x2);
    x5 = v4_mul(x3, x2);
    x4 = v4_or(v4_andn(x4, x5), x1);                          /* s */
    x1 = q_c(0x0092dbd0);                                     /* 0.5 */
    x4 = v4_mul(x4, x1);
    x1 = v4_mul(x1, x3);
    v4_st(S + 0x70, x1);
    x1 = q_c(0x0092dce0);
    v4_st(S + 0x80, x4); v4_st(S + 0x90, x0); v4_st(S + 0xa0, x0); v4_st(S + 0x40, x1);
    q_perm(S, 0x10, 0x90, 0x40);
    v4_st(S + 0x50, x0); v4_st(S + 0x60, q_c(0x0092dc10)); v4_st(S + 0x40, q_c(0x0092dcf0));
    q_perm(S, 0x90, 0x50, 0x40);
    x0 = v4_ld(S + 0x10);
    x1 = v4_cmple(v4_ld(S + 0x90), x0);
    x7 = q_c(0x0092dd10);
    x3 = v4_shuf(x1, x1, 0xaa);
    x5 = v4_or(v4_andn(x3, q_c(0x0092dd20)), v4_and(x3, x7));
    x6 = v4_and(x3, q_c(0x0092dcc0));
    x7 = q_c(0x0092dd80);
    x0 = v4_shuf(x1, x1, 0);
    x4 = v4_or(v4_andn(x0, x5), v4_and(q_c(0x0092dd20), x0));
    x5 = v4_or(v4_andn(x3, q_c(0x0092dcd0)), x6);
    v4_st(S + 0x10, x0);
    x6 = v4_or(v4_andn(v4_ld(S + 0x10), x5), v4_and(q_c(0x0092dcd0), x0));
    v4_st(S + 0x40, x6);
    x6 = v4_andn(x3, q_c(0x0092dd90));
    x3 = v4_and(x3, x7);
    x7 = q_c(0x0092dcb0);
    x6 = v4_or(x6, x3);
    x3 = v4_and(q_c(0x0092dd90), x0);
    x2 = v4_shuf(x1, x1, 0x55);
    x5 = v4_or(v4_andn(x0, x6), x3);
    x1 = v4_shuf(x1, x1, 0xff);
    x6 = v4_or(v4_andn(x2, v4_ld(S + 0x40)), v4_and(x2, x7));
    x6 = v4_and(x6, x0);
    x6 = v4_or(x6, v4_andn(x0, v4_ld(S + 0x40)));
    v4_st(S + 0x10, x1);
    x3 = v4_or(v4_andn(v4_ld(S + 0x10), x6), v4_and(q_c(0x0092dbc0), x1));
    x6 = v4_or(v4_and(q_c(0x0092dd70), x2), v4_andn(x2, x5));
    v4_st(S + 0x40, x3);
    x6 = v4_and(x6, x0);
    x7 = v4_andn(x0, x5);
    x6 = v4_or(x6, x7);
    x5 = v4_and(q_c(0x0092dda0), x1);
    x3 = v4_or(v4_andn(x1, x6), x5);
    x5 = v4_ld(S + 0x70);
    v4_st(S + 0x50, x5); v4_st(S + 0x60, x5);
    x6 = v4_or(v4_andn(x2, x4), v4_and(x2, q_c(0x0092dd00)));
    x6 = v4_and(x6, x0);
    x6 = v4_or(x6, v4_andn(x0, x4));
    x5 = v4_or(v4_andn(x1, x6), v4_and(q_c(0x0092dd30), x1));
    v4_st(S + 0x70, x5);
    q_perm(S, 0x10, 0x50, 0x70);                              /* 0.5 r_k */
    v4_st(S + 0x50, v4_ld(S + 0x20)); v4_st(S + 0x60, v4_ld(S + 0x30)); v4_st(S + 0x30, q_c(0x0092dd40));
    q_perm(S, 0x20, 0x50, 0x30);
    x1 = v4_ld(S); x0 = v4_ld(S + 0x20);
    v4_st(S + 0x50, x1); v4_st(S + 0x60, x0); v4_st(S, q_c(0x0092dd50));
    q_perm(S, 0x20, 0x50, 0x00);                              /* U */
    v4_st(S + 0x60, x0); v4_st(S + 0x50, x1); v4_st(S, q_c(0x0092dd60));
    q_perm(S, 0x30, 0x50, 0x00);                              /* V */
    v4_st(S + 0x50, v4_ld(S + 0x80));
    x0 = v4_mul(v4_add(v4_mul(v4_ld(S + 0x30), v4_ld(S + 0x40)), v4_ld(S + 0x20)), v4_ld(S + 0x10));   /* W */
    v4_st(S + 0x60, x0);
    v4_st(S + 0x80, x3);
    q_perm(S, 0x00, 0x50, 0x80);
    SU32(S, -4, 0x0040160d);                                  /* call __security_check_cookie */
    return v4_ld(S);
}
RX_FUNCTION(0x00401210, rx_world_matrix3_to_quaternion)

/* 00401740 Matrix3_ToQuaternionToEsi (cdecl M, ESI = out): out = Matrix3_ToQuaternion(M). Frame: push ebp; mov ebp, esp; and
 * esp, -16; push M; call (return 0040174f); the callee's push ebp saves this frame's EBP. Entered at its return slot. The _fr form
 * also writes the caller's EBP that this frame saves (its callers that keep a register model). */
void rx_world_matrix3_to_quaternion_to(rx_addr out, rx_addr M) {
    const rx_addr E = rx_sp(), A = (E - 4u) & ~15u, sp = rx_sp();
    SU32(A, -4, M); SU32(A, -8, 0x0040174f);
    SU32(A, -0xc, E - 4);                                     /* 00401210's push ebp */
    rx_set_sp(A - 8);
    const rx_v4 q = rx_world_matrix3_to_quaternion(M);
    rx_set_sp(sp);
    v4_st(out, q);
}
RX_FUNCTION(0x00401740, rx_world_matrix3_to_quaternion_to)
void rx_world_matrix3_to_quaternion_to_fr(rx_addr out, rx_addr M, uint32_t ebp) {
    SU32(rx_sp(), -4, ebp);
    rx_world_matrix3_to_quaternion_to(out, M);
}

/* 0041f430 (ESI = A, ECX = B, EDX = out): out = A * B (rows of A times the columns of B: B transposed into a stack buffer by
 * 0041f650, each entry A_i . Bt_j summed (x + y) + z; lane 3 repeats lane 2). Row i of A is read after row i - 1 of out is
 * written (the original's order, for out == A). */
void rx_world_matrix3_mul(rx_addr out, rx_addr A, rx_addr B) {
    world_frame f = world_frame_enter(0x40, 0, 1);
    const rx_addr bt = f.esp;
    WORLD_CALL_SP(4, rx_world_matrix3_transpose(bt, B));
    const rx_v4 c0 = v4_ld(bt), c1 = v4_ld(bt + 0x10), c2 = v4_ld(bt + 0x20);
    for (uint32_t i = 0; i < 3; i++) {
        const rx_v4 a = v4_ld(A + 0x10 * i);
        const float d0 = phys_dot3(a, c0), d1 = phys_dot3(a, c1), d2 = phys_dot3(a, c2);
        v4_st(out + 0x10 * i, v4(d0, d1, d2, d2));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0041f430, rx_world_matrix3_mul)

/* 0041f800 (cdecl A, B, C, D, out11, out12, out21, out22): the inverse of the block matrix [[A, B], [C, D]] of 3x3 blocks
 * through the Schur complement: Ai = A^-1, S = C Ai B - D, Si = S^-1; out12 = Ai B Si, out21 = Si C Ai, out11 = Ai - Ai B Si C Ai,
 * out22 = -Si. Products by 0041f430, inverses by 0041f6e0, the temporaries in the original's frame. */
void rx_world_block_inverse(rx_addr A, rx_addr B, rx_addr C, rx_addr D, rx_addr out11, rx_addr out12, rx_addr out21,
                            rx_addr out22) {
    world_frame f = world_frame_enter(0x128, 2, 1);   /* push ebp; and esp, -16; sub esp, 0x128; push esi; push edi */
    const rx_addr sp = f.esp;
    const rx_addr Si = sp + 0x30, X = sp + 0x60, Ai = sp + 0x90, AiB = sp + 0xc0, CAi = sp + 0xf0;
    WORLD_CALL_SP(4, rx_world_matrix3_inverse(Ai, A));
    WORLD_CALL_SP(0, rx_world_matrix3_mul(CAi, C, Ai));
    WORLD_CALL_SP(0, rx_world_matrix3_mul(AiB, Ai, B));
    WORLD_CALL_SP(0, rx_world_matrix3_mul(Si, C, AiB));                       /* C Ai B, then S = that - D */
    for (uint32_t r = 0; r < 3; r++) v4_st(X + 0x10 * r, v4_sub(v4_ld(Si + 0x10 * r), v4_ld(D + 0x10 * r)));
    WORLD_CALL_SP(4, rx_world_matrix3_inverse(Si, X));
    WORLD_CALL_SP(0, rx_world_matrix3_mul(out12, AiB, Si));
    WORLD_CALL_SP(0, rx_world_matrix3_mul(out21, Si, CAi));
    WORLD_CALL_SP(0, rx_world_matrix3_mul(X, AiB, out21));
    for (uint32_t r = 0; r < 3; r++) v4_st(out11 + 0x10 * r, v4_sub(v4_ld(Ai + 0x10 * r), v4_ld(X + 0x10 * r)));
    for (uint32_t r = 0; r < 3; r++) v4_st(out22 + 0x10 * r, v4_ld(Si + 0x10 * r));
    for (uint32_t r = 0; r < 3; r++) v4_st(out22 + 0x10 * r, v4_sub(v4_zero(), v4_ld(out22 + 0x10 * r)));
    world_frame_leave(&f);
}
RX_FUNCTION(0x0041f800, rx_world_block_inverse)

/* 0041f390 (ECX = v, stdcall out; RET 4): the cross-product matrix of v, built in out from the writable zero vector 0xd61670
 * (rows (0, -z, y, y), (z, 0, -x, -x), (-y, x, 0, 0); n = 0 - v); v is re-read after out is written, as the original. */
void rx_world_matrix3_skew(rx_addr out, rx_addr v) {
    const rx_v4 n = v4_sub(v4_zero(), v4_ld(v));
    for (uint32_t r = 0; r < 3; r++) v4_st(out + 0x10 * r, v4_ld(WORLD_ZERO4));
    rx_v4 row = v4_ld(out);
    row = v4(row.v[0], n.v[2], row.v[2], row.v[3]);
    v4_st(out, row);
    rx_v4 s = v4_ld(v);
    v4_st(out, v4(row.v[0], row.v[1], s.v[1], s.v[1]));
    s = v4_ld(v); row = v4_ld(out + 0x10);
    v4_st(out + 0x10, v4(s.v[2], row.v[1], row.v[2], row.v[3]));
    row = v4_ld(out + 0x10);
    v4_st(out + 0x10, v4(row.v[0], row.v[1], n.v[0], n.v[0]));
    row = v4_ld(out + 0x20);
    v4_st(out + 0x20, v4(n.v[1], row.v[1], row.v[2], row.v[3]));
    row = v4_ld(out + 0x20); s = v4_ld(v);
    v4_st(out + 0x20, v4(row.v[0], s.v[0], row.v[2], row.v[3]));
}
RX_FUNCTION(0x0041f390, rx_world_matrix3_skew)

/* 0041f160 (ECX = out, stdcall q; RET 4): the rotation matrix of q = (x, y, z, w) in double-precision intermediates (each
 * product / sum rounded to float where the original stores it): x2 = x * -2 (0x977698), a = -0 - x2 (= 2x) ..., entries
 * 1 - (..) with 1.0 (0x977638); lane 3 of each row repeats lane 2. */
void rx_world_quat_to_matrix3(rx_addr out, rx_addr q) {
    const double K = -2.0 /* 0x977698 */, one = 1.0 /* 0x977638 */;
    const float x = F32(q, 0), y = F32(q, 4), z = F32(q, 8), w = F32(q, 0xc);
    const float x2 = (float)((double)x * K), y2 = (float)((double)y * K), z2 = (float)((double)z * K);
    const float a = -0.0f /* 0x9774a8 */ - x2, b = -0.0f - y2, c = -0.0f - z2;
    const float ax = (float)((double)a * x), cz = (float)((double)c * z), byy = (float)((double)b * y);
    const float wx = (float)((double)w * x2), wy = (float)((double)w * y2), wz = (float)((double)w * z2);
    const float ay = (float)((double)a * y), az = (float)((double)a * z), bz = (float)((double)b * z);
    const float r00 = (float)(one - ((double)cz + byy)), r01 = (float)((double)ay + wz), r02 = (float)((double)az - wy);
    v4_st(out, v4(r00, r01, r02, r02));
    const float r10 = (float)((double)ay - wz), r11 = (float)(one - ((double)cz + ax)), r12 = (float)((double)bz + wx);
    v4_st(out + 0x10, v4(r10, r11, r12, r12));
    const float r20 = (float)((double)az + wy), r21 = (float)((double)bz - wx), r22 = (float)(one - ((double)byy + ax));
    v4_st(out + 0x20, v4(r20, r21, r22, r22));
}
RX_FUNCTION(0x0041f160, rx_world_quat_to_matrix3)

/* 0041feb0 (ESI = skeleton, EDX = bone, EDI = out): a bone's point in world space: the bone (0x170-byte records from +0x1b30)
 * has its body index at +0x1b50 (record + 0x20), its local point at +0x1b60; the body's orientation is the quaternion at
 * skeleton +0xb00 + 0x70 * body, its position at skeleton + 0x80 * (body + 1). out = the point rotated (row vector times
 * R(q), 0041f... veh_quat_rows) plus the position: ((vx m00 + vy m10) + vz m20 + px, (vy m11 + vx m01) + vz m21 + py, (vy m12 + vx m02) + vz m22 + pz). */
void rx_world_bone_point_world(rx_addr out, rx_addr skel, uint32_t bone) {
    const uint32_t body = U32(skel, (bone + 0x13) * 0x170);
    const rx_addr q = skel + 0xb00 + body * 0x70, pos = skel + (body + 1) * 0x80;
    const veh_rot R = veh_quat_rows(F32(q, 0), F32(q, 4), F32(q, 8), F32(q, 0xc));
    const rx_addr v = skel + 0x1b60 + bone * 0x170;
    const float vx = F32(v, 0), vy = F32(v, 4), vz = F32(v, 8);
    const float ox = phys_add(phys_add(phys_add(phys_mul(vx, R.m[0][0]), phys_mul(vy, R.m[1][0])), phys_mul(vz, R.m[2][0])), F32(pos, 0));
    const float oy = phys_add(phys_add(phys_add(phys_mul(vy, R.m[1][1]), phys_mul(vx, R.m[0][1])), phys_mul(vz, R.m[2][1])), F32(pos, 4));
    const float oz = phys_add(phys_add(phys_add(phys_mul(vy, R.m[1][2]), phys_mul(vx, R.m[0][2])), phys_mul(vz, R.m[2][2])), F32(pos, 8));
    SF32(out, 0, ox); SF32(out, 4, oy); SF32(out, 8, oz);
}
RX_FUNCTION(0x0041feb0, rx_world_bone_point_world)

/* 0041f030 (ESI = skeleton): the bones' world orientations from the root out: for the bones of the order list (+0x37f0,
 * count +0x1b20) from the last to the first, body(bone) = normalize(q(parent) * q(local)) where the bone record (0x170 bytes at
 * +0x1b30 + 0x170 * k) holds the parent body +0, its own body +0x20, the local point +0x30 and the local quaternion +0xb0
 * (orientations at skeleton +0xb00 + 0x70 * body); then 00420770 and 00423b80 (stdcall skeleton). */
void rx_world_skeleton_orient(rx_addr skel) {
    world_frame f = world_frame_flat(2, 0);   /* push ebx; push edi */
    for (int32_t i = I32(skel, 0x1b20) - 1; i > -1; i--) {
        const uint32_t k = U32(skel, 0x37f0 + 4u * (uint32_t)i);
        const rx_addr rec = skel + k * 0x170;
        const rx_v4 parent = v4_ld(skel + 0xb00 + U32(rec, 0x1b30) * 0x70);
        const rx_v4 q = veh_quat_mul(parent, v4_ld(rec + 0x1be0));
        v4_st(skel + 0xb00 + U32(rec, 0x1b50) * 0x70, veh_normalize4(q));
    }
    {
        const rx_regs caller = world_ctx_regs(&WCTX(.esi = skel));
        uint32_t ecx = 0, edx = 0;
        WORLD_CALL_SP(4, phys_rider_joint_limits(skel, &caller));
        WORLD_CALL_SP(4, phys_ragdoll_fix_positions(skel, &ecx, &edx));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0041f030, rx_world_skeleton_orient)

/* 0041fe60 (EDX = skeleton, EDI = v, stdcall arg; RET 4): every body (count +0x7c) gets +0x90 + 0x80 k = *v and its angular
 * velocity +0xb10 + 0x70 k = the zero vector; then 00422450 (EDX = skeleton, ESI = arg). EDX is the skeleton after. */
void rx_world_skeleton_set_velocity(rx_addr skel, rx_addr v, rx_addr arg) {
    world_frame f = world_frame_enter(0xc, 1, 1);
    for (int32_t k = 0; k < I32(skel, 0x7c); k++) {
        v4_st(skel + 0x90 + 0x80u * (uint32_t)k, v4_ld(v));
        v4_st(skel + 0xb10 + 0x70u * (uint32_t)k, v4_ld(WORLD_ZERO4));
    }
    WORLD_CALL_SP(0, phys_rigbody_add_world_angular_velocity(skel, arg));
    world_frame_leave(&f);
}
RX_FUNCTION(0x0041fe60, rx_world_skeleton_set_velocity)

/* ---- 0041e080 helpers: the inlined float cosine of the vehicle code: |x| reduced by 2 pi (truncation through an int),
 *      t = pi - r, the polynomial ((c5 t^2 + c4) t^4 + (c3 t^2 + c2)) t^4 + (c1 t^2 + c0) (0x935d3c ..; it is -cos(x)) ---- */
static float trig_t2(float x) {
    const float ax = rx_bf(rx_fb(x) & 0x7fffffffu);   /* Float_Abs 00401000 */
    const float k = (float)rx_cvtt(phys_mul(ax, 0.159154937f /* 0x9774a0 */));
    const float t = phys_sub(3.14159274f /* 0x977498 */, phys_sub(ax, phys_mul(k, 6.28318548f /* 0x97749c */)));
    return phys_mul(t, t);
}
static float trig_poly(float t2) {
    const float t4 = phys_mul(t2, t2);
    float a = phys_mul(phys_add(phys_mul(t2, rx_bf(0xb46beed0u)), rx_bf(0x37cb076bu)), t4);
    a = phys_mul(phys_add(a, phys_add(phys_mul(t2, rx_bf(0xbab5a662u)), rx_bf(0x3d2aa372u))), t4);
    return phys_add(a, phys_add(phys_mul(t2, rx_bf(0xbeffff3du)), rx_bf(0x3f7ffff3u)));
}
static float trig_cos(float x) { return phys_sub(-0.0f, trig_poly(trig_t2(x))); }
static float trig_sin(float x) { return trig_poly(trig_t2(phys_add(x, 1.57079637f /* 0x977508 */))); }

/* the spline segment of a parameter: round(t - 0.5) as the x87 code does it (copysign (0.5, d) + d, _ftol2_sse; d = t -
 * 0.49999999999 in double), through a float (cvtsi2ss / cvttss2si) */
static int32_t spline_segment(float t) {
    const double d = (double)t - 0.49999999999 /* 0x977558 */;
    const int32_t k = phys_ftol2_sse(rx_f80_from_f64(crt_copysign(0.5 /* 0x9774e8 */, d) + d), 53);
    return rx_cvtt((float)k);
}
/* the cubic of a segment k at u (double products, rounded to float): ((c1 u + c0) + (c2 u) u) + ((c3 u) u) u with
 * c0..c2 at spline + 16 k + base and c3 at spline + 16 (k + c3row) */
static float spline_cubic(rx_addr sp, int32_t k, uint32_t base, uint32_t c3row, float u) {
    const rx_addr e = sp + 16u * (uint32_t)k;
    const double U = u;
    return (float)((((double)F32(e, base + 4) * U + (double)F32(e, base)) + ((double)F32(e, base + 8) * U) * U) +
                   (((double)F32(sp, 16u * ((uint32_t)k + c3row)) * U) * U) * U);
}

/* 0041e080 (ESI = the track spline: 4 cubic segments of two angles, coefficients at +0xe4 / +0x124 (16-byte rows), cubic
 * terms in rows 0xf / 0x13; stdcall float* a1, float* a2; RET 8): the spline parameter nearest to the direction of the
 * angles (*a1, *a2) on the unit sphere: 16 samples (t = i / 4) by the squared chord distance (double precision), then up to
 * 5 Newton steps on the distance's derivative (abandoned for a segment outside 0..3 or a step above 0.5; t wrapped into
 * 0..4 by 3.98), and the angles at the result written back. */
void rx_world_spline_nearest_angles(rx_addr sp, rx_addr a1, rx_addr a2) {
    world_frame f = world_frame_flat(3, 0x80);
    const float v2 = F32(a2, 0);
    const float c2 = trig_cos(v2), s2 = trig_sin(v2);
    const float v1 = F32(a1, 0);
    const float c1 = (float)((double)trig_cos(v1) * (double)s2), s1 = (float)((double)trig_sin(v1) * (double)s2);
    const float n2 = (float)((((double)c2 * c2 + 1.0) + (double)c1 * c1) + (double)s1 * s1);
    const double c2x2 = (double)c2 * 2.0 /* 0x9774f8 */;
    float best = 100.0f /* 0x93005c */, best_t = F32(f.esp, 0x34);   /* (uninitialised in the original when no sample wins) */
    for (int32_t i = 0; i < 16; i++) {
        const float u = (float)((double)(float)i * 0.25 /* 0x977868 */);
        const int32_t k = spline_segment(u);
        const float t = (float)((double)u - (double)(float)k);
        const float A = spline_cubic(sp, k, 0xe4, 0xf, t), B = spline_cubic(sp, k, 0x124, 0x13, t);
        const float cA = trig_cos(A), cB = trig_cos(B), sA = trig_sin(A), sB = trig_sin(B);
        const float err = (float)(((double)n2 - (double)cA * c2x2) -
                                  ((double)cB * (double)c1 + (double)sB * (double)s1) * ((double)sA * 2.0));
        if ((double)best > (double)err) { best = err; best_t = u; }
    }
    SF32(f.esp, 0x34, best_t);
    float t = best_t;
    for (int it = 0; it < 5; it++) {
        const int32_t k = spline_segment(t);
        if (k > 3 || k < 0) { t = best_t; break; }
        const float kf = (float)k;
        const float u = (float)((double)t - (double)kf);
        const rx_addr e = sp + 16u * (uint32_t)k;
        const float a3 = F32(sp, 16u * ((uint32_t)k + 0xf)), b3 = F32(sp, 16u * ((uint32_t)k + 0x13));
        const float A = spline_cubic(sp, k, 0xe4, 0xf, u), B = spline_cubic(sp, k, 0x124, 0x13, u);
        const double U = u;
        const double a2x2 = (double)F32(e, 0xec) * 2.0, b2x2 = (double)F32(e, 0x12c) * 2.0;
        const float dA = (float)((((double)a3 * 3.0 /* 0x977500 */) * U) * U + (U * a2x2 + (double)F32(e, 0xe8)));
        const float dB = (float)((((double)b3 * 3.0) * U) * U + (U * b2x2 + (double)F32(e, 0x128)));
        const float cA = trig_cos(A), cB = trig_cos(B), sA = trig_sin(A), sB = trig_sin(B);
        const float K1 = (float)((double)sB * (double)s1 + (double)cB * (double)c1);
        const float K2 = (float)((double)sA * (double)c2 - (double)K1 * (double)cA);
        const double cBs1 = (double)cB * (double)s1;
        const float K3 = (float)((double)sB * (double)c1 - cBs1);
        const float T1 = (float)((double)dB * cBs1), T2 = (float)(((double)dB * (double)c1) * (double)sB);
        const float num = (float)(((double)K2 * (double)dA - ((double)T1 - (double)T2) * (double)sA) * 2.0);
        const float ddA = (float)((((double)a3 * 6.0 /* 0x977668 */) * U) + a2x2);
        const float ddB = (float)((((double)b3 * 6.0) * U) + b2x2);
        double den = (double)ddA * (double)K2 + ((((double)cA * 2.0) * (double)dA) * (double)dB) * (double)K3;
        den += ((double)ddB * (double)K3 + ((double)dB * (double)dB) * (double)K1) * (double)sA;
        den += ((double)K1 * (double)sA + (double)cA * (double)c2) * ((double)dA * (double)dA);
        const float denf = (float)(den * 2.0);
        const float step = (float)((double)num / (double)denf);
        if (rx_bf(rx_fb(step) & 0x7fffffffu) > 0.5f /* 0x9322cc */) { t = best_t; break; }
        t = (float)((double)(float)((double)u - (double)step) + (double)kf);
        if (t > 4.0f /* 0x9322c0 */) t = (float)((double)t - 3.9800000190734863 /* 0x977a40 */);
        else if (0.0f > t) t = (float)((double)t + 3.9800000190734863);
    }
    {
        const int32_t k = spline_segment(t);
        const float u = (float)((double)t - (double)(float)k);
        SF32(a2, 0, spline_cubic(sp, k, 0xe4, 0xf, u));
        SF32(a1, 0, spline_cubic(sp, k, 0x124, 0x13, u));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0041e080, rx_world_spline_nearest_angles)
