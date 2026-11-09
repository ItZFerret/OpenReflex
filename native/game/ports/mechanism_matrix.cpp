// RavenPhysics matrix objects (mechanism group): the virtual methods of Matrix3x3 / 3x4 / 4x3 / 4x4 / 4x6 / 6x4 / 5x6 and Vec3 / Vec6.
// Layout: +0 vtable, +4 rows, +8 columns (block matrices: sub-matrix objects), rows of vec4 from +0x10 (stride 0x10, every lane
// stored, W included). Block matrices (4x6, 6x4, 5x6/6x6/6x5) hold sub-matrix objects and forward to their virtual methods.
// Slots: 1 clear, 2 transpose into arg, 3 copy from arg, 4 arg0 = this - arg1, 5 arg0 = arg1 + this, 6 negate in place.
#include "mechanism_util.h"

using namespace mech;
using reflex_port::OUT_EAX;
using namespace x86;

namespace {
// The stores of a register-to-memory sequence are done in the original's order (callers may pass aliased objects).
inline void row_op_sub(Guest& g, uint32_t out, uint32_t a, uint32_t b, int rows) {   // out = a - b, row by row
    for (int r = 0; r < rows; r++) { uint32_t o = 0x10 + 0x10 * r; V4 x0 = ld(g, b + o), x1 = ld(g, a + o); st(g, out + o, sub(x1, x0)); }
}
}

// 00857c30 Matrix3x3::vf1: clear the three rows with the global vec4 at 00d61670. EAX = this.
REFLEX_PORT_FN(0x00857c30, Matrix3x3_Clear, "mechanism", OUT_EAX) {
    Guest g{m}; uint32_t t = m.r[1];
    for (int r = 0; r < 3; r++) st(g, t + 0x10 + 0x10 * r, ld(g, 0x00d61670));
    m.r[0] = t; return m.ret(0);
}

// 00857c90 Matrix3x3::vf2: arg = transpose(this). W lanes: row0.w = r2.x, row1.w = r2.y, row2.w = r2.z (the original's packing).
REFLEX_PORT_FN(0x00857c90, Matrix3x3_Transpose, "mechanism", OUT_EAX) {
    Guest g{m}; uint32_t t = m.r[1], out = m.arg(0);
    V4 x0 = ld(g, t + 0x10), x2 = ld(g, t + 0x30), x1 = ld(g, t + 0x20);
    V4 x4 = shuf(x0, x0, 0xaa);
    V4 x3 = movss(x2, x4);
    x4 = shuf(x1, x0, 0xe1);
    x4 = movss(x4, x0);
    x4 = shuf(x4, x2, 4);
    st(g, out + 0x10, x4);
    x4 = x1;
    x0 = shuf(x0, x0, 0x55);
    x4 = movss(x4, x0);
    x0 = shuf(x4, x2, 0x54);
    x1 = shuf(x1, x3, 0xe9);
    st(g, out + 0x20, x0);
    x1 = movss(x1, x3);
    st(g, out + 0x30, x1);
    m.r[0] = out; return m.ret(4);
}

// 00857bf0 Matrix3x3::vf3 (also Matrix3x4's): copy rows / columns counts and the three rows from arg. EAX = this, ECX = arg.
REFLEX_PORT_FN(0x00857bf0, Matrix3x3_Copy, "mechanism", OUT_EAX) {
    Guest g{m}; uint32_t t = m.r[1], s = m.arg(0);
    g.u32(t + 4, g.u32(s + 4)); g.u32(t + 8, g.u32(s + 8));
    for (int r = 0; r < 3; r++) st(g, t + 0x10 + 0x10 * r, ld(g, s + 0x10 + 0x10 * r));
    m.r[0] = t; m.r[1] = s; return m.ret(4);
}

// 00858160 Matrix3x3::vf4: arg0 = this - arg1 (three rows). EAX = arg0, EDX = arg1.
REFLEX_PORT_FN(0x00858160, Matrix3x3_Sub, "mechanism", OUT_EAX) {
    Guest g{m}; uint32_t t = m.r[1], out = m.arg(0), b = m.arg(1);
    row_op_sub(g, out, t, b, 3);
    m.r[0] = out; m.r[2] = b; return m.ret(8);
}

