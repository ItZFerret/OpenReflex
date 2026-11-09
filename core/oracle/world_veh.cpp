// world_veh.cpp -- oracle adapters for core/game/world_veh_math.c (group rx_world): the small math helpers of the vehicle code.
#include "oracle.h"
#include "f80/f80.h"
extern "C" {
#include "../game/world_veh.h"
}
#include "phys_oracle.h"
using namespace reflex_port;
using namespace phys_oracle;

static rx_v4 v4s(float x) { rx_v4 r = {{x, 0.0f, 0.0f, 0.0f}}; return r; }
static rx_v4 v4b(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    rx_v4 r; uint32_t u[4] = {a, b, c, d}; std::memcpy(r.v, u, 16); return r;
}

// 00401000 (xmm0 lane 0 in / out; xmm1 = the mask as movss leaves it).
RX_PORT(0x00401000, Float_Abs, "rx_world", OUT_XMM0 | (OUT_XMM0 << 1)) {
    set_xmm(m, 0, v4s(rx_world_float_abs(xmm(m, 0).v[0])));
    set_xmm(m, 1, v4b(0x7fffffffu, 0, 0, 0));
    return m.ret(0);
}
RX_PORT(0x00401040, Float_Abs_Thunk, "rx_world", OUT_XMM0 | (OUT_XMM0 << 1)) {
    set_xmm(m, 0, v4s(rx_world_float_abs_thunk(xmm(m, 0).v[0])));
    set_xmm(m, 1, v4b(0x7fffffffu, 0, 0, 0));
    return m.ret(0);
}
// 00401020 (cdecl a, b; st(0) = fmod(a, b)).
RX_PORT(0x00401020, Float_Fmod, "rx_world", OUT_FPU) {
    const float r = rx_world_float_fmod(argf(m, 0), argf(m, 1));
    m.fpu.push_back(reflex_f80::from_f32(rx_oracle::bits(r)));
    return m.ret(0);
}
// 00401630 (EAX = a, ECX = c, cdecl out; EDX = out after).
RX_PORT(0x00401630, Vec3_Cross, "rx_world", OUT_EDX) {
    rx_world_vec3_cross(m.arg(0), m.r[1], m.r[0]);
    m.r[2] = m.arg(0);
    return m.ret(0);
}
// 0041c6c0 (stdcall out, s; RET 8; EAX = out).
RX_PORT(0x0041c6c0, Vec4_Splat, "rx_world", OUT_EAX) {
    rx_world_vec4_splat(m.arg(0), argf(m, 1));
    return m.ret(8, m.arg(0));
}
// 0041c640 (xmm0 = a, xmm1 = b, cdecl flags; xmm0 = xmm1 = the mask).
RX_PORT(0x0041c640, Vec4_CmpLe_CR6, "rx_world", OUT_XMM0 | (OUT_XMM0 << 1)) {
    const rx_v4 mask = rx_world_vec4_cmple_cr6(xmm(m, 0), xmm(m, 1), m.arg(0));
    set_xmm(m, 0, mask); set_xmm(m, 1, mask);
    return m.ret(0);
}
// 0041f5c0 (ECX = out, EDX = v, cdecl M; EAX = M).
RX_PORT(0x0041f5c0, Matrix3_MulVec3, "rx_world", OUT_EAX) {
    rx_world_matrix3_mul_vec3(m.r[1], m.r[2], m.arg(0));
    return m.ret(0, m.arg(0));
}
// 0041f650 (ECX = src, cdecl dst; EAX = dst).
RX_PORT(0x0041f650, Matrix3_Transpose, "rx_world", OUT_EAX) {
    rx_world_matrix3_transpose(m.arg(0), m.r[1]);
    return m.ret(0, m.arg(0));
}
// 0041f6e0 (ECX = dst, cdecl src).
RX_PORT(0x0041f6e0, Matrix3_Inverse, "rx_world", 0) {
    rx_world_matrix3_inverse(m.r[1], m.arg(0));
    return m.ret(0);
}
// 00401760 (EAX = src, EDX = dst; ECX = 0 after rep movsd).
RX_PORT(0x00401760, FUN_00401760, "rx_world", OUT_ECX) {
    rx_world_matrix4_transpose(m.r[2], m.r[0]);
    m.r[1] = 0;
    return m.ret(0);
}
// 0041ad80 (ECX = cursor; EAX = old, EDX = old + 4).
RX_PORT(0x0041ad80, Cursor_Advance4, "rx_world", OUT_EAX | OUT_EDX) {
    const uint32_t old = rx_world_cursor_advance4(m.r[1]);
    m.r[2] = old + 4;
    return m.ret(0, old);
}
// 00401210 (cdecl M; xmm0 = q). The permute loops leave EAX = 16 and DL = the last byte copied (q.w's top byte); ECX = the
// security cookie (__security_check_cookie's argument).
static void quat_scratch(reflex_vm::Machine& m, const rx_v4& q) {
    m.r[2] = (m.r[2] & 0xffffff00u) | (rx_oracle::bits(q.v[3]) >> 24);
    m.r[1] = uint32_t(m.mem.read(0x009b45e4, 4));
    m.r[0] = 0x10;
}
RX_PORT(0x00401210, Matrix3_ToQuaternion, "rx_world", OUT_XMM0 | OUT_EAX | OUT_ECX | OUT_EDX) {
    const rx_v4 q = rx_world_matrix3_to_quaternion(m.arg(0));
    set_xmm(m, 0, q);
    quat_scratch(m, q);
    return m.ret(0, m.r[0]);
}
// 00401740 (cdecl M, ESI = out; xmm0 and the scratch registers as the callee leaves them).
RX_PORT(0x00401740, Matrix3_ToQuaternionToEsi, "rx_world", OUT_XMM0 | OUT_EAX | OUT_ECX | OUT_EDX) {
    rx_world_matrix3_to_quaternion_to(m.r[6], m.arg(0));
    const rx_v4 q = rx_ld4(m.r[6]);
    set_xmm(m, 0, q);
    quat_scratch(m, q);
    return m.ret(0, m.r[0]);
}
// 0041f430 (ESI = A, ECX = B, EDX = out).
RX_PORT(0x0041f430, FUN_0041f430, "rx_world", 0) { rx_world_matrix3_mul(m.r[2], m.r[6], m.r[1]); return m.ret(0); }
// 0041f800 (cdecl A, B, C, D, out11, out12, out21, out22).
RX_PORT(0x0041f800, FUN_0041f800, "rx_world", 0) {
    rx_world_block_inverse(m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4), m.arg(5), m.arg(6), m.arg(7));
    return m.ret(0);
}
// 0041f390 (ECX = v, stdcall out; RET 4; EAX = out).
RX_PORT(0x0041f390, FUN_0041f390, "rx_world", OUT_EAX) { rx_world_matrix3_skew(m.arg(0), m.r[1]); return m.ret(4, m.arg(0)); }
// 0041f160 (ECX = out, stdcall q; RET 4; EAX = q).
RX_PORT(0x0041f160, FUN_0041f160, "rx_world", OUT_EAX) { rx_world_quat_to_matrix3(m.r[1], m.arg(0)); return m.ret(4, m.arg(0)); }
// 0041feb0 (ESI = skeleton, EDX = bone, EDI = out).
RX_PORT(0x0041feb0, FUN_0041feb0, "rx_world", 0) { rx_world_bone_point_world(m.r[7], m.r[6], m.r[2]); return m.ret(0); }
// 0041f030 (ESI = skeleton).
RX_PORT(0x0041f030, FUN_0041f030, "rx_world", 0) { rx_world_skeleton_orient(m.r[6]); return m.ret(0); }
// 0041fe60 (EDX = skeleton, EDI = v, stdcall arg; RET 4; EDX = the skeleton after: its caller reads it).
RX_PORT(0x0041fe60, FUN_0041fe60, "rx_world", OUT_EDX) {
    const uint32_t skel = m.r[2];
    rx_world_skeleton_set_velocity(skel, m.r[7], m.arg(0));
    m.r[2] = skel;
    return m.ret(4);
}
