// bike_oracle.h -- ABI glue shared by the bike adapters (oracle/bike_*.cpp): register snapshots and xmm access.
// The original's callers (LTCG) may read scratch registers after a call (plan/live.tsv): an adapter restores every scratch
// register to its entry value (what an untouched register holds), then sets those the original changes (its declared outputs).
#pragma once
#include "oracle.h"
#include <cstdio>
#include <cstdlib>

namespace bike_oracle {
using reflex_port::OUT_EAX; using reflex_port::OUT_ECX; using reflex_port::OUT_EDX; using reflex_port::OUT_XMM0;
using reflex_port::OUT_FPU;
constexpr uint32_t XMM(int i) { return reflex_port::OUT_XMM0 << i; }

struct Scratch {
    uint32_t r0, r1, r2; reflex_vm::Xmm x[8];
    explicit Scratch(const reflex_vm::Machine& m) : r0(m.r[0]), r1(m.r[1]), r2(m.r[2]) { for (int i = 0; i < 8; i++) x[i] = m.xmm[i]; }
    void restore(reflex_vm::Machine& m) const { m.r[0] = r0; m.r[1] = r1; m.r[2] = r2; for (int i = 0; i < 8; i++) m.xmm[i] = x[i]; }
};

inline rx_v4 xmm(const reflex_vm::Machine& m, int i) {
    uint32_t b[4] = {uint32_t(m.xmm[i].lo), uint32_t(m.xmm[i].lo >> 32), uint32_t(m.xmm[i].hi), uint32_t(m.xmm[i].hi >> 32)};
    rx_v4 v; std::memcpy(v.v, b, 16); return v;
}
inline float xmm_f(const reflex_vm::Machine& m, int i) { return rx_oracle::f32(uint32_t(m.xmm[i].lo)); }
inline void set_xmm(reflex_vm::Machine& m, int i, rx_v4 v) {
    uint32_t b[4]; std::memcpy(b, v.v, 16);
    m.xmm[i].lo = b[0] | (uint64_t(b[1]) << 32); m.xmm[i].hi = b[2] | (uint64_t(b[3]) << 32);
}
// movss xmm, m32: lane 0 = f, lanes 1..3 zero
inline void set_xmm_ss(reflex_vm::Machine& m, int i, float f) { m.xmm[i].lo = rx_oracle::bits(f); m.xmm[i].hi = 0; }
// movss xmm, xmm: lane 0 = f, lanes 1..3 kept
inline void set_xmm_lane0(reflex_vm::Machine& m, int i, float f) { m.xmm[i].lo = (m.xmm[i].lo & ~0xffffffffull) | rx_oracle::bits(f); }
inline float argf(reflex_vm::Machine& m, int i) { return rx_oracle::f32(m.arg(i)); }

// The core's x87 code assumes the simulation thread's control word 0x027f (53-bit precision, round to nearest): report any other once.
inline void check_cw(const reflex_vm::Machine& m, const char* who) {
    static bool said = false;
    if ((m.fpu_cw & 0xf00) != 0x200 && !said) { said = true; std::fprintf(stderr, "rx_bike: %s runs with x87 CW %04x (core assumes 027f)\n", who, m.fpu_cw); }
}
}  // namespace bike_oracle