// 008581a0 Matrix3x3::vf5: arg0 = arg1 + this (three rows; addps arg1, this). EAX = arg0, EDX = arg1.
REFLEX_PORT_FN(0x008581a0, Matrix3x3_Add, "mechanism", OUT_EAX) {
    Guest g{m}; uint32_t t = m.r[1], out = m.arg(0), b = m.arg(1);
    for (int r = 0; r < 3; r++) { uint32_t o = 0x10 + 0x10 * r; V4 x1 = ld(g, t + o), x0 = ld(g, b + o); st(g, out + o, add(x0, x1)); }
    m.r[0] = out; m.r[2] = b; return m.ret(8);
}

// 008581e0 Matrix3x3::vf6: this = 0 - this (three rows). EAX = this.
REFLEX_PORT_FN(0x008581e0, Matrix3x3_Negate, "mechanism", OUT_EAX) {
    Guest g{m}; uint32_t t = m.r[1];
    for (int r = 0; r < 3; r++) { uint32_t o = 0x10 + 0x10 * r; st(g, t + o, sub(zero(), ld(g, t + o))); }
    m.r[0] = t; return m.ret(0);
}

// 00858220 Matrix3x4::vf2: arg (4x3) = transpose(this (3x4)): copies the three rows to arg, then fixes lanes through memory in the
// original's order (the read-backs of arg are part of the result when arg aliases this).
REFLEX_PORT_FN(0x00858220, Matrix3x4_Transpose, "mechanism", OUT_EAX) {
    Guest g{m}; uint32_t c = m.r[1], a = m.arg(0);
    st(g, a + 0x10, ld(g, c + 0x10));
    st(g, a + 0x20, ld(g, c + 0x20));
    st(g, a + 0x30, ld(g, c + 0x30));
    V4 x0 = ld(g, a + 0x10), x1 = ld(g, c + 0x20);
    x1 = shuf(x1, x0, 0xe1); x1 = movss(x1, x0); st(g, a + 0x10, x1);
    x1 = ld(g, c + 0x30); x0 = ld(g, a + 0x10); x0 = shuf(x0, x1, 4); st(g, a + 0x10, x0);
    x0 = ld(g, c + 0x10); x1 = ld(g, a + 0x20); x0 = shuf(x0, x0, 0x55); x1 = movss(x1, x0); st(g, a + 0x20, x1);
    x1 = ld(g, c + 0x30); x0 = ld(g, a + 0x20); x0 = shuf(x0, x1, 0x54); st(g, a + 0x20, x0);
    x0 = ld(g, c + 0x10); x1 = ld(g, a + 0x30); x0 = shuf(x0, x0, 0xaa); x1 = movss(x1, x0); st(g, a + 0x30, x1);
    x0 = ld(g, a + 0x30); x1 = ld(g, c + 0x20); x1 = shuf(x1, x0, 0xe9); x1 = movss(x1, x0); st(g, a + 0x30, x1);
    x0 = ld(g, c + 0x10); x1 = ld(g, a + 0x40); x0 = shuf(x0, x0, 0xff); x1 = movss(x1, x0); st(g, a + 0x40, x1);
    x0 = ld(g, a + 0x40); x1 = ld(g, c + 0x20); x1 = shuf(x1, x0, 0xed); x1 = movss(x1, x0); st(g, a + 0x40, x1);
    x0 = ld(g, a + 0x40); x1 = ld(g, c + 0x30); x0 = shuf(x0, x1, 0xf4); st(g, a + 0x40, x0);
    m.r[0] = a; return m.ret(4);
}

