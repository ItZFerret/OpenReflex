// crt_oracle.h -- ABI glue shared by the rx_crt adapters (core/oracle/crt_*.cpp): x87 stack, xmm lanes, stack arguments, and the
// crt_bypass hand-off (a port that reaches a path it does not implement calls the original through its own entry: the adapter
// then declines with RUN_ORIGINAL, game/crt_util.h).
#pragma once
#include "oracle.h"
#include "f80/f80.h"
extern "C" {
#include "../src/rx_x87.h"
#include "../game/crt.h"
#include "../game/crt_util.h"
}

namespace crt_oracle {
inline rx_f80 to_rx(const reflex_f80::F80& v) { rx_f80 r; r.kind = v.kind; r.sign = v.sign; r.mant = v.mant; r.exp = v.exp; return r; }
inline reflex_f80::F80 from_rx(const rx_f80& v) { reflex_f80::F80 r; r.kind = v.kind; r.sign = v.sign; r.mant = v.mant; r.exp = v.exp; return r; }
inline rx_f80 st(reflex_vm::Machine& m, int i) { return to_rx(m.fpu[m.fpu.size() - 1 - size_t(i)]); }
inline void fpop(reflex_vm::Machine& m, int n = 1) { while (n-- > 0) m.fpu.pop_back(); }
inline void fpush(reflex_vm::Machine& m, const rx_f80& v) { m.fpu.push_back(from_rx(v)); }
inline double argd(reflex_vm::Machine& m, int i) { return rx_bd(uint64_t(m.arg(i)) | (uint64_t(m.arg(i + 1)) << 32)); }
inline uint64_t xmm_lo(reflex_vm::Machine& m, int i) { return m.xmm[i].lo; }
// the bypass: true when this entry is the port's own call of the original (the adapter must return RUN_ORIGINAL)
inline bool bypass(uint32_t address) {
    if (crt_bypass != address) return false;
    crt_bypass = 0;
    static int shown = 0;   // diagnostics: the first fallbacks to the original (paths not ported)
    if (shown < 20) { shown++; std::fprintf(stderr, "[rx_crt] %08x: unported path (%s), the original runs\n", address, crt_why ? crt_why : "?"); }
    crt_why = nullptr;
    return true;
}
}  // namespace crt_oracle

// RX_CRT_PORT: RX_PORT with the bypass check first.
#define RX_CRT_PORT(addr, name, outputs)                                                       \
    static uint32_t rxcrt_body_##name(reflex_vm::Machine& m);                                  \
    RX_PORT(addr, name, "rx_crt", outputs) {                                                   \
        if (crt_oracle::bypass(addr)) return reflex_vm::RUN_ORIGINAL;                          \
        const rx_addr saved_sp_ = crt_entry_sp; crt_entry_sp = m.r[4];                         \
        uint32_t saved_regs_[8]; std::memcpy(saved_regs_, crt_caller_regs, sizeof saved_regs_);\
        std::memcpy(crt_caller_regs, m.r, sizeof saved_regs_);                                 \
        const uint32_t pc_r_ = rxcrt_body_##name(m);                                           \
        std::memcpy(crt_caller_regs, saved_regs_, sizeof saved_regs_);                         \
        crt_entry_sp = saved_sp_; return pc_r_; }                                              \
    static uint32_t rxcrt_body_##name(reflex_vm::Machine& m)
