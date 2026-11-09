/* phys_mech_matrix.c -- the virtual methods of the RavenPhysics matrix / vector objects (see phys_mech_matrix.h for the layout and
 * the slot numbering). Each function is one original vfunc, written row by row in the original's store order (callers may pass
 * objects that alias). Vec4 constants that the original reads from writable globals are read from memory every time. */
#include "phys_mech_matrix.h"
#include "phys_vec.h"
#include "phys_mech_kernel.h"

#define G_ZERO3   0x00d61670u   /* runtime vec4 the 3x3 / 6-vector clears copy */
#define G_ZERO4   0x00d61660u   /* runtime vec4 the 4x4 clear copies */

/* ---- virtual calls ---- */
static uint32_t vslot_addr(rx_addr obj, uint32_t slot) { return U32(U32(obj, 0), 4u * slot); }

/* The callee runs with ESP as the original's call leaves it: the return address and the stack arguments below the caller's ESP
 * (a registered core function sees rx_sp() = its entry ESP; rx_call_original pushes them itself). */
uint32_t phys_vcall0(rx_addr obj, uint32_t slot) {
    rx_addr fn = vslot_addr(obj, slot);
    uint32_t (*f)(rx_addr) = RX_FN(fn, uint32_t (*)(rx_addr));
    if (!f) return rx_call_original(fn, RX_THISCALL, obj, 0, NULL);
    rx_addr sp = rx_sp(); rx_set_sp(sp - 4);
    uint32_t r = f(obj);
    rx_set_sp(sp);
    return r;
}
uint32_t phys_vcall1(rx_addr obj, uint32_t slot, uint32_t a) {
    rx_addr fn = vslot_addr(obj, slot);
    uint32_t (*f)(rx_addr, uint32_t) = RX_FN(fn, uint32_t (*)(rx_addr, uint32_t));
    uint32_t args[1] = {a};
    if (!f) return rx_call_original(fn, RX_THISCALL, obj, 1, args);
    rx_addr sp = rx_sp(); rx_set_sp(sp - 8);
    uint32_t r = f(obj, a);
    rx_set_sp(sp);
    return r;
}
uint32_t phys_vcall2(rx_addr obj, uint32_t slot, uint32_t a, uint32_t b) {
    rx_addr fn = vslot_addr(obj, slot);
    uint32_t (*f)(rx_addr, uint32_t, uint32_t) = RX_FN(fn, uint32_t (*)(rx_addr, uint32_t, uint32_t));
    uint32_t args[2] = {a, b};
    if (!f) return rx_call_original(fn, RX_THISCALL, obj, 2, args);
    rx_addr sp = rx_sp(); rx_set_sp(sp - 12);
    uint32_t r = f(obj, a, b);
    rx_set_sp(sp);
    return r;
}

/* ---- row-wise helpers ---- */
/* out.row[r] = this.row[r] - b.row[r] for r < rows, each row loaded and stored before the next (subps this, b) */
static void rows_sub(rx_addr self, rx_addr out, rx_addr b, int rows) {
    for (int r = 0; r < rows; r++) {
        rx_addr o = 0x10u + 0x10u * (uint32_t)r;
        rx_v4 vb = v4_ld(b + o), va = v4_ld(self + o);
        v4_st(out + o, v4_sub(va, vb));
    }
}
/* out.row[r] = b.row[r] + this.row[r] (addps b, this: b is the first operand) */
static void rows_add(rx_addr self, rx_addr out, rx_addr b, int rows) {
    for (int r = 0; r < rows; r++) {
        rx_addr o = 0x10u + 0x10u * (uint32_t)r;
        rx_v4 va = v4_ld(self + o), vb = v4_ld(b + o);
        v4_st(out + o, v4_add(vb, va));
    }
}

/* ---- Matrix3x3 ---- */

/* 00857c30 Matrix3x3::vf1: the three rows = the runtime vec4 0xd61670. Returns this. */
uint32_t phys_matrix3x3_clear(rx_addr self) {
    for (int r = 0; r < 3; r++) v4_st(self + 0x10u + 0x10u * (uint32_t)r, v4_ld(G_ZERO3));
    return self;
}
RX_FUNCTION(0x00857c30, phys_matrix3x3_clear)

/* 00857c90 Matrix3x3::vf2: out = transpose(this). The original packs the fourth lane of each output row from the third input
 * row (out.row0 = (m00 m10 m20 m20), out.row1 = (m01 m11 m21 m21), out.row2 = (m02 m12 m22 W2) with W2 = this.row2.w), in this
 * shuffle order. Returns out. */