// 00858440 Matrix4x3::vf2: arg (3x4) = transpose(this (4x3)), one lane at a time through memory: arg.row[r].lane[k] =
// this.row[k].lane[r] (r = 0..2, k = 0..3; integer word copies). EAX = arg, ECX = this.row3.lane2 (last copied word).
REFLEX_PORT_FN(0x00858440, Matrix4x3_Transpose, "mechanism", OUT_EAX) {
    Guest g{m}; uint32_t c = m.r[1], a = m.arg(0), w = 0;
    for (int r = 0; r < 3; r++)
        for (int k = 0; k < 4; k++) {
            w = g.u32(c + 0x10 + 0x10 * k + 4 * r);
            g.u32(a + 0x10 + 0x10 * r + 4 * k, w);
        }
    m.r[0] = a; m.r[1] = w; return m.ret(4);
}

// 00858660 Matrix4x3::vf4 (also Matrix4x4's): arg0 = this - arg1 (four rows). EAX = arg0, EDX = arg1.
REFLEX_PORT_FN(0x00858660, Matrix4x3_Sub, "mechanism", OUT_EAX) {
    Guest g{m}; uint32_t t = m.r[1], out = m.arg(0), b = m.arg(1);
    row_op_sub(g, out, t, b, 4);
    m.r[0] = out; m.r[2] = b; return m.ret(8);
}

// 008583c0 Matrix4x3::vf5 (also Matrix4x4's): arg0 = arg1 + this (four rows). EAX = arg0, EDX = arg1.
REFLEX_PORT_FN(0x008583c0, Matrix4x3_Add, "mechanism", OUT_EAX) {
    Guest g{m}; uint32_t t = m.r[1], out = m.arg(0), b = m.arg(1);
    for (int r = 0; r < 4; r++) { uint32_t o = 0x10 + 0x10 * r; V4 x1 = ld(g, t + o), x0 = ld(g, b + o); st(g, out + o, add(x0, x1)); }
    m.r[0] = out; m.r[2] = b; return m.ret(8);
}

// 00858620 Matrix4x4::vf1: clear the four rows with the global vec4 at 00d61660. EAX = this.
REFLEX_PORT_FN(0x00858620, Matrix4x4_Clear, "mechanism", OUT_EAX) {
    Guest g{m}; uint32_t t = m.r[1];
    for (int r = 0; r < 4; r++) st(g, t + 0x10 + 0x10 * r, ld(g, 0x00d61660));
    m.r[0] = t; return m.ret(0);
}

// 00858a80 Matrix4x6::vf2: transpose = the sub-matrices' transposes (this+10 -> arg+10, this+60 -> arg+50).
REFLEX_PORT_FN(0x00858a80, Matrix4x6_Transpose, "mechanism", 0) {
    Guest g{m}; uint32_t t = m.r[1], a = m.arg(0);
    vcall(g, t + 0x10, 2, {a + 0x10});
    vcall(g, t + 0x60, 2, {a + 0x50});
    return m.ret(4);
}

// 00858bd0 Matrix6x4::vf2: transpose = the sub-matrices' transposes (this+10 -> arg+10, this+50 -> arg+60).
REFLEX_PORT_FN(0x00858bd0, Matrix6x4_Transpose, "mechanism", 0) {
    Guest g{m}; uint32_t t = m.r[1], a = m.arg(0);
    vcall(g, t + 0x10, 2, {a + 0x10});
    vcall(g, t + 0x50, 2, {a + 0x60});
    return m.ret(4);
}

// 008588d0: copy a Matrix4x6 (ECX = source, arg = destination): counts of both sub-matrices and their rows. EAX = arg.
REFLEX_PORT_FN(0x008588d0, Matrix4x6_CopyFrom, "mechanism", OUT_EAX) {
    Guest g{m}; uint32_t s = m.r[1], d = m.arg(0), w = 0;
    for (uint32_t o : {0x04u, 0x08u, 0x14u, 0x18u}) { w = g.u32(s + o); g.u32(d + o, w); }
    for (uint32_t o = 0x20; o <= 0x50; o += 0x10) st(g, d + o, ld(g, s + o));
    for (uint32_t o : {0x64u, 0x68u}) { w = g.u32(s + o); g.u32(d + o, w); }
    for (uint32_t o = 0x70; o <= 0xa0; o += 0x10) st(g, d + o, ld(g, s + o));
    m.r[0] = d; m.r[2] = w; return m.ret(4);
}

