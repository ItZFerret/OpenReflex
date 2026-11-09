// Straight-line SSE kernels of the mechanism group (matrix-vector and matrix-matrix products of the RavenPhysics solver).
// Generated from the original instructions by symbolic execution (scratchpad tool sym.py): every guest load at its original
// program point (aliased arguments behave as in the original), every stored lane with the original operation order.
#include "mechanism_kernels.h"

using namespace mech;
using namespace x86;
using reflex_port::OUT_EAX; using reflex_port::OUT_ECX; using reflex_port::OUT_EDX;

namespace mech {

// 00854650 FUN_00854650: ECX(3x3) = EDX(3x3) * arg(3x3)^T: row j = (dot3(B0,Aj), dot3(B1,Aj), dot3(B2,Aj), W = lane 2); rows written in order
void Mat3_MulABt(Guest& g, uint32_t ecx, uint32_t edx, uint32_t arg0) {
    const V4 L0 = ld(g, edx + 0x10);
    const V4 L1 = ld(g, arg0 + 0x10);
    const V4 L2 = ld(g, arg0 + 0x20);
    const V4 L3 = ld(g, arg0 + 0x30);
    const float t1 = dot3(L3, L0);
    st(g, ecx + 0x10, V4{{dot3(L1, L0), dot3(L2, L0), t1, t1}});
    const V4 L4 = ld(g, edx + 0x20);
    const V4 L5 = ld(g, arg0 + 0x10);
    const V4 L6 = ld(g, arg0 + 0x20);
    const V4 L7 = ld(g, arg0 + 0x30);
    const float t2 = dot3(L7, L4);
    st(g, ecx + 0x20, V4{{dot3(L5, L4), dot3(L6, L4), t2, t2}});
    const V4 L8 = ld(g, edx + 0x30);
    const V4 L9 = ld(g, arg0 + 0x10);
    const V4 L10 = ld(g, arg0 + 0x20);
    const V4 L11 = ld(g, arg0 + 0x30);
    const float t3 = dot3(L11, L8);
    st(g, ecx + 0x30, V4{{dot3(L9, L8), dot3(L10, L8), t3, t3}});
    g.m.r[0] = arg0;
}

// 00854490 FUN_00854490: vec4 [ECX] = arg(3x3) * vec4 [EDX] (raw vec4s, no header); W = z
void Mat3_MulVec(Guest& g, uint32_t ecx, uint32_t edx, uint32_t arg0) {
    const V4 L0 = ld(g, edx);
    const V4 L1 = ld(g, arg0 + 0x10);
    const V4 L2 = ld(g, arg0 + 0x20);
    const V4 L3 = ld(g, arg0 + 0x30);
    const float t1 = dot3(L3, L0);
    st(g, ecx, V4{{dot3(L1, L0), dot3(L2, L0), t1, t1}});
    g.m.r[0] = arg0;
}

// 00854ac0 FUN_00854ac0: ECX(4 rows) = EDX(4 rows of xyz) * arg(3x3)^T, W = z; row 3 multiplies in the other operand order
void Mat4x3_MulMat3t(Guest& g, uint32_t ecx, uint32_t edx, uint32_t arg0) {
    const V4 L0 = ld(g, edx + 0x10);
    const V4 L1 = ld(g, arg0 + 0x10);
    const V4 L2 = ld(g, arg0 + 0x20);
    const V4 L3 = ld(g, arg0 + 0x30);
    const float t1 = dot3(L3, L0);
    st(g, ecx + 0x10, V4{{dot3(L1, L0), dot3(L2, L0), t1, t1}});
    const V4 L4 = ld(g, edx + 0x20);
    const V4 L5 = ld(g, arg0 + 0x10);
    const V4 L6 = ld(g, arg0 + 0x20);
    const V4 L7 = ld(g, arg0 + 0x30);
    const float t2 = dot3(L7, L4);
    st(g, ecx + 0x20, V4{{dot3(L5, L4), dot3(L6, L4), t2, t2}});
    const V4 L8 = ld(g, edx + 0x30);
    const V4 L9 = ld(g, arg0 + 0x10);
    const V4 L10 = ld(g, arg0 + 0x20);
    const V4 L11 = ld(g, arg0 + 0x30);
    const float t3 = dot3(L11, L8);
    st(g, ecx + 0x30, V4{{dot3(L9, L8), dot3(L10, L8), t3, t3}});
    const V4 L12 = ld(g, edx + 0x40);
    const V4 L13 = ld(g, arg0 + 0x10);
    const V4 L14 = ld(g, arg0 + 0x20);
    const V4 L15 = ld(g, arg0 + 0x30);
    const float t4 = dot3(L12, L15);
    st(g, ecx + 0x40, V4{{dot3(L12, L13), dot3(L12, L14), t4, t4}});
    g.m.r[0] = arg0;
}

// 00854cb0 FUN_00854cb0: ECX(3x4) = EDX(3x4) * arg(4x4)^T (dot4)
void Mat3x4_MulMat4t(Guest& g, uint32_t ecx, uint32_t edx, uint32_t arg0) {
    const V4 L0 = ld(g, edx + 0x10);
    const V4 L1 = ld(g, arg0 + 0x10);
    const V4 L2 = ld(g, arg0 + 0x20);
    const V4 L3 = ld(g, arg0 + 0x30);
    const V4 L4 = ld(g, arg0 + 0x40);
    st(g, ecx + 0x10, V4{{dot4(L0, L1), dot4(L0, L2), dot4(L0, L3), dot4(L0, L4)}});
    const V4 L5 = ld(g, edx + 0x20);
    const V4 L6 = ld(g, arg0 + 0x10);
    const V4 L7 = ld(g, arg0 + 0x20);
    const V4 L8 = ld(g, arg0 + 0x30);
    const V4 L9 = ld(g, arg0 + 0x40);
    st(g, ecx + 0x20, V4{{dot4(L5, L6), dot4(L7, L5), dot4(L8, L5), dot4(L9, L5)}});
    const V4 L10 = ld(g, arg0 + 0x10);
    const V4 L11 = ld(g, edx + 0x30);
    const V4 L12 = ld(g, arg0 + 0x20);
    const V4 L13 = ld(g, arg0 + 0x30);
    const V4 L14 = ld(g, arg0 + 0x40);
    const float t1 = dot4(L13, L11);
    const float t2 = dot4(L14, L11);
    st(g, ecx + 0x30, V4{{dot4(L10, L11), dot4(L12, L11), t1, t2}});
    g.m.r[0] = bits(t2);
    g.m.r[2] = bits(t1);
}

// 00855240 FUN_00855240: ECX(4x4) = EDX(4x3) * arg(4x3)^T (dot3)
void Mat4x3_MulMat4x3t(Guest& g, uint32_t ecx, uint32_t edx, uint32_t arg0) {
    const V4 L0 = ld(g, edx + 0x10);
    const V4 L1 = ld(g, arg0 + 0x10);
    const V4 L2 = ld(g, arg0 + 0x20);
    const V4 L3 = ld(g, arg0 + 0x30);
    const V4 L4 = ld(g, arg0 + 0x40);
    st(g, ecx + 0x10, V4{{dot3(L1, L0), dot3(L2, L0), dot3(L3, L0), dot3(L4, L0)}});
    const V4 L5 = ld(g, edx + 0x20);
    const V4 L6 = ld(g, arg0 + 0x10);
    const V4 L7 = ld(g, arg0 + 0x20);
    const V4 L8 = ld(g, arg0 + 0x30);
    const V4 L9 = ld(g, arg0 + 0x40);
    st(g, ecx + 0x20, V4{{dot3(L6, L5), dot3(L7, L5), dot3(L8, L5), dot3(L9, L5)}});
    const V4 L10 = ld(g, edx + 0x30);
    const V4 L11 = ld(g, arg0 + 0x10);
    const V4 L12 = ld(g, arg0 + 0x20);
    const V4 L13 = ld(g, arg0 + 0x30);
    const V4 L14 = ld(g, arg0 + 0x40);
    st(g, ecx + 0x30, V4{{dot3(L11, L10), dot3(L12, L10), dot3(L13, L10), dot3(L14, L10)}});
    const V4 L15 = ld(g, edx + 0x40);
    const V4 L16 = ld(g, arg0 + 0x10);
    const V4 L17 = ld(g, arg0 + 0x20);
    const V4 L18 = ld(g, arg0 + 0x30);
    const V4 L19 = ld(g, arg0 + 0x40);
    const float t1 = dot3(L18, L15);
    const float t2 = dot3(L19, L15);
    st(g, ecx + 0x40, V4{{dot3(L16, L15), dot3(L17, L15), t1, t2}});
    g.m.r[0] = bits(t2);
    g.m.r[2] = bits(t1);
}

// 00855590 FUN_00855590: ECX(3x3) = EDX(3x4) * arg(3x4)^T (dot4), W = lane 0
void Mat3x4_MulMat3x4t(Guest& g, uint32_t ecx, uint32_t edx, uint32_t arg0) {
    const V4 L0 = ld(g, edx + 0x10);
    const V4 L1 = ld(g, arg0 + 0x10);
    const V4 L2 = ld(g, arg0 + 0x20);
    const V4 L3 = ld(g, arg0 + 0x30);
    const float t1 = dot4(L1, L0);
    st(g, ecx + 0x10, V4{{t1, dot4(L2, L0), dot4(L3, L0), t1}});
    const V4 L4 = ld(g, edx + 0x20);
    const V4 L5 = ld(g, arg0 + 0x10);
    const V4 L6 = ld(g, arg0 + 0x20);
    const V4 L7 = ld(g, arg0 + 0x30);
    const float t2 = dot4(L5, L4);
    st(g, ecx + 0x20, V4{{t2, dot4(L6, L4), dot4(L7, L4), t2}});
    const V4 L8 = ld(g, edx + 0x30);
    const V4 L9 = ld(g, arg0 + 0x10);
    const V4 L10 = ld(g, arg0 + 0x20);
    const V4 L11 = ld(g, arg0 + 0x30);
    const float t3 = dot4(L9, L8);
    const float t4 = dot4(L10, L8);
    const float t5 = dot4(L11, L8);
    st(g, ecx + 0x30, V4{{t3, t4, t5, t3}});
    g.m.r[0] = bits(t4);
    g.m.r[2] = bits(t5);
}

// 00853bd0 FUN_00853bd0: ECX+10 = arg(4x4) * EDX+10 (dot4)
void Mat4_MulVec(Guest& g, uint32_t ecx, uint32_t edx, uint32_t arg0) {
    const V4 L0 = ld(g, edx + 0x10);
    const V4 L1 = ld(g, arg0 + 0x10);
    const V4 L2 = ld(g, arg0 + 0x20);
    const V4 L3 = ld(g, arg0 + 0x30);
    const V4 L4 = ld(g, arg0 + 0x40);
    const float t1 = dot4(L3, L0);
    const float t2 = dot4(L4, L0);
    st(g, ecx + 0x10, V4{{dot4(L1, L0), dot4(L2, L0), t1, t2}});
    g.m.r[0] = bits(t2);
    g.m.r[2] = bits(t1);
}

// 008540a0 FUN_008540a0: ECX(+10,+20) = arg(6x6: 3x3 blocks at +20/+60/+a0/+e0) * EDX(+10,+20); W = z
void Mat6_MulVec(Guest& g, uint32_t ecx, uint32_t edx, uint32_t arg0) {
    const V4 L0 = ld(g, edx + 0x20);
    const V4 L1 = ld(g, arg0 + 0x60);
    const V4 L2 = ld(g, edx + 0x10);
    const V4 L3 = ld(g, arg0 + 0x20);
    const V4 L4 = ld(g, arg0 + 0x70);
    const V4 L5 = ld(g, arg0 + 0x30);
    const V4 L6 = ld(g, arg0 + 0x80);
    const V4 L7 = ld(g, arg0 + 0x40);
    const V4 L8 = ld(g, arg0 + 0xe0);
    const V4 L9 = ld(g, arg0 + 0xa0);
    const V4 L10 = ld(g, arg0 + 0xf0);
    const V4 L11 = ld(g, arg0 + 0xb0);
    const V4 L12 = ld(g, arg0 + 0x100);
    const V4 L13 = ld(g, arg0 + 0xc0);
    const V4 v1 = add(V4{{dot3(L3, L2), dot3(L5, L2), dot3(L7, L2), dot3(L7, L2)}}, V4{{dot3(L1, L0), dot3(L4, L0), dot3(L6, L0), dot3(L6, L0)}});
    st(g, ecx + 0x10, v1);
    const V4 v2 = add(V4{{dot3(L9, L2), dot3(L11, L2), dot3(L13, L2), dot3(L13, L2)}}, V4{{dot3(L8, L0), dot3(L10, L0), dot3(L12, L0), dot3(L12, L0)}});
    st(g, ecx + 0x20, v2);
    g.m.r[0] = arg0;
}

// 00853a30 FUN_00853a30: EDX+10 = arg(4x6: blocks at +20/+70) * ECX(+10,+20)
void Mat4x6_MulVec(Guest& g, uint32_t ecx, uint32_t edx, uint32_t arg0) {
    const V4 L0 = ld(g, ecx + 0x20);
    const V4 L1 = ld(g, arg0 + 0x70);
    const V4 L2 = ld(g, ecx + 0x10);
    const V4 L3 = ld(g, arg0 + 0x20);
    const V4 L4 = ld(g, arg0 + 0x80);
    const V4 L5 = ld(g, arg0 + 0x30);
    const V4 L6 = ld(g, arg0 + 0x90);
    const V4 L7 = ld(g, arg0 + 0x40);
    const V4 L8 = ld(g, arg0 + 0xa0);
    const V4 L9 = ld(g, arg0 + 0x50);
    const V4 v1 = add(V4{{dot3(L3, L2), dot3(L5, L2), dot3(L7, L2), dot3(L9, L2)}}, V4{{dot3(L1, L0), dot3(L4, L0), dot3(L6, L0), dot3(L8, L0)}});
    st(g, edx + 0x10, v1);
    g.m.r[0] = bits(v1.f[3]);
    g.m.r[1] = bits(v1.f[2]);
}

// 00857750 FUN_00857750: ECX+10 -= arg(4x6: blocks at +20/+70) * EDX(+10,+20)
void Mat4x6_MulVecSub(Guest& g, uint32_t ecx, uint32_t edx, uint32_t arg0) {
    const V4 L0 = ld(g, edx + 0x20);
    const V4 L1 = ld(g, arg0 + 0x70);
    const V4 L2 = ld(g, edx + 0x10);
    const V4 L3 = ld(g, arg0 + 0x20);
    const V4 L4 = ld(g, arg0 + 0x80);
    const V4 L5 = ld(g, arg0 + 0x30);
    const V4 L6 = ld(g, arg0 + 0x90);
    const V4 L7 = ld(g, arg0 + 0x40);
    const V4 L8 = ld(g, arg0 + 0xa0);
    const V4 L9 = ld(g, arg0 + 0x50);
    const V4 L10 = ld(g, ecx + 0x10);
    const V4 v1 = add(V4{{dot3(L3, L2), dot3(L5, L2), dot3(L7, L2), dot3(L9, L2)}}, V4{{dot3(L1, L0), dot3(L4, L0), dot3(L6, L0), dot3(L8, L0)}});
    st(g, ecx + 0x10, sub(L10, v1));
    g.m.r[0] = bits(v1.f[3]);
    g.m.r[2] = bits(v1.f[2]);
}

// 00857a00 FUN_00857a00: ECX(+10,+20) -= arg(6x4: rows +20..40, +60..80) * EDX+10 (dot4); W = first row's
void Mat6x4_MulVecSub(Guest& g, uint32_t ecx, uint32_t edx, uint32_t arg0) {
    const V4 L0 = ld(g, edx + 0x10);
    const V4 L1 = ld(g, arg0 + 0x20);
    const V4 L2 = ld(g, arg0 + 0x30);
    const V4 L3 = ld(g, arg0 + 0x40);
    const V4 L4 = ld(g, arg0 + 0x60);
    const V4 L5 = ld(g, arg0 + 0x70);
    const V4 L6 = ld(g, arg0 + 0x80);
    const V4 L7 = ld(g, ecx + 0x10);
    const float t1 = dot4(L1, L0);
    st(g, ecx + 0x10, sub(L7, V4{{t1, dot4(L2, L0), dot4(L3, L0), t1}}));
    const V4 L8 = ld(g, ecx + 0x20);
    const float t2 = dot4(L4, L0);
    const float t3 = dot4(L5, L0);
    const float t4 = dot4(L6, L0);
    st(g, ecx + 0x20, sub(L8, V4{{t2, t3, t4, t2}}));
    g.m.r[0] = bits(t3);
    g.m.r[2] = bits(t4);
}

// 00853e80 FUN_00853e80: ECX(+10,+20) = arg(6x4: rows +20..40, +60..80) * EDX+10 (dot4); W = first row's
void Mat6x4_MulVec(Guest& g, uint32_t ecx, uint32_t edx, uint32_t arg0) {
    const V4 L0 = ld(g, edx + 0x10);
    const V4 L1 = ld(g, arg0 + 0x20);
    const V4 L2 = ld(g, arg0 + 0x30);
    const V4 L3 = ld(g, arg0 + 0x40);
    const V4 L4 = ld(g, arg0 + 0x60);
    const V4 L5 = ld(g, arg0 + 0x70);
    const V4 L6 = ld(g, arg0 + 0x80);
    const float t1 = dot4(L1, L0);
    st(g, ecx + 0x10, V4{{t1, dot4(L2, L0), dot4(L3, L0), t1}});
    const float t2 = dot4(L4, L0);
    const float t3 = dot4(L5, L0);
    const float t4 = dot4(L6, L0);
    st(g, ecx + 0x20, V4{{t2, t3, t4, t2}});
    g.m.r[0] = bits(t3);
    g.m.r[2] = bits(t4);
}

// 00857e50 FUN_00857e50: ECX rows = rotation matrix of the quaternion (x,y,z,w) at arg (constants -2 at 009d7b30, -0 at 009774a8, 1 at 00931f24)
void Matrix3x3_FromQuaternion(Guest& g, uint32_t ecx, uint32_t arg0) {
    const uint32_t W0 = g.u32(arg0 + 0x8);
    const uint32_t W1 = g.u32(arg0 + 0xc);
    const uint32_t W2 = g.u32(arg0 + 0x4);
    const uint32_t W3 = g.u32(arg0);
    const uint32_t W4 = g.u32(0x009d7b30);
    const uint32_t W5 = g.u32(0x009774a8);
    const uint32_t W6 = g.u32(0x00931f24);
    const uint32_t W7 = g.u32(0x00931f24);
    const float t1 = mul(flt(W0), flt(W4));
    const float t2 = sub(flt(W5), mul(flt(W0), t1));
    const float t3 = mul(flt(W2), flt(W4));
    const float t4 = sub(flt(W5), mul(flt(W2), t3));
    const float t5 = mul(flt(W3), flt(W4));
    const float t6 = sub(flt(W5), mul(flt(W2), t5));
    const float t7 = mul(flt(W1), t1);
    const float t8 = sub(flt(W5), mul(flt(W0), t5));
    const float t9 = mul(flt(W1), t3);
    const float t10 = sub(t8, t9);
    st(g, ecx + 0x10, V4{{sub(flt(W6), add(t2, t4)), add(t6, t7), t10, t10}});
    const float t11 = sub(flt(W5), mul(flt(W3), t5));
    const float t12 = sub(flt(W5), mul(flt(W0), t3));
    const float t13 = mul(flt(W1), t5);
    const float t14 = add(t12, t13);
    st(g, ecx + 0x20, V4{{sub(t6, t7), sub(flt(W7), add(t2, t11)), t14, t14}});
    const float t15 = sub(flt(W7), add(t4, t11));
    st(g, ecx + 0x30, V4{{add(t8, t9), sub(t12, t13), t15, t15}});
    g.m.r[0] = arg0;
}

// 00857d00 FUN_00857d00: ECX rows = rotation matrix of the quaternion at arg, the +2 form (constant 2 at 0092e330)
void Matrix3x3_FromQuaternion2(Guest& g, uint32_t ecx, uint32_t arg0) {
    const uint32_t W0 = g.u32(arg0 + 0x8);
    const uint32_t W1 = g.u32(arg0 + 0x4);
    const uint32_t W2 = g.u32(arg0);
    const uint32_t W3 = g.u32(arg0 + 0xc);
    const uint32_t W4 = g.u32(0x0092e330);
    const uint32_t W5 = g.u32(0x00931f24);
    const float t1 = mul(flt(W0), flt(W4));
    const float t2 = mul(flt(W0), t1);
    const float t3 = mul(flt(W1), flt(W4));
    const float t4 = mul(flt(W1), t3);
    const float t5 = mul(flt(W2), flt(W4));
    const float t6 = mul(flt(W1), t5);
    const float t7 = mul(flt(W3), t1);
    const float t8 = mul(flt(W0), t5);
    const float t9 = mul(flt(W3), t3);
    const float t10 = sub(t8, t9);
    st(g, ecx + 0x10, V4{{sub(flt(W5), add(t2, t4)), add(t6, t7), t10, t10}});
    const uint32_t W6 = g.u32(0x00931f24);
    const uint32_t W7 = g.u32(0x00931f24);
    const float t11 = mul(flt(W2), t5);
    const float t12 = mul(flt(W0), t3);
    const float t13 = mul(flt(W3), t5);
    const float t14 = add(t12, t13);
    st(g, ecx + 0x20, V4{{sub(t6, t7), sub(flt(W6), add(t2, t11)), t14, t14}});
    const float t15 = sub(flt(W7), add(t4, t11));
    st(g, ecx + 0x30, V4{{add(t8, t9), sub(t12, t13), t15, t15}});
    g.m.r[0] = arg0;
}

// 00857ff0 FUN_00857ff0: arg rows = cross-product matrix of the vec4 at ECX (cleared with 00d61670 first, then lanes set through memory)
void Matrix3x3_Skew(Guest& g, uint32_t ecx, uint32_t arg0) {
    const V4 L0 = ld(g, ecx);
    const V4 L1 = ld(g, 0x00d61670);
    st(g, arg0 + 0x10, L1);
    const V4 L2 = ld(g, 0x00d61670);
    st(g, arg0 + 0x20, L2);
    const V4 L3 = ld(g, 0x00d61670);
    st(g, arg0 + 0x30, L3);
    const V4 L4 = ld(g, arg0 + 0x10);
    st(g, arg0 + 0x10, V4{{L4.f[0], sub(flt(0x00000000), L0.f[2]), L4.f[2], L4.f[3]}});
    const V4 L5 = ld(g, ecx);
    const V4 L6 = ld(g, arg0 + 0x10);
    st(g, arg0 + 0x10, V4{{L6.f[0], L6.f[1], L5.f[1], L5.f[1]}});
    const V4 L7 = ld(g, ecx);
    const V4 L8 = ld(g, arg0 + 0x20);
    st(g, arg0 + 0x20, V4{{L7.f[2], L8.f[1], L8.f[2], L8.f[3]}});
    const V4 L9 = ld(g, arg0 + 0x20);
    const float t1 = sub(flt(0x00000000), L0.f[0]);
    st(g, arg0 + 0x20, V4{{L9.f[0], L9.f[1], t1, t1}});
    const V4 L10 = ld(g, arg0 + 0x30);
    st(g, arg0 + 0x30, V4{{sub(flt(0x00000000), L0.f[1]), L10.f[1], L10.f[2], L10.f[3]}});
    const V4 L11 = ld(g, arg0 + 0x30);
    const V4 L12 = ld(g, ecx);
    st(g, arg0 + 0x30, V4{{L11.f[0], L12.f[0], L11.f[2], L11.f[3]}});
    g.m.r[0] = arg0;
}

// 00858090 FUN_00858090: arg rows = transposed cross-product matrix of the vec4 at ECX
void Matrix3x3_SkewT(Guest& g, uint32_t ecx, uint32_t arg0) {
    const V4 L0 = ld(g, ecx);
    const V4 L1 = ld(g, 0x00d61670);
    st(g, arg0 + 0x10, L1);
    const V4 L2 = ld(g, 0x00d61670);
    st(g, arg0 + 0x20, L2);
    const V4 L3 = ld(g, 0x00d61670);
    st(g, arg0 + 0x30, L3);
    const V4 L4 = ld(g, arg0 + 0x10);
    const V4 L5 = ld(g, ecx);
    st(g, arg0 + 0x10, V4{{L4.f[0], L5.f[2], L4.f[2], L4.f[3]}});
    const V4 L6 = ld(g, arg0 + 0x10);
    const float t1 = sub(flt(0x00000000), L0.f[1]);
    st(g, arg0 + 0x10, V4{{L6.f[0], L6.f[1], t1, t1}});
    const V4 L7 = ld(g, arg0 + 0x20);
    st(g, arg0 + 0x20, V4{{sub(flt(0x00000000), L0.f[2]), L7.f[1], L7.f[2], L7.f[3]}});
    const V4 L8 = ld(g, ecx);
    const V4 L9 = ld(g, arg0 + 0x20);
    st(g, arg0 + 0x20, V4{{L9.f[0], L9.f[1], L8.f[0], L8.f[0]}});
    const V4 L10 = ld(g, ecx);
    const V4 L11 = ld(g, arg0 + 0x30);
    st(g, arg0 + 0x30, V4{{L10.f[1], L11.f[1], L11.f[2], L11.f[3]}});
    const V4 L12 = ld(g, arg0 + 0x30);
    st(g, arg0 + 0x30, V4{{L12.f[0], sub(flt(0x00000000), L0.f[0]), L12.f[2], L12.f[3]}});
    g.m.r[0] = arg0;
}

// 00858d90 FUN_00858d90: arg = copy of ECX (four 3x3 blocks: counts and rows)
void Matrix5x6_CopyFrom(Guest& g, uint32_t ecx, uint32_t arg0) {
    const uint32_t W0 = g.u32(ecx + 0x4);
    g.u32(arg0 + 0x4, W0);
    const uint32_t W1 = g.u32(ecx + 0x8);
    g.u32(arg0 + 0x8, W1);
    const uint32_t W2 = g.u32(ecx + 0x14);
    g.u32(arg0 + 0x14, W2);
    const uint32_t W3 = g.u32(ecx + 0x18);
    g.u32(arg0 + 0x18, W3);
    const V4 L4 = ld(g, ecx + 0x20);
    st(g, arg0 + 0x20, L4);
    const V4 L5 = ld(g, ecx + 0x30);
    st(g, arg0 + 0x30, L5);
    const V4 L6 = ld(g, ecx + 0x40);
    st(g, arg0 + 0x40, L6);
    const uint32_t W7 = g.u32(ecx + 0x54);
    g.u32(arg0 + 0x54, W7);
    const uint32_t W8 = g.u32(ecx + 0x58);
    g.u32(arg0 + 0x58, W8);
    const V4 L9 = ld(g, ecx + 0x60);
    st(g, arg0 + 0x60, L9);
    const V4 L10 = ld(g, ecx + 0x70);
    st(g, arg0 + 0x70, L10);
    const V4 L11 = ld(g, ecx + 0x80);
    st(g, arg0 + 0x80, L11);
    const uint32_t W12 = g.u32(ecx + 0x94);
    g.u32(arg0 + 0x94, W12);
    const uint32_t W13 = g.u32(ecx + 0x98);
    g.u32(arg0 + 0x98, W13);
    const V4 L14 = ld(g, ecx + 0xa0);
    st(g, arg0 + 0xa0, L14);
    const V4 L15 = ld(g, ecx + 0xb0);
    st(g, arg0 + 0xb0, L15);
    const V4 L16 = ld(g, ecx + 0xc0);
    st(g, arg0 + 0xc0, L16);
    const uint32_t W17 = g.u32(ecx + 0xd4);
    g.u32(arg0 + 0xd4, W17);
    const uint32_t W18 = g.u32(ecx + 0xd8);
    g.u32(arg0 + 0xd8, W18);
    const V4 L19 = ld(g, ecx + 0xe0);
    st(g, arg0 + 0xe0, L19);
    const V4 L20 = ld(g, ecx + 0xf0);
    st(g, arg0 + 0xf0, L20);
    const V4 L21 = ld(g, ecx + 0x100);
    st(g, arg0 + 0x100, L21);
    g.m.r[0] = arg0;
    g.m.r[2] = W18;
}

// 00856b40 FUN_00856b40: arg's two 3x4 row blocks (+20..40, +60..80) = block * EDX(4x4)^T in place (dot4); ECX(+20..50, +70..a0) = their transposes
void Fn_00856b40(Guest& g, uint32_t ecx, uint32_t edx, uint32_t arg0) {
    const V4 L0 = ld(g, edx + 0x10);
    const V4 L1 = ld(g, arg0 + 0x20);
    const V4 L2 = ld(g, arg0 + 0x30);
    const V4 L3 = ld(g, edx + 0x20);
    const V4 L4 = ld(g, edx + 0x30);
    const V4 L5 = ld(g, edx + 0x40);
    const V4 L6 = ld(g, arg0 + 0x40);
    const float t1 = dot4(L0, L1);
    const float t2 = dot4(L3, L1);
    const float t3 = dot4(L4, L1);
    const float t4 = dot4(L5, L1);
    st(g, arg0 + 0x20, V4{{t1, t2, t3, t4}});
    const float t5 = dot4(L0, L2);
    const float t6 = dot4(L3, L2);
    const float t7 = dot4(L4, L2);
    const float t8 = dot4(L5, L2);
    st(g, arg0 + 0x30, V4{{t5, t6, t7, t8}});
    const float t9 = dot4(L0, L6);
    const float t10 = dot4(L3, L6);
    const float t11 = dot4(L4, L6);
    const float t12 = dot4(L5, L6);
    st(g, arg0 + 0x40, V4{{t9, t10, t11, t12}});
    st(g, ecx + 0x20, V4{{t1, t5, t9, t1}});
    st(g, ecx + 0x30, V4{{t2, t6, t10, t2}});
    st(g, ecx + 0x40, V4{{t3, t7, t11, t3}});
    st(g, ecx + 0x50, V4{{t4, t8, t12, t4}});
    const V4 L7 = ld(g, edx + 0x10);
    const V4 L8 = ld(g, arg0 + 0x60);
    const V4 L9 = ld(g, edx + 0x20);
    const V4 L10 = ld(g, edx + 0x30);
    const V4 L11 = ld(g, arg0 + 0x70);
    const V4 L12 = ld(g, edx + 0x40);
    const V4 L13 = ld(g, arg0 + 0x80);
    const float t13 = dot4(L7, L8);
    const float t14 = dot4(L9, L8);
    const float t15 = dot4(L10, L8);
    const float t16 = dot4(L12, L8);
    st(g, arg0 + 0x60, V4{{t13, t14, t15, t16}});
    const float t17 = dot4(L7, L11);
    const float t18 = dot4(L9, L11);
    const float t19 = dot4(L10, L11);
    const float t20 = dot4(L12, L11);
    st(g, arg0 + 0x70, V4{{t17, t18, t19, t20}});
    const float t21 = dot4(L7, L13);
    const float t22 = dot4(L9, L13);
    const float t23 = dot4(L13, L10);
    const float t24 = dot4(L13, L12);
    st(g, arg0 + 0x80, V4{{t21, t22, t23, t24}});
    st(g, ecx + 0x70, V4{{t13, t17, t21, t13}});
    st(g, ecx + 0x80, V4{{t14, t18, t22, t14}});
    st(g, ecx + 0x90, V4{{t15, t19, t23, t15}});
    st(g, ecx + 0xa0, V4{{t16, t20, t24, t16}});
    g.m.r[0] = bits(t20);
    g.m.r[2] = bits(t24);
}

// 00855ee0 FUN_00855ee0: 6x6 block product: EDX / arg / ECX 3x3-block matrices (dot3 pairs); see the generated stores
void Fn_00855ee0(Guest& g, uint32_t ecx, uint32_t edx, uint32_t arg0) {
    const V4 L0 = ld(g, arg0 + 0x20);
    const V4 L1 = ld(g, ecx + 0x20);
    const V4 L2 = ld(g, ecx + 0x30);
    const V4 L3 = ld(g, ecx + 0x40);
    const V4 L4 = ld(g, arg0 + 0x30);
    const V4 L5 = ld(g, arg0 + 0x40);
    const V4 L6 = ld(g, arg0 + 0x50);
    const V4 L7 = ld(g, arg0 + 0x70);
    const V4 L8 = ld(g, ecx + 0x60);
    const V4 L9 = ld(g, ecx + 0x70);
    const V4 L10 = ld(g, ecx + 0x80);
    const V4 L11 = ld(g, arg0 + 0x80);
    const V4 L12 = ld(g, arg0 + 0x80);
    const V4 L13 = ld(g, arg0 + 0x80);
    const V4 L14 = ld(g, arg0 + 0x90);
    const V4 L15 = ld(g, arg0 + 0x90);
    const V4 L16 = ld(g, arg0 + 0x90);
    const V4 L17 = ld(g, arg0 + 0xa0);
    const V4 v1 = add(V4{{dot3(L8, L7), dot3(L1, L4), dot3(L1, L5), dot3(L1, L6)}}, V4{{dot3(L1, L0), dot3(L8, L11), dot3(L8, L14), dot3(L8, L17)}});
    st(g, edx + 0x20, v1);
    const V4 v2 = add(V4{{dot3(L2, L0), dot3(L2, L4), dot3(L2, L5), dot3(L2, L6)}}, V4{{dot3(L9, L7), dot3(L9, L12), dot3(L9, L15), dot3(L9, L17)}});
    st(g, edx + 0x30, v2);
    const V4 v3 = add(V4{{dot3(L3, L0), dot3(L3, L4), dot3(L3, L5), dot3(L3, L6)}}, V4{{dot3(L10, L7), dot3(L10, L13), dot3(L10, L16), dot3(L10, L17)}});
    st(g, edx + 0x40, v3);
    const V4 L18 = ld(g, arg0 + 0x20);
    const V4 L19 = ld(g, ecx + 0xa0);
    const V4 L20 = ld(g, ecx + 0xb0);
    const V4 L21 = ld(g, ecx + 0xc0);
    const V4 L22 = ld(g, arg0 + 0x30);
    const V4 L23 = ld(g, arg0 + 0x30);
    const V4 L24 = ld(g, arg0 + 0x30);
    const V4 L25 = ld(g, arg0 + 0x40);
    const V4 L26 = ld(g, arg0 + 0x40);
    const V4 L27 = ld(g, arg0 + 0x40);
    const V4 L28 = ld(g, arg0 + 0x50);
    const V4 L29 = ld(g, arg0 + 0x70);
    const V4 L30 = ld(g, ecx + 0xe0);
    const V4 L31 = ld(g, arg0 + 0x70);
    const V4 L32 = ld(g, ecx + 0xf0);
    const V4 L33 = ld(g, arg0 + 0x70);
    const V4 L34 = ld(g, ecx + 0x100);
    const V4 L35 = ld(g, arg0 + 0x80);
    const V4 L36 = ld(g, arg0 + 0x80);
    const V4 L37 = ld(g, arg0 + 0x80);
    const V4 L38 = ld(g, arg0 + 0x90);
    const V4 L39 = ld(g, arg0 + 0x90);
    const V4 L40 = ld(g, arg0 + 0x90);
    const V4 L41 = ld(g, arg0 + 0xa0);
    st(g, arg0 + 0x20, V4{{v1.f[0], v2.f[0], v3.f[0], v3.f[0]}});
    st(g, arg0 + 0x30, V4{{v1.f[1], v2.f[1], v3.f[1], v3.f[1]}});
    st(g, arg0 + 0x40, V4{{v1.f[2], v2.f[2], v3.f[2], v3.f[2]}});
    st(g, arg0 + 0x50, V4{{v1.f[3], v2.f[3], v3.f[3], v3.f[3]}});
    const V4 v4 = add(V4{{dot3(L30, L29), dot3(L32, L31), dot3(L34, L33), dot3(L34, L33)}}, V4{{dot3(L19, L18), dot3(L20, L18), dot3(L21, L18), dot3(L21, L18)}});
    st(g, arg0 + 0x70, v4);
    const V4 v5 = add(V4{{dot3(L19, L22), dot3(L20, L23), dot3(L21, L24), dot3(L21, L24)}}, V4{{dot3(L30, L35), dot3(L32, L36), dot3(L34, L37), dot3(L34, L37)}});
    st(g, arg0 + 0x80, v5);
    const V4 v6 = add(V4{{dot3(L19, L25), dot3(L20, L26), dot3(L21, L27), dot3(L21, L27)}}, V4{{dot3(L30, L38), dot3(L32, L39), dot3(L34, L40), dot3(L34, L40)}});
    st(g, arg0 + 0x90, v6);
    const V4 v7 = add(V4{{dot3(L19, L28), dot3(L20, L28), dot3(L21, L28), dot3(L21, L28)}}, V4{{dot3(L30, L41), dot3(L32, L41), dot3(L34, L41), dot3(L34, L41)}});
    st(g, arg0 + 0xa0, v7);
    st(g, edx + 0x60, V4{{v4.f[0], v5.f[0], v6.f[0], v7.f[0]}});
    st(g, edx + 0x70, V4{{v4.f[1], v5.f[1], v6.f[1], v7.f[1]}});
    st(g, edx + 0x80, V4{{v4.f[2], v5.f[2], v6.f[2], v7.f[2]}});
    g.m.r[0] = bits(v7.f[2]);
    g.m.r[1] = bits(v6.f[2]);
}

}  // namespace mech

