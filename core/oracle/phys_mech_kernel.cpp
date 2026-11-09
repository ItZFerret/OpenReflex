// phys_mech_kernel.cpp -- oracle adapters for core/game/phys_mech_kernel.c (the solver's matrix kernels, LTCG registers).
// Registers: EAX r[0], ECX r[1], EDX r[2], ESI r[6], EDI r[7]; stack arguments m.arg(i). The leaf kernels load their first stack
// argument into EAX and leave ECX / EDX / xmm5..7 alone; where the original leaves spilled dot-product bits or the callee
// clobbers (flagged below) the output is not declared.
#include "phys_oracle.h"
extern "C" {
#include "../game/phys_mech.h"
}
using namespace phys_oracle;
using reflex_port::OUT_EAX; using reflex_port::OUT_ECX; using reflex_port::OUT_EDX; using reflex_port::OUT_FPU;
#define XMM(i) (reflex_port::OUT_XMM0 << (i))

// ---- matrix-vector products: (ECX, EDX, stack A); EAX = A, ECX / EDX kept ----
#define KERNEL3(addr, Name, fn, mask) \
    RX_PORT(addr, Name, "rx_phys", mask) { const uint32_t A = m.arg(0); fn(m.r[1], m.r[2], A); m.r[0] = A; return m.ret(0); }
KERNEL3(0x00854490, Mat3_MulVec, phys_mat3_mulvec, OUT_EAX | OUT_ECX | OUT_EDX | XMM(5))
KERNEL3(0x00854520, Mat3x3_MulVec, phys_mat3x3_mulvec, 0)
KERNEL3(0x008545b0, Mat3x4_MulVec, phys_mat3x4_mulvec, 0)
KERNEL3(0x00853ca0, Mat4x3_MulVec, phys_mat4x3_mulvec, 0)
KERNEL3(0x00853bd0, Mat4_MulVec, phys_mat4_mulvec, 0)
KERNEL3(0x00853d80, Mat3x6_MulVec, phys_mat3x6_mulvec, 0)
KERNEL3(0x008542c0, Mat5x6_MulVec, phys_mat5x6_mulvec, 0)
KERNEL3(0x00853fa0, Mat6x3_MulVec, phys_mat6x3_mulvec, 0)
KERNEL3(0x008540a0, Mat6_MulVec, phys_mat6_mulvec, OUT_EAX | OUT_EDX)
KERNEL3(0x00853a30, Mat4x6_MulVec, phys_mat4x6_mulvec, 0)
KERNEL3(0x00853e80, Mat6x4_MulVec, phys_mat6x4_mulvec, 0)
KERNEL3(0x00857650, Mat3x6_MulVecSub, phys_mat3x6_mulvec_sub, 0)
KERNEL3(0x00857750, Mat4x6_MulVecSub, phys_mat4x6_mulvec_sub, 0)
KERNEL3(0x008578f0, Mat6x3_MulVecSub, phys_mat6x3_mulvec_sub, 0)
KERNEL3(0x00857a00, Mat6x4_MulVecSub, phys_mat6x4_mulvec_sub, 0)

// ---- products: (ECX = out, EDX = B, stack A) ----
KERNEL3(0x00854650, Mat3_MulABt, phys_mat3_mul_abt, OUT_EAX | OUT_ECX | OUT_EDX | XMM(5) | XMM(6) | XMM(7))
KERNEL3(0x00854ac0, Mat4x3_MulMat3t, phys_mat4x3_mul_mat3t, OUT_EDX)
KERNEL3(0x00854cb0, Mat3x4_MulMat4t, phys_mat3x4_mul_mat4t, 0)
KERNEL3(0x00855590, Mat3x4_MulMat3x4t, phys_mat3x4_mul_mat3x4t, 0)
KERNEL3(0x00855240, Mat4x3_MulMat4x3t, phys_mat4x3_mul_mat4x3t, 0)
KERNEL3(0x008547d0, Mat3x6_MulABt, phys_mat3x6_mul_abt, 0)
KERNEL3(0x00854910, Mat4x6_MulABt, phys_mat4x6_mul_abt, 0)
// couplings (ECX, EDX, stack): EDX is left holding spilled bits: not declared
RX_PORT(0x00857220, Coupling3, "rx_phys", 0) { phys_coupling3(m.r[1], m.r[2], m.arg(0)); return m.ret(0); }
RX_PORT(0x00856b40, Coupling4, "rx_phys", 0) { phys_coupling4(m.r[1], m.r[2], m.arg(0)); return m.ret(0); }
RX_PORT(0x00855810, Coupling6x3, "rx_phys", 0) { phys_coupling6x3(m.r[1], m.r[2], m.arg(0)); return m.ret(0); }
RX_PORT(0x00855ee0, Coupling6x4, "rx_phys", 0) { phys_coupling6x4(m.r[1], m.r[2], m.arg(0)); return m.ret(0); }