// 008588c0 Matrix4x6::vf3: this = copy of arg (008588d0 with ECX = arg, arg = this).
REFLEX_PORT_FN(0x008588c0, Matrix4x6_Copy, "mechanism", OUT_EAX) {
    Guest g{m}; uint32_t t = m.r[1], s = m.arg(0);
    g.call(0x008588d0, {t}, s);
    return m.ret(4);
}

// 00858e80 Matrix5x6::vf1 (also 6x6 / 6x5): clear the four sub-matrices (+10, +50, +90, +d0; the last one as a tail call).
REFLEX_PORT_FN(0x00858e80, Matrix5x6_Clear, "mechanism", OUT_EAX) {
    Guest g{m}; uint32_t t = m.r[1];
    for (uint32_t o : {0x10u, 0x50u, 0x90u, 0xd0u}) vcall(g, t + o, 1);
    return m.ret(0);
}

// 00858f80 Matrix5x6::vf2: transpose = the sub-matrices' transposes, off-diagonal blocks swapped (+10->+10, +90->+50, +50->+90,
// +d0->+d0).
REFLEX_PORT_FN(0x00858f80, Matrix5x6_Transpose, "mechanism", 0) {
    Guest g{m}; uint32_t t = m.r[1], a = m.arg(0);
    vcall(g, t + 0x10, 2, {a + 0x10});
    vcall(g, t + 0x90, 2, {a + 0x50});
    vcall(g, t + 0x50, 2, {a + 0x90});
    vcall(g, t + 0xd0, 2, {a + 0xd0});
    return m.ret(4);
}

// 00858c70 Matrix5x6::vf4: arg0 = this - arg1, block by block (sub-matrix slot 4).
REFLEX_PORT_FN(0x00858c70, Matrix5x6_Sub, "mechanism", 0) {
    Guest g{m}; uint32_t t = m.r[1], out = m.arg(0), b = m.arg(1);
    for (uint32_t o : {0x10u, 0x50u, 0x90u, 0xd0u}) vcall(g, t + o, 4, {out + o, b + o});
    return m.ret(8);
}

// 00858d80 Matrix5x6::vf3: this = copy of arg (00858d90 with ECX = arg, arg = this).
REFLEX_PORT_FN(0x00858d80, Matrix5x6_Copy, "mechanism", OUT_EAX) {
    Guest g{m}; uint32_t t = m.r[1], s = m.arg(0);
    g.call(0x00858d90, {t}, s);
    return m.ret(4);
}

// 0042f2e0 Vec6::vf0: clear both halves with the global vec4 at 00d61670. EAX = this.
REFLEX_PORT_FN(0x0042f2e0, Vec6_Clear, "mechanism", OUT_EAX) {
    Guest g{m}; uint32_t t = m.r[1];
    st(g, t + 0x10, ld(g, 0x00d61670)); st(g, t + 0x20, ld(g, 0x00d61670));
    m.r[0] = t; return m.ret(0);
}

// 0042f310 Vec6::vf1: copy from arg. EAX = this, ECX = arg.
REFLEX_PORT_FN(0x0042f310, Vec6_Copy, "mechanism", OUT_EAX) {
    Guest g{m}; uint32_t t = m.r[1], s = m.arg(0);
    st(g, t + 0x10, ld(g, s + 0x10)); st(g, t + 0x20, ld(g, s + 0x20));
    m.r[0] = t; m.r[1] = s; return m.ret(4);
}

// 0042f3b0 Vec3::vf1 (also Vec4's): copy from arg. EAX = arg.
REFLEX_PORT_FN(0x0042f3b0, Vec3_Copy, "mechanism", OUT_EAX) {
    Guest g{m}; uint32_t t = m.r[1], s = m.arg(0);
    st(g, t + 0x10, ld(g, s + 0x10));
    m.r[0] = s; return m.ret(4);
}
