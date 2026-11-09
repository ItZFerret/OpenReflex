// phys_math.cpp -- oracle adapters for core/game/phys_math.c (LTCG xmm conventions; callers read only xmm0).
#include "phys_oracle.h"
extern "C" {
#include "../game/phys_math.h"
}
using namespace phys_oracle;
using reflex_port::OUT_XMM0;

RX_PORT(0x0042c240, Vec4Sin_RangeReduced, "rx_phys", OUT_XMM0) { set_xmm(m, 0, phys_vec4_sin(xmm(m, 0))); return m.ret(0); }
RX_PORT(0x0042c650, Vec4Atan, "rx_phys", OUT_XMM0) { set_xmm(m, 0, phys_vec4_atan(xmm(m, 0))); return m.ret(0); }
RX_PORT(0x0042c880, Vec4Atan2, "rx_phys", OUT_XMM0) { set_xmm(m, 0, phys_vec4_atan2(xmm(m, 0), xmm(m, 1))); return m.ret(0); }
RX_PORT(0x0042ca00, QuatSlerp_Simd, "rx_phys", OUT_XMM0) {
    const rx_v4 q0 = xmm(m, 0), q1 = xmm(m, 1), t = xmm(m, 2);
    set_xmm(m, 0, phys_quat_slerp(q0, q1, t));
    phys_quat_slerp_dead_stack(m.r[4], m.r[5], q0, q1, t);   // the original's dead stack (notes/dead-stack.md)
    return m.ret(0);
}
RX_PORT(0x0042ccd0, Vec4Floor, "rx_phys", OUT_XMM0) { set_xmm(m, 0, phys_vec4_floor(xmm(m, 0))); return m.ret(0); }

// 0042c390 (xmm0 = x, cdecl sin_out*, ECX = cos_out*): callers read ECX (unchanged).
RX_PORT(0x0042c390, Vec4Cos_RangeReduced, "rx_phys", reflex_port::OUT_ECX) {
    phys_vec4_sincos(xmm(m, 0), m.arg(0), m.r[1]);
    return m.ret(0);
}
// 0042caf0 (xmm0, cdecl lane index): ECX = the stack cookie value as the original's check leaves it.
RX_PORT(0x0042caf0, Vec4_SplatLane, "rx_phys", OUT_XMM0 | reflex_port::OUT_ECX) {
    set_xmm(m, 0, phys_vec4_splat_lane(xmm(m, 0), int32_t(m.arg(0))));
    m.r[1] = m.mem.read(0x009b45e4, 4);
    return m.ret(0);
}
extern "C" {
#include "../game/phys_quat.h"
}
// 0042ce40 (EAX = a, ESI = b, EDI = out, cdecl t): memory only (callers read no register: the math agent's analysis).
RX_PORT(0x0042ce40, QuatSlerp_Packed, "rx_phys", 0) {
    phys_quat_slerp_packed(m.r[0], m.r[6], m.r[7], argf(m, 0));
    return m.ret(0);
}
// 0042db20 (ECX, EDX, 4 stack args; RET 0x10): EAX = probe, EDX = target pointer as the original leaves them.
RX_PORT(0x0042db20, ProbeFrame_Setup, "rx_phys", reflex_port::OUT_EAX | reflex_port::OUT_EDX) {
    phys_probe_frame_setup(m.r[1], m.r[2], m.arg(0), m.arg(1), m.arg(2), m.arg(3));
    m.r[0] = m.arg(0); m.r[2] = m.arg(1);
    return m.ret(0x10);
}
// 0042de70 (ESI = limits, ECX = q_b*; q_a*; RET 4): EAX = in both cones. (EDX: the CRT callees' leftover, not reproduced.)
RX_PORT(0x0042de70, Quat_WithinConeLimits, "rx_phys", reflex_port::OUT_EAX) {
    m.r[0] = uint32_t(phys_quat_within_cone_limits(m.r[6], m.r[1], m.arg(0)));
    return m.ret(4);
}
// 00841c90 (ECX = pose a, EDX = out; cdecl pose b, count, t).
RX_PORT(0x00841c90, Pose_Blend, "rx_phys", 0) {
    phys_pose_blend(m.r[1], m.r[2], m.arg(0), int32_t(m.arg(1)), argf(m, 2));
    return m.ret(0);
}