uint32_t phys_matrix3x3_transpose(rx_addr self, rx_addr out) {
    rx_v4 x0 = v4_ld(self + 0x10), x2 = v4_ld(self + 0x30), x1 = v4_ld(self + 0x20);
    rx_v4 x4 = v4_shuf(x0, x0, 0xaa);
    rx_v4 x3 = v4_movss(x2, x4);                 /* (m02, m21, m22, W2) */
    x4 = v4_shuf(x1, x0, 0xe1);
    x4 = v4_movss(x4, x0);
    x4 = v4_shuf(x4, x2, 4);                     /* (m00, m10, m20, m20) */
    v4_st(out + 0x10, x4);
    x4 = v4_movss(x1, v4_shuf(x0, x0, 0x55));
    x0 = v4_shuf(x4, x2, 0x54);                  /* (m01, m11, m21, m21) */
    x1 = v4_shuf(x1, x3, 0xe9);
    v4_st(out + 0x20, x0);
    x1 = v4_movss(x1, x3);                       /* (m02, m12, m22, W2) */
    v4_st(out + 0x30, x1);
    return out;
}
RX_FUNCTION(0x00857c90, phys_matrix3x3_transpose)

/* 00857bf0 Matrix3x3::vf3 (also Matrix3x4's): this = copy of src: the row and column counts and the three rows. Returns this. */
uint32_t phys_matrix3x3_copy(rx_addr self, rx_addr src) {
    SU32(self, 4, U32(src, 4));
    SU32(self, 8, U32(src, 8));
    for (int r = 0; r < 3; r++) v4_st(self + 0x10u + 0x10u * (uint32_t)r, v4_ld(src + 0x10u + 0x10u * (uint32_t)r));
    return self;
}
RX_FUNCTION(0x00857bf0, phys_matrix3x3_copy)

/* 00858160 Matrix3x3::vf4: out = this - b (three rows). Returns out. */
uint32_t phys_matrix3x3_sub(rx_addr self, rx_addr out, rx_addr b) { rows_sub(self, out, b, 3); return out; }
RX_FUNCTION(0x00858160, phys_matrix3x3_sub)

/* 008581a0 Matrix3x3::vf5: out = b + this (three rows). Returns out. */
uint32_t phys_matrix3x3_add(rx_addr self, rx_addr out, rx_addr b) { rows_add(self, out, b, 3); return out; }
RX_FUNCTION(0x008581a0, phys_matrix3x3_add)

/* 008581e0 Matrix3x3::vf6: this = 0 - this (three rows). Returns this. */
uint32_t phys_matrix3x3_negate(rx_addr self) {
    for (int r = 0; r < 3; r++) {
        rx_addr o = self + 0x10u + 0x10u * (uint32_t)r;
        v4_st(o, v4_sub(v4_zero(), v4_ld(o)));
    }
    return self;
}
RX_FUNCTION(0x008581e0, phys_matrix3x3_negate)

/* ---- Matrix3x4 / Matrix4x3 / Matrix4x4 ---- */

/* 00858220 Matrix3x4::vf2: out (4x3) = transpose(this (3x4)). The three rows are copied first, then every output row is fixed
 * up lane by lane through memory in the original's order (the read-backs of out are part of the result). Returns out. */
uint32_t phys_matrix3x4_transpose(rx_addr c, rx_addr a) {
    rx_v4 x0, x1;
    v4_st(a + 0x10, v4_ld(c + 0x10));
    v4_st(a + 0x20, v4_ld(c + 0x20));
    v4_st(a + 0x30, v4_ld(c + 0x30));
    /* out.row0 = (c00 c10 c20 c20) */
    x0 = v4_ld(a + 0x10); x1 = v4_ld(c + 0x20);
    x1 = v4_shuf(x1, x0, 0xe1); x1 = v4_movss(x1, x0); v4_st(a + 0x10, x1);
    x1 = v4_ld(c + 0x30); x0 = v4_ld(a + 0x10); x0 = v4_shuf(x0, x1, 4); v4_st(a + 0x10, x0);
    /* out.row1 = (c01 c11 c21 c21) */
    x0 = v4_ld(c + 0x10); x1 = v4_ld(a + 0x20); x0 = v4_shuf(x0, x0, 0x55); x1 = v4_movss(x1, x0); v4_st(a + 0x20, x1);
    x1 = v4_ld(c + 0x30); x0 = v4_ld(a + 0x20); x0 = v4_shuf(x0, x1, 0x54); v4_st(a + 0x20, x0);
    /* out.row2 = (c02 c12 c22 c23) */
    x0 = v4_ld(c + 0x10); x1 = v4_ld(a + 0x30); x0 = v4_shuf(x0, x0, 0xaa); x1 = v4_movss(x1, x0); v4_st(a + 0x30, x1);
    x0 = v4_ld(a + 0x30); x1 = v4_ld(c + 0x20); x1 = v4_shuf(x1, x0, 0xe9); x1 = v4_movss(x1, x0); v4_st(a + 0x30, x1);
    /* out.row3 = (c03 c13 c23 c23) */
    x0 = v4_ld(c + 0x10); x1 = v4_ld(a + 0x40); x0 = v4_shuf(x0, x0, 0xff); x1 = v4_movss(x1, x0); v4_st(a + 0x40, x1);
    x0 = v4_ld(a + 0x40); x1 = v4_ld(c + 0x20); x1 = v4_shuf(x1, x0, 0xed); x1 = v4_movss(x1, x0); v4_st(a + 0x40, x1);
    x0 = v4_ld(a + 0x40); x1 = v4_ld(c + 0x30); x0 = v4_shuf(x0, x1, 0xf4); v4_st(a + 0x40, x0);
    return a;
}
RX_FUNCTION(0x00858220, phys_matrix3x4_transpose)

