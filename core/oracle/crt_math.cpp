// crt_math.cpp -- oracle adapters for core/game/crt_math.c (msvcr90 libm entry points).
#include "crt_oracle.h"
using namespace crt_oracle;
using namespace reflex_port;

// 785a1254 _copysign(double x, double y): st(0) = the result; EAX = its high dword.
RX_CRT_PORT(0x785a1254, CopySign, OUT_EAX | OUT_FPU) {
    const double r = crt_copysign(argd(m, 0), argd(m, 2));
    fpush(m, rx_f80_from_f64(r));
    m.r[0] = uint32_t(rx_db(r) >> 32);
    return m.ret(0);
}
// 785a166d _finite(double x): EAX = EDX = the result, ECX = the exponent word & 0x7ff0.
RX_CRT_PORT(0x785a166d, Finite, OUT_EAX | OUT_ECX | OUT_EDX) {
    const int32_t r = crt_finite(argd(m, 0));
    m.r[0] = m.r[2] = uint32_t(r);
    m.r[1] = (m.arg(1) >> 16) & 0x7ff0u;                        // mov ecx, eax (0x7ff0); and cx, [ebp+0xe]
    return m.ret(0);
}
// 785a2f90 floor / 785a2da0 ceil (double on the stack, written back where the original stores): st(0); EAX = sign and exponent.
// NaN: EAX = &errno (the error path 785a8fa1).
RX_CRT_PORT(0x785a2f90, Floor, OUT_EAX | OUT_FPU) {
    uint32_t eax = 0;
    const double r = crt_floor_args_ex(m.r[4] + 4, m.fpu_cw, m.mxcsr, &eax);
    fpush(m, rx_f80_from_f64(r));
    m.r[0] = eax;
    return m.ret(0);
}
RX_CRT_PORT(0x785a2da0, Ceil, OUT_EAX | OUT_FPU) {
    uint32_t eax = 0;
    const double r = crt_ceil_args_ex(m.r[4] + 4, m.fpu_cw, m.mxcsr, &eax);
    fpush(m, rx_f80_from_f64(r));
    m.r[0] = eax;
    return m.ret(0);
}
// __cintrindisp2 register results: crt_ci_atan2_ex / crt_ci_fmod_ex (crt_math.c: the normal tail's EAX / ECX / EDX, or the error path's).
// 7859aa6a _CIatan2: st(1) = y, st(0) = x popped, result pushed.
RX_CRT_PORT(0x7859aa6a, CIatan2, OUT_EAX | OUT_ECX | OUT_EDX | OUT_FPU) {
    const rx_f80 y = st(m, 1), x = st(m, 0);
    uint32_t regs[3];
    const rx_f80 r = crt_ci_atan2_ex(y, x, m.fpu_cw, regs);
    fpop(m, 2); fpush(m, r);
    m.r[0] = regs[0]; m.r[1] = regs[1]; m.r[2] = regs[2];
    return m.ret(0);
}
// 7859ac8a _CIfmod: st(1) = x, st(0) = y.
RX_CRT_PORT(0x7859ac8a, CIfmod, OUT_EAX | OUT_ECX | OUT_EDX | OUT_FPU) {
    const rx_f80 x = st(m, 1), y = st(m, 0);
    uint32_t regs[3];
    const rx_f80 r = crt_ci_fmod_ex(x, y, m.fpu_cw, regs);
    fpop(m, 2); fpush(m, r);
    m.r[0] = regs[0]; m.r[1] = regs[1]; m.r[2] = regs[2];
    return m.ret(0);
}
// 7859afc0 _CIpow: st(1) = x, st(0) = y.
RX_CRT_PORT(0x7859afc0, CIpow, OUT_EAX | OUT_ECX | OUT_EDX | OUT_FPU) {
    const rx_f80 x = st(m, 1), y = st(m, 0);
    uint32_t regs[3];
    const rx_f80 r = crt_ci_pow_ex(x, y, m.fpu_cw, m.mxcsr, regs);
    fpop(m, 2); fpush(m, r);
    m.r[0] = regs[0]; m.r[1] = regs[1]; m.r[2] = regs[2];
    return m.ret(0);
}
// 7859e9d7 __libm_sse2_pow: xmm0 = x, xmm1 = y (doubles); xmm0 = the result (both lanes as the original leaves them).
RX_CRT_PORT(0x7859e9d7, LibmSse2Pow, OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM0) {
    uint64_t lanes[2]; uint32_t regs[3];
    uint64_t H[8]; for (int i = 0; i < 8; i++) H[i] = m.xmm[i].hi;
    crt_libm_sse2_pow_lanes(rx_bd(m.xmm[0].lo), rx_bd(m.xmm[1].lo), H, lanes, regs);
    m.xmm[0].lo = lanes[0]; m.xmm[0].hi = lanes[1];
    m.r[0] = regs[0]; m.r[1] = regs[1]; m.r[2] = regs[2];
    return m.ret(0);
}
// 7859fb0c __libm_sse2_sin: xmm0 lane 0 = the result, lane 1 = the caller's xmm4 lane 1 (the result is built in xmm4 by scalar
// operations); EAX = the table entry, EDX = its offset (main path).
RX_CRT_PORT(0x7859fb0c, LibmSse2Sin, OUT_EAX | OUT_EDX | OUT_XMM0) {
    uint32_t regs[3] = {m.r[0], m.r[1], m.r[2]}; int ok;
    const double x = rx_bd(m.xmm[0].lo);
    const double r = crt_libm_sse2_sin_ex(x, regs, &ok);
    if (!ok) {
        int lane1_zero, ok2;
        const double b = crt_libm_sse2_sin_big(x, m.r[4], m.fpu_cw, regs, &lane1_zero, &ok2);
        if (ok2) {
            m.r[0] = regs[0]; m.r[1] = regs[1]; m.r[2] = regs[2];
            m.xmm[0].lo = rx_db(b); if (lane1_zero) m.xmm[0].hi = 0;
            return m.ret(0);
        }
        m.xmm[0].lo = rx_db(crt_libm_sse2_sin(x)); return m.ret(0);
    }
    const uint16_t ax = uint16_t((uint16_t(m.xmm[0].lo >> 48) & 0x7fff) - 0x3030);
    if (ax <= 0x10c5) { m.r[0] = regs[0]; m.r[2] = regs[2]; m.xmm[0].hi = m.xmm[4].hi; }
    else m.r[0] = uint16_t(ax >> 4);                            // pextrw (zero-extended), 16-bit and / sub / shr
    m.xmm[0].lo = rx_db(r);
    return m.ret(0);
}
