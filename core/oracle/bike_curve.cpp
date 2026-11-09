// bike_curve.cpp -- oracle adapters for core/game/bike_curve.c.
#include "bike_oracle.h"
extern "C" {
#include "../game/bike_curve.h"
}
using namespace bike_oracle;

// 00430810: xmm2 = t, stack a, b (ret 0); result xmm0 = (r, 0, 0, 0). The interpolating path leaves xmm1 = (a, 0, 0, 0).
RX_PORT(0x00430810, Lerp_Clamped, "rx_bike", OUT_EAX | OUT_ECX | OUT_EDX | XMM(0) | XMM(1) | XMM(2) | OUT_FPU) {
    const float t = xmm_f(m, 2), a = argf(m, 0), b = argf(m, 1);
    const float r = bike_lerp_clamped(t, a, b);
    if (!(0.0f >= t) && !(t >= 1.0f)) set_xmm_ss(m, 1, a);
    set_xmm_ss(m, 0, r);
    return m.ret(0);
}

// 00438ee0: ESI = curve, xmm0 = x (ret 0); result xmm0 = (y, 0, 0, 0). Left over: EDX = segment (-1 outside), EAX / ECX = the
// knot loop's counter / pointer (4-way unrolled loop over i < n-1, then the remainder).
RX_PORT(0x00438ee0, ParamCurve_EvalCubic, "rx_bike", OUT_EAX | OUT_ECX | OUT_EDX | XMM(0) | OUT_FPU) {
    const uint32_t C = m.r[6];
    const float x = xmm_f(m, 0);
    float t;
    const int32_t seg = bike_param_curve_segment(C, x, &t);
    const float y = bike_param_curve(C, x);
    const int32_t last = int32_t(m.mem.read(C, 4)) - 1;
    int32_t i = 0; uint32_t p = m.r[1];
    if (last >= 4) { p = C + 8; do { i += 4; p += 0x10; } while (i < last - 3); }
    if (i < last) { p = C + 4 * uint32_t(i) + 4; do { i++; p += 4; } while (i < last); }
    m.r[0] = uint32_t(i); m.r[1] = p; m.r[2] = uint32_t(seg);
    set_xmm_ss(m, 0, y);
    return m.ret(0);
}

// 004390d0: ESI = table, xmm0 = x (ret 0); result xmm0 = (y, 0, 0, 0) (Lerp_Clamped's). Only xmm0 is read by callers.
RX_PORT(0x004390d0, IndexedCurve_Interp, "rx_bike", XMM(0) | OUT_FPU) {
    check_cw(m, "004390d0");
    const float y = bike_indexed_curve(m.r[6], xmm_f(m, 0));
    set_xmm_ss(m, 0, y);
    return m.ret(0);
}
