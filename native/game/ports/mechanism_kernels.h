// Kernels of the mechanism group callable from other mechanism ports (mechanism_kernels.cpp).
#pragma once
#include "mechanism_util.h"

namespace mech {

// 00854650: ECX(3x3) = EDX(3x3) * arg(3x3)^T: row j = (dot3(B0,Aj), dot3(B1,Aj), dot3(B2,Aj), W = lane 2); rows written in order
void Mat3_MulABt(Guest& g, uint32_t ecx, uint32_t edx, uint32_t arg0);
// 00854490: vec4 [ECX] = arg(3x3) * vec4 [EDX] (raw vec4s, no header); W = z
void Mat3_MulVec(Guest& g, uint32_t ecx, uint32_t edx, uint32_t arg0);
// 00854ac0: ECX(4 rows) = EDX(4 rows of xyz) * arg(3x3)^T, W = z; row 3 multiplies in the other operand order
void Mat4x3_MulMat3t(Guest& g, uint32_t ecx, uint32_t edx, uint32_t arg0);
// 00854cb0: ECX(3x4) = EDX(3x4) * arg(4x4)^T (dot4)
void Mat3x4_MulMat4t(Guest& g, uint32_t ecx, uint32_t edx, uint32_t arg0);
// 00855240: ECX(4x4) = EDX(4x3) * arg(4x3)^T (dot3)
void Mat4x3_MulMat4x3t(Guest& g, uint32_t ecx, uint32_t edx, uint32_t arg0);
// 00855590: ECX(3x3) = EDX(3x4) * arg(3x4)^T (dot4), W = lane 0
void Mat3x4_MulMat3x4t(Guest& g, uint32_t ecx, uint32_t edx, uint32_t arg0);
// 00853bd0: ECX+10 = arg(4x4) * EDX+10 (dot4)
void Mat4_MulVec(Guest& g, uint32_t ecx, uint32_t edx, uint32_t arg0);
// 008540a0: ECX(+10,+20) = arg(6x6: 3x3 blocks at +20/+60/+a0/+e0) * EDX(+10,+20); W = z
void Mat6_MulVec(Guest& g, uint32_t ecx, uint32_t edx, uint32_t arg0);
// 00853a30: EDX+10 = arg(4x6: blocks at +20/+70) * ECX(+10,+20)
void Mat4x6_MulVec(Guest& g, uint32_t ecx, uint32_t edx, uint32_t arg0);
// 00857750: ECX+10 -= arg(4x6: blocks at +20/+70) * EDX(+10,+20)
void Mat4x6_MulVecSub(Guest& g, uint32_t ecx, uint32_t edx, uint32_t arg0);
// 00857a00: ECX(+10,+20) -= arg(6x4: rows +20..40, +60..80) * EDX+10 (dot4); W = first row's
void Mat6x4_MulVecSub(Guest& g, uint32_t ecx, uint32_t edx, uint32_t arg0);
// 00853e80: ECX(+10,+20) = arg(6x4: rows +20..40, +60..80) * EDX+10 (dot4); W = first row's
void Mat6x4_MulVec(Guest& g, uint32_t ecx, uint32_t edx, uint32_t arg0);
// 00857e50: ECX rows = rotation matrix of the quaternion (x,y,z,w) at arg (constants -2 at 009d7b30, -0 at 009774a8, 1 at 00931f24)
void Matrix3x3_FromQuaternion(Guest& g, uint32_t ecx, uint32_t arg0);
// 00857d00: ECX rows = rotation matrix of the quaternion at arg, the +2 form (constant 2 at 0092e330)
void Matrix3x3_FromQuaternion2(Guest& g, uint32_t ecx, uint32_t arg0);
// 00857ff0: arg rows = cross-product matrix of the vec4 at ECX (cleared with 00d61670 first, then lanes set through memory)
void Matrix3x3_Skew(Guest& g, uint32_t ecx, uint32_t arg0);
// 00858090: arg rows = transposed cross-product matrix of the vec4 at ECX
void Matrix3x3_SkewT(Guest& g, uint32_t ecx, uint32_t arg0);
// 00858d90: arg = copy of ECX (four 3x3 blocks: counts and rows)
void Matrix5x6_CopyFrom(Guest& g, uint32_t ecx, uint32_t arg0);
// 00856b40: arg's two 3x4 row blocks (+20..40, +60..80) = block * EDX(4x4)^T in place (dot4); ECX(+20..50, +70..a0) = their transposes
void Fn_00856b40(Guest& g, uint32_t ecx, uint32_t edx, uint32_t arg0);
// 00855ee0: 6x6 block product: EDX / arg / ECX 3x3-block matrices (dot3 pairs); see the generated stores
void Fn_00855ee0(Guest& g, uint32_t ecx, uint32_t edx, uint32_t arg0);

}  // namespace mech
