// The exe's own runtime helpers (statically linked CRT pieces at 0x008fxxxx) and the thin libm wrappers at 0x0042c1d0..0x0042c23e.
#include "math_util.h"
#include "f80/f80.h"

using reflex_game::Guest;
using namespace x86;
using namespace mathx;
using reflex_f80::F80;

namespace {
int x87_bits(const reflex_vm::Machine& m) { int pc = (m.fpu_cw >> 8) & 3; return pc == 0 ? 24 : pc == 2 ? 53 : 64; }
int x87_rc(const reflex_vm::Machine& m) { return (m.fpu_cw >> 10) & 3; }

// fistp qword: round with the control word's mode; NaN, infinity and out of range give the integer indefinite (INT64_MIN)
int64_t fistp64(const F80& v, int rc) {
    if (v.kind != 0) return INT64_MIN;
    F80 ri = reflex_f80::round_to_integer(v, rc);
    if (ri.mant == 0) return 0;
    if (ri.exp > 63 || (ri.exp == 63 && !(ri.sign && ri.mant == (1ull << 63)))) return INT64_MIN;
    if (ri.exp == 63) return INT64_MIN;
    uint64_t mag = ri.mant >> (63 - ri.exp);
    return ri.sign ? -int64_t(mag) : int64_t(mag);
}

// 008fa106 _ftol2: st(0) -> EDX:EAX, truncated toward zero (popped). The CRT's way: n = fistp (nearest), then the sign of the
// float x - n corrects n by one toward zero (the add-0x7fffffff carry trick).
uint32_t ftol2(reflex_vm::Machine& m) {
    const F80 x = m.fpu.back();
    const int rc = x87_rc(m);
    const uint32_t xf = reflex_f80::to_f32(x, rc);           // fst dword [esp + 0x18]
    const int64_t n = fistp64(x, rc);                         // fistp qword [esp + 0x10]
    const F80 nf = reflex_f80::from_int(n);                   // fild
    uint32_t eax = uint32_t(n), edx = xf, hi = uint32_t(uint64_t(n) >> 32);
    if (eax == 0) {
        edx = hi;
        if ((edx & 0x7fffffffu) == 0) {                       // n == 0 (or INT64_MIN's low half): both popped, no correction
            m.fpu.pop_back();
            m.r[0] = eax; m.r[2] = edx;
            return m.ret(0);
        }
    }
    const F80 d = reflex_f80::add(x, nf, x87_bits(m), rc, true);   // fsubp st(1): x - n
    m.fpu.pop_back();                                         // x (replaced by d, which fstp pops)
    const uint32_t df = reflex_f80::to_f32(d, rc);
    uint32_t ecx;
    if (int32_t(edx) < 0) {                                   // x < 0: n + 1 when x - n < 0 (n rounded below x)
        ecx = df ^ 0x80000000u;
        bool cf = uint64_t(ecx) + 0x7fffffffu > 0xffffffffu;
        ecx += 0x7fffffffu;
        uint64_t s = uint64_t(eax) + (cf ? 1 : 0);
        eax = uint32_t(s); cf = s >> 32;
        edx = hi + (cf ? 1 : 0);
    } else {                                                  // x >= 0: n - 1 when x - n < 0
        bool cf = uint64_t(df) + 0x7fffffffu > 0xffffffffu;
        ecx = df + 0x7fffffffu;
        bool borrow = eax < (cf ? 1u : 0u);
        eax -= cf ? 1 : 0;
        edx = hi - (borrow ? 1 : 0);
    }
    m.r[0] = eax; m.r[1] = ecx; m.r[2] = edx;
    return m.ret(0);
}
}  // namespace

REFLEX_PORT_FN(0x008fa106, Ftol2, "math", reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX | reflex_port::OUT_FPU) {
    return ftol2(m);
}

// 008fa0d0 _ftol2_sse: with SSE2 (the CRT's flag at 0xdcf760) st(0) is stored as a double and truncated by cvttsd2si into EAX;
// without it, _ftol2.
REFLEX_PORT_FN(0x008fa0d0, Ftol2_Sse, "math", reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX | reflex_port::OUT_FPU) {
    if (Guest{m}.u32(0xdcf760) == 0) return ftol2(m);
    double d = dbl(reflex_f80::to_f64(m.fpu.back(), x87_rc(m)));
    m.fpu.pop_back();
    m.r[0] = (d > -2147483649.0 && d < 2147483648.0) ? uint32_t(int32_t(d)) : 0x80000000u;   // NaN: indefinite
    return m.ret(0);
}

// 008f9efe __security_check_cookie: ECX must equal the cookie at 0x9b45e4, else ___report_gsfailure (a jump, not a call).
REFLEX_PORT_FN(0x008f9efe, SecurityCheckCookie, "math", 0) {
    if (m.r[1] == Guest{m}.u32(0x9b45e4)) return m.ret(0);
    return 0x008fa43c;
}

// 0042c230 sqrtf(float) for SSE callers: xmm0 lane 0 = sqrt of the stack argument (upper lanes kept), cdecl.
REFLEX_PORT_FN(0x0042c230, SqrtF, "math", reflex_port::OUT_XMM0) {
    Regs r(m);
    r[0] = sqrtss(r[0], X::ss(m.arg(0)));
    r.store(m);
    return m.ret(0);
}

// 0042c1d0 ceilf: xmm0 lane 0 widened to a double and passed to MSVCR90's ceil (result on the x87 stack).
REFLEX_PORT_FN(0x0042c1d0, CeilF, "math", reflex_port::OUT_XMM_ALL | reflex_port::OUT_FPU | reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX) {
    Regs r(m);
    r[0] = cvtss2sd(r[0], r[0]);
    r.store(m);
    Guest g{m};
    g.call(g.u32(0x916238), {r[0].u[0], r[0].u[1]});
    return m.ret(0);
}

// 0042c1f0 powf: xmm0, xmm1 lane 0 widened and passed to MSVCR90's __libm_sse2_pow (through the exe's thunk at 0x9009b4),
// the double result narrowed back into xmm0.
REFLEX_PORT_FN(0x0042c1f0, PowF, "math", reflex_port::OUT_XMM_ALL | reflex_port::OUT_FPU | reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX) {
    Regs r(m);
    r[0] = cvtss2sd(r[0], r[0]);
    r[1] = cvtss2sd(r[1], r[1]);
    call_regs(m, r, 0x009009b4);
    r[0] = cvtsd2ss(r[0], r[0]);
    r.store(m);
    return m.ret(0);
}