// ---- composites ----
// 00854f00 (ECX = A, ESI = out, stack B): the last call is the slot-5 add: EAX = out.11, EDX = the Matrix3x3 local T (S + 0x20,
// S = ((entry esp - 4) & ~15) - 0x68 - 8).
RX_PORT(0x00854f00, Mat6_MulABt, "rx_phys", OUT_EAX | OUT_EDX) {
    const uint32_t sp0 = m.r[4], out = m.r[6], S = ((sp0 - 4) & ~15u) - 0x68 - 8;
    phys_mat6_mul_abt(m.r[1], out, m.arg(0));
    m.r[0] = out + 0xd0; m.r[2] = S + 0x20; return m.ret(0);
}
RX_PORT(0x008550f0, Mat3x3_MulABtSum, "rx_phys", OUT_EAX | OUT_EDX) { phys_mat3x3_mul_abt_sum(m.r[6], m.r[7], m.arg(0)); return m.ret(0); }
RX_PORT(0x00855190, Mat4x4_MulABtSum, "rx_phys", 0) { phys_mat4x4_mul_abt_sum(m.r[6], m.r[7], m.arg(0)); return m.ret(0); }
RX_PORT(0x00855090, Mat6_Mul3x4tBlocks, "rx_phys", 0) { phys_mat6_mul_3x4t_blocks(m.r[6], m.arg(0), m.arg(1)); return m.ret(0); }
RX_PORT(0x00855030, Mat6_Mul3tBlocks, "rx_phys", OUT_EAX | OUT_EDX) { phys_mat6_mul_3t_blocks(m.r[6], m.arg(0), m.arg(1)); return m.ret(0); }
RX_PORT(0x00854a90, Mat3x4_MulBlocks, "rx_phys", 0) { phys_mat3x4_mul_blocks(m.r[7], m.r[6], m.arg(0)); return m.ret(0); }
RX_PORT(0x00854a60, Mat3_MulBlocks, "rx_phys", 0) { phys_mat3_mul_blocks(m.r[7], m.r[6], m.arg(0)); return m.ret(0); }

// ---- inverses ----
// 00859250 (ECX = out, stack in): ends with out.transpose(out): ECX = out, EDX = out's vtable slot 2, EAX = out.
RX_PORT(0x00859250, Mat3_Invert, "rx_phys", OUT_ECX | OUT_EDX) {
    const uint32_t out = m.r[1];
    phys_mat3_invert(out, m.arg(0));
    m.r[0] = out; m.r[1] = out; m.r[2] = U32(U32(out, 0), 8); return m.ret(0);
}
// 00859340 (ESI = out, EDI = in): ECX = the stack cookie (xor of the stored copy with esp): EDX (stack bits) is not declared.
RX_PORT(0x00859340, Mat4_Invert, "rx_phys", OUT_ECX) {
    phys_mat4_invert(m.r[6], m.r[7]); m.r[1] = U32(0, 0x009b45e4); return m.ret(0);
}
// 008597e0 (ECX = in, stack out) / 00859520 (stack out, in): the epilogue leaves ECX = the cookie and EDX = the slot-6 address of
// out.D (a Matrix3x3 negate).
RX_PORT(0x008597e0, Mat6_Invert, "rx_phys", OUT_ECX | OUT_EDX) {
    const uint32_t out = m.arg(0);
    phys_mat6_invert(m.r[1], out);
    m.r[1] = U32(0, 0x009b45e4); m.r[2] = U32(U32(out + 0xd0, 0), 0x18); return m.ret(0);
}
RX_PORT(0x00859520, Mat5_Invert, "rx_phys", OUT_ECX | OUT_EDX) {
    const uint32_t out = m.arg(0);
    phys_mat5_invert(out, m.arg(1));
    m.r[1] = U32(0, 0x009b45e4); m.r[2] = U32(U32(out + 0xd0, 0), 0x18); return m.ret(0);
}

// ---- constructors ----
RX_PORT(0x00853550, Matrix3x6_Construct, "rx_phys", 0) { phys_matrix3x6_construct(m.r[0]); return m.ret(0); }
RX_PORT(0x00853710, Matrix6x3_Construct, "rx_phys", 0) { phys_matrix6x3_construct(m.r[0]); return m.ret(0); }
RX_PORT(0x008535b0, Matrix5x6_Construct, "rx_phys", 0) { phys_matrix5x6_construct(m.r[6]); m.r[0] = m.r[6]; return m.ret(0); }
RX_PORT(0x00853640, Matrix6x5_Construct, "rx_phys", 0) { phys_matrix6x5_construct(m.r[6]); m.r[0] = m.r[6]; return m.ret(0); }

// ---- rotation / cross-product matrices, copy ----
RX_PORT(0x00857e50, Matrix3x3_FromQuaternion, "rx_phys", OUT_ECX) { phys_matrix3x3_from_quaternion(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00857d00, Matrix3x3_FromQuaternion2, "rx_phys", 0) { phys_matrix3x3_from_quaternion2(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00857ff0, Matrix3x3_Skew, "rx_phys", 0) { phys_matrix3x3_skew(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00858090, Matrix3x3_SkewT, "rx_phys", OUT_EDX | XMM(5) | XMM(6)) {
    const uint32_t out = m.arg(0); phys_matrix3x3_skew_t(m.r[1], out); m.r[0] = out; return m.ret(4);
}
// 00858d90 (ECX = source, stack destination): EAX = destination, EDX = the last word read (source + 0xd8)
RX_PORT(0x00858d90, Matrix5x6_CopyFrom, "rx_phys", OUT_EAX | OUT_EDX) {
    const uint32_t src = m.r[1], edx = U32(src, 0xd8);
    m.r[0] = phys_matrix5x6_copy_from(src, m.arg(0)); m.r[2] = edx; return m.ret(4);
}

// 00401700 Vec3_Sub (ECX = a, EDX = b, stack out; EAX = out; ECX / EDX / xmm2 kept)
RX_PORT(0x00401700, Vec3_Sub, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX | XMM(2)) {
    m.r[0] = phys_vec3_sub(m.r[1], m.r[2], m.arg(0)); return m.ret(0);
}