REFLEX_PORT_FN(0x00854650, Mat3_MulABt, "mechanism", OUT_EAX | OUT_ECX | OUT_EDX) { Guest g{m}; Mat3_MulABt(g, m.r[1], m.r[2], m.arg(0)); return m.ret(0); }
REFLEX_PORT_FN(0x00854490, Mat3_MulVec, "mechanism", OUT_EAX | OUT_ECX | OUT_EDX) { Guest g{m}; Mat3_MulVec(g, m.r[1], m.r[2], m.arg(0)); return m.ret(0); }
REFLEX_PORT_FN(0x00854ac0, Mat4x3_MulMat3t, "mechanism", OUT_EAX | OUT_ECX | OUT_EDX) { Guest g{m}; Mat4x3_MulMat3t(g, m.r[1], m.r[2], m.arg(0)); return m.ret(0); }
REFLEX_PORT_FN(0x00854cb0, Mat3x4_MulMat4t, "mechanism", OUT_EAX | OUT_ECX | OUT_EDX) { Guest g{m}; Mat3x4_MulMat4t(g, m.r[1], m.r[2], m.arg(0)); return m.ret(0); }
REFLEX_PORT_FN(0x00855240, Mat4x3_MulMat4x3t, "mechanism", OUT_EAX | OUT_ECX | OUT_EDX) { Guest g{m}; Mat4x3_MulMat4x3t(g, m.r[1], m.r[2], m.arg(0)); return m.ret(0); }
REFLEX_PORT_FN(0x00855590, Mat3x4_MulMat3x4t, "mechanism", OUT_EAX | OUT_ECX | OUT_EDX) { Guest g{m}; Mat3x4_MulMat3x4t(g, m.r[1], m.r[2], m.arg(0)); return m.ret(0); }
REFLEX_PORT_FN(0x00853bd0, Mat4_MulVec, "mechanism", OUT_EAX | OUT_ECX | OUT_EDX) { Guest g{m}; Mat4_MulVec(g, m.r[1], m.r[2], m.arg(0)); return m.ret(0); }
REFLEX_PORT_FN(0x008540a0, Mat6_MulVec, "mechanism", OUT_EAX | OUT_ECX | OUT_EDX) { Guest g{m}; Mat6_MulVec(g, m.r[1], m.r[2], m.arg(0)); return m.ret(0); }
REFLEX_PORT_FN(0x00853a30, Mat4x6_MulVec, "mechanism", OUT_EAX | OUT_ECX | OUT_EDX) { Guest g{m}; Mat4x6_MulVec(g, m.r[1], m.r[2], m.arg(0)); return m.ret(0); }
REFLEX_PORT_FN(0x00857750, Mat4x6_MulVecSub, "mechanism", OUT_EAX | OUT_ECX | OUT_EDX) { Guest g{m}; Mat4x6_MulVecSub(g, m.r[1], m.r[2], m.arg(0)); return m.ret(0); }
REFLEX_PORT_FN(0x00857a00, Mat6x4_MulVecSub, "mechanism", OUT_EAX | OUT_ECX | OUT_EDX) { Guest g{m}; Mat6x4_MulVecSub(g, m.r[1], m.r[2], m.arg(0)); return m.ret(0); }
REFLEX_PORT_FN(0x00853e80, Mat6x4_MulVec, "mechanism", OUT_EAX | OUT_ECX | OUT_EDX) { Guest g{m}; Mat6x4_MulVec(g, m.r[1], m.r[2], m.arg(0)); return m.ret(0); }
REFLEX_PORT_FN(0x00857e50, Matrix3x3_FromQuaternion, "mechanism", OUT_EAX | OUT_ECX | OUT_EDX) { Guest g{m}; Matrix3x3_FromQuaternion(g, m.r[1], m.arg(0)); return m.ret(4); }
REFLEX_PORT_FN(0x00857d00, Matrix3x3_FromQuaternion2, "mechanism", OUT_EAX | OUT_ECX | OUT_EDX) { Guest g{m}; Matrix3x3_FromQuaternion2(g, m.r[1], m.arg(0)); return m.ret(4); }
REFLEX_PORT_FN(0x00857ff0, Matrix3x3_Skew, "mechanism", OUT_EAX | OUT_ECX | OUT_EDX) { Guest g{m}; Matrix3x3_Skew(g, m.r[1], m.arg(0)); return m.ret(4); }
REFLEX_PORT_FN(0x00858090, Matrix3x3_SkewT, "mechanism", OUT_EAX | OUT_ECX | OUT_EDX) { Guest g{m}; Matrix3x3_SkewT(g, m.r[1], m.arg(0)); return m.ret(4); }
REFLEX_PORT_FN(0x00858d90, Matrix5x6_CopyFrom, "mechanism", OUT_EAX | OUT_ECX | OUT_EDX) { Guest g{m}; Matrix5x6_CopyFrom(g, m.r[1], m.arg(0)); return m.ret(4); }
REFLEX_PORT_FN(0x00856b40, Fn_00856b40, "mechanism", OUT_EAX | OUT_ECX | OUT_EDX) { Guest g{m}; Fn_00856b40(g, m.r[1], m.r[2], m.arg(0)); return m.ret(0); }
REFLEX_PORT_FN(0x00855ee0, Fn_00855ee0, "mechanism", OUT_EAX | OUT_ECX | OUT_EDX) { Guest g{m}; Fn_00855ee0(g, m.r[1], m.r[2], m.arg(0)); return m.ret(0); }
