// phys_oracle.h -- ABI glue shared by the rx_phys adapters (core/oracle/phys_*.cpp): xmm registers as rx_v4, x87 st(0).
#pragma once
#include "oracle.h"
#include "f80/f80.h"
extern "C" {
#include "../src/rx_x87.h"
}

namespace phys_oracle {
inline rx_v4 xmm(reflex_vm::Machine& m, int i) {
    const auto& x = m.xmm[i];
    uint32_t b[4] = {uint32_t(x.lo), uint32_t(x.lo >> 32), uint32_t(x.hi), uint32_t(x.hi >> 32)};
    rx_v4 r; std::memcpy(r.v, b, 16); return r;
}
inline void set_xmm(reflex_vm::Machine& m, int i, const rx_v4& v) {
    uint32_t b[4]; std::memcpy(b, v.v, 16);
    m.xmm[i].lo = b[0] | (uint64_t(b[1]) << 32); m.xmm[i].hi = b[2] | (uint64_t(b[3]) << 32);
}
inline float argf(reflex_vm::Machine& m, int i) { return rx_oracle::f32(m.arg(i)); }
}  // namespace phys_oracle

namespace phys_oracle {
// x87 st(0) as the core's rx_f80 (same fields as reflex_f80::F80)
inline rx_f80 st0(reflex_vm::Machine& m) {
    const reflex_f80::F80& v = m.fpu.back();
    rx_f80 r; r.kind = v.kind; r.sign = v.sign; r.mant = v.mant; r.exp = v.exp; return r;
}
inline void fpush_f32(reflex_vm::Machine& m, float f) { m.fpu.push_back(reflex_f80::from_f32(rx_oracle::bits(f))); }
}  // namespace phys_oracle