/* 00858440 Matrix4x3::vf2: out (3x4) = transpose(this (4x3)), one 32-bit word at a time through memory:
 * out.row[r].lane[k] = this.row[k].lane[r] (r = 0..2 outer, k = 0..3 inner). Returns out. */
uint32_t phys_matrix4x3_transpose(rx_addr self, rx_addr out) {
    for (uint32_t r = 0; r < 3; r++)
        for (uint32_t k = 0; k < 4; k++)
            SU32(out, 0x10 + 0x10 * r + 4 * k, U32(self, 0x10 + 0x10 * k + 4 * r));
    return out;
}
RX_FUNCTION(0x00858440, phys_matrix4x3_transpose)

/* 00858660 Matrix4x3::vf4 (also Matrix4x4's): out = this - b (four rows). Returns out. */
uint32_t phys_matrix4x3_sub(rx_addr self, rx_addr out, rx_addr b) { rows_sub(self, out, b, 4); return out; }
RX_FUNCTION(0x00858660, phys_matrix4x3_sub)

/* 008583c0 Matrix4x3::vf5 (also Matrix4x4's): out = b + this (four rows). Returns out. */
uint32_t phys_matrix4x3_add(rx_addr self, rx_addr out, rx_addr b) { rows_add(self, out, b, 4); return out; }
RX_FUNCTION(0x008583c0, phys_matrix4x3_add)

/* 00858620 Matrix4x4::vf1: the four rows = the runtime vec4 0xd61660. Returns this. */
uint32_t phys_matrix4x4_clear(rx_addr self) {
    for (int r = 0; r < 4; r++) v4_st(self + 0x10u + 0x10u * (uint32_t)r, v4_ld(G_ZERO4));
    return self;
}
RX_FUNCTION(0x00858620, phys_matrix4x4_clear)

/* ---- block matrices (sub-matrix objects at fixed offsets; each call goes through the sub-matrix's own vtable) ---- */

/* 00858a80 Matrix4x6::vf2: out = transpose(this): this+0x10 -> out+0x10, this+0x60 -> out+0x50. Returns the last callee's EAX. */
uint32_t phys_matrix4x6_transpose(rx_addr self, rx_addr out) {
    phys_vcall1(self + 0x10, 2, out + 0x10);
    return phys_vcall1(self + 0x60, 2, out + 0x50);
}
RX_FUNCTION(0x00858a80, phys_matrix4x6_transpose)

/* 00858bd0 Matrix6x4::vf2: out = transpose(this): this+0x10 -> out+0x10, this+0x50 -> out+0x60. Returns the last callee's EAX. */
uint32_t phys_matrix6x4_transpose(rx_addr self, rx_addr out) {
    phys_vcall1(self + 0x10, 2, out + 0x10);
    return phys_vcall1(self + 0x50, 2, out + 0x60);
}
RX_FUNCTION(0x00858bd0, phys_matrix6x4_transpose)

/* 008588d0 Matrix4x6 copy (ECX = source, arg = destination): the row / column counts of the block matrix (+4 / +8) and of both
 * sub-matrices (+0x14 / +0x18, +0x64 / +0x68), and the rows of both (+0x20..+0x50, +0x70..+0xa0). Returns dst. */
