// fx_rtbump.cpp -- oracle adapters for core/game/fx_rtbump.c (the RT_Bump cascade layout). 007ba8d0 also checks
// fx_rtbump_camera_inputs (the preparation 007c1010 does before the call) against the arguments the original receives:
// RX_RTBUMP_PREP=1 prints the calls whose preparation differs (stderr), and a summary at exit.
#include "phys_oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
extern "C" {
#include "../game/fx_rtbump.h"
}
using namespace phys_oracle;

namespace {
struct PrepStats {
    unsigned calls = 0, bad = 0;
    ~PrepStats() { if (calls && std::getenv("RX_RTBUMP_PREP")) std::fprintf(stderr, "[rtbump] prep: %u calls, %u differ\n", calls, bad); }
} prep_stats;

void check_prep(reflex_vm::Machine& m) {
    if (!std::getenv("RX_RTBUMP_PREP")) return;
    fx_rtbump_inputs in;
    fx_rtbump_camera_inputs(m.arg(1), &in);
    const uint32_t fh = m.r[1], rh = m.r[2], up = m.arg(4);
    const float got[] = {F32(fh, 0), F32(fh, 4), F32(fh, 8), F32(rh, 0), F32(rh, 4), F32(rh, 8), F32(up, 0), F32(up, 4), F32(up, 8),
                         argf(m, 5), xmm(m, 1).v[0]};
    const float mine[] = {in.fwd_h[0], in.fwd_h[1], in.fwd_h[2], in.right_h[0], in.right_h[1], in.right_h[2], in.up[0], in.up[1], in.up[2],
                          in.aspect, in.fov};
    prep_stats.calls++;
    for (int i = 0; i < 11; i++)
        if (std::memcmp(&got[i], &mine[i], 4) != 0) {
            prep_stats.bad++;
            std::fprintf(stderr, "[rtbump] prep differs at %d: original %.9g core %.9g\n", i, got[i], mine[i]);
            break;
        }
}
}  // namespace

// 007ba8d0 (ECX fwd_h*, EDX right_h*; stack event*, camera*, position*, forward*, up*, aspect, view_out*, trap_out*; xmm1 fov;
// RET 0x20): callers read no registers.
RX_PORT(0x007ba8d0, RTBump_FitCascadeMatrices, "rx_rtbump", 0) {
    check_prep(m);
    { static bool once = false; if (!once && std::getenv("RX_RTBUMP_PREP")) { once = true; std::fprintf(stderr, "[rtbump] view_out %08x trap_out %08x\n", m.arg(6), m.arg(7)); } }
    fx_rtbump_fit_cascades(m.r[1], m.r[2], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4), argf(m, 5), m.arg(6), m.arg(7),
                           xmm(m, 1).v[0], nullptr);
    return m.ret(0x20);
}
// 007ba260 (ECX p0*, EDX p1*; stack out*, p3*, p2*; RET 0xc)
RX_PORT(0x007ba260, RTBump_TrapezoidMatrix, "rx_rtbump", 0) {
    fx_rtbump_trapezoid_matrix(m.r[1], m.r[2], m.arg(0), m.arg(1), m.arg(2));
    return m.ret(0xc);
}
