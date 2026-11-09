// phys_crt.cpp -- oracle adapters for core/game/phys_crt.c.
#include "phys_oracle.h"
extern "C" {
#include "../game/crt.h"
#include "../game/phys_crt.h"
}
using namespace phys_oracle;
using namespace reflex_port;

// the x87 precision control of the calling thread (control word bits 8-9: 00 = 24, 10 = 53, 11 = 64 bits)
static int x87_precision(const reflex_vm::Machine& m) { const int pc = (m.fpu_cw >> 8) & 3; return pc == 0 ? 24 : pc == 2 ? 53 : 64; }

// 008fa106 _ftol2: st(0) popped, EDX:EAX.
RX_PORT(0x008fa106, Ftol2, "rx_phys", OUT_EAX | OUT_EDX | OUT_FPU) {
    const int64_t v = phys_ftol2(st0(m), x87_precision(m));
    m.fpu.pop_back();
    m.r[0] = uint32_t(v); m.r[2] = uint32_t(uint64_t(v) >> 32);
    return m.ret(0);
}
// 008fa0d0 _ftol2_sse: st(0) popped, EAX (the SSE2 path leaves ECX / EDX; the _ftol2 path sets EDX).
RX_PORT(0x008fa0d0, Ftol2_Sse, "rx_phys", OUT_EAX | OUT_FPU) {
    const rx_f80 x = st0(m);
    if (m.mem.read(0x00dcf760, 4) == 0) {
        const int64_t v = phys_ftol2(x, x87_precision(m));
        m.r[2] = uint32_t(uint64_t(v) >> 32);
    }
    m.r[0] = uint32_t(phys_ftol2_sse(x, x87_precision(m)));
    m.fpu.pop_back();
    return m.ret(0);
}
RX_PORT(0x008f9efe, SecurityCheckCookie, "rx_phys", 0) { phys_security_check_cookie(m.r[1]); return m.ret(0); }
// 0042c230 SqrtF: xmm0 lane 0 (lanes 1..3 kept).
RX_PORT(0x0042c230, SqrtF, "rx_phys", OUT_XMM0) {
    rx_v4 x = xmm(m, 0); x.v[0] = phys_sqrtf(argf(m, 0)); set_xmm(m, 0, x);
    return m.ret(0);
}
// 0042c1d0 CeilF: the result on the x87 stack (the original's xmm0 holds the widened argument).
RX_PORT(0x0042c1d0, CeilF, "rx_phys", OUT_FPU) {
    rx_v4 x = xmm(m, 0);
    const size_t depth = m.fpu.size();
    const float r = phys_ceilf(x.v[0]);
    while (m.fpu.size() > depth) m.fpu.pop_back();          // the callee's push, replaced by the core's result
    fpush_f32(m, r);
    const uint64_t d = rx_db(double(x.v[0])); std::memcpy(x.v, &d, 8); set_xmm(m, 0, x);
    return m.ret(0);
}
// 0042c1f0 PowF: xmm0 = the CRT's xmm0 with lane 0 narrowed (cvtsd2ss keeps lanes 1..3); ECX as the CRT leaves it.
RX_PORT(0x0042c1f0, PowF, "rx_phys", OUT_XMM0 | OUT_ECX) {
    const float base = xmm(m, 0).v[0], exponent = xmm(m, 1).v[0];
    uint64_t x0[2]; uint32_t regs[3];
    crt_libm_sse2_pow_ex(double(base), double(exponent), x0, regs);
    const float r = phys_powf(base, exponent);
    rx_v4 x; std::memcpy(x.v, x0, 16); x.v[0] = r; set_xmm(m, 0, x);
    m.r[1] = regs[1];
    return m.ret(0);
}