uint32_t phys_matrix4x6_copy_from(rx_addr src, rx_addr dst) {
    static const uint32_t counts_a[4] = {0x04, 0x08, 0x14, 0x18};
    for (int i = 0; i < 4; i++) SU32(dst, counts_a[i], U32(src, counts_a[i]));
    for (uint32_t o = 0x20; o <= 0x50; o += 0x10) v4_st(dst + o, v4_ld(src + o));
    SU32(dst, 0x64, U32(src, 0x64));
    SU32(dst, 0x68, U32(src, 0x68));
    for (uint32_t o = 0x70; o <= 0xa0; o += 0x10) v4_st(dst + o, v4_ld(src + o));
    return dst;
}
RX_FUNCTION(0x008588d0, phys_matrix4x6_copy_from)

/* 008588c0 Matrix4x6::vf3: this = copy of src (008588d0 with ECX = src, arg = this). Returns this. */
uint32_t phys_matrix4x6_copy(rx_addr self, rx_addr src) { return phys_matrix4x6_copy_from(src, self); }
RX_FUNCTION(0x008588c0, phys_matrix4x6_copy)

/* 00858e80 Matrix5x6::vf1 (also 6x6 / 6x5): clear the four sub-matrices (+0x10, +0x50, +0x90, +0xd0; the last as a tail call,
 * so EAX is its result). */
uint32_t phys_matrix5x6_clear(rx_addr self) {
    phys_vcall0(self + 0x10, 1);
    phys_vcall0(self + 0x50, 1);
    phys_vcall0(self + 0x90, 1);
    return phys_vcall0(self + 0xd0, 1);
}
RX_FUNCTION(0x00858e80, phys_matrix5x6_clear)

/* 00858f80 Matrix5x6::vf2: out = transpose(this), the off-diagonal blocks swapped: +0x10 -> out+0x10, +0x90 -> out+0x50,
 * +0x50 -> out+0x90, +0xd0 -> out+0xd0. Returns the last callee's EAX. */
uint32_t phys_matrix5x6_transpose(rx_addr self, rx_addr out) {
    phys_vcall1(self + 0x10, 2, out + 0x10);
    phys_vcall1(self + 0x90, 2, out + 0x50);
    phys_vcall1(self + 0x50, 2, out + 0x90);
    return phys_vcall1(self + 0xd0, 2, out + 0xd0);
}
RX_FUNCTION(0x00858f80, phys_matrix5x6_transpose)

/* 00858c70 Matrix5x6::vf4: out = this - b, block by block (each sub-matrix's slot 4). Returns the last callee's EAX. */
uint32_t phys_matrix5x6_sub(rx_addr self, rx_addr out, rx_addr b) {
    static const uint32_t block[4] = {0x10, 0x50, 0x90, 0xd0};
    uint32_t r = 0;
    for (int i = 0; i < 4; i++) r = phys_vcall2(self + block[i], 4, out + block[i], b + block[i]);
    return r;
}
RX_FUNCTION(0x00858c70, phys_matrix5x6_sub)

/* 00858d80 Matrix5x6::vf3: this = copy of src through 00858d90 (ECX = src, arg = this; a Matrix5x6 copy owned elsewhere).
 * Returns its EAX (= this). */
uint32_t phys_matrix5x6_copy(rx_addr self, rx_addr src) {
    rx_addr sp = rx_sp(); rx_set_sp(sp - 8);
    uint32_t r = phys_matrix5x6_copy_from(src, self);
    rx_set_sp(sp);
    return r;
}
RX_FUNCTION(0x00858d80, phys_matrix5x6_copy)

/* ---- vectors ---- */

/* 0042f2e0 Vec6::vf0: both halves (rows 1 and 2) = the runtime vec4 0xd61670. Returns this. */
uint32_t phys_vec6_clear(rx_addr self) {
    v4_st(self + 0x10, v4_ld(G_ZERO3));
    v4_st(self + 0x20, v4_ld(G_ZERO3));
    return self;
}
RX_FUNCTION(0x0042f2e0, phys_vec6_clear)

/* 0042f310 Vec6::vf1: this = copy of src (both halves). Returns this. */
uint32_t phys_vec6_copy(rx_addr self, rx_addr src) {
    v4_st(self + 0x10, v4_ld(src + 0x10));
    v4_st(self + 0x20, v4_ld(src + 0x20));
    return self;
}
RX_FUNCTION(0x0042f310, phys_vec6_copy)

/* 0042f3b0 Vec3::vf1 (also Vec4's): this = copy of src (one vec4 at +0x10). Returns src (EAX holds the argument). */
uint32_t phys_vec3_copy(rx_addr self, rx_addr src) {
    v4_st(self + 0x10, v4_ld(src + 0x10));
    return src;
}
RX_FUNCTION(0x0042f3b0, phys_vec3_copy)
